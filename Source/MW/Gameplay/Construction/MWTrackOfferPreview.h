#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MWTrackOfferPreview.generated.h"

class UBoxComponent;
class UTextRenderComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UMWTrackPieceDefinition;
class AMWTrackSelectionBoard;

UENUM(BlueprintType)
enum class EMWTrackPreviewShape : uint8 { AuthoredMesh, Straight, Curve, Ramp };

// 로컬 선택판 전용. 실제 코스 도로로 등록하거나 복제하지 않는다.
UCLASS()
class MW_API AMWTrackOfferPreview : public AActor
{
    GENERATED_BODY()
public:
    AMWTrackOfferPreview();
    virtual void OnConstruction(const FTransform& Transform) override;
    void InitializeOffer(int32 Id, UMWTrackPieceDefinition* Definition, AMWTrackSelectionBoard* Board);
    void RefreshAppearance(bool bHovered, bool bConfirmed, const FText& Players, const FVector& CameraLocation);
    int32 GetOfferId() const { return OfferId; }
    AMWTrackSelectionBoard* GetBoard() const { return OwningBoard.Get(); }

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Preview")
    EMWTrackPreviewShape PreviewShape = EMWTrackPreviewShape::AuthoredMesh;
    UPROPERTY(EditDefaultsOnly, Category="Preview")
    TObjectPtr<UStaticMesh> SourceMesh;
    UPROPERTY(EditDefaultsOnly, Category="Preview")
    TObjectPtr<UMaterialInterface> PreviewMaterial;
    UPROPERTY(VisibleAnywhere, Category="Preview")
    TObjectPtr<UBoxComponent> SelectionBox;
    UPROPERTY(VisibleAnywhere, Category="Preview")
    TObjectPtr<UTextRenderComponent> Marker;
    UPROPERTY(VisibleAnywhere, Category="Preview")
    TArray<TObjectPtr<UStaticMeshComponent>> Sections;
private:
    void RebuildGeometry();
    int32 OfferId = INDEX_NONE;
    FText DisplayName;
    FTransform AuthoredPreviewTransform = FTransform::Identity;
    TWeakObjectPtr<AMWTrackSelectionBoard> OwningBoard;
};
