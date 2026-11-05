# ProjectMW P3 후속 — 차량 UI 배치 및 참가자 Preview 검증

작성일: 2026-10-30 KST. 실제 작업 프로젝트: `C:/workspace/UE/ProjectMW/MW.uproject`, UE 5.8.4.

## 1. 결과와 작업 범위

기존 World Space 차량 선택 화면을 확장했다. 카드 프레임 폭은 Designer 기준 120→172.5로 43.75% 증가했고, 5개 프레임의 선택 영역 점유율은 실제 1920×1080 캡처 측정으로 약 55.0→78.5%가 됐다. 2/2, 2/4, 4/4, 혼합 확정, 전원 확정, 동일 차량 다중 포인터를 실제 PIE로 검증했다.

**참가자 2명·4명은 명시적으로 표시된 개발 Preview 데이터다. 실제 다중 계정 접속·서버 차량 확정 검증이 아니다.** Asset Registry의 실제 `UMWVehicleDefinitionData`는 여전히 0개다. Preview 차량은 기존 원본 이미지 5개이며 게임 VehicleId, Pawn, PlayerState, 카탈로그에 가짜 차량을 등록하지 않았다.

기존 `L_Title_WorldUI_Dev`의 타이틀 버튼으로 차량 Preview에 들어가고 같은 레벨의 타이틀 메뉴로 돌아오는 실제 클릭 흐름도 확인했다. `L_VehicleSelection_UI_Dev`는 기존 독립 배치 검증용으로 유지했다. 새 레벨을 추가하지 않았다.

정식 `Content/Levels/L_Title.umap`, 두 개발 레벨, 카메라, World WidgetComponent/Actor BP, 기존 타이틀 Root, 서버 시스템은 이번 작업에서 변경하지 않았다. 새 작업용 프로젝트 사본이나 worktree를 만들지 않았다. 현재 실제 ProjectMW의 기존 개발 UI 파일을 수정했다.

브랜치 `feature/title-world-ui`, HEAD `3db9c5d15e5496a4b699cb48e7796d826622bfb4` 유지. Git Commit/Push, UVCS Checkin/Update/Switch, main 병합은 수행하지 않았다. 기존 미커밋 변경도 보존했다.

## 2. 적용 지침 및 기존 구조 조사

`AGENTS.md`와 `.agents/skills/mw-task-routing/SKILL.md`에 따라 Client UI/Designer 작업으로 분류했다. 공용 Git·UVCS 지침, Client, Art/TA, 공용 Client·Server 문서를 적용했다. 서버 코드는 읽기 전용으로 조사했다. 작업 전 판단은 `IMPLEMENTATION_DECISION.md`에 남겼다.

| 기존 클래스 | 실제 확인한 책임/API | 이번 활용·경계 |
|---|---|---|
| `UMWVehicleSelectionWidget` | AssetManager 조회, 카드 생성·선택, `OnSelectionRequested`, `OnBackRequested`, `FinishSelectionRequest` | 그대로 확장. 참가자 표시와 Preview 시나리오만 추가 |
| `UMWVehicleCardWidget` | 원본 이미지·프레임·이름, Hover/Pressed/Unavailable/Selected, `OnChosen`, `OnHoverChanged` | 기존 카드 재사용. 프레임 아래 참가자 마커 그리드 추가 |
| `UMWVehiclePlayerEntryWidget` | 플레이어 표지판과 차량 썸네일 표시 | 공통 `SetPresentation`으로 실제/Preview 표시 수렴 |
| `AMWVehicleSelectionPreviewActor` | PIE 전용 World Widget 생성·소유, 카메라 전환, Back 시 위젯 해제·카메라 복원 | 코드와 BP 모두 변경 없음. 기존 수명 관리 사용 |
| `UMWVehicleDefinitionData` | `VehicleId`, `DisplayName`, 구체 `PawnClass`, `DrivingInputData`, `ValidateDefinition` | 게임 정의의 정본. 변경·가짜 인스턴스 생성 없음 |
| `UMWAssetManager` | `GetAssetIdsUnderRoot`, `LoadAssetSynchronous`, StartupAssetData 카탈로그 | 기존 등록·검증 경로 유지. ID 배열/별도 카탈로그 신설 없음 |
| `AMWPlayerController` | `CreateLobbyScreen`, `RefreshLobbyScreen`, `OnLobbyScreenRefresh`, `OnRep_PlayerState`, GameState/Subsystem Delegate 연결·해제 | 차량 RPC는 없음. 향후 기존 UI 생성·갱신 진입점 사용 |
| `AMWPlayerState` | Host/Ready, `LobbyMemberId`, 세션 등록 확인, 이름·UniqueId RepNotify→`NotifyLobby` | 차량 ID/차량 확정 필드 없음. Ready를 차량 확정으로 해석하지 않음 |
| `AMWGameState` | `RoomSettings`, `RoomSessionId`, `PlayerArray`, `OnLobbyChanged`, Add/RemovePlayerState 및 RoomSettings RepNotify | 실제 참가자·정원 표시 데이터로 재사용. 게임 Phase는 아직 없음 |
| `AMWGameMode` | 승인 방 설정, 입장 검사, PostLogin의 MemberId 부여, 세션 등록, `SetLobbyAdmission` | 초기 Pawn 자동 생성 억제. Ready 판정·차량 선택·Phase 전환 구현 없음 |
| `UMWRoomLobbyWidget` | 실제 PlayerArray를 MemberId로 정렬하고 RoomSettings 정원 표시, Delegate 갱신 | 실제 데이터 흐름 참고. 기존 Ready/Start 비활성 유지; 기존 고정 4슬롯 구현을 새 UI에 복사하지 않음 |

`LobbyMemberId`는 서버가 입장마다 증가시키는 식별자여서 `MemberId-1`을 UI 좌석으로 사용할 수 없다. 실제 표시에서는 약한 PlayerState 참조→UI 좌석 맵으로 현재 참가자의 자리를 유지한다. 퇴장한 참조를 제거하고 빈 자리에 새 참가자를 넣는다. 이것은 레이아웃 캐시이며 복제 상태나 서버 좌석 계약이 아니다. UI 재생성/클라이언트 사이에서 동일 좌석 번호를 보장하는 용도로 사용하지 않는다.

기존 차량 조사도 재사용했다. 로컬 `origin/feature/vehicle-movement` 조사 결과는 이동 관련 C++ 5개이며 구체 차량 Pawn BP/정의 등록까지 완료된 상태가 아니다. UCM 복구 리소스의 차량 외형은 Preview 후보일 뿐 승인 게임 차량이 아니다. 상세 근거는 기존 `P3_VEHICLE_SELECTION_REVIEW.md`와 `work/p3-vehicle`의 차량/브랜치 조사 JSON에 있다. 이번 변경은 이동 브랜치를 병합하지 않았다.

## 3. 배치 원인과 확대 결과

원인은 UniformGrid 셀의 기본 Left/Top 정렬이었다. 220폭 셀 안에서 카드 Desired Size가 120에 머물렀다. 임의 C++ 오프셋 대신 BP에 노출한 Fill 정렬과 Designer의 카드 종횡비·그리드 할당 영역을 조정했다.

| 항목 | 이전 | 이후 | 설정 소유 위치 |
|---|---|---|---|
| World DrawSize | 1100×780 | 동일 | 기존 Preview Actor BP WidgetComponent |
| 카메라 | 거리 1850, FOV 45°, 16:9 | 동일 | 기존 레벨 CameraActor |
| 그리드 | (0,85), 1100×290 | (0,132), 1100×240 | Selection WBP Designer |
| 카드 외부 DesignSize | 120×120 | 120×160 | Card WBP Designer |
| 원본 프레임/클릭 Artwork | 120×120 | 동일 비율 | Card WBP Designer |
| 카드 슬롯 | Left/Top | Fill/Fill | Selection BP 기본값 |
| 마커 영역 | 단일 선택 표시 | 카드 아래 120×36, 2열 | Card WBP / MarkerColumnCount |
| 카드 프레임 실제 할당 폭 | 120 | 172.5 | 230높이 셀÷160비율, 자동 ScaleBox |
| 플레이어 영역 | 우측 치우친 (450,390), 650×220 | (0,375), 1100×240 중앙 Overlay | Selection WBP Designer |
| 플레이어 패널 | 기존 1인 Preview | 220×230, 좌우 Padding 각12 | PlayerEntry WBP / BP 기본값 |

마커용 높이를 별도로 예약해 이미지·이름·테두리와 겹치지 않도록 했다. 참가자 영역은 정원만큼 생성한 동일 크기 패널을 중앙 HBox로 배치한다. 따라서 정원2에서는 2개만 중앙에, 정원4에서는 대기 슬롯까지 항상 4개가 유지된다. 해상도나 2인/4인에 따른 C++ 좌표 분기를 넣지 않았다.

실측: 동일 1920×1080 PIE에서 원본 프레임의 중성색 픽셀 경계를 5개 관찰 영역 안에서 측정했다. AA 테두리 포함으로 약 ±2px 오차가 있다. 변경 전 폭 151/151/152/152/152px, 변경 후 216/217/216/217/216px. 합계 758→1082px, 기존 카메라 투영 그리드 1378.06px 대비 **55.0→78.5%**. Designer 계산은 **54.55→78.41%**, 목표 75~85% 안이다. 평균 프레임 사이 공간은 약124→59px로 감소했다. 측정 코드와 원시 좌표는 `pixel-measurement.json`에 있다.

기존 카메라 Transform은 독립 검증 레벨 Location(-1850,0,400), Rotation(0,0,0), FOV45. 타이틀 내 Preview 카메라는 Location(-1850,3000,400), 같은 Rotation/FOV이며 모두 변경하지 않았다. 원래 타이틀 CameraActor로의 복원도 그대로 사용했다.

원본의 차량 5개 아이콘·프레임·선택 테두리·노란 메뉴 버튼·붉은 플레이어 표지판을 재사용했다. 새 그림으로 교체하지 않았다. 참가자 식별용 빨강/파랑/초록/주황 마커 색은 BP 설정이며 원본과 동일한 플레이어 색 정책이라고 주장하지 않는다. 초안에서 원본 붉은 표지판에 파랑을 곱해 어두워진 문제는 제거했고 최종 캡처는 원본 붉은 표지판을 유지한다.

## 4. 참가자 데이터와 상태 표현

`WBP_VehicleSelection_Preview` 기본값의 `PreviewScenarios`가 테스트 데이터의 소유자다. 데이터는 Label/Capacity/LocalSlotIndex/Participants이며 참가자는 SlotIndex/DisplayName/PreviewCardIndex/bConfirmed를 가진다. PreviewCardIndex는 이미지 배열 인덱스이고 게임 VehicleId가 아니다. 활성 시나리오는 transient 복사본으로 시험하므로 BP 원본 데이터나 서버 객체를 변경하지 않는다.

| 사례 인덱스 | 정원/참가 | 선택 상태 | 기본 로컬 |
|---|---|---|---|
| 0 A | 2/2 | 서로 다른 차량, 1명 선택 중·1명 확정 | P1 |
| 1 B | 4/2 | A와 같은 선택, 빈 2자리 ‘참가 대기’ | P2 |
| 2 C | 4/4 | 서로 다른 4대, 전원 선택 중 | P3 |
| 3 C | 4/4 | 서로 다른 4대, 2명 확정·2명 선택 중 | P3 |
| 4 C | 4/4 | 3명 확정·1명 선택 중 | P4 |
| 5 C | 4/4 | 전원 확정 | P3 |
| 6 D | 4/4 | 같은 차량에 4명, 선택/확정 혼합 | P2 |

화면 위에 ‘디자인 Preview · 실제 참가자 및 서버 확정 결과가 아닙니다.’와 실제 등록 차량0대를 표시한다. ‘사례 변경’은 BP 배열 길이에 따라 순환하고 ‘로컬 바꾸기’는 해당 시나리오의 실제 참가자 목록을 순환한다. 고정 인원 배열을 C++에 만들지 않았다.

선택 중/확정은 표지판의 상태 문구 및 카드 아래 마커의 ‘선택/확정’으로 구별한다. 로컬은 패널의 노란 프레임·‘내 선택’과 해당 카드의 로컬 식별색/‘나’로 강조한다. 다른 참가자 패널은 로컬 강조를 받지 않는다. 같은 카드를 4명이 가리키면 마커 셀 (0,0),(0,1),(1,0),(1,1)을 사용한다. **중복 차량 허용 정책을 구현한 것이 아니라 다중 포인터 표시 검증이다.**

실제 모드는 GameState의 RoomSettings.MaxPlayers와 PlayerArray를 읽고 이름/로컬 여부/대기 슬롯을 같은 `FMWVehiclePlayerPresentation` 및 `SetPresentation` 경로에 전달한다. 차량 선택 필드가 없으므로 실제 모드의 차량 상태는 ‘차량 선택 서버 계약 미연결’로 표시한다. 실제 Ready나 Preview 값을 차량 확정의 정본으로 복사하지 않는다.

## 5. 실제 PIE 증거

증거 폴더: `C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-multiplayer-ui`.

아래 PNG는 실제 PIE의 `shot showui` 출력이다. Editor Viewport 이미지나 합성 UI가 아니다. 창 테두리가 포함되어 PNG 크기는 1932×1156/1292×796/972×616이고 실제 클라이언트 viewport는 각각 1920×1080/1280×720/960×540이다. `scenario-captures.json`은 실제 viewport, 참가자 상태, 마커 수, 로컬 강조 위젯, 좌석 위젯 경로를 캡처마다 기록한다.

| 필수 증거 | 실제 파일 |
|---|---|
| 수정 전 1920 | `before-1920.png` |
| 수정 후 5개 카드·4명·2확정/2선택 중 | `case-c-mixed-1920.png` |
| 2명/정원2 | `case-a-2of2-1920.png`, `case-a-2of2-1280.png`, `case-a-2of2-960.png` |
| 2명/정원4 | `case-b-2of4-1920.png`, `case-b-2of4-1280.png`, `case-b-2of4-960.png` |
| 4명/정원4 선택 중 | `case-c-selecting-1920.png`, 同1280/960 |
| 2명 확정·2명 선택 중 | `case-c-mixed-1920.png`, 同1280/960 |
| 3명 확정·1명 선택 중 | `case-c-3confirmed-1920.png`, 同1280/960 |
| 전원 확정 | `case-c-confirmed-1920.png`, 同1280/960 |
| 동일 차량 마커4개 | `case-d-overlap-1920.png`, 同1280/960 |
| 타이틀 내 실제 진입 | `title-before-preview.png`, `title-vehicle-4of4-mixed-1920.png` |
| 타이틀 복귀 | `title-return-1920.png`, `title-native-flow.json` |
| 실제 참가자별 변경 | `native-participant-changes-1280.png`, `native-click-tests.json` |

![변경 전 실제 PIE](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-multiplayer-ui/before-1920.png)

![변경 후 실제 PIE — 4명, 2명 확정](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-multiplayer-ui/case-c-mixed-1920.png)

![2명과 빈 슬롯2개, 960×540 실제 PIE](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-multiplayer-ui/case-b-2of4-960.png)

![4명 전원 확정, 960×540 실제 PIE](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-multiplayer-ui/case-c-confirmed-960.png)

짧은 영상 `vehicle-participant-changes.mp4`: 실제 1280×720 PIE의 엔진 캡처100프레임을 실제 캡처 타임스탬프로 H264 인코딩했다. 약22.9초, 창 포함1292×796, 목표5fps의 저프레임 증거 영상이다. 마우스로 P3 차량을 Fred로, 로컬을 P4로 바꾸고 P4 차량을 The Raven으로 변경했다. 100개 PNG 해시가 모두 다르고 참가자 상태4종이 기록됐다. 프레임 합성/보간을 하지 않았다. P4 확정까지의 최종 클릭은 별도 PNG/JSON에도 남겼다. 성능 측정용 영상은 아니다.

## 6. 기존 멀티플레이 연결 설계 — 구현/승인 대기

이 절의 API 이름·자료형은 **서버 담당자에게 전달할 제안**이며 현재 구현된 API가 아니다. 기존 공용 문서 08/09의 GameMode 권한, GameState Phase, PlayerState 개인 상태, Phase/RoundEpoch 구조를 우선한다. 새 Subsystem이나 별도 서버 상태 머신을 만들지 않는다.

### 6.1 현재 존재하는 시작 경로

`UMWMultiplayerSubsystem::RequestCreateRoom(const FMWRoomSettings&)` / `RequestJoinRoom(const FString& SessionId,int32 ListRevision)` → 승인 테마/월드 이동 → GameMode 입장 검사·설정 적용·PostLogin → 세션 등록 확인 → `NotifyListenWorldReady` / `NotifyClientLobbyReady` → PlayerController `RefreshLobbyScreen` → BP `OnLobbyScreenRefresh` / GameState `OnLobbyChanged` → 실제 로비 표시.

Ready 값은 있으나 서버 Ready 요청/전원 Ready 판정과 다음 단계 전환은 아직 연결돼 있지 않다. 이번에 Ready 버튼을 켜지 않았다.

### 6.2 필요한 추가 계약과 책임

| 소유자 | 필요한 계약 제안 | 현재 상태 |
|---|---|---|
| GameMode(서버) | 실제 등록 참가자 Ready·입장 정책을 검증하고 VehicleSelection Phase 시작 | 미구현, 담당자 승인 필요 |
| GameState | 공용 `{Phase,RoundEpoch,PhaseEndServerTime}` 복제. 차량 선택/트랙 조각 선택·배치 단계 표시 | 현재 RoomSettings/SessionId만 존재. 공용 설계 준수해 추가 필요 |
| PlayerState | 한 구조체 `{VehicleId,bConfirmed,RoundEpoch,SelectionRevision}` 복제와 RepNotify | 현재 Host/Ready/MemberId/등록 확인만 존재 |
| 소유 PlayerController | `ServerRequestVehicleSelection(VehicleId,RoundEpoch,ExpectedSelectionRevision,RequestId)` | 제안, 실제 RPC 없음 |
| 소유 PlayerController | `ServerRequestVehicleConfirmation(ExpectedVehicleId,bConfirmed,RoundEpoch,ExpectedSelectionRevision,RequestId)` 및 명시적 Clear 요청 | 제안. 변경/확정 해제/선택 삭제를 구분 |
| 서버 선택 검증 | 승인 카탈로그→정의 검증→VehicleId 대조→PS 상태 갱신 | 기존 AssetManager/Definition API 재사용 가능, 권한 호출부 필요 |
| UI | 기존 `OnSelectionRequested(VehicleId)`를 소유 Controller 요청에 연결, 결과/pending 표시 | Delegate 존재. 복제된 원격 참가자 선택 연결은 미구현 |

확정 요청에 예상 VehicleId와 Revision을 포함해 이전 카드의 확정 요청이 새 선택에 적용되지 않게 한다. Phase/RoundEpoch 불일치, 비소유/등록 미완료/퇴장 플레이어, 오래된 Revision, 중복 RequestId, 허용되지 않은 정의를 서버에서 거절한다. 승인된 정책이 생기면 중복 차량 검사를 같은 경로에 추가한다. 클라이언트는 PawnClass/에셋 경로/스폰 Transform을 보내지 않는다.

`UMWAssetManager`의 GameplayTag 카탈로그 ID와 정의의 FName VehicleId는 서로 다른 식별자다. 서버는 설정된 루트 아래 승인 정의를 로드하고 `ValidateDefinition`으로 Pawn/InputData까지 확인한 뒤 안정 VehicleId를 대조해야 한다. 클라이언트 문자열을 에셋 경로로 만들지 않는다. 정의 준비는 선택 단계 진입/데이터 준비 시 수행하고 클릭마다 새 카탈로그를 구성하지 않는다. 승인 실제 차량5대가 등록되면 UI는 기존 조회 결과로 동적 표시한다.

### 6.3 갱신/요청 흐름

1. 서버가 Ready 및 입장 조건 확인 후 GameState Phase를 차량 선택으로 복제한다.
2. 로컬 PlayerController의 기존 `RefreshLobbyScreen`/`OnLobbyScreenRefresh`를 통해 설정된 위젯·World Component를 표시한다. 클래스 참조는 BP 설정이며 경로 리터럴을 추가하지 않는다.
3. 클라이언트 선택/확정 의도를 소유 Controller RPC로 전달한다. UI 로컬 Hover는 미리보기일 뿐 선택 정본이 아니다.
4. 서버가 현재 Phase/Epoch/소유권/요청 Revision/등록/승인 정의를 검증한다. 검증 성공 후 PlayerState 구조체를 원자적으로 갱신한다.
5. PlayerState RepNotify→기존 `NotifyLobby`→GameState `OnLobbyChanged`→공통 UI 갱신. Listen Host의 서버 setter도 같은 Notify 경로를 호출해야 한다.
6. 복제 선택ID를 승인 정의/카드에 매핑해 기존 `FMWVehiclePlayerPresentation`/마커 표시를 채운다. 현재 실제 분기는 차량 선택 계약 미연결 표시까지 구현돼 있다.
7. 전원 확정 판정은 GameMode가 현재 유효·등록된 참가자와 승인 정의를 기준으로 수행한다. UI의 패널 수/체크 표시가 다음 Phase를 결정하지 않는다.
8. 서버가 확정 스냅샷을 고정하고 다음 트랙 조각 선택·배치 Phase로 진행한다. 두 건설 입력 모드는 공용 문서대로 로컬 입력 모드이며 불필요한 서버 Phase를 추가하지 않는다.

Controller와 위젯이 같은 Delegate를 통해 동일 화면을 중복 갱신하지 않도록 실제 호스팅 시 구독 책임을 하나로 정해야 한다. 현재 위젯은 AddUnique/Destruct 해제 및 GameState 교체 시 기존 구독 해제를 적용했다. 실제 로비 호스팅에서는 늦은 GameState/PlayerState 도착 시 기존 Controller 재갱신 경로에서 `RefreshPlayers`를 호출하도록 연결할 필요가 있다.

RPC 결과는 요청 pending/오류 해제용이고 복제 상태가 선택 정본이다. RPC 응답과 RepNotify의 도착 순서를 가정하지 않는다. 현재 `FinishSelectionRequest(Message)`는 pending 해제용이며, 실제 연결 시 RequestId 및 권위 Revision과 맞춰 호출하는 계약이 필요하다. 무응답/취소 시 영구 pending에 머무르지 않도록 기존 세션 오류·UI 해제와 함께 종료 조건을 정한다.

### 6.4 변경/취소/중복/접속 수명

| 상황 | 서버 계약 및 UI 처리 제안 |
|---|---|
| 선택 변경 | 선택 미확정 상태에서 새 ID 검증. 확정 상태는 먼저 확정 해제 후 변경하는 방식 제안; 최종 정책 승인 필요 |
| 확정 해제 | ID를 보존하고 확정만 false. Preview의 ‘다시 선택’과 같은 시각 동작 |
| 선택 삭제 | 별도 Clear 의도. NAME_None 허용 여부와 시점은 서버 계약으로 명시 |
| 같은 차량 여러 명 | 지금 UI는 마커를 분리해 표시 가능. 중복 허용/독점 여부와 경합 승자는 서버가 결정 |
| 퇴장/끊김 | 기존 RemovePlayerState→OnLobbyChanged로 빈자리 표시. 서버 Logout에서 등록/선택 정리 및 전원 확정 재평가 필요 |
| 늦은 참가 | 기존 `SetLobbyAdmission`을 단계 정책에 연결하거나 허용 시 현재 Phase/PS 스냅샷 제공. 허용 여부를 UI가 결정하지 않음 |
| 재접속 | 기존 UniqueId·세션 등록 검증을 사용. 이전 PS 복원/새 참가 처리와 Epoch 유효성은 서버가 결정 |
| 로컬 강조 | 실제 모드에서 OwningPlayer.PlayerState 객체와 비교. Preview의 LocalSlot을 실제 네트워크 ID로 사용하지 않음 |
| 단계 변경/여행/파괴 | pending 요청 종료, Delegate 해제, 위젯 해제. 현재 Preview Actor의 수명 처리는 재사용 가능하나 PIE 전용 가드 유지 |

현재 Preview Actor는 의도적으로 PIE 전용이다. 실제 경기 월드의 차량 선택 화면은 기존 PlayerController BP UI 생성/갱신과 레벨 Component·CameraActor 참조를 통해 호스팅하도록 서버/클라이언트 담당자가 합의해야 한다. Preview Actor의 PIE 가드를 제거해 곧바로 정식 경기 흐름으로 쓰지 않는다.

### 6.5 서버 Pawn 생성·Possess 시점

현재 GameMode는 초기 Pawn 자동 생성을 억제한다. 차량 확정 클릭 즉시 UI나 클라이언트에서 Pawn을 생성하지 않는다. 권장 순서는 전원 확정→승인 정의 고정→트랙 조각 선택·배치→트랙/충돌 및 공용 Revision 준비 확인→주행 준비 단계에서 서버가 정의의 PawnClass로 Spawn/Possess다. 스폰 위치는 승인 레벨/데이터에서 가져온다. 정확한 단계는 서버·차량 담당자 승인으로 결정한다.

클라이언트는 Pawn/Controller/Phase/InputData가 준비된 뒤 기존 입력 설계에 따라 입력을 등록한다. Spawn 실패·일부 참가자 미준비·끊김은 부분 생성 정리 및 시작 취소/예외 종료 계약을 가져야 한다. UI가 실패를 숨기고 다음 단계로 진행하지 않는다.

## 7. 실제 변경 파일과 책임

이전 P3 작업 시작 기준이 아니라 **이번 요청 직전 114개 파일 해시 기준**으로 비교했다. 기존10개 파일 변경, 신규4개 코드/에셋, 보고서1개 추가다. 104개 기준 파일이 동일하다. 아래 경로는 `C:/workspace/UE/ProjectMW/` 기준이다.

| 파일 | 변경 |
|---|---|
| `Source/MW/UI/MWVehicleSelectionWidget.h/.cpp` | Designer 정렬/패널 Padding 노출, Preview 시나리오/로컬 전환, 공통 참가자 갱신, 실제 PS 좌석 보존, 마커 전달 |
| `Source/MW/UI/MWVehicleCardWidget.h/.cpp` | 다중 마커와 로컬 색, 기존 선택/잠금 가드 유지 |
| `Source/MW/UI/MWVehiclePlayerEntryWidget.h/.cpp` | `SetPresentation` 공통 입력, 로컬 프레임 표시 |
| 신규 `Source/MW/UI/MWVehicleUITypes.h` | 수동 UI 표시/개발 Preview 구조체. 게임 ID/복제 상태 소유하지 않음 |
| 신규 `Source/MW/UI/MWVehiclePlayerMarkerWidget.h/.cpp` | 카드 아래 작은 P#/선택·확정/로컬 표시. 기존 큰 PlayerEntry를 무리하게 축소하지 않기 위한 표시 전용 위젯 |
| `Content/UI/Lobby/VehicleSelection/WBP_VehicleCard_Dev.uasset` | 카드 비율·마커 별도 영역·클래스 기본값 |
| `.../WBP_VehiclePlayerEntry_Dev.uasset` | 패널 크기·이름/차량/상태 정렬·로컬 강조 |
| `.../WBP_VehicleSelection_Dev.uasset` | 카드·참가자 Layout, 개발 버튼, 텍스트/색/정렬 기본값 |
| `.../WBP_VehicleSelection_Preview.uasset` | 개발 시나리오7개 데이터 |
| 신규 `.../WBP_VehiclePlayerMarker_Dev.uasset` | 원본 Badge 텍스처와 HBox 라벨 Designer |
| 신규 `Docs/Engineering/Client/P3_VEHICLE_MULTIPLAYER_UI_LAYOUT.md` | 이 보고서 |

중복된 단일 PreviewPlayerName/Preview 확정 bool/이전 전용 PlayerEntry setter 경로를 공통 Presentation 및 로컬 참가자 데이터 기반으로 대체했다. 게임 선택 상태를 복제하는 새 변수나 범용 Manager/Subsystem을 만들지 않았다.

기존 `AMWVehicleSelectionPreviewActor`, `UMWTitleRootWidget`, Editor helper, GameMode/GameState/PlayerState/PlayerController/Subsystem, VehicleDefinition/AssetManager, 카탈로그/입력 데이터, 원본 텍스처는 이번 기준 해시와 동일하다. 정식 L_Title SHA-256:

`844e37bd881e91ebd1487f4dd354b172f1ca24656caf1d21a5632d5bd345cf67`

기존 개발 Title 레벨 SHA-256 `b7d392c1546b58343972b1cbb7821c7fd9b2e62e6ed2fbd4226a7998ba12c187`, 독립 차량 개발 레벨 `c5ffe48caec2d26f596dbedbceafb5767d2165250343ea71d31e76bb8559b762`도 보존했다. 실제 변경 해시와 source diff는 `preservation-audit.json`, `current-task-source.diff`에 있다.

## 8. 설정 소유권·하드코딩 검사

배치/Anchor/Alignment/SizeBox/ScaleBox/Font/Padding/마커 열 수/위젯 클래스 참조는 WBP Designer와 BP 기본값에 있다. 시나리오 인원·슬롯·이름·이미지 인덱스·표시용 확정·색도 BP 데이터다. 해상도별 픽셀 분기, 카메라 상수, 에셋 강제 경로 로딩, 고정 차량5대·인원2/4개 C++ 배열을 추가하지 않았다.

이번 C++9개 파일에 대해 `/Game/`·`/Engine/` 경로, ConstructorHelpers/LoadObject, Server UFUNCTION, ReplicatedUsing/DOREPLIFETIME, 2/4/5 고정 비교, Tick, 레이아웃 SetPosition/SetSize/DrawSize 패턴을 검사했고 일치0이었다. `hardcode-audit.json` 참조. BindWidget 필드명은 기존 프로젝트의 명시적 BP 계약이며 런타임 문자열 위젯 검색을 추가하지 않았다.

Editor Python의 경로/디자인 수치는 소유한 WBP를 작성하는 일회성 authoring 값이다. 런타임 C++에 고정되지 않고 저장된 Designer/BP에서 수정할 수 있다. 원본 데이터를 대신하는 게임 규칙을 Python이나 Preview 데이터로 생성하지 않았다.

## 9. 빌드·PIE 검증 결과 및 오류

| 검증 | 실제 결과 |
|---|---|
| UE5.8.4 Editor Win64 Development | 최종 성공, 26.94초, `EditorBuild-verified.log` |
| Game MW Win64 Development | 최종 성공, 26.60초, `GameBuild-verified.log` |
| WBP 컴파일/저장 | 작성 시5개 모두 성공, 최종 BS_UP_TO_DATE, dirty Content/Map 0 |
| 1920/1280/960 × 시나리오7개 | 실제 PIE21캡처, 참가/정원/확정/로컬/마커 수/게임ID None 검증 성공 |
| 2/4→4/4 안정 배치 | 세 해상도에서 PlayerEntry4개 객체 경로 동일, 빈자리를 채우는 구조 확인 |
| 로컬 변경/확정/해제 | 네이티브 마우스. P2만 Smiet→Fred→확정, 원격 값 보존, 잠금 중 다른 카드 클릭 무시 |
| 사례/로컬 변경 버튼 | 네이티브 클릭으로 D→A, 2/2에서 P1→P2 강조 및 잠금 전환 성공 |
| 같은 차량4명 | 겹치지 않는 서로 다른 마커 셀4개. 중복 정책 미구현 |
| 타이틀 진입/복귀/재진입 | 실제 EOS 로그인 후 네이티브 버튼. 같은 L_Title_WorldUI_Dev, 카메라 복원, Widget 해제, 새 기본 시나리오로 재진입 |
| 실제 등록 차량 | 최종 Asset Registry 정의0개. Preview 게임 VehicleId 항상 None |
| 오류 로그 | 최종 실행 Fatal0, Blueprint Runtime Error/Accessed None0. 아래 ensure1회는 미해결 |
| 정식 L_Title/서버/데이터 보존 | 요청 직전 해시와 동일 |

초기 C++ 빌드에서 UE5.8 UButton에 없는 `SetIsFocusable` 호출 및 UWidget::Slot을 가리는 지역변수명으로 실패했다. 해당 호출 제거/변수명 수정 후 최종 두 타깃 빌드 성공. XGE license warning은 있었으나 성공 결과에 영향을 주지 않았다.

검증 스크립트 재진입 시 Python 지역 변수 `mp_qa`가 없어 NameError1건(로그3줄)이 발생했다. 모듈을 `sys.modules`에서 가져오도록 수정해 완료했다. 게임/BP 런타임 예외와 구분한다.

최종 에디터 로그 `VehicleMPVerified.log`에서 **레벨 전환 시 `FAppTime::Get / IsInGameThread`, AppTime.cpp:38 handled ensure1회**가 재현됐다(출력2줄은 같은 사건). Renderer의 Reflection/Scene Release 호출 스택이다. 에디터 종료나 UI 클릭 실패로 이어지지는 않았으며 그 뒤 타이틀 PIE/인증/복귀를 완료했다. 원인을 확정하거나 해결했다고 보고하지 않는다. EOS TickTracker 지연 경고도 발생했다. 집계는 `runtime-log-review.json`에 보관한다.

첫 빠른 연속 캡처에는 전환 잔상이 있어 최종21장을 PIE 창 활성화 후 다시 촬영하고 안정된 화면을 확인했다. 실제 월드 UI는 작은 보조 마커가 960에서 가장 작게 보인다. 주요 이름/차량/패널/버튼은 잘림 없이 읽히며 로컬 프레임·대기 슬롯·전원 확정 상태를 구분할 수 있다. 임의 PNG 합성으로 수정하지 않았다.

실행하지 않은 검증: 실제 2/4계정·네트워크 복제, Ready 판정, 차량 Server RPC, 서버 승인 선택/확정, 늦은 참가/퇴장/끊김의 네트워크 재현, 게임 Pawn Spawn/Possess, Cook/패키지·Shipping. 해당 서버 기능이 이번 범위에서 미구현이므로 완료로 주장하지 않는다.

## 10. 재현·이식 순서

1. 현재 MW.uproject를 UE5.8.4에서 열고 기존 `/Game/Levels/L_Title_WorldUI_Dev`를 PIE 실행한다. 기존 EOS 로그인 후 좌하단 ‘차량 UI 미리보기’ 버튼을 사용한다. 독립 배치만 확인하려면 기존 `L_VehicleSelection_UI_Dev` PIE는 인증 없이 Preview를 연다.
2. ‘사례 변경’으로 A/B/C/D, ‘로컬 바꾸기’로 로컬 표시를 변경한다. 선택 중인 로컬만 차량 클릭/선택 확정, 확정된 로컬은 ‘다시 선택’ 후 변경한다. ‘뒤로’는 기존 타이틀 화면과 카메라를 복원한다.
3. 이번 UI 델타는 기존 P3 개발 레벨과 WidgetComponent를 그대로 사용하므로 레벨 전체 덮어쓰기 없이 C++9파일 및 WBP5개만 반영 가능하다. 기존 원본 이미지/폰트/Actor/Root 의존성은 유지한다. `dependencies-final.json`에 Asset Registry 참조를 기록했다.
4. 승인 차량 정의·구체 Pawn이 준비되면 기존 AssetData/AssetManager 방식으로 등록하고 실제 모드의 조회를 검증한다. Preview 이미지 데이터를 게임 정의로 전환하거나 그대로 등록하지 않는다.
5. 서버 담당자가 6절 Ready/Phase/PS선택/RPC/중복정책/Pawn시점을 승인·구현한 뒤, 기존 Controller/BP 갱신 흐름에 연결하고 실제 선택ID→공통 Presentation/마커를 채운다. 실제 참가자 수신 시 기존 좌석 캐시는 유지한다.
6. 이후 실제 2인/4인 별도 클라이언트에서 선택 변경·취소·중복 경합·전원 확정·이탈·늦은 참가·재접속을 검증하고 패키지 테스트를 진행한다. 정식 L_Title 변경과 VCS 제출은 이번에 수행하지 않았다.

복구가 필요하면 `before-backup`과 기준 해시를 대조해 이번10개 파일만 선택 복원한다. 기존 미커밋 파일 전체 초기화나 서버 파일 덮어쓰기를 하지 않는다. 신규 마커 WBP는 카드 참조를 먼저 복원한 뒤 제거할 수 있다.

인계 ZIP은 **현재 P3를 기준으로 한 이번 변경 델타**이며 새 프로젝트 전체가 아니다. 원본 텍스처/개발 레벨/기존 Actor·Root는 현재 프로젝트와 기존 P3 인계에 이미 있고 이번에 동일 해시로 보존됐다. 빈 프로젝트에 ZIP만 풀어 단독 실행하는 용도가 아니다.
