# 방 생성·로비 UI 구현 및 PIE 검증

검증일: 2026-10-28. 분야: Client + Server. 엔진: UE 5.8.4.

> 이 문서는 최초 방 생성·종료 단계의 검증 기록이다. 이후 방 목록·실제 참가·로비 인원/ID 동기화를 구현하고 두 EOS 계정으로 검증했다. **현재 상태와 재현 절차는 [12번 문서](12_현재방목록_참가_로비동기화_구현_및_검증.md)를 함께 따른다.** 아래의 참가 미구현·Root 자식 3개 등의 설명은 이 문서의 최초 검증 시점 기준이다.

## 1. 현재 완료 범위

다음 흐름을 실제 EOS Sessions와 1인 PIE에서 확인했다.

```text
L_Title
 → 기존 WBP_LoginTest에서 실제 로그인
 → WBP_Title
 → 경기 생성
 → WBP_CreateRoom에서 이름·정원·전체 테마 ID 선택
 → Creating: 입력·확인·뒤로 가기 잠금
 → AssetData/AssetManager가 지정한 L_Dev_Default로 Listen 이동
 → 서버 방 설정 적용·호스트 EOS 등록·방 공개 확인
 → Hosting: WBP_RoomLobby 한 개 표시
 → 방 종료
 → Leaving: 로비 조작 잠금
 → 세션 정리·Standalone 타이틀 복귀·None
 → 로그인 유지 상태에서 재생성
```

첫 성공 방은 정원 3명, 재생성한 방은 정원 4명으로 시험했다. 두 방 모두 정상 종료했다. 시험 종료 시 PIE도 중지했다.

경기 참가 버튼은 준비 중 안내를 표시한다. 검색·참가·Ready 변경·경기 시작·차량 스폰은 이번 구현에 포함하지 않는다. 로비의 Ready와 경기 시작 버튼은 비활성화되어 있다.

## 2. 에셋과 코드의 역할

| 대상 | 실제 위치와 역할 |
|---|---|
| 생성 폼 | `/Game/UI/Lobby/WBP_CreateRoom`: 화면 배치. 부모 `UMWCreateRoomWidget` |
| 로비 | `/Game/UI/Lobby/WBP_RoomLobby`: 화면 배치. 부모 `UMWRoomLobbyWidget` |
| 메뉴 진입·화면 전환 | `/Game/UI/Lobby/WBP_TitleRoot`: 부모 `UMWTitleRootWidget`. 기존 로그인·타이틀 위젯 유지 |
| 경기 Controller | `/Game/BluePrints/Player/BP_MWPlayerController`: 기존 사용자 에셋 경로 보존 |
| 경기 GameMode | `/Game/System/BP_MWGameMode`: 위 Controller BP를 사용 |
| 경기 맵 | `/Game/Levels/L_Dev_Default`: GameMode Override 연결, LobbyPlayerStart 추가, 이전 로그인 시험 생성 그래프 제거 |

C++ 기반 클래스는 [MWCreateRoomWidget.h](../../../Source/MW/UI/MWCreateRoomWidget.h), [MWTitleRootWidget.h](../../../Source/MW/UI/MWTitleRootWidget.h), [MWRoomLobbyWidget.h](../../../Source/MW/UI/MWRoomLobbyWidget.h)에 있다. 각 `.cpp`가 버튼 처리·상태 조회·이벤트 수명을 담당한다.

Widget Blueprint에서 디자인을 바꿀 때 `BindWidget`에 사용한 아래 이름과 타입을 유지한다. 위젯 이름을 바꾸면 C++ 멤버와 함께 변경하고 다시 컴파일해야 한다.

### WBP_CreateRoom

| 이름 | 타입 | 의미 |
|---|---|---|
| Input_RoomName | EditableTextBox | 방 이름 입력 |
| Select_MaxPlayers | ComboBoxString | `2`, `3`, `4`; 기본 `3` |
| Select_MapTheme | ComboBoxString | 카탈로그에서 받은 전체 ID 목록 |
| Button_CreateRoom | Button | 생성 확인 |
| Button_Back | Button | 작업이 없는 상태에서 타이틀로 복귀 |
| Button_RetryCleanup | Button | CleanupRequired에서만 표시 |
| Text_Status | TextBlock | 현재 진행 상태 |
| Text_Error | TextBlock | 입력·세션 오류 |

`AvailableMapThemeIds`와 `SelectedMapThemeId`가 전체 FName ID를 보관한다. 현재 실제 반환값은 `Asset.MapTheme.DevDefault` 한 개다. `HandleCreateClicked`는 이를 GameplayTag로 변환하고 `FMWRoomSettings`의 RoomName/MaxPlayers/MapThemeId를 채워 `RequestCreateRoom`에 전달한다. 위젯은 경기 레벨 경로를 보관하거나 직접 OpenLevel하지 않는다.

생성 허용 조건은 로그인 완료, 로그인 작업 없음, `SessionState == None`, 유효한 테마 선택이다. `IsSessionBusy == false`를 생성 허용 조건으로 사용하지 않는다. Creating/Leaving 및 다른 세션 상태에서는 생성을 막고, CleanupRequired에서는 재시도 안내를 표시한다. Construct에서 현재 상태와 마지막 오류를 읽고 Destruct에서 자신의 구독을 해제한다.

### WBP_TitleRoot

기존 WidgetSwitcher의 자식 `LoginScreen`, `TitleScreen`을 유지한다. `CreateRoomWidgetClass`에 WBP_CreateRoom을 지정해 런타임에 생성 폼을 한 번 만들고 세 번째 화면으로 추가한다. 타이틀의 생성 버튼과 폼의 뒤로 가기를 C++에서 연결한다.

로그인·세션 이벤트는 현재 Subsystem 상태를 다시 읽는 데 사용한다. 콜백의 성공 bool만으로 인증이나 방 준비 성공을 확정하지 않는다. 기존 Root의 Blueprint 로그인 전환 그래프는 Native 로직과 중복 실행되지 않도록 제거했다. 기존 WBP_Title의 클릭 확인용 Print String 그래프도 실제 메뉴 연결로 교체했다.

### WBP_RoomLobby와 Controller

Controller BP의 `LobbyWidget` 변수는 `UMWRoomLobbyWidget` 참조다.

```text
CreateLobbyScreen
 → LobbyWidget 유효성 검사
 → 없을 때만 WBP_RoomLobby 생성, Owning Player = Self
 → 변수 보관 → Add to Viewport → UI Only → 커서 표시

OnLobbyScreenRefresh
 → 기존 LobbyWidget이 유효하면 Refresh 호출

EndPlay
 → 유효한 LobbyWidget 제거 → 참조 초기화
```

로비의 Refresh는 실제 `AMWGameState.GetRoomSettings`, `PlayerArray`, 각 `AMWPlayerState.IsRoomHost/IsReady`를 읽는다. Creating/Joining에서는 준비 표시, Leaving에서는 종료 표시를 사용한다. 방 설정과 본인 PlayerState가 준비된 Hosting/Joined에서 방 정보를 표시한다. 방 종료·나가기는 기존 `RequestLeaveRoom`에 연결했다.

## 3. 실행을 막던 문제 수정

### 타이틀 경로 검사

`UGameMapsSettings::GetGameDefaultMap`은 객체 경로가 아닌 패키지 이름을 반환한다. 기존 코드는 이를 그대로 AssetManager의 객체 경로 조회에 넘겨, 올바르게 설정된 L_Title도 거절했다.

`ResolveTitleMapPackageName`에서 패키지 이름을 `/Game/Levels/L_Title.L_Title` 형태로 변환한 뒤 조회하도록 수정했다. 10번 가이드와 `Examples/RoomHosting/MWMultiplayerSubsystem.cpp`의 동일 예제도 갱신했다.

### PIE 방 설정 적용

UE 5.8의 `UWorld::IsEditorWorld()`는 PIE에서도 true다. 기존 `AMWGameMode::ApplyApprovedRoomSettings`의 거절 조건 때문에 PIE에서 승인 방 설정이 적용되지 않았다.

검사를 `!World || !World->IsGameWorld()`로 바꿔 에디터 프리뷰를 보호하면서 PIE와 실제 게임 월드를 허용했다. 수정 후 서버의 RoomSettings와 로비 표시가 일치했다.

### EOS 개발 정책

실제 서비스가 호스트 등록을 `403 policy_missing_action`, `missing matchmaking:managePlayers`로 거절했다. 사용자 승인 후 다음 설정을 변경했다.

- 제품: ProjectMW.
- 클라이언트: MW_Windows_Dev.
- 정책: MW_P2P_Dev, Peer2Peer → Custom.
- 추가한 작업: `Sessions / matchmaking:managePlayers` 하나.
- 기존 기능 스위치 및 61개 작업의 기존 선택 상태를 대조해 보존했다. 기존 `User required`도 유지했다.
- 저장 후 정책을 다시 열어 Custom, User required, managePlayers와 기능별 작업 수를 확인했다.

Custom 전환 시 포털이 권한을 초기화하므로, 단순히 유형만 바꾸고 저장하면 기존 기능이 빠질 수 있다. 이번에는 기존 선택값을 복원한 뒤 변경 차이가 managePlayers 한 개임을 확인했다. 정책 이름은 유지했다. 별도 비밀 키를 새로 만들거나 변경하지 않았다.

## 4. 수행한 검증

| 시험 | 실제 결과 |
|---|---|
| Editor/Game 정상 빌드 | 최종 소스로 두 타깃 성공. 에디터 종료 후 빌드·재시작 |
| 관련 Blueprint | 생성 폼·로비·Root·타이틀·Controller·GameMode 모두 BS_UP_TO_DATE |
| 미로그인 | 생성 불가, 로그인 화면 표시 |
| 폼 초기값 | 정원 3명, 전체 테마 ID 선택 확인 |
| 잘못된 방 이름 | 공백 이름 거절, 오류 표시, None 유지 |
| 뒤로 가기·재진입·경기 참가 | 폼 재진입 가능, 참가 준비 중 안내 |
| 생성 중 | Creating, 이름·생성 확인·뒤로 가기 잠금. 생성 중복 호출 차단 |
| 첫 정상 방 | Listen + Hosting, 서버 이름 앞뒤 공백 정리, 정원 3, 테마 일치 |
| 실제 NetDriver | `/Script/SocketSubsystemEOS.NetDriverEOS` 로그 확인 |
| 참가자 | 호스트 포함 1명, 호스트 플래그 true, 준비 대기 |
| 로비 중복 방지 | 생성 이벤트 재호출·Refresh 반복 후에도 위젯 1개 |
| 종료 | Leaving과 종료 버튼 잠금 확인, Standalone L_Title·None 복귀 |
| 재생성 | 같은 PIE/GameInstance에서 다른 이름·정원 4로 Hosting 재진입 |
| 재종료 | 이전 로비 0개, 타이틀 1개, None·빈 오류·로그인 유지 |
| UI 제거·재표시 | Root와 폼의 이벤트 구독 해제·재연결, 자식 3개 유지, 재로그인 없음 |
| 실패 복구 | 권한 거절 및 준비 제한시간 초과 시 실제 정리·타이틀 복귀 확인 |
| 최종 로그 | Blueprint Runtime Error, Accessed None, Assertion failed, Ensure condition failed, Blueprint 컴파일 오류 미검출 |

UI 동작 시험은 Unreal MCP에서 실제 Widget의 OnClicked 이벤트와 Subsystem/Actor/Widget 인스턴스를 사용했다. 최종 로비의 표시값도 실제 TextBlock에서 읽었다. 중간 생성 폼은 화면으로 확인했으나, Esc로 화면 제어가 중지되어 최종 로비의 픽셀 화면 검사는 진행하지 않았다. 이 제한을 실제 상태·위젯 검증과 구분한다.

CleanupRequired의 생성 금지·재시도 분기는 코드에서 확인했으며, 최종 빌드에서 정리 서비스 실패를 강제로 유발한 시험은 하지 않았다. 2~4명 실제 접속, 검색·참가, 3PC, 패키징·Cook은 미실행이다.

세부 JSON 증거와 수정 전 에셋 사본은 `Saved/RoomLobbyIntegration/`에 있다. Content는 현재 저장소 설정에 따라 Plastic SCM에서 관리되며 대상 파일에 충돌이나 타인의 체크아웃이 없었다. Git 커밋·푸시와 Plastic 체크인은 수행하지 않았다.

## 5. 직접 다시 시험하는 순서

1. 에디터에서 `L_Title`을 연다.
2. **New Editor Window PIE / Players=1 / Net Mode=Play Standalone / Run Under One Process=켬 / Dedicated Server=끔**으로 실행한다.
3. 로그인 후 경기 생성을 누른다.
4. 방 이름을 입력한다. 기본 정원 `3`, 테마 `Asset.MapTheme.DevDefault`를 확인하고 생성 확인을 누른다.
5. 준비 후 로비에서 방 이름, `1 / 3명`, 테마, 호스트·준비 대기를 확인한다. Ready와 경기 시작은 비활성이 정상이다.
6. **방 종료**를 눌러 타이틀로 돌아간다. Stop으로 방 종료 절차를 대체하지 않는다.
7. 재로그인 없이 새 방을 만들고, 정원을 `4`로 바꿔 `1 / 4명` 표시를 확인한다.
8. 다시 방 종료 후 PIE를 Stop한다.

다음 구현은 방 검색·실제 참가이며, 이후 여러 사용자/실행 인스턴스로 정원·여러 방·이탈을 검증한다.
