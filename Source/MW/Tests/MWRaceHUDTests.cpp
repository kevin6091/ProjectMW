#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"
#include "UnrealClient.h"
#include "System/GameState/MWGameplayGameState.h"
#include "UI/Gameplay/MWGameplayHUDSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWRaceHUDStateLifetimeTest, "MW.UI.RaceHUD.StateLifetime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMWRaceHUDStateLifetimeTest::RunTest(const FString& Parameters)
{
    // 화면·온라인 연결 없이 실제 GameState 이벤트와 World Subsystem 수명만 검사한다.
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    World->SetBegunPlay(true);

    ON_SCOPE_EXIT
    {
        World->EndPlay(EEndPlayReason::Quit);
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    };

    UMWGameplayHUDSubsystem* HUD = World->GetSubsystem<UMWGameplayHUDSubsystem>();
    if (!TestNotNull(TEXT("경기 HUD Subsystem 생성"), HUD))
    {
        return false;
    }

    AMWGameplayGameState* First = World->SpawnActor<AMWGameplayGameState>();
    World->SetGameState(First);

    // HUD 연결 전에 이미 바뀐 값도 초기 조회로 받아야 한다.
    {
        FMWGameplayProgress Progress = First->GetProgress();
        Progress.Phase = EMWRoomPhase::TrackPlacement;
        First->ApplyProgressFromServer(Progress);
    }
    HUD->Tick(0.f);
    TestTrue(TEXT("연결 전 현재 단계 반영"), HUD->GetObservedRoomPhase() == EMWRoomPhase::TrackPlacement);

    const FName Handler(TEXT("HandleGameplayStateChanged"));
    TestTrue(TEXT("실제 GameState 이벤트 구독"), First->OnGameplayStateChanged.Contains(HUD, Handler));

    {
        FMWGameplayProgress Progress = First->GetProgress();
        Progress.Phase = EMWRoomPhase::Racing;
        First->ApplyProgressFromServer(Progress);
    }
    TestTrue(TEXT("서버 setter 이벤트로 갱신"), HUD->GetObservedRoomPhase() == EMWRoomPhase::Racing);

    AMWGameplayGameState* Second = World->SpawnActor<AMWGameplayGameState>();
    World->SetGameState(Second);
    HUD->Tick(0.f);
    TestFalse(TEXT("이전 GameState 구독 해제"), First->OnGameplayStateChanged.Contains(HUD, Handler));
    TestTrue(TEXT("새 GameState 초기 상태 반영"), HUD->GetObservedRoomPhase() == EMWRoomPhase::PreparingConstruction);

    {
        FMWGameplayProgress Progress = First->GetProgress();
        Progress.Phase = EMWRoomPhase::Failed;
        First->ApplyProgressFromServer(Progress);
    }
    TestTrue(TEXT("이전 객체 이벤트 무시"), HUD->GetObservedRoomPhase() == EMWRoomPhase::PreparingConstruction);

    World->SetGameState(nullptr);
    TestTrue(TEXT("오버레이 준비 전에도 상태 제거를 감지"), HUD->IsTickable());
    HUD->Tick(0.f);
    TestFalse(TEXT("원본과 오버레이가 없으면 연결 틱 중단"), HUD->IsTickable());
    TestFalse(TEXT("GameState 부재 시 표시 값 제거"), HUD->GetObservedRoomPhase().IsSet());
    TestFalse(TEXT("제거된 GameState 구독 해제"), Second->OnGameplayStateChanged.Contains(HUD, Handler));

    World->SetGameState(Second);
    HUD->Tick(0.f);
    World->EndPlay(EEndPlayReason::Quit);
    TestFalse(TEXT("월드 종료 시 구독 해제"), Second->OnGameplayStateChanged.Contains(HUD, Handler));
    TestFalse(TEXT("월드 종료 시 표시 값 제거"), HUD->GetObservedRoomPhase().IsSet());
    return true;
}

// 이 검사는 사용자가 준비한 PIE의 단계를 바꾼다. 일반 무인 회귀 검사와 분리한다.
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FMWRaceHUDPIEPhaseTest, "MW.UI.RaceHUD.PIEPhase",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::RequiresUser)

class FMWWaitForPIEPhase : public IAutomationLatentCommand
{
public:
    FMWWaitForPIEPhase(FAutomationTestBase* InTest, EMWRoomPhase InPhase, const FString& InName)
        : Test(InTest)
        , Phase(InPhase)
        , Name(InName)
        , StartedAt(FPlatformTime::Seconds())
    {
    }

    virtual bool Update() override
    {
        const double Now = FPlatformTime::Seconds();
        if (AppliedAt.IsSet())
        {
            // 다음 Slate 프레임이 실제 문구를 그린 뒤 캡처하고 검사를 끝낸다.
            if (!bCaptured && Now - AppliedAt.GetValue() >= 0.2)
            {
                FString Directory;
                if (FParse::Value(FCommandLine::Get(), TEXT("MWRaceHUDCaptureDir="), Directory))
                {
                    FScreenshotRequest::RequestScreenshot(FPaths::Combine(Directory, Name + TEXT(".png")), true, false);
                }
                bCaptured = true;
            }
            return Now - AppliedAt.GetValue() >= 0.5;
        }

        // Editor 자동화 준비가 기존 PIE를 종료할 수 있어 새 PIE 준비까지 기다린다.
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            AMWGameplayGameState* State = World ? World->GetGameState<AMWGameplayGameState>() : nullptr;
            if (Context.WorldType != EWorldType::PIE || !IsValid(State) || !State->HasAuthority())
            {
                continue;
            }

            const UMWGameplayHUDSubsystem* HUD = World->GetSubsystem<UMWGameplayHUDSubsystem>();
            if (!HUD || !HUD->GetObservedRoomPhase().IsSet())
            {
                continue;
            }

            {
                FMWGameplayProgress Progress = State->GetProgress();
                Progress.Phase = Phase;
                State->ApplyProgressFromServer(Progress);
            }
            Test->TestTrue(TEXT("실제 PIE 상태 이벤트 수신"), HUD->GetObservedRoomPhase() == Phase);
            AppliedAt = Now;
            return false;
        }

        if (Now - StartedAt >= 15.0)
        {
            Test->AddError(TEXT("15초 안에 MWGameplayGameState와 HUD가 준비된 PIE를 시작하지 못했습니다."));
            return true;
        }
        return false;
    }

private:
    friend class FMWRaceHUDPIEPhaseTest;

    struct FMWRaceHUDPhaseCase
    {
        const TCHAR* Name;

        EMWRoomPhase Phase;
    };

    static constexpr FMWRaceHUDPhaseCase PhaseCases[] =
    {
        { TEXT("SelectingTrack"), EMWRoomPhase::PreparingConstruction },
        { TEXT("PlacingTrack"), EMWRoomPhase::TrackPlacement },
        { TEXT("PlacingElements"), EMWRoomPhase::ElementPlacement },
        { TEXT("PreparingRace"), EMWRoomPhase::PreparingRace },
        { TEXT("Countdown"), EMWRoomPhase::Countdown },
        { TEXT("Racing"), EMWRoomPhase::Racing },
        { TEXT("Failed"), EMWRoomPhase::Failed },
        { TEXT("LobbyHidden"), EMWRoomPhase::Lobby },
    };

    // 프레임워크가 잠재 명령 실행 동안 소유하는 검사 객체이며 UObject 참조가 아니다.
    FAutomationTestBase* Test;

    EMWRoomPhase Phase;

    FString Name;

    double StartedAt;

    TOptional<double> AppliedAt;

    bool bCaptured = false;
};

void FMWRaceHUDPIEPhaseTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
    for (const auto& TestCase : FMWWaitForPIEPhase::PhaseCases)
    {
        Names.Add(TestCase.Name);
        Commands.Add(TestCase.Name);
    }
}

bool FMWRaceHUDPIEPhaseTest::RunTest(const FString& Parameters)
{
    for (const auto& TestCase : FMWWaitForPIEPhase::PhaseCases)
    {
        if (Parameters == TestCase.Name)
        {
            ADD_LATENT_AUTOMATION_COMMAND(FMWWaitForPIEPhase(this, TestCase.Phase, Parameters));
            return true;
        }
    }

    AddError(TEXT("알 수 없는 경기 단계 검사입니다."));
    return false;
}

#endif
