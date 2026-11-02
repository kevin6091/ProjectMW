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
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
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
        UClass* Class = Definition->PreviewActorClass ? Definition->PreviewActorClass.Get() : AMWTrackOfferPreview::StaticClass();
        FActorSpawnParameters Params;
        Params.Owner = this;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AMWTrackOfferPreview* Preview = GetWorld()->SpawnActor<AMWTrackOfferPreview>(
            Class, Anchors[Offer.BoardSlotIndex]->GetComponentTransform(), Params);
        if (!Preview)
        {
            OutError = FText::FromString(TEXT("후보 표시를 생성하지 못했습니다."));
            ClearOffers();
            return false;
        }
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
            const int32 Id = Selection.bConfirmed ? Selection.SelectedOfferId
                : (bSelf ? LocalHover : Selection.HoveredOfferId);
            if (Id != Pair.Key) continue;
            bLocalHover |= bSelf && !Selection.bConfirmed;
            bAnyConfirmed |= Selection.bConfirmed;
            Names += FString::Printf(TEXT("P%d %s\n"), PS->GetLobbyMemberId(),
                Selection.bConfirmed ? TEXT("SELECTED") : TEXT("HOVER"));
        }
        Pair.Value->RefreshAppearance(bLocalHover, bAnyConfirmed, FText::FromString(Names), Camera->GetComponentLocation());
    }
}
