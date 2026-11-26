# 체크포인트와 경기 결과 UI 연속 캡처

기준은 `feature/match-results-tally`의 `4192c37`이다. 적용 분야는 Client다. 복제된 복귀 시각과 지급 점수, 기존 경기 단계 및 3초 라운드 결과 제한을 사용했다.

| 수정 위치 | 편집값과 동작 |
| --- | --- |
| `MWGameplayHUDSubsystem` | 배지와 카운트다운을 디자인 캔버스 상단의 같은 행에 두었다. 소유 차량의 슬롯을 중심으로 선수색 고리가 작아지고 사라진다. 크기와 여백, 고리 반경은 `CheckpointRecoveryLayout`에서 편집한다. 일반 통과에는 복귀 표시를 띄우지 않는다. |
| `MWMatchResultsWidget` · `WBP_MatchScoreColumn` | 순위 숫자와 ‘위’를 별도 슬롯으로 따라 움직이게 했다. 더 큰 집계 점수와 차량 배지를 기존 막대 상단에 배치했다. 같은 지급 항목의 열별 속도와 조기 정지는 기존 로직으로 처리한다. |
| `WBP_MatchResults` · `WBP_MatchPodiumCard` | 제목 서체 크기, 판 각도, 메달 슬롯, 차량 크기와 진입 비율을 WBP에서 편집한다. 메달 이후 판이 들어오며 이름은 커지고 차량은 나중에 제자리에 들어온다. 판을 5.5° 기울였다. |
| `MWMatchResultsWidget::BeginFinishShot` | 결승 소켓 기준 기본 시점과 WBP의 대체 시점을 사용한다. 가시성 충돌과 코스 메시 경계로 가림을 확인한다. 코스 설치나 트랙 충돌에는 관여하지 않는다. |

## 로컬 WBP 버전

| 에셋 | SHA-256 |
| --- | --- |
| `Content/UI/Results/WBP_MatchResults.uasset` | `5CBB2A8AE6D21EA70D4CBE09840B66C4025840489804476E580D05A8E095B399` |
| `Content/UI/Results/WBP_MatchScoreColumn.uasset` | `A194026980E703C0CE4017CE7DD3B9752E695A0D57E75BFA8C3B2AD9EF21BD4C` |
| `Content/UI/Results/WBP_MatchPodiumCard.uasset` | `D8793B39F587A905C3C6CCBF711EAAAE59605E6E63D17F0FD5B95DFDAD6D71C6` |

UE 5.8 Editor 빌드와 위 세 WBP의 컴파일을 확인한 로컬 검토본이다. 이전 검증과 참조 영상 프레임 번호는 [4192c37 검증 기록](RACE_PRESENTATION_REFERENCE_VERIFICATION.md)을 따른다.

## 최종 빌드의 Cua Driver 3인 PIE

리슨 서버와 클라이언트 두 창을 실행했다. 호스트의 실제 게임 뷰포트는 1920×1080, 두 클라이언트는 800×482였다. 검증용 코드가 목표를 550점으로 설정하고 기존 설치 판정과 체크포인트·결승 중첩을 사용했다. 차량을 트리거까지 옮기거나 코스 아래에 떨어뜨려 각 사건을 재현했다. 수동 완주 영상과는 입력 조건이 다르다.

| 장면 | 캡처 수 | 확인한 결과 |
| --- | ---: | --- |
| 일반 통과 | 11 | 100점이 지급됐으며 Pawn 카메라와 주행 HUD가 유지됐다. 복귀 배지와 고리는 나타나지 않았다. [캡처](Evidence/RaceUIPolish/checkpoint.jpg) |
| 체크포인트 복귀 | 11 | 호스트와 원격 클라이언트에서 0.8초 복귀 대기와 약 0.84초 뒤 주행 권한 재개를 확인했다. 상단 배지·카운트다운이 아이템 슬롯을 가리지 않고, 흰 슬롯과 선수색 고리가 차량 아래에 표시됐다. [캡처](Evidence/RaceUIPolish/recovery.jpg) |
| 점수 가산 | 14 | 오른쪽의 +50 열이 누적 200점에서 먼저 멈췄다. 그 시점 이후에도 다른 두 열은 각각 400·550점까지 올라갔다. 선착 보너스는 1위 완주자에게만 이어서 가산됐다. [캡처](Evidence/RaceUIPolish/tally.jpg) |
| 누적 순위 | 7 | 열 순서를 유지한 채 큰 순위 숫자와 작은 ‘위’를 표시했다. 막대의 400·700·200점 값과 차량 배지는 같은 슬롯에 남았다. [캡처](Evidence/RaceUIPolish/positions.jpg) |
| 최종 시상 | 30 | 세 창 모두 결승 카메라 후보 3번으로 전환했고, 체크무늬 결승선 앞 가림이 해소됐다. 세 창의 1·2·3위와 점수는 P2 700점, P1 400점, P3 200점으로 일치했다. 호스트 첫 1위 표본에서 메달·판·이름·차량 불투명도는 0.249·0.028·0·0이었다. [캡처](Evidence/RaceUIPolish/podium.jpg) |

초기 PIE에서 복귀 배경이 창 높이까지 늘어나는 것을 확인했다. 디자인 캔버스 상단의 오버레이 슬롯이 행의 정렬을 맡도록 고친 뒤 전체 빌드와 위 다섯 구간을 다시 확인했다.

로컬 연속 캡처는 `Saved/CheckpointResultsReference/race-ui-polish-five-scenes.zip`에 있다. 5개 폴더의 JPEG 73장과 장면별 `capture-timing.csv`를 포함한다. Cua Driver 창 캡처에서 테두리와 제목줄 영역을 제외하고 1920×1080으로 잘라 저장했다. 샘플 요청 간격의 중앙값은 장면별 0.18–0.20초, 최대 간격은 0.38초였다. 각 장면의 발생 시점은 CSV의 0초 기준으로 확인할 수 있다. [빌드·뷰포트·카메라·위젯 로그](Evidence/RaceUIPolish/pie-log.txt)

## 영상과의 차이

- 원본 Switch 영상과 이번 PIE의 제목→첫 체크포인트는 약 0.92초와 0.39초, 완주 점수→보너스는 약 2.05초와 0.65초, 보너스→누적 순위는 약 3.12초와 0.73초였다. 서버가 유지하는 3초 안에서 현재 라운드의 실제 지급 항목을 재생한다.
- 시상은 ProjectMW의 5.5° 판과 차량 썸네일을 사용한다. 원본에는 약 10° 판과 판 위 3D 차량이 나온다. 복귀도 ProjectMW의 주행 카메라와 고리·슬롯을 사용하며 원본의 화면 플래시와 슬롯 파편은 표시하지 않는다.
