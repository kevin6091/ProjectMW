# 경기 점수판·시상 카드 배치 검증

`feature/match-results-tally`의 `9dade78` 후속 Client UI 작업이다. 제공된 경기 영상의 같은 종류 화면과 최신 빌드를 대조했다. 기존 텍스처를 그대로 참조하고, 편집값은 WBP의 부모 슬롯과 브러시에 두었다.

## 수정 위치

| 위치 | 변경 |
| --- | --- |
| `WBP_MatchResults` | `TallyStack`에서 제목을 올리고, 열 영역을 별도 `TallyColumnExtent` 중앙 슬롯에 넣었다. 3인에만 집계 열 너비 1,040 디자인 단위를 적용하고, 나머지는 슬롯 기본 1,270 단위를 사용한다. `TallySignLogo`와 `TallySignPole`은 판의 중심축에 맞췄다. 시상 때는 세 카드의 상대 앵커로 순위 간 간격을 정한다. |
| `WBP_MatchScoreColumn` | 닉네임을 각 막대 중심에 배치하고, 배지와 점수는 막대 안의 한 행에서 중앙 정렬한다. `TallyRankGroup` 안에 등수 숫자와 ‘위’를 두고 접미사의 좌측·하단 패딩으로 간격과 기준선을 정한다. |
| `WBP_MatchPodiumCard` | 왼쪽 메달과 겹치지 않게 이름 시작점을 정하고, 오른쪽 썸네일의 슬롯을 확보한다. `PodiumNameFit`은 길 때만 배정 폭에 맞춰 이름을 축소한다. 카드 기울기 −5.5°→−7°를 적용했다. `PlateAspectExtent`가 695:126 비율을 유지하고 브러시는 Image로 그린다. |
| `MWMatchResultsWidget.h/.cpp` | 참가 인원수로 WBP가 가진 열 너비 값을 찾고, 순위 그룹을 집계 진행률과 같은 세로 앵커에서 이동시킨다. 그룹과 폭 슬롯이 없는 WBP에도 기존 경로를 유지한다. |

## 동일 조건의 1080p 실측

Cua Driver로 1920×1080 호스트와 1280×720, 800×482 클라이언트를 띄웠다. 실제 3인 경기에서 `길모퉁이 레이서`, `VECTOR-9`, `NIGHT SHIFT`로 가산, 누적 순위, 결승 시상을 캡처했다. 검증용 목표 550점과 위치 재현을 사용했다. 전후 비교는 같은 PIE 배경과 같은 합성 점수·닉네임을 표시 전용 위젯에 넣었다. 4인 표본은 `푸른질주`를 추가한 UI 검증이다.

| 집계 위치 | `9dade78` | 수정 후 |
| --- | ---: | ---: |
| 3개 막대 양 끝 사이 | 1,242px | 1,012px |
| 막대 하나 너비 | 395.3px | 318.7px |
| 판 1,480px 폭에서 열이 차지하는 비율 | 83.9% | 68.4% |
| 판 안쪽 좌우 여유 | 각 119px | 각 234px |
| 제목 슬롯과 판 위쪽 간격 | 49px | 37px |
| 로고 슬롯이 판 상단 위로 나온 높이 | 63px | 74px |
| 로고·기둥 중심 X | 960px | 960px |
| 판과 막대 하단 간격 | 5px | 5px |

제공 영상에서 읽은 3개 막대의 폭은 약 900px였다. ProjectMW에서는 막대 여러 개 사이의 간격과 88px 배지·다섯 자리 숫자가 들어갈 공간을 반영해 1,012px로 줄였다. 첫 열의 배지 중심은 막대 왼쪽 기준 60px→66.8px이며, 배지 오른쪽과 점수 슬롯 사이 간격은 10px다. 배지와 점수의 슬롯 세로 중심 차이는 0.1px 이내였다. 축소 조건인 4인 720p에서는 점수 슬롯 오른쪽이 막대 안쪽으로 약 3.9px 들어왔고, 네 닉네임과 다섯 자리 점수가 화면에 표시됐다.

같은 첫 열의 `02720→03100` 가산 표본에서 닉네임·배지·점수의 상단이 각각 45.7px 올라갔다. 실제 주행 경기에서도 가산 스테이지의 전후 GPU 프레임으로 같은 묶음 움직임을 확인했다. [1080p 집계·순위·시상 전후](Evidence/ResultsPositionReview/layout-before-after.jpg) · [4인 720p 집계와 순위](Evidence/ResultsPositionReview/layout-four-at-720p.jpg) · [실제 3인 주행·가산·시상](Evidence/ResultsPositionReview/live-named-race.jpg)

순위 화면에서 밝은 글자 픽셀의 오른쪽·왼쪽 경계와 아래쪽을 쟀다. 3인 1080p에서 숫자와 ‘위’의 가로 간격은 25–27px→15–17px, 아래쪽 차이는 8px→1px 이내였다. 4인 720p 표본의 첫 세 열에서 가로 간격은 15–18px→9–11px, 아래쪽 차이는 5–6px→1px 이내였다. 그룹을 처음 적용했을 때 ‘위’가 최대 15px 아래였기 때문에 WBP에서 하단 패딩을 32 디자인 단위로 올려 다시 캡처했다.

## 이름판과 종횡비

1위 메달의 화면 중심은 1080p에서 (365,278)→(365,268)로 약 10px 올라갔다. 2·3위 중심은 (477,608)→(477,599), (588,852)→(588,863)이다. 첫 카드의 이름 슬롯은 x478→462로 메달 쪽에 가까워지고, 오른쪽의 2D 차량 영역 시작점은 x920→965로 이동했다. `길모퉁이 레이서`와 `NIGHT SHIFT`가 오른쪽 그림을 덮었던 전후 차이를 캡처했다. 실제 3인 시상의 1위는 별도 3D 차량을 그리며, 카드 쪽 2D 차량이 그 자리에서 숨겨진다. 이 검증에서는 3D 차량의 궤적과 공개 시간을 따로 유지하고, 세 창에서 시상 정착을 확인했다.

| 수정 후 슬롯 | 3인·4인 1920×1080 | 3인·4인 1280×720 |
| --- | --- | --- |
| 로고 | 122×122px | 81.2×81.2px |
| 원형 배지 | 88×88px | 약 58.6×58.6px |
| 점수판 차량 썸네일 | 76×76px | 약 50.6×50.6px |
| 1위 메달 | 약 203×203px | 135.4×135.4px |
| 1위 카드의 2D 차량 썸네일 | 약 153×153px | 101.7×101.7px |

판의 오른쪽 짙은 띠를 1080p에서 가로 31개 행으로 비교했다. 중앙값은 기존 22px, 9분할 변경 시도 27px, 최종 Image 브러시 22px였다. 원단에 있는 어두운 윤곽은 유지된다. 3인과 4인 모두 카드 폭이 달라져도 디자인 비율을 유지하도록 했다. 4인 720p 저점수 검증의 500점 막대는 둥근 윗부분을 볼 수 있다. 120점과 50점은 4,000점 대비 높이가 약 9.6px와 4.0px로, 둥근 부분이 낮아진다. 막대 높이를 실제 점수보다 부풀리는 표시 변경은 적용하지 않았다. [오른쪽 끝 1:1 크롭](Evidence/ResultsPositionReview/podium-edge-native.png) · [3인·4인 시상 비율](Evidence/ResultsPositionReview/podium-aspect-samples.jpg) · [저점수 표본 전후](Evidence/ResultsPositionReview/low-scores-at-720p.jpg)

## 빌드와 자료

실제 3인 경기 호스트의 1080p 백버퍼는 552장, 33.986초, 평균 16.21fps였다. 대조 캡처의 위치는 렌더 프레임과 측정 위젯 기하를 함께 확인했다. 4인은 합성 결과를 표시해 검증했다. 전후 비교 전용 위젯에서 발생한 객체 이름 충돌은 인스턴스마다 이름을 생성하도록 고친 뒤 다시 캡처했다. [실제 시상 세 창](Evidence/ResultsPositionReview/three-live-podium.jpg) · [위젯 기하](Evidence/ResultsPositionReview/layout-geometry.csv) · [PIE 검증 로그](Evidence/ResultsPositionReview/verification.txt) · [렌더 프레임 로그](Evidence/ResultsPositionReview/render-frames.csv)

UE 5.8.4에서 WBP 세 개를 컴파일했다. 임시 캡처 소스와 비교용 WBP 복제본을 제거한 뒤 `MWEditor Win64 Development` 빌드도 통과했다. 해당 바이너리는 로컬 콘텐츠 작업 공간에 있고 코드와 검증 자료는 Git 대상이다. UVCS 체크인, 푸시, 병합, 패키징은 보류했다.

| 로컬 WBP | 최종 SHA-256 |
| --- | --- |
| `Content/UI/Results/WBP_MatchResults.uasset` | `DBFACBE7768CDF110A6B1B442271E33938F3C5DD8B77619784B3DE18FBF60D7B` |
| `Content/UI/Results/WBP_MatchScoreColumn.uasset` | `452A1FC8909E9AA7E84D14F988D8698B50C851965B150E81ED72F4F2B4A8FEBB` |
| `Content/UI/Results/WBP_MatchPodiumCard.uasset` | `D5B94096BAFE06D0D0674EAA4C4A13E74FB9D25195CBBAD72733D0CF7B96EFB4` |

`Saved/CheckpointResultsReference/results-prep/layout-position-reference-preview.jpg`와 `ResultsPositionReview/initial-position-review.jpg`는 제공된 원본과 이 문서의 최종 대조 캡처로 대체해 정리했다. 오른쪽 띠가 늘어난 중간 증거는 1:1 크롭에 보존했다.
