# 트랙 배치 안내·스냅·설치 연출 검증

2026-11-23 KST · `feature/presentation-integration` · UE 5.8.4 Win64 Development Editor

## 작업별 수정 위치

| 커밋 | 작업 | 편집 위치와 동작 |
| --- | --- | --- |
| `c5e33e0` | 좌상단 판과 참가자 목록 제거 | `/Game/UI/Construction/WBP_TrackSelectionTest`의 `ConstructionStatusSafeZone` 아래 `ConstructionStatusFooter → Text_SessionStatus`가 하단 안내를 소유한다. Safe Zone 너비의 18~82%를 사용하고 하단에서 30 디자인 단위를 띄운다. `MWTrackSelectionWidget`과 `MWTrackPlacementViewComponent`는 소유 플레이어에게 필요한 문구만 전달한다. 후보 위 이름표와 기존 마름모 단계 안내는 각 표시 경로에서 제공한다. |
| `02f1f49` | 스냅 정착과 열린 끝점 표시 | [MWTrackPlacementViewComponent](../../../Source/MW/Gameplay/Construction/MWTrackPlacementViewComponent.cpp)는 스냅 진입에 0.18초 정착 곡선을 사용한다. 후반에 작은 오버슈트가 생기고 최종 자세는 기존 스냅 위치에 정착한다. 화면에 정착한 포트가 현재 열린 끝점과 일치할 때 [MWTrackSelectionWidget](../../../Source/MW/UI/MWTrackSelectionWidget.cpp)이 0.20초 접점 섬광을 그린다. 흰 화살표는 복제된 열린 끝점을 각 창 카메라로 투영한다. |
| `29561ee` | 서버 승인 뒤 노면 팝 | [MWTrackPlacementPop](../../../Plugins/MWEnvironmentGameplay/Source/MWEnvironmentGameplay/Private/MWTrackPlacementPop.cpp)은 설치 승인과 복제 이후 새 조각의 보이는 메시를 일시적으로 표시용 복제본으로 대체한다. 맞물린 포트를 중심으로 약 0.23초 동안 1.00→1.075→1.00배로 정착한다. 원본 도로의 Actor·포트·충돌은 서버가 승인한 자세에 놓이며, 로컬 표시 복제본은 충돌을 사용하지 않는다. |
| `e5aade1` | 철골 순차 공개 | [MWTrackSupports](../../../Plugins/MWEnvironmentGameplay/Source/MWEnvironmentGameplay/Private/MWTrackSupports.cpp)는 기존 0.44초 공개 대기 이후 상단부터 바닥까지 0.22초에 걸쳐 각 층을 최종 크기로 등록한다. 설치 과정에 참가하지 않고 이미 놓인 코스를 받은 월드는 완성 상태를 구성한다. |

10월 30일 참고 영상 1:28 부근의 접점, 1:44 부근의 설치 섬광·연기·골조 공개를 기준으로 삼았다. 원본의 스케일 곡선과 연기에 가린 층별 생성 시각은 영상에서 확정하기 어려워, 이 문서에는 적용한 프로젝트 설정과 측정한 PIE 결과를 기록한다. [이전 좌상단 판](Images/TrackStatusPanelMixedDpi.jpg)

## 크기와 DPI가 다른 Cua Driver 3인 PIE

| 창 | 뷰포트 | UMG DPI | 조각 및 역할 |
| --- | ---: | ---: | --- |
| P1 | 784×525 | 0.4856 | 리슨 호스트 · 45도 곡선 |
| P2 | 1098×544 | 0.5032 | 원격 클라이언트 · 경사 |
| P3 | 1416×385 | 0.4440 | 원격 클라이언트 · 직선 |

- 설원 `L_Dev_Drive`의 일반 후보로 두 차례 실행했다. 두 번째 실행은 Cua Driver 녹화를 켜고 세 명이 실제로 차량을 선택하고 트랙을 클릭했다. 좌상단 영역이 비었고, 선택·배치·설치 완료 문구가 하단 중앙에 표시됐다. P1 설치 뒤 P2·P3의 화살표가 다음 끝점 `(3088.25, -721.34, 6000.00)`으로 옮겨갔고 P2 설치 뒤 P3의 표시는 `(6526.10, -4159.18, 7167.23)`으로 옮겨갔다. [세 창 선택·배치·출발선](Images/TrackPlacementFooterMixedDpi.jpg)
- 접점 섬광은 같은 이음새의 좌표로 호스트와 두 클라이언트에서 기록됐다. 두 번째 실행에서 P1은 고스트가 아직 이동 중일 때 클릭했지만, 클릭 재판정과 고정된 표시 위치 차이는 `0.000000cm`였다. 서버가 승인한 조각 2~4의 최종 Transform과 연결 포트는 두 실행에서 일치했다.
- 곡선 팝의 확대 전환은 두 번째 실행의 세 창에서 `1.0682배`로 관측됐다. 직선과 경사도 세 창에 시작·전환·복구 로그가 남았다. 곡선 아래 철골 1·2·3층은 세 창 모두 설치 효과 시작 후 게임 시간 `0.443 / 0.569 / 0.660초`에 등록됐으며, 각 층의 아래 면은 Z=`3919.23 / 1959.61 / 0.00cm`였다. [곡선 설치 프레임](Images/TrackPlacementMotionMixedDpi.jpg)
- 설치 영상은 평균 약 16fps로 녹화했다. 캡처의 PTS와 게임 시간은 서로 다른 시계다. 골조 단계는 로그로 확정했고, 캡처는 섬광에서 바닥 연결까지의 보이는 차이를 담았다. 녹화 중 보이는 녹색 이름표는 Cua Driver 포인터 표식이다.
- 두 실행 모두 서버가 세 명의 설치를 승인했고 세 창이 실제 출발선에서 각자의 차량을 소유한 채 `Racing`에 진입했다. MWEditor 빌드, WBP 컴파일, `MW.Construction.PreviewMatchesSnap` 자동화가 통과했다. [PIE 및 자동화 로그](TRACK_PLACEMENT_FEEDBACK_PIE_LOG.txt)

## UVCS 인계

- **cs:73**은 `Content/UI/Construction/WBP_TrackSelectionTest.uasset` 한 파일만 바꿨다. Cua Driver PIE에 사용한 에셋과 서버 원본의 SHA-256은 `db71f35a45fe549371bd700a6567761e0a89001f04ab93bdec3edeb4c8eae094`로 일치했다. 스냅·설치 팝·철골의 변경은 Git 코드만 사용한다.
- 검증한 작업공간은 **cs:71 에셋 조합과 cs:73 WBP**를 사용한다. 서버 `cs:72`에는 이 Git 브랜치가 사용하는 `Content/Data/TrackDefinition/DA_Track_{Curve,End,Start,Straight,Test}.uasset` 삭제가 있다. WBP 한 파일만 로드한 부분 작업공간에서 체크인했다. cs:71 기준 로컬 작업공간에는 서버와 동일한 WBP 바이트가 들어 있으므로 해당 파일이 `Changed`로 보일 수 있다.
- 동일 조합을 재현하려면 Git `feature/presentation-integration`의 위 네 코드 커밋과 UVCS cs:71 에셋을 기준으로 WBP만 `cm cat "Content/UI/Construction/WBP_TrackSelectionTest.uasset#cs:73" "--file=<임시 다운로드 경로>"`로 받아 SHA-256을 비교한다. 로컬 파일을 교체할 때는 기존 WBP 작업이 있는지 먼저 확인한다.
