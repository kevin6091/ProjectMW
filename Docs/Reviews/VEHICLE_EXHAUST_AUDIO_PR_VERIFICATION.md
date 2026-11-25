# 차량 일반 배기·부스터 전환·엔진음 검증

## 기준과 수정 위치

- Git 기준은 `origin/main`의 `1b57b3b9c7781129e850403867b88225b89fe6ba`다. `feature/vehicle-exhaust-audio`는 차량 배기와 엔진음 변경을 분리한다.
- 일반 배기 Niagara `Content/Gameplay/Vehicles/Art/Exhaust/NS_MW_VehicleExhaust.uasset`은 UVCS cs:67, 엔진 루프 `Content/Gameplay/Vehicles/Art/EngineAudio/SW_MW_EngineExhaustLoop.uasset`은 cs:66에서 들어왔다. 격리 프로젝트의 두 파일을 cs:80 서버 원본과 대조했다. 이 PR의 UE 바이너리 변경은 0개다.

| 경로 | 변경 |
| --- | --- |
| `MWVehiclePawn.*` | 기존 차체·바퀴 표시를 갱신한 뒤 로컬 배기와 엔진음을 갱신한다. Pawn 종료 때 두 효과를 정리한다. |
| `MWVehicleExhaust.cpp` / `MWVehicleExhaustSettings.h` | 차체 충돌 박스 기준 뒤쪽 양쪽에 배기를 붙인다. 정차·경기 단계·부스터에 맞춰 Niagara를 켜고 끈다. 위치·크기·발생 속도는 프로젝트 설정에서 읽는다. |
| `MWItemComponent.h` / `MWItemComponentVisual.cpp` | 각 창에서 현재 표시 중인 부스터 분사구를 사용한다. 부스터가 붙기 직전에 일반 배기를 즉시 끈다. 서버 아이템 부여·사용과 기존 복제 필드는 유지한다. |
| `MWVehicleEngineAudio.cpp` / `MWVehicleEngineAudioSettings.h` | 로컬 예측 또는 서버 스냅샷의 종방향 속도로 피치·게인을 계산한다. 운전자 창은 페달 반응도 사용한다. 차량별 공간음 루프와 감쇠 범위를 설정한다. |
| `DefaultGame.ini` / `MW.Build.cs` | 두 에셋의 소프트 참조·연출값·쿠킹 대상 경로와 Niagara 모듈 의존성을 연결한다. #22의 설치 VFX 등록과 떨어진 줄에 차량 경로를 등록했다. |

## 초기 3인 PIE와 회귀 결과

- UE 5.8.4 Win64 Development Editor 빌드 성공. `MW.Vehicle` 9개, `MW.Items.PickupAndUse` 1개, `MW.Gameplay.DriveControl` 4개 총 14개를 통과했다. 이번 기준의 `MW.Vehicle.WallGroundContact`도 통과했다.
- 최초 cs:80 구성에서는 `main`의 `MWTrackSelectionWidget::Refresh`가 필수 바인딩 `Text_Players`를 읽는 중 종료됐다. cs:80 선택 WBP에 해당 노드가 없으며 #21에서 선택 바인딩으로 처리한다. 실제 차량 검증에서는 격리 프로젝트의 선택 WBP 한 개만 호환되는 cs:62 서버 원본으로 교체했다. 차량 배기·음향 에셋은 cs:80을 사용했다. 선택 WBP cs:62 해시는 `5C646421F2CDA85E90E69869C7B62CD52AC3AF837A8ECC12140DCE252A00BA1B`다.
- Cua Driver로 P1·P2·P3이 서로 다른 후보를 확정하고 설치했다. 출발 조각과 종료 캡까지 5조각이 됐고 세 창 모두 실제 설원 출발선에서 `Racing`을 시작했다. [실제 출발 화면](vehicle-exhaust-audio-evidence/racing-start.png)
- 계속 밟는 입력과 감속은 격리 PIE의 기존 차량 입력 경로를 사용해 반복 가능한 상태를 만들었다. 같은 P2 DOUG가 정차→가속→감속하는 동안 세 창 모두 루프가 차량당 하나였고, 피치·게인이 각각 증가한 뒤 정차 값에 가까워졌다.
- 일반 배기 두 개를 DOUG의 차체 기준 `(-80.64, ±6.80, 4.50) cm`, 배율 `0.32`로 확인했다. 실제 코스에 테스트용 기존 픽업을 잠깐 배치하고 서버 충돌로 부스터를 받았다. P2 창에서 Cua Driver로 Space를 눌렀고 자동 종료까지 관찰했다. 부스터 전환 동안 관찰 시간을 확보하려고 격리 PIE의 시간 배율만 0.32로 설정했다. 세 창 각각 부스터 활성 49개 표본에서 일반 배기 동시 활성은 0개였다. [사용 전](vehicle-exhaust-audio-evidence/exhaust-before-p2.png), [종료 뒤](vehicle-exhaust-audio-evidence/exhaust-after-p2.png)

| 창의 P2 관측 | 정차 피치 / 게인 | 가속 최대 | 감속 뒤 | Niagara / 부스터 전 → 중 → 후 |
| --- | --- | --- | --- | --- |
| P1 호스트에서 원격 차 | 0.82 / 0.10 | 1.23937 / 0.23343 | 0.82279 / 0.10089 | 2/0 → 0/1 → 2/0 |
| P2 운전자 | 0.82 / 0.10 | 1.31624 / 0.25789 | 0.82467 / 0.10149 | 2/0 → 0/1 → 2/0 |
| P3에서 원격 차 | 0.82 / 0.10 | 1.23601 / 0.23237 | 0.82324 / 0.10103 | 2/0 → 0/1 → 2/0 |

거리 감쇠는 P2의 소스 위치를 기준으로 임시 청취점을 옮기고, Unreal 오디오 디버거의 거리 감쇠 적용 후 엔진 웨이브 볼륨으로 확인했다. 200cm `0.10`, 2200cm `0.05`, 4800cm `0.00`, 200cm 복귀 `0.10`이었다. 차량의 컴포넌트 게인은 이 검사에서 0.10으로 유지됐다. [계측·에셋 해시·크래시 발췌](vehicle-exhaust-audio-evidence/verification.txt)

## 인계와 병합 조건

- 차량 소스는 #21·#22의 API에 의존하지 않는다. GitHub Draft 기준은 `main`이다. **팀의 cs:80 전체 구성을 쓰는 경기 재검증에는 #21의 WBP 호환 수정이 필요하다.**
- 현재 main과 #21·#22 모두에 대해 작업 트리를 바꾸지 않는 병합 모의검사를 실행한다. 실제 반영 전 최신 main으로 다시 대조한다.
- 세 창의 눈 테마 1회와 위 범위의 정량 검사다. 별도 프로세스와 패키지, 참고 영상과의 음색 대조는 이번 실행 범위에 포함하지 않았다.
- 되돌릴 경우 차량 연출 커밋을 revert한다. UVCS에 새 변경 세트가 없어 에셋 되돌리기는 필요하지 않다.

## cs:80 전체 구성 보충 검증

- #21 `ea98d74`와 #23 `ef8b318`을 병합 모의 트리 `6d497980187b92670779b8a5ef242fc1f485fcf9`로 만든 격리 에디터에서 확인했다. UVCS 작업공간의 cs:80 추적 파일을 복사했다. UVCS에 없는 개인 충돌 메시는 제외했다. 서버에서 별도로 내려받은 경기 레벨·트랙 카탈로그·선택/경기 WBP·배기/엔진음 에셋 6개가 시험 사본 해시와 일치했다.
- UE 5.8.4 Win64 Development Editor 빌드 및 선택 WBP·경기 HUD WBP·플레이어 컨트롤러 BP 컴파일 3개 성공. Cua Driver 3인 PIE에서 세 명이 각자 후보를 확정하고 설치했다. 코스 5조각이 완성됐고 세 창 모두 `Racing`에 들어갔다. [실제 출발선](vehicle-exhaust-audio-evidence/cs80/cs80-racing-start-p1.png)
- P2 DOUG가 서버 픽업 충돌로 부스터를 받고, Cua Driver에서 P2의 Space 입력으로 사용했다. 세 창 모두 전·중·후 일반 배기/부스터가 `2/0 → 0/1 → 2/0`이었다. 부스터 활성 64개 표본씩에서 동시 활성은 0건이었다. 픽업과 시간 배율 0.22는 격리 PIE의 시험 보조다. [P1 원격](vehicle-exhaust-audio-evidence/cs80/cs80-booster-active-p1.png), [P2 본인](vehicle-exhaust-audio-evidence/cs80/cs80-booster-active-p2.png), [P3 원격](vehicle-exhaust-audio-evidence/cs80/cs80-booster-active-p3.png), [P2 배기 복귀](vehicle-exhaust-audio-evidence/cs80/cs80-exhaust-after-p2.png)
- 운전자와 두 원격 창에서 엔진 루프 1개를 확인했다. 정차 피치/게인 `0.82/0.10`에서 가속 최대값은 P1 `1.12356/0.19659`, P2 `1.21850/0.22679`, P3 `1.12381/0.19667`이었다. 감속 뒤 세 창 모두 피치 0.825 아래, 게인 0.102 아래로 돌아왔다. 믹서 거리 적용 뒤 볼륨은 200cm `0.10`, 2200cm `0.05`, 4800cm `0.00`, 200cm 복귀 `0.10`이었다.
- 차량·아이템·주행 입력 14개, 라운드·건설·커서 8개를 별도 프로세스에서 통과했다. 두 그룹을 한 프로세스에서 연달아 실행한 첫 시도는 `UNetConnection::SetClientLoginState`의 가비지 컬렉션 중 종료됐다. 실제 PIE는 정상 종료했다. [서버 해시·3창·오디오·자동화 로그](vehicle-exhaust-audio-evidence/cs80/verification.txt)

cs:80 전체 구성에서 #21·#23을 함께 적용한 경기 진입과 차량 연출을 확인했다. #23 단독과 cs:80 선택 WBP의 기존 호환 문제는 #21이 해결한다. 이 실행은 설원 3인 PIE 기준이다.
