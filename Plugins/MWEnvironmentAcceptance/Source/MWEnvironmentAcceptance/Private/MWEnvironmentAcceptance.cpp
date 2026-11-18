// 명시적으로 요청한 배경 검증만 실행하는 도구다. 일반 게임에 자동 합격 판정을 넣지 않는다.

#include "Modules/ModuleManager.h"
#include "MWEnvFlowProbe.h"
#include "MWEnvironmentRestore.h"
#include "Camera/CameraActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HighResScreenshot.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

class FMWEnvironmentAcceptance : public IModuleInterface
{
    FTSTicker::FDelegateHandle Handle;
    TWeakObjectPtr<AMWEnvironmentGenerator> Generator;
    TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Initial;
    FVector Start;
    TOptional<FVector> NextCameraLocation;
    bool bResetNextTick = false;
    FString Theme, Output;
    int32 Phase = 0, Frame = 0, Move = 0, TotalTicks = 0;

    TSharedPtr<FJsonObject> State() const
    {
        TSharedPtr<FJsonObject> Object;
        auto Reader = TJsonReaderFactory<>::Create(Generator->GetEnvironmentDebug());
        check(FJsonSerializer::Deserialize(Reader, Object));
        return Object;
    }

    TMap<FString, FString> Placements(const TSharedPtr<FJsonObject>& Object) const
    {
        TMap<FString, FString> Map;
        for (const auto& Entry : Object->GetArrayField(TEXT("generated")))
        {
            const auto& Tile = Entry->AsObject();
            Map.Add(FString::Printf(TEXT("%d,%d"), int32(Tile->GetNumberField(TEXT("x"))),
                                    int32(Tile->GetNumberField(TEXT("y")))),
                    Tile->GetStringField(TEXT("placement_crc")));
        }
        return Map;
    }

    // 선택한 검증의 화면·타이머 상태를 정리하고 실행 결과를 남긴다.
    bool Finish(const FString& Error = FString())
    {
        Report->SetStringField(TEXT("theme"), Theme);
        Report->SetStringField(TEXT("error"), Error);
        Report->SetBoolField(TEXT("passed"), Error.IsEmpty());
        FString Text;
        auto Writer = TJsonWriterFactory<>::Create(&Text);
        FJsonSerializer::Serialize(Report.ToSharedRef(), Writer);
        FFileHelper::SaveStringToFile(Text, *(Output / (Theme + TEXT("_Runtime.json"))));
        UE_LOG(LogTemp, Display, TEXT("MW_ENV_RUNTIME_ACCEPTANCE %s passed=%d error=%s"), *Theme, Error.IsEmpty(),
               *Error);
        FPlatformMisc::RequestExitWithStatus(false, Error.IsEmpty() ? 0 : 1);
        return false;
    }

    // 현재 테마의 실제 검증 화면을 지정한 결과 경로에 저장한다.
    void Capture(const FString& Label)
    {
        FScreenshotRequest::RequestScreenshot(Output / (Theme + TEXT("_") + Label + TEXT(".png")), false, false);
    }

    // 명시적으로 시작한 검증만 단계별로 진행하고 준비 실패·기한을 확인한다.
    bool Tick(float)
    {
        if (++TotalTicks > 1200)
            return Finish(TEXT("Smoke timeout"));
        if (!Generator.IsValid())
        {
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                UWorld* World = Context.World();
                if (!World || !World->IsGameWorld() || !World->HasBegunPlay())
                    continue;
                for (TActorIterator<AMWEnvironmentGenerator> It(World); It; ++It)
                {
                    Generator = *It;
                    break;
                }
            }
            if (!Generator.IsValid())
                return true;
            if (!Generator->Theme || !Generator->ViewCamera)
                return Finish(TEXT("Missing theme or camera"));
            Start = Generator->ViewCamera->GetActorLocation();
            if (auto* Controller = Generator->GetWorld()->GetFirstPlayerController())
                Controller->SetViewTarget(Generator->ViewCamera);
            Generator->AutoUpdate = false;
            Generator->Seed = 42;
            Generator->InitializeEnvironment();
            const auto Registration = State();
            Report->SetObjectField(TEXT("registration"), Registration);
            if (Registration->GetNumberField(TEXT("registered")) != 121 ||
                !Registration->GetArrayField(TEXT("generated")).IsEmpty())
                return Finish(TEXT("Initial registration"));
        }
        if (NextCameraLocation.IsSet())
        {
            Generator->ViewCamera->SetActorLocation(NextCameraLocation.GetValue());
            NextCameraLocation.Reset();
        }
        if (bResetNextTick)
        {
            Generator->InitializeEnvironment();
            bResetNextTick = false;
        }
        Generator->UpdateVisibility();
        ++Frame;
        if (Frame < 12)
            return true;
        Frame = 0;
        const auto Current = State();
        if (Phase == 0)
        {
            Initial = Current;
            if (Current->GetNumberField(TEXT("active")) <= 0)
                return Finish(TEXT("No initial features"));
            TArray<UInstancedStaticMeshComponent*> Components;
            Generator->GetComponents(Components);
            int32 Instances = 0;
            TArray<TSharedPtr<FJsonValue>> Transforms;
            for (const auto* Component : Components)
            {
                Instances += Component->GetInstanceCount();
                for (int32 Index = 0; Index < Component->GetInstanceCount(); ++Index)
                {
                    FTransform Transform;
                    Component->GetInstanceTransform(Index, Transform, true);
                    auto Row = MakeShared<FJsonObject>();
                    Row->SetStringField(TEXT("mesh"), Component->GetStaticMesh()->GetPathName());
                    const FVector P = Transform.GetTranslation(), S = Transform.GetScale3D();
                    const FQuat Q = Transform.GetRotation();
                    TArray<TSharedPtr<FJsonValue>> Values;
                    for (double Value : {P.X, P.Y, P.Z, Q.X, Q.Y, Q.Z, Q.W, S.X, S.Y, S.Z})
                        Values.Add(MakeShared<FJsonValueNumber>(Value));
                    Row->SetArrayField(TEXT("values"), Values);
                    Transforms.Add(MakeShared<FJsonValueObject>(Row));
                }
            }
            Report->SetArrayField(TEXT("initial_instance_transforms"), Transforms);
            Report->SetNumberField(TEXT("initial_components"), Components.Num());
            Report->SetNumberField(TEXT("initial_instances"), Instances);
            if (Instances == 0)
                return Finish(TEXT("No render instances"));
            Report->SetObjectField(TEXT("initial"), Current);
            Capture(TEXT("01_Initial"));
            Phase = 1;
        }
        else if (Phase == 1)
        {
            if (Move == 4)
                Capture(TEXT("02_Expansion"));
            if (Move < 8)
                NextCameraLocation = Start + FVector(0, 51000 * ++Move, 0);
            else
            {
                Report->SetObjectField(TEXT("expanded"), Current);
                if (Current->GetNumberField(TEXT("registered")) <= 121)
                    return Finish(TEXT("No expansion"));
                Capture(TEXT("03_Far"));
                NextCameraLocation = Start;
                Phase = 2;
            }
        }
        else if (Phase == 2)
        {
            const auto Old = Placements(Initial);
            const auto Now = Placements(Current);
            for (const auto& Entry : Old)
            {
                if (!Now.Contains(Entry.Key) || Now.FindChecked(Entry.Key) != Entry.Value)
                    return Finish(TEXT("Reentry CRC"));
            }
            if (Current->GetNumberField(TEXT("reactivations")) < Initial->GetNumberField(TEXT("active")))
                return Finish(TEXT("Reactivation count"));
            Report->SetObjectField(TEXT("returned"), Current);
            Capture(TEXT("04_Return"));
            bResetNextTick = true;
            Phase = 3;
        }
        else if (Phase == 3)
        {
            if (!Placements(Initial).OrderIndependentCompareEqual(Placements(Current)))
                return Finish(TEXT("Same seed"));
            Report->SetBoolField(TEXT("same_seed"), true);
            Generator->Seed = 43;
            Generator->InitializeEnvironment();
            Phase = 4;
        }
        else if (Phase == 4)
        {
            if (Placements(Initial).OrderIndependentCompareEqual(Placements(Current)))
                return Finish(TEXT("Other seed"));
            Report->SetBoolField(TEXT("other_seed_changed"), true);
            Generator->ClearEnvironment();
            const auto Cleared = State();
            Report->SetObjectField(TEXT("cleared"), Cleared);
            TArray<UInstancedStaticMeshComponent*> Components;
            Generator->GetComponents(Components);
            if (Cleared->GetNumberField(TEXT("registered")) || Cleared->GetBoolField(TEXT("timer_active")) ||
                !Components.IsEmpty())
                return Finish(TEXT("Runtime cleanup"));
            Phase = 5;
        }
        else
            return Finish();
        return true;
    }

public:
    // 선택적 검증 실행 명령을 등록한다. 정상 플레이에서 자동으로 검사하지 않는다.
    void StartupModule() override
    {
        if (StartMWEnvFlowProbe())
            return;
        if (!FParse::Value(FCommandLine::Get(), TEXT("MWEnvSmoke="), Theme))
            return;
        FParse::Value(FCommandLine::Get(), TEXT("MWEnvOutput="), Output);
        if (Output.IsEmpty())
            Output = FPaths::ProjectSavedDir() / TEXT("EnvironmentAcceptance");
        IFileManager::Get().MakeDirectory(*Output, true);
        Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FMWEnvironmentAcceptance::Tick),
                                                      0.12f);
    }

    // 검증 예약과 명령 등록을 해제해 모듈 종료 뒤 콜백이 남지 않게 한다.
    void ShutdownModule() override
    {
        StopMWEnvFlowProbe();
        if (Handle.IsValid())
            FTSTicker::RemoveTicker(Handle);
        Handle.Reset();
    }
};

IMPLEMENT_MODULE(FMWEnvironmentAcceptance, MWEnvironmentAcceptance)
