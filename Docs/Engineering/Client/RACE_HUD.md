# 주행 HUD — 속도와 기존 경기 단계

## 화면에서 바뀐 점

실제 경기의 Racing 단계에서 내가 조종하는 차량의 속도를 오른쪽 아래에 표시한다.
기존 아이템 단독 테스트 맵 `/Game/Developers/user/L_Dev_Drive_ItemPickup`의 PIE에서도 확인할 수 있다.
아이템 슬롯·룰렛·Space 사용은 기존 구현을 사용한다.

## 추가한 코드

| 파일 / 함수 | 역할 |
|---|---|
| `Source/MW/UI/Gameplay/MWRaceHUD.h` / `SMWRaceHUD` | 주행 정보를 그리는 Slate 위젯. Controller·차량은 약한 참조로 보관한다. |
| `MWRaceHUD.cpp` / `Construct` | 기존 게시판·한글 글꼴·화면 비율 함수를 재사용해 속도판을 만든다. 숫자와 단위는 같은 행에서 판 안에 맞춘다. |
| `MWRaceHUD.cpp` / `Tick` | 소유 차량과 Subsystem이 관찰한 기존 경기 단계를 읽어 속도판 표시 여부를 정한다. |
| `MWRaceHUD.cpp` / `UpdateSpeed` | 기존 Movement의 속도를 km/h로 바꿔 표시한다. 차량 교체·파괴 시 표시 이력을 비운다. |
| `MWGameplayHUDSubsystem.cpp` / `SMWGameplayHUD::Construct` | 기존 오버레이에 위젯을 한 개 연결한다. 추가 Subsystem·플러그인은 없다. |

계산식은 `sqrt(Velocity.X² + Velocity.Y²) × 0.036`이다. UE의 cm/s를 km/h로 바꾸며, 낙하 속도는 제외하고 후진도 양수로 표시한다. 위치 차분을 사용하지 않아 텔레포트나 위치 보정을 주행 속도로 오인하지 않는다.

표시는 초당 최대 10번 갱신한다. 반올림한 정수가 그대로면 `SetText`를 호출하지 않는다. 차량 전체 검색·별도 Timer·RPC·복제 필드는 추가하지 않았다.

GameMode, GameState, Pawn, Movement, Snapshot, 트랙, BP, Config는 수정하지 않았다. 기존 World HUD가 종료될 때 새 Slate 위젯도 함께 제거된다.

## 신규 검증 — 2026-11-13

- 원본 MW Editor / Game Win64 Development 빌드 성공. 최종 배치 수정 후 각각 15.00초 / 31.79초.
- 실제 렌더링 PIE에서 정지 0 → 가속 → 1,800cm/s의 65km/h 표시 → 감속 후 0 확인.
- 1920×1080 / 1296×759에서 숫자·단위·아이템 슬롯이 잘리지 않는지 캡처로 확인.
- 기존 맵의 픽업 중첩으로 부스터를 받은 뒤 CUA Space 입력으로 소모되는 것을 확인. 픽업 준비에는 테스트용 위치 이동을 사용했다.
- CUA W 단타로 실제 차량 이동을 확인했고, 지속 가속 수치 비교는 엔진 강제 키 입력으로 수행했다. 사람의 실제 키보드 주행이나 멀티클라이언트 검증과 구분한다.
- 에셋을 저장하지 않았다. 최초 계측 스크립트의 Python API 오타는 수정 후 재실행했다.

캡처와 계측 원본은 Codex 작업 폴더의 `outputs/RACE_HUD_20261113`에 있다. Git에는 실행 코드와 이 설명서만 포함한다.

## 기존 경기 단계 연결

왼쪽 위에 서버가 정한 현재 단계를 표시한다. 순서를 진행시키거나 카운트다운 시간을 만들지 않는다.

| 서버 단계 | 표시 |
|---|---|
| PreparingConstruction | 트랙 선택 |
| TrackPlacement | 트랙 배치 |
| ElementPlacement | 위험 요소 배치 |
| PreparingRace | 경기 준비 |
| Countdown | 출발 대기 |
| Racing | 주행 중 + 오른쪽 아래 속도판 |
| Failed | 경기 진행 오류 |
| 로비·차량 선택·이동 등 나머지 단계 또는 GameState 부재 | 새 경기 HUD 숨김 |

아이템 단독 테스트 맵은 이전처럼 슬롯과 속도만 표시한다. GameState가 없는 테스트 맵에서 경기 단계를 가짜로 만들어 표시하지 않는다.

### 연결 코드를 확인하는 순서

1. `MWGameplayHUDSubsystem.h`의 `GetObservedRoomPhase()`는 관찰 중인 단계만 읽는 C++ 조회 함수다. setter나 RPC가 아니다.
2. `MWGameplayHUDSubsystem.cpp`의 `RefreshGameStateBinding()`은 현재 월드의 유효한 GameState를 찾아 이전 이벤트를 해제하고, 새 이벤트를 구독한 뒤 현재 값을 읽는다.
3. `HandleGameplayStateChanged()`는 기존 `OnGameplayStateChanged` 알림을 받아 캐시를 갱신한다. 호스트의 setter 알림과 클라이언트의 RepNotify는 기존 GameState가 같은 이벤트로 전달한다.
4. `IsTickable()`은 최초 연결·GameState 교체·제거 정리에 필요한 때만 연결 틱을 켠다. 오버레이 생성 전에 원본이 사라져도 캐시를 정리한다.
5. `MWRaceHUD.cpp`의 `GetRoomPhaseLabel()`과 `UpdatePhase()`가 내부 enum을 위 표의 한글 문구로 바꾼다. 같은 단계면 텍스트를 다시 만들지 않는다.
6. `RemoveOverlay()`는 월드 종료·Subsystem 종료 때 실제 구독한 GameState·Controller·ItemComponent와 Viewport 연결을 해제한다. 이전 월드의 객체를 새 HUD에 넘기지 않는다.

### 검사 코드와 실제 검증

`Source/MW/Tests/MWRaceHUDTests.cpp`는 기존 Unreal Automation 방식이며 Shipping에는 포함되지 않는다.

- `MW.UI.RaceHUD.StateLifetime`: 에셋·온라인 연결 없이 초기 값, setter 이벤트, GameState 교체·제거, 이전 이벤트 무시, 오버레이 준비 전 원본 제거, 월드 종료 정리를 검사한다.
- `MW.UI.RaceHUD.PIEPhase.*`: `RequiresUser`로 분리한 수동 PIE 검사 8개다. 기존 `SetRoomPhaseFromServer()`를 호출하고 HUD가 같은 값을 받는지 확인한다. 에디터 자동화가 기존 PIE를 종료할 수 있으므로 검사 시작 후 15초 안에 `MWGameplayGameState`가 있는 새 테스트 PIE를 준비해야 한다.
- `-MWRaceHUDCaptureDir=<출력 폴더>`가 있으면 수동 검사가 다음 Slate 프레임의 실제 게임 화면을 캡처한다. 값이 없으면 캡처 파일을 만들지 않는다.

최종 코드에서 Editor / Game Win64 Development 빌드 성공: 각각 11.77초 / 16.25초. 위 자동화 1개와 수동 PIE 8개, 총 9개가 모두 성공했다. 단계별 PNG도 직접 확인했다. 1920×1080 / 1296×759 CUA 캡처에서 배치를 확인했다.

PIE 준비에는 기존 개발 맵의 GameMode를 메모리에서만 바꾸고 기존 배치 차량을 재사용했다. 원본 설정은 종료 때 복원했고 맵·BP·Config는 저장하지 않았다. 이전 검증 실행에서는 소유 Pawn을 다른 배치 차량으로 교체하고 이전 Pawn을 제거한 뒤 UI 참조 오류가 없는 것도 확인했다.

이것은 UI 연결 검증이다. 실제 서버가 단계를 진행시키는 판정, 멀티클라이언트 복제, EOS 왕복, 소리, Cook·패키징을 통과했다는 뜻이 아니다. 현재 서버의 실제 숫자 카운트다운 API는 없으므로 Countdown 단계의 문구만 연결했다.

검증 준비 중 Python 바인딩·에디터 SET 제한·자동화의 PIE 종료 동작과 DLL 잠금으로 실패한 기록은 외부 작업 폴더에 보존했다. 최종 판정은 `FINAL_EDITOR_BUILD.log`, `FINAL_GAME_BUILD.log`, `FINAL_PIE.log`와 `FINAL_VERIFICATION.json`을 기준으로 한다. 일반 엔진의 선택적 프로파일러 DLL 탐색 경고는 게임 에셋 누락과 구분한다.
