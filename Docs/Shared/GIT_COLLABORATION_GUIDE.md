# 3인 팀 Git 브랜치·리뷰·병합 공통 규칙

**적용:** Art·Client·Server·TA·문서·설정 등 ProjectMW의 모든 변경. Git 작업 전 필독한다.
**정책:** v1.0 / 2026-10-22. 구체 게임 요구와 담당 영역은 기존 기획·분야 명세를 따른다.

이 문서는 Git 협업 규칙의 단일 원본이다. 다른 문서는 담당 영역과 기능 검증만 보완한다. 목표는 충돌 가능성을 줄이고, 다른 사람의 작업을 보존하며, 검증한 변경만 main에 반영하는 것이다. 아래 필수 기준은 팀 정책이며, Git/GitHub가 자동으로 강제하는 설정과 구분한다.

## 1. 반드시 지킬 기준

1. `main`은 검증된 통합 기준이다. 직접 개발·직접 push·일괄 브랜치 병합을 기본 절차로 사용하지 않는다.
2. 최신 `origin/main`에서 **한 작업·한 책임자·한 짧은 브랜치·한 PR**을 만든다. 사람별 장기 브랜치와 별도 상시 `develop`은 두지 않는다.
3. 변경 전에 실제 루트·브랜치·원격·진행 중 Git 작업·기존 변경을 확인한다. 기존 사용자 변경은 자동 stash·초기화·폐기하지 않는다.
4. 다른 사람이 받았거나 push한 커밋의 rebase·amend와 force push는 하지 않는다. 수정은 후속 커밋으로 남긴다.
5. 공유 API/데이터/설정은 생산자와 소비자를 함께 변경한다. 미완성 계약을 먼저 main에 넣고 다른 사람이 나중에 고치게 하지 않는다.
6. `.uasset`·`.umap`은 **동일 파일에 한 명만 작업**한다. LFS 추적만으로 잠금과 의미 있는 병합이 보장되지는 않는다.
7. PR에는 정확한 변경 범위·검증 커밋·실행 결과·에셋 기록·되돌릴 방법을 남긴다. 관련 없는 파일을 함께 stage하지 않는다.
8. 일반 변경은 작성자가 아닌 팀원 **1명 이상**, 고위험 변경은 **나머지 2명**의 검토를 받는다. 자기 승인·에이전트 자체 검토는 독립 리뷰를 대체하지 않는다.
9. 병합은 담당자 한 명이 **PR 한 개씩 순차 실행**한다. 앞 PR 반영 후 다음 PR의 기준·충돌·검증·승인을 다시 확인한다.
10. 충돌 해결은 작업 브랜치에서 한다. 전체 파일을 일괄 `ours/theirs`로 고르거나 바이너리 포인터를 편집해 해결하지 않는다.
11. main 문제는 신규 통합을 멈추고 수정/되돌리기 PR로 복구한다. 공유 이력을 reset·force push로 되감지 않는다.
12. 커밋·원격 push·PR 생성·main 병합·브랜치/태그 삭제는 각각 요청 범위와 적용 정책을 확인한다. 과거의 일회성 push/병합 요청을 이후 작업 전체의 상시 허가로 해석하지 않는다.

## 2. 확인된 저장소 상태와 남은 설정

2026-10-22 확인 시점의 스냅샷이다. 작업할 때 다시 조회하고, 설정이 바뀌면 이 표를 갱신한다.

| 항목 | 확인 결과 | 정책 적용에 필요한 사항 |
|---|---|---|
| 저장소 | `kevin6091/ProjectMW`, 공개 GitHub 저장소, 기본 브랜치 `main` | 각자 GitHub 계정·저장소 권한·커밋 작성자 확인 |
| 로컬 출발점 | 문서 작업 전 `main`과 fetch한 `origin/main`이 동일, 작업 트리 깨끗함 | 이후에도 동일하다고 가정하지 않음 |
| LFS | Git LFS 3.7.1, `.uasset`/`.umap` 추적 속성 존재 | 실제 파일 업로드/다운로드와 잠금 차단은 별도 확인 |
| 파일 잠금 | `git lfs locks --json` 조회 성공, 당시 빈 목록. `lockable`은 미설정 | 9절의 잠금 운영 및 팀원별 환경 검증 필요 |
| PR 병합 방법 | merge commit·squash·rebase 모두 허용, auto-merge 꺼짐 | 팀 기본은 merge commit. 허용 옵션 제한은 별도 설정 사항 |
| main 보호 | 보호 규칙/우회 권한의 실제 적용 여부 미확인 | 13절 기준으로 관리자 확인 필요 |
| 자동 검사 | 현재 `.github/workflows` 없음, 외부 CI는 미확인 | 실제 실행되는 검사를 준비한 뒤 Required check로 등록 |
| CODEOWNERS | 현재 파일 없음, A/B/C의 실제 계정 매핑 미확정 | 계정·write 권한 확인 후 별도 적용 |

이 문서를 추가했다고 main 보호나 LFS 강제가 자동 활성화되는 것은 아니다. 현재 미적용/미확인 항목을 적용 완료로 보고하지 않는다.

## 3. 브랜치 구조와 작업 단위

| 브랜치 | 용도 | 수명/반영 |
|---|---|---|
| `main` | 검증된 통합 상태 | 보호 대상, PR로 반영 |
| `feature/a-vehicle-knockback` | A의 차량 넉백 등 한 기능 | 예시. 검증·병합 후 종료 |
| `feature/b-track-placement` | B의 서버 배치 등 한 기능 | 예시. 생산자/소비자까지 완료 |
| `feature/c-item-boost` | C의 아이템 등 한 기능 | 예시. 관련 에셋은 명시 |
| `fix/a-respawn-input` | 재현 가능한 결함 수정 | 원인·회귀 확인 포함 |
| `docs/shared-git-workflow` | 공용 문서 작업 | 문서 검사·독립 리뷰 |
| `hotfix/b-match-blocked` | 배포/통합 진행을 막는 긴급 수정 | PR·리뷰·필수 검증을 유지 |

- 이름은 예시이며 A/B/C는 실제 담당자 역할이다. `feature/a-vehicle` 같은 브랜치를 프로젝트 내내 재사용하지 않는다. 같은 파일을 만지는 장기 병행 작업을 줄인다.
- 하루 안에 끝낼 수 있는 검증 가능한 작은 변경을 우선한다. 일정을 맞추려고 미완성 PR을 강제로 병합하지 않는다. 장기 작업은 API·최소 동작·후속 확장을 독립적으로 검증 가능한 단위로 나눈다.
- 다른 사람의 feature 브랜치를 임의로 기반/병합하지 않는다. 의존 API는 먼저 작은 PR로 main에 넣고 소비 브랜치가 받는다. 불가피한 의존 PR은 base·순서·포함 커밋을 명시하고 독립 검토한다.
- 자동 정렬·포매팅·이름 변경·폴더 이동·에셋 일괄 재저장은 기능 변경과 분리한다. 줄바꿈 정책 변경/재정규화도 별도 PR로 다룬다.

## 4. 담당 영역과 파일 예약

- 기존 명세의 A(차량/입력/카메라), B(서버 규칙/건설), C(아이템/TA)를 기본 담당으로 한다. 담당자는 계약·리뷰 책임을 가지며, 바이너리의 현재 수정자는 별도로 한 명만 지정한다.
- 공용 타입·RPC·Build.cs·uproject·기본 Config·Input/Collision 설정·메인 맵은 변경 전에 영향 파일과 소비자를 맞춘다. 공용 타입/Config를 각 기능 브랜치에서 서로 다르게 정의하지 않는다.
- 실제 파일 경로가 기준이다. 현재 맵 실파일은 `Content/Levels/L_Dev_Default.umap`이며 기존 명세의 `MP_Core.umap`은 제안 이름이다. 파일 존재/연결을 확인하고 잠근다.
- 공유 맵은 B가 통합하고 A/C는 별도 테스트 레벨을 사용한다. 이동/이름 변경/Redirector 정리는 참조 에셋까지 영향을 줄 수 있으므로 별도 예약·PR로 진행한다.
- 작업 예약에는 `실제 경로 / 담당자 / 브랜치 / 기준 main SHA / 목적 / 잠금 ID 또는 중앙 예약 기록 / 시작·예상 인계 시점 / 상태`를 남긴다. 예약은 자동 만료시키지 않고 지연/취소 시 인계한다.
- 예약 대장은 같은 파일을 여러 브랜치에서 수정하는 방식으로 만들지 않는다. 팀 공용 이슈/게시판 등 한 곳을 정하고, 수동 운영이면 담당자 한 명이 예약을 순차 확정한다. 에이전트가 팀원에게 메시지를 보내는 권한은 별도로 따른다.

## 5. 작업 시작과 로컬 환경

한 checkout은 한 작업/에이전트가 쓰는 것을 기본으로 한다. 병행 작업은 각자 clone 또는 분리된 worktree를 쓴다. 같은 폴더에서 Fork·터미널·에이전트가 서로 브랜치를 바꾸지 않는다.

```powershell
git rev-parse --show-toplevel
git status --short --branch
git remote -v
git branch -vv
git diff --stat
git diff --cached --stat
```

- 예상 경로/브랜치/원격이 아니거나 merge/rebase가 진행 중이면 원인부터 확인한다. 미커밋 변경이 있으면 소유자와 범위를 파악해 보존하고, 무관한 변경을 임의 커밋하거나 stash하지 않는다.
- 각자 `user.name`/`user.email`과 GitHub 인증 계정을 확인한다. 커밋 작성자와 push 인증 계정은 다른 정보다. 한 계정을 공동 사용하거나 토큰을 문서/로그에 남기지 않는다.
- 에셋이 바뀌는 switch/pull/merge/revert 전에는 Editor의 저장되지 않은 작업을 보존하고 프로젝트를 닫는다. 열린 Editor가 이전 상태로 다시 저장할 가능성을 제거한다.
- 위 확인과 깨끗한 상태 확인 후 다음처럼 만든다. 아래 이름은 예시이며, 각 명령의 성공을 확인한 뒤 다음 명령으로 넘어간다.

```powershell
git fetch origin
git switch --no-track -c feature/a-vehicle-knockback origin/main
```

`--no-track`은 새 작업 브랜치가 실수로 `origin/main`을 upstream으로 삼는 것을 피한다. 기존 동명 브랜치를 덮어쓰지 않는다. 로컬 main 갱신은 깨끗한 main에서 `git pull --ff-only origin main`을 사용하고, 분기됐으면 reset하지 말고 차이를 조사한다. [Git pull](https://git-scm.com/docs/git-pull)

## 6. 작업 브랜치 동기화와 커밋

1. 시작/검토 요청/병합 직전에 fetch한다. 큰 공용 API나 에셋 변경을 받기 전에도 예약과 작업 상태를 확인한다.
2. **현재 작업 브랜치**의 변경을 보존·커밋한 깨끗한 상태에서 `git merge origin/main`으로 최신 main을 반영한다. 이미 공개한 브랜치를 rebase하거나 충돌을 자동 숨기는 pull/autostash 옵션으로 대체하지 않는다.
3. 충돌이 나면 10절에 따라 해결한다. 자동 병합됐어도 함수 의미·BP 연결·설정 값과 기능을 다시 확인한다.
4. 명시한 파일만 stage하고 `git diff --cached`로 파일·줄·삭제·비밀값·불필요한 에셋 저장을 확인한다. 에이전트의 기본 stage 명령으로 `git add .`/`git add -A`를 사용하지 않는다.
5. 커밋은 문제/의도가 드러나는 단위로 남기고, 임시 에셋·누락한 소비자 때문에 그 변경 자체를 실행할 수 없는 상태를 main에 제출하지 않는다.
6. 원격 push가 허용된 작업이면 **원격과 작업 브랜치명을 명시**한다. `--all`, `--mirror`, force, hook/LFS 검사 우회를 사용하지 않는다. upstream이 main인지 확인하지 않은 무인자 push를 피한다.

저장소별 선택 설정으로 `git config --local pull.ff only`, `git config --local merge.conflictStyle diff3`를 검토할 수 있다. 이는 이 문서 작성에서 적용한 설정이 아니며 전역 설정을 일괄 변경하지 않는다. 공개 이력 재작성의 영향은 [Git rebase](https://git-scm.com/docs/git-rebase)를 참고한다.

## 7. PR 내용·검증·리뷰

[PR 템플릿](../../.github/PULL_REQUEST_TEMPLATE.md)을 사용한다. 동작과 범위를 먼저 설명하고, 다음 정보를 확인 가능하게 남긴다.

- base=`main`, source 브랜치, 작성자·검토자·병합 담당자, 관련 작업/요구, 바뀐 계약·데이터·파일.
- 검증한 feature HEAD SHA와 기준 main SHA, 빌드/엔진/실행 환경, 성공·실패·미실행 항목. 스크린샷만으로 네트워크 판정이나 패키징 성공을 대신하지 않는다.
- 바이너리는 실제 경로·잠금/예약 소유자·전후 화면/설정·BP 컴파일/참조·관련 맵/패키지 검사를 적는다. LFS 포인터 diff만으로 내용 리뷰를 마치지 않는다.
- 문서만 수정했다면 링크·규칙 정합성·diff 검사를 수행한다. C++·네트워크·물리·데이터 변경은 [개발 필독 가이드](../Engineering/DEVELOPMENT_GUIDE.md)의 해당 검증을 따른다. 모든 문서 PR에 UE 전체 빌드를 요구하지 않는다.
- 테스트를 돌린 뒤 코드/에셋/충돌 해결 결과가 바뀌면 영향받는 검증과 리뷰를 다시 받는다. 기준 main이 바뀌면 최신 통합 결과를 재확인한다.

| 위험 | 독립 리뷰 기준 | 예 |
|---|---|---|
| 일반 | 작성자가 아닌 팀원 1명 이상 | 한 기능 내부 수정, 일반 문서 |
| 고위험 | 작성자를 제외한 두 팀원 검토 | 공용 API/복제 계약·main 맵/공용 BP·빌드/기본 Config·Git 규칙/보호/LFS·광범위 이동/삭제·main 복구 |

공동 작성으로 독립 리뷰 두 명을 확보할 수 없으면 변경을 분리하거나 팀이 명시적으로 검토 방법을 정한다. 에이전트가 기준을 낮췄다고 간주하지 않는다. 영역 담당자가 작성자면 다른 팀원이 검토한다. CODEOWNERS만 추가해 여러 지정자 전원의 승인이 강제된다고 가정하지 않는다. 같은 경로에 여러 소유자가 있어도 GitHub는 그중 한 명의 승인으로 충족할 수 있다. [Code owners](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/about-code-owners)

## 8. main 병합 순서

1. **팀 전체에서 동시에 병합을 실행하는 담당자는 한 명**으로 정한다. 공용 기록에 진행 PR·기준 SHA·병합 시작/완료를 남기고 다음 담당자에게 인계한다. A/B/C가 교대할 수 있으며 B에게 모든 병합을 고정하지 않는다. 작성자 단독 판단으로 반영하지 않는다.
2. base/source·전체 diff·포함 커밋·필수 리뷰·미해결 의견·필요한 검사·잠금 소유자·복구 방법을 확인한다. 의도하지 않은 다른 브랜치 커밋이 포함됐으면 먼저 분리한다.
3. 최신 main을 작업 브랜치에 반영한 **실제 병합 후보**를 확인한다. 그 기준과 최종 HEAD에 대한 관련 검증/승인을 확인한 뒤 한 PR만 반영한다. main이 먼저 움직였으면 갱신·충돌 해결·영향 검증을 다시 한다.
4. 팀 기본은 GitHub PR의 **Create a merge commit**이다. 커밋과 병합 지점을 보존해 추적한다. 이는 프로젝트 선택이며 모든 팀에 유일한 정답이라는 의미는 아니다. Squash/Rebase로 임의 전환하지 않는다. [병합 방식](https://docs.github.com/en/pull-requests/reference/pull-request-merges)
5. main 반영 후 변경 종류에 맞는 빠른 통합 확인을 한다. 실패하면 다음 병합을 멈춘다. 다음 PR은 갱신된 main을 기준으로 다시 확인한다.
6. 에셋 인계를 마친 뒤 잠금을 해제하고, 병합·의존 PR·미보존 변경·다른 사용자의 사용 여부 확인 후 브랜치를 정리한다. 로컬 삭제는 안전 검사되는 `git branch -d`가 기본이다. `-D`로 검사 실패를 우회하지 않는다. 원격 삭제는 별도 권한/요청 범위를 따른다.

3인 팀은 수동 순차 병합을 기본으로 한다. 자동 병합/merge queue는 실제 CI와 팀 운용을 검증해 채택할 때만 추가한다. “모든 브랜치 머지” 요청도 목록·변경·미병합 커밋을 조사한 뒤 PR별로 처리하고, 브랜치 목록을 그대로 merge/push하는 반복문으로 실행하지 않는다.

## 9. UE 바이너리와 LFS 잠금

현재 `.gitattributes`는 LFS 추적만 정의한다. 잠금 운영 도입 시 다음 속성을 별도 검토·PR로 적용하고, 세 명 모두 설치/인증·읽기 전용 상태·잠금 획득/해제·타인 잠금 파일 push 차단을 시험한다.

```gitattributes
*.uasset filter=lfs diff=lfs merge=lfs -text lockable
*.umap filter=lfs diff=lfs merge=lfs -text lockable
```

- 잠금은 **같은 저장소의 같은 실제 경로 전체**를 독점 대상으로 운영한다. 다른 브랜치라는 이유로 같은 파일을 동시에 수정하지 않는다. 잠금 전에 최신 main의 파일을 받아 기존 담당자의 미병합 작업을 확인한다.
- 조회 후 소유자 확인 → 수정 전 잠금 획득 성공 → Editor 작업 → 지정 파일 commit/push → PR 검증/병합 → main 반영 및 수신 확인 → 소유자 해제 → 다음 담당자 순서다. commit/push 직후 바로 잠금을 풀지 않는다.
- 실제 파일 예시는 아래와 같다. 조회와 획득의 성공·소유자를 확인하고, **unlock은 병합/인계 후에만** 실행한다. 아래 명령을 한꺼번에 실행하지 않는다.

```powershell
git lfs locks
git lfs lock "Content/Levels/L_Dev_Default.umap"
# 편집·커밋·PR·main 반영·인계가 완료된 뒤
git lfs unlock "Content/Levels/L_Dev_Default.umap"
```

- 실제 LFS endpoint의 `lfs.<url>.locksverify`와 pre-push hook이 동작하는지 확인한다. 잠금 서버 오류를 무시하거나 LFS hook을 건너뛰며 push하지 않는다. `lockable`/읽기 전용은 클라이언트 보조 장치이며 서버 측 강제 보호와 동일하지 않다. [LFS lock](https://github.com/git-lfs/git-lfs/blob/main/docs/man/git-lfs-lock.adoc), [LFS config](https://github.com/git-lfs/git-lfs/blob/main/docs/man/git-lfs-config.adoc)
- 잠금이 불가능하거나 아직 검증되지 않았으면 공유 바이너리는 지정 담당자만 중앙 예약을 확인하고 순차 편집한다. 잠금 조회 오류/담당자 불명 상태에서 다른 사람 작업을 덮어쓰는 편집·push는 보류한다. 독립 코드/문서 작업은 계속한다.
- 타인 잠금을 강제 해제하지 않는다. 담당자 부재/취소는 원본 보존·미병합 커밋 확인·명시적 인계 후 관리자 절차로 처리한다. [LFS unlock](https://github.com/git-lfs/git-lfs/blob/main/docs/man/git-lfs-unlock.adoc)
- 새 파일도 경로 예약을 먼저 하고, 경로 변경은 기존/새 경로와 참조 패키지를 함께 관리한다. Editor에서 이동/이름 변경하고 Redirector 및 참조 정리를 검토한다. 탐색기 이동이나 전 프로젝트 무차별 Fix Up/재저장으로 다른 작업을 끌어들이지 않는다. [Epic Redirectors](https://dev.epicgames.com/documentation/en-us/unreal-engine/asset-redirectors-in-unreal-engine)
- LFS 적용 전에 이미 큰 바이너리가 일반 Git 이력에 들어갔다면 별도 이관 계획을 세운다. 활성 협업 중 `git lfs migrate`로 공유 이력을 임의 재작성하지 않는다.

## 10. 충돌 해결

- Git이 자동 병합해도 API 의미·설정 값·BP 참조가 맞는지 검증한다. “충돌 표시 없음”과 “통합 성공”을 구분한다.
- 텍스트는 공통 조상·내 변경·main 변경을 비교해 두 의도를 보존한다. 공용 계약/설정은 담당자와 소비 경로를 확인한다. `ours/theirs` 방향은 작업 종류에 따라 다르므로 파일 전체 일괄 선택을 기본으로 쓰지 않는다.
- 바이너리는 두 사람이 만든 파일을 텍스트처럼 합치지 않는다. 각 결과를 보존하고 담당자 한 명이 최신 승인본을 기준으로 다른 변경을 Editor에서 재적용한다. Unreal 비교/병합 도구가 지원하는 특정 에셋도 결과를 검토·재검증한 경우만 사용한다.
- LFS 포인터의 OID/size를 수동 조합하지 않는다. 실제 패키지 내용·참조·LFS 객체가 일치해야 한다.
- 해결 후 diff·BP/소스 컴파일·해당 기능·참조/패키지 등 영향받는 항목을 검사하고 PR을 갱신한다. GitHub 웹 편집기로 복잡한 코드/Config 충돌만 해소하고 검증을 생략하지 않는다.
- 진행 중인 merge를 중단하려면 상태 확인 후 `git merge --abort`를 쓴다. 시작 전에 미커밋 변경이 있으면 원상 복원이 보장되지 않으므로 보존부터 한다. 이미 진행 중인 rebase의 취소는 그 상태를 확인해 별도로 처리한다. [Git merge](https://git-scm.com/docs/git-merge)

## 11. main 복구·안정 태그·정리

- main 결함은 영향 PR/커밋·재현·현재 잠금을 기록하고 다음 통합을 중지한다. 마지막 정상 빌드/태그를 시연에 사용할 수 있지만, 공유 main 이력을 과거로 강제 이동시키지는 않는다.
- 최신 main에서 수정 또는 되돌리기 브랜치를 만들고 PR로 반영한다. merge commit을 되돌릴 때는 부모를 확인한 뒤 `git revert -m 1 <merge-commit>`의 기준이 main인지 검토한다. 취소 후 후속 변경/에셋 참조까지 회귀 검사한다. [Git revert](https://git-scm.com/docs/git-revert)
- merge를 revert한 뒤 옛 브랜치를 그대로 재병합하면 수정이 다시 들어온다고 가정하지 않는다. 재적용은 새 수정 커밋 또는 명시적인 되돌리기 취소 계획으로 검토한다.
- 안정 태그는 실제 통과한 main SHA와 빌드·엔진·환경·검증 결과를 연결한다. 태그 이름은 재사용/이동하지 않는다. 매일 확인하되 통과하지 않은 날에는 정상 태그를 억지로 만들지 않는다.
- 비정상 종료·잠금 방치·미완료 브랜치는 삭제보다 보존/인계를 우선한다. 다른 사람 브랜치·태그·stash를 추측으로 정리하지 않는다.

## 12. Fork와 에이전트 실행 기준

- Fork는 Git 클라이언트다. 현재 브랜치·작업 경로·원격이 실제 Git 상태와 일치하는지 확인한다. 앱 이름 Fork와 GitHub의 별도 fork 저장소를 혼동하지 않는다.
- Fetch는 원격 참조 확인, Pull은 현재 브랜치 통합, Merge는 **현재 브랜치가 받는 쪽**이다. 브랜치를 선택한 화면만 보고 대상이 바뀌었다고 가정하지 않는다. FF-only/명시적 merge를 GUI에서 확인하기 어렵다면 CLI로 정확히 수행한다.
- 자동 stash, 자동 rebase, force, 전체 stage/push, 충돌 전부 선택은 확인 없이 사용하지 않는다. Fork와 터미널의 작업이 겹치면 한 작업이 끝난 뒤 상태를 다시 읽는다.
- 에이전트는 수정 전에 브랜치를 조사하고, main이 깨끗하면 별도 작업 브랜치를 만든다. 기존 작업 브랜치/미커밋 변경이 있으면 범위를 확인하고 임의로 다른 작업과 섞지 않는다.
- 사용자가 main 반영을 요청하면 이 규칙의 PR·검증·리뷰 경로로 준비한다. 권한/검사/리뷰가 부족하면 부족한 조건을 정확히 알리고 안전한 작업 브랜치와 변경을 보존한다. 보호 규칙 우회나 승인 조작으로 완료하지 않는다.
- 이력 재작성·대량 삭제·LFS 이관·보호 변경을 일반 push의 부수 작업으로 취급하지 않는다. 이미 명시적으로 허가된 범위는 반복 확인하지 않고 수행한다.

## 13. GitHub 보호 설정의 목표 상태

현재 적용 확인과 실제 설정 변경은 별도 작업이다. 관리자가 확인할 때 아래 결과·날짜·적용 대상을 기록한다. 공개 저장소의 보호 브랜치 지원 및 설정은 [GitHub 공식 문서](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-protected-branches/about-protected-branches)를 따른다.

| 설정 | 목표 |
|---|---|
| main PR 필수 | 직접 push를 통합 경로로 사용하지 않음 |
| 승인 | 최소 1명, 고위험은 정책상 두 팀원 검토를 추가 확인 |
| 승인 갱신 | 새 변경으로 낡은 승인을 무효화, 최신 변경 독립 검토 |
| 대화 | 미해결 리뷰 대화 처리 후 병합 |
| 관리자/우회 | 관리자도 같은 규칙 적용, 상시 우회 사용 안 함 |
| force push/삭제 | main에서 허용하지 않음 |
| Required checks | 실제 있고 안정적으로 실행되는 고유 검사 이름만 지정 |
| 최신 main | 최신 base를 포함한 검증 강제. CI가 없으면 수동 통합 증거 확인 |
| 병합 방식 | merge commit 기본, 이를 막는 linear history 요구는 사용하지 않음 |
| 자동 병합 | 초기에는 꺼 두고 병합 담당자가 순차 실행 |

문서만 바뀐 PR의 검사가 경로 필터 때문에 영구 Pending이 되지 않게 설계한다. 체크 이름만 등록해 놓고 실행기를 마련하지 않거나, 로컬 1PC 실행을 GitHub CI 성공으로 대체하지 않는다. CODEOWNERS를 도입할 때는 실제 계정·권한·경로와 두 명 검토 정책의 차이를 확인한다.

## 14. 근거와 프로젝트 선택의 구분

- Git/GitHub의 기능·제약은 각 절의 공식 문서가 근거다. 짧은 브랜치, merge commit 기본, 고위험 두 명 리뷰, 수동 순차 병합은 **3인 UE 팀을 위한 운영 선택**이다.
- LFS는 큰 파일을 포인터와 외부 객체로 관리한다. 파일 잠금·리뷰·의미 있는 에셋 통합은 별도로 운영한다. [Git LFS 공식 매뉴얼](https://github.com/git-lfs/git-lfs/blob/main/docs/man/git-lfs.adoc)
- Unreal의 소스 관리 기능은 사용한 provider에 따라 다르다. Epic의 Checkout 설명을 현재 Git 플러그인의 잠금 보장으로 그대로 옮기지 않는다. [Epic Source Control](https://dev.epicgames.com/documentation/en-us/unreal-engine/source-control-in-unreal-engine)
- GitHub는 PR을 최신 base로 갱신하는 merge/rebase 방식을 제공한다. 이 프로젝트는 공개 이력 보존을 위해 merge로 갱신한다. [PR 동기화](https://docs.github.com/en/pull-requests/how-tos/create-pull-requests/keeping-your-pull-request-in-sync-with-the-base-branch)
- 문서 링크는 2026-10-22 확인했다. 이 규칙도 실제 충돌·리뷰 지연·복구 사례를 근거로 수정하고, 단순 편의를 위해 검증/보존 기준을 생략하지 않는다.
