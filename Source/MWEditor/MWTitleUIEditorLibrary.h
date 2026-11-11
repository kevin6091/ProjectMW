#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "MWTitleUIEditorLibrary.generated.h"

class UBlueprint;
class UWidget;
class UWidgetBlueprint;
class UWidgetTree;
class UWorld;
class APlayerController;
class UFont;
class UFontFace;

// Editor-only access to engine authoring APIs. Callers supply all assets and layout data.
UCLASS()
class MWEDITOR_API UMWTitleUIEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MW|Editor|Title")
	static UWidgetTree* GetWidgetTree(UWidgetBlueprint* Blueprint);

	UFUNCTION(BlueprintCallable, Category = "MW|Editor|Title")
	static bool SetWidgetTreeRoot(UWidgetBlueprint* Blueprint, UWidget* Root);

	UFUNCTION(BlueprintCallable, Category = "MW|Editor|Title")
	static UWidget* AddDesignerWidget(UWidgetBlueprint* Blueprint, TSubclassOf<UWidget> WidgetClass, FName WidgetName);

	/** 계층 재구성 후 제거된 위젯의 GUID를 정리하고 현재 위젯의 GUID를 보장한다. */
	UFUNCTION(BlueprintCallable, Category = "MW|Editor|UI")
	static void SynchronizeDesignerWidgetGuids(UWidgetBlueprint* Blueprint);

	UFUNCTION(BlueprintCallable, Category = "MW|Editor|Title")
	static UBlueprint* GetLevelBlueprint(UWorld* World);

	UFUNCTION(BlueprintCallable, Category = "MW|Editor|Title")
	static int32 ReplaceWidgetClassPins(UBlueprint* Blueprint, UClass* PreviousClass, UClass* ReplacementClass);

	UFUNCTION(BlueprintCallable, Category = "MW|Editor|Title")
	static FString ExportGraphs(UBlueprint* Blueprint);

	UFUNCTION(BlueprintCallable, Category = "MW|Editor|Title")
	static bool CompileBlueprint(UBlueprint* Blueprint);

	UFUNCTION(BlueprintCallable, Category = "MW|Editor|Title")
	static bool ResizePlayWindow(APlayerController* Controller, FIntPoint ViewportSize);

	UFUNCTION(BlueprintCallable, Category = "MW|Editor|Title")
	static bool SetSingleTypefaceFace(UFont* Font, UFontFace* Face);
};
