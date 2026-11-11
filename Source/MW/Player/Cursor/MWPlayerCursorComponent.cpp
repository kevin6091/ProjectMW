#include "Player/Cursor/MWPlayerCursorComponent.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformTime.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

bool FMWSharedCursorState::IsValidPosition(const FVector& Value) const
{
	if (Value.ContainsNaN()) return false;
	if (Space == EMWCursorSpace::NormalizedWidget)
		return Value.X >= 0.0 && Value.X <= 1.0 && Value.Y >= 0.0 && Value.Y <= 1.0 && Value.Z == 0.0;
	return Space == EMWCursorSpace::World && Value.GetAbsMax() <= 1000000.0;
}

UMWPlayerCursorComponent::UMWPlayerCursorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMWPlayerCursorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMWPlayerCursorComponent, State);
}

bool UMWPlayerCursorComponent::HasServerAuthority() const
{
	return GetOwner() && GetOwner()->HasAuthority();
}

bool UMWPlayerCursorComponent::OpenStreamForServer(FName Context, EMWCursorSpace Space)
{
	if (!HasServerAuthority() || !GetWorld() || Context.IsNone() || State.StreamId == MAX_int32
		|| (Space != EMWCursorSpace::NormalizedWidget && Space != EMWCursorSpace::World)) return false;
	CloseStreamForServer();
	++State.StreamId;
	State.Context = Context;
	State.Space = Space;
	LastReceivedSequence = 0;
	Tokens = 30.0;
	LastBudgetTime = FPlatformTime::Seconds();
	GetOwner()->ForceNetUpdate();
	return true;
}

void UMWPlayerCursorComponent::CloseStreamForServer()
{
	if (!HasServerAuthority()) return;
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ExpiryTimer);
	State.Context = NAME_None;
	State.bVisible = false;
	State.Position = FVector::ZeroVector;
	GetOwner()->ForceNetUpdate();
}

void UMWPlayerCursorComponent::SubmitLocalCursor(FName Context, bool bVisible, FVector Position)
{
	const APlayerState* PS = Cast<APlayerState>(GetOwner());
	const APlayerController* PC = PS ? PS->GetPlayerController() : nullptr;
	if (!PC || !PC->IsLocalController() || !GetWorld() || !State.IsOpen() || State.Context != Context) return;
	if (bVisible && !State.IsValidPosition(Position)) return;
	if (LocalStreamId != State.StreamId)
	{
		LocalStreamId = State.StreamId;
		LocalSequence = 0;
		LastSendTime = -1.0;
		bLastSentVisible = false;
	}
	const double Now = GetWorld()->GetRealTimeSeconds();
	// 화면 종료/창 이탈의 첫 숨김은 즉시 보내고 유실 시 다음 heartbeat로 보완한다.
	const bool bHiding = bLastSentVisible && !bVisible;
	if ((!bHiding && LastSendTime >= 0.0 && Now - LastSendTime < SendInterval) || LocalSequence == MAX_int32) return;
	LastSendTime = Now;
	bLastSentVisible = bVisible;
	ServerSubmitCursor(LocalStreamId, ++LocalSequence, bVisible, bVisible ? Position : FVector::ZeroVector);
}

void UMWPlayerCursorComponent::ServerSubmitCursor_Implementation(int32 StreamId, int32 Sequence, bool bVisible, FVector Position)
{
	const APlayerState* PS = Cast<APlayerState>(GetOwner());
	const APlayerController* PC = PS ? PS->GetPlayerController() : nullptr;
	// PlayerState → 소유 Controller의 연결로만 요청한다. 다른 사람의 ID를 인자로 받지 않는다.
	if (!PS || PS->IsInactive() || !PC || PC->PlayerState != PS || PC->GetWorld() != GetWorld()) return;
	ReceiveSampleForServer(StreamId, Sequence, bVisible, Position);
}

bool UMWPlayerCursorComponent::ReceiveSampleForServer(int32 StreamId, int32 Sequence, bool bVisible, const FVector& Position)
{
	if (!HasServerAuthority() || !GetWorld() || !State.IsOpen() || State.StreamId != StreamId
		|| Sequence <= LastReceivedSequence || (bVisible && !State.IsValidPosition(Position))) return false;
	const double Now = FPlatformTime::Seconds();
	Tokens = FMath::Min(30.0, Tokens + FMath::Max(0.0, Now - LastBudgetTime) * 30.0);
	LastBudgetTime = Now;
	if (Tokens < 1.0) return false;
	Tokens -= 1.0;
	LastReceivedSequence = Sequence;
	if (!bVisible)
	{
		HideExpiredCursor();
		return true;
	}
	// 같은 위치도 만료를 연장한다. 값이 같으면 불필요한 복제 갱신은 하지 않는다.
	GetWorld()->GetTimerManager().SetTimer(ExpiryTimer, this, &ThisClass::HideExpiredCursor, ExpirySeconds, false);
	const double Tolerance = State.Space == EMWCursorSpace::NormalizedWidget ? 0.00001 : 0.1;
	if (!State.bVisible || !State.Position.Equals(Position, Tolerance))
	{
		State.Position = Position;
		State.bVisible = true;
		GetOwner()->ForceNetUpdate();
	}
	return true;
}

void UMWPlayerCursorComponent::HideExpiredCursor()
{
	if (!HasServerAuthority()) return;
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ExpiryTimer);
	if (!State.bVisible) return;
	State.bVisible = false;
	GetOwner()->ForceNetUpdate();
}

void UMWPlayerCursorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ExpiryTimer);
	Super::EndPlay(EndPlayReason);
}
