#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "System/MWRoomTypes.h"
#include "MWArrivalScreen.generated.h"

class FMWArrivalFrame;
class SBorder;
class UMaterialInterface;
class AMWGameplayPlayerController;

// 각 게임 인스턴스가 자기 뷰포트의 출발 가림을 목적지 첫 장면까지 유지한다.
UCLASS(Config=Game)
class MW_API UMWArrivalScreen : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    FName GetLoadingPackage() const { return LoadingWidgetClass.ToSoftObjectPath().GetLongPackageFName(); }
    TSharedRef<SWidget> CreateLoadingContent();
    void Begin(APlayerController& Controller, FName Destination);
    void Cancel();
    virtual void Deinitialize() override;
    bool IsCovered() const { return Cover.IsValid(); }

    DECLARE_MULTICAST_DELEGATE_ThreeParams(FCollectEnvironment, UWorld&, TArray<UMaterialInterface*>&, FString&);
    FCollectEnvironment CollectEnvironment;
private:
    UPROPERTY(Config) TSoftClassPtr<class UUserWidget> LoadingWidgetClass;

    // 설치된 추가 밉을 잠시 기다린다. 기본 리소스와 실제 첫 프레임 확인은 별도로 유지한다.
    UPROPERTY(Config, meta = (ClampMin = "0", Units = "s"))
    float FirstViewQualityWaitSeconds = 2.f;

    static void CheckMaterials(UWorld& World, const TArray<UMaterialInterface*>& Materials,
        FString& Pending, FString& QualityPending);
    enum class EFailureState : uint8
    {
        None,
        MapTravelTimeout,
        NeedsUserAction
    };

    bool Tick(float Delta);
    void HandleConnectionFailure(UWorld* World, const FString& Reason);
    void RestartPreparation(APlayerController* Controller);
    void ShowFailure(bool bCanRetry = true, const FText& Message = FText::GetEmpty(),
                     EFailureState NewFailure = EFailureState::NeedsUserAction);
    FDelegateHandle TravelFailure;
    FDelegateHandle NetworkFailure;
    TSharedPtr<SBorder> Cover;
    TWeakObjectPtr<UGameViewportClient> Viewport;
    TWeakObjectPtr<UWorld> Source;
    FName ExpectedMap;

    // 월드가 정지하거나 바뀌어도 출발 가림부터 목적지까지의 시간은 계속 잰다.
    UPROPERTY(Config, meta = (ClampMin = "1", Units = "s"))
    float MapTravelTimeoutSeconds = 45.f;
    TOptional<double> MapTravelStartedAt;
    FGuid Attempt;
    FString Pending;
    double VisibleSeconds = 0;
    TOptional<double> QualityWaitStartedAt;
    EFailureState Failure = EFailureState::None;
    FTSTicker::FDelegateHandle Ticker;
    TSharedPtr<FMWArrivalFrame, ESPMode::ThreadSafe> Frame;
};
