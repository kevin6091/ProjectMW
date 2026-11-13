# 경기 HUD 수정 위치와 구조

## 지금 사용하는 구현

속도 계기판과 왼쪽 위 경기 단계 표시는 `/Game/UI/Race/WBP_RaceHUD`에서 만든다.
이전 C++ Slate 계기판 `MWRaceHUD.*`, `MWRaceSpeedometer.*` 네 파일은 제거했다.
기존 아이템 슬롯·중앙 표지판·대기판은 이번 전환 대상이 아니며 기존 구현을 유지한다.
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
