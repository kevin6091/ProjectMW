# 경기 출발 슬롯과 신호등 검증

`feature/race-start-grid`에서 `origin/main@5a4006d`를 기준으로 수정했다. 사용자 제공 Switch 영상의 0:54–0:59 구간에서 참가 차량이 체크무늬 뒤의 ㄷ자 칸에 먼저 정렬하고 준비 신호가 끝나면 함께 출발하는 흐름을 확인했다. ProjectMW는 로비에서 확정한 참가 순서로 슬롯을 배정하고 기존 서버 Racing 전환에서 전원 주행을 허용한다.

## 원인과 수정 위치

| 위치 | 수정 |
| --- | --- |
| `MWGameplayGameMode.cpp`의 `PrepareRace` | 확정 코스 첫 조각을 출발 관리자로 넘긴다. 코스가 없는 개발 경로는 기존 레벨 슬롯을 사용한다. |
| `MWPlayerStartManager.cpp`, `MWTrackSnap.cpp` | 시작 BP의 ㄷ자 표식 중심을 첫 참가자부터 순서대로 읽는다. 회전은 같은 BP의 `StartGrid` 방향, 높이는 테마가 관리하는 출발 높이를 사용한다. 기존 PlayerStart와 서버 차량 생성·재배치 경로를 재사용한다. |
| `MWTrackRuntimeComponent.cpp`, `MWTrackRuntimeSettings.h`, `DefaultGame.ini` | 시작 BP에 있는 빨간 행 두 개와 초록 행을 연결한다. 신호의 재질은 각 경기 월드에서만 인스턴스화한다. PreparingRace에서는 소등, 복제된 Countdown 잔여 시간에 따라 빨간불 두 행, Racing에서 초록불을 표시한다. 슬롯 바인딩·신호 순서·카메라 조정값은 프로젝트 설정에서 편집한다. |
| `MWVehiclePawn.cpp`의 `UpdateStartCamera` | 출발 그리드와 아치를 한 구도로 보여 주는 시점을 차량의 카메라 계산에서 적용한다. Racing에서 기존 차량 추적으로 보간한다. |
| `MWGameplayPlayerController.cpp`의 `IsLocalRaceReady` | 첫 실행에서 일부 클라이언트의 적색 신호가 로딩 가림막에 가렸다. 각 로컬 가림이 걷힌 뒤 기존 출발 준비 보고가 가능하다. 맵 이동·로딩 복구·서버 카운트다운 시간은 기존 경로를 사용한다. |

## 첫 검증에서 확인한 항목

`MWEditor Win64 Development` 빌드를 통과했다. Cua Driver로 Snow 경기의 3인 Listen Server PIE를 실행했다. 차량 3대의 선택과 실제 코스 배치 검사를 통과하고, 서버와 클라이언트 두 창에서 첫 출발을 확인했다.

| 참가자 | 체크무늬 부근의 기존 XY (cm) | ㄷ자 표식 중심의 수정 후 XY (cm) | 초록불 후 전진 (cm) |
| --- | --- | --- | ---: |
| 1 | (−2620, 375) | (−3314.61, −312) | 1001.11 |
| 2 | (−2620, 525) | (−3314.61, 1212) | 907.16 |
| 3 | (−2870, 375) | (−4364.61, −312) | 907.14 |

- 세 차량 모두 처음 지정한 표식 중심과 XY가 0.01cm 이내에서 일치했다. 출발 순서대로 앞왼쪽·앞오른쪽·뒤왼쪽에 섰다.
- 세 창 모두 가림을 걷은 후 출발 준비를 보고했다. 첫 소등 표본부터 약 3.08초 뒤 모든 창에서 초록불과 주행 허용 상태가 동시에 관측됐다. 빨간불 동안 가속 입력을 줘도 서버에서는 주행이 잠겼다. 초록불 뒤 실제 가속 입력으로 세 차량 모두 전진했다.
- 첫 설정의 0.45초 전환에서는 추적 시점으로 이동할 때 아치가 너무 일찍 프레임 밖으로 나갔다. 1.1초 전환 설정으로 다시 실행해 세 창 모두 초록불과 출발 칸을 한 프레임에서 확인했다. 서버 카운트다운 길이와 차량 입력 개방 조건은 그대로다.

[출발 전후 위치](Evidence/RaceStartGrid/grid-before-after.jpg) · [세 창 신호등](Evidence/RaceStartGrid/lights-three-windows.jpg) · [출발 슬롯 실측](Evidence/RaceStartGrid/start-slots.json) · [신호·카메라·입력 타임라인](Evidence/RaceStartGrid/signal-frames.csv) · [PIE 로그 발췌](Evidence/RaceStartGrid/verification.txt)

첫 검증의 범위는 설원 3인 PIE였다. 기존 시작 BP와 재질을 사용했다.

## 4인과 테마별 출발 재검증

기준은 기능 커밋 `e42a9d8`과 원격 `main@5a4006d`다. 원격 main은 기능 커밋의 직계 조상이며, 변경 파일의 `git diff --check`를 통과했다. Unreal Engine 5.8에서 `MWEditor Win64 Development` 빌드를 통과했다. UVCS 작업공간 기준은 cs:82이며, 기존 시작 BP·테마 레벨과 신호등 재질을 사용했다. 출발 그리드에 필요한 새 UVCS 변경 세트는 없다.

리뷰 대기 중인 #27과는 차량 Pawn과 경기 GameMode를 함께 수정한다. `merge-tree`로 두 브랜치의 병합 결과를 임시 객체 저장소에 계산해 텍스트 충돌이 없음을 확인했다. 두 PR의 결합 동작은 별도 검증 범위다.

Cua Driver로 Listen Server 1창과 로컬 클라이언트 3창을 띄웠다. 로비 준비와 선택은 검증 스크립트로 실행했고, 네 명 모두 각 창에서 좌클릭해 서버의 실제 트랙 설치를 완료했다. 출발 전후에는 네 차량에 동일한 가속 입력을 주입해 이동을 비교했다. 신호 단계의 캡처 동안만 PIE 월드 시간을 늦췄으며 표의 전진 거리는 캡처 단계를 해제한 뒤 서버 좌표에서 측정한 값이다.

| 테마 | 차량의 출발 표식 중심 XY 오차 | 카운트다운 중 서버 XY 이동 | 녹색 신호 뒤 P1 / P2 / P3 / P4 전진 |
| --- | ---: | ---: | --- |
| 설원 | 전원 0.01cm 미만 | 0.00cm | 1013.6 / 931.7 / 931.7 / 931.7cm |
| 장난감 상자 | 전원 0.01cm 미만 | 0.00cm | 1004.9 / 952.5 / 952.5 / 952.5cm |
| 열대 | 전원 0.01cm 미만 | 0.00cm | 1014.0 / 971.2 / 971.2 / 971.2cm |

세 테마 모두 네 차량이 앞왼쪽·앞오른쪽·뒤왼쪽·뒤오른쪽에 차례로 섰다. 네 창에서 소등 → 적색 첫 줄 → 적색 두 줄 → 녹색 순서였다. 적색 동안 서버와 각 창의 소유 차량 주행이 잠겼고, 녹색 단계에서 모두 주행할 수 있었다. 세 테마는 공통 경기 레벨 `L_Dev_Drive`에서 각자 다른 환경 테마를 로드했다.

### 장난감 상자 테마에서 P4 화면 공개 지연

- P4가 트랙을 먼저 설치한 뒤 해당 클라이언트에 실제 로딩 가림막을 다시 띄웠다. `PreparingRace`에 진입한 뒤 14초 동안 P4의 첫 화면 확인을 지연했다. 나머지 세 PIE 창과 서버의 준비·복제는 평소 경로를 사용했다.
- 서버가 `Ready=3/4`를 기록한 뒤 P1~P3의 화면은 출발 그리드에서 소등 상태로 대기했고 P4에는 로딩 화면이 보였다. 약 15.94초 뒤 가림막 공개가 완료되어 P4가 준비를 보고했고, 그 뒤 카운트다운을 시작했다. 로그에서 P4 공개, 준비 4/4, 첫 적색 신호 순서를 확인할 수 있다.
- 검증 범위는 한 PC의 네 PIE 월드다. 늦은 로딩은 로컬 첫 화면 가림 지연으로 재현했다. 별도 PC, 네트워크를 통한 맵 전송 지연, 패키지 실행은 검증 범위 밖이다. 기존 검증 때와 달리 이번 지연은 경기 맵에 이미 도착한 뒤에 넣었다.

[설원 네 창](Evidence/RaceStartGrid/FourPlayer/snow-four-windows.jpg) · [장난감 상자 네 창과 P4 로딩 대기](Evidence/RaceStartGrid/FourPlayer/toybox-four-windows.jpg) · [열대 네 창](Evidence/RaceStartGrid/FourPlayer/tropic-four-windows.jpg) · [출발 표식과 신호 실측](Evidence/RaceStartGrid/FourPlayer/start-grid-measurements.json) · [PIE 로그](Evidence/RaceStartGrid/FourPlayer/pie-log.txt)

### 검증 도구와 로컬 상태

자동 PIE 방의 기본 정원이 3명이어서 `MWGameInstance.cpp`에 4인과 테마 선택을 여는 에디터 전용 검증 옵션을 임시로 추가했다. `MWArrivalScreen.cpp`의 검증 명령은 P4에만 가림막을 열고, 지연이 끝난 뒤 기존 첫 화면 검사를 재개했다. 두 파일을 검증 전 내용으로 돌린 뒤 동일한 Editor 구성을 다시 빌드했다. 이번 커밋에는 검증 문서와 증거만 추가한다. 로컬 아이템 VFX 작업의 소스와 UVCS 변경은 작업자 상태대로 보존했다. 에디터 빌드에서 해당 아이템 코드의 기존 `GetBindings()` 사용 중단 경고 1건이 기록됐다.

첫 검증의 임시 `Source/MW/Tests/MWRacePresentationLocalQA.cpp` 및 중간 보고서는 첫 검증 종료 시 제거했다. 이번 검증의 원본 프레임과 전체 로그는 `Saved/TrackHazardValidation/RaceStartGridExpanded`에 남겼으며, 위 링크는 각 검사의 대표 화면과 관련 로그다.
