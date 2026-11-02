#include "Gameplay/Construction/MWTrackOfferPreview.h"
#include "Gameplay/Construction/MWTrackSelectionBoard.h"
#include "Data/MWTrackPieceDefinition.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AMWTrackOfferPreview::AMWTrackOfferPreview()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SelectionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("SelectionBox"));
    SelectionBox->SetupAttachment(RootComponent);
    SelectionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    SelectionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    SelectionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    SelectionBox->SetGenerateOverlapEvents(false);
    Marker = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Marker"));
    Marker->SetupAttachment(RootComponent);
    Marker->SetHorizontalAlignment(EHTA_Center);
    Marker->SetWorldSize(40.f);
    Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    SourceMesh = Cube.Object;
    for (int32 I = 0; I < 12; ++I)
    {
        UStaticMeshComponent* Section = CreateDefaultSubobject<UStaticMeshComponent>(
            *FString::Printf(TEXT("Section%d"), I));
        Section->SetupAttachment(RootComponent);
        Section->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Section->SetGenerateOverlapEvents(false);
        Sections.Add(Section);
    }
}

void AMWTrackOfferPreview::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RebuildGeometry();
}

void AMWTrackOfferPreview::RebuildGeometry()
{
    TArray<FVector> Points;
    if (PreviewShape == EMWTrackPreviewShape::Curve)
    {
        for (int32 I = 0; I <= 8; ++I)
        {
            const double A = static_cast<double>(I) / 8.0 * UE_HALF_PI;
            Points.Add(FVector(350.0 * FMath::Sin(A) - 175.0, 350.0 * (1.0 - FMath::Cos(A)) - 175.0, 0.0));
        }
    }
    else if (PreviewShape == EMWTrackPreviewShape::Ramp)
    {
        Points = { FVector(-400, 0, 0), FVector(-200, 0, 0), FVector(200, 0, 150), FVector(400, 0, 150) };
    }
    else Points = { FVector(-400, 0, 0), FVector(400, 0, 0) };

    FBox Bounds(ForceInit);
    for (int32 I = 0; I < Sections.Num(); ++I)
    {
        UStaticMeshComponent* Section = Sections[I];
        const bool bUsed = SourceMesh && I < Points.Num() - 1;
        Section->SetVisibility(bUsed);
        Section->SetStaticMesh(bUsed ? SourceMesh.Get() : nullptr);
        if (!bUsed) continue;
        const FVector Delta = Points[I + 1] - Points[I];
        const bool bAuthored = PreviewShape == EMWTrackPreviewShape::AuthoredMesh;
        Section->SetRelativeTransform(bAuthored ? AuthoredPreviewTransform : FTransform(
            Delta.Rotation(), (Points[I] + Points[I + 1]) * 0.5 + FVector(0, 0, 15),
            FVector((Delta.Size() + 3.f) / 100.f, 2.6f, 0.3f)));
        if (PreviewMaterial) Section->SetMaterial(0, PreviewMaterial);
        Bounds += Section->CalcBounds(Section->GetRelativeTransform()).GetBox();
    }
    if (Bounds.IsValid)
    {
        SelectionBox->SetRelativeLocation(Bounds.GetCenter());
        SelectionBox->SetBoxExtent(Bounds.GetExtent() + FVector(20));
        Marker->SetRelativeLocation(FVector(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Max.Z + 110));
    }
}

void AMWTrackOfferPreview::InitializeOffer(int32 Id, UMWTrackPieceDefinition* Definition, AMWTrackSelectionBoard* Board)
{
    OfferId = Id;
    OwningBoard = Board;
    DisplayName = Definition->DisplayName;
    AuthoredPreviewTransform = Definition->PreviewRelativeTransform;
    if (PreviewShape == EMWTrackPreviewShape::AuthoredMesh) SourceMesh = Definition->Mesh;
    RebuildGeometry();
}

void AMWTrackOfferPreview::RefreshAppearance(bool bHovered, bool bConfirmed,
    const FText& Players, const FVector& CameraLocation)
{
    const FLinearColor Color = bConfirmed ? FLinearColor(1.f, 0.65f, 0.08f)
        : (bHovered ? FLinearColor(0.05f, 0.75f, 1.f) : FLinearColor(0.38f, 0.48f, 0.65f));
    for (UStaticMeshComponent* Section : Sections)
    {
        if (!Section->IsVisible()) continue;
        UMaterialInstanceDynamic* MI = Cast<UMaterialInstanceDynamic>(Section->GetMaterial(0));
        if (!MI) MI = Section->CreateDynamicMaterialInstance(0);
        if (MI) MI->SetVectorParameterValue(TEXT("Color"), Color);
    }
    // 기본 거리장 폰트로 표시할 월드 배지는 ASCII를 사용하고, 전체 이름은 UMG에 표시한다.
    const TCHAR* ShapeName = PreviewShape == EMWTrackPreviewShape::Curve ? TEXT("CURVE")
        : (PreviewShape == EMWTrackPreviewShape::Ramp ? TEXT("RAMP") : TEXT("STRAIGHT"));
    Marker->SetText(FText::Format(FText::FromString(TEXT("{0}\n{1}")), FText::FromString(ShapeName), Players));
    Marker->SetTextRenderColor(bConfirmed ? FColor::Yellow : FColor::White);
    Marker->SetWorldRotation((CameraLocation - Marker->GetComponentLocation()).Rotation());
}
