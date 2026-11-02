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

// AuthoredMesh는 정의의 메시를 사용한다. 나머지는 Cube 구간으로 만든 테스트 형상이다.
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
    // 커서의 Visibility 검사에만 반응한다. 실제 도로의 물리 충돌이나 설치 간섭용이 아니다.
    UPROPERTY(VisibleAnywhere, Category="Preview")
    TObjectPtr<UBoxComponent> SelectionBox;
    UPROPERTY(VisibleAnywhere, Category="Preview")
    TObjectPtr<UTextRenderComponent> Marker;
    // 필요한 구간만 표시한다. 선택 변화마다 컴포넌트를 새로 만들지 않는다.
    UPROPERTY(VisibleAnywhere, Category="Preview")
    TArray<TObjectPtr<UStaticMeshComponent>> Sections;
private:
    void RebuildGeometry();
    int32 OfferId = INDEX_NONE;
    FText DisplayName;
    FTransform AuthoredPreviewTransform = FTransform::Identity;
    TWeakObjectPtr<AMWTrackSelectionBoard> OwningBoard;
};
