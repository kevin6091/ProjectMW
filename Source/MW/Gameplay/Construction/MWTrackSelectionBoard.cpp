#include "Gameplay/Construction/MWTrackSelectionBoard.h"
#include "Gameplay/Construction/MWTrackOfferPreview.h"
#include "Gameplay/Construction/MWPlayerConstructionComponent.h"
#include "Data/MWTrackPieceDefinition.h"
#include "System/MWAssetManager.h"
#include "System/MWPlayerState.h"
#include "Player/MWPlayerController.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

AMWTrackSelectionBoard::AMWTrackSelectionBoard()
{
    // 위치·색상·배지는 각 화면에서 구성하므로 표시용 Actor 자체를 복제하지 않는다.
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    // 세 후보를 나란히 놓을 기준점과 전체 선택판을 내려다보는 카메라를 구성한다.
    for (int32 I = 0; I < 3; ++I)
    {
        USceneComponent* Anchor = CreateDefaultSubobject<USceneComponent>(*FString::Printf(TEXT("Anchor%d"), I));
        Anchor->SetupAttachment(RootComponent);
        Anchor->SetRelativeLocation(FVector(0, (I - 1) * 1100.f, 80));
        Anchors.Add(Anchor);
    }
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("SelectionCamera"));
    Camera->SetupAttachment(RootComponent);
    const FVector CameraPosition(1800, -1900, 2900);
    Camera->SetRelativeLocation(CameraPosition);
    Camera->SetRelativeRotation((FVector(0, 0, 100) - CameraPosition).Rotation());
    Camera->FieldOfView = 65.f;
}

void AMWTrackSelectionBoard::BeginPlay()
{
    Super::BeginPlay();
    // 로컬 Controller만 카메라와 후보 표시를 관리한다. 원격 Controller에는 화면을 만들지 않는다.
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (AMWPlayerController* PC = Cast<AMWPlayerController>(It->Get()))
            if (PC->IsLocalController()) PC->RegisterTrackSelectionBoard(this);
}

void AMWTrackSelectionBoard::EndPlay(const EEndPlayReason::Type Reason)
{
    ClearOffers();
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (AMWPlayerController* PC = Cast<AMWPlayerController>(It->Get()))
            if (PC->IsLocalController()) PC->UnregisterTrackSelectionBoard(this);
    Super::EndPlay(Reason);
}

bool AMWTrackSelectionBoard::BuildOffers(const FMWTrackSelectionSession& Session, FText& OutError)
{
    ClearOffers();
    for (const FMWTrackOffer& Offer : Session.Offers)
    {
        UMWTrackPieceDefinition* Definition = UMWAssetManager::Get()
            .LoadAssetSynchronous<UMWTrackPieceDefinition>(Offer.DefinitionAssetId);
        if (!Definition || !Definition->Mesh || !Anchors.IsValidIndex(Offer.BoardSlotIndex))
        {
            OutError = FText::FromString(TEXT("후보 트랙을 준비할 수 없습니다."));
            ClearOffers();
            return false;
        }
        // 정의에 지정된 BP를 우선 사용하고, 없으면 AuthoredMesh를 지원하는 기본 클래스를 쓴다.
        UClass* Class = Definition->PreviewActorClass ? Definition->PreviewActorClass.Get() : AMWTrackOfferPreview::StaticClass();
        FActorSpawnParameters Params;
        Params.Owner = this;
        // 선택 화면용 표현이므로 월드 충돌에 따른 Spawn 거절을 설치 판정으로 사용하지 않는다.
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AMWTrackOfferPreview* Preview = GetWorld()->SpawnActor<AMWTrackOfferPreview>(
            Class, Anchors[Offer.BoardSlotIndex]->GetComponentTransform(), Params);
        if (!Preview)
        {
            OutError = FText::FromString(TEXT("후보 표시를 생성하지 못했습니다."));
            ClearOffers();
            return false;
        }
        // OfferId는 커서 판정의 반환값이고 Board 참조는 다른 선택판과 구분하는 데 사용한다.
        Preview->InitializeOffer(Offer.OfferId, Definition, this);
        Offers.Add(Offer.OfferId, Preview);
    }
    return true;
}

void AMWTrackSelectionBoard::ClearOffers()
{
    for (const auto& Pair : Offers) if (IsValid(Pair.Value)) Pair.Value->Destroy();
    Offers.Reset();
}

void AMWTrackSelectionBoard::RefreshMarkers(AMWPlayerController* Viewer,
    const FMWTrackSelectionSession& Session, int32 LocalHover)
{
    const AGameStateBase* GS = GetWorld()->GetGameState();
    if (!GS) return;
    TArray<AMWPlayerState*> Players;
    for (APlayerState* Base : GS->PlayerArray)
        if (AMWPlayerState* PS = Cast<AMWPlayerState>(Base)) Players.Add(PS);
    // PlayerArray의 수신 순서와 관계없이 배지 순서를 참가자 번호로 고정한다.
    Players.Sort([](const AMWPlayerState& A, const AMWPlayerState& B)
    { return A.GetLobbyMemberId() < B.GetLobbyMemberId(); });
    for (const auto& Pair : Offers)
    {
        FString Names;
        bool bLocalHover = false;
        bool bAnyConfirmed = false;
        for (AMWPlayerState* PS : Players)
        {
            const UMWPlayerConstructionComponent* Personal = PS->GetPlayerConstructionComponent();
            if (!Personal) continue;
            const FMWPlayerTrackSelection Selection = Personal->GetSelection();
            if (Selection.SelectionSessionId != Session.SelectionSessionId || Selection.OffersRevision != Session.OffersRevision) continue;
            const bool bSelf = Viewer->GetPlayerState<AMWPlayerState>() == PS;
            // 자신은 로컬 Hover로 지연 없이 표시하고, 확정 후에는 서버가 확정한 후보를 표시한다.
            const int32 Id = Selection.bConfirmed ? Selection.SelectedOfferId
                : (bSelf ? LocalHover : Selection.HoveredOfferId);
            if (Id != Pair.Key) continue;
            bLocalHover |= bSelf && !Selection.bConfirmed;
            bAnyConfirmed |= Selection.bConfirmed;
            Names += FString::Printf(TEXT("P%d %s\n"), PS->GetLobbyMemberId(),
                Selection.bConfirmed ? TEXT("SELECTED") : TEXT("HOVER"));
        }
        // 같은 후보에 여러 사람의 배지를 모은다. 누군가 선택해도 다른 사람의 선택을 막지 않는다.
        Pair.Value->RefreshAppearance(bLocalHover, bAnyConfirmed, FText::FromString(Names), Camera->GetComponentLocation());
    }
}
