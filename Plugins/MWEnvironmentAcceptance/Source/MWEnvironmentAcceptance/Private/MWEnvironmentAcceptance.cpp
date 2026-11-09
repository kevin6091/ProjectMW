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

    TMap<FString,FString> Placements(const TSharedPtr<FJsonObject>& Object) const
    {
        TMap<FString,FString> Map;
        for (const auto& Entry : Object->GetArrayField(TEXT("generated")))
        {
            const auto& Tile = Entry->AsObject();
            Map.Add(FString::Printf(TEXT("%d,%d"), int32(Tile->GetNumberField(TEXT("x"))),
                int32(Tile->GetNumberField(TEXT("y")))), Tile->GetStringField(TEXT("placement_crc")));
        }
        return Map;
    }

    bool Finish(const FString& Error = FString())
    {
        Report->SetStringField(TEXT("theme"), Theme);
        Report->SetStringField(TEXT("error"), Error);
        Report->SetBoolField(TEXT("passed"), Error.IsEmpty());
        FString Text;
        auto Writer = TJsonWriterFactory<>::Create(&Text);
        FJsonSerializer::Serialize(Report.ToSharedRef(),Writer);
        FFileHelper::SaveStringToFile(Text, *(Output / (Theme + TEXT("_Runtime.json"))));
        UE_LOG(LogTemp, Display, TEXT("MW_ENV_RUNTIME_ACCEPTANCE %s passed=%d error=%s"), *Theme, Error.IsEmpty(), *Error);
        FPlatformMisc::RequestExitWithStatus(false,Error.IsEmpty()?0:1);
        return false;
    }

    void Capture(const FString& Label)
    {
        FScreenshotRequest::RequestScreenshot(Output / (Theme + TEXT("_") + Label + TEXT(".png")),false,false);
    }

    bool Tick(float)
    {
        if (++TotalTicks > 1200) return Finish(TEXT("Smoke timeout"));
        if (!Generator.IsValid())
        {
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                UWorld* World = Context.World();
                if (!World || !World->IsGameWorld() || !World->HasBegunPlay()) continue;
                for (TActorIterator<AMWEnvironmentGenerator> It(World); It; ++It)
                {
                    Generator = *It;
                    break;
                }
            }
            if (!Generator.IsValid()) return true;
            if (!Generator->Theme || !Generator->ViewCamera) return Finish(TEXT("Missing theme or camera"));
            Start = Generator->ViewCamera->GetActorLocation();
            if (auto* Controller = Generator->GetWorld()->GetFirstPlayerController())
                Controller->SetViewTarget(Generator->ViewCamera);
            Generator->AutoUpdate = false;
            Generator->Seed = 42;
            Generator->InitializeEnvironment();
            const auto Registration = State();
            Report->SetObjectField(TEXT("registration"),Registration);
            if (Registration->GetNumberField(TEXT("registered")) != 121 ||
                !Registration->GetArrayField(TEXT("generated")).IsEmpty()) return Finish(TEXT("Initial registration"));
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
        if (Frame < 12) return true;
        Frame = 0;
        const auto Current = State();
        if (Phase == 0)
        {
            Initial = Current;
            if (Current->GetNumberField(TEXT("active")) <= 0) return Finish(TEXT("No initial features"));
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
                    Component->GetInstanceTransform(Index,Transform,true);
                    auto Row = MakeShared<FJsonObject>();
                    Row->SetStringField(TEXT("mesh"),Component->GetStaticMesh()->GetPathName());
                    const FVector P = Transform.GetTranslation(), S = Transform.GetScale3D();
                    const FQuat Q = Transform.GetRotation();
                    TArray<TSharedPtr<FJsonValue>> Values;
                    for (double Value : {P.X,P.Y,P.Z,Q.X,Q.Y,Q.Z,Q.W,S.X,S.Y,S.Z})
                        Values.Add(MakeShared<FJsonValueNumber>(Value));
                    Row->SetArrayField(TEXT("values"),Values);
                    Transforms.Add(MakeShared<FJsonValueObject>(Row));
                }
            }
            Report->SetArrayField(TEXT("initial_instance_transforms"),Transforms);
            Report->SetNumberField(TEXT("initial_components"),Components.Num());
            Report->SetNumberField(TEXT("initial_instances"),Instances);
            if (Instances == 0) return Finish(TEXT("No render instances"));
            Report->SetObjectField(TEXT("initial"),Current);
            Capture(TEXT("01_Initial"));
            Phase = 1;
        }
        else if (Phase == 1)
        {
            if (Move == 4) Capture(TEXT("02_Expansion"));
            if (Move < 8)
                NextCameraLocation = Start + FVector(0,51000 * ++Move,0);
            else
            {
                Report->SetObjectField(TEXT("expanded"),Current);
                if (Current->GetNumberField(TEXT("registered")) <= 121) return Finish(TEXT("No expansion"));
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
                if (!Now.Contains(Entry.Key) || Now.FindChecked(Entry.Key) != Entry.Value) return Finish(TEXT("Reentry CRC"));
            }
            if (Current->GetNumberField(TEXT("reactivations")) < Initial->GetNumberField(TEXT("active")))
                return Finish(TEXT("Reactivation count"));
            Report->SetObjectField(TEXT("returned"),Current);
            Capture(TEXT("04_Return"));
            bResetNextTick = true;
            Phase = 3;
        }
        else if (Phase == 3)
        {
            if (!Placements(Initial).OrderIndependentCompareEqual(Placements(Current))) return Finish(TEXT("Same seed"));
            Report->SetBoolField(TEXT("same_seed"),true);
            Generator->Seed = 43;
            Generator->InitializeEnvironment();
            Phase = 4;
        }
        else if (Phase == 4)
        {
            if (Placements(Initial).OrderIndependentCompareEqual(Placements(Current))) return Finish(TEXT("Other seed"));
            Report->SetBoolField(TEXT("other_seed_changed"),true);
            Generator->ClearEnvironment();
            const auto Cleared = State();
            Report->SetObjectField(TEXT("cleared"),Cleared);
            TArray<UInstancedStaticMeshComponent*> Components;
            Generator->GetComponents(Components);
            if (Cleared->GetNumberField(TEXT("registered")) || Cleared->GetBoolField(TEXT("timer_active")) ||
                !Components.IsEmpty()) return Finish(TEXT("Runtime cleanup"));
            Phase = 5;
        }
        else return Finish();
        return true;
    }

public:
    void StartupModule() override
    {
        if (StartMWEnvFlowProbe()) return;
        if (!FParse::Value(FCommandLine::Get(),TEXT("MWEnvSmoke="),Theme)) return;
        FParse::Value(FCommandLine::Get(),TEXT("MWEnvOutput="),Output);
        if (Output.IsEmpty()) Output = FPaths::ProjectSavedDir() / TEXT("EnvironmentAcceptance");
        IFileManager::Get().MakeDirectory(*Output,true);
        Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this,&FMWEnvironmentAcceptance::Tick),0.12f);
    }
    void ShutdownModule() override
    {
        StopMWEnvFlowProbe();
        if (Handle.IsValid()) FTSTicker::RemoveTicker(Handle);
        Handle.Reset();
    }
};

IMPLEMENT_MODULE(FMWEnvironmentAcceptance, MWEnvironmentAcceptance)
