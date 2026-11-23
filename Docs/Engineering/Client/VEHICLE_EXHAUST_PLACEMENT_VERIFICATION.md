# 차량 일반 배기 분사 위치와 크기 조정

2026-11-23 KST · `feature/presentation-integration` · Git 위치 설정 `ddb4358` · UVCS 에셋 `cs:68`

10월 30일 멀티플레이 영상 02:07.1–02:10.0에서 일반 배기 두 줄의 시작점이 차체 뒤쪽 아래에 가깝고 좌우 간격이 좁은 것을 확인했다. 차량 종류와 카메라가 달라 영상에서 cm 값을 가져올 수 없으므로, ProjectMW 차체 대비 간격과 입자 크기를 조정했다.

| 편집 위치 | 이전 | 변경 |
| --- | --- | --- |
| `Config/DefaultGame.ini`의 `MWVehicleExhaustSettings.RearAnchor` | 충돌 박스 반치수 비율 X -1.10, Z 0.25 | X -0.96, Z 0.10. 장착점이 차체 가까이 이동하고 높이는 내려간다. |
| 같은 설정의 `SideOffset` | 0.26 | 0.17. 중심에서 좌우 거리를 줄인다. |
| `Content/Gameplay/Vehicles/Art/Exhaust/NS_MW_VehicleExhaust.uasset`의 Initialize Particle | 초기 스프라이트 8–16 cm | 3.5–7 cm. 월드 공간 방출과 기존 감쇠를 사용한다. |

UVCS `cs:68`은 Niagara 에셋 1개만 변경했다. 서버에서 다시 받은 파일과 로컬 SHA-256이 `a3139a914b3151c21d209ee0b4517a9b64195d1889f459b7f869cb525c23ced7`로 일치했다. 설정은 Git 커밋과 같이 받아야 최종 배기 위치가 적용된다.

## Cua Driver 3인 PIE

설원 일반 로비에서 차량 선택, 세 명의 트랙 설치, 체크무늬 출발선의 Racing을 확인했다. 코스에 아이템 상자가 없어 준비된 기존 `AMWPickupBox` 하나를 출발 구간에 임시로 배치했다. P2 DOUG가 서버 충돌로 획득한 부스터를 Cua Driver로 사용했다. 근접 비교에는 PIE에서만 생성한 추적 카메라를 사용했다.

같은 DOUG의 실측 장착점은 상대 좌표 `(-80.64, ±6.80, 4.50) cm`였고 세 창이 일치했다. 이전 설정으로 동일 차종의 충돌 박스에 적용하면 `(-92.40, ±10.40, 11.25) cm`다. 세 창 모두 일반 배기 활성 2개 → 부스터 발동 중 0개 → 자동 종료 뒤 주행 중 2개를 관찰했다. 전후 캡처는 같은 차종과 추적 카메라 설정에서 촬영했으며 재생 위치와 입자 프레임은 서로 다르다.

[세 창 전후와 부스터 회귀 캡처](Images/ExhaustPlacementThreePlayers.jpg) · [트랙 설치·분사 좌표·전환 로그](VEHICLE_EXHAUST_PLACEMENT_PIE_LOG.txt)
