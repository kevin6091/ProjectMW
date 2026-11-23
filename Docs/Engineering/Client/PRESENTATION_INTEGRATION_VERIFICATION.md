# 표현 기능 브랜치 통합과 검증

2026-11-23 KST · 로컬 `feature/presentation-integration` · 기준 `origin/main 9a1d9be` (PR #17 포함) · UE 5.8 · UVCS `/main cs:67`

## 통합한 기능

| 작업 브랜치 | 반영 및 확인 |
| --- | --- |
| `feature/vehicle-exhaust-vfx` `2bdb511` | 일반 배기 Niagara와 원격 부스터 겹침 수정. 호스트와 두 클라이언트에서 같은 DOUG의 발동 전, 발동 중, 자동 종료 뒤 배기를 다시 확인했다. |
| `feature/track-supports` `4fe138a` | 설치 조각의 고정 도로 충돌 아래에 지지대를 붙인다. 설원에서 직선, 45도 곡선, 경사, 자동 도착 조각 아래의 지지대가 세 창에서 일치했다. |

두 브랜치가 `Config/DefaultGame.ini`의 같은 위치에 각자의 소프트 에셋 쿠킹 경로를 추가해 충돌했다. `/Game/Gameplay/Vehicles/Art/Exhaust`와 `/Game/Gameplay/Tracks/Art/Supports`를 한 번씩 남겼다. `MWVehicleExhaustSettings`의 Niagara 경로와 `MWEnvironmentGameplay.TrackSupportMesh`도 각각 유지했다. C++은 자동 통합됐으며, 에셋은 기존 UVCS 버전을 사용했다. 지지대 메시·재질은 `cs:65`, Niagara는 `cs:67`의 서버 해시와 일치한다. 이전에 보류한 InvisiblePilot 차량 BP와 로컬 충돌 메시는 이 검증 범위 밖이다.

## 통합 빌드와 3인 실행

- Editor Win64 Development 빌드 성공. 기존 C4996 및 XGE 라이선스 경고가 출력됐다. 패키징은 진행하지 않았다.
- Cua Driver로 일반 로비 준비 → 차량 선택 → 트랙 선택과 설치 → 실제 체크무늬 출발선을 확인했다. P1이 직선, P3이 45도 곡선, P2가 경사를 설치했다. 세 창의 지지대 수는 첫 설치 후 1개, 곡선 추가 후 2개, 경사와 자동 도착 조각이 추가된 뒤 4개였다. 12개 인스턴스 모두 충돌이 꺼졌고 바닥 높이는 0.00 cm였다. 도로 쪽 높이는 직선 5,878.84 cm, 곡선 5,878.84 cm, 경사 6,462.41 cm, 도착 7,046.07 cm였다. 세 창 모두 배정 차량을 소유하고 Racing에 진입했다.
- 정상 코스에 상자가 없어 기존 `AMWPickupBox`를 PIE 메모리에만 두었다. 근접 촬영을 준비하는 동안 P2가 코스 밖에 나가 첫 픽업 위치가 어긋나, 차량을 출발 구간에 다시 놓은 뒤 서버 충돌로 직접 획득했다. Cua Driver로 Space를 눌렀고, 세 창의 임시 추적 카메라로 일반 Niagara 2/부스터 0 → 0/1 → 2/0을 확인했다. 종료 뒤 캡처에는 가속 테스트 입력으로 차량이 코스 밖으로 이동하는 장면이 포함된다. 테스트는 효과의 배타 표시와 자동 종료까지이며, 완주·낙하 복귀와 완구·열대의 바닥 접촉은 별도 범위다.
- 기존 자동화 10개 중 9개 통과. `MW.Construction.PreviewMatchesSnap`과 차량 테스트 8개가 통과했다. `MW.Vehicle.CollisionAndGround`의 상승 시 접지 해제와 상승 속도 유지 두 단언만 실패했다. 기준 main에 기록된 두 단언과 같았다. [차량 이동 기준 기록](https://github.com/kevin6091/ProjectMW/blob/03dc60b/Docs/Engineering/Client/VEHICLE_PAWN_LOCAL_MOVEMENT.md#L273-L276)

[3인 화면](Images/PresentationIntegrationThreePlayers.jpg) · [설치·배기 계측·자동화 로그](PRESENTATION_INTEGRATION_LOG.txt)

## 개별 브랜치에 남긴 후속 작업

| 작업 브랜치 · HEAD | 현재 상태와 보류 이유 |
| --- | --- |
| `feature/track-snap-blending` · `3901158` | 실제 설치는 재판정과 일치했지만 전환 중 클릭한 화면의 고스트와 설치 위치 차이가 최대 12,806.346 cm였다. 운영 클릭 경로에서 표시와 확정 피드백을 맞춘 뒤 재검증한다. 자유 Q/E의 포트 중앙 보간은 로컬 전용이며 원격 창은 이전 보간이다. |
| `feature/track-held-sway` · `b4651a5` | 스냅 변경 3개를 포함하므로 동일 결함을 가져온다. 소유 창은 실제 포인터 속도, 원격 창은 수신한 포즈의 화면 이동량을 사용한다. 혼합 DPI와 곡선·경사에서 원격 방향 전환 검증이 남았다. |
| `feature/vehicle-engine-audio` · `932b8f8` | 직접 Racing에 진입한 3인 PIE에서 루프와 피치·게인 변화는 계측됐다. 정상 설치 코스에서 청취, 감쇠, 종료 검증이 남았다. Niagara와 함께 합치면 `Config/DefaultGame.ini`, `MWVehiclePawn.cpp`, `MWVehiclePawn.h`에서 Git 충돌이 난다. 두 기능의 루프/표시 호출과 정리 경로가 모두 살아야 한다. 대응 SoundWave는 `cs:66`에 있다. |
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

통합 브랜치에서 Niagara 병합을 먼저 확인하고 지지대 설정 충돌을 해결했다. 기능별 원래 브랜치와 커밋은 보존했으며 결과는 로컬 통합 브랜치에만 기록했다.
