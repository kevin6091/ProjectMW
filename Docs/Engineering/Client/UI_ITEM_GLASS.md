# 아이템 슬롯 유리와 아이콘 편집

`Source/MW/UI/Gameplay/MWItemGlassWidget.h`와 `.cpp`가 재사용 UMG 부품을 제공한다. `WBP_MatchPanels`의 `ItemSlot → ItemLayers → Glass`에서 유리 색, 재질과 그리기 크기 속성을 편집한다. 유리와 `ItemIconHost`는 같은 Overlay의 가로·세로를 채운다. 아이콘 패딩은 별도 슬롯에서 편집한다.

기본 벡터 원본은 `Content/UI/Common/Slate/ItemSlotGlass.svg`다. 패키징의 `DirectoriesToAlwaysStageAsUFS`가 해당 폴더를 포함한다. 하단 슬롯의 위치와 기준 크기는 WBP, 최소 실제 픽셀·최대 높이 비율·아이콘 크기는 `DefaultGame.ini`의 `ItemSlotLayout`에서 바꾼다. 보유 변경, 룰렛의 최종 결과와 사용 페이드는 `MWGameplayHUDSubsystem`이 소유한다. 실제 플레이어의 `MWItemComponent`를 구독하고 Pawn 교체·월드 종료 때 이전 구독을 해제한다.
