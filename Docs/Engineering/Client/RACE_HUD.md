# 경기 HUD 수정 위치와 구조

## 지금 사용하는 구현

속도 계기판과 왼쪽 위 경기 단계 표시는 `/Game/UI/Race/WBP_RaceHUD`에서 만든다.
이전 C++ Slate 계기판 `MWRaceHUD.*`, `MWRaceSpeedometer.*` 네 파일은 제거했다.
대기판·중앙 단계 표지판·아이템 슬롯은 `/Game/UI/Race/WBP_MatchPanels`에서 편집한다. `DefaultGame.ini`의 `MatchPanelsClass`가 이 WBP를 지정한다.

| 화면 | Designer 편집 위치 | 실행 시 상태와 입력 |
|---|---|---|
| 경기 대기판 | `WaitingBoard` 아래 `LogoAspect/Logo`와 `Pole`의 브러시·슬롯, `RosterPanel`의 배경·테두리·크기, 제목과 네 모서리 볼트 | `PreparingRace`에서 소유 Pawn이 없을 때 표시한다. `RosterHost`의 네 열은 GameState 참가자를 0.25초 간격으로 비교하고 내용이 바뀌면 갱신한다. 열 폭·셀 패딩·행 높이·폰트는 `WaitingBoardLayout`에 있다. |
| 단계 안내 | 중앙 앵커의 `SelectSign`, `BuildSign`에서 실제 문구, 색, 테두리 두께와 크기 | 기존 경기 단계가 선택·배치인지 판단한다. 로딩이 끝난 뒤 `ConstructionPromptSeconds`만큼 보인다. |
| 아이템 슬롯 | `ItemSlot` 좌하단 앵커와 여백, `Glass`의 색·그림자 높이 비율·재질, `ItemIconHost`의 안쪽 여백 | 경기 중 소유자의 아이템 변경으로 룰렛과 사용 페이드가 갱신된다. 작은 화면의 최소 픽셀과 최대 높이 비율은 `ItemSlotLayout`에서 편집한다. |
작은 왼쪽 위 단계판은 이 브랜치에서 추가한 안내로 원본 게임의 복구 UI가 아니다.

## 디자인을 바꾸는 순서

1. Content Browser에서 `UI/Race/WBP_RaceHUD`를 연다.
2. **Designer**에서 아래 표의 위젯을 선택한다.
3. 오른쪽 **Details**의 위치·크기·색·글꼴을 바꾼다.
4. **Compile → Save** 후 PIE에서 확인한다. 디자인 변경에 C++ 재빌드는 필요 없다.

| 바꿀 것 | Designer에서 선택할 것 | 편집 위치 |
|---|---|---|
| 계기판 전체 위치·크기 | `Panel_Speed` | Canvas Slot의 Anchors·Position·Size·Alignment |
| 속도 숫자·단위 글꼴과 색 | `Text_Speed`, `Text_Unit` | Appearance의 Font·Color·Shadow, Canvas Slot |
| 눈금 숫자의 위치·색·개수 | `Panel_Ticks`의 자식 TextBlock | 자식 위젯 추가·제거·배치. 숫자 값은 자식 순서에 따라 자동 분배 |
| 바늘 두께·길이·색·이미지 | `Image_Needle` | Canvas Slot, Color and Opacity, Brush |
| 바늘 회전 중심 | `Image_Needle` | Render Transform Pivot. 현재 아래 중앙 `(0.5, 1)` |
| 단계 표시 위치·크기·배경 | `Panel_Phase` | Canvas Slot, Brush Color |
| 단계 표시 글꼴·색 | `Text_Phase` | Font·Color |

`Class Defaults`에서 눈금 상한 `MaxSpeedKmh`, 바늘 시작·끝 각도, 바늘 응답 속도를 바꿀 수 있다.
현재 상한은 120km/h다. 숫자는 상한을 초과해도 실제 속도를 표시하고 바늘만 눈금 끝에 멈춘다.
`PhaseLabels`에서 각 경기 단계에 표시할 문구를 수정한다.
숫자 눈금을 추가·제거하면 눈금 값은 자동으로 나뉜다. 곡선의 눈금 간격과 숫자 위치는 함께 맞춰야 한다.
`BindWidget`으로 연결된 주요 위젯 이름은 유지한다. 이름을 바꾸면 Blueprint 컴파일에서 연결 누락을 확인해야 한다.

## 곡선 눈금 수정

`UI/Race/Materials/MI_RaceDial`을 열면 다음 파라미터를 편집할 수 있다.

| 항목 | 파라미터 |
|---|---|
| 눈금·속도 강조·배경 색 | `DialColor`, `AccentColor`, `FaceColor` |
| 배경 투명도 | `FaceOpacity` |
| 곡선 반지름·두께 | `ArcRadius`, `ArcWidth` |
| 큰·작은 눈금 개수·길이·두께 | `MajorTickCount/Radius/Width`, `MinorTickCount/Radius/Width` |
| 경계 부드러움 | `EdgeSoftness` |
| 곡선 시작점·표시 범위 | `StartTurns`, `SweepTurns` |

눈금 시작·끝을 바꾸면 WBP Class Defaults의 바늘 각도와 숫자 위치도 함께 맞춘다.
`SpeedRatio`는 실행 중 실제 속도로 갱신되므로 정적 디자인 조정값과 구분한다.
원본 `M_RaceDial`은 일반 Material 노드로 만든 UI 재질이다. Custom HLSL이나 작은 확대용 텍스처를 쓰지 않는다.
레퍼런스의 얇은 곡선·큰 숫자·투명한 면을 참고하고 기존 공통 글꼴과 파랑·노랑을 사용했다.
현재 게임에 없는 RPM·기어·브레이크 밸런스 값을 가짜로 표시하지 않는다.

## C++가 담당하는 부분

- `MWGameplayHUDSubsystem`: Config의 `RaceHUDWidgetClass`를 로드하고 월드당 WBP 한 개를 만든다. Transient UObject 참조로 수명을 유지한다.
- `MWRaceHUDWidget::NativePreConstruct`: WBP 눈금 숫자와 Designer 미리보기 값을 갱신한다. 위치·색·글꼴을 만들지 않는다.
- `SampleSpeed`: 기존 소유 차량의 `Velocity.Size2D() × 0.036`을 읽는다. 측정식은 이전 HUD와 동일하다. 샘플링은 최대 10Hz, 문자열은 정수가 바뀔 때만 갱신한다.
- `NativeTick`: 기존 Subsystem이 관찰한 경기 단계와 소유 Pawn을 읽는다. 표시용 바늘·속도 곡선만 보간한다.
- `NativeDestruct`: 차량 약한 참조·표시 캐시·MID를 해제한다. 기존 월드 HUD 제거 경로도 WBP 참조를 비운다.

서버 판정·RPC·Snapshot·차량 이동·트랙·EOS 코드는 수정하지 않았다.
기존 Slate 오버레이와의 연결에는 `TakeWidget()`을 사용하지만, 새 경기 HUD의 레이아웃과 그리기를 C++ Slate에서 생성하지 않는다.

## 새 에셋 전달 주의

프로젝트 `.gitignore`는 Content를 UVCS 관리 대상으로 제외한다. 이 정책을 바꾸거나 강제 Git 추가를 하지 않았다.
아래 새 에셋 세 개는 원본 MW 로컬에 저장돼 있으며 **UVCS 등록·업로드는 아직 하지 않았다.**
다른 PC에 코드만 받으면 새 HUD가 완성되지 않으므로 세 에셋도 함께 전달해야 한다.

- `Content/UI/Race/WBP_RaceHUD.uasset`
- `Content/UI/Race/Materials/M_RaceDial.uasset`
- `Content/UI/Race/Materials/MI_RaceDial.uasset`

## 2026-11-13 검증 결과

- 원본 MW Editor / Game Win64 Development 빌드 성공: 최종 C++ 기준 29.71초 / 31.97초.
- 실제 저장된 WBP와 재질을 로드한 에디터를 다시 실행해 PIE 확인. WBP 컴파일 성공, 눈금 7개·단계 문구 7개 연결 확인.
- 1920×1080 / 1296×759에서 숫자·단위·눈금·바늘이 잘리지 않고 아이템 슬롯과 겹치지 않는지 CUA 캡처 확인.
- 실제 속도 1,800cm/s에 숫자 65km/h·바늘·곡선 표시가 일치했다. 정지 0 표시도 확인.
- 기존 픽업 중첩을 테스트용 위치 이동으로 준비한 뒤, CUA Space로 Booster → None 소모 확인. 서버·아이템 로직은 수정하지 않았다.
- 최종 PIE 로그에 Error·assert·ensure 없음. BP·맵을 PIE 검증 중 저장하지 않았다.
- 검증 스크립트의 미노출 Python API와 명령 파일 읽기 경합은 외부 스크립트에서 수정 후 재실행했다. 제품 오류로 집계하지 않았다.
- 경기 단계별 전체 진행·EOS·멀티클라이언트·Cook/패키징은 이번 디자인 변경에서 재검증하지 않았다.

캡처·빌드 로그·에셋 해시는 Codex 작업 폴더 `outputs/RACE_WBP_20261113`에 보존한다.
속도 증가 현상의 별도 조사 결과는 [조향 속도 분석](STEERING_SPEED_AUDIT.md)을 참고한다.

## 오른쪽 계기판 디자인 보정

계기판은 기존 오른쪽 아래 배치를 유지한다. Anchors `(1,1)`, Position `(-42,-32)`, Size `(360,324)`를 변경하지 않았다.
레퍼런스는 계기판 내부 외형에만 적용했다. 중앙 속도판과 왼쪽 계기판 배치는 폐기했다.

- `WBP_RaceHUD`: 눈금 글꼴 크기 15 → 13, 속도 숫자 52 → 44와 자간 80, 단위 16 → 14.
- 바늘 폭 3 → 2, 길이 120 → 100. 흰색 계열로 바꾸고 허브도 8 → 5로 줄였다. 회전 중심은 유지한다.
- `MI_RaceDial`: 원형 배경을 비움(`FaceOpacity=0`), 곡선·대소 눈금을 가늘게 조절하고 흰색·옅은 파랑으로 표시한다.
- 밝은 배경에서도 눈금을 읽을 수 있도록 `Image_DialShadow`를 같은 WBP 내부에 얇게 추가했다.
- 색·글꼴·크기·배치는 여전히 WBP와 재질 인스턴스에서 편집한다. C++ 디자인 생성이나 중앙 표시용 별도 코드는 추가하지 않았다.
- 잘못 만들었던 `M_RaceReadoutPanel`은 참조가 0개임을 확인한 후 삭제했다. 원본 경로와 해시는 외부 검증 결과에 기록했다.

이번 변경의 C++·Config·주행·서버 코드는 이전 커밋과 같다. 속도 측정식도 변경하지 않았다.
실제 디자인 변경 파일은 WBP와 재질 인스턴스 두 개이며, 프로젝트 정책상 Git에서 제외되는 Content이므로 UVCS 인계가 필요하다.

최종 Editor/Game Development 빌드는 46.84초 / 54.08초에 성공했다. WBP 컴파일 후 에디터를 재시작한 PIE에서 0·65km/h 표시, 바늘 움직임과 1920×1080·1296×759 배치를 확인했다.
캡처 원본은 Codex 작업 폴더 `outputs/RACE_HUD_REFERENCE_20261113/RIGHT_*`이다. 이번 디자인 검증에서는 멀티클라이언트·EOS·Cook을 재검증하지 않았다.


## 아이템 룰렛·사용 연출 시간 설정

`Config/DefaultGame.ini`의 `[/Script/MW.MWGameplayHUDSubsystem]`에서 편집한다.

- `ItemRouletteSwitchTimes`: 획득 후 누적 초다. 기존값은 0.045, 0.095, 0.15, 0.215, 0.29, 0.38, 0.49, 0.63, 0.80, 0.99이며 마지막 시각에 실제 지급 아이템에 멈춘다. 각 값 사이의 간격을 늘릴수록 뒤로 갈수록 느려진다.
- `ItemUseDuration`: 사용 시 아이콘 축소·페이드 시간이다. 기존 0.20초를 유지한다. 축소 배율·보간식·슬롯 디자인은 변경하지 않았다.
- 예를 들어 두 번째 전환을 바꾸려면 두 번째 `+ItemRouletteSwitchTimes` 행의 값만 바꾼다. 값은 유한한 양수·오름차순이어야 한다. 마지막 값은 룰렛 전체 길이다. 설정 변경 후 새 실행에서 확인한다.
- 빈 시각 배열은 룰렛을 생략한다. 잘못된 순서·중복·음수·비정상 시각은 경고 후 룰렛을 생략한다. 사용 시간이 0이면 즉시 비우고, 음수·비정상 시간도 경고 후 즉시 비운다. 실제 지급·사용 상태를 바꾸거나 기다리지 않는다.
- Slate 화면 생성 때 설정을 한 번 복사·검증한다. 새 클래스·Tick·RPC를 추가하지 않으며 서버·아이템·주행 코드와 BP·맵은 수정하지 않는다.


### 룰렛 시간 설정 분리 검증 — 2026-11-16

- Editor/Game Development 빌드 성공: 40.88초 / 49.02초. 기존 MovieScene GetBindings C4996 경고는 남아 있다.
- 실제 Config의 전환 시각 10개(double)와 사용 시간(float)이 변경 전 C++ 값과 정확히 일치했다. WBP_RaceHUD 컴파일 성공, 에셋 저장 없음.
- 변경 전후 실제 PIE에서 기존 픽업 중첩으로 획득하고 CUA의 실제 Space로 소비했다. 룰렛은 실제 부스터에 멈췄으며 사용 후 슬롯은 비었다.
- 로그에서 관찰한 룰렛/사용 표시 시간(ms): 변경 전 1003/213, 변경 후 993/218. 설정은 990/200ms로 동일하며, 관측값 차이는 화면 갱신 경계를 포함한다. 캡처 간격으로 시간을 계산하지 않았다.
- 메모리에만 적용한 시험 설정(전환 0.10·0.25·0.60초, 사용 0.40초)의 실제 관측은 603/401ms였다. 설정을 실제로 소비하는지 확인한 시험이며 원본 Config에는 저장하지 않았다.
- 기존 MW.Items.PickupAndUse와 MW.UI.RaceHUD.StateLifetime 자동 검사 2개 통과. 보호한 코드·문서·BP 280개 SHA-256 유지 확인.
- 이번 변경에서는 전체 EOS·멀티클라이언트·Cook·패키징을 다시 검증하지 않았다. 원본 주행·서버·아이템 판정·BP·맵을 수정하지 않았고 UVCS 갱신·체크인과 원격 Push는 하지 않는다.
- 기록: 프로젝트 밖 `outputs/ITEM_ROULETTE_TIMING_20261116`의 CONFIG_AND_BP.json, BEFORE_PIE.json, AFTER_PIE.json, CUSTOM_PIE.json, 각 PIE 로그, Automation/index.json, FINAL_VERIFICATION.json.


## 표지판 정렬·중복 안내 정리 — 2026-11-17

- `WBP_RaceHUD` Class Defaults의 `PhaseLabels`에서 `PreparingConstruction`, `TrackPlacement`, `Racing` 문구를 제거했다. 트랙 선택·배치는 기존 전용 안내창을 사용하고, 원작에 없는 ‘주행 중’ 배지는 표시하지 않는다. 계기판 배치·디자인은 유지했다.
- `SMWGameplaySign::OnPaint`는 줄마다 폭을 측정해 각각 중앙에 놓는다. 글자 블록의 가로·세로가 마름모 안쪽에 들어오도록 같은 비율로 축소한다. 기존 문구·글꼴·판 모양·로딩 회전은 유지한다.
- 원본 프로젝트를 재실행한 2인 로컬 Listen Server PIE에서 로비 준비 → 차량 선택 → 트랙 선택·배치 → Racing을 실제 CUA 클릭으로 진행했다. 두 화면의 중복 단계 배지와 ‘주행 중’ 표시가 사라졌고 작은 창의 중앙 문구도 판 안에 들어왔다.
- 연속 주행은 엔진 `Input.+key/-key`로 입력을 주입했다. 물리 키보드 주행과 구분하며 속도·차량 위치·경기 단계·RPC를 직접 설정하지 않았다. 서로 다른 두 경기에서 FRED와 인비저블맨을 각각 호스트로 조종했다. 자국은 0→62개, 0→60개로 증가했고 실제 화면에도 표시됐다. 인비저블맨은 BP의 가상 바퀴 2개를 사용한다.
- 계기판 숫자·바늘의 가속/정지 갱신을 확인했다(표시 최대 83km/h, 정지 0). 소유 클라이언트의 정지 계기판도 표시됐다. 클라이언트의 연속 주행·원격 스키드 공유와 전 차량/전 테마를 이번에 모두 검증한 것은 아니다.
- Editor/Game Development 빌드 성공: 28.40초 / 35.59초. WBP 컴파일·저장·에디터 재시작 후 설정 재로드 확인. 서버·차량·트랙 코드는 변경하지 않았다. UVCS·Commit·Push는 실행하지 않았다.
- 현재 코드와 함께 변경된 `Content/UI/Race/WBP_RaceHUD.uasset`도 전달해야 한다. Content는 Git 제외 정책을 그대로 사용한다. 캡처·상태 관찰 로그는 프로젝트 밖 `outputs/RACE_SKID_PIE_20261117`에 둔다.


## 중앙 안내 글씨 확대·표시 시간 설정 — 2026-11-17

- 선택 안내는 ‘트랙을 / 선택하세요’ 두 줄이다. 일반 안내 글꼴 기준을 47→54로 높이고 검은색으로 표시한다. 표지판 크기·줄별 중앙 정렬을 유지하며 마름모 안쪽 면의 여백에 맞춰 축소한다.
- `SMWGameplaySign::FArguments`의 `AnimateLoading` 기본값을 false로 명시했다. 생략한 bool의 미초기화 때문에 일반 안내에 로딩용 크기·회전이 섞일 수 있던 부분이다. 로딩 호출부는 기존처럼 true를 명시한다.
- `DefaultGame.ini`의 `[/Script/MW.MWGameplayHUDSubsystem]` 아래 `ConstructionPromptSeconds=2.0`이 선택·배치 중앙 안내 시간이다. 단계 진입 후 기본 2초에 숨긴다. 0·음수·비정상 값은 생략한다. 화면 생성 때 한 번 읽고 기존 단계 시계를 사용하며 입력이나 경기 진행을 기다리게 하지 않는다.
- 최종 검은색 코드로 Editor/Game Development 빌드 성공(11.41초/16.91초). 재시작한 2인 로컬 PIE에서 실제 CUA 클릭으로 로비·차량·트랙 선택·배치 단계까지 진행했다. 호스트 렌더에서 선택 안내 1.25초 표시/2.50초 숨김, 배치 안내 1.343초 표시/2.546초 숨김을 확인했다. 캡처 관찰 시각이며 정확한 숨김 이벤트 시각은 아니다.
- Stop PIE 때 서버가 ‘건설 중 시작 참가자가 나갔습니다’ 오류 1건을 기록했다. UI 검증 중 오류·ensure·크래시는 없었고 서버 코드는 변경하지 않았다. 로딩 초기의 기존 Lumen 경고도 별도다. 모든 언어·해상도, 멀티 PC/EOS·전체 매치는 이번에 다시 검증하지 않았다.
- 실행 근거는 외부 `outputs/SIGN_READABILITY_20261117/Black`과 `RESULT.json`에 보존한다. Git 코드 커밋은 `821596c`이며 기존 BP 문구 변경은 별도 에셋 전달 대상이다.
