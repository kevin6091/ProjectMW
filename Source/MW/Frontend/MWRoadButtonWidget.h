#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MWRoadButtonWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMWRoadButtonClicked);

/** Shared UMG button, styled with the original extracted Make Way sign sprite. */
UCLASS(Abstract)
class MW_API UMWRoadButtonWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="MW|Frontend") int32 LabelFontSize = 36;
	UPROPERTY(EditAnywhere, Category="MW|Frontend") float IconSize = 54.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MW|Frontend") FText LabelText;
	UPROPERTY(BlueprintAssignable, Category="MW|Frontend") FMWRoadButtonClicked OnClicked;
	void SetLabel(const FText& Text);
	void SetIconAngle(float Angle);
	void FocusButton(APlayerController* Controller);
protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> ActionButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Label;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> Arrow;
private:
	UFUNCTION() void Click();
	UFUNCTION() void Hover();
	UFUNCTION() void Unhover();
	UFUNCTION() void Press();
	UFUNCTION() void Release();
};
