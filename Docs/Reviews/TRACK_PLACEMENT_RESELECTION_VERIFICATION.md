# 트랙 배치 불가 시 이번 라운드 되돌리기와 전원 재선택

기준 `54c9ade` · 브랜치 `fix/track-placement-clearance` · UE 5.8.4

## 합의된 동작

- 선택한 조각이 두 방향 모두 지면 검사를 통과할 수 없을 때 좌클릭 또는 Space로 재선택을 요청한다. 유효한 방향이 하나 있으면 Q/E로 돌려서 설치한다.
- 재선택 후보를 먼저 준비한 뒤 **이번 라운드에 플레이어가 설치한 조각만** 되돌린다. 출발 조각, 이전 라운드 도로의 Actor·Transform, 경기 점수와 라운드 번호를 보존한다.
- 이미 설치했던 플레이어를 포함해 전원이 기존 트랙 선택판으로 돌아온다. 기존 안내 슬롯에 **“배치할 수 있는 트랙이 없어 다시 선택합니다 · 좌클릭으로 선택”**을 표시한다.
- 전원 확정 뒤 같은 라운드의 시작 끝점부터 다시 설치한다. 이번 배치 세션 번호를 새로 부여해 되돌리기 전의 클릭·미리보기 요청을 거절한다. 선택과 개인 조각이 다른 순서로 수신돼도 이전 고스트에서 입력이 들어가지 않게 한다.
- 대체 직선을 강제하는 설정과 조각 건너뛰기 경로를 제거했다. 이번 라운드 시작점에 바로 붙는 후보가 부족할 때의 최신 처리는 아래의 「즉시 설치 가능한 후보가 참가자 수보다 적을 때」에 기록했다.
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

## 하단 안내판의 프로젝트 공용 스타일 적용

기준 `bc62b16` · UE 5.8.4 · UVCS `cs:74`

- 수정 위치는 `Content/UI/Construction/WBP_TrackSelectionTest.uasset` 한 개다. Designer의 `Canvas_WorldOverlay → ConstructionStatusSafeZone → ConstructionStatusFooter → ConstructionStatusSign → ConstructionStatusContentScale → ConstructionStatusLabelWidth → Text_SessionStatus`에서 편집한다.
- 기존 로비에서 쓰는 `T_MW_MenuSign`과 `F_NotoSansKR_Bold_Font`를 재사용한다. 판면은 9-slice, 안전 영역의 하단 중앙 앵커·정렬은 `(0.5, 1)`이다. 여백·색·줄 폭·표시 배율은 모두 Designer에 저장했다. 같은 텍스트 바인딩이 정상 선택·대기·배치·재선택 상태를 갱신한다.
- 기존 후보 이름표는 `Text_SessionStatus`의 글꼴을 참조한다. 원래 글꼴 크기를 보존하고 안내판 내부 `ScaleBox`에서만 확대했다. 안내판의 최대 줄 폭은 Designer에서 정해 자동 폭과 자동 줄바꿈이 서로 영향을 주지 않게 했다. 장식 판면은 `Hit Test Invisible`이다. C++·설정·문구 리소스 변경은 없다.
- UVCS `cs:74` 서버 파일을 다시 내려받아 저장본과 대조했다. 두 파일의 SHA-256은 `BF82EC665AF64E377C6AEF54A6D15F2E46DDCBDAF7C413EE70789BAEE51A511A`다. 팀원은 해당 변경 세트의 WBP를 받으면 된다.

### Cua Driver 3인 PIE 재검증

| 참가자 | 뷰포트 | UMG 배율 | 자료 |
| --- | --- | --- | --- |
| 호스트 | 794×514 | 0.4755 | `final-selection-1.png`, `reselection-style-1.png`, `final-placement-1.png`, `final-racing-1.png` |
| 클라이언트 1 | 1099×517 | 0.4782 | `final-selection-2.png`, `reselection-style-2.png`, `final-placement-2.png`, `final-racing-2.png` |
| 클라이언트 2 | 1384×393 | 0.4440 | `final-selection-3.png`, `reselection-style-3.png`, `final-placement-3.png`, `final-racing-3.png` |

- 변경 전 캡처는 `work/track-round-reselection-2026-11-24/first-selection.png`와 `reselection-p1.png`다. 변경 후 캡처·로그는 `work/track-footer-style-2026-11-24`에 있다.
- 세 창 모두 정상 선택 안내가 한 줄, 긴 재선택 문구가 두 줄로 하단 중앙에 표시됐다. 재선택 문구의 외형은 외부 QA 스크립트로 같은 문구를 실제 PIE 위젯에 표시해 확인했다. 설치를 되돌리고 전원이 다시 고르는 서버 경로는 이 문서 앞 절에 기록한 `bc62b16` 실제 재현이 근거다.
- 세 참가자가 선택판에서 각각 후보 3·1·4를 실제로 클릭해 선택했다. 배치 안내와 설치 완료 대기가 같은 하단 판에서 갱신됐고, 세 명의 설치를 서버가 각각 승인했다. 출발 트랙 `BP_Track_WoodStartCapped_C_0`에서 FRED·DOUG·MAMMOTH가 각각 X=-2620·-2620·-2870cm에 배치돼 `Racing`에 진입했다. 주행에서 하단 판은 사라졌다.
- WBP 최종 컴파일·저장은 `designer-final.log`의 `MW_FOOTER_STYLE_FINAL`로 확인했다. `pie.log` 2843–2848행은 긴 문구, 2854–2883행은 실제 선택과 배치 문구, 2895·2954·3079행은 서버 설치 승인, 3163·3165·3167행은 3인 `Racing` 진입을 기록한다. 실제 출발 좌표는 `final-racing-state.json`에 남겼다.
- 남은 화면 확인 사항은 P3처럼 가로가 넓고 높이가 낮은 창에서 긴 영문 후보 이름표끼리 일부 겹치는 경우다. 후보 이름표의 배치·이름 표기 작업에서 따로 확인한다.

## 재선택 안내 문장 단위 줄바꿈

- `MWTrackSelectionWidget.cpp`의 기존 현지화 키 `PlacementReselection`에서 안내를 다음 두 줄로 지정했다. 재선택 상태 판단과 입력 처리에는 변경이 없다.

  배치할 수 있는 트랙이 없어 다시 선택합니다<br>
  좌클릭으로 선택

- `WBP_TrackSelectionTest` Designer의 문구 줄 폭을 넓혀 첫 문장이 중간에서 꺾이지 않도록 했다. 위치·글꼴·배율은 기존 하단 안내판을 따른다. WBP 컴파일과 `MWEditor Win64 Development` 빌드 성공을 확인했다.
- Cua Driver 3인 PIE에서 794×514 / 1099×517 / 1384×393, UMG 배율 0.4755 / 0.4782 / 0.4440으로 비교했다. 외부 QA 스크립트가 실제 변경한 현지화 문구를 활성 PIE 위젯에 표시했고, 세 창 모두 이유 한 줄과 클릭 안내 한 줄로 렌더링했다. 이후 세 명의 실제 좌클릭을 서버가 받아 각 후보 1·4·3을 확정했고 전원이 `TrackPlacement`에 들어갔다.
- 변경 전 `work/track-footer-style-2026-11-24/reselection-style-1.png`, 변경 후 `work/track-footer-line-break-2026-11-24/two-line-1.png`·`two-line-2.png`·`two-line-3.png`. 같은 폴더의 `pie.log` 2804–2809행과 `placement-state.json`에 표시와 입력 결과를 남겼다.
- 이 줄바꿈에 맞춘 WBP는 **UVCS `cs:75`**다. 이 변경 세트는 `WBP_TrackSelectionTest.uasset`만 포함한다. 내려받은 서버 파일과 PIE 에셋의 SHA-256은 `75B1798AFBC76A0228EE325362D29E51ACB95D2160A34BDC08FE2683A9548776`로 일치한다.


## 즉시 설치 가능한 후보가 참가자 수보다 적을 때

기준 `fix/track-selection-presentation` · UE 5.8.4 · UVCS `cs:78`

- [MWTrackConstructionComponent.cpp](../../Source/MW/Gameplay/Construction/MWTrackConstructionComponent.cpp)의 `BeginSelectionInternal`에서 이번 라운드 시작점에 바로 붙는 후보를 우선 뽑는다. 3인 방에서 그 후보가 0개 또는 2개여도, 다른 위치에서 사용할 수 있는 일반 카탈로그 조각으로 기존 선택판의 빈 칸을 채운다. 참가자마다 직접 선택하며 서버가 고른 조각을 자동 설치하는 경로는 없다.
- 기존 재선택 경로가 같은 경기·라운드의 선택 세션만 갱신한다. 이전 라운드의 코스 액터·Transform과 점수는 보존한다. 이번 라운드의 임시 설치는 되돌리고, 설치할 때에는 당시 열린 끝점의 방향·지면 조건과 클릭 위치를 서버에서 다시 검사한다. 현재 제공된 후보가 실제로 모두 막히면 다시 요청할 수 있다.
- [MWVehicleMovementTests.cpp](../../Source/MW/Tests/MWVehicleMovementTests.cpp)에 2라운드의 첫 끝점이 지면 검사 기준 아래에 있어 즉시 가능한 조각이 없는 재현을 추가했다. 0개에서 새 후보 6개와 전원 재선택이 열리고, 이전 라운드 조각·점수·경기 상태를 보존하며 오래된 설치 요청을 거절하는 것을 확인했다. 이 합성 지형의 낮은 끝점에 설치가 가능해졌다고 간주하지 않는다.

### 3인 PIE

설원에서 뷰포트 794×514, 994×532, 1039×372로 실행했다. 로비 준비와 차량 확정은 반복 시험 설정이 준비했고, 실제 트랙 선택·설치·P2 재선택 요청은 Cua Driver로 조작했다. 시험용 빌드에서 P2의 첫 트랙이 양방향으로 막히게 하고, 0개·2개 시험은 즉시 적합한 것으로 집계할 후보 수를 제한했다. 실제 후보 준비, 서버의 판정과 전원 재선택, 코스 되돌리기와 재설치는 제품 경로를 사용했다. 임시 조건은 커밋할 소스에서 제거했다.

| 조건 | 관측 결과 | 화면 |
| --- | --- | --- |
| 즉시 적합 후보 0개 | 후보 6개, 선택 세션 1→2, 설치 조각 2→1. 세 창에 기존 두 줄 안내가 뜨고 재선택·재설치 후 실제 출발선부터 Racing. | [세 창](Images/TrackReselectionZeroComparison.jpg) |
| 즉시 적합 후보 2개 | 후보 6개, 선택 세션 1→2, 설치 조각 2→1. 세 창이 같은 방에서 다시 고르고 설치해 Racing. | [세 창](Images/TrackReselectionTwoComparison.jpg) |
| 적합 후보 충분 | 기존 후보 6개와 재선택·재설치 흐름을 확인했다. 세 창이 결승 조각을 포함한 5조각 코스의 실제 출발선에서 Racing에 진입했다. | [세 창](Images/TrackReselectionNormalComparison.jpg) |

[서버·세 클라이언트 발췌 로그](TRACK_RESELECTION_SAME_ROOM_PIE_LOG.txt). 2개 재현에서는 3창 GPU 메모리 예산 경고가 표시됐고, 세 창 모두 해당 세션의 설치와 Racing까지 도달했다.

### 독립 클라이언트 첫 진입과 최종 빌드

- 임시 시험 조건을 제거한 MWEditor Win64 Development 빌드에서 호스트 PID 27072와 새 클라이언트 PID 41440이 각각 실행됐다. 설원 경기로 처음 이동한 클라이언트는 `L_Dev_Drive` 선택 카메라 공개 로그 후 파란 판과 후보 6개를 표시했다. Cua Driver로 클라이언트에서 1번 후보를 클릭했고, 서버가 세션 1·리비전 1의 요청을 승인했다. [독립 클라이언트](Images/TrackReselectionSeparateClient.jpg) · [첫 진입 및 승인 로그](TRACK_RESELECTION_SAME_ROOM_PIE_LOG.txt)
- 최종 빌드 성공. `MW.Construction`, `MW.Gameplay`, `MW.Vehicle`, `MW.Cursor`의 자동화 23개가 두 프로세스 실행에서 전부 통과했다. 첫 일괄 시도는 기존 가짜 연결의 GC 정리에서 중단됐다. 해당 연결 테스트만 독립 실행하고 나머지 22개를 다시 실행해 모두 통과했다.
- 로컬 원본은 `work/reselection-exit-2026-11-24/same-room-pie`, `separate-first`, `final-automation.log`, `final-network-test.log`에 있다. Git 커밋은 C++·검증 문서·캡처·로그로 구성한다. UVCS 체크인이나 패키지 실행은 이 작업의 검증에 포함하지 않았다.


## 하단 안내의 뷰포트 상대 크기

- 수정 위치는 `Content/UI/Construction/WBP_TrackSelectionTest.uasset` 한 개다. Designer의 `ConstructionStatusSafeZone` 안에서 `ConstructionStatusViewport → ConstructionStatusViewportScale → ConstructionStatusDesignSize → ConstructionStatusFooter`로 연결한다. 새 Canvas의 가로 22–78%, 세로 84–98% 앵커가 안전 영역 안에서 안내판이 들어갈 범위를 잡는다.
- `ConstructionStatusViewportScale`의 **Scale to Fit**으로 900×152 디자인 면을 맞춘다. 내부 안내판은 그 면의 하단 중앙에 맞추며, 배경과 여백 및 글자가 같이 움직인다. 기존 Noto Sans KR Bold 18, 내용 1.5배, 문장별 줄바꿈, 클릭을 통과하는 가시성 속성은 Designer에서 계속 관리한다.
- 이전에는 콘텐츠의 1.5배와 전역 UMG 배율만 적용됐다. 이번 비교의 두 뷰포트는 모두 전역 UMG 0.4440이다. 좁아질 때 안내판을 줄이는 값은 전용 ScaleBox의 실제 슬롯 크기에서 나온다. 공용 DPI 설정과 C++ 변경은 없다.
- Unreal Editor Python 명령렛에서 해당 WBP 컴파일·저장을 확인했다. 같은 소스의 검증 사본에서 Cua Driver로 3인 Listen Server PIE를 실행했다. 첫 실행에서 세 창의 실제 선택 승인과 배치 진입을 확인했다. 해당 실행의 호스트에서는 가장 긴 재선택 문구를 시험 보조로 표시해 이유 한 줄과 클릭 안내 한 줄을 확인했다. 재선택 서버 경로의 검증은 앞 절에 기록했다.
- 두 번째 실행의 같은 P2 창에서 배치 안내가 표시된 상태로 크기를 바꿨다. 974×462에서 판은 246×57px, 754×356에서는 196×44px이었다. PNG에서 노란 판의 경계를 측정했다. 두 창 크기 모두 중앙 정렬과 배치 지시 두 줄이 보였다. 캡처에는 GPU 메모리 예산 안내가 함께 표시된다. [픽셀 크기를 보존한 비교 캡처](../Engineering/Client/Images/TrackFooterResponsiveResize.jpg)

관측 로그 `work/track-footer-responsive-2026-11-24/pie.log`의 시간은 UTC다.

```text
08:10:05–08:10:06 P1·P2·P3 TrackPlacement, SelectedOfferId 1·2·5, bConfirmed=True
08:12:14.559 P2 TrackPlacement viewport=[754,356] dpi=0.4440, 스냅·회전 안내 두 줄
08:12:31.590 P2 TrackPlacement viewport=[974,462] dpi=0.4440, 같은 두 줄
```

- 로컬 WBP SHA-256 **`7A3177DEC53287102009CB5381999926204BB8B80670A868B83A0C455EE5A699`**. 공유 기준 에셋은 UVCS `cs:75`의 `75B1798AFBC76A0228EE325362D29E51ACB95D2160A34BDC08FE2683A9548776`다. 이 변경의 WBP는 로컬 에셋이며, Git에는 이 문서와 캡처를 기록했다.

- 원본 에디터가 파일을 사용 중이어서 같은 Designer 편집을 원본 에디터 안에서 다시 적용해 컴파일·저장했다. 검증 사본의 SHA-256은 **716CF7190D0A47640F4EA3BD723C94302EC0763BA362502E185866726923B3A7**다. 두 저장에서 위젯 경로, 디자인 면 크기, 앵커, 글꼴 크기와 문구가 동일하다.
