# ProjectMW UI 편집값 점검

2026-11-19 · fix/post-merge-gameplay · 변경 전 소스, Config 및 로컬 WBP 파일 기준. 동일한 값이 화면 모드에 따라 다른 경로에서 결정되는 곳을 표시했다.

| 화면 | 실제 편집 위치 | 실행 시 소비자와 덮어쓰기 |
|---|---|---|
| 로그인·타이틀·메뉴 | 타이틀 루트에 연결된 로그인·메뉴 WBP와 월드 WidgetComponent | MWTitleRootWidget이 인증·세션 상태로 활성 화면을 고른다. MWUCMTitleFlow가 카메라, 월드 표면, 입력 포커스를 바꾼다. |
| 인트로 스킵·방 종료 확인·타이틀 복귀 | MWUCMTitleFlow.cpp의 Slate | 스킵 버튼과 확인창은 액터 수명이다. 복귀 가림은 게임 인스턴스의 UMWUCMIntroSession이 뷰포트에 붙이고 복귀 월드·컨트롤러·카메라·메뉴를 확인한다. |
| 방 만들기·검색·목록 행 | 부모 WBP에 연결한 생성·검색·행 WBP, 테마 표시명은 DefaultGame.ini | MWCreateRoomWidget·MWRoomBrowserWidget·MWRoomListEntryWidget이 설정·방 목록을 표시하고 입력을 기존 세션 경로에 전달한다. |
| 운영 로비 | WBP_RoomLobby, 월드 연출의 표면·카메라 | MWLobbyHUD가 위젯과 로컬 입력을 소유한다. 연출 제공자 유무에 따라 월드 표면 또는 뷰포트에 붙인다. |
| 차량 선택 카드·명패·커서 | VehicleSelection WBP와 카드·참가자 행·마커 WBP. 월드 카드 그룹의 위치·크기, 범례 오프셋, 원격 커서 배율은 DefaultGame.ini | SetWorldPresentationLayout이 화면 슬롯을 저장하고 월드 값을 적용한 뒤 복원한다. HUD는 일반 차량 썸네일 다섯 개를 코드로 대입한다. 운영 취소는 위젯 → HUD → 소유 컨트롤러 경로다. |
| 경기 출발·로딩 | MWUCMTitleFlow와 SMWGameplaySign의 Slate | 출발 액터가 페이드·색·중앙 표지판을 만들고 EndPlay에서 제거한다. HUD는 연출 완료 콜백과 연출 불가 시 즉시 보고 경로를 가진다. |
| 트랙 선택·배치 | TrackSelectionWidgetClass의 WBP, MWTrackSelectionWidget의 동적 이름표, 월드 선택판의 카메라·Anchors·HitAreas | 선택판이 월드 후보를 생성하고 화면 좌표로 이름표를 옮긴다. 배치 시 같은 카메라가 코스 범위와 화면비를 사용한다. 월드 클릭 판정은 선택판·배치 컴포넌트에 있다. |
| 경기 대기판 | DefaultGame.ini의 WaitingBoardLayout, BoardLogo/Pole/Bolt, DisplayFont | MWGameplayHUDSubsystem의 Slate가 치수를 소비하고 네 열의 참가자 목록을 0.25초 간격으로 갱신한다. 로고는 ScaleToFit을 적용한다. |
| 단계 안내 | SMWGameplaySign이 외형을 직접 그리고 HUD가 중앙 배치·문구를 정한다. ConstructionPromptSeconds는 설정값이다. | HUD가 실제 경기 단계에 따라 선택·배치 표지판을 표시한다. |
| 아이템 슬롯 | ItemSlotLayout, 아이콘·재질, 룰렛 시각·사용 시간은 DefaultGame.ini. 유리 외형은 Content/UI/Common/Slate/ItemSlotGlass.svg | HUD가 보유 변경을 구독하고 룰렛·사용 연출과 최소 화면 크기를 계산한다. SVG 폴더는 DirectoriesToAlwaysStageAsUFS 대상이다. |
| 계기판·주행 닉네임 | RaceHUDWidgetClass가 선택한 WBP_RaceHUD의 Designer, 닉네임은 HUD의 Slate | 계기판은 차량 표시값을 받고 외부 FitDesignCanvas가 표시 영역을 정한다. 닉네임 위치는 투영된 픽셀에서 위젯 로컬 좌표로 변환한다. |

