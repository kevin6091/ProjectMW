# 최종 점수 가독성과 시상 순차 공개 검증

Client UI 작업이며 출발점은 [f1d3c27 검증](FINAL_RESULTS_PACING_VERIFICATION.md)이다. 전달받은 7cedd52 편집 명세, MakeWay-reference, 체크포인트·리스폰·점수 동작 Ghidra 분석을 대조했다. 원본별 재생 조건과 프레임 번호는 증거에만 사용했다.

## 바뀐 위치

| 위치 | 변경 |
| --- | --- |
| `UMWMatchResultsWidget::PrepareTally`, `BeginTally`, `NativeTick` | 같은 항목 안에서 지급량이 적은 열은 먼저 멈춘다. 항목이 정착한 뒤 읽는 시간은 최종 완주·선착 보너스에서 따로 사용한다. 일반 라운드는 기존 기한과 비례 압축을 사용한다. |
| `WBP_MatchResults` 최종 표시 설정 | 최대 집계 시간 18.5초, 완주 정착 뒤 1.08초, 보너스 정착 뒤 2.02초, 누적 순위 2.5초를 편집한다. 서버 전환과 점수 판정은 기존 경로를 따른다. |
| `UMWMatchResultsWidget::UpdatePodium`, `WBP_MatchResults` | 순위 간 시작 간격을 따로 두어 메달과 판의 이동이 겹칠 수 있게 했다. 메달과 판은 창 밖에서 시작하고 이름은 판이 들어온 뒤 확대된다. 이어지는 판이 앞 판의 이름을 기다리지 않는다. 시상 카드 이동에는 기존 감속 곡선을 쓴다. |
| `WBP_MatchPodiumCard → RankMedals`, `WBP_MatchResults → WinnerVehicleShot` | 금·은·동 이미지를 순위별로 조정하고 우승 차량의 후반 위치를 1위판 쪽으로 이동했다. |
| `UMWMatchResultsWidget::BeginFinishShot`, `UpdatePodiumCamera` | 결승 포트 로컬 X·Z와 옆 이동을 별도로 줄인 경로도 검사한다. 선택되는 모든 경로는 기존 시야 및 이동 경로 검사를 통과해야 한다. |

## 검증

새 Editor 실행에서 Cua Driver로 3인 Listen Server PIE를 열었다. 4,000점 경기의 7라운드 결과는 4,200/3,150/2,450과 1/2/3위였으며 세 창의 값이 같았다. 두 번째 세션은 같은 에디터에서 새 경기를 시작해 최종 시상을 연속 렌더로 기록하는 용도로 사용했다. 차량을 실제 트리거에 놓은 단계와 스로틀로 체크포인트에 접근한 단계를 구분했다.

| 단계 | f1d3c27 | 수정 후 첫 경기 | Switch |
| --- | ---: | ---: | ---: |
| 완주 → 선착 보너스 | 1.67초 | 2.02초 | 2.05초 |
| 선착 보너스 → 누적 | 1.78초 | 3.08초 | 3.12초 |
| 누적 → 카메라 | 1.90초 | 2.50초 | 2.57초 |

두 번째 경기에서도 7라운드에 4,300/3,250/2,550점을 확정했다. 완주(F422) → 선착 보너스(F493) → 누적 순위(F603) → 카메라(F693)는 2.03 / 3.08 / 2.49초였다. 완주 가산의 색 경계는 초록 0.17초, 파랑 0.77초, 빨강 0.88초에 1px 이내로 정착했다. 마지막 열의 정착 후 다음 제목까지 약 1.16초였다. 선착 보너스는 빨강만 상승했고 0.93초에 정착해 다음 제목까지 약 2.15초를 확보했다. [첫 표시와 전환 프레임](Evidence/FinalResultsReadability/milestones.csv) · [프레임별 막대 좌표](Evidence/FinalResultsReadability/award-settling.csv)

시상 시작 뒤 0.26초인 F790에는 들어오는 첫 판에 이름이 없었다. 0.57초의 F800에는 3위 판의 앞부분이 화면에 들어왔고 1.12초의 F818에는 세 이름이 모두 보였다. 720p 캡처의 메달 지름을 1080p로 환산하면 약 243/200/170px다. 720p에서 1위판과 차량 외곽 사이의 가로 간격은 이전 약 56px에서 이번 약 24px로 줄었다. 가시성 검사는 두 경기 모두 옆 이동 100%를 허용했다. 두 번째 경기는 가림 때문에 대체 시작점을 써 원본보다 옆에서 본 구도가 남았다. [같은 경과시간 비교](Evidence/FinalResultsReadability/same-elapsed-scenes.jpg) · [프레임 대응 및 시각](Evidence/FinalResultsReadability/paired-frames.csv) · [세 창 캡처](Evidence/FinalResultsReadability/cua-three-windows.jpg)

P1·P2를 같은 체크포인트 통과 뒤 동시에 낙하시켰다. 두 차주의 화면에 각자 색 표시와 슬롯이 나타나고, P3는 조작 가능 상태를 유지했다. 둘 다 약 0.8초에 다시 조작할 수 있었다. 두 차주의 가이드는 재출발 때 사라진다. 원본에는 다음 체크포인트 조건으로 여럿을 묶어 배치하는 별도 흐름과 더 밝은 플래시·파편이 있다. 이번 비교는 현재 서버의 개별 낙사 복귀를 동시에 발생시킨 조건이다. [세 창의 복귀](Evidence/FinalResultsReadability/multi-recovery.jpg) · [창별 프레임](Evidence/FinalResultsReadability/multi-recovery-frames.csv) · [소유자별 복귀 로그](Evidence/FinalResultsReadability/runtime-events.jsonl)

4열 사이 간격은 1920×1062, 1280×720, 800×482에서 각각 3.9/2.7/1.8px였다. 세 창의 왼쪽 여백은 약 11%였다. 시상 중 P2를 연결 해제했을 때 해당 창의 카메라, 시상 차량, 표시 타이머가 해제됐다. [열 간격](Evidence/FinalResultsReadability/column-gaps.csv) · [렌더와 종료 로그](Evidence/FinalResultsReadability/pie-events.txt)

로컬 `Saved/CheckpointResultsReference/FinalResultsRefinement/warm/verification`에 실제 프레임 간격을 보존한 `match-final.mp4`(894프레임, 25.924초)와 `recovery.mp4`(180프레임, 6.485초)를 저장했다. 60fps의 단일 프레임 정합은 이 렌더의 검증 범위 밖이다.

## 로컬 WBP 인계

| 에셋 | SHA-256 |
| --- | --- |
| `Content/UI/Results/WBP_MatchResults.uasset` | `441E1033A3E0A4557E565D7EA196E8F44A4B344EFA8AE2BE60B03FCE05636A68` |
| `Content/UI/Results/WBP_MatchPodiumCard.uasset` | `995DAD8E3149100EF0156790D513072BF131191007B10765AF20F7B172F0D670` |
| `Content/UI/Results/WBP_MatchScoreColumn.uasset` | `695AEE7CC20CC2A9348BF149A46E7DB825683DC162D56C9CF15E6893F30F4C3D` |

두 WBP는 로컬에서 컴파일·저장했다. 임시 계측을 제거한 뒤 `MWEditor Win64 Development` 빌드를 통과했다. 점수 열의 패딩과 인원별 폭은 이전 WBP 값이다. Git에는 확인한 소스와 검증 자료를 포함한다. WBP는 UVCS 작업공간에 있고 팀원에게 동일한 화면을 전달하려면 별도 에셋 인계가 필요하다.
