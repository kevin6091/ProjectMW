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
- 작은 보정은 충돌체를 즉시 맞추고 `DrivePresentation` 아래 차체와 카메라만 약 55ms 반감기로 따라오게 한다. 150cm 또는 45도를 넘는 표시 차이나 새 조종 스트림은 즉시 맞춘다. Blueprint에서 새 외형을 붙일 때는 충돌 없는 외형을 `DrivePresentation`에 연결한다.
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
