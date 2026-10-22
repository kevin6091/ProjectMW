# 개발 공통 작업 규칙

Client/Server와 TA 코드 작업은 공용 문서, 이 파일, `Docs/Shared/Client,Server` 전체를 먼저 읽는다. 그 뒤 실제 영향을 받는 분야 지침을 적용한다.

- [Git 공통 규칙](../Shared/GIT_COLLABORATION_GUIDE.md): 작업 브랜치·공유 계약 변경·리뷰·main 순차 병합·충돌 복구.
- [개발 전 필독 가이드](DEVELOPMENT_GUIDE.md): 요구·책임·계약·수명·코드 품질·검증·완료 기준. 개발/설계/리뷰 전에 읽고 작업에 해당하는 항목을 적용한다.
- [Client 지침](Client/WORK_RULES.md): 로컬 입력·표시·예측·소유자 요청.
- [Server 지침](Server/WORK_RULES.md): 권한·검증·규칙·복제·접속/매치 진행.
- C++/Blueprint 파일이라는 이유만으로 Client로 분류하지 않는다. Authority에서 수행하는 판정과 소유 클라이언트 동작을 확인한다.
- 공유 구조체/API/복제 상태를 변경하면 생산자·소비자·수명과 초기화 경로를 함께 확인한다.
- 기존 UE **5.8.3** 프로젝트와 `MW` 모듈/폴더 구조를 기준으로 작업한다. 버전 변경·기능 범위 확대는 실제 요청과 명세에 근거한다.
- Content Browser의 BP·DataAsset·Input·Animation 배치는 [공용 폴더 기준](<../Shared/폴더·에셋 배치 기준 및 규칙.md>)을 따른다. Content의 기능 폴더와 기존 Source/MW의 C++ 경로를 구분하며, 이번 Content 구조를 이유로 C++ 소스를 재배치하지 않는다.
- 변경 성격에 맞는 컴파일·실행·멀티플레이·패키지 검증을 수행한다. 문서/단순 설정 수정에 관련 없는 게임 테스트를 요구하지 않는다.
- TA/Editor 구현은 개발 공통과 Art/TA 규칙을 함께 적용하며, Runtime으로 Editor 의존성이 새지 않는지 확인한다.
