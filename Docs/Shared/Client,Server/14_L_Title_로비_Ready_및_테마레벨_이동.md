# L_Title 로비 · Ready · 선택한 테마 레벨로 이동

> 2026-11-02 연결 모듈 분리: L_Title 로비 흐름은 유지하며, EOS·세션 책임은 MWConnection으로 옮겼다. 아래 Subsystem 내부 멤버·온라인 등록 플래그·시작 처리 설명은 당시 기록이다. 현재 경계·API·검증 결과는 [15번 문서](15_독립_연결모듈_API와_프로젝트_연동.md)를 우선한다.

2026-10-29 사용자 지시로 적용한 맵 흐름이다. Client + Server, UE 5.8.4. 10~13번 문서에서 생성 직후 L_Dev_Default로 이동하던 설명과 과거 검증 기록은 당시의 구조이며, 현재 동작은 이 문서를 따른다.

## 현재 레벨 흐름

```text
L_Title (Standalone)
  로그인 → 타이틀 → 방 이름·정원·테마 입력
  → EOS 세션 생성
L_Title?listen (호스트의 Listen Server 로비)
  다른 계정도 방 선택·참가 후 같은 L_Title에 접속
  → 호스트 포함 현재 전원 Ready, 최소 2명
  → 호스트 게임 시작
  → 서버가 권한·인원·등록·Ready·현재 로비를 재검사
  → 입장 잠금 + 참가자 스냅샷 + 온라인 세션 참가 불가 갱신
  → 갱신 완료 후 참가자와 준비 상태 재검사
  → 선택 Theme의 레벨로 Seamless ServerTravel
선택한 테마 레벨 (현재는 L_Dev_Default)
  같은 세션·참가자 유지, 로비/타이틀 UI 숨김, GameOnly 입력
```

방 생성 후에도 레벨 패키지는 L_Title이다. 다만 로컬 메뉴에서 네트워크 서버로 바뀌므로 호스트는 **동일한 L_Title을 Listen 옵션으로 다시 연다.** 참가자는 기존 방 목록·JoinSession·ClientTravel 경로로 이 Listen 로비에 연결한다. 경기 시작 시 호스트만 OpenLevel하는 방식은 사용하지 않는다.

## 기존 MW 기능과 테마 계약

기능 정본은 기존 MWMultiplayerSubsystem, MWGameMode, MWGameState, MWPlayerState, MWPlayerController와 MW UI 클래스다. 삭제한 Frontend C++·제작 플러그인을 다시 사용하지 않는다. 기존 Frontend 디자인을 가진 WBP 7개도 그대로 사용한다.

- `PendingMapPackageName`: 생성할 Listen 로비의 패키지, 즉 프로젝트 기본 맵 L_Title.
- `PendingGameplayMapPackageName`: 승인한 Theme를 AssetData/AssetManager로 해석한 경기 목적지.
- `PendingJoinMapPackage`: 참가 완료를 확인할 로비 L_Title. 선택한 Theme와 실제 세션 ID도 별도로 확인한다.
- `FMWRoomSettings.MapThemeId`: 선택한 테마 ID. Widget이 레벨 경로를 지정하지 않는다.

`Asset.MapTheme.DevDefault → DA_AssetData → MWAssetManager → /Game/Levels/L_Dev_Default` 연결을 유지했다. 생성 때 테마 유효성만 검사하고, 게임 시작 때 같은 승인 카탈로그에서 목적지를 다시 확인한다. 카탈로그·테마 에셋·레벨 경로 표를 중복 생성하지 않았다.

맵 흐름이 다른 이전 실행과 섞이지 않도록 온라인 프로토콜과 버킷을 `MW_DEV_V2`로 올렸다. 양쪽 실행 모두 새 빌드가 필요하며 V1 방은 새 목록에 섞이지 않는다.

## 클래스별 변경

| 대상 | 현재 책임 |
|---|---|
| L_Title World Settings | 기본 GameMode를 기존 BP_MWGameMode로 지정. MW GameState/PlayerState/Controller를 메뉴와 Listen 로비에서 함께 사용 |
| MWGameState | `EMWRoomPhase { Lobby, Starting, InGame }` 복제. 현재 참가자 전원의 등록·ID·Ready·최소/최대 인원 검사 |
| MWPlayerState | 기존 Ready/호스트/ID/등록 확인을 사용. Seamless Travel의 CopyProperties에 해당 상태 복사 |
| MWPlayerController | 로컬 Ready/Start 요청 → 소유 Server RPC → 서버 GameMode 검증. 실패는 요청자에게만 알림 |
| MWGameMode | 자신의 Ready만 변경, 시작은 Listen Host만 허용. 입장 잠금과 실제 시작 조건 검증. Seamless Travel 활성화 |
| MWMultiplayerStart.cpp | **기존 Subsystem의 동일 클래스**에 시작·서비스 갱신·공동 이동·완료/실패 정리 구현 분리 |
| MWRoomLobbyWidget | 복제 Ready로 버튼 글자/표시 갱신. 전원 준비 때 호스트만 시작 활성화. Starting 중 조작 잠금, 테마 월드에서 숨김 |
| MWTitleRootWidget | Standalone L_Title에서 메뉴 표시, 네트워크 L_Title에서는 메뉴를 숨겨 로비와 겹치지 않게 함 |

`EMWSessionState`는 로그인 후 생성/검색/참가/정리 작업 상태다. InGame에서도 호스트는 Hosting, 참가자는 Joined를 유지하며, 로비/시작/경기 월드 구분은 GameState의 RoomPhase가 담당한다. 이후 건설·레이스 규칙 전체를 구현했다는 뜻은 아니다.

## 서버 시작 조건과 경합 처리

호스트도 Ready를 눌러야 한다. 정원은 최대 허용 인원이며, 정원 3명 방에 현재 2명이 있으면 그 2명이 모두 준비한 뒤 시작할 수 있다. 기본 최소 인원은 09번 문서대로 2명이다.

1. 소유 Controller로 요청자를 식별한다. UI가 PlayerId를 보내 다른 사람의 Ready를 바꾸지 않는다.
2. Ready 변경은 실제 Listen 로비에서 연결·등록이 끝난 현재 참가자에게만 허용한다.
3. 시작 요청은 Lobby 상태의 호스트만 허용한다. UI 활성화와 별개로 서버가 재검사한다.
4. 입장을 먼저 닫고 Starting을 복제한다. Ready/시작/나가기 버튼은 잠근다.
5. 온라인 세션 광고와 진행 중 참가를 끄고 상태를 Starting으로 갱신한다. 서버 PreLogin/PostLogin도 입장 잠금을 검사하므로 오래된 방 목록으로 시작 중 방에 들어갈 수 없다.
6. 갱신 응답이 와도 시작 때 확정한 참가자 ID 스냅샷과 전원 Ready를 다시 확인한다. 인원 변경·서비스 실패 등으로 안전하게 시작할 수 없으면 기존 세션 정리 경로로 방을 종료한다.
7. 서버가 AssetManager로 해석한 목적지로 한 번만 ServerTravel한다. 늦은 콜백·제한시간·이동 실패는 기존 정리 상태에 연결한다.

온라인의 Starting 값은 입장이 닫힌 상태를 뜻한다. 실제 테마 월드 준비 완료는 서비스 문자열만으로 판정하지 않고 새 MWGameState의 InGame·세션 ID·테마와 서버 월드로 확인한다. 맵 이동 중 GameState/PlayerState 수신 순서를 가정하지 않는다.

## 실제 검증

같은 PC의 독립 Standalone PIE 두 창과 서로 다른 실제 EOS 계정으로 검증했다. UI 전용 fixture나 가짜 로그인 상태를 만들지 않았다.

| 항목 | 확인 결과 |
|---|---|
| 생성·참가 목적지 | 두 창 모두 L_Title. Hosting/Joined, 같은 인원·ID·이름 표시 |
| 메뉴/로비 중복 | Standalone은 메뉴 표시·로비 숨김, 네트워크 로비는 메뉴 숨김·로비 표시 |
| 1인 시작 | Ready를 해도 최소 2명 조건으로 거절, L_Title 유지 |
| 전원 Ready 전 시작 | 거절, Lobby 유지 |
| 원격 시작 요청 | 전원 준비 여부와 무관하게 시작 불가 |
| Ready 및 취소 | 실제 원격 Ready 버튼 클릭과 취소/재준비가 양쪽 PlayerState/화면에 반영 |
| 시작 활성화 | 전원 Ready일 때 호스트만 활성화 |
| 실제 시작 클릭 | Starting 복제·조작 잠금 후 두 창 모두 L_Dev_Default로 Seamless Travel |
| 이동 후 상태 | 양쪽 InGame, 같은 ID 01·02와 이름·등록/Ready 유지, 세션 Hosting/Joined 유지 |
| UI/입력 | 테마 월드에서 타이틀 없음·로비 숨김·커서 플래그 false |
| 중복/경기 중 요청 | 두 번째 이동이나 Ready 변경을 만들지 않음 |
| 진행 중 방 목록 | 참가자가 나가서 새 조회했을 때 진행 중 호스트 방 0개 |
| 이동 후 종료·재생성 | 참가자/호스트 모두 L_Title의 메뉴·None으로 복귀. 재생성·재참가 후 두 명 모두 Ready=false, ID/인원 정상 초기화 |
| 로비 이탈·종료 | 참가자 나가면 1명으로 감소, 호스트 종료 후 세션 정리 |
| 최종 빌드·로드 | Win64 Development Editor/Game 빌드 성공. 재실행 후 저장된 L_Title GameMode·Seamless 설정·정식 WBP 7개·새 Ready/시작 툴팁 확인 |
| 변경 범위·게임 오류 | 콘텐츠 실파일 변경은 L_Title 하나. 나머지 기존 콘텐츠 84파일 해시 동일. 실제 이동 시험에서 Blueprint Runtime Error / Accessed None / Fatal / Assertion failed 검색 결과 0건 |

주요 근거는 `Saved/TitleLobbyFlow`의 JSON과 빌드 로그다. 과거 12/13번 검증과 이번 L_Title 로비 검증을 구분한다. 3인·다른 PC/WAN·패키징·강제 네트워크 장애/입장 경합의 모든 조합은 미검증이다.

실제 2계정 시험 뒤에는 툴팁 안내와 Editor 전용 PIE 설정 검사를 보완하고 최종 두 타깃을 다시 빌드했다. 그 후 새 PIE에서 메뉴/숨긴 로비와 툴팁을 확인했으며, 두 계정 인증·이동을 다시 반복하지는 않았다. 기존 사용자의 두 Subsystem cpp 서식 변경은 보존했다. Git 커밋·push·UVCS 체크인은 수행하지 않았다.

## PIE 재현 방법

1. L_Title을 연다. New Editor Window PIE, Players=2, Net Mode=Play Standalone, Run Under One Process=켜기, Dedicated Server=끄기로 실행한다.
2. **프로젝트 기본 설정에 `net.AllowPIESeamlessTravel=1`을 저장했으므로 매번 콘솔에 입력할 필요가 없다.** `Config/DefaultEngine.ini`의 `[ConsoleVariables]`에서 에디터 실행 시 자동 적용한다. 다른 설정이나 콘솔로 값을 0으로 덮어쓴 경우에만 시작 안내가 표시된다. 앞선 실제 2계정 이동 검증에서는 콘솔로 같은 값을 켰으며, 이후 자동 적용은 새 프로세스의 시작값으로 별도 확인했다. 일반 게임 실행의 Seamless Travel 설정은 GameMode의 bUseSeamlessTravel이다.
3. 두 창을 서로 다른 EOS 계정으로 로그인한다.
4. 첫 창에서 방 생성 → L_Title 로비인지 확인한다. 둘째 창에서 목록·선택·참가한다.
5. 한 명만 준비했을 때 시작 비활성, 준비 취소 시 다시 비활성인지 확인한다.
6. 호스트 포함 두 명 모두 준비한 뒤 호스트의 게임 시작을 누른다. 두 창이 선택한 테마 맵에 같이 도착하는지 확인한다.

현재 범위는 **테마 레벨로 함께 이동하는 것**까지다. 차량 스폰·건설 UI·카운트다운·주행, 경기 중 나가기 메뉴, 결과에서 로비로 돌아오는 재경기 흐름은 후속 작업이다. 테마 레벨에 차량이나 경기 HUD가 아직 없는 것은 이번 이동 기능의 실패와 구분한다.
