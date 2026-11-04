# Pawn 기반 차량 로컬 이동

2026-11-04 / Client + Server. 사용자의 Pawn 전환 지시와 단계 1~6 구현 요청을 적용한다.
기존 07 차량 설계의 Character/CMC 설명은 과거 방향이며, 이번 로컬 구현은 이 문서를 따른다.
현재 S/Reverse 입력은 제동 후 후진을 지원한다. 기존 구현을 유지한 것이며 S를 제동 전용으로 바꾸지 않았다.

## 연결과 초기화

- `AMWVehiclePawn : APawn`이 `CollisionRoot`와 `VehicleMovement`를 생성한다.
- `UMWVehicleMovementComponent : UPawnMovementComponent`의 UpdatedComponent는 CollisionRoot다.
- 루트는 물리 시뮬레이션을 끈 BoxComponent이며 Query Collision을 사용한다. Blueprint에서 Box Extent/충돌 설정을 조정한다.
- Blueprint의 VehicleMovement에 DriveData, Pawn에 DrivingInputData를 지정해야 한다. 에셋 경로를 C++에 하드코딩하지 않는다.
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
0.25초를 넘는 정지 구간의 초과 시간은 재현하지 않는다. 네트워크 시간 처리 계약은 후속 구현 대상이다.
Standalone에서는 차량을 시뮬레이션하고 네트워크 월드에서는 로컬 제어 차량만 실행한다.
원격 입력 전송/서버 재현이 없는 상태를 서버 권한 네트워크 이동 완료로 해석하지 않는다.

## 접지와 충돌

- 중심선 대신 실제 루트 CollisionShape의 아래 방향 Sweep을 사용한다.
- Object Type과 Collision Response를 함께 사용해 실제 이동 충돌과 검사 대상을 맞춘다. Visibility 채널에 의존하지 않는다.
- MaxGroundAngle보다 가파른 면은 주행 바닥으로 인정하지 않는다.
- 바닥 법선 방향 이탈 속도가 GroundDetachSpeed를 넘으면 접지를 해제해 위쪽 충격을 지우지 않는다.
- GroundProbeDistance 범위 안에서 GroundClearance를 유지한다. 착지 시 간격이 작아지면 위로, 크게 벌어지면 아래로 안전 이동한다.
- 간격 보정 이동량을 Velocity에 더하지 않는다.
- 박스 회전은 중간/최종 자세 겹침 검사로 막힌 후보를 거절한다. 연속 회전 궤적 전체의 정확한 충돌이나 모든 좁은 틈을 보장하는 방식은 아니다.
- 충돌은 기본적으로 반발 없이 표면을 따라 이동한다. 차체 간 상호 충격/질량/반발 계산은 미구현이다.
- 월드 -Z 중력과 고정된 지형을 우선 대상으로 한다. 차체 Pitch/Roll 정렬, 계단 오르기, 움직이는 바닥 추종은 미구현이다.

## 설정 검증과 수명

속도/가속도/그립/제동/조향/중력/접지 설정의 유한값과 범위를 검사한다.
GroundProbeDistance > GroundClearance, FullBrakeDeceleration >= CoastDeceleration,
FullSteeringSpeed > MinSteeringSpeed 관계를 확인한다.
MinDriftSteering 및 DriftFrictionMultiplier는 [0,1], MaxGroundAngle은 [0,89], MaxFallSpeed는 1 이상이다.

UnPossessed에서는 저장 입력과 자신이 설치한 Driving Context를 정리한다.
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

네트워크 입력 전송/예측/보정/원격 보간, 실제 차량 충격, 효과/복귀/경기 단계 전체 통합은 이번 단계 1~6의 완료 범위 밖이다.
