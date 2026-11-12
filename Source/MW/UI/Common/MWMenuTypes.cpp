// 기존 테마 ID를 사용자 표시명으로 바꾸는 메뉴용 조회다. 실제 요청 ID는 유지한다.

#include "UI/Common/MWMenuTypes.h"

#include "Misc/ConfigCacheIni.h"

// 내부 테마 ID를 설정의 사용자 표시명으로 바꾸되 실제 요청 ID는 변경하지 않는다.
FText MWMenu::GetMapThemeDisplayName(FName ThemeId)
{
    FText DisplayName;
    if (GConfig && GConfig->GetText(TEXT("MW.UI.MapThemeDisplayNames"), *ThemeId.ToString(), DisplayName, GGameIni) &&
        !DisplayName.IsEmpty())
    {
        return DisplayName;
    }

    // 새 테마의 번역이 아직 없으면 내부 경로 전체 대신 마지막 이름을 읽기 쉬운 형태로 표시한다.
    FString LeafName = ThemeId.ToString();
    int32 Separator;
    if (LeafName.FindLastChar(TEXT('.'), Separator))
    {
        LeafName.RightChopInline(Separator + 1);
    }
    return FText::FromString(FName::NameToDisplayString(LeafName, false));
}
