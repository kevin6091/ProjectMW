# 프런트엔드 잔여 3건 — 팀원 코드 수정 요청

기준: `fix/frontend-five-issues`, 시작 HEAD `8aaa6684aa383aec1cb32547bc713237126f915d`, 2026-11-06.

사용자 지시: 본인 소유 로그인·인트로만 수정한다. 이 문서는 팀원 코드의 검토 요청이며, 아래 차량 선택·공유 커서·서버 로직은 변경하지 않았다. 작성자는 Git blame 기준이며 현재 담당자 확정과는 다를 수 있다.

## 1. 우클릭으로 실제 차량 선택 취소

### 현재 왜 안 되는가 — 코드로 확인됨

`Source/MW/UI/MWVehicleSelectionWidget.cpp`의 `CancelLocalSelection()`은 `!bPreviewOnly`이면 즉시 false를 반환한다. 우클릭 이벤트는 있으나 실제 방에서 취소를 거절한다. 이 조건은 `e349f924`, kevin6091 작성이다.

실제 선택은 `HandleCardChosen → HandleConfirm → OnSelectionRequested → MWLobbyHUD::HandleVehicleSelectionRequested → MWLobbyPlayerController::RequestConfirmVehicle → ServerConfirmVehicle → MWLobbyGameMode::RequestConfirmVehicle` 순서로 서버에 전달된다.

서버는 이미 유효한 `VehicleAssetID`가 있으면 같은 ID만 중복 성공으로 처리하고 다른 ID를 거절한다. `MWPlayerState::ConfirmVehicleAssetIDFromServer()`도 빈 ID를 허용하지 않으며 기존 선택을 교체하지 않는다. 현재 취소 API는 없다. 서버 계약은 b03cd6ef/kevin6091 및 Docs/Shared/Client,Server/16번 문서에 해당한다.

### 제안하는 최소 수정

- 기존 Widget에 취소 요청 이벤트를 연결한다. UI만 선택 해제하고 서버 값을 그대로 두지 않는다.
- 기존 Lobby Controller에 소유자 취소 요청/RPC를 추가하고 GameMode에 검증을 맡긴다.
- GameMode는 VehicleSelection 단계, 유효 소유 Controller/PlayerState, 현재 월드, 고정 시작 참가자 여부를 검사한다. Traveling 이후는 거절한다.
- 승인되면 PlayerState의 기존 차량 ID만 서버에서 비우고 기존 복제·알림 경로로 모든 화면을 갱신한다. 별도의 선택 상태 복제품을 만들지 않는다.
- 요청 대기/성공/거절 때 Widget의 카드 선택·고정 커서·입력 잠금을 서버 결과에 맞춰 갱신한다. 현재 RefreshPlayers는 유효 ID 수신에만 pending을 해제하므로 취소 성공 처리도 검토한다.
- 반복 우클릭은 중복 상태 변경을 만들지 않게 한다. 마지막 선택과 취소가 경합하면 서버 처리 순서와 Phase가 최종 기준이다.

예상 파일: `MWVehicleSelectionWidget.h/.cpp`, `MWLobbyHUD.h/.cpp`, `MWLobbyPlayerController.h/.cpp`, `MWLobbyGameMode.h/.cpp`, `MWPlayerState.h/.cpp`. 실제 필요 파일은 담당자 검토 후 좁힌다. EOS·차량 이동 코드는 대상이 아니다.

필수 검증: 호스트/원격 각각 선택→우클릭→전원 화면 해제, 다른 차량 재선택, 중복 취소, 대기 중 취소, 마지막 선택과의 경합, Traveling 이후 거절. 16번 문서의 기존 '확정 후 변경 불가' 계약도 승인 후 함께 갱신해야 한다.

## 2. 원격 커서 크기·색상

### 확인된 사실과 아직 미확정인 원인

`MWVehicleSelectionWidget::ConfigureSharedCursorVisual()`과 `MWRemoteCursorOverlay`는 de6600eb/Yasasayi 작성이다. 로컬 소프트웨어 커서는 `GetSoftwareCursorSize()`의 슬롯 크기×Viewport DPI, 원격 Overlay는 ConfigureSharedCursorVisual의 슬롯 크기와 Overlay의 ScaleToFit을 이용한다. 선택 위치에 고정된 표시는 3D WidgetComponent 안에서 별도로 그려진다. 현재 저장 LocalPlayerCursor 슬롯은 약 154.835×154.835 논리 단위다.

`SetWorldPreviewMarkerScales()`는 !bPreviewOnly일 때 반환한다. 따라서 MWUCMTitleFlow의 게시판 보정 요청이 실제 방에는 적용되지 않는다. 게시판은 BeginPlay에서 1.33배 확대된다. 이것이 보고된 모든 확대 현상의 직접 원인이라는 실행 증거는 아직 없다.

`BuildLobbyParticipants()`는 각 클라이언트가 유지하는 PlayerSlots의 빈자리에 참가자를 배정하고 자리 번호로 색을 정한다. 기존 슬롯은 유지하므로 참가자 복제 도착 순서가 다르면 화면마다 자리/색이 달라질 가능성이 있다. 실제 색 변화의 직접 원인은 멀티클라이언트로 재현해야 확정할 수 있다.

### 제안하는 최소 수정

- 로컬 이동 커서, 로컬 고정 표시, 원격 Overlay의 실제 화면 크기를 같은 해상도에서 측정한다. 논리 단위·픽셀·DPI·월드 투영 배율을 구분한다.
- 이동 커서는 UI 게시판 확대와 별개로 일정한 화면 크기를 유지한다. 고정 표시 크기도 의도된 디자인에 맞추고 확대를 중복 적용하지 않는다.
- 화살표 끝 Hotspot을 유지하며 클릭 위치와 원격 투영 지점이 일치하도록 한다.
- 모든 클라이언트가 이미 공유하는 안정적인 참가자 식별/정렬 기준으로 자리·색을 결정한다. 로컬 여부로 타인의 색을 다시 지정하지 않는다. 색 동기화를 위해 불필요한 새 RPC를 추가하지 않는다.

예상 검토 파일: `MWVehicleSelectionWidget.cpp`, `UI/Cursor/MWRemoteCursorOverlay.cpp`, 필요 시 `MWLobbyHUD.cpp`; 네트워크 커서 전송 프로토콜은 현 단계에서 변경 필요성이 입증되지 않았다.

필수 검증: 호스트+원격 2인 이상, 다른 입장 순서, 선택 전/후/취소, 참가자 이탈, DPI와 해상도 변경, 커서 끝 위치. 현재 실제 멀티플레이 재현/수정 테스트는 미수행이다.

## 3. 전원 선택 후 원본 카메라 진입 연출

### 현재 왜 사라지는가 — 코드로 확인됨

`MWLobbyGameMode::TryCompleteVehicleSelection()`은 전원 선택 후 Traveling 설정과 ServerTravel을 같은 호출 흐름에서 실행한다. `MWLobbyHUD::RefreshLobbyScreen()`은 VehicleSelection 단계가 아니면 즉시 선택 화면을 닫는다. 따라서 로딩 진입 카메라 연출을 재생하는 단계/연결이 없다.

로컬 원본 자료 `work/source-world-routes.json`에는 `Camera & Lighting/Dolly - Select Vehicle To Loading/...` 항목이 존재한다. 그러나 `work/world-routes-author.json`의 현재 이식 경로는 RoomPanel/Online/Lobby/Vehicle/MainVehicle이며 출발 경로가 없다. 원본 경로 메타데이터 존재와 UE에서 완성된 재생 경로 존재를 혼동하지 않는다. 원본 곡선/속도/시선/종료 조건 복구 상태를 추가 확인해야 한다.

### 제안하는 최소 수정

- 전원 선택 승인 → 추가 선택/취소 잠금 → 각 클라이언트의 원본 출발 카메라 연출 → 맵 이동으로 연결한다.
- 기존 UCM 경로 재생 기능을 재사용한다. 임의로 새 카메라 움직임을 만들거나 고정 Delay만 넣지 않는다.
- GameMode의 전원 선택 판정·목적지 검증·최종 ServerTravel 권한은 유지한다. UI가 자체적으로 게임 시작을 승인하지 않는다.
- 로컬 연출 완료 신호가 서버 이동 조건에 사용된다면 소유자·현재 단계·중복·이탈을 검증하고, 완료 누락으로 영구 대기하지 않도록 실패 처리를 설계한다. 호스트의 완료만으로 모든 클라이언트 완료를 가정하지 않는다.
- HUD는 전환 시작 즉시 카메라/표시 소유자를 정리하지 않게 수명을 조정한다. Travel 후 이전 월드 참조는 해제한다.

예상 검토 파일: `MWLobbyGameMode.h/.cpp`, `MWLobbyPlayerController.h/.cpp`, `MWLobbyHUD.h/.cpp`, 기존 `MWLobbyPresentation` 인터페이스와 `MWUCMTitleFlow`, 실제 카메라 데이터가 저장되는 현재 통합 에셋. 계약/파일 범위는 담당자 승인 후 결정한다.

필수 검증: 마지막 선택 후 원본 경로 재생, 호스트/원격 완료 차이, 중복 완료, 이탈·완료 누락, Travel 실패, 선택 화면 재오픈, 카메라/커서 정리. 현재 카메라 경로 복구·서버 연동·실행 검증 모두 미완료다.

## 이번 변경과 구분

이번에 허용된 수정은 Youngong 소유의 로그인 Widget 비율 대응 및 UCM 인트로 스킵이다. 위 3건을 해결했다고 주장하지 않는다. 차량/서버/공유 커서 코드는 그대로 보존한다. Git Commit/Push, UVCS checkin/update/unlock은 하지 않는다. 로그인 uasset은 현재 Git Content 제외 정책상 Git 코드 변경만으로 전달되지 않는다.
