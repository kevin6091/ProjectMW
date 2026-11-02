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
    // 메시 대신 단순 Box로 커서를 받는다. Overlap 이벤트나 주행 물리 충돌은 발생시키지 않는다.
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
    // BP의 기본 구성요소로 미리 확보하고 형상에 필요한 구간만 활성화한다.
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
    // 인접한 두 점을 하나의 메시 구간으로 연결한다. 경로 데이터는 테스트 화면용이다.
    TArray<FVector> Points;
    if (PreviewShape == EMWTrackPreviewShape::Curve)
    {
        // 반지름 350의 90도 커브를 여덟 직선 구간으로 근사한다.
        for (int32 I = 0; I <= 8; ++I)
        {
            const double A = static_cast<double>(I) / 8.0 * UE_HALF_PI;
            Points.Add(FVector(350.0 * FMath::Sin(A) - 175.0, 350.0 * (1.0 - FMath::Cos(A)) - 175.0, 0.0));
        }
    }
    else if (PreviewShape == EMWTrackPreviewShape::Ramp)
    {
        // 평지 → 상승 구간 → 높은 평지 순서로 경사 형태를 표시한다.
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
        // 작성된 메시에는 정의의 변환을 적용한다. 테스트 Cube는 길이·중심·방향에 맞춰 늘린다.
        // Cube 기본 길이 100을 기준으로 스케일을 구하고, 3만큼 겹쳐 구간 사이 틈을 줄인다.
        Section->SetRelativeTransform(bAuthored ? AuthoredPreviewTransform : FTransform(
            Delta.Rotation(), (Points[I] + Points[I + 1]) * 0.5 + FVector(0, 0, 15),
            FVector((Delta.Size() + 3.f) / 100.f, 2.6f, 0.3f)));
        if (PreviewMaterial) Section->SetMaterial(0, PreviewMaterial);
        Bounds += Section->CalcBounds(Section->GetRelativeTransform()).GetBox();
    }
    // 전체 형상을 감싸는 선택 영역을 만든다. 커브 내부 빈 공간까지 포함하는 단순 외곽 Box다.
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
    // 확정 표시를 Hover보다 우선한다. 금색=누군가 확정, 파란색=자기 Hover, 기본색=그 외.
    const FLinearColor Color = bConfirmed ? FLinearColor(1.f, 0.65f, 0.08f)
        : (bHovered ? FLinearColor(0.05f, 0.75f, 1.f) : FLinearColor(0.38f, 0.48f, 0.65f));
    for (UStaticMeshComponent* Section : Sections)
    {
        if (!Section->IsVisible()) continue;
        // 공유 원본 머티리얼은 바꾸지 않고 이 구간의 동적 인스턴스 색상만 갱신한다.
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
