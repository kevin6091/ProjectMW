# 차량 좌우 배기 Niagara 구현과 검증

- Git 브랜치 `feature/vehicle-exhaust-vfx`, 시작점 `e05bd00`
- UVCS `cs:67@br:/main` — 차량 뒤 좌우 배기 나이아가라 에셋 추가
- 10월 30일 영상 02:05–02:10에 나타나는 뒤쪽의 두 줄을 참고했다. ProjectMW에 있던 연기 재질과 감쇠 곡선을 사용해 별도 차량용 시스템을 만들었다.

| 수정 위치 | 변경 |
| --- | --- |
| `Source/MW/Gameplay/Vehicle/Visual/MWVehicleExhaust.cpp` | 차체 표시 부모 아래 좌·우에 로컬 Niagara 컴포넌트를 만든다. 자기 차량의 페달 입력과 각 창의 전후진 속도로 활성 상태를 결정하고 정차하면 새 입자 방출을 멈춘다. 아이템 부스터가 활성일 때 일반 배기를 중단한다. |
| `Source/MW/Gameplay/Vehicle/MWVehiclePawn.h` 및 `.cpp` | 기존 표시 갱신과 Pawn 종료 경로에 연결한다. |
| `Source/MW/Gameplay/Vehicle/Visual/MWVehicleExhaustSettings.h`, `Config/DefaultGame.ini` | 시스템 경로, 차체 기준 좌우 위치, 크기, 움직임 감지 속도, 에셋의 쿠킹 경로를 등록한다. |
| `Source/MW/MW.Build.cs`, `MW.uproject` | 런타임 Niagara 모듈과 프로젝트 플러그인 의존성을 명시한다. |
| `Content/Gameplay/Vehicles/Art/Exhaust/NS_MW_VehicleExhaust.uasset` | 에셋에서 입자 수명 0.12–0.24초, 초기 스프라이트 크기 8–16 cm, 초당 55개 방출을 지정한다. 모양 오프셋을 끄고 월드 공간을 사용해 짧은 자취가 차체 뒤에 남게 했다. |

UVCS 서버에서 다시 내려받은 Niagara 에셋의 SHA-256은 `b18e33be1b78bc0c07afbd3bb5416c64c80d16b13d59c75d96e804b059d44a90`이며 로컬과 일치했다.

## 검증

- UE 5.8 Editor Win64 Development 빌드 통과. Cua Driver로 3인 리슨 서버 PIE를 실행했다. 배기 효과에 집중하기 위해 직행 경기 설정을 에디터 메모리에만 적용했다. 프로젝트 게임 모드 BP의 디스크 파일은 UVCS 서버 해시와 일치했다.
- 첫 실화면에서 공용 연기 크기가 FRED를 가렸다. 수명과 스프라이트 크기를 줄이고 월드 공간으로 바꾼 뒤, 주행 중 차체 뒤의 짧은 두 줄을 확인했다.
- 호스트 FRED를 3초 가속하는 테스트를 사용했다. 호스트와 두 원격 창에서 `0 → 2 → 0` 활성 상태를 확인했다. 좌우 부착 위치는 세 창에서 동일한 `(-66.0, ±15.08, 17.0)` cm였다.
- 직행 경기에서 정차·가속·정차를 확인했다. 일반 트랙 설치부터의 통합 실행과 실제 아이템 부스터 동시 사용 화면은 이번 검증 범위에 포함하지 않았다.

[입자 조정 전후와 세 창 비교](Images/VehicleExhaustThreePlayers.jpg) · [세 창의 활성 상태와 부착 위치](VEHICLE_EXHAUST_VFX_PIE_LOG.txt)
