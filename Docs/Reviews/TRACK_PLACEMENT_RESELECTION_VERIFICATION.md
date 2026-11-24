# 트랙 배치 불가 시 이번 라운드 되돌리기와 전원 재선택

기준 `54c9ade` · 브랜치 `fix/track-placement-clearance` · UE 5.8.4

## 합의된 동작

- 선택한 조각이 두 방향 모두 지면 검사를 통과할 수 없을 때 좌클릭 또는 Space로 재선택을 요청한다. 유효한 방향이 하나 있으면 Q/E로 돌려서 설치한다.
- 재선택 후보를 먼저 준비한 뒤 **이번 라운드에 플레이어가 설치한 조각만** 되돌린다. 출발 조각, 이전 라운드 도로의 Actor·Transform, 경기 점수와 라운드 번호를 보존한다.
- 이미 설치했던 플레이어를 포함해 전원이 기존 트랙 선택판으로 돌아온다. 기존 안내 슬롯에 **“배치할 수 있는 트랙이 없어 다시 선택합니다 · 좌클릭으로 선택”**을 표시한다.
- 전원 확정 뒤 같은 라운드의 시작 끝점부터 다시 설치한다. 이번 배치 세션 번호를 새로 부여해 되돌리기 전의 클릭·미리보기 요청을 거절한다. 선택과 개인 조각이 다른 순서로 수신돼도 이전 고스트에서 입력이 들어가지 않게 한다.
- 대체 직선을 강제하는 설정과 조각 건너뛰기 경로를 제거했다. 현재 라운드 시작점에 호환되는 서로 다른 후보를 참가 인원만큼 확보하지 못하면 안내를 표시하고 현재 코스를 보존한다.
- 보존된 도로의 지지대를 완성 상태로 복원해, 되돌릴 때 이전 도로에 설치 팝·불티·철골 조립 연출이 반복되지 않게 했다.

## 수정 위치

| 코드 | 역할 |
| --- | --- |
| `MWTrackConstructionPlacement.cpp` | 두 방향 거절, 이번 라운드 시작점과 참가자 보존, 서버 되돌리기와 배치 재개 |
| `MWTrackConstructionComponent.cpp` | 유효 후보를 준비한 뒤 선택 세션 재개, 기존 트랙 선택 RPC·소유권 재사용 |
| `MWTrackSelectionTypes.h`, `MWTrackPlacementTypes.h` | 재선택 참가자와 보존 구간 복제 |
| `MWGameplayPlayerController.cpp`, `MWTrackPlacementViewComponent.cpp`, `MWTrackSelectionBoard.cpp` | 선택과 배치의 로컬 카메라·입력·고스트 전환 |
| `MWTrackSelectionWidget.cpp`, `MWGameplayHUDSubsystem.cpp` | 재선택 사유, 기존 선택·배치 안내 표시 |
| `MWTrackSupports.cpp`, `MWTrackPlacementPop.cpp` | 제거된 표시를 정리하고 보존 도로의 효과 재생을 막음 |
| `MWVehicleMovementTests.cpp` | 2라운드 강제 불가 상황에서 이전 도로의 Actor·Transform, 전원 재선택, 기존 요청 무효화, 경기 진행 회귀 검증 |

## 실행 결과

로컬 검증 자료는 `work/track-round-reselection-2026-11-24`에 있다.

### Cua Driver 설원 3인 PIE

뷰포트는 P1 794×514, P2 1099×517, P3 1384×393이며 UMG 배율은 각각 0.4755 / 0.4782 / 0.4440이다. P2의 첫 후보만 두 방향을 확실히 막을 수 있도록 PIE 검증 빌드에 임시 충돌 검사점을 추가했다. 후보 선택·확정·설치는 실제 게임 입력으로 수행했고, 재선택 조건과 서버 처리 및 안내는 수정한 실행 경로를 그대로 사용했다. 임시 검사점 코드는 최종 소스와 최종 빌드에서 제거했다.

| 단계 | 확인 결과 |
| --- | --- |
| 최초 설치 | P1이 첫 후보를 설치했다. 출발 조각과 P1 조각을 합쳐 2개였다. P1 설치 캡처 (`p1-installed-before-rollback.png`) |
| P2 요청·되돌리기 | 서버가 `players=3 offers=6 pieces=2->1`로 기록했다. 출발 조각만 유지됐고, 세 창 모두 세션 2와 코스 리비전 3을 받았다. 되돌리기 전 (`before-rollback.json`) · 전원 재선택 (`during-reselection.json`) |
| 안내 | 세 창 모두 화면 하단에 “배치할 수 있는 트랙이 없어 다시 선택합니다 · 좌클릭으로 선택”을 표시했다. P3 캡처 시점에는 이미 후보를 확정해 대기 안내가 보이며, 최초 안내 문구는 로그 3019행에 있다. P1 (`reselection-p1.png`) · P2 (`reselection-p2.png`) · P3 선택 완료 (`reselection-p3.png`) |
| 전원 재설치·출발 | 서버가 세 참가자의 재선택을 받은 뒤 배치를 재개했다. 세 명이 각각 새 조각을 설치했고 종료 조각까지 5개였다. 세 창 모두 라운드 1, Epoch 1의 실제 출발 그리드에서 자기 차량을 소유하고 `Racing`에 들어갔다. 재선택 완료 (`after-reselection.json`) · 실제 출발 상태 (`racing-result.json`) |

서버 승인과 세 창의 안내는 로컬 검증 폴더의 PIE 로그 (`pie.log`) 2894–3356행에 있다. 출발선에서 관측한 세 차의 X는 각각 -2620, -2620, -2870cm였다.

### 최종 빌드·자동화

- 임시 PIE 검사점을 제거한 `MWEditor Win64 Development` 최종 빌드가 성공했다. 최종 빌드 (`editor-verified-build.log`)
- `MW.Construction+MW.Gameplay` 테스트 10개 전부 통과했다. 2라운드에서는 이전 코스의 도로 4개를 유지하면서 이번 라운드에 설치한 조각만 1개 제거했고, 전원의 재선택 후 경기·점수·서든데스 진행을 확인했다. 최종 자동화 (`automation-verified.log`)
- 중간 실행에서 첫 라운드 완주 검사 1건이 실패했다. 무작위 후보에 체크포인트가 있어도 첫 라운드에서 통과시키지 않던 기존 테스트를, 실제 코스의 체크포인트를 차례로 통과하고 그 개수로 점수를 검증하도록 보완했다. 중간 실행 로그 (`automation-final.log`)
- 변경 범위는 Git 소스·설정·검증 문서다. WBP 및 맵은 기존 에셋을 사용했다.
