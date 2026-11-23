# 설치 트랙 철골 지지대 구현과 검증

- Git 작업 브랜치 `feature/track-supports`, 시작점 `e05bd00`
- UVCS `cs:65@br:/main` — 설치 트랙 철골 지지대 메시와 재질 추가
- 참고 영상 `2026-10-30 13-56-11.mp4`의 01:44.70–01:45.10, 02:44–02:49. 설치 확정 후 트랙 아래에 생기는 가느다란 교차 보강 구조를 확인했다. ProjectMW에서는 새로 만든 철골 메시를 사용한다.

## 수정 위치

| 파일 | 처리 |
| --- | --- |
| `Plugins/MWEnvironmentGameplay/Source/MWEnvironmentGameplay/Private/MWTrackSupports.cpp` | 서버가 게시한 설치 조각 중 고정 도로 충돌이 준비된 조각을 읽는다. 원점과 가까운 고정 충돌 형상의 아래쪽에 지지대 꼭대기를 맞추고 바닥까지 높이를 조절한다. 환경별 인스턴스 묶음에 추가하며 충돌·오버랩·내비게이션 영향은 끈다. |
| `Plugins/MWEnvironmentGameplay/Source/MWEnvironmentGameplay/Private/MWEnvironmentGameplay.cpp` 및 `Public/MWEnvironmentGameplay.h` | 기존 월드 상태 점검에서 설치를 확인한다. 월드·테마 변경 시 렌더러를 정리하고 세션이 바뀌면 인스턴스를 비운다. 에셋 교체는 설정의 `TrackSupportMesh`에서 한다. |
| `Plugins/MWEnvironmentRestore/Source/MWEnvironmentRestore/Public/MWEnvironmentRestore.h` 및 `Private/MWEnvironmentRestore.cpp` | 바닥 렌더러가 준비된 뒤 실제 바닥 렌더링이 사용하는 높이를 제공한다. |
| `Source/MW/Gameplay/Track/MWTrackRuntimeComponent.h`, `.cpp` | 고정 도로 충돌 형상에 식별 태그를 붙여 움직이는 발판과 구분한다. 설치 판정과 물리 충돌 설정은 기존 경로를 사용한다. |
| `Config/DefaultGame.ini` | 지지대 메시의 소프트 참조와 쿠킹 디렉터리를 등록했다. |

## UVCS 인계

`cs:65`의 파일 변경은 아래 두 에셋이다. 서버에서 다시 내려받아 로컬 파일의 SHA-256과 일치함을 확인했다.

| `Content/Gameplay/Tracks/Art/Supports/` | 서버 SHA-256 |
| --- | --- |
| `M_MW_TrackSupportSteel.uasset` | `e27a43e1de2fb8442fd2e8aa2aa598dc279368e093e639a8076c3f8bba1d7636` |
| `SM_MW_TrackSupportTower.uasset` | `5e31b6f69bc816bb9385c1c1108ec73c3de5d5f6855ecc00c73955e0f1e20a2e` |

## 확인 결과

- UE 5.8 Editor Win64 Development 빌드 성공. 별도 패키징은 수행하지 않았다.
- Cua Driver로 설원 3인 리슨 서버 PIE에 접속했다. 호스트·클라이언트 2명 모두 차량을 선택한 뒤 목재 직선 조각을 차례로 확정했다.
- UI가 배치 단계일 때 세 월드에서 지지대가 0 → 1 → 2로 늘었다. 마지막 플레이어가 확정한 시점에는 세 번째 설치 조각과 자동 결승 조각에 지지대가 추가됐다. 출발 조각과 확정되지 않은 고스트는 대상에서 제외했다.
- 세 월드의 지지대 4개 모두 바닥 Z=0.00 cm, 도로 아래 접점 Z=5,878.84 cm였다. 같은 지지대의 호스트·원격 XY 차이는 최대 0.003 cm 이하였다. 12개 인스턴스의 충돌은 모두 꺼져 있었다.
- 전원이 각자의 차량을 소유한 상태로 실제 출발선에서 Racing에 들어갔다. 곡선·경사·움직이는 랜드마크에 대한 실화면과 패키지 동작은 이번 검증 범위에 포함하지 않았다.

[설치 전후 및 출발선 비교](Images/TrackSupportsThreePlayers.jpg) · [3인 좌표와 경기 진입 로그](TRACK_SUPPORTS_PIE_LOG.txt)
