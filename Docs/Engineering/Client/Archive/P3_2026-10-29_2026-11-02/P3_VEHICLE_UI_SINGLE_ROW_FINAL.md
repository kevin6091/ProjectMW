# P3_VEHICLE_UI_SINGLE_ROW_FINAL

2026-11-02 / Client·Art·TA / UE5.8.4 / `feature/title-world-ui`

## 결과

4+1은 폐기했고, 선택 가능한5대는 **Smiet / Mammoth / Ufology / Doug / Fred**의 중앙 단일 행이다. Candidate C를 그대로 출발점으로 사용했으며3+2·4+1 재검토나 신규 기능·배경 제작은 하지 않았다. 상단 이름은 기존 Noto 글꼴의 크기만18→26으로 바꿨고, 원본 프레임·색·차량 이미지 비율·이름 하단 위치·볼트·포인터 Sprite를 보존했다. 하단 전시판은 기존 중앙 Overlay/이름22/고정 어두운 글자색/분리된 식별 badge가 요구 구조에 맞아 파일을 수정하지 않고 실제 렌더링을 검증했다.

새 Editor에서 저장본을 재로드한 실제 PIE21개 화면과 실제 마우스 Hover·선택·확정·다시선택·Back을 확인했다. 다인 상태는 기존 개발 Preview fixture이며 실제 서버 다중 참가가 아니다. 배경 차이는 이번 UI 완료 판정 대상에서 제외한다.

## 실제 프로젝트와 변경 파일

프로젝트 `C:/workspace/UE/ProjectMW/MW.uproject`. 개발맵은 `Content/Levels/L_Title_WorldUI_Dev.umap`, `Content/Levels/L_VehicleSelection_UI_Dev.umap`이다. 새 프로젝트/브랜치/worktree는 만들지 않았다.

변경한 기존 Content6개:

- `Content/Levels/L_Title_WorldUI_Dev.umap`
- `Content/Levels/L_VehicleSelection_UI_Dev.umap`
- `Content/UI/Lobby/VehicleSelection/WBP_VehicleCard_Dev.uasset`
- `Content/UI/Lobby/VehicleSelection/WBP_VehicleSelection_Dev.uasset`
- `Content/UI/Lobby/VehicleSelection/WBP_VehicleSelection_Preview.uasset`
- `Content/UI/Lobby/VehicleSelection/Art/M_VehicleSelectSky_Dev.uasset`

새 Content1개:

- `Content/UI/Lobby/VehicleSelection/T_MW_VehiclePreview_Mammoth.uasset`

WBP 변경은3개다. `WBP_VehicleSelection_Dev`는 Wrap/그룹 설정, `WBP_VehicleCard_Dev`는 이름 font size, `WBP_VehicleSelection_Preview`는 Mammoth 데이터 연결만 바꿨다. `WBP_VehiclePlayerEntry_Dev`, `WBP_VehiclePlayerMarker_Dev`, Preview Actor BP 및 기존 Sprite·폰트·다른4개 차량 Texture는 해시가 동일하다. 두 개발맵 변경은 아래의 Fidelity 배경 복구이며 UI Actor·Camera·WidgetComponent는 유지했다.

## 4+1 → 단일 행 / 최소 가독성 보정

|항목|직전 Fidelity|현재 저장값/실측|
|---|---|---|
|WrapBox|978 / 두 행4+1|1224 / 단일 행5개|
|카드 design|240²|240² 그대로|
|안쪽 간격|6×6|6×6 그대로|
|그룹 Canvas slot|(510,228),900×430|(470,300),980×240 — 기존 Candidate C|
|1920 실제 카드 폭|209.6px|189.7px; 확대하지 않음|
|1920 실제 전체 행|854×425 다층|약967×190 단일 행; 화면 가로 중심 정렬|
|차량명 font|Noto Bold18|같은 글꼴26 / 이름 박스208×48 유지|
|차량명 잉크 하단|약13.5px|약8.4px / 원본 Doug 약10px와 가까운 하단 영역|
|960 실제 차량명 높이|8px|11px — TextBlock만 확장한 결과가 아님|
|하단 이름|22 / 중앙 Overlay / 잉크 아래약14px|동일 설정·동일 파일; 한글 잘림·색 대비 검증|

선택판의 등록 데이터가 현재5개일 때 한 행이 되도록 Designer Wrap 너비를 지정했다. C++의5개 좌표/순서/개수 분기를 추가하지 않았다. 더 많은 실제 데이터가 등록되면 기존 WrapBox가 자동으로 다음 행을 만든다. 원본의 여러 잠금/미보유 카드를 추가하지 않았다.

텍스트는 원본처럼 아래쪽에 남겨 두었다. 동일한 이름 영역 안에서 글꼴 크기만 올려 실제 획을 확보했으며 이미지 영역208×172, 이름 간격9, 하단 Spacer1은 바꾸지 않았다. 실제 이미지 영역과 이름 잉크 사이에도 여유가 있고, 프레임 하단과 글자가 접하거나 잘리지 않았다. 상단5개 차량명 잉크 하단은1920에서 모두 y508,1280에서 y338,960에서 y254로 일치했다. 하단2·3·4인도 같은 해상도에서 이름 하단 기준선이 동일했다.

원본 FontAsset는 `Race 1 Brannt Plus Vehicle Names`이고 재사용 가능한 동일 TTF는 이전 조사에서 확인되지 않았다. 대체 Noto Bold를 원본 글꼴이라고 표기하지 않는다. 이번에는 글꼴 교체·새 Outline·새 그림자를 추가하지 않았다. 원본 다수 카드 화면과5대 단일 행의 전체 밀도 차이는 사용자 확정 제약으로 남는다.

## Mammoth 실제 리소스 교체

원본 추출 파일 `C:/workspace/Unpack/AssetStudio/Texture2D/T_vehicle_mammoth_icon.png`, RGBA2048², alpha0..255. SHA-256 `8f73ea63820e6264fe7f70bab2dd206f9824460638f3311f701e8a387847c040`.

원본 게임 `Make Way_Data/sharedassets0.assets`의 Texture2D **PathID251 / T_vehicle_mammoth_icon**를 직접 읽어 PNG와 RGBA 픽셀이 완전히 동일함을 확인했다. `avatars/mammoth` 번들에는 차체/부품 Texture가 있지만 이 메뉴 아이콘은 없었으며, Frontend sharedassets0에서 확인했다. 잘못된 번들 경로를 아이콘 원본으로 주장하지 않는다. `mammoth-native-source.json`에 source/hash/native pixel 검증을 남겼다.

새 UI Texture는 기존 나머지 Preview 아이콘과 동일한 UI compression/sRGB/no mipmaps/최대512 설정이다. 원본 PNG를 다시 그리거나 배경을 합성하지 않았다. 새 차량 Pawn·VehicleDefinitionData·카탈로그 등록0이다.

PreviewCards의 The Raven 항목(기존 배열 index1)의 DisplayName과 Thumbnail을 함께 바꿨다.10개 Preview 시나리오의 기존 차량 인덱스 참조는 유지했다. 실제2인·3인·4인 화면, Hover/선택/확정/재선택에서 Mammoth 이름과 실제 Mammoth Brush 리소스가 상단·하단에 연결된다. 기존 Raven Texture는 삭제하지 않았지만 현재 선택 가능한5대 데이터와 모든21개 실행 결과에는 Raven이 없다. AssetRegistry에서 Raven Texture의 현재 referencer0, Mammoth는 WBP_VehicleSelection_Preview가 참조하는 것을 확인했다. 복구 패키지까지 총22개 package의 참조 목록을 `resource-reference-review.json`에 남겼다. 과거 리소스를 임의 삭제하거나 다른 시스템의 참조를 깨뜨리지 않았다.

## Fidelity 배경만 복구·비활성화

수정 전 실제6개 Fidelity 변경 파일은 이전 결과 SHA와 모두 일치했다. 현재 작업249개 기준 파일과 소유 맵/WBP 백업을 따로 보관했다. 레벨 전체를 과거 버전으로 덮어쓰지 않고 Actor별 속성을 수정했다.

|대상 / 각 개발맵|적용|보존|
|---|---|---|
|FIDELITY_Betty_L/R_0..6 /14개|HiddenInGame·collision off|Actor/메시/재질 파일 삭제 없음|
|FIDELITY_GridTile_0_0..2_3 /12개|HiddenInGame·collision off|Actor/복구 geometry 파일 유지|
|FIDELITY_GridOverlay/Rear /2개|기존 숨김 유지|실험 overlay2개도 삭제하지 않음|
|REBUILD_DecorativeSmiet_0/1|Fidelity가 숨긴 기존 물체를 다시 표시|원래 transform/mesh 유지|
|TrackDeck/TrackDeckRear|Fidelity override 해제해 native mesh material로 복구; 뒤 Deck X4800→4500|원래 메시·나머지 transform 유지|
|BoardSupport_L/R,SupportFoot_L/R|Fidelity Y±540→직전값 base±900|기존 UI 개발 지지대 유지·삭제 없음|
|M_VehicleSelectSky_Dev|Editor 정상 종료 후 Fidelity 직전 백업 바이트로 정확히 복원|새 하늘 색상 설계 없음|

신규 비활성화는 각 맵26개, 기존 숨김2개 유지로 총28개 Fidelity Actor가 비활성 상태다. 이 Actor가 여전히 원본 패키지를 참조하므로 이미 복사된 UCM 리소스·Fidelity Material은 파일로 남겼다. 다른 시스템 참조·소유권을 무시한 삭제는 하지 않았다. 신규 복구 UCM 메시/재질/난간/장식차 복사0이다.

전후 Actor 목록/transform/material/hidden 상태와 Camera FOV·WidgetComponent DrawSize/클래스/공간/hardware input을 비교했다. 허용된 복구 외 Actor 변경0, Actor 생성·삭제0, Camera/WidgetComponent 변화0이다. `background-dev.json`, `background-title.json`이 상세 목록이다. Sky의 현재 SHA `0553f39249b58038c903cd31c294a46be0d05b0fc2cfcafd128b631867bbc14d`는 Fidelity 직전 백업과 정확히 일치한다. 원본 L_Title/팀 공용 배경/P1/UCM 원본은 수정하지 않았다.

## 실제 렌더링·가독성

아래는 화면을 확대한 DesiredSize 수치가 아닌 native client PNG에서 분리한 실제 글자 잉크 높이와 하단 여백이다. 전후 비교의 수정 전은2026-10-30 타이틀 개발맵의 실제 PIE 자료이며 현재 수정 전 WBP/맵 해시가 해당 저장본과 일치한다. 수정 후21개는2026-11-02 독립 검증맵의 새 Editor 실제 PIE다. 두 맵의 상대 카메라 설정(FOV45/Pitch-6)·동일3인 fixture로 비교하며 월드Y 원점은 다르다. 사용자 요청대로 배경은 복구돼 달라졌고 UI 판정에서 제외한다.

|해상도 가로|차량명 높이 / 하단|플레이어명 높이 / 하단|차량명 대표 명암비|단일 포인터 원림 지름|
|---|---|---|---|---|
|1920|22px / 8.4px|26px / 14.0px|5.15~5.42:1|119~120px|
|1280|14px / 6.3px|17px / 9.7px|5.14~5.31:1|80~80px|
|960|11px / 4.2px|12px / 7.5px|4.95~5.24:1|60~60px|

상단·하단 이름의 분리된 ROI 경계에 닿는 잉크0. 한글의 받침을 포함한 글자가 잘리지 않고, 이미지와 이름이 겹치지 않음을 native 이미지로 확인했다. 대표 명암비는 글자 코어/주변색의 median 샘플로, 접근성 인증을 의미하지 않는다.960의 핵심 이름은11/12px로 구분 가능해졌지만 고해상도와 같은 읽기 크기는 아니다. 작은 보조 조작 안내는 그대로이며 저해상도에서 여전히 작다. 원본 스타일 보존 요구 때문에 상태판/명패/글자를 전시판 밖에 추가하지 않았다.

원형 포인터는 같은 원본 Sprite와 기존176 design root·152 outer/121.6 inner·45도 방향을 유지했다. 작은 Candidate C 행으로 투영되면서 실화면 지름도 약120px가 된다. 원본132px와 차이는 남지만 Sprite를 새로 그리거나 재설계하지 않았다. 다수 참가자가 같은 차량을 선택한 경우 기존2×2 마커 표시를 유지했다.

## 2·3·4인 실제 PIE

1920×1080,1280×720,960×540 각각 아래7개를 촬영했다(총21장):

- 2명/정원2: 패널2개, 서로 다른 차량, 로컬 강조.
- 2명/정원4: 참가자2/대기2, 고정 슬롯.
- 3명/한 명 미선택: Smiet·Mammoth 표시, 세 번째 전시판 비어 있음.
- 3명/전원 차량 표시: Smiet·Mammoth·Ufology, 확정/선택중 구분.
- 4명/서로 다른 차량:2확정·2선택중, 로컬1명 강조.
- 4명/동일 Smiet: 마커4개2×2, 중복 선택 정책을 서버에 추가하지 않음.
- 4명/전원 확정: 기존 상태 표현 유지.

모든 화면에서 실제 Wrap 행은 [5],5개 이름·이미지 순서가 동일하며 게임 VehicleId=None/승인 Definition0을 유지한다. 인원·차량·상태 데이터는 기존 WBP PreviewScenarios이며 PlayerState/서버 정본으로 가짜 데이터를 등록하지 않았다. `final-captures.json`에는 참가자·이미지 Brush 실제 경로·포인터 수·카메라·실제 viewport/카드 위치를 보관했고 모든 Mammoth 참가자의 하단 Texture 경로를 검사했다.

추가로 `L_Title_WorldUI_Dev`의 실제 PIE에서도 같은5대 단일 행과 Mammoth 상·하단 표시를 촬영했다(`title-dev-3-filled-1920.png`). 이1장은 기존 OpenPreview 개발 API로 스테이징하고 로그인 Root만 해당 PIE에서 Collapsed로 둔 검증이다. 로그인 상태/저장 에셋을 변경하지 않았고 인증된 타이틀 버튼 전체 경로를 검증했다는 뜻이 아니다. 필수21개와 마우스 검증은 독립 개발맵에서 수행한 결과로 계속 구분한다.

## 실제 마우스 입력 / 컴파일·빌드

|검사|해상도|결과|
|---|---|---|
|실제 drag로 Hover / 미선택 참가자의 Mammoth 미리보기|960|통과 — 실제 Sky 마우스 입력, Delegate Broadcast 사용0|
|실제 이미지 클릭 / Mammoth 선택|960|통과 — 실제 Sky 마우스 입력, Delegate Broadcast 사용0|
|실제 확정 클릭 / Mammoth 잠금|960|통과 — 실제 Sky 마우스 입력, Delegate Broadcast 사용0|
|확정 상태에서 Fred 클릭 / Mammoth 유지|960|통과 — 실제 Sky 마우스 입력, Delegate Broadcast 사용0|
|실제 다시 선택 클릭 / 잠금 해제|960|통과 — 실제 Sky 마우스 입력, Delegate Broadcast 사용0|
|잠금 해제 후 Fred 선택|960|통과 — 실제 Sky 마우스 입력, Delegate Broadcast 사용0|
|Mammoth 재선택 / 상·하단 원본 이미지 일치|960|통과 — 실제 Sky 마우스 입력, Delegate Broadcast 사용0|
|1280 차량명 영역 실제 클릭|1280|통과 — 실제 Sky 마우스 입력, Delegate Broadcast 사용0|
|1920 차량명 영역 실제 클릭|1920|통과 — 실제 Sky 마우스 입력, Delegate Broadcast 사용0|
|원형 포인터가 겹친 카드 영역 실제 클릭|1920|통과 — 실제 Sky 마우스 입력, Delegate Broadcast 사용0|
|실제 Back 클릭 / 위젯 해제·Surface 숨김|1920|통과 — 실제 Sky 마우스 입력, Delegate Broadcast 사용0|

Sky 창 관찰→실제 입력→재관찰 후 기존 C++ 상태와 실제 화면으로 판정했다. fixture 설정/해상도/캡처는 기존 개발 API를 사용했고, 위 Hover·선택·확정·Reset·Back 검증에는 Button Delegate 직접 Broadcast를 사용하지 않았다. 한 번 OBS 창이 Preview를 가려 클릭 전에 대상을 재관찰·활성화했으며 다른 앱의 제어는 하지 않았다. 이번 실행에 보안·인증 창 차단은 없었다. EOS 로그인·실제 타이틀 메뉴 전체 경로는 이번 차량 UI 검증 범위에서 다시 실행하지 않았다. 독립 Preview의 Back은 위젯 해제·Surface 숨김을 확인한 것이며 전체 EOS→타이틀 복귀 검증으로 바꾸어 쓰지 않는다.

변경 WBP3개를 Entry 월드에서 컴파일·저장했고 새 Editor에서 관련 Widget5개+Actor BP1개의 BS_UP_TO_DATE를 확인했다. 배치된 Actor BP 재컴파일는 하지 않았다. 실제 게임 `/Game` 참조 누락0. MWEditor Win64 Development: Succeeded/Up to date/2.67초. MW Win64 Development: Succeeded/Up to date/1.67초. C++ 변경0이라 새 소스 컴파일은 없었다. Cook/패키징·실서버 다인 접속은 미실행이다.

최종 PIE 로그 오류 서명은 `{'Assertion failed': 0, 'Pure virtual not implemented': 0, 'Ensure condition failed': 1, 'Blueprint Runtime Error': 0}`. 기존에도 발생했던 renderer `AppTime.cpp:38 IsInGameThread` handled ensure1회가 있으며 무오류 실행이라고 주장하지 않는다. Fatal/assertion/Pure virtual/BP runtime error는 재발하지 않았다. 이전 TickTaskManager assertion과 Pure virtual의 근본 원인을 이번 UI 작업에서 수정했다고 주장하지 않는다. 편집과 PIE를 분리하고 Entry의 WBP만 편집했으며 종료 직후 동기 맵 로드·Reinstancing을 섞지 않았다. 종료 뒤 약341.7초 Idle/game_world=None, 미저장 패키지0, 콜백0인 제한된 관찰 결과는 `final-runtime-validation.json` 참조. 이후 안전한 posttick으로 타이틀 개발맵을 다시 열어 저장 Actor 상태와 불일치0을 확인했다. Authoring 중 Python API 이름·Struct 주소를 값으로 비교한 진단 오류는 수정 후 재실행했고 실제 PIE 엔진 오류와 구분한다.

## 기존 코드·협업 보존

`UMWVehicleSelectionWidget::RefreshVehicles/AddCard/RefreshPlayers/RefreshCardPointers`, 기존 Card/PlayerEntry/PlayerMarker의 SetPresentation/SetParticipants/Button 이벤트·Delegate 수명, Preview Actor OpenPreview/ClosePreview를 그대로 사용한다. 새 C++ 클래스·함수·Subsystem·서버 RPC0, C++/Config/uproject 변경0. 카탈로그·AssetManager·VehicleDefinitionData·GameMode/GameState/PlayerState/PlayerController/EOS 파일이 기준 해시와 동일하다. 새 C++ 경로/차량명/ID/개수/좌표/해상도 분기 하드코딩0이다.

Source/Config/Content/Docs/uproject249개 기준 비교에서 허용한6개 기존 Content만 달라졌고 새 Content는 Mammoth Texture1개다. 정식 L_Title SHA `844e37bd881e91ebd1487f4dd354b172f1ca24656caf1d21a5632d5bd345cf67` 유지. 기존 Git Pending·UVCS Pending을 초기화하지 않았다. HEAD3db9c5d/feature/title-world-ui와 UVCS 로컬cs16을 유지했다. 조회 시 원격head는 시작20에서 종료21로 바뀌었으나 이 작업에서 Update하거나 새 체인지셋을 통합하지 않았다. Commit/Push/Checkin/Update/Switch/main 병합 없음. 작업 시작 전 대상 경로에 타인 잠금이 없음을 확인했고 차량/Pawn/입력·팀 잠금 파일은 변경하지 않았다. 원격head21의 변경과의 통합은 미검증이다.

## 남은 차이·미검증 / 인계

1. 원본 Race1 글꼴과 Noto의 획·폭 차이, 원본 다수 카드와5개 단일 행 밀도 차이는 남는다. 원본 전체 화면과 동일한 구성이라고 주장하지 않는다.
2.960 핵심 이름은 이전보다 커졌지만 보조 안내는 작다. 이름을 프레임 중앙으로 올리거나 새 상태판으로 UI를 확장하지 않았다.
3. 실제 멀티플레이 차량 선택 Replication·RPC·확정 정책·Pawn 생성/주행·새 EOS 인증·전체 타이틀 로그인 흐름은 미검증이다. 승인 차량0인 상태 유지.
4. 기존 Pure virtual의 정확한 Tick 객체는 여전히 미확정이다. 이번 정상 실행을 장시간 안정성 완치로 표현하지 않는다.
5. 배경은 Fidelity 추가분을 비활성화/복구한 것이고 원본 Make Way 배경을 새로 제작한 결과가 아니다. 배경 외형은 이번 UI 승인 기준에서 제외한다.

`COMPARISON.html`, 원본A/B 대비 `COMPARE_A/B.png`,4+1 전후 `COMPARE_BEFORE_AFTER.png`, native text crop `NAME_DETAIL_NATIVE.png`,21장 실제 PIE·마우스 증거·보존/참조/빌드 결과를 제공한다. 비교 PNG는 명시적인 좌우 편집 자료이며 실제 PIE 원본 PNG와 구분한다. client PNG는 OS 테두리만 자르고 해상도를 늘리지 않았다. Delta ZIP은 기존 개발 브랜치/Pending 위에서 적용하는7개 Content+보고서와6개 변경 전 파일의 rollback이며 독립 프로젝트가 아니다. 원본 UCM·전체C++·계정/EOS raw 로그는 넣지 않는다.
