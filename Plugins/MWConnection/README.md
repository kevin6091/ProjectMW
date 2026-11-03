# MWConnection 공개 인터페이스와 사용법

이 Runtime 플러그인은 로그인·방 생성·목록·참가·최초 월드 연결·종료를 담당한다. `MW` 게임 모듈, 차량, 테마 카탈로그, GameMode 파생 클래스에 의존하지 않는다.

외부에서 읽을 헤더는 두 개다.

```cpp
#include "MWConnectionSubsystem.h"
#include "MWConnectionTypes.h"
```

호출 모듈의 Build.cs에 `MWConnection` 의존성만 추가한다. EOS SDK, OnlineSubsystem, OnlineSubsystemEOS 헤더·라이브러리를 호출 모듈에 추가하지 않는다. 이 플러그인 자체도 EOS 전용 모듈을 링크하지 않고 UE의 온라인 인터페이스를 Private에서 사용한다. 온라인 실행에는 프로젝트의 EOS 플러그인·설정이 별도로 필요하다.

## 1. 진입점과 수명

```cpp
UMWConnectionSubsystem* Connection =
    GameInstance->GetSubsystem<UMWConnectionSubsystem>();
```

GameInstance마다 하나다. 전역 Singleton이 아니며 다른 PIE의 상태를 공유하지 않는다. 모든 요청·조회·이벤트는 게임 스레드에서 사용한다.

프로젝트에서는 GameInstance 같은 한 연결 담당 객체만 이 API를 사용하도록 권장한다. 게임 규칙 객체는 자체 GameState·PlayerState·서버 RPC로 동작한다.

## 2. 요청 API

아래 요청은 모두 `bool`을 반환하고 마지막 인수로 `FText& OutError`를 받는다.

| 함수 | 입력 | 성공 완료의 의미 |
|---|---|---|
| `RequestLogin` | 없음 | 유효한 온라인 계정 인증 완료 |
| `RequestHostRoom` | `FMWConnectionRoomOptions` | 리슨 로비 실행, 온라인 프로필이면 호스트 등록·방 공개까지 완료 |
| `RequestRefreshRooms` | 없음 | 방 목록 캐시 갱신 |
| `RequestJoinRoom` | 현재 목록의 `RoomHandle` | 선택한 온라인 방의 실제 연결·등록·대상 방 확인 완료 |
| `RequestJoinAddress` | `host:port` 문자열 | LocalIP 서버의 실제 연결·방 정보 수신 완료 |
| `RequestSetRoomJoinable` | `bool` | 호스트의 입장 허용 상태 적용, 온라인이면 공개 상태도 갱신 |
| `RequestLeaveRoom` | 없음 | 연결·온라인 세션 정리와 복귀 맵 도착 완료 |

`false`이면 요청을 시작하지 않았다. `OutError`를 표시하며 **완료 이벤트는 오지 않는다.**

`true`이면 요청을 받아들였다. GameInstance가 살아 있는 동안 `OnOperationCompleted`가 해당 작업에 한 번 발생한다. 요청 함수 내부에서 완료 이벤트가 재진입하지 않는다. 일반 작업은 한 번에 하나만 허용하고, 중복·실행 불가능한 요청은 즉시 거절한다.

`RequestLeaveRoom`은 진행 중 작업을 중단하고 정리할 수 있다. 중단한 작업은 `Cancelled`로 완료하며 늦은 SDK 성공이 새 연결을 만들지 않는다. GameInstance 종료 시에는 구독·타이머·SDK Delegate를 해제하고 추가 완료 이벤트를 전달하지 않는다.

## 3. 읽기 전용 조회와 이벤트

```cpp
FMWConnectionSnapshot View = Connection->GetSnapshot();
bool CanHost = Connection->CanExecute(EMWConnectionAction::HostRoom);
```

`GetSnapshot`은 캐시를 복사해서 반환하며 새로운 네트워크 요청을 하지 않는다. `CanExecute`는 UI 활성화 판단용이다. 입력 값과 현재 상태의 최종 검사는 요청 함수에서 다시 수행한다.

| 이벤트 | 용도 |
|---|---|
| `OnSnapshotChanged()` | 현재 상태·메뉴·목록 다시 읽기 |
| `OnOperationCompleted()` | `(EMWConnectionAction, const FMWConnectionResult&)`로 완료 처리 |
| `OnLocalWorldReady()` | 이 GameInstance의 월드 준비 알림. 일반 이동과 Seamless Travel 대상 월드를 포함 |

Snapshot이 먼저 갱신되고 완료 이벤트가 전달된다. 외부는 이벤트를 구독·해제하며 발행하지 않는다. `OnLocalWorldReady`는 최초 메뉴·접속·이동·복귀 월드에서 호출될 수 있으므로, 이벤트마다 방 생성 요청을 다시 보내지 않는다.

## 4. 입출력 타입

### 방 생성 입력

```cpp
FMWConnectionRoomOptions Options;
Options.RoomName = TEXT("My room");
Options.MaxPlayers = 3; // 호스트 포함 총 정원
Options.Properties.Add(TEXT("MapTheme"), TEXT("Asset.MapTheme.DevDefault"));

FText Error;
if (!Connection->RequestHostRoom(Options, Error))
{
    // 즉시 거절. Error 표시. 완료 이벤트를 기다리지 않는다.
}
```

일반 제한은 방 이름 1~64자, 정원 1~64명, 메타데이터 16개 이하, 키 1~32자·값 256자 이하다. 게임의 2~4인 제한이나 테마 유효성은 게임 쪽에서 별도로 검사한다.

`Properties`는 문자열 메타데이터다. 위 `MapTheme`는 사용 예시이며 모듈에 필수인 키가 아니다. 모듈은 태그를 해석하거나 AssetManager를 조회하지 않는다.

### 방 목록

`FMWConnectionRoomInfo`는 `RoomHandle`, `RoomName`, `CurrentPlayers`, `MaxPlayers`, `Properties`, `bJoinable`, `UnavailableReason`을 제공한다.

`RoomHandle`은 해석·생성하지 않고 `RequestJoinRoom`에 그대로 전달한다. 모듈은 현재 목록에 존재하는 참가 가능한 대상만 받는다. 목록의 인원수는 조회 시점의 값이다. 접속 후 실제 참가자는 게임의 PlayerArray로 읽는다.

### 상태와 결과

`FMWConnectionSnapshot`은 연결 상태, 현재 작업, LocalIP 여부, 로그인 여부, 작업 중 여부, 방 연결 여부, 호스트 여부, 방 정보, 목록, 오류 메시지를 제공한다. 복제되는 게임 상태가 아니다. 게임 시작 조건으로 `State == Hosting`을 요구하지 않는다.

`FMWConnectionResult`는 `bSuccess`, `ErrorCode`, `Message`다. 오류 종류는 Busy·InvalidArgument·NotLoggedIn·ServiceUnavailable·RoomUnavailable·ConnectionFailed·Timeout·Cancelled·RestartRequired 범위로 공개한다. EOS 오류 타입은 반환하지 않는다.

## 5. 구독 예시

```cpp
// 담당 UObject의 초기화 시 한 번 구독
SnapshotHandle = Connection->OnSnapshotChanged().AddUObject(
    this, &ThisClass::RefreshConnectionMenu);
ResultHandle = Connection->OnOperationCompleted().AddUObject(
    this, &ThisClass::HandleConnectionResult);

// 종료 시 같은 핸들로 해제
Connection->OnSnapshotChanged().Remove(SnapshotHandle);
Connection->OnOperationCompleted().Remove(ResultHandle);
```

완료 처리의 예:

```cpp
void UMyAdapter::HandleConnectionResult(
    EMWConnectionAction Action, const FMWConnectionResult& Result)
{
    if (!Result.bSuccess)
    {
        ShowError(Result.Message);
        return;
    }
    // HostRoom/JoinRoom 성공 시 연결은 이미 완료됐다.
    // OpenLevel, ClientTravel, RegisterPlayers를 여기서 다시 호출하지 않는다.
}
```

## 6. 실행 설정

기본은 OnlineEOS다. ProjectMW의 기존 EOS 제품·정책 설정을 그대로 사용한다. 세션 프로토콜은 `MW_DEV_V3`이며 이전 구현의 V2 방과 섞이지 않는다.

다음은 선택 설정이며 프로젝트 `Config/DefaultGame.ini`에 둘 수 있다.

```ini
[MWConnection]
Profile=OnlineEOS
Protocol=MW_DEV_V3
TimeoutSeconds=60
```

`LobbyMap`, `ReturnMap`을 생략하면 엔진의 Game Default Map을 사용한다. 경로를 지정한다면 `/Game/Levels/L_Title` 같은 **패키지 경로**를 사용하고 `.L_Title` 객체 이름을 덧붙이지 않는다.

LocalIP 시험은 프로세스 실행 인수 `-MWConnectionLocal` 또는 `Profile=LocalIP`로 선택한다. 이 프로필은 온라인 로그인·검색을 사용하지 않으며, 프로세스의 GameNetDriver를 리슨 드라이버 생성 전에 IpNetDriver로 설정한다. 한 프로세스 안에서 OnlineEOS/LocalIP 프로필을 혼합하지 않는다. 로컬 참가에는 `RequestJoinAddress`를 쓴다.

## 7. 실패 처리

호스트·참가 실패는 부분 연결과 온라인 세션을 정리하고 복귀한다. 새 호스트·참가는 정리가 확인되기 전까지 허용하지 않는다. SDK 작업이 제한시간 뒤 성공해도 내부에서 정리하며 외부에 두 번째 성공을 전달하지 않는다.

정리 결과를 확인할 수 없으면 `RestartRequired`와 실행 불가 상태를 제공한다. 호출자가 SDK Delegate를 지우거나 세션 포인터를 수정해서 복구하지 않는다. 접속한 참가자 한 명의 정상 이탈은 호스트 전체 연결 실패로 처리하지 않는다.

## 8. 모듈 외부에서 하지 않을 일

- Private 헤더 포함·구현 포인터 접근
- EOS/OnlineSubsystem 타입 사용
- 직접 세션 등록·삭제 또는 SDK Delegate 관리
- Host/Join 완료 후 중복 OpenLevel·ClientTravel
- Snapshot을 게임 Phase·Ready·차량 상태의 원본으로 사용

프로젝트의 차량·테마·건설 규칙을 추가할 때 이 모듈은 수정하지 않는다. 추가 온라인 기능이 필요하면 공개 계약 변경으로 별도 다룬다.

## 9. 검증 기록

엔진 기본 GameInstance·GameMode만 사용하는 별도 프로젝트에서 EOS 없이 2프로세스 직접 연결, 실제 EOS 로그인·생성·종료·재생성·목록 조회를 시험했다. ProjectMW에서는 LocalIP 실제 3프로세스의 Ready RPC·테마 이동·ID 유지·복귀를 확인했다. 서로 다른 EOS 계정의 참가와 인터넷 NAT 환경은 이번 변경 후 재검증하지 않았다.

사용자 요청으로 검증 전용 코드·실행 인수·임시 프로젝트·결과 파일은 제거했다. 실제 검증 범위·기존 에셋의 별도 오류는 [프로젝트 연동 문서](../../Docs/Shared/Client,Server/15_독립_연결모듈_API와_프로젝트_연동.md)에 기록한다.
