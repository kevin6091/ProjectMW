# 일반 경기 코스 아이템 상자 배치 검증

기준 `origin/main@41ac179` 및 상자 연출 `8e9fd14`. 브랜치 `feature/course-item-pickups`. #24 없이 빌드하고 실행했다.

## 변경과 편집 위치

| 파일 | 역할 |
| --- | --- |
| [MWCoursePickupComponent.cpp](../../Source/MW/Gameplay/Items/MWCoursePickupComponent.cpp) | 서버가 이번 라운드의 확정 코스에 상자를 배치한다. 첫 줄은 가장 앞 출발 슬롯과 시작 조각의 출구 사이에 둔다. 이후 조각은 입구·출구 위치와 접선에서 후보 지점을 계산한다. 각 차선 위치에서 해당 조각의 도로 충돌을 수직으로 확인하고 실제 표면 높이를 쓴다. |
| [MWGameplayGameMode.cpp](../../Source/MW/System/GameMode/MWGameplayGameMode.cpp) | 코스·차량·출발 슬롯을 준비한 뒤 배치한다. 라운드 결과와 경기 실패에서 상자를 치우고, 다음 라운드의 준비 단계에서 새 코스를 다시 계산한다. |
| [MWCoursePickupSettings.h](../../Source/MW/Gameplay/Items/MWCoursePickupSettings.h) · [DefaultGame.ini](../../Config/DefaultGame.ini) | Project Settings에서 `MW Course Pickups`를 열어 배치 활성화, 첫 줄 위치, 줄 간격, 차선 오프셋, 조각당 최대 줄 수, 경계·높이 조건을 조정한다. 기존 시작값은 반복 간격 2,300cm, 차선 오프셋 −525/0/525cm다. `Row Spacing`이 0이면 시작 줄만 사용한다. |
| [MWPickupBox.cpp](../../Source/MW/Gameplay/Items/MWPickupBox.cpp) · [MWAssetManager.cpp](../../Source/MW/System/MWAssetManager.cpp) | 생성된 상자에 설정의 기존 몸체·뚜껑·손잡이 메시를 적용한다. 개발맵 또는 BP가 지정한 외형은 보존한다. 공용 에셋 관리자의 기존 쿠킹 목록에 설정이 참조하는 세 메시 패키지만 추가했다. |

기존 `AMWPickupBox`가 차량 중첩을 확인하고, 기존 `UMWItemComponent::TryGrantBooster()`가 경기 단계와 빈 슬롯을 검사해 지급한다. 설정값은 새 상자 배치·외형에 적용된다. 카탈로그에 움직임·교통·기차가 설정된 조각과 설정에서 제외한 조각은 반복 배치에서 건너뛴다. 경사·오프셋에서 적합한 도로 충돌을 못 찾은 차선도 건너뛴다. 결승 캡에는 배치하지 않는다.

## 검증 결과

- UE 5.8.4 에디터 빌드 성공. 아이템 지급·사용 1개와 경기 흐름 3개 자동화 검사를 모두 통과했다. 빌드에는 기존 차량 부스터 시퀀스의 `GetBindings()` C4996 경고가 남아 있다.
- 점수·코스 확장·서든데스 자동화에 `RowSpacing=0`을 1회성 설정으로 적용하자, 반복 실행된 모든 준비 단계에서 시작 1줄과 상자 3개만 생성됐다. 같은 자동화의 기본 설정에서는 코스가 늘며 3줄에서 16줄까지 생성됐다. 저장된 기본값은 2,300cm다.
- cs:80 에셋의 설원 일반 경기 `L_Dev_Drive`에 리슨 호스트와 두 클라이언트로 입장했다. Cua Driver로 세 명이 트랙을 골라 설치했다. 서버는 시작·종료를 포함한 5조각 코스에 상자 17개, 6줄을 배치했다. 첫 줄 좌표는 X=−1323.825cm, Y=−75/450/975cm, Z=6040cm였다.
- 각 창에서 Cua Driver의 키 입력으로 검증 주행을 시작했다. 테스트 도우미는 차량의 기존 주행 입력 함수로 가속·조향·제동을 전달했다. 정상 출발 슬롯에서 주행했고, 강제 이동·상자 지급 주입·추가 상자 생성은 사용하지 않았다. 수동 키보드 조향은 이 검증에 포함되지 않는다.
- 서버 기준으로 세 대가 각기 다른 상자와 겹쳐 부스터를 획득했고, 각 소유 창의 슬롯에 아이템이 표시됐다. 세 창에서 첫 줄 상자 세 개가 모두 사라진 상태를 확인했다. 이후 줄 배치는 서버 위치 로그로 확인했다.

| 플레이어 | 목표 차선 | 소유 창 기준 이동 거리 | 서버 획득 속도 |
| --- | --- | ---: | ---: |
| P1 호스트 | 가운데 | 11.96m | 14.00m/s |
| P2 클라이언트 | 오른쪽 | 14.52m | 14.37m/s |
| P3 클라이언트 | 왼쪽 | 16.35m | 15.52m/s |

Cua Driver 캡처에는 같은 PC에서 D3D12 PIE 세 창을 실행할 때 나온 VRAM 예산 경고가 보인다. 해당 실행에서 세 명의 획득이 모두 성공했다. GPU 성능 측정과 패키징은 이번 검증 범위에서 제외했다. 바이너리 에셋 변경은 없다.

[출발선 첫 줄](course-item-pickups-evidence/race-start-p1.png) · [P1 획득](course-item-pickups-evidence/pickup-p1.png) · [P2 획득](course-item-pickups-evidence/pickup-p2.png) · [P3 획득](course-item-pickups-evidence/pickup-p3.png) · [검증 로그 발췌](course-item-pickups-evidence/verification.txt)
