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
