# 최종 집계·시상·체크포인트 복귀 검증

`feature/match-results-tally`의 Client UI 작업이다. 시작점은 [7569f65의 실측 기록](FULL_MATCH_PRESENTATION_VERIFICATION.md)이다. 같은 종류 사건을 기준으로 원본 영상과 프로젝트 프레임을 각자의 실제 경과시간으로 맞췄다. 원본은 60fps, 프로젝트는 3인 Listen Server PIE의 호스트 백버퍼를 사용했다.

## 편집 위치

| 소유 위치 | 변경 |
| --- | --- |
| `UMWMatchResultsWidget::BeginTally`, `EndTally` | 일반 라운드는 서버가 복제한 종료 시각까지 집계를 압축한다. 최종 결과가 예상되는 마지막 라운드는 초기 점수와 집계 대기를 표시한다. 실제 `MatchResults`를 받은 뒤 최종 집계와 순위 공개를 한 번 시작한다. 일반 라운드에서 둘 이상이 기준 점수에 도달한 경우에는 기존 집계를 유지하며 서버의 다음 상태를 기다린다. |
| `WBP_MatchResults → FinalMaxTallySeconds / FinalPositionsHoldSeconds` | 최종 집계 상한 16초, 최종 순위 읽기 1.9초. 단계별 시간과 열별 점수는 기존 WBP 설정 및 서버 확정 결과를 사용한다. 작은 점수를 받은 열은 먼저 정착하는 기존 보간 경로를 쓴다. 새 설정이 0인 기존 WBP는 이전 재생 경로를 사용한다. |
| `WBP_MatchResults → PodiumCameraKeys / WinnerVehicleShot` | 결승 포트 기준 낮은 사선 시점으로 들어간다. 기존 코스 가시성 검사에 따라 이동 배율을 선택한다. 우승 차량은 화면 중앙보다 오른쪽에서 진입하고 상단으로 이동한다. 기존 시상 전용 차량과 기존 재질을 사용한다. |
| `WBP_MatchResults`의 순위별 슬롯과 `WBP_MatchPodiumCard` | 금·은·동의 위치와 진입 높이를 조정했다. 카드 기울기 −7→−8.5°, 메달 렌더 배율 1→1.13이다. plate·이름·차량에 기존 독립 공개 곡선을 사용한다. |
| `WBP_MatchResults → TallySignFace·TallySignLogo`, `WBP_MatchScoreColumn → RankSuffix·TallyOutlineParticipantMix` | 제목이나 막대 기준을 이동시키지 않고 판 윗부분을 늘렸다. 접미사 글꼴은 32→37 디자인 단위다. 어두운 브러시에 참가자색을 9.5% 섞고 기존 투명도·테두리 굵기·안쪽 여백을 보존했다. |
| `AMWVehiclePawn`의 편집 가능한 복귀 카메라 기본값, `DefaultGame.ini → CheckpointRecoveryLayout` | 좀 더 높은 복귀 시점을 잡고, 조작 재개까지 슬롯이 읽히도록 파편 시작을 늦췄다. 복귀 카메라 충돌 검사는 기존 경로로 실행한다. 슬롯과 고리는 월드 좌표를 투영하고, 화면의 선수색 표식과 플래시는 기존 WBP 표시를 사용한다. |

게임 규칙은 기본 4,000점, 서버의 라운드 전환과 복귀 시간, 확정 점수다. 이 작업의 변경은 로컬 표시 경로에 있다. 점수판·시상 WBP는 임시 테스트 코드와 분리해 저장했다.

## 실제 재생 결과

재검증에서 호스트와 두 클라이언트의 뷰포트는 1920×1062, 1280×720, 800×482였다. 3·4열 배치 캡처는 표시 전용 데이터다. 실제 경기는 첫 체크포인트 근처에 차량을 놓고 스로틀로 통과했으며, 곧이어 테스트 낙하를 일으켰다. 나머지 지점은 기존 트리거에 각 차를 놓아 서버에서 주행 점수를 받았다.

세 창에서 7라운드 최종 점수 4,900/3,950/3,250과 1/2/3위가 일치했다. 약 3초의 서버 라운드 결과 동안 `AwaitingFinalResults`를 세 창이 한 번씩 표시했다. 이후 확정된 최종 집계를 공통 순서로 한 번 재생했다. 이 경기는 체크포인트가 8개, 비교 원본은 7개이므로 각 사건의 시작을 따로 맞췄다. [세 창 캡처](Evidence/FinalResultsPacing/cua-three-windows.jpg) · [최종 점수 기록](Evidence/FinalResultsPacing/match-results.jsonl) · [단계 로그](Evidence/FinalResultsPacing/pie-timeline.txt)

| 구간 | 7569f65 | 수정 후 프레임과 경과 | Switch 원본 |
| --- | ---: | --- | ---: |
| 체크포인트 1 → 2 | 0.28초 | F135 → F173, 1.28초 | 1.33초 |
| 완주 점수 → 선착 보너스 | 0.34초 | F437 → F483, 1.67초 | 2.05초 |
| 선착 보너스 → 누적 순위 | 0.36초 | F483 → F542, 1.78초 | 3.12초 |
| 누적 순위 → 결승 카메라 | 약 0.61초 | F542 → F605, 1.90초 | 2.57초 |
| 결승 카메라 → 1위판 | 2.36초 | F605 → F687, 2.46초 | 약 2.53초 |

완주 점수에서 열의 색 경계가 1px 이내에 정착한 시점은 초록 F442, 파랑 F460, 빨강 F462였다. 선착 보너스에서는 빨강만 움직였고 F512에 정착했다. 색 경계는 기존 다섯 자리 점수와 같은 표시값을 사용한다. [프레임 ID](Evidence/FinalResultsPacing/milestones.csv) · [열의 색 경계](Evidence/FinalResultsPacing/finish-fill-motion.csv)

**복귀.** F29는 주행으로 체크포인트를 통과한 프레임 근처다. 테스트 낙하 뒤 F92~F113에서 복귀가 진행됐으며, 약 0.80초에 차량이 다시 조작 가능했다. 높은 시점에서 게이트·체크무늬·차량·슬롯이 함께 보인다. +0.55초의 F107에서도 복귀 가이드가 보인다. F113에는 복귀 가이드가 사라지고 트랙에 원래 그려진 슬롯이 남는다. Switch의 +0.25초 화면 플래시는 더 밝고 여러 차량의 슬롯이 함께 보인다. 검증한 경로는 한 차량의 낙사 복귀다.

**카메라와 시상.** 결승 포트 기준 경로는 현재 코스의 가시성 검사에서 전체 이동량의 35%를 채택했다. 정착 영상에 사선 트랙과 상단에 일부 잘리는 우승 차량이 보인다. 여전히 같은 각도와 이동량이 보장되는 코스는 아니다. 1280×720 클라이언트를 1920×1080으로 환산한 정착 메달 중심은 약 (380,251)/(459,629)/(531,854), 지름은 224/189/173px다. 이전 (366,269)/(478,600)/(590,863), 201/167/151px보다 금·은 크기와 금·은·동 중심이 원본 범위에 가까워졌다. 동메달은 원본 약 166px보다 크다. 배경 가림과 차량 모델 차이가 있는 프레임의 픽셀 비교는 위치·크기 판정에서 제외했다. [같은 경과시간의 연속 장면](Evidence/FinalResultsPacing/same-elapsed-scenes.jpg) · [프레임 비교표](Evidence/FinalResultsPacing/paired-frames.csv)

**판과 테두리.** 호스트의 제목 상단 여백은 27.8→33.7px, 1위 접미사 실제 획은 41→47px였다. 판 바닥 간격은 5px, 4열 사이 간격은 3.9px였다. 호스트에서 선수색 가장자리 4px와 진한 띠 6px가 유지됐다. 띠 안쪽 RGB는 빨강 70/33/39→100/32/38, 파랑 32/50/76→33/57/97, 초록 31/55/49→31/67/50이었다. 공통 어두운 브러시에 선수별 색이 더 뚜렷하게 반영되며 원본과 밝기 차이는 남는다. [판 기하](Evidence/FinalResultsPacing/board-geometry.csv) · [테두리 픽셀 단면](Evidence/FinalResultsPacing/edge-profiles.csv)

### 실제 경과시간 영상

원시 캡처는 1920×1062다. 복귀용 MP4는 140프레임 6.010초, 최종 집계부터 시상까지는 797프레임 25.822초다. 가변 프레임 간격을 보존한 1280×708 영상이며, 디코딩한 모든 프레임의 시각이 순서대로 증가한다. 원시 렌더는 `Saved/Screenshots/RaceRendered/20261127-145648`과 `20261127-145810`에 있고, 영상은 로컬 `Saved/CheckpointResultsReference/FinalResultsPacing/verification/recovery.mp4`, `match-final.mp4`에 있다. 60fps 기준 단일 프레임 타이밍은 이번 측정 범위를 벗어난다.

## 에셋과 배포 범위

| 로컬 에셋 | SHA-256 |
| --- | --- |
| `Content/UI/Results/WBP_MatchResults.uasset` | `86E8A9E4742CAAA7465099DDB9AF3CF56ACCC4030BAD70879A824E61F57211A8` |
| `Content/UI/Results/WBP_MatchScoreColumn.uasset` | `695AEE7CC20CC2A9348BF149A46E7DB825683DC162D56C9CF15E6893F30F4C3D` |
| `Content/UI/Results/WBP_MatchPodiumCard.uasset` | `0C66600C46D04B142610CAAE2CBE8AACD906727E920B3D6AD88944EFBB5826F4` |

첫 실행에서는 새 CDO 설정을 같은 프로세스에서 저장한 직후 재생했다. 제목과 배치가 최신이었지만 새 클래스 기본값은 위젯 인스턴스에 적용되지 않았다. UE의 Blueprint 인스턴스 초기화 목록을 저장본에서 다시 구성하게 에디터를 재시작하고 측정을 반복했다. 비교 증거는 재시작 뒤 실행으로 작성했다. WBP 3개의 저장·컴파일과 임시 검증 코드를 옮긴 뒤의 `MWEditor Win64 Development` 빌드를 확인했다. `BP_MWGameplayGameMode`의 SHA-256은 `DC4C06640563F99B7EC4D36C3B21B8E8589CCCFCCAEF93A2A0B5D5249A4888EC`이다.

에셋은 로컬 UVCS 작업공간에 두고, Git에는 클라이언트 코드·설정·검증 자료를 기록한다.
