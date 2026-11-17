## 문제와 변경 후 동작

- 닫힌 #11의 변경과 이후 race-hud 작업을 main에 맞춘 PR입니다. 인트로 스킵, 차량 선택·취소·커서·출발 카메라, 레벨 이동 BGM, 아이템 룰렛·계기판, 부스터·휠·스키드 연결 코드와 3테마를 포함합니다.
- `7a66505` 안정본에 PR #13 이후 main을 반영해 차량 Pawn 두 파일의 충돌을 해결했습니다. 미검증 안내 UI `592048d`와 접지 실험 `0520c19`는 제외합니다.

## 영향과 검토

- base: `main` / source: `fix/frontend-main`. 커밋 작성·PR 등록: `youngsowicked-commits`. 영역 검토 대상: `kevin6091`, `Yasasayi`; 독립 리뷰·병합 담당 지정 전입니다.
- 고위험: UI·로비 API·차량 효과·Config를 함께 포함합니다. main의 접지 완충을 유지하며 기존 휠·카메라 연결만 맞췄습니다. 서버 보정·Snapshot 파일은 main과 같습니다.
- 별도 선행 PR은 없습니다. 최신 main의 `VisualRoot` 아래 기존 BP 부모 `DrivePresentation`을 연결하고 이동 → 표시 → 카메라 순서를 유지합니다.

## 검증한 변경

- 제출 HEAD: PR의 head SHA. 기준 main: `b837b504325af7de3b62a4d4f0258de27359d958`.
- 신규 검증: UE 5.8.4 / Win64 Editor·Game 빌드, BP 15개 컴파일 성공. 서버·접지 핵심·BP 보존 19개 일치. 주행 PIE에서 6종의 박스 루트·카메라 부모와 360도 루프 전진을 확인했습니다. 지속 주행은 Enhanced Input 키 주입을 사용했습니다.
- 자동 검사 13개 중 12개 통과. 기존 접지 상승 충격 검사 1개 실패가 남습니다. **2인 PIE 클라이언트 Ready 미반영으로 전체 경기 진입은 미완료입니다.** Ready 함수는 안정본·main과 같으며 원인은 미확정입니다.
- **현재 로컬 BP 6종의 WheelVisuals가 비어 있어 휠·스키드 동작은 미검증입니다.** 이전 에셋 인계 버전과 해시도 다릅니다. 이전 선택·취소·룰렛 등의 성공 기록과 이번 결과를 구분해 [상세 문서](https://github.com/kevin6091/ProjectMW/blob/fix/frontend-main/Docs/Reviews/FrontendIntegration/MAIN_FRONTEND_INTEGRATION.md)에 적었습니다. 다른 PC·전체 매치·Cook은 미실행입니다.

## 바이너리와 복구

- `.uasset`/`.umap`은 Git 제외입니다. [에셋 인계 목록](https://github.com/kevin6091/ProjectMW/blob/fix/frontend-main/Docs/Reviews/FrontendIntegration/PR_ASSET_HANDOFF.csv)의 대응 에셋·바퀴 설정 확인이 필요합니다. UVCS·잠금은 조회/변경하지 않았고 이번에 에셋을 저장하지 않았습니다.
- 문제 발생 시 소스·Config와 대응 에셋을 함께 확인해 후속 되돌리기 커밋으로 복구합니다. 인계·병합 후 수신 PC 검증 담당은 팀에서 지정합니다.

## 병합 직전 확인

- [ ] 전체 diff·포함 커밋을 확인했고 무관한 변경/누락한 소비자가 없다.
- [ ] 최신 main 반영 후 위 HEAD에 필요한 검증과 독립 리뷰를 받았다.
- [ ] 미해결 의견·검사 실패·타인 잠금 충돌이 없다.
- [ ] 병합 담당자가 merge commit 방식으로 PR 한 개씩 반영한다.

Git 작업의 단일 기준: [공통 Git 규칙](https://github.com/kevin6091/ProjectMW/blob/main/Docs/Shared/GIT_COLLABORATION_GUIDE.md).
