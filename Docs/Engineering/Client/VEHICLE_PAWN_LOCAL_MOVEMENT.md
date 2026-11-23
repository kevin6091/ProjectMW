# Pawn 기반 차량 로컬 이동

2026-11-04 / Client + Server. 사용자의 Pawn 전환 지시와 단계 1~6 구현 요청을 적용한다.
기존 07 차량 설계의 Character/CMC 설명은 과거 방향이며, 이번 로컬 구현은 이 문서를 따른다.
현재 S/Reverse 입력은 제동 후 후진을 지원한다. 기존 구현을 유지한 것이며 S를 제동 전용으로 바꾸지 않았다.

## 연결과 초기화

- `AMWVehiclePawn : APawn`이 `CollisionRoot`와 `VehicleMovement`를 생성한다.
- `UMWVehicleMovementComponent : UPawnMovementComponent`의 UpdatedComponent는 CollisionRoot다.
- 루트는 물리 시뮬레이션을 끈 BoxComponent이며 Query Collision을 사용한다. Blueprint에서 Box Extent/충돌 설정을 조정한다.
- Blueprint의 VehicleMovement에 DriveData, Pawn에 DrivingInputData를 지정해야 한다. 에셋 경로를 C++에 하드코딩하지 않는다.
- 주행 Action 바인딩과 Mapping Context 수명은 `MWGameplayPlayerController`에서 관리한다. 기존 차량 BP의 DrivingInputData를 읽어 현재 조종 중인 Pawn에 `SubmitDriveInput()`으로 입력을 전달한다. Pawn은 이동·네트워크 처리와 입력 초기화를 담당한다.
- `MWDriveTestGameMode`의 C++ 기본 Controller는 `MWGameplayPlayerController`다. 기존 테스트 Controller 클래스 삭제 후 개발자 주행 GameMode BP는 `/Game/System/BP_GameplayPlayerController`를 사용한다.
- BeginPlay에서 충돌 루트와 DriveData를 검증한다. 필수 설정 누락/오류이면 로그를 남기고 속도 초기화 및 Tick 중단으로 실패한다.
- DriveConfig는 DriveData의 사본이다. 실행 중 원본 DataAsset을 수정하지 않는다.

## 시뮬레이션 순서

1. 접지 검사와 노면 자세 조사
2. 노면 정렬·입력 조향·충돌 회전을 한 경로에서 계산하고 회전 후보 겹침 검사
3. 접지 시 추진/제동/자연 감속/횡그립, 공중이면 중력 계산
4. SafeMoveUpdatedComponent 이동, 접촉면별 속도 보정 후 남은 시간 재이동
5. 이동 후 접지 재검사 및 바닥 간격 보정
6. 컴포넌트 속도 정보 갱신

Tick은 1/120초 이하 간격으로 나눠 계산하고 한 프레임당 최대 0.25초를 처리한다.
Standalone은 한 프레임에 최대 0.25초를 처리한다. 네트워크 주행은 같은 물리 단계 함수를 120Hz 고정 스텝으로 실행하며 프레임에 쌓을 시간을 0.10초로 제한한다.
네트워크 월드에서는 서버와 소유 클라이언트가 주행을 시뮬레이션한다. 다른 클라이언트의 차량은 서버 스냅샷을 보간한다.

## 접지와 충돌

- 중심선 대신 실제 루트 CollisionShape의 아래 방향 Sweep을 사용한다.
- Box 바닥 Sweep이 옆벽과 겹친 상태로 시작하면 가로·세로 반경을 최대 0.5cm씩 줄여 한 번 더 검사한다. 검사 높이와 실제 차량 충돌체는 유지하고, 재검사에서도 경사와 이탈 속도 기준을 적용한다.
- Object Type과 Collision Response를 함께 사용해 실제 이동 충돌과 검사 대상을 맞춘다. Visibility 채널에 의존하지 않는다.
- MaxGroundAngle보다 가파른 면은 주행 바닥으로 인정하지 않는다.
- 바닥 법선 방향 이탈 속도가 GroundDetachSpeed를 넘으면 접지를 해제해 위쪽 충격을 지우지 않는다.
- GroundProbeDistance 범위 안에서 GroundClearance를 유지한다. 착지 시 간격이 작아지면 위로, 크게 벌어지면 아래로 안전 이동한다.
- 간격 보정 이동량을 Velocity에 더하지 않는다.
- 박스 회전은 중간/최종 자세 겹침 검사로 막힌 후보를 거절한다. 연속 회전 궤적 전체의 정확한 충돌이나 모든 좁은 틈을 보장하는 방식은 아니다.
- 접지 중 벽에 닿으면 노면과 평행한 접근 속도를 반사한다. `WallRestitution` 기본값은 0.3이며, `MinWallBounceSpeed` 기본값인 150cm/s보다 느린 접근에서는 안쪽 속도만 제거한다. 전체 주행 속도가 아니라 벽을 향하는 속도로 판단한다.
- 벽 반발 방향은 현재 `GroundNormal`에 수직인 평면으로 제한한다. 월드 Z를 지우거나 상승 속도·차체 충돌 회전을 추가하지 않는다. 도로 지지면과 공중 접촉에는 기존 속도 보정을 적용하며 다른 차량에 벽 반발 설정을 적용하지 않는다.
- 각 접촉 뒤 남은 시간과 변경된 속도로 다시 Sweep한다. 기존 `SlideAlongSurface`의 충돌 전 이동량 재사용은 제거했다. 한 이동 스텝에서 최대 4회 접촉을 처리하고, 시간과 속도 모두 진행되지 않는 접촉에서는 중단한다.
- 차량끼리 부딪히면 서버가 질량과 상대 속도로 두 차량의 수평 충돌 반응을 계산한다. `CollisionMomentumRetention`은 접촉 법선 방향의 공통 속도를 얼마나 유지할지 정한다. 가까운 속도로 달리는 차량이 가볍게 닿을 때는 공통 감속량을 상대 접근 속도에 비례해 제한한다.
- 월드 -Z 중력을 사용하고 노면에 맞춰 차체 Pitch/Roll을 정렬한다. 접지 하중·다운포스·자세 조사는 main의 이동 계산을 따른다. 계단 오르기와 지연된 원격 승강기 추종의 완성은 보장하지 않는다.

## 설정 검증과 수명

속도/가속도/그립/제동/조향/중력/접지 설정의 유한값과 범위를 검사한다.
GroundProbeDistance > GroundClearance, FullBrakeDeceleration >= CoastDeceleration,
FullSteeringSpeed > MinSteeringSpeed 관계를 확인한다.
MinDriftSteering 및 DriftFrictionMultiplier는 [0,1], MaxGroundAngle은 [0,89], MaxFallSpeed는 1 이상이다.

Controller는 Pawn 교체·입력 컴포넌트 재생성·종료 시 등록한 바인딩과 Driving Context를 정리하고 이전 차량의 입력을 초기화한다. 같은 조종 상태를 다시 통지받아도 바인딩을 중복 등록하지 않는다.
Pawn의 UnPossessed에서도 저장 입력을 초기화한다.
Controller의 입력 핸들러는 Pawn의 현재 입력을 읽어 한 항목씩 갱신하므로 텔레포트로 초기화된 가속 입력을 다시 적용하지 않는다.
TeleportTo 성공 시 Pawn/Movement의 저장 입력과 접지/회전 캐시를 초기화한다.
텔레포트 자체가 속도를 지우지는 않는다. 체크포인트 복귀에서 정지가 필요하면 StopMovementImmediately를 함께 사용한다.
EndPlay에서도 입력과 Driving Context를 정리한다. 경기 Phase 허용 조건과 실제 준비 보고·카운트다운 진행은 아래 2026-11-20 절에 연결했다.

## 검증

설치 엔진은 UE 5.8.4 / CL 58902052다. 기존 문서의 5.8.3과 달리 실제 설치 버전으로 기록하며 엔진을 변경한 것은 아니다.

`MW.Vehicle` 엔진 자동화는 실제 Physics Scene을 가진 임시 게임 월드에서 native Pawn과 충돌체를 생성한다.
Pawn 제어 입력 API와 이동 Tick을 실행하므로 키보드/Enhanced Input 바인딩이나 실제 게임 맵의 시각 품질 검증과 구분한다.

| 자동화 | 확인 대상 |
|---|---|
| ConfigValidation | 기본 설정과 NaN/잘못된 범위/관계 거절 |
| LocalControls | 정지 조향, 최고속도, 자연 감속, 동시 제동, 후진/방향 전환, 관성 조향, 드리프트, 입력 잠금, 비정상 입력 |
| CollisionAndGround | 중력/착지/바닥 간격, 상승 시 접지 해제, 낙하 속도 제한, 사선 벽 충돌, 첫 스텝의 회전 겹침 거절, 가장자리 추락, 경사/급경사 |
| WallBounce | 반발 계수, 접선 속도, 저속 스침, 같은 스텝의 반대 방향 이동, 코너 연속 접촉, 공중 접촉 유지, 프레임 간 비교, 회전된 노면의 반발 방향 |
| FrameRateAndReset | 30/120FPS 결과 비교, 텔레포트/조종 해제 후 입력 잔류 |

검증 로그와 Blueprint 확인 자료는 로컬 `Saved/Codex/VehiclePawnLocal`에 둔다.
자동화 결과, Editor/Game 빌드, Blueprint 연결, 수동 시각 주행, 패키지/네트워크 검증을 각각 구분해 보고한다.

2026-11-04 최종 소스 검증: MWEditor Win64 Development 및 MW Win64 Development 빌드 통과.
MW.Vehicle 자동화 4/4 통과(테스트 경고 0, 실패 0). 30/120FPS 비교 허용 오차는 위치 1cm, 속도 1cm/s다.
첫 실행에서 착지 후 목표 간격 미복원을 찾아 상하 양방향 보정으로 수정했다.
드리프트는 조향 방향이 초기 횡속도를 따라잡는 경우 절대 횡속도가 더 작아질 수 있으므로,
횡그립 비교는 반대 횡속도를 가진 카운터스티어 조건으로 검증했다(일반 376.86, 드리프트 448.13cm/s).
### 차량 Blueprint 교체와 기존 맵 주행

사용자 지시에 따라 `/Game/Gameplay/Vehicles/BP_MWVehicleCharacter`를 같은 패키지 및 클래스 경로에 새로 만들고 직접 부모를 `/Script/MW.MWVehiclePawn`으로 지정했다.
기존 `DA_Driving_InputData`와 카메라 설정을 적용하고 새 VehicleMovement에 `DA_Vehicle_DriveData`를 지정했다.
기존 테스트 큐브 메시와 재질은 새 CollisionRoot에 연결했다. 차체 외형을 200×100×60cm 박스에 맞추고 메시 충돌을 껐다.
원본은 `Saved/Codex/VehiclePawnLocal/Before/BP_MWVehicleCharacter.uasset`에 보관한다.

UE 5.8.4의 새 에디터 프로세스에서 저장된 BP의 직접 부모, 데이터 연결과 컴파일을 확인했다.
기존 `/Game/Developers/user/L_Dev_Drive`의 DriveTest GameMode가 같은 경로의 BP를 스폰하고 조종했으며,
차체 메시가 CollisionRoot에 붙고 Movement의 UpdatedComponent가 CollisionRoot를 가리키는 것을 확인했다.
렌더링을 끈 1인 PIE에서 Enhanced Input 테스트 키로 실제 입력 매핑 및 바인딩을 실행했다.

| 입력 | 관측 결과 |
|---|---|
| W 약 0.75초 | 전방 445.01cm 이동, 전진 속도 1186.70cm/s |
| W+D 약 0.4초 | 우측 Yaw 39.17도 |
| W+S 약 0.9초 | 0cm/s까지 제동 |
| S 약 0.65초 | 차체 전방 기준 -275.01cm 이동 |

측정 위치·속도·접지 결과는 `Saved/Codex/VehiclePawnLocal/pie-drive-result.json`에 기록했다.
UVCS item 907의 잠금 `db21ba2e-182b-4025-96a3-2b66708bafc8`을 확보해 차량 BP 한 파일을 교체했고, 로컬 체크아웃 상태로 인계한다.

### 후속 작업에서 추가한 네트워크 주행

- Pawn의 소유 클라이언트는 120Hz 고정 스텝으로 즉시 주행하고, 각 입력과 실행 결과를 순번별로 보관한다. 30Hz 목표 주기로 아직 확인되지 않은 입력을 묶어 Unreliable Server RPC로 전송한다.
- 서버는 소유 연결과 조종 스트림, 입력 범위와 순번을 검사한다. 중복을 버리고 누락된 순번은 재전송을 기다린다. 서버 시간이 허용하는 만큼만 같은 고정 스텝을 실행한다.
- 서버는 위치·회전·속도·접지 상태와 마지막 처리 순번을 함께 복제한다. 소유 클라이언트는 같은 순번의 예측 결과에서 허용 오차를 벗어나면 서버 상태를 복원하고 미확인 입력을 다시 실행한다.
- 관전자와 다른 조종자의 클라이언트는 받은 스냅샷 사이의 위치와 회전을 보간한다. 조종 변경, 텔레포트, 원격 입력 시간 초과는 이전 입력 스트림을 폐기한다. 네트워크 상태 복제와 UE 기본 Actor 이동 복제가 경쟁하지 않도록 Pawn에서 기본 이동 복제를 끈다.

UE 5.8.4에서 Win64 Development의 MWEditor와 MW 빌드 및 `MW.Vehicle` 자동화 4개를 통과했다.
`L_Dev_Drive`를 PIE Listen Server로 열고 두 클라이언트가 로컬 IP로 접속한 상태에서 원격 주행, 제동, 호스트 주행을 확인했다.
서버와 조종 클라이언트의 송신 지연을 각각 100ms, 패킷 손실을 10%로 설정하고 서버에만 벽을 생성했다.
후진 키를 누른 상태에서 서버 차량이 약 200cm 주행한 뒤 벽에 정지했으며 속도는 0cm/s였다.
보정 로그에 미확인 입력 재실행이 기록됐고, 키를 뗀 뒤 소유자와 관전자 차량의 위치는 각각 서버와 0.04cm 이내로 수렴했다.
조종 클라이언트의 송신을 1.35초 동안 100% 유실시켰을 때 서버 차량의 이동은 0cm였고, 송신 복구 후 새 전진 입력으로 약 284cm 이동했다.
측정 결과와 로그는 로컬 `Saved/Codex/VehicleNetwork`에 있다.

차량 간 충돌 튜닝 검증 당시, 3인 로컬 IP Listen Server에서 약 880cm/s의 원격 차량이 정지한 원격 차량을 추돌했을 때 밀린 차량의 첫 서버 속도가 약 200cm/s였다. 정지 뒤 소유자와 관전자 위치 오차는 0.05cm 이하였다. 결과는 로컬 `Saved/Codex/VehicleContact`에 있다.

실제 맵의 벽에 닿은 상태에서 바닥 Sweep이 옆벽과의 초기 겹침을 반환해 접지가 끊기는 경우를 확인했다. 관련 진단 기록은 로컬 `Saved/Codex/VehicleWallDiagnosis`에 있다. 바닥 재검사 회귀 테스트는 접지 유지와 후진, 바닥이 없는 곳에서 옆벽만으로 접지되지 않는 동작을 확인한다.
수정 후 Win64 Development의 MWEditor와 MW 빌드 및 `MW.Vehicle` 자동화 6개를 통과했다. `L_Dev_Drive`의 기존 북쪽 벽을 3인 Listen Server에서 65·75·85도 접근으로 재현했다. 후진, 탈출 방향 조향, 입력 해제의 9개 케이스 모두 서버와 소유 클라이언트의 접지를 유지했다. 75도 접근에서 후진 후 벽과의 간격이 28cm 늘었고, 탈출 방향 조향에서 서버 Yaw가 59.2도 바뀌었다. 측정 결과는 로컬 `Saved/Codex/VehicleGroundContact`에 있다.

아이템 충격 효과와 경기 단계 전체 통합은 후속 범위다.

### 2026-11-16 노면 자세 조사 범위 분리

곡면에서 목표 자세가 면 경계마다 급변하는 현상을 줄이기 위해 자세 조사 범위를 충돌 Box 치수에서 분리했다.
`GroundPlaneProbeHalfLength`와 `GroundPlaneProbeHalfWidth`는 CollisionRoot 전방/좌우 기준의 반거리(cm)이며,
기본값은 각각100cm와40cm다. 기존 `GroundPlaneProbeInset` 비율 설정은 제거했다.
기존 DataAsset에 새 필드가 없으면 구조체 기본값을 사용하며, 이번 변경에서 DataAsset/BP를 다시 저장하지 않는다.

실제 접지와 하중·속도 운반에는 기존 Box Sweep의 `GroundNormal`을 사용한다.
자세 정렬만 독립 범위의4점 높이에서 구한 `GroundOrientationNormal`을 사용한다.
면 방향·높이·평면 오차 검사와 실패 시 실제 접촉 법선으로 돌아가는 처리는 유지한다.
표시 메시나 바퀴 애니메이션의 현재 변환을 이동 계산으로 읽지 않는다.

이 값은 Client/Server 공용 DriveConfig의 초기 사본에 포함된다. 각 이동 스텝에서 공간적으로 다시 계산하므로
새 시간 보간 상태나 스냅샷 필드를 추가하지 않는다. `VisualRoot` 및 카메라의 보간 설정도 이번 변경에서 조정하지 않는다.
진단을 켜면 기존 `mw.Vehicle.SupportLog`와 함께 `VehicleOrientation`에 평면 계산 성공 여부, 조사 반거리,
이번 스텝의 최종 목표 법선을 기록한다. 로그를 위해 추가 Trace를 실행하지 않는다.

100/40cm는 현재 차량·트랙 조합에서 비교한 기본값이다. 모든 차량의 실제 바퀴 간격을 뜻하지 않으며,
차량/트랙별 적용 시 조사 범위와 유효점·평면 오차 거절 결과를 함께 검증해야 한다.

주행 입력 Controller 이전 후 Win64 Development의 MWEditor·MW 빌드와 차량 자동화 6개를 통과했다. 기존 테스트 Controller BP와 native Gameplay Controller 각각에서 W, W+D, W+S, S, 드리프트 누름·해제, 조종 해제와 재조종을 PIE로 확인했다. native Controller 검증은 주행 테스트 GameMode의 Controller 클래스만 프로세스 안에서 바꿔 실행했다. 3인 Listen Server에서도 같은 native Controller로 원격 주행과 호스트 주행이 전달됐다. 결과는 로컬 `Saved/Codex/ControllerDrivingInput`에 있다.

### 차량 간 충돌 예측과 보정

- 서버는 기존 박스 충돌로 두 차량의 수평 속도를 확정한다. 옆으로 치우친 접촉은 접촉 구간 중심으로 Yaw 회전 속도도 계산한다. 회전 속도는 조향과 별도로 감쇠하며, 벽이나 차량에 겹치는 회전은 적용하지 않는다.
- 소유 클라이언트는 최근 서버 상태에서 상대 박스를 같은 예측 시각으로 최대 250ms 추정한다. 상대 이동량과 새 서버 자료로 위치가 달라진 양을 함께 Sweep해 중간 접촉을 빠뜨리지 않는다. 충격 계산에는 물리 속도만 쓰며 위치 보정량을 충격 속도로 간주하지 않는다. 실제 로컬 반응은 자기 차량에만 적용한다.
- 보정은 ACK와 정확히 일치하는 입력의 실행 결과와 비교한다. ACK가 그대로여도 서버가 차량을 밀었으면 다시 보정할 수 있다. 서버 위치·회전·속도·접지·충돌 회전 속도와 ACK 시점의 상대 추정 체크포인트를 복원한 뒤 미확인 입력을 다시 계산한다. 입력을 처음 예측한 서버 추정 시각은 재실행에서도 유지한다. 스냅샷 공개 시각에 미확인 입력 개수를 더해 상대를 미래로 이동시키지 않는다. 입력 이력의 서버 원본은 보존하며 해당 시각을 포함하는 더 최신 서버 자료가 있으면 우선한다.
- 작은 보정은 충돌체를 즉시 맞추고 `DrivePresentation` 아래 차체와 카메라만 약 55ms 반감기로 따라오게 한다. 접촉 근처에서는 35ms 반감기를 쓴다. 평상시 짧은 표시 예측과 보정 오프셋을 분리한다. 150cm 또는 45도를 넘는 표시 차이나 새 조종 스트림은 보정 오프셋을 비운다. Blueprint에서 새 외형을 붙일 때는 충돌 없는 외형을 `DrivePresentation`에 연결한다.
- 먼 상대는 서버 시간 순서대로 약 75ms의 여유를 두고 재생한다. 새 상태가 없을 때 마지막 상태를 최대 50ms만 연장하고 재생 시각도 그 끝에서 대기한다. 접촉에 가까워지면 필요한 보정 시간을 고려해 미리 근접 표시를 준비한다. 일반 표시의 위치 오차 보정에는 별도 속도 제한을 적용하고 실제 접촉 프레임에는 충돌 후 상태를 바로 표시한다.
- 입력 이력은 120Hz 기준 최대 1.5초다. 서버는 짧은 순번 누락은 재전송을 기다리고, 누락이나 송신 중단이 길어지면 새 조종 스트림을 시작한다. `LogMWVehicleNet`의 Verbose 로그로 같은 ACK 보정, 재실행 수, 이력 포화와 스트림 초기화를 확인할 수 있다.

차량 간 충돌은 가까운 상대의 움직임을 짧게 추정한다. 여러 차량의 물리를 함께 되감거나 아이템 효과를 예측하는 기능은 포함하지 않는다.

`L_Dev_Drive`의 2인 로컬 IP Listen Server를 별도 프로세스로 실행해 60FPS, 양쪽 송신 지연 95ms(편차 25ms), 손실 7%에서 검증했다. 옆으로 치우친 추돌의 Yaw가 서버 결과 수신 전에 소유자에서 시작했고, 피해 차량의 ACK를 잠시 고정해도 보정이 적용됐다. 정지 후 두 차량의 위치는 서버에서 각각 0.05cm 안에 일치했다. 원격 입력 송신을 약 1.45초 끊으면 서버가 스트림을 재설정했으며 복구 후 입력을 다시 처리했다. 차량 BP 5종의 외형 컴포넌트가 클라이언트에서 `DrivePresentation` 아래에 있는 것도 확인했다. 렌더링을 끈 측정이므로 실제 화면에서 카메라와 차체의 체감은 별도로 확인한다.

### 2026-11-12 차체와 카메라의 고정 스텝 표시 보완

이 절은 선행 작업의 검증 기록이다. 한 스텝 늦게 보간하는 표시 방식은 아래 「접촉 시점 측정과 표시 지연 보완」에서 현재 시각을 짧게 추정하는 방식으로 교체했다. 여기서 측정한 Camera Lag 개선은 유지한다.

실제 2인 Listen Server의 충돌체, FRED 차체 메시, `DrivePresentation`, `FollowCamera`, `PlayerCameraManager` 위치를 매 프레임 측정했다. 100FPS 등속 주행에서 차체 이동량은 120Hz 고정 스텝 수에 따라 달라졌고 카메라는 SpringArm의 Camera Lag로 더 부드럽게 이동했다. 두 위치의 갱신 방식 차이가 자기 차량의 화면상 흔들림을 만들었다. 측정 구간의 카메라 컴포넌트 위치와 최종 카메라 캐시는 일치했다.

- 호스트 자기 차량과 소유 클라이언트는 이전·현재 고정 스텝 위치를 남은 시간 비율로 보간한다. 표시 지연은 한 스텝인 약 8.33ms다. 충돌체, 서버 판정과 입력 ACK는 이 표시 위치를 사용하지 않는다.
- `DrivePresentation`의 외형 연결과 카메라 Tick 선행 조건은 호스트에도 적용한다. 기존 Camera Lag 설정과 사용자 카메라 거리는 유지한다.
- 호스트가 보는 참가자 차량은 서버 스냅샷 시간축으로 외형만 보간한다. 참가자 충돌체는 서버가 계산한 실제 위치에 유지한다. 클라이언트의 기존 상대 차량 보간도 같은 함수로 계산한다.
- 서버 보정은 보간된 표시 위치를 기준으로 월드 좌표 오프셋을 구한다. 보정 재실행, 조종 스트림 변경과 텔레포트 시 이전 표시 이력을 구분한다.

렌더링을 켜고 프레임 시간이 변하는 조건에서도 측정했다. 차체 보간을 적용한 후에도 기존 SpringArm Camera Lag의 프레임 시간 의존성이 카메라 흔들림을 남겼다. `UMWVehicleSpringArmComponent`는 이전 목표와 현재 목표 사이의 선형 이동을 고려한 지수 감쇠로 위치 Lag를 계산한다. 회전 Lag는 가장 짧은 회전 차이를 사용하며, 조종 스트림 변경과 텔레포트 시 카메라 이력을 초기화한다. `CameraBoom` 이름, Camera Lag 활성화·속도, 팔 길이, 소켓 오프셋과 충돌 검사는 유지한다. 해석적인 감쇠를 사용하므로 Lag 계산에 기존 SpringArm의 시간 쪼개기 옵션은 사용하지 않는다.

다음 값은 등속 주행에서 실제 프레임 이동량과 `속도 × 프레임 시간`의 차이를 계산한 RMS이며 단위는 cm다. 서버와의 위치 오차를 뜻하지 않는다. 먼저 렌더링을 끄고 동일한 60·100·144FPS 조건에서 수정 전후를 비교했다.

| 실행 | FPS | 차체 수정 전 | 차체 수정 후 |
|---|---:|---:|---:|
| 호스트 | 100 | 6.0001 | 0.0003 |
| 참가자 | 100 | 5.9999 | 0.0003 |
| 호스트 | 144 | 5.5908 | 0.0007 |
| 참가자 | 144 | 5.5900 | 0.0007 |

100FPS에서 호스트 화면의 상대 차체와 자기 카메라 사이 프레임별 거리 변화 RMS는 5.6764cm에서 0.0004cm로 줄었다. 상대 차량의 네트워크 상태나 프레임 시간이 급변하는 경우까지 화면 변화가 완전히 없어지는 것을 보장하는 수치는 아니다.

실제 D3D SM6 렌더링을 켠 960×540 게임 창 두 개에서도 위치를 기록하고 화면을 확인했다. Camera Lag 수정 전 가변 FPS에서 남았던 카메라 이동 오차를 수정 후 0.001cm 미만으로 줄였다. 실제 FPS 약 47~116인 일반 렌더링 측정과 양쪽 송신 지연 95ms·손실 7%인 최종 렌더링 측정에서 호스트·참가자 자기 차체 기준점의 프레임별 투영 화면 좌표 변화는 기록 정밀도인 0.0001픽셀 이내였다. 등속 측정 구간에 한정한 결과이며 가속·조향 시 의도한 Camera Lag 이동을 없애지 않는다.

최종 렌더링에서는 서버에만 있는 벽으로 보정을 강제했다. 정지 후 서버와 참가자 위치 차이는 약 0.002cm였고 5,000cm 텔레포트와 이후 주행도 복구됐다. 별도 카메라 검사에서는 60FPS·144FPS·혼합 프레임 시간으로 같은 위치·회전 Lag 결과를 얻었고 Yaw 회전 경계, 일시 정지와 이력 초기화를 확인했다. 임시 계측·검사 코드와 생성 파일을 제거한 후 MWEditor·MW Win64 Development 빌드 및 기존 `MW.Vehicle` 6개 검사를 다시 통과했다.

### 2026-11-12 접촉 시점 측정과 표시 지연 보완

이 절의 수치는 11월 12일 구현 당시 기록이다. 이후 11월 13일 검사에서 근접 표시 전환의 급가속과 예측 시각 오류를 추가로 확인했다. 현재의 시간 기준과 상대 표시 보완은 아래 후속 절을 따른다.

#### 확인한 원인

1. 충돌 계산은 상대의 예상 현재 위치를 사용하지만 외형은 지연된 스냅샷을 재생했다. 따라서 밀리는 순간에도 화면에서는 두 차량 사이에 간격이 남았다.
2. 예측 접촉이 일어나면 그 고정 스텝의 남은 자기 이동을 생략했다. 상대 추정치는 계속 이동했으므로 붙어서 밀릴 때 자기 차가 멈추고 겹침·재충돌·보정이 반복됐다.
3. 재실행 때 상대의 충돌 후 위치·속도·회전 이력을 충분히 복원하지 못했다. 새 ACK 기준 시각과 과거 상대 상태의 기준 시각도 달랐다.
4. 손실 조건에서는 새 위치가 이전 스트림 번호·Revision과 함께 읽히는 사례가 있었다. 차량 스냅샷 전체에 `NetSerialize`를 적용해 스트림·ACK·위치·회전·속도·접지·충돌 회전 속도·서버 시각을 한 묶음으로 전송하도록 했다. 벡터 압축에는 `NetCore`를 사용한다.

#### 현재 처리 순서

```text
Movement Tick
  → 서버 계산 또는 소유자 고정 스텝 예측·재실행
  → 접촉까지 이동
  → 충격으로 속도·회전 속도 변경
  → 바뀐 속도로 그 스텝의 남은 시간도 이동

모든 차량 이동 후 PresentationTick (PostPhysics)
  → 자기 외형: 남은 0~8.33ms만 현재 시각까지 추정
  → 벽 Sweep·상대 박스 검사로 표시 예측의 범위 제한
  → 작은 서버 보정은 외형에서 감쇠
  → 근접 상대: 접촉 예측과 같은 시각·충돌 후 추정 상태 사용
  → 붙어 있는 두 외형은 자기 차량의 표시 보정 좌표계 공유
  → 제한된 외형 접촉 정렬
  → CameraBoom
  → PlayerCameraManager
```

`UMWVehiclePresentationComponent`는 이동 계산이 모두 끝난 뒤 외형을 갱신한다. 상대 외형은 로컬 차량 외형 이후에 갱신하고, 조종 차량이 바뀌면 이전 Tick 선행 관계를 해제한다. CameraBoom은 자기 PresentationTick 이후에 실행한다. 서버 충돌체를 외형 위치로 옮기지 않는다.

자기 외형은 이전·현재 상태의 지연 보간 대신 현재 충돌체 위치와 속도로 남은 한 스텝 미만의 시간만 추정한다. 입력 패킷을 더 기다리거나 입력 버퍼를 늘리지 않는다. 외형 정렬은 실제 접촉 프레임에 0.15cm의 여유를 사용하고, 표시 추정만으로 먼저 닿는 경우에는 1.5cm를 남긴다. 정렬 이동이 40cm를 넘으면 억지로 붙이지 않는다. 이 제한은 서버 판정에 영향을 주지 않는다.

상대 추정 체크포인트에는 위치·속도·회전·회전 속도·유효 시간이 포함된다. 같은 ACK 재보정에서도 해당 체크포인트를 복원하고 새 서버 기준 시각에 맞춘다. 접촉 중 자기 위치가 보정되면 붙어 있는 상대 추정치에도 그 위치 차이를 반영한다. 과거 접촉을 재실행할 때 화면 접촉 이벤트를 매번 다시 발생시키지는 않는다.

#### 실제 측정

같은 PC의 별도 프로세스 두 개를 Listen Server와 참가자로 실행했다. `L_Dev_Drive`에서 실제 FRED 차량 BP와 기존 입력 전달 API를 사용했으며, 로그인·방 생성은 로컬 연결로 우회해 차량 네트워크 경로를 독립적으로 검사했다. EOS 기능이나 에셋은 수정하지 않았다.

서버의 충격 적용, 참가자의 실시간 예측 접촉, 프레임 말 외형 위치에 같은 단조 시계 타임스탬프를 기록했다. 재실행 접촉은 별도 기록했다. 표시 접촉은 `DrivePresentation` 위치의 3D 박스 간격 1cm 이하로 판정했다. FRED 차체 메시의 실제 바운딩 박스도 함께 측정했다. 두 차체 경계에는 원래 약 2.48cm의 충돌 여유가 있으므로 메시 경계는 3cm 이하를 같은 접촉 기준으로 비교했다. 메시 삼각형끼리 닿는 픽셀이나 모니터 발광 시점을 측정한 것은 아니다.

다음은 정면 충돌의 **각 화면 내부 접촉 이벤트부터 외형 접촉 확인까지** 걸린 시간이다. 서버와 참가자의 서로 다른 예측 시점 차이를 뜻하지 않는다.

| 조건 | 화면 | 수정 전 | 최종 수정 후 |
|---|---|---:|---:|
| 추가 지연·손실 없음 | 호스트: 서버 접촉 → 외형 | 183.70ms | 0.84ms, 같은 프레임 |
| 추가 지연·손실 없음 | 참가자: 로컬 예측 → 외형 | 83.71ms | 0.42ms, 같은 프레임 |
| 양쪽 송신 지연 95ms·손실 7% | 호스트: 서버 접촉 → 외형 | 133.31ms | 0.98ms, 같은 프레임 |
| 양쪽 송신 지연 95ms·손실 7% | 참가자: 로컬 예측 → 외형 | 50.44ms | 0.52ms, 같은 프레임 |

최종 지연 테스트는 D3D SM6 렌더링을 켠 960×540 게임 창 두 개로 실행하고 두 창을 직접 확인했다. 정면 충돌의 서버 접촉을 0ms로 놓으면 참가자의 예측 접촉은 -86.78ms, 외형 접촉 확인은 -86.26ms, 호스트 외형 접촉 확인은 +0.98ms였다. 음수는 참가자가 서버보다 먼저 예측했다는 뜻이며 네트워크 지연이 사라졌다는 뜻이 아니다.

호스트 추돌·참가자 추돌·정면 충돌·비껴 맞는 추돌 네 유형 모두 최종 렌더링 측정에서 참가자의 예측과 외형 접촉을 같은 프레임에 확인했다. 전진 입력 이후 네 유형의 측정 구간에서 외형 박스의 최소 간격은 약 0.15cm였다. 각 구간 사이의 텔레포트에서는 새 스트림과 새 위치가 함께 적용됐고, 비껴 맞는 구간 시작 간격도 정상적인 680cm였다. 스냅샷 직렬화 왕복 검사도 통과했다.

나란히 1,800cm/s로 주행한 구간에서는 `차체 프레임 이동량 - 속도 × 실제 프레임 시간`의 RMS가 호스트·참가자 모두 약 0.0007cm였다. 자기 차체 기준점의 투영 화면 좌표 변화 범위는 0.00004픽셀 미만이었다. 실제 렌더링 프레임 시간은 호스트 약 32~60FPS, 참가자 약 59~60FPS 범위였다. 이는 등속 구간의 결과이며 충돌 순간의 의도한 속도·카메라 변화까지 없다는 의미는 아니다.

두 역할 모두 게임 입력 API 호출과 같은 엔진 프레임에 논리 이동·차체 표시 이동을 시작했다. 위의 1ms 안팎 수치는 엔진 안에서 기록한 이벤트 간격이다. 키보드 입력에서 실제 모니터 표시까지의 절대 지연으로 해석하지 않는다.

임시 계측·직렬화 검사 코드를 제거한 뒤 MWEditor·MW Win64 Development 빌드를 통과했고, 기존 `MW.Vehicle` 자동화 6개도 모두 통과했다. 측정용 원본 CSV·로그·임시 컴파일 산출물은 정리하고 결과 수치와 조건만 이 문서에 남긴다.

#### 남은 제한

- 서버가 최종 판정을 확정한다. 참가자는 받은 자료로 먼저 예상하므로 새 조향·충격이나 손실이 겹치면 예측과 서버 결과가 달라져 보정이 필요하다.
- 접촉 추정 250ms, 외형 정렬 40cm와 큰 보정 초기화 150cm/45도에는 상한이 있다. 심한 지연·장기 손실·급격한 방향 변경에서 모든 화면의 접촉 시점과 위치를 완전히 같게 만들지는 못한다.
- 여러 차량의 물리를 함께 되감는 방식이나 아이템 충격의 별도 예측은 포함하지 않는다. 외형과 충돌체가 항상 동일한 위치라는 보장도 추가하지 않는다.
- 기존 `ABP_UfologyPilot`의 `GetSteeringInput` 참조 컴파일 오류는 별도 에셋 문제로 남아 있다. 이번 차량 네트워크 변경에서 해당 에셋은 수정하지 않았다.

### 2026-11-13 상대 외형의 순간적인 가속 보완

참가자가 본 상대 차량의 순간적인 빠른 이동을 별도로 재현했다. 충돌 없는 통과에서도 발생했고 호스트에서도 확인했다. 서버의 실제 속도, 지연 보간 목표, 근접 예측 목표, 전환 가중치, 최종 표시 위치와 접촉 정렬량을 프레임별로 나누어 측정했다.

#### 원인

1. 근접 전환 가중치가 위치 차이와 관계없이 약 0.1초 만에 증가했다. 과거 보간 위치와 현재 예측 위치 사이의 수백 cm를 짧게 따라잡으면서 실제 주행 속도에 큰 표시 속도가 추가됐다.
2. 새 자료가 없어 외형은 최대 50ms 추정 뒤 멈췄지만 `ProxyPlaybackTime`은 계속 진행했다. 새 패킷을 받으면 이미 앞서간 시각의 위치를 한 프레임에 표시했다.
3. 참가자의 접촉 시각을 `서버 스냅샷 공개 시각 + 미확인 입력 개수 × 고정 스텝`으로 계산했다. 공개 시각과 입력의 실제 예측 시각은 다르며, 미확인 개수에는 왕복 전송 대기가 포함된다. 상대를 과도하게 미래에 놓고 ACK 때 다시 이동시키는 오차가 있었다.

#### 현재 처리

- 각 입력을 처음 예측할 때의 서버 추정 시각을 `FPredictedMove.ResultTime`에 저장하고 재실행에서 유지한다. 현재 접촉/표시는 현재 서버 추정 시각과 고정 스텝 잔여 시간을 기준으로 한다. ACK 수신 시 공개 시각 차이만큼 상대 체크포인트를 다시 밀지 않는다.
- `ProxyPlaybackTime`은 `최신 스냅샷 시각 + 50ms`에서 대기한다. 재수신 후에는 남은 경로를 연속 재생한다. 추정 한계의 부동소수점 경계에서도 표시 속도를 0으로 처리해 자료 없이 계속 진행하지 않는다.
- `SmoothProxyPresentation`은 직전 표시 위치에 물리 속도를 적분한 결과를 먼저 구하고, 목표와의 위치 차이만 별도로 줄인다. 일반 위치 보정의 반감기는 80ms, 추가 보정 속도 상한은 `max(120cm/s, 상대 속력 × 0.15)`다. 1,800cm/s 차량의 일반 표시 보정에는 최대 약 270cm/s만 추가된다.
- 근접 표시 진입은 표시 위치 차이를 좁히는 데 필요한 시간을 고려해 0.5~2.5초의 접근 범위에서 준비한다. 목표 전환 자체가 수백 cm의 즉시 표시 이동이 되지 않게 한다.
- 실제 접촉을 판정한 프레임은 근접 목표를 사용하고 일반 표시 완화를 거치지 않는다. 충돌 후 위치·속도·회전을 바로 표시한 뒤 기존 접촉 정렬을 수행한다. 자기 차량의 표시 보정 좌표계 공유도 유지한다. 일반 보정 속도 제한으로 실제 충격 반응을 늦추지 않는다.
- 접촉 정렬을 마친 최종 위치를 다음 프레임의 표시 기준으로 저장한다. 새 스트림·텔레포트·조종 변경에서는 표시 이력을 비운다. 자기 입력 대기열이나 RPC 전송 간격은 늘리지 않는다.

#### 측정과 검증 범위

동일 PC의 별도 프로세스 2개, 양쪽 송신 지연 95ms·손실 7%, 60FPS 목표에서 수정 전후를 비교했다. 3인 연결의 D3D SM6 800×450 게임 창에서도 호스트와 주행 참가자 시점을 확인했다. 기존 FRED BP와 로컬 IP 연결을 사용했다. 아래 값은 충돌 없는 구간에서 프레임별 표시 위치 차이로 계산한 **최대 표시 속력(cm/s)**이다. 해당 경로의 실제 최고 속력은 1,800cm/s다.

| 화면·상황 | 수정 전 | 최종 수정 후 |
|---|---:|---:|
| 참가자: 상대가 가까이 통과 | 6,751.19 | 2,070.11 |
| 호스트: 상대가 가까이 통과 | 3,238.20 | 1,831.01 |
| 참가자: 250ms 송신 단절 후 복구 | 14,391.33 | 2,070.11 |
| 호스트: 250ms 송신 단절 후 복구 | 4,420.71 | 2,069.14 |

최종 3인 렌더링의 호스트·주행 참가자에서도 통과와 복구 구간의 최대 표시 속력은 약 2,071cm/s 이하였다. 대기 중인 세 번째 참가자의 별도 계측 파일은 완료되지 않아 세 시점 전체의 계측 완료로 기록하지 않는다. 연결된 세 차량의 표시와 호스트·주행 참가자의 전체 시나리오는 확인했다.

최종 코드의 추가 지연·손실 0 조건과 95ms·7% 조건 모두, 정면 충돌·추돌의 첫 접촉 프레임에서 호스트와 주행 참가자의 외형 박스 간격이 약 0.15cm였다. 3인 렌더링의 두 주행 시점도 같은 첫 프레임 간격을 확인했다. 각 시나리오 사이의 새 스트림/레벨 내 텔레포트는 이전 표시 이력과 이어지지 않았다.

위 속도 상한은 **일반 표시 위치 보정**에 대한 값이다. 실제 충돌 반응과 붙은 두 차량이 공유하는 자기 차량의 서버 보정 좌표 이동은 별도 처리한다. 이 경우 월드 좌표 속력만으로 표시 오류를 판정하지 않고 실제 반응·두 차의 간격을 함께 확인한다. 장기 패킷 손실에서는 일시 정지나 더 오래 걸리는 따라잡기가 남을 수 있으며, 아직 수신하지 못한 상대의 조향·충격까지 알아내는 방식은 아니다.

임시 계측 코드를 제거한 뒤 MWEditor·MW Win64 Development 빌드와 기존 `MW.Vehicle` 자동화 6개를 모두 통과했다. 측정 CSV·로그·계측 컴파일 산출물은 정리하고 조건과 결과만 이 문서에 남긴다.

### 2026-11-20 경기 주행 제어 연결

`AMWVehiclePawn::CanDrive()`가 현재 소유 관계, 유효한 Gameplay GameState/PlayerState, `Racing`, 양쪽의 같은 0보다 큰 RoundEpoch, 개인 `Active`를 함께 검사한다. 클라이언트는 차량 스냅샷의 `bDriveAllowed`와 RoundEpoch도 일치해야 한다. Controller의 `GetInputVehicle`, Pawn의 `SubmitDriveInput`, 서버의 기존 `ServerSubmitDriveMoves`가 같은 조회를 사용한다. 기존 입력 데이터/바인딩 준비와 `InputEnabled` 검사도 Controller에 유지한다.

GameState/PlayerState의 기존 서버 알림과 RepNotify 공통 경로가 Pawn의 `RefreshDrivePermission()`을 호출한다. Pawn 생성·조종 변경에도 확인하며, Movement Tick 시작에서 재확인해 복제 도착 순서를 가정하지 않는다. 이 함수는 전달된 bool을 승인하지 않고 현재 상태에서 다시 계산한다. GameMode의 `EnterPhase` 외에 Phase를 쓰는 경로를 추가하지 않았다.

허용 여부가 바뀔 때만 저장 입력, 선속도, 충돌 Yaw 속도, 미확인 예측 입력/접촉 이력과 잔여 시뮬레이션 시간을 비운다. 서버는 기존 `ResetDriveStream`으로 입력 큐/ACK를 초기화하고 새로운 StreamId를 발급한다. 대기 중 매 Tick 새 스트림을 발급하지 않는다. `StopMovementImmediately`는 이제 충돌 Yaw 속도와 목표 회전도 초기화한다.

잠긴 차량은 주행/중력 계산과 예측 입력 생성을 멈추지만 Box 충돌, Movement Tick, 서버 스냅샷 처리, PresentationTick, 카메라 Tick을 유지한다. Active 차량이 잠긴 차량에 닿으면 기존 월드 충돌로 막히며 대상에 충돌 속도/회전을 부여하지 않는다. 클라이언트에서도 잠긴 상대를 동적 충격 예측에서 제외하고 일반 Box Sweep으로 처리한다. 외부 코드가 임의로 Actor를 이동시키는 것을 막는 기능은 아니다.

`FMWVehicleDriveSnapshot`에 서버 허용 결과 `bDriveAllowed`와 그 경기의 RoundEpoch를 추가했다. 기존 `NetSerialize` 묶음에 포함하므로 위치/StreamId와 별도 허용 RPC를 경쟁시키지 않는다. Pawn의 비복제 `bDriveAllowed`는 전환 감지용 캐시이며 별도 경기 상태 정본이 아니다. 같은 바이너리 버전의 서버/클라이언트가 필요하다.

자유 주행은 GameState의 기존 복제 `GameModeClass`가 `AMWDriveTestGameMode` 계열일 때만 명시적으로 허용한다. Gameplay 상태가 없다는 이유로 허용하지 않는다. 기존 순수 이동 테스트 월드에도 해당 시험 모드를 명시했다. 소유권 없는 시험용 차량의 기존 충돌/낙하 검사도 유지한다.

추가 자동화 `MW.Gameplay.DriveControl`은 허용 조건·정지/재허용·잠긴 차량 충돌, 서버 입력 처리와 이전 스트림 거절, 소유자 예측 정리, 스냅샷 직렬화를 검사한다. 엔진 테스트 연결과 직접 호출로 네트워크 처리 함수를 검사하며 실제 패킷 전달·복수 창의 시각 검증과는 구분한다. 실제 준비 보고·3초 카운트다운, 주행 제한시간·결과 집계는 이번 단계에 포함하지 않는다.

최종 UE 5.8.4 Win64 Development의 MWEditor·MW 빌드 성공. `MW.Gameplay.DriveControl` 3개, 기존 `MW.Vehicle` 6개, `MW.Gameplay.PlayerRound` 2개, `MW.Construction.PreviewMatchesSnap` 1개로 총 12/12 성공(테스트 오류·경고 0). 결과는 `Saved/Automation/DriveControlFinal/index.json`, 빌드/실행 로그는 `Saved/Logs/DriveControl*Final.log`에 있다. 이번에는 실제 다중 프로세스·3인 PIE·Enhanced Input 키보드·렌더링 화면·Cook/패키징 검증을 수행하지 않았다.

### 2026-11-20 출발 준비와 카운트다운 연결

현재 흐름은 트랙 확정 → `PreparingRace`의 차량 생성/정지/Possess → 소유 클라이언트 전원 준비 보고 → 기본 3초 `Countdown` → `Racing`이다. 건설을 생략하는 기존 고정 코스 경로도 같은 준비·카운트다운을 사용한다. TrackPlacement의 코스 ACK 대기와 PrepareRace 끝의 즉시 Racing 전환을 제거했다.

`Progress`에 목표 `CourseSessionId/CourseRevision`을 추가하고 Phase/Epoch/종료 시각과 함께 NetSerialize한다. 0/0은 서버가 건설 생략을 승인한 고정 코스이며, 동적 코스 데이터가 아직 없다는 뜻으로 사용하지 않는다. 기존 준비 RPC를 `ServerReportRaceReady(RoundEpoch, CourseSessionId, CourseRevision)`로 바꿨다. 호스트도 같은 경로로 보고한다.

Controller의 `IsLocalRaceReady`는 현재 소유 관계와 Waiting Epoch, 코스 Actor/확정 Transform/필수 충돌의 등록·물리 상태, 서버 시작 위치 적용, 입력 바인딩/Mapping Context/Enhanced PlayerInput, 차량 카메라와 표시 예정 스트리밍 레벨을 확인한다. 준비는 조작 허용과 별개이며 이 시점 `CanDrive`는 false다. 입력 준비가 늦으면 기존 연결 함수를 재사용하고 전체 키별 한 번 보고한다. `FMWPlacedTrack.bRequiresCollision`로 서버에서 실제 충돌을 가진 조각만 충돌 준비를 요구해 비충돌 종료 캡도 지원한다.

Pawn의 `PrepareForRaceFromServer`는 이미 정지 중인 차량도 입력·속도를 비우고 현재 Epoch의 시작 스냅샷과 새 StreamId를 공개한다. `IsReadyForRace`는 이동 데이터/충돌/표시·카메라 구성, 시작 스냅샷과 실제 위치 일치를 조회한다. Waiting 상태에서 새 Epoch 스냅샷을 받기 전에 준비를 보고하지 않는다.

GameMode는 서버 전용 준비 집합 하나, 타이머 하나와 기존 `Progress.PhaseEndServerTime`으로 진행한다. 준비 제한시간 기본 30초와 Countdown 기본 3초는 GameMode 기본 프로퍼티다. 보고의 소유권/명단/Epoch/코스/기한/서버 차량 상태를 검증하고 전원 완료면 EnterPhase(Countdown)을 호출한다. Countdown 동안 집합을 보존하며 기한 도달 시 다시 검증하고 EnterPhase(Racing)을 호출한다. 중복 보고는 시각을 다시 시작하지 않는다. 준비 시간초과·출발 전 이탈·Countdown 중 코스 변경은 Failed로 처리하며 과거 타이머는 Phase/Epoch/키/기한 불일치 시 무시한다.

`GameState.GetPhaseSecondsRemaining()`은 서버 동기 시각으로 남은 시간을 계산하는 BlueprintPure 조회다. 별도의 카운트다운 숫자 위젯은 추가하지 않았다. 기존 Phase/PlayerRound와 ② 경기 제어가 준비·카운트다운 동안 잠금을 유지하고 Racing에서만 새 주행 입력을 허용한다. 주행 제한시간·결과·점수·복귀는 후속 단계다.

최종 MWEditor·MW Win64 Development 빌드 성공. `MW.Gameplay.RaceStart` 4개와 기존 관련 12개로 총 16/16 성공(테스트 오류·경고 0). 새 검사는 실제 임시 월드의 Gameplay GameMode와 3명 시작 명단, 엔진 타이머·LocalPlayer·Enhanced Input 구성으로 전원 대기/3초 전환, 코스·충돌·입력·카메라·스트리밍 준비, 중복·이전 키, 시간초과·이탈·코스 변경·과거 타이머, Progress 직렬화를 확인한다. 결과는 `Saved/Automation/RaceStartCompleted/index.json`, 실행 로그는 `Saved/Logs/RaceStartAutomationCompleted.log`, 빌드 로그는 `Saved/Logs/RaceStartEditorBuildFinal.log`와 `RaceStartGameBuildFinal.log`다. 테스트 구성의 LocalPlayer Outer/월드 EndPlay/스트리밍 요청 플래그 오류를 수정한 최종 결과다. 실제 소켓 패킷·3인 PIE/독립 프로세스·키보드 입력·렌더링 화면·Cook/패키징 검증은 수행하지 않았다.

### main과 GameProgress 통합 — 2026-11-20

- main의 노면 정렬·접지 하중·다운포스·조향 속도 제한·벽 반발·트랙 효과·아이템·바퀴·스키드마크와 GameProgress의 Phase/Epoch·준비 보고·주행 잠금·접촉 예측·동일 ACK 보정·표시 완화를 함께 보존한다. 새 경기 규칙이나 라운드 루프는 추가하지 않는다.
- `GroundNormal`은 `ImpactYawRate`, 서버 시각, 주행 허용/Epoch와 같은 스냅샷 안에서 커스텀 직렬화한다. 복원·스냅샷 발행·보정 판단도 법선을 포함한다. 네트워크 처리의 기준은 GameProgress이며 아래 과거 통합 기록의 단순 Proxy 처리로 되돌리지 않는다.
- 기존 BP에 저장된 `CollisionRoot → VisualRoot → DrivePresentation → 차체/CameraBoom` 계층과 컴포넌트 이름을 유지한다. VisualRoot는 호환 부모로 두고, 이동 후 PostPhysics의 네트워크 표시를 먼저 계산한 뒤 Pawn Tick에서 같은 DrivePresentation에 높이·기울기 완충을 합성한다. 바퀴 표시와 카메라는 그 뒤에 갱신한다. 노면 표시의 기준은 DrivePresentation이며 충돌체를 다시 움직이지 않는다. 기존 BP의 표시 가지는 시작 시 같은 부모 아래로 정리하고 준비 검사도 이 계층을 확인한다. 저장된 상속 부모 관계를 뒤집으면 BP 재컴파일 중 순환 부착이 발생할 수 있어 계층을 역전시키지 않는다.
- 선택적 아이템 입력이 있으면 주행 바인딩 13개, 없으면 12개를 확인한다. 등록한 실제 핸들 검증과 해제는 기존 배열을 사용한다. Gameplay 테스트는 제거된 Phase 전용 setter 대신 Progress 적용 계약을 사용하고 기존 경기 제어 테스트를 유지한다.
- 아래 외형 이관과 이전 네트워크 통합 절은 당시의 작업·검증 이력이다. 과거에 적힌 컴포넌트 계층이나 미구현 상태를 현재 계약으로 해석하지 않는다. Content의 차량 BP·HUD·복구 에셋은 기존 UVCS 전달 범위를 유지한다.
- 최종 통합본은 MWEditor/MW Win64 Development 빌드와 자동화 23/23을 통과했다. 대상은 RaceStart 4개, DriveControl 3개, PlayerRound 2개, Vehicle 8개, Construction 스냅 1개, Items 1개, HUD 상태 수명 1개, Cursor 3개다. 선택적 아이템 바인딩이 있는 준비 검사와 GroundNormal을 포함한 스냅샷 직렬화도 포함한다. 테스트 결과는 `Saved/Automation/MergeGameProgressComplete/index.json`, 최종 빌드/실행 로그는 `Saved/Logs/MergeGameProgress*Complete.log`에 있다.
- 별도 에디터 프로세스에서 차량 BP 6개·파일럿 ABP·Race HUD·차량 선택 WBP 2개, 총 10개를 컴파일했고 오류·ensure가 없었다. 항상 숨기는 PreviewNotice는 선택적 바인딩으로 연결해 기존 선택 WBP의 기능을 유지한다. 에셋은 저장하지 않았고 대상 10개 파일의 전후 해시가 같다. 실제 다중 클라이언트 주행·화면·Cook/패키징은 이번 병합에서 수행하지 않았다.

2026-11-23 사용자 지시로 `Source/MW/Tests/MWRaceStartTests.cpp`와 GameMode/Controller의 전용 friend 선언을 삭제했다. 위 RaceStart 검사 수와 결과는 이전 실행 이력이며 현재 테스트 목록에는 포함되지 않는다. 라운드 반복은 공용 18번 문서의 0절을 따르며 개인 완주 단위부터 구현한다.

### 개인 완주·충돌 해제와 준비 시 복원 — 2026-11-23

- 공식 개인 상태는 기존 PlayerState.PlayerRound다. Finished=2와 FinishOrder를 기존 equality/NetSerialize에 추가했다. 서버 `TryFinishPlayerRace`가 Racing/Epoch/최초 명단/소유 관계/현재 코스의 마지막 종료 트랙을 검증한 뒤 승인한다. `NextFinishOrder`는 Racing 진입에 1로 초기화한다. RoundResults·전원 집계·다음 Epoch 전환은 이 작업에 포함하지 않는다.
- 현재 종료 캡에는 카탈로그의 Finish 형상이 없으므로, PreparingRace에서 연결점 기준 서버 Box를 기존 Runtime 컴포넌트에 추가한다. 기존 CreateShape/OnTrigger/ProcessRaceTrigger로 감지하며, 에셋을 수정하거나 새 결승 Actor/Subsystem을 만들지 않는다. GameMode.FinishTriggerHalfExtent 기본값 (50,1350,1250)cm가 영역의 반크기다. 현재 코스 마지막 종료 Actor 외의 Finish는 승인하지 않는다.
- Racing 진입에서 BeginPlayerRace로 서버 Runtime의 체크포인트/완주/시작 시각을 새 Epoch에 맞춘 뒤 개인 Active를 적용한다. 준비 때 이미 출발 볼륨 안에 있던 차량도 별도의 Start Overlap 없이 완주할 수 있다. 일반 DriveTest의 기존 체크포인트·복귀 기록 경로는 유지한다.
- PlayerState 공용 Notify → RefreshDrivePermission이 서버 차량을 정지시키고 루트 충돌의 원래 모드를 보관한 뒤 NoCollision으로 바꾼다. 입력·선형 속도·충돌 회전·예측 이력·기존 아이템을 초기화하며 차량/표시 Tick은 유지한다. 중간 이동 Overlap에서 잠겨도 이후 이동 스텝이 다시 가속하지 않는다.
- 기존 차량 이동 스냅샷에 한 비트 `bFinishCollisionDisabled`를 추가했다. 서버에서 적용한 충돌 상태와 위치/스트림을 함께 전달하며 공식 Finished/완주 순서의 원본을 대체하지 않는다. OnRep_ServerDriveSnapshot과 기존 이동 전 갱신을 사용하므로 소유자와 Controller/PlayerState가 아직 없는 표시 차량도 적용한다.
- 다음 Waiting 상태만으로는 충돌을 복원하지 않는다. 기존 GameMode.PrepareRace가 유지 중인 차량을 출발 슬롯으로 Teleport한 뒤 PrepareForRaceFromServer를 호출한다. 이 함수가 충돌 모드를 복원하고 새 위치/정지/충돌 상태가 담긴 시작 스냅샷을 공개한다. 클라이언트는 해당 스냅샷의 위치를 적용한 뒤 충돌을 켜므로 결승선의 이전 위치를 먼저 막지 않는다.
- 아이템 지급/사용에도 Gameplay의 개인 CanDrive를 적용한다. 기존 PlayerRound·DriveControl·Items 검사를 확장했으며 RaceStart 테스트는 복구하지 않았다. 실제 다중 프로세스 플레이와 전체 라운드 반복은 후속 확인 범위다.

검증 결과: UE 5.8.4 `MWEditor`/`MW` Win64 Development 정상 빌드 성공, 자동화 21/21 성공. 실제 서버 Box 통과 중 완주·즉시 정지, 두 참가자의 순서 1/2, 중복/오래된 Epoch/변경 코스/명단 외 거절, 다음 준비에서 QueryOnly 원래 모드 복원, 소유자/표시 차량의 스냅샷 수신 경로를 포함한다. 역할 전환과 RepNotify 호출 검사는 실제 패킷 전송 시험과 구분한다. 차량 Pawn 6개·파일럿 ABP·Race HUD·종료 캡·Gameplay GameMode·기존 주행 테스트 GameMode 2개, 총 BP 12개를 별도 프로세스에서 컴파일했고 Error/ensure가 없었다. 카탈로그를 포함한 검사 에셋 13개의 SHA-256이 전후 같아 에셋 저장은 없다. 빌드 로그는 `Saved/Logs/PersonalFinishEditorBuild.log`, `PersonalFinishGameBuild.log`, 자동화 결과는 `Saved/Automation/PersonalFinish/index.json`, BP/카탈로그 검증 데이터는 `Saved/Logs/PersonalFinishAssets.json`이다. 임시 검증 스크립트는 결과 데이터와 로그를 남긴 뒤 삭제한다.

### 점수·라운드 반복·서든데스 연결 — 2026-11-23

- 기존 GameMode.EnterPhase/준비 보고/타이머와 PlayerState.PlayerRound를 확장했다. 목표 4000점, 구간별 무탈락 체크포인트 100점, 완주 1/2/3등 300/150/50점이다. 일반 복귀는 Active를 유지하며 현재 구간만 무득점으로 처리한다. 최초 체크포인트 이전 실패는 출발 위치를 재사용한다.
- GameState의 기존 FMWGameplayProgress에 ResultsEpoch와 기존 개인 상태 배열 Results를 넣어 Phase와 함께 직렬화한다. 결과 대기 기본 3초 뒤 목표 도달자 0명은 새 Epoch·선택·기존 코스 확장, 1명은 최종 결과, 2명 이상은 해당 플레이어만 같은 코스의 서든데스다. 새 결과 구조체나 관리 클래스는 추가하지 않는다.
- 이전 종료 캡만 제거하고 직전 도로 끝을 체크포인트로 전환한다. 기존 도로/배치 Actor/PieceId와 최초 출발점을 유지한다. Pawn은 다음 준비에서 최초 슬롯으로 이동한 뒤 충돌을 복원하며, 기존 준비 보고·3초 Countdown을 반복한다.
- 서든데스의 영구 탈락과 비참가자는 기존 충돌 해제 스냅샷 경로를 재사용한다. Controller는 남은 참가자를 기존 차량 카메라로 관전한다. 서버는 0.25초마다 코스 최저 높이보다 1000cm 아래인 추락을 검사하고 KillZ도 같은 복귀/탈락 경로에 연결한다. 별도의 차량 파괴·선두 거리 차 검출기는 추가하지 않았다.
- 기존 HUD에 누적/목표 점수와 결과 텍스트를 표시하고 환경을 RoundResults/MatchResults까지 유지한다. 완주자 우선·탈락자 역순과 관전자 뒤 순위를 결과에 저장한다. 주행 중 이탈자를 명단에서 제외하며 서든데스 참가자가 전부 이탈하면 기존 Failed 경로로 종료해 관전자만 무한 대기하지 않게 한다. 최종 결과 확정 후 이탈/월드 정리는 결과를 바꾸지 않는다.
- 최종 검증: MWEditor/MW Win64 Development 빌드 성공, 자동화 23/23(경고 0), BP 18개 컴파일 성공·검사 에셋 19개 SHA-256 동일. 통합 검사는 실제 Definition 선택/소켓 배치로 정상 3라운드·코스 확장·점수·목표 종료·서든데스까지 수행한다. 역할 전환 검사를 실제 다중 프로세스 패킷 전달과 구분한다. 검증 경로는 `Saved/Automation/RoundLoop/index.json`, `Saved/Logs/RoundLoop*Build.log`, `Saved/Logs/RoundLoopAssets.json`, `Saved/Logs/RoundLoopVerification.json`이다. 실제 3PC 플레이·지연/손실·화면·Cook/패키징은 미실행이다.

### 준비 재보고·결과 이탈·주행 조회 보완 — 2026-11-23

- Controller는 같은 준비 키도 PreparingRace 동안 0.5초 간격으로 다시 보고한다. 서버는 중복 등록/로그 없이 전원 준비를 재평가하고 기존 Countdown으로 전환한다. 새 ACK·타이머·복제 상태는 추가하지 않는다.
- 종료 경로의 목표 도달 인원과 최종 순위는 현재 PlayerArray 대신 해당 Epoch의 확정 Results를 사용한다. 완료 후 떠난 사람의 결과는 유지하고 다음 서든데스에는 남은 자격자만 참가한다. 자격자가 모두 떠나면 기존 Failed로 종료한다.
- GameState에 C++ 읽기 전용 GetProgressRef를 추가하여 CanDrive·충돌 상태 갱신·이동 스냅샷의 Epoch 조회에서 Results 배열 복사를 제거했다. Blueprint GetProgress와 타이머/Phase 전환의 값 스냅샷은 유지한다. 프레임 성능 개선 수치는 측정하지 않았다.
- 이번 보완의 MWEditor/MW Win64 Development 빌드와 자동화 23/23(테스트 경고 0)을 통과했다. 기존 RoundLoop 검사에 준비 거절/재수락/중복 재평가와 결과 대기 중 이탈 경계를 추가했다. 빌드에는 변경 범위 밖의 MWItemComponentVisual.cpp GetBindings() C4996 경고가 남아 있다. 결과는 `Saved/Automation/RoundLoopFix/index.json`, `Saved/Logs/RoundLoopFix*Build.log`, `Saved/Logs/RoundLoopFixVerification.json`에 있다. 서버 진입점의 재수신 검사이며 실제 클라이언트 재보고 주기·다중 프로세스 패킷·프레임 성능·Cook/패키징 검증은 수행하지 않았다.

### main 경사로 접지 변경의 병합 보완 — 2026-11-23

- 대상 머지는 `50200ea`(GameProgress `c4ce90c` + main `59c2dc7`)다. 수정 전 정상 Editor 빌드에서 UpdateGroundRotation 인자 불일치(C2660), 누락된 PredictedContacts(C2065), bool 함수의 반환값 누락(C2561)을 확인했다. 한 스텝에 회전 함수가 두 번 호출되는 병합 흔적도 남아 있었다.
- 기존 UpdateGroundRotation에 main의 bool 반환을 적용하되 PredictedContacts 인자를 유지한다. SimulateDriveStep는 이를 한 번 호출하여 이동 여부를 받고, 기존 CanDrive 재검사 후 main의 조건부 UpdateGroundState를 수행한다. 공중 분기는 목표 회전만 갱신하고 실제 이동은 MoveVehicle에서 수행하므로 false를 반환한다.
- main의 접지 중 안쪽 Box 재검사, bWasGrounded 전달, 다른 트랙의 볼록 곡률을 미리 적용하지 않는 CrossTrackCrest와 바닥 검사 재사용 조건을 유지한다. 기존 회전 중 예측 접촉 검사·충돌 회전 감쇠·주행 잠금·라운드/점수/서든데스 계약도 유지한다. 새 RPC·필드·클래스·게임 규칙은 추가하지 않는다.
- 기존 StreamAndPrediction 자동화에 공중 90도/초의 120Hz 단일 스텝 회전량 0.75도, 스텝당 한 번 감쇠, 실제 상대 Actor 없이 저장한 예측 접촉만으로 회전 차단을 확인하는 회귀 검사를 추가했다. 기존 역할 전환 fixture를 사용하며 별도 테스트 GameMode나 Runtime 접근 API는 만들지 않는다.
- 검증: UE 5.8.4 MWEditor/MW Win64 Development 정상 빌드는 모두 성공했다. 자동화는 22/23 성공이며, 추가한 회전·감쇠·예측 접촉 검사와 기존 주행 잠금·완주·충돌·접지·프레임 시간 검사는 통과했다. RoundLoop는 현재 초기화된 카탈로그에 Asset.Track.Straight.Test가 없어 첫 선택 승인 단계에서 실패했다. 새 프로세스에서 RoundLoop만 실행해도 같은 실패가 재현되어 다른 테스트 실행 순서와 구분했다. 이 데이터 문제를 이번 병합 수정으로 해결한 것으로 처리하지 않으며 이전 머지 전 23/23 결과를 현재 결과로 재사용하지 않는다.
- main 부모의 FindGroundCandidate, EstimateRequiredNormalAcceleration, CalculateGroundNormalLoadAcceleration는 공백을 제외한 함수 본문 비교에서 모두 일치했다. GameMode/PlayerState/GameState/Controller/차량 네트워크 코드와 카탈로그 설정은 이번 수정에 포함하지 않았다. 빌드의 기존 MWItemComponentVisual.cpp GetBindings() C4996 경고는 유지된다. 실제 다중 프로세스 주행·지연/손실·렌더링·Cook/패키징은 이번에 실행하지 않았다.
- 결과 경로: Saved/Logs/MainMergeFixEditorBuild.log, MainMergeFixGameBuild.log, MainMergeFixVerification.json, Saved/Automation/MainMergeFix/index.json(전체), Saved/Automation/MainMergeFixRoundLoop/index.json(단독 재현). 수정 전 컴파일 실패 증거는 Saved/Logs/MainMergeReviewEditorBuild.log에 보존한다.

### 복구 차량 외형·바퀴 표시 이관 — 2026-11-05

`feature/vehicle-visuals`의 로컬 ProjectMW에서 Smiet, Mammoth, Ufology, Doug, Fred의 Pawn Blueprint 5개를 직접 메시 구조로 변경했다. Commit/Push/UVCS 제출은 수행하지 않았다.

- 계층은 `CollisionRoot → VehicleBody(StaticMesh) → 바퀴·장식(StaticMesh)`다. 네이티브 CollisionRoot, VehicleMovement, SpringArm/Camera는 유지하며 외형용 중간 SceneComponent와 별도 외형 Actor 스폰은 제거했다. 기존 입력·VehicleDefinition 경로도 유지했다.
- 차체 원본의 누적 변환을 새 `Art/DirectMeshes/SM_MW_*_Body` 메시 5개에 베이크했다. 차체 컴포넌트에 비균일 스케일을 두면 회전한 하위 메시가 틀어질 수 있으므로, 차체 컴포넌트에는 균일 스케일을 사용하고 바퀴·장식의 원본 누적 위치·회전·스케일을 직접 연결했다. 기존 주행 Box Collision 크기와 충돌 방식은 유지한다.
- 바퀴 회전·조향은 각 Pawn Blueprint의 EventGraph가 처리한다. 이동 거리와 각 바퀴 반지름으로 회전량을 구하고, Movement의 실제 Steering 입력으로 앞바퀴를 ±35도로 조향한다. 정지 상태에서도 A/D로 조향하며 입력 해제 시 중앙으로 복귀한다. 조향을 위해 차체의 정지 주행 동작을 바꾸지 않았다.
- 원본 복구 데이터의 유효 바퀴 수는 Smiet 2개, Mammoth 4개, Ufology 3개, Doug 4개, Fred 4개다. Smiet의 뒷바퀴 슬롯은 원본에서 비활성/스케일 0이고 Ufology의 앞바퀴 두 항목은 동일 메시·동일 위치라 중복을 제거했다. Mammoth의 큰 뒷바퀴와 Smiet의 좌우 크기 차이는 원본 누적 스케일에 근거한다. 이전의 일괄 4개 검증을 정정한다.
- Doug에만 `DougSwim` Timeline과 외부 `Animation/CF_DougSwim` Curve를 연결했다. 꼬리 ±30도, 지느러미 ±15도, 속도 0~70m/s에 따라 0.5~6Hz로 변화한다. 해당 Blueprint의 명시적 Tail/Fin 참조와 편집 가능한 파라미터를 사용하며, 다른 차량에서 Doug 부품을 검색하지 않는다. Fred 눈 및 Smiet 경적 연동 눈 애니메이션은 추가하지 않았다.
- `MWVehicleWheelVisualComponent.h/.cpp`, 자체 추가했던 `MWVehicleVisualValidationTests.cpp`, 임시 `MWDirectMeshAuthoringLibrary.h/.cpp`를 프로젝트에서 제거했다. 임시 Editor 모듈 의존성도 복원했다. 이전 외형 Actor BP 5종(BP_VehicleVisual_Smiet/Mammoth/Ufology/Doug/Fred)을 참조 검사·백업 후 삭제했다. Smiet이 참조되던 L_Title_WorldUI_Dev와 L_VehicleSelection_UI_Dev에서는 REBUILD_DecorativeSmiet 장식 4대를 사용자 지시로 제거했다. 기존 GameMode 클래스 경로는 유지했고 UI Widget·현재 주행 GameMode·C++는 변경하지 않았다. 누락된 구형 클래스 경로 보존을 위한 메모리 전용 직렬화 객체는 디스크에 저장하지 않았다. 새 Pawn은 이전 외형 Actor를 스폰하지 않는다.
- 남은 C++ 변경은 Pawn Blueprint Tick 활성화/PostPhysics 순서 지정과 읽기 전용 `GetSteeringInput()` 노출뿐이다. 이동 계산, 충돌, RPC, 복제 코드는 수정하지 않았다.
- 최종 삭제 후 원본 MW의 Editor/Game Win64 Development 빌드와 기존 `MW.Vehicle` 자동화 6/6을 통과했다. 새 프로세스에서 저장된 5개 Blueprint 컴파일, 메시 부모 계층, 별도 외형 Actor 부재, Doug 전용 Timeline을 확인했다.
- `Developers/user/L_Dev_Drive`의 NullRHI PIE에서 엔진 Enhanced Input 강제 키 W/A/D로 5종을 검사했다. 정지 중 앞바퀴 좌우 ±35도 및 복귀, 차체 정지, 바퀴 중심 유지, 전진 시 바퀴 회전, Doug 꼬리·지느러미 변화가 확인됐다. 이는 실제 하드웨어 키보드 입력 또는 멀티클라이언트 검증을 대체하지 않는다. 새 바퀴 조향의 원격 표시 및 패키징은 미검증이다.
- Content는 기존 `.gitignore`에서 제외된다. 수정 Pawn 5개와 새 차체 메시 5개·Curve 1개는 로컬 Content에 있으며, 코드 브랜치만 전달하면 함께 전달되지 않는다. Git/UVCS 제출은 별도다.
- 백업·해시·그래프·PIE 수치는 Codex 작업 폴더의 `outputs/VEHICLE_DIRECT_MESH_20261105`에 보관한다. 현재 원본 MW의 `Developers/user/L_Dev_Drive`는 BP_MWDriveTestGameMode의 DefaultPawnClass가 BP_MWVehiclePawn_DOUG로 설정돼 Doug를 생성한다. 해당 설정은 이전 차량 테스트 때 변경된 것으로 확인했으며 사용자 요청으로 UVCS 잠금을 해제했다. 후속 검사에서는 이 GameMode를 수정하거나 다시 잠그지 않았다. Smiet Pawn의 최신 이름은 BP_MWVehicle_Smiet이며 기존 BP_MWVehicleCharacter 리다이렉터는 보존한다.

### 휠 시각 처리 C++ 전환 — 2026-11-06

- 최신 휠 구현은 `AMWVehiclePawn::InitializeWheelVisuals/UpdateWheelVisuals`다. 차량별 BP의 `MW|Visual|Wheels`에서 메시 참조, 실제 반지름(cm), 부모 좌표 기준 회전축·조향축, 조향 여부와 최대 각도를 설정한다. 부품 참조와 기본 회전은 시작 시 한 번 캐시하고, 실제 이동 거리로 구름량을 계산하며 값이 변한 경우에만 회전을 적용한다. 정지 조향과 텔레포트 시 이동 이력 초기화를 포함한다.
- Smiet/Mammoth/Ufology/Fred의 휠 EventGraph는 비웠다. Doug에는 기존 꼬리·지느러미 Timeline/Curve와 재생 속도 BP 처리만 남겼다. 전체 그래프 노드는 232→27이며, 남은 27개에는 차량별 빈 ConstructionScript 진입 노드가 포함된다. 메시 계층·위치·스케일·재질은 이주 전후 일치 검사를 통과했다. `WheelVisualComponent`와 별도 외형 Actor를 다시 추가하지 않았다.
- 이번 작업 시점의 Smiet 정본 경로는 `/Game/Gameplay/Vehicles/BP_MWVehiclePawn_Smiet`이다. 현재 GameMode와 기존 리다이렉터는 수정하지 않았다. 5개 Pawn BP에서 이전 WheelDistance/VisualPreviousLocation/SteeringAngleDegrees 변수를 제거하고 현재 값은 새 설정에 이관했다.
- 임시 Editor 이주 도구 `MWVehicleWheelMigrationLibrary.h/.cpp`는 작업 후 삭제했고, MWEditor.Build.cs는 작업 전 바이트로 복원했다. 임시 도구가 없는 최종 Editor/Game Development 빌드, 실제 렌더 및 NullRHI PIE의 5종 W/A/D/S 강제 입력·정지 조향·중립 복귀·전후진 회전·텔레포트 검사와 기존 MW.Vehicle 자동화 6/6을 통과했다. 하드웨어 입력·패키징·멀티클라이언트 검증은 수행하지 않았다.
- 이동/충돌/RPC/복제 스냅샷은 변경하지 않았다. 현재 스냅샷에 조향값이 없어 원격 차량의 정지 조향 표시 문제는 별도 네트워크 검토 범위로 남는다. CPU trace는 확보했지만 이번 전환의 FPS 개선율을 측정했다고 주장하지 않는다. Git/UVCS 제출은 하지 않았다.
- 백업·변경 코드·BP 그래프 전후·실제 PIE 수치·렌더 캡처는 Codex 작업 폴더 `outputs/VEHICLE_NATIVE_WHEELS_20261106`에 보관한다.

### 최신 브랜치 이식 및 위치 복원 시각 회전 보완 — 2026-11-06

- `feature/vehicle-visuals-latest`는 `origin/main@035910b`에서 분기했다. main의 PlayerController 입력 소유권과 공유 커서 구현을 유지하고 차량 시각 처리만 이식했다. 이동·충돌 계산, RPC, Snapshot 타입과 송수신 구현은 변경하지 않았다.
- 직접 컴포넌트 위치 복원이 이전 프레임과의 차이로 계산되어 바퀴 회전이 튀는 문제를 보완했다. 로컬/Authority의 명시적 순간이동과 새 이동 스트림에서 시각 위치 이력을 무효화한다. SimulatedProxy의 일반 보간도 TeleportPhysics를 사용하므로, 보간마다 초기화하지 않고 스트림 변경을 구분한다. Root TransformUpdated는 약한 참조로 연결하고 EndPlay에서 해제한다.
- 실제 저장된 `Developers/user/BP_MWDriveTestGameMode`의 구형 Controller 참조를 native `MWGameplayPlayerController`로 교체했다. 기본 Pawn은 유지했다. 새 프로세스에서 메모리 Controller 교체 없이 저장된 주행 레벨과 GameMode를 사용했다. 다른 팀원 소유 주행 레벨은 저장하지 않았다.
- 수정 후 Editor/Game Win64 Development 빌드와 기존 MW.Vehicle 자동화 6/6을 통과했다. 5종 렌더 PIE에서 정지 조향, 전후진, 감속, 회전, 일반/직접 컴포넌트 위치 복원, 10도 경사 상승, 부모 Pitch/Yaw/Roll 변경, 바퀴 중심 유지와 캐시된 컴포넌트 제거를 확인했다. 직접 위치 복원 시 회전 튐은 5종 모두 재현 후 수정본에서 사라졌다. 이 PIE 입력은 엔진 Enhanced Input 테스트 명령을 사용했다.
- OS 키보드 입력 검증은 일부 진행 후 사용자 Escape로 중단되어 5종 전체 완료가 아니다. 이번 보완의 실제 멀티클라이언트, 패키징, 비균일 스케일 검증도 완료하지 않았다. 이전 기록의 성공을 이 미검증 항목의 성공으로 해석하지 않는다.
- Git 전달 범위는 차량 C++ 3개와 이 문서다. Content는 기존 UVCS 관리 정책을 유지하므로, 차량 BP 5종·메시·Curve·저장한 GameMode는 이 Git 브랜치 Push에 포함되지 않는다. 동일 에셋 버전은 별도로 맞춰야 한다. 작업 중 사용자가 저장한 Doug와 이후 편집 내용은 덮어쓰지 않았다.
- 전후 프레임별 Transform, 실행 로그와 테스트 결과는 Codex 작업 폴더의 `outputs/VEHICLE_REMAINING_FIX_20261106`에 보존한다. 사용자는 OS 키보드 전체 검증 미완료 상태를 유지한 작업 브랜치 Commit/Push를 별도로 승인했다.

### 휠 시각 조향 블렌딩 — 2026-11-06

- 본인 소유 `AMWVehiclePawn`의 휠 표시 코드에만 보간을 추가했다. 이동 입력·물리 조향·충돌·RPC·Snapshot·Doug Timeline은 변경하지 않았다. 기존 Tick에서 Pawn당 한 번만 지수 보간하고 그 결과를 조향 바퀴에 적용한다. 부품 검색이나 별도 Tick/컴포넌트는 추가하지 않았다.
- `MW|Visual|Wheels > WheelSteeringInterpSpeed` 기본값은 12(1/s)다. 목표각의 약 95%까지 약 0.25초이며 값이 작으면 느리게, 크면 빠르게 따라간다. 입력 해제 시 중립 복귀와 정지 좌우 조향에도 적용한다. 기존 바퀴 구름 거리·반지름·회전축·조향축과 바퀴 중심은 유지한다. 기존 차량 BP 5종을 저장하거나 덮어쓰지 않았다.
- Editor/Game Development 빌드와 기존 MW.Vehicle 자동화 6/6을 통과했다. 실제 렌더 PIE의 5종에서 정지 조향·반전·중립 복귀·바퀴 중심 유지·전후진 구름과 Doug 꼬리 동작을 확인했다. Smiet의 30/60/120FPS 상한 조건에서는 첫 유효 조향 프레임이 각각 약 11.54/6.34/3.33도였고 모두 약 35도로 수렴했다. 이 입력은 Enhanced Input 강제 키이며 OS 키보드 및 실제 멀티클라이언트 검증은 아니다.
- 코드에는 0으로 보간을 끄는 분기가 있으나, 0 설정 실행 검증은 테스트 fixture의 EditDefaultsOnly 값 적용 실패로 미완료다. 주요 7개 조건은 완료됐지만 초기 전체 테스트 runner는 이 추가 조건에서 실패했다. 이를 8개 전체 성공으로 기록하지 않는다. 프로젝트 코드 수정으로 테스트 제한을 우회하지 않았다.
- 백업·실제 조향 곡선·빌드/회귀 로그와 실패한 추가 fixture 기록은 Codex 작업 폴더 `outputs/WHEEL_STEERING_BLEND_20261106`에 보존한다. Commit/Push/UVCS checkin은 하지 않았다.


### 차량 간 충돌 예측과 보정

- 서버는 기존 박스 충돌로 두 차량의 수평 속도를 확정한다. 옆으로 치우친 접촉은 접촉 구간 중심으로 Yaw 회전 속도도 계산한다. 회전 속도는 조향과 별도로 감쇠하며, 벽이나 차량에 겹치는 회전은 적용하지 않는다.
- 소유 클라이언트는 최근 서버 상태에서 상대 박스의 위치를 최대 100ms 추정하고 상대 이동량을 포함해 접촉을 검사한다. 로컬 반응은 자기 차량에만 적용한다. 다음 스텝의 접촉 검사에는 상대도 밀렸다고 잠시 가정해 이전 스냅샷 때문에 같은 충격을 연달아 적용하는 일을 줄인다.
- 보정은 ACK와 정확히 일치하는 입력의 실행 결과와 비교한다. ACK가 그대로여도 서버가 차량을 밀었으면 다시 보정할 수 있다. 서버 위치·회전·속도·접지·충돌 회전 속도를 복원한 뒤 입력별로 저장한 상대 박스로 미확인 입력을 다시 계산한다.
- 작은 보정은 충돌체를 즉시 맞추고 `DrivePresentation` 아래 차체와 카메라만 약 55ms 반감기로 따라오게 한다. 평상시 고정 스텝 보간과 보정 오프셋을 분리한다. 150cm 또는 45도를 넘는 표시 차이나 새 조종 스트림은 보정 오프셋을 비운다. Blueprint에서 새 외형을 붙일 때는 충돌 없는 외형을 `DrivePresentation`에 연결한다.
- 상대 차량은 패킷 수신 시점마다 다시 보간을 시작하지 않고 서버 시간 순서대로 재생한다. 약 75ms의 여유를 두고, 새 상태가 없을 때 마지막 상태를 최대 50ms만 연장한 뒤 유지한다.
- 입력 이력은 120Hz 기준 최대 1.5초다. 서버는 짧은 순번 누락은 재전송을 기다리고, 누락이나 송신 중단이 길어지면 새 조종 스트림을 시작한다. `LogMWVehicleNet`의 Verbose 로그로 같은 ACK 보정, 재실행 수, 이력 포화와 스트림 초기화를 확인할 수 있다.

차량 간 충돌은 가까운 상대의 움직임을 짧게 추정한다. 여러 차량의 물리를 함께 되감거나 아이템 효과를 예측하는 기능은 포함하지 않는다.

`L_Dev_Drive`의 2인 로컬 IP Listen Server를 별도 프로세스로 실행해 60FPS, 양쪽 송신 지연 95ms(편차 25ms), 손실 7%에서 검증했다. 옆으로 치우친 추돌의 Yaw가 서버 결과 수신 전에 소유자에서 시작했고, 피해 차량의 ACK를 잠시 고정해도 보정이 적용됐다. 정지 후 두 차량의 위치는 서버에서 각각 0.05cm 안에 일치했다. 원격 입력 송신을 약 1.45초 끊으면 서버가 스트림을 재설정했으며 복구 후 입력을 다시 처리했다. 차량 BP 5종의 외형 컴포넌트가 클라이언트에서 `DrivePresentation` 아래에 있는 것도 확인했다. 렌더링을 끈 측정이므로 실제 화면에서 카메라와 차체의 체감은 별도로 확인한다.


### 2026-11-12 Kevin 기준 통합 범위

- 기준은 `fix/Network-Prediction@de73fd4`다. 차량 네트워크 소스와 Snapshot 타입은 해당 기준을 그대로 사용한다.
- 위의 볼록 충돌 적용 기록은 이전 작업 이력이다. 이번 실행은 기존 BoxComponent를 루트와 UpdatedComponent로 유지하며 DrivingCollisionMesh와 에셋은 삭제하지 않는다.
- 우리 아이템·트랙 효과·바퀴·HUD 연결을 Kevin 구조에 맞춘다. Pilot ABP의 GetSteeringInput 조회 API도 유지한다.
- 이전 `47fed23`의 발판 상대 Snapshot/구형 Proxy 보간 코드는 적용하지 않는다. 승강기 위 원격 차량 추종은 Kevin 담당자와 별도 검증·연동이 필요하며 완료로 취급하지 않는다.

### 2026-11-12 투명차 가상 바퀴 스키드마크와 ItemPickup 기본 Pawn

- `L_Dev_Drive_ItemPickup`에 배치된 `BP_MWVehiclePawn_DOUG_C_1`의 `Auto Possess Player`를 `Player 0`에서 `Disabled`로 변경했다. GameMode의 기본 Pawn은 변경하지 않았으며 현재 `BP_MWVehiclePawn_InvisiblePilot`이다. 배치된 Doug가 기본 Pawn 설정을 가로채던 연결만 해제했다.
- `FMWVehicleVirtualSkidWheel`과 Pawn의 `VirtualSkidWheels` 설정을 추가했다. **차량 BP → Class Defaults → MW → Visual → Skid Marks → Virtual Skid Wheels**에서 각 항목의 `Local Center`(Pawn 로컬 좌표, cm)와 `Radius Cm`을 편집한다. X는 앞/뒤, Y는 좌/우, Z는 위/아래이며 위치에는 Pawn Transform을, 반지름에는 기존 바퀴 표시와 동일한 Actor 스케일을 적용한다.
- 투명차는 보이는 바퀴 메시를 추가하지 않고 뒤쪽 가상 바퀴 두 개를 사용한다. 현재 BP 초기값은 `(-40, -23, -37)`, `(-40, 23, -37)`, 반지름은 각각 `15 cm`다. 기존 충돌 박스의 반범위 `(60, 25, 52)` 안쪽 뒤축과 바닥 높이에 맞춘 조정 시작값이며, 원본 게임에서 추출한 바퀴 좌표라고 주장하지 않는다. 실제 자국의 간격과 위치는 PIE에서 확인한 뒤 이 BP 값만 조정한다.
- `VirtualSkidWheels`가 비어 있으면 기존 메시 기반 자국 처리를 유지한다. 값이 있으면 지정한 가상 접지점만 사용하여 중복 자국을 방지한다. 설정은 기존 초기화 시점에 한 번 검증·캐시하며 차종 이름 분기, 새 Tick, 새 물리 바퀴나 SceneComponent는 추가하지 않는다. 바퀴 메시가 0개여도 가상 접지점이 있으면 기존 드리프트·바닥 검사로 자국을 그린다.
- 이동·접지·충돌·서버 보정·Snapshot·RPC 코드는 변경하지 않았다. 기존 로컬 조종 차량만 자국을 그리는 범위도 유지하므로 다른 플레이어 차량의 자국 동기화는 이번 수정에 포함되지 않는다.
- 이번 소스의 Editor/Game Win64 Development 빌드는 모두 성공했다. 사용자 요청으로 신규 PIE·주행 검증은 수행하지 않았다. BP 컴파일과 지정 두 에셋의 저장은 완료했으며, 실행 검증과 저장 에셋을 재로드한 회귀 검증은 미완료다. 검증할 항목은 기본 투명차 스폰, 직진 시 자국 없음, `W + Left Shift + A/D` 드리프트 시 두 줄 생성, 드리프트 해제 시 생성 중단, 기존 바퀴 차량 유지다.
- 변경 바이너리는 `Content/Gameplay/Vehicles/BP_MWVehiclePawn_InvisiblePilot.uasset`과 `Content/Developers/user/L_Dev_Drive_ItemPickup.umap` 두 개다. 둘 다 현재 Git 제외 대상이므로 소스 PR만 받아서는 BP 값과 레벨 설정까지 전달되지 않는다. 이번 작업에서 UVCS 명령·checkin·예약 변경은 하지 않았다. 팀 전달 시 이 두 에셋의 버전을 별도로 맞춘다.

### 2026-11-12 부스터 장착 표시 후속 수정

- 투명차에는 차체 StaticMesh가 없어 기존 메시 기반 장착 계산이 임시값 `(-65, 0, 10) / Scale 0.05`로 빠졌다. ItemComponent에 선택적인 BP 장착 변환을 추가했다. 차량 BP의 상속된 `Items` 컴포넌트에서 `MW → Items → Booster Visual`의 `Override Booster Mount`, `Booster Mount Transform`, `Booster Exhaust Length Scale`을 편집한다. 기본값은 Override 꺼짐이므로 기존 다섯 차량의 장착 계산은 유지한다.
- 투명차에만 Override를 켜고 Pawn 로컬 위치 `(-42, 0, 4) cm`, Yaw `-90°`, 균일 Scale `0.1`, 배기 길이 배율 `6.2`를 저장한다. 최종 외형 맞춤은 사용자 PIE 확인 후 이 BP 설정을 조정한다. 새 차종 이름 분기는 추가하지 않았다.
- 복구된 부스터 시퀀스의 Actor 바인딩에도 `MovieSceneSpawnTrack`이 있다. 기존의 `Binding.GetTracks().IsEmpty()`로 Actor와 부품을 구분하던 코드를 `FindSpawnable(Guid)`로 변경하여 Actor 바인딩에는 실제 장착 Actor를, 부품 바인딩에는 해당 부품을 연결한다. 원본 시퀀스와 키 데이터는 수정하지 않았다. 실제 원거리 표시 증상의 해소 여부는 PIE 미검증이다.
- 효과음 원본 `Booster Fire.wav`(48kHz, stereo, 3.558초)는 peak 약 0.812이고 full-scale 클리핑 샘플은 0개였다. 이는 게임의 최종 믹서 출력 클리핑이 없다는 증거가 아니다. 사용자 지시에 따라 오디오 중복 재생 방지·페이드·볼륨 변경은 추가하지 않았으며 소리 깨짐의 실행 원인은 미확정이다.
- 최종 Editor/Game Development 빌드 성공. 사용자에게 PIE 검증을 맡겼으며 이번 부스터 수정의 실제 화면·청취 결과는 통과로 기록하지 않는다. 아이템 획득/사용 판정, 지속시간, 서버 복제, 차량 이동·Snapshot 소스는 변경하지 않았다.

### 2026-11-12 차체와 카메라의 고정 스텝 표시 보완

실제 2인 Listen Server의 충돌체, FRED 차체 메시, `DrivePresentation`, `FollowCamera`, `PlayerCameraManager` 위치를 매 프레임 측정했다. 100FPS 등속 주행에서 차체 이동량은 120Hz 고정 스텝 수에 따라 달라졌고 카메라는 SpringArm의 Camera Lag로 더 부드럽게 이동했다. 두 위치의 갱신 방식 차이가 자기 차량의 화면상 흔들림을 만들었다. 측정 구간의 카메라 컴포넌트 위치와 최종 카메라 캐시는 일치했다.

- 호스트 자기 차량과 소유 클라이언트는 이전·현재 고정 스텝 위치를 남은 시간 비율로 보간한다. 표시 지연은 한 스텝인 약 8.33ms다. 충돌체, 서버 판정과 입력 ACK는 이 표시 위치를 사용하지 않는다.
- `DrivePresentation`의 외형 연결과 카메라 Tick 선행 조건은 호스트에도 적용한다. 기존 Camera Lag 설정과 사용자 카메라 거리는 유지한다.
- 호스트가 보는 참가자 차량은 서버 스냅샷 시간축으로 외형만 보간한다. 참가자 충돌체는 서버가 계산한 실제 위치에 유지한다. 클라이언트의 기존 상대 차량 보간도 같은 함수로 계산한다.
- 서버 보정은 보간된 표시 위치를 기준으로 월드 좌표 오프셋을 구한다. 보정 재실행, 조종 스트림 변경과 텔레포트 시 이전 표시 이력을 구분한다.

렌더링을 켜고 프레임 시간이 변하는 조건에서도 측정했다. 차체 보간을 적용한 후에도 기존 SpringArm Camera Lag의 프레임 시간 의존성이 카메라 흔들림을 남겼다. `UMWVehicleSpringArmComponent`는 이전 목표와 현재 목표 사이의 선형 이동을 고려한 지수 감쇠로 위치 Lag를 계산한다. 회전 Lag는 가장 짧은 회전 차이를 사용하며, 조종 스트림 변경과 텔레포트 시 카메라 이력을 초기화한다. `CameraBoom` 이름, Camera Lag 활성화·속도, 팔 길이, 소켓 오프셋과 충돌 검사는 유지한다. 해석적인 감쇠를 사용하므로 Lag 계산에 기존 SpringArm의 시간 쪼개기 옵션은 사용하지 않는다.

다음 값은 등속 주행에서 실제 프레임 이동량과 `속도 × 프레임 시간`의 차이를 계산한 RMS이며 단위는 cm다. 서버와의 위치 오차를 뜻하지 않는다. 먼저 렌더링을 끄고 동일한 60·100·144FPS 조건에서 수정 전후를 비교했다.

| 실행 | FPS | 차체 수정 전 | 차체 수정 후 |
|---|---:|---:|---:|
| 호스트 | 100 | 6.0001 | 0.0003 |
| 참가자 | 100 | 5.9999 | 0.0003 |
| 호스트 | 144 | 5.5908 | 0.0007 |
| 참가자 | 144 | 5.5900 | 0.0007 |

100FPS에서 호스트 화면의 상대 차체와 자기 카메라 사이 프레임별 거리 변화 RMS는 5.6764cm에서 0.0004cm로 줄었다. 상대 차량의 네트워크 상태나 프레임 시간이 급변하는 경우까지 화면 변화가 완전히 없어지는 것을 보장하는 수치는 아니다.

실제 D3D SM6 렌더링을 켠 960×540 게임 창 두 개에서도 위치를 기록하고 화면을 확인했다. Camera Lag 수정 전 가변 FPS에서 남았던 카메라 이동 오차를 수정 후 0.001cm 미만으로 줄였다. 실제 FPS 약 47~116인 일반 렌더링 측정과 양쪽 송신 지연 95ms·손실 7%인 최종 렌더링 측정에서 호스트·참가자 자기 차체 기준점의 프레임별 투영 화면 좌표 변화는 기록 정밀도인 0.0001픽셀 이내였다. 등속 측정 구간에 한정한 결과이며 가속·조향 시 의도한 Camera Lag 이동을 없애지 않는다.

최종 렌더링에서는 서버에만 있는 벽으로 보정을 강제했다. 정지 후 서버와 참가자 위치 차이는 약 0.002cm였고 5,000cm 텔레포트와 이후 주행도 복구됐다. 별도 카메라 검사에서는 60FPS·144FPS·혼합 프레임 시간으로 같은 위치·회전 Lag 결과를 얻었고 Yaw 회전 경계, 일시 정지와 이력 초기화를 확인했다. 임시 계측·검사 코드와 생성 파일을 제거한 후 MWEditor·MW Win64 Development 빌드 및 기존 `MW.Vehicle` 6개 검사를 다시 통과했다.


### 2026-11-12 예측보정 수정2와 Item UI 통합

- 실제 merge 대상은 `fix/Network-Prediction@bab3245`와 `fix/item-ui-polish@b201e1a`다. 서버 보정·Snapshot 및 새 SpringArm 구현은 Kevin 원본을 유지하고 기존 UI·아이템·바퀴·가상 접지점 표시를 보존한다.
- 충돌한 Pawn include는 두 시각 컴포넌트를 모두 포함하고 BeginPlay는 외형 재연결 → `ResetDrivePresentation()` → `InitializeWheelVisuals()` 순서로 연결한다. `GetSteeringInput()` 조회 API를 유지해 파일럿 Animation BP와의 계약이 빠지지 않게 한다.
- 위 2인 지연·손실·프레임 측정 수치는 Kevin 브랜치의 이전 검증 기록이다. 이번 통합에서 새로 수행한 PIE 결과가 아니다. 사용자가 PIE 검증을 담당하므로 통합본의 실제 입력·다중 클라이언트·음성 결과는 별도로 확인해야 한다.
- 이번 통합본의 신규 검증은 MWEditor/MW Win64 Development 빌드 성공과 차량 6종·파일럿 ABP·UI 6종, 총 13개 BP 컴파일 성공이다. 에셋은 저장하지 않았으며 로컬 차량 에셋·테스트 맵·GameMode·개인 설정 170개 해시가 통합 전과 같다. 차량 6종의 CameraBoom은 새 `MWVehicleSpringArmComponent`이고 BP 기본 팔 길이는 모두 750cm로 확인했다. PIE·원격 승강기 추종·부스터 소리의 실행 검증은 아직 완료되지 않았다.

### 2026-11-17 벽 반발 검증

`WallRestitution`은 [0,1], `MinWallBounceSpeed`는 0 이상의 유한값으로 검증한다.
두 값은 기존 DriveData 사본에 포함되며 서버·소유자·입력 재실행이 같은 이동 계산을 사용한다.
반발은 기존 `Velocity`에 반영하므로 별도 타이머·입력 잠금·RPC·스냅샷 필드를 추가하지 않는다.

UE 5.8.4에서 MWEditor·MW Win64 Development 빌드와 diff 검사를 통과했다.
`MW.Vehicle` 자동화는 수정 전 5/6, 수정 후 6/7 통과했다. 신규 `WallBounce`의 13개 장면은 통과했다.
기존 `CollisionAndGround`에서 접지 중 `Velocity.Z`를 직접 대입하는 상승 검사 2개는 수정 전후 동일하게 실패하며,
이번 변경에서 해결하거나 전체 통과로 처리하지 않았다. 벽 근처 회전 거절 검사는 반발로 떨어지기 전 첫 스텝을 확인한다.
30/60/120FPS 벽 반발 위치·속도는 일치했다. 240FPS에서는 엔진 Sweep의 거리별 접촉 앞당김 때문에
위치가 약 0.59cm 달랐고 반발 속도는 같았다. 회전된 노면 검사는 중력을 제외한 방향 검증이며 360도 트랙 완주 검증은 아니다.

3인 단일 프로세스 PIE에서 로컬 IP Listen Server로 연결해 기존 `L_Dev_Drive`의 Cube3 벽을 사용했다.
현재 해당 GameMode의 기본 Pawn은 InvisiblePilot이며, 이전 차량 경로는 실제로 `BP_MWVehiclePawn_Smiet`로 해석된다.
테스트 프로세스에서만 기본 Pawn을 대상 차량으로 지정하고, `VehicleGround`를 Ignore하던 넓은 바닥을 Block으로 지정했다.
맵·GameMode·차량·DriveData 에셋은 저장하지 않았으며 대상 파일의 전후 해시가 같음을 확인했다.
따라서 원본 개발 맵의 접지 채널 설정은 별도 확인이 필요한 상태다.

정면·45도·75도 충돌을 정상 통신 및 송신 방향별 100ms 지연·5% 손실에서 확인한 6개 조건과,
서버에만 임시 벽을 둔 강제 보정 1개 조건을 통과했다. 기록된 서버·소유자 샘플에서 높이·수직 속도·기울기 변화와
접지 손실이 없었고 제동 후 정지와 후진이 작동했다. 서버 전용 벽에서는 미확인 입력 28개 재실행을 확인했다.
제동·안정화 후 소유자·관전자의 최대 위치 오차는 0.05cm 미만이었다. 지연 중 순간 오차나 실제 3PC 결과를 뜻하지 않는다.
로그·수치·그림·재현 스크립트는 로컬 `Saved/Codex/VehicleWallBounce`에 있다.
