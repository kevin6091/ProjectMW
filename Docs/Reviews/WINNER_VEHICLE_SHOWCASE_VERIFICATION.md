# 누적 1위 차량 컷신과 결과판 검증

`feature/match-results-tally`의 `b1b927b` 이후 Client 연출 변경이다. 출발·점수·경기 전환은 기존 서버 경로를 사용한다.

## 편집 위치

| 위치 | 변경 내용 |
| --- | --- |
| `MWWinnerVehicleShowcase.h` · `.cpp` | 결승 카메라에 맞춰 우승 차량의 정적·스켈레탈 메시를 그리는 로컬 액터다. 각 키의 화면 상대 위치·보이는 크기·자세를 월드 공간으로 환산한다. 카메라와의 거리를 바꾸면서 기존 차량 VFX를 재사용한다. 원격 템플릿은 BP 부모와 네이티브 부품, 소켓 변환까지 합성한다. 액터에 주행·충돌·복제 기능은 없다. |
| `MWMatchResultsWidget.h` · `.cpp` | 확정 결과의 `FinalRank==1`과 멤버 ID로 우승 차량을 선택한다. 주행 Pawn이 보이는 뷰는 현재 외형을, 원격 Pawn이 없는 뷰는 선택한 차량 클래스의 메시 템플릿을 사용한다. 외형 클래스가 비상주일 때는 비동기로 준비한다. 3D 차량이 보이는 동안 1위 카드 썸네일을 숨기며, 외형 준비에 실패하면 기존 카드로 표시한다. 위젯 종료 시 액터와 로드 핸들을, 액터 종료 시 시점별 숨김과 효과를 정리한다. |
| `WBP_MatchResults` | `WinnerVehicleShot`에서 컷신 시작 시점, 카메라 상대 키, 효과 구간을 편집한다. 1·2·3위 앵커와 제목 설정도 WBP가 소유한다. 집계 배경은 코너와 중앙을 나눠 그리는 박스 브러시를 쓴다. |
| `WBP_MatchScoreColumn` | 88 단위 배지를 유지한다. 배지와 점수 행은 막대 좌상단으로 옮기고 가장 낮은 열에서도 배지가 판 밖으로 나오지 않게 최소 높이를 조정했다. 등수 글꼴과 `위` 슬롯을 함께 조정했다. |
| `WBP_MatchPodiumCard` | `PlateAspectExtent`에서 695:126의 디자인 비율을 유지한다. 초기 9-slice 테스트에서 기존 텍스처의 안쪽 테두리가 넓어지는 것을 확인해 이미지 비율 유지로 전환했다. 2·3위 썸네일 높이도 이 슬롯에 맞췄다. |

## 검증 조건

UE 5.8.4 `MWEditor Win64 Development`로 Cua Driver 3인 PIE를 실행했다. 실제 경주 경로를 짧게 검증하기 위해 PIE 한정 목표 점수를 550으로 두고, 현재 설치 처리로 코스를 만들었다. 호스트는 체크포인트 앞 지면에서 전진 입력을 받아 주행했다. 복귀와 결승 판정의 재현에는 테스트용 차량 위치 지정이 포함된다. 수동 전체 랩 검증과는 범위가 다르다.

원본 비교에서는 차량의 접근, 회전, 카드 공개, 마지막 자세를 순서대로 관측했다. ProjectMW는 기존 결승 카메라와 차량을 유지하며 다른 궤적과 구도로 연출한다.

## 최종 빌드의 영상과 실측

마지막 라운드 선착 보너스 150점은 P2·DOUG가 받았다. 최종 누적 순위는 P1·FRED 550점, P2 450점, P3 300점이었고 세 창의 3D 우승차는 P1·FRED였다. 최종 녹화에서는 세 창에 주행 Pawn이 남아 있었고, 원격 Pawn이 보이지 않았던 앞 실행에서는 두 클라이언트가 같은 FRED의 클래스 템플릿으로 차체와 바퀴를 조립했다. 결과 복제로 최종 위젯이 집계를 다시 그리는 경우에는 가산과 누적 순위가 끝난 뒤 차량이 들어오도록 순서를 연결했다.

| 장면 | 원본 프레임 | ProjectMW 1920×1080 실측 |
| --- | --- | --- |
| 체크포인트 | F33829, 차량이 체크무늬에 진입 | #40 부근 실제 전진 입력으로 서버 체크포인트 통과 100점. 이번 랜덤 코스의 해당 체크포인트에는 화면에서 구분되는 체크무늬가 없어 판정 시점으로 장면을 맞췄다. |
| 복귀·가산·누적 순위 | 복귀 F8260, 첫 가산 F29164, 순위 F29998 | P1·P2의 복귀와 입력 회복을 기록했다. 가산 뒤 점수와 열 위치를 유지하며 순위를 공개했다. 이 비교는 순서를 맞춘다. |
| 우승차 접근 | F30200 전후 | #198의 투영 중심 `(1056,462)`, AABB 약 108×145px, 카메라 기준 롤 0.1°다. |
| 공중 회전과 판 진입 | F30260–30312 | #213의 투영 중심 `(1180,278)`, AABB 약 395×401px, 롤 167.2°다. 1위판이 보이는 동안 차량이 회전한다. |
| 우승차 정착 | F30320–30352 | #220의 투영 중심 `(1279,302)`, AABB 약 324×360px, 롤 19.0°다. 실제 화면에서 1위판 옆의 차량과 나머지 두 카드의 썸네일을 확인했다. |

AABB는 3D 차량의 월드 경계를 화면에 투영한 값으로, 화면에서 보이는 실루엣 크기와 각도는 [연속 장면](Evidence/WinnerVehicleShowcase/winner-timeline.jpg)에서 비교할 수 있다. [세 창 캡처](Evidence/WinnerVehicleShowcase/winner-three-windows.jpg) · [원본과 실행 비교 MP4](../../Saved/CheckpointResultsReference/WinnerShowcase/winner-showcase-comparison.mp4) · [전체 실행 MP4](../../Saved/CheckpointResultsReference/WinnerShowcase/winner-showcase-gameplay.mp4)

게임이 실제 렌더한 1920×1080 백버퍼는 **365장**, 캡처 구간 **33.947초**, 평균 **10.72fps**다. 요청 번호 누락, 저장 큐 유실, 인접 동일 CRC는 각각 0건이다. 두 MP4는 실제 타임스탬프 간격으로 프레임을 배치한 30fps 무음 재생본이다. 원본 60fps와의 프레임 단위 타이밍 일치는 이 계측 범위를 넘는다. [원시 프레임 로그](Evidence/WinnerVehicleShowcase/render-frames.csv) · [우승차 기하](Evidence/WinnerVehicleShowcase/winner-pose.csv) · [비교 구간 정렬표](Evidence/WinnerVehicleShowcase/comparison-frames.csv)

| 뷰포트 | 점수판 하단 y | 막대 하단 y | 배지 왼쪽 여유 | 배지 지름 |
| --- | ---: | ---: | ---: | ---: |
| 1920×1080 | 977.0 | 972.0 | 16.0px | 88.0px |
| 1280×720 | 650.6 | 647.3 | 10.7px | 58.6px |
| 800×482 | 435.2 | 433.0 | 7.2px | 39.2px |

1080p #175의 글리프 높이는 제목 79px, 등수 97–98px다. 원본 대조값은 약 77px와 100px다. 이전 86.2px였던 배지 왼쪽 여유를 16px로 줄였다. 메달 중심은 `(378,309)`, `(532,608)`, `(629,845)`였다. 지름은 각각 180px·147px·132px이며 원본보다 작다. [누적 순위 1080p](Evidence/WinnerVehicleShowcase/score-positions.jpg) · [최종 PIE 로그](Evidence/WinnerVehicleShowcase/pie-verification.txt)

## 로딩과 반복 종료의 별도 계측

로딩 텍스처 품질 대기는 최종 녹화에서 세 창 모두 2.04초였다. 이동 월드의 최초 진입과 텍스처 품질 요청을 함께 운영하는 화면은 별도 검증 항목으로 남겼다. 이번 우승 차량의 부품 클래스는 기존 카탈로그를 조회하며 필요할 때 비동기로 준비한다. [별도 프로세스 로딩·메모리 기초 점검](../../Saved/CheckpointResultsReference/b1b927b-followup-audit.md)

원격 부품 조립 수정 직후의 빌드에서 같은 Editor 프로세스(PID 25576)로 3인 경기→시상→PIE 종료를 두 번 실행했다. 두 번째는 연속 프레임 저장을 끈 상태였다. 종료 직후와 명시적 GC 이후에 PIE 경로가 남은 결과 위젯·결과 행·HUD·카메라·우승차·Niagara 컴포넌트는 두 회 모두 0개였다.

| `memreport -full`, GC 이후 | 첫 종료 | 두 번째 종료 |
| --- | ---: | ---: |
| 프로세스 물리 메모리 | 4696.14 MB | 4903.12 MB |
| RHI transient 사용량 | 128.000 MB | 128.000 MB |
| Slate 위젯 할당 | 30.385 MB | 30.543 MB |
| Slate GPU 텍스처 | 37.606 MB | 37.606 MB |
| 메모리 리포트의 `Texture2D` | 360 | 361 |

두 실행의 코스 후보가 달랐다. 두 번째 스냅샷에 추가된 트랙 텍스처 `T_Scrolling_08a5cf32`의 상주량은 8KB였다. D3D 시스템·비디오 메모리와 정적 메시 메모리도 각각 약 54.7MB, 29.1MB, 9.5MB 증가했다. 전체 물리 메모리 증가 207MB의 소유자를 확인하려면 같은 코스에서 반복하고 할당을 추적해야 한다.

## 빌드와 에셋 인계

검증용 빌드의 UE 모듈 BuildId는 `55116800`이며 `UnrealEditor-MW.dll` SHA-256은 `3E2F15ABC5BD6011E537EB1D5F4F8317C8DF7218883E38899E1174DB24311E4D`다.

| 로컬 에셋 | SHA-256 |
| --- | --- |
| `Content/UI/Results/WBP_MatchResults.uasset` | `F47FC62C355D3045B52681092CDB2DD4C0C67E769039A6EFCC9E682BC7DB3B31` |
| `Content/UI/Results/WBP_MatchScoreColumn.uasset` | `896511FE3432E818D78B566AEDD7DEE8E77F4D4CB94D33A5D91741969D4E618C` |
| `Content/UI/Results/WBP_MatchPodiumCard.uasset` | `C30101BDC62248C5B656580468B072EB7032357C643249A1BC91A08DA3D911B2` |

세 WBP를 각각 컴파일했고 임시 계측 코드를 제거한 Editor 빌드도 통과했다. 파일은 UVCS 작업 공간에 로컬 저장되어 있고 Git 변경에는 C++·문서·비교 캡처가 포함된다. 원본 자료는 사용자가 제공한 영상과 참조 폴더에서 대조했다. 임시 중복 로그 `winner-showcase-template-flat-events-archive.jsonl`은 해시가 같은 `winner-showcase-template-flat-events.jsonl`을 유지하고 정리했다.
