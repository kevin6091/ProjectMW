# Frontend 디자인과 MW 방 참가 기능 통합

작업·검증일: 2026-10-29. 분야: Client + Server. UE 5.8.4.

> 후속 정리: 사용자 요청으로 통합에 사용하지 않는 Frontend 원본 WBP·L_Frontend·테스트 프리뷰·RoadButton WBP, Frontend 런타임 클래스와 MWFrontendAuthoring 플러그인을 삭제했다. 미사용 MenuDiamond/MenuOverlay 텍스처도 제거했다. 아래 원본 분석과 테스트 기록은 통합 당시의 이력이며, 현재 실행은 `L_Title`과 `Content/UI/Lobby`의 MW 위젯 7개다. 정식 UI가 참조하는 Common 이미지·폰트는 유지한다.

후속 삭제 검증: 에셋 8개·런타임 소스 10개·제작 플러그인의 소스/Build.cs/descriptor 9개가 디스크에서 제거됐다. 보존 대상 128개 파일의 SHA256은 동일하다. 삭제 후 Editor/Game 빌드 성공, 새 에디터에서 정식 WBP 7개·실제 Framework BP·맵 로드, 전체 `/Game`의 삭제 에셋 참조 없음, 제거 클래스 미등록 및 타이틀 PIE의 Root 1개·4화면·기본 정원 3·AssetManager 테마를 확인했다. 삭제 후 2계정 EOS 접속을 다시 실행한 것은 아니다. 검증 근거는 `Saved/FrontendCleanup`에 있다.

플러그인의 `Binaries`/`Intermediate` 생성 캐시 32개는 자동 승인 검토가 파일 삭제 명령을 차단해 남아 있다(구체 사유 미제공). `.uplugin`과 소스는 제거됐으며 새 에디터에 해당 플러그인 클래스가 등록되지 않는 것을 확인했다. 생성 캐시는 현재 기능의 소스·에셋이 아니다.

## 적용한 기준

사용자 지시에 따라 **Frontend의 Designer 구성·기존 이미지·폰트만 사용하고, 기능은 기존 MW 구현을 유지**했다. FrontendConnection 어댑터를 만들거나 Frontend의 화면 상태·입력 검증·세션 의도 전달 코드를 연결하지 않았다.

정식 진입점은 계속 `/Game/Levels/L_Title`이다. 원본 검토용 `/Game/Levels/L_Frontend`는 후속 정리에서 삭제했다. 기본 맵·GameMode·Controller 설정을 바꾸지 않았다.

## 실제 수정한 화면

기존 경로와 부모 클래스를 유지하고 Designer를 교체했다. 따라서 기존 레벨과 BP_MWPlayerController가 같은 에셋을 생성한다.

| 기존 MW 에셋 | 적용한 Frontend 표현 | 기능 정본 |
|---|---|---|
| `/Game/UI/Lobby/WBP_LoginTest` | FrontendLogin의 표지판·고지 프레임·로그인 배치 | 기존 로그인 EventGraph와 MWMultiplayerSubsystem |
| `/Game/UI/Lobby/WBP_Title` | 기울어진 생성·참가 표지판 | MWTitleRootWidget |
| `/Game/UI/Lobby/WBP_TitleRoot` | 로그인·타이틀 RenderScale 정상화 | 기존 4화면 생성·전환·구독 |
| `/Game/UI/Lobby/WBP_CreateRoom` | 방 이름·정원·테마·확인/뒤로 배치 | MWCreateRoomWidget |
| `/Game/UI/Lobby/WBP_RoomBrowser` | 참가 제목·목록·선택 안내·하단 버튼 | MWRoomBrowserWidget |
| `/Game/UI/Lobby/WBP_RoomListEntry` | 노란 표지판, 파란 선택, 기존 폰트 | MWRoomListEntryWidget |
| `/Game/UI/Lobby/WBP_RoomLobby` | 4인 표지판·방 정보·나가기·Ready/시작 배치 | MWRoomLobbyWidget |

원본 `/Game/UI/Frontend` WBP와 `/Game/UI/Common` 리소스는 수정하지 않았다. 새 이미지 임포트도 없다. 로그인 EventGraph는 교체 전후 export SHA256이 동일하다.

Frontend의 RoadButton UserWidget은 **기본 UButton + TextBlock + Image**로 풀어서 Designer에 배치했다. 따라서 MWRoadButtonWidget C++ 이벤트나 애니메이션 처리도 실행하지 않는다. 방 목록은 Frontend ListView 대신 기존 MW의 ScrollBox/Box_Rooms/행 위젯 구조를 사용한다. 화면 디자인은 가져오되 선택·요청 책임은 옮기지 않는 방식이다.

원본 Frontend의 시간 기반 버튼 애니메이션·마우스 기울기 C++는 이식하지 않았다. 기본 UButton의 Normal/Hovered/Pressed/Disabled 브러시 상태는 유지한다. 화면은 1920×1080 디자인 영역을 ScaleBox로 맞추며, 실제 맵 배경은 기존 L_Title/L_Dev_Default를 사용한다.

## 실제 호출과 동기화 경로

```text
L_Title → 기존 WBP_LoginTest의 실제 EOS 로그인
 → MWTitleRootWidget → Frontend 디자인의 경기 참가 버튼
 → MWRoomBrowserWidget::OpenRoomList
 → MWMultiplayerSubsystem::RequestRefreshRooms → EOS FindSessions
 → FMWRoomInfo 목록 → 기존 MWRoomListEntryWidget
 → UButton 클릭 → 세션 ID + 목록 리비전 선택
 → MWRoomBrowserWidget::HandleJoin
 → MWMultiplayerSubsystem::RequestJoinRoom
 → EOS JoinSession → 접속 주소 확인 → ClientTravel
 → 기존 MWGameMode / MWGameState / MWPlayerState / MWPlayerController
 → 맵·세션·테마·본인 등록 확인 → Joined
 → 기존 WBP_RoomLobby를 한 번 표시 → 복제된 값으로 Frontend 표지판 갱신
```

이 경로에서 MWFrontendWidget, MWFrontendConnection, MWFrontendPlayerController, MWFrontendGameMode를 사용하지 않는다. Frontend 테스트 fixture도 설치하지 않았다. 해당 미사용 클래스·원본 에셋·전용 제작 플러그인은 통합 후 삭제했다.

방 생성의 테마는 그대로 `GetAvailableMapThemeIds` → 전체 `Asset.MapTheme.DevDefault` ID → FMWRoomSettings → 기존 AssetData/AssetManager 해석을 사용한다. Widget에서 레벨 경로를 만들지 않는다.

## C++ 변경의 한계

이번 작업에서 새로 바꾼 런타임 소스는 두 파일이다.

- [MWRoomListEntryWidget.cpp](../../../Source/MW/UI/MWRoomListEntryWidget.cpp): 선택 여부를 기존 Frontend의 노랑/파랑 팔레트로 표시한다. 세션 ID·리비전·선택 이벤트는 그대로다.
- [MWRoomLobbyWidget.cpp](../../../Source/MW/UI/MWRoomLobbyWidget.cpp): 기존 MWGameState.PlayerArray를 방 안의 ID 순으로 정렬해 ParticipantSlot/ParticipantState/EmptySlotIcon/ParticipantBoard 0~3에 표시한다. 서버 상태를 쓰거나 새 참가자를 생성하지 않는다.

방 정보는 MWGameState.GetRoomSettings, 참가 인원은 PlayerArray.Num, 이름·ID·호스트·Ready·등록 확인은 각 MWPlayerState에서 읽는다. ID는 EOS 계정 ID가 아니라 서버가 발급한 현재 방 안의 번호다. 빈 슬롯과 정원 밖 슬롯도 구분한다. Ready와 경기 시작은 기존처럼 비활성 상태다.

Subsystem·목록/참가 cpp·GameMode·GameState·PlayerState·Controller의 기능 변경은 없다. 작업 시작 전 있던 Frontend include 수정 2건과 Subsystem의 서식 수정도 그대로 보존했다.

## 교차 검증 결과

실제 두 EOS 계정으로, 같은 PC의 **독립 Standalone PIE 두 창**에서 검증했다. 미리 연결하는 Listen PIE 모드나 가짜 방 fixture를 쓰지 않았다.

| 검증 | 실제 결과 |
|---|---|
| 기존 로그인 → 타이틀 | 실제 로그인 상태 확인 후 Frontend 선택 화면 표시 |
| 생성 입력 검증 | 공백 이름은 기존 MW 오류를 표시하고 None 유지 |
| 실제 방 생성 | MW Frontend Integration / 3명 / Asset.MapTheme.DevDefault로 Hosting |
| 현재 방 목록 | 실제 EOS 결과 1개, 1/3명 표시. 선택 전 참가 비활성 |
| 행 선택 | 선택한 방 안내·파란 행·참가 활성화 |
| 실제 참가 | JoinSession/ClientTravel 후 기존 L_Dev_Default에 합류, Joined |
| 양쪽 로비 | 같은 2/3명, ID 01·02, 서로 다른 두 이름, 호스트/참가자, 등록 확인 표시 |
| 실제 마우스 입력 | 경기 참가 메뉴 → 행 선택 → 참가, 나가기 클릭 성공 |
| 참가자 나가기 | 타이틀 복귀·None, 호스트는 1/3명으로 갱신 |
| 재참가 | 두 화면 모두 2/3명, ID 01·03으로 갱신. 기존 서버 ID 정책 유지 |
| 호스트 종료 | 두 창 타이틀 복귀·None, 로비 0개·타이틀 Root 1개씩 |
| 종료한 방 | 새 조회 결과 0개 |
| 추가 생성 중 EOS 장애 | 이후 별도 새 PIE의 생성 요청에서 EOS HTTP 503 / ask_timed_out 반복. 기존 제한시간 → CleanupRequired → 늦은 실패 콜백 정리 후 None 복귀·오류 표시 확인 |
| 표시 보정 | 작은 참가자 창에서도 테마 전체 ID·방 이름 표시, 상태 안내 대비 보정 |
| 정원 초기 표시 | Designer의 초기 선택값을 비워 기존 MW NativeConstruct가 3을 선택하도록 통일. UE 5.8의 동일 값 재선택 시 빈 표시 문제 방지 |
| 호출 경로 | 양쪽 활성 UI에 Frontend Widget/RoadButton/RoomRow/Controller 인스턴스 없음 |
| 에셋 의존성 | 7개 정식 WBP에서 Frontend WBP·WBP_RoadButton·테스트 fixture 참조 없음. Common 이미지·폰트만 재사용 |
| Blueprint / C++ | 정식 WBP 7개 컴파일, Win64 Development Editor/Game 빌드 성공 |
| 게임 오류 | 해당 실제 흐름에서 Blueprint Runtime Error / Accessed None / Assertion / Fatal 검색 결과 없음 |

작은 창은 640×480, 호스트 창은 1280×720 게임 영역이었다. 로비 폰트 보정은 두 계정 로비의 런타임에서 확인한 동일 설정을 Designer에도 저장했다. 이후 새 PIE 인스턴스의 생성 폼/목록 클래스와 저장된 로비 설정을 추가 점검했다. 정원 초기 표시 수정은 추가 로그인·세션 성공으로 위장하지 않고 새 인스턴스의 화면 단독 확인으로 검증했다. 하단 상태·오류 안내는 서로 겹치지 않도록 크기와 간격을 보정했다.

추가 생성 시험에서 발생한 EOS 503을 UI 통합 성공으로 기록하지 않는다. 그 요청은 호스팅에 진입하지 않았고, 실패 정리 후 None 복귀를 확인했다. 앞선 실제 2인 목록·참가·동기화·재참가·호스트 종료 결과와 별개의 서비스 실패 관측이다.

로컬 근거는 `Saved/FrontendIntegration`에 있다. 주요 파일은 `two_account_join_sync.json`, `physical_click_rejoin.json`, `guest_left.json`, `host_close_cleanup.json`, `closed_room_absent.json`, `final_assets_and_dependencies.json`, `functional_source_preserved.json`, `EditorBuild.log`, `GameBuild.log`다. Saved는 공유 산출물이 아니며 지워질 수 있으므로 검증 결과는 이 문서에 기록했다.

## 직접 다시 시험하기

1. UVCS의 수정된 `Content/UI/Lobby` WBP 7개와 대응 소스를 함께 받는다.
2. L_Title을 열고 New Editor Window PIE, Players=2, Net Mode=Play Standalone, Run Under One Process=켜기, Dedicated Server=끄기로 실행한다.
3. 두 창을 서로 다른 EOS 계정으로 로그인한다. 같은 계정은 실제 두 참가자 시험에 사용하지 않는다.
4. 첫 창에서 방을 생성하고 Frontend 로비에 호스트 한 명이 표시되는지 확인한다.
5. 둘째 창에서 경기 참가를 누른다. 목록 행을 선택하고 참가한다. 두 창에서 2/정원, 이름·ID·역할이 같은지 확인한다.
6. 둘째 창에서 나가기 후 재참가한다. 마지막에는 호스트가 방 종료한다. 중복 UI와 Accessed None이 없는지 Output Log를 확인한다.

3인·서로 다른 PC/WAN 경로·패키징/Cook은 이번 검증 범위가 아니다. Ready/경기 시작/차량 스폰은 후속 구현이다. 2인 EOS 참가 완료를 이 항목들의 완료로 해석하지 않는다.

에셋은 UVCS 체크아웃 상태이며 Git에는 포함되지 않는다. 원격 UVCS 체크인·Git 커밋·push·병합은 이번 작업에서 수행하지 않았다. 팀 전달 시 바이너리 7개와 소스를 함께 인계해야 한다.
