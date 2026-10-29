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
	/** Creates an unsaved authenticated Editor fixture from the current production tree. */
	UFUNCTION(BlueprintCallable, Category="MW|Frontend|Editor") static UUserWidget* CreateTransientTestWidget(APlayerController* Controller);
	UFUNCTION(BlueprintCallable, Category="MW|Frontend|Editor") static FString ExportWidgetLogic(const FString& AssetPath);
	UFUNCTION(BlueprintCallable, Category="MW|Frontend|Editor") static bool ApplyLoginPresentation();
	/** PIE fixture only: exercise the unchanged login Blueprint without account credentials. */
	UFUNCTION(BlueprintCallable, Category="MW|Frontend|Test") static bool SetLoginProgressForTest(UUserWidget* Login, bool bInProgress);
	UFUNCTION(BlueprintCallable, Category="MW|Frontend|Editor") static bool BuildNewWidgets();
	UFUNCTION(BlueprintCallable, Category="MW|Frontend|Editor") static void BeginTestPreview();
	UFUNCTION(BlueprintCallable, Category="MW|Frontend|Editor") static void SetTestPreviewSize(APlayerController* Controller, int32 Width, int32 Height);
	UFUNCTION(BlueprintCallable, Category="MW|Frontend|Editor") static bool BuildTestPreviewWidget();
	UFUNCTION(BlueprintCallable, Category="MW|Frontend|Editor") static UUserWidget* ShowTestPreview(APlayerController* Controller);
};
