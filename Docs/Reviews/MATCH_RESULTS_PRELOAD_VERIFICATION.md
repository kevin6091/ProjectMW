# 경기 첫 결과판 로딩과 월드 이탈 검증

Client UI 변경이다. [이전 결과 연출 검증](FINAL_RESULTS_READABILITY_VERIFICATION.md)의 첫 집계에서 결과 WBP와 차량 썸네일이 동기 로드되는 경로를 계측했다. 실제 로딩이 발생한 두 자원만 먼저 준비했다.

## 구현 위치

- `UMWGameplayHUDSubsystem::HandleGameplayStateChanged`가 카운트다운 또는 Racing을 처음 관찰하면 설정의 결과 WBP 클래스와 참가자의 차량 썸네일을 요청한다. 썸네일은 기존 차량 선택 데이터를 사용하고 겹치는 경로는 한 번만 요청한다.
- `PrefetchMatchResults`의 핸들은 경기 월드의 HUD가 회차 사이에 보유한다. `ReleaseMatchResultsPreload`는 GameState 교체와 오버레이 제거에서 대기 중인 요청을 취소하고 핸들을 해제한다. 요청에 완료 콜백을 등록하지 않으므로 늦은 완료가 위젯을 다시 열지 않는다.
- 아직 준비되지 않은 자원은 `RefreshMatchResults`와 `UMWMatchResultsWidget::Present`의 기존 표시 경로로 읽는다. 첫 표시 경로와 점수 전환 조건은 기존과 같다. 이 변경에서 편집한 에셋은 없다.

## 첫 집계 계측

새 Unreal Editor 프로세스의 3인 Listen Server PIE에서 결과 WBP를 사전에 에디터로 열지 않았다. OS 파일 캐시는 초기화하지 않았다. 호스트의 에포크 1 첫 결과에 대해 클래스 읽기, 위젯 생성, `Present`를 각각 쟀다. 썸네일 세 개의 로드 합계는 `Present`에 포함된다.

| 호스트 실행 | WBP 동기 읽기 | 썸네일 3개의 읽기 합계 | 최초 열기 계측 합계 |
| --- | ---: | ---: | ---: |
| 적용 전, 새 Editor | 37.1146ms | 5.1504ms | 73.5651ms |
| 적용 전, 같은 Editor 재경기 | 0.0030ms | 0.0082ms | 3.5166ms |
| 적용 후, 새 Editor | 0.0034ms | 0.0099ms | 5.7846ms |
| 적용 후, 반복용 Editor에서 1차 | 0.0035ms | 0.0097ms | 5.9963ms |
| 적용 후, 반복용 Editor에서 2차 | 0.0038ms | 0.0116ms | 4.4888ms |

적용 후 세 창이 각각 카운트다운에 WBP와 3장 썸네일을 한 번 요청했다. 첫 결과의 세 창에서 클래스와 썸네일이 상주했다. 수치는 API 호출과 위젯 준비에 든 CPU 시간이며 GPU의 첫 화면 표시 시간은 별도다. [창별 수치](Evidence/MatchResultsWarmup/first-results-profile.csv)

## 이탈과 반복 경기

4,000점 경기의 7라운드 끝에 세 창에서 4,200/3,150/2,450점과 최종 순위를 확인했다. 시상 정착 뒤 P2만 연결을 끊자 해당 결과판의 카메라, 우승 차량, 타이머, 프리로드 핸들이 해제됐다. P1과 P3의 시상은 유지됐다. [세 창의 이탈 전후](Evidence/MatchResultsWarmup/client-exit.jpg)

다른 새 Editor 하나에서 같은 설원과 차량으로 두 경기를 연속 실행했다. 각 경기 끝의 같은 타이틀 에디터 상태에서 GC를 예약하고 1.2초 뒤 프로세스 및 UObject 수를 읽었다. PIE 월드, 결과 위젯, 점수 행, 시상 차량은 두 차례 모두 0개였다. Windows 작업 집합은 3,178.52 → 3,918.06 → 4,003.99MiB, private 메모리는 4,728.79 → 5,549.16 → 5,708.86MiB였다. 두 번째 종료의 private 메모리는 이전 종료보다 159.70MiB 높았다. 자동 배치로 생성된 코스 모양이 다를 수 있으므로 Editor 전체 메모리로 특정 자원의 장기 누수 여부를 판정할 수 없다. [GC 계측](Evidence/MatchResultsWarmup/gc-samples.csv) · [로딩과 정리 로그](Evidence/MatchResultsWarmup/load-lifetime.txt)
## 요청이 대기 중인 상태로 이탈

임시 CVar로 카운트다운의 비동기 요청을 UE `bStartStalled` 상태에 두고 세 창에서 `Stalled=1 Pending=1`을 확인했다. 그 상태에서도 기존 동기 대체 경로가 실제 7라운드 집계와 시상을 표시했다. 시상 정착 후 P2만 연결을 끊었다. 해당 월드의 `PrefetchRelease Pending=1 Remains=0` 뒤에 실험용 취소 콜백에서 `Canceled=1 Active=0`을 확인했다. 다른 두 창은 PIE 종료 때 같은 방식으로 취소됐다. 이후 로그에 실험용 완료 콜백은 없었다. 생산 코드는 완료·취소 콜백을 설치하지 않는다. 이 시험은 엔진이 아직 실행을 시작하지 않은 대기 요청을 취소하는 조건을 재현했다. 디스크 I/O 도중 경합은 구분한다. [취소 실측 로그](Evidence/MatchResultsWarmup/load-lifetime.txt)
