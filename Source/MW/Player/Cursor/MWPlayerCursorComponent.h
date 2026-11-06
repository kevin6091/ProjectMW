#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Player/Cursor/MWSharedCursorTypes.h"
#include "MWPlayerCursorComponent.generated.h"

// PlayerState의 기본 서브오브젝트. 소유 연결의 전송·검사·복제·만료만 담당한다.
// 화면 생성, 마우스 수집, 카메라 투영, 선택 승인, Phase 전환은 담당하지 않는다.
UCLASS(ClassGroup=(MW), BlueprintType)
class MW_API UMWPlayerCursorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMWPlayerCursorComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category="MW|Cursor")
	FMWSharedCursorState GetCursorState() const { return State; }

	// 서버 진행 코드가 화면 수명을 허용/종료한다. 클라이언트는 스트림을 열 수 없다.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="MW|Cursor")
	bool OpenStreamForServer(FName Context, EMWCursorSpace Space);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="MW|Cursor")
	void CloseStreamForServer();

	// 로컬 화면에서 매 프레임 호출해도 최대 20Hz로 보낸다. 정지/숨김도 heartbeat를 보낸다.
	UFUNCTION(BlueprintCallable, Category="MW|Cursor")
	void SubmitLocalCursor(FName Context, bool bVisible, FVector Position);

	// RPC 수신과 서버 시험이 사용하는 공통 검사 경로. 요청자는 이 컴포넌트의 소유자다.
	bool ReceiveSampleForServer(int32 StreamId, int32 Sequence, bool bVisible, const FVector& Position);

	static constexpr double SendInterval = 0.05;
	static constexpr float ExpirySeconds = 0.75f;

private:
	UPROPERTY(Replicated)
	FMWSharedCursorState State;
	UFUNCTION(Server, Unreliable)
	void ServerSubmitCursor(int32 StreamId, int32 Sequence, bool bVisible, FVector Position);

	void HideExpiredCursor();
	bool HasServerAuthority() const;
	FTimerHandle ExpiryTimer;
	int32 LastReceivedSequence = 0;
	double Tokens = 30.0;
	double LastBudgetTime = 0.0;
	int32 LocalStreamId = 0;
	int32 LocalSequence = 0;
	double LastSendTime = -1.0;
	bool bLastSentVisible = false;
};
