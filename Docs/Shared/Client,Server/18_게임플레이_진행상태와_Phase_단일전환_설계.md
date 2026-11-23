# 게임플레이 진행 상태와 Phase 단일 전환 설계

- **문서 번호:** MW-GAMEPLAY-FLOW-018 / v1.4
- **작성:** 2026-11-12 / Client + Server
- **기준:** fix/Network-Prediction, HEAD 772d2e0, 조사 시작 시 작업 트리 깨끗함
- **엔진:** 설치 UE 5.8.4 / CL 58902052, MW.uproject의 EngineAssociation은 5.8
- **상태:** ① 개인 상태, ② 경기 제어, ③ 출발 흐름과 개인 완주에 이어 2026-11-23 확정 규칙의 점수·전원 종료·결과 스냅샷·코스 확장·4000점 종료·서든데스를 기존 클래스에 연결했다. 최신 구현 계약은 0.8~0.9절이다. 아래 772d2e0의 출발점·미구현 표기는 초안 작성 당시 기록이다.

현재 구현은 사용자의 작은 구조 요구에 따라 검증된 완료 콜백/RPC/타이머에서 곧바로 `EnterPhase`를 호출한다. 초안의 `RequestFlowUpdate`/`TryAdvanceGameplay` 예약 계층과 GameMode의 완료 세션 복사본은 도입하지 않았다. 이후 절에 남은 해당 이름은 초안 제안이며 현재 구현 API가 아니다.

## 0. 라운드 반복과 경기 종료 · 2026-11-23

### 0.1 이번 범위와 완료 조건

`Racing → 전원 종료 → RoundResults → PreparingConstruction → TrackPlacement → PreparingRace → Countdown → Racing`을 같은 월드에서 반복한다. 새 Manager·Subsystem·단계별 객체·맵 이동은 추가하지 않는다. 점수·최종 승리·서든데스를 포함하고 주행 제한시간·재경기는 후속 범위다. 일반 경기에서 체크포인트로 복귀한 사람은 같은 라운드를 계속 달리며 완료 인원으로 세지 않는다.

첫 구현은 현재 코스의 서버 결승 트리거를 완주 기준으로 연결한다. 체크포인트 순서·방향·점수의 정식 판정은 후속 경기 규칙 작업에서 보강한다. 기존 복귀 구현은 재사용한다.

사용자 지시로 `MWRaceStartTests.cpp`와 그 테스트 전용 friend 선언을 삭제했다. 다른 종류의 테스트는 유지한다. 본문의 과거 RaceStart 실행 결과는 당시 기록이며 현재 실행 가능한 테스트 목록이 아니다.

### 0.2 상태와 소유자

| 기존 소유자 | 추가 또는 변경할 최소 상태·역할 |
|---|---|
| `FMWPlayerRoundState` / PlayerState | 기존 Waiting=0, Active=1을 유지하고 Finished=2를 추가한다. FinishOrder=0은 미완주이며 수락된 완주에 서버가 1부터 순서를 부여한다. equality와 기존 NetSerialize도 같은 필드를 처리한다. 개인 상태가 전원 완료 집계의 원본이다. |
| GameMode | 서버 전용 `MatchMembers` 멤버 ID 집합 하나를 최초 명단에서 구성한다. 이탈 시 제거하고 새 라운드까지 남은 명단을 유지한다. 다음 완주 순서 카운터와 기본 3초를 제안하는 결과 대기 프로퍼티를 둔다. |
| GameState | 기존 `FMWGameplayProgress`에 `ResultsEpoch`와 `TArray<FMWPlayerRoundState> Results`를 추가한다. 별도의 결과 구조체 없이 개인 상태를 복사하고 Phase와 함께 NetSerialize한다. 결과 진입 때 확정하고 다음 결과까지 보존한다. |
| TrackRuntimeSubsystem | 기존 Racers의 체크포인트·복귀 위치·경과 시간 계산을 유지한다. 새 Epoch의 Racing 진입 직전에 이번 차량들의 기록을 초기화하고 StartTime을 서버 출발 시각으로 설정한다. 완주 기록은 GameMode가 승인한 뒤에만 기존 조회 호환용 bFinished에 반영한다. |
| Construction | 기존 PlacedTracks·CourseRevision을 코스 원본으로 유지한다. 서버 전용 `FMWOpenTrackEnd NextRoundEnd`에 종료 캡 직전의 열린 끝을 저장하고 Id 유효성으로 초기 상태를 구분한다. |

`RoundIndex`는 표시 번호, `RoundEpoch`는 실행 구분자다. 첫 라운드는 기존 1/1이고, RoundResults에서 다음 선택 단계로 갈 때 둘을 한 번 증가시킨다. 선택·배치·준비·카운트다운·주행 사이에서는 Epoch가 바뀌지 않는다.

### 0.3 완주 이벤트와 개인 상태

1. 기존 `MWTrackRuntimeComponent::OnTrigger`가 서버의 결승 겹침을 감지하고 기존 `ProcessRaceTrigger`로 전달한다.
2. Gameplay 월드에서는 `GameMode.TryFinishPlayerRace(Vehicle, FinishTrack, RoundEpoch)`라는 일반 서버 C++ 진입점으로 승인받는다. 새 Client RPC나 NetMulticast는 필요 없다. 개인 완주 단위에서 이 API를 구현했다.
3. GameMode는 Racing·현재 Epoch·MatchMembers·정상 소유 차량·개인 Active·현재 코스의 마지막 종료 트랙을 검사한다. 중복 Finished, 이전 Epoch, 다른 코스 결승은 상태를 바꾸지 않는다.
4. 수락한 플레이어에게만 Finished와 FinishOrder를 저장한다. 기존 PlayerState Notify → Pawn.RefreshDrivePermission → CanDrive 경로로 입력·속도·충돌 회전·이동 스트림을 정리한다. 이동/표시 전체 Tick은 끄지 않는다.
5. 완주 차량이 결승선을 막지 않도록 충돌 루트의 질의 충돌을 서버와 표시 클라이언트에서 끈다. 기존 충돌 설정을 보관하고 다음 PreparingRace에서 출발 위치로 재배치한 뒤 복원한다. 단순히 다음 선택의 Waiting 상태가 도착했다는 이유로 결승선에 겹쳐 있는 차량들의 충돌부터 다시 켜지 않는다. 이 적용은 RefreshDrivePermission의 허용 bool 조기 반환보다 앞에서 개인 상태와 준비 상태로 평가한다. 다른 차량의 Controller가 없는 원격 표시 차량도 개인 Finished로 판정해야 한다. 서버 아이템 사용도 개인 주행 허용을 확인한다.
6. GameMode가 `TryEndRound()`로 남은 참가자의 개인 상태를 재조회한다. 한 명이라도 현재 Epoch의 Active면 주행을 유지한다. 전원 Finished이면 `EnterPhase(RoundResults)`를 호출한다. 완료 인원 카운터를 별도 원본으로 보관하지 않는다.

복귀는 기존 `RespawnAtTrackCheckpoint → Pawn.RespawnAtCheckpoint`를 사용하며 Finished로 바꾸지 않는다. Runtime의 기록 초기화는 Racing 진입 전 한 번 수행한다. 출발 트리거가 준비 단계에서 이미 겹쳤더라도 새 Overlap을 기다리지 않고 모든 참가자의 출발 시각을 설정한다. Gameplay가 아닌 기존 DriveTest의 체크포인트 경로는 보존한다.

### 0.4 결과와 다음 Epoch

`EnterPhase(RoundResults)`는 Racing에서만 허용하고 전원 완료를 다시 확인한다. 개인 상태를 Waiting으로 덮지 않고 Finished/Eliminated로 보존한 채 결과 스냅샷을 한 번 만든다. 기존 `RacePhaseTimer`와 `Progress.PhaseEndServerTime`을 재사용해 결과 대기를 예약한다. `WaitForRacePhaseDeadline`는 기존 Phase/Epoch/코스/기한 일치 검사 후 목표 도달 인원에 따라 다음 건설·서든데스 준비·최종 결과를 선택한다. 클라이언트의 표시 타이머는 전환을 결정하지 않는다. 결과 진입 처리를 끝내면 반드시 return하여 실패 정리 경로로 내려가지 않는다.

`EnterPhase(PreparingConstruction)`에는 최초 진입과 RoundResults 이후 진입 두 경우만 둔다. 다음 라운드에서 Epoch/표시 번호를 증가시키고, 남은 MatchMembers를 Waiting·FinishOrder=0으로 초기화하며 기존 준비 보고 집합을 비운다. 종료된 이전 선택 SessionId가 0이 아니라는 이유로 반복을 거절하지 않는다. 기존 BeginSelection이 새 선택 세션을 발급하도록 한다. 결과 스냅샷과 설치된 도로는 이 단계에서 지우지 않는다.

처음 접속 완료를 기다릴 때는 기존 GameInstance.StartingPlayers 전원 확인을 유지한다. 시작 후의 CollectStartingControllers는 MatchMembers에 속한 정상 Controller를 기존 시작 순서로 수집한다. 준비·카운트다운 중 이탈은 기존 Failed 정책을 유지한다. Racing 중 이탈은 명단에서 제거하고 TryEndRound를 호출한다. RoundResults 중 이탈은 다음 명단에만 반영하며 이미 확정한 결과를 고치지 않는다. 남은 참가자가 없으면 기존 Failed/방 종료 경로로 정리한다.

### 0.5 기존 코스에 다음 배치 이어 붙이기

현재 BeginPlacement는 PlacedTracks가 있으면 거절하고, FinishPlacement는 OpenEnds를 비운다. 따라서 Phase를 되돌리거나 CancelPlacement를 호출하는 것만으로 반복되지 않는다.

- FinishPlacement에서 종료 캡을 Publish하기 전에 Tail을 서버의 다음 확장 끝으로 저장한다.
- 기존 BeginPlacement의 외부 시그니처를 유지한다. 빈 코스이면 현재처럼 시작 조각을 만든다. 완료된 기존 코스이면 저장한 끝을 검사하고 새 PendingTracks와 새 종료 캡을 먼저 준비한다.
- 준비가 성공한 뒤 이전 종료 캡만 서버에서 제거하고 PlacedTracks에서도 뺀다. 나머지 도로 Actor·PieceId·배치 Transform은 유지한다. 저장한 끝을 OpenEnds에 복구하고 CourseRevision과 코스 범위를 갱신한다.
- 새 선택 세션 ID를 배치 SessionId로 사용하고 현재 방식으로 설치를 진행한다. 새 시작 트랙을 중복 생성하거나 PlacedTracks 전체를 초기화하지 않는다. 실제 도로 연결은 기존 MWTrackSnap.AlignPorts와 서버 PlaceTrack을 그대로 사용한다.
- 마지막 설치 뒤 기존 FinishPlacement가 새 종료 캡을 붙이고 새 확장 끝을 저장한다. 기존 OnPlacementFinished → EnterPhase(PreparingRace)로 연결한다.

### 0.6 차량과 표시 소비자

PrepareRace는 기존 Pawn을 유지하고 기존 PlayerStart 슬롯으로 안전하게 이동시킨다. 기존 TeleportSucceeded/OnTeleported와 PrepareForRaceFromServer로 스트림·시작 스냅샷·카메라 이력을 초기화한다. 성공 위치를 공개한 뒤 기존 클라이언트 준비 보고와 3초 Countdown을 다시 수행한다. 새 Epoch/세션 키이므로 이전 보고는 재사용되지 않는다.

EnvironmentGameplay의 환경 유지 Phase에 RoundResults를 포함한다. HUD는 RoundResults와 결과 스냅샷을 읽고 기존 서버 시각 조회로 대기 시간을 표시한다. 같은 월드의 반복을 위해 ArrivalScreen.Begin이나 ServerTravel을 다시 호출하지 않는다. 현재 트랙 위젯의 생성·제거와 카메라 전환 경로를 재사용한다. 전용 결과 UI 시스템이나 표시 완료 ACK는 이번 반복 조건에 추가하지 않는다.

구현 순서는 개인 Finished/서버 완주 승인 → 전원 집계/결과 스냅샷 → 다음 Epoch/명단/타이머 → 코스 확장 → 차량 재배치/기존 준비 흐름 재사용 → HUD/환경 조건 연결이다. 전체 루프 구현 후 실제 플레이에서 2개 라운드를 연속 수행하고, 중복 완주·기존 결승·복귀 후 완주·주행 중 이탈을 확인한다. 0.1~0.6은 전체 설계이며 전체 루프의 실행 검증 완료 기록이 아니다.

### 0.7 개인 완주 1차 구현 기록 · 이후 확장은 0.8절 우선

- 실제 `DA_TrackRuntime`의 `BP_Track_WoodEndCap_C`는 바닥 충돌만 가지며 Finish 트리거가 없다. PreparingRace에서 현재 종료 캡의 Runtime을 즉시 연결하고 기존 `CreateShape`/`OnTrigger`로 서버 전용 결승 Box를 만든다. 연결점 +X가 코스 밖인 기존 프레임을 사용해 Box의 바깥 면을 연결점에 맞춘다. GameMode의 `FinishTriggerHalfExtent` 기본값은 (50, 1350, 1250)cm이고 X는 진행 깊이, Y는 도로 반폭, Z는 높이의 절반이다. 폭·높이는 기존 WoodFinish 결승 형상에 맞췄다. 이미 결승 형상이 있거나 준비된 영역이 있으면 재사용한다. 카탈로그와 BP 에셋은 수정하지 않는다.
- `FMWPlayerRoundState`에 `Finished=2`, `FinishOrder=0`을 추가하고 equality/NetSerialize에 포함했다. 기존 PlayerState의 서버 setter·공용 Notify·RepNotify를 유지한다.
- GameMode의 `TryFinishPlayerRace`는 현재 Racing/Epoch, 최초 StartingPlayers 명단, Controller/Pawn/PlayerState 소유 관계, 현재 확정 코스와 마지막 종료 Actor를 검증한다. 이 단위에서는 별도 MatchMembers 집합 없이 최초 명단을 재사용한다. `NextFinishOrder`는 Racing 진입에 1로 초기화하고 승인할 때만 증가한다. 전원 집계나 RoundResults 전환은 하지 않는다.
- Racing 진입에서 기존 Runtime의 `BeginPlayerRace`를 개인 Active 적용 전에 호출해 체크포인트·완주 기록을 새 Epoch로 초기화하고 출발 시각을 설정한다. Gameplay 트리거는 현재 Active/기록 Epoch를 요구하며 공식 완주 승인 뒤에만 기존 bFinished 조회 값을 갱신한다. 기존 DriveTest 기록 경로는 보존한다. 0/0의 건설 생략 Gameplay 경로에는 코스 결승 원본이 없으므로 이 승인 API로 완주를 확정하지 않는다.
- 서버 Pawn은 공식 Finished를 받아 기존 RefreshDrivePermission으로 입력·속도·회전·예측 이력·아이템을 정리하고 CollisionRoot를 NoCollision으로 바꾼다. 이전 충돌 모드를 저장한다. 이동/표시 Tick과 카메라 계층은 유지한다. 서버의 중간 이동 Overlap으로 완주해도 남은 스텝이 이동을 재개하지 않는다.
- 기존 차량 이동 스냅샷에 `bFinishCollisionDisabled`를 한 비트로 포함한다. 이것은 서버 Pawn의 충돌 적용 상태를 전달하는 파생 값이며, 공식 완주 상태/순서의 원본은 PlayerState다. 스냅샷 RepNotify와 이동 Tick의 공용 갱신으로 소유자·Controller/PlayerState가 늦는 원격 표시 차량에도 적용한다. 다른 Actor의 복제 순서를 가정하지 않는다.
- 다음 Waiting만으로 충돌을 켜지 않는다. 기존 GameMode.PrepareRace가 유지 중인 Pawn을 출발 슬롯으로 Teleport한 후 PrepareForRaceFromServer를 호출하여 충돌 모드를 복원하고 새 시작 스냅샷을 공개한다. 클라이언트는 충돌을 복원하는 스냅샷의 위치부터 적용한 후 충돌을 켠다. 실제 다음 라운드 전환 경로는 후속 작업이다.
- 아이템의 Gameplay 허용 조건도 개인 CanDrive를 요구하여 완주 뒤의 지급/사용을 차단한다. 기존 Items 자동화 fixture도 정상 PlayerState와 Epoch를 준비하도록 맞췄다. 삭제한 RaceStart 테스트/전용 GameMode를 다시 만들지 않고 기존 PlayerRound·DriveControl 검사에 완주/복원 범위를 추가한다.

검증: UE 5.8.4 Editor 빌드와 자동화 21/21을 통과했다. 개인 완주 검사는 실제 서버 Box Overlap으로 이동 중 완주, 순서 1/2, 중복/이전 Epoch/코스 변경/명단 외 거절, 충돌 복원과 기존 DriveTest 회귀를 포함한다. 소유자/표시 차량의 수신 순서 검사는 역할 전환·RepNotify 호출을 통한 단위 검증이며 실제 다중 프로세스 네트워크 플레이와 구분한다. 결과는 `Saved/Automation/PersonalFinish/index.json`, 실행 로그는 `Saved/Logs/PersonalFinishAutomation.log`에 남긴다. Game 타깃과 BP 최종 확인 결과는 차량 문서에 기록한다.

### 0.8 4000점 경기와 기존 코스 반복 구현 계약

사용자 확정 규칙: 정상 경기에서 구간별 무탈락 체크포인트 통과 100점, 완주 순서 1/2/3등에 300/150/50점이다. 복귀한 구간은 0점이고 다음 체크포인트를 승인한 뒤 다음 구간은 다시 득점할 수 있다. 서버가 라운드 종료 후 누적 4000점 이상인 사람을 센다. 0명은 다음 건설, 1명은 최종 결과, 2명 이상은 해당 플레이어만 기존 코스에서 서든데스를 진행한다. 서든데스는 점수를 추가하지 않으며 복귀 없이 탈락하고, 완주 순서 우선·탈락 순서 역순으로 최종 순위를 정한다.

**기존 상태 재사용:** 새 결과 구조체/Manager/Subsystem은 없다. `FMWPlayerRoundState`에 MemberId, 체크포인트 수, 체크포인트/완주 획득점수, 누적점수, 탈락 순서, 라운드/최종 순위, 참가 여부, 구간 득점 가능 여부를 추가했다. GameState의 기존 `FMWGameplayProgress.Results`가 개인 상태 배열을 복사하고 ResultsEpoch와 Phase를 같은 NetSerialize로 전달한다. 다음 준비에서 개인 라운드 값은 초기화하지만 누적점수와 직전 결과 배열은 유지한다.

**서버 판정:** 기존 Runtime의 체크포인트 감지 → TryPassPlayerCheckpoint에서 코스에 표시된 체크포인트 순서를 검사한 뒤 한 번만 100점을 지급한다. TryFinishPlayerRace는 모든 체크포인트 통과도 요구하고 정상 경기의 완주 보너스를 한 번만 지급한다. HandlePlayerRaceFailure는 정상 경기의 해당 구간 득점을 차단하거나 서든데스에서 Eliminated와 서버 탈락 순서를 기록한다. 개인 상태의 공용 Notify로 기존 차량 정지/아이템 정리/충돌 해제와 스냅샷 복제를 재사용한다.

**단일 전환:** TryEndRound는 남은 현재 참가자 전원이 Finished 또는 Eliminated일 때만 EnterPhase(RoundResults)를 호출한다. 정상 경기의 복귀는 Active를 유지하므로 완료로 세지 않는다. RoundResults에서 결과를 확정하고 기존 RacePhaseTimer/PhaseEndServerTime으로 기본 3초 대기한다. 기한 콜백은 Phase/Epoch/코스/기한을 다시 검사한 뒤 목표 도달 인원에 따라 MatchResults, PreparingRace(서든데스), PreparingConstruction(다음 정상 라운드)로 전환한다. 정상 라운드 또는 서든데스를 새로 시작할 때만 RoundIndex/Epoch를 한 번 증가시킨다.

**코스 확장:** FinishPlacement가 종료 캡 직전의 FMWOpenTrackEnd를 보관한다. 다음 BeginPlacement는 선택 트랙과 새 종료 캡을 준비한 뒤 이전 종료 캡만 제거한다. 남은 도로/Actor/PieceId는 유지하고 저장한 끝에서 새 트랙을 붙인다. 이전 결승은 마지막 기존 도로에 서버 체크포인트 Box와 복귀 후보를 추가하고 기존 PlacedTracks.bCheckpoint에 표시한다. 원래 체크포인트가 있는 트랙도 게시 시 표시하여 코스 순서 판정에 포함한다. 이전 종료 Actor, 세션, 리비전에 대한 오래된 요청은 승인하지 않는다.

**준비와 탈락:** 기존 Pawn을 최초 출발 슬롯에 재배치한 뒤 충돌을 복원하고 새 시작 스냅샷을 공개한다. 준비 보고/3초 Countdown을 매 라운드 재사용한다. 서든데스 비참가자는 Spectating, 참가자는 Waiting→Active로 설정하여 준비 집계와 입력에서 분리한다. 비참가자/탈락자의 로컬 Controller는 남은 참가자 차량을 기존 카메라로 본다. 서버는 코스 최저 높이보다 기본 1000cm 아래인 추락을 0.25초 간격으로 확인하고 기존 복귀 경로에 전달한다. 최초 체크포인트 이전 실패는 출발 위치로 복귀한다. KillZ도 Pawn 파괴 전에 같은 복귀/탈락 경로를 사용한다. 선두 거리 차와 차량 파괴의 별도 검출기는 이 단위에 새로 만들지 않았다. 추가 탈락 원인은 같은 HandlePlayerRaceFailure로 연결한다.

**소비자와 종료:** 기존 HUD에 점수와 결과 텍스트를 표시하고 EnvironmentGameplay는 RoundResults/MatchResults에서도 환경을 유지한다. 정상 최종 결과는 누적점수순, 동점은 직전 라운드 순위·참가 ID순이다. 서든데스 비참가자는 참가자 뒤에서 누적점수·참가 ID순이다. 주행 중 이탈자는 서버 명단에서 제거하고 완료 집계를 다시 한다. 확정된 라운드 결과 스냅샷은 이탈로 고치지 않는다. 기존 건설/참가자의 출발 준비 중 이탈 실패 정책은 유지한다. 주행 제한시간·재경기/로비 복귀 UI는 추가하지 않는다.

**검증:** UE 5.8.4 MWEditor/MW Win64 Development 정상 빌드, 자동화 23/23 성공(경고 0). 실제 트랙 Definition의 선택/소켓 배치로 정상 3라운드와 기존 도로 보존·종료 캡 교체를 수행하고, 100점/복귀 구간 0점/중복 거절, 300·150·50점, 결과 보존·Epoch 증가, 단독 정확히 4000점 종료, 복수 도달자만의 서든데스, 무복귀 탈락·전원 탈락 역순·관전자만 남을 때 무한 대기 방지를 검증했다. 비동기 메시 컴파일은 통합 fixture에서 FinishAllCompilation으로 마무리한 뒤 실제 물리 준비 조건을 검사한다. Blueprint 18개 컴파일 성공, 검사 에셋 19개 SHA-256 전후 동일. 결과는 `Saved/Automation/RoundLoop/index.json`, 빌드는 `Saved/Logs/RoundLoopEditorBuild.log`, `Saved/Logs/RoundLoopGameBuild.log`, BP 검증은 `Saved/Logs/RoundLoopAssets.json`에 있다. 실제 3PC/다중 프로세스 패킷·지연/손실·화면 확인과 Cook/패키징은 수행하지 않았다.

임시 `Saved/Codex/RoundLoopAudit.py`는 최종 BP/에셋 검증 JSON으로 대체한 뒤 삭제했다. 중간 `Saved/Automation/RoundLoopFocused`와 `Saved/Logs/RoundLoopFocused*.log`는 최종 RoundLoop 결과가 대체하지만, 정리 명령이 자동 승인 검토의 정책 차단을 받아 유지했다(상세 차단 사유 미제공). 고유 최종 결과·에셋 해시·코드 검증 증거는 RoundLoop 결과와 로그/검증 JSON에 유지한다.

### 0.9 준비 재보고·결과 확정 후 이탈·반복 조회 보완

- **준비 재보고:** Controller는 로컬 준비 조건이 충족되면 기존 Reliable ServerReportRaceReady를 보낸다. 같은 Epoch/Session/Revision도 PreparingRace 동안 0.5초 간격으로 다시 보낸다. 새 준비 키는 즉시 보고하고 PostSeamlessTravel에서 재보고 시각도 초기화한다. 서버 거절에 별도 ACK나 새 복제 상태를 만들지 않는다. 서버는 기존 RaceReadyMembers에 한 번만 등록/로그를 남기며 중복 보고에서도 AreAllRaceReady를 다시 평가한다. 성공하면 기존 EnterPhase(Countdown)으로 이동하여 로컬 재보고가 멈춘다. 기존 30초 준비 제한시간은 유지한다.
- **확정 결과와 접속 명단:** CountQualifiedPlayers는 현재 Epoch의 Results와 기록된 TargetScore로 목표 도달 인원을 센다. 최종 BuildResults는 확정 배열을 복사해 FinalRank를 부여한다. RoundResults 대기 중 PlayerState가 사라져도 단독 도달자의 경기 종료, 서든데스 완주자/탈락자의 순위와 점수를 다시 계산하지 않는다. PlayerState 갱신은 현재 접속한 멤버에만 적용하고 모든 결과 행은 GameState에 보존한다.
- **다음 주행:** 서든데스 자격은 직전 결과에서, 실제 다음 참가자는 남은 MatchMembers에서 가져온다. 둘 이상이 자격을 얻은 뒤 한 명이 나가도 서든데스 경로를 유지하며 남은 자격자가 주행한다. 자격자가 모두 떠나 관전자만 남으면 기존 Failed로 종료한다. 다음 일반 라운드 역시 남은 명단으로 진행하며 준비 중 이탈의 기존 실패 정책은 유지한다.
- **복사 비용:** Blueprint의 값 반환 GetProgress는 유지하고 C++ 전용 const 참조 GetProgressRef를 추가한다. CanDrive, 차량의 권한/충돌 갱신 및 이동 스냅샷의 Epoch 조회는 참조 경로를 사용한다. EnterPhase의 Current/Next, 타이머의 Expected, 결과 저장은 과거 값을 고정해야 하므로 값 복사를 유지한다. 참조는 멤버로 보관하거나 지연 콜백에 캡처하지 않는다.
- **검증:** UE 5.8.4 MWEditor/MW Win64 Development 빌드 성공, 자동화 23/23 성공(테스트 경고 0). 기존 RoundLoop 통합 검사에 일시적 코스 미준비 보고 거절→동일 키 재수락, 이전 Epoch 거절, 먼저 보고한 차량의 준비 상태 회복→중복 보고로 Countdown 진입, 단독 도달자/서든데스 완주자의 결과 대기 중 이탈과 PlayerState 제거, 복수 자격자 중 한 명 이탈 후 서든데스 유지, 전원 자격자 이탈 후 Failed를 추가했다. 기존 점수/확장/복제/차량/아이템/HUD/커서 검사도 통과했다. 빌드에는 이번에 수정하지 않은 MWItemComponentVisual.cpp의 GetBindings() C4996 경고가 남아 있다. 결과는 `Saved/Automation/RoundLoopFix/index.json`, `Saved/Logs/RoundLoopFixEditorBuild.log`, `Saved/Logs/RoundLoopFixGameBuild.log`, `Saved/Logs/RoundLoopFixVerification.json`이다. 준비 재보고 검사는 서버 진입점으로 재수신을 주입한 단위/통합 검사이며 실제 클라이언트의 0.5초 주기·패킷 지연/손실·다중 프로세스 실행과 성능 측정, Cook/패키징은 미실행이다.

## 1. 목적과 적용 범위

기존 트랙 선택·배치에 경기 규칙을 연결한다. 콜백마다 다음 단계를 직접 시작하는 구조를 다음 경로로 통일한다.

```text
완료 이벤트 / 소유 Controller RPC / 타이머 / 참가자 이탈
 → 검증 및 서버 상태 기록
 → 현재 Phase/Epoch/요청 키/기한 확인
 → EnterPhase: 단계 전환 실행
 → GameState 저장·복제 및 로컬 표시 갱신
```

첫 완료 목표는 기존 배치 후 세 참가자가 준비·카운트다운·주행·결과까지 도달하는 것이다. 그다음 체크포인트·점수·복귀·코스 확장·최종 결과·재경기를 연결한다.

- 기존 AGameModeBase / AGameStateBase와 프로젝트 Phase를 유지한다.
- 새 상위 Manager, Subsystem, 범용 이벤트 버스, 단계별 UObject 클래스는 첫 구현에 도입하지 않는다.
- 전체 경기의 전환 결정은 Gameplay GameMode에 모으고, 배치·이동·표시의 세부 구현은 기존 담당 객체에 둔다.
- 준비 확인·타이머·실패 원인마다 새로운 Phase를 만들지 않는다.
- Network Prediction/Mover 도입과 이동 모델 교체는 범위 밖이다.
- 서버 경기 상태 보관·복제를 다룬다. 앱 종료 후 보존하는 SaveGame·계정 DB는 범위 밖이다.

규칙은 [기획](../MakeWay_기획최종안.md)과 [개발명세](MakeWay_클라이언트_개발명세서_v1.0.md)를 따른다. 이 문서는 사용자가 요청한 게임플레이 진행·단일 전환의 구현 기준을 구체화한다. [16번 계약](16_로비_게임플레이_분리와_차량선택_전환_계약.md)의 로비·차량 선택 책임은 유지한다. 이 문서의 RoundEpoch는 테마 월드의 경기 실행용이며 로비 차량 선택 계약에 추가하지 않는다.

과거 문서의 Character/CMC 설명보다 실제 Pawn 구현과 [현재 차량 문서](../../Engineering/Client/VEHICLE_PAWN_LOCAL_MOVEMENT.md)를 적용한다. 문서에 남은 과거 미구현·검증 기록을 현재 소스 상태로 읽지 않는다.

## 2. 현재 브랜치에서 확인한 출발점

### 2.1 구현 사실과 이번에 만들 것

| 영역 | 772d2e0에서 확인한 사실 | 이 설계에서 연결할 것 |
|---|---|---|
| 로비/게임플레이 분리 | GameMode·GameState·Controller가 분리되고 공용 MWPlayerState 사용 | 기존 분리 유지 |
| 연결 | MWConnection의 프로젝트 소비자는 MWGameInstance, StartingPlayers와 방 설정 인계 존재 | 초기 명단만 받아 경기 중 명단은 GameMode가 관리 |
| Gameplay GameMode | 초기화, 참가자 맵 로드 확인, 트랙 선택/배치, 코스 수신 확인, 차량 생성/Possess | 단일 진행 검사, 카운트다운, 경기 종료 |
| Gameplay GameState | RoomSettings, 별도 RoomPhase와 CountdownEndServerTime 복제, Construction 컴포넌트 보유 | 진행 구조체와 확정 결과 |
| PlayerState | 호스트·로비 Ready·LobbyMemberId·VehicleAssetID, 건설/커서 컴포넌트 | 누적 점수와 개인 주행 상태 |
| 트랙 선택 | 공유 후보 3개, 선택 세션/리비전, 요청 캐시와 전원 확정 이벤트 | 완료 보고를 공통 진행에 연결 |
| 트랙 배치 | 서버 Transform 확정, 시작/종료 조각, PieceId·CourseRevision·Actor 목록, 완료 이벤트 | 실패 보고, 성공/취소 구분, 이후 코스 확장 |
| 출발 준비 | Controller가 설치 Actor와 Transform을 확인하여 보고, 서버가 전원 보고 후 스폰 | 코스 충돌과 차량·입력·카메라 준비까지 확대 |
| 주행 입력 | Controller가 입력을 바인딩하고 Pawn.SubmitDriveInput으로 전달 | 경기 Phase/개인 상태의 허용 조건 |
| 차량 | APawn + Box CollisionRoot + UPawnMovementComponent, 직접 예측/보정 | 경기 경계에서 기존 스트림/정지 API 연결 |
| 차량 표시 | PresentationTick → DrivePresentation → CameraBoom, 별도 표시 컴포넌트 | 표시/카메라 수명 보존, 복귀 시 기존 초기화 재사용 |
| 경기 규칙 | 체크포인트·점수·복귀·라운드 결과·최종 결과의 Runtime 구현 없음 | 후속 단계로 추가 |
| 요소 배치 | ElementPlacement enum은 있으나 이 조사 범위의 Runtime 배치 시스템 없음 | 완료/스킵 이벤트 계약부터 제공 |

주요 기준 소스:

- [Gameplay GameMode](../../../Source/MW/System/GameMode/MWGameplayGameMode.cpp)
- [Gameplay GameState](../../../Source/MW/System/GameState/MWGameplayGameState.h)
- [PlayerState](../../../Source/MW/System/MWPlayerState.h)
- [Gameplay Controller](../../../Source/MW/Player/MWGameplayPlayerController.cpp)
- [Construction](../../../Source/MW/Gameplay/Construction/MWTrackConstructionComponent.h), [배치 구현](../../../Source/MW/Gameplay/Construction/MWTrackConstructionPlacement.cpp)
- [Pawn](../../../Source/MW/Gameplay/Vehicle/MWVehiclePawn.h), [네트워크 이동](../../../Source/MW/Gameplay/Vehicle/MWVehiclePawnNetwork.cpp), [표시 컴포넌트](../../../Source/MW/Gameplay/Vehicle/MWVehiclePresentationComponent.cpp)

### 2.2 현재 흐름과 책임 분산

현재 흐름은 다음과 같다.

```text
PreparingConstruction
 → 전원 선택 이벤트가 StartTrackPlacement 호출
 → BeginPlacement가 GameState를 TrackPlacement로 직접 변경
 → 배치 완료 이벤트가 코스 전달 30초 타이머 시작
 → 전원 코스 수신 보고
 → GameMode가 PreparingRace로 변경하고 PrepareRace 호출
 → 차량 생성/Possess 성공 직후 Racing으로 직접 변경
```

Gameplay GameMode에는 초기 Phase, PreparingRace, Racing, Failed 변경이 있고 Construction에도 TrackPlacement와 Failed 변경이 있다. 타임아웃과 Logout도 직접 실패 경로를 실행한다.

Countdown은 enum과 저장 필드만 있고 실행되지 않는다. 현재 Controller의 GetInputVehicle은 경기 Phase/개인 주행 상태를 검사하지 않는다. 따라서 지금의 PrepareRace를 그대로 두고 카운트다운 UI만 추가해서는 출발 전 조작을 막을 수 없다.

### 2.3 최신 차량 작업을 보존할 제약

772d2e0은 선행 de73fd4 이후의 충돌 예측·표시 보완을 포함한다.

- 서버/소유자 120Hz 이동, 입력 묶음 전송, ACK 기반 재실행과 상대 접촉 추정이 존재한다.
- 스냅샷은 StreamId·Revision·ACK·Transform·속도·접지·충돌 회전 속도·서버 시각을 NetSerialize로 묶는다.
- 자기 외형은 남은 한 스텝 미만의 시간을 짧게 추정한다. 선행 기록의 일률적인 한 스텝 지연 보간을 현행 방식으로 가정하지 않는다.
- 별도 MWVehiclePresentationComponent가 이동 이후 표시를 갱신한다. 상대 표시와 CameraBoom의 Tick 선행 관계가 있다.
- ResetDriveStream은 private이고 입력 큐·서버 순번·표시 이력을 정리하며 새 스트림 스냅샷을 공개한다. 이 함수 하나가 저장 입력과 물리 속도를 모두 지운다고 가정하지 않는다.
- TeleportSucceeded와 Movement.OnTeleported의 기존 초기화를 재사용한다. 텔레포트 전에 정지시켜 새 스냅샷에 이전 속도가 담기지 않게 한다.
- 현재 BP·SpringArm 설정·표시 Tick 순서를 경기 흐름 작업의 부수 변경으로 교체하지 않는다.
- 차량 문서에는 빌드/차량 자동화 6개 및 별도 프로세스 측정 기록이 있다. 이번 문서 작업에서 재실행한 결과가 아니다. 기존 ABP_UfologyPilot의 GetSteeringInput 참조 오류도 경기 설계 완료로 해결됐다고 표시하지 않는다.

## 3. 상태 소유자와 최소 데이터

### 3.1 책임

| 객체 | 권한과 원본 | 하지 않을 일 |
|---|---|---|
| Gameplay GameMode | 서버의 전환 판단·타이머·참가 명단·점수/복귀 판정 | UI 생성, 트랙 소켓 계산, 차량 네트워크 내부 직접 조작 |
| Gameplay GameState | 공용 진행 구조체·경기 설정·확정 결과 복제 | 다음 단계 선택, 코스 목록 중복 관리 |
| PlayerState | 개인 점수·라운드 진행 상태 복제 | 차량 위치/입력 이력 보관 |
| Gameplay Controller | 소유자 요청, 준비 보고, 로컬 입력/카메라 연결 | 직접 점수 확정, 다음 Phase 지정 요청 |
| Construction | 선택·설치·코스 원본, 기능 완료/실패 보고 | 전체 경기 Phase 쓰기 |
| Vehicle Pawn/Movement | 실제 정지·입력 적용·텔레포트·기존 예측/보정 | 경기 승패 결정 |
| 로컬 UI | 현재 복제 상태 표시·사용자 의도 전달 | 로컬 타이머로 출발/종료 확정 |

현재 Controller가 생성하는 트랙 Widget을 이번 전환 정리와 함께 전면 HUD 이관하지 않는다. UI 소유권 개선은 16번 문서의 방향을 유지하되 별도 작업으로 분리한다.

### 3.2 GameState의 진행 구조체

다음은 설계용 필드이며 아직 소스에 존재하지 않는다.

```cpp
FMWGameplayProgress
{
    EMWRoomPhase Phase;
    int32 RoundIndex;
    int32 RoundEpoch;
    double PhaseEndServerTime;
    EMWRaceMode RaceMode;       // Normal / SuddenDeath
};
```

- 초기 RoundEpoch 0은 경기 실행 미시작이다. 최초 진입에서 1부터 시작한다.
- RoundIndex는 표시 번호이고 새 일반 라운드에서 증가한다. 재경기는 1로 돌아간다.
- RoundEpoch는 새 일반 라운드·서든데스·재경기마다 증가하며 같은 월드에서 재사용하지 않는다.
- 일반 단계 사이에서는 Epoch를 유지한다. 별도 MatchInstanceId·PhaseRevision을 첫 구현에 추가하지 않는다.
- PhaseEndServerTime 0은 해당 단계의 공개 마감 없음이다. 단위는 서버 월드 시간의 초다.
- 현행 RoomPhase/CountdownEndServerTime을 이 구조체로 통합한다. GetRoomPhase 같은 소비용 Getter는 유지할 수 있다.
- GameMode에도 별도의 변경 가능한 CurrentPhase 원본을 두지 않는다. 전환 판단은 GameState의 진행 상태를 읽는다.
- RoomSettings와 코스 상태는 진행 구조체에 복사하지 않는다.
- TargetScore는 서버 설정에서 매치 시작 때 정해 GameState의 별도 공개 경기 설정으로 보관한다. 기존 방 생성 RoomSettings/UI의 필수 입력을 늘리지 않는다.

서버 대입 후 Listen Host를 위한 공통 Notify를 직접 호출하고 원격 OnRep도 같은 Notify로 연결한다. 구조체로 묶어도 다른 Actor·PlayerState·코스의 준비 완료까지 보장하지 않는다.

Progress의 Phase·Epoch·마감·경기 방식은 항상 같은 서버 스냅샷으로 해석해야 한다. 현행 차량 스냅샷의 경험을 반영해 작은 Progress 구조체는 전체 필드를 함께 쓰는 NetSerialize를 구현하는 방향으로 잡는다. PlayerRound의 Epoch·개인 상태·진행 기록도 같은 기준으로 묶는다. 단순히 USTRUCT에 넣었다는 이유만으로 사용자 정의 원자적 직렬화가 완성됐다고 간주하지 않으며, 손실 조건의 값 조합을 검증한다. 이는 서로 다른 Actor의 도착 순서를 보장하는 장치가 아니다.

### 3.3 PlayerState의 경기 기록

| 필드 | 수명 |
|---|---|
| TotalScore | 매치 동안 유지, 재경기에서 초기화 |
| PlayerRound.RoundEpoch | 해당 개인 라운드 기록의 실행 식별자 |
| PlayerRound.RaceStatus | Waiting / Active / PendingRespawn / Finished / Out |
| PlayerRound.NextCheckpointIndex | 다음 인정 체크포인트 |
| PlayerRound.RoundScore | 해당 라운드 획득 점수 |
| PlayerRound.FinishOrder / FinishTime | 유효 완주 기록, 미완주면 무효값 |

라운드 필드를 FMWPlayerRoundState 한 구조체로 묶는다. 기존 건설 선택·배치 상태는 MWPlayerConstructionComponent에 유지하며 PlayerState 본체에 중복 필드를 추가하지 않는다.

- Waiting: 출발 대기. Active: 주행 중. PendingRespawn: 일반 경기 복귀 대기.
- Finished: 완주. Out: 이번 주행에서 더 진행하지 않음. 이탈/시간초과/서든데스 탈락 원인은 결과 사유로 구분한다.
- 누적 점수 원본은 TotalScore 하나로 정한다. APlayerState::Score와 동시에 갱신하는 이중 원본을 만들지 않는다.
- 기존 bReady는 로비 전용이다. 출발 준비 보고나 재경기 승인에 재사용하지 않는다.
- 기존 CopyProperties의 참가자 ID·호스트·차량 선택 인계를 유지한다. 경기 기록을 무조건 로비로 복사하지 않는다. 같은 테마 월드의 라운드/재경기는 명시적 초기화로 처리한다.

### 3.4 GameMode의 서버 전용 진행 자료

필요한 자료만 둔다.

- 초기 StartingPlayers에서 만든 실행 참가 명단과 현재 유효 참가자 집합.
- 현재 건설 SessionId와 그 세션이 속한 RoundEpoch.
- 성공 완료된 배치 SessionId/CourseRevision. 선택 완료는 기존 상태 재조회로 확인 가능하다.
- 현재 출발 준비 키와 보고한 참가자 집합, 전원 준비를 확정한 서버 시각.
- 최초 도착 대기/현재 단계 타이머, 예약된 진행 검사 핸들, 예약 여부.
- 최초의 유효한 진행 실패 사유.
- 라운드 참가 인원 N, 체크포인트 지급 이력, 복귀 대기 자료.

위 판정용 자료를 전부 복제하지 않는다. 이벤트 도착 횟수나 Hover·커서 갱신을 경기 진행 자료로 쌓지 않는다.

## 4. 서버 단일 진행 경로

### 4.1 함수 계약

| 함수 | 실행과 책임 |
|---|---|
| RequestFlowUpdate | 서버 진행 검사 1회 예약. 이미 예약됐으면 합친다 |
| TryAdvanceGameplay | 최초 진입 또는 현재 Phase의 다음 단계 판단을 실행 |
| EvaluateNextPhase | 현재 사실을 조회하여 다음 Phase 또는 대기를 반환. 부작용 없음 |
| EnterPhase | 실제 전환의 유일한 실행 경로. 초기화·타이머·시작 작업·공개 |
| RecordFlowFailure | 현재 실행의 유효한 실패를 한 번 기록하고 진행 검사 예약 |
| Begin/End 단계별 helper | GameMode가 시작·정리 작업을 위임하는 작은 함수 |

EnterPhase는 Gameplay GameMode의 private 함수이고 RPC가 아니다. 외부 객체는 다음 Phase를 지정하지 않는다. Gameplay GameState의 변경 API를 호출하는 production 경로는 EnterPhase로 제한한다. Lobby GameMode/전용 테스트 GameMode는 각자 분리된 규칙이며 기존 로비 setter를 함께 개편하지 않는다.

### 4.2 최초 진입

현재 InitGameState는 World BeginPlay 이전에도 실행된다. 여기서 선택판·출발 매니저가 모두 준비됐다고 가정하거나 BeginSelection을 바로 시작하지 않는다.

1. InitGameState/StartPlay에서 방 설정을 적용하고 기존 소유 Controller/PlayerState 연결을 유지한다.
2. StartPlay에서 최초 도착 제한시간을 한 번 시작한다. 아직 맵에 도착하지 않은 시작 참가자를 기다린다.
3. StartPlay·HandleStartingNewPlayer·필수 준비 완료 알림은 모두 RequestFlowUpdate를 호출한다.
4. 공통 진행 검사에서 World 시작, GameState/설정, 초기 명단의 전원 도착·차량 ID·필수 레벨 참조를 확인한다.
5. 준비되면 최초 EnterPhase를 한 번 실행하고 Epoch를 1로 공개한다. enum 기본값과 같은 Phase여도 최초 진입 작업은 실행한다.
6. 최초 진입 이후 같은 Epoch/같은 Phase의 재진입은 무시한다.

필수 레벨 참조는 현재와 같이 PlayerStartManager 하나, 건설 사용 시 TrackSelectionBoard 하나다. 재평가마다 전체 월드를 검색하지 않고 최초 준비 후 유효 참조를 보관한다.

초기 도착 검사를 이후 모든 Phase 앞의 전역 return 조건으로 남기지 않는다. 이탈로 명단이 줄었을 때 Failed/결과 판단까지 막히면 안 된다. 유효한 실패와 종료 판단은 참가자 전원 준비 검사보다 먼저 처리한다.

### 4.3 RequestFlowUpdate

```text
Authority/수명 확인
 → 예약 중이면 반환
 → 예약 플래그 설정
 → SetTimerForNextTick으로 TryAdvanceGameplay 1회 예약
```

타이머 핸들을 보관하고 EndPlay 때 해제한다. 설치 5.8.4의 TimerManager.h에서 이 API를 확인했다. 실행 시 예약 플래그를 해제한다.

이 예약은 컴포넌트의 완료 함수가 끝난 뒤 진행 판단을 실행하기 위한 것이다. 고정 Delay로 복제 완료를 추정하는 방식이 아니다. 매 Tick 폴링과 전체 이벤트 FIFO는 첫 구현에 필요 없다. 여러 완료가 예약 전에 들어와도 현재 상태와 성공 기록에서 다시 판단하므로 알림 횟수를 유지할 필요가 없다.

### 4.4 공통 진행 검사

```cpp
// 설계용 의사코드. 초기화/수명 검사와 구체 타입은 생략.
void TryAdvanceGameplay()
{
    bFlowUpdateQueued = false;

    if (HasPendingFlowFailure())
    {
        EnterPhase(Failed);
        return;
    }

    if (Progress.RoundEpoch == 0)
    {
        TryEnterInitialPhase(); // 최초 준비 충족 시에만 EnterPhase
        return;
    }

    const auto Next = EvaluateNextPhase();
    if (Next.IsSet())
        EnterPhase(Next.GetValue());
}
```

한 번의 검사에서는 최대 한 번 전환한다. 다음 단계의 시작 작업이 동기적으로 끝나도 EnterPhase 마지막에 다시 검사를 예약한다. EnterPhase 안에서 다음 EnterPhase를 직접 연쇄 호출하지 않는다.

### 4.5 EnterPhase의 실행 순서

1. Authority·현재 실행·허용된 전이·동일 단계 중복 여부를 확인한다.
2. 다음 단계의 필수 전제와 준비 가능한 자원을 검사한다.
3. 이전 단계 타이머를 정리하고 필요한 종료 작업만 수행한다.
4. 다음 진행 구조체를 적용한다. 새 라운드라면 개인 기록과 서버 집계도 새 Epoch로 초기화한다.
5. 해당 단계의 시작 helper를 한 번 실행한다. 시작 실패는 RecordFlowFailure로 기록한다.
6. 성공한 시작의 기한에 맞춰 단계 타이머를 예약하고 공통 진행 검사를 다시 요청한다. Failed/MatchResults는 자동 진행 검사를 반복 예약하지 않고 사용자 요청이 있을 때만 깨운다.

Phase 진입은 그 작업을 시작했다는 뜻이다. 다른 Actor·선택 세션·입력까지 준비됐다는 뜻은 아니다. 클라이언트는 Phase와 해당 세션/개인 상태가 함께 준비됐을 때만 조작을 허용한다. 시작 실패가 발생하면 다음 공통 검사에서 Failed로 전환하며, 이미 준비한 부분 자원은 해당 helper 또는 Failed 정리에서 회수한다. 실패 기록은 Failed 진입에서 소비하고, 종료 정리로 발생한 알림이 실패 진입을 반복시키지 않게 한다.

PreparingConstruction 종료 시 EndSelection을 먼저 호출하지 않는다. 현재 BeginPlacement가 참가자의 확정 선택을 읽은 뒤 EndSelection을 호출하는 순서를 보존한다. 정상 완료로 넘길 자료를 취소 정리에서 먼저 지우지 않는다.

### 4.6 타이머와 오류

- 타이머 callback은 다음 Phase를 정하지 않고 RequestFlowUpdate를 호출한다.
- 공통 검사에서 현재 단계의 마감과 완료 기록을 확인한다. 이미 지난 단계의 타이머가 늦게 실행돼도 현 단계를 실패 처리하지 않는다.
- 서버 RPC와 감지 이벤트 수신 시에도 기한을 검사한다. 기한 안에 완료한 사실과 서버 완료 시각을 기록하여, 예약된 검사 실행이 늦었다는 이유로 성공을 실패로 바꾸지 않는다.
- 현재 실행의 필수 자원 실패·준비 중단은 RecordFlowFailure로 모은다. 이미 저장된 첫 실패를 반복 콜백으로 덮어쓰지 않는다.
- Failed 진입에서 타이머·선택·미확정 배치·출발 조작을 정리한다. 이 정리가 다시 실패 보고를 발생시키지 않게 한다.

## 5. 트랙 컴포넌트의 완료·실패 보고

### 5.1 서버 Delegate

기존 두 완료 이벤트를 활용하고 세션 인자를 명시하는 방향으로 변경한다.

```cpp
OnAllSelectionsConfirmed(SessionId);
OnPlacementFinished(SessionId, CourseRevision);
OnConstructionFailed(SessionId, Error);
```

일반 서버 C++ Delegate다. 컴포넌트와 GameMode 사이에 RPC/NetMulticast를 만들지 않는다. 기존 OnSelectionSessionChanged는 로컬 UI 알림 용도로 유지하고 경기 진행 이벤트와 구분한다. Hover와 커서는 진행 이벤트를 발생시키지 않는다.

GameMode가 초기화 때 한 번 구독하고 EndPlay 때 해제한다. BeginSelection 등 시작 API를 호출하기 전에 구독한다. 새 라운드마다 중복 AddUObject하지 않는다. 전용 TrackSelectionTestGameMode 등 기존 소비자도 시그니처와 실패 처리 변경에 맞춰 수정 대상에 포함한다.

### 5.2 완료 처리

```cpp
// 제안 시그니처. 실제 소스에는 아직 없음.
void HandlePlacementFinished(int32 SessionId, int32 Revision)
{
    if (!MatchesCurrentConstruction(SessionId))
        return;

    CompletedPlacementSessionId = SessionId;
    CompletedCourseRevision = Revision;
    RequestFlowUpdate();
}
```

- 이벤트는 기능의 서버 상태 확정 후 한 번 발생시킨다.
- GameMode는 현재 Epoch와 건설 SessionId의 연결, 현재 Phase, 최종 리비전을 재검사한다.
- 선택 완료 handler는 진행 검사를 요청하고 EvaluateNextPhase가 기존 전원 확정 상태를 재조회한다. 필요한 읽기 전용 조회를 노출한다.
- 배치 완료 handler는 종료 조각까지 성공한 세션/리비전을 기록한다.
- handler에서 BeginPlacement, PrepareRace, EnterPhase를 호출하지 않는다.

### 5.3 실패와 정상 거절

| 종류 | 예 | 처리 |
|---|---|---|
| 정상 요청 거절 | CourseChanged, NotSnapped, 이미 확정, 오래된 요청 | 기존 응답 경로로 거절·재확인. 경기 실패로 전파하지 않음 |
| 동기 시작 실패 | BeginSelection/BeginPlacement의 필수 정의·스폰 실패 | 기존 bool + OutError 반환 → GameMode가 RecordFlowFailure |
| 진행 중 치명적 실패 | 종료 조각을 정상적으로 확정할 수 없음 | 컴포넌트가 자기 자원 정리 후 OnConstructionFailed |
| 정상 취소/월드 종료 | GameMode의 Failed 정리, EndPlay | 완료/치명적 실패 이벤트 없이 취소 |

컴포넌트의 GameState Phase 읽기와 요청 검증은 유지할 수 있다. 제거 대상은 전체 Phase를 쓰는 책임이다.

### 5.4 성공/취소 구분과 기존 코드 변경

현재 FinishPlacement와 CancelPlacement 모두 PlacementSession.bIsOpen을 false로 만든다. 따라서 닫혔다는 사실만으로 성공을 판정하지 않는다.

- 성공 조건: 현재 SessionId의 OnPlacementFinished가 기록됐고 최종 CourseRevision이 일치한다.
- 성공 이벤트는 최종 코스 확정 후 발생한다.
- 취소는 성공 기록을 만들지 않는다. 미개시 SessionId 0이나 이전 라운드의 닫힌 세션도 완료로 취급하지 않는다.
- BeginPlacement의 직접 TrackPlacement 변경을 제거한다.
- RemovePlacementPlayer의 직접 Failed 변경을 실패 보고로 교체한다.
- GameMode가 결정한 이탈 정책을 컴포넌트가 독자적으로 계속/중단 정책으로 바꾸지 않는다.

## 6. Phase와 입력의 최소 계약

### 6.1 전환표

| 현재 Phase | 진입 작업 | 성공 시 다음 | 기다리는 것/실패 |
|---|---|---|---|
| PreparingConstruction | 건설 세션/후보 시작, 이전 건설 성공 기록 초기화 | TrackPlacement | 현재 세션 전원 확정. 필수 준비 실패는 Failed |
| TrackPlacement | BeginPlacement로 서버 배치 시작 | ElementPlacement | 명시적 배치 성공. 막힌 코스는 Failed |
| ElementPlacement | 요소 배치/스킵 시작 | PreparingRace | 전원 처리 또는 서버 마감 정책 |
| PreparingRace | 코스 준비 키 고정, 차량 생성/정지/Possess, 출발 준비 대기 | Countdown | 기한 내 전원 실제 준비. 실패/시간초과는 Failed |
| Countdown | 입력·차량 정지 유지, 3초 종료 시각 | Racing | 서버 종료 시각 도달 |
| Racing | 유효 참가자 Active, 주행 제한시간 시작 | RoundResults | 전원 완주/종료 또는 경기 시간초과 |
| RoundResults | 결과 스냅샷 확정, 주행 차단 | 다음 일반 라운드 / 서든데스 / MatchResults | 결과 표시 기한 뒤 서버가 선택 |
| MatchResults | 최종 결과 유지, 주행 차단 | 재경기 또는 로비 복귀 | 호스트 요청/나가기 |
| Failed | 타이머·미확정 자원 정리, 입력 차단, 사유 표시 | 로비 복귀/방 나가기 | 자동 재시도 없음 |

RoundResults와 MatchResults를 enum에 추가할 때 기존 직렬화 enum의 숫자를 바꾸지 않는다. 현재 Failed 뒤에 새 값을 추가하거나 기존 값을 명시적으로 유지하고 BP 참조를 확인한다.

서든데스는 RaceMode만 바꾸고 PreparingRace → Countdown → Racing을 재사용한다. 신규 트랙을 추가하지 않고 Epoch만 새로 발급한다. 이는 별도 SuddenDeath Phase 두 개를 두는 이전 설계에 대한 상태 수 축소 제안이며 경기 규칙은 유지한다.

### 6.2 입력 허용과 정지

```text
CanDrive =
  올바른 소유 차량·입력 준비
  AND Progress.Phase == Racing
  AND PlayerRound.RoundEpoch == Progress.RoundEpoch
  AND PlayerRound.RaceStatus == Active
```

Controller의 입력 전달과 서버의 이동 입력 수용에 같은 기준을 적용한다. 클라이언트 UI/IMC 해제만으로 서버 입력을 차단한 것으로 처리하지 않는다.

- Countdown과 결과 단계는 입력 0뿐 아니라 속도·회전 충격을 정리하고 차량이 밀리지 않게 정지 상태를 유지한다.
- 차단 중에도 필요한 네트워크 상태 수신·스냅샷 공개·PresentationTick·카메라는 유지한다. 전체 컴포넌트 Tick을 무조건 끄는 방식은 사용하지 않는다.
- Pawn에 경기 제어 적용용 좁은 서버 진입점을 추가하는 후속 작업을 둔다. GameMode가 private 입력 큐/표시 오프셋/접촉 이력을 직접 수정하지 않는다.
- 차단·재허용 경계와 복귀에서 기존 StreamId를 갱신해 과거 입력 묶음을 거절한다. RoundEpoch와 StreamId는 각각 경기와 이동 이력의 식별자이며 서로 대신 사용하지 않는다.
- 새 스트림/과거 스냅샷 처리와 원자적 NetSerialize 경로를 보존한다. 제어 허용 때문에 별도의 위치 복제 경로를 추가하지 않는다.
- 잠금 상태에서 입력을 누르고 다음 상태로 넘어갈 때 잔류 가속·드리프트가 살아나지 않게 한다.
- MWDriveTestGameMode와 순수 이동 자동화는 로비·Gameplay GameState가 없는 기존 시험 경로다. 실게임에 등록된 차량의 경기 제어와 분리해 이 시험 경로를 보존한다. production에서 GameState 미수신을 자유 주행 허용으로 해석하지 않는다.

## 7. 코스·요소 배치와 출발 준비 보고

### 7.1 준비 대기의 위치

현재 TrackPlacement에서 수행하는 코스 전달 대기를 PreparingRace로 옮긴다. 단순 setter 이관과 구분되는 의도적인 단계 경계 변경이다.

```text
서버 트랙 배치 성공
 → 요소 배치 완료/스킵
 → PreparingRace
   서버: 최종 코스 준비 키 고정, 차량 스폰/소유 확인/정지
   클라: 코스 Actor·충돌 + 자기 Pawn + 입력·카메라 준비 확인
 → 전원 보고
 → Countdown
```

요소 시스템 미구현 상태의 첫 개발 검증은 명시적인 개발/검증 경로에서 생략할 수 있다. production에서 빈 handler를 성공으로 처리해 요소 배치 구현 완료로 표시하지 않는다. 기존 ConstructionTrackDefinitionIds가 빈 개발용 경로도 의도와 시작 Phase를 명시한다.

고정 코스 개발 경로는 동적 PlacedTracks가 없어도 서버가 지정한 고정 코스의 준비 조건으로 판단한다. 이 경로의 CourseSessionId 0 허용은 명시적인 개발 설정에 한정한다. production의 잘못된 세션/빈 코스를 준비 완료로 수용하는 일반 예외로 사용하지 않는다.

### 7.2 출발 준비 키와 보고

기존 ServerConfirmTrackCourseReady를 최종 출발 준비 보고로 확장한다. 새 RPC와 기존 RPC를 동시에 같은 출발 승인에 사용하지 않는다.

```cpp
ServerReportRaceReady(RoundEpoch, CourseSessionId, CourseRevision);
```

- 이름은 제안이며 실제 변경 때 기존 호출자·수신자·BP 노출을 함께 갱신한다.
- 서버가 `Progress.CourseSessionId/CourseRevision`에 예상 키를 고정하고 준비 집합을 비운 뒤 PreparingRace를 공개한다. 별도의 GameMode 완료 세션 복사본은 없다.
- 클라이언트는 Progress와 자신의 PlayerRound가 같은 Epoch이고 확정 코스/개인 상태가 준비되면 한 번 보고한다.
- 서버는 소유 Controller/참가자/Phase/키/기한과 서버의 코스·차량 상태를 검사하고 집합에 추가한다. 전원 완료면 직접 `EnterPhase(Countdown)`을 호출한다.
- 호스트도 로컬 Controller의 동일 조건과 RPC 진입 경로를 사용한다.
- 현재 LastReadyPlacementSessionId를 Epoch·SessionId·Revision 전체 키로 바꾼다. 서든데스는 같은 코스라도 새 Epoch이므로 다시 보고해야 한다.
- 로컬 보고 키는 Controller 객체 수명에 맞춰 관리하고 새 키의 준비 완료 전에는 완료로 기록하지 않는다.

클라이언트 준비 조건:

1. 예상한 설치 Actor가 모두 유효하고 확정 Transform이 적용됐다.
2. 코스·요소의 주행/필수 충돌 컴포넌트가 등록·준비됐다. 단순히 Actor 포인터만 존재하는 상태와 구분한다.
3. 자기 Pawn을 수신했고 Controller 소유 연결이 맞다.
4. 이동 데이터 검증·입력 바인딩·카메라 연결이 끝났다. 이때 조작 허용은 아직 false다.

현재 조회는 `Construction.IsCourseReadyForRace`, `Pawn.IsReadyForRace`, `Controller.IsLocalRaceReady`로 구현했다. `FMWPlacedTrack.bRequiresCollision`은 서버 공개 시 실제 질의 충돌 유무를 기록해 장식용 종료 캡에 충돌을 강제하지 않는다. 필요한 충돌의 등록·물리 상태, 서버 시작 위치 적용, 입력 바인딩·매핑·카메라와 표시 예정 스트리밍 레벨의 로드/표시를 확인한다. Widget 애니메이션 종료나 로비 bReady로 대체하지 않는다.

코스/요소 상태는 각 기존 소유자가 원본을 유지한다. 요소의 확정 Actor 목록과 준비 식별자는 요소 배치 기능의 계약으로 추가하고 트랙 조각 수와 섞지 않는다. 입력·카메라·Actor 수신 순서를 가정하지 않고 준비 관련 변경 시 재평가한다. 현행 Controller Tick의 재평가를 먼저 보존할 수 있으며 이벤트 중심 최적화는 실제 준비 신호를 확보한 뒤 진행한다.

요소 목록은 현재 Epoch의 세션이 확정됐다는 상태와 함께 확인한다. 아직 요소 정보를 받지 못한 빈 목록을 전원 스킵 완료로 해석하지 않는다. PreparingRace에서 코스/요소를 동결한 뒤에는 설치를 추가하지 않으므로 준비 키의 의미가 대기 도중 바뀌지 않는다.

기존 30초 코스 수신 타이머를 출발 준비 제한시간으로 이전했다. GameMode의 `RacePreparationTimeoutSeconds=30`, `CountdownSeconds=3`을 기본값으로 사용하며 기존 BP 기본값에서 조정할 수 있다. 타이머 하나와 `Progress.PhaseEndServerTime`을 두 Phase에서 재사용한다. 준비 시간초과·출발 전 시작 참가자 이탈은 Failed로 처리한다. 카운트다운 만료 시 전원·코스·차량을 재검증하고 `EnterPhase(Racing)`으로 전환한다. 표시용 남은 시간은 `GameState.GetPhaseSecondsRemaining()`으로 조회한다.

## 8. 체크포인트·점수·복귀·결과

### 8.1 체크포인트와 점수

서버 Trigger 감지 → GameMode의 체크포인트 규칙 → PlayerState 기록 순서로 처리한다.

- 현재 Epoch, Racing, 유효 참가자, Active 상태, 다음 인덱스, 진행 방향을 검사한다.
- 지급 키는 (RoundEpoch, CheckpointIndex, LobbyMemberId)다. PlayerArray 순서를 참가자 ID로 쓰지 않는다.
- 통과 순서를 확정하고 중복 기록을 먼저 남긴 뒤 점수와 진행 인덱스를 갱신한다.
- 라운드 시작 때 고정한 N에 따라 유효 도착 순서별 N, N-1, …, 1점을 지급한다. 중도 이탈로 이미 지급한 점수를 재계산하지 않는다.
- 지급 시 RoundScore와 TotalScore를 함께 올린다. 결과 화면에서 RoundScore를 TotalScore에 다시 더하지 않는다.
- 마지막 체크포인트는 결승 역할을 겸한다. 별도 결승 이벤트로 점수를 다시 지급하지 않는다.
- 이전 라운드의 체크포인트도 새 Epoch에서 다시 지급한다.
- 예측 재실행/표시 접촉에서 점수 처리를 실행하지 않는다.

현재 TrackDefinition은 DisplayName과 TrackActorClass 중심이며 체크포인트·안전 복귀 메타데이터의 Runtime 소비가 없다. 기존 명세의 데이터 계약을 후속 작업으로 인수해야 한다. 이번 MD는 해당 에셋 필드를 제작하거나 TA 인수를 완료한 기록이 아니다.

### 8.2 복귀

일반 경기의 추락·파괴·거리 차 탈락은 Active → PendingRespawn으로 통일한다. 원인별 Phase나 개인 상태를 늘리지 않는다.

- PendingRespawn은 주행 입력·점수 판정에서 제외한다.
- 선두의 다음 유효 체크포인트 통과를 복귀 계기로 사용한다.
- 같은 Pawn을 유지하고 안전한 서버 위치로 이동시킨다. 여러 차량은 후보 슬롯을 나눠 배치한다.
- 이동 전 저장 입력·속도·충돌 회전·아이템/임시 효과를 정리하고 기존 TeleportTo/스트림 초기화 경로를 사용한다.
- 복귀 위치에 맞춰 NextCheckpointIndex를 전진시키며 건너뛴 점수는 지급하지 않는다.
- 전원 이탈·선두 정지에는 하나의 구조 대기 기한을 둔다. 기한 후 제한된 안전 후보로 복귀를 시도하고 유효 후보가 없으면 라운드를 종료한다. 무한 대기/탐색은 금지한다.
- 선두 거리 차 자동 탈락은 필수 규칙이다. 거리 측정 방식·임계값·평가 주기·복귀 후 재개 조건은 미정이므로 별도로 확정한다.
- 서든데스는 PendingRespawn을 사용하지 않고 Out으로 종료한다.

### 8.3 라운드·최종 결과

전원 Finished/Out 또는 주행 제한시간 만료 시 서버가 RoundResults에 한 번 들어간다. PendingRespawn을 성공적인 완주로 세지 않는다. 주행 마감 이후 들어온 체크포인트는 인정하지 않는다.

결과는 GameState의 불변 값 스냅샷으로 보관한다.

- RoundEpoch, 결과 종류(라운드/최종/중단), 종료 사유, 우승자 ID.
- 각 행의 참가자 ID·이름·이번 점수·누적 점수·완주/미완주·기록.
- PlayerState 포인터만 저장하지 않는다. 참가자 이탈/개인 기록 초기화 후에도 결과를 표시한다.
- Progress와 결과가 별도로 복제되면 Epoch와 결과 종류가 현재 결과 Phase에 맞을 때 표시한다. 결과가 먼저/나중에 도착해도 재평가한다.
- 결과 표시 기한은 설정으로 두고 그 기한 이후 공통 진행 검사에서 다음 경로를 결정한다. 모든 클라이언트의 화면 확인 ACK는 첫 구현에 추가하지 않는다.

라운드 종료 시 목표 달성자 0명은 다음 일반 라운드, 1명은 MatchResults, 복수는 서든데스로 진행한다. 주행 도중 점수 초과만으로 매치를 끝내지 않는다. 목표 점수 2/3/4인 20/35/50은 기존 기획의 시험 가설값이다.

TargetScore는 매치 시작 때 확정하고 참가자 이탈로 수시 변경하지 않는다. 이탈자는 Logout 전에 이름·점수·진행 기록을 서버의 종료 참가자 레코드에 값으로 보존한다. 이 레코드는 살아 있는 PlayerState와 동시에 갱신하는 점수 원본이 아니라 이탈 시점의 결과 자료다. 최소 정책에서는 이탈자를 최종 우승 후보에서 제외하고 결과표에 이탈/미완주로 표시한다.

서든데스는 기존 코스를 사용하며 복귀하지 않는다. 완주/생존 순서, 전원 탈락 시 역탈락 순서와 진행도·시간 기록을 사용한다. 참가 범위·완전 동률·점수 추가 여부는 아직 확정되지 않은 결정으로 남긴다.

Racing의 종료 조건은 RaceMode를 읽는다. 일반 경기는 전원 종료/주행 마감, 서든데스는 확정된 완주·마지막 생존·전원 탈락/마감 규칙을 적용한다. 서든데스의 RoundResults에서는 목표 점수를 다시 검사해 서든데스를 반복 시작하지 않고, 해당 주행에서 확정한 결과로 MatchResults에 들어간다.

## 9. 다음 라운드의 코스 확장과 재경기

### 9.1 현행 배치의 한계

현재 BeginPlacement는 PlacedTracks가 비어 있어야 시작하고 매번 시작 조각/종료 조각을 준비한다. FinishPlacement는 OpenEnds를 비운다. CancelPlacement는 이미 설치한 도로를 제거하지 않는다.

따라서 RoundResults에서 PreparingConstruction으로 돌아가는 것만으로 두 번째 라운드가 동작하지 않는다. 다음 변경은 Phase 이관과 구분하여 구현한다.

### 9.2 코스와 건설 세션의 수명 분리

기존 Construction 컴포넌트 안에서 다음 책임을 구분한다. 별도 TrackManager를 중복 추가하지 않는다.

| 수명 | 유지할 것 |
|---|---|
| 매치 코스 | 설치 도로·요소·안정적인 PieceId·CourseRevision·끝점·체크포인트 순서 |
| 건설 1회 | SessionId·후보·선택·미확정 Actor·프리뷰·배치 완료 기록 |

다음 라운드는 시작 조각을 다시 만들지 않고 기존 말단에서 참가 인원수만큼 연장한다.

- 종료 캡은 단일 포트이므로 일반 도로처럼 새 출구를 가진다고 가정하지 않는다.
- 기존 말단의 연결 프레임을 보존하고 종료 캡을 이동/교체하여 새 조각을 잇는다.
- 이전 결승의 논리 체크포인트는 유지한다. 종료 캡 Actor 수명과 체크포인트 기록을 분리해 캡을 옮겨도 이전 체크포인트가 사라지지 않게 한다.
- 현재의 PlacedTracks.Num()+1을 그대로 쓰면 종료 캡 제거 뒤 PieceId가 재사용될 수 있다. 매치 안에서 증가하는 NextPieceId를 사용하고 기존 ID를 재사용하지 않는다.
- 도로의 성공한 연결 순서로 체크포인트 인덱스를 관리한다. 폐기된 캡 ID나 현재 배열 인덱스로 영구 진행도를 식별하지 않는다.
- 기존 도로/요소와 TotalScore는 보존하고 차량은 최초 출발 그리드로 다시 배치한다.
- 현재 세션의 성공 확인·준비 집합·요청 응답 캐시·개인 RoundState는 초기화한다.
- 코스 정보 구조 분리는 이 연장 기능을 구현하는 단계에서 한다. 단일 Phase 전환 정리만을 위해 전체 건설 코드를 먼저 재작성하지 않는다.

### 9.3 재경기

MatchResults의 호스트 소유 Controller 요청으로만 승인한다. 해당 결과 Epoch와 현재 Phase를 검사한다. 기본 제안은 현재 연결·방 설정·선택 차량을 유지하고 같은 테마에서 새 경기로 시작하는 것이다.

요청 handler는 재경기 의도를 한 번 기록하고 RequestFlowUpdate만 호출한다. 공통 진행 검사에서 그 의도를 소비하고 재경기 정리와 최초 단계 진입을 실행한다. UI/RPC callback에서 재경기용 EnterPhase를 직접 호출하지 않는다. 최종 결과가 확정된 뒤의 참가자 퇴장으로 이미 공개한 우승 기록을 다시 계산하지 않는다.

1. 기존 단계/진행 예약 타이머와 이전 작업을 취소한다.
2. 입력 차단, 차량·효과·아이템·이전 결과를 정리한다.
3. Construction 소유의 동적 도로/종료 캡/요소를 명시적으로 제거하고 코스/개인 건설 상태를 초기화한다. CancelPlacement만으로 대체하지 않는다.
4. 유효한 남은 참가자로 명단을 다시 구성하고 TotalScore를 0, RoundIndex를 1로 설정한다.
5. 새 RoundEpoch를 발급하고 공통 EnterPhase로 시작한다.
6. 같은 월드의 SelectionSessionId·CourseRevision·NextPieceId 카운터를 과거 값으로 되돌리지 않는다. 이전 준비 키·RPC가 새 경기에서 승인되지 않게 한다.

방 나가기는 기존 MWGameInstance.RequestLeaveRoom/MWConnection 경로를 사용한다. 방을 유지한 전원 L_Title 로비 복귀와 개인의 방 나가기는 구분한다. 전원 로비 복귀에는 서버 맵 이동, 로비 Ready/시작 대상 재설정, 연결 모듈을 통한 입장 재개가 필요하며 현재 코드에서 이 전체 경로가 완성됐다고 가정하지 않는다. 별도 후속 연동 전까지 기존 방 나가기/타이틀 복귀를 로비 유지 복귀로 표시하지 않는다. 재경기 때문에 EOS API나 연결 플러그인의 책임을 GameMode로 옮기지 않는다.

## 10. 최소 예외 정책

아래는 상태 수를 늘리지 않기 위한 구현 제안이다. 새 합의가 필요한 세부 규칙은 13절에 분리한다.

| 상황 | 정책 |
|---|---|
| 건설/출발 준비 중 참가자 이탈 | 현재의 엄격한 시작 대상 정책을 유지하여 Failed 중단 |
| Racing 중 원격 이탈 | 기록 보존, 해당 참가자를 Out과 대기 집계 제외로 처리. 남은 2명 이상은 계속 |
| 유효 인원 2명 미만 | 승자를 자동 부여하지 않고 Failed 중단을 기본 제안으로 사용 |
| 호스트 종료 | 기존 연결 종료로 매치 종료. Host Migration 추가 없음 |
| 초기 도착/코스/차량 준비 시간초과 | Failed와 구체 사유 |
| 트랙 배치 시간초과 | 기존 기획의 18초 후 제한된 유효 기본 후보로 보완. 모두 실패하면 Failed |
| 요소 배치 시간초과 | 미배치자는 스킵 확정. 이미 설치한 요소는 유지하는 최소안 |
| 주행 시간초과 | 남은 참가자 Out/미완주로 정리하고 RoundResults |
| 복귀 장기 대기 | 제한된 구조 복귀, 불가 시 라운드 종료 |
| 중복 완료/준비/종료 | 세션·Epoch·성공 기록·집합·현재 Phase로 한 번만 처리 |
| 오래된 RPC/콜백 | 현재 Epoch·세션·리비전·Phase를 확인하고 정상 거절/무시 |
| 정상 요청 거절 | 해당 요청만 거절. Failed 전환이나 연결 종료로 처리하지 않음 |

경기 명단은 시작 후 GameMode가 관리한다. GI.StartingPlayers는 초기 인계 자료로 남기고 매번 그 원본 인원수만으로 준비/완주를 기다리지 않는다. 이번 라운드의 점수 기준 N은 시작 때 고정하고 다음 라운드에서만 새 참가 인원을 적용한다.

정상 stale 거절을 WithValidation 실패로 처리하지 않는다. 체크포인트 지급, 결과 확정, 로비 이동도 서버에서 한 번만 확정한다.

## 11. 실제 파일별 변경 계획

모두 후속 구현 대상이다. 이 문서 작성에서 아래 소스는 변경하지 않는다.

| 파일/영역 | 후속 변경 |
|---|---|
| System/MWRoomTypes 또는 별도 작은 GameplayTypes | 결과 Phase 추가, 경기 방식·진행/개인/결과 구조체. 기존 enum 값 보존 |
| MWGameplayGameState | Progress/결과 복제, 기존 Getter 호환, 공통 Notify. 정책 판단 제거 |
| MWGameplayGameMode | RequestFlowUpdate/EvaluateNextPhase/EnterPhase/실패 기록, 최초 진입·타이머·명단 정리 |
| MWGameplayGameMode의 StartTrackPlacement | 이벤트 직결 해제, TrackPlacement 진입 helper로 사용 |
| MWGameplayGameMode의 PrepareRace | 차량 준비 결과 반환. 내부 Racing/Failed 직접 전환 제거 |
| MWGameplayGameMode의 WaitForCourseDelivery | PreparingRace의 준비 제한시간으로 이동. callback은 진행 검사 예약 |
| MWGameplayGameMode의 FailGameplay | 실패 기록으로 축소, 자원 정리는 Failed 진입으로 이동 |
| MWTrackConstructionComponent/Placement | 직접 Phase 쓰기 제거, 완료/실패 Delegate, 성공/취소 구분, 이후 연장 API |
| MWGameplayPlayerController | 준비 RPC/키/허용 Phase와 입력 조건 갱신. 기존 수신 재평가 보존 |
| MWPlayerState | TotalScore와 PlayerRound 복제, 로비/건설 기존 계약 보존 |
| MWVehiclePawn/Movement | 경기 제어 적용용 진입점, 정지·스트림 경계 연결. 현행 예측/표시 보존 |
| MWTrackSelectionTestGameMode | 바뀐 Delegate를 수신하고 테스트 단계 진입을 직접 명시. 컴포넌트의 암묵적 Phase 변경에 의존하지 않음 |
| MWDriveTestGameMode/차량 자동화 | 기존 단독 주행 시험의 명시적 동작 보존 |
| 후속 Race/요소 기능 | 서버 체크포인트 감지·안전 위치·요소 완료 계약을 실제 데이터와 연결 |

현재 MWTrackSelectionTestGameMode.TryFinishSelection은 컴포넌트의 TrackPlacement 변경에 의존한다. setter 삭제만 적용하면 FindPendingTrack의 Phase 검사를 통과하지 못한다. 테스트 GameMode도 자기 테스트 전환을 명시하도록 같은 변경 단위에 포함한다.

## 12. 구현 순서와 검증 기준

### A. 기존 동작을 보존하는 전환 경로 정리

- 초기 진입 식별과 공통 판단에 필요한 GameState Progress 구조체를 이 단계에서 함께 도입한다. 개인 주행 기록/입력 허용의 실제 소비는 B에서 연결한다.
- 현행 이벤트/호출자를 중앙 진행 경로로 연결하고 컴포넌트의 Phase 쓰기를 제거한다.
- 현재 PrepareRace의 스폰/Possess·부분 실패 회수 동작을 helper에 보존한다.
- 이 단계에서 코스 대기를 TrackPlacement에 유지할 수 있다. PreparingRace 이동·카운트다운 추가는 B에서 함께 검증한다.
- 완료: 기존 선택·배치·코스 전달·차량 생성 흐름이 유지되고 production의 실제 Phase 변경이 EnterPhase 한 경로를 통과한다.

### B. 개인 경기 상태와 준비·카운트다운·결과

- 준비 보고를 PreparingRace로 이동하고 차량·입력·카메라 준비를 포함한다.
- 경기 제어 조건, 3초 카운트다운, 서버 주행 제한시간과 RoundResults를 연결한다.
- 처음에는 주행 제한시간으로 실제 결과에 도달하게 하여 미구현 체크포인트 성공을 꾸며내지 않는다.
- 완료: 전원 준비 전/카운트다운 중 움직이지 않고, 출발 후 조작되며 같은 결과를 표시한다.

### C. 체크포인트·점수·복귀와 두 번째 라운드

- 데이터 인수 후 서버 순서/중복/방향·점수·복귀를 연결한다.
- 코스/세션 수명을 나누고 종료 캡·안정적인 ID·체크포인트를 보존하며 연장한다.
- 완료: 점수 중복 없이 복귀하고 기존 코스를 유지한 두 번째 라운드를 진행한다.

### D. 최종 결과·서든데스·재경기·요소 실기능

- 미정 규칙을 확정한 범위부터 최종 결과와 재경기를 연결한다.
- 요소 배치의 실기능을 완료 이벤트와 최종 준비 조건에 통합한다.
- 완료: 3인으로 두 라운드 이상 진행하고 최종 결과·재경기까지 도달한다.

각 단계의 검증은 다음을 공통으로 적용한다.

| 검증 | 기대 결과 |
|---|---|
| 호스트 + 원격 2명 정상 진행 | Phase/Epoch/점수/결과 일치. 호스트도 상태 알림과 준비 보고 수행 |
| 같은 완료/준비 이벤트 반복 | 중복 스폰·타이머 재시작·다중 Phase 진입 없음 |
| 배치 취소/이전 닫힌 세션 | 성공으로 진행하지 않음 |
| 선택 → 배치 전환 | 확정 선택 소비 전에 참가자 정보가 지워지지 않음 |
| 지연된 Actor/충돌/Pawn 준비 | 실제 준비 전 Countdown 진입 없음, 기한 초과 정리 |
| 기한 직전 정상 완료 | 예약된 검사 실행 지연만으로 실패하지 않음 |
| 이전 타이머/이전 Epoch 요청 | 새 라운드·재경기에 영향 없음 |
| 카운트다운·복귀·결과에서 입력 유지 | 이전 가속/드리프트·이동 패킷이 재허용 뒤 살아나지 않음 |
| 체크포인트 왕복/순서 건너뜀 | 중복/잘못된 점수 없음. 마지막 체크포인트 점수 1회 |
| 동시 추락·선두 정지 | 제한시간 내 구조/라운드 종료, 영구 대기 없음 |
| 참가자 이탈 | 정책에 맞춰 중단 또는 집계 제외, 기존 지급 점수 재계산 없음 |
| 다음 라운드·재경기 | 유지/초기화 항목 정확, PieceId·세션·Epoch 재사용 없음 |
| 기존 차량/건설 시험 | DriveTest·TrackSelectionTest와 기존 자동화 동작 보존 |

구현 시 관련 Editor/Game 빌드, BP enum/참조, 기존 MW.Vehicle 6개와 MW.Construction.PreviewMatchesSnap 및 추가 규칙 검증을 수행한다. 이는 772d2e0의 실제 테스트 이름을 확인한 것이다. 3창 PIE·독립 프로세스·실제 3PC 패키지 결과는 구분하여 기록한다. 이 문서 작성만으로 어느 실행 검증도 통과했다고 표시하지 않는다.

## 13. 아직 확정하지 않은 값과 규칙

설계를 위해 필요하지 않은 상태를 추가하지 않되 다음 결정은 숨기지 않는다.

| 항목 | 확정할 내용 | 독립 진행 가능한 작업 |
|---|---|---|
| 거리 차 탈락 | 거리 지표·임계값·선두 선정·평가 주기·복귀 후 재개 | 공통 PendingRespawn 경로 |
| 경기/구조 시간 | 주행 제한시간·구조 대기·결과 표시 시간, 준비 제한시간 튜닝 | 설정 필드와 타이머 경로 |
| 안전 복귀 데이터 | 실제 트랙별 후보·간격·충돌 여유, 데이터 생산자/소비자 | 동일 Pawn 복귀 API와 상태 |
| 목표/상한 | 목표 점수 튜닝, 라운드/트랙 상한 및 도달 시 승자 규칙 | 점수 기록·결과 스냅샷 |
| 서든데스 | 참가 범위·완전 동률·점수 추가 여부 | 같은 준비/카운트다운/주행 경로 재사용 |
| 요소 배치 | 실제 요소 카탈로그·슬롯·기한·준비 정보 | 완료/스킵/실패 보고 계약 |

10절의 최소 인원 중단·요소 마감 스킵은 이 문서의 제안 정책이며 기존 Runtime 구현 완료가 아니다. 결정이 필요한 규칙을 임의 수치나 숨은 성공 처리로 대신하지 않는다.

## 14. 작성 검증과 참고

이번 산출물의 검증 범위:

- 기준 브랜치/커밋·작업 상태 확인, 관련 실제 클래스·호출자·복제 필드·차량 최신 API 조사.
- 현재 직접 Phase 변경 지점, 완료/취소 구분, 테스트 GameMode 의존성, 다음 라운드 확장 한계 대조.
- 문서 링크·Markdown 구조·용어/정책 일관성·소스 미변경 확인.
- 게임 실행·컴파일·패키징은 이번 문서 작업의 검증 결과에 포함하지 않는다.

주요 참고:

- [08 리슨 서버와 Framework](08_리슨서버_구조와_Gameplay_Framework.md)
- [16 로비/게임플레이 계약](16_로비_게임플레이_분리와_차량선택_전환_계약.md)
- [현재 Pawn/차량 예측·표시 문서](../../Engineering/Client/VEHICLE_PAWN_LOCAL_MOVEMENT.md)
- [개발 공통 가이드](../../Engineering/DEVELOPMENT_GUIDE.md)
- [Epic Gameplay Timers](https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-timers-in-unreal-engine): 타이머와 다음 갱신 예약 API. 설치 5.8.4 헤더와 함께 확인한다.
- [Epic Replicated Object Execution Order](https://dev.epicgames.com/documentation/en-us/unreal-engine/replicated-object-execution-order-in-unreal-engine): 서로 다른 속성/Actor의 수신 순서를 준비 완료로 가정하지 않는 근거.

문서의 제안 이름·의사코드는 아직 호출 가능한 구현 API가 아니다. 후속 구현은 A → B → C → D 순서로 검증 가능한 단위로 진행하며 사용자 소스 수정 요청 전에는 착수하지 않는다.

