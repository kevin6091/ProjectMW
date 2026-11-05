#include "MWMultiplayerSubsystem.h"
#include "System/MWGameInstance.h"
UMWGameInstance* UMWMultiplayerSubsystem::App() const { return Cast<UMWGameInstance>(GetGameInstance()); }

void UMWMultiplayerSubsystem::Initialize(FSubsystemCollectionBase& C)
{
	Super::Initialize(C);
	if (auto* A = App())
	{
		A->OnLoginFinished.AddUniqueDynamic(this, &ThisClass::ForwardLoginFinished);
		A->OnSessionStateChanged.AddUniqueDynamic(this, &ThisClass::ForwardSessionStateChanged);
		A->OnSessionError.AddUniqueDynamic(this, &ThisClass::ForwardSessionError);
		A->OnCreateRoomFinished.AddUniqueDynamic(this, &ThisClass::ForwardCreateRoomFinished);
		A->OnRoomsUpdated.AddUniqueDynamic(this, &ThisClass::ForwardRoomsUpdated);
		A->OnJoinRoomFinished.AddUniqueDynamic(this, &ThisClass::ForwardJoinRoomFinished);
	}
}

void UMWMultiplayerSubsystem::Deinitialize()
{
	if (auto* A = App())
	{
		A->OnLoginFinished.RemoveDynamic(this, &ThisClass::ForwardLoginFinished);
		A->OnSessionStateChanged.RemoveDynamic(this, &ThisClass::ForwardSessionStateChanged);
		A->OnSessionError.RemoveDynamic(this, &ThisClass::ForwardSessionError);
		A->OnCreateRoomFinished.RemoveDynamic(this, &ThisClass::ForwardCreateRoomFinished);
		A->OnRoomsUpdated.RemoveDynamic(this, &ThisClass::ForwardRoomsUpdated);
		A->OnJoinRoomFinished.RemoveDynamic(this, &ThisClass::ForwardJoinRoomFinished);
	}
	Super::Deinitialize();
}

void UMWMultiplayerSubsystem::ForwardLoginFinished(bool bSuccess, const FString& ErrorMessage)
{
	OnLoginFinished.Broadcast(bSuccess, ErrorMessage);
}

void UMWMultiplayerSubsystem::ForwardSessionStateChanged(EMWSessionState PreviousState, EMWSessionState NewState)
{
	OnSessionStateChanged.Broadcast(PreviousState, NewState);
}

void UMWMultiplayerSubsystem::ForwardSessionError(const FString& ErrorMessage)
{
	OnSessionError.Broadcast(ErrorMessage);
}

void UMWMultiplayerSubsystem::ForwardCreateRoomFinished(bool bSuccess, const FString& ErrorMessage)
{
	OnCreateRoomFinished.Broadcast(bSuccess, ErrorMessage);
}

void UMWMultiplayerSubsystem::ForwardRoomsUpdated(bool bSuccess, const FString& ErrorMessage)
{
	OnRoomsUpdated.Broadcast(bSuccess, ErrorMessage);
}

void UMWMultiplayerSubsystem::ForwardJoinRoomFinished(bool bSuccess, const FString& ErrorMessage)
{
	OnJoinRoomFinished.Broadcast(bSuccess, ErrorMessage);
}

void UMWMultiplayerSubsystem::RequestLogin()
{
	if (auto* A = App()) A->RequestLogin();
}

void UMWMultiplayerSubsystem::RequestRefreshRooms()
{
	if (auto* A = App()) A->RequestRefreshRooms();
}

void UMWMultiplayerSubsystem::RequestLeaveRoom()
{
	if (auto* A = App()) A->RequestLeaveRoom();
}

void UMWMultiplayerSubsystem::RetrySessionCleanup()
{
	if (auto* A = App()) A->RetrySessionCleanup();
}

bool UMWMultiplayerSubsystem::IsLoggedIn() const
{
	return App() && App()->IsLoggedIn();
}

bool UMWMultiplayerSubsystem::IsLoginInProgress() const
{
	return App() && App()->IsLoginInProgress();
}

bool UMWMultiplayerSubsystem::IsSessionBusy() const
{
	return App() && App()->IsSessionBusy();
}

EMWSessionState UMWMultiplayerSubsystem::GetSessionState() const
{
	return App() ? App()->GetSessionState() : EMWSessionState::None;
}

FString UMWMultiplayerSubsystem::GetLastSessionError() const
{
	return App() ? App()->GetLastSessionError() : TEXT("MWGameInstance 설정이 필요합니다.");
}

int32 UMWMultiplayerSubsystem::GetRoomListRevision() const { return App() ? App()->GetRoomListRevision() : 0; }

TArray<FMWRoomInfo> UMWMultiplayerSubsystem::GetAvailableRooms() const
{
	return App() ? App()->GetAvailableRooms() : TArray<FMWRoomInfo>();
}

TArray<FName> UMWMultiplayerSubsystem::GetAvailableMapThemeIds() const
{
	return App() ? App()->GetAvailableMapThemeIds() : TArray<FName>();
}

void UMWMultiplayerSubsystem::RequestCreateRoom(const FMWRoomSettings& Settings)
{
	if (auto* A = App()) A->RequestCreateRoom(Settings);
}

void UMWMultiplayerSubsystem::RequestJoinRoom(const FString& SessionId, int32 ListRevision)
{
	if (auto* A = App()) A->RequestJoinRoom(SessionId, ListRevision);
}

bool UMWMultiplayerSubsystem::ValidateRoomSettings(const FMWRoomSettings& InputSettings,
                                                   FMWRoomSettings& OutValidatedSettings,
                                                   FString& OutErrorMessage) const
{
	return App() && App()->ValidateRoomSettings(InputSettings, OutValidatedSettings, OutErrorMessage);
}
