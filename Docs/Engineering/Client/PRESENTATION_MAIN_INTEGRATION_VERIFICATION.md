# 최신 경기 진행 기준 표현 기능 통합 검증

2026-11-23 KST · 로컬 `feature/presentation-main-integration` · 기준 `fix/race-arrival@d3a8cbf` · 검증한 코드 `ef2ed63` · UVCS `cs:73`

최신 원격 `main` `1b57b3b`와 `d3a8cbf`는 통합 브랜치 이력에 포함돼 있다. 팀원의 카탈로그 기반 트랙 클래스, 후보 점유와 서버 설치 승인, 라운드 진행과 확정 점수, 후보 재질의 셰이더 준비 경로를 기준으로 기존 표현 변경을 연결했다.

## 커밋과 수정 위치

| 커밋 | 수정 위치와 동작 |
| --- | --- |
| `cdb9245` | 기존 표현 통합 `c453986`을 반영했다. `MWTrackPlacementViewComponent`에서 서버의 `bReversed`를 표시 회전의 시작점으로 사용하고 스냅·해제·흔들림을 적용한다. 클릭은 그 순간의 커서로 다시 판정하고 서버 승인을 받는다. `MWTrackConstructionComponent`의 기존 서버 델리게이트에 로컬 설치 연출 구독을 함께 연결했다. `MWTrackSelectionWidget`은 점유된 후보를 숨기고 하단 문구만 갱신한다. `MWVehiclePawn`은 기존 예측 보정과 부스터·Niagara 연동을 함께 유지한다. `MWArrivalFrame`은 첫 렌더 판정을 분리하고 `d3a8cbf`의 후보 프리로더를 보존한다. |
| `2cd24ce`, `6280a71` | `MWVehiclePawn::Tick/EndPlay`와 `Config/DefaultGame.ini`에 일반 배기와 엔진 루프가 공존한다. `MWVehicleEngineAudio`에서 `Racing`에 재생하고 단계 종료 때 중지한다. |
| `dbd2dd2` | `MWTrackRuntimeSubsystem::IsCourseOnlyTrack`을 후보 추첨과 `MWTrackPlacementPop`에서 공유해 시작·종료 등 코스 전용 조각을 카탈로그 클래스 설정으로 구분한다. |
| `b860aff`, `3e03f03` | `MWTrackSelectionBoard`에서 전시판을 현재 2행 3열의 외곽 범위에 맞춘다. 레벨이 선택한 메시와 높이는 보존한다. 마우스 광선은 표시된 미점유 후보의 상자와 비교해 테마 장식의 월드 충돌이 선택에 관여하지 않게 한다. |
| `ef2ed63` | `MWCursorProjection::SampleViewportPointer`를 선택 및 배치에 연결했다. 엔진 마우스 캐시가 비어 있으면 활성 창의 Slate 절대 좌표를 해당 뷰포트 픽셀로 변환한다. 숨은 커서와 비활성 창은 제외한다. |
| `b7aa36e`, `fbab3ff` | 완주 시네마틱과 테마 색의 기존 조사 기록을 가져왔다. 실제 시네마틱·테마별 재질 작업은 아직 구현 전이다. 현재 기준의 서버 결과 스냅샷은 `MWRoomTypes.h`와 `MWGameplayGameMode.cpp`, 간단한 결과 텍스트는 `MWGameplayHUDSubsystem.cpp`가 소유한다. |

콘텐츠 경로는 `cs:72`의 트랙 카탈로그를 유지하고 `cs:73`의 `Content/UI/Construction/WBP_TrackSelectionTest.uasset`을 사용한다. `cs:72 → cs:73` 서버 변경은 이 WBP 한 파일이다. 서버 SHA-256은 `DB71F35A45FE549371BD700A6567761E0A89001F04AB93BDEC3EDEB4C8EAE094`로 로컬과 일치했다. 파란 배경 레벨은 `cs:71`, 엔진 루프는 `cs:66`, Niagara 축소는 `cs:68`, 설치 불티·연기 재질은 `cs:69`에 들어 있어 `cs:73` 스냅샷에서 같이 받는다. 이번 통합에서 새 바이너리 에셋은 저장하거나 체크인하지 않았다.

## 최신 조합 검증

- UE 5.8.4 `MWEditor Win64 Development` 빌드 성공. `MW.Gameplay`, `MW.Construction`, `MW.Vehicle` 기존 자동화 18개가 최종 코드에서 모두 성공했다. 배치 좌표, 결과 직렬화, 점수·코스 확장·서든데스, 차량 접지와 보정이 포함되며, 예전 `CollisionAndGround` 실패는 이번 실행에서 재현되지 않았다. 기존 `GetBindings()` C4996 경고와 XGE 로컬 실행 경고는 남았다.
- D3D12, 설원 일반 로비, 한 프로세스의 리슨 호스트와 두 클라이언트로 검증했다. 각 창은 `794×514 @0.4755`, `1099×517 @0.4782`, `1384×393 @0.4440`이었다. 첫 실행에서 파란 판이 여섯 후보를 덮지 못했고 표시된 조각을 눌러도 확정되지 않았다. 창을 직접 조작하면서 UE 마우스 조회가 좌표를 반환하지 않는 것을 계측했다. 전시판과 포인터 경로를 수정한 뒤 세 창 모두 정상 공개되고 실제 클릭으로 라운드 1 후보를 서로 다른 ID로 확정했다.
- 세 명이 경사, 분할 직선, 시소 다리를 그 순서로 설치했다. 서버가 승인한 좌표와 클릭 재판정의 최대 축별 차이는 `0.000000482 cm`였다. 설치 조각 3개의 표시 팝이 세 창에서 각각 시작·종료했고 철골 마지막 층은 바닥 `Z=0.00 cm`까지 공개됐다. `MWPlacement accept`의 기존 `pointer=(0,0)` 디버그 필드는 캐시 조회 값이다. 이 실행에서 설치에 사용된 좌표는 Slate 보완 경로에서 얻었고 서버 승인과 비교한 값은 로그의 `resolved`다.
- 전원이 시작·결승 조각을 포함한 5조각 코스의 체크무늬 출발선에서 배정 차량을 소유하고 `Racing`에 들어갔다. 세 창에서 차량별 엔진 루프 3개가 재생됐다.
- Cua Driver의 짧은 반복 키로 정상 출발과 첫 경사 진입까지 확인했다. **전 구간을 손으로 완주한 검증은 아니다.** 다음 단계는 PIE 메모리에서만 세 차량을 실제 결승 볼륨 앞에 옮겼다. 점수·Phase·완주 이벤트에는 직접 쓰지 않았다. 세 창에서 Cua Driver로 가속해 실제 볼륨을 통과했고 서버 완주 순서는 P2 → P1 → P3였다. `RoundResults`에서 결과 세 개와 모든 엔진 루프의 중지를 계측했다. 다음 라운드 세 창에는 동일한 `300 / 150 / 50`점이 유지됐으며 전원이 새 후보를 클릭해 2라운드 `TrackPlacement`까지 들어갔다.

[세 창 전후·실제 출발·2라운드 캡처](Images/PresentationMainIntegrationThreePlayers.jpg) · [런타임·자동화 검증 로그](PRESENTATION_MAIN_INTEGRATION_PIE_LOG.txt)

## main 대상 PR 준비

PR 제목 초안은 **최신 경기 진행에 트랙·로딩·배기 표현 통합**이다. 기준 `main`의 `1b57b3b`가 로컬 통합 브랜치의 조상이고 `git diff --check origin/main...HEAD`가 통과했다. `main`과 `fix/race-arrival`의 원격 SHA는 각각 `1b57b3b`, `d3a8cbf`이다. Git 푸시와 PR 생성 및 `main` 병합은 보류했다.

PR에는 위 `cs:73` 에셋 기준과 3인 PIE의 정상 출발·결승선 직전 보조 재현을 함께 명시한다. 전체 수동 완주, 엔진음 거리 감쇠·복귀 청감, 신버전 완구·열대 철골 접지, 이 SHA에서 기본 45초 뒤 늦게 도착하는 복구 경로와 실제 3대 PC 재시험은 남아 있다. 완주 시네마틱 및 테마별 색 재질은 위 기능 브랜치에 구현된 코드가 아직 없다. 팀 Git 규칙의 독립 리뷰는 PR 전달 후 진행한다. 이번 검증은 에디터/PIE 범위다.
