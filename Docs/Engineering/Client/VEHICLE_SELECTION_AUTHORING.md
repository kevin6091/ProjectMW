# 차량 선택 화면 편집과 프리뷰 문구 제거

운영 로비가 생성하는 `WBP_VehicleSelection_Preview`는 `WBP_VehicleSelection_Dev`의 Designer 트리를 상속한다. 에셋 이름의 Preview는 클래스 식별자다. 변경 전 부모 트리에는 `PreviewControls/PreviewNotice` 텍스트 블록과 `디자인 Preview · 서버 상태 아님` 문구가 저장돼 있었다. 런타임의 `RefreshControls()`는 이를 숨겼지만 `NativeConstruct()`는 해당 위젯이 없으면 버튼 연결을 중단했다. 카드 이름·참가자 상태도 별도 Slate 툴팁으로 등록돼 마우스 정지 시 사각창을 생성했다.

| 파일 또는 에셋 | 수정 위치와 결과 |
|---|---|
| `Content/UI/Lobby/VehicleSelection/WBP_VehicleSelection_Dev.uasset` | `PreviewNotice` 노드를 제거하고 이름/GUID를 정리했다. `PreviewControls` 기본값은 Collapsed다. 같은 Canvas 아래의 숨긴 `WorldCardBounds` 슬롯이 월드 카드 행의 앵커·크기·정렬을 소유한다. 부모에 저장한 변경이 파생 WBP에 적용된다. |
| `Source/MW/UI/VehicleSelection/MWVehicleSelectionWidget.{h,cpp}` | 필수 바인딩·초기화 검사에서 삭제한 문구를 제거했다. 월드 진입 시 `AlignmentCardGroup`에 `WorldCardBounds`의 슬롯을 복사하고 모드 종료 시 기존 슬롯·자동 크기·커서 변환을 복원한다. 옛 WBP는 기존 월드 설정을 읽는다. |
| `MWVehicleCardWidget.cpp` · `MWVehiclePlayerEntryWidget.cpp` · `MWVehiclePlayerMarkerWidget.cpp` | 중복 사각 툴팁을 만드는 텍스트 등록을 제거했다. 차량명·닉네임·확정 표시는 기존 카드와 참가자 행에 남는다. |
| `MWVehiclePlayerMarkerWidget.h` · `Source/MW/UI/Cursor/MWPointerCursor.h` · `WBP_VehiclePlayerMarker_Dev.uasset` | 마커 단위의 정규화 핫스폿을 지원한다. 원본 화살표의 렌더 변환을 반영해 현재 WBP의 끝점 (0, 0)을 지정했다. 같은 마커 경로가 입력·표시에 쓰인다. |
| `Config/DefaultGame.ini` · `Source/MW/UI/Lobby/MWLobbyHUD.cpp` | HUD가 일반 차량 다섯 개의 썸네일 경로를 덮어쓰던 코드를 지웠다. WBP의 차량별 지정이 우선하고 미지정 ID는 `DefaultVehicleThumbnails`에서 찾는다. 비밀 차량 항목도 유지한다. |

## 검증

UE 5.8에서 부모와 파생 WBP를 컴파일했다. 로컬 3인 PIE의 운영 로비에서 세 참가자의 차량 선택 인스턴스마다 하위 위젯 80개를 조사했고 `PreviewNotice`가 없었다. Cua Driver로 세 번째 창에서 SMIET를 좌클릭해 서버 차량 ID와 초록색 확정 카드가 일치하는지 확인했다. 같은 카드에서 우클릭해 차량 ID가 비고 화면이 ‘차량 선택 중’으로 돌아오는 것도 확인했다. 삭제한 안내문을 참조하던 초기화 조건이 남았다면 이 입력이 동작하지 않는다. 수신 측에서 같은 부모 WBP와 마커 해시를 적용해야 한다.
