# 로딩 직후 단계판과 복구창

2026-11-19. 정상 진입과 준비 실패를 따로 재현했다. 정상 실행에서 “불러오는 중” 직후 짧게 보이는 UI는 경기 HUD의 단계 안내였다. 선택 화면에서는 `SelectSign`, 배치 화면에서는 `BuildSign`이 표시된다. 후보를 누락시킨 실행에서 뒤늦게 나타나는 작은 기본 버튼 창은 `UMWArrivalScreen::ShowFailure`가 만들었다.

## 수정 위치

| 화면과 편집 주체 | 구체적인 변경 |
|---|---|
| `Content/UI/Race/WBP_MatchPanels.uasset` Designer | `SelectSign`·`BuildSign` 자리에 로비의 `T_MW_MenuSign` 원본 판면과 `F_NotoSansKR_Bold_Font`를 사용했다. 740×172 디자인 단위의 중앙 앵커 판면, 49pt 한 줄 문구, 9분할 브러시로 확장해도 모서리·볼트가 찌그러지지 않도록 했다. 문구·색·크기·패딩은 여기서 바꾼다. |
| `Source/MW/UI/Gameplay/MWGameplayHUDSubsystem.cpp`, `Config/DefaultGame.ini` | 기존 HUD가 단계와 가림 상태를 읽어 표시한다. `ConstructionPromptSeconds`의 기존 2초가 적용된다. |
| `Source/MW/UI/Gameplay/MWGameplaySign.{h,cpp}` | 경기와 로비 복귀가 함께 쓰는 `MWGameplayUI::RecoveryPanel`을 추가했다. 기존 로비 확인창의 `SMWGameplayPanel` 판면·볼트·파란 테두리·공용 한글 글꼴을 사용한다. 내용은 중앙 정렬하며 긴 사유 문구는 줄바꿈과 스크롤을 지원한다. 버튼의 콜백과 초기 포커스는 호출 화면이 전달한다. |
| `Source/MW/UI/Gameplay/MWArrivalScreen.cpp` | 경기 첫 화면의 준비 실패 안내에 공용 판면을 적용했다. 후보 재준비가 가능하면 재시도 버튼, 이동·연결 오류로 재시도가 불가능하면 방 나가기 버튼에 포커스를 준다. |
| `Plugins/MWUCMFrontend/Source/MWUCMFrontend/Private/MWUCMTitleFlow.cpp` | 타이틀 복귀의 세션 정리 오류에도 같은 안내창을 연결했다. 재시도·종료는 기존 세션 정리·종료 콜백으로 전달한다. |
| `Docs/Reviews/FrontendIntegration/PR_ASSET_HANDOFF.csv` | Git 제외 WBP의 배포 경로, 현재 크기와 SHA-256을 갱신했다. |

## 실행 결과

- **정상 3인 PIE** (PID 8600). 세 창 모두 준비된 Snow 테마와 3개의 월드 후보 위에 새 단계판이 보였다. 각 창의 첫 공개와 약 2초 뒤 프레임을 대조했다. 후속 프레임에는 중앙 안내가 사라졌고 후보와 카메라는 유지됐다. 기존 서버 출발 준비 보고는 Member 1·2·3 각각 한 번, 남은 인원 2→1→0이었다.
- **별도 프로세스의 첫 진입**. 각각 새 게임 클라이언트로 시작했다. PID 38868은 트랙 선택, PID 18572는 가림 중 선택을 모두 확정해 트랙 배치로 바로 들어갔다. 둘 다 1280×720에서 첫 프레임과 약 2초 뒤를 대조했다. 선택 첫 장면에는 후보와 “트랙을 선택하세요”, 배치 첫 장면에는 시작 트랙·고스트와 “트랙을 배치하세요”가 나타났다. 두 번째 프레임에서 안내만 사라졌다. 프로세스 간 OS 캐시는 유지했다.
- **경기 복구** (PID 12364, 3인 PIE). Client 2에서 후보 1의 표시 액터를 제거해 실패 경로를 재현했다. 다른 두 창은 정상 진입했고 해당 창에는 파란 판면, 한글 안내, 두 동작이 표시됐다. Cua Driver로 ‘첫 화면 다시 준비’를 클릭하자 가림막이 돌아왔으며 누락된 후보가 복구된 뒤 세션 1·리비전 1로 공개됐다. 실패와 재공개는 같은 시도 `01DE73ED4787DE532A7B16961B55677E`였다. 타이틀 복귀의 세션 서비스 오류와 실제 네트워크 단절은 이번 실행에서 주입하지 않았다.
- 같은 회복 실행에서 Member 3의 포인터를 오른쪽 직선 후보로 옮겼다. Client 2의 붉은 로컬 화살표와 Client 1에 보인 녹색 원격 화살표 끝이 같은 후보 위치에 놓였고 수신 화면에 닉네임 258이 보였다.
- UE 5.8 Win64 Development `MWEditor`·`MW` 빌드 성공. WBP 저장 뒤 새 에디터 프로세스로 다시 열고 컴파일해 오류 0건을 확인했다. 경고 1건은 프로젝트의 기존 EOS Ecom 설정 경고다.
- WBP 로컬 인계본은 72,127바이트, SHA-256 `c760335644a94389bcbb98b60f0f6b928c44ad3a793eee3ddc5ab231b271a422`이다. Git에 포함되지 않는 바이너리는 별도 ZIP에 제공한다.
