# 점수 막대 테두리와 전체 경기 표시 검증

Client UI 작업이다. `feature/match-results-tally@63d3a6f`와 현재 로컬 WBP를 사용자 제공 Switch 영상과 대조했다. Cua Driver로 Listen Server PIE의 호스트와 클라이언트 두 창을 조작하고 캡처했다. 비교용 4열의 점수와 3,000점 표시 범위는 임시 표시 위젯에만 넣었다. 실제 3인 경기는 기존 4,000점으로 진행했다.

## 테두리의 시작과 두께

막대의 선수색이 처음 분명하게 보이는 픽셀에서 안쪽으로 읽었다. 좌우 변은 글자가 없는 행의 RGB 중앙값, 윗변은 막대 중심 부근 열의 RGB 중앙값을 사용했다. 선수색을 대표하는 RGB 채널에서 채움과 어두운 띠의 중간값으로 두께 경계를 정했다. 원본 압축과 경계 보간에 따른 오차는 약 1px이다.

| 표본 | 색 가장자리 폭 | 진한 띠 폭 |
| --- | ---: | ---: |
| Switch 1920×1080, 빨강 왼쪽, F29390·F30020 | 4px | 5~6px |
| Switch 1920×1080, 빨강·파랑·주황, 좌우·윗변 | 2~4px | 5~7px |
| MW 호스트 1920×1062, 4열 좌우·윗변 | 4px | 6px |
| MW 클라이언트 1280×720, 4열 좌우·윗변 | 2~3px | 4px |
| MW 클라이언트 800×482, 4열 좌우·윗변 | 1~2px | 3px |

원본 빨강 F30020에서 첫 선수색 픽셀은 x362, 진한 띠는 x366부터다. 로컬 호스트의 해당 픽셀은 x377과 x381이다. 띠의 폭과 안쪽 여백이 원본 관측 범위에 들어가므로 `WBP_MatchScoreColumn → TallyFillOutline`의 4 디자인 단위 안쪽 여백과 6 단위 브러시를 유지했다. 원본은 선수색마다 다른 어두운 계열이고 현재 WBP는 공통 어두운 브러시를 사용한다. [1px 단면 확대](Evidence/FullMatchPresentation/red-stroke-pixels.png) · [대조 RGB 전체](Evidence/FullMatchPresentation/edge-profiles.csv)

## 3·4열 배치

`WBP_MatchResults → TallyWidthByPlayerCount`의 3인 914→900, 4인 1220→1190을 적용했다. `WBP_MatchScoreColumn → ColumnFrame` 좌우 패딩은 각각 5→2 디자인 단위로 줄였다. 판 크기와 바닥 기준은 유지했다.

| 뷰포트 | 4열 사이 | 판 왼쪽부터 첫 열 | 판 바닥과 막대 바닥 |
| --- | ---: | ---: | ---: |
| Switch 1920×1080 | 약 3px | 약 11% | 5px |
| MW 호스트 1920×1062 | 9.8→3.9px | 10.2→11.0% | 5.0px |
| MW 클라이언트 1280×720 | 6.7→2.7px | 10.2→11.0% | 3.4px |
| MW 클라이언트 800×482 | 4.4→1.8px | 10.2→11.0% | 2.2px |

호스트에서 3열 사이도 약 4px이며 3열 그룹은 판 중앙에 정렬된다. 원본/호스트에서 흰색 글자 픽셀의 제목 높이는 78/80px, 1위 숫자는 100/103px, 등수 접미어는 50/41px였다. 제목의 판 상단 여백은 34/28px다. 원형 테두리 지름은 원본 약 88px, 호스트 WBP 슬롯 86.5px다. 제목과 접미어 높이 차이는 별도로 기록했고 글꼴·판 브러시·차량 이미지는 유지했다. [세 창 전후](Evidence/FullMatchPresentation/layout-before-after.jpg) · [판 좌표와 글자 실측](Evidence/FullMatchPresentation/board-geometry.csv)

## 실제 4,000점 경기

기존 로비·건설 승인·복제·점수 판정을 사용하는 3인 경기를 7라운드까지 실행했다. 2라운드 호스트의 첫 체크포인트는 차량을 입구에 놓은 다음 스로틀 입력으로 통과했다. 통과 직후 의도적으로 차량을 떨어뜨려 기존 복귀를 실행했고 조작 복원까지 약 0.83초였다. 나머지 지점과 결승은 차량을 실제 트리거에 놓아 기존 판정을 받았다. 전 코스를 수동으로 운전한 검증과 구분한다.

프로젝트 렌더 프레임 ID와 원본 사건을 [프레임 대조표](Evidence/FullMatchPresentation/rendered-events.csv)에 남겼다. 일반 통과는 F17 부근, 복귀 시작과 조작 복원은 F47/F71 부근이다. 원본의 일반 통과 사례에는 게이트 구조와 체크무늬가 차량과 함께 보인다. 이번 PIE의 선택된 구간에서는 같은 세 요소가 한 구도에 잡히지 않았다. 복귀 프레임에는 차량 주위 원과 흰 슬롯이 보인다. 원본과 동일한 화면 플래시·파편 타이밍은 확인하지 못했다. [통과·복귀·집계 캡처](Evidence/FullMatchPresentation/checkpoint-to-results.jpg)

2라운드에서 완주 점수가 올라가는 동안 파랑·초록 열의 색 경계가 빨강보다 먼저 멈췄다. 안정된 색 경계에 처음 닿는 프레임은 초록 F146, 파랑 F153, 빨강 F161이었다. 누적 순위 F183부터 F196까지 세 열의 좌표가 유지됐다. 다음 건설 상태에 가까운 F198 앞에 921ms 렌더 간격이 있어 판 퇴장의 단일 프레임 튐은 판정할 수 없다. [채움 높이 원시값](Evidence/FullMatchPresentation/tally-fill-motion.csv)

7라운드에 4,300/3,250/2,550점이 세 창에 복제됐고, 서버 최종 순위가 1/2/3위로 확정됐다. 호스트에서 누적 순위 → 결승선 카메라 → 1위판 → 세 순위판이 보였다. 카메라 접근에서 1위판 상태까지 약 2.36초, 1위판에서 나머지 순위판 상태까지 약 0.62초다. Switch 영상은 집계판 제거 F30152부터 1위판 F30304까지 약 2.53초이고 2·3위판 표시는 각각 F30316·F30328이다. 동기 프레임이나 같은 레이스는 아니다.

1280×720 클라이언트의 정착 캡처를 1920×1080 좌표로 환산하면 메달 중심은 약 (366,269)/(478,600)/(590,863), 지름은 201/167/151px다. 원본 약 (386,244)/(458,618)/(503,865), 254/203/166px와 차이가 있다. 1위판의 기울기는 약 −7°로 원본 약 −10°보다 완만하다. 1위 차량은 정착 캡처에서 보인다. 원본의 상단 잘림, 메달의 첫 진입과 이름 확대를 프레임 단위로 같은 속도라고 판단할 수 없다. 이번 WBP 변경 범위는 점수판의 열 배치다. [최종 단계 캡처](Evidence/FullMatchPresentation/final-continuous.jpg) · [최종 시상 세 창](Evidence/FullMatchPresentation/final-three-clients.jpg) · [세 창 경기 단계/점수 로그](Evidence/FullMatchPresentation/match-events.jsonl)

### 연속 프레임 자료

Editor의 호스트 백버퍼가 실제로 그린 1920×1062 프레임이다. 복귀·집계 세션은 694장 평균 21.68fps, 최종 시상은 744장 평균 26.56fps다. 큐 누락과 연속 동일 CRC는 두 구간 모두 0이다. 60fps 비교는 미완료다. 프레임 사이의 단일 프레임 겹침이나 넘침까지 배제할 수 없다. 재생용 WebP는 실제 간격을 유지하고 960×531로 축소했다.

| 구간 | 로컬 원시 프레임·시각 장부 | 로컬 재생용 파일 |
| --- | --- | --- |
| 체크포인트·복귀·집계 | `Saved/Screenshots/RaceRendered/20261127-141952` | `Saved/CheckpointResultsReference/FullMatchPresentation/StrokePixels/checkpoint-rendered.webp` |
| 최종 집계와 시상 | `Saved/Screenshots/RaceRendered/20261127-142125` | `Saved/CheckpointResultsReference/FullMatchPresentation/StrokePixels/final-rendered.webp` |

### 로딩과 수명

첫 맵 이동의 가림→공개는 세 창 각각 4.19초, 4.19초, 4.36초였다. 차량 텍스처는 약 2초 뒤 기존 품질 조건의 대체 경로로 진행했고, 준비→공개는 약 24~27ms였다. 동일 PIE 프로세스의 첫 이동 기록이다.

라운드를 반복할 때 각 창에서 결과판 1개와 결과 카메라 1개가 표시됐고, Racing에서 각각 0개로 돌아갔다. 뷰포트에서 분리됐지만 아직 수집되지 않은 결과 위젯도 세었다. 호스트는 검증용 위젯까지 포함해 2라운드 결과 때 6개, 3라운드 Racing에서 1개를 기록했다. 호스트 Racing 시점 프로세스 사용 물리 메모리는 2라운드 약 3.77GB, 7라운드 약 3.09GB였다. 같은 에디터 프로세스의 수치이며 코스·프레임 캡처 비용도 포함한다. `ClearPresentation()`의 타이머·카메라·차량 정리와 `RemoveOverlay()`의 델리게이트 해제 경로를 확인했다. 독립 프로세스 재접속과 장시간 힙은 측정하지 않았다. [로딩 및 생명주기 측정값](Evidence/FullMatchPresentation/pie-timeline.txt)

## 로컬 에셋 인계

| 에셋 | SHA-256 |
| --- | --- |
| `Content/UI/Results/WBP_MatchResults.uasset` | `5F2CE1F9C5541F0D5AEB4872A439EA8F5DA269D7F48FF2D2C929E6D46221FEAD` |
| `Content/UI/Results/WBP_MatchScoreColumn.uasset` | `4AF3921D575BA405FBD23E6CD7B01B9688F71025FC2A3792088BEBA398A9073C` |
| `Content/System/BP_MWGameplayGameMode.uasset` | `DC4C06640563F99B7EC4D36C3B21B8E8589CCCFCCAEF93A2A0B5D5249A4888EC` (이전과 동일) |

WBP 2개의 컴파일·저장과 검증 도구를 소스 트리 밖으로 옮긴 뒤의 `MWEditor Win64 Development` 빌드를 확인했다. 두 WBP는 로컬 UVCS 작업공간에 있으며 Git 커밋에는 검증 자료를 기록했다.
