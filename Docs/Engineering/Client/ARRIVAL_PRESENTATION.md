# 로딩 직후 단계판과 복구창

2026-11-19. 정상 진입과 준비 실패를 따로 재현했다. 정상 실행에서 “불러오는 중” 직후 짧게 보이는 UI는 경기 HUD의 단계 안내였다. 선택 화면에서는 `SelectSign`, 배치 화면에서는 `BuildSign`이 표시된다. 후보를 누락시킨 실행에서 뒤늦게 나타나는 작은 기본 버튼 창은 `UMWArrivalScreen::ShowFailure`가 만들었다.

## 수정 위치

| 화면과 편집 주체 | 구체적인 변경 |
|---|---|
| `Content/UI/Race/WBP_MatchPanels.uasset` Designer | `SelectSign`·`BuildSign` 자리에 로비의 `T_MW_MenuSign` 원본 판면과 `F_NotoSansKR_Bold_Font`를 사용했다. 740×172 디자인 단위의 중앙 앵커 판면, 49pt 한 줄 문구, 9분할 브러시로 확장해도 모서리·볼트가 찌그러지지 않도록 했다. 문구·색·크기·패딩은 여기서 바꾼다. |
| `Source/MW/UI/Gameplay/MWGameplayHUDSubsystem.cpp`, `Config/DefaultGame.ini` | 기존 HUD가 단계와 가림 상태를 읽어 표시한다. `ConstructionPromptSeconds`의 기존 2초가 적용된다. |
| `Source/MW/UI/Gameplay/MWGameplaySign.{h,cpp}` | 경기와 로비 복귀가 함께 쓰는 `MWGameplayUI::RecoveryPanel`을 추가했다. 기존 로비 확인창의 `SMWGameplayPanel` 판면·볼트·파란 테두리·공용 한글 글꼴을 사용한다. 내용은 중앙 정렬하며 긴 사유 문구는 줄바꿈과 스크롤을 지원한다. 버튼의 콜백과 초기 포커스는 호출 화면이 전달한다. |
| `Source/MW/UI/Gameplay/MWArrivalScreen.cpp` | 경기 첫 화면의 준비 실패 안내에 공용 판면을 적용했다. 후보 재준비가 가능하면 재시도 버튼, 이동·연결 오류로 재시도가 불가능하면 방 나가기 버튼에 포커스를 준다. |
| `Plugins/MWUCMFrontend/Source/MWUCMFrontend/Private/MWUCMTitleFlow.cpp` | 타이틀 복귀의 세션 정리 오류에도 같은 안내창을 연결했다. 재시도·종료는 기존 세션 정리·종료 콜백으로 전달한다. |
| `Docs/Reviews/FrontendIntegration/PR_ASSET_HANDOFF.csv` | Git 제외 WBP의 배포 경로, 현재 크기와 SHA-256을 갱신했다. |

## 실행 결과

- **정상 3인 PIE** (PID 8600). 세 창 모두 준비된 Snow 테마와 3개의 월드 후보 위에 새 단계판이 보였다. 각 창의 첫 공개와 약 2초 뒤 프레임을 대조했다. 후속 프레임에는 중앙 안내가 사라졌고 후보와 카메라는 유지됐다. 기존 서버 출발 준비 보고는 Member 1·2·3 각각 한 번, 남은 인원 2→1→0이었다.
- **별도 프로세스의 첫 진입**. 각각 새 게임 클라이언트로 시작했다. PID 38868은 트랙 선택, PID 18572는 가림 중 선택을 모두 확정해 트랙 배치로 바로 들어갔다. 둘 다 1280×720에서 첫 프레임과 약 2초 뒤를 대조했다. 선택 첫 장면에는 후보와 “트랙을 선택하세요”, 배치 첫 장면에는 시작 트랙·고스트와 “트랙을 배치하세요”가 나타났다. 두 번째 프레임에서 안내만 사라졌다. 프로세스 간 OS 캐시는 유지했다.
- **경기 복구** (PID 12364, 3인 PIE). Client 2에서 후보 1의 표시 액터를 제거해 실패 경로를 재현했다. 다른 두 창은 정상 진입했고 해당 창에는 파란 판면, 한글 안내, 두 동작이 표시됐다. Cua Driver로 ‘첫 화면 다시 준비’를 클릭하자 가림막이 돌아왔으며 누락된 후보가 복구된 뒤 세션 1·리비전 1로 공개됐다. 실패와 재공개는 같은 시도 `01DE73ED4787DE532A7B16961B55677E`였다. 타이틀 복귀의 세션 서비스 오류와 실제 네트워크 단절은 이번 실행에서 주입하지 않았다.
- 같은 회복 실행에서 Member 3의 포인터를 오른쪽 직선 후보로 옮겼다. Client 2의 붉은 로컬 화살표와 Client 1에 보인 녹색 원격 화살표 끝이 같은 후보 위치에 놓였고 수신 화면에 닉네임 258이 보였다.
- UE 5.8 Win64 Development `MWEditor`·`MW` 빌드 성공. WBP 저장 뒤 새 에디터 프로세스로 다시 열고 컴파일해 오류 0건을 확인했다. 경고 1건은 프로젝트의 기존 EOS Ecom 설정 경고다.
- WBP 로컬 인계본은 72,127바이트, SHA-256 `c760335644a94389bcbb98b60f0f6b928c44ad3a793eee3ddc5ab231b271a422`이다. Git에 포함되지 않는 바이너리는 별도 ZIP에 제공한다.

## 중앙 트랙 배치 안내의 마름모 복원 · 2026-11-20

[수정 전후 3인 PIE 비교](Images/PR17BuildSignDiamond.jpg)

11월 19일 `16ea3b4`에서 로딩 뒤 잠깐 뜨는 안내와 복구창을 프로젝트의 로비 UI와 맞추면서, 중앙 단계 안내에도 `T_MW_MenuSign` 로비 판면을 적용했다. 이때 배치 안내까지 직사각형으로 바꾼 것은 스타일을 한 번에 맞추려던 작업 판단이었다. 기존 마름모 외형을 바꾸라는 요청은 없었다. 로딩·복구창을 정비하는 작업과 경기 단계판의 기존 외형 유지를 구분했어야 했다.

### 수정 위치

| 경로 | 변경 |
|---|---|
| `Content/UI/Race/WBP_MatchPanels.uasset` → Designer → `BuildSign` | 직사각형 `Border`와 자식 `BuildSignLabel`을 제거했다. 같은 이름에 기존 `MWGameplaySignWidget`을 연결해 11월 17일 `821596c`의 385×385 디자인 크기, 중앙 앵커와 정렬 (0.5, 0.5), 노란 앞면, 짙은 테두리와 검은색 기본 54pt 문구를 복원했다. 로딩 회전은 꺼 두었다. |
| `BuildSign`의 `Label` | 요청한 ‘트랙을 배치하세요’를 ‘트랙을’과 ‘배치하세요’ 두 줄로 유지했다. 11월 17일 당시 문구는 ‘트랙을 만드세요’였다. |
| [기존 표지판 렌더러](../../../Source/MW/UI/Gameplay/MWGameplaySign.cpp) | `SMWGameplaySign::OnPaint()`가 판과 문구를 같은 로컬 좌표에서 그린다. 줄마다 폭을 재고 마름모 안에 맞춰 배치하므로, 판면과 글자의 위치를 별도 위젯에서 계산할 필요가 없다. |

`BuildSign`만 수정했다. `SelectSign`·대기판·아이템 슬롯은 기존 Designer 항목을 유지한다. [경기 HUD](../../../Source/MW/UI/Gameplay/MWGameplayHUDSubsystem.cpp)는 기존 이름으로 `BuildSign`의 표시·숨김을 제어하고, `ConstructionPromptSeconds=2.0`은 기존 설정을 사용한다.

### 전후 검증

UE 5.8 Win64 Development · 설원 `L_Dev_Drive` · LocalIP 리슨 서버와 참가자 2명. 수정 전은 UVCS `cs:60` 에셋을 사용한 Cua Driver 검증 캡처를 대조했다. 정상 실행에서 직사각형 안에 맞은 화면과 지연 실행에서 문구가 판 밖으로 벗어난 화면을 함께 남겼다. 수정 후에는 세 창의 뷰포트를 모두 924×472로 맞추고 로비 준비·시작, 차량 선택, 세 명의 트랙 확정을 Cua Driver로 수행했다. 서버에서 3인 배치 세션 개방을 확인했다.

| 창 | 배치 단계 서버 로그부터 첫 캡처 파일 기록까지 | 다음 캡처 |
|---|---:|---:|
| P1 호스트 | 0.169초 | 0.629초 |
| P2 참가자 | 0.252초 | 0.760초 |
| P3 참가자 | 0.318초 | 0.864초 |

세 창의 두 시점 모두 판 중앙에 두 줄이 들어갔다. 캡처 픽셀에서 판 중심과 첫째·둘째 줄의 가로 중심 차이는 각각 0.5px와 0px, 문구 전체의 세로 중심 차이는 1px였다. 이후 세 창 모두 중앙 안내만 닫히고 배치 화면은 유지됐다. 이 시간은 서버 단계 로그와 Cua Driver PNG 파일 기록 시각을 대조한 값이다. GPU에서 그린 첫 프레임 자체의 시각까지 분리하지는 않았다. 저장된 WBP는 별도 에디터 실행에서 다시 불러와 컴파일했고 오류 0건을 확인했다. 기존 EOS Ecom 경고 1건이 남았다.

### 에셋 인계 버전

- 수정 전 `cs:60` SHA-256 `c760335644a94389bcbb98b60f0f6b928c44ad3a793eee3ddc5ab231b271a422`
- 수정 후 `cs:63` SHA-256 `ca5bc07eb791d1457dd6b7e10f6f8fc106cf07e5d0d044dfb91dc7266c4854ea`, 74,275바이트
- `cs:63`에는 위 WBP 한 파일이 올라갔다. 서버에서 내려받은 파일의 해시가 3인 PIE 검증본과 같다. [팀원 인계와 해시 확인](PR17_ARRIVAL_RECOVERY_VERIFICATION.md#팀원-수신-검증)

## 중앙 트랙 선택 안내의 마름모 복원 · 2026-11-20

11월 17일 `821596c`의 중앙 선택 안내는 공통 마름모에 ‘트랙을 / 선택하세요’를 두 줄로 표시했다. 11월 19일 `16ea3b4`에서 선택·배치 안내가 함께 로비 직사각형으로 바뀌었고, UVCS `cs:63`에서는 배치 안내 `BuildSign`을 복원했다. 이번에는 그 `cs:63` 파일을 기준으로 선택 안내만 복원했다.

### 수정 위치와 파일 버전

| Designer 위치 | 적용값 |
|---|---|
| `Content/UI/Race/WBP_MatchPanels.uasset` → `SelectSign` | `Border`와 자식 `SelectSignLabel`을 기존 `MWGameplaySignWidget`으로 교체했다. 이름 `SelectSign`을 유지해 HUD의 기존 바인딩을 사용한다. |
| `SelectSign` → Canvas Slot | 중앙 앵커·정렬 각각 (0.5, 0.5), 오프셋 (0, 0), 크기 385×385, Z Order 0, Size To Content 꺼짐. |
| `SelectSign` → Appearance | ‘트랙을’과 ‘선택하세요’를 줄바꿈으로 구분한다. 글자 크기 54, 노란 판면 RGB (255, 162, 22), 짙은 테두리 RGB (50, 43, 31), 두께 비율 0.042, 로딩 회전 꺼짐. 같은 WBP의 `BuildSign`이 사용하던 표지판 속성을 적용했다. |
| `BuildSign`, 대기판, 아이템 슬롯 | 저장 전후 위젯 구조와 슬롯을 비교했다. `BuildSign`은 기존 문구, 색, 글자 크기와 회전 여부까지 일치했다. |

`MWGameplayHUDSubsystem`이 로딩 공개 뒤 선택·배치 단계에 맞춰 기존 `ConstructionPromptSeconds=2.0`으로 표시한다. 문구와 외형은 WBP의 Designer 값이다. [단계별 표시 코드](../../../Source/MW/UI/Gameplay/MWGameplayHUDSubsystem.cpp#L204) · [공통 판면 위젯](../../../Source/MW/UI/Gameplay/MWGameplaySignWidget.cpp#L10)

| 구분 | `WBP_MatchPanels.uasset` SHA-256 |
|---|---|
| 복원 전 UVCS `cs:63` | `ca5bc07eb791d1457dd6b7e10f6f8fc106cf07e5d0d044dfb91dc7266c4854ea` |
| 복원 후 UVCS `cs:64` 서버 원본 · 71,242바이트 | `6fa01454bdf2b6151588bd1d6de38105ce1aac22705cc9c13654b15a33c1dd89` |

`Content/`의 WBP 수정본은 UVCS `cs:64` 한 파일로 인계했다. 서버에서 다시 내려받은 원본은 초기 3인 PIE 검증 기록 `a8ea589`의 로컬 파일과 SHA-256이 같다. Git에는 문서·캡처·로그를 기록한다. 팀원은 PR #17의 Git 브랜치와 `cs:64`를 사용한다. 로컬에서 되돌릴 경우 에디터를 닫고 `cs:63`의 위 해시를 확인해 원본 파일을 복원한다. [팀원 수신과 해시 확인](PR17_ARRIVAL_RECOVERY_VERIFICATION.md#팀원-수신-검증)

### Cua Driver 3인 PIE

UE 5.8 Win64 Development Editor에서 빌드했다. 저장한 WBP를 새 에디터 프로세스로 열어 컴파일 성공과 SHA-256을 확인했다. 같은 인계 목록의 나머지 9개 파일은 `cs:63` 해시와 일치했다.

설원 `L_Dev_Drive` 로컬 리슨 서버의 호스트와 클라이언트 두 명으로 진행했다. Cua Driver로 로비 준비·게임 시작·차량 선택을 수행하고, 마지막 차량 확정부터 세 창을 연속 캡처했다. 관측 스크립트는 경기 단계와 선택 상태를 읽었다. 세 창의 PIE 뷰포트는 462×222, UMG DPI는 약 0.444였다.

| 참가자 | 첫 공개 로그부터 선택 마름모 첫 캡처까지 | 첫 줄·둘째 줄 가로 중심 오차 | 글자 전체 세로 중심 오차 | 기존 배치 안내 재확인 |
|---|---:|---:|---:|---|
| P1 호스트 | 0.127초 | 0px / 0px | 아래로 1px | 같은 중앙 마름모 |
| P2 참가자 | 0.193초 | 0px / 0px | 아래로 1px | 같은 중앙 마름모 |
| P3 참가자 | 0.262초 | 0px / 0px | 아래로 1px | 같은 중앙 마름모 |

공개 시간의 기준은 이번 실행에서 기록된 첫 `MWArrival reveal` 행이다. 세 창의 캡처 파일 작성 시각과 대조했다. 가로·세로 오차는 각 창 원본 캡처에서 노란 판면의 중심과 글자 픽셀 영역의 중심 차이다.

세 창의 후속 캡처에서 중앙 안내가 사라진 뒤에도 후보 세 개와 좌상단 선택 목록이 보였다. 세 명이 목재 직선 트랙을 확정하자 `TrackPlacement`에서 기존 ‘트랙을 / 배치하세요’ 마름모가 중앙에 표시됐다. 배치 캡처의 ‘Preparing Shaders’ 문구는 에디터 표시다.

[세 창의 선택 첫 화면·안내 종료·배치 안내 비교](Images/TrackSelectionDiamondThreePlayers.jpg) · [에셋 대조와 PIE 로그](TRACK_SELECTION_DIAMOND_PIE_LOG.txt)

## 첫 장면 렌더 검사 소스 분리

`refactor/arrival-frame`은 PR #17이 포함된 `origin/main`에서 분기했다. `Source/MW/UI/Gameplay/MWArrivalFrame.h`에 Scene View Extension 선언을, `MWArrivalFrame.cpp`에 카메라·세션 확인, 선택 장면 색 샘플링, GPU fence 확인을 옮겼다. `UMWArrivalScreen`은 기존처럼 전방 선언된 렌더 검사를 만들고 가림·입력·이동 실패를 관리한다. 설정값과 공개 조건은 그대로다.

UE 5.8 Win64 Development Editor 빌드에서 `MWArrivalFrame.cpp`와 `MWArrivalScreen.cpp`가 각각 컴파일됐고 링크가 성공했다. 이 브랜치의 검증 범위는 코드 대조와 Editor 빌드다.
