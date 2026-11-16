## 문제와 변경 후 동작

- 관련 작업/요구: 미병합으로 닫힌 PR #11의 기능과 `race-hud` 후속 작업을 최신 main에 맞춰 통합합니다.
- 변경 동작: 인트로 스킵, 차량 선택·우클릭 취소·참가자별 커서·출발 연출, 맵 이동 중 BGM 유지, 아이템 룰렛·오른쪽 계기판을 연결합니다. 기존 휠·스키드·부스터와 Snow/Toybox/Tropic도 포함합니다.
- 범위: 기존 UI를 재사용하고 차량 카드 순서·슬롯/대기판 치수·부스터 배율을 Config로 분리했습니다. 기존 기본값과 연출은 유지합니다. 실제 사용 아이템은 부스터 1종이며 나머지 아이콘은 룰렛 표시용입니다. [최신 변경·검증 기록](https://github.com/kevin6091/ProjectMW/blob/fix/frontend-main/Docs/Reviews/FrontendIntegration/MAIN_FRONTEND_INTEGRATION.md)을 참고해 주세요.

## 영향과 검토

- base: `main` / source: `fix/frontend-main`
- 작성자: `youngsowicked-commits` / 독립 검토 예정: `kevin6091`, `Yasasayi` / 영역별 승인·병합 담당: 팀 지정 필요. 아직 리뷰를 요청하지 않았습니다.
- 위험: **고위험** — 로비 API·이동 효과 연결·Config가 함께 바뀌어 두 팀원의 검토가 필요합니다.
- API·소비자: 선택 취소와 전원 출발 연출 완료 대기는 Widget→HUD→Controller→GameMode/PlayerState까지 포함합니다. 부스터·발판 연결은 이동 컴포넌트의 기존 효과 연결부를 사용합니다. **main의 보정·Snapshot·접지 핵심 함수는 유지합니다.**
- 의존 PR·순서: #12가 반영된 `main@ed6cbe3` 기준입니다. 수정2·3의 별도 네트워크 구현으로 덮어쓰지 않습니다. 확인 시점 main과 텍스트 충돌은 없습니다.

## 검증한 변경

- 검증 대상: `b824f94` 이후 설정 분리·개발 바닥 표시 수정까지 포함한 최종 변경 / 기준 main: `ed6cbe3de1d6eb5305902f33229f9fab3fecc917`. 이전 실제 선택·취소 검증은 `dc44603` 기록이며 새 결과와 구분합니다.
- 이번 설정 정리: Editor/Game 빌드·필수 BP 20개·자동검사 3개 성공. 일반 실행 창의 대기판·슬롯은 1280×720/1920×1080으로 확인했습니다. 개발 맵의 Z=0 중복 바닥은 기존 환경 숨김 설정에 연결했습니다. 맵·콜리전·서버 코드는 유지했습니다.
- 기존 입력 검증(이번 재실행 아님): UE 5.8.4 / Win64 Development: Editor·Game 빌드 성공. 주요 BP 20개 컴파일·파일 해시 유지 확인. 실제 LocalIP 호스트·클라이언트에서 비밀 차량 선택·우클릭 취소·양쪽 그림 표시와 기존 5대 썸네일을 확인했습니다.
- 남은 문제·미실행: 기존 접지 자동검사 1개에서 2개 assertion 실패는 차량 담당자 확인 대상입니다. main 단독 실행과 비교하지 않았습니다. 최종 전체 매치·원격 승강기·다른 PC·인터넷 다중 접속·Cook/패키징은 미검증입니다. 이번 PIE 대기판 fixture는 실패했으며 일반 실행 표시 검사로 구분합니다. 단계별 이전 PIE·EOS 기록은 상세 문서와 구분합니다.
- 문서 검사: PR 템플릿 항목·상대 링크·에셋 목록을 확인했습니다. 기존 타이틀 헤더의 줄 끝 공백 경고는 유지했습니다.

## 바이너리와 복구

- `.uasset`·`.umap`은 Git에 포함하지 않습니다. [에셋 인계 목록](https://github.com/kevin6091/ProjectMW/blob/fix/frontend-main/Docs/Reviews/FrontendIntegration/PR_ASSET_HANDOFF.csv)의 진입 에셋 52개와 연결 패키지 971개는 로컬에 있습니다. **새 인비저블맨 썸네일을 포함해 수신 PC의 에셋 버전 일치 확인이 필요합니다.** UVCS 버전·잠금/예약·인계 소유자는 이번에 조회하지 않았습니다.
- 전후 화면·BP·참조 증거는 상세 문서에 기록했습니다. Asset Registry 참조 검사는 Cook 성공이나 수신 PC 검증을 대신하지 않습니다. Git에는 직접 작성한 `ItemSlotGlass.svg` 소스만 별도로 포함됩니다.
- 복구: 담당자가 소스·Config와 대응 에셋을 함께 확인해 후속 되돌리기 커밋으로 처리합니다. 공유 이력 초기화·타인 파일 덮어쓰기는 하지 않습니다.
- 병합 후 확인·에셋 인계 담당: 팀 지정 필요. 메뉴→로비→차량 선택→경기 HUD·아이템→방 종료를 수신 버전으로 확인합니다.

## 병합 직전 확인

- [ ] 전체 diff·포함 커밋을 확인했고 무관한 변경/누락한 소비자가 없다.
- [ ] 최신 main 반영 후 위 HEAD에 필요한 검증과 독립 리뷰를 받았다.
- [ ] 미해결 의견·검사 실패·타인 잠금 충돌이 없다.
- [ ] 병합 담당자가 merge commit 방식으로 PR 한 개씩 반영한다.

Git 작업의 단일 기준: [공통 Git 규칙](https://github.com/kevin6091/ProjectMW/blob/main/Docs/Shared/GIT_COLLABORATION_GUIDE.md).
