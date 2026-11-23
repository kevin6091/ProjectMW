# 트랙 선택 화면과 로딩 통합 검증

## 최신 main과 로딩 렌더 검사 반영

- 작업 브랜치 `feature/presentation-integration`. 최신 `origin/main` `59c2dc7`을 `f3578bf`에 반영하고, `refactor/arrival-frame` `7e7cb36`을 `987a682`에 반영했다. 자동 병합은 두 건 모두 충돌 없이 완료됐다.
- 첫 장면의 렌더 콜백·펜스 수명은 [MWArrivalFrame](../../../Source/MW/UI/Gameplay/MWArrivalFrame.cpp)이 담당한다. 공개 조건과 늦게 도착한 뒤의 가림막·입력 복구는 [MWArrivalScreen](../../../Source/MW/UI/Gameplay/MWArrivalScreen.cpp)이 담당한다. 이 작업 단위는 Git 소스만 반영했다.
- UE 5.8.4에서 `MWEditor Win64 Development` 빌드 성공. Cua Driver로 리슨 호스트와 클라이언트 2개의 창에서 조작했다. 관측 스크립트는 뷰포트와 게임 상태를 읽고 로그를 남겼다.

| 플레이어 | 게임 뷰포트 | UMG DPI 스케일 |
| --- | ---: | ---: |
| P1 호스트 | 954×447 | 0.4440 |
| P2 클라이언트 | 1149×555 | 0.5134 |
| P3 클라이언트 | 1474×762 | 0.7050 |

- **정상 진입.** 로비에서 세 명이 준비하고 차량을 선택했다. 출발 보고 뒤 세 창이 `L_Dev_Drive`의 트랙 선택 카메라를 공개했다. P3 경사, P1 직선, P2 곡선을 차례로 설치했다. 서버는 조각 2·3·4를 승인했고 세 창 모두 시작 조각 위에서 자기 차량을 소유한 상태로 Racing에 진입했다. 경사 조각 설치와 Racing 시작까지를 검증 범위로 기록한다. [세 창 출발선](Images/PresentationIntegrationRace.jpg)
- **45초 뒤 늦은 도착.** 별도 실행에서 P3의 출발 보고를 지연시키고 P1·P2 타임아웃을 기다렸다. P1·P2에서 출발 월드 `L_Title`의 기본 45초 복구 화면을 기록했다. 이후 지연을 풀어 원래 출발 보고 경로로 `L_Dev_Drive`에 진입했다. 기존 시도의 ID로 새 월드가 공개됐고 세 창의 실제 트랙 선택 클릭이 승인되어 배치까지 열렸다. [45초 안내와 복구 캡처](Images/PresentationIntegrationRecovery.jpg)
- **차량 자동화.** 9개 중 8개 통과. `MW.Vehicle.CollisionAndGround`의 `Upward impulse releases ground`, `Upward impulse is not erased` 두 단언은 기준 main 이력과 동일하게 실패했다. [이전 차량 이동 검증](VEHICLE_PAWN_LOCAL_MOVEMENT.md) · [이번 실행 로그](PRESENTATION_ARRIVAL_PIE_LOG.txt)
