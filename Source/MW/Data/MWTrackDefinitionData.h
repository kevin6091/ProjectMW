#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MWTrackDefinitionData.generated.h"

class AActor;

UCLASS(BlueprintType)
class MW_API UMWTrackDefinitionData : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintPure, Category = "MW|Track")
	FText GetDisplayName() const { return DisplayName; }
	
	UFUNCTION(BlueprintPure, Category = "MW|Track")
	TSubclassOf<AActor> GetTrackActorClass() const { return TrackActorClass; }
	
	bool ValidateDefinition(FText& OutError) const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AActor> TrackActorClass;
};
