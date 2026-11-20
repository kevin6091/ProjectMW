# 차량 엔진 배기음 구현과 검증

- Git 브랜치 `feature/vehicle-engine-audio`, 시작점 `e05bd00`
- UVCS `cs:66@br:/main` — 가속 반응용 차량 엔진 배기음 루프 추가
- 10월 30일 영상 02:05–02:10의 정차·주행 대비를 입력과 속도 반응의 기준으로 삼았다. 사용한 음원은 ProjectMW용으로 새로 합성한 모노 2초 루프이며 전 차종이 공통으로 사용한다.

| 수정 위치 | 변경 |
| --- | --- |
| `Source/MW/Gameplay/Vehicle/Visual/MWVehicleEngineAudio.cpp` | 각 차량의 표시 부모에 공간 음향 컴포넌트 하나를 부착한다. Racing 시작 시 한 번 재생하고, 실제 주행 방향 속도로 음높이와 게인을 바꾼다. 자기 차량에서는 페달의 즉시 반응도 반영하며 원격 차량은 기존 복제 속도를 사용한다. 경기 종료 및 Pawn 해제 시 재생을 중지한다. |
| `Source/MW/Gameplay/Vehicle/MWVehiclePawn.h` 및 `.cpp` | Pawn 수명과 기존 표시 갱신 경로에 엔진음 호출을 연결한다. |
| `Source/MW/Gameplay/Vehicle/Visual/MWVehicleEngineAudioSettings.h`, `Config/DefaultGame.ini` | 루프 에셋, 게인·피치 범위, 반응 속도와 거리 감쇠를 게임 설정이 소유하도록 했다. |
| `Content/Gameplay/Vehicles/Art/EngineAudio/SW_MW_EngineExhaustLoop.uasset` | 루프와 가상화 값은 SoundWave가 소유한다. UVCS 서버에서 내려받은 원본 SHA-256은 `553c6988784dc8124b03d192cae7a823994e9a31ab6e8c0bcc4c446fc1331040`이며 작업본과 일치했다. |

## 검증

- UE 5.8 Editor Win64 Development 빌드 통과.
- 정상 3인 선택 흐름은 차량 선택·경기 월드 로딩까지 통과했으나 트랙 선택 클릭이 확정되지 않았다. 같은 화면의 직접 픽셀 광선은 트랙 선택 상자에 닿았다. 세 창의 주행 음향 검증은 에디터 메모리에서만 트랙 후보를 비운 직행 경기로 따로 수행했다. 게임 모드 에셋을 저장하지 않았다.
- Cua Driver로 3인 경기 직행 후 세 창에서 차량 소유와 Racing 진입을 확인했다. 각 월드에 FRED·DOUG·UFOLOGY의 엔진음 루프가 각각 하나씩 재생됐다. 계측 중 주행 전 단계에서는 재생 컴포넌트가 없었다.
- 테스트 입력으로 호스트 FRED를 2초 가속했다. 정차 피치 0.82·게인 0.10에서 호스트 피치 1.58855·게인 0.34454, 두 원격 창 피치 1.58392·게인 0.34307을 관찰했다. 물리 속도가 바뀌며 두 원격 창의 재생 값도 따라왔다.
- 음원 루프와 PIE의 재생 상태·가속 반응을 계측했다. 원본 음색과의 청감 대조 및 실제 설치 코스에서 배기음까지 이어지는 검증은 남아 있다.

[세 창 캡처](Images/VehicleEngineAudioThreePlayers.jpg) · [재생 및 가속 계측](VEHICLE_ENGINE_AUDIO_PIE_LOG.txt)
