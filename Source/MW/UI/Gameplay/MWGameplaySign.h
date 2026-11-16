#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class UFont;
class UTexture2D;

namespace MWGameplayUI
{
    inline constexpr float DesignWidth = 1920.f;

    inline constexpr float DesignHeight = 1080.f;

    // 원본 1920×1080 화면의 구도를 기준으로 배치하고
    // PIE 창 비율이 달라도 안전한 16:9 영역 안에 맞춘다.
    MW_API TSharedRef<SWidget> FitDesignCanvas(TSharedRef<SWidget> Content,
                                               EHorizontalAlignment Horizontal = HAlign_Center,
                                               EVerticalAlignment Vertical = VAlign_Center);
}

// 공통 폰트·로고·볼트 자원의 수명을 유지해 Slate 표시 중 참조가 사라지지 않게 한다.
struct MW_API FMWGameplayUIArt
{
    FMWGameplayUIArt();

    // 프로젝트의 공통 글꼴을 사용하고 자원이 없을 때만 기존 대체 글꼴로 표시한다.
    FSlateFontInfo Font(float Size) const;

    // 공통 로고 자원이 준비됐으면 그 브러시를 반환하고 없으면 표시할 로고가 없음을 알린다.
    const FSlateBrush* LogoBrush() const;

    // 공통 볼트 자원을 우선 사용하고 누락된 경우에만 기존 벡터 대체 표시를 반환한다.
    const FSlateBrush* BoltBrush() const;

private:
    TStrongObjectPtr<UObject> DisplayFont;

    TStrongObjectPtr<UObject> Logo;

    TStrongObjectPtr<UObject> Bolt;

    FSlateBrush LogoImage;

    FSlateBrush BoltImage;
    FSlateRoundedBoxBrush FallbackBolt{FLinearColor(.34f, .39f, .44f), 12.f};
};

// 공통 게시판의 테두리·볼트와 내부 콘텐츠를 같은 디자인 기준으로 배치한다.
class MW_API SMWGameplayPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SMWGameplayPanel)
    {
    }
    SLATE_ARGUMENT(FVector2D, Size)
    SLATE_ARGUMENT(FLinearColor, Fill)
    SLATE_ARGUMENT(FLinearColor, Border)
    SLATE_ARGUMENT(float, BorderWidth)
    SLATE_DEFAULT_SLOT(FArguments, Content)
    SLATE_END_ARGS()

    // 공통 게시판의 크기·색·테두리와 기존 콘텐츠를 같은 Slate 패널에 구성한다.
    void Construct(const FArguments& Args);

private:
    FMWGameplayUIArt Art;
    FSlateRoundedBoxBrush Plate{FLinearColor::Transparent};
};

// 표지판 모양과 로딩 회전만 표시하며 게임 단계의 최종 판정은 하지 않는다.
class MW_API SMWGameplaySign : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SMWGameplaySign)
    {
    }
    SLATE_ARGUMENT(FText, Label)
    SLATE_ARGUMENT(bool, AnimateLoading)
    SLATE_END_ARGS()

    // 표지판 문구와 로딩 애니메이션 상태를 준비한다. 게임 진행 단계를 자체 승인하지 않는다.
    void Construct(const FArguments& Args);

    // 벡터 표지판의 앞·뒤·테두리와 공통 글꼴을 현재 회전 상태로 그린다.
    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
                          FSlateWindowElementList& DrawElements, int32 Layer, const FWidgetStyle& Style,
                          bool bParentEnabled) const override;

private:
    // 표시 시간으로 원본 표지판의 회전·크기 연출만 갱신한다.
    EActiveTimerReturnType AdvanceLoading(double CurrentTime, float DeltaTime);

    FMWGameplayUIArt Art;

    FText Label;

    FSlateFontInfo SignFont;

    TOptional<double> LoadingStartedAt;

    float LoadingAngle = 0.f;

    float LoadingScale = 1.f;

    bool bAnimateLoading = false;
};
