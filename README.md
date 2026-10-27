# ProjectMW

플레이어가 함께 트랙을 만들고, 직접 만든 코스에서 경쟁하는 멀티플레이 아케이드 레이싱 게임입니다.

**Make Way**의 건설과 레이싱 흐름을 바탕으로, 플레이어별 3인칭 시점과 차량 조작, 마우스 기반 트랙 배치를 구현하고 있습니다. 한 라운드가 끝나도 코스는 남아 있으며, 다음 라운드에 새로운 조각을 이어 붙여 더 긴 코스를 완성합니다.

> 현재 개발 중입니다. 공용 데이터·입력 기반과 EOS 로그인 코드가 있으며, 방 생성·참가부터 건설·주행·결과까지 이어지는 게임 흐름은 개발 예정입니다. GitHub 소스만으로 실행에 필요한 모든 에셋을 받을 수는 없습니다.

## 게임 흐름

```text
타이틀 → 방 생성 또는 검색·참가 → 로비에서 준비
       → 트랙 선택·배치 → 위험 요소·보조 요소 배치
       → 카운트다운 → 레이싱 → 라운드 결과
       → 기존 트랙 확장 또는 최종 결과
```

- **함께 만드는 코스** — 각자 트랙을 선택하고 배치를 요청하면 서버가 순서대로 확정합니다.
- **차량 조작과 충돌** — 가속·제동·조향·기본 드리프트를 중심으로 조작감을 다듬습니다.
- **아이템과 코스의 변수** — 부스터, 범퍼, 가속 패드와 아이템 박스를 우선 구현합니다.
- **탈락과 복귀** — 추락하거나 선두와 일정 거리 이상 벌어지면 탈락하고, 기본 레이스에서는 유효한 체크포인트에서 복귀합니다.
- **라운드마다 확장되는 경기** — 체크포인트 점수를 쌓고 코스를 늘려 가며, 목표 점수에 따라 최종 승자를 정합니다.

위 내용은 목표로 하는 게임 규칙입니다. 구현 상태는 아래 개발 현황을 참고해 주세요.

## 개발 환경

| 항목 | 내용 |
| --- | --- |
| 엔진 | Unreal Engine 5.8 계열 · 팀 문서 기준 5.8.3 |
| 플랫폼 | Windows |
| 개발 | C++ · Blueprint |
| 입력 | Enhanced Input · Gameplay Tags |
| 온라인 | OnlineSubsystem EOS |
| 멀티플레이 | 플레이어 PC가 호스트가 되는 Listen Server |
| 인원 | 2~4인 설계, 3인 환경 우선 검증 |
| 소스 관리 | GitHub |
| 에셋 관리 | Unity Version Control |
| 팀 구성 | 클라이언트 개발 2명 · 클라이언트/TA 1명 |

## 개발 현황

소스 확인 기준: **2026-10-27 · main · `987d402`**

| 구분 | 현재 상태 |
| --- | --- |
| 에셋 목록·로딩 | `UMWAssetData`, `UMWAssetManager` 구현 |
| 입력 설정 | `UMWInputData`, 운전·건설 입력 태그 구현 |
| 차량 정의 | 차량 ID·생성 클래스·주행 입력을 담는 `UMWVehicleDefinitionData` 구현 |
| EOS 로그인 | `UMWMultiplayerSubsystem`에 로그인 요청·상태 조회·결과 이벤트 구현 |
| 온라인 설정 | EOS 플러그인, 기본 서비스와 EOS NetDriver 설정 추가 |
| 시작 에셋 연결 | `StartupAssetData`는 아직 미지정 |
| 방 생성·검색·참가 | 구현 계획 정리 |
| 로비·경기 상태·차량 이동 | Gameplay Framework와 ACharacter 기반 차량 설계 정리 |
| 트랙 건설·점수·복귀·아이템·HUD | 기능 명세와 마일스톤 정리 |
| 에셋 검사 도구 | TA 명세와 개발 계획 정리 |

EOS 로그인 코드는 중복 요청 방지, 로그인 결과 전달, 종료 시 콜백 정리를 포함합니다. 로그인 API의 구현과 실제 인증 성공, 세션 생성·참가 및 원격 게임 연결은 각각 별도로 확인해야 합니다.

이번 README 작성 과정에서는 빌드·로그인·패키징·멀티플레이 테스트를 실행하지 않았습니다. UVCS에 있는 Blueprint·맵·에셋의 최신 상태도 직접 확인하지 않았습니다.

## 프로젝트 실행 준비

### 1. 소스 받기

```bash
git clone https://github.com/kevin6091/ProjectMW.git
cd ProjectMW
```

### 2. Content 에셋 받기

현재 `Content/`는 Git 추적에서 제외되어 있고 **Unity Version Control**에서 별도로 관리합니다. 팀에서 사용하는 UVCS 프로젝트와 리비전을 확인한 뒤, 이 소스와 호환되는 에셋을 프로젝트의 `Content/`에 준비해 주세요.

기본 맵 설정은 다음 에셋을 가리킵니다.

```text
Content/Levels/L_Dev_Default.umap
```

Git만 복제한 상태에서는 맵·Blueprint·입력 액션·메시 등이 없어 정상적인 게임 실행을 확인할 수 없습니다. Git 커밋과 UVCS 리비전을 함께 기록해 작업 환경을 맞춥니다.

### 3. 엔진과 개발 도구 준비

1. 팀 기준인 Unreal Engine **5.8.3**과 해당 엔진에서 지원하는 Windows C++ 빌드 도구를 준비합니다.
2. `MW.uproject`에서 사용하는 플러그인을 확인합니다. 현재 EOS와 에디터용 RiderLink, PythonScriptPlugin, EditorScriptingUtilities, ModelingToolsEditorMode가 활성화돼 있습니다.
3. `MW.uproject`의 프로젝트 파일을 생성한 뒤 솔루션을 엽니다.
4. **Development Editor / Win64**에서 `MWEditor`를 빌드하고 `MW.uproject`를 실행합니다.

`MW.uproject`에는 엔진 연결 값이 `5.8`로 기록돼 있습니다. 팀원끼리는 실제 설치한 패치 버전도 맞춰 주세요.

### 4. EOS 환경 확인

현재 로그인은 `accountportal` 인증 경로를 사용합니다. 실제 실행에는 팀의 EOS 제품·배포 환경과 인증 설정이 필요합니다.

- 기본 OnlineSubsystem이 EOS로 선택되는지 확인합니다.
- 실행 환경에 맞는 EOS 설정과 테스트 계정 권한을 확인합니다.
- UI에서 `RequestLogin()`을 호출하고 `OnLoginFinished`로 성공 여부와 오류를 받습니다.
- `IsLoggedIn()`, `IsLoginInProgress()`로 화면의 현재 상태를 반영합니다.
- 로그인 성공 이후의 방 생성·검색·참가와 실제 게임 연결은 후속 개발 범위입니다.

## 기본 조작

아래는 채택한 조작 설계입니다. 실제 플레이는 입력 에셋과 차량·Controller의 연결 및 검증이 필요합니다.

| 상황 | 입력 | 동작 |
| --- | --- | --- |
| 주행 | W | 차량 전방으로 추진 |
| 주행 | S | 제동 · 정지 후 자동 후진 없음 |
| 주행 | A / D | 좌우 조향 |
| 주행 | Left Shift | 누르는 동안 기본 드리프트 요청 |
| 주행 | Space Bar | 아이템 사용 요청 |
| 트랙 선택 | 마우스 이동 / 좌클릭 | 후보 확인 / 선택 |
| 트랙 배치 | 마우스 이동 / 좌클릭 | 배치 미리보기 / 배치 요청 |
| 트랙 배치 | 마우스 휠 | 구조물 회전 |

건설 회전의 축·각도와 최종 소켓 배치에 반영하는 방식은 추가 결정이 필요합니다. 기본 드리프트는 필수 조작이고, 드리프트 부스트는 선택 기능입니다.

## 구조

```text
ProjectMW/
├─ Config/                 엔진·입력·에셋·온라인 설정
├─ Docs/
│  ├─ Engineering/         개발 가이드와 분야별 작업 규칙
│  └─ Shared/              기획·기능 명세·마일스톤·협업 문서
├─ Source/
│  └─ MW/
│     ├─ Data/             에셋·입력·차량 정의
│     ├─ System/           에셋 관리와 EOS 로그인
│     ├─ MWGameplayTags.*  공용 에셋·입력 태그
│     └─ MW.Build.cs       Runtime 모듈 의존성
├─ Content/                UVCS에서 별도로 받는 에셋
└─ MW.uproject
```

경기 규칙과 배치·점수·아이템 판정은 호스트 서버가 확정하고, 클라이언트는 입력과 요청·화면 표시를 담당하는 구조로 개발합니다. 차량은 최신 설계에서 **ACharacter와 CharacterMovement 확장**을 기반으로 합니다.

공용 데이터와 에셋 관리자는 각 실행 프로세스의 로컬 서비스입니다. 이 데이터가 자동으로 네트워크에 복제되는 것은 아닙니다.

## 앞으로의 개발

1. 공용 에셋과 입력을 실제 게임 시작 경로에 연결하기
2. EOS 인증을 검증하고 방 생성·검색·참가 구현하기
3. 로비의 참가자·준비 상태와 호스트 시작 기능 연결하기
4. 세 명이 같은 코스에서 차량을 조작하고 충돌·복귀 시험하기
5. 동시 트랙 선택과 서버 배치, 코스 준비 확인 구현하기
6. 점수·탈락·복귀·아이템·결과를 연결해 한 매치 완성하기
7. 실제 세 대의 PC에서 패키지·네트워크 회귀 테스트 진행하기

## 팀 작업

| 역할 | 주요 업무 |
| --- | --- |
| 차량·입력 | 차량 이동, 카메라, 입력 전환, 충돌·효과 적용, 체크포인트 감지와 복귀 |
| 건설·경기 규칙 | 트랙 연결·배치, 경기 상태, 점수·승패, 건설 화면 |
| 아이템·TA | 아이템과 위험·보조 요소, HUD 연동, 에셋 규격·검사 도구 |

소스 작업은 짧은 작업 브랜치와 PR로 관리합니다. 공용 데이터·설정 변경은 사용하는 사람과 먼저 맞추고, 같은 에셋 파일은 동시에 수정하지 않도록 작업자를 정합니다.

현재 에셋은 UVCS 관리로 전환된 상태입니다. 협업 문서의 과거 Git LFS 예시와 함께 볼 때는 실제 저장소 설정과 현재 팀 운영 방식을 먼저 확인합니다.

## 문서와 업무 관리

- [게임 기획](https://github.com/kevin6091/ProjectMW/blob/main/Docs/Shared/MakeWay_%EA%B8%B0%ED%9A%8D%EC%B5%9C%EC%A2%85%EC%95%88.md)
- [기능 명세](https://github.com/kevin6091/ProjectMW/blob/main/Docs/Shared/Client%2CServer/MakeWay_%ED%81%B4%EB%9D%BC%EC%9D%B4%EC%96%B8%ED%8A%B8_%EA%B0%9C%EB%B0%9C%EB%AA%85%EC%84%B8%EC%84%9C_v1.0.md)
- [전체 개발 마일스톤](https://github.com/kevin6091/ProjectMW/blob/main/Docs/Shared/Client%2CServer/02_%ED%81%B4%EB%9D%BC%EC%9D%B4%EC%96%B8%ED%8A%B8_%EB%A7%88%EC%9D%BC%EC%8A%A4%ED%86%A4_4%EC%A3%BC.md)
- [리슨서버와 Gameplay Framework](https://github.com/kevin6091/ProjectMW/blob/main/Docs/Shared/Client%2CServer/08_%EB%A6%AC%EC%8A%A8%EC%84%9C%EB%B2%84_%EA%B5%AC%EC%A1%B0%EC%99%80_Gameplay_Framework.md)
- [타이틀부터 게임플레이까지의 개발 순서](https://github.com/kevin6091/ProjectMW/blob/main/Docs/Shared/Client%2CServer/09_%ED%83%80%EC%9D%B4%ED%8B%80%EB%B6%80%ED%84%B0_%EA%B2%8C%EC%9E%84%ED%94%8C%EB%A0%88%EC%9D%B4%EA%B9%8C%EC%A7%80_%EB%A7%88%EC%9D%BC%EC%8A%A4%ED%86%A4.md)
- [차량 입력·이동·효과 설계](https://github.com/kevin6091/ProjectMW/blob/main/Docs/Shared/Client%2CServer/07_%EC%B0%A8%EB%9F%89_%EC%9E%85%EB%A0%A5_%EC%9D%B4%EB%8F%99_%ED%9A%A8%EA%B3%BC_%EC%84%A4%EA%B3%84.md)
- [TA 마일스톤](https://github.com/kevin6091/ProjectMW/blob/main/Docs/Shared/TA/01_TA_%EB%A7%88%EC%9D%BC%EC%8A%A4%ED%86%A4_4%EC%A3%BC.md)
- [개발 가이드](https://github.com/kevin6091/ProjectMW/blob/main/Docs/Engineering/DEVELOPMENT_GUIDE.md)
- [Git 협업 규칙](https://github.com/kevin6091/ProjectMW/blob/main/Docs/Shared/GIT_COLLABORATION_GUIDE.md)
- [Notion 업무 관리](https://app.notion.com/p/406adad4cf08817da36cde53e394e5a4) · 작업 공간 접근 권한 필요

