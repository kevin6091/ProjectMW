# P2.1 Screen Space UI 통합 검토

이 브랜치는 P2.1의 버튼·생성 폼·방 목록·공통 로비 및 Editor authoring 안전성 변경을 가져온 검토 후보다. 전체 통합 완료 상태가 아니다. P1/P3A의 3D 레벨·카메라·World Space 코드는 포함하지 않는다.

## 실행과 알려진 미적용 부분

`/Game/Levels/L_Frontend`의 `MWFrontendGameMode` → `MWFrontendPlayerController` → `WBP_FrontendRoot` 흐름을 유지한다. 기본 시작 맵과 기존 레벨 파일은 변경하지 않았다.

첫 로그인 화면인 `/Game/UI/Lobby/WBP_LoginTest`는 다른 작업자의 UVCS 잠금 때문에 수정하지 않았다. 실제 PIE에서 기존 작은 회색 버튼과 RenderScale 0.1, P2.1 고지 표지판 부재를 확인했다. 따라서 이 브랜치를 받는 것만으로 P2.1 로그인 디자인까지 표시되지는 않는다. 잠금 해제·인계 후 최신 EventGraph를 보존하면서 Designer 변경만 적용하고 실제 클릭을 검증해야 한다.

기존 MultiplayerSubsystem의 RequestLogin/IsLoggedIn/IsLoginInProgress/OnLoginFinished는 그대로 사용한다. 실제 인증 확인 없이 다음 화면으로 이동하지 않는다. 생성·검색·참가·Ready·Start·Leave의 실제 서비스 어댑터는 아직 구현되지 않았고, Runtime은 테스트 성공으로 대체하지 않는다.

## 변경 범위

- Runtime Frontend: 빈 이름 오류 표시, Full→Joinable 선택 시 이전 오류 제거, Ready/참가자 상태 투영, 원본 표지판 버튼 상태·방향 아이콘, Widget 해제 시 안전한 구독 정리.
- Editor authoring: 클래스·CDO·리플렉션 형식 검증, 컴파일/저장 실패 처리, 원본 로그인 그래프를 보존하는 명시적 Designer 적용 도구. 이 도구를 잠긴 로그인 에셋에 실행하지 않았다.
- Editor QA: 현행 정식 Root를 transient Blueprint로 복제해 fixture를 만든다. 오래된 공유 Preview 에셋을 덮어쓰거나 새로 저장하지 않는다. 동일 이름 방의 SessionId 구분과 2~64행 목록 overflow 검사용 데이터는 Editor 모듈에만 있다.

Git에는 C++와 문서만 포함한다. Content는 별도 UVCS `/main/fronted_UI`에서 관리한다. 새 텍스처 7개와 수정 WBP 3개가 후보이며 로그인 및 Developers 테스트 에셋은 제외한다. 바이너리 출처는 기존 Make Way 추출물이다. 사용자가 팀 내부 공유 권한을 확인했으며, 공개 Git 및 공개 배포물에 바이너리를 추가하는 권한으로 해석하지 않는다.

## 검증과 인계 상태

설치 UE 5.8.4 Win64 Development Editor/Game 빌드, 정식 WBP 4개 컴파일 및 `/Game` 참조 검사 수행. P2.1 동일 해시 사본의 실제 마우스 회귀와 실제 팀 프로젝트의 자동화·PIE를 구분한다. 팀 자동화에서 오래된 Preview 트리의 슬롯을 테스트가 직접 역참조해 크래시한 것을 확인하여, transient 정식 트리 사용 및 누락 슬롯 안전 검사로 보정했다. 이 최종 보정은 C++ 컴파일을 통과했지만 실행 중인 Editor의 DLL 잠금(LNK1104)으로 최종 링크·재실행 검증이 대기 중이다.

따라서 검토용 Draft PR이며 자동화 전부 성공 상태가 아니다. 팀 프로젝트 전체 마우스·실제 EOS 인증 성공·실제 세션/WAN·Cook/패키징은 완료로 표시하지 않는다.

최종 실행 결과와 UVCS changeset은 PR 본문에 기록한다. main 병합은 이번 작업에 포함하지 않는다. 독립 팀원 리뷰와 로그인 파일 인계를 마친 뒤 통합 완료 여부를 판단한다. 되돌릴 경우 이 브랜치의 소스 변경과 대응 UVCS UI changeset을 함께 되돌리며, 기존 EOS·공용 맵은 대상에 포함하지 않는다.
