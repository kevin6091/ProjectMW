#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MWTrackPieceDefinition.generated.h"

class UStaticMesh;
class AMWTrackOfferPreview;

UCLASS(BlueprintType)
class MW_API UMWTrackPieceDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Track")
    FName PieceId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Track")
    FText DisplayName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Track")
    TObjectPtr<UStaticMesh> Mesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Preview")
    TSubclassOf<AMWTrackOfferPreview> PreviewActorClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Preview")
    FTransform PreviewRelativeTransform;
};
