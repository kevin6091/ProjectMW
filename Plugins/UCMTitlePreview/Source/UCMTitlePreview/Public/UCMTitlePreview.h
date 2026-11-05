#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "Components/Button.h"
#include "GameFramework/Actor.h"
#include "UCMTitlePreview.generated.h"
class UFontFace; class UCanvasPanel; class ULevelSequence; class ULevelSequencePlayer; class ALevelSequenceActor;
USTRUCT(BlueprintType)
struct FUCMSourceUIElement {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") FName ElementName;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") FString SourceRef;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") TObjectPtr<UTexture2D> Texture;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") FString Text;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") FVector2D Position=FVector2D::ZeroVector;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") FVector2D Size=FVector2D(100,100);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") FLinearColor Color=FLinearColor::White;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") float FontSize=70;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") int32 FontOutlineSize=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") FVector2D TextShadowOffset=FVector2D::ZeroVector;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") float Rotation=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") bool bEnabled=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") bool bButton=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") FName SourceActionId;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") FLinearColor HighlightColor=FLinearColor::White;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") int32 SourceJustification=1;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") bool bSliced=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") float SourcePixelsPerUnit=1;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") FMargin SliceMargin;
 // Opt-in presentation of the existing title continue element; other source UI is unchanged.
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") bool bContinuePrompt=false;

};
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUCMPreviewActivated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUCMSourceAction,FName,Action);
class UUCMSourcedWidget;
UCLASS() class UCMTITLEPREVIEW_API UUCMSourceActionButton:public UButton {
 GENERATED_BODY()
public:
 UPROPERTY() FName SourceAction;
 UPROPERTY() TObjectPtr<UUCMSourcedWidget> SourceOwner;
 UFUNCTION() void HandleSourceClick();
 UFUNCTION() void HandleSourceHover();
};
/** Native widget tree is a source-parameterised UMG canvas. Typography remains an explicit approximation. */
UCLASS(Blueprintable)
class UCMTITLEPREVIEW_API UUCMSourcedWidget:public UUserWidget {
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") TArray<FUCMSourceUIElement> Elements;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") bool bSourceUITintsSRGB=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") float SourceFontPixelsToPoints=1;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") FVector2D SourceCanvasSize=FVector2D(3000,3000);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") TObjectPtr<UFontFace> SourceFontFace;
 UPROPERTY(BlueprintAssignable,Category="UCM Preview") FUCMPreviewActivated Activated;
 UFUNCTION(BlueprintCallable,Category="UCM Preview") void ActivatePreviewButton();
 UPROPERTY(BlueprintAssignable,Category="UCM Source") FUCMSourceAction SourceActionActivated;
 UPROPERTY(BlueprintAssignable,Category="UCM Source") FUCMSourceAction SourceActionHovered;
 UFUNCTION(BlueprintCallable,Category="UCM Source") void SelectSourceAction(FName Action);
 void HandleSourceAction(FName Action);
 UFUNCTION(BlueprintCallable,Category="UCM Source") FString GetSourceWidgetEvidence()const;
private:
 TArray<TWeakObjectPtr<UWidget>> SourceVisuals;
 FLinearColor SourceTint(FLinearColor C)const;

protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
};
UCLASS()
class UCMTITLEPREVIEW_API UUCMWorldWidgetComponent:public UWidgetComponent {
 GENERATED_BODY()
public:
 UUCMWorldWidgetComponent();
 // The menu uses its own pointer; other world widgets keep hardware input.
 void DisableHardwareInput() { bReceiveHardwareInput = false; }
};
USTRUCT(BlueprintType) struct FUCMSourceBindingOverride { GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") FGuid BindingId;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") FString ActorName;
};
USTRUCT(BlueprintType) struct FUCMSourceMenuEntry {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") FName ActionId;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") FTransform WorldWidgetTransform;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") FVector2D CanvasSize=FVector2D(400,90);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") FName SourceMethod;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") FName SourceDestination;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") FString SourceRef;
};
USTRUCT(BlueprintType) struct FUCMSourceCameraSample { GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") float Time=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") FTransform Pose;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUCMPreviewGameEntryRequested,FName,SourceMethod,FName,SourceDestination);
/** Preview-only choreography. It never signs in, loads a match, or changes any team system. */
UCLASS(Blueprintable)
class UCMTITLEPREVIEW_API AUCMTitleFlowPreview:public AActor {
 GENERATED_BODY()
public:
 AUCMTitleFlowPreview();
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="MW Integration") bool bStartAutomatically=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="MW Integration") bool bExternalMenuPresentation=false;
 DECLARE_MULTICAST_DELEGATE(FPresentationChanged);
 FPresentationChanged PresentationChanged;
 DECLARE_MULTICAST_DELEGATE_OneParam(FExternalMenuAction,FName);
 FExternalMenuAction ExternalMenuAction;
 void SetSourceMenuPresentation(bool bVisible,bool bInputEnabled);
 void RestoreTitlePose();
 bool bSuppressPreviewInput=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="UCM Preview") TObjectPtr<UUCMWorldWidgetComponent> TitleCanvas;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="UCM Preview") TObjectPtr<UUCMWorldWidgetComponent> MenuCanvas;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") TSubclassOf<UUserWidget> TitleWidgetClass;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") TSubclassOf<UUserWidget> MenuWidgetClass;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") TObjectPtr<ULevelSequence> IntroSequence;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") TObjectPtr<AActor> PreviewCamera;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") TObjectPtr<AActor> MenuCamera;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") TArray<FUCMSourceBindingOverride> SourceBindingOverrides;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") float SourceOpenSplashWait=17.8500003815f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") FString SourceCompanyLogo;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") FString SourceSplashScreen;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") FString SourceMainMenu;
 UPROPERTY(BlueprintReadOnly,Category="UCM Preview") int32 PreviewStage=0;
 UPROPERTY(BlueprintReadOnly,Category="UCM Preview") float SequenceFinishedAt=-1;
 UPROPERTY(BlueprintReadOnly,Category="UCM Preview") int32 IgnoredIntroInputs=0;
 UPROPERTY(BlueprintReadOnly,Category="UCM Preview") int32 GameEntryRequests=0;
 UPROPERTY(BlueprintReadOnly,Category="UCM Preview") TObjectPtr<ULevelSequencePlayer> SequencePlayer;
 UPROPERTY(BlueprintReadOnly,Category="UCM Preview") TArray<FString> PreviewEvents;
 UPROPERTY(BlueprintAssignable,Category="UCM Preview") FUCMPreviewGameEntryRequested GameEntryRequested;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") bool bEnableSourceFrontendMenus=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") bool bEnableSourceCameraTransition=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") TArray<FUCMSourceCameraSample> SourceTransitionForward;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") TArray<FUCMSourceCameraSample> SourceTransitionReverse;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") float SourceBrainBlendSeconds=.5f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") float SourceTransitionVerticalFOV=45;
 UPROPERTY(BlueprintReadOnly,Category="UCM Source") int32 IgnoredTransitionInputs=0;
 UPROPERTY(BlueprintReadOnly,Category="UCM Source") float CameraTransitionTime=0;

 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UCM Source") TArray<FUCMSourceMenuEntry> SourceMenuEntries;
 UPROPERTY(BlueprintReadOnly,Category="UCM Source") int32 SelectedSourceMenu=0;
 UPROPERTY(BlueprintReadOnly,Category="UCM Source") int32 SuppressedDuplicateInputs=0;
 UPROPERTY(BlueprintReadOnly,Category="UCM Source") FName LastSourceMenuRequest;
 UPROPERTY(BlueprintAssignable,Category="UCM Source") FUCMPreviewGameEntryRequested MenuActionRequested;
 UFUNCTION(BlueprintCallable,Category="UCM Source") void MoveSourceMenu(int32 Direction);
 UFUNCTION(BlueprintCallable,Category="UCM Source") void ReturnToSourceTitle();
 UFUNCTION() void OnSourceMenuAction(FName Action);
 UFUNCTION() void OnSourceMenuHover(FName Action);
 UFUNCTION(BlueprintCallable,Category="UCM Preview") void AcceptPreviewInput();
 UFUNCTION(BlueprintCallable,Category="UCM Preview") void IntroFinished();
 UFUNCTION(BlueprintCallable,Category="UCM Preview") FString GetPreviewEvidence() const;
 UFUNCTION(BlueprintCallable,Category="UCM Preview") void ReplayPreview();
protected:
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaTime) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:

 bool bRestoringTitlePose=false;
 UPROPERTY() TObjectPtr<ALevelSequenceActor> SequenceActor;
 UPROPERTY() TArray<TObjectPtr<UUCMWorldWidgetComponent>> SourceMenuCanvases;
 TWeakObjectPtr<UUCMWorldWidgetComponent> PressedSourceMenu;
 TWeakObjectPtr<UUCMWorldWidgetComponent> HoveredSourceMenu;
 FVector2D LastSourceMousePosition=FVector2D::ZeroVector;
 bool bHaveSourceMousePosition=false;
 double StartedAt=0;
 uint64 LastAcceptedInputFrame=MAX_uint64;
 bool bCameraTransitionActive=false;
 bool bCameraTransitionReverse=false;
 FTransform TransitionOutgoing;
 float TransitionOutgoingFOV=0;
 void BeginSourceCameraTransition(bool bReverse);
 void UpdateSourceCameraTransition(float DeltaTime);
 void RefreshSourceMenu();
 void UpdateSourceMenuPointer(APlayerController* PC);
 void Record(const FString& Event);
};
