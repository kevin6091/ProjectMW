# 경기 점수 막대 테두리 픽셀 비교

`feature/match-results-tally`의 `0db5a06` 후속 Client 작업이다. 사용자 제공 Switch 영상에서 `F29390`과 `F30020`의 1920×1080 표본을 사용했다. 막대별 좌우 경계는 Y 750~849에서 열 단위 RGB 중앙값으로 읽었고, 윗변은 중심 X 기준 좌우 14px 구간의 행 단위 RGB 중앙값으로 읽었다.

## 비교와 편집 위치

원본에서 파란 판과 막대 사이의 전환 픽셀 다음에 선수색이 약 **3~5px** 보인다. 이어서 진한 띠가 약 **5~7px** 보이고 안쪽은 선수색 면으로 이어진다. 압축과 계단 완화 때문에 경계 픽셀의 분류에는 약 1px 차이가 있다. 예를 들어 빨강 막대 왼쪽의 F30020 단면에서 x363은 RGB (167,31,35), x368은 (77,1,1), x376은 (196,24,22)다. 비교 시안의 진한 외곽선은 선수색 띠 없이 0px에서 시작했다. [원본 RGB 단면](Evidence/TallyInsetStroke/reference-rgb.txt)

| 편집 위치 | 역할 |
| --- | --- |
| `WBP_MatchScoreColumn → ScoreBar.WidgetStyle.FillImage` | 기존 단색 진행 막대가 선수색 면을 그린다. 브러시 윤곽 폭은 0이다. |
| 같은 WBP의 `MeterLabels → TallyFillOutlineClip → TallyFillOutline` | 투명 면을 가진 RoundedBox 브러시가 진한 띠 한 겹을 그린다. 캔버스 슬롯의 위·왼쪽·오른쪽 여백은 4 디자인 단위, 브러시 두께는 6이다. 클립이 막대 바닥 아래로 연장한 브러시를 잘라 바닥에 가로 띠가 생기지 않게 한다. 배지와 글자는 위 레이어에 있다. |
| `UMWMatchResultRowWidget::SetTallyScore` | 기존 채움 진행률에서 띠의 위쪽 앵커를 갱신한다. 슬롯의 여백·색·굵기는 WBP가 소유한다. 슬롯이 없는 기존 WBP도 기존 채움 경로로 표시된다. |

## Cua Driver 3인 PIE

Listen Server와 클라이언트 두 창의 실제 뷰포트는 1920×1062, 1280×720, 800×482였다. UI에 동일한 4인 데이터를 넣어 고점수, 중간 가산 점수, 0/50/120/500점, 누적 순위의 4가지 조건을 검증했다. 이 표본에서 배지·숫자는 채움 사각형 안에 있었고 선수색 면의 RGB는 이전 값과 같다. 좌우와 위쪽을 포함해 변경 후 단면 144개를 측정했다. UI 격리 표본에는 아래에 있는 경기 HUD나 기존 차량 표식이 일부 보인다.

| 실제 뷰포트 | 경계에서 진한 띠 시작까지 | 진한 띠의 폭 |
| --- | ---: | ---: |
| 1920×1062 | 3~4px | 6px |
| 1280×720 | 2~3px | 4px |
| 800×482 | 1~2px | 3px |

설원 실제 3인 경기에서 차량 선택과 트랙 설치를 거쳐 Racing에 들어갔다. 완주 위치는 테스트에서 지정했고, 기존 서버 결승 트리거가 세 차량에 300/150/50점을 확정했다. 세 창의 완주 점수 집계와 호스트의 누적 순위 이후 다음 라운드 복귀를 확인했다. 2라운드에서 같은 위치 지정 도구는 새 코스의 결승 통과를 재현하지 못했다. 따라서 2라운드 시도는 정상 주행이나 통과 증거로 사용하지 않았다.

GPU가 그린 연속 60fps 녹화 대신 Cua Driver의 창 캡처를 사용했다. 완료된 누적 순위에서 다음 게임 단계로 전환되기까지 약 5.5초 동안 28장을 추가로 저장했다. 원본과 픽셀 위치를 대조할 때는 네이티브 폭의 캡처를 사용했다.

[1:1 전후 비교](Evidence/TallyInsetStroke/inset-stroke-native.jpg) · [픽셀 단면 CSV](Evidence/TallyInsetStroke/pixel-profiles.csv) · [실제 경기 캡처](Evidence/TallyInsetStroke/live-results.jpg) · [PIE 로그](Evidence/TallyInsetStroke/pie-log.txt)

## 인계와 후속 검증

| 항목 | 현재 확인 범위 |
| --- | --- |
| 점수판 가산·누적 순위 | 막대와 숫자·배지가 기존 진행률을 따라 움직인다. 중간 가산과 순위 화면에서 새 테두리를 확인했다. 게임 전체 가산 단계와 원본의 프레임 단위 타이밍 차이는 이번 캡처로 판정하지 않았다. |
| 폰트와 에셋 비율 | 세 뷰포트에서 다섯 자리 숫자와 배지의 슬롯이 막대 안에 있다. 판과 로고, 뱃지의 기존 브러시와 크기를 재사용했다. 길이가 더 긴 이름의 극한 조건과 별도 클라이언트 첫 로딩은 이번 실행에서 확인하지 않았다. |
| 최종 시상 | 기존 구현은 `PodiumCameraKeys`, `WinnerVehicleShot`, 순위별 캔버스와 카드 공개 순서를 사용한다. 이전 검증에서 1위판은 순위판 퇴장 후 약 2.54초에 공개됐고, 난간 각도와 메달 크기 차이가 남았다. 이번 수정에서는 새 시상 실행을 측정하지 않았다. [시상·복귀 기록](PODIUM_RECOVERY_RETIMING_VERIFICATION.md) |
| 통과·복귀·재출발 | 기존 복귀 HUD는 원형 표식과 펄스를 그리고, 주행 카메라가 별도로 복귀 높이를 적용한다. 동일 코스에서 일반 통과와 복귀를 각각 주행한 60fps 대조는 미검증이다. [복귀 기록](PODIUM_RECOVERY_RETIMING_VERIFICATION.md) |
| 선택판·배치 연출 | 트랙 선택·설치 연출은 별도 변경으로 기록돼 있다. 설치 지점 중심 카메라와 선택판 천의 움직임은 이번 테두리 수정에서 검증한 범위 밖이다. [건설 연출 검증](TRACK_CONSTRUCTION_PRESENTATION_VERIFICATION.md) |

WBP 컴파일·저장을 확인했고 임시 검증 C++을 제거한 뒤 `MWEditor Win64 Development`를 다시 빌드했다. 전후 비교용 WBP 두 개는 배포 에셋 디렉터리 밖의 `Saved/CheckpointResultsReference/TallyInsetStroke`로 옮겼다. 이 폴더의 캡처는 동일 조건의 비교와 로그 판독에 사용했다.

로컬 `Content/UI/Results/WBP_MatchScoreColumn.uasset`의 SHA-256은 `DD5713EF5C1747D070838C939B5C103DF827873CF9B646AAE42D6B4FCC7D65BB`다. Git에는 진행률 연결 코드와 검증 자료가 들어가며 이 WBP는 로컬 UVCS 작업공간에 있다. UVCS 체크인과 Git 푸시는 보류했다. `WBP_MatchResults`와 `WBP_MatchPodiumCard`의 해시는 수정 시작 전과 같다.
