# 시상 메달 진입 순서와 낙사 회귀 검증

Client UI 작업이다. 기준은 `feature/match-results-tally`의 `d68c746`이다. 사용자 제공 Switch 영상에서 누적 순위판이 걷히는 F30152를 카메라 시작으로 두고, 메달이 처음 식별되는 픽셀을 확인했다.

## 수정 위치

- `WBP_MatchResults`의 `PodiumMedalDelaySeconds`를 1·2·3위 기준 0.05 / 0.00 / 0.13초로 편집했다. `PodiumMedalEntrySeconds`는 0.26초다. `DetailsEntryFraction`은 0.72로 두어 판이 들어온 다음 이름을 읽을 수 있게 했다.
- `UMWMatchResultsWidget::UpdatePodium`이 메달과 판의 진행을 따로 계산한다. 메달은 왼쪽에서 들어오면서 감속하고, 이름판은 1·2·3위 순서대로 들어온다. 메달 일정을 설정하지 않은 기존 위젯은 기존 진행률을 사용한다.
- `BeginPodiumApproach`에서 시상 패널을 투명 상태로 배치하고 `BeginPodium`에서 WBP의 원래 불투명도를 복원한다. `UMWMatchResultRowWidget::SetPodiumEntry`는 레이아웃 기하가 준비될 때까지 메달을 숨긴다. 첫 시험에서 은메달이 임시 오프셋으로 화면 안에 나타난 다음 왼쪽으로 되돌아가는 것이 보여 이 경로를 보완했다.

## Cua Driver 3인 PIE

설원 4,000점 경기에서 세 창이 7라운드를 마쳤고 최종 점수는 4,200 / 3,150 / 2,450이었다. 차량 선택과 트랙 설치는 서버의 기존 경로를 사용했다. 첫 체크포인트는 스로틀로 통과했고, 이후 점수 확인 단계에서는 차량을 실제 서버 트리거에 넣었다. 결과 뷰포트는 호스트 1920×1062, 클라이언트 1280×720과 800×482였다. [세 창의 시상 정착](Evidence/PodiumMedalOrder/three-windows.jpg)

| 실제로 처음 보인 요소 | Switch | ProjectMW |
| --- | --- | --- |
| 은메달 | F30293 · 2.350초 | F739 · 2.352초 |
| 금메달 | F30296 · 2.400초 | F740 · 2.384초 |
| 동메달 | F30301 · 2.483초 | F744 · 2.496초 |

은메달과 금메달은 왼쪽 가장자리에서 먼저 잘린 상태로 보이고 다음 실제 프레임에서 오른쪽으로 이어졌다. 첫 시험에서 보였던 은메달의 역방향 이동은 최종 호스트 녹화에서 관측되지 않았다. ProjectMW 1·2·3위판은 약 2.496 / 2.702 / 2.822초에 식별됐다. 3위판은 원본 표본의 2.917초보다 약 0.095초 먼저 보였다. 프레임 표에서 이름판 첫 표시와 읽을 수 있는 이름을 구분했다. 시상 MP4는 실렌더 147장, 약 4.34초, 프레임 간격 중앙값 29ms다. 원본 60fps 영상과 동일 프레임률로 촬영한 조건은 아니다. [진입 픽셀 확대](Evidence/PodiumMedalOrder/medal-entrance-detail.jpg) · [원본·실행 대응 CSV](Evidence/PodiumMedalOrder/visible-frame-pairs.csv)

## 단독 낙사 회귀

P1만 낙사시킨 뒤 직전 체크포인트 복귀를 확인했다. 낙사 위치 지정은 F123, 조작 제한이 관측된 복귀는 F131, 다시 조작 가능한 시점은 F155였다. 첫 복귀 활성 관측부터 조작 복원까지 약 0.828초였다. P2·P3는 해당 구간 중 모두 조작 가능했다. F8260~8348의 원본은 선두 게이트 통과에 연동된 단체 복귀이며, 이 시험에서 확인한 동작은 기존 단독 낙사다. [복귀 프레임](Evidence/PodiumMedalOrder/individual-recovery-frames.csv) · [플레이어별 로그](Evidence/PodiumMedalOrder/race-observations.jsonl)

두 MP4와 전체 캡처 시간표는 로컬 `Saved/CheckpointResultsReference/MedalRecoveryVerified/verification`의 `podium.mp4`, `individual-recovery.mp4`, 각 `*-timeline.csv`에 저장했다. 두 MP4는 디코딩 검사도 통과했다. 시상 녹화 범위는 카메라 진입부터 세 순위판의 정착 이후까지다.

## 빌드와 로컬 에셋

녹화용 C++을 제외한 `MWEditor Win64 Development` 빌드가 성공했다. 재실행 시 로컬 `Content/UI/Results/WBP_MatchResults.uasset`의 SHA-256을 `F0E48F65DAF8D3192E62DF4EC3C94322494DD0F1E7C181037C114C98C7149150`으로 확인했다. WBP는 UVCS 작업공간의 로컬 변경이다.
