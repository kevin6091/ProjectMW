// 기존 공통 글꼴·게시판 자원과 벡터 표지판을 화면 크기에 맞춰 그리는 표시 구현이다.

#include "UI/Gameplay/MWGameplaySign.h"

#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Styling/CoreStyle.h"
#include "UI/Gameplay/MWGameplayHUDSubsystem.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"

// 기준 1920×1080 구도를 실제 Viewport의 안전 영역 안에 비율대로 맞춘다.
TSharedRef<SWidget> MWGameplayUI::FitDesignCanvas(TSharedRef<SWidget> Content, EHorizontalAlignment Horizontal,
                                                  EVerticalAlignment Vertical)
{
    return SNew(SScaleBox)
        .Stretch(EStretch::ScaleToFit)
        .HAlign(Horizontal)
        .VAlign(Vertical)[SNew(SBox).WidthOverride(DesignWidth).HeightOverride(DesignHeight)[Content]];
}

FMWGameplayUIArt::FMWGameplayUIArt()
{
    const auto* Settings = GetDefault<UMWGameplayHUDSubsystem>();
    DisplayFont.Reset(Settings->DisplayFont.LoadSynchronous());
    Logo.Reset(Settings->BoardLogo.LoadSynchronous());
    Pole.Reset(Settings->BoardPole.LoadSynchronous());
    Bolt.Reset(Settings->BoardBolt.LoadSynchronous());
    LogoImage.SetResourceObject(Logo.Get());
    PoleImage.SetResourceObject(Pole.Get());
    BoltImage.SetResourceObject(Bolt.Get());
    LogoImage.DrawAs = ESlateBrushDrawType::Image;
    PoleImage.DrawAs = ESlateBrushDrawType::Image;

    // 브러시 기본 크기 대신 실제 텍스처 비율을 사용해 로고가 가로로 늘어나지 않게 한다.
    if (const UTexture2D* Texture = Cast<UTexture2D>(Logo.Get()))
    {
        LogoImage.SetImageSize(FVector2f(Texture->GetSizeX(), Texture->GetSizeY()));
    }
    BoltImage.DrawAs = ESlateBrushDrawType::Image;
}

// 프로젝트의 공통 글꼴을 사용하고 자원이 없을 때만 기존 대체 글꼴로 표시한다.
FSlateFontInfo FMWGameplayUIArt::Font(float Size) const
{
    return DisplayFont ? FSlateFontInfo(DisplayFont.Get(), Size) : FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), Size);
}

// 공통 로고 자원이 준비됐으면 그 브러시를 반환하고 없으면 표시할 로고가 없음을 알린다.
const FSlateBrush* FMWGameplayUIArt::LogoBrush() const
{
    return Logo ? &LogoImage : FCoreStyle::Get().GetBrush(TEXT("NoBrush"));
}

// 로고에 포함된 기둥과 별도의 기둥을 겹쳐 그리지 않도록 분리 자원만 사용한다.
const FSlateBrush* FMWGameplayUIArt::PoleBrush() const
{
    return Pole ? &PoleImage : FCoreStyle::Get().GetBrush(TEXT("NoBrush"));
}

// 공통 볼트 자원을 우선 사용하고 누락된 경우에만 기존 벡터 대체 표시를 반환한다.
const FSlateBrush* FMWGameplayUIArt::BoltBrush() const
{
    return Bolt ? &BoltImage : &FallbackBolt;
}

// 공통 게시판의 크기·색·테두리와 기존 콘텐츠를 같은 Slate 패널에 구성한다.
void SMWGameplayPanel::Construct(const FArguments& Args)
{
    Plate = FSlateRoundedBoxBrush(Args._Fill, 24.f, Args._Border, Args._BorderWidth);
    ChildSlot[SNew(SBox)
                  .WidthOverride(Args._Size.X)
                  .HeightOverride(
                      Args._Size
                          .Y)[SNew(SOverlay) +
                              SOverlay::Slot()[SNew(SBorder).BorderImage(&Plate).Padding(0.f)[Args._Content.Widget]] +
                              SOverlay::Slot()
                                  .HAlign(HAlign_Left)
                                  .VAlign(VAlign_Top)
                                  .Padding(18.f)[SNew(SBox).WidthOverride(22.f).HeightOverride(
                                      22.f)[SNew(SImage).Image(Art.BoltBrush())]] +
                              SOverlay::Slot()
                                  .HAlign(HAlign_Right)
                                  .VAlign(VAlign_Top)
                                  .Padding(18.f)[SNew(SBox).WidthOverride(22.f).HeightOverride(
                                      22.f)[SNew(SImage).Image(Art.BoltBrush())]] +
                              SOverlay::Slot()
                                  .HAlign(HAlign_Left)
                                  .VAlign(VAlign_Bottom)
                                  .Padding(18.f)[SNew(SBox).WidthOverride(22.f).HeightOverride(
                                      22.f)[SNew(SImage).Image(Art.BoltBrush())]] +
                              SOverlay::Slot()
                                  .HAlign(HAlign_Right)
                                  .VAlign(VAlign_Bottom)
                                  .Padding(18.f)[SNew(SBox).WidthOverride(22.f).HeightOverride(
                                      22.f)[SNew(SImage).Image(Art.BoltBrush())]]]];
}

// 표지판 문구와 로딩 애니메이션 상태를 준비한다. 게임 진행 단계를 자체 승인하지 않는다.
void SMWGameplaySign::Construct(const FArguments& Args)
{
    Label = Args._Label;
    bAnimateLoading = Args._AnimateLoading;
    FaceColor = Args._FaceColor;
    EdgeColor = Args._EdgeColor;
    FaceInset = .914f - FMath::Clamp(Args._EdgeThickness, 0.f, .4f);
    SignFont = Art.Font(Args._FontSize > 0.f ? Args._FontSize : (bAnimateLoading ? 74.f : 54.f));
    const float Size = bAnimateLoading ? 568.f : 385.f;
    ChildSlot[SNew(SBox).WidthOverride(Size).HeightOverride(Size)];

    if (bAnimateLoading)
        RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SMWGameplaySign::AdvanceLoading));
}

// 표시 시간으로 원본 표지판의 회전·크기 연출만 갱신한다.
EActiveTimerReturnType SMWGameplaySign::AdvanceLoading(double CurrentTime, float)
{
    if (!GetVisibility().IsVisible())
        return EActiveTimerReturnType::Continue;
    if (!LoadingStartedAt.IsSet())
        LoadingStartedAt = CurrentTime;

    // 원본 표지판은 잠시 유지한 뒤 빈 뒷면으로 돌아갔다가 다시 안정되는 순서다.
    const float Cycle = FMath::Fmod(static_cast<float>(CurrentTime - LoadingStartedAt.GetValue()), 1.85f);
    const float Turn = FMath::Clamp((Cycle - .16f) / .56f, 0.f, 1.f);
    LoadingAngle = FMath::SmoothStep(0.f, 1.f, Turn) * 2.f * PI;
    LoadingScale = .88f + .18f * FMath::Cos(Cycle * 2.f * PI / 1.85f);
    Invalidate(EInvalidateWidgetReason::Paint);
    return EActiveTimerReturnType::Continue;
}

// 벡터 표지판의 앞·뒤·테두리와 공통 글꼴을 현재 회전 상태로 그린다.
int32 SMWGameplaySign::OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
                               FSlateWindowElementList& DrawElements, int32 Layer, const FWidgetStyle&, bool) const
{
    const FVector2f Center(Geometry.GetLocalSize() * .5);
    const float BaseRadius = FMath::Min(Center.X, Center.Y);
    const float Radius = BaseRadius * LoadingScale;
    constexpr float TextEdgePadding = .025f;
    const float Face = FMath::Cos(LoadingAngle);
    const float Depth = FMath::Sin(LoadingAngle);
    const auto Project = [Center, Radius, Face, Depth](FVector2f Point, float Thickness)
    {
        const float Perspective = 1.f / (1.f + .20f * (Point.X * Depth + Thickness * Face));
        return Center + FVector2f(Point.X * Face - Thickness * Depth, Point.Y) * (Radius * Perspective);
    };

    TArray<FSlateVertex> Vertices;
    TArray<SlateIndex> Indices;
    const auto Polygon = [&](TArrayView<const FVector2f> Points, FColor Color, float Scale = 1.f, float Z = 0.f)
    {
        const SlateIndex First = static_cast<SlateIndex>(Vertices.Num());
        for (const FVector2f Point : Points)
        {
            Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
                Geometry.GetAccumulatedRenderTransform(), Project(Point * Scale, Z), FVector2f(.5f, .5f), Color));
        }
        for (int32 Index = 1; Index + 1 < Points.Num(); ++Index)
            Indices.Append({First, static_cast<SlateIndex>(First + Index), static_cast<SlateIndex>(First + Index + 1)});
    };

    // 끝부분의 짧은 경사면과 별도 투영한 뒤판으로
    // 뒤집는 중에도 두께가 보이게 한다. 각 면은 벡터로 그린다.
    const FVector2f Outline[] = {{-.027f, -.986f}, {.027f, -.986f}, {.986f, -.027f}, {.986f, .027f},
                                 {.027f, .986f},   {-.027f, .986f}, {-.986f, .027f}, {-.986f, -.027f}};
    Polygon(Outline, FColor(127, 105, 68), 1.f, .034f);
    Polygon(Outline, FColor(255, 187, 49));
    Polygon(Outline, EdgeColor, .914f);
    Polygon(Outline, FaceColor, FaceInset);

    if (bAnimateLoading)
    {
        const FVector2f Bright[] = {{-.86f, 0.f}, {-.33f, -.52f}, {.08f, .14f}};
        const FVector2f Shade[] = {{-.04f, -.85f}, {.37f, -.50f}, {.03f, -.07f}};
        const FVector2f Lower[] = {{.10f, .72f}, {.43f, -.04f}, {.79f, .08f}};
        Polygon(Bright, FColor(255, 188, 39));
        Polygon(Shade, FColor(229, 136, 14));
        Polygon(Lower, FColor(255, 180, 29));
    }

    if (Face > 0.f)
    {
        for (const FVector2f Axis :
             {FVector2f(0.f, -1.f), FVector2f(1.f, 0.f), FVector2f(0.f, 1.f), FVector2f(-1.f, 0.f)})
        {
            FVector2f Disc[12];
            for (int32 Index = 0; Index < UE_ARRAY_COUNT(Disc); ++Index)
            {
                const float Angle = Index * (2.f * PI / UE_ARRAY_COUNT(Disc));
                Disc[Index] = Axis * .79f + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * .025f;
            }
            Polygon(Disc, FColor(80, 79, 72));
        }
    }

    const auto Renderer = FSlateApplication::Get().GetRenderer();
    const auto Handle = Renderer->GetResourceHandle(*FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")));
    FSlateDrawElement::MakeCustomVerts(DrawElements, Layer, Handle, Vertices, Indices, nullptr, 0, 0);

    if (Face > .12f)
    {
        // 줄마다 폭을 따로 재서 길이가 다른 문장도 각각 판 중앙에 놓는다.
        TArray<FString> Lines;
        Label.ToString().ParseIntoArrayLines(Lines, false);
        const auto FontMeasure = Renderer->GetFontMeasureService();
        const double LineHeight = FontMeasure->GetMaxCharacterHeight(SignFont);
        double MaximumWidth = 0.0;
        for (const FString& Line : Lines)
        {
            MaximumWidth = FMath::Max(MaximumWidth, FontMeasure->Measure(Line, SignFont).X);
        }

        // 마름모 안쪽 면에서 일정한 여백을 남긴다. 축 끝의 볼트를 기준으로
        // 전체 문구를 과하게 줄이지 않고, 글자 블록의 모서리가 면 안에 들도록 맞춘다.
        const double TextHeight = LineHeight * Lines.Num();
        const double SafeRadius = BaseRadius * (FaceInset - TextEdgePadding);
        const double Fit = FMath::Min(1.0, SafeRadius / FMath::Max(1.0, (MaximumWidth + TextHeight) * .5));
        const FVector2D TextScale(Face * LoadingScale * Fit, LoadingScale * Fit);
        for (int32 Index = 0; Index < Lines.Num(); ++Index)
        {
            const FVector2D TextSize = FontMeasure->Measure(Lines[Index], SignFont);
            const FVector2D TextOrigin(Center.X - TextSize.X * TextScale.X * .5,
                                      Center.Y + (LineHeight * Index - TextHeight * .5) * TextScale.Y);
            const auto TextGeometry = Geometry.MakeChild(TextSize, FSlateLayoutTransform(TextOrigin),
                                                         FSlateRenderTransform(FScale2D(TextScale)), FVector2D::ZeroVector);
            FSlateDrawElement::MakeText(DrawElements, Layer + 1, TextGeometry.ToPaintGeometry(), Lines[Index], SignFont,
                                        ESlateDrawEffect::None,
                                        bAnimateLoading ? FLinearColor(.065f, .045f, .027f) : FLinearColor::Black);
        }
    }
    return Layer + 1;
}
