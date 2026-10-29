# 최초 호스팅 단계의 전체 파일 참조본

이 폴더의 두 파일은 10번 가이드 작성 당시의 **로그인·맵 테마 검사·방 생성·종료** 코드를 합친 기록용 참조본이다. 방 목록·참가 구현을 포함하지 않으며 현재 Source 전체본과 다르다.

**현재 Source를 이 폴더의 파일로 교체하지 않는다.** 새 API와 참가 상태·정리 코드가 사라지고 현재 GameMode/Controller/UI의 호출과 맞지 않게 된다. 현재 실제 구현 전체는 다음 세 파일을 함께 읽는다.

- [MWMultiplayerSubsystem.h](../../../../../Source/MW/System/MultiplayerSubSystem/MWMultiplayerSubsystem.h): 현재 선언 전체.
- [MWMultiplayerSubsystem.cpp](../../../../../Source/MW/System/MultiplayerSubSystem/MWMultiplayerSubsystem.cpp): 현재 로그인·생성·공통 정리.
- [MWMultiplayerRooms.cpp](../../../../../Source/MW/System/MultiplayerSubSystem/MWMultiplayerRooms.cpp): 같은 클래스의 목록·참가 구현.

현재 에셋 연결·서버 계약·실제 두 계정 검증 결과는 [12번 문서](../../12_현재방목록_참가_로비동기화_구현_및_검증.md)를 따른다. 현재 GameState의 ApplyRoomSettingsFromServer(Settings, SessionId)는 두 인자를 받는다. 아래 적용 위치와 주석 해제 안내 등은 최초 호스팅 참조본 작성 시점의 기록이다.

- [MWMultiplayerSubsystem.h](MWMultiplayerSubsystem.h): 헤더 전체.
- [MWMultiplayerSubsystem.cpp](MWMultiplayerSubsystem.cpp): 구현 전체.
- [10번 구현 가이드](../../10_AssetManager_맵테마_기반_방생성_구현가이드.md): 클래스/멤버/콜백의 의미와 에디터 연결.

## 적용할 위치

```text
Source/MW/System/MultiplayerSubSystem/MWMultiplayerSubsystem.h
Source/MW/System/MultiplayerSubSystem/MWMultiplayerSubsystem.cpp
```

위 경로는 최초 참조본의 적용 대상이었다. 현재는 위의 최신 소스 링크를 사용한다. 예제 폴더를 Source 아래에 별도 복사해 같은 클래스를 두 벌 만들지 않는다.

## 포함 범위

- EOS accountportal 로그인, 결과/중복 요청/Delegate 정리.
- AssetData/AssetManager 카탈로그를 통한 테마 목록과 레벨 검사.
- 세션 생성, 엔진 기본 호스트 등록 완료 관찰, Listen 맵 준비, 방 공개.
- 종료, 타이틀 복구, 제한시간, 늦은 콜백과 정리 확인 불가 상태.
- 검색/참가/Ready/경기 시작 기능의 본문은 포함하지 않는다. 이 단계의 호스트 생성·종료 범위다.

## 이미 있어야 하는 파일과 API

현재 저장소에서 확인한 경로에 맞췄다.

| 파일 | 필요한 항목 |
|---|---|
| System/MultiplayerSubSystem/MWMultiplayerTypes.h | EMWSessionState(CleanupRequired 포함), FMWRoomSettings |
| System/MWAssetManager.h/.cpp | HasCatalog, FindAssetPath, GetAssetDataForPath, GetAssetIdsUnderRoot |
| System/MWGameMode.h/.cpp | GetApprovedRoomSettingsForWorld 호출, ExpectHostRegistration 호출, TryPrepareLobby, SetLobbyAdmission |
| System/MWGameState.h/.cpp | GetRoomSettings, 방 설정 복제 |
| System/MWPlayerState.h/.cpp | 엔진 기본 UniqueId/등록과 로비 상태 |
| Player/MWPlayerController.h/.cpp | OnSessionStateChanged/OnSessionError를 구독하는 로컬 UI |

MW.Build.cs의 Private 의존성은 AssetRegistry, EngineSettings, OnlineSubsystemEOS를 포함해야 한다. 현재 파일에는 이미 들어 있다.

## 함께 확인할 세 가지

1. 현재 MWPlayerController.cpp의 BeginPlay/EndPlay에는 세션 이벤트 구독/해제 블록이 주석 처리되어 있다. 전체 Subsystem을 연결한 뒤 이 두 블록의 주석을 풀어 UI 갱신을 연결한다.
2. 프로젝트 기본 맵은 별도의 L_Title이어야 한다. 테마의 경기 맵과 기본 타이틀 맵이 같으면 생성 요청을 거절한다. 존재하는 맵을 만든 뒤 설정/Cook 목록을 갱신한다.
3. 테마 ID와 기존 구조체 이름은 그대로 사용한다. Asset.MapTheme.DevDefault를 카탈로그에서 해석하며 Subsystem에 테마 경로 표를 추가하지 않는다.

## 검증 범위

선언/정의 대응, 현재 Framework의 호출 이름, include 경로, 기존 가이드와의 코드 대응을 검사했다. 이 폴더의 전체본을 실제 Source로 교체해 UHT/Editor/Game 빌드하거나 EOS 생성/접속을 실행한 결과는 아니다. 교체 후 에디터를 닫고 정상 빌드한 다음 생성→종료→재생성을 시험한다.

