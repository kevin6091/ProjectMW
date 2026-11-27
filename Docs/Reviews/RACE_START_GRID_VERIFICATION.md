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

## 세 창에서 확인한 항목

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

4인, 다른 테마와 별도 PC는 이 실행에서 확인하지 않았다. 기존 시작 BP와 재질을 사용했다. 바이너리 에셋, 푸시, UVCS 체크인, 패키징, 병합은 보류했다.


검증에 생성한 임시 `Source/MW/Tests/MWRacePresentationLocalQA.cpp`와 `Saved/TrackHazardValidation/RaceStartGrid/verification-draft.md`를 제거했다. 최종 캡처·좌표·로그는 위 링크에서 확인할 수 있다.
