# 트랙 선택판 확대와 커서 복구

## 파란 전시판 관통

후보가 확대될 때 이전 코드는 슬롯의 중심을 기준으로 조각 전체를 확대했다. 기본 위치가 판에 가까운 후보의 아래쪽은 배율이 커지는 동안 내려갔다.

- [MWTrackSelectionBoard.cpp](../../../Source/MW/Gameplay/Construction/MWTrackSelectionBoard.cpp)의 `GetBackdropTop`은 레벨에 배치된 전시판 메시 경계를 상대 트랜스폼으로 변환해 상면을 구한다.
- `BuildOffers`는 기존 화면 맞춤에서 구한 조각 높이로 기준 위치를 잡고, 실제 표시 메시의 아래쪽에 확대 피벗을 둔다. `RefreshOffers`는 이 피벗만 확대한다. 클릭 상자는 전시 슬롯의 회전과 후보의 기본 위치를 따르며 확대 전후에 같은 크기를 유지한다.
- `FrameSelection`은 설정의 최대 배율에서 후보가 차지할 위쪽 높이까지 계산한다. 파란 판이 없는 레벨에서는 중심 확대를 유지한다. 호버 시간과 최대 배율은 기존 MW Track Presentation 프로젝트 설정을 사용한다.

## 전시판 확대 검증

- 빌드 잠금이 걸린 기존 에디터는 그대로 둔 채 별도 사본에서 MWEditor Win64 Development 빌드를 통과했다. 변경한 소스, 프로젝트 설정, 경기 맵, 판 메시, 반투명 재질 등 12개 파일의 SHA-256이 작업 폴더와 일치했다. 기존 `MWItemComponentVisual.cpp`의 C4996 경고가 남아 있다.
- Cua Driver로 일반 속도의 3인 Listen Server PIE를 실행했다. 보조 스크립트는 로비 준비와 차량 선택까지만 진행했다. 세 사람이 각 창에서 실제 마우스로 후보 6개를 가리켰다. 세 창의 18개 후보 관측에서 최대 배율은 1.11999, 판과의 최소 높이 차이는 부동소수점 오차 범위의 0cm였다. 후보마다 확대 전후 클릭 상자 크기가 동일했다. 로그의 `slot`은 0부터 시작한다.
- 세 플레이어가 Cua Driver로 약속된 후보를 눌러 P1 1번, P2 2번, P3 5번이 실제 승인됐다. 세 창이 트랙 배치로 넘어갔다. 캡처는 배율 측정과 별도로 채취했다. 배율과 최소 높이는 프레임 측정 로그를 기준으로 한다. GPU 메모리 안내가 보인 캡처가 있다. [세 창 캡처](Images/TrackPickerCursorAndFloor.jpg) · [빌드 입력 해시와 프레임 로그](TRACK_PICKER_CURSOR_FLOOR_PIE_LOG.txt)
