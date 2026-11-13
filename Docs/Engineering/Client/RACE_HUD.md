# 주행 HUD — 속도 표시

## 화면에서 바뀐 점

실제 경기의 Racing 단계에서 내가 조종하는 차량의 속도를 오른쪽 아래에 표시한다.
기존 아이템 단독 테스트 맵 `/Game/Developers/user/L_Dev_Drive_ItemPickup`의 PIE에서도 확인할 수 있다.
아이템 슬롯·룰렛·Space 사용은 기존 구현을 사용한다.

## 추가한 코드

| 파일 / 함수 | 역할 |
|---|---|
| `Source/MW/UI/Gameplay/MWRaceHUD.h` / `SMWRaceHUD` | 주행 정보를 그리는 Slate 위젯. Controller·차량은 약한 참조로 보관한다. |
| `MWRaceHUD.cpp` / `Construct` | 기존 게시판·한글 글꼴·화면 비율 함수를 재사용해 속도판을 만든다. 숫자와 단위는 같은 행에서 판 안에 맞춘다. |
| `MWRaceHUD.cpp` / `Tick` | 소유 차량과 기존 경기 단계를 읽어 속도판 표시 여부를 정한다. |
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
