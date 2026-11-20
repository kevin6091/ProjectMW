# PR #17 로딩·HUD 복구 수정 및 검증

2026-11-19 · `fix/track-ui-dpi-verification` · UE 5.8 · Windows Development

출발점 `cafc46b`. 3인 PIE와 두 개의 별도 Win64 게임 프로세스에서 설원 `L_Dev_Drive` 진입을 확인했다. GUI 입력과 화면 확인은 Cua Driver로 수행했다. PIE 준비에는 검증 스크립트를 함께 사용했다.

[실행 화면 비교](Images/PR17ArrivalRecovery.jpg)

## 변경 위치와 롤백 단위

| 커밋 | 위치 | 변경 전·후 |
|---|---|---|
| 첫 화면을 그릴 기본 밉과 고해상도 스트리밍을 분리 | `MWArrivalScreen.cpp`의 `CheckMaterials()`와 `Tick()`, `MWArrivalScreen.h`의 `FirstViewQualityWaitSeconds` | 사용 텍스처의 모든 밉을 기다리던 검사를 기본 렌더 준비와 추가 화질 대기로 나눴다. 셰이더 맵, 초기화된 텍스처 리소스 및 `NumNonStreamingLODs` 상주량을 필수로 본다. 추가 대기는 `NumNonOptionalLODs`까지 기본 2초이며, 이후 기존 GPU 프레임 확인을 거쳐 공개한다. 엔진 스트리밍은 그대로 진행한다. 같은 검사에서 참조하는 2D 텍스처가 겹치면 한 번만 확인한다. |
| 출발 월드의 맵 이동 대기에도 별도 제한 시간 적용 | `MWArrivalScreen.cpp`의 `Begin()`, `Tick()`, `Cancel()`, `MWArrivalScreen.h`의 `MapTravelTimeoutSeconds` | 도착 월드의 화면 준비 제한에 앞서 이동 구간을 자체 제한한다. 가림 시작 시 단조 시각을 저장하고 목적지 월드를 확인하면 타이머를 끝낸다. 기본값 45초를 넘겨 출발·전환 월드에 남으면 실제 월드와 목적지를 기록하고 방 나가기 버튼에 포커스를 준다. 에디터나 게임 창이 최소화되면 프레임 확인을 멈추고 복원 시 이동에 걸린 실제 시간을 대조한다. |
| 경기 HUD 패널을 독립 갱신하고 필수 대기판 누락을 즉시 안내 | `MWGameplayHUDSubsystem.{cpp,h}`의 `SMWGameplayHUD` 바인딩 및 `GetWaitingBoardReadiness()`, `MWArrivalScreen.cpp`의 `PreparingRace` 분기 | 공통 반환 때문에 함께 멈추던 대기판·아이템 슬롯·단계 안내 갱신을 패널별 조건으로 분리했다. HUD 생성 및 명단 반영 대기(`Pending`), 실제 표시 가능(`Ready`), 대기판 또는 명단 호스트를 구성할 수 없는 상태(`Unavailable`)를 구분한다. Pawn 없이 경기 준비 화면이 첫 표시 대상일 때만 대기판을 필수로 보고 복구 안내를 띄운다. 바인딩 경고에는 위젯 클래스와 해당 이름 또는 타입을 남긴다. |

두 시간 설정은 Game 설정의 `[/Script/MW.MWArrivalScreen]`에서 조정할 수 있다. 코드는 가능 상태, 수명 및 입력을 담당하고 기존 WBP가 레이아웃을 제공한다. 기존 로비 에셋 선로딩, 출발 가림 완료 보고, 서버 이동 절차, 목적지의 렌더 완료 판정과 입력 복원을 사용한다. 새로 도입한 이동 제한과 기존 도착 화면 준비 제한은 서로 다른 구간에서 작동한다. 복구 버튼은 기존 `RequestLeaveRoom()`으로 연결했다.

세 커밋을 각각 역적용하는 Git 병합을 작업 트리 변경 없이 검사했다. 세 건 모두 텍스트 충돌이 없었다. 아래 실행 결과는 세 수정이 모두 적용된 빌드에서 얻었다.

## 빌드 및 정상 진입

- **에디터와 패키지.** `MWEditor Win64 Development` 빌드 성공. UAT `BuildCookRun`으로 `MW Win64 Development` 빌드·쿠킹·스테이징·Pak/IoStore·패키징 성공. 기존 `UMovieScene::GetBindings()` C4996 경고가 남아 있다.
- **3인 PIE.** 로컬 리슨 호스트와 두 참가자에서 실제 뷰포트 960×540 / 1280×722 / 1600×902, UI DPI 배율 약 0.500 / 0.668 / 0.835. 서버는 플레이어 1·2·3의 출발 보고를 각각 한 번 받아 `ServerTravel`했다. `covered`부터 같은 시도의 `reveal`까지 세 창은 4.46~4.63초였다. 공개 카메라는 `MWTrackSelectionBoard_0`, 선택 세션과 후보 버전은 모두 1이었다. Cua Driver로 각 창에서 트랙을 설치했고 서버에 멤버별 조각 2·3·4가 기록됐다. 세 창이 `Racing`에 들어간 것을 화면과 상태 로그에서 확인했다.
- **일반 패키지.** 호스트와 루프백으로 연결한 별도 참가자에서 차량 선택 후 각각 설원 트랙 선택이 열렸다. 가림부터 공개까지 호스트 8.34초, 참가자 2.78초였다. 이 세션의 확인 범위는 트랙 선택 화면 공개까지다. 참가자는 새 게임 프로세스였다. OS·디스크 캐시 초기화는 수행하지 않았다.

## 리뷰 항목별 재현

| 조건 | 3인 PIE | Win64 패키지 |
|---|---|---|
| 고해상도 밉이 대기 시간 내에 안 들어옴 | 정상 후보 경로에 스트리밍 풀 64MB, 밉 바이어스 4를 설정해 세 창 모두 공개. 공개 직후 및 2초 뒤 엔진 캡처에서 보이는 설원 첨탑·바닥과 후보가 회색 대체 재질로 바뀌는 현상은 관찰되지 않았다. 아래 `BuildSign` 케이스의 주행 창 두 곳에서 실제 밉 부족으로 2.02초 뒤 본 장면을 공개했다. | 일반 패키지의 참가자에 풀 32MB, 밉 바이어스 4 적용. 설원 바닥의 상주 밉 11개, 추가 대기 목표 12개에서 2.01초에 `quality fallback`이 기록됐다. 약 1.07초 뒤 같은 시도의 트랙 선택 공개 로그와 Cua Driver 화면을 확인했다. |
| 목적지 맵에 못 들어가고 출발 월드에 남음 | 제한을 2초로 줄이고 P3 로컬 출발 연출을 시간 배율 0.05로 지연. 가림 표시를 완료한 P1·P2는 약 2.04초 뒤 `L_Title`에서 이동 실패 문구와 방 나가기 버튼을 표시했다. | 2초 제한을 준 호스트와 3600ms 인위적 지연을 준 참가자로 재현. 호스트가 출발 월드 `L_Title`에서 가림 시작 2.02초 뒤 `map travel timeout`을 기록했고 Cua Driver로 해당 복구 화면을 확인했다. 별도 반복에서 2.03초에 같은 안내를 확인했고 방 나가기 입력 뒤 타이틀이 다시 열렸다. |
| 필수 대기판 에셋을 로드할 수 없음 | 로컬 HUD 설정의 `MatchPanelsClass`에 존재하지 않는 경로 지정. 대기 상태를 보기 위해 트랙 후보를 비우고 호스트 패킷 지연 650ms를 적용했다. Pawn 없이 도착한 두 창은 `waiting board asset/binding` 복구로 진입했다. 차량을 소유할 때 도착한 나머지 창은 주행 화면을 공개했다. | 경기 준비 직행 데이터만 담은 검증용 패키지와 존재하지 않는 `MatchPanelsClass`를 사용했다. 참가자의 1800ms 패킷 지연으로, Pawn보다 먼저 진입한 호스트가 맵 로드 뒤 약 0.18초 만에 필수 대기판 실패를 보고했다. Cua Driver로 방 나가기를 눌러 타이틀 복귀까지 확인했다. |
| 선택 바인딩 `BuildSign`만 누락 | 대기판과 아이템 슬롯을 가진 임시 WBP에서 `BuildSign` 조회 이름만 제거했다. 세 창에서 바인딩 경고를 확인했으며, 호스트는 `PreparingRace`(`phase=7`)의 관전자 카메라에서 공개했다. 이후 세 창의 주행 HUD에서 아이템 슬롯과 계기판 표시를 확인했다. | 바인딩 하나만 누락한 케이스는 PIE에서 재현했다. |

패키지에서 실제로 측정한 고화질 미준비 조건은 **설치된 밉 중 하나가 상주하지 않은 상태**다. 선택 고해상도 청크를 실제로 뺀 후속 검증은 문서 아래에 기록했다. 일반 실행에서 사용한 대기판은 이 PC에 인계된 `/Game/UI/Race/WBP_MatchPanels`이며 SHA-256은 `C760335644A94389BCBB98B60F0F6B928C44AD3A793EEE3DDC5AB231B271A422`이다. 이 에셋은 Git 추적 밖의 기존 인계 대상이다. 팀원 PC 수신 상태는 별도로 확인해야 한다.

필수 대기판 패키지 검증은 일반 패키지와 SHA-256이 동일한 C++ 실행 파일에 `BP_MWGameplayGameMode`의 건설 후보 배열만 비워 별도 쿠킹했다. 원래 `BP_MWGameplayGameMode.uasset`은 복원 전 백업과 복원 뒤 파일의 SHA-256이 `DC4C06640563F99B7EC4D36C3B21B8E8589CCCFCCAEF93A2A0B5D5249A4888EC`로 일치했다. PIE용 임시 위젯도 디스크에 저장하지 않았다. 구성 파일과 운영 WBP는 커밋에 포함하지 않았다.

패키지 두 창이 열린 상태에서 별도의 실패 검증용 3인 PIE를 함께 시작했을 때 GPU 메모리 부족으로 한 차례 에디터가 종료됐다. 두 종류의 실행을 순서대로 다시 진행해 위 결과를 얻었다.

## 추가 검증 · 선택 고해상도 밉을 실제로 제외한 패키지

[대조군·제외 패키지·선택 입력 캡처](Images/PR17OptionalMipVerification.jpg) · [컨테이너 목록과 실행 로그](PR17_OPTIONAL_MIP_VERIFICATION_LOG.txt)

같은 실행 파일을 사용해 Win64 Development의 설원 방을 호스트와 별도 참가자에서 실행했다. 복사한 프로젝트에서만 Windows의 `TEXTUREGROUP_World`에 `OptionalMaxLODSize=1024`를 적용해 2048×2048 설원 바닥 텍스처 `T_T_snowFloor_6db453bbd5f8`의 고해상도 데이터를 선택 밉으로 만들었다. `-GenerateOptionalPakForNonChunkedBuild`로 분리하고 대조군은 모두 포함했다. 검증용 참가자 복사본에는 선택 파일만 포함시키지 않았다. 운영 프로젝트의 에셋·설정·코드는 그대로다.

| 패키지 | 실제로 제외한 파일과 확인 방법 | 결과 |
|---|---|---|
| IoStore | `MWoptional-Windows.utoc`, `.ucas`, `.pak` 제외. `UnrealPak -ListContainer`에서 제외된 컨테이너에 `OptionalBulkData` 116개와 설원 바닥 청크 `483e5ff7ef71d05b00000003`가 있었다. 실행용 복사본의 기본 컨테이너에는 선택 청크가 0개였다. 기본 컨테이너에 설원 바닥의 필수 데이터와 `L_Dev_Drive`가 남아 있고, 대조군과 필수 파일·실행 파일 해시가 같다. | 트랙 선택이 열렸고 Cua Driver로 목재 직선 트랙을 확정했다. `MWArrival reveal`의 카메라는 `MWTrackSelectionBoard_0`, 단계는 4, 선택 세션/후보 버전은 1/1이었다. 가림에서 공개까지 참가자 1.96초, 대조군 호스트 1.85초였다. 두 창 모두 `MWArrival failed`가 없었다. |
| Pak | `MWoptional-Windows.pak` 제외. 제외 파일 목록에서 설원 바닥 `.uptnl`을 확인했다. 참가자 복사본의 메인 Pak과 느슨한 파일에는 `.uptnl`이 없고, 메인 Pak의 설원 바닥 `.uasset/.ubulk/.uexp`와 맵은 유지했다. | 트랙 선택이 열렸고 Cua Driver로 목재 직선 트랙을 확정했다. 가림에서 공개까지 참가자 1.60초, 대조군 호스트 3.80초였다. |

트랙 선택 입력, UI와 멀티플레이 단계 전환은 PIE에서 확인할 수 있다. 이번 가설은 쿠킹에서 분리된 선택 밉이 **설치 파일에 실제로 없을 때**의 동작을 묻는다. 일반 PIE는 에디터의 에셋을 읽으므로 컨테이너 누락과 동일한 조건을 만들기 어렵다. 앞선 스트리밍 풀 제한 실험은 설치된 밉이 아직 상주하지 않은 경우를 확인했고, 이번 패키지는 선택 밉이 디스크에 없는 경우를 확인했다.

### 이전 검증에서 로비로 돌아간 이유

이전 3인 PIE 실패 재현에서만 P3의 로컬 출발 연출 속도를 0.05로 낮췄다. 서버는 P1·P2의 완료 보고를 받았고 P3는 기한 안에 보고하지 않았다. P1·P2에서는 테스트에만 적용한 2초 이동 제한으로 `map travel timeout`이 기록됐다. 이어 서버의 기본 12초 출발 기한에 `Remaining=1`과 `Start failed: 일부 참가자의 출발 연출이 끝나지 않았습니다.`가 나왔다. 그 실행에는 `ServerTravel`이 없었다. 로비 GameMode의 `FailStart()`가 `RequestLeaveRoom()`을 호출해 방을 종료하고 세 창을 타이틀 맵 `L_Title`로 되돌렸다. 맵 파일을 생성하는 단계는 없다. 방에서 선택한 테마는 이미 존재하는 `L_Dev_Drive`를 가리킨다.

일반 플레이에서 비슷하게 돌아온 사례를 구분하려면 서버의 `[MWTravel]`과 `Start failed:` 직전 로그가 필요하다. 완료 보고 뒤에도 `ServerTravel()` 호출 자체가 실패하면 서버가 같은 정리 경로를 사용한다. 월드 이동 중 엔진 오류는 연결 서브시스템의 이동 실패 처리로 이어진다. 이번 두 패키지 실행에서는 완료 보고와 실제 맵 이동을 확인했다.

### 사각형 개발 구역과 출발 트랙

방에서 설원을 선택하면 기본 경기 레벨은 `/Game/Levels/L_Dev_Drive`다. 이번 트랙 선택 캡처에서도 화면 아래쪽에 보이는 사각 구역은 이 레벨의 개발용 오브젝트다. 구역을 이루는 `Cube`, `Cube2`, `Cube3`, `Cube4`가 레벨에 저장돼 있다. 읽기 전용으로 맵을 열어 네 벽의 위치와 `BP_MWPlayerStartManager`의 출발 화살표를 확인했다. 출발 슬롯은 `Z=6035`cm이고 사각 구역 벽의 중심은 `Z=120`cm다. 별도 차량 이동 테스트 맵 `L_VehicleMovement_Test`는 PIE 계기판 확인 대상으로 등록돼 있다.

정상 경기에서는 전원 트랙 선택 뒤 배치가 시작되면서 서버가 출발 그리드에 `WoodenCapped` 시작 트랙을 놓는다. 이후 설치한 코스를 모든 참가자가 받으면 차량을 출발 슬롯에 생성한다. 정상 3인 PIE에서는 목재 시작 트랙과 체크무늬 출발선 위에서 주행에 들어갔다. 이전 대기판 검증용 직행에서는 임시로 `ConstructionTrackDefinitionIds`를 비웠다. 이때는 코스 생성 없이 같은 높은 위치에 차량이 생성되므로 아래의 개발 구역이 보일 수 있다. 원래 GameMode 에셋에는 후보가 3개 있는 것을 재확인했다. 사용자가 본 화면이 이 직행 캡처라면 배치를 건너뛴 조건으로 설명된다.

## 정상 3인 PIE · 차량 선택부터 출발선까지

2026-11-19 · UE 5.8 Win64 Development Editor · 로컬 코드 `85c8603` · 설원 · LocalIP 리슨 호스트 1명과 참가자 2명. 세 게임 뷰포트는 각각 1152×650이었다. Cua Driver로 PIE 시작, 전원 준비, 호스트 시작, 차량과 트랙 선택, 세 명의 좌클릭 설치를 수행했다. 트랙 후보 3개를 그대로 사용했고 패킷 지연·시간 배율·이동 제한·스트리밍 설정에 테스트용 값을 적용하지 않았다. 관측 스크립트가 읽은 시간 배율은 세 창 모두 1.0이다.

[세 창 비교 캡처](Images/PR17NormalThreePIE.jpg) · [서버 로그와 창별 상태 기록](PR17_NORMAL_THREE_PIE_LOG.txt)

| 참가자 | 차량 | Cua Driver로 설치한 조각 | 주행 진입 시 차량 위치(cm) |
|---|---|---|---|
| P1 호스트 | FRED | 조각 2, 시작 조각 1의 포트 0에 연결 | `(-2620, 375, 6069.2)` |
| P2 참가자 | DOUG | 조각 3, 조각 2의 포트 1에 연결 | `(-2620, 525, 6046.2)` |
| P3 참가자 | SMIET | 조각 4, 조각 3의 포트 1에 연결 | `(-2870, 375, 6051.2)` |

1. 차량 확정 뒤 서버는 P1·P2·P3의 출발 완료를 한 번씩 받아 `ServerTravel`로 이동했다. 세 창은 `L_Dev_Drive`의 트랙 선택 카메라를 공개했다. 같은 시도의 가림과 공개를 대조한 소요 시간은 4.156~4.476초였다.
2. 세 창에서 목재 직선 트랙을 선택하고 실제 설치했다. 서버에는 목재 시작 트랙, 참가자별 설치 조각 3개, 종료 트랙이 기록됐다. P3의 마지막 설치 직후 배치 완료 안내와 완성 코스를 확인했다.
3. 세 창이 `Racing`에 도달했고 각 참가자가 선택한 차량을 소유했다. 캡처에서 목재 시작 트랙의 체크무늬 출발선 위에 세 차량이 표시됐다. 별도 차량 테스트맵은 이 실행의 월드 목록에 들어 있지 않았고, 차량 높이도 아래쪽 개발 구역과 구분된다.
4. 주행 진입 뒤 141.31초 시점까지 세 창이 `L_Dev_Drive`의 `Racing`을 유지했다. `ServerTravel` 이후 관찰 구간에서 로비·타이틀로의 복귀, `Start failed`, 이동 시간 초과, `MWArrival failed`는 기록되지 않았다.

초기 관측 스크립트에 Unreal Python 속성명이 맞지 않는 문제가 있어 먼저 수정했다. 정상 실행 도중에도 배치 구조체를 읽는 관측 부분을 고쳐 같은 세션에 다시 연결했다. 로그와 캡처의 플레이 입력은 Cua Driver가 수행했고, 관측 스크립트는 단계·화면 크기·차량·코스를 읽었다. 후반 캡처의 빨간 영문 줄은 에디터의 GPU 메모리 예산 경고다. 세션과 차량은 관찰 종료까지 유지됐다.

### PR #17 병합 보류 사유에 대한 검증 판정

리뷰 기준은 PR #17의 `49261f5`다. 이번 3인 PIE는 세 수정이 들어간 로컬 코드에서 **정상 경로가 유지되는지** 검증한다. 아래의 결함 재현 결과와 함께 봐야 세 지적에 답할 수 있다.

| 리뷰 사유 | 앞선 재현 결과 | 이번 정상 3인 PIE가 확인한 범위 |
|---|---|---|
| P1 · 선택 고해상도 밉 미준비 때문에 트랙 선택이 막힘 | 스트리밍 제한으로 밉 대기 종료를 재현했다. 선택 밉 청크를 디스크에서 실제로 제외한 Pak 및 IoStore 두 방식에서 트랙 선택이 열리고 입력이 확정됐다. | 일반 조건에서도 세 창이 준비된 트랙 선택 화면을 열고 배치로 진행했다. |
| P2 · 맵 이동이 시작되지 않으면 로딩 제한이 동작하지 않음 | 출발 완료 보고를 지연하고 검증용 2초 제한으로 출발 월드에 남는 경로를 만들었다. PIE와 패키지에서 이동 실패 화면을 확인했고, 패키지에서 방 나가기로 타이틀에 복귀했다. | 정상 보고와 이동에 새 제한이 개입하지 않았다. 세 창의 주행 진입과 관찰 종료까지 복귀가 없었다. |
| P2 · HUD 바인딩 하나가 다른 패널까지 막음 | `BuildSign` 누락 PIE에서 대기판과 아이템 슬롯이 각각 갱신됐다. 별도 재현에서 필수 `MatchPanels` 누락 시 PIE와 패키지가 복구 안내를 띄웠다. | 현재 PC에 인계된 WBP로 정상 주행 HUD와 아이템 슬롯이 보였다. 팀원 PC의 WBP 수신·해시·동일 실행은 별도의 확인 항목이다. |

판정은 **세 리뷰 지적에 대응하는 로컬 검증을 확보했고 이번 정상 회귀도 통과**다. PR이 실제로 병합 가능한 상태인지는 수정 커밋이 반영된 PR과 인계 에셋을 받은 팀원 환경을 기준으로 판단해야 한다. 이번 작업은 문서·캡처·로그를 로컬에 남겼다.

## 기본 45초 이동 제한과 UVCS 인계 대조

### 서버 종료 신호가 없는 출발 월드

UE 5.8 Win64 Development Editor의 LocalIP 3인 PIE에서 실행했다. `MapTravelTimeoutSeconds`는 코드 기본값 45초를 사용했다. 실행 로그와 INI 검색에서 덮어쓰기 값을 확인할 수 없었다. P3의 출발 보고만 늦추고, P1과 P2가 실제 출발 보고를 한 뒤 서버 월드 시간 배율을 0.05로 낮춰 서버의 기존 12초 타이머가 먼저 끝나지 않게 했다. P2의 로컬 월드 시간 배율은 1.0을 유지했다. PIE 밖의 코드, 설정, 맵 및 트랙 후보는 편집하지 않았다.

| 창 | 가림부터 시간 초과 로그까지 | 확인 결과 |
|---|---|---|
| P1 리슨 호스트 | 약 45초 · UTC 로그 차 44.967초 | `source=L_Title current=/Game/Levels/L_Title destination=/Game/Levels/L_Dev_Drive`. 이동 실패 문구와 방 나가기 표시. |
| P2 참가자 | 약 45초 · UTC 로그 차 44.939초 | 같은 출발 월드와 목적지. 같은 안내를 Cua Driver로 확인. |

두 안내가 표시될 때까지 서버 이동, 출발 실패, 연결 종료 로그는 없었다. 로그의 UTC 시각 차는 관측값이며 실제 제한 판정은 `FPlatformTime::Seconds()`로 수행한다. P2에서 방 나가기를 눌러 타이틀 월드 로드 완료를 로그로 확인했다. 이후 시험 스크립트가 종료된 월드를 참조해 PIE가 끝났으므로 타이틀 메뉴가 표시된 뒤의 캡처는 없다.

[가림과 복구 안내 비교 캡처](Images/PR17DefaultTravelTimeout.jpg) · [기본 설정과 실행 로그](PR17_DEFAULT_TRAVEL_TIMEOUT_LOG.txt)

### InvisiblePilot의 해시가 다른 이유

UVCS `/main`의 `cs:59`에서 `BP_MWVehiclePawn_InvisiblePilot.uasset`를 임시 폴더로 읽어 비교했다. UVCS 파일은 34,976바이트, SHA-256 `46ebebbe6d8125add44da5ad0f57639f3d007dfb1788e6921e1475bee83f2146`로 PR 인계 목록과 일치한다. 로컬 파일은 35,429바이트, SHA-256 `b08b6359a43abd31d7c9e4db9e901c4f082d72b9e7b636db928190e8816dae42`, UVCS 상태 `CH`다.

검증 복사본에서 Unreal로 Blueprint와 클래스 기본값을 각각 텍스트로 꺼내 비교했다. 로컬 `Items`의 `bOverrideBoosterMount`가 켜졌고, `BoosterMountTransform`은 위치 `(-42, 0, 4)`cm, Z축 회전 -90°, 스케일 `(0.1, 0.1, 0.1)`이다. 생성된 실행 그래프 GUID도 달랐다. 충돌 박스 치수와 후방 가상 바퀴 두 개의 위치·반지름은 일치했다. 이 diff에서 확인한 동작 차이는 투명차 부스터 장착 설정이다.

### 체크인 전 UVCS cs:59 대조와 인계 계획

| WBP | 팀원에게 전달할 검증본 SHA-256 | UVCS /main 상태 |
|---|---|---|
| `WBP_MWLoading` | `6abf86be047011e3703b792bfee9a996992067a80741b6b01abb5621f5826e97` | 로컬 전용. 신규 등록 필요. |
| `WBP_MatchPanels` | `c760335644a94389bcbb98b60f0f6b928c44ad3a793eee3ddc5ab231b271a422` | 로컬 전용. 신규 등록 필요. |
| `WBP_RaceHUD` | `8f239f6472de2cbf50b0a3fa353cf26269887b000d2d099b74445619ef675035` | `cs:59`와 동일. 그 버전을 받으면 된다. |
| `WBP_VehiclePlayerMarker_Dev` | `241d7defda07884162c470b48d825687fe25a7f58668ad44b756f54bdf3f7498` | `cs:51`과 로컬 내용이 다름. |
| `WBP_VehicleSelection_Dev` | `b85969aae71ac53a22975c0b8b6c89f970d9996d0d3691d4a3f2f1c2a01902f7` | `cs:51`과 로컬 내용이 다름. |

PR 인계 목록 57개를 현재 로컬 파일과 UVCS `/main`에서 대조했다. **47개 동일, 4개 내용 다름, 6개 경로 없음**이다. 같은 데이터인지 판정할 때 UVCS의 내용 해시와 로컬 MD5를 비교했고 위 핵심 파일의 SHA-256은 UVCS 개정 파일을 직접 읽어서도 확인했다. `ItemSlotGlass.svg`는 UVCS 상태에 `CH`로 표시됐지만 `cs:57` 파일과 SHA-256이 같다.

1. **로딩·경기 HUD 변경 세트.** `WBP_MWLoading`, `WBP_MatchPanels`, 대기판이 직접 참조하는 `T_Make_Way_Logo_v1_Pole_49ad24987e58`와 `T_Make_Way__Logo_Small_Sign_5909de5ef398`을 함께 인계한다. 네 파일이 모두 UVCS main에 없다. 기존 `UI_WBP_인계.zip`의 WBP 4개는 위 검증본과 일치하지만 로고 텍스처 두 개는 ZIP에 들어 있지 않다.
2. **차량 선택 UI 변경 세트.** 이전 PR의 차량 카드·커서·월드 프리뷰도 배포할 때 `WBP_VehiclePlayerMarker_Dev`, `WBP_VehicleSelection_Dev`, `BP_VehicleSelectionPreview_Dev`의 세 수정본과 `M_VehicleSelectionBoard`, `T_MW_VehiclePreview_InvisiblePilot`의 두 신규 파일을 묶는다. `WBP_VehicleSelection_Preview` 래퍼는 `cs:31`과 동일하다.
3. **투명차 부스터 변경 세트.** `BP_MWVehiclePawn_InvisiblePilot`의 로컬 부스터 장착 값을 팀에 적용한다면 차량·아이템 담당자 확인과 주행 부스터 검증 후 별도의 변경 세트로 처리한다. 로딩·대기판 검증에는 위 파일의 로컬 부스터 변경이 필요하지 않다.

팀원은 지정된 변경 세트와 대응 Git 코드를 받아 경로·해시를 맞춘 뒤 로딩과 패키지 진입을 재검증할 수 있다. 이번 확인은 UVCS 조회와 검증 사본 다운로드까지 수행했다.

[UVCS 에셋별 대조표](PR17_UVCS_ASSET_AUDIT.csv)

## UVCS 로딩·경기 HUD 인계 완료 · cs:60

2026-11-19 17:38 KST · `/main@ProjectMW/ProjectMW@sms1599@unity` · 변경 세트 **`cs:60`**

체크인 설명은 **경기 진입 로딩·HUD 위젯과 대기판 로고 인계**이다.

팀원이 받아야 할 에셋 개정은 다음 네 파일의 `cs:60`이다. 서버에서 변경 세트의 추가 파일 목록을 읽어 네 파일을 확인했고, 서버 개정을 다시 내려받아 기존 검증본과 SHA-256이 모두 일치하는 것을 확인했다.

| 팀원이 받을 경로 | 개정 | 검증본 SHA-256 |
|---|---|---|
| `Content/UI/Common/WBP_MWLoading.uasset` | `cs:60` | `6abf86be047011e3703b792bfee9a996992067a80741b6b01abb5621f5826e97` |
| `Content/UI/Race/WBP_MatchPanels.uasset` | `cs:60` | `c760335644a94389bcbb98b60f0f6b928c44ad3a793eee3ddc5ab231b271a422` |
| `Content/UI/Common/T_Make_Way_Logo_v1_Pole_49ad24987e58.uasset` | `cs:60` | `f1ba6eb5e7a5de071624a5185871ece66b017ab973817b80f8f2282560644083` |
| `Content/UI/Common/T_Make_Way__Logo_Small_Sign_5909de5ef398.uasset` | `cs:60` | `38131eb337a71b27aca3b246f08684e6fda8679a6b86ecea2b216c7e458c6275` |

계기판 `Content/UI/Race/WBP_RaceHUD.uasset`은 기존 `cs:59`를 사용한다. 위 네 파일은 로컬 Git 코드 `85c8603`과 함께 검증한 에셋이다. 팀원 PC의 수신·패키지 진입은 팀원 환경에서 확인할 항목이다. 차량 선택 UI와 InvisiblePilot은 다음 변경 세트로 보류했다. 이 인계에 Git 커밋과 푸시는 수행하지 않았다.

## UVCS 차량 선택 UI 인계 완료 · cs:61

2026-11-19 17:43 KST · `/main@ProjectMW/ProjectMW@sms1599@unity` · 별도 변경 세트 **`cs:61`**

체크인 설명은 **차량 선택 화면·참가자 커서와 프리뷰 에셋 인계**이다. 서버 변경 세트의 파일 목록에서 수정 3개와 신규 2개를 확인했다. 다섯 파일의 `cs:61` 개정을 다시 내려받아 검증본과 SHA-256을 대조했다.

| 팀원이 받을 경로 | 개정 | 변경 | 검증본 SHA-256 |
|---|---|---|---|
| `Content/UI/Lobby/VehicleSelection/WBP_VehiclePlayerMarker_Dev.uasset` | `cs:61` | 수정 | `241d7defda07884162c470b48d825687fe25a7f58668ad44b756f54bdf3f7498` |
| `Content/UI/Lobby/VehicleSelection/WBP_VehicleSelection_Dev.uasset` | `cs:61` | 수정 | `b85969aae71ac53a22975c0b8b6c89f970d9996d0d3691d4a3f2f1c2a01902f7` |
| `Content/UI/Lobby/VehicleSelection/BP_VehicleSelectionPreview_Dev.uasset` | `cs:61` | 수정 | `63d4fc9928cb7c49d654e0b0505ed62d89b4b62ff3523a7f586dfb13dfc642f0` |
| `Content/UI/Lobby/VehicleSelection/M_VehicleSelectionBoard.uasset` | `cs:61` | 신규 | `cebc5b123e7144f1c97077f159cc1c70790774ae0e6ae5ca292776cb66af8668` |
| `Content/UI/Lobby/VehicleSelection/T_MW_VehiclePreview_InvisiblePilot.uasset` | `cs:61` | 신규 | `187b1799516495f7cf573f4575cdefe18b89393fcb931e90b6d68db56edd3e9d` |

팀원은 UVCS `/main`의 `cs:61`을 기준으로 차량 선택 UI를 받으면 된다. 이 변경 세트는 로딩·경기 HUD를 인계한 `cs:60` 위에 적용된다. `WBP_VehicleSelection_Preview.uasset` 래퍼는 기존 `cs:31` 개정을 사용한다. 이 변경 세트의 InvisiblePilot 관련 파일은 차량 선택용 썸네일 텍스처이며, `BP_MWVehiclePawn_InvisiblePilot.uasset`의 부스터 변경은 보류 중이다. Git 커밋·푸시는 수행하지 않았다. 팀원 PC의 수신 확인은 별도다.

## UVCS 운영 경기 트랙 UI 인계 완료 · `cs:62`

2026-11-20 · UVCS `ProjectMW/ProjectMW`의 `/main` · 별도 변경 세트 **`cs:62`**

체크인 설명은 **운영 경기 트랙 선택·배치 위젯 수정본 인계**이다. 서버 변경 목록과 서버에서 다시 내려받은 파일로 수정 1개와 SHA-256을 확인했다.

| 팀원이 받을 경로 | 개정 | 크기(바이트) | 서버 SHA-256 |
|---|---|---:|---|
| `Content/UI/Construction/WBP_TrackSelectionTest.uasset` | `cs:62` | 75758 | `5c646421f2cda85e90e69869c7b62cd52ac3af837a8ecc12140dce252a00ba1b` |

`WBP_TrackSelectionTest`는 실제 경기와 개발맵에서 함께 사용한다. Cua Driver로 설원 `L_Dev_Drive` 3인 PIE에 진입해, 운영 `BP_MWGameplayPlayerController`가 트랙 선택과 배치 두 단계에서 세 창에 `WBP_TrackSelectionTest_C`를 표시하는 것을 확인했다.

- `cs:61` 스냅샷의 기존 `cs:41` 개정으로도 세 명 모두 트랙을 확정·설치하고 실제 출발선에서 `Racing`에 진입했다. 선택·스냅·설치 안내와 표본의 원격 커서도 동작했다.
- 기존 개정에서는 어두운 안내판이 표시됐고 후보 이름표가 빈 흰 사각형으로 보였다. 배치 안내와 명단의 여백도 빠졌다. `cs:62`에 등록한 파일의 해시는 노란 안내판, 읽을 수 있는 후보 이름표와 배치 여백을 확인한 로컬 검증본과 같다. [트랙 선택 검증](TRACK_SELECTION_MIXED_DPI.md) · [배치 커서·문구 검증](TRACK_PLACEMENT_CURSOR_VERIFICATION.md)

팀원은 `cs:62`를 받으면 앞선 `cs:60`·`cs:61`의 인계 에셋도 함께 받는다.

## 트랙 UI 변경 세트 조합 검증

2026-11-20 KST · Git `0ae39e8` · UE 5.8 · 설원 `L_Dev_Drive` · LocalIP 2인

같은 버전은 호스트와 참가자가 같은 에디터 프로세스의 2인 PIE에서 실행했다. 혼합 버전은 리슨 호스트를 PIE로 실행하고 별도 `UnrealEditor.exe -game` 참가자를 붙였다. 별도 참가자는 공용 콘텐츠·게임 모듈·설정을 호스트와 공유하고, 임시 실행 경로에서 `WBP_TrackSelectionTest.uasset`만 UVCS `cs:61` 스냅샷의 원본을 읽었다. 실행 전에 두 경로의 SHA-256을 확인했다. 두 조합 모두 Cua Driver로 차량 선택, 트랙 확정, 좌클릭 설치까지 진행했다.

| 조합 | 트랙 이름표 | 배치 안내 | 설치와 출발선 |
|---|---|---|---|
| 호스트 `cs:62` · 참가자 `cs:62` | 두 창 모두 목재 경사 트랙, 목재 45도 커브, 목재 직선 트랙 이름표를 표시했다. [호스트](Images/PR17Cs62Pie/matched-host-track-selection.png) · [참가자](Images/PR17Cs62Pie/matched-client-track-selection.png) | 노란 안내판과 안내·명단 사이 여백이 두 창에 표시됐다. [호스트](Images/PR17Cs62Pie/matched-host-placement.png) · [참가자](Images/PR17Cs62Pie/matched-client-placement.png) · [설치 후 대기](Images/PR17Cs62Pie/matched-host-placement-installed.png) | 서버에서 P1의 조각 2와 P2의 조각 3을 확인했다. 양쪽 모두 체크무늬 출발선에서 `Racing`에 진입했다. [호스트](Images/PR17Cs62Pie/matched-host-start-line.png) · [참가자](Images/PR17Cs62Pie/matched-client-start-line.png) |
| 호스트 `cs:62` · 별도 참가자 `cs:61` | 호스트는 후보 이름표 3개를 표시했다. 참가자 창에서만 이름표가 글자 없는 흰 사각형으로 보였다. [호스트](Images/PR17Cs62Pie/mixed-host-track-selection.png) · [참가자](Images/PR17Cs62Pie/mixed-client-track-selection.png) | 호스트는 노란 판과 여백을 유지했다. 참가자는 기존 어두운 판에서 명단 아래에 안내를 표시했고, 설치 완료·대기 안내는 갱신됐다. [호스트](Images/PR17Cs62Pie/mixed-host-placement.png) · [참가자](Images/PR17Cs62Pie/mixed-client-placement.png) · [설치 후 대기](Images/PR17Cs62Pie/mixed-client-placement-installed.png) | 서버에서 P2의 조각 2와 P1의 조각 3을 확인했다. 호스트와 별도 참가자 모두 FRED·DOUG가 실제 출발선에 있는 화면과 `Racing` 로그를 남겼다. [호스트](Images/PR17Cs62Pie/mixed-host-start-line.png) · [참가자](Images/PR17Cs62Pie/mixed-client-start-line.png) |

**판정** · 이 혼합 조합에서는 `cs:61` 참가자의 접속·트랙 확정·설치·주행 진입이 모두 동작했다. 수정된 이름표, 노란 안내판과 배치 여백이 필요한 각 팀원은 `cs:62`를 받아야 한다. [서버·참가자 실행 로그와 에셋 SHA-256](PR17_CS62_PIE_LOG.txt)

## 팀원 수신 검증

### 1. Git과 UVCS 버전 확인

- Unreal Editor를 닫고 ProjectMW 루트에서 실행한다. Git은 PR #17의 `fix/track-ui-dpi-verification`을 사용한다. 기존 검증·인계 기준은 `c3784bf`이고, 절차를 추가한 문서 커밋이 포함된 브랜치를 받아도 실행 코드는 동일하다. 결과에는 실제 사용한 전체 Git SHA를 남긴다.
- UVCS `ProjectMW/ProjectMW`의 `/main`을 `cs:62`에 맞춘다. 이 스냅샷은 `cs:60`·`cs:61`의 에셋과 운영 트랙 WBP를 포함한다. 이미 받은 경우 조회 명령만 실행한다. 다른 버전이라면 UVCS 대기 변경을 확인하고 로컬 작업을 보존한 뒤 `cm update --changeset=cs:62`로 받는다.

```powershell
git rev-parse HEAD
cm status --header
```

### 2. 원본 에셋 10개 SHA-256 대조

에디터에서 에셋을 열거나 저장하기 전에 ProjectMW 루트에서 다음 블록을 실행한다. 대상은 [에셋 인계 목록](../../Reviews/FrontendIntegration/PR_ASSET_HANDOFF.csv)의 `UVCS_CHECKED_IN` 행이다.

```powershell
$manifest = 'Docs/Reviews/FrontendIntegration/PR_ASSET_HANDOFF.csv'
$assets = @(Import-Csv -LiteralPath $manifest -Encoding UTF8 |
    Where-Object { $_.status -eq 'UVCS_CHECKED_IN' })

$results = foreach ($asset in $assets) {
    $actual = if (Test-Path -LiteralPath $asset.local_path -PathType Leaf) {
        (Get-FileHash -LiteralPath $asset.local_path -Algorithm SHA256 -ErrorAction Stop).Hash
    }
    $result = if (!$actual) {
        'MISSING'
    } elseif ($actual -ine $asset.sha256) {
        'MISMATCH'
    } else {
        'OK'
    }
    [pscustomobject]@{
        Result   = $result
        Path     = $asset.local_path
        Expected = $asset.sha256
        Actual   = $actual
    }
}
$results | Format-List
$matched = @($results | Where-Object Result -eq 'OK').Count
"$matched/$($assets.Count) SHA-256 일치"
if ($assets.Count -ne 10 -or $matched -ne 10) {
    throw '인계 에셋 10개의 경로와 SHA-256을 확인하세요.'
}
```

통과 기준은 **`10/10 SHA-256 일치`**다. `MISSING`은 파일 경로, `MISMATCH`는 출력된 기준·실제 해시와 UVCS 개정 및 로컬 수정 여부를 확인한다. 차이가 해결되면 해시를 다시 확인한다. 팀원은 2인 PIE에서 후보 이름표, 노란 배치 안내판과 여백, 트랙 확정·설치 및 실제 출발선의 `Racing` 진입을 확인할 수 있다. 아래 패키지 체크리스트는 배포본을 직접 확인할 때 사용한다.

### 3. Win64 Development 패키지 진입 체크리스트

- [ ] UE 5.8에서 ProjectMW를 열고 **Platforms → Windows → Development → Package Project**로 새 출력 폴더에 패키징했다. 프로젝트의 Pak·IoStore 설정을 사용했고 UAT 로그에 `BUILD SUCCESSFUL` 또는 `ExitCode=0`이 기록됐다.
- [ ] 새 패키지의 `Windows` 폴더에서 호스트와 참가자를 별도 `MW.exe` 프로세스로 실행했다. 호스트가 설원 방을 만든 뒤 같은 PC의 참가자가 아래 주소로 합류했다. 같은 포트를 다른 세션이 사용 중이면 호스트 포트와 참가자 주소의 포트를 함께 바꾼다.

```powershell
# 첫 번째 PowerShell 창에서 실행하고 설원 방을 만든다.
.\MW.exe -MWConnectionLocal -windowed -port=7840 "-abslog=$PWD\PR17-host.log"

# 방을 만든 뒤 두 번째 PowerShell 창에서 실행한다.
.\MW.exe 127.0.0.1:7840 -MWConnectionLocal -windowed -port=7841 "-abslog=$PWD\PR17-client.log"
```

- [ ] 두 창에서 준비하고 서로 다른 차량을 선택·확정했다. 로딩이 표시되고, 가림이 걷힌 뒤 호스트와 참가자 모두 설원 `L_Dev_Drive`의 트랙 선택 화면에서 실제로 트랙을 확정했다. 이 단계까지가 패키지 첫 진입 통과다.
- [ ] 선택과 배치에서 노란 안내판이 보였다. 선택 화면에서 후보 이름표 글자가 읽히고, 배치 화면에서 설치 안내와 참가자 명단 사이에 여백이 유지됐다.
- [ ] 각자 트랙을 실제로 설치했다. 두 차량이 체크무늬 출발선에서 `Racing`에 진입했고, 주행을 시작한 뒤 관찰한 시간 동안 로비나 타이틀로 돌아가지 않았다. 대기판이 표시된 경우 로고와 명단을, 주행 중에는 아이템 슬롯을 확인했다. 대기판이 보이지 않은 실행은 해당 항목을 미관찰로 기록한다.
- [ ] 호스트 로그에서 `ServerTravel`을 확인했다. 각 창에서 `MWArrival covered`와 `MWArrival reveal`이 같은 시도 ID로 기록됐는지 확인했다. 실패 시 어느 창의 어느 단계에서 멈췄는지와 `MWArrival failed`, 이동 실패 또는 관련 에셋 로드 오류를 남겼다.

이 실행은 **팀원 PC의 정상 경로 확인**이다. 기본 이동 제한 45초와 기본 트랙 후보를 사용한다. 테스트용 패킷 지연, 시간 배율 변경, 후보 비우기, 선택 밉 제거는 기존 결함 재현 기록의 조건이다.

### 4. PR에 남길 결과

| 항목 | 기록 |
|---|---|
| 환경 | Git 전체 SHA, UVCS 변경 세트, UE 버전, Win64 Development, 참가 프로세스 수 |
| 에셋 | SHA-256 일치 개수, 불일치 시 경로·기준 및 실제 해시 |
| 빌드 및 첫 진입 | UAT 로그, 호스트·참가자 각각의 트랙 확정 결과와 캡처, 양쪽 실행 로그 |
| 주행 | 출발선 캡처, `Racing` 진입, 관찰 시간과 로비 복귀 여부, 대기판·아이템 슬롯 관찰 여부 |
| 실패 시 | 재현 순서, 해당 창, 마지막 정상 단계, 관련 로그나 캡처 |

팀원 PC의 수신과 PIE 실행 결과는 팀원이 이 절차를 완료한 뒤 채운다. 배포본을 직접 실행한 경우 패키지 결과도 함께 남긴다.

### UVCS에서 별도 체크인하지 않은 파일

체크인 전 [UVCS 대조표](PR17_UVCS_ASSET_AUDIT.csv)는 당시 인계 목록 57개를 기록한다. 그중 47개는 기존 개정과 내용이 같았고, 손볼 10개 중 UI 에셋 9개는 `cs:60`·`cs:61`로 전달했다. 최신 목록에는 운영에서 쓰는 트랙 WBP 1개를 `cs:62`로 추가했다. 계기판 `WBP_RaceHUD`와 차량 선택 래퍼 `WBP_VehicleSelection_Preview`는 각각 기존 `cs:59`와 `cs:31` 개정을 사용한다.

트랙 WBP 체크인 뒤 `cs:62` 워크스페이스에 남은 세 경로는 아래와 같다.

| 로컬 파일 | 확인 결과와 인계 범위 |
|---|---|
| `Content/Gameplay/Vehicles/BP_MWVehiclePawn_InvisiblePilot.uasset` | 팀원은 기존 `cs:59` 차량 BP를 사용한다. 로컬 변경은 부스터 장착 오버라이드와 장착 위치·회전·스케일이다. 차량·아이템 동작에 해당하므로 이번 UI 인계에서 보류했다. 이 값을 적용할 때는 차량·아이템 담당자 확인과 실제 주행 중 부스터 검증을 거쳐 별도 변경 세트로 다룬다. 차량 선택용 썸네일은 `cs:61`에 포함돼 있다. |
| `Content/UI/Common/Slate/ItemSlotGlass.svg` | UVCS에는 `CH`로 표시되지만 로컬과 서버의 `cs:61` 스냅샷이 SHA-256까지 같다. 이미 전달된 기존 `cs:57` 개정을 사용한다. |
| `Content/Gameplay/Vehicles/Art/SM_MW_InvisiblePilot_DrivingCollision.uasset` | 로컬 전용 파일이며 서버의 `cs:62` 스냅샷에 경로가 없다. 현재 차량 이동·충돌 통합은 기존 박스 충돌체를 사용한다. 복구용 충돌 메시의 도입과 차량 검증은 별도 작업으로 남긴다. |
