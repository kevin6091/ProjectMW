# 최신 main 기준 프런트엔드 통합

기준일: 2026-11-16. 브랜치: `fix/frontend-main`.

- 기준 main: `ed6cbe3de1d6eb5305902f33229f9fab3fecc917`.
- 가져온 작업: `feature/race-hud@71247e59b37398d42cf0fc3f4d536b6454d2f88a`. 닫힌 PR #11의 미병합 기능과 이후 계기판 작업을 포함한다.
- 기존 브랜치는 그대로 보존했다. main에 직접 병합하거나 Push·PR 등록·UVCS 조작을 하지 않았다.
- `RACE_HUD_HANDOFF.md`의 main SHA·PR OPEN 표기는 11월 13일 당시 기록이다. 현재 통합 기준은 이 문서다.

## 무엇이 달라졌나

기존 브랜치는 수정2의 차량 예측·보간 코드가 포함돼 main의 접지·다운포스 변경과 충돌했다. 새 브랜치는 최신 main에서 시작하고, 프런트엔드 기능을 해당 구조에 맞춰 연결했다.

### main 그대로 유지한 부분

- `MWVehiclePawnNetwork.cpp`: RPC, 입력 순번, 예측 이력, 서버 보정, 원격 보간 전체.
- `MWVehicleNetworkTypes.h`: `GroundNormal`을 포함한 Snapshot 구조.
- `MWVehicleDriveData.h/.cpp`: 접지·다운포스·마찰 설정과 검증.
- 이동 컴포넌트의 `SimulateDriveStep`, Snapshot 저장·복원, 접지·다운포스·노면 정렬·차량 충돌 함수.
- `DefaultEngine.ini`의 `VehicleGround` 채널과 `MWPlayerState.h`의 main 변경.

수정2 전용 충돌 예측 자료·충돌 후 회전 상태·별도 표시 보간은 가져오지 않았다. 기존 수정2의 네트워크 동작까지 유지했다는 의미가 아니다. 네트워크 동작은 main을 따른다.

### main에 맞춰 연결한 부분

| 파일·함수 | 연결 내용 |
|---|---|
| `MWVehicleMovementComponent.h/.cpp` | 바퀴·파일럿이 조향 입력을 읽는 조회 함수와 기존 트랙 효과 API 추가 |
| `GetMaxSpeed`, `ResolveDriveControl` | 기존 부스터의 전진 제한·추진량 연결. 후진 제한과 입력 차단 유지 |
| `UpdateDriveVelocity` | 기존 부스터 가속과 종료 후 자연 감속 연결. main의 타이어 접지력 제한을 거친 뒤 적용 |
| `MoveVehicle` | 기존 롤러 등 표면 속도의 이동량만 추가. main의 Sweep·충돌 응답 유지 |
| `OnTeleported`, `EndPlay` | 기존 트랙 효과 만료와 발판 틱 선행 관계 정리 |
| `MWVehiclePawn.h/.cpp` | 바퀴·조향 블렌딩·가상 접지점·스키드·아이템·부스터 카메라 표시 유지. 네트워크 멤버는 main 기준 |

기존 `DrivePresentation` 컴포넌트 이름은 BP 부모 참조 호환을 위해 남겼다. 별도 네트워크 위치 보간과 외형 재부착에는 사용하지 않는다.

UI 폴더 이동, 로그인·인트로·메뉴·차량 선택·아이템 룰렛·오른쪽 계기판, 음악·교통·Environment 코드는 기존 작업을 옮겼다. 이번 작업에서 화면 디자인과 BP·맵 에셋을 다시 만들지 않았다.

## 검증

| 항목 | 결과 |
|---|---|
| Editor Development 빌드 | 성공, 113.94초 |
| Game Development 빌드 | 성공, 69.36초 |
| 차량 BP 6개 + UI BP 8개 | 전부 `BS_UP_TO_DATE`, 저장하지 않고 컴파일 |
| 자동 검사 | 13개 중 12개 성공, 아래 1개 실패 |
| 실제 CUA 키 입력 | W 이동, Space 아이템 소비, 복귀 후 S+Space 방 검색 진입 확인 |
| 실제 CUA 마우스 입력 | 방 이름 입력 → 방 생성 → 로비 → 방 종료 → 확인창 승인 → 세션 종료·복귀 확인. API로 방을 종료하지 않음 |
| 실제 EOS | 로그인 성공, 테스트 방 생성·종료 확인. 로그인 버튼을 누른 비캐시 인증 경로는 별도 미검증 |
| UI 표시 | 아이템 슬롯·오른쪽 계기판을 1920×1080, 1280×720에서 확인. BP 디자인 변경 없음 |
| 차량 5종 + 투명차 PIE | 아래 조건에서 Pawn 입력 API 주행·정지 조향·바퀴 회전·드리프트 검사. 물리 키보드로 6종 전부를 운전한 검사는 아님 |
| 다중 클라이언트·Cook·패키징 | 이번 통합본에서는 미검증 |

### 차량·부스터 결과

- 5종의 정지 좌우 조향과 주행 중 바퀴 회전 확인. 검사 구간의 바퀴 Relative Location 차이는 최대 `4×10⁻¹¹cm` 미만으로, 위치 점프는 계측되지 않았다.
- Doug의 `Fin`, `Tail` 자세 변화 확인. 기존 BP Timeline을 수정하지 않았다.
- 드리프트 후 실제 자국 인스턴스: Doug 56개, Fred 138개, Mammoth 134개, Smiet 138개, Ufology 135개, 투명차 128개.
- 6종 모두 가속 구간 약 64.8km/h 도달. 부스터는 실제 Space 사용 후 약 81km/h까지 올라갔으며, 추진 입력을 유지해도 종료 후 약 64.8km/h로 복구됐다.
- 일반 주행·부스터 검사와 UI 캡처는 아래의 테스트 바닥 호환 설정을 **PIE 메모리에만 적용한 조건**이다. 저장된 맵에서 아무 준비 없이 주행 검증을 통과한 것으로 해석하지 않는다.

### 테스트 맵의 새 접지 채널 연결 필요

`/Game/Developers/user/L_Dev_Drive_ItemPickup`의 `Plane`이 main에 새로 추가된 `VehicleGround` 채널에 `Ignore`로 응답했다. 일반 충돌은 차량을 받치지만 main의 접지 Sweep은 바닥을 찾지 못한다. 실제 `IsGrounded=false`와 `NoCandidate` 로그를 확인했다.

PIE 복제 월드의 해당 바닥만 `VehicleGround=Block`으로 바꾸자 `IsGrounded=true`가 되고 정상 주행했다. 검증 스크립트가 메모리에서만 바꿨으며 맵·BP 파일은 저장하지 않았다. 기존 복구 트랙 런타임 충돌체는 `SetCollisionResponseToAllChannels(ECR_Block)`으로 생성한다. 모든 저장 트랙 에셋·테마의 채널 상태를 전수 검증했다는 뜻은 아니다.

맵 담당자는 최신 에셋에서 바닥·주행면의 `VehicleGround=Block` 설정을 확인해야 한다. main의 채널 기본값을 전역 Block으로 바꾸거나 접지 검사에 우회 조건을 추가하지 않았다.

### 방 종료 복귀의 실제 동작

현재 가져온 코드에는 `MWUCMTitleFlow.cpp`의 `RestoreCreateRoomForm()` 경로가 있다. 이번 실행도 `CREATE_ROOM_FORM_RESTORED` 로그와 함께 **방 생성 창으로 복귀**했다. 이후 마우스로 ‘뒤로’를 눌러 메인에 돌아간 뒤 S+Space로 방 검색을 열었다. 방 종료 직후 메인 표지판으로 곧바로 복귀했다고 보고하지 않는다.

로그에서 최초 인트로 시작 1회와 `SESSION_RETURN_NO_INTRO`를 확인했다. 음악은 맵 이동 전후 동일한 `/Engine/Transient.AudioComponent_0`의 `Continue ... playing=1` 기록을 확인했다. 음질·연속 재생을 별도 녹음으로 검증한 결과는 아니다. 새 연속 동영상은 녹화하지 않았다.

### 남은 실패

`MW.Vehicle.CollisionAndGround`의 위쪽 충격 검사 2개가 실패했다. 테스트가 접지된 차량의 `Velocity`에 위쪽 속도 600cm/s를 직접 대입한다. main의 접지 유지 경로는 이전 노면의 접선 속도를 새 노면으로 운반하며, 외부 충격으로 접지를 해제하는 처리는 별도 `ApplyExternalVelocityChange` 경로에 있다.

관련 접지·외부 충격 함수와 기존 테스트 본문은 main과 동일하다. 부스터·표면 효과도 이 검사에서는 비활성이다. 다만 최신 main 단독 빌드를 따로 실행한 비교는 하지 않았으므로, main 단독 실행에서도 실패를 확인했다고 주장하지 않는다. 테스트를 숨기거나 main의 접지 코드를 임의로 변경하지 않았다. PR을 통과 상태로 제출하기 전에 담당자가 충격 적용 계약과 테스트를 확인해야 한다.

기존 `MWItemComponentVisual.cpp`의 비상수 `UMovieScene::GetBindings()` 사용에는 C4996 경고가 남아 있다. 두 빌드 모두 해당 경고와 함께 성공했다.

## 에셋·협업 상태

- 차량 BP 6개와 계기판 에셋 3개의 변경 전 해시를 기록했다. BP 컴파일 후 파일 내용 변경 없음.
- Content는 기존 관리 정책을 유지한다. 계기판 WBP·재질 등 필수 에셋은 다른 PC에 별도로 전달돼 있어야 한다.
- 기존 미추적 `KEVIN3_COMPARISON_AND_PR_SPLIT.md`, `PR_DRAFT.md`는 수정·커밋하지 않는다.
- 빌드 로그·자동 검사 JSON·CUA 캡처·변경 전 보존 자료는 프로젝트 밖 `outputs/MAIN_FRONTEND_INTEGRATION_20261116`에 둔다. 핵심 결과는 `MAIN_PRESERVATION.json`, `BLUEPRINT_COMPILE.json`, `Automation/index.json`, `VEHICLE_VISUAL_SUMMARY.json`, `SKID_INSTANCES.json`, `BOOST_RECOVERY_PIE_SUMMARY.json`, `PIE_VEHICLES.log`다.
- 변경은 작업 브랜치에 커밋해 보존한다. 자동 검사 실패와 에셋 연결 조건은 미해결로 남긴다. 이번 작업에서는 PR 등록과 main 병합을 하지 않는다.


## 후속: 우리 표시 코드의 설정 분리

- 실제 참가자 색은 `DefaultGame.ini`의 `[/Script/MW.MWParticipantPalette]`에서 편집한다. 차량 선택의 실제 참가자 표시와 경기 닉네임이 같은 설정을 읽는다. 경기 HUD는 더 이상 색을 얻으려고 차량 선택 WBP를 로드하지 않는다.
- 차량 선택의 개발 Preview 팔레트는 기존 BP 저장값을 유지한다. Preview를 위한 값을 실제 경기의 공통 설정과 혼동하지 않는다.
- 차량별 부스터 기본 장착은 `[/Script/MW.MWItemComponent]`의 `BoosterMountPresets`에서 편집한다. `VehicleClass`는 BP 클래스의 전체 경로, `Transform`은 Pawn 로컬 cm·회전·배율, `ExhaustLengthScale`은 배기 길이 배율이다.
- 우선순위는 기존 BP의 `bOverrideBoosterMount` → 가장 구체적인 클래스의 Config 기본값 → 기존 외형 경계 계산이다. 투명차의 BP override와 미등록 차량의 계산 경로를 보존한다. 실제 클래스 경로를 비교하며 메시 이름으로 차종을 검사하지 않는다.
- 이 변경은 표시 설정만 옮긴다. 아이템 지급·사용·복제, 차량 이동·보정·Snapshot, 트랙·GameMode 코드는 수정하지 않는다. BP·맵도 저장하지 않는다. 검증 결과는 후속 기록을 따른다.


### 설정 분리 검증 — 2026-11-16

- Editor/Game Development 빌드 성공: 48.70초 / 45.09초. 기존 MovieScene 비상수 GetBindings C4996 경고는 남아 있다.
- 차량·UI BP 15개 모두 컴파일 성공, 에셋 저장 없음. 기존 4색·대체 색과 무효 MemberId·팔레트 순환 9개 조회가 이전 값과 일치했다.
- 실제 아이템 테스트 맵 PIE에서 기존 픽업 중첩으로 6종에 부스터를 생성했다. 5종의 이전 C++ 장착 표와 투명차 BP override를 기준으로 위치·Yaw·배율·배기 길이 모두 일치했다. 키보드·마우스로 운전한 검사가 아니라 외부 스크립트로 실제 PIE를 계측한 결과다.
- 기존 `MW.Items.PickupAndUse`, `MW.UI.RaceHUD.StateLifetime` 자동 검사 2개 통과.
- 차량 이동·Pawn·네트워크·아이템 판정 cpp·로비 서버·기존 미추적 문서 및 보호한 BP의 SHA-256 유지 확인. 차량 선택 파일 안의 팀원 공유 커서 함수도 변경 전과 동일하다.
- 이번 변경에서 3인 커서 동기화·전체 EOS 흐름·Cook·패키징은 재검증하지 않았다. 앞 절의 접지 검사 실패와 테스트 맵 VehicleGround 연결 문제를 해결한 것으로 간주하지 않는다.
- 설정 분리용 기록은 프로젝트 밖 `outputs/FRONTEND_DECOUPLING_20261116`의 `CONFIG_AND_BLUEPRINTS.json`, `MOUNTS_PIE.json`, `Automation/index.json`, `FINAL_VERIFICATION.json`을 따른다.


## 후속: 차량 선택 월드 배치 설정 분리 — 2026-11-16

기존 `MWUCMTitleFlow.cpp::FitVehiclePreview()`는 차량 선택 WBP 안의 이름을 직접 찾아 카드 좌표·크기를 바꾸고 안내 위젯을 현재 위치에서 70씩 위로 옮겼다. 같은 인스턴스에 다시 호출하면 안내 위치가 누적될 수 있는 구조였다. 실제 사용자 세션에서 이 누적 현상을 재현했다고 주장하지 않는다.

이제 Flow는 `SetWorldPresentationLayout(true, VehiclePreviewBoardScale)`로 월드 표시만 요청한다. 내부 배치는 기존 `UMWVehicleSelectionWidget`이 담당한다. 새 Subsystem·Tick·타이머는 추가하지 않았다.

### 배치 수정 위치

`Config/DefaultGame.ini`의 `[/Script/MW.MWVehicleSelectionWidget]`에서 다음 월드용 값을 편집한다. 기존 WBP의 Designer 배치와 구분하며 런타임 인스턴스에서 설정을 바꾸는 용도가 아니다.

| 설정 | 현재 값 | 의미 |
|---|---|---|
| `WorldCardGroup` | `AlignmentCardGroup` | 크기·위치를 바꿀 기존 카드 영역 |
| `WorldCardPosition` | `(60, 210)` | 기존 1920×1080 캔버스의 카드 위치 |
| `WorldCardSize` | `(1800, 440)` | 월드 게시판에 표시할 카드 영역 크기 |
| `WorldShiftedWidgets` | 기존 안내·장식 위젯 10개 | 이동할 위젯 이름 목록 |
| `WorldLegendOffset` | `(0, -70)` | 최초 위치 기준 이동량 |
| `WorldCardMarkerScale` | `0.58` | 기존 카드 안 표시 배율 |

카메라·월드 게시판의 `VehiclePreviewBoardScale=1.33`은 그대로 둔다. 커서 보정도 기존처럼 그 역수를 사용한다. 서버 참가자 색, 공유 커서 투영·동기화 함수, 선택·취소 요청은 수정하지 않았다.

### 적용·복원 방식

- 첫 적용 때 바꿀 Canvas 슬롯의 위치·크기와 커서·카드 표시의 원래 배율을 저장한다. 반복 적용은 아무것도 더 움직이지 않는다.
- 해제, 위젯 교체, 화면 표시로의 전환, `NativeDestruct()`에서 저장값을 복원한다. 원래 카드 영역은 위치 `(470, 300)`, 크기 `(980, 240)`이다. WBP 파일에 월드용 값을 덮어쓰지 않는다.
- 커서의 현재 마우스 위치는 되감지 않는다. 배율·Pivot·Alignment만 복원한다. 카드가 재생성되면 새 카드의 원래 표시 배율도 보관한다.
- 저장 대상은 약한 참조로 보관하고 파괴된 대상은 건너뛴다. 무효 배율·배치 값 또는 없는 카드 영역이면 원래 배치를 유지하고 경고를 남긴다.

### 이번 변경의 검증과 한계

- Editor Development 빌드 성공: 15.13초. Game Development 빌드 성공: 51.24초. Editor 첫 시도에서 새 지역 변수명이 UWidget 멤버와 겹쳐 컴파일 오류가 났으며 이름 수정 후 재빌드했다.
- 실제 차량 선택 WBP Dev·Preview 및 타이틀 Flow BP 3개 컴파일 성공. BP·맵 저장 없음.
- 원본 MW의 실제 `L_Title` PIE와 UCM 카메라에서 5개 카드 표시를 확인했다. 기존 `-MWConnectionLocal`과 개발 Preview 시나리오를 사용했으며 EOS·멀티클라이언트 검증을 대신하지 않는다.
- 변경 전후 위치·크기·앵커·정렬·배율·Pivot이 일치했다. 움직이는 커서의 현재 위치는 비교에서 제외했다. 반복 적용, 해제·재적용, 두 번 닫기·재진입, 카드 재생성 후 복원, 잘못된 배율 거부도 통과했다.
- CUA 실제 좌클릭을 시도했지만 이 개발 Preview 실행에서는 선택 확정이 확인되지 않았다. 클릭 성공 또는 전체 선택·취소 회귀 통과로 보고하지 않는다. 변경 전 시도에서도 확정이 관측되지 않았지만 원인을 확정하지 않았으므로 기존 버그라고 단정하지 않는다. 입력·서버 코드를 추가 수정하지 않았다.
- 외부 스크립트의 첫 기준 PIE는 시간 제한으로 끝났지만 배치 스냅샷·화면은 확보했다. 추가 검증 중 `EditDefaultsOnly` 설정의 인스턴스 편집 시도가 거부된 실행도 보존하며, 정상 재실행 결과와 구분한다.
- 수정 대상 외 보호 파일 345개의 SHA-256과 같은 cpp 안 팀원 공유 커서 함수가 유지됐다. 기존 미추적 문서 2개는 그대로 둔다. UVCS·Push·PR 작업은 하지 않았다.
- 프로젝트 밖 `outputs/VEHICLE_LAYOUT_20261116`의 `LAYOUT_COMPARISON.json`, `FINAL_VERIFICATION.json`, `CONFIG_AND_BP.json`, `AFTER_PIE.json`, `AFTER_INPUT_FINAL_PIE.json`, `BEFORE_INITIAL.png`, `AFTER_INITIAL.png`에서 근거를 확인한다. 실패·중단 기록은 성공 근거와 분리한다.

## 후속: 차량 선택 실제 클릭·취소 검증 — 2026-11-16

### 원인과 수정

앞 절에서 미검증으로 남겼던 선택 입력을 다시 조사했다. 차량 게시판의 충돌 Trace는 `PreviewSurface`에 도달했지만, Slate 입력 이벤트는 투명한 전체 화면 `WBP_TitleRoot_UCM_Dev`에 막혔다. 루트의 빈 영역이 월드 Widget보다 먼저 클릭을 받았다.

우리 `UMWUCMTitleRootWidget::OnEntryPresentationUpdated()`에 3줄을 추가했다. 기본 루트가 표시 상태를 갱신한 다음, `Visible`인 루트만 `SelfHitTestInvisible`로 바꾼다. 빈 영역은 입력을 통과시키고 로그인 입력칸·버튼 같은 자식은 계속 입력을 받는다. 기본 클래스의 갱신이 표시 상태를 다시 지정하므로 생성 시점 한 번만 바꾸는 시도는 실패했으며 최종 코드에서 제거했다. Kevin의 기본 루트·선택 RPC·서버 승인·공유 커서 코드는 수정하지 않았다.

### 새로 확인한 결과

- 최종 저장 코드로 새 Editor를 실행한 `L_Title` PIE: CUA 좌클릭으로 Smiet 선택 완료, 실제 우클릭으로 취소 확인. 선택·취소 API를 직접 호출하지 않았다. 외부 관측 스크립트는 개발 Preview 진입과 버튼 이벤트·상태 기록에 사용했다.
- 별도 게임 프로세스 3개를 LocalIP로 연결: CUA로 준비 3명·호스트 시작을 눌러 실제 차량 선택 단계에 진입했다. 클라이언트의 MAMMOTH 선택·우클릭 취소와 서버 차량 ID 변경을 확인했다.
- UFOLOGY 위 비밀 영역은 삭제되지 않았다. 실제 호스트 클릭으로 서버 ID가 `Asset.Vehicle.InvisiblePilot`이 되고 우클릭 후 `None`으로 돌아왔다. 개발용 `bPreviewOnly=true` 모드에서는 기존 조건에 따라 숨겨진다. 실제 방은 같은 Preview WBP라도 `bPreviewOnly=false`로 사용한다.
- 상대 화면에서 빨강·파랑·초록 커서와 닉네임을 확인했다. 호스트에서 2·3번 커서, 2번 클라이언트에서 호스트·3번 커서의 실제 렌더 캡처를 확보했다. 포커스가 다른 앱으로 넘어가면 커서가 숨겨지는 상태와 구분했다. 모든 PC·방향 조합을 검증한 것은 아니다.
- Editor/Game Development 빌드 성공: 15.33초 / 21.13초. 차량 선택 Dev·Preview, 타이틀 Flow, 타이틀 Root BP 4개 컴파일 성공. 저장한 에셋·맵 없음.
- 보호 파일 345개 SHA-256 일치. 소스 변경은 우리 UCM Flow cpp 3줄뿐이다. UVCS·Push·PR 작업 없음.

### 남은 범위

단일 Editor의 다중 PIE에서는 클라이언트 입력을 확인하지 못해 별도 프로세스 실행으로 검증했다. 그 모드의 입력 문제 원인을 엔진 결함으로 확정하지 않는다. EOS 인터넷 연결·다른 PC·Cook·패키징은 이번 검증에 포함하지 않았다. 인비저블맨 선택은 승인되지만 아래 참가자 판의 차량 그림은 비어 있는 상태를 확인했으며 이번 입력 수정 범위에서 변경하지 않았다. 앞 절의 접지 자동 검사 실패도 이번 작업으로 해결한 것이 아니다.

검증 근거는 프로젝트 밖 `outputs/VEHICLE_CLICK_20261116`에 둔다. `Final/CLICK_PIE.json`, `SECRET_SELECTED.json`, `SECRET_CANCELLED.json`, `GAME_OBSERVER_29852.json`, `GAME_OBSERVER_13916.json`, `REMOTE_*.png`, `BLUEPRINT_COMPILE.json`, `EDITOR_FINAL.log`, `GAME_FINAL.log`를 확인한다. 실패한 중간 시도와 최종 저장 코드의 성공 기록은 구분해 보존했다.

## 후속: 인비저블맨 하단 썸네일 연결 — 2026-11-16

선택 요청과 서버 승인은 정상이었지만, 기존 로비의 썸네일 표에는 일반 차량 5대만 있었다. 비밀 차량 카드가 빈 그림을 받아 하단 `Image_Vehicle`이 숨겨지는 것이 원인이었다.

- 우리 `MWVehicleSelectionWidget.h`에 `DefaultVehicleThumbnails` 설정을 추가하고, `CreateVehicleCard()`는 기존 `VehicleThumbnails`에 그림이 없을 때만 이 설정을 읽는다. 기존 로비·WBP의 그림이 우선한다. 차종별 분기·새 Tick·런타임 SceneCapture는 추가하지 않았다.
- `Config/DefaultGame.ini`의 `[/Script/MW.MWVehicleSelectionWidget]`에 `Asset.Vehicle.InvisiblePilot` 그림 한 개만 연결했다. 팀원의 로비·서버·선택 RPC와 차량 정의·BP·맵은 수정하지 않았다.
- 새 로컬 에셋: `Content/UI/Lobby/VehicleSelection/T_MW_VehiclePreview_InvisiblePilot.uasset`. 현재 InvisiblePilot BP의 Pilot 메시와 재질·Idle 포즈를 에디터에서 촬영한 1024×1024 UI 텍스처다. 비압축 UI 설정과 투명 배경을 사용한다. 기존 그림·원본 UCM 에셋은 덮어쓰지 않았다.
- 에셋 SHA-256: `187B1799516495F7CF573F4575CDEFE18B89393FCB931E90B6D68DB56EDD3E9D`.
- Editor/Game Development 빌드 성공: 35.20초 / 47.41초. 차량 선택 Dev·Preview BP 2개 컴파일 및 설정 상속 확인 성공. 새 BP 저장 없음.
- 실제 CUA 마우스로 LocalIP 호스트·클라이언트 각각 비밀 영역을 선택하고 우클릭 취소했다. 양쪽 화면의 올바른 썸네일 Brush·선택 완료·취소 후 그림 숨김을 확인했다. 일반 차량 5종도 각각 클릭해 기존 그림 경로와 화면이 일치함을 확인했다. 스크립트에서 선택·취소 RPC를 호출하지 않았다.
- 이번 실제 입력 검증은 같은 PC의 별도 `UnrealEditor -game` 프로세스 2개다. PIE·다른 PC·EOS 인터넷·Cook·패키징을 새로 검증한 것으로 표시하지 않는다. 기존 `/Game/UI/Lobby` AlwaysCook 설정은 유지했으며 새 패키징은 실행하지 않았다.
- 보호 파일 345개 SHA-256 유지. 최초 촬영용 명령줄 실행의 셰이더 대기와 텍스처 색공간 변경 중 assert는 실패 기록으로 보존했다. 최종 에셋은 일반 에디터 촬영→PNG 임포트로 생성했고, 새 실행에서 정상 로드·표시됐다. 관측 스크립트의 Python API 이름 오류도 수정 후 정상 기록과 구분한다.
- Content는 기존 Git 제외 정책을 유지한다. 이 커밋에는 코드·설정·이 기록만 포함한다. 다른 PC에서 사용하려면 위 신규 `.uasset`을 별도 에셋 전달에 포함해야 한다. UVCS 명령·Push·PR은 실행하지 않았고 공개 배포 권한을 확인한 것으로 취급하지 않는다.

외부 근거: `outputs/INVISIBLE_THUMBNAIL_20261116/CONFIG_AND_BP.json`, `HOST_SECRET_SELECTED.json`, `CLIENT_SEES_HOST_SECRET.json`, `CLIENT_SECRET_SELECTED_18960.json`, `CLIENT_SECRET_SELECTED_5196.json`, `CLIENT_CANCELLED_*.json`, `HOST_*_SELECTED.json`, `SCREEN_18960_0.png`, `SCREEN_5196_7.png`, `EDITOR_BUILD.log`, `GAME_BUILD.log`. 숨김 검증 프로세스 2개는 사용자의 명시적 승인 후 종료했고, 실제 검증 창은 CUA로 정상 종료했다.

## PR 준비 점검 — 2026-11-16

- 등록할 제목: **최신 main 기반 UI·아이템·차량 시각·3테마 통합**. 본문은 저장소의 `.github/PULL_REQUEST_TEMPLATE.md` 항목을 그대로 사용한 [PR_DRAFT.md](PR_DRAFT.md)다. 과거 설명서는 PR #11 당시 기록임을 표시했다.
- 원격 fetch 후 기준 main은 계속 `ed6cbe3de1d6eb5305902f33229f9fab3fecc917`, 검증 구현은 `dc44603aea9408f5146616c142e22c44bcd90915`다. main은 HEAD의 조상이고 가상 병합 결과는 HEAD 트리와 같다. 작업 트리 병합·브랜치 전환·코드 충돌 해결은 필요하지 않았다.
- 구현 6커밋 기준 134개 변경 경로(수정 37·추가 71·이동 14·삭제 12)다. 옛 UI 경로 이동을 포함한 수치다. 이번 PR 본문·인계 CSV가 추가되면 전체 경로는 136개다. 이전 미병합 PR #11과 race-hud의 기능도 포함하므로 최근 설정 분리만의 PR로 설명하지 않는다.
- `MWVehiclePawnNetwork.cpp`, `MWVehicleNetworkTypes.h`, `MWVehicleDriveData.h/.cpp`는 main과 같은 Git blob이다. 접지·다운포스·Snapshot 적용 등을 포함한 핵심 이동 함수 27개도 내용이 같다. 부스터·발판 연결부의 기존 변경과 선택 취소·출발 대기용 로비 API는 그대로 diff에 남고 두 팀원이 검토해야 한다. 서버 코드 전체가 main과 같다고 표시하지 않는다.
- [PR_ASSET_HANDOFF.csv](PR_ASSET_HANDOFF.csv)에 코드·설정의 직접 참조와 UI 진입 에셋 52개의 로컬 경로·크기·SHA-256·소비자를 기록했다. Asset Registry의 hard/soft package 및 management 참조를 따라간 971개 패키지에서 로컬 누락은 없었다. 개발·호환 경로도 포함한 목록이며 전부 신규 에셋이라는 뜻이 아니다. 수신 PC의 버전 일치·UVCS 잠금/예약·공개 배포 권한은 확인하지 않았다.
- 주요 BP 20개를 이번 점검에서 저장 없이 컴파일했고 모두 성공했다. 차량 6종·파일럿 ABP·선택 Dev/Preview·타이틀·폼·로비·HUD·교통 BP를 포함한다. 컴파일 전후 디스크 해시는 유지됐다. Editor/Game 빌드와 실제 선택·취소는 직전 dc44603 구현 검증 결과를 사용하며, 이번 문서 작업에서 재실행한 것으로 기록하지 않는다.
- 접지 자동검사 1개의 위쪽 충격 assertion 2건과 테스트 맵 `VehicleGround` 설정은 차량·맵 담당자가 확인할 항목이다. 관련 함수·검사 본문이 main과 같지만 main 단독 실행에서 같은 실패를 재현하지는 않았다. 이번 PR 준비에서는 접지·보정·Snapshot·검사 코드를 수정하거나 실패를 숨기지 않았다.
- 원격 승강기·최종 전체 매치·다른 PC·인터넷 다중 접속·Cook/패키징은 별도 미검증이다. 이전 타이틀 헤더의 줄 끝 공백 160건은 사용자의 서식 보존 지시에 따라 유지했다. 기능 오류나 병합 충돌로 집계하지 않는다.
- GitHub 조회에서 PR #11은 closed/merged=false이고 #12는 현재 main으로 병합됐다. `fix/frontend-main` head 검색 결과 등록된 PR은 없었다. 새 PR·리뷰 요청·Push·UVCS 작업을 실행하지 않았다. 독립 검토·에셋 인계·남은 검사 판단 전에는 병합 완료나 전체 검증 통과로 표시하지 않는다.

외부 근거: `outputs/PR_READINESS_20261116/GIT_AUDIT.json`, `ASSET_AUDIT.json`, `BLUEPRINT_READINESS.json`. 전체 package 참조 간선은 외부 Asset Registry 기록에 보존하고, Git에는 인계에 필요한 진입 에셋 목록만 포함한다.

## 후속: 남은 조정값 설정 분리 — 2026-11-16

기존 숫자와 목록을 C++에서 `Config/DefaultGame.ini`로 옮겼다. 값을 바꾼 작업이 아니다. 이후 설정을 고치고 게임을 다시 실행하면 C++ 재빌드 없이 조정할 수 있다. WBP Designer로 이전한 작업과는 구분한다.

| 대상 | 이전 | 지금 수정하는 위치 | 소비 코드 |
|---|---|---|---|
| 일반 차량 표시 순서 | `RefreshVehicles()`에 5개 태그 나열 | `MWVehicleSelectionWidget.VisibleVehicleAssetIds` | 설정 순서를 읽고 중복·빈 태그를 제외한 뒤 기존 카탈로그로 카드를 만든다. 비밀 차량 경로는 유지한다. |
| 아이템 슬롯·대기판 치수 | Slate의 크기·여백·글자 크기 숫자 | `MWGameplayHUDSubsystem.ItemSlotLayout`, `WaitingBoardLayout` | `InitializeLayout()`에서 생성 시 한 번 복사·검증한다. 매 프레임 설정 로드나 Widget 재생성은 추가하지 않았다. |
| 부스터 배율 | 이동 연결부의 `1.25f`, `1.5f` | `MWItemComponent.BoosterSpeedMultiplier=1.25`, `BoosterAccelerationMultiplier=1.5` | `GetActiveItemBooster()`가 현재 Pawn의 활성 아이템을 읽고 두 getter로 배율을 가져온다. |

- 기본 카드 순서 SMIET→MAMMOTH→UFOLOGY→DOUG→FRED, 슬롯 168·최소 96px·아이콘 158, 대기판 1152×676, 룰렛 45~990ms·사용 200ms를 유지했다. 설정 치수가 유효하지 않으면 기존 배치로 복구한다.
- 부스터 배율은 클라이언트와 서버에 같은 Config를 배포해야 한다. 잘못된 배율은 `BeginPlay()`에서 한 번 경고하고 해당 값을 1로 복구한다. 서버 승인·복제 필드·효과 종료 시각·후진 제한·초과 속도 감속은 변경하지 않았다.
- main 이동 핵심 함수 27개, 네트워크·Snapshot·DriveData 파일 4개는 그대로다. 세 소비 함수의 새 getter를 기존 상수로 치환하면 변경 전 함수와 일치한다. 아이템 승인·사용·복제·종료 함수 9개도 내용이 같다.
- SVG의 원본 168×176 좌표와 공용 디자인 캔버스, 색상·모양·이름표 등의 나머지 표현 코드는 이번에 이동하지 않았다. 모든 UI 하드코딩이나 Slate 의존성이 제거됐다고 설명하지 않는다.

### 이번 실행 결과

- 최종 Editor/Game Development 빌드 성공(26.55초 / 25.24초). 필수 BP 20개 저장 없이 컴파일 성공. 설정 기본값·기존 BP 상속 검사 40개 성공.
- `MW.Items.PickupAndUse`, `MW.UI.RaceHUD.StateLifetime`, `MW.Vehicle.OverspeedRecovery` 3개 자동검사 성공. 이전 접지 검사 실패를 고친 결과가 아니며 관련 코드는 유지했다.
- 일반 `UnrealEditor -game` 표시 fixture에서 대기판·아이템 슬롯을 1280×720, 1920×1080으로 캡처했다. 크기·여백·글자 및 아이콘 표시를 확인했다. fixture의 차량 열은 카탈로그 로딩 흐름을 만들지 않아 비어 있다. 실제 방 생성·인터넷 접속·실제 마우스 선택을 이번에 다시 통과했다고 표시하지 않는다.
- PIE 대기판 fixture는 초기 GameState 구성·Python API 접근 및 Editor `set` 제한 때문에 실패했다. 그 기록을 보존하고 일반 게임 실행의 표시 검증으로 전환했다. PIE 대기판 검증 성공이나 전후 실플레이 비교로 대체 표기하지 않는다.
- 기존 UI·차량·레벨을 포함한 진입 에셋 52개의 디스크 해시를 보존했다. 새 에셋·플러그인·런타임 검색 Tick을 추가하지 않았다. 기존 미추적 Kevin 비교 문서도 그대로 둔다. UVCS·Push·PR 등록은 실행하지 않는다.

### 캡처 중 확인한 개발 맵 바닥 겹침

`/Game/Developers/user/L_Dev_Drive_ItemPickup`의 `PersistentLevel.StaticMeshActor_1`(Plane)과 생성된 Snow 바닥이 모두 Z=0이었다. Snow 바닥은 Plane 메시 인스턴스 121개이며 표본 12개의 Z도 모두 0이었다. 이 실행에서 개발 Plane 표시만 숨긴 전후 캡처로 겹침이 사라지는 것을 확인했다.

`DefaultGame.ini`의 기존 `MWEnvironmentGameplay.HiddenBackdropActorPaths`에 그 Actor 경로 1개를 추가했다. 기존 로직이 Environment가 생성됐을 때만 개발 바닥을 숨기고 종료 때 기존 표시 상태를 복원한다. 수동 숨김 없는 새 실행에서도 대기판과 슬롯을 다시 캡처했다. 개발 맵 에셋, 바닥 높이·충돌, Environment 생성기·주행·서버 코드는 수정하지 않았다. 다른 경기 맵의 Z-fighting까지 해결했다고 주장하지 않는다.

외부 근거: `outputs/SETTINGS_CLEANUP_20261116/SETTINGS_AND_BP.json`, `Automation/index.json`, `PRESERVATION.json`, `LAYOUT_GAME.json`, `FLOOR_PROBE.json`, `EDITOR_FINAL.log`, `GAME_FINAL.log`. 실패한 fixture 기록과 최종 일반 실행 기록을 분리해 보존했다.
