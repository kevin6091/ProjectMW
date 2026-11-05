# P3 차량 선택 UI — 원본 화면 기반 재구성

2026-10-30 / UE 5.8.4 / `C:\workspace\UE\ProjectMW\MW.uproject` / Git `feature/title-world-ui`

## 1. 결과와 적용 위치

기존 P3의 흰 카드 행·작은 하단 마커·붉은 가로 이름판·검정 상태 바를 최종 화면 기준으로 사용하지 않고, 사용자 제공 실제 3인 원본의 **카드 격자 → 카드 위 대형 원형 포인터 → 하단 사각 차량 전시판 → 지지대·트랙·난간·장식 차량** 구조로 재구성했다.

실제 프로젝트의 기존 개발 레벨 `/Game/Levels/L_Title_WorldUI_Dev`, `/Game/Levels/L_VehicleSelection_UI_Dev`에 저장했다. 기존 카탈로그·동적 카드 생성·Preview 선택/잠금·Delegate·수명 관리·타이틀 연결을 재사용한다. 정식 `L_Title`, EOS·세션·GameMode/GameState/PlayerState/차량 이동 코드는 변경하지 않았다. Commit/Push/Checkin/Update/Switch/병합은 수행하지 않았다.

실행은 `L_Title_WorldUI_Dev`를 열어 PIE → 기존 EOS 로그인 → 왼쪽 아래 ‘차량 미리보기’로 진입한다. 기본 Preview는 **3명, 두 차량 표시 + 세 번째 미선택**이다. 개발 사례 버튼은 `WBP_VehicleSelection_Preview`의 Class Defaults에서 Show Preview Controls를 켜거나 기존 SetPreviewControlsVisible을 호출해 표시할 수 있다. 실제 승인 차량 정의는 0개이며 게임용 VehicleId는 `None`이다. 본문과 비교 HTML의 다인 화면은 실제 PIE에서 실행한 개발 Preview이고 실제 서버 다인 접속 완료가 아니다.

최종 실행 경로·저장본 검증 기록은 `title-flow-regression.json`, `final-editor-validation.json`을 참조한다. 실행하지 못한 항목은 아래 검증 범위에 명시한다.

## 2. 최우선 원본 자료와 구조 분석

입력 ZIP: `C:\Users\user\Downloads\MakeWay_VehicleSelection_Reference_20261030.zip`.

- `Original/MakeWay_1.4.0.6_SelectVehicle_A.jpg`
- `Original/MakeWay_1.4.0.6_SelectVehicle_B.jpg`
- 기존 P3 2인·4인 캡처 3장과 `REFERENCE_CHECKLIST_KO.md`.

A/B 모두 실제 3인이다. 원본 카드 격자는 화면 상부 약 3~61%, 하단 전시판은 약 62~85%에 있다. 큰 포인터는 카드 **위에 중첩**된다. 하단에는 각자 독립적인 사각 전시판과 네 볼트, 큰 차량 그림, 하단 이름, 지지대, 소형 조작 안내가 있다. A의 세 번째 차량 그림은 비어 있고 B에는 차량이 보인다. 이 사실만으로 Ready나 확정 상태를 판단하지 않는다.

기존 P3와 다른 점은 텍스트 크기보다 전체 구조에 있었다. 이번에는 원본 상하 영역 비중, 카드 위 포인터, 독립 전시판과 공간 배경을 우선 재현했다. 원본의 수십 대 대신 5대만 쓰므로 상부 격자의 가로 점유율은 줄이고 3열 2행(3+2)으로 재설계했다. 가짜 잠금 카드로 원본의 빈 공간을 채우지 않았다.

`COMPARISON.html`은 원본·이전 P3·이번 PIE를 같은 16:9 클라이언트 영역으로 나란히 표시한다. 창 테두리는 CSS로 제외하며 원본 PNG/JPG 링크도 제공한다. 합성 화면을 PIE라고 주장하지 않는다.

## 3. Unity Scene/Prefab/카메라 근거

실제 읽은 원본: `C:\Program Files (x86)\Steam\steamapps\common\Make Way\Make Way_Data\level0`.

SHA-256 `da399b05859cbaaa2f25970846f5f0dbc9566b1064166e71318884e185571c57`, 기존 조사와 동일함을 재확인했다. 원본은 읽기 전용이다. 전체 증거는 `unity-native-evidence.json`.

| 대상 | 실제 확인한 경로/컴포넌트 | 해석 범위 |
|---|---|---|
| 전체 화면 | `UI/Select Vehicle`, Canvas 6972, renderMode 2(World Space), RectTransform, GraphicRaycaster, SelectVehicleMenu 11063 | 일반 Screen Space 메뉴라고 보지 않음 |
| 차량 카드 | `sharedassets0.assets`, GameObject 1782 `Vehicle Slot Prefab`, SelectVehicleEvents, Frame/Outline/RawImage/TMP Name | 기존 카드 클래스로 시각 구조 재사용 가능 |
| 카드 색상 | achievement=(1,.698,.129), notAchievement=(0,.596,.973), notAchievementIcon/paidDLC/Locked/DLC 필드 | 주황/파랑은 단순 Hover/Selected 색상이 아님. 직렬화 필드 확인과 실제 실행 조건 검증은 구분 |
| 원형 포인터 | `UI/Select Vehicle/Player Pointers/Player One`, SelectVehiclePointer 10990, Arrow Rounded + Vehicle Badge Background 외곽100/내부80 | 독립 플레이어 포인터. 네트워크 알고리즘을 복제하지 않음 |
| 하단 전시판 | `UI/Select Vehicle/Player Slots/Player 1/Sign`, Sprite `t_playerSlotSign` 1245, 512², Border 75, Sliced Image | 원본 전시판 Sprite를 그대로 사용 |
| 볼트·차량·이름 | Sign/Bolt 01~04 별도 Image, Vehicle RawImage, Name TMP | 기존 붉은 가로 이름판 위에 정보를 쌓는 구조가 아님 |
| 하단 지지대 | Player 1/Pole, Image의 Make Way Logo v1 Pole Sprite | 원본 하단 지지대가 모두 MeshRenderer라고 단정하지 않음 |
| 원본 카메라 | `Camera & Lighting/Dolly - Lobby To Select Vehicle`의 Virtual Camera, SmoothPath, DollyCart, LookAt, FOV45 | 경로/렌즈 직렬화 확인. 이번에 Level Sequence나 이동 연출을 추가한 것은 아님 |

원본 Lobby→Select Vehicle Camera Path의 마지막 waypoint는 (-82.55,106.4,118), LookAt 마지막 waypoint는 (-160.9,112,90)이다. Unity 좌표·단위와 현재 독립 개발 장면의 크기가 달라 숫자를 그대로 UE 카메라에 복사하지 않았다.

추출된 C#에는 메서드 본문이 비어 있는 자료도 있다. 이를 실행 로직 증거로 사용하지 않았다. 화살표/다운로드·상점·실루엣 표시는 관련 필드와 Sprite 관계까지 확인했지만 구매·해금 판정 전체를 검증한 것은 아니다. 현재 없는 잠금/구매 기능은 UI에도 추가하지 않았다.

## 4. Designer와 포인터·전시판 구현

### 상부 선택판

- 기존 데이터 기반 `Grid_Vehicles`, `ColumnCount` BP 설정을 사용한다. C++에 5개를 고정하지 않았다.
- 디자인 캔버스 1920×1080, 그리드 (480,35), 960×590, 3열.
- 카드 디자인 200×200, 원본 Frame/Border/볼트 유지, 큰 이미지와 하단 이름 영역을 분리했다.
- 5개 Preview는 기존 원본 이미지 Smiet/The Raven/Ufology/Doug/Fred다. 실제 승인 카탈로그를 변경하지 않았다.
- 원본의 notAchievement 파란 계열을 사용하며 UMG 선형 색 공간의 표시 차이를 보정했다. 이 색이 ProjectMW에서 DLC 소유 여부를 판정한다는 뜻은 아니다.

### 큰 원형 포인터

- 기존 `UMWVehiclePlayerMarkerWidget`과 BP를 재사용했다. 새 Actor/Manager/Subsystem이 없다.
- 원본 Arrow Rounded, 원형 배지 외곽/내부를 재사용하고 카드 위에 ScaleBox로 배치했다.
- 단독 포인터 표시 공간은 200×200이며 같은 카드에 여럿이면 기존 UniformGrid의 2×2 배치로 식별한다.
- 색상 외에 P번호, 선택/미리보기/확정 문구로 구분한다. ‘나’ 표시는 기존 로컬 데이터에서 나온다. 같은 카드 4명에서도 상태를 읽기 쉽도록 상태 글꼴 16과 독립 높이 32를 확보하고 Hover 문구는 ‘보기’로 줄였다.
- Hover는 로컬 포인터와 전시판의 임시 표현만 바꾼다. 클릭 전에는 기존 선택 데이터가 바뀌지 않고 Unhover 시 복귀한다.
- 차량 미선택 상태의 포인터 위치를 표현하기 위해 **개발용** `PreviewPointerCardIndex`를 추가했다. `PreviewCardIndex`/게임 VehicleId와 분리돼 있다.
- 미래 원격 플레이어는 선택 차량·확정 상태 프레젠테이션을 공급하는 구조를 유지한다. 마우스 좌표 RPC/매 프레임 복제를 만들지 않았다.

### 하부 전시판

- 기존 PlayerEntry의 `SetPresentation`을 그대로 사용한다.
- 260×260 사각 원본 Sign, 네 개의 원본 Bolt, 큰 차량 이미지, 하단 참가자 이름으로 구성한다. 전체 엔트리 높이는 지지대와 안내를 포함해 390이다.
- 이전 붉은 가로판과 검정 차량명/상태 바는 clean 화면에서 사용하지 않는다. 기존 위젯은 삭제 대신 숨겨 관련 binding과 비교 가능성을 유지했다.
- 빈 참가자/미선택의 차량 그림은 숨기고 이름/전시판은 유지한다. Hover 이미지와 실제 선택을 구분한다.
- 확정된 Preview의 배경 tint는 현재 ProjectMW 표현 규칙이다. 원본 A/B의 색 변화가 Ready를 뜻한다고 역으로 단정하지 않는다.
- 로컬 전시판은 테두리로 강조한다. 참가 인원/정원 기반 기존 HBox를 사용해 2인은 중앙, 3인은 원본 비교, 4인은 균형 있게 표시한다.
- 지지대는 원본 Pole Sprite를 사용한다. CTRL/ALT의 조향 보조·색상 변경 기능은 미연결이므로 해당 버튼이 작동하는 것처럼 꾸미지 않았다. 원본 스타일 소형 안내판에 현재 가능한 마우스 선택 안내를 표시했다.

### Clean/개발 모드

기존 `SetPreviewControlsVisible`로 개발 사례·로컬 변경·등록 수·상세 상태 그룹을 표시/숨긴다. Clean에는 실제 동작하는 Back/Reset/Confirm만 남긴다. 테스트 환경·보고서·비교 HTML은 모두 개발 Preview라고 표시한다. 조작 패널을 숨긴 것을 서버 연결 완료로 표현하지 않는다. 실제 오류 Text_Error는 개발 패널 밖 왼쪽 빈 여백에 분리했으며, 메시지가 있을 때만 표시한다. 기존 FinishSelectionRequest로 UI 전용 오류를 주입/해제해 clean에서도 오류가 숨겨지지 않는 것을 확인했다(`clean-error-regression.json`). 이 검증은 실제 서버 거절 응답을 받았다는 뜻이 아니다.

## 5. 실제 3D 배경과 카메라

읽기 전용 기준: `C:\Users\user\Documents\Codex\UCM_MasterLibrary\MakeWay_AssetReview.uproject`, UE 5.8. 기존 원본 UCM 파일은 수정하지 않았다.

트랙 표면·하부·좌우/후방 난간과 장식용 Smiet visual의 필요한 패키지 **38개**를 참조 경로 보존 방식으로 복사했다. 기존 대상 파일은 덮어쓰지 않았다. 실제 AssetRegistry 검사에서 추가 Roughness 텍스처 1개를 발견해 보완한 뒤 38개 모두 로드·참조 검증했다. `ucm-resource-plan.json`, `ucm-dependency-validation.json`, `preservation-audit.json` 참조.

배경 Smiet는 복구된 정적 Visual BP이며 게임 Pawn/승인 차량으로 등록하지 않았다. 원본 캡처의 녹색 미니차와 같은 차량이라고 주장하지 않는다. 개발 전용 Material Instance의 색상과 레벨 컴포넌트 override만 조정했다. 원본 차량 재질·메시는 보존했다.

기존 기본 격자 Floor/구름은 독립 개발 레벨에서 삭제하지 않고 숨겼다. 원본 트랙 메시·난간, 실제 3D 상단 선택판 지지대/받침, 장식 차량, 청록색 배경을 배치했다. 새 지지대는 기존 엔진 기본 메시와 개발 재질을 사용하며 원본 메시 추출본이라고 주장하지 않는다. 원본 화면의 검정 바닥/노란 격자 데칼 전체는 재현하지 못했고, 현재는 복구된 목재 트랙 재질이다.

최종 설정:

| 설정 | 값 |
|---|---|
| Widget DrawSize/Scale | 1920×1080 / 1 |
| 독립 UI 위치/회전 | (0,0,650) / Pitch6,Yaw180,Roll0 |
| 독립 카메라 | (-2335,0,895.42), Pitch-6,Yaw0,Roll0 |
| 타이틀 Preview UI/카메라 | 위 값의 Y에 50000 적용 |
| FOV/화면비 | 45 / 16:9 |

수치는 레벨 CameraActor·Component/BP 설정이며 C++ 리터럴이 아니다. 차량 전시 공간은 메인 타이틀 시야를 침범하지 않도록 같은 개발 레벨의 Y=50000에 분리했다. 메인 `CAM_TitleWorldUI_Dev`와 정식 레벨을 변경하지 않았다. 타이틀 통합 경로는 인증된 PIE에서 이 배치를 먼저 검증하고 같은 Transform을 Editor 레벨에 저장한 뒤 별도 재로드 smoke test로 확인했다. 자세한 Actor 이름·Transform은 `world-final.json`에 있다. 카메라 이동/Sequence는 추가하지 않았다.

## 6. 2인·3인·4인 및 실제 입력 검증

기존 7개 2인·4인 시나리오는 보존하고 3인 3개를 추가했다.

| 사례 | 검증한 표시 |
|---|---|
| 2명/정원2 | 중앙 전시판2, 서로 다른 차량/포인터2 |
| 2명/정원4 | 참가2+대기2 전시판, 슬롯 위치 유지 |
| 3명 A | 차량2+빈 전시판1, 포인터3 |
| 3명 B | 전시판3 모두 차량 표시, 원본 B 구조 비교 |
| 3명 선택 중 | 전원 선택 중의 별도 사례 |
| 4명 선택 중/혼합/3명 확정/전원 확정 | 4전시판·로컬 강조·각자 포인터·상태 |
| 동일 차량4명 | 한 카드에 2×2 포인터와 각각의 전시판, 중복 허용 서버 정책은 미정 |

1920×1080, 1280×720, 960×540에서 clean 30장, 개발 패널 표시 6장을 실제 PIE로 촬영했다. `scenario-captures.json`에는 실제 Viewport, 참가자·차량·포인터·로컬·확정 상태가 함께 있다. 자동 fixture 촬영 중 외부 마우스 Hover가 개입하지 않도록 카드 루트의 hit testing만 일시적으로 끄고, 실제 전시판의 렌더링 차량명과 참가자 모델의 차량명이 일치하는지도 검사했다. 이 설정은 QA 실행 중에만 적용하고 복구했으며 BP에는 저장하지 않았다. 실제 마우스 검증은 hit testing이 활성화된 상태에서 별도로 수행했다. 최종 새 에디터의 저장본 재검증은 `saved-reload-validation.json`에 별도로 기록한다.

실제 마우스 입력 회귀(`native-input-regression.json`):

1. 세 번째 빈 전시판에서 Fred Hover → 포인터/하단 임시 이미지 변경, 선택 모델은 비어 있음.
2. Fred 실제 클릭 → 로컬 참가자의 선택·전시판만 Fred로 갱신, 나머지 참가자 상태 보존.
3. 기존 Confirm 클릭 → 로컬 확정/잠금과 포인터 상태 갱신.
4. 다른 실제 카드 클릭 → 확정 Fred 유지.
5. 기존 Reset 클릭 → 로컬 잠금만 해제, 기존 선택은 유지.

`OnChosen`, `OnHoverChanged`, `SetPresentation`, `OnBackRequested`와 기존 Begin/EndPlay 수명 경로를 유지했다. 별도의 가짜 Server RPC나 게임 차량 정의는 만들지 않았다.

## 7. 변경 파일·설정 소유권·보존

기준 185개 파일 중 수정 **15개**, 보존 **170개**. 신규 에셋은 UCM 복사 38개와 개발 재질 3개로 **41개**다. 보고서는 별도로 추가한다. 정확한 전체 상대 경로·Before/After SHA는 `preservation-audit.json`과 인계 manifest를 참조한다.

기존 C++ 변경 7개:

- `Source/MW/UI/MWVehicleUITypes.h`: Hover 표현/미선택 개발 포인터 대상 필드.
- `MWVehicleSelectionWidget.h/.cpp`: 기존 참가자 렌더링에서 포인터 갱신을 helper로 분리, Hover 시 임시 표현만 갱신.
- `MWVehiclePlayerMarkerWidget.h/.cpp`: 원본 화살표 Optional Image, Hover 문구 설정.
- `MWVehiclePlayerEntryWidget.h/.cpp`: 전시판 Optional Image와 BP 소유 neutral tint.

기존 에셋 6개 (`Content/UI/Lobby/VehicleSelection`):

- `WBP_VehiclePlayerMarker_Dev`, `WBP_VehicleCard_Dev`, `WBP_VehiclePlayerEntry_Dev`.
- `WBP_VehicleSelection_Dev`, `WBP_VehicleSelection_Preview`, `BP_VehicleSelectionPreview_Dev`.

기존 개발 레벨 2개: `L_Title_WorldUI_Dev`, `L_VehicleSelection_UI_Dev`.

신규 개발 재질 (`Content/UI/Lobby/VehicleSelection/Art`): `M_VehicleSelectSky_Dev`, `M_VehicleSelectSupport_Dev`, `MI_VehicleSelectDecorSmiet_Dev`.

신규 C++ 클래스·Manager·Subsystem·RPC·Tick 없음. 카메라·레이아웃·팔레트·문구·열 수·Preview 시나리오는 Designer/BP/레벨에서 관리한다. 새 C++ 행의 경로 고정·강제 로드·좌표·차량 수·복제/RPC·Tick 검사 모두 0이다. 차량 5개 숫자가 QA fixture 검증에 있는 것과 런타임 카탈로그를 5개로 고정하는 것은 구분한다.

기존 EOS/프런트엔드 소스와 개발 Login/Title/Create/Browser/RoomEntry 에셋, 서버/차량 이동/AssetManager/DataAsset/설정은 기준 해시를 보존했다. 원본 UCM 38개는 복사본과 SHA가 같으며 원본을 수정하지 않았다. 기존 미커밋 결과와 백업/ZIP도 유지했다.

## 8. 빌드·오류·미검증 범위

- Editor 최종 빌드 성공: `EditorBuild-final.log`, 31.21초.
- Game 빌드 성공: `GameBuild.log`, 53.14초.
- XGE 라이선스 경고가 있었으며 빌드 실패는 아니다.
- 기존 개발 BP 6개 컴파일·저장 성공, 저장되지 않은 패키지 없음 확인.
- 기존 Renderer `FAppTime::Get / IsInGameThread / AppTime.cpp:38` handled ensure는 재현돼 별도 기록했다. 레이아웃 작업으로 해결했다고 표시하지 않는다.
- Python Authoring/QA의 타입/API·검증 컨텍스트 차이로 실패한 호출은 수정했고 게임 Blueprint 오류와 구분했다. 최초 의존성 검증을 PIE 중 EditorAssetLibrary로 수행한 것은 잘못된 검증 컨텍스트였으며, PIE 종료 후 실제 누락 텍스처를 찾아 보완했다.
- 장시간 Pressed 순간만의 선명한 별도 캡처, Cook/패키징, 실제 두 번째 계정 참가, Ready/Replication/서버 차량 선택/Pawn 생성·Possess/트랙 단계는 미검증이다.
- 이전 EOS 로그인·검색·방 생성·종료 성공 자료는 직전 P3 보고서에 있다. 이번에 관련 소스를 변경하지 않았으며, 이번 실제 타이틀 실행 결과와 이전 EOS 성공을 구분한다.

## 9. 남은 차이와 후속 작업

원본 전체 화면 1:1 복제 완료로 보고하지 않는다. 5대용 3+2 격자, 4인 확장, P번호/상태 문자, Back/Reset/Confirm은 ProjectMW에 맞춘 차이다. 원본 검정/노란 격자 바닥, 녹색 미니차, 원본 카메라 이동과 조작 보조/차량 색 변경까지 복구한 것은 아니다. 하단 Pole Sprite는 실제 Mesh로 위장하지 않는다.

서버 담당자 계약 후 기존 프레젠테이션에 승인 VehicleId·확정 상태를 연결해야 한다. 선택/취소·중복 선택 정책·퇴장/늦은 참가·전원 확정·Pawn 소유권은 기존 서버 API 설계 문서를 따른다. 개발 Preview 배열이나 포인터 탐색 인덱스를 PlayerState 정본에 복사하지 않는다.

정식 타이틀 승격이나 소스관리 제출 전에는 동봉 Before 해시·기존 Pending·잠금을 비교하고 이번 diff/에셋 변경만 검토한다. 원본 자료·리소스와 인계 ZIP은 내부 검토용이며 공개 저장소 제출을 수행하지 않았다.
