---
name: mw-task-routing
description: Classify ProjectMW repository tasks as Art, Client, Server, TA, or shared work and read the required Docs guidance before implementation, review, or configuration changes. Applies only to work in ProjectMW.
---

# ProjectMW 작업 분야별 필독

## 적용 순서

1. 요청의 산출물과 변경 영향을 기준으로 분야를 분류한다. 파일 확장자나 키워드 하나만으로 분류하지 않는다.
2. [공용 작업 규칙](../../../Docs/Shared/WORK_RULES.md)을 포함한 `Docs/Shared` **직속 `.md` 전체**를 먼저 읽는다. 여기에는 기획최종안도 포함된다. 이 단계에서 하위 폴더를 무조건 전부 읽지는 않는다.
3. 아래 표에 해당하는 분야 문서와 Shared 문서의 `.md` 본문을 읽는다. 복합 작업은 필독 집합의 합집합을 적용한다.
4. 분야와 적용 문서를 짧게 알린 뒤 작업한다. 범위가 바뀌면 새 분야 필독을 먼저 완료한다.

같은 대화에서 본문 전체를 이미 읽었고 파일 내용이 그대로임을 확인했다면 재사용할 수 있다. 문서 변경, 내용 유실 또는 새 대화에서는 다시 읽는다. 일부 검색 결과·요약·파일 이름만 본 것은 본문 확인으로 간주하지 않는다.

## 필독 경로

아래 경로는 저장소 루트 기준이다. **폴더 전체**는 해당 폴더와 하위 폴더의 `.md` 전체를, **직속**은 하위 폴더를 제외한 `.md`를 뜻한다.

| 작업 분야 | 판단 기준 | 공용 필독에 추가할 문서 |
|---|---|---|
| Art | 메시·머티리얼·텍스처·리깅·VFX·임포트·콜리전·소켓·에셋 메타데이터 제작/수정 | `Docs/Art` 전체 + `Docs/Shared/TA` 전체 |
| Client | 로컬 입력·카메라·Widget/HUD·Ghost·주행 표시·소유 클라이언트 요청/예측 | `Docs/Engineering` 직속 + `Docs/Engineering/Client` 전체 + `Docs/Shared/Client,Server` 전체 |
| Server | 서버 권한·RPC 검증·복제 계약·배치 승인·점수·체크포인트 판정·매치/접속 규칙 | `Docs/Engineering` 직속 + `Docs/Engineering/Server` 전체 + `Docs/Shared/Client,Server` 전체 |
| TA 코드/도구 | Editor Validator·임포트 자동화 등 C++/Python/Blueprint 도구 구현 | Art 필독 + `Docs/Engineering` 직속 + `Docs/Shared/Client,Server` 전체. Runtime 동작도 바꾸면 Client/Server 해당 필독 추가 |
| 공용 운영 | Git·폴더 정리·공용 작업 지침 등 특정 게임 기능을 바꾸지 않는 작업 | 공용 필독. 분야별 문서 자체를 수정하면 그 문서가 속한 분야 필독도 추가 |

`Clinet`라는 요청 표기는 `Client`로 해석한다. 실제 폴더명은 `Client`다. `Docs/Shared/Client,Server`는 쉼표를 포함하는 하나의 폴더이며 이름을 임의로 바꾸지 않는다.

## 경계 판단

- Listen Server의 서버 코드도 Server 개발이다. 별도 Dedicated Server 폴더의 존재 여부로 판단하지 않는다.
- 아이템의 사용 요청/HUD와 서버 사용 판정을 함께 바꾸면 Client + Server다. 새 아이콘/이펙트 에셋도 만들면 Art를 추가한다.
- 트랙 소켓/Footprint를 바꾸면 Art + TA 공유 문서를 읽는다. 런타임 스냅/설치 검증 계약도 바뀌면 Server를 추가한다.
- 기존 복제 값을 보여주는 HUD만 수정하면 Client다. 점수 계산까지 바꾸면 Server를 추가한다.
- 분야를 판단할 수 없으면 공용 문서와 관련 폴더를 먼저 조사한다. 그래도 결정할 수 없는 요구만 확인하고, 관계없는 분야를 임의로 생략하지 않는다.

## 확인과 보고

필수 폴더/문서가 없거나 본문을 읽지 못했으면 해당 부분의 구현을 보류하고 누락 경로를 알린다. 단순 폴더 이동으로 링크가 깨졌다면 실제 위치를 확인해 링크와 이 표를 함께 수정한다. 완료 보고에는 적용 분야, 변경 결과, 실제 검증과 남은 제한을 포함한다.
