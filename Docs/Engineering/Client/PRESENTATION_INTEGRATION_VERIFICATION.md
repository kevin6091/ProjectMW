# 표현 기능 브랜치 통합과 검증

2026-11-23 KST · 로컬 `feature/presentation-integration` · 기준 `origin/main 9a1d9be` (PR #17 포함) · UE 5.8 · UVCS `/main cs:68`

## 통합한 기능

| 작업 브랜치 | 반영 및 확인 |
| --- | --- |
| `feature/vehicle-exhaust-vfx` `2bdb511` | 일반 배기 Niagara와 원격 부스터 겹침 수정. 호스트와 두 클라이언트에서 같은 DOUG의 발동 전, 발동 중, 자동 종료 뒤 배기를 다시 확인했다. |
| `feature/track-supports` `4fe138a` | 설치 조각의 고정 도로 충돌 아래에 지지대를 붙인다. 설원에서 직선, 45도 곡선, 경사, 자동 도착 조각 아래의 지지대가 세 창에서 일치했다. |
| `feature/track-snap-blending` `a6705f2` | `55dcb42`에서 통합했다. 클릭 직후 표시와 서버 설치 위치를 재판정 좌표에 맞췄다. 원격 Q/E 회전의 포트 중심 이동은 최대 0.003cm였다. |
| `feature/track-held-sway` `f4c5a4e` | `f10674c`에서 통합했다. 서로 다른 UE DPI 세 창에서 곡선과 경사의 속도·방향 기반 기울기와 정지 0°를 확인했다. |
| 배기 위치와 크기 `ddb4358`, `c83192d` | 10월 30일 영상에 맞춰 차체 뒤 아래로 분사점과 좌우 간격을 조정했다. 초기 입자 크기는 3.5–7cm다. UVCS `cs:68`은 Niagara 한 파일의 새 버전이다. |

Niagara와 지지대가 `Config/DefaultGame.ini`의 같은 위치에 각자의 소프트 에셋 쿠킹 경로를 추가해 충돌했다. `/Game/Gameplay/Vehicles/Art/Exhaust`와 `/Game/Gameplay/Tracks/Art/Supports`를 한 번씩 남겼다. `MWVehicleExhaustSettings`의 Niagara 경로와 `MWEnvironmentGameplay.TrackSupportMesh`를 각각 유지했다. 스냅과 흔들림 소스는 자동 병합됐다.

UVCS `cs:68`에서 지지대 메시·재질, 엔진음, Niagara 해시를 다시 대조했다. Pending 13개 중 UI와 로고 11개의 바이트가 서버와 일치했다. 서버와 실제 내용이 다른 파일은 InvisiblePilot 차량 BP이고, 로컬 전용인 파일은 InvisiblePilot 주행 충돌 메시다. 두 파일은 기존 차량 작업 범위에 속한다.

## 지지대와 배기 1차 통합 빌드·3인 실행

- Editor Win64 Development 빌드 성공. 기존 C4996 및 XGE 라이선스 경고가 출력됐다. 패키징은 진행하지 않았다.
- Cua Driver로 일반 로비 준비 → 차량 선택 → 트랙 선택과 설치 → 실제 체크무늬 출발선을 확인했다. P1이 직선, P3이 45도 곡선, P2가 경사를 설치했다. 세 창의 지지대 수는 첫 설치 후 1개, 곡선 추가 후 2개, 경사와 자동 도착 조각이 추가된 뒤 4개였다. 12개 인스턴스 모두 충돌이 꺼졌고 바닥 높이는 0.00 cm였다. 도로 쪽 높이는 직선 5,878.84 cm, 곡선 5,878.84 cm, 경사 6,462.41 cm, 도착 7,046.07 cm였다. 세 창 모두 배정 차량을 소유하고 Racing에 진입했다.
- 정상 코스에 상자가 없어 기존 `AMWPickupBox`를 PIE 메모리에만 두었다. 근접 촬영을 준비하는 동안 P2가 코스 밖에 나가 첫 픽업 위치가 어긋나, 차량을 출발 구간에 다시 놓은 뒤 서버 충돌로 직접 획득했다. Cua Driver로 Space를 눌렀고, 세 창의 임시 추적 카메라로 일반 Niagara 2/부스터 0 → 0/1 → 2/0을 확인했다. 종료 뒤 캡처에는 가속 테스트 입력으로 차량이 코스 밖으로 이동하는 장면이 포함된다. 테스트는 효과의 배타 표시와 자동 종료까지이며, 완주·낙하 복귀와 완구·열대의 바닥 접촉은 별도 범위다.
- 기존 자동화 10개 중 9개 통과. `MW.Construction.PreviewMatchesSnap`과 차량 테스트 8개가 통과했다. `MW.Vehicle.CollisionAndGround`의 상승 시 접지 해제와 상승 속도 유지 두 단언만 실패했다. 기준 main에 기록된 두 단언과 같았다. [차량 이동 기준 기록](https://github.com/kevin6091/ProjectMW/blob/03dc60b/Docs/Engineering/Client/VEHICLE_PAWN_LOCAL_MOVEMENT.md#L273-L276)

[3인 화면](Images/PresentationIntegrationThreePlayers.jpg) · [설치·배기 계측·자동화 로그](PRESENTATION_INTEGRATION_LOG.txt)

## 최종 통합 빌드와 3인 실행

`f10674c`의 Editor Win64 Development 빌드 성공. UI 인계 파일 10개와 UVCS `cs:68`의 지지대·Niagara 3개 파일을 서버 해시로 확인했다. Cua Driver로 설원 일반 로비에서 차량을 선택하고 P1 직선, P3 곡선, P2 경사를 차례로 설치했다. 설치 클릭과 서버 좌표의 최대 차이는 0.000000488cm였다. 세 창이 체크무늬 출발선의 지정 위치에서 배정 차량을 소유한 채 `Racing`에 진입했다.

세 창의 도로 아래에 충돌을 끈 철골 지지대가 4개씩 보였다. 네 지지대의 발은 Z 0.00cm였다. 같은 DOUG가 움직이는 동안 호스트와 두 클라이언트에서 일반 배기 2개가 작동했고 상대 장착점은 `(-80.64, ±6.80, 4.50) cm`였다. 부스터 동시 표시 검증은 앞서 배기 보정 구성에서 발동·자동 종료까지 수행했다. 최종 통합 실행은 정상 경기 진입, 설치와 배기 공존을 확인했다. 최대화한 PIE의 비디오 메모리 경고와 기존 C4996 경고가 있었다.

기존 자동화 `MW.Construction.PreviewMatchesSnap`이 통과했다. 차량 테스트는 9개 중 8개가 통과했다. `MW.Vehicle.CollisionAndGround`의 상승 시 접지 해제와 상승 속도 유지 두 단언이 기존 main 기록과 같게 실패했다.

[통합 빌드의 지지대와 세 창 출발선](Images/PresentationFinalThreePlayers.jpg) · [에셋 해시·설치·출발·배기 로그](PRESENTATION_FINAL_PIE_LOG.txt)

## 개별 브랜치에 남긴 후속 작업

| 작업 브랜치 · HEAD | 현재 상태와 보류 이유 |
| --- | --- |
| `feature/vehicle-engine-audio` · `cbd95a1` | 정상 설치 코스 3인 PIE에서 각 창의 차량별 루프와 피치·게인 변화를 확인했다. 단일 원격 차량의 거리 감쇠와 가까이 돌아왔을 때의 출력, 실제 경기 완료 뒤 종료를 추가로 확인해야 한다. 통합 시 `Config/DefaultGame.ini`, `MWVehiclePawn.cpp`, `MWVehiclePawn.h`가 겹친다. SoundWave는 `cs:66`에 있다. |
| `feature/track-theme-colors` · `b32d2a8` | 영상·코드 검토 문서만 있다. 테마별 차이가 확인되지 않았으며 실게임 재질 적용은 없다. |
| `feature/race-results-presentation` · `794d140` | 영상·서버 연동 검토 문서만 있다. 실경기 점수 화면과 시네마틱, 서버 확정 점수 계약이 연결되기 전이다. |

## 이전 로컬 브랜치 점검

기준은 `origin/main`의 최신 SHA와 로컬 브랜치별 공통 조상 이후 변경이다. 이 목록의 컴파일·PIE는 다시 실행하지 않았다.

| 상태 | 브랜치와 근거 |
| --- | --- |
| 최신 main 이력에 포함 | `feature/city-driving-test`, `feature/frontend-ui`, `feature/frontend-ui-integration`, `feature/title-world-ui`, `feature/vehicle-physics-spike`, `feature/vehicle-visuals`, `fix/fix-vehicle`, `fix/post-merge-gameplay`, `fix/track-ui-dpi-verification`. 모두 고유 커밋 0개다. |
| PR #17 인계가 우선 | `fix/track-selection-diamond`의 `a8ea589`은 문서·캡처·로그만 변경한다. PR #17의 `e05bd00`이 같은 검증과 최종 `cs:64` 인계를 포함한다. |
| 에셋 작업 보류 | `fix/frontend-main`의 고유 커밋 `31aef6c`는 InvisiblePilot 장착값 검증과 에셋 인계 문서만 바꾼다. 대응 BP 변경은 UVCS 보류 상태다. |
| 환경·UI의 예전 통합 시도 | `feature/environment-restore-integration`, `feature/race-hud`, `feature/race-hud-guidance`, `fix/frontend-five-issues`, `fix/item-ui-polish`, `integration/environment-ui`. 환경, 로딩·조작, 아이템·HUD 또는 Config의 별도 이력이 남아 있고 PR #17 및 현 환경·UI 소스와 영역이 겹친다. HUD guidance의 고유 커밋은 미검증 WIP다. |
| 차량·충돌·타이틀 이력 보존 | `feature/vehicle-visuals-latest`, `feature/vehicle-wheel-contact`, `fix/vehicle-fit`, `review/vehicle-visuals-audit`. 바퀴 표시·접지 또는 주행 충돌과 타이틀 트래픽 WIP를 포함한다. 현 차량 이동 기준에서 별도 기능 단위 확인이 필요하다. |
| 팀원 브랜치 | `Server/GameProgress`, `fix/Network-Prediction`. 팀원 영역이며 독립적인 서버·예측 변경 이력이 있다. |
| 오래된 로컬 기준 | 로컬 `main`은 `742fe54`이며 최신 `origin/main`과 98개 커밋 차이가 있었다. 통합 기준은 최신 원격 참조를 사용했다. |

기능별 브랜치와 커밋을 보존했고, 통합 결과는 로컬 `feature/presentation-integration`에 있다. 현재 에셋 인계 기준은 UVCS `cs:68`이다.
