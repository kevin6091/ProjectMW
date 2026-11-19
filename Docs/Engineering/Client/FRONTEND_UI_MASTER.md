# ProjectMW — Frontend / Vehicle Selection UI 기준 문서

> 현재 기준 문서 · 2026-11-02 KST · P3 보고서11개 및 로컬 소스·WBP 저장본·검증 자료와 대조
>
> 최신 시각 상태: 2026-11-03 **11.19절**. 사용자가 11.16 후속의 작은 중앙 상태판·아이콘 없는 좌우 안내를 채택했다. 추가 요청에 따라 하단 상태판 배경을 기존 커서 PlayerTint와 연결했고, 11.19에서 차량 전체를 프레임 내부 ScaleBox에 맞춰 머리·꼬리의 상단 침범과 이름 겹침을 함께 보정했다. 기능·서버 계약은 유지한다. 이전 기능 기준: **11.13절**의 사용자 변경 요구에 따라 개발 Preview는 차량 클릭 즉시 완료, 전원 완료 전 취소, 전원 완료 후 취소 차단으로 변경했다. 별도 선택 버튼을 숨기고 선택 전후 Windows 화살표를 UI 영역 안에서 숨긴다. 960/1920 실제 입력과 OS 커서 상태 조회를 통과했고, 자동 입력 중단 후 사용자가 1920 화면에서 “화살표 없이 고정·취소 정상”을 직접 확인했다. P3 Preview 기능은 현재 동작으로 동결하고 실제 서버 통합 대기로 전환한다. 11.10~11.12는 당시 요구·검증 이력으로 보존한다. 실제 서버 정책은 변경하지 않았으며 FGameplayTag 계약과 서버 통합 대기 상태를 유지한다.

## 1. 문서 관리 정책

- UCM 공급자 연동은 사용자 요청으로 별도 작성한 [입력 계약](PROJECTMW_UI_UCM_INTEGRATION_CONTRACT.md), [요구 에셋 CSV](PROJECTMW_UI_REQUIRED_ASSETS.csv), [실제 진입점](PROJECTMW_UI_INTEGRATION_ENTRYPOINTS.md)을 따른다. 현재 UI를 교체하는 새 프레임워크가 아니며, 이번 문서 작성에서 코드·에셋 연결은 수행하지 않았다. 조사/보존 검증은11.20절이다.

- **이 파일을 P3 타이틀·차량 선택 UI의 유일한 현재 상태 문서로 사용한다.** 후속 소규모 수정은 여기의 해당 절과 변경 기록만 갱신한다.
- 기존11개 보고서는 중요 기술 기록만 날짜가 명확한 Archive에 남긴다. 나머지 중복 MD는 통합·원문 보존 경로·링크를 확인한 후 제거한다. 처리 결과와 SHA는 이 문서의 정리 부록을 기준으로 한다.
- 과거 보고서의 `완료`, `최종`, `승인` 표현이 최신 상태보다 우선하지 않는다. 특히 3+2·4+1 배치는 **폐기된 설계안**이다.
- 대용량 로그·캡처·검증 JSON·동영상은 기존 증거 저장 위치/인계 ZIP에 보관하고 본 문서에는 **경로와 검증 요지만** 적는다. 임시 `Rollback` 파일은 증거/기록과 구분한다.
- 회귀 문제, 구조 변경, 서버 계약, 출시 전 검증이 필요한 중대한 이슈만 별도 ADR/이슈 문서를 고려한다. 수정마다 새 `P3_*.md`를 생성하지 않는다.

## 2. 현재 구현 범위 (보고서 기준)

| 영역 | 최신 상태 | 주의 |
|---|---|---|
| 타이틀 | `/Game/Levels/L_Title_WorldUI_Dev`에서 기존 메뉴 위젯을 World Space 표지판으로 제시 | 정식 `/Game/Levels/L_Title`은 변경하지 않음 |
| 실제 EOS | 초기 P3에서 로그인·방 생성/검색·로비 진입/종료·타이틀 복귀 시험 기록 | **최근 UI 변경 전체에 대한 최신 EOS End-to-End 재검증은 없음** |
| 차량 선택 UI | 5대 단일 행: **Smiet / Mammoth / Ufology / Doug / Fred** | 5대는 원본 텍스처를 사용하는 **Preview 콘텐츠** |
| 차량 카드·전시판 | 원본 Sprite 기반 카드, 원형 포인터, 하단 플레이어 전시판 및 한 줄 상태판 | 원본 전체 격자 수량·폰트·배경과 1:1 동일하지 않음 |
| 로컬 마우스 커서 | Preview 클릭 즉시 완료·원형 고정, 전원 완료 전 우클릭 취소·추적 복귀, 전원 완료 후 잠금. UI 안에서 Windows 화살표 숨김 | Preview만 변경. 실제 서버 승인·취소 정책 변경 아님 |
| 다인 참가 화면 | 2·3·4인, 미선택·혼합 확정·동일 차량 등 Preview 시나리오 | 실제 2~4계정 서버 Replication 검증 아님 |
| 명패·참가자 식별 | 11.16에서 원본 Sign·Bolt와 하단 중앙 이름을 유지하며 추가 색상 점을 숨김. 11.17에서 작은 상태판 배경에 커서와 같은 PlayerTint 적용 | 상태판의 참가자 색은 ProjectMW 표시 요소이며 원본 1:1 재현으로 주장하지 않음 |
| 주행 차량 | P4 로컬 AssetRegistry 재조회: VehicleDefinition·DriveData 및 차량 카탈로그 항목 **0개** | main의 주행 C++ 추가와 실제 승인 에셋 준비를 구분. 다른 팀 UVCS 미수신 자료는 미확인. 11절 참조 |

### 데이터 및 이벤트 연결 요약

- 타이틀: 기존 `UMWTitleRootWidget` 및 `UMWMultiplayerSubsystem`을 재사용한다. World 메뉴는 개발 WBP/WidgetComponent 경로로 제시한다.
- 차량 UI: 기존 `UMWVehicleSelectionWidget`, `UMWVehicleCardWidget`, `UMWVehiclePlayerEntryWidget`, `UMWVehiclePlayerMarkerWidget`, `AMWVehicleSelectionPreviewActor`를 재사용한다.
- 차량 데이터: 실제 게임에서는 기존 `UMWVehicleDefinitionData`와 `UMWAssetManager` 카탈로그에 연결하도록 설계했다. 현재 5개 Preview 리소스를 승인된 차량 정의/주행 Pawn으로 취급하지 않는다.
- 선택 데이터와 표시 위치는 별개다. Preview 미선택 시 기존 Marker WBP를 UE 소프트웨어 커서로 표시한다. 클릭 후 소프트웨어 매핑을 유지하되 표시만 숨기고 기존 World Space 원형 하나를 고정한다. 따라서 선택 후에도 UI 안에서 일반 화살표를 표시하지 않는다. 카드 위 좌우 레전드에 좌클릭 선택·우클릭 취소 안내를 표시하고 전원 완료 전 취소하면 추적으로 복귀하며, UI 이탈·포커스 상실·제거 시 이전 커서 매핑을 복원한다. **로컬 원형 두 개를 동시에 표시하지 않으며 실제 원격 커서 동기화는 없다.**
- 로비 Ready/Start → 차량 선택 단계 진입 → 서버 검증/확정 → 트랙 조각 선택·배치의 연동은 서버 담당자와 계약이 필요하며, 실제 완성 기능으로 기재하지 않는다.

## 3. 최신 확인 사항과 미해결 이슈

### 확인·부분 완료

1. **차량 5대 단일 행 + Mammoth 교체:** `P3_VEHICLE_UI_SINGLE_ROW_FINAL.md` 기준. Mammoth 원본 아이콘은 `sharedassets0.assets` Sprite/Texture2D 출처 확인 기록이 있다.
2. **하단 검정 상태판:** `선택 중`/`확정` 한 줄로 축약. `P3_VEHICLE_PLAYER_PANEL_READABILITY.md` 기준.
3. **로컬 포인터:** 최신 사용자 요구와 구현은 11.13절이다. 미선택 소프트웨어 원형과 클릭 후 World 고정 원형은 상호 배타적이다. Preview는 별도 확정 클릭 없이 완료되며 실제 참가자가 모두 완료하기 전까지 취소할 수 있다. 원격 Preview 마커와 구분하며 클릭 후 로컬 추적 원형을 추가 표시하지 않는다.
4. **현재 하단 외형:** 11.16에서 원본에 없는 색상 점·큰 로컬 테두리를 숨기고, Sign 모서리·외곽선 및 차량 점유율을 복원했다. 아래는 이전 정렬 이력이다. 최근 `P3_VEHICLE_UI_FINAL_ALIGNMENT_AUDIT.md`에서 WBP 한 개 수정. 1920에서 점↔이름 수직 중심 차이 약 0.5px, 왼쪽 여백 25px, 아래쪽 여백 18px; 960에서 각각 0px/11px/9px로 보고했다. 원본 명패에는 같은 색상 점이 보이지 않으므로 ProjectMW 추가 요소로 구분한다.

### 미해결 / 향후 판정 필요

| 우선순위 | 문제 | 근거 및 필요한 검증 |
|---|---|---|
| 수정/범위 제한 | **개발 레벨 기둥의 추적 커서 가림·입력 차단** | 11.10에서 미선택 커서를 별도 소프트웨어 표시로 분리하고 두 개발 레벨 좌우 장식 기둥의 Visibility만 Ignore로 저장·재로드 확인. 11.11에서 차량 개발 레벨의 실제 1920×1080/960×540 좌우 기둥 입력과 자유 원형 표시 통과. 타이틀 전체 EOS 흐름 및 다른 장면의 장식물까지 해결한 것은 아님. |
| Major | **실제 멀티플레이 차량 선택 미연결** | Ready/Start, 서버 단계·VehicleId 검증·PlayerState 선택/확정 복제·차량 Pawn/트랙 전환 계약 미완성. |
| Major | **Git/UVCS 공유 통합 전 점검** | 마지막 감사 시점 Git `feature/title-world-ui` / HEAD `3db9c5d`, UVCS 로컬 cs16, 원격 head23까지 전진. 실제 최신값·타인 잠금·Pending을 재조회하고 공용 작업과 충돌 없이 통합할 절차 필요. |
| Major | **최신 수정본에 대한 전체 EOS 흐름 검증** | 개발 Preview와 타이틀의 실제 EOS 진입/방 생성/참가·복귀는 구분해야 함. |
| 사용자 확인 통과 | **Windows 화살표 표시** | 11.13에서 선택 전후 화살표 숨김으로 변경했다. 960/1920 실제 입력 중 GetCursorInfo flags=0/handle=null 확인 후, 사용자가 1920 실제 화면에서 “화살표 없이 고정·취소 정상”을 확인했다. 11.11~11.12의 당시 관찰은 이력으로 보존한다. |
| 검증 | Renderer `AppTime.cpp:38 IsInGameThread` handled ensure | 최신 감사에서 1회 발생(로그 2행). 과거 TickTaskManager assertion/PureVirtual의 근본 원인도 미확정. |
| 후속 | 960 가독성, 원본 글꼴/색감, 게임패드·긴 실계정명, 패키징 | 테스트 범위를 분리해 보완. 배경 신규 제작은 현재 UI 작업 범위에서 제외. |

### 기둥 수정 범위 유지

- 전체 World Space UI를 Screen Space로 전환하거나 모든 Widget Material의 Depth Test·레벨 기둥 충돌을 무조건 비활성화하지 않는다.
- 11.10에서 입력 차단(`Visibility trace`)과 표시 가림(`depth`)을 분리해 수정했다. 자유 원형만 UE 소프트웨어 커서로 표시하며, 두 개발 레벨 좌우 장식 기둥의 Visibility 응답만 Ignore다. 전체 World UI 또는 전역 충돌 정책을 바꾸지 않는다.
- 복제/서버 관련 변경은 담당자 합의 없이 시행하지 않는다.

## 4. 프로젝트 소유권 및 작업 제한

- 실제 프로젝트: `C:/workspace/UE/ProjectMW/MW.uproject`.
- Git 작업 브랜치: `feature/title-world-ui` (최근 보고서 기준; 실행 전 재검사).
- 개발 레벨: `/Game/Levels/L_Title_WorldUI_Dev`, `/Game/Levels/L_VehicleSelection_UI_Dev`.
- 정식 타이틀 `/Game/Levels/L_Title`, 공용 EOS·서버·차량 이동 코드는 유지.
- 하드코딩 금지는 상시 규칙. 기존 C++·UMG Designer·BP 설정·AssetManager/DataAsset·Delegate·소유권 패턴을 우선 재사용한다.
- 별도 프로젝트 복제/worktree, 임의 브랜치 변경, 공용 파일 덮어쓰기 및 무단 Git commit/push/UVCS checkin/merge 금지.

## 5. 역사 문서 인덱스 — 핵심5개 선택 보관

11개 모두 동일 바이트의 기존 작업 원문과 대조했다. Git 미추적, UVCS private·잠금 없음이며 다른 팀 문서에서 직접 참조하는 링크는 없었다. 아래5개는 프로젝트 Archive에 이동했고 나머지6개는 내용 통합 후 중복 사본만 삭제했다. 6개의 당시 원문은 기존 검증 폴더에 보존되어 설계 결정·실패/수정 과정·측정·원본 분석·증거 경로가 손실되지 않는다. 모든 과거 MD를 Archive에 복사하지 않는다.

|당시 보고서 / 보존 원문|수정 시각 KST|처리와 역할|SHA-256|
|---|---|---|---|
|[P3_UI_VISUAL_POLISH_REPORT.md](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-visual-polish/P3_UI_VISUAL_POLISH_REPORT.md)|2026-10-30T13:44:48|delete_duplicate_md: 현재 상태/핵심 결정은 Master에 통합. 동일 SHA의 기존 작업 기록 원문·증거 경로 보존. 별도 현재 보고서 중복 제거|`d4dd5533017c1b7f2ef1a57e39446dfae83a8ab989bf58888d5d121fa56c66db`|
|[P3_VEHICLE_CURSOR_TRACKING_AND_ALIGNMENT.md](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-latest-validation/P3_VEHICLE_CURSOR_TRACKING_AND_ALIGNMENT.md)|2026-11-02T14:27:17|delete_duplicate_md: 현재 상태/핵심 결정은 Master에 통합. 동일 SHA의 기존 작업 기록 원문·증거 경로 보존. 별도 현재 보고서 중복 제거|`8929f6f1e02c000585b6c1de2e36e3bbcc4dcb30ab76fa21d0d28cb965f9c6cb`|
|[P3_VEHICLE_MULTIPLAYER_UI_LAYOUT.md](Archive/P3_2026-10-29_2026-11-02/P3_VEHICLE_MULTIPLAYER_UI_LAYOUT.md)|2026-10-30T10:29:37|archive_move: 실제 API와 제안 서버 계약의 구분|`d13c318cde5d2c3fa8e28c7bf4a0c1c2e148e1ebd1c0be578e8cc89922a3f616`|
|[P3_VEHICLE_ORIGINAL_SCREEN_REBUILD_REPORT.md](Archive/P3_2026-10-29_2026-11-02/P3_VEHICLE_ORIGINAL_SCREEN_REBUILD_REPORT.md)|2026-10-30T15:43:35|archive_move: 원본 Unity Scene/Prefab·카메라 분석|`b760e0a8af191641d19d83232ec5a400350121ba79aac98fe6a67ce4b66ec68d`|
|[P3_VEHICLE_PLAYER_PANEL_READABILITY.md](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-player-panel-readability/P3_VEHICLE_PLAYER_PANEL_READABILITY.md)|2026-11-02T10:57:54|delete_duplicate_md: 현재 상태/핵심 결정은 Master에 통합. 동일 SHA의 기존 작업 기록 원문·증거 경로 보존. 별도 현재 보고서 중복 제거|`40b4dfec75370804d488a5966cf964f808f31b8df7433cd84f3c6122e60d5c1b`|
|[P3_VEHICLE_SELECTION_REVIEW.md](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-vehicle/P3_VEHICLE_SELECTION_REVIEW.md)|2026-10-30T09:14:30|delete_duplicate_md: 현재 상태/핵심 결정은 Master에 통합. 동일 SHA의 기존 작업 기록 원문·증거 경로 보존. 별도 현재 보고서 중복 제거|`653ab6e37df3dc68be10f4125a84920aca54c2dab841e810645e9a5cd9e9fb77`|
|[P3_VEHICLE_UI_ALIGNMENT_CORRECTION.md](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-alignment-correction/P3_VEHICLE_UI_ALIGNMENT_CORRECTION.md)|2026-10-30T17:18:06|delete_duplicate_md: 현재 상태/핵심 결정은 Master에 통합. 동일 SHA의 기존 작업 기록 원문·증거 경로 보존. 별도 현재 보고서 중복 제거|`10325a8bc6798f1cb5c64554e218e48e3337bf4094dfdd3e1586598fbe99e7cc`|
|[P3_VEHICLE_UI_FIDELITY_REVIEW.md](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-fidelity-review/P3_VEHICLE_UI_FIDELITY_REVIEW.md)|2026-10-30T19:23:28|delete_duplicate_md: 현재 상태/핵심 결정은 Master에 통합. 동일 SHA의 기존 작업 기록 원문·증거 경로 보존. 별도 현재 보고서 중복 제거|`320a5f94aa3fcd0d7bdf0eb29811ec922923882762eb994c9447b79a1df68c4b`|
|[P3_VEHICLE_UI_FINAL_ALIGNMENT_AUDIT.md](Archive/P3_2026-10-29_2026-11-02/P3_VEHICLE_UI_FINAL_ALIGNMENT_AUDIT.md)|2026-11-02T15:51:43|archive_move: 현재 명패 최종 실측·커서 가림 재현|`e8b38c140c5041cd3ac2803199e3e487498d7f2bbd7c13b3b13fe110ea470697`|
|[P3_VEHICLE_UI_SINGLE_ROW_FINAL.md](Archive/P3_2026-10-29_2026-11-02/P3_VEHICLE_UI_SINGLE_ROW_FINAL.md)|2026-11-02T10:04:06|archive_move: Mammoth native 출처·단일행·배경 복구 결정|`3fc8c566366f8903acacca9a44c34d1aa54ba86b2e8f33775d2e87e6732c08fd`|
|[TITLE_WORLD_UI_P3_REPORT.md](Archive/P3_2026-10-29_2026-11-02/TITLE_WORLD_UI_P3_REPORT.md)|2026-10-29T15:29:00|archive_move: 초기 World 타이틀 구조·실제 EOS 입력 기록|`ea3c90cfc4f87e6db7cbb6aa74eff60624c3c632b3624f96a0a4ac7e7965f544`|

P1/P2 원본 보고서, Shared·Art·Server 공용 문서, Client 작업 규칙·트랙 설계 문서는 이동·수정하지 않는다. 수정 시각은 파일 mtime이며 승인 시각 또는 구현 완료 판정으로 해석하지 않는다.

## 6. 이후 문서 갱신 규칙

수정이나 검증을 수행할 때마다 **새 MD를 만들지 말고** 이 문서 아래 변경 기록에 한 줄을 추가하고, 영향받은 '현재 구현'·'미해결'·'테스트 상태' 절만 갱신한다. 중요한 결함의 재현 로그는 기존 증거 폴더에 날짜/이슈ID로 저장하고 링크한다.

| 날짜 | 변경 | 수정 파일 | 검증 | 남은 문제 |
|---|---|---|---|---|
| 2026-11-02 | 색상 점의 모서리 여백·이름 시각 중심 보정 | `WBP_VehiclePlayerEntry_Dev.uasset` | 보고서상 2·3·4인 Preview/마우스 회귀 통과 | 기둥 가림·실서버 연동·EOS 전체 회귀 |
| 2026-11-02 | 생성 파일1874개 실제 삭제, 기준 MD 통합, 핵심5개 Archive·중복 MD6개 제거 | Master·P3 생성물 | MP4 전체 디코딩7개, 기존90링크 정상, 복구 SHA 대조 | Intermediate/에셋 보류, 기둥·ensure 미해결, 볼륨 순여유 변화 음수 |
| 2026-11-02 | P4 통합 사전 조사: main 6a47da9 / 서버 브랜치 927fcff / 로컬 P3 API·에셋 대조 | 이 Master만 갱신 | GitHub 현재 ref, Git 소스 비교, Editor AssetRegistry·CDO 읽기 | ID 타입·생성 책임·서버 공개 API/복제 미완성; 새 빌드·PIE 없음 |

## 7. 확인 범위와 아직 필요한 통합

문서/생성 파일 정리 당시 Git branch는 feature/title-world-ui / HEAD3db9c5d15e5496a4b699cb48e7796d826622bfb4였다. UVCS는 로컬cs16이며 해당 작업에서 변경·체크인·업데이트·스위치하지 않았다. 개발 에셋/코드는 미제출 작업물이므로 삭제하지 않는다. 추출 리소스 팀 내부 공유 권한 확인은 기존 사용자 답변을 유지하되 Git/UVCS 제출 완료를 뜻하지 않는다. 후속 P4 조회 결과는 11절을 따른다.

원본 L_Title, 공용 로비, EOS·서버·차량 코드와 에셋을 보존한다. 정리 시작 시 저장본의 소스·WBP·레벨 해시는 직전 최종 정렬 검증 자료와 같았다. 이후 관찰된 레벨 저장/해시 변경은 정리 부록의 예외 기록을 따른다. 이번은 문서/생성 파일 정리이므로 새 PIE, Blueprint compile, Editor/Game build 결과를 만들어 주장하지 않는다. 사용자 에디터를 종료·저장하지 않으며 Intermediate 정리도 보류한다.

## 8. 현재 코드 연결과 판정 기준

- `UMWTitleRootWidget`가 기존 Login/Title/CreateRoom/Browser의 전환과 Subsystem 이벤트 구독·해제를 담당한다. TitleScreen은 BP가 지정한 WorldMenuActor의 WidgetComponent에 연결하고 입력 폼은 기존 루트를 사용한다.
- 개발 타이틀의 차량 Preview는 동일 World의 `AMWVehicleSelectionPreviewActor.OpenPreview`를 찾아 카메라 ViewTarget을 바꾸는 경로다. `L_VehicleSelection_UI_Dev`는 독립 검증용이며 차량 Preview 진입 자체가 그 레벨로 Travel하는 구조가 아니다. PreviewActor는 PIE 가드를 유지하고 Root 소유·Back·Component 해제·기존 카메라/입력 모드 복원을 담당한다.
- `UMWVehicleSelectionWidget.RefreshVehicles`는 PreviewCards 또는 AssetManager 카탈로그를 통해 동적으로 Card를 만든다. `RefreshPlayers`는 개발 fixture 또는 실제 GameState.PlayerArray를 공통 presentation으로 수렴시킨다. 실제 분기는 `OnLobbyChanged`를 구독하며 Ready를 차량 확정으로 대신 사용하지 않는다.
- Card/Entry/Marker는 기존 Delegate와 `SetPresentation`을 사용한다. SelectionWidget이 소프트웨어 커서를 소유하며, Preview 클릭 시 매핑을 해제하지 않고 내용의 opacity만 숨겨 World LocalPlayerCursor 하나만 남긴다. UI 이탈·포커스 상실·제거 시 이전 매핑을 복원한다. 전원 완료 전 우클릭 취소 시 고정 표시를 숨기고 소프트웨어 원형을 다시 표시한다. virtual Slate 좌표와 pinned 좌표·선택값 분리는 유지한다. 다른 참가자의 Grid 마커는 Preview이며 실제 원격 커서 동기화는 없다.
- 로컬 P3에서 카탈로그 GameplayTag와 Definition의 FName VehicleId는 다르며 `ValidateDefinition`은 Pawn/InputData를 검증한다. **main 6a47da9에서는 DriveData 검증으로 변경됐다.** Preview 5대를 승인 Definition/Pawn으로 등록하지 않았다. 상세 차이는 11절 참조.
- 로컬 main 계열의 GameState는 RoomSettings/SessionId/OnLobbyChanged, PlayerState는 Host/Ready/MemberId/등록 확인 상태를 갖는다. 과거 보고서의 Phase/Epoch·별도 선택 확정 bool·RPC 제안은 실제 API가 아니다. **서버 담당 브랜치의 최신 문서는 VehicleAssetID 하나와 RoomPhase를 사용하고 추가 Epoch/Revision/선택 bool을 도입하지 않는다. 11절의 비교·계약을 우선한다.**

최신 명패 Designer는 Badge(18,4),20² / NameCenter(46,-10),136×48이며 이름 중심은 부모 기준114로 유지된다. Noto22와 원본 Sprite는 유지하고 긴 이름만 ScaleBox DownOnly로 맞춘다. 실제 glyph 중심 차이는1920에서0.5~1px/960에서0~0.5px다. 점을 단순히 아래로 내린 중간안은 최종안이 아니다.

명패 정렬의 과거 검증은 `work/p3-final-alignment-audit`의 FINAL_VALIDATION_SUMMARY.json, final-captures.json, final-clearance.json, final-input-*.json/PNG, final-inset-cursor.mp4와 FinalEditorBuild.log/FinalGameBuild.log에 보존한다. 당시 1920·960 각7개 fixture와 1280 3개 fixture를 시험했고 기둥 가림은 미해결이었다. 현재 커서·기둥 판정은 11.10~11.11 및 `work/p3-cursor-occlusion/input-state.jsonl`의 FINAL/FREEZE 구간이 우선한다. Renderer AppTime handled ensure의 근본 원인은 여전히 미확정이며 UI 입력 판정과 분리한다.

## 9. 실제 정리 부록 — 삭제 전 승인 범위와 목록

2026-11-02 실행 시작 여유 공간: 114,206,572,544 bytes. 중간 PNG 삭제 전 개별 절대경로·크기·SHA-256·삭제 사유·대체 MP4·프레임 번호는 [삭제 전 전체 목록](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-doc-consolidation/png-delete-plan.json)에 기록했다. [MP4 전체 디코딩 및 PNG 비교](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-doc-consolidation/recording-verification.json)에서7개 모두 통과했다. 이 목록을 쓰고 나서 삭제하며, 실제 결과를 별도로 덧붙인다.

|삭제 후보 경로 — 바로 아래 중간 frame PNG 중 목록 지정분만|개수|bytes|사유|
|---|---:|---:|---|
|`C:\Users\user\Documents\Codex\2026-10-27\make-way-ue-0-make-way-2\work\p3-final-alignment-audit\final-inset-cursor`|45|109,044,733|정상 MP4에 포함된 중간 프레임. 대표/상태 전환 PNG는 유지|
|`C:\Users\user\Documents\Codex\2026-10-27\make-way-ue-0-make-way-2\work\p3-final-alignment-audit\free-cursor-final`|70|166,767,742|정상 MP4에 포함된 중간 프레임. 대표/상태 전환 PNG는 유지|
|`C:\Users\user\Documents\Codex\2026-10-27\make-way-ue-0-make-way-2\work\p3-final-alignment-audit\mouse-tracking`|165|110,295,963|정상 MP4에 포함된 중간 프레임. 대표/상태 전환 PNG는 유지|
|`C:\Users\user\Documents\Codex\2026-10-27\make-way-ue-0-make-way-2\work\p3-latest-validation\gateA-current-input`|370|250,653,616|정상 MP4에 포함된 중간 프레임. 대표/상태 전환 PNG는 유지|
|`C:\Users\user\Documents\Codex\2026-10-27\make-way-ue-0-make-way-2\work\p3-latest-validation\gateB-fixed-marker-input`|526|357,426,313|정상 MP4에 포함된 중간 프레임. 대표/상태 전환 PNG는 유지|
|`C:\Users\user\Documents\Codex\2026-10-27\make-way-ue-0-make-way-2\work\p3-latest-validation\gateC-final-input`|558|381,779,039|정상 MP4에 포함된 중간 프레임. 대표/상태 전환 PNG는 유지|
|`C:\Users\user\Documents\Codex\2026-10-27\make-way-ue-0-make-way-2\work\p3-multiplayer-ui\video-frames`|83|102,228,296|정상 MP4에 포함된 중간 프레임. 대표/상태 전환 PNG는 유지|

현재는 활성 Unreal Editor를 종료하지 않는다. 시작 조회에서 dirty map/content0, PIE 없음, 기존 캡처 callback 전부 해제를 확인했다. 이전 미저장 상태의 처리자·이유는 확정하지 않았으며 이번 작업에서 저장/폐기하지 않았다. Intermediate와 Unreal 에셋 삭제는 보류한다.

### PNG 정리 실제 결과

정상 MP4에 대응하는 중간 PNG **1817개 / 1,478,195,702 bytes**를 실제 삭제했다. 휴지통 이동이 아니다. 대표/상태 전환94개와 비디오7개, 기존 보고서의 직접 참조 PNG는 보존했다. 개별 결과: [png-delete-result.json](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-doc-consolidation/png-delete-result.json). 남아 있는 원본 프레임 metadata에는 당시 전체 파일 경로가 있으며, 삭제된 중간 프레임은 삭제 ledger의 MP4/프레임 번호/캡처 시간으로 대조한다. 이를 현재 PNG 전체가 남았다는 의미로 사용하지 않는다.

### ZIP·SHA 중복 삭제 전 기록

개별 절대경로·바이트·SHA·사유는 [ZIP 삭제 전 목록](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-doc-consolidation/zip-delete-plan.json) 및 [SHA 중복 삭제 전 목록](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-doc-consolidation/duplicate-delete-plan.json)에 고정했다. ZIP마다 모든 member의 SHA와 유지 파일 경로가 연결되어 있다. 일부 ZIP에만 있는 작은 README/JSON은 기존 검증 폴더의 retained-zip-records에 보존했다. 새 통합 ZIP은 만들지 않았다. 정적 JSON/TXT/로그·문서 및 직접 참조 경로는 SHA가 같아도 보수적으로 유지한다.

|삭제할 과거 ZIP 절대경로|bytes|사유|
|---|---:|---|
|`C:\Users\user\Documents\Codex\2026-10-27\make-way-ue-0-make-way-2\work\p3-cursor-original-shape\P3_VEHICLE_CURSOR_TRACKING_AND_ALIGNMENT_HANDOFF_v2.zip`|26,946,447|전체 member가 유지 파일에서 바이트 단위 복구 가능|
|`C:\Users\user\Documents\Codex\2026-10-27\make-way-ue-0-make-way-2\work\p3-cursor-tracking\P3_VEHICLE_CURSOR_TRACKING_AND_ALIGNMENT_HANDOFF.zip`|24,793,100|전체 member가 유지 파일에서 바이트 단위 복구 가능|
|`C:\Users\user\Documents\Codex\2026-10-27\make-way-ue-0-make-way-2\work\p3-fidelity-review\P3_VEHICLE_UI_FIDELITY_REVIEW_HANDOFF.zip`|129,489,618|전체 member가 유지 파일에서 바이트 단위 복구 가능|
|`C:\Users\user\Documents\Codex\2026-10-27\make-way-ue-0-make-way-2\work\p3-multiplayer-ui\P3_VEHICLE_MULTIPLAYER_UI_HANDOFF.zip`|43,708,885|전체 member가 유지 파일에서 바이트 단위 복구 가능|
|`C:\Users\user\Documents\Codex\2026-10-27\make-way-ue-0-make-way-2\work\p3-original-rebuild\P3_VEHICLE_ORIGINAL_SCREEN_REBUILD_HANDOFF.zip`|78,911,971|전체 member가 유지 파일에서 바이트 단위 복구 가능|
|`C:\Users\user\Documents\Codex\2026-10-27\make-way-ue-0-make-way-2\work\p3-player-panel-readability\P3_VEHICLE_PLAYER_PANEL_READABILITY_HANDOFF.zip`|95,833,354|전체 member가 유지 파일에서 바이트 단위 복구 가능|
|`C:\Users\user\Documents\Codex\2026-10-27\make-way-ue-0-make-way-2\work\p3-visual-polish\P3_UI_VISUAL_POLISH_HANDOFF.zip`|62,808,850|전체 member가 유지 파일에서 바이트 단위 복구 가능|

SHA 중복 PNG 후보: 44개 / 23,414,337 bytes. 유지할 동일 SHA 대표 경로는 각 항목의 retained_equivalent에 명시했다. 현재 보고서·HTML·정적 metadata에서 실제 사용하는 별도 경로는 삭제 목록에서 제외했다.

### 문서 삭제·이동 전 기록

[문서별 원래 경로·크기·SHA·사유·보존 원문·이동 목적지](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-doc-consolidation/md-cleanup-plan.json)를 이 절과 역사 인덱스에 삭제 전 기록했다. 대상11개 중 핵심5개는 Archive로 이동하며 본문 바이트를 바꾸지 않는다. 나머지6개는 Work 원문 동일 SHA 확인 후 프로젝트의 중복 MD만 삭제한다. 다른 팀 문서·공용 파일은 수정하지 않는다.

### 보존한 결정과 폐기한 방향

- 상단3+2·4+1 후보와 거대327px 카드 단일행은 당시 실험이다. 현행은 콘텐츠5대가 한 줄이 되도록 기존 Designer/동적 카드 구조를 설정한 상태다.
- The Raven 항목은 Mammoth 이름과 실제 Thumbnail로 교체됐다. 원본 T_vehicle_mammoth_icon, sharedassets0.assets Texture2D PathID251, PNG SHA `8f73ea63820e6264fe7f70bab2dd206f9824460638f3311f701e8a387847c040`의 출처·native pixel 대조는 Single Row 기술 기록과 work/p3-single-row-final/mammoth-native-source.json에 있다. Raven 리소스 파일 자체는 지우지 않는다.
- Fidelity 배경 실험은 비활성/복구된 이력이다. 숨김 Actor도 에셋 참조를 유지하므로 이번 정리에서 Actor/Material/Texture/레벨을 삭제하지 않는다.
- 커서 Arrow Rounded는 cropped Sprite428×500 대신 원본 논리512² Texture2D를 사용한 복원 이력이 있다. Unity RectTransform/Pivot과 원 밖으로 돌출된 삼각형은 원본에서 확인됐다. 현재 원형 커서의 색감·원격 opacity0.65와 로컬1 차이는 완전 원본 일치로 판정하지 않는다.
- TickTaskManager1992 assertion/PureVirtual524는 과거 재현 기록이다. PIE 종료·동기 map load·BP compile/Reinstancing/자동화 callback을 섞지 않는 안전 절차를 유지한다. 최종 실행의 Renderer AppTime38 handled ensure1건(로그2행)은 원인 미확정이며 이번 파일 정리로 고쳤다고 주장하지 않는다.
- 실제 서버 Ready/차량 선택 RPC/선택 복제/전원 확정/Pawn Spawn·Possess·트랙 단계 전환은 담당자 계약이 필요한 미구현 범위다. 선택 UI의 fixture 확정을 Ready/서버 승인으로 대신하지 않는다.

### 증거 보존 위치와 이동 영향

검증 자료 기본 경로는 `C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/`다. p3-title, p3-vehicle, p3-multiplayer-ui, p3-visual-polish, p3-original-rebuild, p3-alignment-correction, p3-fidelity-review, p3-single-row-final, p3-player-panel-readability, p3-latest-validation, p3-cursor-original-shape, p3-final-alignment-audit의 원본 분석·대표 PNG·원본 비교·영상·JSON·로그·고유 rollback은 보존한다. 이 경로들은 로컬 증거이며 Git 문서만 받아 다른 팀 PC에서 자동 접근되는 것은 아니다.

기존 보고서의 명시 로컬 링크90건(고유84경로)과 basename 증거124건은 정리 전 존재했다. 직접 참조 PNG는 삭제하지 않으며 정리 후 다시 검사한다. 과거 보고서에 쓰인 일부 ZIP 이름은 당시 산출 이력이다. 삭제한 ZIP은 zip-delete-result.json의 member→보존 파일/SHA 대응으로 추적하며 현재 ZIP 존재를 보장하는 링크로 해석하지 않는다.

### 주요 코드 위치

- [TitleRoot](../../../Source/MW/UI/Title/MWTitleRootWidget.cpp)
- [Vehicle selection](../../../Source/MW/UI/VehicleSelection/MWVehicleSelectionWidget.cpp)
- [Card](../../../Source/MW/UI/VehicleSelection/MWVehicleCardWidget.cpp)
- [Player Entry](../../../Source/MW/UI/VehicleSelection/MWVehiclePlayerEntryWidget.cpp)
- [Player Marker](../../../Source/MW/UI/VehicleSelection/MWVehiclePlayerMarkerWidget.cpp)
- [Preview host](../../../Source/MW/UI/VehicleSelection/MWVehicleSelectionPreviewActor.cpp)
- [AssetManager](../../../Source/MW/System/MWAssetManager.h)
- [Vehicle Definition](../../../Source/MW/Data/MWVehicleDefinitionData.h)
- [MultiplayerSubsystem](../../../Source/MW/System/MultiplayerSubSystem/MWMultiplayerSubsystem.h)

## 10. 실제 정리 결과 — 2026-11-02

|분류|실제 삭제 개수|파일 크기 합계 bytes|
|---|---:|---:|
|MP4 검증된 중간 PNG|1817|1,478,195,702|
|SHA 동일 중복 PNG|44|23,414,337|
|구성 파일 복구 가능한 과거 ZIP|7|462,492,225|
|통합 후 중복 MD|6|117,321|

모두 실제 파일 삭제이며 휴지통 이동이나 전체 폴더 복사로 용량 확보를 대신하지 않았다. 신규 통합 ZIP 생성0. 핵심 기술 기록5개는 Archive로 이동했고 SHA가 같았다. 최신 인계/고유 source·level·rollback ZIP8개, 원본 비교·최종 PNG·대표 영상·JSON·크래시 증거는 보존했다. 영상으로 확인되지 않은 원본 녹화 프레임은 지우지 않았다.

기존 명시 링크90건 모두 유효, 이번 새 깨짐0건. 전체 Docs 검사에서 트랙 가이드의 기존 깨진 링크5건을 확인했으며 해당 팀 문서는 해시가 같고 수정하지 않았다. 링크 검사에서 C++ 코드 예시2개를 제외했다. [링크 검사 결과](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-doc-consolidation/links-after-cleanup.json)를 따른다.

실행 시작16:27:44 KST의 디스크 기준과 별도로, 에디터 조회 시 미저장 패키지는0개였다. 실행 중16:28:05에 에디터 Saving Map 로그와 L_VehicleSelection_UI_Dev.umap 해시 변경을 관찰했다. 이번 정리 코드/Unreal 조회는 저장·폐기·종료·Actor 변경을 호출하지 않았다. 처리자와 변경 원인은 단정하지 않고 그 파일을 그대로 보존했다. 읽기 전용 Actor 비교에서는 최초 조회 이후 Transform/hidden/Actor 목록 차이0개였다. [보존 검사](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-doc-consolidation/execution-preservation-after.json) 및 editor-observed-external-change.json을 확인한다.

Intermediate는 삭제0. 에디터가 실행 중이고 종료 허가·이전 미저장 변경의 소유권을 확인하지 못했으므로 보류했다. 활성 Source/Config/.uproject/Blueprint/Texture·정식 L_Title은 그대로다. 이번에는 UE 재실행·PIE·BP compile·Editor/Game 재빌드를 하지 않았다. 이 정리는 활성 프로젝트/빌드 캐시를 수정하지 않았으며, 과거 테스트 결과를 정리 후 새 회귀 테스트로 주장하지 않는다. 기둥 커서 가림·Renderer ensure·실제 멀티플레이 미연결은 미해결 상태를 유지한다.

### 이후 작업 기준

소규모 P3 수정·검증은 이 파일의 현재 상태와 변경 이력만 갱신한다. FINAL_v2/FINAL_v3/REVIEW_NEW 등의 새 Markdown 보고서와 새 통합 ZIP을 반복 생성하지 않는다. 중요한 독립 아키텍처/기술 검증만 별도 문서 필요성을 검토한다. PNG/영상/JSON/로그는 기존 증거 체계에 보관하고 대표 자료만 본 문서에 연결한다.

### 용량·소스 관리 최종 대조

- 시작 여유 공간: 114,206,572,544 bytes (106.363 GiB).
- 정리 후 여유 공간: 113,911,087,104 bytes (106.088 GiB).
- 실제 관측 순증가: -295,485,440 bytes (-0.275 GiB). **볼륨 전체 여유 공간의 순증가는 음수이며, 양수 확보로 보고하지 않는다.**
- 실제 삭제1874개, 파일 크기 합계1,964,219,585 bytes. P3 대상 폴더의 실제 파일 용량은 새 감사 metadata를 포함해 1,958,012,774 bytes (1.824 GiB) 감소했다.
- 볼륨 전체의 별도 디스크 사용 변화가 겹쳤으며 원인은 미확정이다. 시스템/다른 작업 또는 파일시스템 요인을 실제로 확인한 원인처럼 쓰지 않는다. 삭제 합계·대상 폴더 감소·드라이브 여유 공간 순변화를 구분한다.
- feature/title-world-ui와 Git index를 유지했다. Git 변경은 P3 문서 통합/선택 Archive/중복 제거이며 기존 Source 변경은 그대로다. UVCS Pending 경로 집합은 전후 동일하고 로컬cs16 유지. Commit/Push/Checkin/Update/Switch/merge 없음.

[실제 정리 집계](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-doc-consolidation/CLEANUP_EXECUTION_RESULTS.json) 및 [대상 폴더 전후 용량](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-doc-consolidation/target-directory-size-after.json)을 확인한다. PNG 선별 보존 뒤 과거 인코딩 스크립트를 무심코 재실행해 보존 MP4를 축소 프레임 영상으로 덮어쓰지 않는다. 당시 전체 프레임 metadata와 원본 MP4가 기준 증거다.

## 11. P4 차량 선택 UI·서버 통합 준비 — 2026-11-02

### 11.1 조사 기준과 보존 범위

**현재 우선 계약:** `Server/Lobby-Design`의 자료형·RPC·Replication·Phase·게임 규칙을 그대로 소비한다. 클라이언트 유지를 이유로 서버를 변경하자는 제안은 하지 않는다. 2026-11-02 후속 지시에 따라 UI 내부 최소 구현을 진행했으며, 아래 11.8은 최초 문서 조사 이후의 변경 결과다.

Client + Server 통합 분석이다. 공용/분야 필독은 같은 대화에서 읽은 문서의 SHA 일치를 확인해 재사용했고, 새 서버 브랜치의 14~16번 계약과 실제 소스를 추가 대조했다. 아래 세 기준을 섞지 않는다.

| 기준 | 실제 상태 |
|---|---|
| 로컬 작업 트리 | `C:/workspace/UE/ProjectMW`, `feature/title-world-ui`, HEAD `3db9c5d15e5496a4b699cb48e7796d826622bfb4`. P3 차량 UI 파일은 미추적, 기존 UI 등의 미커밋 변경 존재 |
| GitHub 최신 main | API로 확인한 `6a47da9cea5fb07f75592004d11ee4192ddb60d3` / 2026-11-02 차량 데이터 수정. 로컬 origin/main과 같음. HEAD 대비 main에만 7커밋, HEAD에만 0커밋 |
| 서버 담당 브랜치 | GitHub API와 로컬 원격 ref 모두 `Server/Lobby-Design`의 `927fcffc6da7519d04565cb83ae44dc554d2fb65`. main에 병합되지 않은 별도 구현·설계이며 이번 채팅에서 적용 승인된 변경이 아님 |
| 에디터·UVCS | UE 5.8.4, Editor PID32736, 현재 `L_VehicleSelection_UI_Dev`. 조사 전후 dirty map/content 0/0. UVCS local cs16 / head23, 기존 Pending 보존, lock list 반환 항목 없음 |

Git fetch/merge/reset/rebase/stage/commit/push, UVCS checkin/update/switch, 에디터 저장/컴파일/맵 이동/PIE/종료는 수행하지 않았다. 타이틀 카메라·UCM·TA 도구·주행/물리·배경·정식 L_Title은 이번 조사 범위가 아니다. 기둥의 커서 렌더링·입력 가림과 Renderer ensure는 기존 미해결 항목으로 유지한다.

소스 기준 링크: [main 차량 데이터 변경](https://github.com/kevin6091/ProjectMW/commit/6a47da9cea5fb07f75592004d11ee4192ddb60d3), [서버 담당 16번 전환 계약](https://github.com/kevin6091/ProjectMW/blob/927fcffc6da7519d04565cb83ae44dc554d2fb65/Docs/Shared/Client,Server/16_로비_게임플레이_분리와_차량선택_전환_계약.md), [15번 연결 모듈 계약](https://github.com/kevin6091/ProjectMW/blob/927fcffc6da7519d04565cb83ae44dc554d2fb65/Docs/Shared/Client,Server/15_독립_연결모듈_API와_프로젝트_연동.md). 링크는 조사 SHA에 고정했다. 과거 문서의 성공 기록은 이번 실행 결과가 아니다.

### 11.2 로컬 UI와 main의 API 차이

| 조사 대상 | 로컬 P3 작업 트리 | main 6a47da9 / 영향 |
|---|---|---|
| `UMWVehicleDefinitionData` | FName VehicleId, FText DisplayName, PawnClass, UMWInputData DrivingInputData. `GetPawnClass`, `GetDrivingInputData` 제공 | VehicleId/DisplayName 유지. PawnClass·기존 InputData 멤버와 GetPawnClass 제거. `UMWVehicleDriveData* DriveData` 추가 |
| `GetDrivingInputData()` | `const UMWInputData*` 반환 | **이름은 같지만 `const UMWVehicleDriveData*` 반환**. 입력 액션 데이터로 해석하면 안 됨. 명칭 개선은 소유자 합의 대상 |
| `ValidateDefinition()` | ID, 구체적 PawnClass, InputData 검증 | ID 및 DriveData.ValidateConfig 검증. Pawn/입력 준비 완료를 보장하지 않음. 잘못된 DriveData에도 `DriveData is required`가 덧붙으므로 서버 오류 분류에 유의 |
| `UMWVehicleDriveData` | 클래스/파일 없음 | main에 추가. 감속·후진·조향·드리프트의 FMWVehicleDriveConfig와 범위 검증. UI 이름/아이콘/외형/생성 클래스는 없음 |
| `UMWAssetData` / `UMWAssetManager` | 태그→SoftObjectPath 카탈로그, 타입 로드, Root 하위 태그 정렬 조회 | 조사한 main과 동일. 차량 전용 승인 목록이나 ID 변환표를 자동 생성하지 않음 |
| `UMWVehicleSelectionWidget` | 미추적 P3 소스. 기존 FName 선택 Delegate를 FGameplayTag VehicleAssetID로 변경. Preview는 실제 ID 없음 | main에 파일 없음. UI는 카탈로그 태그/GetDisplayName/ValidateDefinition을 소비하며 제거된 GetPawnClass의 직접 호출은 없음. 병합 빌드 성공으로 해석하지 않음 |
| Preview host | 실제 클래스명은 **AMWVehicleSelectionPreviewActor**. PIE 전용 World Widget 수명 소유 | main에 파일 없음. 운영용 로비 UI host로 승인된 상태가 아님 |
| `AMWPlayerState`, `AMWGameState`, `AMWPlayerController` | 기존 등록/Ready·PlayerArray·OnLobbyChanged 경로 | local HEAD/main 동일. 차량 ID/선택 확정·차량 선택 RPC·RoomPhase 없음 |
| `UMWRoomLobbyWidget` | 기존 main 동작에 P3 Destruct 유효성 가드 추가. Ready/Start는 false로 비활성 | main도 Ready/Start 미구현. 서버 담당 브랜치의 작동하는 Ready/Start와 구분 |

main의 주행 구현에서 `AMWVehicleCharacter`는 BP 기본값의 `DrivingInputData`로 입력을 연결하고, `UMWVehicleMovementComponent`는 자신의 `DriveData`로 Config를 읽는다. Definition.DriveData를 해당 Component에 적용하는 연결은 조사 소스에 없다. `AMWDriveTestGameMode`가 DefaultPawnClass를 AMWVehicleCharacter로 지정하는 것은 **주행 테스트 경로**이며, 승인 VehicleId에 따른 경기용 Spawn/Possess 계약이 아니다.

따라서 새 구조의 책임은 VehicleId/표시명=Definition, 주행 수치=DriveData, 입력=Character의 UMWInputData, 실제 외형=차량 BP/컴포넌트 설정이다. **Definition에는 외형·Thumbnail·Pawn 클래스 매핑이 없다.** UI Thumbnail은 기존 WBP의 VehicleThumbnails 태그 맵에서 관리한다. 서버가 어떤 승인 Character BP를 생성하고 어떤 DriveData를 적용할지는 서버·차량 담당자의 결정이 필요한 공백이다. 클라이언트가 PawnClass나 임의 에셋 경로를 보내 이 공백을 메우지 않는다.

### 11.3 실제 로컬 데이터와 Preview 경계

AssetRegistry 로딩 완료 상태에서 `/Game` 152개 등록 에셋과 Definition/DriveData 파생 클래스 조회, 카탈로그 객체, WBP CDO를 읽었다. 에셋을 저장하거나 등록하지 않았다.

| 항목 | 확인 결과 / 입력 계약 |
|---|---|
| 승인 차량 | 로컬 `UMWVehicleDefinitionData` 0, `UMWVehicleDriveData` 0, `Asset.Vehicle` 하위 카탈로그 항목 0. 서버·차량 담당자가 승인 Definition 및 생성/입력/주행 매핑을 준비해야 함 |
| 카탈로그 | DefaultGame.ini의 StartupAssetData=`/Game/Data/DA_AssetData`. 4항목: `Asset.Input.Driving`, `Asset.Input.PlaceConstruction`, `Asset.Input.SelectConstruction`, `Asset.MapTheme.DevDefault`. PrimaryAsset scan 설정은 MWAssetData를 /Game/Data에서 수집 |
| 실제 UI WBP | `WBP_VehicleSelection_Dev`: preview_only=false, VehicleAssetRoot=Asset.Vehicle, PreviewCards/Scenarios/VehicleThumbnails 모두 0 |
| Preview WBP | `WBP_VehicleSelection_Preview`: preview_only=true, 카드 Smiet/Mammoth/Ufology/Doug/Fred 5개, 시나리오 10개, 실제 VehicleThumbnails 맵 0. 이미지 경로 모두 해당 Preview Texture2D로 확인 |
| 차량 외형 | 로컬 BP_VehicleVisual_Smiet의 부모는 Actor. 승인 주행 Pawn/Character로 간주하지 않음. 다른 팀의 미수신 UVCS 차량 BP·DA 존재 여부는 이번 조회로 확정할 수 없음 |
| 플레이어 이름·입퇴장 | 실제 PlayerArray/AMWPlayerState.GetPlayerName/GetLobbyMemberId가 출처. PreviewScenario의 DisplayName·SlotIndex는 개발 fixture |
| 선택·확정 | 실제 서버 계약은 승인 VehicleAssetID로 판정할 예정. PreviewCardIndex/bConfirmed는 네트워크 정본이 아니며 RPC 입력에 사용하지 않음 |
| 진입·종료 | 실제 로비 RoomPhase와 로컬 UI 소유자의 수명으로 제어할 대상. PreviewActor.OpenPreview/ClosePreview는 개발 경로 유지 |

카탈로그 tag와 Definition FName은 별개이며 문자열이 우연히 같다고 동일 ID로 가정하지 않는다. 이번 변경에서 Card.SetVehicle이 AssetManager가 반환한 AssetId를 그대로 보관하고 GetVehicleAssetId로 전달한다. Selection의 GetSelectedVehicleAssetId/OnSelectionRequested도 동일 FGameplayTag를 사용한다. FName→태그 변환이나 문자열 매핑은 없다. SetPreview는 태그를 EmptyTag로 초기화하며 Preview 선택 조회는 항상 EmptyTag다. 기존 5개 이미지에 이름으로 ID를 붙이거나 실제 Definition을 임의 생성하지 않는다. 서버와 클라이언트의 동일 카탈로그/패키징 준비도 별도 인수 항목이다.

### 11.4 서버 담당자가 이미 정한 방향과 아직 없는 구현

서버 브랜치 16번 문서의 채택 방향을 우선한다. 이전 P3 보고서의 별도 확정 bool, Epoch/Revision, 임의 RPC 명칭을 새 계약으로 재도입하지 않는다.

- 로비 책임은 `AMWLobbyGameMode` / `AMWLobbyGameState` / `AMWLobbyPlayerController`, 게임플레이는 각각 Gameplay 클래스. UI 생성·갱신·제거는 `AMWLobbyHUD`, 개인 상태는 공용 `AMWPlayerState`다.
- `EMWRoomPhase`에는 VehicleSelection이 있지만 **실제 HandleStartPrepared는 아직 Traveling→ServerTravel로 즉시 이동**한다. VehicleSelection 진입·선택 RPC·전원 확정 대기는 미구현이다.
- PlayerState에 `FGameplayTag VehicleAssetID=EmptyTag`, ReplicatedUsing 선언과 CopyProperties 복사는 있지만 **DOREPLIFETIME 등록·공개 Getter·서버 확정 Setter가 없다**. 현재 실제 복제되는 차량 결과라고 보고하지 않는다.
- 문서상 계약명 `RequestConfirmVehicle(VehicleAssetID)`는 아직 callable API가 아니다. 기존 `RequestSetReady`/`RequestStartGame` RPC만 구현돼 있다.
- 비어 있는 VehicleAssetID=미선택, 서버 승인한 유효 태그=확정. 별도 bConfirmed/선택 Revision/RequestId를 추가하지 않는 방향이다. 같은 차량 다인 선택은 문서에서 허용한다. 다른 ID로 확정 후 변경은 거절하며, 같은 ID 재요청은 멱등 처리한다.
- 확정 전 Hover/클릭 후보는 로컬 상태다. 문서에 원격 Hover/선택 중 후보 ID 복제가 없으므로 다른 플레이어의 미확정 차량 위치를 네트워크로 보여준다고 약속하지 않는다. 원격은 미선택/확정된 차량 상태를 표시하고 Preview 마커와 구분한다.
- 연결 경계는 `UMWGameInstance → MWConnection`이며 기존 `UMWMultiplayerSubsystem`은 BP 호환 전달자다. 새 UI가 EOS를 직접 구독하거나 별도 세션 상태를 보관하지 않는다.

**소유자에게 확인할 불일치:** 16번 문서의 서버 검증에는 Character 생성 클래스·입력 데이터 검증이 남아 있는데 main 6a47da9 Definition에서는 해당 참조가 제거됐다. 승인 생성 클래스·입력/DriveData 연결 위치를 합의해야 한다. 태그 또는 ValidateDefinition 통과만으로 실제 주행 차량 준비를 인정할 수 없다. HUD의 기존 고정 WBP 경로/초기 바인딩 재시도 상수도 서버 브랜치의 기존 구현이며, 새 UI 코드에 복사하거나 이번에 담당자 파일을 수정하지 않는다.

### 11.5 서버 담당자에게 전달할 최소 인터페이스·데이터 흐름

아래는 **서버가 이미 채택한 계약을 클라이언트가 소비할 연결 명세**다. 서버 정책 변경 제안이 아니며 이번에 서버 API를 추가하지 않았다. `RequestConfirmVehicle`만 기존 담당 문서의 예정 이름을 사용한다. Getter/Setter/오류 전달의 최종 서명은 담당자가 확정한다.

| 단계 | 호출 주체·소유 Actor / 입력 | 서버 검증·정본 | UI 반영 / 실패·취소·이탈 |
|---|---|---|---|
| 로비 준비 | 각 소유 LobbyPC.RequestSetReady(bool), 호스트 RequestStartGame → LobbyGM | 기존 최소 인원·정원·ID·호스트·전원 Ready, GS.RoomPhase=Lobby | HUD가 GS.OnLobbyChanged 및 PS.OnPlayerStateChanged를 읽음. 실패는 기존 PC.OnLobbyActionErrorChanged. Ready는 차량 확정으로 재사용하지 않음 |
| 선택 진입 | LobbyGM.HandleStartPrepared, GameInstance의 고정 StartingPlayers | 입장 잠금 완료·현재 참가자 재검사 후 **VehicleSelection으로 전환하는 구현 필요** | HUD가 현재 Phase 초기 조회 후 World UI 표시. 늦게 생성된 UI도 현재 snapshot 표시. 아직 바로 Travel하는 경로를 서버 담당자가 분리 |
| 로컬 후보 | 차량 Card/SelectionWidget의 Hover·클릭 / 승인 카탈로그 태그 | 이 단계에서는 서버 상태를 변경하지 않음 | 기존 자유 커서·클릭 고정 유지. 우클릭은 미전송 로컬 후보만 취소. 다른 클라이언트의 미확정 후보는 추정하지 않음 |
| 확정 요청 | 로컬 UI Confirm → **자기 LobbyPC.RequestConfirmVehicle(FGameplayTag)** → 소유 Server RPC → LobbyGM | VehicleSelection, 고정 시작 대상·활성 참가자·소유권, 서버 승인 카탈로그의 Definition/ID/DriveData/생성·입력 준비 검사. 클라이언트 PawnClass·경로·타인 PlayerId는 받지 않음 | 요청 중 Confirm/카드 중복 입력 차단. 기존 UI Delegate를 FGameplayTag로 직접 변경 완료. 별도 어댑터 없음. 실제 PC 연결은 서버 API 구현 후 적용 |
| 서버 승인·복제 | LobbyGM → 자기 PlayerState의 서버 전용 확정 경로 | VehicleAssetID를 서버에서만 기록, DOREPLIFETIME 및 OnRep/Listen Host 알림 필요. 같은 ID 재요청은 중복 확정 없음, 다른 ID 변경 거절 | 소유자는 PS 확정 태그를 읽어 pending 해제 후 잠금, 원격도 같은 태그로 이미지/마커 갱신. 로컬 성공 문구로 정본을 대체하지 않음 |
| 거절·취소 | 소유 PC 오류 이벤트 → UI; 방 이탈은 기존 GameInstance.RequestLeaveRoom | 잘못된 ID/Phase는 정본 불변. UI 후보 취소는 서버 확정 취소가 아님. **확정 취소 RPC는 현재 계약에 없음** | 거절은 pending 해제·오류·재선택 가능. 성공 후 RMB/Reset으로 서버 확정을 지우지 않음. Back으로 숨기기만 해서 서버 대기를 방치하지 않도록 방 이탈 의미와 활성 시점을 담당자와 합의 |
| 입퇴장·실패 | LobbyGM/연결 모듈, GS.PlayerArray·StartingPlayers | 선택 중 신규 입장 차단. 고정 대상 이탈/연결 실패·필수 데이터 준비 실패·대기 초과는 담당 문서의 Failed/방 정리 정책. 제한시간 값·오류 통지는 담당자 결정 | HUD가 참가자 구독 해제/빈 슬롯/마커 정리. Phase·연결 종료 시 pending 및 Widget 해제. 재접속/늦은 참가를 UI가 임의 허용하지 않음 |
| 전원 확정·종료 | LobbyGM, 고정 대상 전체의 VehicleAssetID | 전원 승인·참가자 유지·테마 유효성 검사 후 Traveling에서 한 번만 ServerTravel. 추가 Ready/두 번째 시작 없음 | HUD/World UI·마우스/Delegate 정리, 다음 월드에서 상태 재조회. 전환 순서를 가정하지 않음 |
| 이후 게임플레이 | GameplayGM/GS, 공용 PS.CopyProperties | PreparingConstruction→TrackPlacement→ElementPlacement→PreparingRace에서 승인 차량 생성·Possess→Countdown→Racing은 후속 범위 | UI는 단계 표시만 소비. Spawn/Possess를 차량 선택 UI가 수행하지 않음 |

최소 소비 인터페이스는 ① RoomPhase·PlayerArray·참가자 ID/이름 조회 ② 승인 카탈로그 태그 목록과 Definition 조회 ③ PS의 확정 VehicleAssetID 조회·변경 알림 ④ 소유 PC 확정 요청과 거절 알림 ⑤ Phase/연결 종료 알림이다. 새로운 범용 Manager·Subsystem·복제 선택 구조체는 필요하지 않다. 서버 채택안에 없는 확정 취소/변경은 구현하거나 요구하지 않는다. UI는 확정 태그를 읽어 기존 입력 경로를 잠그며, 이 잠금의 실제 연결은 PS 조회·복제가 제공된 후 수행한다.

### 11.6 재사용 코드와 UI 측 보완 범위

| 기존 함수/구조 | 재사용할 내용 | 정적 조사에서 확인한 제한과 최소 보완 |
|---|---|---|
| SelectionWidget.RefreshVehicles / AddCard / ClearCards | Root 태그 동적 조회, Definition 소유자의 ValidateDefinition 검증, 기존 Designer Wrap/Grid, Card Delegate 해제 | 수정 완료: pending 중 Refresh는 즉시 반환. 실제 모드의 후보는 동일 GameplayTag가 여전히 유효할 때만 복원하고 pin을 유지. 사라진/무효 후보는 해제하고 승인으로 표시하지 않음. UI 자체의 중복 FName 검사 제거. 서버 승인 목록 검증은 대체하지 않음 |
| Card.SetVehicle / GetVehicleAssetId / SetInteractionLocked | 기존 이미지/이름/프레임, 마우스 이벤트·잠금 | 수정 완료: 카탈로그 태그 직접 보관, 불필요한 Definition 객체 보관 제거. 무효 태그/Definition 카드 선택 불가. Preview 이름/순서로 매핑하지 않음 |
| SelectionWidget.RefreshPlayers / BuildPreviewParticipants / BuildLobbyParticipants / RenderParticipants | 기존 참가자 생성 코드를 같은 클래스의 두 private 함수로 분리, 공통 FMWVehiclePlayerPresentation→Entry 유지. Preview는 GS를 구독하지 않음. 실제 PlayerArray 이름·정원, 안정 좌석, 이탈 weak key 제거 유지 | 실제 분기는 VehicleName/Thumbnail/Confirmed를 채우지 않고 MissingServerSelectionText만 표시. 서버 태그 조회 후 카탈로그로 해석하는 경로 필요. PS 교체·일시적인 미수신·정원 변경 검증 |
| RefreshCardPointers / RefreshLocalCursor | 로컬 추적 CursorLayer와 원격 카드 마커의 분리 | 원격 카드 마커는 현재 bPreviewOnly 분기에만 있음. 실제 확정 태그별 참가자 grouping이 필요. 같은 차량 다인 선택도 기존 SetParticipants의 Grid 재사용. 이탈 시 해당 marker 구독·표시 정리 |
| Entry.SetPresentation / Marker.SetPresentation | 이름·색상·차량 이미지·확정색·로컬 강조, 기존 WBP Designer | Presentation.bConfirmed는 서버 bool이 아니라 승인 태그에서 유도한 화면 값으로 유지 가능. 알 수 없는 태그는 임의 차량으로 대체하지 않고 누락 표시/로그, 이후 카탈로그 준비 시 재표시 |
| HandleConfirm / FinishSelectionRequest | pending 동안 카드·Confirm·Back·Refresh 차단, 요청 Delegate | FinishSelectionRequest는 실패/로컬 요청 해제용이며 실제 서버 승인 처리로 연결하면 안 됨. 실제 서버 확정 잠금은 아직 적용하지 않음. 오류/승인/Phase 종료를 구별해 갱신해야 함. 현재 응답 누락 시 대기 해제 경로가 없으며 제한시간/실패 정책은 서버 계약과 연결 |
| CancelLocalSelection / HandleReset / HandleBack | Preview의 우클릭·재선택·Back 유지 | 현재 RMB는 실제 모드에서도 로컬 SelectedCard만 지움. **서버 취소 성공으로 표시하면 안 됨.** Preview 확정 해제는 fixture에만 적용. 실제 확정 후 잠금/Back 의미는 위 계약을 따름 |
| NativeConstruct/NativeDestruct, AddUniqueDynamic/RemoveDynamic | 기존 GC 참조, Delegate 수명, Window 비활성 커서 정리 | RefreshPlayers 최초 호출 시 GameState가 없으면 그 함수만으로는 늦은 도착 재시도 없음. 서버 브랜치 HUD의 초기 조회/바인딩 재시도/GS·PS 구독 패턴을 재사용하되 중복 구독 주체는 하나로 정함 |
| AMWVehicleSelectionPreviewActor.OpenPreview/ClosePreview | 개발 Preview와 World Widget 소유/해제·Back 회귀 환경 | PIE 전용·GetFirstPlayerController 경로라 운영 host로 그대로 사용하지 않음. 정식 로컬 HUD가 소유 Controller와 BP 설정 WidgetComponent를 연결하도록 별도 적용 승인 필요. 카메라/배경은 이번 작업에서 변경하지 않음 |

FMWVehiclePlayerPresentation은 출력용 구조체이고 현재 public snapshot 입력 함수는 없다. 이번에 RefreshPlayers의 Preview/실제 참가자 작성 부분을 기존 클래스 내부 private 함수로 분리하고 공통 RenderParticipants 호출을 유지했다. 서버 정본을 주입하는 임시 public setter나 새 캐시는 만들지 않았다. 기존 Entry/Marker/카드 레이아웃을 다시 만들 필요는 없다. 실제 서버 값의 새 캐시나 Subsystem은 만들지 않는다.

### 11.7 독립 준비 작업·대기 항목·통합 순서

**이번에 바로 구현한 범위:** 카탈로그 GameplayTag의 카드→후보 조회→요청 Delegate 전달, Preview 태그 격리, pending 중 Refresh 차단, 유효한 로컬 후보의 태그 기반 목록 갱신 복원, 기존 참가자 Presentation 작성 경로 분리, NativeDestruct에서 로컬 pending 정리. 클래스·Subsystem·RPC·에셋은 추가하지 않았다.

**서버 구현 완료를 기다리는 범위:**

1. 담당자의 PS VehicleAssetID 조회·실제 복제·변경 알림과 소유 LobbyPC 선택 요청/거절 API.
2. VehicleSelection Phase의 실제 진입 및 전원 확정/이탈/실패 처리. 현재 UI는 Phase 변경이나 ServerTravel을 실행하지 않는다.
3. 서버 브랜치가 제거한 옛 AMWGameState/Controller를 사용하는 클라이언트 include/cast/구독 경로 이전. 담당자 코드가 로컬에 승인 통합된 후 해당 클라이언트 소비부를 변경한다.
4. 기존 LobbyHUD의 UI 소유/초기 조회/GS·PS 구독 방식을 재사용한 World Widget 연결. 이번에는 서버 소유 HUD도 수정하지 않는다. 초기 snapshot을 즉시 읽고, 참가자 추가/제거에 맞춰 PS 구독을 갱신하며, UI 제거·Phase 종료에 모두 해제하도록 연결한다. 중복 구독 주체는 만들지 않는다.
5. PS의 승인 태그가 유효하면 기존 Confirm/카드/우클릭 취소/Reset을 잠근다. 승인 전 로컬 후보·pending과 확정을 구분하며, 거절은 확정으로 표시하지 않는다. Preview의 자유 재선택은 별개다. 원격은 확정 태그만 카드별로 모아 기존 SetParticipants로 표시한다. 별도 서버 bool/Revision이나 원격 Hover RPC를 요구하지 않는다.

**차량 데이터 소유자에게 확인할 사항:** 승인 GameplayTag→Definition의 실제 DA_AssetData 등록, 표시명/Thumbnail 연결, main의 DriveData 검증 및 차량 외형·입력·생성 클래스 소유 위치, 서버가 승인 태그로 실제 생성할 준비 상태. 태그 규격이나 확정 정책을 재협상하는 질문이 아니다. Definition.FName을 네트워크 ID로 사용하지 않으며 Preview 5개를 승인 차량으로 간주하지 않는다.

권장 순서는 **승인 차량 데이터 준비 + 담당 서버 계약 구현 → 허가받은 로컬 브랜치 동기화 → 기존 HUD/PS 이벤트와 UI 소비부 연결 → 실제 복수 클라이언트 시험**이다. UI는 Lobby → Starting → VehicleSelection → Traveling → PreparingConstruction을 읽고 따르며 변경하지 않는다. 같은 차량 선택 허용, 동일 ID 재요청 멱등, 확정 후 다른 ID 변경·취소 금지는 서버의 확정 정책이다.

**남은 차단 요소:** 실제 승인 차량 0/Thumbnail 맵 0, 새 Definition의 생성·입력 연결 미준비, 선택 진입/RPC/PS 복제/전원 대기 미구현, 운영 HUD와 World UI 연결 미적용. FName/GameplayTag의 UI 전달 자료형 차이는 이번에 제거했지만 서버 통합 완료를 뜻하지 않는다. 개발 레벨 좌우 기둥의 로컬 추적 커서 표시·입력 가림은 11.10에서 수정했으며 종료 전 검증 범위는 11.11을 따른다. Renderer ensure의 근본 원인은 미해결이다. 이 둘을 서버 통합의 자료형·복제 계약과 혼동하지 않는다.

통합 후 Host+원격 클라이언트에서 초기/늦은 UI 생성, 각자 Confirm, 승인/거절/중복/잘못된 ID/잘못된 Phase, 같은 차량 다인 선택, 확정 후 RMB 변경 차단, 이탈/시간초과, 전원 확정 전 Travel 금지·후 1회 이동, PS 값 인계·구독 해제를 검증한다. 기존 2·3·4인 Preview는 이 서버 시험을 대신하지 않는다.

### 11.8 이번 변경·검증 결과

2026-11-02 후속 서버 계약 우선 지시 반영. 최초 P4 조사에서는 이 문서만 수정했으나, 이번에는 아래 기존 UI 소스 4개를 최소 수정했다.

| 변경 파일 | 변경 함수·내용 |
|---|---|
| Source/MW/UI/MWVehicleCardWidget.h/.cpp | SetVehicle/SetPreview/GetVehicleAssetId: 카탈로그 GameplayTag 보관·Preview ID 분리. Definition 객체 중복 보관 제거 |
| Source/MW/UI/MWVehicleSelectionWidget.h/.cpp | OnSelectionRequested/GetSelectedVehicleAssetId/HandleConfirm: FGameplayTag 전달. RefreshVehicles: pending 보호·유효 후보 복원. BuildPreviewParticipants/BuildLobbyParticipants: 기존 구현 분리. NativeDestruct: pending 정리 |
| Docs/Engineering/Client/FRONTEND_UI_MASTER.md | 기존 서버 계약의 확정 정책, 구현과 대기 범위, 검증 결과 갱신 |

새 클래스·RPC·서버 상태·ID 매핑·하드코딩된 차량 개수/이름/에셋 경로/좌표는 추가하지 않았다. 서버·차량·EOS 코드를 변경하지 않았으며 WBP/Config/레벨도 수정하거나 저장하지 않았다. 기존 카드/명패/마커 디자인, 카메라, 배경, 마우스 이벤트 처리와 Preview fixture는 보존했다.

- GitHub live ref 재확인: main `6a47da9`, Server/Lobby-Design `927fcff`. 최신 미구현 API를 구현된 것으로 표시하지 않음.
- 헤더 변경 전 기존 프로젝트 BP/WBP 25개 graph 조회에서 이전 GetSelectedVehicleId/OnSelectionRequested/FinishSelectionRequest 참조 0건. 외부 Blueprint 서명 사용처 없음.
- 빌드 초기 실행은 UBA 캐시 디렉터리 접근 거부로 중단. 다음 실행에서 C++ 컴파일은 통과했지만 실행 중 Editor의 DLL 잠금으로 LNK1104 발생. 정상 종료 전 dirty map/content 0/0 재확인 후 quit_editor로 종료했으며 강제 종료/자동 저장/Hot Reload 없음.
- 최종 UE 5.8.4 MWEditor Win64 Development 및 MW Win64 Development 빌드 성공. Game 빌드의 XGE 라이선스 경고는 standalone 빌드로 진행됨. 서버/main 병합 빌드가 아니라 현재 UI 브랜치의 빌드다.
- 새 DLL을 로드한 별도 commandlet에서 기존 BP/WBP 25개 모두 컴파일 성공(에셋 저장 없음). Preview CDO의 5종 Smiet/Mammoth/Ufology/Doug/Fred·10시나리오 유지, 실제 WBP의 Preview=false·fixture 0 유지. 이번에는 PIE/실제 마우스 회귀를 재실행하지 않았으므로 시각·입력 통과로 해석하지 않는다.
- 보호 대상 275파일 SHA 대조: 위 UI 소스 4개와 Master만 변경, 추가/삭제 0. Content/Config/나머지 Source 및 MW.uproject 모두 동일. Git HEAD·status 경로 집합 유지, UVCS Pending 101경로 동일, 잠금 목록 비어 있음. Master 명시 로컬 링크 30개 모두 유효. stage/commit/push/merge/checkin/update/switch 없음. 에디터는 미저장 변경 없이 정상 종료된 상태다.
- 실제 서버 선택·Replication·복수 클라이언트·EOS 로그인 시험은 미실행이며, 미구현 서버 API 때문에 통합 보류 상태다. 이번 소스/컴파일 검증을 시각·마우스 회귀 시험으로 주장하지 않는다.

검증 로그는 기존 `work/p3-doc-consolidation/` 체계에 `p4-client-editor-build-final.log`, `p4-client-game-build.log`, `p4-client-blueprint-validation.log/json`으로 보관한다. 새 P4 보고서/ZIP은 만들지 않았다.

### 11.9 타입 전환 후 실제 PIE 회귀 검증 — 2026-11-03

**판정: 이번에 검증한 P3 Preview 핵심 마우스 입력은 통과. FGameplayTag 전환으로 인한 새 입력 회귀를 발견하지 않았다.** 서버 통합 완료 판정이 아니다. 11.8의 빌드 소스 그대로 검증했으며 C++/WBP/에셋/Config/레벨 추가 수정은 없다.

11월 2일 Windows 보안 창으로 중단했던 검증을 사용자 재개 지시 후 이어갔다. 보안 창이 없는 것을 관찰하고 현재 저장된 `/Game/Levels/L_VehicleSelection_UI_Dev`에서 New Editor Window PIE를 마우스로 시작했다. 실제 UEDPIE 월드와 Standalone Preview Actor를 확인했다. 시작/종료 dirty map/content는 0/0이다. 게임 Controller의 실제 ViewportSize 조회는 **1920×1080**이며, 보존한 Windows 도구 캡처는 창 테두리를 포함한 **966×578 축소 이미지**다. 이를 원본 해상도의 픽셀 측정 자료나 별도 960×540 실행으로 주장하지 않는다.

| 검증 | 실제 입력·관찰 | 결과 |
|---|---|---|
| 5종 표시 | Smiet/Mammoth/Ufology/Doug/Fred 단일 행, 실제 카드·하단 이미지 확인 | 통과 |
| Hover | 실제 마우스를 Mammoth로 이동(0-delta scroll 입력으로 포인터 이동, 클릭 없음). 로컬 하단 이미지 Mammoth로 바뀌지만 SelectedDisplayName은 빈 값 | 통과: Hover와 선택 분리 |
| 클릭·고정 | Mammoth 클릭 후 마우스를 Fred로 이동. 원형 마커는 Mammoth 클릭 위치에 남고 선택명도 Mammoth 유지 | 통과 |
| 확정·다른 카드 차단 | 선택 버튼 클릭 후 Preview locked=true. Fred 실제 클릭 뒤에도 Mammoth/확정 유지 | 통과 |
| Preview 취소·재선택 | 우클릭으로 로컬 선택명/fixture 확정 해제. Fred 재클릭 성공. 다시 선택 버튼으로 선택명 해제·하단 미선택 복귀 | 통과. 서버 확정 취소 시험이 아님 |
| 다인 Preview | 실제 사례 변경 버튼으로 3인 A/B·서로 다른 차량, 2/2, 2/4, 4/4 전환. 2/4는 빈 패널 2개. 4/4에서 Doug 클릭·확정하여 다른 참가자와 같은 차량 표시 확인 | 통과. 실제 다계정/네트워크 시험 아님 |
| 목록 갱신·유효 선택 | 2인에서 Fred 클릭 후 실제 새로고침 버튼 클릭 | Fred 선택명·하단 이미지 유지 |
| 목록 갱신·누락 선택 | 재진입 후 Fred 클릭. 살아 있는 **Transient Preview 인스턴스** 배열에서 마지막 카드만 임시 제거한 뒤 실제 새로고침 버튼 클릭 | 4카드, 선택명 빈 값, 하단 차량 선택 중, 확정 버튼 비활성 확인. DA/카탈로그/WBP 저장 없음 |
| 테스트 복구 | 같은 Transient 배열에 원래 Fred 항목 복원 후 실제 새로고침 | 5종 복원. Preview는 기존 CardIndex를 보관하므로 같은 항목이 돌아오면 Fred를 다시 표시함. 누락 중 표시/확정 차단 검증이며 Preview 인덱스의 영구 삭제나 서버 카탈로그 변경 시험은 아님 |
| UI 종료·재진입 | 실제 뒤로 클릭으로 WidgetComponent의 widget=null 확인. 기존 개발 host.OpenPreview API로 재진입 후 실제 Fred 클릭 | 새 Widget 인스턴스 _C_1, 기본 3인 시나리오, 입력 정상. 재진입 API 호출을 마우스 메뉴 진입으로 주장하지 않음. 중복 OpenPreview는 false |
| 서버 ID 격리 | Hover/선택/확정/취소/다인/갱신/재진입마다 공개 GetSelectedVehicleAssetId를 GameplayTagLibrary.IsGameplayTagValid로 조회 | 전 상태 false. Preview fixture의 bConfirmed를 서버 승인으로 표시하거나 PS에 등록하지 않음 |
| 종료 | 두 번째 뒤로 실제 클릭 후 widget=null, EditorRequestEndPlay로 PIE 종료. 이어서 별도 호출로 종료/dirty 조회 | PIE=false, dirty map/content 0/0. ESC 입력·동기 맵 로딩·BP 재컴파일 없음 |

기존 **기둥의 커서 렌더링·입력 가림은 미해결**로 유지한다. 이번에는 기둥을 피한 조작 영역에서 핵심 동작을 확인했으며 해당 결함의 해결을 주장하지 않는다. Preview 새로고침 때 유효한 차량 선택은 유지되지만 커서 pin은 기존 Preview 경로대로 풀린다. 운영 서버의 확정 이후 잠금/취소 금지, 승인 차량 태그의 재생성 복원, 실제 Replication/전원 완료/Travel은 승인 데이터·서버 구현 후 별도로 검증해야 한다.

이번 실행 구간의 엔진 로그에서 fatal/assertion/ensure는 발견되지 않았다. 기존 Renderer AppTime ensure의 근본 원인을 해결한 것은 아니다. EOS 초기화/RTC tick 지연/P2P 종료 경고가 있었다. 검증 보조 Python에서 GameplayTagLibrary 이름, protected Cards 조회, EditDefaultsOnly 인스턴스 setter 때문에 3회 예외가 있었으며 게임 크래시와 구분한다. 공개 함수 조회로 정리하고, 누락 테스트는 반환된 Transient fixture 배열의 항목 제거/복원으로 수행했다. 프로젝트 코드의 접근 지정자나 설정을 변경하지 않았다.

**보존 증거(3장만):**

- [2인 선택 화면](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p4-preview-regression/PIE_2P_SELECTION.png)
- [3인 Mammoth 확정·Fred 클릭 차단](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p4-preview-regression/PIE_3P_CONFIRMED.png)
- [4인 Doug 확정·동일 차량 표시](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p4-preview-regression/PIE_4P_CONFIRMED.png)
- [입력 후 공개 상태 조회 로그](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p4-preview-regression/input-state.jsonl)
- [해당 PIE 실행 로그 발췌](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p4-preview-regression/engine-excerpt.log)

11.8 빌드 완료 당시의 275개 보호 파일과 대조하여 이 Master 이외에 변경/추가/삭제가 없음을 확인했다. 서버·C++·WBP·카메라·배경은 동일하다. feature/title-world-ui 및 기존 Git/UVCS Pending을 보존했다. 새 보고서 MD/ZIP/연속 PNG 없음, stage/commit/push/merge/checkin/update/switch 없음. 실행 코드가 변하지 않아 같은 소스를 재빌드하거나 PIE 중 WBP를 재컴파일하지 않았다.

### 11.10 기둥 입력·표시 가림 수정 및 원형 마커 단일 표시 — 2026-11-03

**현재 동작:** 미선택 → 원형 하나가 실제 마우스를 추적. 차량 클릭 → 추적 원형을 해제하고 클릭 위치에 원형 하나를 고정. 마우스를 다른 카드로 이동해도 원형을 추가 생성해 따라오게 하지 않음. Preview 우클릭/다시 선택 → 고정 표시를 숨기고 원형 하나가 다시 추적. 선택 데이터와 좌표는 기존대로 독립이다. 다른 참가자의 카드 마커는 별개 Preview 표시이며 실제 네트워크 커서가 아니다.

처음에는 요청의 자유 커서/고정 마커 분리를 두 원형의 동시 표시로 구현했으나 사용자가 직접 중복 표시를 지적했다. **그 구현은 폐기했다.** 위 단일 표시 동작이 최종 사용자 정정 지시이며, 중간 `ENGINE_SOFTWARE_CURSOR.png`와 FINAL 접두사 이전 로그를 최종 동작 증거로 사용하지 않는다.

**원인 분리와 수정:**

- 수정 전 실제 마우스가 기둥 옆에 있을 때 원형의 일부가 기둥 뒤에 가려졌다. 기둥 위에서는 Visibility Trace가 `StaticMeshActor_7.StaticMeshComponent0` (`REBUILD_BoardSupport_L`)에 먼저 충돌했고 사례 변경 버튼 클릭이 차단됐다.
- PIE 사본에서 해당 컴포넌트의 Visibility만 임시 Ignore로 바꾸자 동일 클릭 위치의 hit가 `BP_VehicleSelectionPreview_Dev_C_1.PreviewSurface`로 바뀌고 사례 7→8 전환이 성공했다. 원형의 시각적 가림은 여전히 남았다. 따라서 Trace와 Depth를 별도 문제로 확인했다.
- 표시: 기존 SelectionWidget이 기존 Marker WBP 클래스의 인스턴스를 소유하고 `UGameViewportClient` 소프트웨어 커서에 등록한다. Slate가 실제 마우스 위치에 그리므로 월드 기둥 Depth와 무관하다. 크기는 기존 LocalPlayerCursor Designer Slot과 프로젝트 DPI 규칙을 사용한다. 새 Tick, 좌표/해상도 분기, 새 클래스, 에셋 경로 문자열은 없다.
- 수명: 기존 커서 매핑/사용 설정을 보관하고 선택·종료 시 복원한다. 다른 소유자가 매핑을 교체했다면 덮어쓰지 않는다. 약한 Slate 바인딩과 UPROPERTY Transient 참조를 사용한다. 기존 월드 `LocalPlayerCursor`는 고정 표시만 담당하며, fixture의 초기 로컬 선택도 기존 카드 Grid를 사용해 하나만 표시한다. 로컬 미선택 fixture의 가짜 고정 Hover 마커는 표시하지 않는다.
- 입력: `L_VehicleSelection_UI_Dev`, `L_Title_WorldUI_Dev`의 `REBUILD_BoardSupport_L/R` 네 컴포넌트 인스턴스만 Custom profile / Visibility Ignore로 저장했다. Camera/Pawn은 Block 유지. 메시, Transform, 재질, 전역 Collision 설정 및 전체 UI Depth Test는 변경하지 않았다. 공유 Cylinder 에셋도 그대로다.
- 재실행 과정에서 ProjectMW Editor 두 개가 같은 맵을 열어 저장 실패가 발생했다. 사용자의 종료 지시 후 먼저 열린 PID 4536을 미저장 없는 상태에서 정상 종료했다. 검증용 에디터에서 다시 저장하고 두 레벨을 재로드하여 Custom/Ignore가 실제 유지되는 것을 확인했다. 임시 PIE 응답 변경만으로 저장 완료라고 판정하지 않았다.

**변경 파일:**

| 파일 | 최종 변경 |
|---|---|
| Source/MW/UI/MWVehicleSelectionWidget.h/.cpp | RegisterSoftwareCursor / ReleaseSoftwareCursor / GetSoftwareCursorSize, RefreshLocalCursor의 추적·고정 상호 배타 표시, RefreshCardPointers의 로컬 중복 제거. 기존 선택/확정/취소 및 FGameplayTag 전달 계약 유지 |
| Content/Levels/L_VehicleSelection_UI_Dev.umap | 좌우 장식 기둥의 Visibility 응답만 변경 |
| Content/Levels/L_Title_WorldUI_Dev.umap | 같은 개발 장면의 좌우 장식 기둥에 동일 응답 적용 |
| Docs/Engineering/Client/FRONTEND_UI_MASTER.md | 현재 상태와 이번 재현·검증·제한 갱신 |

WBP, 카드/명패/마커 Sprite, 서버 소유 클래스, EOS, 차량 데이터, Config, 정식 L_Title은 수정하지 않았다. 이번 원형 표시 변경을 서버 확정 취소 허용이나 실제 차량 등록으로 해석하면 안 된다.

**최종 저장본 실제 PIE 입력:**

| 항목 | 결과와 범위 |
|---|---|
| 기둥 위 추적 | 미선택 원형 하나가 좌측 기둥 위에서도 가려지지 않고 표시. 실제 hit는 PreviewSurface |
| 클릭·이동 | Mammoth 클릭 후 Fred로 마우스 이동. 선택명 Mammoth 유지, 고정 좌표 `(763.513,422.513)` 유지, 이동하는 추가 원형 없음 |
| 확정 차단 | 실제 선택 버튼 클릭 후 locked=true. Fred 실제 클릭에도 Mammoth 유지 |
| 우클릭 | 선택명 해제, locked=false, 고정 marker Collapsed, 원형 하나가 실제 마우스 추적 재개. Preview fixture 동작이며 서버 확정 취소가 아님 |
| 좌우 기둥 뒤 입력 | 좌측 사례 버튼의 가려진 위치 실제 클릭으로 7→8. 우측 기둥 뒤 새로고침 Hover/클릭 및 PreviewSurface hit 확인 |
| 다인 | 3인에서 위 전체 순서 확인. 기존 public fixture 함수로 2인·4인 설정 후 실제 다시 선택/카드 클릭 확인. 4인 Fred 선택 후 원격 3개와 로컬 1개 구분. fixture 함수 호출을 실제 서버 참가나 마우스 시나리오 전환으로 주장하지 않음 |
| Back·재진입 | 실제 Back→widget=null. 기존 host.OpenPreview API→새 `_C_1` 인스턴스, 기본 3인 미선택·추적 복원. 추적 중 실제 Back에서도 원형 제거 확인 |
| 가장자리·포커스 | 우측 화면 가장자리까지 추적, 창 경계에서 정상 clipping. 제목 표시줄로 이동하면 mouse=null 및 원형 잔상 없음. 다른 창으로 포커스가 나갔다가 돌아오는 상황도 관찰. 다른 창 위에 게임 원형을 계속 그리지 않음 |
| ID 격리 | 최종 로그의 모든 선택/확정/취소 상태에서 server_tag_valid=false. 승인 차량 DA나 서버 정본 생성 없음 |

최종 위 입력 실행의 실제 ViewportSize는 **960×540**이다. 최종 엔진 `shot showui` PNG는 창 장식을 포함한다. 수정 전/중간 실행은 1920×1080이었으므로 수정 전후 자료를 동일 해상도의 픽셀 측정으로 주장하지 않는다. 최종 단일 표시 코드의 1920×1080 재실행, 타이틀 EOS→차량 전체 경로, 패키지·실제 멀티플레이는 이번에 검증하지 않았다. 현재 카메라에서 차량 카드는 기둥과 직접 겹치지 않으므로, 기둥 뒤 **버튼**의 입력 복구와 중앙 차량 카드의 Hover/선택을 구별한다.

OS 커서는 UE 5.8의 `FSlateUser::ProcessCursorReply`가 소프트웨어 커서 등록 시 Custom으로 설정하고 WindowsCursor의 Custom handle은 null이다. 엔진 showui에는 원형 외의 기본 화살표가 함께 그려지지 않았다. Windows 입력 도구 캡처에는 별도 검정 화살표가 보이므로 이를 게임 원형 복제와 혼동하지 않으며, 외부 도구 없는 화면의 OS 커서 중복 여부는 독립 육안 확인 범위로 남긴다. 선택 후 기본 마우스 포인터가 움직이고 원형은 고정되는 것은 의도된 동작이다.

**빌드·안정성:** 최종 MWEditor 및 MW Win64 Development 빌드 성공. Editor 빌드의 XGE 라이선스 경고는 standalone 실행으로 처리됐다. 최종 PIE 시작 로그는 `No blueprints needed recompiling`이며 WBP 변경/저장/PIE 중 재컴파일 없음. 최종 마우스 실행 중 fatal/assertion은 없었다. 다만 PIE 종료 후 저장 상태 확인을 위한 맵 재로드에서 기존 `AppTime.cpp:38 IsInGameThread` handled ensure가 재발했다(`FScene::Release → UpdateReflectionSceneData`). 근본 원인은 미해결이며 이번 커서 수정으로 해결했다고 주장하지 않는다. 두 레벨 MapCheck는 0 errors/0 warnings, 마지막 미저장 map/content는 0/0이다.

**대표 증거와 로그:**

- [수정 전 기둥 Depth 가림](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/BEFORE_DEPTH.png)
- [최종 기둥 위 단일 추적 원형](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/FINAL_FREE_ON_PILLAR.png)
- [최종 클릭 후 원형 하나 고정](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/FINAL_SINGLE_PIN.png)
- [실제 입력 후 상태·Trace 로그 — FINAL 접두사 구간](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/input-state.jsonl)
- [기둥 저장·재로드 확인](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/reload-validation.json)
- [최종 엔진 실행·ensure 발췌](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/final-engine-excerpt.log)
- [Editor 빌드](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/editor-build-final.log), [Game 빌드](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/game-build-final.log)

275개 보호 파일 SHA 비교에서 위 소스 2개·개발 레벨 2개·Master 외 변경 없음. 브랜치 `feature/title-world-ui`, HEAD `3db9c5d` 유지. Git 상태 경로 집합과 UVCS Pending 101경로 유지, 다른 담당자의 건설 테스트 BP/카탈로그 잠금 2건 보존. stage/commit/push/merge/checkin/update/switch 없음. 새 MD·ZIP·연속 PNG는 만들지 않았다.

### 11.11 P3 종료 전 최종 저장본 검증 — 2026-11-03

**판정: P3 종료 보류.** 11.10의 기둥 입력 수정과 원형 추적/고정 상호 배타 표시는 유지한다. 1920×1080 실제 입력은 아래 범위에서 통과했으나, 사용자가 당시 미선택 원형과 Windows 화살표의 실제 동시 표시를 보고했다. 후속 11.12의 재확인 답변은 별도로 보존하며 이 당시 판정은 이력이다. 따라서 당시에는 P3 완료·기능 동결 후 서버 통합 대기 전환의 통과 조건을 충족했다고 선언하지 않는다. 이번 요청은 검증만이므로 새 기능·추가 수정은 하지 않았다.

| 검증 | 실제 결과 |
|---|---|
| 해상도 | PlayerController.GetViewportSize로 1920×1080과 960×540을 각각 확인. 초기 창의 1920×994/1920×1084 및 960×542는 해당 해상도 통과 증거에서 제외. 모니터 DPI/창 배치에 따른 창 크기 차이는 임시 PlaySettings로만 맞췄고 레벨·카메라·UI는 수정하지 않음 |
| 1920 미선택 추적 | 3인 Preview에서 로컬 초록 원형이 좌우 기둥 위까지 이동. 기존 World LocalPlayerCursor는 Collapsed이며 원격 빨강/파랑 카드 마커와 구분 |
| 1920 클릭 고정 | Mammoth 실제 클릭 후 Fred로 마우스만 이동. 선택 Mammoth 유지, 고정 위치 (763.513,422.513) 유지. 로컬 추적 원형을 추가 생성하지 않음 |
| 1920 우클릭 복귀 | Fred 위치 실제 우클릭으로 선택명 해제, locked=false, 고정 World 마커 Collapsed. 자유 원형 추적 복귀 |
| 실제 OS 커서 확인 — 실패 | 자동 입력을 중단한 1920 미선택 화면에서 사용자 직접 관찰 요청. 사용자가 “Windows 화살표도 함께 보임”으로 답했다. 외부 입력 도구 오버레이로 치부하지 않으며 실제 중복 표시 결함으로 기록. 정확한 발생 경로는 미확정 |
| 1920/960 좌우 기둥 입력 | 각 해상도에서 좌측 기둥 뒤 사례 변경 버튼 실제 클릭으로 scenario 7→8. 우측 기둥 뒤 새로고침 Hover/클릭 표시 확인. 입력 후 Visibility hit는 모두 PreviewSurface. 기둥 위치에 카드가 없으므로 기둥 뒤 버튼 입력과 중앙 차량 선택 시험을 구분 |
| 960 기둥 표시 | 좌우 기둥 위 원형이 가려지지 않고 표시. 이번 960 시험의 직접 범위는 기둥 표시/입력이며 전체 선택 순서 반복 또는 사용자 OS 중복 육안 재검증으로 확대하지 않음 |
| 서버 격리 | 모든 FREEZE 상태 조회의 server_tag_valid=false. Preview 5대·fixture는 서버 승인 데이터가 아님. FGameplayTag VehicleAssetID, Phase, RPC, 복제 정책은 변경 없음 |
| 종료·보존 | PIE 정상 종료, dirty map/content 0/0. 임시 PlaySettings는 시작 시 폭960/높이538/위치3269,290/Center=false로 복원. 프로젝트 설정 저장, 레벨 저장, BP 컴파일, 엔진 수정 없음 |

**증거:** 기존 [입력·Trace 로그](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/input-state.jsonl)에 FREEZE-1920/FREEZE-960 구간을 추가했다. 1920 실제 입력은 10:13:51~10:16:16 KST, 정확한 960×540 시험은 10:17:22~10:17:59 KST다. 화면은 computer-use 실시간 캡처로 관찰했으며 새 PNG/동영상/ZIP/MD는 만들지 않았다. [이번 PIE 시작·종료 발췌](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/freeze-engine-excerpt.log)를 기존 증거 폴더에 보관한다.

**AppTime ensure — UI 통과/실패와 별도:** 기존 동일 에디터 로그의 2026-11-03 09:45:25.804 KST(00:45:25.804 UTC)에 발생했다. 이전 PIE 종료(09:44:51) 후 저장 상태 확인을 위한 맵 재로드 중 Scene Release에서 기록됐으며 마우스 클릭 중 발생으로 판정하지 않는다. 스택은 FAppTime::Get → UpdateReflectionSceneData(RendererScene.cpp:5071) → FScene::Update → FScene::Release(RendererScene.cpp:4390) → RenderCommandPipe/렌더 스레드다. 하나의 handled ensure가 Error/EnsureFailed에 반복 출력됐다. [발생 전후 및 스택](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/freeze-existing-ensure-context.log)에 보존했다. 이번 최종 검증은 맵 재로드·BP 재컴파일 없이 PIE 시작/입력/종료만 했으며 새 ensure/fatal/assertion 로그는 발견되지 않았다. 같은 프로세스의 ensure 재출력 제한 가능성도 있으므로 이것을 근본 원인 해결이나 재현 불가로 해석하지 않는다. 엔진·렌더링 구조는 수정하지 않았다.

**변경과 제한:** 프로젝트 변경은 이 Master의 3·8·11.7 현재 설명과 11.11 검증 결과뿐이다. 11.9/11.10 및 과거 명패 감사 당시의 미해결 기록은 역사로 보존한다. C++/WBP/레벨/서버 코드 변경이 없어 재빌드하지 않았으며, 11.10의 빌드 성공을 이번 새 빌드로 주장하지 않는다. 최종 Git 브랜치는 feature/title-world-ui, 기존 UVCS Pending 101경로 및 타인 건설 BP/카탈로그 잠금 2건을 보존했다. Git stage/commit/push/merge 및 UVCS checkin/update/switch는 실행하지 않았다.

최종 SHA-256 대조: 시작 시 보호 파일 275개 중 Master만 변경, 나머지 코드·WBP·레벨·Config는 동일. 기준 문서 명시 링크 49개 모두 유효하다.

### 11.12 Windows 커서 중복 원인 조사 — 2026-11-03, 재현 판정 대기

이번 단계에서 C++/WBP/레벨/Config는 수정하지 않았다. 원인을 확정하지 않은 상태에서 bShowMouseCursor=false, OS ShowCursor, 하드웨어 커서 전역 교체 등의 우회 수정을 넣지 않았다. 11.10의 표시·입력 수정과 FGameplayTag 계약을 그대로 유지한다.

**사용자 최신 관찰:** 자동 입력을 멈추고 우클릭 취소 → 무클릭 이동을 요청한 데 대해 사용자는 “그때는 안보이는거 같은데 선택된 상태에서 마우스를 다른데로 옮기면 마우스가 원래 보였던걸로 기억하는데 지금은 computer Use가 켜져있어서 확인불가”라고 답했다. 미선택 중복이 해결됐다는 확정도, 이전 보고가 도구 오버레이였다는 단정도 하지 않는다. **선택 후 고정 원형과 일반 화살표가 함께 존재하는 것은 정상 요구사항**이며, 미선택/우클릭 복귀 때 두 커서가 함께 따라오는 현상과 구분한다. 도구 사용이 끝난 실제 화면의 최종 확인은 아직 필요하다.

**소스·런타임 조사:**

- RegisterSoftwareCursor는 기존 Marker WBP를 SBox에 넣고 해당 GameViewportClient의 Default 매핑과 소프트웨어 커서 사용 여부를 보관·변경한다. ReleaseSoftwareCursor는 매핑 소유권이 자신에게 남아 있을 때만 이전 매핑/사용 여부를 복원한다. RefreshLocalCursor는 SelectedCard 유효 여부에 따라 추적 등록/해제를 나눈다.
- PreviewActor는 Open에서 이전 bShowMouseCursor를 보관하고 true/GameAndUI로 전환하며 Close에서 복원한다. 해당 Actor/Controller 코드는 수정하지 않았다. 프로젝트/PIE UseMouseForTouch는 모두 false다.
- 실행 중 네이티브 디버거로 FSlateUser::ProcessCursorReply의 소프트웨어 커서 분기를 확인했다. UserIndex=0, 실제 MW Preview 창, 유효한 SBox 커서, FSceneViewport.bIsSoftwareCursorVisible=false이며 Cursor->SetType(Custom) 실행 후 CurrentType=Custom이었다. FWindowsCursor의 Custom 기본/Override 핸들은 모두 null이었다. 소프트웨어 등록 자체가 호출되지 않는다는 가설은 이 실행에서 성립하지 않았다.
- Default 재설정 스택은 창 비활성화의 ResetToDefaultPointerInputSettings 경로와, CursorWidget이 없는 ProcessCursorReply 경로에서 관찰했다. 그러나 이후 상태 대조에서 해당 진단 실행은 Ufology가 이미 선택·고정된 상태였으므로 이를 **미선택 중복의 원인 증거로 사용하지 않는다**. 실행 중 화살표 핸들 65539를 읽었다는 사실만으로 미선택 결함을 입증했다는 중간 설명은 정정한다.
- 실제 우클릭으로 선택명을 비우고 고정 World 마커 Collapsed까지 확인했다. 이후 Windows 상태 조회 때 PC mouse=null이어서 창 내부의 미선택 중복 판정에는 부적합했다. 별도 90초 읽기 전용 샘플 135개 역시 mouse=null 상태뿐이라 재현 성공으로 기록하지 않는다. 포커스 이탈 시 기존 HideLocalCursor가 소프트웨어 매핑을 직접 해제하지 않는 소스 경로는 검토 대상이지만, 이번 관찰만으로 중복의 근본 원인이라고 확정하지 않는다.

[상태별 선택값·OS 커서 읽기 로그](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/os-cursor-state.jsonl), [입력 없는 시간 제한 진단 기록](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/os-live-probe.json)을 기존 폴더에 보관했다. 이 기록은 이미지 캡처가 아니며 Windows GetCursorInfo를 읽기만 했다. 진단 콜백은 90초 후 자동 해제됐으며 Runtime Tick을 추가하지 않았다. 임시 엔진 소스 breakpoint 3개는 one-shot 종료로 모두 제거됐고, 기존 사용자 breakpoint는 변경하지 않았다. 디버거 일시 중단으로 computer-use 입력 한 번이 timeout했고 재관찰 후 진행했으며 중지 신호를 우회하지 않았다.

**현재 완료 범위/남은 범위:** 기존 11.11의 1920/960 기둥 입력·원형 추적/고정/우클릭 결과는 유지한다. 이번에는 수정 전 원인 조사를 수행했으며 새 코드 빌드·수정 후 두 해상도 검증·Back/재진입/포커스 복원 최종 검증은 아직 없다. P3 종료 차단 해제를 선언하지 않는다. 사용자의 직접 확인을 위해 자동 입력을 중단하고 현재 960×540 Preview를 남겨 둔다. 미선택 원형과 기본 포인터의 동시 추적이 재현되는 경우 그 상태에서 원인을 계속 조사한다.

### 11.13 Preview 클릭 즉시 완료·전원 완료 전 취소 — 2026-11-03

**사용자 요구 변경:** 차량 클릭 후 별도 `선택` 버튼을 다시 누르지 않는다. 클릭하면 원형 하나를 고정하고 해당 Preview 참가자를 완료로 표시한다. 다른 참가자가 아직 완료하지 않았으면 우클릭/기존 다시 선택으로 취소하고 추적으로 복귀한다. 실제 참가자 전원이 완료하면 취소를 차단한다. 선택 전후 UI 영역 안에서 Windows 화살표를 숨긴다. 원본 Make Way의 정확한 취소 정책을 검증했다는 의미는 아니다.

**적용 범위:** 개발 `bPreviewOnly` 경로만 즉시 완료·전원 완료 정책을 적용했다. 승인된 서버 계약의 FGameplayTag VehicleAssetID, 개인 승인 후 변경 금지, RPC/Replication/Phase는 그대로다. 새 Preview 정책을 실제 서버에서 사용하려면 서버 담당자의 계약 승인이 필요하며, 이번에는 서버 코드를 변경하거나 로컬 완료를 서버 승인으로 표시하지 않았다.

**최소 변경과 소유권:**

- `Source/MW/UI/MWVehicleSelectionWidget.cpp`: 기존 HandleCardChosen에서 Preview 참가자의 기존 bConfirmed를 클릭 시 설정한다. RefreshControls에서 Preview의 Confirm을 Collapsed로 하고 전원 완료 시 Reset을 비활성화한다. CancelLocalSelection도 동일 조건으로 거절한다.
- 같은 `.h/.cpp`: private 조회 함수 AreAllPreviewParticipantsConfirmed 추가. 기존 ActivePreview.Participants와 사용 가능한 Cards를 조회하며 별도 완료 상태를 저장하지 않는다. 실제 참가자가 없으면 false, 미완료/잘못된 카드/사용 불가 카드가 있으면 false다. 빈 Capacity 슬롯은 참가자로 세지 않는다. 차량 ID/수량/좌표/경로를 고정하지 않았다.
- RefreshLocalCursor는 선택한 Preview에서도 기존 소프트웨어 커서 매핑을 유지하며 내용만 opacity=0으로 숨긴다. World 고정 원형 하나만 보이고 Slate가 하드웨어 화살표를 억제한다. 취소하면 같은 표시를 다시 활성화한다. bShowMouseCursor=false나 OS ShowCursor, 전역 설정, Tick, 새 클래스는 추가하지 않았다.
- HideLocalCursor는 UI 이탈/창 비활성화 시 기존 ReleaseSoftwareCursor를 호출한다. 이전 매핑을 소유권 검사 후 복원하며, 재진입 이벤트에서 다시 등록한다. NativeDestruct의 기존 해제도 유지한다.
- 카드/명패/포인터 WBP, PreviewActor, 디자인·카메라·배경·레벨·Config·서버·EOS·차량 정의 파일은 수정하지 않았다. 기존 다시 선택 버튼은 보존하며 전원 완료 후 비활성화한다. 별도 선택 버튼만 숨긴다.

**실제 PIE 검증 (Standalone 개발 fixture이며 네트워크 참가자 시험 아님):**

| 항목 | 실제 관찰 |
|---|---|
| 960×540 | PlayerController ViewportSize 확인. 3인 미선택 → Mammoth 실제 클릭 즉시 locked=true 및 참가자 confirmed=true → Fred 클릭 차단 → 우클릭으로 선택명/완료 해제·고정 marker Collapsed 확인 |
| 3인 전원 완료 | 기존 scenario8의 두 완료 참가자 + 로컬 Fred 실제 클릭으로 전원 완료. 이후 Mammoth 위치 우클릭에도 Fred/locked=true 유지, 다시 선택 버튼 비활성 |
| 2명/정원4 | 기존 scenario1에서 공개 API로 local slot0 지정 후 Fred 실제 클릭. 실제 두 참가자 완료, 빈 두 슬롯은 참가 대기 유지. 우클릭 취소 차단 |
| 4인 전원 완료 | 기존 scenario4에서 local slot3 지정 후 Fred 실제 클릭. 네 명 모두 완료 및 우클릭 취소 차단 |
| 1920×1080 | 실제 ViewportSize 확인. Mammoth Hover/클릭 즉시 완료, Fred 클릭 차단, 우클릭 취소·추적 복귀 확인. 별도 선택 버튼 없음 |
| OS 커서 | 유효한 PC mouse 좌표가 있는 960/1920 미선택·선택·이동·취소 시 GetCursorInfo flags=0, cursor_handle=null. bShowMouseCursor는 true 유지. 도구 캡처의 포인터 형상만으로 OS 표시를 판정하지 않았으며 사용자 직접 관찰은 별도로 요청 |
| UI 밖 | 1920 창 제목 표시줄로 실제 이동하면 원형 사라짐, mouse=null 및 OS flags=1/handle=65539로 기본 화살표 복원 |
| 기둥 | 1920 양쪽 기둥 위 자유 원형 표시, Trace가 PreviewSurface인 상태 유지. 새 기둥/충돌 수정 없음 |
| Back/재진입 | 실제 Back 클릭 후 widget=null, 기존 PreviewActor가 이전 PC bShowMouseCursor=false 복원. 기존 OpenPreview API 재진입 후 `_C_1` 새 인스턴스, 미선택·단일 자유 원형·OS 숨김 복구. 재진입 API를 실제 메뉴 클릭이라고 주장하지 않음 |
| 데이터 격리 | ONECLICK 기록의 모든 Preview 선택에서 server_tag_valid=false. 승인 차량 DA/Pawn 등록이나 서버 요청 추가 없음 |

참가자 사례/로컬 슬롯 설정은 기존 공개 개발 API로 하고 카드 선택/취소/Back은 실제 마우스로 검증했다. 초기 사용자의 창 이동으로 실제 Viewport가 480×270이 된 기록은 요청 해상도 통과 자료에서 제외했다. 비동기 EndPIE 처리 완료 후 별도 호출에서 임시 PlaySettings를 지정하여 최종 960×540/1920×1080을 확인했다. 이번에는 2명/정원2, 1920에서 모든 다인 조합, 타이틀 EOS 전체 경로, 실제 여러 계정, 게임패드, 패키지 및 별도 창으로의 완전한 포커스 전환을 모두 재검증한 것은 아니다. UI 경계 이탈·복귀는 위 범위에서 확인했다.

**빌드·안정성:** MWEditor 및 MW Win64 Development 모두 Succeeded. XGE 라이선스 경고로 standalone 빌드했으며 컴파일 오류 없음. 미저장 map/content=0/0 확인 후 ProjectMW만 정상 종료·재실행했고 AssetReview는 유지했다. 새 PIE 로그는 No blueprints needed recompiling이며 에셋 저장·재컴파일 없음. 이번 실행에 fatal/assertion/ensure 기록 없음. 기존 PlayerTint 초기화 오류가 에디터 시작 로그에 남아 있고 직전 백업 로그에서도 동일 오류를 확인했다. 기존 MWVehicleUITypes.h는 수정하지 않았으며 무오류 실행으로 주장하지 않는다. 비공개 software_cursor Python 조회 한 번 실패한 것은 진단 코드 오류로 구분한다. AppTime ensure의 근본 원인 해결은 이번 범위가 아니다.

**검증 자료:** 새 MD/ZIP/영상/연속 PNG 없음. 기존 [입력 기록의 ONECLICK 구간](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/input-state.jsonl), [동일 시점 OS 조회](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/os-cursor-state.jsonl), [Editor 빌드](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/oneclick-editor-build.log), [Game 빌드](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/oneclick-game-build.log)에 보존한다. 화면은 실시간 computer-use 관찰이며 새 엔진 캡처 파일은 만들지 않았다.

**보존·남은 판정:** 275개 보호 파일 SHA 대조에서 SelectionWidget.h/.cpp와 Master 외 변경 없음. 기존 Git 상태 경로 집합, feature/title-world-ui, UVCS Pending101경로와 타인 잠금2건 보존. stage/commit/push/merge/checkin/update/switch 없음. 1920 Preview를 사용자 직접 확인용으로 열어 두고 자동 입력을 중단했다. 후속 사용자 직접 관찰 결과: “화살표 없이 고정·취소 정상”(2026-11-03 KST). 이 확인은 자동 입력을 중단한 1920 Preview의 클릭 고정·마우스 이동·우클릭 추적 복귀와 Windows 화살표 비표시를 대상으로 한다. 해당 종료 차단 항목을 통과 처리하고 P3 Preview 기능을 현재 동작으로 동결한다. 실제 서버 통합은 담당자 계약 승인·구현 대기이며, 기존 PlayerTint 초기화 오류 및 AppTime ensure의 별도 미해결 기록은 유지한다. 이번 확인 반영은 Master만 수정했고 추가 입력·코드·에셋·VCS 작업은 하지 않았다.

### 11.14 중복 다시 선택 버튼 제거·조작 안내 — 2026-11-03

사용자 후속 요청으로 `다시 선택` 버튼을 화면·입력에서 Collapsed/비활성화했다. 호환성을 위해 기존 BindWidget/Delegate 자체는 남겼으며 새 입력 경로는 만들지 않았다. 기존 Text_Status를 개발 HUD의 FeedbackRows에서 DesignCanvas로 옮겨 Clean 화면에서도 표시한다. 최종 안내는 카드 위 빈 공간 중앙의 `좌클릭: 차량 선택 · 우클릭: 선택 취소 (전원 완료 전)`이며, 실제 참가자 전원 완료 시 `전원 선택 완료 · 취소할 수 없습니다`로 바뀐다. 처음 시도한 하단 배치는 실제 PIE에서 3D 지지대에 글씨가 가려져 채택하지 않았다. 기존 배경·기둥·카메라·UI Depth는 변경하지 않았다.

변경 파일은 MWVehicleSelectionWidget.cpp, WBP_VehicleSelection_Dev.uasset, WBP_VehicleSelection_Preview.uasset 및 이 Master다. C++는 Reset 표시 제거와 Text_Status의 완료 문구 선택 조건만 바꿨다. 안내 문자열은 기존 PreviewText/PreviewLockedText의 WBP 기본값에서 관리한다. 폰트30, 기존 Noto 폰트·흰색, outline1/그림자, 중앙 Anchor/Alignment 및 1340×60 Slot은 Designer 설정이다. 런타임 좌표·문자열·경로 하드코딩과 새 클래스·Tick은 없다.

저장 WBP 두 개 컴파일 성공, 재시작 후 저장된 부모 DesignCanvas/폰트/Slot 재확인. MWEditor 및 MW Win64 Development 빌드 성공(`work/p3-cursor-occlusion/hints-editor-build.log`, `hints-game-build.log`). 실제 Viewport 1920×1080에서 최종 안내의 잘림·기둥 가림 없음, Reset 제거, Mammoth 좌클릭 즉시 완료 및 Fred 위치 우클릭 취소 정상. 기존 scenario8에서 Fred 실제 클릭 후 전원 완료 안내 전환 확인. 960×540에서도 동일 위치의 안내와 Reset 제거를 실제 화면으로 확인했다. 상태 기록은 기존 input-state.jsonl의 HINTS 구간이며 새 PNG·영상·MD·ZIP은 만들지 않았다. 초기 Python 진단의 메서드/객체 조회 오류는 진단 실패로 구분하며 저장·컴파일·재시작 이후 확인은 별도로 성공했다.

미저장 map/content=0/0, 정식 L_Title·개발 레벨·서버 계약·EOS·차량 데이터·커서 입력 구조는 변경하지 않았다. 사용자의 11.13 커서 확인을 무효화하는 동작 변경은 없으며 이번 범위는 버튼/안내만이다. 전체 EOS/다계정 네트워크/패키지 검증은 반복하지 않았다. 자동 입력을 멈추고 최종 960 Preview를 열어 두었다. Git/UVCS Pending·타인 잠금을 보존하고 stage/commit/push/merge/checkin/update/switch를 수행하지 않았다.

### 11.15 원본풍 조작 레전드·상태판 — 2026-11-03

11.14의 긴 문장형 안내를 폐기하고 카드 위 좌우 조작 레전드와 중앙 상태판으로 교체했다. 왼쪽은 `좌클릭 / 차량 선택`, 오른쪽은 `우클릭 / 선택 취소`. 원본 A의 어두운 조작판과 굵은 글자를 참고했으며 이번 세 패널 구성·한국어 문구는 ProjectMW 개선안이다. 원본 마우스 버튼 전용 이미지가 확인되지 않아 기존 Border/TextBlock과 Noto Bold로 구성했다. 1:1 원본 재현으로 주장하지 않는다. 입력명·중앙 상태는 노랑, 동작명·보조 상태는 흰색이며 카드·명패·커서는 그대로다. 버튼/hover 효과가 없고 전체 레전드는 HitTestInvisible이어서 입력을 소비하지 않는다.

**상태와 서버 경계:**

- 미선택: `차량을 선택하세요`. 우클릭 레전드는 비활성 표현.
- 운영 모드의 유효한 로컬 후보: `선택한 차량을 확정하세요`. 기존 Button_Confirm·OnSelectionRequested(FGameplayTag) 경로를 유지했다. 새 확정 버튼이나 서버 요청은 없다.
- 개발 Preview의 완료 상태: `선택 완료` + `다른 플레이어 대기 중…`. 기존 클릭 즉시 완료 및 전원 완료 전 취소를 그대로 유지하므로 이때 우클릭 레전드는 남긴다. 이것은 **서버 최초 승인 이후 취소 허용을 뜻하지 않는다**.
- Preview 전원 완료: `전원 선택 완료`, 우클릭 레전드 Hidden. 레이아웃 공간은 보존하여 중앙 상태판이 이동하지 않는다. 기존 Preview 계산은 bPreviewOnly 안에서만 쓰며 운영 전원 완료 판정에 사용하지 않는다.
- 현재 로컬 AMWPlayerState/AMWGameState에는 승인 VehicleAssetID·VehicleSelection Phase 소비 경로가 없다. 실제 서버 C/D 상태와 승인 후 힌트 숨김은 미연결·미검증이며, UI가 승인 상태를 추정하거나 별도 bool/RPC/Phase를 만들지 않았다. 서버 연결 후 승인 필드·서버 Phase를 소비해야 한다. 현재 구현을 운영 통합 완료로 표시하지 않는다.

**변경:** SelectionWidget.h/.cpp에 optional Text_SelectionDetail/Legend_Cancel 바인딩과 WBP 편집 FText 세 개를 추가하고 RefreshControls의 텍스트·가시성·활성 표현만 변경했다. HandleCardChosen 이후 선택/취소/확정 함수 본문은 직전 스냅샷과 완전히 동일함을 비교했다. 커서 등록·추적·고정·복귀 코드는 수정하지 않았다. WBP_VehicleSelection_Dev 내부에 HorizontalBox·Border·VerticalBox·TextBlock을 추가했고 자식 WBP_VehicleSelection_Preview의 문구 기본값을 저장했다. 기존 Text_Status, PreviewLockedText/ChooseVehicleText, 기존 폰트를 재사용한다. 레이아웃1320×140, 패널 비율1:1.6:1, 폰트27/32/34/24와 색상·Padding·Anchor는 WBP Designer 설정이다. 문자열/차량 수량/ID/좌표/경로를 C++에 고정하지 않았다. 기존 다시 선택은 Collapsed/비활성 유지.

**검증:** 저장 WBP 두 개 컴파일 성공, 정상 에디터 종료 후 최종 MWEditor/MW Win64 Development 빌드 성공(`work/p3-cursor-occlusion/legend-editor-build.log`, `legend-game-build.log`). 최종 코드 재실행 후 저장 트리의 SelectionStatePanelRows 및1320×140 설정 확인. 실제 PC ViewportSize1920×1080과960×540을 확인했고 1920의 미선택→Mammoth 실제 클릭→우클릭 취소→기존3인 scenario8→Fred 실제 클릭으로 대기/전원 완료 및 취소 안내 숨김을 관찰했다. 960에서2·3·4인 기존 fixture의 명패·카드·레전드 간 비중첩을 확인하고4인 마지막 Fred 실제 클릭으로 전원 완료, 이어 실제 우클릭 취소 차단을 확인했다. fixture 설정은 공개 개발 API이며 실제 네트워크 인원 시험이 아니다. 각 글자와 패널은 두 해상도에서 잘림 없이 읽히며 배경 기둥과 겹치지 않았다. 모든 LEGEND 로그의 Preview server_tag_valid=false. 입력 중 OS GetCursorInfo flags=0/handle=null, 원형 클릭 고정·취소 복귀 확인. 과거 Windows 화살표 결함을 문구 수정으로 해결한 것으로 취급하지 않는다.

운영 후보→확정 요청·실제 승인 후 C/D, 복수 계정·EOS 전체 흐름·패키지는 미검증이다. 별도 서버 데이터나 가짜 차량 정의를 생성하지 않았다. 최신 실행에 새 fatal/assertion/ensure는 발견되지 않았으며 기존 PlayerTint 초기화 오류와 과거 AppTime ensure의 미해결 기록은 유지한다. 진단 중 Rider 연결 부재로 중복 실행한 새 ProjectMW는 미저장0 확인 후 즉시 정상 종료했고 기존 ProjectMW도 PIE 종료/All Saved 확인 후 정상 종료하여 최종 검증은 한 인스턴스로 진행했다. AssetReview는 건드리지 않았다.

자료는 기존 input-state.jsonl/os-cursor-state.jsonl의 LEGEND 구간과 위 빌드 로그에만 추가했다. 화면은 computer-use 실시간 관찰이며 새 PNG·영상·MD·ZIP은 없다. 변경은 SelectionWidget.h/.cpp, 위 WBP2개, Master 총5개로 보호 SHA 대조했고 서버·정식 L_Title·개발 레벨·카메라·배경·EOS·차량 파일은 동일하다. 최종 미저장 map/content=0/0. Git/UVCS 작업은 상태 조회만 수행하며 Pending과 타인 잠금을 유지한다. 최종 960 3인 미선택 화면으로 복귀하고 자동 입력을 중단했다.

**11.15 후속 캡처 제출 — 2026-11-03:** 사용자 캡처 요청 후 실제 최종 PIE에서 F9 엔진 캡처 3장만 저장했다. 기존 폴더의 [1920 미선택](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/LEGEND_1920.png), [1920 선택 완료·대기](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/LEGEND_1920_SELECTED.png), [960 미선택](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/LEGEND_960.png). PNG는 창 테두리를 포함하여 각각1932×1156,1932×1156,972×616이며 실제 게임 영역1920×1080/960×540과 구분한다. 합성·확대·크롭하지 않은 실제 캡처다. Rider 연결 부재로 현재 PIE 콘솔에서 기존 ResizePlayWindow 에디터 API만 호출해 창 크기를 맞췄으며 새 에디터를 실행하거나 코드·WBP·레벨을 수정하지 않았다. 중간 PNG 연속 녹화는 없다. 자동 입력은 다시 중단했다.

### 11.16 하단 원본 복원·상단 원본풍 신규 디자인 — 2026-11-03, 시각 승인 대기

사용자의 재작업 지시서(첨부42c8798b)를 적용했다. 11.15의 검정3패널은 미승인 설계로 폐기한다. 하단은 원본 자료와 직접 비교하고, 상단은 ProjectMW 신규 디자인으로 평가한다. 실제 수정은 WBP_VehiclePlayerEntry_Dev, WBP_VehicleSelection_Dev, WBP_VehicleSelection_Preview 및 Master뿐이다. **C++ 변경0**, 새 클래스/Tick/Actor/에셋0, 데이터·입력·확정 정책 변경0이다.

**원본 근거와 현재 차이**

실제 Unity 원본 level0의 SHA-256은 DA399B05859CBAAA2F25970846F5F0DBC9566B1064166E71318884E185571C57로 기존 증거와 동일했다. PlayerVehicleSelectedEvents MonoBehaviour를 UnityPy로 다시 읽어 `defaultColour=(.7294118,.7294118,.7294118,1)`, playerBackground/Image·playerVehicle/RawImage·playerName/TMP·controller/steering/paint/team 참조가 분리된 것을 확인했다. 결과는 `work/p3-cursor-occlusion/redesign-unity-player-fields.json`에 보존한다. level0 경로는 `C:/Program Files (x86)/Steam/steamapps/common/Make Way/Make Way_Data/level0`이다.

기존 `work/p3-original-rebuild/unity-native-evidence.json`의 `UI/Select Vehicle/Player Slots/Player 1` 계층을 실제 원본 해시와 대조했다. Sign은 sharedassets0.assets Sprite1245 `t_playerSlotSign`,512×512/Border75, 볼트는 Sprite1355 `T_signInAnywayBolt`,128×128이다. Sign은 슬롯 전체0..1, Name은 X.1..9/Y0..12, Vehicle RawImage는 X.0375..9625/Y.075..1, RawImageAspectFitter/Shadow를 사용한다. Vehicle의 저장 m_Texture는 null이고 alpha0이므로 **이 Scene 직렬화만으로 실행 시 차량 이미지 공급원을 확정하지 않는다**. 추출 PlayerVehicleSelectedEvents.cs의 SelectVehicle/HighlightVehicle/ChangeColour 메서드는 본문이 비어 있어 런타임 결과의 증거로 삼지 않았다. 원본 하단의 색상 점은 확인되지 않았고 CTRL/ALT·조향보조·페인트 안내는 실제 원본에 있다.

원본 A/B는 실제3인 화면이며 A의 세 번째는 차량 미표시, B는 세 명 모두 차량 표시다. 기존 ORIGINAL_SCREEN_REBUILD, MULTIPLAYER_UI_LAYOUT, FIDELITY_REVIEW, PLAYER_PANEL_READABILITY 원문과 현재 Master를 참고했다. 과거4+1·3+2와 배경 변경 결과는 채택하지 않고 현재5대 단일 행·배경·카메라를 유지했다.

| 요소 | 원본에서 확인 | 수정 전 ProjectMW | 이번 적용 / 남은 차이 |
|---|---|---|---|
| Sign | 작은 둥근 모서리·얇은 외곽선·별도 볼트 | 큰 둥근 모서리, 굵은 외곽선 | 원본 Sign 그대로 재사용하며 Slate9-slice 표시 밀도를 보정. 실제260×260 판 크기는 유지 |
| 차량 | 큰 RawImage와 AspectFitter, 이름 위까지 외형 점유 | 228×194 영역의 ScaleBox 안에서 정사각 아이콘이194²로 축소 | 같은 원본 아이콘을240.5² 영역으로 직접 표시(+24% 할당 변 길이). 비율/이미지 파일은 보존. 운영 차량 렌더·페인트·동일 차종의 원본 하단 각도는 미확정 |
| 이름 | 하단 중앙, TMP Race1 계열 | 원 옆 좁은 Fit 영역, 원본보다 위쪽 | 전체 판 중앙208×40의 독립 Fit. 폰트22·기존 긴 이름 축소 경로를 유지. 대체 NotoKR와 원본 Race1 차이는 남음 |
| 추가 색상 점/큰 로컬 프레임 | 동일 구성 미확인 | 원형 점·황금 사각 테두리 | Designer opacity0으로 숨김. 관련 binding/participant 데이터는 보존. 구분은 기존 플레이어명·카드 원형 마커·상태로 유지 |
| 하단 작은 안내 | 원본 CTRL/ALT·조향보조·페인트 | 선택 중/확정 상태판 | 기존 상태판 유지. 미구현 원본 기능을 작동하는 조작으로 위장하지 않음 |
| 빈 슬롯 | 실제 참가자 미선택 판 존재 | 실제 참가자·대기 슬롯 모두 표현 | 차량 null이면 숨김, 참가 대기는 기존 표시 유지. 가짜 차량/플레이어 생성 없음 |

**하단 Designer 변경**

Image_Vehicle을 기존 DesignCanvas의(9.75,-8),240.5²로 옮겨 z4에서 표시하고, Text_PlayerName을 독립 ScaleBox OriginalNameFit의(26,216),208×40에 중앙 정렬했다. 원본 Vehicle/Name의 상대적 영역을 참고하되 대체 글꼴·아이콘 알파 패딩에 필요한 안전 여백을 두었다. 이름과 차종별 그림이 겹치지 않는 것은 실제 캡처에서 확인했다. 원본 텍스처의 알파 패딩·비율·파일을 크롭/재제작하지 않았다.

Sign은 같은 T_MW_PlayerSlotSign을 사용한다. Slate Box는 소스 텍스처 크기×Margin으로 테두리의 기하 폭을 정하므로 Brush.ImageSize 변경만으로는 큰 모서리가 충분히 해결되지 않았다(엔진 ElementBatcher.cpp857~860 확인). 원본 Texture의 곡선 전체를 포함하는 UV구간.25와1300² 레이아웃→Designer RenderScale.2/Pivot0을 사용해 표시 크기260² 및 약3px 외곽선을 맞췄다. 이는 텍스트를 임의 좌표로 밀어 맞추는 보정이 아닌 원본 Sprite의 테두리 픽셀 밀도 변환이다. 설정값은 WBP에만 저장했고 C++·Texture는 변경하지 않았다. 실제 확대 비교에서 이전 큰 모서리와 구분해 확인했다. Neutral/Confirmed tint는 원본 defaultColour의 중성값으로 통일하되 Unity/UE의 셰이더·색공간이 완전히 같다고 주장하지 않는다. 확정 상태는 기존 상태판·마커로 계속 구분한다.

**상단 신규 디자인**

원본에 존재하는 조작판을 복제했다고 주장하지 않는다. 같은 Sign·Bolt로 연결된 얇은 황색 사인 스트립과 마우스 왼쪽/오른쪽을 파랑으로 표시한 작은 UMG 도형을 만들었다. 기존 세 검정 Border는 투명화하고 기존 Text_Status/SelectionDetail/Legend_Cancel 바인딩을 재사용한다. 새 클릭 버튼·Hover 효과는 없다. 전체는 HitTestInvisible이며 기존 입력을 소비하지 않는다. 상태는 차량 선택 / 선택 완료(작은 대기 문구) / 전원 선택 완료다. 전원 완료 시 우클릭 안내만 숨기고 자리와 스트립은 유지한다. 기존1320×140에서1200×88로 할당 면적약43% 감소했다. 상단은 원본풍 ProjectMW 신규 디자인, 하단은 원본 재현 작업으로 구분한다.

**실제 검증·증거**

원본/수정 전/수정 후 전체와 하단 확대는 [비교 갤러리](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/REDESIGN_COMPARISON.html)에 정리했다. [전체 비교 PNG](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/REDESIGN_COMPARISON.png), [하단 확대 비교 PNG](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/REDESIGN_LOWER_COMPARISON.png)는 실제 원본/PIE에서 client 영역을 잘라 나란히 둔 **편집 비교 자료**이며 새로운 PIE 실행 화면으로 주장하지 않는다. 하단 확대는1.5배 nearest다.

원본 PIE PNG는 창 테두리 포함1932×1156 또는972×616이다. 실제 PlayerController viewport1920×1080/960×540을 별도로 조회했다. `REDESIGN_BEFORE_1920.png`, `REDESIGN_AFTER_1920_A.png`(3인 중1명 미선택), `REDESIGN_AFTER_1920_B.png`(실제 Mammoth 클릭), `REDESIGN_AFTER_1920_ALL.png`(4인 완료 fixture), `REDESIGN_AFTER_960_2P.png`(2명/정원4), `REDESIGN_AFTER_960_3P.png`, `REDESIGN_AFTER_960_4P.png`(4인 완료)를 기존 증거 폴더에 저장했다. 최종 외곽선 보정 이후 모두 갱신했다. 파일 크기·구분은 redesign-captures.json이다. 대량 프레임·영상·ZIP·새 MD 없음.

1920에서 실제 Mammoth 클릭→Fred 클릭 차단·고정 위치 유지→Fred 위치 우클릭 취소·원형 복귀, 실제 마지막 Fred 클릭→전원 완료→우클릭 차단을 확인했다. 실제 Back 후 widget=null·기존 bShowMouseCursor 복원, 기존 OpenPreview API 재진입으로 새 인스턴스를 확인했다. 재진입을 실제 타이틀 메뉴 버튼 클릭으로 주장하지 않는다.960의 실제 마지막 Fred 클릭·전원 완료 표시와2/3/4인 레이아웃을 확인했다. fixture 설정은 기존 공개 개발 API이며 네트워크 참가자 상태가 아니다. 최종 외곽선만 추가 보정한 뒤1920 실제 Mammoth 클릭과960저장 외형을 다시 확인했다. 모든 REDESIGN 로그는 server_tag_valid=false이며 실제 OS GetCursorInfo flags0/handle=null을 확인했다. 이번에는 사용자 직접 OS 커서 육안 재확인을 반복하지 않았으며 11.13의 승인과 구분한다.

변경 WBP3개 컴파일/저장 성공, 정상 종료 후 새 Editor에서 BS_UP_TO_DATE 재로드 확인. MWEditor/Game Development는 모두Succeeded/up to date(1.91초/1.27초). C++ 변경이 없어 신규 소스 컴파일이 발생하지 않은 것을 명시한다. 최종 미저장 map/content0/0. 정식 L_Title·개발맵·카메라·배경·EOS·서버·차량·AssetManager는 변경하지 않았다. 기존 FGameplayTag VehicleAssetID·서버 최초 승인 후 변경/취소 금지·서버 전원 완료 판정은 그대로다. 운영 C/D·다계정·EOS 전체·패키지는 미검증이다.

편집 중11:46:59 KST에 기존 AppTime.cpp38 IsInGameThread handled ensure가 재발했다. 이번 스택은 FSlate3DRenderer→SlateRHIRenderer::CreateSlateRenderBatchOp 경로이며 직전 PIE 종료 뒤 WBP 편집·컴파일 시점이다. redesign-ensure-context.log에 보존했다. 근본 원인을 해결했다고 주장하지 않고, 원인 불명 상태에서 엔진·렌더러·서버 코드를 수정하지 않았다. 중간 Authoring의 SlateVector 타입/API 오류 및 임시 z순서 실패는 수정했고 최종 캡처와 구분한다.

**남은 시각 차이·판정:** 원본 차종 AERYO/COOL_KYLE과 이번5종은 다르며 같은 차종 하단의 런타임 렌더·페인트·그림자 공급원을 확정하지 못했다. 기존 실제 추출 아이콘은 유지했으며 임의 이미지·Pawn·SceneCaptureActor로 대체하지 않았다. 원본 Race1과 Noto의 획·폭, 하단 CTRL/ALT 안내 내용, 명패의 세부 색감·볼트 크기·원근 및 배경 차이가 남는다. 따라서 시각 디자인 최종 승인을 선언하지 않는다. 캡처와 비교 자료를 사용자에게 제출하고 승인을 기다린다.

Git feature/title-world-ui, UVCS Pending101경로 및 다른 담당자 잠금2건 보존. Commit/Push/Merge/Reset/Rebase/Checkin/Update/Switch 없음. C++ 소스 해시 비교 동일, UCM/Unity 원본은 읽기 전용이다. 원본 추출 리소스와 비교 자료는 로컬 내부 검토 위치에만 보관했다.

**11.16 후속 시각 수정 — 2026-11-03 13:03 KST, 상단 크기·형태 및 아이콘 제거**

사용자는 최초 안내 스트립을 채택하지 않고 `표지판 크기와 형태 변경`을 요청했으며, 이어 좌클릭·우클릭의 마우스 아이콘 제거를 명시했다. 하단 전시판 수정안은 유지했다. 긴1200×88 판을 중앙380×92의 짧은 사각 상태판으로 바꾸고, 좌우에는 `좌클릭 / 선택`, `우클릭 / 취소` 글자만 분리했다. 같은 원본 Sign/Bolt를 사용하며 텍스처 파일은 변경하지 않았다. 아이콘 SizeBox는 Widget Tree의 표시 계층에서 제거해 아이콘과 여분 Slot Padding을 함께 없앴다. 글자 묶음은 기존 HorizontalBox의 Fill/Center Slot으로 정렬했다. 세 묶음의 위치·크기는 기존 DesignCanvas의 중앙 Anchor·Alignment 및 WBP Designer 값으로 관리하며 C++ 보정은 없다. 큰 검정3판이나 신규 클릭 버튼은 추가하지 않았다.

실제 변경 바이너리는 `Content/UI/Lobby/VehicleSelection/WBP_VehicleSelection_Dev.uasset` **1개**다. 상속 Preview WBP도 컴파일·저장을 실행했으나 SHA-256은 동일했다. 이번 시작 시점 기준 Source 전체·차량 UI 폴더·Content/Levels 직접 하위 맵 해시를 비교했으며 위 WBP 외에는 동일했다. 특히 하단 Entry·카드·마커·Preview Actor·개발맵·정식 L_Title·카메라·배경은 보존했다. 기존 Git 브랜치와 UVCS Pending101경로·타인 잠금2건을 읽기 전용 확인했으며 VCS 변경 명령은 실행하지 않았다. 증거는 기존 폴더의 `legend-shape-before-sha256.json`, `legend-shape-changed-files.json`이다.

최종 저장본 실제 PIE에서1920×1080의 Mammoth 좌클릭→선택 완료·작은 대기 문구→우클릭 취소·차량 선택 복귀를 확인했다. 로그 `input-state.jsonl`/`os-cursor-state.jsonl`의 `LEGEND-TEXT-*` 항목으로 구분한다. 클릭 후 원형 고정, 취소 후 고정 마커 숨김, server_tag_valid=false를 확인했고 클릭 시 OS GetCursorInfo flags0/handle=null이었다. 사용자 직접 OS 커서 육안 재확인을 이번에 추가로 받았다고 주장하지 않는다.960×540은 기존4인 전원 완료 fixture5에서 중앙 `전원 선택 완료`와 우클릭 안내 숨김·아이콘 제거·가독성을 확인했다. 전원 완료는 개발 Fixture 표시이며 실제 서버 승인/네트워크 시험이 아니다. 기존2인 검사·전체 잠금/Back/재진입 회귀 기록은 앞선11.16을 유지하며 이번 크기·형태 후속 수정에서 전부 반복하지 않았다.

[1920 최종 실제 PIE](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/LEGEND_TEXT_ONLY_1920.png), [960 최종 실제 PIE](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/LEGEND_TEXT_ONLY_960.png), [상단 전후 확대 비교](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/LEGEND_TEXT_ONLY_COMPARISON.png)를 제출한다. 실제 PC viewport1920×1080/960×540을 조회했고, 최종 PNG는 PIE의 `Shot showui` 출력으로 창 테두리를 포함한1932×1156/972×616이다. 비교 PNG는 두 실제 출력의 같은 영역을 잘라 배치한 편집 자료다. 초기 `LEGEND_SHAPE_ENGINE_1920.png`는 AutomationLibrary가 Editor viewport를 촬영한 실패 자료로 최종 PIE 증거에서 제외했다. Sky 표시용 축소 화면과 원본 해상도 증거를 구분한다.

WBP 및 상속 Preview 컴파일·저장 성공, 최종 미저장 map/content0/0. C++·빌드 설정 변경이 없어 이전 Editor/Game 성공 기록은 유지하고 이번 후속에서 재빌드는 실행하지 않았다. 기존 AppTime ensure는 해결로 간주하지 않으며 이번 최종 PIE 입력·캡처 구간의 로그 말미에는 새 fatal/assertion을 확인하지 못했다. 새 MD·ZIP·연속 캡처는 생성하지 않았다. 이 후속안 역시 사용자 시각 확인 전 최종 채택으로 표시하지 않는다.

### 11.17 참가자 커서색과 하단 상태판 연결 — 2026-11-03

사용자는 11.16 후속의 작은 중앙 상태판과 아이콘 없는 좌우 조작 안내에 대해 `ㅇㅇ 훨씬 낫고`라고 확인했다. 해당 상단안을 채택하고 유지한다. 이어 하단 `선택 중` 표시만으로 참가자를 구분하기 어려워 커서색과 같은 칸 배경 또는 상태색을 요청했다. 차량 전시판 전체를 칠하지 않고 **기존 작은 상태판의 배경을 참가자 커서색으로 표시**하는 안을 적용했다. 상태 글자는 흰색을 유지하고 WBP Font Outline1을 적용해 어두운 빨강·파랑에서도 읽히게 했다. 색상은 플레이어 정체성이며 선택 중/확정 여부는 기존 문구로 계속 구분한다.

변경 에셋은 `Content/UI/Lobby/VehicleSelection/WBP_VehiclePlayerEntry_Dev.uasset`1개다. 기존 `UMWVehiclePlayerEntryWidget::SetPresentation`은 이미 `Image_IdentityBadge`에 `Player.PlayerTint`를 적용하고 비참가 슬롯에서는 숨긴다. 이 연결을 재사용했다. 표시하지 않던 원형 색상 점을 부모에서 분리해 LegacyIdentityBadge로 구분하고, 같은 WBP의 기존 OriginalControlPlate를 Image_IdentityBadge로 연결했다. 상태판의 **기존 T_MW_MenuSign Sprite·프레임·볼트·크기·위치는 보존**했다. 기존 커서의 Image_Badge/Image_PointerArrow와 상태판이 동일 Presentation.PlayerTint를 소비한다. 새 색상 배열·플레이어 번호별 분기·네트워크 상태·코드 좌표·새 클래스·Tick은 없다. C++ 수정0이며 헤더/빌드 설정도 변경하지 않았다.

실제1920×1080의4인 Preview에서 빨강/파랑/초록/주황 상태판과 커서색을 비교하고 3명 확정+1명 선택 중을 확인했다. 실제 마우스로 마지막 Fred를 클릭해 주황 상태판의 `선택 중`→`확정`, 중앙 전원 완료, 우클릭 안내 숨김, 원형 고정을 확인했다. `input-state.jsonl`의 `PLAYER-STATUS-COLOR-1920-CLICK`에 selected=Fred, locked=true, server_tag_valid=false를 남겼다. 상태판의 실제 런타임 RGBA4종과 원본 Sprite 참조는 `player-status-color-runtime.json`에 기록했다. 색상값을 새로 정의하지 않았다.

960×540에서도4인 혼합 상태의 글씨와 색 구분을 확인했다. 기존2명/정원4 Fixture에서 실제 참가자2명만 빨강·파랑 상태판을 표시하고 빈2슬롯은 상태판 Collapsed인 것을 확인했다.3인 Fixture에서도 같은 색 규칙·미선택 참가자 표시를 확인했다. 이는 개발 Preview/API Fixture 시험이며 실제 서버 참가자·Replication 검증이 아니다. 이번 색상 후속에서는 Back/재진입·EOS·서버 전원 확정·패키징 전체를 다시 시험하지 않았다. 기존 입력 회귀 결과와 분리한다.

[1920 실제 PIE](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/PLAYER_STATUS_COLOR_1920.png), [960 실제 PIE](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/PLAYER_STATUS_COLOR_960.png), [하단 확대](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/PLAYER_STATUS_COLOR_DETAIL.png). 두 전체 캡처는 실제 PIE의 Shot showui 출력이며 창 테두리 포함1932×1156/972×616이다. 확대본은 실제1920 PNG에서 하단을 자른 편집 증거다. 연속 프레임·영상·ZIP·새 MD는 만들지 않았다.

WBP 컴파일·저장 성공, 실제 저장본 PIE 생성과 동적 색 반영 확인, 미저장 map/content0/0. 시작 시점 해시 대조에서 해당 Entry WBP 외 Source 전체·차량 UI 폴더·Content/Levels 직접 하위 맵은 동일했다. `player-status-tint-before-sha256.json`/`player-status-tint-changed-files.json`에 보존한다. Editor/Game 빌드는 C++ 변경이 없어 이번에 재실행하지 않았으며 이전 성공 기록과 구분한다. 서버 FGameplayTag·RPC·Replication·Phase·EOS·차량/배경/카메라·상단 WBP는 변경하지 않았다. feature/title-world-ui 유지, Git/UVCS 변경 명령은 실행하지 않았다. 잠금 조회 결과는 이번 확인 시 빈 목록이었으며 기존2건을 직접 해제하지 않았다. 상태판 색상안은 사용자에게 실제 캡처로 제출한다.

### 11.18 하단 차량과 플레이어 이름의 겹침 보정 — 2026-11-03

사용자가 11.17 캡처에서 차량 외형이 이름 영역을 침범하는 것을 지적했다. 특히 Doug의 하단 외형과 이름이 붙어 보여, 11.16의 `차종별 그림이 겹치지 않는다`는 기존 판정은 이번 실제 화면으로 교정한다. TextBlock 수용 여부만으로 판정하지 않는다.

`WBP_VehiclePlayerEntry_Dev`의 기존 Image_Vehicle Canvas Slot만240.5×240.5→220×220(변 길이 약8.5% 축소), 위치(9.75,-8)→(20,-8)로 바꿨다. 판의 수평 중심130을 유지한다. 이미지 영역 하단212와 기존 이름 Fit 영역 상단216을 분리했다. 이4 Designer px 간격 외에 텍스처 알파와 실제 글자 잉크에 따른 여백은 캡처로 별도 확인했다. 이미지 비율·원본 아이콘·플레이어명 위치/폰트/크기·판 Sprite/볼트·상태판 PlayerTint·상단 안내는 그대로다. 차종별 임의 보정이나 C++ 좌표 분기는 없다.

실제 저장본 PIE1920×1080에서 동일4인/동일 차량 Smiet·Mammoth·Ufology·Doug의 수정 전후를 비교했다.960×540에서는 Fred가 선택된4인 화면까지 확인했다. 따라서5종 모두 이번 표시 크기의 실제 하단 출력을 확인했으나 모든 차종×모든 해상도 조합을 따로 캡처했다고 주장하지 않는다. Fred 선택 상태와 원형 고정·기존 확정 표시·서버 태그 무효 상태는 `input-state.jsonl`의 `VEHICLE-NAME-GAP-FRED`에 남겼다. 해당 자동 클릭 시도는 사용자 직접 입력 감지로 거부됐으므로 자동 클릭 성공으로 기록하지 않는다. 이후는 표시 확인만 수행했다. 전체 Back/취소/EOS/멀티플레이 회귀를 반복하지 않았고 기능 코드 변경도 없다.

[1920 실제 PIE](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/VEHICLE_NAME_GAP_1920.png), [960 실제 PIE](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/VEHICLE_NAME_GAP_960.png), [같은4인 차량의 전후 하단 확대](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/VEHICLE_NAME_GAP_COMPARISON.png). 원본 크기는 창 테두리 포함1932×1156/972×616이며 실제 PC viewport를 별도 조회했다. 비교본은 같은 위치를 자른 편집 자료다. 상태 변경 직후 첫 캡처에 글자 잔상이 보여, UI가 안정된 뒤 동일 파일로 다시 촬영해 최종 증거를 교체했다. 다량의 연속 PNG는 생성하지 않았다.

컴파일 중13:18:45 KST에 WidgetBlueprintCompiler.cpp815의 LegacyIdentityBadge 잔여 변수 GUID handled ensure가1회 발생했다. 앞선 상태판 색상 작업에서 표시 계층에서 분리·이름 변경한 옛 점 위젯의 메타데이터가 원인이었다. 설치 엔진 소스의 ValidateAndFixUpVariableGuids가 해당 무효 항목을 RemoveCurrent하는 것을 확인했고, 저장 후 재컴파일(13:19:51~52)에는 재발하지 않았다. 실제 이후 PIE 생성·색상 바인딩·차량 출력도 확인했다. `vehicle-name-gap-guid-ensure.log`, 재컴파일 이후 `vehicle-name-gap-validation.log`에 보존했다. 에디터 재시작 재로드까지 시험한 것은 아니며 기존 AppTime ensure와도 구분한다. 엔진·C++·전역 설정 변경 없이 WBP 컴파일/저장으로 처리했다.

이번 변경은 Entry WBP와 본 Master뿐이다. `vehicle-name-gap-before-sha256.json`/`vehicle-name-gap-changed-files.json` 해시 대조에서 Source·나머지 차량 UI·Content/Levels 직접 하위 맵은 동일했다. 최종 미저장 map/content0/0. C++·빌드 설정 변경이 없어 Editor/Game 재빌드는 실행하지 않았다. Git Commit/Push/Merge·UVCS Checkin/Update/Switch 등은 하지 않았다. 사용자 작업·상단 승인안·서버 계약·배경·카메라를 유지했다.

### 11.19 차량 전체 외형의 프레임 내부 맞춤 — 2026-11-03

11.18에서 하단 이름과의 겹침만 줄이고 기존 Image_Vehicle의 Canvas Y=-8을 남겨, 사용자가 지적한 Ufology 머리와 Doug 꼬리의 상단 프레임 침범을 놓쳤다. 11.18의 이름 분리 결과를 전체 프레임 내부 맞춤 완료로 해석하지 않는다. 이번에는 차종별 임의 오프셋 대신 기존 레이아웃의 SizeBox/ScaleBox를 재사용해 전체 그림의 표시 영역을 제한했다.

Image_Vehicle을 DesignCanvas에서 기존 `AlignmentPlayerImage > VehicleAspect` 안으로 다시 연결했다. 기존 AlignmentPlayerContent의 시작을(16,16), AlignmentPlayerImage는228×188, VehicleAspect는 ScaleToFit·중앙 정렬로 관리한다. 현재 정사각 원본 아이콘은188×188로 표시되어 논리상 x36..224/y16..204 안에 들어간다. 이름 Fit의 기존(26,216)/208×40, 전시판260², 테두리·볼트·상태판 PlayerTint는 유지했다. 원본 이미지·알파 패딩·비율은 변경하지 않았고 머리나 꼬리를 잘라 숨기는 방식도 아니다. 영역 ClipToBounds는 일탈 방지용이며 실제 전체 아이콘은 먼저 축소되어 영역 안에 들어간다. 새 위젯·클래스·이미지 에셋·차종별 데이터/좌표·C++ 수정은 없다.

동일4인 Smiet/Mammoth/Ufology/Doug 상태를 실제1920×1080 및960×540 PIE로 확인했다. Ufology 머리와 원본 아이콘의 반투명 장식, Doug 꼬리 끝이 모두 위쪽 프레임 아래에 들어오며 하단 이름과도 분리되는 것을 확대 캡처로 확인했다. Slot 수치만으로 판정하지 않았다. 이 단계에서 Fred의 별도 하단 재캡처·입력 전체 회귀·2/3인 전체·멀티플레이는 반복하지 않았다. 같은 공용 Image/ScaleBox 규칙을 사용하는 것과 실제 시험한 조합을 구분한다.

[1920 실제 PIE](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/VEHICLE_FRAME_FIT_1920.png), [960 실제 PIE](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/VEHICLE_FRAME_FIT_960.png), [하단 확대](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/VEHICLE_FRAME_FIT_DETAIL.png), [같은4인 전후 비교](C:/Users/user/Documents/Codex/2026-10-27/make-way-ue-0-make-way-2/work/p3-cursor-occlusion/VEHICLE_FRAME_FIT_COMPARISON.png). 실제 PC viewport를 조회했고 전체 PNG는 창 테두리 포함1932×1156/972×616이다. 확대·비교 PNG는 실제 출력의 기계적인 크롭/배치 자료이며 별도 PIE 출력으로 주장하지 않는다. 기존 증거 폴더를 사용했고 새 MD/ZIP·연속 프레임은 없다.

WBP 컴파일·저장·PIE 생성 성공, 미저장 map/content0/0. `FRAME_FIT_AUTHOR_BEGIN` 이후 로그에서 새 ensure/fatal/assertion을 찾지 못했다. `vehicle-frame-fit-validation.log`에 보존한다. 기존 AppTime ensure의 근본 해결을 주장하지 않는다. `vehicle-frame-fit-before-sha256.json`/`vehicle-frame-fit-changed-files.json` 대조에서 Entry WBP 외 Source·나머지 차량 UI·Content/Levels 직접 하위 맵은 동일했다. Editor/Game 재빌드는 C++/설정 변경이 없어 실행하지 않았다. feature/title-world-ui, UVCS Pending101경로를 유지했고 잠금 조회는 빈 목록이었다. Git/UVCS 변경·잠금 해제 명령은 수행하지 않았다. 최종 이미지 영역 보정은 Designer 설정이며 서버 계약·입력·상단 안내·색상 상태판·배경·카메라는 변경하지 않았다.

### 11.20 UI ↔ UCM 입력 계약 준비 — 2026-11-03

사용자가 명시한 연동 문서3개를 Client 문서 폴더에 작성했다. [PROJECTMW_UI_UCM_INTEGRATION_CONTRACT.md](PROJECTMW_UI_UCM_INTEGRATION_CONTRACT.md), [PROJECTMW_UI_REQUIRED_ASSETS.csv](PROJECTMW_UI_REQUIRED_ASSETS.csv), [PROJECTMW_UI_INTEGRATION_ENTRYPOINTS.md](PROJECTMW_UI_INTEGRATION_ENTRYPOINTS.md). 버전 UI-UCM-1.0이며 현재 UI 소비자 측 입력 계약이다. 공급자 상호 승인이나 실제 연결 완료로 해석하지 않는다.

실제 ProjectMW는 `C:/workspace/UE/ProjectMW/MW.uproject`, UCM은 `C:/Users/user/Documents/Codex/UCM_MasterLibrary/MakeWay_AssetReview.uproject`다. Source·실제 저장 CDO·Widget Tree12개·AssetRegistry·실행 중 Preview Actor를 읽었다. 카드5종은 Smiet/Mammoth/Ufology/Doug/Fred 순서이며 현재5개 UE Texture 조회값은512²이다. 운영 Definition0/DriveData0, 현재 카탈로그는 입력3개+맵 테마1개다. PreviewKey는 기존 썸네일의 전체 Object Path를 고정한 인계용 키이며 Runtime ID 필드를 추가한 것이 아니다. Unity SourceRef·내부 FName VehicleId·서버 FGameplayTag를 구분한다.

현재 차량은2D Thumbnail 입력이며 World Space WidgetComponent가 내부 RT를 사용하는 것과 차량 SceneCapture/3D 모델 Preview는 다르다. 현재 호스트는 WidgetComponent1개이고 차량 시각 BP 교체/정리 API는 없다. 기존 필드 PreviewCards[].Thumbnail 및 VehicleThumbnails에 받을 데이터와 후속3D의 미구현 범위를 구분했다. 트랙 UI·트랙 전용 정의 수신 API는 현재 로컬에 없고, 방 생성의 MapTheme는 UWorld다. Manifest133개를 UI/맵 테마에 자동 등록하지 않는다.

UCM 담당 채팅 `Extract car assets into UE5`의 기존 Handoff139행(차량5+Manifest133+별도 Preview1)과 의존성4586행/고유1019패키지를 읽었다. SourceRef와5개 실제 Visual BP 경로·SHA를 대조했으며 source BP를 Pawn/승인 차량으로 간주하지 않았다. 공급자에게 받을 필드·폴더·충돌·권한 범위를 문서화했다. 다른 채팅에 메시지를 보내거나 UCM 문서를 대신 수정하지 않았다.

검증: 요구 CSV64행/21열, requirement_id64개 고유, PreviewKey5개 고유·CDO 순서 일치, 선택적 시각 BP5개, 문서 링크28건 및 CSV에 기재한 기존 에셋 경로 오류0.64행은 보존할 기존 참조와 후속 보류 항목을 포함하며64개 신규 에셋 요청이 아니다. Source/Config/UI/맵·uproject 및 UCM 차량 BP를 포함한 보호 파일137개 SHA-256 대조 결과 변경0이다. `work/ui-ucm-contract/current-ui-audit.json`, `data-render-audit.json`, `before-sha256.json`, `validation.json`에 보존한다. 스크립트와 JSON은 기존 Codex 작업 경로 안에 있다.

이번 변경은 위 문서3개와 본 Master 링크/기록뿐이다. 새 UI·C++·WBP·카탈로그·레벨·상용 바이너리 복사0, 미저장 map/content0/0 유지. 새 PIE 실행/입력·빌드·Cook·UCM load/compile/spawn·원격 최신 fetch는 수행하지 않았다. 로컬 HEAD3db9c5d, main tracking6a47da9, Server/Lobby-Design tracking927fcff와 그 설계/실제 미구현 API를 구분했다. Git/UVCS Pending과 브랜치 보존, stage/commit/push/merge/checkin/update/switch 없음. 실제 연결은 이 UI 채팅에서 공급자 매핑을 재대조한 후 별도 승인 범위로 진행한다.


### 11.21 UCM 인트로·타이틀 및 장소별 카메라 연결 — 2026-11-03

이 절은 개발 프로필로 시험하던 당시 기록이다. 현재 실행 경로는 11.22절을 따른다.

사용자가 현재 `feature/title-world-ui` 브랜치에 직접 사용하도록 명시한 범위에서 로컬 반영했다. 기존 UI 디자인·방 생성/검색·EOS/로비 기능을 재작성하지 않았다. 보호 파일 4,288개(그중 UCM 마스터 4,052개)를 대조했으며 허용한 기존 TitleRoot 헤더/구현의 표시 연결 외 예상 밖 변경은 없다. 원본 Widget Blueprint, 공식 L_Title/L_Dev_Default, 기존 서버·차량 소스, 원본 UCM은 보존했다. 초기 535개와 후속 4개 신규 파일은 의존성 폐쇄와 통합 전용 파일이며 전체 UCM을 복사하지 않았다. 이번 후속은 자체 추가 파일 8개 갱신과 기존 Root에 해석 이벤트 2줄 추가다. 전체 목록은 아래 CHANGES.csv로 구분한다.

개발 프로필 `-CustomConfig=UCMFrontend`의 `/Game/Levels/L_Title_UCM_Integration_Dev`에서 기존 MW 로그인 성공 뒤 원본 UCM 인트로/스플래시/메인 카메라를 사용한다. 원본 상단 표지판 3개는 글자만 방 생성/방 검색/종료로 연결하고 아래 4개는 제외했다. 원래 복구되어 있던 표지판 기둥을 실제 소스 행렬로 배치했다. UCM 버튼은 MW의 기존 버튼 이벤트에 연결하고 별도 MW 메뉴를 연속 표시하지 않는다. GameInstance 수명 플래그로 메뉴 복귀 시 인트로를 반복하지 않는다.

사용자의 후속 지시에 따라 생성/참가는 기존 WorldDev **창형 위젯을 viewport에 유지**한다. 360도 루프 앞의 기울어진 world-space 폼 방식은 폐기했다. 배경은 원본 Main To Select Battle Mode 경로(기존 Roller A 쪽)로 이동하며 뒤로 가기는 동일 경로를 역행해 메인 메뉴로 돌아간다. 실제 로비는 기존 서버가 요구하는 타이틀/테마 구분을 지키는 개발용 영구 맵 `/Game/Levels/L_Lobby_UCM_Integration_Dev`에서 기존 BP_MWGameMode와 WBP_RoomLobby를 사용한다. 이 맵은 같은 UCM 장면을 AlwaysLoaded로 참조하며 별도 월드 외형을 재제작하지 않았다. 기존 EOS의 OpenLevel 때문에 UWorld 재로드는 발생한다. 로비 위젯 표시 크기는 원본 카메라 절두체 안에 들어가도록 어댑터 데이터만 맞췄다. Lobby To Select Vehicle와 역방향은 기존 BP_VehicleSelectionPreview_Dev/WBP_VehicleSelection_Preview를 그대로 호스팅한다.

원본 ProjectMW MWEditor 및 Game Development 빌드는 모두 Succeeded(최종 123.06초/122.89초). 해시를 대조한 격리 검증본을 새 편집기에서 재로드하고 실제 캐시 EOS 로그인·방 생성·호스트 등록·로비 1/3명, 차량 Preview 5종 장소 이동과 Back, 방 종료와 메뉴 복귀를 확인했다. 최종 PIE 실행 세션의 INTRO_PLAY는 1회, 카메라 바인딩 오류 0, 복귀 후 활성 Root 1개, 종료 전 미저장 map/content 0/0이다. 검증 행동은 엔진 delegate/API 호출이며 직접 마우스 클릭 성공으로 주장하지 않는다. 원본 프로젝트 실행/패키지 검증과 격리본 실행을 구분한다.

마우스 hardware input/hover는 구현했으나 PC 제어 도구가 `foreground window did not report a process id`로 실패하여 실제 클릭 회귀는 미완료다. 현재 브랜치는 실제 Ready/Start와 서버 차량 선택 단계가 미구현이므로 임의로 활성화하지 않았다. 차량 화면은 기존 PIE Preview 진입 API이며 실제 서버 단계나 추가 운영 메뉴가 아니다. 다계정 참가, Cook/패키지, 오디오(-NoSound 검증)와 Cinemachine 픽셀 동등성은 미검증이다. 기존 차량 WBP의 삭제 변수 GUID handled ensure 및 PlayerTint 경고는 원본을 덮어써 해결했다고 처리하지 않는다.

HEAD 3db9c5d 유지. 원격 main/Server/Lobby-Design은 최종 읽기 전용 ls-remote에서도 b03cd6e이며 병합하지 않았다. UVCS는 cs16/head28, 기존 Pending 보존. Git/UVCS stage/commit/push/merge/checkin/update/switch/reset/stash/checkout/잠금 변경 없음. 기본 시작 맵은 바꾸지 않았으며 개발 프로필로만 활성화한다. 상용 리소스 공개 배포 권한은 미확인 상태로 유지하며 로컬 사용 외 공개 업로드·등록은 하지 않았다.

증거: [화면 모음](C:/Users/user/Documents/Codex/2026-11-03/projectmw-ucm-ui-unreal-engine-5/outputs/RESULT.html), [검증과 미완료 항목](C:/Users/user/Documents/Codex/2026-11-03/projectmw-ucm-ui-unreal-engine-5/outputs/VALIDATION.json), [전체 변경 파일](C:/Users/user/Documents/Codex/2026-11-03/projectmw-ucm-ui-unreal-engine-5/outputs/CHANGES.csv), [현재 브랜치 개발 프로필 실행](C:/Users/user/Documents/Codex/2026-11-03/projectmw-ucm-ui-unreal-engine-5/outputs/Open_MW_UCM.ps1). 초기 잘못된 루프 앞 폼 및 Editor viewport 오캡처는 RESULT/VALIDATION의 최종 증거에서 제외했다.


### 11.22 기본 L_Title에 UCM 타이틀·로비 적용 — 2026-11-05

기본 실행은 `L_Title`에서 로그인·인트로·표지판 메뉴를 표시한다. 방 생성 시 같은 맵을 `?listen`으로 다시 열고, 참가자도 그 로비에 합류한다. 전원 Ready와 호스트 시작 이후에는 로비에서 차량 선택 카메라로 이동한다. 서버가 모든 차량 선택을 확정하면 공용 카탈로그의 `Asset.MapTheme.DriveDefault`가 가리키는 `L_Dev_Drive`로 이동하고 참가자별 Definition의 Pawn을 생성한다.

`L_Title`의 Always Loaded 서브레벨에 기존 `L_Title_UCM_Integration_Dev` 씬을 연결한다. UCM 연출 액터가 타이틀 화면을 소유하므로 기존 Level Blueprint의 타이틀 위젯 생성·정리 그래프는 제거했다. `L_Lobby_UCM_Integration_Dev`는 비교용 테스트 맵이다. 카메라 배치와 위젯은 공유 씬에서 관리한다.

커서와 씬의 Cook 경로를 기본 Game 설정으로 옮겼다. 에디터·게임 시작 맵과 연결 설정은 기본 프로필을 사용하며 별도 `-CustomConfig` 인자가 필요 없다.

로비를 나갈 때 마지막 카메라의 위치·회전·시야각을 저장한다. `L_Title`을 다시 열고 같은 구도에서 로비 경로를 역방향으로 재생한 뒤 방 설정 위치에서 멈춘다. 호스트는 방 정보 입력 폼을 다시 열고, 참가자는 경기 참가 UI를 다시 연다. EOS 세션 정리가 끝나면 참가자 쪽 방 목록을 조회한다. 각 화면에서 뒤로 가기를 누르면 메인 메뉴로 돌아간다. 세션 정리 지연이나 복귀 오류는 별도 상태 패널로 표시한다.

로비와 차량 선택 보드의 버튼은 `PreciseClick`으로 처리한다. 월드 위젯에서 마우스를 캡처하지 않고 같은 버튼 위에서 누르고 뗐을 때 기존 `OnClicked`를 호출한다.

별도 Standalone `-game` 프로세스 2개의 LocalIP 검증에서 양쪽 Ready 반영, 참가자 퇴장과 방 목록 복귀, 호스트의 인원 감소, 같은 방 재합류를 확인했다. 호스트의 게임 시작과 양쪽 차량 카드도 직접 클릭했다. 서로 다른 차량이 확정됐고 두 플레이어가 `L_Dev_Drive`에 이동해 각 차량을 소유했다. EOS 방 검색 결과까지 새로 조회하는 동작은 이 검증 범위에서 제외했다.

복귀 화면 변경 후 별도 Standalone 2인 LocalIP로 다시 검증했다. 참가자가 직접 나갔을 때와 호스트가 방을 닫았을 때 모두 참가자 창에 방 목록이 표시됐다. 호스트는 복원된 폼에 새 방 이름을 입력하고 생성 버튼으로 다시 로비에 진입했다. 양쪽 화면에서 뒤로 가기를 눌렀을 때 메인 메뉴로 이동했다.

기본 프로필의 EOS 로그인 위젯 표시와 3인 LocalIP PIE를 확인했다. 당시 복귀 경로에서 방 종료 후 메뉴 복귀와 재생성, 세 참가자의 같은 로비 합류, 차량 보드 표시, 전원 확정 후 `L_Dev_Drive` 이동과 선택한 Pawn의 소유가 통과했다. 실제 EOS 인증·방 목록 조회와 패키징은 이번 실행 검증에 포함하지 않았다.
