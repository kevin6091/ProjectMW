# 테마별 트랙 색상 검토

## 영상에서 확인한 범위

- 10월 30일 영상의 02:00 출발 구간, 02:25 주행, 02:45 배치, 04:45 주행, 08:45 배치, 10:50 순위 화면을 비교했다. 같은 주황색 사막 배경에서 적색·청색·녹색 계열 트랙이 함께 보인다. 이 영상만으로 같은 조각의 색이 테마마다 바뀌는지 결정할 수 없다.
- 10월 27일 영상의 00:50 전후는 모드 메뉴와 배경 코스다. 다른 테마에서 같은 조각을 설치한 비교 장면은 확인되지 않았다.

## ProjectMW의 적용 경로

| 코드·설정 | 현재 소유하는 값 |
| --- | --- |
| `Config/DefaultGame.ini`의 환경 `Bindings` | 설원·완구·열대의 환경 테마 에셋과 첫 장면 재질 |
| `Source/MW/System/GameMode/MWGameplayGameMode.cpp`의 `Construction->BeginSelection` 호출부 | 현재 경기 모드에 설정한 `ConstructionTrackDefinitionIds` |
| `Source/MW/Gameplay/Construction/MWTrackConstructionPlacement.cpp`의 `PrepareTrack` | 선택한 정의 ID에 들어 있는 기존 트랙 BP 클래스 |
| `Source/MW/Gameplay/Construction/MWTrackPreview.cpp`의 `SetWireframeColor` | 미리보기 와이어프레임의 색 |
| `Config/DefaultGame.ini`의 `MWTrackSelectionExposure` | 트랙 선택·배치 카메라의 테마별 노출 |

서버가 설치용 BP를 고를 때 `MapThemeId`를 전달하는 경로는 발견되지 않았다. 현재 테마에 따라 환경 조명과 노출이 달라져 같은 재질도 다르게 보일 수 있다.

## 적용 가능성

같은 트랙의 테마별 원래 색이 확인되면 `MapThemeId`와 트랙 계열에 맞춰 도로 슬롯의 Material Instance를 선택할 수 있다. 기존 도로 메시, 물리 표면, 트랙 정의 ID, 입력 스냅과 커서 색은 현재의 소유 경로에서 유지한다. 슬롯 이름과 적용 대상을 에셋 데이터로 지정하면 BP별 분기 없이 시작·설치·결승 조각을 동일 규칙으로 처리할 수 있다.

검증은 같은 직선 조각을 설원·완구·열대에서 각각 설치한 뒤 도로 재질 경로를 기록하고, 참가자 세 창에서 같은 슬롯이 적용됐는지 비교한다. 이 브랜치에는 영상 및 코드 검토 기록만 올렸다.
