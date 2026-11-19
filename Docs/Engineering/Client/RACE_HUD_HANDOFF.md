# 경기 HUD 후속 작업 인계 지침

기준일: 2026-11-13. 작업 브랜치: `feature/race-hud`.
원본 프로젝트: `C:\workspace\UE\ProjectMW\MW.uproject`.
**계기판은 오른쪽 아래에 둔다. 레퍼런스는 내부 디자인 참고용이다. 중앙·왼쪽으로 옮기지 않는다.**

## 1. 다음 담당자가 먼저 할 일

1. `AGENTS.md`와 작업 분류 스킬을 읽고 공용·해당 분야 문서를 확인한다.
2. 현재 브랜치·HEAD·Git 변경을 확인한다. 기존 변경을 reset/stash/checkout으로 폐기하지 않는다. 추가 worktree나 프로젝트 복제본을 만들지 않는다.
3. 아래 에셋 3개가 있는지 확인하고 해시를 대조한다. **Git 코드만 받으면 HUD가 완성되지 않는다.**
4. 디자인은 `UI/Race/WBP_RaceHUD`의 Designer에서 수정한다. C++에 레이아웃을 다시 만들지 않는다.
5. 변경 전 사용자가 원하는 수정 범위를 설명한다. 팀원 소유 주행·서버 코드는 별도 승인 없이 수정하지 않는다.

## 2. Git 상태와 기존 PR 관계

- 저장소: `kevin6091/ProjectMW`, 공개 저장소. source 브랜치는 `feature/race-hud`다.
- 분기 기준: `fix/item-ui-polish@f4b651eb9fea1f9ee1ea5425c56742bf456866c3`.
- 구현·최종 디자인 기록 기준: `73128a6e82e6f7afdf5f090d3559da8575ca02ad`. 이 인계 문서는 그 이후 커밋에 포함된다. 최종 HEAD는 `git rev-parse HEAD`로 확인한다.
- 확인 시점 `origin/main`: `770f9f69bd4af0b2a7bd41cf9fe246888abcf8e1`. 현재 작업의 조상이다.
- [PR #11](https://github.com/kevin6091/ProjectMW/pull/11)은 확인 시점 OPEN이며 `fix/item-ui-polish` → `main`이다. 이 브랜치는 그 미병합 변경 위에 쌓여 있다.
- PR #11의 최종 처리 상태를 다시 확인한 뒤 후속 PR의 비교 기준을 정한다. 이번 작업에서는 PR 등록·수정·main 병합을 하지 않는다.
- fetch로 확인한 `origin/fix/Network-Prediction`은 `1bff9206fc2a8dcb484847686bbb6f960c94b186`이다. 이번 HUD 작업에 새로 병합하지 않았다. 최신 서버 코드까지 들어 있다고 가정하지 않는다.

| 커밋 | 내용 |
|---|---|
| `ded3b3a` | 소유 차량 속도 표시 추가 |
| `d03e1ae` | 기존 경기 단계 이벤트·수명 회귀 검사 연결 |
| `a48d42c` | 초기 Slate 계기판. 현재는 대체됨 |
| `c964fdc` | 디자인을 WBP·UI 재질로 전환, Slate 계기판 네 파일 제거 |
| `b8bf13a` | 반복 조향 속도 증가 조사. 주행 코드 수정 없음 |
| `73128a6` | 오른쪽 배치 유지·최종 외형·검증 기록 |

## 3. 반드시 함께 전달할 에셋

Content는 `.gitignore`에서 UVCS 관리 대상으로 제외한다. 정책 변경이나 `git add -f`를 하지 않는다.
아래 파일은 원본 MW 로컬에 저장돼 있다. **이 작업에서 UVCS 등록·체크인·업로드를 실행하지 않았다.**
다른 PC 검증 전에 담당자가 별도로 에셋 전달을 완료해야 한다. 사용자가 허용하기 전에는 UVCS 명령·체크아웃·갱신·Undo·잠금 조작을 하지 않는다.

| 파일 | 최종 검증본 SHA-256 |
|---|---|
| `Content/UI/Race/WBP_RaceHUD.uasset` | `b03104a4e364143be3a2c3f3125f1ff9eb1711b6f9234db999c84d135ba84ac0` |
| `Content/UI/Race/Materials/M_RaceDial.uasset` | `b2b0cc5a903f9706551fa746f0e0145742070655872d546775fe69524680869d` |
| `Content/UI/Race/Materials/MI_RaceDial.uasset` | `4e03c6f7fec9d9e3088507a6a47aa9e64e704dd0380f048d78df8b16720af461` |

공통 글꼴 `/Game/UI/Common/F_NotoSansKR_Bold_Font` 등 기존 UI 의존성도 필요하다.
WBP 클래스 연결은 `Config/DefaultGame.ini`의 다음 설정이다.

```ini
[/Script/MW.MWGameplayHUDSubsystem]
RaceHUDWidgetClass=/Game/UI/Race/WBP_RaceHUD.WBP_RaceHUD_C
```

HUD WBP 생성 실패 로그가 나오면 클래스 경로·에셋 전달부터 확인한다. C++ 대체 화면을 추가해서 누락을 숨기지 않는다.

## 4. 현재 디자인과 수정 위치

- `Panel_Speed`: 오른쪽 아래 Anchors `(1,1)`, Position `(-42,-32)`, Size `(360,324)`.
- 원형 배경을 비우고 얇은 흰 눈금·바늘과 옅은 파랑 강조를 사용한다. 숫자·단위는 기존 공통 글꼴이다.
- `Text_Speed`, `Text_Unit`, `Panel_Ticks`의 자식 TextBlock: 글꼴·색·위치·크기를 Designer에서 편집한다.
- `Image_Needle`: 두께·길이·색은 Designer. Pivot은 아래 중앙 `(0.5,1)`이며 회전 중심을 함부로 옮기지 않는다.
- `Image_DialShadow`: 밝은 배경에서 눈금을 읽기 위한 얇은 그림자다.
- `MI_RaceDial`: 곡선·눈금 두께, 배경 투명도, 색, 경계 부드러움을 편집한다.
- Class Defaults: 표시 상한 120km/h, 바늘 각도·응답, 단계별 문구 `PhaseLabels`.
- `BindWidget` 주요 이름은 유지한다. 이름을 바꾸면 C++ 연결도 함께 확인해야 한다.
- RPM·기어·브레이크 밸런스 등의 없는 데이터를 가짜 계기판으로 채우지 않는다.
- 잘못 시도한 중앙·왼쪽 배치와 `M_RaceReadoutPanel`은 폐기했다. 해당 재질은 참조 0개 확인 후 삭제했다.

[디자인 편집 설명](RACE_HUD.md)에 각 항목의 세부 위치가 있다.

## 5. 코드 구조와 보호할 부분

| 파일 | 역할 |
|---|---|
| `Source/MW/UI/Gameplay/MWGameplayHUDSubsystem.h/.cpp` | 기존 GameState 이벤트 관찰, 월드당 WBP 생성·수명 관리, 기존 아이템·표지판 HUD 연결 |
| `Source/MW/UI/Gameplay/MWRaceHUDWidget.h/.cpp` | 소유 Pawn의 실제 속도와 경기 단계를 WBP에 전달 |
| `Source/MW/Tests/MWRaceHUDTests.cpp` | 상태 교체·구독 해제와 수동 PIE 단계별 회귀 검사 |

속도는 `Velocity.Size2D() × 0.036`이다. 최대 10Hz로 샘플링하고 정수가 바뀔 때만 텍스트를 갱신한다.
바늘·속도 곡선만 표시용으로 보간한다. 차량 속도 자체를 바꾸지 않는다.
UI 객체는 `TObjectPtr`/Transient, 차량은 약한 참조로 관리한다. 월드 종료·NativeDestruct의 정리를 보존한다.

이번 브랜치의 신규 HUD 변경은 서버 판정·RPC·Snapshot·차량 이동·트랙 로직을 바꾸지 않는다.
기존 UI·차량·서버·UCM 원본을 임의 정리하지 않는다. 새 플러그인은 추가하지 않았다.
후속 C++는 기존 줄바꿈·한글 주석·언리얼 객체 수명 규칙을 따른다.

## 6. 검증과 재현

1. 위 에셋·Config·빌드가 맞는지 먼저 확인한다. 에디터가 다른 브랜치 DLL·BP를 잡고 있으면 저장 후 정상 종료한다.
2. `L_Dev_Drive_ItemPickup`을 열고 PIE한다. 이 개발 맵에는 슬롯·속도계 단독 표시 예외가 있다.
3. 정지 0 → 주행 시 속도·바늘 → 정지 0을 확인한다. 1,800cm/s는 반올림 65km/h다.
4. 오른쪽 위치, 중앙·왼쪽의 잘못된 추가 표시가 없는지, 아이템 슬롯과 겹치지 않는지 확인한다.
5. 1920×1080과 1296×759에서 확인한다. 에디터 창 크기 변경 직후가 아닌 렌더링이 안정된 화면으로 판단한다.
6. 픽업 후 Space로 아이템이 소모되고 슬롯이 비워지는지 확인한다.
7. 실제 경기는 Racing에서 속도계를 표시한다. 다른 경기 단계의 전체 진행과 로컬 개발 맵의 단독 표시를 구분한다.

최종 오른쪽 디자인 저장 후 Editor/Game Development 빌드 성공: 46.84초 / 54.08초.
에디터 재시작 후 WBP 로드·컴파일, 두 해상도 CUA 캡처, 0·65 표시, 바늘과 실제 CUA Space 소모를 확인했다.
픽업 준비에는 테스트용 위치 이동, 주행 수치 비교에는 공개 입력 API를 사용했다. 모두 사람의 실제 주행 테스트라고 표현하지 않는다.
최종 PIE Error·assert·ensure 없음. 기존 상태 수명/단계 네이티브 검사는 앞선 코드 작업에서 통과했으며 새 디자인의 전체 경기 단계 진행을 다시 검증한 것은 아니다.
**미검증:** 이번 디자인 기준 전체 로그인/EOS/로비 복귀, 실제 멀티클라이언트, Cook/패키징, 다른 PC의 에셋 수신 후 실행.

## 7. 속도 증가 문제는 조사만 완료

반복 조향 시 총 이동 속도는 직선 평균 64.80 → 67.51km/h였다. 실제 위치 변화도 증가했고, 직선 방향 진행 속도는 오히려 낮았다.
전방 속도 성분을 기준으로 가속 여유를 계산하면서 횡속도가 남는 구조다.
**측정식을 바꾸거나 이동 벡터를 제한하는 수정은 승인되지 않았다.** 서버/이동 담당자와 최고 속도의 의미를 정하기 전에는 수정하지 않는다.
[원인·조건·수치](STEERING_SPEED_AUDIT.md)를 먼저 읽는다. 멀티플레이 재현은 미검증이다.

## 8. 로컬 증거와 남은 파일

현재 Codex 작업 폴더의 `outputs`에서 확인한다. 캡처·로그·실험 스크립트는 Git에 올리지 않는다.

- `RACE_HUD_REFERENCE_20261113/RIGHT_FINAL_VERIFICATION.json`: 현재 오른쪽 디자인의 해시·빌드·PIE·보호 확인.
- 같은 폴더의 `RIGHT_DRIVING_1920.png`, `RIGHT_SMALL_CUA.png`, `RIGHT_ITEM_USED_CUA.png`: 최종 화면 증거.
- `RACE_WBP_20261113/SPEED_PROBE_RAW.json`, `SPEED_ANALYSIS.json`: 속도 조사 원본.
- `RACE_HUD_20261113/FINAL_VERIFICATION.json`: 앞선 경기 단계·수명 검사 기록.

`REFERENCE_*` 캡처와 중앙·왼쪽 배치 스크립트는 폐기한 시도다. 최종 정본으로 쓰거나 재실행하지 않는다.
에셋 작성 스크립트는 일회성 도구다. 후속 수정은 현재 WBP를 열어 진행하며 자동 재생성으로 덮어쓰지 않는다.

아래 기존 미추적 문서 두 개는 이번 커밋·푸시에서 제외하고 원본을 유지한다.

- `Docs/Reviews/FrontendIntegration/KEVIN3_COMPARISON_AND_PR_SPLIT.md`
- `Docs/Reviews/FrontendIntegration/PR_DRAFT.md`

이번 요청은 현재 브랜치 푸시까지다. 후속 PR 등록·main 병합·새 브랜치 작업은 별도 지시에 따른다.
