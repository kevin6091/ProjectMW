# 트랙 지면 검사·전원 재선택 검증

## 기준과 포함 범위

- Git 기준은 `origin/main`의 `1b57b3b9c7781129e850403867b88225b89fe6ba`다. `fix/track-placement-reselection`에 설치 지면 검사, 같은 방의 전원 재선택 및 관련 UI 호환을 분리했다.
- UVCS 기준은 `cs:80`이다. 격리 프로젝트에서 cs:80 변경 에셋 12개와 `Content/UI/Construction/WBP_TrackSelectionTest.uasset`의 SHA-256이 서버에서 받은 파일과 일치했다. 선택 WBP의 해시는 `7A3177DEC53287102009CB5381999926204BB8B80670A868B83A0C455EE5A699`다.
- 순차 작업인 전시판·스냅 연출과 커서·로딩·차량 변경은 기존 작업 브랜치에서 이어간다. 참가자 이탈은 최신 main의 종료 정책을 따르며 별도 논의 대상이다.

## 수정 위치

| 위치 | 동작과 이유 |
| --- | --- |
| `MWTrackPlacementClearance.*`, `MWTrackRuntimeSubsystem.*` | 카탈로그의 도로 충돌 형상과 연결 포트로 지면 하한을 검사한다. 미등록 BP에는 기존 충돌 컴포넌트의 경계를 사용한다. |
| `MWTrackRuntimeSettings.h`, `DefaultGame.ini`, `MWEnvironmentGameplay.cpp` | 서버 설치 하한과 테마 바닥의 월드 높이를 프로젝트 설정에서 함께 읽는다. |
| `MWTrackConstructionPlacement.cpp`, `MWTrackPlacementTypes.h` | 확정 클릭 때 서버가 받은 세션·코스 리비전의 연결 포트와 종료 캡을 검증한다. 한 방향만 막히면 뒤집어 설치할 수 있고 두 방향이 모두 막히면 같은 방에서 재선택을 연다. 현재 라운드에 설치한 도로만 제거하고 이전 코스의 액터·좌표를 보존한다. |
| `MWTrackConstructionComponent.*`, `MWTrackSelectionTypes.h` | 전원에게 같은 후보와 준비 상태를 복제한다. 첫 끝점에 붙는 후보가 없거나 참여자 몫을 모으지 못하면 동일 세션에서 다시 준비하며, 오래된 요청과 즉시 중복 요청을 제한한다. |
| `MWGameplayPlayerController.*`, `MWTrackSelectionBoard.cpp`, `MWTrackPlacementViewComponent.*` | 서버 단계는 `TrackPlacement`로 유지한다. 클라이언트 카메라와 입력은 참가자의 복제 상태에 맞춰 선택 모드와 배치 모드를 전환한다. 확정 클릭 시 현재 포인터로 다시 판정한다. |
| `MWTrackSelectionWidget.*`, `MWGameplayHUDSubsystem.cpp` | cs:80 WBP에서 생략된 참가자 목록은 선택 바인딩으로 받는다. 재선택 사유와 좌클릭 안내를 두 줄로 표시하고 중앙 HUD도 로컬 건설 표시에 맞춘다. |
| `MWTrackSnapTests.cpp`, `MWVehicleMovementTests.cpp` | 설치 하한, 0개·2개 후보, 회차와 점수 보존, 현재 라운드 롤백, 전원 재확정과 코스 진행을 자동화한다. |

첫 PIE에서 main의 이름표 생성 경로가 cs:80 화면에 빈 흰색 판을 만들었다. `MWTrackSelectionWidget.cpp`에서 기존 WBP가 이름표 브러시를 실제로 지정한 경우에만 판을 만든다. WBP가 안내의 폰트·배치를 소유하고 C++은 서버 상태에 따라 문구를 바꾼다. `AMWTrackSelectionBoard::FindHoveredOffer`는 같은 전시 상자에 클릭 광선을 직접 대조해 배경의 나무·장식이 클릭을 먼저 받는 문제를 해결했다.

## 검증 결과

- UE 5.8.4 Win64 Development `MWEditor` 빌드 성공. `WBP_TrackSelectionTest`, `BP_MWGameplayPlayerController`, `WBP_MatchPanels` 컴파일 성공과 저장 파일의 SHA-256 불변을 확인했다.
- 실사용 코드의 자동화 8개 통과. `MW.Construction` 2개, `MW.Cursor` 3개, `MW.Gameplay.RoundLoop` 3개다. 0개·2개 부족 및 리비전 검증, 이전 라운드 도로의 Actor·Transform 보존, 재시도 후 재설치를 포함한다.
- 첫 PIE에서 P2의 트랙 클릭으로 재선택에 도달했다. 수정 후 3인 PIE에서는 최초 설치 한 건이 진행된 뒤 두 방향이 막혀 선택으로 돌아갔다. 재현 조건은 격리 편집기의 첫 P2 조각 높이점과 서버 후보 풀 상한 2개로 만들었다. 방·차량 단계 준비와 상태 기록만 스크립트로 지원했다. 전원 복구에 사용한 후보 재요청·선택·설치는 Cua Driver로 각 창에 입력했다.
- 서버는 `pieces=2->1`, `session=2`를 기록했다. 같은 세션의 후보 부족 상태에서 재시도해도 `pieces=1->1`, 코스 리비전 3을 유지했다. 세 창 모두 “배치할 수 있는 트랙이 없어 다시 선택합니다 / 좌클릭으로 후보 다시 준비”를 받았다. [P1](track-placement-reselection-evidence/waiting-p1.png), [P2](track-placement-reselection-evidence/waiting-p2.png), [P3](track-placement-reselection-evidence/waiting-p3.png)
- 후보 풀 상한을 해제하고 P1이 안내를 클릭하자 세 창 모두 세션 2의 리비전 7에서 후보 6개를 받았다. 다시 고를 때 문구가 “좌클릭으로 선택”으로 바뀌었다. P1은 장식이 겹쳐 있는 왼쪽 위 후보를 클릭해 확정했다. 빈 흰색 이름표가 다시 생기지 않았다. [후보가 돌아온 화면](track-placement-reselection-evidence/reselection-open.png)
- 세 창에서 서로 다른 후보를 확정하고 하나씩 설치했다. 완성 코스는 시작 조각·참가자 조각 3개·종료 캡 총 5개였다. `L_Dev_Drive`의 체크무늬 출발선에서 차량 소유와 `Countdown → Racing`을 전원 확인했다. [출발 화면](track-placement-reselection-evidence/racing-start.png), [빌드·자동화·서버 및 클라이언트 로그 발췌](track-placement-reselection-evidence/verification.txt)

| 창 | 게임 뷰포트 px | Unreal UMG DPI 배율 | Racing에서 확인한 차량 위치 cm |
| --- | ---: | ---: | --- |
| P1 서버 | 794 × 514 | 0.4755 | (-2620, 375, 6069.2) |
| P2 클라이언트 | 994 × 532 | 0.4921 | (-2620, 525, 6046.2) |
| P3 클라이언트 | 1039 × 372 | 0.4440 | (-2870, 375, 6054.2) |

검증용 높이점과 후보 수 제한은 격리 사본에만 적용했다. 재현 코드를 제거한 뒤 최종 소스로 빌드, WBP 재컴파일, 회귀 자동화를 다시 실행했다. 기존 `MWItemComponentVisual.cpp`의 GetBindings C4996 경고와 에디터 EOS 설정 경고를 기록했다. 패키지·별도 클라이언트 프로세스 검증은 이번 실행 범위에서 제외했다.

## 병합 전 검토

- 복제 세션과 RPC 계약이 바뀌어 서버·클라이언트가 같은 Git 변경을 받아야 한다. 같은 코드와 UVCS cs:80으로 팀원 두 명이 독립 검토한다.
- 최신 main과 후속 작업에서 건설 컴포넌트·컨트롤러·선택판을 함께 수정하면 파일이 겹칠 수 있다. 후속 PR의 기준을 첫 PR 결과에 맞춰 다시 점검한다.
- 되돌릴 때 이 PR의 커밋을 함께 revert한다. Git에 에셋 변경이 없어 UVCS 되돌리기는 필요하지 않다.
