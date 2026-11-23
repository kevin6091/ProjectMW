# 트랙 설치 시 하부 받침 골조 수정과 검증

2026-11-23 KST · `feature/presentation-integration` · UE 5.8.4 Win64 Development Editor

## 참고 영상과 이전 구현의 차이

`2026-10-30 13-56-11.mp4`의 01:44.1–01:45.1에서 와이어프레임이 설치된 도로로 바뀌고 아래에 가로대와 사선 버팀대가 있는 받침이 보인다. 02:48과 08:50에는 높은 구간에 여러 층을 이은 골조가 보인다. 10:05에 가까운 주행 화면에서도 가로대, 사선대, 결합부를 볼 수 있다. 이전 구현은 도로 중앙에서 바닥까지 좁은 탑 메시 하나를 길게 늘렸다. 이전 검증은 배치 여부와 바닥 접점을 확인했으며 영상의 폭, 골조 실루엣, 설치 시 등장 연출 비교가 빠져 있었다.

영상에서는 확정과 골조 등장 순서를 확인했다. 세부 전개 방향과 경과 시간은 가림과 카메라 이동 때문에 확정할 수 없었다. ProjectMW에서는 도로 아래에 윗부분을 붙이고 바닥 쪽으로 층을 완성하는 0.38초 전개를 적용했다.

## 수정 위치

| 위치 | 변경 |
| --- | --- |
| `Config/DefaultGame.ini` · `MWEnvironmentGameplay` | 기존 프로젝트의 `SM_Library_scaffoldmetalBeamsb199e8eaa6cf_8d1a5db1`을 사용한다. `TrackSupportBuildSeconds=0.38`에서 전개 시간을 편집한다. |
| `Plugins/MWEnvironmentGameplay/Source/MWEnvironmentGameplay/Private/MWTrackSupports.cpp` | 서버가 확정한 도로에서 고정 충돌을 읽어 접점과 유효 폭을 구한다. 직선·경사는 BP의 `MiddleLeg` 위치와 방향을 참고하고, 곡선은 두 포트 사이 진행 방향으로 폭을 맞춘다. 높이에 맞춰 같은 금속 프레임을 여러 층 잇고 위에서 아래로 전개한다. |
| 같은 플러그인의 `MWEnvironmentGameplay.h`, `MWEnvironmentGameplay.cpp` | 전개 중인 인스턴스와 갱신 타이머를 관리한다. 세션 변경과 환경 종료에서 제거한다. |
| `Source/MW/Gameplay/Construction/MWTrackSnap.h` | 기존 `ReadPorts`를 환경 모듈에서 호출할 수 있게 내보낸다. 서버 확정 좌표는 기존 설치 흐름으로 결정한다. |

메시와 연결 재질은 UVCS `cs:68` 서버와 해시가 일치했다. 메시 SHA-256은 `63b349d93dfcdfaee460864ab6952ea1bd8afcca2cc4385677448a4377ad5f13`이다. 기존 쿠킹 대상 `L_Title_UCM_Integration_Dev`의 하드 참조와 이전 쿠킹 목록에서 이 메시를 확인했다. 이번 작업은 기존 공유 에셋 재사용이라 바이너리 변경 세트가 없다.

## Cua Driver 3인 PIE

설원 `L_Dev_Drive`에서 일반 로비 준비 → 차량 선택 → 트랙 선택 → P1 직선, P3 45도 곡선, P2 경사 순서로 설치했다. 확정 전 세 창의 금속 받침 수는 0이었다. 각 창에서 확정 뒤 골조가 나타났고, 좌표 표본에서 골조 윗부분은 도로 아래 같은 높이에 머무르고 아래 끝만 바닥 Z=0까지 내려갔다. 경사와 자동 도착 조각의 전개는 마지막 설치 직후의 프레임 지연을 포함해 최대 0.703초가 계측됐다.

| 조각 | 골조 폭 | 층 수 | 바닥 Z |
| --- | ---: | ---: | ---: |
| 직선 | 2526.78cm | 2 | 0.00cm |
| 45도 곡선 | 2326.54cm | 3 | 0.00cm |
| 경사 | 2513.73cm | 3 | 0.00cm |
| 자동 도착 조각 | 2518.33cm | 3 | 0.00cm |

세 창에 각각 4개 골조가 완성됐고 11개 프레임 인스턴스는 충돌이 꺼져 있었다. 세 명 모두 실제 체크무늬 출발선에서 배정 차량을 소유한 채 `Racing`으로 진입했다. Editor 빌드와 기존 `MW.Construction.PreviewMatchesSnap` 자동화가 통과했다. 검증한 테마는 설원이다.

[이전 탑과 새 골조 비교 및 곡선 설치 순서](Images/TrackScaffoldBeforeAfter.jpg) · [세 창의 골조와 출발선](Images/TrackScaffoldThreePlayers.jpg) · [실측 높이·서버 설치·자동화 기록](TRACK_SCAFFOLD_PLACEMENT_PIE_LOG.txt)
