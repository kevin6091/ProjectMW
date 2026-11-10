#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "MWTrackVisualLibrary.generated.h"

class AActor;

UCLASS()
class MW_API UMWTrackVisualLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Switches the visual mode of a track instance. Call on the local preview actor.
	 * Returns whether the requested mode is available. Without a dedicated wireframe,
	 * a wireframe request shows the normal meshes and returns false.
	 */
	UFUNCTION(BlueprintCallable, Category = "MW|Track|Visual")
	static bool SetTrackWireframePreview(AActor* TrackVisual, bool bEnableWireframe);
};
