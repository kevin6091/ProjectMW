# P3 차량 선택 UI 최종 정렬 감사

2026-11-02 KST · C:/workspace/UE/ProjectMW · feature/title-world-ui · UE 5.8.4

**명패의 상대 정렬·모서리 여백 수정과 실제 입력 회귀 검증 완료. 커서의 기둥 가림은 재현·분석했으며 미해결이다. 전체 World UI의 결함이 모두 해결됐다고 판정하지 않는다.**

## 최종 판정과 변경 파일

실제 수정 에셋은 `C:/workspace/UE/ProjectMW/Content/UI/Lobby/VehicleSelection/WBP_VehiclePlayerEntry_Dev.uasset` 1개다. 새 보고서는 `Docs/Engineering/Client/P3_VEHICLE_UI_FINAL_ALIGNMENT_AUDIT.md`에 저장한다. 변경 전 파일은 인계 ZIP의 Rollback에 보관한다.

추가 지시 전에 점만 아래로 내려 맞춘 중간안은 채택하지 않았다. 비교 자료의 `before-*`는 최초 저장본, `after-*`는 폐기한 수직 보정 중간안, **`final-*`는 모서리·볼트 여백까지 반영한 최종 저장본**이다. 최종 비교는 `NAME_ICON_BEFORE_FINAL_1920.png`와 `NAME_ICON_BEFORE_FINAL_960.png`를 사용한다.

적용 분야는 Client + TA다. mw-task-routing, AGENTS, Docs/Shared 직속 전체, Engineering 직속·Client, Art, Shared/TA, Shared/Client,Server를 적용했다. 기존 전체 읽기 문서는 SHA로 재사용을 확인했고 변경된 직전 커서 보고서를 본문 전체 다시 읽었다. computer-use는 실제 Windows 마우스 입력 검증에만 사용했다.

## Designer 수정

|요소|작업 전|최종 저장본|
|---|---|---|
|Image_IdentityBadge Canvas 위치|(12,2)|(18,4)|
|색상 점 크기|24×24|20×20|
|AlignmentPlayerNameCenter 위치|(40,-6)|(46,-10)|
|이름 영역 크기|148×48|136×48|
|이름 영역 수평 중심|114 — 부모 NameLayer 기준|114 — 동일|
|이름 폰트|Noto Bold 22|동일|
|이름 맞춤|TextBlock 직접 배치|기존 Overlay 안 ScaleBox, ScaleToFit / DownOnly, 중앙 정렬|

수치는 WBP Designer Slot과 ScaleBox에 저장한다. 런타임 C++ 보정값은 추가하지 않았다. 최종 점의 중심 높이는 최초 저장본과 같고, 이름 영역을 위로 조정해 상대 중심을 맞췄다. 수직만 맞춘 중간안보다 점과 이름 모두 위로 이동한다. 점은 안쪽으로 이동하고 지름을 줄여 볼트·둥근 모서리와 분리했다. 점을 단순히 아래로 내려 해결한 결과가 아니다.

이름의 전체 명패 기준 수평 중심은 130으로 유지된다. 좁아진 이름 영역도 `플레이어 1~4`의 자연 DesiredSize를 수용하므로 이 이름들의 실제 글자 크기는 유지한다. 원본 A에서 확인한 `COOL KYLE`은 자연 DesiredSize159가 기존 영역148을 넘었다. 긴 이름만 기존 영역 안으로 줄여 잘림을 방지하며 이름을 오른쪽으로 밀지 않는다. 새 Subsystem/클래스/상태 저장은 없다.

Sign·Bolt·차량 이미지·색상 점의 기존 Sprite, 폰트 파일, 상태판·지지대·상단 카드·버튼은 변경하지 않았다. entry-inset-authoring.json과 원본 대비 전체 파일 해시를 확인한다.

## 실제 PIE 여백과 중심 — 세 항목 분리

아래는 **3인 전원 표시 / 플레이어 3**의 native client pixel이다. 크롭 확대 좌표나 Designer 좌표가 아니다. 점은 실제 유색 픽셀, 이름은 실제 검정 glyph ink를 분리했다. 왼쪽/하단은 점 중심의 행·열에서 보이는 Sign 테두리 안쪽까지의 여백이다. 볼트는 변경 없는 Sprite의 실제 카메라 투영 bounding box와 점 ink 사이의 보수적 수평/수직 간격이다.

|화면 폭|단계|점↔왼쪽 테두리|점↔하단 테두리|점↔볼트 수평 / 수직|점↔이름 수평|점 중심−글자 중심 Y|
|---|---|---:|---:|---:|---:|---:|
|1920|작업 전|19|16|8.2 / 5.2|14|-3.5|
|1920|수직만 보정한 중간안 — 미채택|19|12|8.2 / 1.2|14|+0.5|
|1920|최종 내부 여백 보정|25|18|14.2 / 7.2|12|+0.5|
|960|작업 전|8|8|4.1 / 2.6|7|-2.0|
|960|수직만 보정한 중간안 — 미채택|7|6|4.1 / 0.6|7|+0.0|
|960|최종 내부 여백 보정|11|9|7.1 / 3.6|6|+0.0|

원형 아래쪽의 어두운 초록 픽셀도 포함하도록 badge 분리 기준을 chroma>20, maxRGB>25로 정했다. 이전 밝기>70 기준은 960의 작은 초록 점 아래쪽을 누락하므로 이 감사의 전/중/후 모두 같은 수정된 기준으로 재측정했다. 측정 방법과 ROI는 final-clearance.json, final-pixels.json, 분석 스크립트에 공개한다. 반투명/안티앨리어싱 경계의 포함 여부에 따라 약1px의 차이는 가능하며, 이 표만으로 통과시키지 않고 확대 이미지를 직접 검토했다.

최종 2·3·4인 전체에서 점/글자 중심 오차는 1920에서0.5~1px, 960에서0~0.5px다. 플레이어 3은 각각0.5px/0px. 이름 glyph의 수평 중심 오차는 3인 플레이어3에서 −0.42px/+0.29px이며 변경 전과 같다. 점과 이름은 1920 약11~12px, 960 약6~7px 떨어져 있다. 기존 가로 간격을 무작정 늘리거나 이름을 밀지 않았다.

`플레이어 1~4`, 원본의 영문 `AERYO`, `COOL KYLE`, 한글 `동인천쌀수`, 혼합 대소문자 `Alex`를 기존 Entry.SetPresentation으로 일시 표시했다. 실제 PlayerState/게임 DataAsset에는 등록하지 않았다. 영문·한글 probe는 final-names-* PNG/JSON과 final-names-pixels.json에 있다. 최종 probe의 중심 차이는 절댓값 최대1px이며 ROI 잘림은 없다. 아주 긴 이름·이모지 전체 집합의 보장은 이번 테스트 범위가 아니다.

## 원본 명패와 추가 요소

원본 A/B에서 확인한 것은 사각 Sign, 모서리 볼트, 큰 차량 외형, 하단 참가자 이름 및 작은 CTRL/ALT 조작 안내다. 원본에는 현재 ProjectMW의 이름 왼쪽 큰 식별색 점이 보이지 않는다. 이 점은 ProjectMW 참가자 구분을 위한 추가 요소다. 요청에 따라 삭제하지 않고 내부 여백과 상대 정렬을 보정했다. 색상 점·선택 상태판·대체 Noto 폰트까지 원본과 동일하다고 주장하지 않는다.

원본 A/B와 최종 3인 미선택/전원 표시 화면을 COMPARISON.html에 나란히 제공한다. 배경·상단5대 단일행은 이번 요청에서 유지한 조건이며 원본 전체 구성의 재현으로 확대 판정하지 않는다.

## 커서 원·삼각형 감사

`WBP_VehiclePlayerMarker_Dev`는 이번 작업에서 byte-identical이다. 원본의 원 밖으로 돌출된 대각 화살표는 의도된 형태로 확인되며 원 안으로 밀어 넣지 않았다. 기존 원본512² Texture2D, 중앙 Pivot(.5,.5), 원형 Outline/Inner 비율, Arrow의 −45° 회전과 논리 크기를 유지했다. native Unity RectTransform은 Outline100²/Inner80²/Arrow110², 공통 부모, Pivot(.5,.5), Arrow +45°다. Slate 회전 방향과 Y축 부호를 반영한 기존 BP 설정을 확인했다. 원본 경로/해시는 ReferenceProof에 포함한다.

POINTER_ORIGINAL_CURRENT.png는 원본1920과 최종1920의 동일220px native crop이다. 최종960 crop만2배 nearest 확대해 크기 비교를 명시했다. 원형과 삼각형이 결합된 상태로 무클릭 이동하는 실제 PNG 및 final-inset-cursor.mp4도 제공한다. 원은 약132px/66px이며 눈에 띄는 타원 찌그러짐은 보이지 않았다. 원본과 경계 픽셀 완전 일치나 시각 문제의 전부 해결을 주장하지 않는다.

남은 색 차이는 원본의 밝은 식별색과 현재 진한 ProjectMW tint/World UI 렌더링 차이다. 로컬 RenderOpacity1, remote Preview Grid 부모0.65를 구분했다. remote를 local 검증으로 혼용하지 않았다. 공유 차량 fixture에서는 원격 마커가 기존 Grid로 분리·축소된다. 같은 위치의 실제4인 네트워크 포인터 중첩과 이동 동기화는 미구현·미검증이다. 실제 포인터를 여러 개 만들어 해결하지 않았다.

Windows 기본 화살표는 기존 hardware input 상태에서 함께 보인다. 별도 원형 로컬 커서는 child1이며 중복 원형 생성은 발견되지 않았다. OS 화살표까지 사라진 단일 커서라고 보고하지 않는다.

## 기둥 가림 — 재현됨, 미해결

occlusion-partial-960.png에서 점 중심은 UI를 향하면서 원의 일부/삼각형이 기둥에 가려진다. 실제 마우스와 원 중심의 투영 오차는 약0.096px다. occlusion-upper-960.png에서는 실제 마우스 위치의 Visibility trace가 REBUILD_BoardSupport_L(StaticMeshActor_7)에 먼저 맞고 커서가 Collapsed된다. 따라서 렌더 순서와 입력 hit-test 문제가 함께 있다.

PreviewSurface는 기존 World WidgetComponent, 기본 Widget3DPassThrough_Masked_OneSided → Opaque → Widget3DPassThrough 체인이고 DisableDepthTest=false다. UI actor 원점 X0/Z650, 기둥 중심 X40/Y±900, 카메라는 X−2335의 기존 구도다. 기울어진 UI 평면과 기둥의 깊이 순서가 높이에 따라 바뀐다. 부모 컴포넌트와 배경의 Transform은 수정하지 않았다.

검토한 최소 분리안은 기존 입력·선택 정본과 pinned 좌표를 그대로 두고 **로컬 커서의 표시만** 별도 비충돌 presentation으로 옮기는 것이다. 별도 로컬 overlay 또는 전용 WidgetComponent/material을 비교할 수 있다. 전용 material만 DepthTest를 끄는 방식도 기둥의 입력 차단을 해결하지 못하므로, 기존 viewport 좌표→UI geometry 변환과 창 이탈·Back 수명을 함께 검토해야 한다. 전체 World UI를 화면 UI로 바꾸거나, 전체 material depth test/기둥 충돌을 바꾸는 방법은 적용하지 않았다. 이번에는 구조 검토와 재현 증거만 제공한다. 이 결함을 해결 완료로 보고하지 않는다.

## 나머지 정렬 검사

|항목|실제 결과|변경|
|---|---|---|
|상단5대 이름 기준선|같은 해상도의 glyph 하단 spread0px. 최종1920 높이22px/960 높이11px|없음|
|카드 이미지·이름·포인터|5대 한 줄/동일 프레임. 기존 포인터 중첩 유지|없음|
|하단 차량 이미지와 이름|새 이름 영역에서도 겹침 없음. 플레이어1~4 glyph1920 높이26/960 높이12|명패 이름 영역만 최소 조정|
|색상 점·이름|위 실측표/확대 캡처로 중심·좌우·하단·볼트 여백 확인|Entry Designer|
|검정 상태판|선택 중/확정 glyph 중심이 board 중심에서 1920 약0.9px, 960 약0.2~0.7px. 잘림·충돌 없음|없음|
|상태판·지지대|기존 간격/중앙 지지대 유지, 화면상 충돌 없음|없음|
|뒤로/다시 선택/선택|실제 활성·비활성/클릭 화면에서 라벨이 프레임 안 중앙에 표시. 명패와 겹치지 않음|없음|
|Preview HUD / Clean|개발 HUD를 켠 별도 final-preview-hud-960와 숨긴 fixture 화면을 구분|테스트 중 일시 전환만|

문제가 확인되지 않은 카드·버튼·상태판은 수정하지 않았다. 카드의5대와 Smiet/Mammoth/Ufology/Doug/Fred 데이터는 기존 동적 PreviewCards 그대로다.

## 실제 입력 회귀 — 최종 내부 여백 저장본

재시작한 에디터 PID32736에서 저장본을 로드했다. Delegate 직접 Broadcast 없이 Windows Computer Use의 실제 mouse click/right click 및 무클릭 zero-wheel 이동을 사용했다.

- final-input-hover-960: 선택값 비어 있음 유지, 하단 Hover thumbnail/name만 Mammoth, cursor 중심 오차 약0.025px.
- final-input-selected-960: Mammoth 클릭, 클릭 지점 UI[737.1796,450.8719]에 마커 고정.
- final-input-confirmed-960 / final-input-locked-960: 확정 후 Fred 실제 클릭이 차단되고 Mammoth/locked=true와 동일 marker 위치 유지.
- final-input-right-cancel-960: 실제 우클릭으로 선택·확정 해제, 무클릭 추적 복귀, 중심 오차 약0.016px.
- final-input-fred-960: Fred 실제 선택. final-input-reset-960: 실제 다시 선택으로 비움.
- final-input-fred-before-back-960 / final-input-back.json: Fred 고정 상태에서 실제 Back, surface.widget=None/visible=false, 보이는 이전 로컬 마커0.
- 기존 개발 host.OpenPreview로 재진입해 새 Root/CursorLayer child1 확인. 이를 EOS 로그인이나 타이틀 메뉴의 실제 마우스 진입으로 주장하지 않는다.
- final-pointer-local/moved-1920: 최종 명패가 있는 화면에서 실제 무클릭 빈 공간 이동, circle과 arrow 결합 유지. 중심 오차 약0.046/0.145px.
- final-free-leave-1920: 실제 제목 표시줄 이동 후 mouse_client=None, 자유 cursor Collapsed. 고정 마커의 선택 표시는 이탈 때 남는 기존 정책이다.

final-input-motion.json의 관찰 샘플18202개와 각 상태 PNG/JSON을 포함한다. pinned 상태에서 mouse와 marker 거리가 커지는 것은 의도된 동작이므로 free tracking 오류로 계산하지 않는다. 이동 도구는 좌표 점프 방식이며 영상이 사람 손의 고주사율 연속 궤적과 동일하다고 주장하지 않는다.

중간안 검증 중 사용자 입력 감지로 한 번 재관찰했고, 최초 Reset 시도에서 상태 변화가 없는 관찰을 보존했다. 재관찰 후 재시도 및 최종 저장본의 Reset은 통과했다. 앞선 실패를 최종 통과 증거로 쓰지 않았다. 중지 신호를 무시하거나 보안/인증창을 조작하지 않았다.

최종 저장본의 실제1920·960에서 각7개 fixture(2/2,2/4,3인 미선택/전원 표시,4인 다른 차량/혼합 확정/동일 차량)를 총14장 캡처했다. 최종1280의3인 두 상태/4인 혼합도3장 확인했다. 실제 서버의 참가자 상태가 아닌 개발 Preview다.

## 빌드·컴파일·에디터 안정성

변경 Entry WBP는 PIE가 없는 /Engine/Maps/Entry에서 실제 compile_blueprint 성공 및 save_loaded_asset 성공을 확인했다. 저장 후 에디터를 다시 시작해 최종 Slot·ScaleBox를 읽고 PIE를 실행했다. Actor BP나 인스턴스가 있는 개발 맵에서 재컴파일하지 않았다. 나머지 WBP는 기존 저장본을 로드해 사용하는 범위이며 전부 재컴파일했다고 주장하지 않는다.

FinalEditorBuild.log: MWEditor Win64 Development Succeeded,1.83s. FinalGameBuild.log: MW Win64 Development Succeeded,1.56s. C++ 변경이 없어 Target up to date 결과다. Cook/패키징/Shipping 빌드는 미실행이다.

최종 실행에서 AppTime.cpp:38 IsInGameThread handled ensure1건이 로그에2회 기록됐다. Renderer.UpdateReflectionSceneData/RenderCommandPipe 경로이며 원인은 미확정이다. TickTaskManager.cpp:1992 assertion, EngineBaseTypes.h:524 Pure virtual fatal, Blueprint Runtime Error는 이번 최종 실행에서 재현되지 않았다. 따라서 과거 크래시의 근본 원인 해결이나 에디터 무오류 판정은 하지 않는다. 최종 로그와 ensure excerpt를 포함한다.

PIE 종료 후 맵 로딩·BP 컴파일을 하지 않고90초 이상 관찰했다. game_world=None, callback 없음, dirty content/map0을 확인했다. 최종값은 final-post-stop.json에 있다. 다른 MakeWay_AssetReview 에디터는 조작·종료하지 않았다.

## 코드·파일 보존 및 남은 범위

재사용: Entry.SetPresentation, Marker.SetPresentation, SelectionWidget.RefreshPlayers/HandleCardHover/HandleCardChosen/HandleConfirm/HandleReset/CancelLocalSelection, PreviewActor.OpenPreview/ClosePreview. 기존 AssetManager/VehicleDefinitionData/PreviewCards 데이터 경로를 보존했다.

새 C++ 클래스·함수0, 수정 C++0, EOS/서버/PlayerState/GameState/GameMode/PlayerController 변경0. CPP_DIFF.txt와 final-preservation-audit.json을 제공한다. 기존 미커밋 Source·Config·다른 Content는 SHA 동일이며 maps/materials/textures 변경0. 원본 L_Title 및 두 개발 레벨도 동일하다. 실제 게임 차량 정의·Pawn을 새로 등록하지 않았다.

UVCS 로컬cs16과 기존 Pending을 보존했다. 조회 중 원격 head가21→23으로 변한 것은 외부 상태이며 이 작업에서 Update/Checkin/Switch하지 않았다. DA_AssetData의 다른 작업 잠금은 건드리지 않았다. Git Commit/Push/stage/merge/reset, UVCS mutation 미실행.

남은 항목: 기둥의 렌더·hit-test 가림, 원본 대비 tint/불투명도·대체 폰트 차이, 실제 네트워크 동기화/4인 동시 입력/서버 차량 생성, EOS 타이틀 전체 진입, 키보드·게임패드, 매우 긴 이름/이모지. 명패의 이번 정렬 수정과 이 제한을 구분해서 판정한다.

## 인계

변경 에셋1개와 변경 전 rollback, 보고서, 원본 A/B, 최종/수정 전 PNG·확대 비교·측정 JSON, 최종 실제 입력 캡처/로그, 영상, build log, Python authoring/분석 스크립트를 ZIP에 포함한다. Raw recording PNG 전체는 작업 폴더에 유지하고 ZIP에는 실제 timestamp 영상과 프레임 metadata를 담는다. 실행 스크립트는 검증 근거이며 자동 재실행 지시가 아니다.

다른 프로젝트로 옮긴다면 기존 Entry 부모 클래스/바인딩·참조가 같은지 확인하고 해당 WBP만 동일 package 경로에 적용한 뒤 컴파일·PIE 확인한다. Sprite·텍스처·레벨 전체를 덮어쓰는 이전은 필요하지 않다. rollback은 에디터 종료 상태에서 해당 WBP만 원본 백업으로 복구하는 범위다. 원본 프로젝트로 별도 마이그레이션하거나 배포하지 않았다.
