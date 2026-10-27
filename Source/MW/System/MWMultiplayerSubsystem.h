#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "MWMultiplayerSubsystem.generated.h"

// Subsystem이 Widget에 전달할 로그인 결과입니다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FMWLoginFinished,
	bool, bSuccess,
	const FString&, ErrorMessage
);

UCLASS(BlueprintType)
class MW_API UMWMultiplayerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	// 로그인 작업을 요청.
	// 최종 결과는 OnLoginFinished로 전달.
	UFUNCTION(BlueprintCallable, Category = "MW|Multiplayer|Login")
	void RequestLogin();

	// 온라인 인터페이스의 현재 로그인 상태를 조회.
	UFUNCTION(BlueprintPure, Category = "MW|Multiplayer|Login")
	bool IsLoggedIn() const; 

	UFUNCTION(BlueprintPure, Category = "MW|Multiplayer|Login")
	bool IsLoginInProgress() const
	{
		return bLoginInProgress;
	}

	UPROPERTY(BlueprintAssignable, Category = "MW|Multiplayer|Login")
	FMWLoginFinished OnLoginFinished;
	
private:
	// 현재 World에 맞는 EOS IdentityInterface를 구함.
	IOnlineIdentityPtr ResolveIdentity(FString& OutError) const;

	// 온라인 인터페이스가 호출하는 완료 콜백.
	void HandleLoginComplete(
		int32 LocalUserNum,
		bool bWasSuccessful,
		const FUniqueNetId& UserId,
		const FString& Error,
		uint64 RequestId);

	// 등록했던 엔진 Delegate를 해제.
	void ClearLoginDelegate();

	// 상태 정리 후 UI에 최종 결과를 전달.
	void FinishLogin(bool bSuccess, const FString& ErrorMessage);

private:
	// 이번 구현에서는 실행 인스턴스당 로컬 사용자 한 명.
	static constexpr int32 LoginLocalUserNum = 0;

	UPROPERTY(Blueprintable, meta = (AllowPrivateAccess = "true"))
	bool bLoginInProgress = false;
	bool bDeinitializing = false;

	// 오래된 요청의 콜백을 구분.
	uint64 LoginRequestId = 0;

	// Delegate를 등록한 바로 그 인터페이스를 보관.
	IOnlineIdentityPtr LoginIdentity;

	FDelegateHandle LoginCompleteHandle;
};
