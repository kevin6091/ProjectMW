#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MWTrackSelectionWidget.generated.h"

class UTextBlock;

UCLASS()
class MW_API UMWTrackSelectionWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeOnInitialized() override;
    void SetSelectionText(const FText& Players, const FText& Status);
private:
    UPROPERTY(Transient) TObjectPtr<UTextBlock> PlayersText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
};
