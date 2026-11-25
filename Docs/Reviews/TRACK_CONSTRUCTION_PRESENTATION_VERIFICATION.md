# 트랙 선택·배치 연출 분리 검증

## 작업 기준과 수정 위치

- 최신 `origin/main`은 `1b57b3b9c7781129e850403867b88225b89fe6ba`다. 연출 변경은 [#21](https://github.com/kevin6091/ProjectMW/pull/21)의 `ea98d74fa90690610aa8ee7fa47b6eca3762a821`에서 분리했다. UVCS 기준은 `cs:80`이다.
- 팀원이 받는 Git diff에는 선택판, 로컬 후보·고스트 표현, 설치 승인 후 시각 효과, 트랙 화면 안내 및 포인터 연동이 들어간다. 차량 배기·엔진음, 로딩 렌더 클래스 분리, 완주 결과 연출은 기존 통합 작업에 있다.

| 수정 위치 | 상태와 편집값의 소유 |
| --- | --- |
| `MWTrackSelectionBoard.*` | 후보 슬롯, 히트 박스, 파란 배경을 로컬 `SelectionStage` 아래에 둔다. 이전 코스의 경계와 선택 카메라 시야로 창마다 이동 거리를 계산한다. 배치 카메라로 돌아오면 판과 전시 조각을 숨긴다. 판 메시·재질 참조는 경기 레벨에 있다. |
| `MWTrackPreview.*`, `MWTrackPreviewPreload.*`, `MWTrackPresentationSettings.h` | 후보 회전과 호버 확대, 와이어프레임과 기존 표면을 합성한다. 표면 투명도 0.25는 기존 UVCS 머티리얼이 소유한다. 후보 배율·시간·색은 `DefaultGame.ini`가 소유한다. |
| `MWTrackPlacementViewComponent.*` | 포인터 이동량에 따른 고스트 기울기, 자유 이동 및 Q/E 회전 보간, 끝점 스냅 확대와 접점 표시. 확정 입력은 현재 포인터에서 자세와 지면을 다시 판정하고 #21의 서버 경로에 같은 요청을 보낸다. |
| `MWTrackSelectionWidget.*`, `MWGameplayPlayerController.*`, `MWCursorProjection.*` | WBP가 하단 안내의 스타일과 배치를 소유한다. C++은 조작 문구·끝점 피드백·포커스·좌표 변환을 담당한다. 원격 포인터는 수신 창의 전시 이동 거리를 적용한다. |
| `MWEnvironmentGameplay.*`, `MWTrackPlacementPop.cpp`, `MWTrackSupports.cpp`, `MWEnvironmentRestore.*`, `MWTrackRuntimeComponent.*` | 서버가 설치한 트랙의 표현용 메시를 접점 중심으로 재생한다. 불티·연기 뒤 골조를 위에서 바닥까지 순서대로 공개한다. 고정 도로 형상과 실제 테마 바닥 정보를 사용하고 움직이는 부분에는 지지대를 붙이지 않는다. |
| `MWTrackRuntimeSubsystem.*`, `MWTrackSnap.h`, `DefaultGame.ini` | 기존 코스 전용 클래스 판정과 포트 읽기를 설치 연출에서도 사용한다. 기존 메시·재질 소프트 참조와 연출값을 설정에 둔다. |

## 검증

- UE 5.8.4 Win64 Development Editor 빌드 성공. 선택 WBP, 경기 HUD WBP, 컨트롤러 BP 컴파일 3개 성공 및 파일 SHA-256 불변 확인. 건설·커서·라운드 자동화 8개 통과.
- Cua Driver로 실제 `L_Dev_Drive`의 Listen Server 1창과 클라이언트 2창을 관찰·캡처했다. 1라운드 정상 설치와 주행 진입 뒤, 격리 PIE의 차량을 기존 결승 트리거 위치로 옮겨 2라운드를 준비했다. 점수와 경기 단계는 실제 서버의 트리거 겹침과 라운드 전환 코드로 갱신됐다.
- **2라운드 선택**에서 이전 도로·출발·종료 조각 5개 전부가 파란 판과 월드 평면에서 분리됐다. 보수적으로 각 코스 액터의 전체 AABB 모서리를 투영했을 때 모든 오른쪽 경계의 X가 0 미만이었다. 아래 월드 간격은 전시판과 코스 조각의 XY AABB 분리 간격 중 최솟값이다. [P1 서버](track-construction-presentation-evidence/round2-selection-p1.png), [P2 클라이언트](track-construction-presentation-evidence/round2-selection-p2.png), [P3 클라이언트](track-construction-presentation-evidence/round2-selection-p3.png)
- **2라운드 배치**에서는 세 창의 파란 판이 숨겨졌고 충돌은 양쪽 단계 모두 꺼져 있었다. 3명 배치 확정 후 새 종료 캡과 함께 다시 `Racing` 진입을 확인했다. 테스트 로그에 확정 순간 위치 차이 0.000000cm, 로컬 접점 섬광·확대, 접점 기준 노면 팝과 골조의 위·아래 층 공개가 기록됐다. [배치 화면](track-construction-presentation-evidence/round2-placement.png), [서버·클라이언트 로그와 해시](track-construction-presentation-evidence/verification.txt)

| 창 | 게임 뷰포트 px | UMG DPI 배율 | 이전 코스의 가장 오른쪽 X px | 최소 월드 간격 cm |
| --- | ---: | ---: | ---: | ---: |
| P1 서버 | 794 × 514 | 0.4755 | -470.48 | 31606.91 |
| P2 클라이언트 | 994 × 532 | 0.4921 | -554.16 | 32810.32 |
| P3 클라이언트 | 1039 × 372 | 0.4440 | -413.05 | 40997.55 |

눈 테마와 위 창 크기에서 확인했다. 여기서 공간 분리는 같은 경기 월드 안의 로컬 전시 공간 이동이다. 이전 코스가 선택 카메라에 보이지 않고 좌표도 겹치지 않는다. 다른 테마·별도 프로세스·패키지 검증은 이번 실행 범위에 포함되지 않는다.

## 의존성과 인계

- 이 Draft는 #21 브랜치를 base로 둔다. 로컬 건설 단계, 재선택 세션, 지면 검사를 함께 써야 재선택 중에도 판과 카메라가 올바르게 전환된다. #21과 파일 11개를 공유하며 주요 접점은 선택판, 위젯, 플레이어 컨트롤러, 배치 고스트, 런타임 트랙 설정이다. #21 반영 뒤 base를 `main`으로 변경하고 그때의 main과 다시 비교한다.
- 현재 main과의 병합 모의검사에 충돌이 없다. 서버 설치 확정 함수와 복제 구조체는 #21과 동일하다. 이후 main에 같은 진입점이나 `DefaultGame.ini` 수정이 들어오면 최종 통합에서 재확인한다.
- 기존 cs:80 서버의 `L_Dev_Drive`, 선택판 메시와 재질, 반투명 표면 재질, 선택 WBP, 골조 메시, 설치 입자 재질 7개가 이 시험의 파일 해시와 일치했다. 경로별 해시는 위 검증 로그에 있다. 이 PR에서 추가하거나 수정한 UE 바이너리는 0개다.
- 되돌릴 경우 이 Draft의 연출 커밋만 revert한다. #21의 지면·재선택은 별도 커밋으로 유지된다.
