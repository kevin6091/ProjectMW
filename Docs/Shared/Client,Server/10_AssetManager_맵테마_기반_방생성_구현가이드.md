# AssetManager 맵 테마 기반 방 생성 구현 가이드

**문서 번호:** MW-ROOM-IMPLEMENTATION-001 / v1.0 / 2026-10-28\
**분야:** Client + Server / MW Runtime / Listen Server\
**정본:** [06 공용 데이터 이식 6.3절](06_공용_데이터_이식.md)\
**범위:** 현재 로그인·설정 검사 이후부터 호스트 방 생성 → Listen 로비 준비 → 공개 → 종료·재생성까지. 검색·참가와 Ready/경기 시작은 마지막 절의 후속 계약을 따른다.

> 이 문서의 추가 코드는 직접 작성할 참조안이다. 현재 Source에 추가되어 있거나 컴파일/접속 시험을 통과한 코드라는 뜻이 아니다. 기존 구현과 이번에 추가할 코드를 구분한다. 기존 문서의 엔진 기준은 5.8.3이며 현재 확인된 설치본은 5.8.4다.

> **후속 구현 반영:** 현재 Source에는 방 목록·실제 참가·로비 동기화까지 구현되어 있다. 최신 계약과 실제 시험 결과는 [12 현재 방 목록·참가·로비 동기화](12_현재방목록_참가_로비동기화_구현_및_검증.md)를 따른다. 아래 코드와 Examples/RoomHosting는 최초 호스팅 단계의 참조본이다. 현재 Source를 이 예제로 전체 교체하면 참가 구현이 사라진다. 특히 현재 ApplyRoomSettingsFromServer는 방 설정과 실제 SessionId **두 인자**를 받으며, Subsystem 구현은 기존 cpp와 MWMultiplayerRooms.cpp로 나뉜다.

**따라 읽는 순서:** 1~2절에서 정본과 현재 상태를 확인하고, 3~4절의 공용 타입/카탈로그 조회부터 작성한다. 5~10절은 서로 연결된 클래스와 세션 코드를 파일별로 채우는 구간이다. 전체 선언·정의가 연결된 다음 11절의 에디터/UI를 설정하고 12절의 첫 생성 시험을 수행한다. 13절의 검색·참가·Ready는 다음 작업이다. 코드 블록마다 기존 파일에 추가할 것인지 새 파일 전체인지 설명을 확인한다.

**헤더·cpp 전체본:** 부분 코드를 조립하지 않고 파일 단위로 보려면 [전체 파일 안내](Examples/RoomHosting/README.md), [MWMultiplayerSubsystem.h 전문](Examples/RoomHosting/MWMultiplayerSubsystem.h), [MWMultiplayerSubsystem.cpp 전문](Examples/RoomHosting/MWMultiplayerSubsystem.cpp)을 먼저 연다. 기존 로그인·검증 코드와 이 가이드의 세션 코드를 합쳤고, 현재 사용자가 정리한 System/MultiplayerSubSystem 경로에 맞췄다. Source를 자동 교체한 파일이 아니다.

## 1. 이번에 확정한 정본

맵 테마의 **ID와 레벨 참조 연결은 MWAssetData**, 실행 중 조회는 **MWAssetManager**가 담당한다.

```text
생성 화면: MapThemeId = Asset.MapTheme.DevDefault
  → UMWMultiplayerSubsystem.ValidateRoomSettings
  → ResolveMapPackageName
  → 등록된 FGameplayTag인지, Asset.MapTheme 하위인지 확인
  → UMWAssetManager.FindAssetPath
  → 시작 UMWAssetData 카탈로그의 레벨 참조
  → Asset Registry로 존재·UWorld 타입 확인
  → /Game/Levels/L_Dev_Default 패키지 이름 확보
  → 세션 생성 성공
  → 엔진 OpenLevel(..., "listen")
```

현재 유효한 입력은 `Asset.MapTheme.DevDefault`이며 `Dev_Default`는 유효한 ID가 아니다. FName 타입은 그대로 유지한다.

| 대상 | 원본과 책임 |
|---|---|
| MWAssetData | 제작자가 테마 ID와 레벨 soft reference를 등록 |
| MWAssetManager | 시작 때 카탈로그를 검증하고 실행용 조회 스냅샷 유지 |
| MultiplayerSubsystem | 생성 입력 검사, 테마 해석, 세션 작업, 맵 이동 |
| GameMode | 승인된 설정으로 서버 초기화, 정원과 입장 허용 결정 |
| GameState | 방 이름·정원·테마를 참가자에게 복제 |
| PlayerState | 서버가 정한 호스트 표시와 개인 Ready |
| PlayerController / Widget | 본인 화면과 입력 연결 |

다음 기준을 함께 지킨다.

- Subsystem·GameMode·Widget에 테마→맵 경로 `switch`나 별도 TMap을 만들지 않는다.
- Widget은 표시 이름이나 임의 파일 경로를 생성 요청으로 보내지 않는다.
- 레벨을 일반 에셋처럼 `LoadAssetSynchronous`로 캐시에 넣지 않는다. 월드는 엔진 맵 이동으로 로딩한다.
- 레벨에 `AssetLabel.Preload`를 붙이지 않는다.
- 서비스에는 테마 ID를 문자열로 기록하고 입장 후에는 서버 GameState를 표시한다.
- AssetManager는 각 프로세스에 존재한다. 호스트 카탈로그가 원격으로 자동 복제되는 것이 아니다.
- 현재 카탈로그 경로는 `/Game/Data/DA_AssetData.DA_AssetData`다. 새 카탈로그를 만들거나 기존 파일을 옮길 필요가 없다.

### 1.1 태그 등록 위치는 한 곳을 관리 원본으로 선택

작성 중 현재 소스에 `MWGameplayTags::Asset_MapTheme_DevDefault` Native 선언/정의가 추가되었고, 기존 DefaultGameplayTags.ini의 동일 이름 항목도 남아 있는 것을 확인했다. 태그 이름을 한쪽에서 바꾸거나 지우고 다른 쪽에 남겨 두는 혼동을 피하도록 등록 원본을 정리한다. 두 곳에 이름이 있다고 곧바로 엔진 오류라고 단정하는 것은 아니다.

| 선택 | 남길 것 | 정리할 것 |
|---|---|---|
| 기존 이니 경로 유지 | DefaultGameplayTags.ini의 GameplayTagList 항목 | 같은 테마의 Native DECLARE/DEFINE |
| 새 Native 경로 채택 | MWGameplayTags.h/.cpp의 DECLARE/DEFINE | 동일 테마의 이니 GameplayTagList 항목 |

어느 쪽이든 실제 이름 `Asset.MapTheme.DevDefault`와 카탈로그의 AssetId는 그대로다. 새 Native 상수는 C++에서 ID를 사용하기 편하게 하는 것이며 레벨 참조를 대신하지 않는다. 아래 코드의 RequestGameplayTag는 등록 출처에 의존하지 않는다. 이번 문서 작성에서 사용자의 Native/Config 소스는 수정하지 않았다.

Native 상수로 테스트 요청을 만들 때는 FName으로 변환한다. 이 예시는 Native 경로를 선택했을 때만 사용한다.

```cpp
#include "MWGameplayTags.h"

FMWRoomSettings Settings;
Settings.RoomName = TEXT("MW_TestRoom");
Settings.MaxPlayers = 3;
Settings.MapThemeId =
    MWGameplayTags::Asset_MapTheme_DevDefault.GetTag().GetTagName();
```

이니 경로라면 `FName(TEXT("Asset.MapTheme.DevDefault"))` 또는 UI가 반환한 ID를 사용한다. 어느 경우에도 여기서 맵 파일 경로를 지정하지 않는다.

## 2. 시작점과 첫 완료 조건

### 2.1 이미 있는 것

`Source/MW/System/MultiplayerSubSystem/MWMultiplayerSubsystem.h/.cpp`에 다음이 있다.

- 로그인 요청·상태 조회·완료 처리·로그인 Delegate 해제.
- `EMWSessionState`, `IsSessionBusy`, `SetSessionState`, `SessionRequestId` 검사.
- `FMWRoomSettings`: RoomName, MaxPlayers, MapThemeId.
- `ValidateRoomSettings`: 공백 정리, 이름 1~32 TCHAR, 제어문자, 정원 2~4, 테마 검사.
- `ResolveSession`: 현재 World의 EOS Session 인터페이스 조회.
- `ResolveMapPackageName`: 카탈로그 및 UWorld 메타데이터 검사.
- `PendingRoomSettings`: 아직 생성 경로에서 쓰이지 않는 보관 멤버.
- `RequestCreateRoom`: 선언만 실질적으로 존재하며 본문은 비어 있음.

Config의 테마 태그·StartupAssetData·첫 맵 MapsToCook도 연결되어 있다. 기존 설정 검사 13개는 2026-10-28 재실행에서 통과했다. 이것은 세션/Listen 접속 검증이 아니다.

### 2.2 첫 구현 목표

```text
로그인 → 생성 폼 → Creating
  → EOS CreateSession
  → 카탈로그에서 승인한 맵을 Listen으로 이동
  → 서버 설정·호스트 PlayerState·수신 준비 확인
  → 세션 RoomStatus=Lobby 갱신
  → Hosting → 로비 표시
  → Leaving → DestroySession → 타이틀 이동 완료 → None
```

실패 때는 정리 중과 정리 확인 불가를 구분한다. 재생성은 로컬/서비스 정리와 타이틀 복구가 확인된 다음에 허용한다.

## 3. 추가·수정할 파일과 에셋

| 순서 | 파일/에셋 | 작업 |
|---|---|---|
| 1 | Source/MW/System/MultiplayerSubSystem/MWMultiplayerTypes.h | 기존 방 설정과 세션 enum을 이동할 공용 타입 헤더 |
| 2 | Source/MW/System/MWAssetManager.h/.cpp | 카탈로그 스냅샷의 ID 열거 함수 추가 |
| 3 | Source/MW/System/MultiplayerSubSystem/MWMultiplayerSubsystem.h/.cpp | 현재 클래스 확장. 새 Subsystem은 만들지 않음 |
| 4 | Source/MW/System/MWGameState.h/.cpp | 방 설정 복제와 로비 변경 알림 |
| 5 | Source/MW/System/MWPlayerState.h/.cpp | 호스트 표시·Ready 상태 복제 |
| 6 | Source/MW/Player/MWPlayerController.h/.cpp | 로컬 로비 UI와 이벤트 연결 |
| 7 | Source/MW/System/MWGameMode.h/.cpp | 서버 초기화·입장 정책·준비 확인 |
| 8 | Source/MW/MW.Build.cs | 실제 사용하는 모듈 의존성 |
| 9 | /Game/Levels/L_Title | 로컬 타이틀 맵 신규 제작 |
| 10 | /Game/System/BP_MWTitleGameMode | 메뉴용 AGameModeBase BP |
| 11 | /Game/System/BP_MWGameMode | 경기용 AMWGameMode BP |
| 12 | /Game/System/BP_MWPlayerController | AMWPlayerController BP, UI 이벤트 구현 |
| 13 | /Game/UI/Lobby/WBP_Title, WBP_CreateRoom, WBP_RoomLobby | 필요한 최소 화면 |
| 14 | Config/DefaultEngine.ini, DefaultGame.ini | 시작 맵·Cook 설정 확인 |

현재 사용자가 정리한 경로를 유지한다. Subsystem/공용 타입은 System/MultiplayerSubSystem, PlayerController는 Player, 나머지 Framework는 System에 둔다. BP는 공용 Content 기준에 따라 System, UI는 UI/Lobby에 둔다. 에셋은 현재 팀의 UVCS 절차로 제작·전달한다.

### 3.1 공용 타입 헤더

기존 헤더의 `EMWSessionState`와 `FMWRoomSettings` 정의를 다음 새 헤더로 **이동**한다. 기존 위치에 중복 정의를 남기지 않는다.

```cpp
#pragma once

#include "CoreMinimal.h"
#include "MWMultiplayerTypes.generated.h"

UENUM(BlueprintType)
enum class EMWSessionState : uint8
{
    None,
    Creating,
    Hosting,
    Searching,
    Joining,
    Joined,
    Leaving,
    CleanupRequired
};

USTRUCT(BlueprintType)
struct MW_API FMWRoomSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString RoomName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxPlayers = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName MapThemeId;
};
```

`CleanupRequired`는 새로 **끝에 추가**하는 값이다. 기존 enum 순서와 기존 구조체/필드 이름·타입은 유지한다. 이 이동은 `/Script/MW.MWRoomSettings`의 타입명을 바꾸지 않는다. 에디터를 닫고 UHT를 포함한 정상 빌드 후 BP를 확인한다.

Subsystem 헤더에는 `#include "System/MultiplayerSubSystem/MWMultiplayerTypes.h"`를 추가한다. 자신의 `MWMultiplayerSubsystem.generated.h`는 여전히 마지막 include다.

`IsSessionBusy`에는 CleanupRequired를 false 그룹에 추가한다. 이 상태는 진행 스피너를 끝내고 복구 선택을 표시하는 상태다. 생성/검색 허용 조건은 Busy가 아니라 반드시 None이다.

### 3.2 Build.cs

기존 Public 의존성은 유지한다. 현재 Private의 중복 OnlineSubsystem 항목을 다음으로 교체하는 안이다.

```csharp
PrivateDependencyModuleNames.AddRange(new string[]
{
    "AssetRegistry",
    "EngineSettings",
    "OnlineSubsystemEOS"
});
```

- AssetRegistry: 레벨의 FAssetData를 직접 사용하는 cpp.
- EngineSettings: 프로젝트 기본 타이틀 맵을 UGameMapsSettings에서 조회.
- OnlineSubsystemEOS: cpp에서 EOS Bucket 키의 공개 헤더를 사용.
- 공개 헤더는 EOS SDK 타입을 노출하지 않는다. EOS 전용 include는 cpp에만 둔다.
- Slate를 직접 작성하지 않는 이번 UMG 연결에 Slate 모듈을 추가할 필요는 없다.

## 4. AssetManager에서 테마 선택 목록 만들기

현재 ID 하나를 UI에 직접 넣을 수는 있지만, 카탈로그를 정본으로 운영하려면 목록도 그 스냅샷에서 얻는 편이 일관된다. 테마 레벨을 로딩하지 않고 등록 ID만 열거한다.

### 4.1 MWAssetManager.h public에 추가

```cpp
TArray<FGameplayTag> GetAssetIdsUnderRoot(FGameplayTag Root) const;
```

### 4.2 MWAssetManager.cpp에 추가

```cpp
TArray<FGameplayTag> UMWAssetManager::GetAssetIdsUnderRoot(
    FGameplayTag Root) const
{
    check(IsInGameThread());

    TArray<FGameplayTag> Result;
    if (!HasCatalog() || !Root.IsValid())
    {
        return Result;
    }

    for (const auto& Pair : AssetPaths)
    {
        if (Pair.Key != Root && Pair.Key.MatchesTag(Root))
        {
            Result.Add(Pair.Key);
        }
    }

    Result.Sort([](const FGameplayTag& A, const FGameplayTag& B)
    {
        return A.ToString() < B.ToString();
    });
    return Result;
}
```

`AssetPaths`는 이미 있는 스냅샷 멤버다. 원본 DataAsset을 실행 중 다시 읽어 두 번째 목록을 만들지 않는다. 이 함수는 파일·UObject를 로딩하지 않는다.

### 4.3 Subsystem의 UI용 함수

헤더 public:

```cpp
UFUNCTION(BlueprintCallable, Category = "MW|Multiplayer|Room")
TArray<FName> GetAvailableMapThemeIds() const;
```

cpp:

```cpp
TArray<FName> UMWMultiplayerSubsystem::GetAvailableMapThemeIds() const
{
    TArray<FName> Result;
    const FGameplayTag Root = FGameplayTag::RequestGameplayTag(
        FName(TEXT("Asset.MapTheme")), false);

    const UMWAssetManager& Assets = UMWAssetManager::Get();
    for (const FGameplayTag Id : Assets.GetAssetIdsUnderRoot(Root))
    {
        FString Error;
        if (!ResolveMapPackageName(Id.GetTagName(), Error).IsNone())
        {
            Result.Add(Id.GetTagName());
        }
    }
    return Result;
}
```

생성 화면을 열 때 한 번 호출한다. Tick이나 UMG의 반복 Pure 바인딩에 넣지 않는다. 현재 목록에는 DevDefault 한 항목이 나와야 한다. 표시 이름을 “개발 맵”으로 꾸며도 선택 값은 원래 FName으로 보관한다.

새 테마 추가는 태그·카탈로그 레벨 참조·Cook 대상을 함께 추가하고 재시작한다. UI나 세션 코드에 새 경로 분기를 추가하지 않는다. 기존 AssetData에는 테마 표시명/썸네일 필드가 없으므로 이번 최소 UI는 ID를 표시해도 된다.

## 5. Gameplay Framework 코드부터 준비하기

GameInstance는 엔진 기본 클래스를 그대로 사용해도 된다. 이미 있는 GameInstanceSubsystem이 맵 이동을 넘는 작업을 맡는다. GameSession도 엔진이 GameMode에서 생성하는 기본 객체를 사용한다. 이번 최소판에 새 GameInstance/GameSession 관리 클래스를 추가할 필요는 없다.

### 5.1 GameState — 방 설정을 모두에게 전달

`MWGameState.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "System/MultiplayerSubSystem/MWMultiplayerTypes.h"
#include "MWGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMWLobbyChanged);

UCLASS()
class MW_API AMWGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;
    virtual void AddPlayerState(APlayerState* PlayerState) override;
    virtual void RemovePlayerState(APlayerState* PlayerState) override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category = "MW|Lobby")
    FMWRoomSettings GetRoomSettings() const { return RoomSettings; }

    UPROPERTY(BlueprintAssignable, Category = "MW|Lobby")
    FMWLobbyChanged OnLobbyChanged;

    void ApplyRoomSettingsFromServer(const FMWRoomSettings& Settings);
    void NotifyLobbyChanged();

private:
    UPROPERTY(ReplicatedUsing = OnRep_RoomSettings)
    FMWRoomSettings RoomSettings;

    UFUNCTION()
    void OnRep_RoomSettings();
};
```

`MWGameState.cpp`:

```cpp
#include "System/MWGameState.h"
#include "Player/MWPlayerController.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

void AMWGameState::BeginPlay()
{
    Super::BeginPlay();

    // GameState가 Controller보다 늦게 준비된 경로도 연결합니다.
    for (FConstPlayerControllerIterator It =
        GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (AMWPlayerController* PC = Cast<AMWPlayerController>(It->Get()))
        {
            if (PC->IsLocalController())
            {
                PC->RefreshLobbyScreen();
            }
        }
    }
    NotifyLobbyChanged();
}

void AMWGameState::AddPlayerState(APlayerState* PlayerState)
{
    Super::AddPlayerState(PlayerState);
    NotifyLobbyChanged();
}

void AMWGameState::RemovePlayerState(APlayerState* PlayerState)
{
    Super::RemovePlayerState(PlayerState);
    NotifyLobbyChanged();
}

void AMWGameState::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AMWGameState, RoomSettings);
}

void AMWGameState::ApplyRoomSettingsFromServer(
    const FMWRoomSettings& Settings)
{
    if (!HasAuthority())
    {
        return;
    }

    RoomSettings = Settings;
    ForceNetUpdate();
    NotifyLobbyChanged(); // Listen Host의 표시도 갱신합니다.
}

void AMWGameState::OnRep_RoomSettings()
{
    NotifyLobbyChanged();
}

void AMWGameState::NotifyLobbyChanged()
{
    OnLobbyChanged.Broadcast();
}
```

이 시점에는 로비용 방 설정만 추가한다. 기존 계획의 게임 Phase·라운드·서버 시각은 다음 경기 진행 작업에서 확장한다. 로컬 세션 상태 enum을 GameState의 경기 Phase로 사용하지 않는다.

### 5.2 PlayerState — 참가자별 상태

`MWPlayerState.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "MWPlayerState.generated.h"

UCLASS()
class MW_API AMWPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void OnRep_PlayerName() override;
    virtual void OnRep_UniqueId() override;

    UFUNCTION(BlueprintPure, Category = "MW|Lobby")
    bool IsRoomHost() const { return bRoomHost; }

    UFUNCTION(BlueprintPure, Category = "MW|Lobby")
    bool IsReady() const { return bReady; }

    void SetLobbyFlagsFromServer(bool bInHost, bool bInReady);

private:
    UPROPERTY(ReplicatedUsing = OnRep_LobbyFlags)
    bool bRoomHost = false;

    UPROPERTY(ReplicatedUsing = OnRep_LobbyFlags)
    bool bReady = false;

    UFUNCTION()
    void OnRep_LobbyFlags();

    void NotifyLobby();
};
```

`MWPlayerState.cpp`:

```cpp
#include "System/MWPlayerState.h"
#include "System/MWGameState.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

void AMWPlayerState::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AMWPlayerState, bRoomHost);
    DOREPLIFETIME(AMWPlayerState, bReady);
}

void AMWPlayerState::SetLobbyFlagsFromServer(bool bInHost, bool bInReady)
{
    if (!HasAuthority())
    {
        return;
    }
    bRoomHost = bInHost;
    bReady = bInReady;
    ForceNetUpdate();
    NotifyLobby();
}

void AMWPlayerState::OnRep_LobbyFlags()
{
    NotifyLobby();
}

void AMWPlayerState::OnRep_PlayerName()
{
    Super::OnRep_PlayerName();
    NotifyLobby();
}

void AMWPlayerState::OnRep_UniqueId()
{
    Super::OnRep_UniqueId();
    NotifyLobby();
}

void AMWPlayerState::NotifyLobby()
{
    if (AMWGameState* GS = GetWorld()->GetGameState<AMWGameState>())
    {
        GS->NotifyLobbyChanged();
    }
}
```

Ready는 아직 저장 공간만 준비한 것이다. Widget이 이 값을 직접 수정하거나 호스트 표시를 권한 부여 값으로 쓰지 않는다. 호스트/Ready 최종 판정은 서버 GameMode 경로에서 한다.

### 5.3 PlayerController — 본인 UI 연결

`MWPlayerController.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "System/MultiplayerSubSystem/MWMultiplayerTypes.h"
#include "MWPlayerController.generated.h"

class AMWGameState;
class UMWMultiplayerSubsystem;

UCLASS()
class MW_API AMWPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnRep_PlayerState() override;

    UFUNCTION(BlueprintCallable, Category = "MW|Lobby")
    void RefreshLobbyScreen();

protected:
    UFUNCTION(BlueprintImplementableEvent, Category = "MW|Lobby")
    void CreateLobbyScreen();

    UFUNCTION(BlueprintImplementableEvent, Category = "MW|Lobby")
    void OnLobbyScreenRefresh();

private:
    TWeakObjectPtr<AMWGameState> BoundGameState;
    TWeakObjectPtr<UMWMultiplayerSubsystem> BoundSubsystem;

    UFUNCTION()
    void HandleLobbyChanged();

    UFUNCTION()
    void HandleSessionError(const FString& ErrorMessage);

    UFUNCTION()
    void HandleSessionStateChanged(
        EMWSessionState PreviousState, EMWSessionState NewState);
};
```

`MWPlayerController.cpp`:

```cpp
#include "Player/MWPlayerController.h"
#include "System/MWGameState.h"
#include "System/MultiplayerSubSystem/MWMultiplayerSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void AMWPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController())
    {
        return;
    }

    UMWMultiplayerSubsystem* Sessions =
        GetGameInstance()->GetSubsystem<UMWMultiplayerSubsystem>();
    BoundSubsystem = Sessions;
    if (Sessions)
    {
        Sessions->OnSessionStateChanged.AddUniqueDynamic(
            this, &AMWPlayerController::HandleSessionStateChanged);
        Sessions->OnSessionError.AddUniqueDynamic(
            this, &AMWPlayerController::HandleSessionError);
    }

    CreateLobbyScreen();
    RefreshLobbyScreen();
}

void AMWPlayerController::RefreshLobbyScreen()
{
    if (!IsLocalController() || !HasActorBegunPlay())
    {
        return;
    }

    AMWGameState* GS = GetWorld()->GetGameState<AMWGameState>();
    if (BoundGameState.Get() != GS)
    {
        if (AMWGameState* Old = BoundGameState.Get())
        {
            Old->OnLobbyChanged.RemoveDynamic(
                this, &AMWPlayerController::HandleLobbyChanged);
        }

        BoundGameState = GS;
        if (GS)
        {
            GS->OnLobbyChanged.AddUniqueDynamic(
                this, &AMWPlayerController::HandleLobbyChanged);
        }
    }

    OnLobbyScreenRefresh();
}

void AMWPlayerController::HandleLobbyChanged()
{
    RefreshLobbyScreen();
}

void AMWPlayerController::HandleSessionError(const FString& ErrorMessage)
{
    RefreshLobbyScreen();
}

void AMWPlayerController::HandleSessionStateChanged(
    EMWSessionState PreviousState, EMWSessionState NewState)
{
    RefreshLobbyScreen();
}

void AMWPlayerController::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    RefreshLobbyScreen();
}

void AMWPlayerController::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    if (AMWGameState* GS = BoundGameState.Get())
    {
        GS->OnLobbyChanged.RemoveDynamic(
            this, &AMWPlayerController::HandleLobbyChanged);
    }
    if (UMWMultiplayerSubsystem* Sessions = BoundSubsystem.Get())
    {
        Sessions->OnSessionStateChanged.RemoveDynamic(
            this, &AMWPlayerController::HandleSessionStateChanged);
        Sessions->OnSessionError.RemoveDynamic(
            this, &AMWPlayerController::HandleSessionError);
    }

    BoundGameState.Reset();
    BoundSubsystem.Reset();
    Super::EndPlay(EndPlayReason);
}
```

GameState·Controller·PlayerState 준비 순서가 달라도 각각의 준비/수신 경로에서 화면을 재평가한다. 이벤트가 오기 전에도 현재 값을 읽어 표시한다. 임의 Delay나 매 Tick 목록 재생성은 필요하지 않다.

### 5.4 GameMode — 서버 설정 적용과 수신 준비

`MWGameMode.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MWGameMode.generated.h"

UCLASS()
class MW_API AMWGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AMWGameMode();

    virtual void InitGameState() override;
    virtual void StartPlay() override;
    virtual FString InitNewPlayer(APlayerController* NewPlayerController,
        const FUniqueNetIdRepl& UniqueId, const FString& Options,
        const FString& Portal) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void PreLogin(const FString& Options, const FString& Address,
        const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;

    virtual void HandleStartingNewPlayer_Implementation(
        APlayerController* NewPlayer) override;

    void TryPrepareLobby();
    void SetLobbyAdmission(bool bOpen) { bAcceptingPlayers = bOpen; }

private:
    bool bAcceptingPlayers = false;
    bool bRoomSettingsApplied = false;

    bool ApplyApprovedRoomSettings();
};
```

`MWGameMode.cpp`:

```cpp
#include "System/MWGameMode.h"
#include "System/MWGameState.h"
#include "System/MWPlayerState.h"
#include "Player/MWPlayerController.h"
#include "System/MultiplayerSubSystem/MWMultiplayerSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameSession.h"

AMWGameMode::AMWGameMode()
{
    GameStateClass = AMWGameState::StaticClass();
    PlayerStateClass = AMWPlayerState::StaticClass();
    PlayerControllerClass = AMWPlayerController::StaticClass();

    // 첫 로비에서는 차량을 자동 생성하지 않습니다.
    DefaultPawnClass = nullptr;
}

void AMWGameMode::InitGameState()
{
    Super::InitGameState();
    ApplyApprovedRoomSettings();
}

bool AMWGameMode::ApplyApprovedRoomSettings()
{
    UMWMultiplayerSubsystem* Sessions =
        GetGameInstance()->GetSubsystem<UMWMultiplayerSubsystem>();
    AMWGameState* GS = GetWorld()->GetGameState<AMWGameState>();

    FMWRoomSettings Approved;
    if (!Sessions || !GS || !GameSession ||
        !Sessions->GetApprovedRoomSettingsForWorld(GetWorld(), Approved))
    {
        return false;
    }

    GS->ApplyRoomSettingsFromServer(Approved);
    GameSession->MaxPlayers = Approved.MaxPlayers;
    GameSession->MaxSpectators = 0; // 첫 로비는 플레이 참가자만 받습니다.
    bRoomSettingsApplied = true;
    return true;
}

void AMWGameMode::StartPlay()
{
    Super::StartPlay();
    TryPrepareLobby();
}

FString AMWGameMode::InitNewPlayer(
    APlayerController* NewPlayerController,
    const FUniqueNetIdRepl& UniqueId,
    const FString& Options, const FString& Portal)
{
    if (GetNetMode() == NM_ListenServer && UniqueId.IsValid() &&
        NewPlayerController->IsLocalController())
    {
        GetGameInstance()->GetSubsystem<UMWMultiplayerSubsystem>()
            ->ExpectHostRegistration(GetWorld());
    }

    // 기본 GameSession/PlayerState 등록 경로를 그대로 실행합니다.
    return Super::InitNewPlayer(
        NewPlayerController, UniqueId, Options, Portal);
}

void AMWGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    AMWGameState* GS = GetWorld()->GetGameState<AMWGameState>();
    const bool bLocalHost =
        GetNetMode() == NM_ListenServer && NewPlayer->IsLocalController();

    // PreLogin 후 입장 잠금/정원이 바뀐 경로를 다시 검사합니다.
    if (!bLocalHost && (!bAcceptingPlayers || !GS ||
        GetNumPlayers() > GS->GetRoomSettings().MaxPlayers))
    {
        if (GameSession)
        {
            GameSession->KickPlayer(NewPlayer,
                FText::FromString(TEXT("방이 닫혔거나 정원이 찼습니다.")));
        }
        return;
    }

    if (AMWPlayerState* PS = NewPlayer->GetPlayerState<AMWPlayerState>())
    {
        PS->SetLobbyFlagsFromServer(bLocalHost, false);
    }
    TryPrepareLobby();
}

void AMWGameMode::PreLogin(const FString& Options,
    const FString& Address, const FUniqueNetIdRepl& UniqueId,
    FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    if (!ErrorMessage.IsEmpty())
    {
        return;
    }

    if (!bRoomSettingsApplied || !bAcceptingPlayers)
    {
        ErrorMessage = TEXT("방을 준비 중이거나 입장이 닫혔습니다.");
    }
}

void AMWGameMode::HandleStartingNewPlayer_Implementation(
    APlayerController* NewPlayer)
{
    // 로비에서는 Super의 기본 Pawn 자동 스폰 경로를 실행하지 않습니다.
    // 차량은 이후 승인 VehicleDefinition을 사용하는 한 경로에서 생성합니다.
}

void AMWGameMode::TryPrepareLobby()
{
    if (GetNetMode() != NM_ListenServer || !GetWorld()->HasBegunPlay())
    {
        return;
    }
    UMWMultiplayerSubsystem* Sessions =
        GetGameInstance()->GetSubsystem<UMWMultiplayerSubsystem>();
    if (!Sessions || Sessions->GetSessionState() != EMWSessionState::Creating)
    {
        return;
    }

    if (!bRoomSettingsApplied && !ApplyApprovedRoomSettings())
    {
        return;
    }

    for (FConstPlayerControllerIterator It =
        GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (PC && PC->IsLocalController())
        {
            AMWPlayerState* PS = PC->GetPlayerState<AMWPlayerState>();
            if (PS && PS->GetUniqueId().IsValid())
            {
                PS->SetLobbyFlagsFromServer(true, false);
                Sessions->NotifyListenWorldReady(GetWorld());
                return;
            }
        }
    }
}
```

첫 로비가 뜨기 전에는 입장을 닫는다. 실제 수신/설정/호스트 준비를 확인한 Subsystem이 공개 요청 직전에 입장을 연다. 공개 실패·종료 때 다시 닫는다.

이 최소판의 PostLogin 재검사는 입장 완료 시 과인원/잠금 변경을 걸러내기 위한 것이다. LM-5의 “마지막 입장과 경기 시작 경합”까지 완료했다고 보지 않는다. 시작 승인과 참가자 확정은 별도 구현한다.

GameSession의 기본 플레이어 등록 경로를 유지한다. 호스트를 수동 등록하는 코드를 무조건 추가하면 기본 등록과 중복될 수 있다. 첫 실행에서 호스트 RegisteredPlayers 및 남은 슬롯을 확인한다.

## 6. Subsystem에 추가할 선언과 멤버

기존 로그인 관련 선언·구현은 유지한다. 이미 이동한 공용 타입은 새 헤더에서 include한다.

### 6.1 include와 결과 이벤트

헤더의 generated.h **위**에 추가한다.

```cpp
#include "Engine/EngineBaseTypes.h"
#include "TimerManager.h"
#include "System/MultiplayerSubSystem/MWMultiplayerTypes.h"
```

include 구간(자신의 generated.h 포함)이 끝난 뒤, 클래스 선언 전에는 포인터 인자용 전방 선언을 둔다.

```cpp
class UNetDriver;
class UWorld;
```

`MWMultiplayerSubsystem.generated.h` 위의 include 구간 아래, 클래스 선언 위에는 다음을 추가한다.

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FMWCreateRoomFinished,
    bool, bSuccess,
    const FString&, ErrorMessage);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FMWSessionError,
    const FString&, ErrorMessage);
```

### 6.2 public에 추가

`RequestCreateRoom`은 이미 있으므로 중복 선언하지 않는다.

```cpp
UFUNCTION(BlueprintCallable, Category = "MW|Multiplayer|Session")
void RequestLeaveRoom();

UFUNCTION(BlueprintCallable, Category = "MW|Multiplayer|Session")
void RetrySessionCleanup();

UFUNCTION(BlueprintPure, Category = "MW|Multiplayer|Session")
FString GetLastSessionError() const { return LastSessionError; }

UPROPERTY(BlueprintAssignable, Category = "MW|Multiplayer|Session")
FMWCreateRoomFinished OnCreateRoomFinished;

UPROPERTY(BlueprintAssignable, Category = "MW|Multiplayer|Session")
FMWSessionError OnSessionError;

// 아래 함수들은 서버 Framework가 호출할 일반 C++ 함수입니다.
bool GetApprovedRoomSettingsForWorld(
    const UWorld* World, FMWRoomSettings& OutSettings) const;

void ExpectHostRegistration(UWorld* World);
void NotifyListenWorldReady(UWorld* World);
```

생성 성공 이벤트는 Hosting 준비 완료 때 발생한다. 실패 이벤트는 생성 시도가 실패했다는 뜻이며, 자원 정리는 Leaving/CleanupRequired일 수 있다. UI는 실패 이벤트만 보고 새 생성 버튼을 활성화하지 않는다.

### 6.3 private 함수와 내부 타입

```cpp
enum class EMWCreateRoomStage : uint8
{
    None,
    CreatingSession,
    LoadingWorld,
    Publishing
};

enum class EMWCleanupCause : uint8
{
    None,
    CreationFailure,
    Leave
};

FName ResolveTitleMapPackageName(FString& OutError) const;
bool IsOwnWorld(const UWorld* World) const;
bool HasPendingSessionWork() const;

void HandleHostPlayersRegistered(
    FName SessionName, const TArray<FUniqueNetIdRef>& Players,
    bool bWasSuccessful, uint64 RequestId);

void ClearHostRegistrationDelegate();

void HandleCreateSessionComplete(
    FName SessionName, bool bWasSuccessful, uint64 RequestId);

void HandleUpdateSessionComplete(
    FName SessionName, bool bWasSuccessful, uint64 RequestId);

void HandleDestroySessionComplete(
    FName SessionName, bool bWasSuccessful, uint64 RequestId);

void HandlePostLoadMap(UWorld* World);

void HandleTravelFailure(
    UWorld* World, ETravelFailure::Type Failure, const FString& Error);

void HandleNetworkFailure(
    UWorld* World, UNetDriver* Driver,
    ENetworkFailure::Type Failure, const FString& Error);

void ClearSessionApiDelegates();
void ArmSessionTimeout();
void ClearSessionTimeout();
void HandleSessionTimeout(uint64 RequestId);

void EmitSessionError(const FString& Error);
void FailCreation(const FString& Error);
void CloseServerAdmission();
void StartCleanup();
void BeginReturnToTitle();
void FinishCleanup();
```

### 6.4 private 멤버

`PendingRoomSettings`과 `SessionRequestId`는 이미 있으므로 아래 멤버만 추가한다.

```cpp
IOnlineSessionPtr SessionApi;
FUniqueNetIdPtr HostUserId;

FDelegateHandle CreateSessionHandle;
FDelegateHandle UpdateSessionHandle;
FDelegateHandle DestroySessionHandle;
FDelegateHandle RegisterPlayersHandle;

FDelegateHandle PostLoadMapHandle;
FDelegateHandle TravelFailureHandle;
FDelegateHandle NetworkFailureHandle;

FTimerHandle SessionTimeoutHandle;

FName PendingMapPackageName;
FName ReturnMapPackageName;
TWeakObjectPtr<UWorld> HostWorld;

EMWCreateRoomStage CreateRoomStage = EMWCreateRoomStage::None;
EMWCleanupCause CleanupCause = EMWCleanupCause::None;

FString LastSessionError;

bool bCreateResultPending = false;
bool bReturningToTitle = false;
bool bCleanupUncertain = false;
bool bWorldLoadPending = false;
bool bHostRegistrationExpected = false;
bool bHostRegistrationConfirmed = false;

// 개발용 시작값입니다. 서비스/맵별 실측 후 조정합니다.
static constexpr float SessionOperationTimeoutSeconds = 60.0f;
```

| 멤버 | 언제 유지·초기화하는가 |
|---|---|
| SessionApi | 생성 요청을 받은 뒤부터 종료 정리까지 유지. Delegate를 다른 인터페이스에서 해제하지 않음 |
| PendingRoomSettings | 검사를 통과한 값만 저장. Hosting 동안 GameMode/상태 초기화에 사용 |
| PendingMapPackageName | 카탈로그에서 해석한 경기 맵 이름. 생성 요청을 받은 시점의 스냅샷 |
| ReturnMapPackageName | 프로젝트 기본 타이틀 맵. 테마 맵과 분리 |
| HostWorld | 준비를 확인한 월드의 약한 참조. 이전 월드를 살려두지 않음 |
| HostUserId | 이번 생성자의 실제 온라인 ID. 임의 클라이언트 입력으로 받지 않음 |
| RegisterPlayersHandle | 엔진 기본 호스트 등록의 완료를 관찰. 별도 등록 요청은 보내지 않음 |
| bWorldLoadPending | 생성 후 요청한 맵의 완료가 아직 오지 않았는가 |
| bHostRegistrationExpected/Confirmed | 기본 등록 결과를 기다리는가 / 성공이 확인되었는가 |
| bCreateResultPending | 승인한 생성 시도의 결과 이벤트를 아직 보내야 하는가 |
| bReturningToTitle | 세션 정리는 끝났고 타이틀 이동 확인을 기다리는가 |
| bCleanupUncertain | 관리하지 않는 세션 잔존 또는 종료 정보 소실로 안전한 재시도 판단이 불가능한가 |

UPROPERTY가 없는 SessionApi/Handle은 UObject가 아니다. HostWorld는 약한 UObject 참조이므로 맵을 붙잡지 않는다. 이미 있는 PendingRoomSettings의 Transient UPROPERTY는 유지한다.

## 7. Subsystem 구현: 수명과 공통 준비

### 7.1 cpp include

기존 include에 다음을 추가한다.

```cpp
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "GameMapsSettings.h"
#include "Misc/PackageName.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemEOSTypesPublic.h"
#include "System/MWGameMode.h"
#include "System/MWGameState.h"
#include "System/MWPlayerState.h"
#include "UObject/UObjectGlobals.h"
```

### 7.2 cpp 파일 범위 상수와 보조 함수

```cpp
namespace
{
    const FName MWRoomNameKey(TEXT("MW_ROOM_NAME"));
    const FName MWMapThemeKey(TEXT("MW_MAP_THEME"));
    const FName MWRoomStatusKey(TEXT("MW_ROOM_STATUS"));
    const FName MWProtocolKey(TEXT("MW_PROTOCOL_ID"));

    const FString MWProtocol(TEXT("MW_DEV_V1"));
    const FString MWBucket(TEXT("MW_DEV_V1"));

    FName MWGetWorldMapPackage(const UWorld* World)
    {
        if (!World)
        {
            return NAME_None;
        }
        return FName(*UWorld::RemovePIEPrefix(
            World->GetOutermost()->GetName()));
    }
}
```

이 문자열들은 서비스 검색 계약이다. 실제 테마→레벨 매핑은 아니다. 생성/검색 쪽에서 같은 키와 값을 사용한다. Bucket은 검색 그룹, Protocol은 호환 계약이다.

PIE의 `UEDPIE_0_` 접두사를 제거한 패키지 이름으로 검사한다. 패키지 이름과 `.L_Dev_Default`까지 붙는 객체 경로, Windows `.umap` 경로를 혼용하지 않는다.

### 7.3 Initialize에 추가

기존 Super 호출과 로그인 초기화 뒤에 추가한다.

```cpp
PostLoadMapHandle =
    FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
        this, &UMWMultiplayerSubsystem::HandlePostLoadMap);

if (GEngine)
{
    TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(
        this, &UMWMultiplayerSubsystem::HandleTravelFailure);

    NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(
        this, &UMWMultiplayerSubsystem::HandleNetworkFailure);
}
```

이 이벤트는 프로세스 공용이다. 콜백에서는 해당 GameInstance의 World인지 검사해야 다른 PIE 인스턴스의 실패를 처리하지 않는다.

### 7.4 공통 함수

```cpp
bool UMWMultiplayerSubsystem::IsOwnWorld(const UWorld* World) const
{
    return World && World->GetGameInstance() == GetGameInstance();
}

bool UMWMultiplayerSubsystem::HasPendingSessionWork() const
{
    return CreateSessionHandle.IsValid() ||
        UpdateSessionHandle.IsValid() ||
        DestroySessionHandle.IsValid() ||
        bWorldLoadPending || bHostRegistrationExpected;
}

void UMWMultiplayerSubsystem::ClearSessionApiDelegates()
{
    if (SessionApi.IsValid())
    {
        if (CreateSessionHandle.IsValid())
        {
            SessionApi->ClearOnCreateSessionCompleteDelegate_Handle(
                CreateSessionHandle);
        }
        if (UpdateSessionHandle.IsValid())
        {
            SessionApi->ClearOnUpdateSessionCompleteDelegate_Handle(
                UpdateSessionHandle);
        }
        if (DestroySessionHandle.IsValid())
        {
            SessionApi->ClearOnDestroySessionCompleteDelegate_Handle(
                DestroySessionHandle);
        }
    }

    CreateSessionHandle.Reset();
    UpdateSessionHandle.Reset();
    DestroySessionHandle.Reset();
}

void UMWMultiplayerSubsystem::ArmSessionTimeout()
{
    ClearSessionTimeout();
    GetGameInstance()->GetTimerManager().SetTimer(
        SessionTimeoutHandle,
        FTimerDelegate::CreateUObject(
            this, &UMWMultiplayerSubsystem::HandleSessionTimeout,
            SessionRequestId),
        SessionOperationTimeoutSeconds, false);
}

void UMWMultiplayerSubsystem::ClearSessionTimeout()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        GI->GetTimerManager().ClearTimer(SessionTimeoutHandle);
    }
}

void UMWMultiplayerSubsystem::EmitSessionError(const FString& Error)
{
    LastSessionError = Error;
    UE_LOG(LogMWEOS, Warning, TEXT("%s"), *Error);
    OnSessionError.Broadcast(Error);
}

FName UMWMultiplayerSubsystem::ResolveTitleMapPackageName(
    FString& OutError) const
{
    OutError.Reset();

    // GetGameDefaultMap은 패키지 이름을 반환하므로 에셋 조회용 객체 경로로 변환합니다.
    const FString TitlePackage = UGameMapsSettings::GetGameDefaultMap(EDefaultMapRequestType::Client);
    const FSoftObjectPath Path(FString::Printf(TEXT("%s.%s"),
        *TitlePackage, *FPackageName::GetLongPackageAssetName(TitlePackage)));
    FAssetData Asset;

    if (!Path.IsValid() ||
        !UMWAssetManager::Get().GetAssetDataForPath(Path, Asset) ||
        !Asset.IsInstanceOf<UWorld>())
    {
        OutError = TEXT("프로젝트 기본 맵을 실제 타이틀 레벨로 설정해 주세요.");
        return NAME_None;
    }
    return Asset.PackageName;
}

bool UMWMultiplayerSubsystem::GetApprovedRoomSettingsForWorld(
    const UWorld* World, FMWRoomSettings& OutSettings) const
{
    OutSettings = FMWRoomSettings{};

    if (bDeinitializing || !IsOwnWorld(World) ||
        CleanupCause != EMWCleanupCause::None ||
        (SessionState != EMWSessionState::Creating &&
         SessionState != EMWSessionState::Hosting) ||
        PendingMapPackageName.IsNone() ||
        MWGetWorldMapPackage(World) != PendingMapPackageName)
    {
        return false;
    }

    OutSettings = PendingRoomSettings;
    return true;
}
```

타이틀은 프로젝트 기본 맵을 단일 원본으로 읽는다. 테마 맵은 AssetManager, 메뉴 복귀 맵은 Project Settings의 기본 맵이라는 서로 다른 책임이다.

GameMode의 초기화 시점에는 아직 Listen이 시작되지 않았을 수 있으므로 승인 설정 조회 함수는 NetMode로 막지 않는다. 실제 Hosting 확정 전에는 아래 별도 수신 준비 검사를 수행한다.

Timer는 GameInstance의 TimerManager에 둔다. 다만 게임 스레드가 블로킹된 맵 로딩 자체를 강제로 중단하는 장치는 아니다. 엔진의 이동 실패 이벤트와 완료 확인도 함께 사용한다.

## 8. RequestCreateRoom 전체 흐름

현재 비어 있는 본문을 다음 참조안으로 채운다.

```cpp
void UMWMultiplayerSubsystem::RequestCreateRoom(
    const FMWRoomSettings& Settings)
{
    if (bDeinitializing)
    {
        return;
    }

    // 중복 요청은 진행 중인 첫 작업을 건드리지 않습니다.
    if (SessionState != EMWSessionState::None)
    {
        UE_LOG(LogMWEOS, Verbose,
            TEXT("Create ignored: session state is not None."));
        return;
    }

    if (bLoginInProgress || !IsLoggedIn())
    {
        EmitSessionError(TEXT("먼저 로그인해 주세요."));
        return;
    }

    FMWRoomSettings Validated;
    FString Error;
    if (!ValidateRoomSettings(Settings, Validated, Error))
    {
        EmitSessionError(Error);
        return;
    }

    const FName MapPackage =
        ResolveMapPackageName(Validated.MapThemeId, Error);
    if (MapPackage.IsNone())
    {
        EmitSessionError(Error);
        return;
    }

    const FName TitlePackage = ResolveTitleMapPackageName(Error);
    if (TitlePackage.IsNone() || TitlePackage == MapPackage)
    {
        EmitSessionError(TitlePackage.IsNone() ? Error :
            TEXT("타이틀 맵과 경기 테마 맵을 분리해 주세요."));
        return;
    }

    IOnlineSessionPtr Sessions = ResolveSession(Error);
    if (!Sessions.IsValid())
    {
        EmitSessionError(Error);
        return;
    }
    if (Sessions->GetNamedSession(NAME_GameSession) != nullptr)
    {
        // 관리 상태와 실제 세션이 불일치합니다. 모르는 작업을 덮어쓰지 않습니다.
        SessionApi = Sessions;
        ReturnMapPackageName = TitlePackage;
        CleanupCause = EMWCleanupCause::Leave;
        bCleanupUncertain = true;
        ++SessionRequestId;
        SetSessionState(EMWSessionState::CleanupRequired);
        EmitSessionError(TEXT(
            "관리 상태와 다른 세션이 남아 있습니다. 상태 확인 또는 게임 재시작이 필요합니다."));
        return;
    }
    const IOnlineIdentityPtr Identity = ResolveIdentity(Error);
    const FUniqueNetIdPtr CreatorId = Identity.IsValid()
        ? Identity->GetUniquePlayerId(LoginLocalUserNum) : nullptr;
    if (!CreatorId.IsValid() || !CreatorId->IsValid())
    {
        EmitSessionError(TEXT("방 생성자의 온라인 ID를 확인할 수 없습니다."));
        return;
    }

#if !WITH_EOS_SDK
    EmitSessionError(TEXT("이 빌드에는 EOS SDK가 포함되어 있지 않습니다."));
    return;
#else
    FOnlineSessionSettings OnlineSettings;
    OnlineSettings.NumPublicConnections = Validated.MaxPlayers;
    OnlineSettings.NumPrivateConnections = 0;
    OnlineSettings.bIsLANMatch = false;
    OnlineSettings.bIsDedicated = false;
    OnlineSettings.bUseLobbiesIfAvailable = false;
    OnlineSettings.bUsesPresence = false;
    OnlineSettings.bAllowJoinInProgress = true;
    OnlineSettings.bAllowInvites = false;
    OnlineSettings.bAllowJoinViaPresence = false;
    OnlineSettings.bAllowJoinViaPresenceFriendsOnly = false;
    OnlineSettings.bShouldAdvertise = false;

    OnlineSettings.Set(MWRoomNameKey, Validated.RoomName,
        EOnlineDataAdvertisementType::ViaOnlineService);
    OnlineSettings.Set(MWMapThemeKey, Validated.MapThemeId.ToString(),
        EOnlineDataAdvertisementType::ViaOnlineService);
    OnlineSettings.Set(MWRoomStatusKey, FString(TEXT("Preparing")),
        EOnlineDataAdvertisementType::ViaOnlineService);
    OnlineSettings.Set(MWProtocolKey, MWProtocol,
        EOnlineDataAdvertisementType::ViaOnlineService);
    OnlineSettings.Set(OSSEOS_BUCKET_ID_ATTRIBUTE_KEY, MWBucket,
        EOnlineDataAdvertisementType::ViaOnlineService);

    SessionApi = Sessions;
    HostUserId = CreatorId;
    PendingRoomSettings = Validated;
    PendingMapPackageName = MapPackage;
    ReturnMapPackageName = TitlePackage;
    HostWorld.Reset();

    LastSessionError.Reset();
    CleanupCause = EMWCleanupCause::None;
    bReturningToTitle = false;
    bCleanupUncertain = false;
    bCreateResultPending = true;
    bWorldLoadPending = false;
    bHostRegistrationExpected = false;
    bHostRegistrationConfirmed = false;

    const uint64 RequestId = ++SessionRequestId;
    CreateRoomStage = EMWCreateRoomStage::CreatingSession;

    CreateSessionHandle =
        SessionApi->AddOnCreateSessionCompleteDelegate_Handle(
            FOnCreateSessionCompleteDelegate::CreateUObject(
                this, &UMWMultiplayerSubsystem::HandleCreateSessionComplete,
                RequestId));

    SetSessionState(EMWSessionState::Creating);

    // 상태 이벤트 이후에도 같은 작업인지 확인합니다.
    if (!IsCurrentSessionRequest(RequestId) ||
        CleanupCause != EMWCleanupCause::None)
    {
        return;
    }

    ArmSessionTimeout();
    const IOnlineSessionPtr ApiForCall = SessionApi;
    const bool bStarted = ApiForCall->CreateSession(
        LoginLocalUserNum, NAME_GameSession, OnlineSettings);

    if (!bStarted && IsCurrentSessionRequest(RequestId) &&
        CreateRoomStage == EMWCreateRoomStage::CreatingSession)
    {
        FailCreation(TEXT("세션 생성 요청이 시작되지 않았습니다."));
    }
#endif
}
```

설정 검사는 현재 함수가 이미 수행한다. Validate의 로컬 맵 이름은 반환되지 않으므로 실제 생성 시 ResolveMapPackageName을 다시 호출해 보관한다. 레벨은 로딩되지 않으며 기존 BP 함수 시그니처도 바꾸지 않는다.

`NAME_GameSession`은 각 실행 인스턴스 내부 이름이다. 여러 호스트가 같은 상수를 사용해도 각자 다른 서비스 SessionId를 가진다. 방 이름이 같은 두 방도 별개다.

현재 설치 EOS의 Create/Update/Destroy 구현은 반환값 true만으로 서비스 성공을 알 수 없다. 완료 Delegate가 최종 판단 경로다. 예상 밖의 즉시 false에서도 작업을 지웠다고 단정하지 않고 아래 정리 상태로 보낸다.

## 9. 생성 완료 → 맵 로드 → 공개

### 9.1 생성 완료

```cpp
void UMWMultiplayerSubsystem::HandleCreateSessionComplete(
    FName SessionName, bool bWasSuccessful, uint64 RequestId)
{
    if (!IsCurrentSessionRequest(RequestId) ||
        SessionName != NAME_GameSession ||
        CreateRoomStage != EMWCreateRoomStage::CreatingSession)
    {
        return;
    }

    ClearSessionTimeout();
    ClearSessionApiDelegates();

    if (CleanupCause != EMWCleanupCause::None)
    {
        StartCleanup(); // 시간초과 후 늦게 온 성공도 여기서 정리합니다.
        return;
    }

    if (!bWasSuccessful)
    {
        FailCreation(TEXT("EOS 방 생성에 실패했습니다."));
        return;
    }

    const FNamedOnlineSession* Named =
        SessionApi->GetNamedSession(NAME_GameSession);
    if (!Named || !Named->SessionInfo.IsValid() ||
        !Named->SessionInfo->GetSessionId().IsValid())
    {
        FailCreation(TEXT("생성된 세션 정보를 찾을 수 없습니다."));
        return;
    }

    RegisterPlayersHandle =
        SessionApi->AddOnRegisterPlayersCompleteDelegate_Handle(
            FOnRegisterPlayersCompleteDelegate::CreateUObject(
                this, &UMWMultiplayerSubsystem::HandleHostPlayersRegistered,
                RequestId));

    CreateRoomStage = EMWCreateRoomStage::LoadingWorld;
    bWorldLoadPending = true;
    ArmSessionTimeout();

    UGameplayStatics::OpenLevel(
        this, PendingMapPackageName, true, TEXT("listen"));
}
```

성공 콜백에서 Hosting으로 바꾸지 않는다. 실제 맵은 오직 PendingMapPackageName, 즉 AssetManager가 승인한 값으로 연다.

### 9.2 맵 완료 알림

```cpp
void UMWMultiplayerSubsystem::HandlePostLoadMap(UWorld* World)
{
    if (bDeinitializing || !IsOwnWorld(World))
    {
        return;
    }
    bWorldLoadPending = false;

    if (bReturningToTitle)
    {
        if (MWGetWorldMapPackage(World) == ReturnMapPackageName &&
            World->GetNetMode() == NM_Standalone)
        {
            FinishCleanup();
        }
        return;
    }

    if (CleanupCause != EMWCleanupCause::None)
    {
        if (!HasPendingSessionWork())
        {
            StartCleanup();
        }
        return;
    }

    if (SessionState == EMWSessionState::Creating &&
        CreateRoomStage == EMWCreateRoomStage::LoadingWorld)
    {
        if (MWGetWorldMapPackage(World) != PendingMapPackageName ||
            World->GetNetMode() != NM_ListenServer)
        {
            FailCreation(TEXT("요청한 Listen 경기 월드가 준비되지 않았습니다."));
            return;
        }

        AMWGameMode* GM = Cast<AMWGameMode>(World->GetAuthGameMode());
        if (!GM)
        {
            FailCreation(TEXT("경기 맵의 GameMode를 확인해 주세요."));
            return;
        }
        GM->TryPrepareLobby();
    }
}
```

PostLoadMapWithWorld는 맵 이동 확인에 사용한다. 엔진의 일반 맵 이동에서 이 시점은 local player 생성과 World BeginPlay 뒤다. GameMode의 StartPlay/PostLogin 경로도 같은 준비 검사를 호출한다. 어떤 경로가 먼저 와도 아래 단계 검사로 중복 공개를 막는다.

### 9.3 실제 Listen 준비 후 공개 요청

```cpp
void UMWMultiplayerSubsystem::NotifyListenWorldReady(UWorld* World)
{
    if (bDeinitializing || !IsOwnWorld(World) ||
        SessionState != EMWSessionState::Creating ||
        CleanupCause != EMWCleanupCause::None ||
        CreateRoomStage != EMWCreateRoomStage::LoadingWorld ||
        MWGetWorldMapPackage(World) != PendingMapPackageName)
    {
        return;
    }

    if (bWorldLoadPending || !bHostRegistrationConfirmed ||
        World->GetNetMode() != NM_ListenServer ||
        !World->HasBegunPlay() || !World->GetNetDriver())
    {
        return;
    }

    AMWGameMode* GM = Cast<AMWGameMode>(World->GetAuthGameMode());
    AMWGameState* GS = World->GetGameState<AMWGameState>();
    FNamedOnlineSession* Named = SessionApi.IsValid()
        ? SessionApi->GetNamedSession(NAME_GameSession) : nullptr;

    if (!GM || !GS || !Named ||
        GS->GetRoomSettings().MapThemeId != PendingRoomSettings.MapThemeId)
    {
        FailCreation(TEXT("서버의 방 설정 초기화를 확인해 주세요."));
        return;
    }

    HostWorld = World;

    UE_LOG(LogMWEOS, Display, TEXT("Listen ready. Map=%s Driver=%s"),
        *PendingMapPackageName.ToString(),
        *World->GetNetDriver()->GetClass()->GetPathName());

    // 실제 서버가 준비된 뒤 참가 가능한 상태를 공개합니다.
    GM->SetLobbyAdmission(true);
    FOnlineSessionSettings Updated = Named->SessionSettings;
    Updated.bShouldAdvertise = true;
    Updated.Set(MWRoomStatusKey, FString(TEXT("Lobby")),
        EOnlineDataAdvertisementType::ViaOnlineService);

    CreateRoomStage = EMWCreateRoomStage::Publishing;
    const uint64 RequestId = SessionRequestId;

    UpdateSessionHandle =
        SessionApi->AddOnUpdateSessionCompleteDelegate_Handle(
            FOnUpdateSessionCompleteDelegate::CreateUObject(
                this, &UMWMultiplayerSubsystem::HandleUpdateSessionComplete,
                RequestId));

    ArmSessionTimeout();
    const IOnlineSessionPtr ApiForCall = SessionApi;
    const bool bStarted = ApiForCall->UpdateSession(
        NAME_GameSession, Updated, true);

    if (!bStarted && IsCurrentSessionRequest(RequestId) &&
        CreateRoomStage == EMWCreateRoomStage::Publishing)
    {
        FailCreation(TEXT("방 공개 요청이 시작되지 않았습니다."));
    }
}
```

현재 EOS Sessions 경로의 SetPermissionLevel은 공개 슬롯 수를 기준으로 공개 권한을 정한다. 따라서 bShouldAdvertise=false만으로 준비 중 방이 숨겨진다고 가정하지 않는다. 검색은 MW_ROOM_STATUS=Lobby 조건으로 제한하고 서버도 입장을 검사한다.

Driver 로그는 `/Script/SocketSubsystemEOS.NetDriverEOS` 사용 여부를 확인하는 증거로 쓴다. Listen이라는 사실만으로 EOS P2P 인터넷 접속 검증까지 통과했다고 표시하지 않는다.

### 9.4 공개 완료

```cpp
void UMWMultiplayerSubsystem::HandleUpdateSessionComplete(
    FName SessionName, bool bWasSuccessful, uint64 RequestId)
{
    if (!IsCurrentSessionRequest(RequestId) ||
        SessionName != NAME_GameSession ||
        CreateRoomStage != EMWCreateRoomStage::Publishing)
    {
        return;
    }

    ClearSessionTimeout();
    ClearSessionApiDelegates();

    if (CleanupCause != EMWCleanupCause::None)
    {
        StartCleanup();
        return;
    }

    UWorld* World = HostWorld.Get();
    if (!bWasSuccessful || !IsOwnWorld(World) ||
        World->GetNetMode() != NM_ListenServer)
    {
        FailCreation(TEXT("방 공개를 완료하지 못했습니다."));
        return;
    }

    CreateRoomStage = EMWCreateRoomStage::None;
    bCreateResultPending = false;
    SetSessionState(EMWSessionState::Hosting);
    OnCreateRoomFinished.Broadcast(true, FString());
}
```

이때부터 UI에 “방 생성 완료”를 표시한다. Widget의 상태 이벤트 처리에서는 표시만 갱신하고, 같은 이벤트 안에서 새 생성/종료를 자동 호출하지 않는다. 사용자가 명시적으로 누른 버튼을 요청 진입점으로 사용한다.

### 9.5 호스트 서비스 등록까지 확인하기

호스트가 정원 3명 중 한 자리를 차지해야 한다. PlayerState가 생기거나 로컬 RegisteredPlayers 배열에 추가된 것만으로 EOS 등록 성공을 확정하지 않는다. 엔진의 기본 등록 요청을 관찰하고 완료 콜백을 확인한 뒤 공개한다.

기본 GameSession 등록을 유지하며 중복 RegisterPlayer 요청은 보내지 않는다. `InitNewPlayer` 진입에서 “기본 등록 완료를 기다린다”를 표시하고, 서비스의 `OnRegisterPlayersComplete`를 받는다.

Subsystem에 추가할 함수의 구현은 다음과 같다. 선언·멤버·기존 함수의 삽입 위치는 앞 절 코드에도 반영되어 있어야 한다.

```cpp
void UMWMultiplayerSubsystem::ExpectHostRegistration(UWorld* World)
{
    if (bDeinitializing || !IsOwnWorld(World) ||
        CreateRoomStage != EMWCreateRoomStage::LoadingWorld ||
        World->GetNetMode() != NM_ListenServer)
    {
        return;
    }

    // 이후 기본 GameSession 경로가 등록을 수행합니다.
    // 취소/정리 중이어도 실제 시작되는 등록 콜백은 추적합니다.
    bHostRegistrationExpected = true;
}

void UMWMultiplayerSubsystem::ClearHostRegistrationDelegate()
{
    if (SessionApi.IsValid() && RegisterPlayersHandle.IsValid())
    {
        SessionApi->ClearOnRegisterPlayersCompleteDelegate_Handle(
            RegisterPlayersHandle);
    }
    RegisterPlayersHandle.Reset();
}

void UMWMultiplayerSubsystem::HandleHostPlayersRegistered(
    FName SessionName, const TArray<FUniqueNetIdRef>& Players,
    bool bWasSuccessful, uint64 RequestId)
{
    if (!IsCurrentSessionRequest(RequestId) ||
        SessionName != NAME_GameSession ||
        !bHostRegistrationExpected || !HostUserId.IsValid())
    {
        return;
    }

    const bool bIncludesHost = Players.ContainsByPredicate(
        [this](const FUniqueNetIdRef& Id)
        {
            return *Id == *HostUserId;
        });
    if (!bIncludesHost)
    {
        return;
    }

    bHostRegistrationExpected = false;
    bHostRegistrationConfirmed = bWasSuccessful;
    ClearHostRegistrationDelegate();

    if (CleanupCause != EMWCleanupCause::None)
    {
        StartCleanup();
        return;
    }
    if (!bWasSuccessful)
    {
        FailCreation(TEXT("호스트의 EOS 참가자 등록에 실패했습니다."));
        return;
    }

    if (UWorld* World = GetGameInstance()->GetWorld())
    {
        if (AMWGameMode* GM = Cast<AMWGameMode>(World->GetAuthGameMode()))
        {
            GM->TryPrepareLobby();
        }
    }
}
```

초기화 순서는 InitNewPlayer → 엔진 기본 등록 → PostLogin → BeginPlay/맵 완료와, 비동기 EOS 등록 완료가 교차할 수 있다. 하나의 순서를 가정하지 않고 두 조건이 모두 충족될 때 TryPrepareLobby가 공개를 진행한다.

## 10. 실패·종료·제한시간의 구현

### 10.1 생성 실패와 서버 입장 닫기

```cpp
void UMWMultiplayerSubsystem::CloseServerAdmission()
{
    UWorld* World = HostWorld.Get();
    if (!World && GetGameInstance())
    {
        World = GetGameInstance()->GetWorld();
    }

    if (IsOwnWorld(World))
    {
        if (AMWGameMode* GM = Cast<AMWGameMode>(World->GetAuthGameMode()))
        {
            GM->SetLobbyAdmission(false);
        }
    }
}

void UMWMultiplayerSubsystem::FailCreation(const FString& Error)
{
    const bool bNotifyCreate = bCreateResultPending;
    bCreateResultPending = false;
    CleanupCause = EMWCleanupCause::CreationFailure;
    LastSessionError = Error;
    CloseServerAdmission();

    // 진행 중 호출/맵 로드의 결과를 먼저 회수합니다.
    // 콜백을 지우고 같은 이름의 새 요청을 시작하지 않습니다.
    if (HasPendingSessionWork())
    {
        SetSessionState(EMWSessionState::CleanupRequired);
    }
    else
    {
        StartCleanup();
    }

    EmitSessionError(Error);
    if (bNotifyCreate)
    {
        OnCreateRoomFinished.Broadcast(false, Error);
    }
}
```

등록 요청, 맵 로드, 공개 요청이 진행 중이라면 그 완료가 올 때까지 새 생성은 막는다. 늦은 성공 콜백은 Hosting으로 진행하지 않고 정리로 보낸다.

API의 완료 이벤트는 SessionName만 전달하는 공용 multicast다. 새 Delegate에 새 RequestId를 바인딩했다는 이유만으로 이전 API의 이벤트를 완전히 구분할 수는 없다. 따라서 **요청 ID 검사와 작업 직렬화가 함께 필요**하다.

### 10.2 정상 나가기

```cpp
void UMWMultiplayerSubsystem::RequestLeaveRoom()
{
    if (bDeinitializing ||
        (SessionState != EMWSessionState::Hosting &&
         SessionState != EMWSessionState::Joined))
    {
        return;
    }

    ++SessionRequestId;
    CleanupCause = EMWCleanupCause::Leave;
    LastSessionError.Reset();
    CloseServerAdmission();
    SetSessionState(EMWSessionState::Leaving);
    StartCleanup();
}
```

현재 Joined로 들어가는 코드는 아직 없다. 후속 참가 구현에서는 SessionApi와 ReturnMapPackageName도 참가 요청에서 준비해야 이 종료 경로를 재사용할 수 있다. 첫 시험은 Hosting → 종료다.

### 10.3 자원 정리 요청과 완료

```cpp
void UMWMultiplayerSubsystem::StartCleanup()
{
    if (bDeinitializing || CleanupCause == EMWCleanupCause::None)
    {
        return;
    }
    if (HasPendingSessionWork())
    {
        SetSessionState(EMWSessionState::CleanupRequired);
        return;
    }
    if (bCleanupUncertain)
    {
        SetSessionState(EMWSessionState::CleanupRequired);
        return;
    }

    ClearSessionTimeout();
    ClearHostRegistrationDelegate();
    CreateRoomStage = EMWCreateRoomStage::None;
    CloseServerAdmission();

    if (!SessionApi.IsValid())
    {
        SetSessionState(EMWSessionState::CleanupRequired);
        EmitSessionError(TEXT("정리에 사용할 세션 인터페이스가 없습니다."));
        return;
    }

    if (!SessionApi->GetNamedSession(NAME_GameSession))
    {
        BeginReturnToTitle();
        return;
    }

    const uint64 RequestId = SessionRequestId;
    DestroySessionHandle =
        SessionApi->AddOnDestroySessionCompleteDelegate_Handle(
            FOnDestroySessionCompleteDelegate::CreateUObject(
                this, &UMWMultiplayerSubsystem::HandleDestroySessionComplete,
                RequestId));

    SetSessionState(EMWSessionState::Leaving);
    ArmSessionTimeout();

    const IOnlineSessionPtr ApiForCall = SessionApi;
    const bool bStarted = ApiForCall->DestroySession(NAME_GameSession);

    if (!bStarted && IsCurrentSessionRequest(RequestId) &&
        DestroySessionHandle.IsValid())
    {
        ClearSessionTimeout();
        SetSessionState(EMWSessionState::CleanupRequired);
        EmitSessionError(TEXT(
            "종료 요청의 시작 여부를 확인하지 못했습니다. 결과 확인이 필요합니다."));
    }
}

void UMWMultiplayerSubsystem::HandleDestroySessionComplete(
    FName SessionName, bool bWasSuccessful, uint64 RequestId)
{
    if (!IsCurrentSessionRequest(RequestId) ||
        SessionName != NAME_GameSession ||
        CleanupCause == EMWCleanupCause::None)
    {
        return;
    }

    ClearSessionTimeout();
    ClearSessionApiDelegates();

    if (!bWasSuccessful)
    {
        bCleanupUncertain = !SessionApi.IsValid() ||
            SessionApi->GetNamedSession(NAME_GameSession) == nullptr;

        SetSessionState(EMWSessionState::CleanupRequired);
        EmitSessionError(TEXT(
            "세션 정리에 실패했습니다. 새 방을 만들기 전에 정리가 필요합니다."));
        return;
    }

    if (!SessionApi.IsValid() ||
        SessionApi->GetNamedSession(NAME_GameSession) != nullptr)
    {
        SetSessionState(EMWSessionState::CleanupRequired);
        EmitSessionError(TEXT("종료 후에도 로컬 세션이 남아 있습니다."));
        return;
    }

    bCleanupUncertain = false;
    BeginReturnToTitle();
}
```

Destroy 완료가 실패인데 로컬 named session만 사라진 경우를 성공으로 간주하지 않는다. 이 경우 서비스 잔존 여부를 해당 로컬 핸들만으로 확인할 수 없다. 가이드 최소판은 새 생성을 막고 진단/프로세스 재시작을 제공한다. 서비스 상태까지 확실히 확인한 복구가 필요하면 그 증거에 맞춰 확장한다.

### 10.4 타이틀 이동 완료 후에만 None

```cpp
void UMWMultiplayerSubsystem::BeginReturnToTitle()
{
    if (ReturnMapPackageName.IsNone())
    {
        FString Error;
        ReturnMapPackageName = ResolveTitleMapPackageName(Error);
        if (ReturnMapPackageName.IsNone())
        {
            SetSessionState(EMWSessionState::CleanupRequired);
            EmitSessionError(Error);
            return;
        }
    }

    bReturningToTitle = true;
    SetSessionState(EMWSessionState::Leaving);
    ArmSessionTimeout();

    UWorld* World = GetGameInstance()->GetWorld();
    if (IsOwnWorld(World) &&
        MWGetWorldMapPackage(World) == ReturnMapPackageName &&
        World->GetNetMode() == NM_Standalone)
    {
        FinishCleanup();
        return;
    }

    // 절대 이동 + 빈 옵션: Listen 옵션을 메뉴로 상속하지 않습니다.
    UGameplayStatics::OpenLevel(
        this, ReturnMapPackageName, true, FString());
}

void UMWMultiplayerSubsystem::FinishCleanup()
{
    if (HasPendingSessionWork() || bCleanupUncertain ||
        (SessionApi.IsValid() &&
         SessionApi->GetNamedSession(NAME_GameSession) != nullptr))
    {
        SetSessionState(EMWSessionState::CleanupRequired);
        return;
    }

    ClearSessionTimeout();
    ClearSessionApiDelegates();
    ClearHostRegistrationDelegate();

    SessionApi.Reset();
    HostUserId.Reset();
    HostWorld.Reset();

    PendingRoomSettings = FMWRoomSettings{};
    PendingMapPackageName = NAME_None;
    ReturnMapPackageName = NAME_None;

    CreateRoomStage = EMWCreateRoomStage::None;
    CleanupCause = EMWCleanupCause::None;
    bReturningToTitle = false;
    bWorldLoadPending = false;
    bHostRegistrationExpected = false;
    bHostRegistrationConfirmed = false;
    bCreateResultPending = false;
    bCleanupUncertain = false;

    ++SessionRequestId;
    SetSessionState(EMWSessionState::None);
}
```

LastSessionError는 타이틀에서 읽을 수 있도록 남긴다. 다음 생성 요청을 실제로 받아들일 때 비운다. 로그인은 GameInstance 수명이므로 정상 맵 이동 때 다시 accountportal 로그인을 시작하지 않는다.

### 10.5 제한시간과 복구 재시도

```cpp
void UMWMultiplayerSubsystem::HandleSessionTimeout(uint64 RequestId)
{
    if (!IsCurrentSessionRequest(RequestId))
    {
        return;
    }
    ClearSessionTimeout();

    if (bReturningToTitle)
    {
        SetSessionState(EMWSessionState::CleanupRequired);
        EmitSessionError(TEXT("타이틀 이동 완료를 확인하지 못했습니다."));
        return;
    }

    if (CleanupCause == EMWCleanupCause::None &&
        SessionState == EMWSessionState::Creating)
    {
        FailCreation(TEXT("방 생성 또는 준비 확인의 제한시간을 초과했습니다."));
        return;
    }

    if (CleanupCause != EMWCleanupCause::None)
    {
        CloseServerAdmission();
        SetSessionState(EMWSessionState::CleanupRequired);
        EmitSessionError(TEXT(
            "정리 결과 확인의 제한시간을 초과했습니다. 재생성을 보류합니다."));
    }
}

void UMWMultiplayerSubsystem::RetrySessionCleanup()
{
    if (bDeinitializing ||
        SessionState != EMWSessionState::CleanupRequired)
    {
        return;
    }
    if (HasPendingSessionWork())
    {
        EmitSessionError(TEXT(
            "이전 작업의 완료를 기다리고 있습니다. 같은 작업을 겹쳐 보내지 않습니다."));
        return;
    }
    if (bCleanupUncertain)
    {
        EmitSessionError(TEXT(
            "세션 정리 상태를 확정할 수 없습니다. 상태 확인 또는 게임 재시작이 필요합니다."));
        return;
    }

    bReturningToTitle = false;
    StartCleanup();
}
```

시간초과 때 Delegate를 무조건 삭제하지 않는다. 진행 중 요청이 뒤늦게 성공할 수 있기 때문이다. CleanupRequired는 스피너를 끝내고 오류·재시도/재시작 안내를 표시하는 종료 상태이며 새 생성은 허용하지 않는다. 늦은 완료 콜백이 오면 정리를 계속할 수 있다.

일반 맵 이동 완료 이벤트가 nullptr로만 끝나 자기 GameInstance를 식별할 수 없는 경우에는 다른 PIE의 작업을 잘못 종료하지 않는다. 제한시간 후 CleanupRequired로 남기고 재시작/진단한다. 이를 정상 재생성 성공으로 기록하지 않는다.

### 10.6 엔진 이동/네트워크 실패

```cpp
void UMWMultiplayerSubsystem::HandleTravelFailure(
    UWorld* World, ETravelFailure::Type Failure, const FString& Error)
{
    if (bDeinitializing || !IsOwnWorld(World))
    {
        return;
    }
    if (SessionState == EMWSessionState::Creating &&
        CleanupCause == EMWCleanupCause::None)
    {
        FailCreation(FString::Printf(TEXT("맵 이동 실패: %s"), *Error));
    }
    else if (bReturningToTitle)
    {
        ClearSessionTimeout();
        SetSessionState(EMWSessionState::CleanupRequired);
        EmitSessionError(FString::Printf(TEXT("타이틀 이동 실패: %s"), *Error));
    }
}

void UMWMultiplayerSubsystem::HandleNetworkFailure(
    UWorld* World, UNetDriver* Driver,
    ENetworkFailure::Type Failure, const FString& Error)
{
    if (bDeinitializing || !IsOwnWorld(World) ||
        (Driver && Driver->NetDriverName != NAME_GameNetDriver))
    {
        return;
    }

    if (SessionState == EMWSessionState::Creating &&
        CleanupCause == EMWCleanupCause::None)
    {
        FailCreation(FString::Printf(TEXT("연결 준비 실패: %s"), *Error));
    }
    else if (SessionState == EMWSessionState::Hosting ||
             SessionState == EMWSessionState::Joined)
    {
        ++SessionRequestId;
        CleanupCause = EMWCleanupCause::Leave;
        CloseServerAdmission();
        EmitSessionError(FString::Printf(TEXT("네트워크 연결 실패: %s"), *Error));
        SetSessionState(EMWSessionState::Leaving);
        StartCleanup();
    }
}
```

Leaving 중 의도적으로 연결이 끊어지는 이벤트는 새 실패 작업으로 처리하지 않는다. 원격 참가자 한 명의 정상 이탈과 호스트 월드의 네트워크 실패도 구분한다. 모든 EOS SDK 로그 경고를 이 함수의 실패와 같은 의미로 처리하지 않는다.

### 10.7 Deinitialize 교체

기존 로그인 정리를 유지하면서 확장한다.

```cpp
void UMWMultiplayerSubsystem::Deinitialize()
{
    bDeinitializing = true;
    ++LoginRequestId;
    ++SessionRequestId;

    ClearSessionTimeout();
    ClearLoginDelegate();
    ClearSessionApiDelegates();
    ClearHostRegistrationDelegate();

    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
    PostLoadMapHandle.Reset();

    if (GEngine)
    {
        GEngine->OnTravelFailure().Remove(TravelFailureHandle);
        GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
    }
    TravelFailureHandle.Reset();
    NetworkFailureHandle.Reset();

    SessionApi.Reset();
    HostUserId.Reset();
    HostWorld.Reset();
    bLoginInProgress = false;

    OnLoginFinished.Clear();
    OnSessionStateChanged.Clear();
    OnCreateRoomFinished.Clear();
    OnSessionError.Clear();

    Super::Deinitialize();
}
```

여기서 비동기 DestroySession을 시작하고 완료를 기다리지는 않는다. 정상 방 종료는 RequestLeaveRoom에서 수행한다. 프로세스 강제 종료 때 디렉터리의 방이 즉시 사라진다고 보장하지 않는다.

## 11. 에디터 연결 절차

**2026-10-28 실제 UI 연결 구조:** 아래 절차의 책임은 유지하되, 화면 배치는 Widget Blueprint, 상태 검사·버튼 처리·구독 해제는 C++ 위젯 기반 클래스에서 관리한다.

구현 후 실제 생성→종료→재생성 시험과 현재 에셋/코드 위치는 [11 방 생성·로비 UI 구현 및 PIE 검증](11_방생성_로비_UI_구현_및_PIE검증.md)에 기록했다. 이 실행 기록은 아래 원래 예제 작성 시점의 미검증 설명과 구분한다.

이후 경기 참가 버튼은 [12번 문서](12_현재방목록_참가_로비동기화_구현_및_검증.md)의 WBP_RoomBrowser로 연결했다. 현재 Root는 로그인·타이틀·생성·목록의 네 화면을 관리한다.

- `WBP_TitleRoot`의 부모는 `UMWTitleRootWidget`이다. 기존 `WBP_LoginTest`와 `WBP_Title`을 유지하고, `CreateRoomWidgetClass=WBP_CreateRoom`을 설정한다. 인증 상태를 다시 확인한 뒤 로그인·타이틀·생성 폼 중 하나를 표시한다.
- `WBP_CreateRoom`의 부모는 `UMWCreateRoomWidget`이다. `GetAvailableMapThemeIds`의 전체 ID를 선택값으로 보관하고 `FMWRoomSettings`를 구성한다. 생성 허용 조건은 로그인 완료와 `SessionState == None`이며, 작업 중 뒤로 가기도 잠근다.
- `WBP_RoomLobby`의 부모는 `UMWRoomLobbyWidget`이다. `Refresh`가 실제 GameState/PlayerState를 읽는다. 생성·표시·제거는 아래 11.4의 Controller BP 이벤트가 담당한다.
- 사용자가 이미 만든 Controller BP의 실제 경로 `/Game/BluePrints/Player/BP_MWPlayerController`를 보존했다. 새 예시 경로 `/Game/System/BP_MWPlayerController`를 중복 생성하지 않았다. `BP_MWGameMode.PlayerControllerClass`와 경기 맵 Override가 이 기존 BP를 사용한다.
- `L_Dev_Default`의 기존 로그인 시험 위젯 생성 그래프를 제거하고 PlayerStart를 추가했다. 차량 자동 생성은 하지 않는다.

`UGameMapsSettings::GetGameDefaultMap`은 패키지 이름을 반환한다. 이를 `FSoftObjectPath`로 조회할 때는 위 10절의 수정된 코드처럼 객체 경로로 변환해야 한다. 예: `/Game/Levels/L_Title` → `/Game/Levels/L_Title.L_Title`. 이 구분이 없으면 올바른 타이틀 설정도 생성 요청 전에 거절된다.

### 11.1 타이틀 맵

이 절의 완료 목표는 “L_Title에서 메뉴 UI와 마우스 클릭이 동작함”이다. 세션 생성 성공이나 경기 로비 완료를 뜻하지 않는다.

1. File → New Level → Basic으로 새 레벨을 만들고 /Game/Levels/L_Title에 저장한다. 첫 메뉴는 월드 배경/디자인을 새로 제작할 필요가 없다.
2. World Outliner에서 PlayerStart를 확인한다. 없다면 Place Actors에서 하나 추가한다. Pawn을 쓰지 않는 메뉴라도 기본 플레이어 초기화가 시작 지점을 찾으므로 배치해 둔다.
3. Content Browser의 /Game/System에서 Blueprint Class → GameModeBase를 부모로 선택하고 BP_MWTitleGameMode를 만든다. 경기용 MWGameMode를 부모로 고르지 않는다.
4. Class Defaults에서 Default Pawn Class=None, Player Controller Class=기본 PlayerController를 사용한다. 메뉴의 Controller는 필요하지만 차량 자동 스폰은 필요하지 않다.
5. BP_MWTitleGameMode에서 Handle Starting New Player를 Override한다. 부모 호출 대신 Print String("Title controller ready")만 연결한다. 기본 Pawn 재시작 경로를 실행하지 않도록 하며, 이벤트가 비어 비활성화되는 혼동도 피한다. Compile/Save한다.
6. L_Title이 열린 상태에서 Window → World Settings → GameMode Override를 BP_MWTitleGameMode로 지정하고 레벨을 저장한다. 이 맵에 메뉴 규칙을 연결하는 설정이다.
7. Project Settings → Maps & Modes의 Game Default Map을 L_Title로 지정한다. 게임 시작 맵이자 현재 Subsystem이 복귀 타이틀로 조회하는 맵이다. 첫 시험은 Editor Startup Map도 L_Title로 지정하면 에디터 진입이 편하다.
8. Packaging의 MapsToCook에 L_Title을 추가한다. 기존 L_Dev_Default는 유지한다. 에디터에 존재하는 레벨과 패키지에 포함되는 레벨은 별개다.
9. 아직 없는 /Game/UI/Lobby/WBP_Title을 UserWidget 기반으로 만든다. 우선 배경, 타이틀 Text, 생성/참가 Button만 만든다. 버튼은 Print String으로 클릭만 확인하고 실제 로그인/생성 연결은 11.3에서 진행한다.
10. WBP_Title의 Class Defaults에서 Is Focusable을 켠다. L_Title의 Open Level Blueprint에서 아래 생성·포커스 흐름을 연결한다.
11. Level Blueprint의 EndPlay에서는 저장한 TitleWidget의 Is Valid를 검사한 뒤 RemoveFromParent한다. 생성 폼은 타이틀 루트의 WidgetSwitcher/자식으로 두면 루트 제거와 함께 정리할 수 있다.
12. 모두 저장하고 플레이 설정을 Number of Players=1, Net Mode=Play Standalone으로 맞춘 뒤 시험한다. 현재 열어 둔 맵이 L_Title인지도 확인한다.

BeginPlay의 실행 핀:

```text
Event BeginPlay
 → Create Widget(Class=WBP_Title, Owning Player=Get Player Controller 0)
 → Set TitleWidget(Return Value를 Promote to Variable)
 → Add to Viewport(Target=TitleWidget)
 → Set Input Mode UI Only(Player Controller=Get Player Controller 0,
                          In Widget to Focus=TitleWidget,
                          Mouse Lock Mode=Do Not Lock)
 → Set Show Mouse Cursor(Target=Get Player Controller 0, 값=true)
```

Get Player Controller는 데이터 핀으로 연결하는 Pure 노드다. 인덱스 0은 현재 실행의 첫 로컬 플레이어이며 전 세계의 호스트 ID를 뜻하지 않는다. Create Widget은 객체 생성, Add to Viewport는 화면 표시, UI Only는 입력 전달 대상, Show Mouse Cursor는 커서 표시라는 서로 다른 일을 한다.

EndPlay의 실행 핀:

```text
Event EndPlay → Is Valid(TitleWidget) → Remove from Parent(TitleWidget)
```

타이틀 레벨이 바뀔 때 Viewport Widget이 자동으로 모두 정리된다고 가정하지 않는다. 이 절은 UI 표시/정리만 먼저 연결하며 11.3에서 Subsystem 이벤트 구독·로그인·생성 폼을 추가한다. Game Default Map은 게임의 기본 시작 맵이고 Editor Startup Map은 에디터를 켰을 때 여는 맵이다. 이미 열린 맵에서 PIE를 누르는 것만으로 Game Default Map 설정을 검증했다고 보지 않는다.

해당 에셋을 만든 뒤 Config에 반영되는 값은 다음과 같다. 없는 예시 에셋 경로를 먼저 활성화하지 않는다.

DefaultEngine.ini:

```ini
[/Script/EngineSettings.GameMapsSettings]
GameDefaultMap=/Game/Levels/L_Title.L_Title
EditorStartupMap=/Game/Levels/L_Title.L_Title
```

DefaultGame.ini의 기존 Packaging 섹션:

```ini
[/Script/UnrealEd.ProjectPackagingSettings]
+MapsToCook=(FilePath="/Game/Levels/L_Title")
+MapsToCook=(FilePath="/Game/Levels/L_Dev_Default")
```

이미 있는 MapsToCook 항목은 중복 추가하지 않는다. StartupAssetData와 테마 맵 참조는 유지한다.

타이틀에서 `OpenLevel(L_Title, ..., "listen")`을 사용하지 않는다. 메뉴는 Standalone이어야 한다.

### 11.2 경기 맵

1. AMWPlayerController 기반 BP_MWPlayerController를 /Game/System에 만든다.
2. AMWGameMode 기반 BP_MWGameMode를 /Game/System에 만든다.
3. BP_MWGameMode의 PlayerControllerClass를 BP_MWPlayerController로 설정한다. 나머지는 C++ 기본 MWGameState/MWPlayerState를 사용해도 된다.
4. /Game/Levels/L_Dev_Default의 World Settings → GameMode Override를 BP_MWGameMode로 지정한다.
5. 기존 로그인 시험 Widget을 생성하는 Level Blueprint가 있다면 타이틀로 옮기거나 제거해 중복 생성을 막는다.
6. 기본 PlayerStart가 있는지 확인한다. 첫 로비에서는 차량을 스폰하지 않는다.
7. 카탈로그는 기존 DevDefault → 이 레벨 참조를 유지한다. BP 클래스 경로나 Level Blueprint를 테마 참조에 넣지 않는다.
8. 새 테마 레벨도 같은 서버 초기화 계약을 만족하는 GameMode를 사용해야 한다.

PIE 검증에서는 `ApplyApprovedRoomSettings`의 월드 가드를 `!World || !World->IsGameWorld()`로 검사한다. UE 5.8의 `IsEditorWorld()`에는 PIE도 포함되므로, 이를 단순 거절 조건으로 사용하면 GameState에 방 설정이 적용되지 않고 Creating에서 준비 제한시간을 초과한다. 에디터 프리뷰 보호와 PIE 허용을 함께 유지한다.

### 11.3 타이틀·생성 화면

WBP_Title:

- 시작 시 현재 IsLoggedIn/IsLoginInProgress를 조회한다.
- 로그인 버튼은 RequestLogin을 호출한다.
- OnLoginFinished는 오류와 버튼 상태를 갱신한다.
- 이미 로그인되어 있으면 타이틀로 돌아왔을 때 다시 로그인하지 않는다.
- Construct에서 현재 세션 상태와 GetLastSessionError도 읽어 이전 실패 표시를 복원한다. 자신이 구독한 세션 이벤트는 Destruct에서 해제한다.
- 경기 생성 버튼은 WBP_CreateRoom을 표시한다.
- 경기 참가 버튼은 검색 구현 전에는 준비 중으로 표시한다. 가짜 성공을 만들지 않는다.

WBP_CreateRoom:

1. Construct에서 실제 GameInstance의 MWMultiplayerSubsystem을 구한다. 클래스 기본 객체를 쓰지 않는다.
2. GetAvailableMapThemeIds를 호출한다.
3. 결과 FName 배열을 Widget 변수에 보관하고 선택 컨트롤을 채운다.
4. 표시 문구와 선택 FName을 구분한다. 인덱스를 보관했다면 목록 교체 시 선택을 재검사한다.
5. 이름 입력, 2~4명 선택, 테마 선택으로 Make MWRoomSettings를 만든다.
6. 확인 버튼은 RequestCreateRoom을 호출한다.
7. OnSessionStateChanged에서 Creating/Leaving은 입력을 비활성화하고 대기 문구를 표시한다.
8. OnSessionError에서 GetLastSessionError를 표시한다.
9. CleanupRequired는 정리 재시도 버튼을 제공하고 새 생성은 막는다.
10. None에서만 생성 버튼을 활성화한다. 로그인 여부도 함께 검사한다.
11. Construct 재진입 시 자신의 이전 구독을 해제한 뒤 연결하고 Destruct에서 해제한다.
12. 생성 작업 중 뒤로 가기/취소는 첫 버전에서 비활성화한다. Widget을 닫는 것만으로 서비스 요청을 취소한 것으로 처리하지 않는다.

OnCreateRoomFinished 실패만 받아 생성 버튼을 활성화하지 않는다. 상태가 Leaving/CleanupRequired일 수 있다. 화면을 다시 열어도 현재 상태 조회로 복원한다.

### 11.4 로비 화면

BP_MWPlayerController:

- CreateLobbyScreen 이벤트에서 WBP_RoomLobby를 한 번 만들고 변수에 저장한다.
- Owning Player는 Self다. Add to Viewport, UI Only, Mouse Cursor를 설정한다.
- OnLobbyScreenRefresh 이벤트에서 Widget의 Refresh 함수만 호출한다.
- 초기 값이 아직 없으면 “로비 정보를 준비 중입니다”를 표시한다.
- CreateLobbyScreen을 Refresh 때마다 호출해 Widget을 중복 생성하지 않는다.
- BP의 EndPlay에서 보관한 WBP_RoomLobby를 RemoveFromParent하고 참조를 비운다. Viewport Widget이 맵 이동 때 자동으로 모두 사라진다고 가정하지 않는다.

WBP_RoomLobby의 Refresh:

1. Subsystem 상태가 Creating/Joining이면 준비 패널, Leaving이면 종료 패널을 표시하고 로비 조작을 막는다.
2. CleanupRequired이면 GetLastSessionError와 정리 재시도/재시작 안내를 표시한다.
3. Hosting/Joined일 때 Owning PlayerController의 PlayerState가 MWPlayerState인지 확인한다.
4. Get Game State가 MWGameState인지 확인한다.
5. 방 설정의 RoomName/MapThemeId가 비어 있으면 대기 표시를 유지한다.
6. GetRoomSettings의 이름·정원·테마를 표시한다.
7. PlayerArray를 순회해 MWPlayerState의 PlayerName·IsRoomHost·IsReady를 표시한다.
8. 인원 표시는 호스트 포함이다. PlayerArray 인덱스를 영구 사용자 ID로 쓰지 않는다.
9. 나가기/방 종료는 Hosting/Joined에서만 Subsystem.RequestLeaveRoom을 호출한다.
10. 첫 구현에서 Ready/경기 시작 버튼은 비활성화한다. 다음 서버 요청 구현 때 연결한다.
11. 서버의 복제 상태와 호스트 표시 갱신을 모두 확인한다.

## 12. 순서대로 빌드·검증하기

| 묶음 | 작성 내용 | 이때 확인할 것 |
|---|---|---|
| A | 공용 타입 이동, AssetManager ID 열거, 모듈 | UHT 포함 Editor/Game 빌드. 기존 13개 설정 검사 유지 |
| B | Subsystem 새 선언과 공통 함수, Framework 네 클래스 작성 | B~C는 서로의 함수를 호출하므로 작성 단위. 함께 완성한 뒤 빌드 |
| C | 생성/등록/맵/공개/정리 콜백 전체 | B~C의 전체 정의를 연결한 뒤 Editor/Game 빌드. 종료·실패 경로까지 포함 |
| D | 타이틀/경기 맵과 BP/UI 연결 | 올바른 GameMode/Controller 생성, Widget 중복 없음 |
| E | 1인 생성→종료→재생성 | 성공 상태와 정리 상태 검증 |
| F | 서로 다른 사용자·별도 프로세스 | 검색/실제 참가를 추가한 뒤 인원·전송 검증 |

리플렉션 타입 이동/추가 후에는 에디터를 닫고 정상 빌드한다. 예:

```powershell
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat' MWEditor Win64 Development '-Project=C:/Users/user/Desktop/GitProject/ProjectMW/MW.uproject' -WaitMutex -NoHotReloadFromIDE
```

이후 Game 타깃도 확인한다.

```powershell
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat' MW Win64 Development '-Project=C:/Users/user/Desktop/GitProject/ProjectMW/MW.uproject' -WaitMutex -NoHotReloadFromIDE
```

### 12.1 첫 실행 로그와 기대 결과

다음 단계에 RequestId·세션 이름·테마 ID·맵 패키지·실제 NetDriver 클래스를 기록한다. 자격 증명은 기록하지 않는다.

```text
Create accepted
CreateSession completed
Approved map load requested
Host registration completed
Listen world ready
UpdateSession completed
State = Hosting
DestroySession completed
Title map loaded in Standalone
State = None
```

위 줄은 권장 로그 항목이다. 참조 코드에 아직 없는 세부 로그는 해당 함수에 추가한다. 콜백 순서 중 Host registration과 맵 완료는 서로 바뀔 수 있다.

### 12.2 인수 체크리스트

| 시험 | 기대 결과 |
|---|---|
| GetAvailableMapThemeIds | 등록된 DevDefault 테마만 표시, 입력 에셋 ID 제외 |
| 빈 이름·정원 오류·미등록/비레벨 테마 | 생성 요청 이전에 명시 거절 |
| 이름에 앞뒤 공백 | 승인 결과의 정리된 이름 사용 |
| 생성 연타 | 세션 한 개, 생성 콜백/맵 이동 한 번 |
| 정상 생성 | 카탈로그에서 고른 맵, 실제 Listen Server |
| 호스트 등록 | EOS 등록 완료 성공, 로컬/서비스 인원에서 호스트 포함 |
| 정원 3 | 호스트 이후 참가 가능 자리는 2명 |
| 준비 중 방 검색 | MW_ROOM_STATUS=Lobby 검색에 노출되지 않음 |
| 타이틀 복귀 | Standalone, 로그인 유지, 이전 맵/Widget/Delegate 잔존 없음 |
| 같은 프로세스 재생성 | 이전 named session 충돌 없음 |
| 공개/맵/등록 실패 | Hosting으로 진행하지 않고 정리 또는 CleanupRequired |
| 늦은 생성·등록·공개 성공 | 실패한 이전 요청을 다시 로비 성공으로 표시하지 않음 |
| 정리 실패·제한시간 | None으로 속이지 않고 복구 안내 |
| PIE 2개 인스턴스 | 다른 GameInstance의 이동/실패 이벤트 무시 |
| 패키지 실행 | 타이틀·카탈로그·입력 참조·테마 레벨 포함 확인 |

EOS Sessions 서비스 인원과 GameState PlayerArray 인원은 별도로 확인한다. 로컬 RegisteredPlayers 배열은 서비스 등록 완료의 대체 증거가 아니다. 만석·입장 경합·원격 시작 거절 등은 이후 실제 참가/Ready 단계에서 추가한다.

## 13. 검색·참가·Ready를 붙일 때 유지할 계약

이번 문서의 추가 코드 범위는 호스트 방 생성·공개·종료다. 전체 사진의 “경기 참가 → 로비 Ready/호스트 시작”은 아래 다음 작업에서 확장한다. 차량 Character나 건설 구현으로 대체하지 않는다.

### 13.1 검색

- RequestFindRooms, OnFindRoomsFinished, FOnlineSessionSearch 보관 멤버, 검색 Delegate Handle, 검색 세대를 추가한다.
- 같은 EOS Bucket, MW_PROTOCOL_ID, MW_ROOM_STATUS=Lobby 조건을 사용한다.
- bUseLobbiesIfAvailable=false인 생성 경로와 동일한 Sessions 경로로 검색한다.
- 각 행은 SessionId와 검색 세대에 연결한다. 같은 방 이름이나 오래된 행 인덱스로 참가하지 않는다.
- 서비스 속성의 테마 문자열을 FName으로 해석하고 자신의 AssetManager로 해석 가능한지 확인한다.
- 검색용 맵 검사는 클라이언트의 호환성 검사다. 클라이언트가 광고 문자열을 임의 맵 이동 경로로 실행하지 않는다.

### 13.2 참가

- RequestJoinRoom, Join 완료 Handle, 연결/초기 상태 준비 제한시간을 추가한다.
- 현재 검색 결과 원본 FOnlineSessionSearchResult로 JoinSession을 호출한다.
- 성공 후 GetResolvedConnectString → 소유 로컬 Controller.ClientTravel을 사용한다.
- 참가자는 자기 카탈로그로 임의 OpenLevel("listen")을 하지 않는다. 선택한 서버의 연결 주소로 이동한다.
- 실제 월드·본인 Controller/PlayerState·GameState 준비가 확인된 뒤 Joined로 전환한다.
- 참가 요청 수락 때 SessionApi와 프로젝트 타이틀 맵도 보관해 기존 나가기 정리를 재사용한다.
- 연결 실패 후 로컬 참가 상태를 정리한다. 네트워크 실패 콜백의 World/Driver가 해당 참가 작업인지 다시 검사한다.

### 13.3 Ready와 시작

- Controller에 소유자 Server RPC를 추가하고 GameMode의 일반 함수로 전달한다.
- Ready는 해당 Controller의 PlayerState만 서버에서 변경한다.
- 시작은 실제 호스트·현재 Lobby·최소 2명·현재 전원 Ready·필수 준비를 확인한다.
- 시작 승인 경로에서 입장을 닫고 참가자를 확정한 다음 서비스 상태도 갱신한다.
- 서비스 갱신 전 도착한 접속도 서버가 거절한다.
- 이번 최소 GameMode의 bAcceptingPlayers는 이후 서버 Phase 검사와 연결한다.
- 실제 Character/코스 준비를 확인하는 Countdown은 별도 단계다.

## 14. 흔한 오류와 이번 문서 검증 범위

| 증상 | 먼저 확인할 것 |
|---|---|
| GetAvailableMapThemeIds가 비어 있음 | 태그·StartupAssetData·카탈로그 검사·레벨 타입, 에디터 재시작 |
| Dev_Default를 넣으면 거절 | 전체 ID Asset.MapTheme.DevDefault 사용 |
| 현재 코드에서 생성 버튼이 아무 일도 안 함 | RequestCreateRoom 본문이 아직 비어 있는지 |
| 새 타입 중복/UHT 오류 | 기존 enum/구조체를 이동하며 이중 정의했는지, generated.h 순서 |
| 맵은 열리지만 Hosting이 안 됨 | GameMode Override, MWPlayerState/UniqueId, 호스트 등록 콜백, NetDriver, 공개 콜백 |
| 호스트 인원이 포함되지 않음 | 엔진 GameSession 등록 경로와 EOS 등록 결과, 무조건 중복 등록하지 않기 |
| 로비 설정은 보이는데 원격에는 없음 | DOREPLIFETIME, RepNotify, 실제 서버 연결 여부 |
| 방 종료 뒤 메뉴도 Listen 상태 | 절대 맵 이동·빈 옵션·Standalone 확인 |
| 새 방 생성 시 Already exists | None만 바꿨고 Destroy 완료를 확인하지 않은 흔적 |
| CleanupRequired에서 재시도가 막힘 | 아직 콜백/맵 작업이 진행 중인지, 종료 결과 불명 상태인지 |
| 프로세스 한 개에서 여러 방을 기대 | 첫 범위는 실행 인스턴스 한 개당 활성 방 한 개 |

2026-10-28 앞선 검토 시점의 기반 구현에서 Editor/Game 빌드와 카탈로그 기반 입력 검사 13개를 재확인했다. 작성 중 감지한 사용자의 후속 Native 태그 변경은 그 빌드 기록과 구분한다. **이 문서에 새로 제시한 코드는 Source에 반영하지 않았으며 컴파일·세션·3PC·Cook 검증을 수행한 코드가 아니다.** 문서 변경 검증은 링크·코드 선언/정의 대응·현재 API/파일과의 정합성이다. 위 인수 체크리스트는 직접 구현 후 수행할 시험이다.

근거:

- 테마 데이터 계약: [06 공용 데이터 이식 6.3절](06_공용_데이터_이식.md).
- 기능 완료 조건: [09 타이틀부터 게임플레이까지](09_타이틀부터_게임플레이까지_마일스톤.md).
- Framework 역할: [08 리슨 서버 설명서](08_리슨서버_구조와_Gameplay_Framework.md).
- 세션 생성/갱신/종료의 완료 Delegate 개요: [Epic Session Interface](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-session-interface-in-unreal-engine).
- GameMode 서버 전용 역할·InitGame/PostLogin/기본 Pawn 스폰: [Epic GameMode and GameState](https://dev.epicgames.com/documentation/en-us/unreal-engine/game-mode-and-game-state-in-unreal-engine).
- 세부 함수·EOS 공개 권한·등록 콜백·맵 완료 순서는 현재 설치 UE 5.8.4의 헤더/소스와 대조했다. 문서 예제 자체의 빌드/서비스 시험을 대신하지 않는다.



