# ProjectMW UI ↔ UCM 에셋 연동 입력 계약

계약 버전 **UI-UCM-1.0**, 조사 시점 **2026-11-03T13:58:19+09:00**. UI 소비자 측의 현재 입력 계약이다. 3D/게임플레이/트랙의 미구현 API를 확정된 구현으로 취급하지 않는다. 공급자의 항목별 매핑 회신과 후속 이식 승인은 별도다.

## 1. 실제 프로젝트와 근거

- ProjectMW: `C:/workspace/UE/ProjectMW/MW.uproject`. 사용자 표기의 `MW/.uproject`는 실제 경로가 아니다.
- UCM: `C:/Users/user/Documents/Codex/UCM_MasterLibrary/MakeWay_AssetReview.uproject`. `UCM_MasterLibrary/MakeWay_AssetReview/` 하위 프로젝트가 아니다.
- ProjectMW Runtime: `5.8.4-58902052+++UE5+Release-5.8`. UCM은 EngineAssociation5.8 및 공급자의5.8.4 검증 기록을 읽었고 이번에 새로 열거나 컴파일하지 않았다.
- 로컬 `feature/title-world-ui`, HEAD `3db9c5d15e5496a4b699cb48e7796d826622bfb4`. 기존 미커밋·UVCS Pending101경로 보존. 잠금 조회는 빈 목록이었으며 소유권 허가를 뜻하지 않는다.
- 읽은 로컬 tracking ref: main `6a47da9`, Server/Lobby-Design `927fcff`(2026-11-02). fetch·원격 최신 조회·merge하지 않았다. 원격에 더 최신 변경이 없는지 확인한 결과는 아니다.
- 현재 UI 근거: Source/MW/UI, CDO PreviewCards, Widget Tree12개, AssetRegistry와 실행 중 PIE의 PreviewSurface를 **읽기 전용** 확인했다. 미저장 map/content0/0이다. 기준 화면은 [FRONTEND_UI_MASTER](FRONTEND_UI_MASTER.md)11.19다.
- 공급자 채팅 **Extract car assets into UE5**와 [기존 UCM Handoff](C:/Users/user/Documents/Codex/2026-10-20/c-program-files-x86-steam-steamapps/outputs/UCM_UIHandoff_20261103/UCM_TO_PROJECTMW_UI_HANDOFF.md)를 읽었다. 이 문서가 없었다는 공급자 기록은 작성 당시 이력이고, 이제 이 계약과 필수 CSV로 재대조한다.

## 2. 현재 UI가 정본인 화면별 경계

| 화면 | 현재 소유·표시 | 실제 필요한 리소스 / 연결 |
|---|---|---|
| 로그인 | WBP_Login_WorldDev의 기존 BP 그래프 → MWMultiplayerSubsystem, RefreshLoginUI | 기존 DisclaimerFrame/Border·SignBolt·MenuSign·폰트 유지. 새 UCM 장면 불필요 |
| 메인 메뉴 | UMWTitleRootWidget, WBP_TitleRoot_WorldDev → WBP_Title_WorldDev; BP_TitleWorldMenu_Dev의 단일 World WidgetComponent에 TitleScreen 연결 | 기존 Sign/Arrow/폰트·WBP 바인딩 유지. UCM 타이틀 레벨을 복사하지 않음 |
| 방 생성 | UMWCreateRoomWidget/WBP_CreateRoom_WorldDev | 방 이름·정원·맵 테마 ID. 맵 테마는 UWorld 카탈로그이며 개별 트랙 조각이 아님 |
| 방 검색·목록 | UMWRoomBrowserWidget/UMWRoomListEntryWidget | 기존 SessionId·목록 Revision·테마·인원 표시와 Sign 유지. 3D 차량/트랙 슬롯 없음 |
| 실제 로비 | 현재 로컬 AMWPlayerController의 BP 이벤트와 UMWRoomLobbyWidget/WBP_RoomLobby | 기존 PlayerState/GameState 표시. 로컬 Ready/Start 버튼은 비활성. 신 서버 브랜치의 HUD 소유권과 아직 다름 |
| 차량 Preview | UMWVehicleSelectionWidget + BP_VehicleSelectionPreview_Dev + 기존 카드/명패/마커 WBP | **Texture2D 썸네일**이 즉시 지원되는 차량 시각 입력. 5종·단일 행·현 상태판·전체 외형 Fit 유지 |
| 트랙 선택 | 이 로컬 UI에는 구현 없음. UI/Construction에 실제 선택 WBP·트랙 Native UI 클래스 없음 | 지금 새 화면을 만들지 않는다. 공급 목록·출처만 인계하고 소비 필드는 후속 건설 계약까지 보류 |

현재 로컬 흐름은 로그인→메인→방 생성/검색→기존 세션 월드의 로비다. 메인→차량 Preview→메인 경로는 **PIE·로그인 완료·세션 None**일 때만 활성화되는 개발 경로다. 서버 VehicleSelection 흐름이 이 개발 Preview로 이미 연결됐다는 뜻이 아니다.

## 3. 차량5종의 식별자와 순서

현재 FMWVehiclePreviewCard는 DisplayName/Thumbnail/bUnavailable/UnavailableReason만 가진다. **PreviewId·VisualClass·PawnClass 필드는 없다.** 이 계약의 `preview_key`는 아래 기존 Texture2D의 전체 Object Path를 고정한 **문서/인계용 기준 키**다. C++ 새 필드나 서버 ID가 아니다. 계약 동안 키와 기존 에셋 이름을 임의 변경하지 않는다. 교체물이 새 경로라면 기준 키는 유지하고 별도 delivered_thumbnail_path로 회신한다. 실제 적용 때는 UI 담당자가 버전·기존 경로·CDO 순서를 함께 확인한다.

| 순서(0부터) | 표시명 | preview_key / 현재 Thumbnail | UCM 시각 BP 후보(패키지) | 운영 VehicleAssetID |
|---|---|---|---|---|
| 0 | Smiet | `/Game/UI/Lobby/VehicleSelection/T_MW_VehiclePreview_Smiet.T_MW_VehiclePreview_Smiet` | `/Game/Gameplay/Vehicles/BP_VehicleVisual_Smiet` | PENDING |
| 1 | Mammoth | `/Game/UI/Lobby/VehicleSelection/T_MW_VehiclePreview_Mammoth.T_MW_VehiclePreview_Mammoth` | `/Game/Gameplay/Vehicles/BP_VehicleVisual_Mammoth` | PENDING |
| 2 | Ufology | `/Game/UI/Lobby/VehicleSelection/T_MW_VehiclePreview_Ufology.T_MW_VehiclePreview_Ufology` | `/Game/Gameplay/Vehicles/BP_VehicleVisual_Ufology` | PENDING |
| 3 | Doug | `/Game/UI/Lobby/VehicleSelection/T_MW_VehiclePreview_Doug.T_MW_VehiclePreview_Doug` | `/Game/Gameplay/Vehicles/BP_VehicleVisual_Doug` | PENDING |
| 4 | Fred | `/Game/UI/Lobby/VehicleSelection/T_MW_VehiclePreview_Fred.T_MW_VehiclePreview_Fred` | `/Game/Gameplay/Vehicles/BP_VehicleVisual_Fred` | PENDING |

- 순서는 기존 PreviewCards 배열 순서이며 영속 ID가 아니다. PreviewScenarios.Participants의 PreviewCardIndex는 이 배열을 가리키므로 순서를 바꾸면 참가자 차량도 바뀐다. 이름과5종 순서를 보존한다.
- UCM SourceRef는 SerializedFile/PathID를 포함한 원본 **차량 Visual BP 출처**다. 원본 썸네일 Sprite의 SourceRef를 새로 검증한 값이 아니다. 문자열 이름·PathID 단독·배열 인덱스로 차량을 식별하지 않는다.
- 운영 ID는 승인 카탈로그의 **FGameplayTag VehicleAssetID**다. PreviewKey/SourceRef/Definition 내부 FName VehicleId를 임의 변환하거나 하드코딩해 만들지 않는다.
- 현재 AssetRegistry: VehicleDefinition0, DriveData0. DA_AssetData에는 입력3개와 Asset.MapTheme.DevDefault→L_Dev_Default만 있다. Preview5종을 승인 차량으로 등록하지 않는다.

## 4. 즉시 받을 수 있는 데이터와 표시 경로

1. `WBP_VehicleSelection_Preview` CDO의 `PreviewCards[].Thumbnail : TSoftObjectPtr<UTexture2D>`가 현재 수신 필드다. DisplayName과 배열 순서를 유지하며 UI 담당자만 후속 연결한다.
2. `RefreshVehicles → UMWVehicleCardWidget::SetPreview → GetThumbnail`과 `MakePreviewPresentation/HandleCardHover → FMWVehiclePlayerPresentation.Thumbnail → UMWVehiclePlayerEntryWidget::SetPresentation`이 같은 그림을 상단·하단에 표시한다.
3. 현재 UE 조회값은5개 모두512×512, sRGB=true, TC_EDITOR_ICON이다. 원본 전체 실루엣·투명 배경·차량 비율을 보존한다. 텍스처에 프레임·글자·커서를 합성하지 않는다. 차종별 확대 좌표를 코드에 넣지 않는다.
4. 하단은 기존228×188 영역의 ScaleToFit으로 전체 그림을 맞춘다. 공급물도 머리·꼬리·바퀴가 원본 캔버스에서 잘리지 않아야 한다. 새 썸네일이 필요하면 독립 Texture2D와 알파/크기 정보를 공급하고, 현재 에셋을 자동 덮어쓰지 않는다.
5. 운영 분기는 `VehicleAssetRoot=Asset.Vehicle` 아래 승인 Definition을 읽고 `VehicleThumbnails : TMap<FGameplayTag,TSoftObjectPtr<UTexture2D>>`로 그림을 받는다. Definition 자체에 Thumbnail/VisualClass 필드는 없다. AssetManager가 ID 문자열 순으로 정렬하므로 운영 등록만으로 Preview 순서가 보장되지 않는다. UI 담당자가 후속 연결 전에 표시 순서를 별도로 해결하며, UCM이 정렬용 가짜 태그를 만들지 않는다.

## 5. 3D Preview 지원 여부와 공급 계약

**현재는 차량3D Preview 미지원이다.** World Space UI와 차량3D 렌더링을 구분한다.

- Preview Actor의 컴포넌트는 WidgetComponent1개이며 DrawSize1920×1080·World·hardware input=true다.
- `PreviewSurface.GetRenderTarget()`의 런타임 TextureRenderTarget2D는 실제 관찰했다. 이는 UMG를 월드에 그리는 내부 RT다. 차량 SceneCapture·차량 전용 RT 에셋이 아니다.
- Source의 차량 UI에는 SceneCaptureComponent2D·차량 Visual BP Spawn/Destroy·3D 선택 교체 필드가 없다. 배경의 정적 장식 차량도 선택 차량 Preview가 아니다.
- UCM Visual BP5개는 이 계약에서 **선택적 metadata 공급물**이다. Blueprint 패키지/Object/GeneratedClass 경로, 실제 ParentClass, cm 단위 Bounds/Pivot/기준 방향, component Transform, Material slot, 텍스처·머티리얼 closure, 검증 이미지/상태, BeginPlay·Tick·충돌·물리·월드 외부 참조 유무를 반환한다. 파일 존재만으로 Spawn/Cook/외형 정상으로 판정하지 않는다.
- 즉시 연결 경로는 기존2D를 유지하는 Texture2D 공급이다. 3D 시각 BP를 바탕으로 오프라인 썸네일을 공급할 수 있으나 별도 제작/권한 승인이 필요하다. 실제 런타임3D는 UI 담당자의 별도 작업 전까지 보류한다.
- 후속3D가 승인되면 기존 Preview 소유자를 우선 확장할지 검토한다. 선택 이전 actor와 비동기 로드를 정리하고, 현재 요청/소유자 유효성을 확인한 뒤 새 시각 actor를 만든다. ClosePreview/EndPlay/위젯 해제 시 참조·캡처·actor를 해제해야 한다. **이는 요구조건이며 현재 API가 아니다.** 새 Subsystem·범용 Adapter를 필수로 요구하지 않는다. Pawn 생성·Possess·서버 Replication은 포함하지 않는다.

현재2D 교체는 Brush/Thumbnail 참조 변경이다. ClearCards는 카드 Delegate를 해제하고 배열·선택·Panel children을 정리한다. NativeDestruct는 GameState 구독·커서 매핑을 해제한다. ClosePreview는 Widget/충돌/소유 플레이어/카메라·입력 상태를 정리한다. 공유 에셋 정리를 이유로 AssetManager.ReleaseAllAssets나 강제 GC를 호출하지 않는다.

## 6. 트랙 카탈로그의 연결 위치와 보류 범위

UCM 공급 CSV139행은 차량5 + Manifest133 + 별도 Preview1이다. Manifest133은 트랙 후보114 + Undo Tester 진단1 + 관련 요소18이며, 모두 플레이 가능한 트랙133종이라는 뜻이 아니다. Offset Attachment/Power Up Blocks/Bulldozer3개 연결, 좌우 변형·해금·런타임 노출 등은 공급자의 미확정 상태를 유지한다.

현재 방 생성의 `GetAvailableMapThemeIds/ResolveMapPackageName`는 Asset.MapTheme 아래 **UWorld**만 허용한다. PieceManifest/트랙BP/썸네일을 이 목록에 넣지 않는다. Source의 SelectConstruction 입력 태그는 화면·카탈로그 구현 증거가 아니다.

후속 건설 UI가 승인되면 공급 정보는 SourceRef/이름/역할·변형 관계/기존 BP 경로/별도 썸네일/Bounds·Pivot/의존성/검증 상태다. 연결은 기존 UMWAssetManager·FMWAssetEntry를 우선 사용하되 **트랙용 데이터 타입·태그 Root·승인 목록·연결/Footprint 규격·실제 UI 수신 함수는 현재 미확정**이다. 서버/건설/TA 담당자가 정본을 정한 뒤 UI는 표시 데이터만 소비한다. SourceRef를 임의 GameplayTag로 바꾸거나 UCM Visual BP를 배치 가능한 건설 정의로 간주하지 않는다. Raw System/Tags/UnlockLevel 값으로 해금·준비 규칙을 추정하지 않는다.

## 7. 공급자가 회신할 최소 매핑

[필수 에셋 CSV](PROJECTMW_UI_REQUIRED_ASSETS.csv)의 requirement_id와 preview_key를 그대로 회신한다. 기존 공급 CSV의 stable_id/source_ref 열도 보존하고 서로 다른 namespace임을 표시한다.

| 필드 | 의미 |
|---|---|
| ui_contract_version / ui_requirement_id / ui_preview_key | `UI-UCM-1.0` 및 본 CSV와 일치하는 인계 키. 새 Runtime ID 생성 아님 |
| source_ref / source_kind / source_sha256 | 실제 원본 출처, 차량BP/썸네일 구분, 해당 소스 파일 해시 |
| ucm_package_path / object_path / generated_class_path | `/Game/.../BP_X`, `/Game/.../BP_X.BP_X`, `/Game/.../BP_X.BP_X_C`를 구분. Texture에는 class path를 넣지 않음 |
| delivered_thumbnail_path | 실제 공급 가능한 Texture2D의 전체 Object Path. 없으면 빈값+이유 |
| dependency_manifest / parent_class / script_modules | 루트별 hard/soft closure, Native 부모, 플러그인·C++ 참조의 실제 역할 |
| proposed_destination / conflict_status / existing_sha256 | 실제 대상과 대조. 동일 SHA도 참조·소유권·외형 검증 완료와는 다름 |
| readiness / validation_scope / evidence | 파일 존재, load/compile/spawn/RHI/Cook/외형 검사별로 분리 |
| rights_status / rights_scope / authority_record | 팀 내부 사용/복제/공개 Git/배포를 구분. 불명확하면 신규 복사하지 않음 |
| gameplay_vehicle_asset_id / definition_path | 소유자가 승인한 경우만 채움. 현재5종은 PENDING; 가짜 Definition/Pawn 생성 금지 |

공급자 의존성 CSV의4586행은 고유1019패키지의 루트별 반복을 포함한다. 차량 closure에 /Script/InterchangeEngine·/Script/NavigationSystem 참조가 관찰되지만 그 자체로 새로운 Runtime 플러그인 의존성을 확정하지 않는다. import metadata/네이티브 부모/실제 실행 호출을 구분하고 UCM 사용자정의 모듈이나 하드코딩 원본 디스크 경로가 Runtime에 필요한지 검사한다. AssetImportData의 Reimport 경로와 Runtime load dependency를 구분한다.

## 8. 서버·차량 담당자 계약을 우회하지 않는다

로컬 Definition은 아직 PawnClass/DrivingInputData 구조다. main6a47da9의 Definition은 DriveData로 바뀌었고 GetDrivingInputData의 반환 타입도 달라졌다. DriveData는 주행 Config이며 Pawn/VisualClass를 갖지 않는다. main에는 MWVehicleCharacter/MovementComponent가 있지만 이번 로컬에는 미반영이다. **최종 Pawn 생성 클래스의 데이터 연결은 이 UI/UCM 계약으로 새로 정하지 않는다.**

정본은 `origin/Server/Lobby-Design@927fcff:Docs/Shared/Client,Server/16_로비_게임플레이_분리와_차량선택_전환_계약.md`다. VehicleAssetID 단일 GameplayTag, 최초 승인 후 취소/변경 불가, 중복 차종 선택 허용, 전원 확정과 Phase/Travel은 서버 책임이다. 해당 스냅샷의 실제 소스에는 차량 선택 RPC·PlayerState 확정 Getter/Setter·전원 대기가 아직 없다. 설계상 RequestConfirmVehicle을 현재 호출 가능한 함수처럼 사용하지 않는다. 실제 운영의 생성/Possess 목표는 PreparingRace이며 시각 BP Spawn과 다르다.

로컬 UI는 AMWGameState/AMWPlayerController를 아직 사용하고, 서버 브랜치는 Lobby/Gameplay 분리와 HUD로 이전했다. 후속 UI 통합은 승인된 서버 API에 맞춰 소비 경로를 조정한다. UCM이 UI를 유지하려고 서버 자료형·Phase·Replication을 바꾸거나 새 RPC·중복 상태를 만들면 안 된다.

## 9. 허용 범위·폴더·소유권

- 이번 단계는 경로·의존성·입력 계약 문서화뿐이다. Source/WBP/레벨/Config/카탈로그·UCM 원본을 수정하거나 에셋을 복사하지 않는다. 실제 연결은 **이 UI 채팅의 소유 작업**이다.
- UI 리소스는 기존 `/Game/UI/Common`, `/Game/UI/Lobby/VehicleSelection`을 유지한다. 차량 BP는 `/Game/Gameplay/Vehicles` 바로 아래, 전용 아트는 Vehicles/Art, 정의는 Vehicles/Data다. 트랙은 Gameplay/Tracks, 별도 Hazard/Utility는 해당 기능 폴더다. 기능마다 Mesh/Texture/BP 폴더를 새로 반복하지 않는다.
- 현재 StartupAssetData는 `/Game/Data/DA_AssetData`다. 새 카탈로그 정책은 Data/Catalogs지만 기존 것을 그곳으로 자동 이동하지 않는다. InputData도 이번에 재배치하지 않는다.
- 공식 L_Title·WBP_RoomLobby, 개발 레벨2개, 카메라/배경·P1/UCM 타이틀 복구물은 보호 대상이다. 기존 타이틀을 UCM 장면으로 덮어쓰지 않는다.
- Source/MW/UI와 WorldDev·VehicleSelection WBP는 UI 소유, 서버 Framework/EOS/MWConnection는 서버 소유, Definition/Drive/차량 주행은 차량 담당자 소유다. 아래 EntryPoints 문서의 충돌 목록을 먼저 확인한다. 잠금이 없다고 편집 권한을 얻는 것은 아니다.
- 기존 텍스처 팀 내부 공유는 사용자 확인 이력이 있다. 공급자 기록의 신규 UCM 복구물 권리 상태는 UNCONFIRMED다. 그 차이를 보존하고 공개 Git/재배포 허가로 확대하지 않는다. 본 작업은 상용 바이너리를 새로 복사하지 않는다.
- Git stage/commit/push/merge/reset/rebase, UVCS checkin/update/switch는 실행하지 않는다. 임의 overwrite·대규모 ZIP·통합 새 UI 제작 금지.

## 10. 후속 연결 전 통과 조건

1. 공급자 매핑의 계약 버전·키·5종 순서를 현재 CDO와 대조하고 승인 범위를 확인한다.
2. 필요한 루트만 선택해 source/dependency SHA·/Game 충돌·Native 클래스·권한·Cook 포함 경로를 재검사한다. 현재 Smiet BP가 같은 경로/동일 바이트여도 즉시 덮어쓰지 않는다.
3. UI 담당자가 현재2D 필드에 연결한다. 실루엣·프레임 내부 Fit·상태색·Hover/선택/취소/잠금·재진입을1920/960에서 다시 확인한다. 3D는 별도 승인된 최소 작업 전까지 연결하지 않는다.
4. 운영 연결은 최신 서버/차량 데이터 계약과 승인 Definition/Tag/요청 API가 준비된 뒤 한다. UI 로컬 확정을 서버 승인으로 표시하지 않는다.
5. 본 문서 검증은 소스/저장 CDO/실행 중 객체/메타데이터 대조다. 이번에 새 PIE 입력·빌드·Cook·UCM compile/spawn을 실행한 결과가 아니다. 보호 파일 해시와 문서/CSV 링크 검사는 별도 검증 결과에 남긴다.

함수별 연결 위치: [ENTRYPOINTS](PROJECTMW_UI_INTEGRATION_ENTRYPOINTS.md). 최신 시각 구현은 [MASTER](FRONTEND_UI_MASTER.md)를 따른다.
