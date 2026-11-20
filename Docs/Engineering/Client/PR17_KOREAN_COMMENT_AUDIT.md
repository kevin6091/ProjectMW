# PR #17 코드·설정 한글 주석 검토

[PR #17](https://github.com/kevin6091/ProjectMW/pull/17)의 기준 `main@03dc60b`과 검토 시작 시점의 작업 브랜치 `adfff86`을 비교했다. PR에 들어간 `Source/`, `Plugins/`, `Config/` 파일 57개를 파일별로 확인했다. 이 커밋에서 34개 파일에 실행 순서, 화면 또는 네트워크 상태의 소유권, 단위, 수명, 호환성 설명을 추가하거나 기존 주석의 위치를 바로잡았다.

코드와 설정 비교는 기존 줄을 보존한 채 전체 줄 주석만 제외해 확인한다. 공백 오류와 미해결 충돌 표식은 Git diff로 검사한다.

| 파일 | 결과 | 확인한 내용 |
|---|---|---|
| [DefaultEngine.ini](../../../Config/DefaultEngine.ini) | 확인 | HZB를 사용하는 이유가 가림 쿼리 설정 바로 위에 적혀 있다. |
| [DefaultGame.ini](../../../Config/DefaultGame.ini) | 보완 | 공통 포인터, 경기 WBP 쿠킹, 설원 첫 화면 재질, 카메라 노출 복귀, 원격 배율과 디자이너 설정의 경계를 설명했다. |
| [MWEnvironmentGameplay.cpp](../../../Plugins/MWEnvironmentGameplay/Source/MWEnvironmentGameplay/Private/MWEnvironmentGameplay.cpp) | 보완 | 차량 소유 전 대기판 시점과 맵 이동 중 선로딩 참조 수명을 설명하고, 생성기 정리 주석을 실제 정리 함수로 옮겼다. |
| [MWEnvironmentPreload.cpp](../../../Plugins/MWEnvironmentGameplay/Source/MWEnvironmentGameplay/Private/MWEnvironmentPreload.cpp) | 보완 | 테마가 바뀐 뒤 도착한 로드 콜백이 이전 재질의 밉을 고정하지 않는 이유를 설명했다. |
| [MWEnvironmentPreload.h](../../../Plugins/MWEnvironmentGameplay/Source/MWEnvironmentGameplay/Private/MWEnvironmentPreload.h) | 확인 | 게임 인스턴스에 선로딩을 보관하는 이유와 밉 유지 시간의 목적이 이미 적혀 있다. |
| [MWEnvironmentGameplay.h](../../../Plugins/MWEnvironmentGameplay/Source/MWEnvironmentGameplay/Public/MWEnvironmentGameplay.h) | 보완 | 테마별 첫 화면에 필요한 추가 재질을 설정하는 목적을 설명했다. |
| [MWEnvironmentRestore.cpp](../../../Plugins/MWEnvironmentRestore/Source/MWEnvironmentRestore/Private/MWEnvironmentRestore.cpp) | 보완 | 화면 안쪽 타일로만 첫 화면 준비를 판단하는 이유를 설명했다. |
| [MWEnvironmentRestore.h](../../../Plugins/MWEnvironmentRestore/Source/MWEnvironmentRestore/Public/MWEnvironmentRestore.h) | 보완 | 차량이 생기기 전에 컨트롤러 시점을 허용하는 상황을 설명했다. |
| [MWUCMTitleFlow.cpp](../../../Plugins/MWUCMFrontend/Source/MWUCMFrontend/Private/MWUCMTitleFlow.cpp) | 보완 | 복귀 가림의 인계 순서와 소프트웨어 포인터의 DPI 변환을 설명했다. |
| [MWUCMTitleFlow.h](../../../Plugins/MWUCMFrontend/Source/MWUCMFrontend/Public/MWUCMTitleFlow.h) | 확인 | 맵 이동에 걸쳐 커서 위젯과 뷰포트 연결을 보존하는 이유가 기존 주석에 있다. 멤버 색 갱신의 이유는 구현에 있다. |
| [MWTrackConstructionPlacement.cpp](../../../Source/MW/Gameplay/Construction/MWTrackConstructionPlacement.cpp) | 보완 | 배치 포인터를 보낼 수 있는 참가자와 설치 성공 후 스트림을 닫는 이유를 설명했다. |
| [MWTrackPlacementViewComponent.cpp](../../../Source/MW/Gameplay/Construction/MWTrackPlacementViewComponent.cpp) | 보완 | 로컬 고스트와 원격 위치 전송의 갱신 주기 차이, 승인 대기 중 안내 유지 이유를 설명했다. |
| [MWTrackPlacementViewComponent.h](../../../Source/MW/Gameplay/Construction/MWTrackPlacementViewComponent.h) | 보완 | 첫 화면 검사에 현재 세션의 고스트가 필요한 이유를 공개 함수에 붙였다. |
| [MWTrackPreview.cpp](../../../Source/MW/Gameplay/Construction/MWTrackPreview.cpp) | 보완 | 상속받은 BP 노드의 재질을 읽을 때 실제 트랙 BP 재정의를 반영하는 이유를 설명했다. |
| [MWTrackPreview.h](../../../Source/MW/Gameplay/Construction/MWTrackPreview.h) | 확인 | 표시용 액터 구분과 스폰 없이 재질을 수집하는 이유가 기존 주석에 있다. |
| [MWTrackPreviewPreload.cpp](../../../Source/MW/Gameplay/Construction/MWTrackPreviewPreload.cpp) | 확인 | 전시 후보만 미리 읽는 범위와 PIE에서 셰이더를 먼저 준비하는 이유가 기존 주석에 있다. |
| [MWTrackPreviewPreload.h](../../../Source/MW/Gameplay/Construction/MWTrackPreviewPreload.h) | 확인 | 로비에서 시작한 로드 참조가 필요한 수명이 클래스에 설명돼 있다. |
| [MWTrackSelectionBoard.cpp](../../../Source/MW/Gameplay/Construction/MWTrackSelectionBoard.cpp) | 보완 | 첫 화면을 공개하기 전 서버의 후보 세션 및 리비전을 대조하는 이유를 설명했다. |
| [MWTrackSelectionBoard.h](../../../Source/MW/Gameplay/Construction/MWTrackSelectionBoard.h) | 보완 | 재시도가 서버 선택 상태를 유지하면서 로컬 전시만 다시 준비한다는 계약을 설명했다. |
| [MWTrackSelectionExposure.cpp](../../../Source/MW/Gameplay/Construction/MWTrackSelectionExposure.cpp) | 확인 | 동일 카메라를 쓰는 선택·배치 구간과 고정 노출을 수동으로 평가하는 이유가 기존 주석에 있다. |
| [MWTrackSelectionExposure.h](../../../Source/MW/Gameplay/Construction/MWTrackSelectionExposure.h) | 확인 | 노출 범위를 카메라 후처리에 적용하고 시점 변경 때 가중치를 낮추는 이유가 클래스와 구현에 있다. |
| [MWTrackRuntimeSubsystem.cpp](../../../Source/MW/Gameplay/Track/MWTrackRuntimeSubsystem.cpp) | 확인 | 전시용 고스트에 경기 트리거를 붙이지 않는 이유가 기존 주석에 있다. 체크포인트 성공 때 속도를 초기화하는 계약은 Pawn API에 있다. |
| [MWVehiclePawn.cpp](../../../Source/MW/Gameplay/Vehicle/MWVehiclePawn.cpp) | 보완 | 텔레포트 성공 콜백과 체크포인트 요청 범위를 연결하는 임시 플래그의 수명을 설명했다. |
| [MWVehiclePawn.h](../../../Source/MW/Gameplay/Vehicle/MWVehiclePawn.h) | 확인 | 서버가 고른 후보에서 복귀 성공 시에만 정지한다는 계약이 공개 함수에 적혀 있다. |
| [MW.Build.cs](../../../Source/MW/MW.Build.cs) | 보완 | 프레임 캡처용 모듈을 에디터 빌드에서만 연결하는 이유를 설명했다. |
| [MWSharedCursorTypes.h](../../../Source/MW/Player/Cursor/MWSharedCursorTypes.h) | 보완 | 차량 선택 포인터와 월드 좌표를 쓰는 배치 스트림을 구분하는 이유를 설명했다. |
| [MWGameplayPlayerController.cpp](../../../Source/MW/Player/MWGameplayPlayerController.cpp) | 보완 | 가림이 걷힌 뒤 경기 단계 입력 모드와 위젯 포커스를 복구하는 순서를 설명했다. |
| [MWGameplayPlayerController.h](../../../Source/MW/Player/MWGameplayPlayerController.h) | 확인 | 공개 함수는 가림 화면에서 호출된다. 단계 입력과 포커스 복구 이유는 구현에 설명돼 있다. |
| [MWAssetManager.cpp](../../../Source/MW/System/MWAssetManager.cpp) | 보완 | 설정으로 읽는 로딩 WBP를 쿠킹 대상에 명시하는 이유를 설명했다. |
| [MWAssetManager.h](../../../Source/MW/System/MWAssetManager.h) | 확인 | 에디터에서만 존재하는 쿠킹 훅 선언이다. 로딩 패키지를 넣는 이유는 구현에 설명돼 있다. |
| [MWVehicleMovementTests.cpp](../../../Source/MW/Tests/MWVehicleMovementTests.cpp) | 확인 | 테스트 이름과 각 검증 문구에 실패 복귀·일반 텔레포트·체크포인트 성공의 속도 조건이 나뉘어 있다. 실제 트랙 방향은 기존 한글 주석으로 설명돼 있다. |
| [MWCursorProjection.cpp](../../../Source/MW/UI/Cursor/MWCursorProjection.cpp) | 보완 | 백그라운드 창의 좌표를 공유하지 않는 이유와 평면에 거의 평행한 광선을 거절하는 이유를 설명했다. |
| [MWCursorProjection.h](../../../Source/MW/UI/Cursor/MWCursorProjection.h) | 보완 | 광선과 주어진 평면을 이용하는 좌표 변환 계약을 공개 함수에 붙였다. |
| [MWPointerCursor.h](../../../Source/MW/UI/Cursor/MWPointerCursor.h) | 보완 | WBP에 끝점 값이 있는 경우와 기존 에셋의 슬롯 계산 경로를 설명했다. |
| [MWRemoteCursorOverlay.h](../../../Source/MW/UI/Cursor/MWRemoteCursorOverlay.h) | 보완 | 트랙 원격 포인터와 로컬 포인터의 공통 크기가 DPI 적용 전 논리 단위라는 점을 설명했다. |
| [MWArrivalScreen.cpp](../../../Source/MW/UI/Gameplay/MWArrivalScreen.cpp) | 보완 | 월드를 넘는 가림의 틱 수명, 공개 직후 밉 유지, 최소화한 창의 제한 시간, 재시도 범위를 설명했다. |
| [MWArrivalScreen.h](../../../Source/MW/UI/Gameplay/MWArrivalScreen.h) | 확인 | 게임 인스턴스가 가림을 유지하는 이유가 클래스에 있다. 틱과 재시도 세부 사항은 구현에 보완했다. |
| [MWGameplayHUDSubsystem.cpp](../../../Source/MW/UI/Gameplay/MWGameplayHUDSubsystem.cpp) | 보완 | WBP와 Slate의 역할 분담, 슬롯 크기의 하한, 가림이 걷히기 전 단계 안내 시간을 소진하지 않는 이유를 설명했다. |
| [MWGameplayHUDSubsystem.h](../../../Source/MW/UI/Gameplay/MWGameplayHUDSubsystem.h) | 확인 | PIE 맵별 HUD 플래그와 WBP 외형 소유권이 기존 주석에 있다. 위젯 연결 및 정리 순서는 구현에 있다. |
| [MWGameplaySign.cpp](../../../Source/MW/UI/Gameplay/MWGameplaySign.cpp) | 보완 | 재시도 여부에 따라 처음 포커스를 줄 버튼을 고르는 이유를 설명했다. |
| [MWGameplaySign.h](../../../Source/MW/UI/Gameplay/MWGameplaySign.h) | 확인 | 복구 화면의 판면과 이동 경로별 동작이 분리된 이유가 기존 주석에 있다. |
| [MWGameplaySignWidget.cpp](../../../Source/MW/UI/Gameplay/MWGameplaySignWidget.cpp) | 보완 | 같은 Slate 호스트에서 UMG 디자이너 속성을 다시 적용하는 이유를 설명했다. |
| [MWGameplaySignWidget.h](../../../Source/MW/UI/Gameplay/MWGameplaySignWidget.h) | 확인 | 문구·색·로딩 애니메이션을 WBP에서 편집한다는 역할이 클래스에 있다. |
| [MWItemGlassWidget.cpp](../../../Source/MW/UI/Gameplay/MWItemGlassWidget.cpp) | 보완 | 아이템 슬롯 유리의 그림자를 아래쪽으로만 늘리는 기준점을 설명했다. |
| [MWItemGlassWidget.h](../../../Source/MW/UI/Gameplay/MWItemGlassWidget.h) | 확인 | 유리 색·그림자 비율·선택적 재질을 디자이너에서 편집한다는 역할이 클래스에 있다. |
| [MWRaceHUDWidget.cpp](../../../Source/MW/UI/Gameplay/MWRaceHUDWidget.cpp) | 확인 | 개발 맵에서 실제 경기 단계 배지를 숨기는 이유는 앞선 주석 커밋에 있다. 계기판 표시 플래그는 HUD 서브시스템이 공급한다. |
| [MWRaceHUDWidget.h](../../../Source/MW/UI/Gameplay/MWRaceHUDWidget.h) | 확인 | 개별 계기판 모드의 setter와 상태 변수가 제거됐다. 맵 설정의 소유권 설명은 서브시스템 및 위젯 구현에 있다. |
| [MWLobbyHUD.cpp](../../../Source/MW/UI/Lobby/MWLobbyHUD.cpp) | 보완 | 연출 유무와 관계없이 한 번만 출발을 보고하고, 보고 전에 로컬 가림을 설치하는 이유를 설명했다. |
| [MWLobbyHUD.h](../../../Source/MW/UI/Lobby/MWLobbyHUD.h) | 확인 | 출발 보고 플래그와 시도 번호가 중복 및 지연 콜백을 거르는 이유는 사용 지점에 설명했다. |
| [MWTrackSelectionWidget.cpp](../../../Source/MW/UI/MWTrackSelectionWidget.cpp) | 보완 | 선택 단계 이름표만 접어 배치 포인터가 같은 캔버스에 남게 하는 이유를 설명했다. |
| [MWTrackSelectionWidget.h](../../../Source/MW/UI/MWTrackSelectionWidget.h) | 보완 | 복제 상태 재조회에 관한 기존 주석을 후보 이름표 생성 함수 위에서 실제 상태 바인딩 함수 위로 옮겼다. |
| [MWVehicleCardWidget.cpp](../../../Source/MW/UI/VehicleSelection/MWVehicleCardWidget.cpp) | 보완 | 차량 이름을 별도 툴팁으로 또 띄우면 카드에 겹치는 문제를 설명했다. |
| [MWVehiclePlayerEntryWidget.cpp](../../../Source/MW/UI/VehicleSelection/MWVehiclePlayerEntryWidget.cpp) | 보완 | 별도 호버 안내가 참가자 행의 상태를 덮지 않도록 하는 이유를 설명했다. |
| [MWVehiclePlayerMarkerWidget.cpp](../../../Source/MW/UI/VehicleSelection/MWVehiclePlayerMarkerWidget.cpp) | 보완 | 움직이는 포인터에 별도 이름 툴팁을 띄우지 않는 이유를 설명했다. |
| [MWVehiclePlayerMarkerWidget.h](../../../Source/MW/UI/VehicleSelection/MWVehiclePlayerMarkerWidget.h) | 확인 | 디자이너 끝점 값과 기존 화살표 계산 방식의 호환성이 기존 주석에 있다. |
| [MWVehicleSelectionWidget.cpp](../../../Source/MW/UI/VehicleSelection/MWVehicleSelectionWidget.cpp) | 보완 | 월드 레이아웃을 나올 때 위치뿐 아니라 앵커·정렬·자동 크기도 복구하는 이유를 설명했다. |
| [MWVehicleSelectionWidget.h](../../../Source/MW/UI/VehicleSelection/MWVehicleSelectionWidget.h) | 확인 | 복원에 필요한 슬롯 데이터 구조다. 전체 레이아웃을 복원하는 이유는 사용 지점에 설명했다. |
