# 로딩 화면과 목적지 공개 조건

2026-11-19, UE 5.8 Win64, 기본 체크아웃 `fix/post-merge-gameplay`. 조사 기준은 `391c4df`다.

| 수정 위치 | 동작과 편집 방법 |
|---|---|
| `Config/DefaultGame.ini` · `/Game/UI/Common/WBP_MWLoading` | `MWArrivalScreen.LoadingWidgetClass`가 불투명 배경과 중앙 표지판 WBP를 선택한다. Designer에서 배경 Stretch, 중앙 슬롯, 표지판 색·문구·글자 크기를 수정한다. `MWGameplaySignWidget`이 로딩 애니메이션과 벡터 그림을 연결한다. `MWAssetManager::ModifyCook`가 설정된 패키지를 쿠킹 입력에 추가한다. |
| `Source/MW/UI/Lobby/MWLobbyHUD.{h,cpp}` | 로컬 출발 시도마다 가림 확보 뒤 기존 `ReportVehicleDepartureReady()`를 전달한다. 연출 제공자가 시작하면 원래 가림 완료 콜백에서, 제공자가 없거나 시작에 실패하면 HUD에서 바로 호출한다. 시도 번호·현재 단계·보고 여부를 확인한다. |
| `Source/MW/UI/Gameplay/MWArrivalScreen.{h,cpp}` | 게임 인스턴스별로 가림막, 목적지 맵, 로컬 뷰포트, 출발 시도, 렌더 완료 신호를 보관한다. 뷰포트 위젯이 맵 교체를 건너며 목적지 장면은 뒤에서 그린다. 다른 PIE 월드·다른 카메라·SceneCapture의 렌더는 제외한다. |
| `Source/MW/Gameplay/Construction/MWTrackSelectionBoard.{h,cpp}` | 현재 세션 ID·후보 버전과 모든 후보 ID를 비교한다. 각 후보의 등록된 표시 메시, 슬롯 재질, 실제 렌더 상태를 확인한다. 재시도는 로컬 후보를 다시 만든다. |
| `Plugins/MWEnvironmentGameplay` · `Plugins/MWEnvironmentRestore` | 선택한 테마의 하늘·바닥·첫 시야 재질을 선로딩한다. Snow에는 첨탑 재질 두 개를 지정했다. 가시 타일과 카메라가 준비된 생성기만 해당 뷰에 기여한다. 테마 변경·월드 종료 때 핸들과 구독을 정리한다. |
| `MWArrivalScreen::MaterialsReady` · `FMWArrivalFrame` · `Source/MW/MW.Build.cs` | 재질 셰이더 맵, 텍스처 리소스, 실제 상주 밉과 스트리밍 완료를 먼저 본다. 목적지 장면 패스 뒤 GPU fence를 확인한다. 고정 트랙 카메라는 시분할 하늘 조명 갱신 뒤 장면 색을 24×16으로 표본화하여 초기 조명 변화를 확인한다. |
| `MWGameplayHUDSubsystem` · `MWGameplayPlayerController` | 경기 준비 시 대기판과 실제 차량 카메라를 구별한다. 단계가 바뀌면 조건을 다시 적용하고 공개 후 그 단계의 입력 모드·커서·포커스를 복구한다. |
| `Plugins/MWUCMFrontend/Source/MWUCMFrontend/Private/MWUCMTitleFlow.cpp` | 출발·타이틀 복귀에 공통 로딩 내용을 사용한다. 복귀 시 맵 경계에서 불투명 가림을 붙이고 기존 목적지 월드·소유 컨트롤러·타이틀 카메라·루트 메뉴 검사가 끝나면 제거한다. |
| 중단과 복구 | 최소화된 뷰포트의 프레임 신호는 폐기한다. 보이는 목적지에서 45초 동안 준비가 끝나지 않으면 실패 대상과 시도 ID를 기록하고 복구 버튼에 포커스를 준다. 통신·이동 오류와 로컬 후보 재시도를 구별한다. |

## 가림 중 단계가 바뀌는 경우

`MWTrackPreview::CollectVisibleMaterials`는 실제 선택 후보, 설치된 트랙, 배치 고스트의 표시 메시와 슬롯 재질을 검사한다. `MWTrackPlacementViewComponent::GetArrivalGhost`가 현재 배치 세션과 멤버 ID에 속한 고스트를 찾아 준다. `MWArrivalScreen`은 로컬 선택 UI와 새 세션의 준비상태를 다시 확인한다. 3인 PIE에서 가림을 유지한 채 세 명의 트랙을 확정해 첫 공개가 배치 단계로 넘어가는 경우를 검사했다.

소유 Pawn이 늦는 경기 준비 경로에서는 `SMWGameplayHUD::IsWaitingBoardReady`가 표시 중인 대기판에 실제 참가자 셀이 구성됐는지 확인한다. NativeWidgetHost가 이전 프레임의 기하 정보를 보관하는지에 의존하지 않는다. 이 단계에서는 준비용 로컬 컨트롤러 카메라로도 환경 타일을 준비할 수 있다.

## 실제 실행

Snow, 로컬 IP 연결, DX12. `-MWCaptureArrival`를 사용한 에디터 검증은 해당 뷰포트의 첫 공개 백버퍼와 2초 뒤 백버퍼를 저장한다. 아래 구간은 각 프로세스의 단조 시계에서 가림 완료부터 공개까지다.

- 3인 PIE PID 13632. 세 창에서 선택 세션 1·버전 1·재질 6을 확인했다. 호스트 1130×476, 두 클라이언트 1280×722. 첫 진입 세 명 모두 서버가 준비 보고를 한 번씩 받았다. 같은 프로세스에서 새 PIE 세션을 열고 세 번째 참가자의 연출 액터를 제거하자 해당 HUD가 직접 보고했고 나머지 둘은 연출 완료 후 보고했다.
- 트랙 후보가 없는 경기 준비 직행은 세 창 모두 각자의 FRED·DOUG·SMIET 차량 카메라에서 공개됐다. 별도 프로세스 첫 진입에서도 트랙 선택과 DOUG 경기 직행을 각각 검사했다. OS 파일 캐시는 초기화하지 않았다.
- 차량 좌클릭 확정과 우클릭 취소 뒤 실제 월드 트랙을 좌클릭했다. 세 번째 참가자의 `HoveredOfferId=1`, `SelectedOfferId=1`, `bConfirmed=True`가 일치했고 전원이 선택하자 서버가 TrackPlacement로 전환했다.
- 시간·빌드·에셋 해시와 예외 시험의 상세 결과는 이 변경 묶음의 검증 기록에 둔다. 로컬 에셋은 `Content/UI/Common/WBP_MWLoading.uasset`이다.
