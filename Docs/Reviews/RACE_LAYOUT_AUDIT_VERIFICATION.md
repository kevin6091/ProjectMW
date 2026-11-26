# 경기 집계판·시상·복귀 화면 검증

기준은 `feature/match-results-tally`의 `c2857d9`다. Client 분야의 `UI_AUTHORING_GUIDELINES_V2.md`에 따라 WBP는 텍스트·색·크기·앵커를 편집하고 C++는 기존 경기 상태를 읽어 애니메이션과 카메라 투영을 갱신한다.

## 수정 위치와 편집값

| 위치 | 수정 내용 |
| --- | --- |
| `WBP_MatchResults` | `TallyStack`·`TallyFit`에서 판 여백과 제목을 편집한다. `RankLayout`에서 1·2·3위 슬롯의 상대 앵커를 각각 정한다. `MedalEntryHeightFractions`의 금·은·동 높이 비율은 `-0.11`, `0.12`, `0.19`다. |
| `WBP_MatchScoreColumn` | `VerticalMeterExtent` 하단 패딩을 75에서 4로 줄이고 순위 글자와 원형 배지를 키웠다. `MarkerMinimumFill`은 점수가 낮아도 배지가 판 아래로 내려가지 않게 한다. 점수 막대와 숫자는 실제 누적값을 사용한다. 참가자가 늘면 기존 가로 `Fill` 슬롯에 동일 비율로 열이 추가된다. |
| `WBP_MatchPodiumCard` | 메달과 이름판을 별도 슬롯에 두고 썸네일 슬롯을 키웠다. 카드의 추가 점수 텍스트는 숨겼다. 기울기는 ProjectMW의 약 5.5°를 사용한다. |
| `WBP_MatchPanels` | `RecoveryPanel`·`RecoveryBadge`·`RecoveryCountdown`·`RecoveryFlash`에서 화면 위치·배경·폰트를 편집한다. |
| `MWMatchResultsWidget.h` · `MWMatchResultsWidget.cpp` | 기존 WBP의 수직 목록도 지원한다. 신규 WBP에서 순위별 슬롯에 카드를 배치하고 화면 왼쪽 경계와 카드 폭을 사용해 진입 시작점을 구한다. 메달·판·이름·썸네일은 각자의 진행률로 움직이며 금메달은 우하향, 은·동메달은 우상향하며 끝에서 감속한다. |
| `MWGameplayHUDSubsystem.h` · `MWGameplayHUDSubsystem.cpp` | 복귀 대기 중인 소유 플레이어의 WBP를 갱신한다. 투영 중심은 차량 충돌 박스 하단 네 꼭짓점의 평균이다. `CheckpointRecoveryLayout`이 고리 반경과 슬롯 파편 시간을 소유한다. 카운트다운과 화면 플래시는 WBP, 차량을 따라 움직이는 고리와 흰 슬롯 파편은 C++에 있다. |

## 검증 방법

UE 5.8.4로 `MWEditor Win64 Development`를 빌드하고 Cua Driver로 리슨 서버와 클라이언트 2개를 실행했다. 랩을 짧게 재현하기 위해 PIE의 승리 점수만 550으로 두고 실제 설치·경주 네트워크 경로를 사용했다. 낙하는 테스트가 시작 위치를 코스 아래로 옮겨 유도했다. 호스트의 백버퍼를 Unreal `FFrameGrabber`로 렌더된 순서대로 저장하고, 프레임 요청 번호·UTC·원시 픽셀 CRC를 기록했다. 게임 렌더 위치를 측정할 때는 캡처 창 테두리를 제외하고 엔진의 `LocalToViewport`로 변환했다.

## 3인 창의 최종 배치

라운드 2의 동일한 누적 순위 데이터(P1 500점, P2 800점, P3 300점)로 측정했다. 좌표는 각 뷰포트 좌상단에서 시작한다.

| 뷰포트 | 판 `(x1,y1)–(x2,y2)` | 열 하단 y | 판과 열의 간격 | 제목 슬롯 높이 | 등수 슬롯 높이 | 배지 지름 |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| 1920×1080 | `(244.0,157.5)–(1724.0,955.5)` | 950.5 | 5.0px | 143.0px | 131.0px | 88.0px |
| 1280×720 | `(163.1,105.9)–(1148.8,636.1)` | 632.8 | 3.3px | 94.0px | 87.2px | 58.6px |
| 800×482 | `(80.8,70.3)–(740.6,426.4)` | 424.1 | 2.3px | 64.0px | 58.4px | 39.2px |

첫 조정에서 `VerticalMeterExtent` 하단 4, `TallyFit` 패딩 24, `TallyStack` 하단 29를 적용했을 때 1920×1080 간격은 25px였다. 이 상태에서 `TallyStack` 하단을 9로 줄여 5px를 실측했다. 원본 판과 열은 각각 y999와 y994에서 끝난다. ProjectMW 판은 원본보다 가로 56px 좁고 하단이 43.5px 위에 있다. [집계 위치와 바닥](Evidence/RaceLayoutAudit/04-positions.jpg) · [실측 로그](Evidence/RaceLayoutAudit/pie-verification.txt)

1920×1080 프레임 #272의 글리프 영역 안에서 밝은 글자 픽셀만 추출했다. 제목은 약 91px, 등수 숫자는 약 80–81px였다. 원본의 77px와 100px에 비해 각각 14px 크고 약 19–20px 작다. 원형 차량 배지는 88px였다. 작은 두 창의 수치는 렌더된 위젯 슬롯으로 측정했다. 4인에도 동일한 슬롯 배분 코드를 사용하며 이번 실행의 표본은 3인이다.

## 다섯 장면의 시간 비교

| 장면 | 원본 기준 프레임 | ProjectMW에서 관측한 동작 |
| --- | --- | --- |
| 일반 통과 | 한국어 영상 F33829에서 차량이 체크무늬에 들어가고 주행 카메라가 따라간다. | 호스트 차량을 트리거 중심 13.70m 전 지면에서 준비한 뒤 전진 입력을 넣었다. 트리거 통과로 100점이 적립됐다. #33→46에서 차가 흰 월드 슬롯 사이를 전진하고 #54에서 체크무늬 위에 들어가 #58에서 벗어난다. 비교표에서 #54를 원본 F33829와 맞췄으며, ProjectMW의 점수 기록은 #54보다 약 0.72초 앞선다. 통과에 복귀 HUD가 뜨지 않았다. |
| 체크포인트 복귀 | Switch F8260 하단 표식, F8268–8284 주황 화면 효과, F8320–8332 흰 슬롯 파편. | #136 낙하, #138 복귀 표식·고리·슬롯, #152 수축·파편화 말기, #156 주행 복구. 청록색 상단 효과와 차량 하부 고리를 사용한다. P1·P2가 각각 약 0.83초와 0.84초 뒤 주행 권한을 되찾았다. 일반 주행부터 복귀까지 각 창의 차량 시점이 유지됐다. |
| 가산 | Switch F29109 라운드 제목, F29164 체크포인트 가산 시작. 첫 열의 변동은 약 0.70초. | #249 라운드 제목, #251 첫 가산 관측, #252 증가 중, #254 지급량 정착. `완주 점수`에서 50점 지급 열의 300점이 먼저 고정됐다. 같은 표본에서 다른 두 열은 411점과 561점이었고 약 0.30초 뒤 500점과 650점으로 정착했다. |
| 누적 순위 | Switch F29998 순위로 전환, F30152 판이 사라짐. 약 2.57초. | #268 보너스, #270 순위 처음 관측, #272 열과 값 유지, #274 결승 카메라로 넘어감. 전환 직전과 직후의 열은 모두 500·800·300점이다. 이 실행에서 가산판 전체는 약 2.90초, 순위 유지 상태는 약 0.53초였다. |
| 시상 | Switch F30296 메달 일부, F30304 1위판, F30316 2위판, F30328 3위판. | #322 결승 카메라, #324 1위 진입, #326 2위 진입, #328 3위 진입, #330 이후 정착. 세 창 모두 P2·P1·P3 순위로 유지됐다. 로그 최초 표본으로는 2위와 3위가 1위 기준 약 +0.30초와 +0.48초였다. 카메라는 결승 소켓을 기준으로 움직이며 이 코스에서는 체크무늬와 골조를 위쪽 각도에서 담았다. |

메달의 정착 중심 `(x,y)` 및 지름은 1920×1080에서 1위 `(378,333)`/180px, 2위 `(574,609)`/142px, 3위 `(700,845)`/126px다. 원본의 `(386,244)`/254px, `(458,618)`/203px, `(503,865)`/166px와 비교해 세 순위를 아래쪽으로 펼치고 가로 간격을 더 벌린 배치다. 판 밖에서 경로를 시작하고 메달마다 이동 높이를 따로 계산한다. 판의 점수는 숨겼고 ProjectMW 차량 썸네일은 화면 안에 온전히 표시한다. 앞 작업에서 제거한 계속 버튼과 경기 개요는 숨김 상태다. 이번 시상은 대기 상태로 남으므로 최종 카드 퇴장 시각은 없다.

**실제 렌더 수집은 평균 14.81fps였다.** 25.924초에 1920×1080 이미지 385장, 인접 원시 CRC 동일 0건, 요청 번호 누락 0건, 저장 큐 유실 0건이다. 프레임 간격 중앙값은 57ms, 최대 282ms였다. `60fps` 제한을 설정한 3인 PIE였으며 실제 렌더 속도는 이보다 낮았다. 원본과 사건별 순서를 비교했고, 한 프레임 단위의 타이밍 일치나 개별 메달 감속의 정밀 실측은 남아 있다. 원본의 약 17.38초 집계와 게임의 3초 결과 제한 차이도 남아 있다. 서버 시간과 전환 변경은 이번 범위에서 보류했다.

[다섯 장면 비교표](Evidence/RaceLayoutAudit/five-scenes.jpg) · [장면별 실제 프레임 번호](Evidence/RaceLayoutAudit/scene-frames.csv) · [전체 렌더 CRC](Evidence/RaceLayoutAudit/render-frames.csv) · [클라이언트 상태](Evidence/RaceLayoutAudit/client-events.jsonl) · [로컬 연속 프레임 385장 ZIP](../../Saved/CheckpointResultsReference/race-layout-rendered-sequences.zip)

## 로컬 WBP와 빌드 식별자

| 에셋 | 검증한 로컬 SHA-256 |
| --- | --- |
| `Content/UI/Results/WBP_MatchResults.uasset` | `532DB62ADF3BCBE0AAEDC4681F59B560BEAEFA963F108EACFC9B649C5E06D20A` |
| `Content/UI/Results/WBP_MatchScoreColumn.uasset` | `F8C7308AC96DCBE617F86D71AF2271B678DC0B67A3EFF95BFAA763667D766FD0` |
| `Content/UI/Results/WBP_MatchPodiumCard.uasset` | `7626CDC5E58AA52147C20AEDAA847EE6C43810D8CC55588EC00CDC5EF7C18E83` |
| `Content/UI/Race/WBP_MatchPanels.uasset` | `ACEDAD0AF61712270EF45B5CBBF0BC41D7F30AB17FB9DE568B8046F6011A1B9F` |

UE 모듈 BuildId는 `55116800`이다. 캡처를 생성한 검증용 `UnrealEditor-MW.dll` SHA-256은 `7E49EBD662F2D49FF88FF35C6D7CBBE42D6BCB581D6B78B5CCC4F6E88E1E65CF`다. 네 WBP는 모두 컴파일했으며 로컬 UVCS 작업 공간의 파일이다. `Content`는 Git에서 제외된다. 위젯 사양과 코드를 커밋해도 바이너리 인계는 별도다. 임시 검증 코드를 제거한 `MWEditor Win64 Development` 빌드도 통과했다.

첫 조정의 `Saved/Screenshots/RaceRendered/20261126-071647`과 `Saved/CheckpointResultsReference/results-prep/first-rendered-sequence`, `first-rendered-contact-sheet.jpg`는 최종 실측과 연속 프레임이 대체해 정리했다. 첫 조정의 배치 수치 25px는 위에 기록했고 실행 로그는 유지했다.
