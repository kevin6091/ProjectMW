# 게임플레이 진행 상태와 Phase 단일 전환 설계

- **문서 번호:** MW-GAMEPLAY-FLOW-018 / v1.0
- **작성:** 2026-11-12 / Client + Server
- **기준:** fix/Network-Prediction, HEAD 772d2e0, 조사 시작 시 작업 트리 깨끗함
- **엔진:** 설치 UE 5.8.4 / CL 58902052, MW.uproject의 EngineAssociation은 5.8
- **상태:** 구현 전 설계. 이번 작업은 이 문서와 문서 연결만 작성하며 Runtime 소스·에셋·설정을 수정하지 않는다.

## 1. 목적과 적용 범위

기존 트랙 선택·배치에 경기 규칙을 연결한다. 콜백마다 다음 단계를 직접 시작하는 구조를 다음 경로로 통일한다.

```text
완료 이벤트 / 소유 Controller RPC / 타이머 / 참가자 이탈
 → 검증 및 서버 상태 기록
 → RequestFlowUpdate: 공통 진행 검사 한 번 예약
 → TryAdvanceGameplay: 현재 단계의 조건 판단
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
- 서버가 예상 키를 고정하고 준비 집합을 비운 뒤 PreparingRace를 공개한다.
- 클라이언트는 Progress와 자신의 PlayerRound가 같은 Epoch이고 확정 코스/개인 상태가 준비되면 한 번 보고한다.
- 서버는 소유 Controller/참가자/Phase/키/기한을 검사하고 집합에 추가한 뒤 RequestFlowUpdate를 호출한다.
- 호스트도 로컬 Controller의 동일 조건과 RPC 진입 경로를 사용한다.
- 현재 LastReadyPlacementSessionId를 Epoch·SessionId·Revision 전체 키로 바꾼다. 서든데스는 같은 코스라도 새 Epoch이므로 다시 보고해야 한다.
- 로컬 보고 키는 Controller 객체 수명에 맞춰 관리하고 새 키의 준비 완료 전에는 완료로 기록하지 않는다.

클라이언트 준비 조건:

1. 예상한 설치 Actor가 모두 유효하고 확정 Transform이 적용됐다.
2. 코스·요소의 주행/필수 충돌 컴포넌트가 등록·준비됐다. 단순히 Actor 포인터만 존재하는 상태와 구분한다.
3. 자기 Pawn을 수신했고 Controller 소유 연결이 맞다.
4. 이동 데이터 검증·입력 바인딩·카메라 연결이 끝났다. 이때 조작 허용은 아직 false다.

현재 이 조건의 완전한 조회 API는 없다. Pawn/Controller에 읽기 전용 준비 조회를 제공하는 작업이 필요하다. Widget 애니메이션 종료나 로비 bReady로 대체하지 않는다.

코스/요소 상태는 각 기존 소유자가 원본을 유지한다. 요소의 확정 Actor 목록과 준비 식별자는 요소 배치 기능의 계약으로 추가하고 트랙 조각 수와 섞지 않는다. 입력·카메라·Actor 수신 순서를 가정하지 않고 준비 관련 변경 시 재평가한다. 현행 Controller Tick의 재평가를 먼저 보존할 수 있으며 이벤트 중심 최적화는 실제 준비 신호를 확보한 뒤 진행한다.

요소 목록은 현재 Epoch의 세션이 확정됐다는 상태와 함께 확인한다. 아직 요소 정보를 받지 못한 빈 목록을 전원 스킵 완료로 해석하지 않는다. PreparingRace에서 코스/요소를 동결한 뒤에는 설치를 추가하지 않으므로 준비 키의 의미가 대기 도중 바뀌지 않는다.

기존 30초 코스 수신 타이머는 출발 준비 제한시간 설정으로 이전·확장한다. 30초는 현행 값이며 최종 밸런스/UX 확정값으로 보장하지 않는다. 준비 시간초과는 새 LoadingFailed Phase 없이 Failed 사유로 처리한다.

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

