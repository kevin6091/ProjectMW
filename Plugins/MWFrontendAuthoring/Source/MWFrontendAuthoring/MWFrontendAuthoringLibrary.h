#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "MWFrontendAuthoringLibrary.generated.h"

class UUserWidget;
class APlayerController;

/** Explicitly invoked authoring command; never modifies assets during module load. */
UCLASS()
class UMWFrontendAuthoringLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="MW|Frontend|Editor") static bool BuildNewWidgets();
	UFUNCTION(BlueprintCallable, Category="MW|Frontend|Editor") static bool BuildTestPreviewWidget();
	UFUNCTION(BlueprintCallable, Category="MW|Frontend|Editor") static UUserWidget* ShowTestPreview(APlayerController* Controller);
};
