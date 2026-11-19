# 트랙 선택의 원격 참가자 커서

## 원인과 편집 위치

트랙 선택 화면의 `UMWTrackSelectionWidget::NativeTick`가 매 참가자마다 텍스트 위젯을 만들어 `+ P번호 닉네임`을 표시했다. 기존 차량·로비의 포인터 클래스와 핫스폿을 사용하지 않아 트랙 선택에서만 모양이 달랐다.

| 파일 | 바뀐 내용 |
|---|---|
| `Config/DefaultGame.ini`, `Source/MW/UI/Cursor/MWRemoteCursorOverlay.h` | 공용 포인터 클래스 설정을 `[/Script/MW.MWRemoteCursorOverlay]`로 옮겼다. 프로젝트의 `WBP_VehiclePlayerMarker_Dev`와 표시 크기를 프런트엔드 및 트랙 선택이 함께 읽는다. 포인터 끝은 해당 WBP의 핫스폿 속성에서 편집한다. |
| `Plugins/MWUCMFrontend/Source/MWUCMFrontend/{Public/MWUCMTitleFlow.h,Private/MWUCMTitleFlow.cpp}` | 프런트엔드 커서 설치 시 공용 설정의 클래스·표시 크기를 읽는다. |
| `Source/MW/UI/MWTrackSelectionWidget.{h,cpp}` | 원격 플레이어의 복제된 월드 위치를 수신자 뷰포트의 위젯 좌표로 투영한다. 공용 오버레이가 마커를 참가자별로 재사용하며 화살표 끝을 해당 위치에 맞춘다. 마커에 멤버 팔레트 색·번호·닉네임·확정 상태를 전달한다. 자기 자신, 비활성 참가자, 다른 세션·리비전, 비가시 커서는 그리지 않는다. 배치 단계에 들어가면 해당 레이어를 숨긴다. |
| `Content/UI/Lobby/VehicleSelection/WBP_VehiclePlayerMarker_Dev.uasset` | 기존 화살표·원형 배지·닉네임 위젯을 그대로 불러온다. 트랙 전용 커서 에셋을 추가할 필요가 없다. |

## 3인 PIE 관찰

- PID 8600, Client 1에서 Cua Driver로 우측 직선 후보에 포인터를 옮겼을 때 해당 후보가 강조됐다. Client 2의 복제 상태에 Member 2가 표시됐고, 수신 창의 소유 Controller에 연결된 `WBP_VehiclePlayerMarker_Dev_C`가 생성됐다.
- PID 12364의 별도 3인 PIE에서 Client 1을 Cua Driver로 캡처했다. 왼쪽 후보 위에 다른 참가자 258의 녹색 화살표·원형 배지와 닉네임이 보였다. Client 1의 명단에서 257이 ‘나’이며 258은 다른 참가자다. 배치로 넘어간 PID 8600의 같은 경로에서는 선택 커서가 숨고 설치 고스트가 표시됐다.
