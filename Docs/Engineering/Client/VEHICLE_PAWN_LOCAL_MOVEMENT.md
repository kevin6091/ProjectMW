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

1. 접지 검사
2. 현재 속도와 입력으로 목표 Yaw 계산 및 회전 후보 겹침 검사
3. 접지 시 추진/제동/자연 감속/횡그립, 공중이면 중력 계산
4. SafeMoveUpdatedComponent 이동, 차단면으로 향하는 속도 제거와 표면 미끄러짐
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
- 벽에 닿으면 충돌면 안쪽 속도 성분을 제거하고 남은 이동량으로 표면을 따라 이동한다.
- 차량끼리 부딪히면 서버가 질량과 상대 속도로 두 차량의 수평 충돌 반응을 계산한다. `CollisionMomentumRetention`은 접촉 법선 방향의 공통 속도를 얼마나 유지할지 정한다. 가까운 속도로 달리는 차량이 가볍게 닿을 때는 공통 감속량을 상대 접근 속도에 비례해 제한한다.
- 월드 -Z 중력과 고정된 지형을 우선 대상으로 한다. 차체 Pitch/Roll 정렬, 계단 오르기, 움직이는 바닥 추종은 미구현이다.

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
EndPlay에서도 입력과 Driving Context를 정리한다. 카운트다운/Phase 허용 조건 통합은 후속 작업이다.

## 검증

설치 엔진은 UE 5.8.4 / CL 58902052다. 기존 문서의 5.8.3과 달리 실제 설치 버전으로 기록하며 엔진을 변경한 것은 아니다.

`MW.Vehicle` 엔진 자동화는 실제 Physics Scene을 가진 임시 게임 월드에서 native Pawn과 충돌체를 생성한다.
Pawn 제어 입력 API와 이동 Tick을 실행하므로 키보드/Enhanced Input 바인딩이나 실제 게임 맵의 시각 품질 검증과 구분한다.

| 자동화 | 확인 대상 |
|---|---|
| ConfigValidation | 기본 설정과 NaN/잘못된 범위/관계 거절 |
| LocalControls | 정지 조향, 최고속도, 자연 감속, 동시 제동, 후진/방향 전환, 관성 조향, 드리프트, 입력 잠금, 비정상 입력 |
| CollisionAndGround | 중력/착지/바닥 간격, 상승 시 접지 해제, 낙하 속도 제한, 벽 미끄러짐, 회전 겹침 거절, 가장자리 추락, 경사/급경사 |
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

주행 입력 Controller 이전 후 Win64 Development의 MWEditor·MW 빌드와 차량 자동화 6개를 통과했다. 기존 테스트 Controller BP와 native Gameplay Controller 각각에서 W, W+D, W+S, S, 드리프트 누름·해제, 조종 해제와 재조종을 PIE로 확인했다. native Controller 검증은 주행 테스트 GameMode의 Controller 클래스만 프로세스 안에서 바꿔 실행했다. 3인 Listen Server에서도 같은 native Controller로 원격 주행과 호스트 주행이 전달됐다. 결과는 로컬 `Saved/Codex/ControllerDrivingInput`에 있다.

### 차량 간 충돌 예측과 보정

- 서버는 기존 박스 충돌로 두 차량의 수평 속도를 확정한다. 옆으로 치우친 접촉은 접촉 구간 중심으로 Yaw 회전 속도도 계산한다. 회전 속도는 조향과 별도로 감쇠하며, 벽이나 차량에 겹치는 회전은 적용하지 않는다.
- 소유 클라이언트는 최근 서버 상태에서 상대 박스의 위치를 최대 100ms 추정하고 상대 이동량을 포함해 접촉을 검사한다. 로컬 반응은 자기 차량에만 적용한다. 다음 스텝의 접촉 검사에는 상대도 밀렸다고 잠시 가정해 이전 스냅샷 때문에 같은 충격을 연달아 적용하는 일을 줄인다.
- 보정은 ACK와 정확히 일치하는 입력의 실행 결과와 비교한다. ACK가 그대로여도 서버가 차량을 밀었으면 다시 보정할 수 있다. 서버 위치·회전·속도·접지·충돌 회전 속도를 복원한 뒤 입력별로 저장한 상대 박스로 미확인 입력을 다시 계산한다.
- 작은 보정은 충돌체를 즉시 맞추고 `DrivePresentation` 아래 차체와 카메라만 약 55ms 반감기로 따라오게 한다. 평상시 고정 스텝 보간과 보정 오프셋을 분리한다. 150cm 또는 45도를 넘는 표시 차이나 새 조종 스트림은 보정 오프셋을 비운다. Blueprint에서 새 외형을 붙일 때는 충돌 없는 외형을 `DrivePresentation`에 연결한다.
- 상대 차량은 패킷 수신 시점마다 다시 보간을 시작하지 않고 서버 시간 순서대로 재생한다. 약 75ms의 여유를 두고, 새 상태가 없을 때 마지막 상태를 최대 50ms만 연장한 뒤 유지한다.
- 입력 이력은 120Hz 기준 최대 1.5초다. 서버는 짧은 순번 누락은 재전송을 기다리고, 누락이나 송신 중단이 길어지면 새 조종 스트림을 시작한다. `LogMWVehicleNet`의 Verbose 로그로 같은 ACK 보정, 재실행 수, 이력 포화와 스트림 초기화를 확인할 수 있다.

차량 간 충돌은 가까운 상대의 움직임을 짧게 추정한다. 여러 차량의 물리를 함께 되감거나 아이템 효과를 예측하는 기능은 포함하지 않는다.

`L_Dev_Drive`의 2인 로컬 IP Listen Server를 별도 프로세스로 실행해 60FPS, 양쪽 송신 지연 95ms(편차 25ms), 손실 7%에서 검증했다. 옆으로 치우친 추돌의 Yaw가 서버 결과 수신 전에 소유자에서 시작했고, 피해 차량의 ACK를 잠시 고정해도 보정이 적용됐다. 정지 후 두 차량의 위치는 서버에서 각각 0.05cm 안에 일치했다. 원격 입력 송신을 약 1.45초 끊으면 서버가 스트림을 재설정했으며 복구 후 입력을 다시 처리했다. 차량 BP 5종의 외형 컴포넌트가 클라이언트에서 `DrivePresentation` 아래에 있는 것도 확인했다. 렌더링을 끈 측정이므로 실제 화면에서 카메라와 차체의 체감은 별도로 확인한다.

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
