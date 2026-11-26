# 체크포인트 통과 안내

## 영상과 구현 범위

Switch 플레이 영상의 1:29–1:38, 4:13–4:25에서 완주 후 체크포인트 점수가 별도 순서로 집계되는 것을 확인했다. 주행 중 통과 문구는 같은 프레임에서 직접 확인하기 어려웠다. 주행 중에는 ProjectMW의 경기 HUD 서체와 색을 사용해 화면 상단에 통과 번호와 실제 획득 점수를 짧게 표시한다. 경기 종료 후 단계별 집계와 1위 공개는 별도 변경 범위다.

## 편집 위치

| 위치 | 역할 |
| --- | --- |
| `Content/UI/Race/WBP_CheckpointFeedback.uasset` | 안전 영역 상단 중앙 배치, 서체, 판과 점수 색, 문구 서식, 표시·퇴장 시간을 편집한다. |
| `Config/DefaultGame.ini`의 `MWGameplayHUDSubsystem` | 어떤 위젯 클래스를 경기에서 사용할지 지정한다. |
| `MWCheckpointFeedbackWidget.cpp` | 기존 한 판을 갱신하고 시간이 지나면 숨긴다. 획득 점수가 없는 통과도 번호를 표시한다. |
| `MWGameplayHUDSubsystem.cpp`의 `RefreshCheckpointPlayer`·`HandleCheckpointProgress` | 현재 창의 PlayerState만 구독한다. 같은 주행 라운드에서 서버 통과 번호가 증가한 경우 표시하며 획득 점수는 서버 누적값의 차이를 사용한다. 최초 접속 시 이미 받은 값은 기준값으로 삼고 라운드가 끝나면 판을 닫는다. |

체크포인트 채점과 복귀는 기존 서버 로직을 사용한다. UI가 클릭이나 차량 입력을 받지 않는다.

## 검증

- UE 5.8 `MWEditor Win64 Development` 빌드 성공. 새 WBP를 생성·컴파일하고 실제 PIE에서 한글·점수 바인딩을 확인했다.
- Cua Driver로 3인 리슨 서버 PIE를 시작하고 각 창에서 두 라운드의 트랙을 설치했다. 검증 스크립트는 로비·선택 준비에 기존 서버 요청을 사용했다. 골인과 체크포인트에서는 차량 위치만 실제 트리거로 옮겼다. 점수나 HUD 값은 주입하지 않았다. 일반 수동 주행은 이 시험 범위에서 제외했다.
- 두 번째 라운드의 실제 체크포인트 조각 4에서 P1·P2·P3가 순서대로 100점을 받았다. 각 로컬 위젯이 `체크포인트 1`과 `+100점`을 표시하고 페이드 후 접혔다. 설치·카운트다운·미통과 상태에서는 접혀 있었다. 호스트 뷰포트는 1280×722, 클라이언트 두 창은 800×482였다.
- [P1 통과](Evidence/CheckpointFeedback/p1-award.png), [P2 통과](Evidence/CheckpointFeedback/p2-award.png), [P3 본인 통과 전](Evidence/CheckpointFeedback/p3-before-own-pass.png), [세 창 서버 판정·복제 관측 로그](Evidence/CheckpointFeedback/pie-evidence.txt)

## 에셋 인계

- UVCS `sh:14@ProjectMW/ProjectMW@sms1599@unity`의 유일한 추가 파일은 `Content/UI/Race/WBP_CheckpointFeedback.uasset`이다. 셸브의 파일 리비전을 읽어 PIE에 사용한 파일과 SHA-256을 대조했다.
- SHA-256 `87FB63FD391C6751924E2E05730D376B8C33EA292F334CB2D1B3FDF435CBC42A`
- 팀원은 Git `feature/checkpoint-feedback`을 받아 빌드하고 Unity Version Control에서 이 셸브의 WBP를 적용한다. 에셋 부모 클래스가 새 C++ 코드에 있으므로 두 변경을 같은 검증 세트로 적용한다.
