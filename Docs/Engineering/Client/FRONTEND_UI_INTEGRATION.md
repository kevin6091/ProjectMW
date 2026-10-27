# Frontend UI 연결 및 검증

분야: Client + Art/TA(UI 원작 리소스 임포트). 작업 시작 main: `987d402542ec2d099b093cc5e84a08d1599a5a31`. 최종 확인 origin/main: `678158c7436f9c87b7deb7fa864c76dd92c366c9` (추가 변경은 README만). 엔진: 설치된 UE 5.8.4 / CL 58902052. 브랜치: `feature/frontend-ui`. 추가 worktree 없음.

## 구현 범위와 원본

현재 main의 실제 온라인 API는 `UMWMultiplayerSubsystem::RequestLogin`, `IsLoggedIn`, `IsLoginInProgress`, `OnLoginFinished`다. 세션 API와 경기 Framework 클래스는 아직 없다. 기존 Subsystem, 로그인 WBP, 공용 맵 및 기본 Config를 수정하지 않는다.

기존 `/Game/UI/Lobby/WBP_LoginTest`는 실제로 존재하며 LogIn 버튼, 로그인 상태/에러 텍스트, Construct/Destruct와 로그인 Delegate 연결을 포함한다. 새 Root의 LoginHost에 이 클래스를 그대로 생성한다. Root는 기존 로그인 완료 이벤트를 구독하고 실제 `IsLoggedIn()`을 재확인한다. 로그인 요청 ID나 진행 상태를 복제하지 않는다.

사용자가 제공한 영상 `2026-10-27 15-15-13.mp4`를 실제 확인했다. 유사 그림을 생성하지 않고 기존 AssetStudio 추출 PNG 및 OTF를 임포트했다. Codex의 `MakeWayReview26/MakeWay_AssetReview/Content/UI/Common`에도 같은 역할의 원작 텍스처가 이미 있었다. 본 프로젝트에는 없었으므로 확인된 원본 `C:/workspace/Unpack/AssetStudio`에서 필요한 리소스만 임포트했다. 별도 Title15의 시네마틱/3D 메뉴 코드는 가져오지 않았다.

| 원작 추출 파일 | 신규 UE 에셋 |
|---|---|
| Sprite/T_listMenuOptionSignStandard.png | /Game/UI/Common/T_MW_MenuSign |
| Sprite/T_listMenuOptionSignOverlay.png | /Game/UI/Common/T_MW_MenuOverlay |
| Sprite/T_listMenuOptionSignDiamond.png | /Game/UI/Common/T_MW_MenuDiamond |
| Sprite/Arrow Rounded.png | /Game/UI/Common/T_MW_Arrow |
| Font/NotoSansKR-Bold.otf | /Game/UI/Common/F_NotoSansKR_Bold 및 F_NotoSansKR_Bold_Font |

실제 버튼/프레임은 Standard/Overlay와 원작 폰트를 사용한다. Diamond/Arrow는 임포트된 공용 리소스이며 현재 화면에서 사용하지 않는다. 원본 파일 해시는 `FrontendOriginalAssets.json`을 참조한다. PNG 원본은 편집하지 않았다. UI compression, sRGB, NoMipmaps를 지정하고 Slate Box가 실제 GPU 텍스처 크기로 모서리를 계산하는 점에 맞춰 Sign/Overlay의 MaxTextureSize를 128로 지정했다. UE 에셋의 원본 source 데이터와 외부 PNG는 유지한다. 폰트는 Inline 로딩이다.

기존 추출 증거의 MainMenu 기본색 `(255,178,33)`, 선택색 `(0,152,248)`, 글자색 `(55,52,53)`을 sRGB에서 선형으로 변환해 사용한다. Standard sprite의 원본 9-slice border는 106.5/512다. 버튼의 Pressed 명도, Disabled 회색, 0.2초 페이드와 마우스 기울기/눌림은 ProjectMW 상호작용을 위해 추가한 표현이며 원작 애니메이션 추출 완료를 뜻하지 않는다. 화면별 폼과 로비 배치는 신규 기능에 맞춘 UMG 구성이다.

추출물의 공개 재배포 권한은 이 작업에서 확인하지 않았다. 현재 로컬 개발/검증 리소스이며 Git PR에 바이너리를 올리지 않는다. UVCS 인계와 공개 패키지 포함 판정은 별개다.

## 클래스와 Widget 구조

| 클래스 | 책임 |
|---|---|
| UMWFrontendWidget | 로그인 이후 로컬 화면 전환, 폼 검증, 요청 의도 전달, 구독 수명 |
| UMWRoadButtonWidget | 실제 UButton을 감싼 공통 버튼. 원작 스타일과 Hover/Pressed 이벤트 |
| UMWRoomRowWidget / UMWRoomListItem | UListView의 실제 검색 결과 행, SessionId 기준 선택과 가상화 수명 |
| UMWFrontendConnection | 아직 없는 팀원 세션 API의 명시적인 연결 지점. 기본 구현은 성공하지 않음 |
| AMWFrontendPlayerController / AMWFrontendGameMode | 신규 메뉴 맵의 로컬 Widget 생성과 커서/입력 모드. 경기 규칙 없음 |
| MWFrontendAuthoring Editor 플러그인 | 신규 WBP 제작, 명시적 UI 테스트 프리뷰, 엔진 자동화 테스트. Game 타깃 제외 |

`WBP_FrontendRoot`: Canvas/ScaleBox/1100×900 디자인 영역 → 원작 패널 → Heading → WidgetSwitcher(LoginHost / MatchChoice / CreateRoom / RoomBrowser / CommonLobby) → StatusLabel.

`WBP_RoadButton`: UButton(ActionButton) + TextBlock(Label). Normal/Hovered/Pressed/Disabled 브러시가 서로 다르다. 모든 화면 버튼이 같은 WBP를 사용한다.

`WBP_RoomRow`: Border(SelectionBorder) + TextBlock(RoomLabel). UListView가 선택을 소유한다. 같은 방 이름도 서로 다른 SessionId이면 별도 행이다. 새 검색에서 기존 선택을 해제한다.

공통 로비는 Root 내부의 단일 CommonLobby 패널이다. 생성과 참가 모두 같은 패널에 진입하며 별도 로비 레벨을 만들지 않는다. `/Game/Levels/L_Frontend`는 최초 로컬 메뉴용 맵이다.

## 팀원 세션 API 연결 계약

`UMWFrontendConnection` 파생 어댑터를 실제 서비스/소유 Controller에 연결한 뒤 Root의 `SetConnection()`에 전달한다. 기존 MultiplayerSubsystem을 교체하거나 세션 시스템을 새로 구현하는 클래스가 아니다. 실제 세션 요청 ID, 진행 상태, timeout, 취소와 부분 생성 자원 정리는 팀원의 기존 서비스가 소유한다.

| 메서드/이벤트 | 의미 |
|---|---|
| GetOperation | 기존 서비스의 현재 작업 상태를 투영. UI용 별도 요청 ID 없음 |
| GetThemes | 서버가 승인한 ID/표시명 목록. UI가 임의 맵 경로를 만들지 않음 |
| CreateRoom | 정규화된 이름, 호스트 포함 2~4명, 승인 ThemeId 요청 |
| SearchRooms / GetRooms | 실제 서비스 검색 결과. 빈 결과와 미연결을 구분 |
| JoinRoom(SessionId) | 현재 결과의 고유 ID로 참가. 행 번호/방 이름으로 참가하지 않음 |
| GetLobby | 실제 현재 월드의 GameState/소유 PlayerState 및 그 복제 상태의 표시용 투영 |
| SetReady / StartMatch | 소유 Controller의 서버 RPC를 호출. 서버가 권한/Phase/인원/Ready/준비를 최종 검증 |
| LeaveRoom | 실제 종료/정리가 확인된 후 연결 상태 변경 |
| CancelOperation | 취소와 자원 정리, 기존 요청 무효화가 안전하게 끝났을 때만 true. 취소 불가 중이면 false |
| OnChanged | 게임 스레드에서 최신 상태를 게시한 뒤 알림. Root가 상태를 다시 읽음 |
| OnError(Message) | 작업 상태를 안전하게 닫은 후 구체적 실패 사유. 상태/화면을 가짜 성공으로 바꾸지 않음 |

요청 함수 반환값은 **시작 수락 여부**이며 네트워크 완료가 아니다. 호출 전에 서비스가 진행 상태를 설정해야 동기 콜백/연타에도 안전하다. 완료 시 서비스가 Idle로 되돌리고 알린다. 별도 World 준비 전에는 `bConnectionReady`를 false로 유지한다.

로비 진입은 현재 월드의 실제 `World->GetGameState()`, 소유 Controller의 PlayerState, 유효하고 중복 없는 GameState.PlayerArray 참가자, 유일한 호스트, 유효 정원, SessionId, 서버 Lobby Phase, 연결 준비를 함께 확인한다. OSS Create/Join 성공만으로 진입하지 않는다. Ready를 클릭해도 UI가 임의로 토글하지 않고 최신 서버 투영만 표시한다. `bCanStart`는 UI 활성 조건이며 서버의 재검증을 대신하지 않는다.

경기 Phase가 Lobby를 벗어나면 Root가 OnPresentationChanged(false)를 알린다. 신규 메뉴 Controller는 커서를 숨기고 GameOnly 입력으로 전환한다. Lobby로 돌아오면 UI 입력을 복원한다. 실제 팀 경기 Controller를 연결할 때도 이 이벤트에서 해당 입력 모드를 설치해야 한다.

Root 종료 시 자기 로그인/Connection/Button/ListView 구독만 해제한다. 공유 서비스 작업을 Widget 파괴와 함께 임의 취소하지 않는다. 맵 이동 후 새 Root는 서비스의 현재 스냅샷을 읽는다. 취소 후 늦은 네트워크 콜백의 요청 세대 검사는 서비스 책임이며, UI 화면 수명과 중복 구현하지 않는다.

## 실행 및 인계

1. 신규 에셋을 UVCS에서 함께 받은 상태에서 Editor의 `/Game/Levels/L_Frontend`를 열고 Play한다.
2. 기존 로그인 버튼을 사용한다. EOS 계정 인증 창은 사용자가 조작한다. 로그인 성공 이벤트와 실제 로그인 상태 확인 후 경기 선택이 표시된다.
3. 실제 세션 어댑터가 없으면 생성/검색/참가는 미연결 사유와 비활성 상태를 표시한다. 임시 방을 실제 검색 결과로 표시하지 않는다.
4. Editor Python에서 `unreal.MWFrontendAuthoringLibrary.show_test_preview(controller)`는 **UI TEST ONLY** 배너가 있는 Editor 전용 fixture를 생성한다. 테스트 테마/방/로비는 실제 EOS 결과가 아니다. Runtime/Game 타깃의 메뉴에는 설치되지 않는다.
5. 자동화 테스트 접두사는 `MW.Frontend`다. Preview WBP는 `/Game/Developers/FrontendTests/WBP_FrontendPreview`에만 있으며 정식 메뉴가 참조하지 않는다.

에셋은 실제 제작돼 있으므로 수동 WBP 제작은 필요 없다. 기본 GameDefaultMap 변경, 경기 Controller/세션 어댑터 연결, 실제 EOS 생성·검색·참가 및 실제 3PC/WAN 검증은 팀 통합 작업이다. 패키징/Cook 완료로 기록하지 않는다.

WBP 제작 도구는 기존 경로가 있으면 덮어쓰지 않고 중단한다. 기존 로그인/공용 맵/타인 에셋을 일괄 재저장하지 않는다. Git에는 Source·Editor 플러그인·연결 문서만, Content는 UVCS로 인계한다. 이 변경의 Build.cs와 UI 계약은 두 팀원의 독립 검토가 필요하며 자동 병합하지 않는다.

## 2026-10-27 최종 로컬 검증

- UE 5.8.4 Win64 Development Editor 및 Game 타깃 빌드 성공. 최종 실행 코드/Editor 도구를 검증했다.
- `MW.Frontend.InputAndLobbyValidation`, `MW.Frontend.WidgetRequestsAndLifetime`: 2/2 성공, 경고 0, 오류 0. 실제 WBP를 생성해 입력 검증, 요청 중복/취소/늦은 알림, 같은 이름의 방 선택, 정원 초과 참가 금지, 로비 연결 확인, Ready/시작 권한, Delegate 해제를 검증했다.
- 실제 Editor 마우스로 폼 입력, 2~4인 선택, 생성/검색/행 선택/참가/Ready/나가기/뒤로 가기를 UI 전용 fixture에서 확인했다. 1280×720 및 960×540에서 배치와 입력을 확인했다. 기존 로그인 WBP 및 신규 WBP 컴파일 성공.
- 기존 로그인 WBP와 MultiplayerSubsystem.cpp의 SHA256은 작업 전후 동일하다. 실제 EOS 계정 인증, WAN/3PC 연결, Cook/패키징은 실행하지 않았다.
- UVCS 신규 바이너리 11개는 로컬 AD pending 상태다. 기존 에셋 변경/삭제는 없다. 원격 changeset 체크인은 아직 없으며 Git PR만으로 Content가 전달되지 않는다. UVCS 잠금 조회는 성공했지만 반환된 항목은 없었다. 중앙 예약 확정이나 두 팀원의 독립 리뷰가 완료됐다고 주장하지 않는다.