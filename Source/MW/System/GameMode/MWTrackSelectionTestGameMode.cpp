#include "System/GameMode/MWTrackSelectionTestGameMode.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Gameplay/Construction/MWPlayerConstructionComponent.h"
#include "Gameplay/Construction/MWTrackConstructionComponent.h"
#include "Gameplay/Construction/MWTrackSelectionBoard.h"
#include "LogChannel.h"
#include "Player/MWGameplayPlayerController.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/MWPlayerState.h"

AMWTrackSelectionTestGameMode::AMWTrackSelectionTestGameMode()
{
	GameStateClass = AMWGameplayGameState::StaticClass();
	PlayerStateClass = AMWPlayerState::StaticClass();
	PlayerControllerClass = AMWGameplayPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
}

void AMWTrackSelectionTestGameMode::StartPlay()
{
	Super::StartPlay();
	TryStartSelection();
}

void AMWTrackSelectionTestGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (AMWPlayerState* PS = NewPlayer->GetPlayerState<AMWPlayerState>())
	{
		PS->SetLobbyMemberIdFromServer(NextTestMemberId++);
	}

	TryStartSelection();
}

void AMWTrackSelectionTestGameMode::HandleStartingNewPlayer_Implementation(APlayerController*)
{
	// 선택판과 카메라를 사용하므로 플레이어 Pawn을 생성하지 않는다.
}

void AMWTrackSelectionTestGameMode::Logout(AController* Exiting)
{
	if (auto* GS = GetGameState<AMWGameplayGameState>())
	{
		GS->GetConstructionComponent()->RemovePlacementPlayer(Exiting->GetPlayerState<AMWPlayerState>());
	}
	Super::Logout(Exiting);
}

void AMWTrackSelectionTestGameMode::TryStartSelection()
{
	AMWGameplayGameState* GS = GetGameState<AMWGameplayGameState>();

	if (bSelectionStarted || !GS || !GetWorld()->HasBegunPlay())
	{
		return;
	}

	TArray<AMWPlayerState*> Players;
	for (APlayerState* State : GS->PlayerArray)
	{
		AMWPlayerState* PS = Cast<AMWPlayerState>(State);
		if (IsValid(PS) && !PS->IsInactive() && PS->GetLobbyMemberId() > 0)
		{
			Players.Add(PS);
		}
	}

	// Standalone PIE 창들은 서로 연결되지 않은 별도 월드다.
	// 혼자 Play한 경우도 후보를 확인할 수 있도록 단독 월드는 1명으로 시작한다.
	const int32 RequiredPlayers = GetNetMode() == NM_Standalone ? 1 : FMath::Max(1, RequiredNetworkPlayers);
	if (Players.Num() < RequiredPlayers)
	{
		return;
	}

	UMWTrackConstructionComponent* Construction = GS->GetConstructionComponent();
	FText Error;
	if (!Construction->BeginSelection(TestTrackDefinitionIds, Players, Error))
	{
		UE_LOG(LogMWServer, Error, TEXT("트랙 선택 시작 실패. %s"), *Error.ToString());
		return;
	}

	bSelectionStarted = true;
	Construction->OnAllSelectionsConfirmed.AddUObject(this, &ThisClass::TryFinishSelection);
}

void AMWTrackSelectionTestGameMode::TryFinishSelection()
{
	AMWGameplayGameState* GS = GetGameState<AMWGameplayGameState>();
	if (!GS)
	{
		return;
	}

	UMWTrackConstructionComponent* Construction = GS->GetConstructionComponent();
	TActorIterator<AMWTrackSelectionBoard> Board(GetWorld());
	FText Error = FText::FromString(TEXT("레벨에 트랙 선택판이 없습니다."));
	// 테스트 맵에서 기존 도로가 향하던 +Y 방향을 유지한다.
	const FTransform BuildFrame(FRotator(0.0, 90.0, 0.0), Board ? Board->GetBuildOrigin() : FVector::ZeroVector);
	if (!Board || !Construction->BeginPlacement(CourseEnds, BuildFrame, Board->GetLayoutScale(), Error))
	{
		Construction->EndSelection();
		GS->SetRoomPhaseFromServer(EMWRoomPhase::Failed);
		UE_LOG(LogMWServer, Error, TEXT("트랙 배치 시작 실패. %s"), *Error.ToString());
	}
}
