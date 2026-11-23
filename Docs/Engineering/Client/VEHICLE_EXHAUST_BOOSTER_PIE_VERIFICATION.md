# 원격 차량 부스터와 일반 배기 겹침 수정

2026-11-23 KST · `feature/vehicle-exhaust-vfx` · UE 5.8 · UVCS `/main cs:67`

## 원인과 수정 위치

기존 일반 배기는 `IsBoosterActive()`를 사용했다. 그 함수가 읽는 `BoosterEndsAt`은 소유자 전용으로 복제되므로 스스로 운전하는 클라이언트는 배기를 끌 수 있지만, 다른 클라이언트는 부스터 불꽃이 붙은 원격 차량에도 일반 배기를 계속 표시할 수 있었다. 네트워크 상태의 복제 범위는 `MWItemComponent.cpp` 65–68줄에 있다.

| 소스 위치 | 수정 내용 |
| --- | --- |
| `Source/MW/Gameplay/Items/MWItemComponent.h:87` | 각 월드에서 실제 생성된 부스터 불꽃을 조회하는 `IsBoosterJetActive()`를 선언했다. |
| `Source/MW/Gameplay/Items/MWItemComponentVisual.cpp:281` | 기존 전체 복제 `BoosterPresentation`을 받아 만든 로컬 표시 컴포넌트의 수명을 조회한다. `StartBoosterJet()`가 불꽃을 등록하기 직전에 일반 배기의 남은 입자도 즉시 제거한다. |
| `Source/MW/Gameplay/Vehicle/MWVehiclePawn.h:175` | 아이템 컴포넌트가 차량 내부의 기존 배기 종료 루틴을 호출할 수 있게 연결했다. |
| `Source/MW/Gameplay/Vehicle/Visual/MWVehicleExhaust.cpp:19` | 불꽃이 존재하는 동안 일반 배기를 멈춘다. 자동 만료로 불꽃이 제거된 다음에는 기존 전후진 속도와 가속 입력 조건에 따라 일반 배기가 재개된다. 발동 시 제거는 기존 `DeactivateImmediate()` 경로를 사용한다. |

## Cua Driver 3인 PIE

정상 로비에서 차량 선택과 설원 코스 배치를 진행했다. P1과 P3이 직선, P2가 경사 조각을 설치했다. 출발·도착 조각을 포함해 서버 코스가 5개였고, 세 창 모두 체크무늬 출발선의 배정 차량을 소유한 상태로 `L_Dev_Drive / Racing`에 진입했다.

이 코스에는 아이템 상자가 없었다. 검사 중에는 기존 `AMWPickupBox`를 PIE 메모리에만 놓고, P2 DOUG가 전진해 서버 충돌로 실제 부스터를 획득하게 했다. Cua Driver로 P2 창에서 Space를 눌렀고, 약 2초의 자동 종료까지 관찰했다. 기본 주행 카메라로 먼저 상태를 기록했다. P2가 관찰 창에서 빠르게 멀어져, 근접 캡처를 위해 같은 경기에서 P2를 출발 구간에 다시 놓고 세 월드의 임시 카메라로 같은 차를 추적했다. 이때 진단 화면 메시지만 숨겼다. 코스나 에셋은 저장하지 않았다.

| 관찰 창 | 발동 전 | 발동 중 | 자동 종료 후 주행 |
| --- | --- | --- | --- |
| P1 호스트에서 원격 DOUG | 일반 2 / 부스터 0 | 일반 0 / 부스터 1 | 일반 2 / 부스터 0 |
| P2 DOUG 본인 | 일반 2 / 부스터 0 | 일반 0 / 부스터 1 | 일반 2 / 부스터 0 |
| P3에서 원격 DOUG | 일반 2 / 부스터 0 | 일반 0 / 부스터 1 | 일반 2 / 부스터 0 |

일반 배기 수치는 활성 Niagara 컴포넌트 수다. 부스터 수치는 화면에 표시되는 `BoosterExhaust` 메시 수다. 세 창 모두 캡처에서 발동 중 일반 배기 자취가 사라지고 종료 뒤 다시 나타났다. 관측한 상태 변경 행에서는 두 효과가 동시에 활성화된 샘플이 없었다. `MWEditor Win64 Development` 빌드도 통과했다. 기존 `UMovieScene::GetBindings` C4996 경고가 출력됐다.

[출발선과 세 창의 발동 전·중·후 캡처](Images/VehicleExhaustBoosterThreePlayers.jpg) · [배치·출발·픽업·효과 상태 로그](VEHICLE_EXHAUST_BOOSTER_PIE_LOG.txt)

## UVCS 미체크인

`/main cs:67` 서버 파일을 내려받아 pending 13개를 로컬 SHA-256과 대조했다. 11개는 서버와 내용이 같았고, 실제 미체크인은 아래 두 건이다.

| 파일 | 현재 상태 |
| --- | --- |
| `Content/Gameplay/Vehicles/BP_MWVehiclePawn_InvisiblePilot.uasset` | 로컬 부스터 장착 위치 `(-42, 0, 4) cm`, yaw `-90°`, 스케일 `0.1`이 포함된 보류본이다. 서버는 기존 `cs:59` 버전이다. 로컬 SHA-256 `b08b6359a43abd31d7c9e4db9e901c4f082d72b9e7b636db928190e8816dae42`, 서버 SHA-256 `46ebebbe6d8125add44da5ad0f57639f3d007dfb1788e6921e1475bee83f2146`. |
| `Content/Gameplay/Vehicles/Art/SM_MW_InvisiblePilot_DrivingCollision.uasset` | UVCS 상태는 `Private`이며 `cs:67` 서버에 파일이 없다. 로컬 복구용 메시다. 현재 주행 충돌은 `CollisionRoot` 박스가 담당한다. 로컬 SHA-256 `93f02b1e794652576fe8a6d34aa3ea837dc2b9c2d33c1de78a79c5bedf2eb561`. |

추가로 트랙 지지대 두 에셋 `cs:65`, 엔진음 `cs:66`, Niagara `NS_MW_VehicleExhaust.uasset` `cs:67`의 서버 해시도 로컬과 일치했다. 이번 수정은 C++과 검증 자료에 한정되어 UVCS 체크인 대상이 생기지 않았다.
