# 트랙 지면 관통 방지 검증

2026-11-24 KST · `fix/track-placement-clearance` · 기준 `34136de` · UE 5.8.4

## 원인과 판정 계약

이전 설원 경기에서 환경 바닥은 Z=0cm였고, 서버는 Z=-562.5cm에 피벗이 놓이는 조각을 승인했다. 이전에는 끝점 거리와 포즈만 검증했다. 피벗 높이는 주행면 높이를 뜻하지 않으므로, 새 판정은 양쪽 연결점과 트랙 카탈로그의 비트리거 블로킹 형상을 조각 로컬 좌표로 읽은 뒤 실제 설치 Transform으로 지면과 비교한다. 카탈로그 밖의 기존 BP에서 쓰는 차단 형상은 원래 충돌을 끄기 전에 읽고, 형상이 없는 장식용 캡에는 연결점 기준을 적용한다.

현행 테마의 생성 바닥은 평면이다. `MWTrackRuntimeSettings.TrackPlacementFloorZ`의 프로젝트 편집값을 서버 배치와 환경 생성기가 함께 읽고, 서버가 이번 세션의 바닥과 결승 조각 형상·포트를 클라이언트에 보낸다. 지면 높이는 클라이언트 장식 로딩 상태에 의존하지 않는다. 형상 단위 바운드를 쓰므로 경사진 비박스 메시나 곡면에는 보수적으로 판정될 수 있다. 지형 높이가 XY에 따라 달라지는 테마에는 별도의 권한 지표 계약이 필요하다.

클릭 순간 스냅 재판정과 서버 요청은 유지한다. 거절된 고스트는 유효 색·접점 섬광을 내지 않는다. 승인할 때 현재 조각과 이어 붙일 결승 조각까지 맞는지 검사해, 설치 중 다른 참가자가 이탈해도 열린 끝에서 코스를 닫을 수 있게 한다. 한 방향이 막히면 기존 Q/E로 반대 방향을 고른다. 두 방향이 모두 막히면 명시적인 복구 입력에 한해서 서버가 설정된 대체 조각의 유효 방향을 확인하고 제공한다. 대체 조각도 들어갈 수 없는 경우 그 참가자의 조각을 건너뛴다.

## 변경 위치

| 파일 | 내용 |
| --- | --- |
| `Gameplay/Construction/MWTrackPlacementClearance.cpp/.h` | 로컬 형상 샘플과 지표, 후보·결승 조각의 공통 검사 |
| `Gameplay/Construction/MWTrackConstructionPlacement.cpp` 및 관련 헤더 | 승인 전 검증, 열린 끝과 결승 조각 검사, 대체 또는 건너뛰기 처리 |
| `Gameplay/Construction/MWTrackPlacementViewComponent.cpp/.h` | 미리보기 판정, Q/E와 복구 안내, 대체 고스트 갱신 및 회신·복제 순서 처리 |
| `Gameplay/Construction/MWTrackPlacementTypes.h` | 서버가 승인하는 세션의 바닥·결승 형상과 거절/복구 요청·응답 |
| `Gameplay/Track/MWTrackRuntimeSubsystem.cpp/.h` | 이미 조회하는 트랙 정의를 공통 재사용 |
| `Gameplay/Track/MWTrackRuntimeSettings.h`, `Config/DefaultGame.ini` | 공용 바닥 높이와 대체 트랙 BP 설정 |
| `MWEnvironmentGameplay.cpp` | 환경 생성기의 평면 바닥 기준을 같은 설정에서 읽음 |
| `Tests/MWTrackSnapTests.cpp`, `Tests/MWVehicleMovementTests.cpp` | 지표 판정 회귀 검증, 점수·서든데스 검증용 합성 레벨의 출발선 높이 보정 |

`Gameplay/`와 `Tests/`는 `Source/MW/` 기준이다. 안내는 기존 WBP의 배치 상태 슬롯을 그대로 사용한다. 맵·WBP 바이너리는 이 변경 세트에 포함되지 않는다.

## 3인 PIE

설원 `L_Dev_Drive`에서 리슨 호스트와 원격 두 창으로 실행했다. Cua Driver로 전원 준비·차량 선택·트랙 선택·설치와 방향 입력을 했다. 후보 풀, 지면 값, 설치 위치는 테스트용으로 변경하지 않았다. P1/P2/P3은 794×514 / 1099×517 / 1384×393, UMG 배율 0.47545 / 0.47822 / 0.44400이다.

| 검증 | 결과 |
| --- | --- |
| P1 하강 곡선 | `WoodAutoExit_Up_Left_135`의 source 1을 설치해 열린 끝을 Z=1000cm까지 낮췄다. 서버 piece 2 승인. |
| P2 하강 방향 클릭 | `WoodElevated_Right_90_Small`의 source 1, target 2/0. “지면에 걸립니다 · Q/E로 방향 변경” 안내. 서버 `Below floor` 거절. |
| P2 E로 회전 후 동일 위치 클릭 | 같은 target 2/0의 source 0. 서버 piece 3 승인. 클릭 포즈와 고정 고스트 위치 차이 0cm. |
| P3 마무리 | `WoodStraight_PowerUps`를 piece 4로 설치. 마지막 piece 5는 서버가 `WoodFinish`로 생성했다. |
| 세 창 출발 | `PreparingRace → Countdown → Racing`을 통과했다. FRED·DOUG·MAMMOTH가 각자의 차량을 소유하고 X=-2620/-2620/-2870cm의 실제 그리드에서 시작했다. |

```text
09:59:17  Placed piece=2 member=1 target=1/0 source=1
09:59:37  Below floor member=2 session=1 target=2/0 source=1 floor=0.000
09:59:59  Placed piece=3 member=2 target=2/0 source=0
10:00:10  Placed piece=4 member=3 target=3/1 source=1
10:00:13  Countdown -> Racing (Round=1 Epoch=1, Course=1/5)
```

위 시각은 원본 PIE 로그의 UTC를 KST로 옮긴 것이다. 증거는 로컬 검증 폴더 `work/track-placement-clearance-2026-11-24`의 `pie.log` 2928–3153행, `racing-result.json`, `ground-rejection-p2.png`, `ground-approved-p2.png`, `racing-1.png`~`racing-3.png`다.

PIE 이후 로그 대조에서 거절 자세에 접점 섬광이 한 번 재생되는 것을 발견했다. 고스트 보간의 스냅 진입도 공통 유효 판정 이후에만 허용하도록 보완했다. 위 화면 캡처는 이 보완 전의 실행이다. 두 방향 모두 막히는 경우의 판정은 자동화에서 확인했고, 대체 수령 및 대체까지 막혀 건너뛰는 조합은 화면에서 별도로 재현하지 않았다. 다른 테마, 패키지, 별도 PC 접속은 이번 검증 범위 밖이다.

## 빌드·자동화

- `MWEditor Win64 Development` 최종 빌드가 성공했다. `editor-build-verified.log`에 결과를 남겼다.
- `Automation RunTests MW.Construction+MW.Gameplay`의 10개 테스트가 모두 통과했다. 바닥 아래 피벗·높은 주행면, 기울어진 조각 모서리, 결승 여유, 방향 변경, 양방향 불가, 기존 스냅, 경기 진행과 서든데스를 확인했다. 결과는 `automation-verified.log` 1901–2276행에 있다.
- 첫 라운드 자동화는 합성 맵의 출발선을 Z=0에 두어서 새 기준에서 시작 조각이 거절됐다. 이 테스트는 점수와 라운드 확장을 검증하므로 합성 출발선에 충분한 높이 여유를 줬다. 원래 플레이 맵의 시작 위치와 판정은 그대로다.
- 이 작업은 Git의 C++·설정·문서만 수정했다. 에셋 체크인, 푸시, 내부 통합과 main 병합은 하지 않았다.
