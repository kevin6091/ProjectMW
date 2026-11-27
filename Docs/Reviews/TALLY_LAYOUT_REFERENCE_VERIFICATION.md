# 점수판 범위·열 간격·주행 HUD 대조

`feature/match-results-tally@d3a4d7c` 이후 Client UI 작업이다. Switch 영상의 `F29110`, `F29390`, `F30020`에서 같은 표시 판에 찍힌 12개 점수와 막대 윗변을 쟀다. Cua Driver의 3인 PIE에서 동일 데이터를 표시 전용 위젯에 넣은 비교와, 실제 서버 점수 300/150/50점의 정상 라운드 전후를 따로 확인했다.

## 세로 차이의 원인

Switch 표본의 막대 바닥은 Y994, 막대의 전체 영역 위쪽은 Y421 부근이다. 12개 표본의 막대 높이를 점수에 맞추면 절편 102.9px, 기울기 0.15679px/점이다. 관측값의 최대 차이는 1.5px다. 이 식에서 막대 전체를 채우는 점수는 약 2,998점이고, 실제 `F30020`에서 3,000점 열이 그 위쪽 경계에 닿는다. 이는 해당 영상의 **표시 범위**에 대한 추정이다. [원본의 12개 점수·좌표](Evidence/TallyLayoutReview/reference-samples.txt)

기존 비교 캡처는 같은 점수와 프로젝트 기본 4,000점 표시 범위를 사용했다. `SetTallyScore`는 0점 바탕 위에서 점수를 표시 범위로 나눈 진행률을 사용한다. `BarScaleScore`는 복제된 목표와 초과한 최고점 중 큰 값을 사용한다. 목표 조건만 3,000점으로 맞춘 표시 전용 위젯에서는 원본과 막대 높이가 1.2~2.9px 차이다. 판 높이를 분모로 한 비율 차이에는 두 판 높이의 차이도 들어간다.

| 점수 | Switch 1080p | MW 4,000점 범위 · 호스트 | MW 3,000점 범위 · 호스트 |
| --- | ---: | ---: | ---: |
| 3,000 | 573px · 69.5% | 457.9px · 56.6% | 574.2px · 71.0% |
| 2,300 | 463px · 56.1% | 376.5px · 46.6% | 465.7px · 57.6% |
| 2,375 | 475px · 57.6% | 385.2px · 47.7% | 477.3px · 59.0% |
| 2,650 | 518px · 62.8% | 417.2px · 51.6% | 519.9px · 64.3% |

기존 WBP로도 비교용 분모에서 같은 세로 값이 나왔다. 닉네임·등수·배지와 테두리는 채움과 기존 기준선을 공유한다. 0점에서 호스트 바탕은 109px이며, 다섯 자리 점수와 내부 테두리도 유지됐다. 생산 코드의 표시식, 경기 목표, 실제 GameMode 설정은 보존했다. [분모별 4인 캡처](Evidence/TallyLayoutReview/same-score-comparison.jpg) · [세 창 위젯 기하](Evidence/TallyLayoutReview/board-metrics.csv)

## WBP 변경과 실측

| 편집 위치 | 변경과 관측값 |
| --- | --- |
| `WBP_MatchResults → TallyWidthByPlayerCount` | 3인 976→914, 4인 1300→1220 디자인 단위다. |
| `WBP_MatchScoreColumn → ColumnFrame` | 좌우 패딩을 각각 14→5로 줄였다. 4인 호스트에서 첫 열까지 간격은 121.9→152.4px, 판 폭 대비 8.2→10.2%다. 열 사이는 27.5→9.8px다. 원본 표본은 약 170px, 11%, 열 사이 약 3px다. 세 창에서 각 열의 색 바탕과 테두리가 분리돼 보이는 간격을 채택했다. |
| 같은 열 WBP의 `RankLabel`·`RankSuffix` | 기존 Noto Sans KR Bold에 글자색과 같은 외곽선 2/1 디자인 단위를 적용했다. 호스트의 1위 숫자에서 밝은 획 면적은 2,830→3,535px²다. |
| `WBP_MatchResults → TallyHeading` | 글자색과 같은 외곽선 1을 적용했다. 한글 누적 순위가 차지하는 획 너비는 판의 21.3→21.4%, 판 중심과 획 중심 차이는 호스트에서 약 3px이다. 영문 `POSITIONS`와 한글 제목의 글리프·자간을 감안해 가로 배율은 유지했다. |
| `WBP_MatchScoreColumn → TallyBadgeBackground` | 기존 두꺼운 반투명 장식 대신 어두운 안쪽 면과 3 디자인 단위의 밝은 원형 선을 그리는 브러시를 사용했다. 배지 슬롯과 차량 아이콘의 크기는 유지했다. |

## 주행 HUD와 결과 이탈

문제가 보고된 네 열 캡처는 Racing 중에 표시 전용 결과판을 올린 표본이다. 그 아래의 기존 속도계·아이템 슬롯과 색 있는 월드 이름표가 함께 그려졌다. 정상 Listen Server PIE에서 세 창의 1라운드 완주를 재현했다. 차량을 실제 결승 트리거에 놓았고 서버의 기존 판정으로 300/150/50점을 받았다. 기본 4,000점 목표를 사용했다.

세 창에서 `RoundResults`일 때 `Panel_Speed`와 `ItemSlot` 모두 `Collapsed`였다. 색 있는 차량 위 이름표도 결과판에서 사라졌다. 집계 중 막대 위에 보이는 흰색 `256/257/258`은 그 시험 경기의 참가자 이름이다. 다음 라운드 준비와 카운트다운에는 주행 패널이 숨겨졌고, Racing에서 속도계·아이템 슬롯·차량 이름표가 돌아왔다. 정상 경로에서 가림과 복원 문제가 재현되지 않아 HUD 상태 처리 코드는 유지했다.

수정 전 실제 라운드와 수정 후 실제 라운드에서 같은 서버 확정 점수의 결과를 대조했다. Switch의 고점수 네 열은 표시 전용 자료이며, 실제 3인 실행의 동적 경계는 그 실행의 점수로 검증했다. 호스트와 두 클라이언트 1920×1062/1280×720/800×482를 사용했다. 전후의 준비 단계와 실제 점수판 프레임은 캡처에 담겼다.

[같은 서버 점수의 전후와 세 창 복원](Evidence/TallyLayoutReview/normal-round-hud.jpg) · [주행 HUD 상태별 로그](Evidence/TallyLayoutReview/normal-hud-events.jsonl) · [정상 경기 PIE 단계 로그](Evidence/TallyLayoutReview/normal-pie-log.txt)

## 인계

| 로컬 WBP | 변경 후 SHA-256 |
| --- | --- |
| `Content/UI/Results/WBP_MatchResults.uasset` | `2CC1CE01AA8D4097F7BC5EA4A9F7F111FA1AE00A50A5912D97977BF95174A1A4` |
| `Content/UI/Results/WBP_MatchScoreColumn.uasset` | `68D7EC9A24179FA6D1F432C68A95EA902BB98A2CCFAA316F16F80887C85DD200` |

WBP 컴파일·저장과 임시 검증 C++을 제거한 뒤의 `MWEditor Win64 Development` 빌드를 확인했다. GameMode 에셋 SHA-256은 이전과 같은 `DC4C06640563F99B7EC4D36C3B21B8E8589CCCFCCAEF93A2A0B5D5249A4888EC`다. 임시 비교용 WBP는 `Saved/CheckpointResultsReference/TallyLayoutReview`로 옮겼다. 검증 자료는 Git, WBP는 로컬 UVCS 작업공간에 기록했다. Git 푸시와 UVCS 체크인은 보류했다.
