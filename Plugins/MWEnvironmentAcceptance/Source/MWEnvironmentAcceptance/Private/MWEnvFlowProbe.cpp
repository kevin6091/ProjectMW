// 로컬 카메라 시야와 배경 갱신을 기록하는 선택적 검증 도구다. 정상 경기 입력을 대체하지 않는다.

#include "MWEnvFlowProbe.h"
#include "MWEnvironmentGameplay.h"
#include "MWEnvironmentRestore.h"
#include "MWGameplayTags.h"
#include "System/MWGameInstance.h"
#include "System/MWPlayerState.h"
#include "System/GameState/MWLobbyGameState.h"
#include "System/GameState/MWGameplayGameState.h"
#include "Player/MWLobbyPlayerController.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HighResScreenshot.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    class FFlowProbe
    {
    public:
        FString Role, Theme, Output;
        FTSTicker::FDelegateHandle Handle;
        double Started = FPlatformTime::Seconds();
        bool Requested = false, Ready = false, StartSent = false, Confirmed = false;
        int32 Frame = 0, Phase = 0, Move = 0;
        int32 DebugTicks = 0;
        TWeakObjectPtr<ACameraActor> Camera;
        TWeakObjectPtr<AMWEnvironmentGenerator> Generator;
        TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
        TSharedPtr<FJsonObject> Initial;
        FVector Origin = FVector(0, -60000, 70000);

        TSharedPtr<FJsonObject> State() const
        {
            TSharedPtr<FJsonObject> O;
            FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Generator->GetEnvironmentDebug()), O);
            return O;
        }

        TMap<FString, FString> Layout(const TSharedPtr<FJsonObject>& O) const
        {
            TMap<FString, FString> M;
            for (const auto& V : O->GetArrayField(TEXT("generated")))
            {
                const auto T = V->AsObject();
                M.Add(FString::Printf(TEXT("%d,%d"), int32(T->GetNumberField(TEXT("x"))),
                                      int32(T->GetNumberField(TEXT("y")))),
                      T->GetStringField(TEXT("placement_crc")));
            }
            return M;
        }

        // 선택적 검증의 예약·카메라·구독을 닫고 성공 여부와 실패 원인을 기록한다.
        bool Finish(const FString& Error = FString())
        {
            Report->SetStringField(TEXT("role"), Role);
            Report->SetStringField(TEXT("theme"), Theme);
            Report->SetBoolField(TEXT("passed"), Error.IsEmpty());
            Report->SetStringField(TEXT("error"), Error);
            FString Text;
            FJsonSerializer::Serialize(Report.ToSharedRef(), TJsonWriterFactory<>::Create(&Text));
            FFileHelper::SaveStringToFile(Text, *(Output / (Role + TEXT(".json"))));
            UE_LOG(LogTemp, Display, TEXT("MW_ENV_FLOW %s %s passed=%d %s"), *Role, *Theme, Error.IsEmpty(), *Error);
            FPlatformMisc::RequestExitWithStatus(false, Error.IsEmpty() ? 0 : 1);
            return false;
        }

        // 현재 실제 화면을 검증 자료로 저장하며 게임 규칙 값을 바꾸지 않는다.
        void Capture(const FString& Name)
        {
            FScreenshotRequest::RequestScreenshot(Output / (Role + TEXT("_") + Name + TEXT(".png")), false, false);
        }

        // 검증 단계의 준비 조건과 제한 시간을 확인하며 다음 촬영 단계로 진행한다.
        bool Tick(float)
        {
            if (FPlatformTime::Seconds() - Started > 180)
                return Finish(TEXT("Timeout waiting for existing gameplay flow"));
            UWorld* World = nullptr;
            for (const auto& C : GEngine->GetWorldContexts())
                if (C.World() && C.World()->IsGameWorld() && C.World()->HasBegunPlay())
                {
                    World = C.World();
                    break;
                }
            if (!World)
                return true;
            auto* App = World->GetGameInstance<UMWGameInstance>();
            APlayerController* PC = World->GetFirstPlayerController();
            if (!App || !PC || !PC->IsLocalController())
                return true;
            if (!Requested)
            {
                if (Role == TEXT("Host"))
                {
                    FMWRoomSettings Settings;
                    Settings.RoomName = TEXT("Environment acceptance");
                    Settings.MaxPlayers = 2;
                    Settings.MapThemeId =
                        FGameplayTag::RequestGameplayTag(FName(*(TEXT("Asset.MapTheme.Environment.") + Theme)), false);
                    App->RequestCreateRoom(Settings);
                }
                else
                {
                    if (!IFileManager::Get().FileExists(*(Output / TEXT("HostReady.txt"))))
                        return true;
                    App->RequestJoinAddress(TEXT("127.0.0.1:17877"));
                }
                Requested = true;
                return true;
            }
            if (auto* Lobby = World->GetGameState<AMWLobbyGameState>())
            {
                auto* LPC = Cast<AMWLobbyPlayerController>(PC);
                auto* PS = PC->GetPlayerState<AMWPlayerState>();
                if (!LPC || !PS || PS->GetLobbyMemberId() < 1 || World->GetNetMode() == NM_Standalone)
                    return true;
                if (Role == TEXT("Host") && World->GetNetMode() == NM_ListenServer)
                    FFileHelper::SaveStringToFile(TEXT("Ready"), *(Output / TEXT("HostReady.txt")));
                Report->SetStringField(TEXT("selected_theme"), Lobby->GetRoomSettings().MapThemeId.ToString());
                if (!PS->IsReady() && Lobby->GetRoomPhase() == EMWRoomPhase::Lobby && (++DebugTicks % 8) == 0)
                {
                    LPC->RequestSetReady(true);
                    Ready = true;
                }
                if ((DebugTicks % 8) == 0)
                {
                    Report->SetStringField(TEXT("last_lobby_error"), LPC->GetLobbyActionError());
                    Report->SetStringField(TEXT("last_lobby_phase"), UEnum::GetValueAsString(Lobby->GetRoomPhase()));
                    Report->SetNumberField(TEXT("last_player_count"), Lobby->PlayerArray.Num());
                    Report->SetBoolField(TEXT("last_local_ready"), PS->IsReady());
                    Report->SetBoolField(TEXT("last_local_host"), PS->IsRoomHost());
                    Report->SetBoolField(TEXT("last_can_start"), Lobby->CanStartGame());
                    FString Text;
                    FJsonSerializer::Serialize(Report.ToSharedRef(), TJsonWriterFactory<>::Create(&Text));
                    FFileHelper::SaveStringToFile(Text, *(Output / (Role + TEXT("_Progress.json"))));
                }
                if (Role == TEXT("Host") && !StartSent && Lobby->CanStartGame())
                {
                    LPC->RequestStartGame();
                    StartSent = true;
                }
                if (!Confirmed && Lobby->GetRoomPhase() == EMWRoomPhase::VehicleSelection)
                {
                    LPC->RequestConfirmVehicle(MWGameplayTags::Asset_Vehicle_SMIET);
                    Confirmed = true;
                    Report->SetBoolField(TEXT("used_existing_ready_start_confirm_requests"), true);
                }
                return true;
            }
            auto* GS = World->GetGameState<AMWGameplayGameState>();
            if (!GS)
                return true;
            auto* Bridge = World->GetSubsystem<UMWEnvironmentGameplay>();
            if (!Bridge || !Bridge->GetGenerator())
                return true;
            Generator = Bridge->GetGenerator();
            Report->SetStringField(TEXT("binding"), Bridge->GetBindingStatus());
            if (!Cast<AMWVehiclePawn>(PC->GetPawn()) || PC->GetPawn()->GetController() != PC)
                return true;
            if (GS->GetRoomSettings().MapThemeId.ToString() != TEXT("Asset.MapTheme.Environment.") + Theme)
                return Finish(TEXT("Selected theme did not survive actual travel/replication"));
            if (Phase == 0)
            {
                if (++Frame < 25 || State()->GetNumberField(TEXT("active")) == 0)
                    return true;
                Report->SetStringField(TEXT("game_state_class"), GS->GetClass()->GetPathName());
                Report->SetStringField(TEXT("gameplay_phase"), UEnum::GetValueAsString(GS->GetRoomPhase()));
                Report->SetStringField(TEXT("pawn"), PC->GetPawn()->GetClass()->GetPathName());
                Report->SetBoolField(TEXT("pawn_possessed"), true);
                Report->SetBoolField(TEXT("actual_pawn_view_target"), PC->GetViewTarget() == PC->GetPawn());
                Report->SetObjectField(TEXT("actual_pawn_camera_environment"), State());
                Report->SetStringField(TEXT("theme_asset"), Generator->Theme->GetPathName());
                for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
                {
                    if (It->GetName() == TEXT("StaticMeshActor_1"))
                    {
                        Report->SetBoolField(TEXT("development_backdrop_hidden"), It->IsHidden());
                        Report->SetNumberField(TEXT("development_backdrop_collision"),
                                               int32(It->GetStaticMeshComponent()->GetCollisionEnabled()));
                    }
                }
                if (!State()->GetBoolField(TEXT("player_projection")))
                    return Finish(TEXT("Player projection unavailable"));
                Capture(TEXT("01_ActualPawnCamera"));
                Phase = 1;
                Frame = 0;
                return true;
            }
            if (Phase == 1)
            {
                if (++Frame < 8)
                    return true;
                Camera = World->SpawnActor<ACameraActor>(Origin, FRotator(-45, 90, 0));
                Camera->GetCameraComponent()->FieldOfView = 80;
                PC->SetViewTarget(Camera.Get());
                Generator->AutoUpdate = false;
                Generator->Seed = 42;
                Generator->InitializeEnvironment();
                Report->SetObjectField(TEXT("registration"), State());
                if (State()->GetNumberField(TEXT("registered")) != 121 ||
                    !State()->GetArrayField(TEXT("generated")).IsEmpty())
                    return Finish(TEXT("Initial registration"));
                Phase = 2;
                Frame = 0;
                return true;
            }
            // 진단용 카메라도 실제 로컬 PlayerCameraManager를 통해 시야를 확인한다.
            // 이 검증 이동은 차량 입력·물리와 공용 게임 규칙을 바꾸지 않는다.
            Generator->UpdateVisibility();
            if (++Frame < 16)
                return true;
            Frame = 0;
            const auto Current = State();
            if (Phase == 2)
            {
                Initial = Current;
                Report->SetObjectField(TEXT("initial"), Current);
                if (!Current->GetBoolField(TEXT("player_projection")) || Current->GetNumberField(TEXT("active")) == 0)
                    return Finish(TEXT("No initial PC-camera features"));
                if (FMath::Abs(Current->GetNumberField(TEXT("camera_fov")) - 80) > 0.1)
                    return Finish(TEXT("Live FOV not used"));
                Capture(TEXT("02_Overview"));
                Phase = 3;
                Camera->SetActorLocation(Origin + FVector(0, 51000, 0));
                Move = 1;
            }
            else if (Phase == 3)
            {
                if (Move < 8)
                {
                    Camera->SetActorLocation(Origin + FVector(0, 51000 * ++Move, 0));
                    return true;
                }
                Report->SetObjectField(TEXT("expanded"), Current);
                if (Current->GetNumberField(TEXT("registered")) <= 121)
                    return Finish(TEXT("No tile expansion"));
                int32 Hidden = 0;
                const auto Old = Layout(Initial);
                for (const auto& V : Current->GetArrayField(TEXT("generated")))
                {
                    const auto T = V->AsObject();
                    const FString K = FString::Printf(TEXT("%d,%d"), int32(T->GetNumberField(TEXT("x"))),
                                                      int32(T->GetNumberField(TEXT("y"))));
                    if (Old.Contains(K) && !T->GetBoolField(TEXT("active")))
                        ++Hidden;
                }
                Report->SetNumberField(TEXT("initial_tiles_hidden"), Hidden);
                if (!Hidden)
                    return Finish(TEXT("No tile deactivation"));
                Capture(TEXT("03_Expanded"));
                Camera->SetActorLocation(Origin);
                Phase = 4;
            }
            else if (Phase == 4)
            {
                const auto After = Layout(Current);
                for (const auto& Row : Layout(Initial))
                    if (After.FindRef(Row.Key) != Row.Value)
                        return Finish(TEXT("Re-entry placement changed"));
                if (Current->GetNumberField(TEXT("reactivations")) <= 0)
                    return Finish(TEXT("No reactivation"));
                Report->SetObjectField(TEXT("reentry"), Current);
                Capture(TEXT("04_Reentry"));
                Generator->InitializeEnvironment();
                Phase = 5;
            }
            else if (Phase == 5)
            {
                if (!Layout(Initial).OrderIndependentCompareEqual(Layout(Current)))
                    return Finish(TEXT("Same-input seed repeat"));
                Report->SetBoolField(TEXT("same_seed_repeated"), true);
                Generator->Seed = 43;
                Generator->InitializeEnvironment();
                Phase = 6;
            }
            else if (Phase == 6)
            {
                if (Layout(Initial).OrderIndependentCompareEqual(Layout(Current)))
                    return Finish(TEXT("Different seed unchanged"));
                Report->SetBoolField(TEXT("other_seed_changed"), true);
                Generator->ClearEnvironment();
                Report->SetObjectField(TEXT("cleared"), State());
                TArray<UInstancedStaticMeshComponent*> Components;
                Generator->GetComponents(Components);
                if (!Components.IsEmpty() || State()->GetBoolField(TEXT("timer_active")))
                    return Finish(TEXT("Generator cleanup"));
                if (Role == TEXT("Client"))
                {
                    FFileHelper::SaveStringToFile(TEXT("Ready"), *(Output / TEXT("ClientDone.txt")));
                    return Finish();
                }
                Phase = 7;
            }
            else
            {
                if (Role == TEXT("Host") && !IFileManager::Get().FileExists(*(Output / TEXT("ClientDone.txt"))))
                    return true;
                return Finish();
            }
            return true;
        }
    };

    TUniquePtr<FFlowProbe> Probe;
    FTSTicker::FDelegateHandle PIEHandle;
}

// 명시적 요청에서만 현재 로컬 배경 시야 검증을 시작한다.
bool StartMWEnvFlowProbe()
{
    if (FParse::Param(FCommandLine::Get(), TEXT("MWEnvPIERequests")))
    {
        PIEHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda(
                [](float)
                {
                    for (const auto& Context : GEngine->GetWorldContexts())
                    {
                        UWorld* World = Context.World();
                        if (!World || World->WorldType != EWorldType::PIE || !World->HasBegunPlay())
                            continue;
                        auto* PC = Cast<AMWLobbyPlayerController>(World->GetFirstPlayerController());
                        auto* GS = World->GetGameState<AMWLobbyGameState>();
                        auto* PS = PC ? PC->GetPlayerState<AMWPlayerState>() : nullptr;
                        if (!GS || !PC || !PC->IsLocalController() || !PS || PS->GetLobbyMemberId() < 1)
                            continue;
                        UE_LOG(LogTemp, Display,
                               TEXT("MW_ENV_PIE_REQUEST World=%s Net=%d Member=%d Ready=%d Host=%d Error=%s"),
                               *World->GetOutermost()->GetName(), int32(World->GetNetMode()), PS->GetLobbyMemberId(),
                               PS->IsReady(), PS->IsRoomHost(), *PC->GetLobbyActionError());
                        if (GS->GetRoomPhase() == EMWRoomPhase::Lobby)
                        {
                            if (!PS->IsReady())
                                PC->RequestSetReady(true);
                            if (World->GetNetMode() == NM_ListenServer && GS->CanStartGame())
                                PC->RequestStartGame();
                        }
                        if (GS->GetRoomPhase() == EMWRoomPhase::VehicleSelection && !PS->GetVehicleAssetID().IsValid())
                            PC->RequestConfirmVehicle(MWGameplayTags::Asset_Vehicle_SMIET);
                    }
                    return true;
                }),
            0.5f);
        return true;
    }
    FString Role;
    if (!FParse::Value(FCommandLine::Get(), TEXT("MWEnvFlow="), Role))
        return false;
    Probe = MakeUnique<FFlowProbe>();
    Probe->Role = Role;
    FParse::Value(FCommandLine::Get(), TEXT("MWEnvTheme="), Probe->Theme);
    FParse::Value(FCommandLine::Get(), TEXT("MWEnvOutput="), Probe->Output);
    IFileManager::Get().MakeDirectory(*Probe->Output, true);
    Probe->Handle =
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(Probe.Get(), &FFlowProbe::Tick), 0.12f);
    return true;
}

// 진행 중인 선택적 시야 검증을 안전하게 중단한다.
void StopMWEnvFlowProbe()
{
    if (PIEHandle.IsValid())
    {
        FTSTicker::RemoveTicker(PIEHandle);
        PIEHandle.Reset();
    }
    if (Probe)
    {
        FTSTicker::RemoveTicker(Probe->Handle);
        Probe.Reset();
    }
}
