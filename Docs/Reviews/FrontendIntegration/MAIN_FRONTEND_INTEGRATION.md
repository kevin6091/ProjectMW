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
