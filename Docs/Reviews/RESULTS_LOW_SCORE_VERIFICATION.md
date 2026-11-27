# 경기 점수판 저점수·열 배치 검증

`feature/match-results-tally`의 `fb0ddcd` 후속 Client UI 작업이다. 제공된 Switch 영상 F10040·F10075와 집계 프레임 묶음의 F29390·F30020을 사용했다. 낮은 점수에서 배지·숫자와 막대가 따로 움직이는 문제와 열 사이의 닉네임 겹침을 수정했다.

## 원인과 수정 위치

| 위치 | 동작과 편집값 |
| --- | --- |
| `Source/MW/UI/Gameplay/MWMatchResultsWidget.cpp`의 `UMWMatchResultRowWidget::SetTallyScore` | 기존에는 색 막대가 목표 대비 점수 비율로 끝나고 배지·닉네임·등수는 WBP 최소 높이에서 멈췄다. 이제 색 막대와 정보행의 세로 경계를 하나의 표시 높이로 계산한다. 점수 숫자는 기존 값을 그대로 표시한다. 배지·점수·닉네임과 등수는 갱신 시 같은 세로 앵커를 사용한다. |
| `Source/MW/UI/Gameplay/MWMatchResultsWidget.h` | `TallyBaseFraction`은 0점 정보행이 들어가는 바탕 높이다. WBP 선택 바인딩 `TallyNameMarker`가 있으면 이름의 부모 슬롯을 움직인다. 기존 이름 슬롯도 읽는다. |
| `Content/UI/Results/WBP_MatchScoreColumn.uasset` | `TallyBaseFraction`을 0.23으로 지정했다. 0점 바탕 위의 남은 높이가 점수에 비례한다. `TallyScoreMarker`의 안쪽 여백 13과 `TallyScoreLine` 좌측 정렬로 숫자 폭과 무관하게 배지 시작점을 고정했다. 점수 글꼴의 크기를 43.5→41 디자인 단위로 내렸다. `TallyNameMarker` ScaleBox는 긴 이름에만 축소를 적용한다. |
| `Content/UI/Results/WBP_MatchResults.uasset` | `TallyWidthByPlayerCount`의 3인 슬롯 너비 1,040→940 디자인 단위. 4인은 기존 슬롯 기본값을 사용한다. |

F10040은 0점 열에도 채색된 바탕, 그 위의 닉네임, 바탕 안의 배지와 `00000`을 보여 준다. F10120에서는 같은 열이 `00017`로 올라가도 이 레이어 순서가 같다. 이번 표시값은 `바탕 + (1 − 바탕) × 점수/목표점수`다. 서버가 계산한 점수·순위·경기 시간·단계 전환은 기존 경로를 사용한다. 보드·차량 배지의 기존 글꼴, 텍스처와 참가자 팔레트를 계속 사용한다. 로컬 WBP와 이 코드의 대응 버전은 아래에 기록했다.

## 같은 점수로 렌더링한 결과

Cua Driver로 호스트 1920×1080, 클라이언트 1280×720·800×482의 세 창을 확인했다. 같은 경기 배경에서 변경 전 WBP 복제본과 변경 후 위젯을 각각 렌더링했다. 전 상태에는 `fb0ddcd`의 앵커·채움 계산을 동일하게 적용했다. 실제 경기는 3인이다. 4인, 동점, 점수 0·50·120·500/4,000 및 정지된 가산 표본은 서버 진행에 연결하지 않는 지역 표시용 데이터다.

| 측정 | 변경 전 | 변경 후 |
| --- | ---: | ---: |
| 1080p 120점 열, 배지 위쪽과 채색 면 위쪽 | 배지가 84.4px 위 | 배지가 12.0px 안 |
| 720p 120점 열, 같은 경계 | 배지가 56.3px 위 | 배지가 8.0px 안 |
| 800×482 120점 열, 같은 경계 | 배지가 37.7px 위 | 배지가 5.4px 안 |
| 1080p 3인 열 전체 폭 | 1,012px | 912px |
| 1080p 4인 열 전체 폭 | 1,242px | 1,242px |
| 1080p 첫 배지 중심과 열 왼쪽의 간격 | 3인 66.8px, 4인 52.2px | 모두 57.0px |
| 1080p 판과 색 막대의 하단 간격 | 5px | 5px |
| 1080p 동일한 첫 숫자 `0`의 밝은 획 | 32×49px, 1,053px² | 31×47px, 972px² |

배지를 쓰는 네 열에서 실제 WBP 배지 지름은 1080p 88px, 720p 약 58.6px, 800×482 약 39.3px다. 숫자 획은 R·G·B가 모두 205를 넘는 픽셀로 측정했다. 참조 F30020의 첫 숫자 `0`은 30×46px, 975px²였다. 17자 한글과 33자 영문·숫자·기호 이름도 생략 없이 표시되고, 합성 표본의 텍스트 ROI 내 밝은 획은 1080p에서 열 양쪽과 최소 11.7/12.7px, 720p에서 7.6/9.0px, 800×482에서 5.6/7.3px 떨어졌다. 이름의 프로젝트 공통 길이 상한은 확인되지 않았다. 작은 창의 33자 이름은 다른 이름과 겹치지 않지만 작게 보인다.

[세 해상도 저점수 전후](Evidence/ResultsLowScore/low-score-before-after.jpg) · [3인·4인 가산 및 동점](Evidence/ResultsLowScore/three-four-layout.jpg) · [글자 실측 CSV](Evidence/ResultsLowScore/text-bounds.csv) · [위젯 기하](Evidence/ResultsLowScore/layout-geometry.csv) · [막대 채움](Evidence/ResultsLowScore/display-fill.csv)

### 원본 오버레이의 남은 차이

F29390은 2,950/1,900/1,675/2,000점, F30020은 3,000/2,300/2,375/2,650점으로 검증 위젯에 같은 값을 넣었다. 중간값 표본도 남겼다. 해상도와 크롭을 일치시킨 다음 원본과 결과를 50%씩 겹쳤다. 좌표는 화면 픽셀이다.

| 1080p 측정 | 참조 | ProjectMW |
| --- | --- | --- |
| 판 경계 | (192,174)–(1728,999) | (220,196)–(1700,977) |
| 4인 열 중심 X | 약 510 / 810 / 1110 / 1410 | 483.8 / 801.3 / 1118.8 / 1436.3 |
| 4인 막대 너비·바닥 Y | 약 298px / 994 | 289.5px / 972 |
| 첫 열 왼쪽부터 배지 중심·첫 숫자 획 | 약 +57px / +108px | +57px / +114px |
| F29390과 동일 점수의 네 채움 경계 Y | 428 / 594 / 628 / 578 | 587.4 / 684.8 / 705.7 / 675.6 |
| F30020과 동일 점수의 네 채움 경계 Y | 422 / 532 / 518 / 476 | 582.8 / 647.7 / 640.8 / 615.3 |

4인 열 간격과 판의 절대 배치에 차이가 남는다. 표시용 목표 4,000에서 막대가 자랄 수 있는 공간도 서로 다르다. 점수가 같아도 위쪽 경계가 78~161px 어긋난다. 참조의 참가자별 채점 기준을 추정해 목표점수를 변경하지 않았다. [F29390 대조](../../Saved/CheckpointResultsReference/LowScoreReview/diagram-F29390.jpg) · [F30020 대조](../../Saved/CheckpointResultsReference/LowScoreReview/diagram-F30020.jpg) · [F29390 절대 픽셀 차이](../../Saved/CheckpointResultsReference/LowScoreReview/diff-F29390.jpg) · [F30020 절대 픽셀 차이](../../Saved/CheckpointResultsReference/LowScoreReview/diff-F30020.jpg)

## 다섯 장면의 연속 렌더

3인 PIE에서 두 라운드를 진행했다. 호스트는 설치된 트랙 위에서 전진 입력으로 체크포인트에 접근했다. 검증용 초기 주행 위치와 낙하 위치를 지정했고, 그 밖의 일부 참가자 통과·결승도 트리거 재현을 사용했다. 테스트 목표는 550점이었다. 세 창의 최종 순위는 550/450/300점으로 일치했다.

| 장면 | 연속 프레임 | 관찰 |
| --- | --- | --- |
| 일반 통과 | 22–87 | 차량이 게이트로 접근하고 70–72에서 체크무늬를 밟는다. 47 부근에서 먼저 서버 통과가 관측됐고, 시각 경계와의 차이는 약 1.14초다. 월드 슬롯과 차량·체크무늬가 한 구도에 들어오는 프레임은 68–72이다. |
| 체크포인트 복귀 | 110–148 | 바깥으로 떨어진 차가 돌아온 뒤 119 부근의 대기 표시, 123의 플래시, 135 부근의 복구, 140의 정상 시점이 보인다. |
| 체크포인트 점수 가산 | 215–242 | 라운드 진입, `체크포인트 1 점수` 첫 표시, 중간 가산과 완주 점수 전환이 확인됐다. |
| 누적 순위 | 239–255 | 242에서 순위 표기, 244–246에서 유지, 252에서 판이 걷힌 결승 뷰를 확인했다. |
| 결승 시상 | 282–315 | 286에서 차량과 결승선, 289에서 1위 카드, 292에서 2위, 297에서 세 카드, 304에서 정착한 상태가 보인다. 정착 이후 화면을 유지했다. |

호스트의 1920×1080 백버퍼 544장을 연속 저장했다. 요청·저장 544장, 큐 유실·인덱스 누락·인접 CRC 중복 각각 0건이다. 33.983초, 평균 15.98fps, 중앙 간격 68ms, 최장 간격 222ms였다. 위 다섯 구간의 실제 속도는 10.42–23.02fps였다. 체크포인트 화면의 기준 프레임을 물리 통과 로그와 분리했다. 이 자료는 상태 순서와 화면·기하 검증에 사용할 수 있고, 원본과 60Hz 단위로 정렬한 첫 표시·퇴장 오차는 미검증이다. 이전 [복귀·시상 대조](PODIUM_RECOVERY_RETIMING_VERIFICATION.md)에서 월드·HUD 책임과 순위별 입장 타이밍을 확인할 수 있다.

[다섯 장면 프리뷰](Evidence/ResultsLowScore/five-scenes.jpg) · [제목별 연속 1080p 원본 프레임 ZIP](../../Saved/CheckpointResultsReference/LowScoreReview/five-scenes-native-1080p.zip) · [기준 프레임과 상대 시각](Evidence/ResultsLowScore/scene-anchors.csv) · [렌더 로그](Evidence/ResultsLowScore/render-frames.csv) · [PIE 기록](Evidence/ResultsLowScore/pie-notes.txt) · [시상 세 창](Evidence/ResultsLowScore/live-podium-three-windows.jpg)

## 빌드·인계

UE 5.8에서 두 WBP의 컴파일과 `MWEditor Win64 Development` 빌드를 통과했다. 검증 중에 닉네임 슬롯을 CanvasSlot으로 가정한 측정 스크립트를 수정했고 재실행 결과만 사용했다. 계측 후 임시 QA C++ 3개와 `Content/MWLocalLowScoreReviewTemporary` 아래 이날 생성한 비교용 WBP 복제본 2개를 제거한 뒤 에디터를 재빌드했다.

| 로컬 WBP | SHA-256 |
| --- | --- |
| `Content/UI/Results/WBP_MatchResults.uasset` | `D881307686A74CB7D1EB44EF57401A40CF9AC60B9A4F9CC4E964DC9A0ADFF1F4` |
| `Content/UI/Results/WBP_MatchScoreColumn.uasset` | `AB2F35BA056448F375A921CF4D44CBF647DDFFA5EEF610E70CAB278B18F01B33` |
| `Content/UI/Results/WBP_MatchPodiumCard.uasset` | `D5B94096BAFE06D0D0674EAA4C4A13E74FB9D25195CBBAD72733D0CF7B96EFB4` |

코드와 검증 문서는 Git에 기록한다. WBP 두 개는 Git 제외 에셋이며 현재 로컬 버전이다. 필요할 경우 `Saved/CheckpointResultsReference/LowScoreReview`에 있는 변경 전 두 WBP의 파일 사본으로 에셋을 복원할 수 있다. 해당 WBP 사본은 `fb0ddcd`의 코드와 대응한다. 푸시·UVCS 체크인·패키징·병합은 보류했다.

임시 `Saved/CheckpointResultsReference/LowScoreReview/dynamic/previews.jpg`는 최종 다섯 장면 프리뷰와 연속 프레임 ZIP으로 대체해 삭제했다.
