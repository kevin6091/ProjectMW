#include "System/MWMultiplayerSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "LogChannel.h"

void UMWMultiplayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	bLoginInProgress = false;
	bDeinitializing = false;
}

void UMWMultiplayerSubsystem::Deinitialize()
{
	bDeinitializing = true;

	// 이전 콜백을 무효화.
	++LoginRequestId;

	ClearLoginDelegate();

	bLoginInProgress = false;

	// 이 Subsystem이 종료되므로 UI 구독도 정리.
	OnLoginFinished.Clear();

	Super::Deinitialize();
}

void UMWMultiplayerSubsystem::RequestLogin()
{
	if (bDeinitializing)
	{
		return;
	}

	if (bLoginInProgress)
	{
		UE_LOG(
			LogMWEOS,
			Verbose,
			TEXT("Login request ignored: another login is in progress."));

		return;
	}

	FString ResolveError;
	IOnlineIdentityPtr Identity = ResolveIdentity(ResolveError);

	if (!Identity.IsValid())
	{
		FinishLogin(false, ResolveError);
		return;
	}

	// 이미 로그인돼 있으면 온라인 로그인 요청을 다시 보내지 않음.
	if (Identity->GetLoginStatus(LoginLocalUserNum) == ELoginStatus::LoggedIn)
	{
		const auto UserId = Identity->GetUniquePlayerId(LoginLocalUserNum);

		if (UserId.IsValid() && UserId->IsValid())
		{
			FinishLogin(true, FString());
		}
		else
		{
			FinishLogin(false, TEXT("Login status is LoggedIn, but the user ID is invalid."));
		}

		return;
	}

	ClearLoginDelegate();

	LoginIdentity = Identity;

	const uint64 CurrentRequestId = ++LoginRequestId;

	// Login()이 콜백을 즉시 호출할 가능성에 대비해 먼저 설정합니다.
	bLoginInProgress = true;

	LoginCompleteHandle = LoginIdentity->AddOnLoginCompleteDelegate_Handle(
			LoginLocalUserNum, 
			FOnLoginCompleteDelegate::CreateUObject(this, &UMWMultiplayerSubsystem::HandleLoginComplete, CurrentRequestId));

	FOnlineAccountCredentials Credentials;
	Credentials.Type = TEXT("accountportal");

	UE_LOG(LogMWEOS, Display, TEXT("Starting EOS login. LocalUserNum=%d"),
	       LoginLocalUserNum);

	// 호출 도중 완료 콜백이 실행돼도 인터페이스 참조가 유지되게 .
	const IOnlineIdentityPtr IdentityForRequest = LoginIdentity;

	const bool bStarted = IdentityForRequest->Login(LoginLocalUserNum, Credentials);

	// Login() 내부에서 이미 콜백이 실행됐을 수 있음.
	// 같은 요청이 아직 진행 중일 때만 즉시 실패를 처리.
	if (!bStarted && bLoginInProgress && LoginRequestId == CurrentRequestId)
	{
		FinishLogin(false, TEXT("EOS rejected the login request before it could start."));
	}
}

bool UMWMultiplayerSubsystem::IsLoggedIn() const
{
	FString Error;
	IOnlineIdentityPtr Identity = ResolveIdentity(Error);

	if (!Identity.IsValid())
	{
		return false;
	}

	if (Identity->GetLoginStatus(LoginLocalUserNum) != ELoginStatus::LoggedIn)
	{
		return false;
	}

	const auto UserId = Identity->GetUniquePlayerId(LoginLocalUserNum);

	return UserId.IsValid() && UserId->IsValid();
}

IOnlineIdentityPtr UMWMultiplayerSubsystem::ResolveIdentity(FString& OutError) const
{
	OutError.Reset();

	if (bDeinitializing)
	{
		OutError = TEXT("Multiplayer subsystem is deinitialized.");
		return nullptr;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;

	if (!World)
	{
		OutError = TEXT("None World");
		return nullptr;
	}

	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(World);

	if (!OnlineSubsystem)
	{
		OutError = TEXT("None OnlineSubsystem");
		return nullptr;
	}

	// NULL 등 다른 서비스로 실행되는 상태를 구분.
	if (OnlineSubsystem->GetSubsystemName() != TEXT("EOS"))
	{
		OutError = FString::Printf(TEXT("Expected EOS, but the active subsystem is '%s'."),
		                           *OnlineSubsystem->GetSubsystemName().ToString());

		return nullptr;
	}

	IOnlineIdentityPtr Identity = OnlineSubsystem->GetIdentityInterface();

	if (!Identity.IsValid())
	{
		OutError = TEXT("EOS identity interface is not valid.");
		return nullptr;
	}

	return Identity;
}

void UMWMultiplayerSubsystem::HandleLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId,
                                                  const FString& Error, uint64 RequestId)
{
	if (bDeinitializing || !bLoginInProgress || RequestId != LoginRequestId || LocalUserNum != LoginLocalUserNum)
	{
		return;
	}

	const bool bValidLogin =
		bWasSuccessful && UserId.IsValid() &&
		LoginIdentity.IsValid() && 
		LoginIdentity->GetLoginStatus(LoginLocalUserNum) == ELoginStatus::LoggedIn;

	FString ResultError = Error;

	if (bValidLogin)
	{
		UE_LOG(LogMWEOS, Display, TEXT("EOS login succeeded. LocalUserNum=%d UserId=%s"), LocalUserNum, *UserId.ToString());
	}
	else if (ResultError.IsEmpty())
	{
		ResultError = TEXT("EOS login completed without a valid authenticated user.");
	}

	FinishLogin(bValidLogin, ResultError);
}

void UMWMultiplayerSubsystem::ClearLoginDelegate()
{
	if (LoginIdentity.IsValid() && LoginCompleteHandle.IsValid())
	{
		LoginIdentity->ClearOnLoginCompleteDelegate_Handle(LoginLocalUserNum, LoginCompleteHandle);
	}

	LoginCompleteHandle.Reset();
	LoginIdentity.Reset();
}

void UMWMultiplayerSubsystem::FinishLogin(bool bSuccess, const FString& ErrorMessage)
{
	const FString FinalError = bSuccess ? FString() : ErrorMessage;

	// 상태를 정리한 후 UI를 호출해야,
	// UI에서 재시도하더라도 이전 요청과 섞이지 않습니다.
	++LoginRequestId;

	ClearLoginDelegate();

	bLoginInProgress = false;

	if (bSuccess)
	{
		UE_LOG(LogMWEOS, Display, TEXT("Login finished successfully."));
	}
	else
	{
		UE_LOG(LogMWEOS, Warning, TEXT("Login failed: %s"), *FinalError);
	}

	OnLoginFinished.Broadcast(bSuccess, FinalError);
}
