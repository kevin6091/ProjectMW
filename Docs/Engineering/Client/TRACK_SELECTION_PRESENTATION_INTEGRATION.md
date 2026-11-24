# 트랙 선택 화면과 로딩 통합 검증

## 최신 main과 로딩 렌더 검사 반영

- 작업 브랜치 `feature/presentation-integration`. 최신 `origin/main` `59c2dc7`을 `f3578bf`에 반영하고, `refactor/arrival-frame` `7e7cb36`을 `987a682`에 반영했다. 자동 병합은 두 건 모두 충돌 없이 완료됐다.
- 첫 장면의 렌더 콜백·펜스 수명은 [MWArrivalFrame](../../../Source/MW/UI/Gameplay/MWArrivalFrame.cpp)이 담당한다. 공개 조건과 늦게 도착한 뒤의 가림막·입력 복구는 [MWArrivalScreen](../../../Source/MW/UI/Gameplay/MWArrivalScreen.cpp)이 담당한다. 이 작업 단위는 Git 소스만 반영했다.
- UE 5.8.4에서 `MWEditor Win64 Development` 빌드 성공. Cua Driver로 리슨 호스트와 클라이언트 2개의 창에서 조작했다. 관측 스크립트는 뷰포트와 게임 상태를 읽고 로그를 남겼다.

| 플레이어 | 게임 뷰포트 | UMG DPI 스케일 |
| --- | ---: | ---: |
| P1 호스트 | 954×447 | 0.4440 |
| P2 클라이언트 | 1149×555 | 0.5134 |
| P3 클라이언트 | 1474×762 | 0.7050 |

- **정상 진입.** 로비에서 세 명이 준비하고 차량을 선택했다. 출발 보고 뒤 세 창이 `L_Dev_Drive`의 트랙 선택 카메라를 공개했다. P3 경사, P1 직선, P2 곡선을 차례로 설치했다. 서버는 조각 2·3·4를 승인했고 세 창 모두 시작 조각 위에서 자기 차량을 소유한 상태로 Racing에 진입했다. 경사 조각 설치와 Racing 시작까지를 검증 범위로 기록한다. [세 창 출발선](Images/PresentationIntegrationRace.jpg)
- **45초 뒤 늦은 도착.** 별도 실행에서 P3의 출발 보고를 지연시키고 P1·P2 타임아웃을 기다렸다. P1·P2에서 출발 월드 `L_Title`의 기본 45초 복구 화면을 기록했다. 이후 지연을 풀어 원래 출발 보고 경로로 `L_Dev_Drive`에 진입했다. 기존 시도의 ID로 새 월드가 공개됐고 세 창의 실제 트랙 선택 클릭이 승인되어 배치까지 열렸다. [45초 안내와 복구 캡처](Images/PresentationIntegrationRecovery.jpg)
- **차량 자동화.** 9개 중 8개 통과. `MW.Vehicle.CollisionAndGround`의 `Upward impulse releases ground`, `Upward impulse is not erased` 두 단언은 기준 main 이력과 동일하게 실패했다. [이전 차량 이동 검증](VEHICLE_PAWN_LOCAL_MOVEMENT.md) · [이번 실행 로그](PRESENTATION_ARRIVAL_PIE_LOG.txt)

## 좌상단 트랙 상태판 축소

- 편집 위치는 `/Game/UI/Construction/WBP_TrackSelectionTest` Designer다. `Size_SelectionGuide` 너비를 402에서 310으로 약 23% 줄였다. `Rows_SelectionGuide` 여백을 좌우 30에서 20으로, 위아래 16·17에서 10으로 줄였다. `PlacementStatusRow`의 위아래 여백은 8·15에서 4·7로 조정했다.
- `Text_Title`과 `Text_SessionStatus`는 기존 Noto Sans KR 글꼴에서 23·18 크기를 사용하고 `Text_Players`는 17을 유지한다. 안내의 줄바꿈 폭은 286에서 270으로 맞췄다. `PlacementStatusSpace` 높이는 129에서 72로 줄여 가장 작은 창에서도 세 줄 안내와 참가자 목록에 공간을 남긴다.
- WBP 컴파일 성공. 같은 세 창에서 수정 전후 선택과 배치를 비교했다. 후보 이름표 3개, 세 참가자 행과 위치·회전 안내를 읽을 수 있었고, P3 설치 뒤 ‘설치했습니다’ 상태와 참가자 목록이 겹치지 않았다. Cua Driver로 세 명의 선택과 설치를 각각 확정했으며 세 창 모두 Racing에 진입했다. 중앙 안내는 `WBP_MatchPanels`가 계속 담당한다. [전후 비교](Images/TrackStatusPanelMixedDpi.jpg) · [승인 및 단계 로그](TRACK_STATUS_PANEL_PIE_LOG.txt)
- 팀원이 받을 파일은 UVCS **cs:70**의 `Content/UI/Construction/WBP_TrackSelectionTest.uasset`이다. 변경 세트는 WBP 1개로 구성됐다. PIE에서 읽은 SHA-256과 서버에서 `cm cat`으로 다시 받은 SHA-256이 `f5905188d18e88c6d2cbd397508b9428128434f9ab16b333f1a2cb05778c514e`로 일치했다.

## 경기 트랙 선택에 파란 전시 배경 복원

- 기존 `SM_TrackSelectionBoard`, `M_TrackSelectionBoard`, BaseColor·Opacity는 UVCS `cs:51`의 `/Game/Gameplay/Tracks/Art/SelectionBoard`에 있었다. 경기 맵 `L_Dev_Drive`에는 참조가 없었고, `L_Dev_TrackSelectionBoardVisual` 개발 레벨에서만 기존 메시를 볼 수 있었다. 10월 30일 참고 영상 1분 16초의 후보 아래 파란 배경을 확인해 이 메시를 연결했다.
- 배치 편집 위치는 `L_Dev_Drive`의 `MWTrackSelectionBoard_0 → SelectionBackdrop`이다. 상대 위치 `(-182, -346, -225)`, 배율 `(2.25, 5.10, 1.00)`으로 후보 3개 아래에 배치했다. 기존 메시와 재질은 그대로 사용했다.
- [MWTrackSelectionBoard.h](../../../Source/MW/Gameplay/Construction/MWTrackSelectionBoard.h)는 컴포넌트를 노출한다. [MWTrackSelectionBoard.cpp](../../../Source/MW/Gameplay/Construction/MWTrackSelectionBoard.cpp)는 현재 선택 세션의 전시 생성 뒤 배경을 표시한다. 배치 단계부터 숨김 상태로 유지하고, 충돌과 오버랩을 끈다. 선택 카메라의 첫 장면 준비에 배경 재질을 합류시켜 후보와 함께 렌더한다.
- 최종 빌드에서 세 창의 뷰포트와 UMG DPI를 통합 기준과 같게 맞췄다. 선택 화면에서 파란 배경과 세 후보·이름표를 확인했다. 세 로컬 월드 모두 가림막 공개 전에 배경 표시 상태였으며, 준비 재질은 6개에서 7개로 늘었다. 배치로 넘어간 뒤 세 월드에서 `hidden=True`, `NO_COLLISION`을 기록했다. DPI는 Unreal 뷰포트의 UMG 배율을 기록했다.
- P1 직선, P2 곡선, P3 경사를 Cua Driver로 선택했다. 서버는 P3 → P1 → P2 순으로 설치를 승인했고, 조각 2·3·4의 연결 끝점과 설치 좌표는 통합 기준 실행과 일치했다. 세 플레이어 모두 시작 조각 위에서 차량을 소유한 상태로 Racing에 진입했다. `MW.Construction.PreviewMatchesSnap` 자동화도 통과했다. [세 창 선택·배치·출발선 화면](Images/TrackBlueBackdropMixedDpi.jpg) · [재질·표시 상태와 서버 배치 로그](TRACK_BLUE_BACKDROP_PIE_LOG.txt)
- 팀원이 받을 경기 맵은 UVCS **cs:71**의 `Content/Levels/L_Dev_Drive.umap`이다. 변경 세트는 맵 1개로 구성됐다. PIE에서 사용한 맵과 서버에서 다시 받은 맵의 SHA-256은 `8e642cd75fd0db3a5c5f68f4bd41b06baab33decc442fd234023286d991bd2f9`로 일치했다. 통합 브랜치의 배경 코드와 UVCS `cs:71`을 함께 적용하면 위 화면을 재현할 수 있다.

## 후보 전시 공간과 배치 손맛

- 첨부 멀티 영상 1:14–1:19와 2:38–2:42, Switch 영상 9:30–9:34에서 후보가 이름표 없이 자기 칸에서 움직이는 모습을 확인했다. [MWTrackSelectionWidget](../../../Source/MW/UI/MWTrackSelectionWidget.cpp)에서 전시 이름표를 제거했다. [MWTrackSelectionBoard](../../../Source/MW/Gameplay/Construction/MWTrackSelectionBoard.cpp)는 후보를 슬롯 중앙에 맞추고 회전시킨다.
- 선택판의 배경·슬롯을 별도 전시 루트로 옮겼다. 이전 라운드 코스와 지지대의 화면 투영 범위에 따라 루트의 거리를 계산하며, 수신 창은 자신의 전시 오프셋을 원격 커서에 적용한다. 배치 카메라와 서버의 연결점 좌표에는 월드 이동을 적용하지 않는다.
- [MWTrackPlacementViewComponent](../../../Source/MW/Gameplay/Construction/MWTrackPlacementViewComponent.cpp)의 하단 안내는 ‘끝점에 가까이 가면 스냅됩니다’와 ‘Q/E 키로 트랙을 회전할 수 있습니다’를 각각 한 줄로 표시한다. 마우스를 움직일 때만 입력 방향과 속도로 고스트가 기울며, 스냅할 때는 기존 위치 보간과 함께 와이어프레임 크기가 한 번 변한다. 클릭 시에는 기존 설치 규칙으로 위치를 다시 판정한다.
- 편집값은 [MW Track Presentation 설정](../../../Config/DefaultGame.ini)에서 관리한다. 후보 회전 30°/초, 배치 이동 보간 0.12초, 스냅 크기 최대 1.10배와 복귀 시간 0.26초, 들기 기울기 최대 6°를 사용했다. 전시 및 배치 안내, 카메라 분리, 스냅 연출에 쓰는 WBP나 레벨 에셋은 수정하지 않았다.
- Cua Driver로 1라운드 트랙 3개를 골라 설치했고 세 창에서 출발선 Racing에 진입했다. P1의 고스트는 좌우 최대 6°를 보였으며 커서를 멈추면 0°로 복귀했다. 스냅 중 와이어프레임의 1.0986배와 복귀한 1.0000배를 기록했다. 설치 승인 로그에서 확정 조각의 배율은 1이고 끝점 오차는 0cm였다. 2라운드 선택 시 이전 라운드 시작 조각의 화면 X 최댓값이 P1 −516px, P2 −605px, P3 −494px로 세 창 모두 왼쪽 화면 밖이었다. [선택 1·2라운드와 배치 안내](Images/TrackSelectionStageAndGuide.jpg)

## 선택 후보의 반투명 표면과 호버 확대

- Switch 영상 34.60–36.78초를 프레임 단위로 확인했다. 35.55초에 빨간 커서가 올라가면서 나선형 후보가 밝아지고 커졌고, 35.70초에는 커서가 남은 상태에서 크기가 돌아오기 시작했다. 36.24초에 좌하단 곡선 후보에서도 확대가 보였다. 36.24초에 나선형 후보가 사라진 것은 별도의 선택 확정 동작이다.
- [MWTrackSelectionBoard](../../../Source/MW/Gameplay/Construction/MWTrackSelectionBoard.cpp)는 로컬 포인터와 기존에 복제되는 원격 호버 상태를 현재 세션·후보 버전에서만 표시한다. 호버 진입 때 후보 표면과 와이어프레임이 함께 최대 1.12배까지 커졌다 0.22초 안에 돌아온다. 가리키는 동안 와이어프레임은 밝은 색으로 유지된다. 클릭 상자는 전시 회전만 따라가고 호버 확대는 상속하지 않는다. 호버 시간·배율·색은 Project Settings의 MW Track Presentation에서 편집한다.
- [MWTrackPreview](../../../Source/MW/Gameplay/Construction/MWTrackPreview.cpp)는 선택용 BP 인스턴스에서 원래 보이던 표면의 텍스처와 색을 반투명 인스턴스로 옮긴다. 이를 기존 와이어프레임과 합성하고 호버 색은 와이어프레임 재질에만 적용한다. 프로젝트의 색·알베도 파라미터와 단일 텍스처 로고·데칼 슬롯을 지원한다. 복합 셰이더를 가진 파라미터 없는 슬롯은 변환에서 제외한다. [선로딩](../../../Source/MW/Gameplay/Construction/MWTrackPreviewPreload.cpp)에서 반투명 셰이더를 후보 BP와 함께 준비한다.
- 편집 에셋은 `/Game/Gameplay/Tracks/Art/SelectionBoard/M_TrackSelectionSurface`이며 Material Editor의 `SurfaceOpacity`에 0.37을 둔다. 원본 도로의 알베도와 색을 사용하며 조명 반사와 노멀은 반투명 전시 재질에 복제하지 않는다. 배치 고스트는 와이어프레임 전용 경로를 사용하고, 서버가 생성하는 설치 조각은 원래 머티리얼을 사용한다.

| 가리킨 플레이어 | 대상 | 자기 창에서 최대 | 다른 두 창에서 최대 | 복귀 배율 | 클릭 상자 배율 |
| --- | --- | ---: | ---: | ---: | ---: |
| P1 | 1번 후보 | 1.08789 | 1.08789 / 1.08789 | 1.00000 | 1.00000 |
| P2 | 2번 후보 | 1.11971 | 1.08934 / 1.08934 | 1.00000 | 1.00000 |
| P3 | 3번 후보 | 1.11996 | 1.08448 / 1.08448 | 1.00000 | 1.00000 |

- 표는 게임의 프레임마다 관측한 값이다. 1.12는 설정상 최대이며, 한 번의 프레임이 정점 전후를 건너뛰면 샘플의 최댓값은 작아진다. 커서를 계속 둔 동안 크기가 1로 복귀한 뒤 다음 입력 전까지 다시 커지지 않았다.
- 최종 Cua Driver 3인 PIE의 선택·설치 창은 794×514 (0.4755), 1099×517 (0.4782), 1384×393 (0.4440)이었다. 호버 단독 입력 때 실제 OS 포인터를 옮기기 위해 세 창을 주 모니터로 옮겼고, 설치 검증 때 위 크기와 DPI로 돌아왔다. 세 창에서 후보 6개마다 반투명 표면과 와이어프레임이 함께 보였다. 각 창의 표면 슬롯 중 75개가 `M_TrackSelectionSurface`를 사용했다.
- 최종 빌드에서 Cua Driver로 고른 후보 번호는 P1 1번, P2 6번, P3 2번이었다. 서버는 설치 조각 2·3·4를 승인했다. 설치 클릭 시점 재판정 후 끝점 오차가 모두 0cm였고 세 창 모두 실제 출발선에서 Racing에 진입했다. 1라운드 통과는 검증 보조로 실제 체크포인트와 결승 트리거를 거쳤다. 최종 2라운드 선택 화면에서도 시작 조각의 화면 X 최댓값이 −698·−819·−668px로 전부 왼쪽 화면 밖이었다.
- 한 프레임에 커서를 크게 옮기고 바로 클릭한 실행에서는 시각 보간 중인 고스트와 클릭 시 서버 검사를 통과한 위치 사이 거리가 로그에 남았다. 입력 직후에도 보간 완료를 기다리지 않고 재판정해 설치하는 기존 동작이다. 세 창 동시 실행 중 GPU 메모리 초과 안내가 캡처에 표시됐다. [선택·호버·출발선 비교](Images/TrackSelectionSurfaceHoverAndRacing.jpg) · [정량 검증 로그](TRACK_SELECTION_PRESENTATION_PIE_LOG.txt)
- 반투명 머티리얼 1개를 UVCS **cs:77**에 별도 체크인했다. `cm cat`으로 받은 서버 파일과 로컬 파일의 SHA-256은 모두 `f942ec4cb059451b6432c91c53eb6e723f39a532b98a62be54e33bcb30859ee3`이다. 팀원은 이 Git 변경과 cs:77의 `Content/Gameplay/Tracks/Art/SelectionBoard/M_TrackSelectionSurface.uasset`을 함께 받아야 한다. Git 원격/PR과 패키지에는 변경을 적용하지 않았다.

## 배치 첫 접점에서 섬광을 시작하고 선택 표면을 더 투명하게 조정

- **원본 기준.** Switch 60fps 영상 00:50.8993에서 곡선 고스트가 끝점으로 접근하고 다음 프레임 00:50.9160에서 접점과 불꽃이 함께 보인다. 멀티 60fps 영상에서도 01:28.0833의 자유 고스트가 다음 01:28.1000 프레임에 붙고 불꽃이 보인다. 카메라·각도·투시가 달라 원본의 고스트 배율과 오퍼시티 파라미터는 영상에서 판독할 수 없다.
- **스냅 타이밍.** [MWTrackPlacementViewComponent](../../../Source/MW/Gameplay/Construction/MWTrackPlacementViewComponent.cpp)의 `BlendGhostPose`가 기존 보간에서 처음 끝점 거리에 도달했을 때 정확한 연결 자세를 반환하고, 이 프레임에 `PulseSnapContact`를 한 번 호출한다. `UpdateVisualPose`는 같은 프레임에 기존 최대 배율을 사용하고 이어서 1.0으로 돌아온다. 현재 접점과 확대 시간은 기존 설정을 사용한다. `HandleAccept`의 클릭 순간 재판정과 서버의 설치 판정은 그대로다.
- **재질.** `M_TrackSelectionSurface` Material Editor의 `SurfaceOpacity` 기본값만 0.37에서 0.25로 낮췄다. 선택판 3개 창에서 표면 아래의 파란 판이 더 보이고 와이어프레임도 같이 표시됐다. 수정 전후 비교는 같은 테마의 유사한 후보를 사용했으며 랜덤 후보와 회전 자세가 서로 다르다. [원본·0.37·0.25 비교](Images/TrackSelectionSurfaceOpacityComparison.jpg)
- **정상 속도 3인 PIE.** 뷰포트는 P1 794×514 (0.4755), P2 1099×517 (0.4782), P3 1384×393 (0.4440)이다. P1 곡선이 스냅된 시점에서 세 창 각각 접점, 위치 보간 비율 1.0000, 와이어프레임 크기 1.1000이 같은 게임 프레임에 기록됐다. 보간 경과 시간은 세 창 모두 0.1214초였다.
- **프레임 비교 캡처.** Cua Driver가 지정한 호스트 창에서 접근과 접점 전후를 연속 캡처했다. 이 캡처에서만 게임 속도를 0.12배로 낮춰 가시 구간을 늘렸다. 원격 두 창에서 0.1159 게임 초에 접점과 1.1000배를 기록했고 0.2615 게임 초에 1.0000배로 돌아왔다. 캡처는 창의 샘플이며 화면에서 접점 섬광 시작 프레임을 단독으로 판독하기 어려워 각 창의 발동은 게임 로그로 확인했다. [Switch·PIE 프레임 비교](Images/TrackSnapContactFrameComparison.jpg)
- **설치와 출발.** 속도를 1배로 되돌린 뒤 세 창에서 직접 설치를 확정했다. 서버는 조각 2·3·4를 각각 승인했고 세 설치의 클릭 재판정 끝점 오차는 0cm였다. Cua Driver가 커서를 크게 옮긴 직후 클릭한 경우 화면상 고스트는 아직 보간 중이었다. 승인 좌표는 클릭 위치에서 다시 계산한 연결 자세였다. 이후 세 창 모두 실제 출발선에서 본인 차량을 소유한 상태로 Racing에 진입했다. MWEditor Win64 Development 빌드 성공. [게임 로그와 서버 좌표](TRACK_SNAP_CONTACT_TIMING_PIE_LOG.txt)
- **인계.** UVCS **cs:78**에는 `Content/Gameplay/Tracks/Art/SelectionBoard/M_TrackSelectionSurface.uasset` 하나만 체크인했다. 서버 원본, 로컬, PIE가 읽은 파일의 SHA-256은 `e951ff4a4988a63310e1954324d611ec8e1d4474c891219d52e4f550916917da`로 일치한다. 0.25 화면을 재현하려면 cs:78의 재질을 받는다.
