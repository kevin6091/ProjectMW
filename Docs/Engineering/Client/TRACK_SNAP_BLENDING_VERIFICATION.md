# 트랙 고스트 스냅 블렌딩

기준은 PR #17의 `411e745`와 UVCS `cs:63`이다. 기존 설치 판정과는 별도로 트랙 고스트가 연결점에 붙고 떨어지는 위치·회전을 보간한다.

## 수정 위치

| 파일·함수 | 변경 |
|---|---|
| [MWTrackPlacementViewComponent.h](../../../Source/MW/Gameplay/Construction/MWTrackPlacementViewComponent.h#L26) · `FGhost` | 고스트마다 직전 스냅 목표와 보간 시작 자세·경과 시간을 저장한다. 고스트가 제거되거나 세션이 바뀌면 기존 고스트 수명과 함께 정리된다. |
| [MWTrackPlacementViewComponent.cpp](../../../Source/MW/Gameplay/Construction/MWTrackPlacementViewComponent.cpp#L224) · `BlendGhostPose` | 진입·해제·스냅 목표 변경 때 마지막으로 표시한 위치와 회전에서 보간을 시작한다. C++의 `GhostSnapBlendSeconds` 한 곳에서 기본 0.12초를 정한다. 이차 감속 곡선과 위치 Lerp·회전 Slerp를 사용한다. |
| [원격 표시](../../../Source/MW/Gameplay/Construction/MWTrackPlacementViewComponent.cpp#L100) · [로컬 표시](../../../Source/MW/Gameplay/Construction/MWTrackPlacementViewComponent.cpp#L181) | 원격은 기존 복제 목표 자세, 로컬은 현재 마우스로 계산한 목표 자세를 공통 함수에 넘긴다. 자유 이동 중 로컬은 포인터를 바로 따르고 원격은 기존 보간을 사용한다. 로컬 스냅 색·설치 안내는 판정 결과를 즉시 반영한다. |
| [클릭 시 설치 재판정](../../../Source/MW/Gameplay/Construction/MWTrackPlacementViewComponent.cpp#L354) | `ResolveGhostFromPointer`에서 현재 커서와 배치 세션으로 정확한 자세와 스냅 상태를 다시 계산한다. 이 경로는 보간 상태를 갱신하지 않으며 기존 `ServerPlaceTrack(SessionId, Session.CourseRevision, Intent)`를 호출한다. 서버는 같은 입력을 독립적으로 검증하고 실제 트랙을 설치한다. 에디터의 기존 `-MWVerifyTrackPlacement` 옵션은 클릭 재판정 결과와 화면 자세의 차이를 따로 출력한다. |

새 WBP·Config·에셋 편집값은 추가하지 않았다. 복제 데이터, 전송 주기, 연결 포트와 스냅 거리, 서버 설치 코드는 기준과 동일하다.

## 팀원 브랜치와 통합할 때

팀원의 `feature/track-construction`에는 원격 `bReversed` 변경을 감지해 위치와 회전을 즉시 적용하는 분기가 있다. 통합 시 그 분기를 기준으로 두고, 방향 전환을 처리한 뒤 해당 원격 고스트의 `SnapBlend`를 비우고 `PreviousSnapTarget`을 최신 스냅 목표로 맞춘다. 일반 이동·스냅 분기에서만 `BlendGhostPose`를 호출한다. 방향 전환과 스냅이 같은 갱신에 들어오면 팀원의 즉시 반영을 우선한다. 합친 코드에서 로컬·원격의 방향 전환과 클릭 재판정을 3인 PIE로 다시 확인한다. [팀원 쪽 원격 갱신](https://github.com/kevin6091/ProjectMW/commit/c09eacf)

## Cua Driver 3인 PIE

2026-11-20 KST · UE 5.8 Win64 Development Editor · 설원 `L_Dev_Drive` · LocalIP 리슨 호스트 P1, 참가자 P2·P3. `MWEditor` 빌드 성공. 프로젝트에 설치된 UI 에셋 10개는 `cs:63` 인계 SHA-256과 일치했다. 로비 준비부터 차량·트랙 선택과 설치까지 Cua Driver로 조작했다. 관측 스크립트는 런타임 상태와 서버가 기록한 자세만 읽었다. 시간 배율 1.0, 패킷 지연·트랙 후보 변경 없이 실행했다.

- **스냅 진입·해제** 세 명이 차례대로 끝점에 접근했다. P1의 진입과 해제, P2·P3의 진입은 조작 창과 나머지 두 창 모두에서 보간 로그가 남았다. 진입 첫 프레임 보간 비율은 0.275~0.409였고 0.129~0.140초의 누적 프레임 시간에 1에 도달했다. P1 해제는 0.120~0.123초에 종료됐다. P3 화면에는 고스트가 연결점으로 이동 중인 프레임이 찍혔다. [P3의 스냅 전·중·후](Images/TrackSnapBlendFrames.jpg)
- **스냅 중 방향 전환** P2가 스냅 상태에서 Q를 눌렀다. 세 창에서 새 목표로 보간을 시작했고, 설치 승인 로그는 P2가 뒤집은 조각의 포트 1을 기록했다. 네트워크와 서버 승인은 기존 경로를 탔다.
- **클릭 재판정** P1은 고스트를 자유 위치에 둔 뒤 스냅 가능한 지점에 바로 좌클릭했다. 클릭 시 남아 있던 표시 자세와 재판정 위치는 11,239.256cm 차이가 났지만, 서버는 클릭으로 재계산한 위치에 설치했다. P2와 P3는 보간이 끝난 뒤 좌클릭했다. 아래 회전 차이는 로그의 쿼터니언 출력 자릿수를 포함한다. [설치·보간 원본 로그 발췌](TRACK_SNAP_BLENDING_PIE_LOG.txt)
- **정상 진행** 서버의 코스에 시작 트랙과 조각 2·3·4, 종료 트랙이 생성됐다. 세 소유 창 모두 체크무늬 출발선의 자기 차량에서 `Racing`에 도달했다. 출발부터 `Racing`까지 로비 복귀나 맵 이동 실패는 없었다. 일부 캡처의 빨간 문구는 에디터의 GPU 메모리 경고다. [세 창 비교와 실제 출발선](Images/TrackSnapBlendThreePlayers.jpg)

| 참가자 | 서버 조각 · 대상 포트 → 자신 포트 | 클릭 재계산 위치 = 서버 기록 위치 (cm, 소수 여섯 자리) | 위치 오차 | 회전 오차 |
|---|---|---|---:|---:|
| P1 · FRED | 2 · 1:0 → 0 | (260.390000, 450.000000, 6000.000000) | 0cm | <0.00002° |
| P2 · DOUG | 3 · 2:1 → 1 | (10260.390000, 450.001311, 6000.000000) | <0.000001cm | <0.00002° |
| P3 · SMIET | 4 · 3:0 → 0 | (10260.390000, 450.001311, 6000.000000) | <0.000001cm | <0.00002° |

P2·P3의 배치 피벗 위치는 같고, 두 조각이 바라보는 방향은 180° 다르다. P2의 반대쪽 포트에 P3가 연결되는 경우다. 세 설치의 스케일 오차는 0이었다.

### 중앙 배치 안내

같은 빌드로 3인 PIE를 한 번 더 시작했다. 마지막 트랙 선택을 확정한 뒤 찍은 첫 마름모 프레임은 서버 배치 전환 기준 P1 0.135초, P2 0.197초, P3 0.265초였다. 중앙에는 세 창 모두 노란 마름모와 두 줄 ‘트랙을 / 배치하세요’가 표시됐다. 각 줄의 가로 중심은 판면 중심과 같았고, 글자 영역의 세로 중심은 판면 중심에서 1px 아래였다. 왼쪽 직사각형은 트랙 선택·배치 단계에서 조작법과 참가자를 표시하는 별도 패널이다. [중앙 마름모와 왼쪽 패널을 함께 찍은 캡처](Images/TrackSnapBlendPlacementDiamond.jpg)

PR #17의 브랜치와 UVCS는 그대로 두었다. 팀원 브랜치 통합 후 검증은 후속 작업이다. 이번 실행 범위는 에디터 빌드와 LocalIP 3인 PIE다.
