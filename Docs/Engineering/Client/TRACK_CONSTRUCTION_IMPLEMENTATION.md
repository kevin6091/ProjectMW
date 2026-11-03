# 트랙 선택·실시간 공유 구현

2026-11-03 · Client + Server + Art/TA · feature/track-construction-test

## 구현 범위

GameState의 MWTrackConstructionComponent가 공용 후보·선택 세션·서버 승인을 담당한다. PlayerState의 MWPlayerConstructionComponent가 개인 Hover·확정 선택·중복 요청 결과를 담당한다. 기존 MWPlayerController가 선택 입력·소유자 RPC·로컬 표시를 연결한다.

현재 구현은 트랙 **선택**까지다. 실제 코스 설치·소켓 정렬·고도·간섭 검사·도착 지점 생성은 포함하지 않는다. 테스트 트랙 BP는 엔진 기본 Cube를 조합한 선택판용 도형이며 주행 승인 에셋이 아니다.

## 생성한 에셋

| 경로 | 용도 |
|---|---|
| `/Game/Developers/user/Construction/L_TrackConstruction_Test` | 선택 기능 전용 맵 |
| 같은 폴더의 `BP_TrackConstructionTestGameMode` | 온라인 로그인 없는 개발용 진입 |
| 같은 폴더의 `BP_TrackConstructionTestController` | 로비를 생성하지 않는 기존 MW Controller 자식 |
| `/Game/Gameplay/Tracks/Tests/BP_Track_Straight_Test` | 직선 표시 |
| `/Game/Gameplay/Tracks/Tests/BP_Track_Curve_Test` | 커브 표시 |
| `/Game/Gameplay/Tracks/Tests/BP_Track_Ramp_Test` | 경사 표시 |
| `/Game/Gameplay/Tracks/Tests/M_TrackSelection_Test` | 후보 강조 재질 |
| `/Game/Gameplay/Tracks/Data/DA_Track_*_Test` | 종류별 메시·BP 정의 |
| `/Game/Data/DA_AssetData` | 일반 게임과 테스트 맵에서 함께 쓰는 공통 카탈로그 |

기존 DA_AssetData의 입력·차량·맵 항목을 보존하고 Asset.Track.Straight/Curve/Ramp 정의를 추가했다. 기존 시작 맵·공유 경기 맵·차량 시험 맵은 변경하지 않았다. Content는 UVCS 관리 대상이다.

테스트 전용 카탈로그는 제거했다. 테스트 GameMode BP에 저장되어 있던 참조도 비웠으며, 네이티브 클래스의 TestCatalog 속성과 InitGameState의 별도 초기화를 제거했다. 모든 경로에서 DefaultGame.ini의 StartupAssetData로 초기화한 공통 카탈로그를 사용한다.

## 트랙 추가와 변형 구분

트랙마다 UMWTrackPieceDefinition DataAsset을 만들고 DA_AssetData의 Entries에 고유 AssetId로 등록한다. 같은 커브 유형이어도 길이·곡률·메시가 다른 트랙은 별도 정의와 별도 AssetId를 사용한다. 카탈로그는 동일한 AssetId를 두 항목에 등록하는 것을 거절한다.

다음은 추가할 때 사용할 수 있는 이름 예시이며, 실제 등록된 트랙 목록은 아니다.

| 개별 트랙 AssetId | 연결할 정의 예시 | 공통 분류 Labels 예시 |
|---|---|---|
| `Asset.Track.Straight.Short` | `DA_Track_Straight_Short` | `AssetLabel.Track.Straight` |
| `Asset.Track.Straight.Long` | `DA_Track_Straight_Long` | `AssetLabel.Track.Straight` |
| `Asset.Track.Curve90.Short` | `DA_Track_Curve90_Short` | `AssetLabel.Track.Curve` |
| `Asset.Track.Curve90.Wide` | `DA_Track_Curve90_Wide` | `AssetLabel.Track.Curve` |

- AssetId는 특정 트랙 정의 하나를 찾는 키다. 네트워크 후보의 DefinitionAssetId도 이 값을 사용한다.
- Labels는 여러 트랙을 같은 유형으로 묶는 분류다. 같은 Label을 여러 항목에 지정할 수 있다. 현재 선택 코드는 이 분류로 후보를 자동 추첨하지 않는다.
- PieceId는 정의 내부의 FName이다. 카탈로그 조회 키로 사용하지 않으며 새 정의마다 구분되는 이름을 지정한다.
- OfferId는 이번 선택판 안의 후보 번호다. 트랙의 영구 ID나 유형 ID가 아니다.
- PreviewShape의 Straight/Curve/Ramp는 테스트 메시를 그리는 방식이다. 지원할 전체 트랙 유형을 제한하는 enum이 아니다. 실제 메시를 표시할 때는 AuthoredMesh를 사용한다.

새 트랙을 추가할 때 고유 태그는 필요하지만 매번 C++의 MWGameplayTags.h/.cpp에 선언·정의를 추가할 필요는 없다. 이 프로젝트는 ImportTagsFromConfig=True이므로 Project Settings → Gameplay Tags에서 데이터 태그를 등록할 수 있다. C++에서 직접 이름을 참조해야 하는 공용 키만 네이티브 태그 상수로 둔다. 에셋이나 BP 이름을 바꾸더라도 기존 AssetId를 유지하면 호출부의 조회 키를 유지할 수 있다.

수동 추가 순서는 다음과 같다.

1. MWTrackPieceDefinition 기반 DataAsset을 생성하고 PieceId·DisplayName·Mesh·PreviewActorClass를 지정한다.
2. Gameplay Tags 설정에서 고유 AssetId를 등록한다. 유형별 조회가 필요하면 분류 Label도 등록한다.
3. DA_AssetData.Entries에 해당 AssetId와 정의 DataAsset을 연결하고 필요한 Labels를 지정한다.
4. 원하는 선택판의 GameMode 기본값에서 TrackSelectionDefinitionIds 또는 TestDefinitionIds에 그 태그를 지정한다.
5. 이미 카탈로그가 초기화된 실행은 종료한 뒤 다시 시작해 수정된 목록을 적용한다.

현재 한 번에 제시하는 후보 수는 세 개다. 트랙을 더 등록할 수 있지만 자동 후보 추첨은 별도 기능이며, 카탈로그 항목 추가만으로 선택판 후보가 바뀌지는 않는다. 제작용 파이썬 스크립트는 삭제했으며, 이후 추가 작업은 에디터에서 위 절차로 수행한다.

## 직접 사용하는 방법

1. 새 네이티브 클래스를 에디터에 반영하기 위해 현재 에디터를 재시작한다. 기존 에디터를 자동 종료하거나 사용자의 열린 작업을 저장하지 않았다.
2. L_TrackConstruction_Test를 연다.
3. 혼자 화면을 보려면 Standalone PIE를 사용한다. 여러 창을 연결하려면 PIE의 서버 추가 게임 옵션(Additional Server Game Options)에 `?NetDriverDef=ConstructionTestNetDriver`를 지정하고 Listen Server와 원하는 클라이언트 수로 시작한다. 전용 IP 드라이버를 사용하므로 EOS 로그인을 전제로 하지 않는다. 기존 GameNetDriver의 EOS 설정은 유지한다.
4. 후보 위에 마우스를 올리면 자기 강조와 플레이어 표시가 바뀐다. 왼쪽 클릭으로 선택을 확정한다.
5. 선택 후에는 자기 입력이 잠기고 다른 사람의 선택은 계속 표시된다. 후보의 P1/P2 배지는 참가자 번호이며 화면 패널에 전체 이름과 트랙 이름을 표시한다.
6. 개발 콘솔의 `BeginTrackSelection`으로 선택을 다시 시작하고 `EndTrackSelection`으로 접수를 닫을 수 있다. 명령은 로컬 서버의 비Shipping 실행에서만 처리한다.

이 절은 사용 절차이며 실행 결과가 아니다.

직접 서버를 여는 개발 콘솔 명령은 `open /Game/Developers/user/Construction/L_TrackConstruction_Test?listen?NetDriverDef=ConstructionTestNetDriver`다. `ConstructionTestNetDriver`는 추가된 전용 정의이며 다른 맵의 기본 연결 방식을 바꾸지 않는다.

테스트 GameMode는 EOS 세션을 생성하거나 실제 참가자 등록을 수행하지 않는다. 해당 맵의 참가자에게만 합성 등록 상태를 부여한다. 새 참가자가 들어오면 현재 접속 인원으로 테스트 선택을 다시 시작한다. 이는 테스트 맵 전용 동작이며 정식 경기의 참가자 고정·Ready 규칙을 변경하지 않는다. Shipping에서는 이 GameMode의 참가를 거부한다.

기존 EOS 방 경로에서는 GameMode.BeginTrackSelection이 승인된 정의로 공용 컴포넌트에 시작을 위임한다. 로비의 정식 Ready/경기 시작 UI는 이번 범위가 아니며 기존 세션 준비 확인은 유지했다.

## 상태·수명

- 전체 경기 Phase/RoundState는 선택 타입에 넣지 않는다.
- 선택 세션과 개인 상태의 세션/버전이 일치한 뒤 입력을 연다.
- 응답 RPC와 개인 상태 복제 순서를 가정하지 않는다. 승인 응답을 받으면 개인 OnRep 전에도 재선택을 잠근다.
- 같은 요청은 캐시 결과를 반환하고 같은 요청 ID의 내용 변경은 거절한다.
- 공용·개인 컴포넌트는 Tick 없이 유지한다. 커서 hit test는 선택 중인 로컬 Controller에서만 한다.
- 선택 종료 시 후보 표시·IMC·Pending을 정리하며 확정 결과는 보존한다. 로비·주행 전환을 자동 결정하지 않는다.
- 실제 코스 상태의 원본은 후속 TrackManager이며 건설 컴포넌트에 중복 저장하지 않는다.

## 플레이어 커서 공유

커서 공유는 feature/track-construction-test에 추가했다. 새 Actor·BP·아이콘 에셋은 만들지 않고 기존 PlayerController → 공용 컴포넌트 → 개인 컴포넌트 → 선택 Widget 경로를 사용한다.

1. Controller.FindTrackCursorWorldPosition이 마우스를 월드 광선으로 변환한다. 후보 밖에서도 표시되도록 Board의 로컬 Z=80cm 평면과 교차시킨다. 창이 비활성 상태이거나 커서가 뷰포트 밖이면 숨김 상태를 보낸다.
2. Controller.UpdateLocalTrackCursor가 선택 화면이 열린 동안 최대 20Hz로 서버에 위치·표시 여부를 보낸다. 확정/Pending 여부와 독립적이므로 트랙 확정 후에도 커서는 공유된다. 자기 커서는 기본 마우스 커서를 사용한다.
3. ServerUpdateTrackCursor는 Unreliable RPC다. 정지·숨김 상태도 주기적으로 보내 마지막 패킷 유실을 보완한다. 증가하는 Sequence로 이전 위치가 뒤늦게 도착해 현재 위치를 덮는 것을 막는다.
4. 공용 컴포넌트는 현재 선택 세션·후보 버전·등록·참가자 조건을 기존 검사로 확인한다. 후보 번호는 필요하지 않으므로 INDEX_NONE을 허용한다. 커서 위치를 선택 승인이나 설치 판정에 사용하지 않는다.
5. 개인 컴포넌트는 FMWPlayerConstructionCursor를 Selection과 별도로 복제한다. NetQuantize는 1cm 단위이며 좌표는 각 축 ±1,000,000cm 범위로 제한한다. 커서 전용 처리 예산은 최대 30개·초당 30개 회복이며 Hover 예산과 분리한다.
6. 서버는 표시 중인 커서의 마지막 유효 요청 이후 0.75초 동안 갱신이 없으면 숨긴다. 같은 위치의 주기 전송도 만료 타이머를 갱신하지만 값이 바뀌지 않으면 ForceNetUpdate를 반복하지 않는다. 선택 종료에는 커서만 숨기고 확정 결과는 유지한다.
7. Widget.UpdateRemoteCursors는 다른 플레이어의 현재 세션과 일치하는 커서만 읽는다. 월드 위치를 VInterpTo로 보간한 뒤 ProjectWorldLocationToWidgetPosition으로 DPI가 반영된 화면 좌표를 구한다. 색상과 '+ P번호 이름'으로 표시하며 입력을 가로채지 않는다. 퇴장한 참가자의 표시를 제거한다.

코드 위치는 MWTrackSelectionTypes.h(데이터), MWPlayerConstructionComponent(저장·복제·만료), MWTrackConstructionComponent(참가 조건 검사), MWPlayerControllerConstruction.cpp(좌표 계산·전송), MWTrackSelectionWidget(보간·투영·표시)다. 컴포넌트 Tick이나 선택 상태 변경 이벤트를 커서 이동에 사용하지 않는다.

현재 좌표는 선택 화면의 공통 평면을 기준으로 한다. 실제 경사 트랙 표면의 충돌점이나 OS 커서의 픽셀 좌표를 공유하는 기능과 구분한다. 다른 카메라에서도 같은 평면상의 지점으로 표시한다. 최초 구현 때는 별도 빌드·PIE·멀티플레이 실행 검증을 수행하지 않았다. 이후 사용자 요청으로 컴파일 오류를 확인해 Widget의 지역 변수 Cursor가 UWidget::Cursor를 가리는 C4458 오류를 RemoteCursor로 이름을 바꿔 수정했다. 2026-11-03 MWEditor / Win64 / Development 빌드는 성공했으며 PIE·멀티플레이 실행 검증은 수행하지 않았다.

## 제작과 검증의 구분

사용자의 지시에 따라 별도 검증 절차를 수행하지 않았다. 새 C++ 클래스를 부모로 BP·DataAsset을 생성하기 위한 에디터 모듈 컴파일과 에셋 생성 명령만 사용했다. 게임 실행·PIE·자동화 테스트·에셋 검증기·Game 타깃/Cook/패키징은 실행하지 않았다. 사용자가 재현하기 전까지 실제 화면·멀티플레이 동작을 검증 완료로 기록하지 않는다.

초기 에셋 생성에 사용했던 파이썬 스크립트는 사용자 요청으로 삭제했다. 2026-11-03 정리에서는 에셋 편집 명령으로 테스트 GameMode BP의 중복 카탈로그 참조를 비우고 해당 카탈로그를 삭제했다. 이 편집을 위해 별도 C++ 빌드·게임 실행·검증 절차를 수행하지 않았다. 생성 경로의 과거 기록은 Saved/TrackConstructionAuthoring에 남아 있으며 현재 에셋 목록의 정본으로 사용하지 않는다. Git commit/push와 UVCS checkin은 수행하지 않았다.
