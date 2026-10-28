#include "MWFrontendAuthoringLibrary.h"
#include "MWFrontendPreview.h"
#include "MWFrontendAuthoringValidation.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"

#include "Frontend/MWFrontendWidget.h"
#include "Frontend/MWRoadButtonWidget.h"
#include "Frontend/MWRoomRowWidget.h"
#include "System/MWMultiplayerSubsystem.h"
#include "Engine/GameInstance.h"
#include "Animation/WidgetAnimation.h"
#include "Animation/WidgetAnimationBinding.h"
#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ButtonSlot.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SWindow.h"
#include "Widgets/SViewport.h"
#include "Framework/Application/SlateApplication.h"
#include "Editor.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ListView.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Modules/ModuleManager.h"
#include "MovieScene.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"
#include "EdGraphUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UObjectIterator.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, MWFrontendAuthoring)

namespace MWFrontendAuthoring
{
	// Source: original MainMenu colours and T_listMenuOptionSignStandard sprite m_Border=106.5/512.
	const FLinearColor Yellow = FLinearColor::FromSRGBColor(FColor(255, 178, 33));
	const FLinearColor Blue = FLinearColor::FromSRGBColor(FColor(0, 152, 248));
	const FLinearColor Ink = FLinearColor::FromSRGBColor(FColor(55, 52, 53));
	UFont* Font = nullptr;
	UTexture2D* Sign = nullptr;
	UTexture2D* PanelTexture = nullptr;
	UTexture2D* PlayerSign = nullptr;
	UTexture2D* LobbyOutline = nullptr;
	UTexture2D* ModeSign = nullptr;
	UTexture2D* ModeOutline = nullptr;
	UTexture2D* Bolt = nullptr;
	UTexture2D* PadIcon = nullptr;
	UClass* ButtonClass = nullptr;
	UClass* RowClass = nullptr;

	bool Save(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(Package, Asset, *FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension()), Args);
	}

	UWidgetBlueprint* NewBlueprint(const FString& Folder, const FString& Name, UClass* Parent)
	{
		// Refuse replacement: an existing WBP may now be owned or edited by another teammate.
		if (FPackageName::DoesPackageExist(Folder / Name)) { return nullptr; }
		UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
		Factory->ParentClass = Parent;
		UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().CreateAsset(Name, Folder, UWidgetBlueprint::StaticClass(), Factory));
		return Blueprint;
	}

	FSlateBrush Brush(UTexture2D* Texture, const FLinearColor& Tint, float Margin)
	{
		FSlateBrush Result;
		Result.SetResourceObject(Texture);
		// Source pixels stay intact; the imported GPU size determines the Box corners.
		Result.ImageSize = FVector2D(64.f);
		Result.DrawAs = ESlateBrushDrawType::Box;
		Result.Margin = FMargin(Margin);
		Result.TintColor = FSlateColor(Tint);
		return Result;
	}

	FSlateFontInfo TextFont(int32 Size)
	{
		return FSlateFontInfo(Font, Size);
	}

	UTextBlock* Text(UWidgetTree* Tree, FName Name, const FString& Value, int32 Size = 28)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Label->SetText(FText::FromString(Value));
		Label->SetFont(TextFont(Size));
		Label->SetColorAndOpacity(FSlateColor(Ink));
		Label->SetAutoWrapText(true);
		return Label;
	}

	void Add(UVerticalBox* Box, UWidget* Widget, float Gap = 12.f)
	{
		UVerticalBoxSlot* Slot = Box->AddChildToVerticalBox(Widget);
		Slot->SetPadding(FMargin(0.f, 0.f, 0.f, Gap));
		Slot->SetHorizontalAlignment(HAlign_Fill);
	}

	UMWRoadButtonWidget* Button(UWidgetTree* Tree, UVerticalBox* Box, FName Name, const FString& Caption)
	{
		UMWRoadButtonWidget* Widget = Tree->ConstructWidget<UMWRoadButtonWidget>(ButtonClass, Name);
		Widget->LabelText = FText::FromString(Caption);
		USizeBox* Height = Tree->ConstructWidget<USizeBox>();
		Height->SetHeightOverride(72.f);
		Height->AddChild(Widget);
		Add(Box, Height, 14.f);
		return Widget;
	}

	UVerticalBox* Panel(UWidgetTree* Tree, FName Name)
	{
		return Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), Name);
	}

	FSlateBrush SolidBrush(const FLinearColor& Color)
	{
		FSlateBrush Result;
		Result.DrawAs = ESlateBrushDrawType::Image;
		Result.TintColor = FSlateColor(Color);
		return Result;
	}

	void StyleCombo(UComboBoxString* Combo)
	{
		// Popup rows need their own opaque fill and padding, not a squeezed sign border.
		FComboBoxStyle ComboStyle = Combo->GetWidgetStyle();
		ComboStyle.SetMenuRowPadding(FMargin(24.f, 12.f));
		FComboButtonStyle& ComboButton = ComboStyle.ComboButtonStyle;
		FButtonStyle& ButtonStyle = ComboButton.ButtonStyle;
		ButtonStyle.SetNormal(Brush(Sign, Yellow, 106.5f / 512.f));
		ButtonStyle.SetHovered(Brush(Sign, Blue, 106.5f / 512.f));
		ButtonStyle.SetPressed(Brush(Sign, Blue, 106.5f / 512.f));
		ButtonStyle.SetDisabled(Brush(Sign, FLinearColor::FromSRGBColor(FColor(110, 115, 115)), 106.5f / 512.f));
		ButtonStyle.SetNormalForeground(FSlateColor(Ink)).SetHoveredForeground(FSlateColor(Ink)).SetPressedForeground(FSlateColor(Ink));
		ButtonStyle.SetNormalPadding(FMargin(0.f)).SetPressedPadding(FMargin(0.f));
		ComboButton.SetMenuBorderBrush(SolidBrush(Ink)).SetMenuBorderPadding(FMargin(2.f));
		ComboButton.SetDownArrowPadding(FMargin(12.f, 0.f, 6.f, 0.f));
		ComboButton.DownArrowImage.ImageSize = FVector2D(16.f, 10.f);
		ComboButton.DownArrowImage.TintColor = FSlateColor(Ink);
		Combo->SetWidgetStyle(ComboStyle);
		Combo->SetContentPadding(FMargin(40.f, 10.f, 32.f, 10.f));
		Combo->SetMaxListHeight(240.f);

		FTableRowStyle Style = Combo->GetItemStyle();
		const FSlateBrush Normal = SolidBrush(Yellow);
		const FSlateBrush Selected = SolidBrush(Blue);
		Style.SetEvenRowBackgroundBrush(Normal).SetOddRowBackgroundBrush(Normal);
		Style.SetEvenRowBackgroundHoveredBrush(Selected).SetOddRowBackgroundHoveredBrush(Selected);
		Style.SetActiveBrush(Selected).SetActiveHoveredBrush(Selected);
		Style.SetInactiveBrush(Selected).SetInactiveHoveredBrush(Selected);
		Style.SetTextColor(FSlateColor(Ink)).SetSelectedTextColor(FSlateColor(Ink));
		Combo->SetItemStyle(Style);
	}

	bool Compile(UWidgetBlueprint* Blueprint)
	{
		// A duplicated tree already has GUIDs. Register newly added preview widgets without
		// replacing stable GUIDs belonging to the source tree or existing event bindings.
		Blueprint->ForEachSourceWidget([Blueprint](UWidget* Widget)
		{
			if (!Blueprint->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
			{
				Blueprint->WidgetVariableNameToGuidMap.Add(Widget->GetFName(), FGuid::NewDeterministicGuid(Widget->GetPathName()));
			}
		});
		for (UWidgetAnimation* Animation : Blueprint->Animations)
		{
			if (Animation && !Blueprint->WidgetVariableNameToGuidMap.Contains(Animation->GetFName()))
			{
				Blueprint->WidgetVariableNameToGuidMap.Add(Animation->GetFName(), FGuid::NewDeterministicGuid(Animation->GetPathName()));
			}
		}
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FCompilerResultsLog Results;
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Results);
		return Results.NumErrors == 0 && Blueprint->Status != BS_Error;
	}

	bool BuildButton()
	{
		UWidgetBlueprint* Blueprint = NewBlueprint(TEXT("/Game/UI/Common"), TEXT("WBP_RoadButton"), UMWRoadButtonWidget::StaticClass());
		if (!Blueprint) { return false; }
		UWidgetTree* Tree = Blueprint->WidgetTree;
		UButton* Action = Tree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ActionButton"));
		Tree->RootWidget = Action;
		FButtonStyle Style;
		Style.SetNormal(Brush(Sign, Yellow, 106.5f / 512.f));
		Style.SetHovered(Brush(Sign, Blue, 106.5f / 512.f));
		Style.SetPressed(Brush(Sign, Blue * .85f, 106.5f / 512.f));
		Style.SetDisabled(Brush(Sign, FLinearColor::FromSRGBColor(FColor(156, 169, 171)), 106.5f / 512.f));
		Style.SetNormalPadding(FMargin(42.f, 12.f));
		Style.SetPressedPadding(FMargin(42.f, 15.f, 42.f, 9.f));
		Action->SetStyle(Style);
		UTextBlock* Label = Text(Tree, TEXT("Label"), TEXT("경기 생성"), 48);
		Label->SetAutoWrapText(false);
		Label->SetJustification(ETextJustify::Left);
		UHorizontalBox* Contents = Tree->ConstructWidget<UHorizontalBox>();
		UHorizontalBoxSlot* LabelSlot=Contents->AddChildToHorizontalBox(Label); LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); LabelSlot->SetVerticalAlignment(VAlign_Center);
		UImage* Arrow=Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Arrow")); Arrow->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr,TEXT("/Game/UI/Common/T_MW_Arrow.T_MW_Arrow")),true); Arrow->SetColorAndOpacity(Ink); Arrow->SetRenderTransformAngle(90.f);
		USizeBox* ArrowSize=Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),TEXT("ArrowFrame")); ArrowSize->SetWidthOverride(54); ArrowSize->SetHeightOverride(54); ArrowSize->AddChild(Arrow); Contents->AddChildToHorizontalBox(ArrowSize)->SetVerticalAlignment(VAlign_Center);
		UButtonSlot* ContentsSlot=CastChecked<UButtonSlot>(Action->AddChild(Contents)); ContentsSlot->SetHorizontalAlignment(HAlign_Fill); ContentsSlot->SetVerticalAlignment(VAlign_Center);
		if (!Compile(Blueprint)) { return false; }
		ButtonClass = Blueprint->GeneratedClass;
		return Save(Blueprint);
	}

	bool BuildRow()
	{
		UWidgetBlueprint* Blueprint = NewBlueprint(TEXT("/Game/UI/Frontend"), TEXT("WBP_RoomRow"), UMWRoomRowWidget::StaticClass());
		if (!Blueprint) { return false; }
		UWidgetTree* Tree = Blueprint->WidgetTree;
		UBorder* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SelectionBorder"));
		Tree->RootWidget = Border;
		Border->SetBrush(Brush(PlayerSign, FLinearColor::White, 75.f / 512.f));
		Border->SetBrushColor(Yellow);
		Border->SetPadding(FMargin(34.f, 20.f));
		Border->AddChild(Text(Tree, TEXT("RoomLabel"), TEXT("방 이름    |    인원    |    테마"), 30));
		if (!Compile(Blueprint)) { return false; }
		RowClass = Blueprint->GeneratedClass;
		return Save(Blueprint);
	}

	void AddTransition(UWidgetBlueprint* Blueprint)
	{
		UWidgetAnimation* Animation = NewObject<UWidgetAnimation>(Blueprint, TEXT("PageTransition"), RF_Transactional);
		Animation->MovieScene = NewObject<UMovieScene>(Animation, NAME_None, RF_Transactional);
		UMovieScene* Scene = Animation->MovieScene;
		Scene->SetTickResolutionDirectly(FFrameRate(24000, 1));
		Scene->SetDisplayRate(FFrameRate(60, 1));
		Scene->SetPlaybackRange(0, 4800);
		const FGuid Binding = Scene->AddPossessable(TEXT("Screen"), UCanvasPanel::StaticClass());
		FWidgetAnimationBinding WidgetBinding;
		WidgetBinding.WidgetName = TEXT("Screen");
		WidgetBinding.AnimationGuid = Binding;
		Animation->AnimationBindings.Add(WidgetBinding);
		UMovieSceneFloatTrack* Track = Scene->AddTrack<UMovieSceneFloatTrack>(Binding);
		Track->SetPropertyNameAndPath(TEXT("RenderOpacity"), TEXT("RenderOpacity"));
		UMovieSceneFloatSection* Section = CastChecked<UMovieSceneFloatSection>(Track->CreateNewSection());
		Section->SetRange(TRange<FFrameNumber>(0, 4801));
		Section->GetChannel().AddCubicKey(0, 0.f);
		Section->GetChannel().AddCubicKey(4800, 1.f);
		Track->AddSection(*Section);
		Blueprint->Animations.Add(Animation);
	}

	void Place(UCanvasPanel* Canvas, UWidget* Widget, float X, float Y, float W, float H, float Angle = 0.f)
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
		Slot->SetPosition(FVector2D(X, Y)); Slot->SetSize(FVector2D(W, H));
		Widget->SetRenderTransformAngle(Angle);
	}

	UBorder* SignPanel(UWidgetTree* Tree, const FLinearColor& Color)
	{
		UBorder* PanelWidget = Tree->ConstructWidget<UBorder>();
		PanelWidget->SetBrush(Brush(Sign, FLinearColor::White, 106.5f / 512.f));
		PanelWidget->SetBrushColor(Color); PanelWidget->SetPadding(FMargin(32.f, 18.f));
		return PanelWidget;
	}


	UImage* Image(UWidgetTree* Tree, FName Name, UTexture2D* Texture, const FLinearColor& Color)
	{
		UImage* Widget=Tree->ConstructWidget<UImage>(UImage::StaticClass(),Name); Widget->SetBrushFromTexture(Texture); Widget->SetColorAndOpacity(Color); Widget->SetVisibility(ESlateVisibility::HitTestInvisible); return Widget;
	}

	UCanvasPanel* RoundedPanel(UWidgetTree* Tree, FName Name, UTexture2D* Texture, float Margin, const FLinearColor& Color, bool bBolts, bool bHideBolts = false)
	{
		UCanvasPanel* Canvas=Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),Name);
		for (int32 Layer=0;Layer<2;++Layer)
		{
			UImage* Surface=Tree->ConstructWidget<UImage>();
			Surface->SetBrush(Brush(Layer==0?Texture:(Texture==PlayerSign?LobbyOutline:ModeOutline),Layer==0?Color:FLinearColor::FromSRGBColor(FColor(139,143,137)),Layer==0?Margin:110.f/476.f));
			Surface->SetVisibility(ESlateVisibility::HitTestInvisible);
			UCanvasPanelSlot* SurfaceSlot=Canvas->AddChildToCanvas(Surface); SurfaceSlot->SetAnchors(FAnchors(0,0,1,1)); SurfaceSlot->SetOffsets(FMargin(0));
		}
		if (bBolts)
		{
			for (const FVector2D Anchor : {FVector2D(0,0),FVector2D(1,0),FVector2D(0,1),FVector2D(1,1)})
			{
				UImage* Pin=Image(Tree,NAME_None,Bolt,FLinearColor::White); UCanvasPanelSlot* PinSlot=Canvas->AddChildToCanvas(Pin);
				Pin->SetVisibility(bHideBolts?ESlateVisibility::Collapsed:ESlateVisibility::HitTestInvisible);
				PinSlot->SetAnchors(FAnchors(Anchor.X,Anchor.Y)); PinSlot->SetAlignment(FVector2D(.5,.5)); PinSlot->SetPosition(FVector2D(Anchor.X==0?34:-34,Anchor.Y==0?34:-34)); PinSlot->SetSize(FVector2D(13,13));
			}
		}
		return Canvas;
	}

	void RoadAt(UWidgetTree* Tree, UCanvasPanel* Canvas, FName Name, const FString& Caption, float X, float Y, float W, float H, float Angle = 0.f)
	{
		USizeBox* Tilt = Tree->ConstructWidget<USizeBox>();
		UMWRoadButtonWidget* Widget = Tree->ConstructWidget<UMWRoadButtonWidget>(ButtonClass, Name);
		Widget->LabelText = FText::FromString(Caption); Widget->LabelFontSize = H >= 110.f ? 52 : 32; Widget->IconSize=H<100.f?42.f:(H<110.f?54.f:76.f); Tilt->AddChild(Widget);
		Place(Canvas, Tilt, X, Y, W, H, Angle);
	}

	bool BuildRoot()
	{
		using namespace MWFrontendAuthoringValidation;
		FClassProperty* EntryProperty=FindFProperty<FClassProperty>(UListView::StaticClass(),TEXT("EntryWidgetClass"));
		FClassProperty* LoginProperty=FindFProperty<FClassProperty>(UMWFrontendWidget::StaticClass(),TEXT("LoginWidgetClass"));
		FStructProperty* FontProperty=FindFProperty<FStructProperty>(UComboBoxString::StaticClass(),TEXT("Font"));
		UClass* LoginClass=LoadClass<UUserWidget>(nullptr,TEXT("/Game/UI/Lobby/WBP_LoginTest.WBP_LoginTest_C"));
		FString Failure;
		if (!CheckWidgetClass(EntryProperty,RowClass,UMWRoomRowWidget::StaticClass(),Failure) || !CheckWidgetClass(LoginProperty,LoginClass,UUserWidget::StaticClass(),Failure) || !FontProperty || FontProperty->Struct!=FSlateFontInfo::StaticStruct())
		{
			UE_LOG(LogTemp,Error,TEXT("MWFrontend BuildRoot dependency validation failed: %s (Combo Font=%s). No root Blueprint saved."),*Failure,FontProperty?TEXT("present"):TEXT("missing")); return false;
		}

		UWidgetBlueprint* Blueprint = NewBlueprint(TEXT("/Game/UI/Frontend"), TEXT("WBP_FrontendRoot"), UMWFrontendWidget::StaticClass());
		if (!Blueprint) { return false; }
		UWidgetTree* Tree = Blueprint->WidgetTree;
		UCanvasPanel* Screen = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Screen")); Tree->RootWidget = Screen;
		UScaleBox* Scale = Tree->ConstructWidget<UScaleBox>(); Scale->SetStretch(EStretch::ScaleToFit);
		UCanvasPanelSlot* Slot = Screen->AddChildToCanvas(Scale); Slot->SetAnchors(FAnchors(0,0,1,1)); Slot->SetOffsets(FMargin(0));
		USizeBox* Design = Tree->ConstructWidget<USizeBox>(); Design->SetWidthOverride(1920); Design->SetHeightOverride(1080); Scale->AddChild(Design);
		UCanvasPanel* Layout = Tree->ConstructWidget<UCanvasPanel>(); Design->AddChild(Layout);
		UWidgetSwitcher* Pages = Tree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("Pages")); Place(Layout,Pages,0,0,1920,1080);
		Place(Layout,Text(Tree,TEXT("Heading"),TEXT(""),24),40,20,1800,42);
		UTextBlock* Status = Text(Tree,TEXT("StatusLabel"),TEXT(""),22); Status->SetColorAndOpacity(FSlateColor(Ink));
		Place(Layout,Status,450,935,725,140);

		UCanvasPanel* LoginPage = Tree->ConstructWidget<UCanvasPanel>(); Pages->AddChild(LoginPage);
		UVerticalBox* Login = Panel(Tree,TEXT("LoginHost")); Place(LoginPage,Login,0,0,1920,1080);
		UCanvasPanel* Choice = Tree->ConstructWidget<UCanvasPanel>(); Pages->AddChild(Choice);
		RoadAt(Tree,Choice,TEXT("OpenCreateButton"),TEXT("경기 생성"),745,325,560,115,-8);
		RoadAt(Tree,Choice,TEXT("OpenBrowserButton"),TEXT("경기 참가"),770,470,560,115,-10);

		UCanvasPanel* Create = Tree->ConstructWidget<UCanvasPanel>(); Pages->AddChild(Create);
		UBorder* Title=SignPanel(Tree,Blue); UTextBlock* TitleText=Text(Tree,TEXT("CreateTitle"),TEXT("경기 생성"),44); TitleText->SetJustification(ETextJustify::Center); Title->AddChild(TitleText); Place(Create,Title,720,90,480,110);
		Place(Create,Text(Tree,TEXT("RoomNameCaption"),TEXT("방 이름  ·  1~32자"),32),320,295,540,55);
		UEditableTextBox* Input=Tree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(),TEXT("RoomNameInput"));
		Input->SetHintText(FText::FromString(TEXT("방 이름을 입력하세요"))); FEditableTextBoxStyle InputStyle=Input->GetWidgetStyle(); InputStyle.SetFont(TextFont(34)); InputStyle.SetForegroundColor(Ink); InputStyle.TextStyle.SetColorAndOpacity(FSlateColor(Ink)); Input->SetWidgetStyle(InputStyle); Input->SetClearKeyboardFocusOnCommit(false); Input->SetSelectAllTextWhenFocused(false); Place(Create,Input,860,280,740,74);
		Place(Create,Text(Tree,TEXT("CapacityCaption"),TEXT("최대 플레이어  ·  호스트 포함"),32),320,415,540,55);
		Place(Create,Text(Tree,TEXT("ThemeCaption"),TEXT("맵 테마"),32),320,535,540,55);
		for (const FName Name : {FName(TEXT("CapacityInput")),FName(TEXT("ThemeInput"))})
		{
			UComboBoxString* Combo=Tree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(),Name);
			const FSlateFontInfo FontInfo=TextFont(32); FontProperty->CopyCompleteValue(FontProperty->ContainerPtrToValuePtr<void>(Combo),&FontInfo);
			StyleCombo(Combo); Place(Create,Combo,860,Name==TEXT("CapacityInput")?400:520,740,80);
		}
		RoadAt(Tree,Create,TEXT("CreateButton"),TEXT("생성"),1280,825,480,105);
		RoadAt(Tree,Create,TEXT("CreateBackButton"),TEXT("뒤로"),150,825,350,105);

		UCanvasPanel* Browser=Tree->ConstructWidget<UCanvasPanel>(); Pages->AddChild(Browser);
		UBorder* SearchTitle=SignPanel(Tree,Blue); SearchTitle->AddChild(Text(Tree,TEXT("BrowserTitle"),TEXT("경기 참가"),42)); Place(Browser,SearchTitle,600,80,720,100,-2);
		Place(Browser,Text(Tree,TEXT("RoomCountLabel"),TEXT("검색 결과"),30),240,225,1450,50);
		UListView* List=Tree->ConstructWidget<UListView>(UListView::StaticClass(),TEXT("RoomList"));
		EntryProperty->SetObjectPropertyValue_InContainer(List,RowClass);
		Place(Browser,List,240,290,1440,470);
		RoadAt(Tree,Browser,TEXT("RefreshButton"),TEXT("새로고침"),720,825,480,105);
		RoadAt(Tree,Browser,TEXT("JoinButton"),TEXT("참가"),1280,825,480,105);
		RoadAt(Tree,Browser,TEXT("BrowserBackButton"),TEXT("뒤로"),150,825,350,105);

		UCanvasPanel* Lobby=Tree->ConstructWidget<UCanvasPanel>(); Pages->AddChild(Lobby);
		for (int32 Index=0;Index<4;++Index)
		{
			const FName BoardName(*FString::Printf(TEXT("ParticipantBoard%d"),Index));
			UCanvasPanel* Board=RoundedPanel(Tree,BoardName,PlayerSign,75.f/512.f,Index==0?Blue:Yellow,false);
			UTextBlock* Player=Text(Tree,FName(*FString::Printf(TEXT("ParticipantSlot%d"),Index)),TEXT(""),42);
			Player->SetAutoWrapText(false); Player->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis); Player->SetClipping(EWidgetClipping::ClipToBounds);
			Place(Board,Player,140,65,610,65);
			UTextBlock* State=Text(Tree,FName(*FString::Printf(TEXT("ParticipantState%d"),Index)),TEXT(""),32);
			State->SetJustification(ETextJustify::Right); State->SetAutoWrapText(false); Place(Board,State,745,70,280,55);
			UImage* Pad=Image(Tree,FName(*FString::Printf(TEXT("EmptySlotIcon%d"),Index)),PadIcon,Ink); Place(Board,Pad,43,70,64,46);
			Place(Lobby,Board,120,90+Index*210,1070,195);
		}
		UTextBlock* Legacy=Text(Tree,TEXT("ParticipantLabel"),TEXT(""),1); Legacy->SetVisibility(ESlateVisibility::Collapsed); Place(Lobby,Legacy,0,0,1,1);
		UCanvasPanel* Header=RoundedPanel(Tree,TEXT("LobbyHeader"),ModeSign,128.f/512.f,Blue,true);
		UTextBlock* LobbyName=Text(Tree,TEXT("LobbyTitle"),TEXT("로비"),30); LobbyName->SetJustification(ETextJustify::Center); LobbyName->SetAutoWrapText(false); LobbyName->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis); LobbyName->SetClipping(EWidgetClipping::ClipToBounds); Place(Header,LobbyName,32,20,480,60);
		UTextBlock* LobbyStatus=Text(Tree,TEXT("LobbyStatus"),TEXT("서버 확인 대기"),24); LobbyStatus->SetJustification(ETextJustify::Center); LobbyStatus->SetColorAndOpacity(FSlateColor(FLinearColor::White)); Place(Header,LobbyStatus,35,85,470,40);
		Place(Lobby,Header,1215,90,550,145);
		UCanvasPanel* Mode=RoundedPanel(Tree,TEXT("LobbyModeBoard"),ModeSign,128.f/512.f,Yellow,true);
		UTextBlock* ModeCaption=Text(Tree,TEXT("LobbyModeCaption"),TEXT("맵 테마"),26); ModeCaption->SetJustification(ETextJustify::Center); Place(Mode,ModeCaption,35,25,480,45);
		UTextBlock* Theme=Text(Tree,TEXT("LobbyTheme"),TEXT(""),34); Theme->SetJustification(ETextJustify::Center); Theme->SetClipping(EWidgetClipping::ClipToBounds); Place(Mode,Theme,45,85,460,135);
		Place(Lobby,Mode,1215,250,550,255);
		UCanvasPanel* Details=RoundedPanel(Tree,TEXT("LobbyDetailsBoard"),ModeSign,128.f/512.f,Yellow,true,true);
		UTextBlock* Info=Text(Tree,TEXT("LobbyInfo"),TEXT(""),24); Info->SetClipping(EWidgetClipping::ClipToBounds); Place(Details,Info,38,30,470,245);
		Place(Lobby,Details,1215,520,550,310);
		RoadAt(Tree,Lobby,TEXT("LeaveButton"),TEXT("나가기"),120,955,310,80);
		RoadAt(Tree,Lobby,TEXT("ReadyButton"),TEXT("준비 완료"),1215,855,550,80);
		RoadAt(Tree,Lobby,TEXT("StartButton"),TEXT("게임 시작"),1215,955,550,80);
		AddTransition(Blueprint);
		if (!Compile(Blueprint)) { return false; }
		UObject* Defaults=Blueprint->GeneratedClass?Blueprint->GeneratedClass->GetDefaultObject():nullptr;
		if (!CheckDefaults(Defaults,UMWFrontendWidget::StaticClass(),Failure)) { UE_LOG(LogTemp,Error,TEXT("MWFrontend BuildRoot: %s. No root Blueprint saved."),*Failure); return false; }
		LoginProperty->SetObjectPropertyValue_InContainer(Defaults,LoginClass);
		return Save(Blueprint);
	}

}

FString UMWFrontendAuthoringLibrary::ExportWidgetLogic(const FString& AssetPath)
{
	UWidgetBlueprint* Blueprint=LoadObject<UWidgetBlueprint>(nullptr,*AssetPath);
	if (!Blueprint) { return TEXT("MISSING_BLUEPRINT"); }
	TArray<UEdGraph*> Graphs; Blueprint->GetAllGraphs(Graphs);
	Graphs.Sort([](const UEdGraph& A,const UEdGraph& B){return A.GetName()<B.GetName();});
	FString Result;
	for (UEdGraph* Graph:Graphs)
	{
		TSet<UObject*> Nodes; for (UEdGraphNode* Node:Graph->Nodes) { Nodes.Add(Node); }
		FString Export; FEdGraphUtilities::ExportNodesToText(Nodes,Export);
		Result+=Graph->GetName()+TEXT("\n")+Export+TEXT("\n");
	}
	return Result;
}

bool UMWFrontendAuthoringLibrary::SetLoginProgressForTest(UUserWidget* Login, bool bInProgress)
{
	if (!Login || !Login->GetWorld() || Login->GetWorld()->WorldType!=EWorldType::PIE || !Login->GetGameInstance()) { return false; }
	UMWMultiplayerSubsystem* Subsystem=Login->GetGameInstance()->GetSubsystem<UMWMultiplayerSubsystem>();
	FBoolProperty* Property=FindFProperty<FBoolProperty>(UMWMultiplayerSubsystem::StaticClass(),TEXT("bLoginInProgress"));
	if (!Subsystem || !Property) { return false; }
	// Explicit editor fixture; never call while a real EOS request is outstanding.
	Property->SetPropertyValue_InContainer(Subsystem,bInProgress);
	return true;
}

bool UMWFrontendAuthoringLibrary::ApplyLoginPresentation()
{
	using namespace MWFrontendAuthoring;
	UWidgetBlueprint* Blueprint=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/UI/Lobby/WBP_LoginTest.WBP_LoginTest"));
	UTexture2D* Frame=LoadObject<UTexture2D>(nullptr,TEXT("/Game/UI/Common/T_MW_DisclaimerFrame.T_MW_DisclaimerFrame"));
	UTexture2D* Outline=LoadObject<UTexture2D>(nullptr,TEXT("/Game/UI/Common/T_MW_DisclaimerBorder.T_MW_DisclaimerBorder"));
	Font=LoadObject<UFont>(nullptr,TEXT("/Game/UI/Common/F_NotoSansKR_Bold_Font.F_NotoSansKR_Bold_Font"));
	Bolt=LoadObject<UTexture2D>(nullptr,TEXT("/Game/UI/Common/T_MW_SignBolt.T_MW_SignBolt"));
	Sign=LoadObject<UTexture2D>(nullptr,TEXT("/Game/UI/Common/T_MW_MenuSign.T_MW_MenuSign"));
	if (!Blueprint || !Frame || !Outline || !Font || !Bolt || !Sign) { return false; }
	UWidgetTree* Tree=Blueprint->WidgetTree;
	USizeBox* Design=Cast<USizeBox>(Tree->RootWidget);
	UCanvasPanel* Canvas=Design?Cast<UCanvasPanel>(Design->GetContent()):nullptr;
	UButton* Login=Cast<UButton>(Tree->FindWidget(TEXT("LogIn")));
	UTextBlock* Caption=Cast<UTextBlock>(Tree->FindWidget(TEXT("TextBlock_0")));
	UTextBlock* Status=Cast<UTextBlock>(Tree->FindWidget(TEXT("상태Text")));
	UTextBlock* Error=Cast<UTextBlock>(Tree->FindWidget(TEXT("에러Text")));
	if (!Canvas || !Login || !Caption || !Status || !Error || Tree->FindWidget(TEXT("LoginNoticeFrame"))) { return false; }
	const FString Before=ExportWidgetLogic(Blueprint->GetPathName());
	Blueprint->Modify(); Design->SetWidthOverride(1920); Design->SetHeightOverride(1080);
	UBorder* Black=Tree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("LoginBlackBackground"));
	Black->SetBrushColor(FLinearColor::Black); Black->SetVisibility(ESlateVisibility::HitTestInvisible);
	Place(Canvas,Black,0,0,1920,1080); CastChecked<UCanvasPanelSlot>(Black->Slot)->SetZOrder(-3);
	// Original sprites remain separate: source borders 70/512 and 22.97325/251.9465.
	UBorder* Plate=Tree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("LoginNoticeFrame"));
	Plate->SetBrush(Brush(Frame,FLinearColor::White,70.f/512.f));
	Plate->SetBrushColor(Yellow); Plate->SetVisibility(ESlateVisibility::HitTestInvisible);
	Place(Canvas,Plate,192,108,1536,756); CastChecked<UCanvasPanelSlot>(Plate->Slot)->SetZOrder(-2);
	UBorder* Rim=Tree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("LoginNoticeOutline"));
	Rim->SetBrush(Brush(Outline,FLinearColor::White,22.97325f/251.9465f)); Rim->SetBrushColor(FLinearColor::Black); Rim->SetVisibility(ESlateVisibility::HitTestInvisible);
	Place(Canvas,Rim,198,114,1524,744); CastChecked<UCanvasPanelSlot>(Rim->Slot)->SetZOrder(-1);
	for (int32 Index=0;Index<4;++Index)
	{
		UImage* Pin=Image(Tree,FName(*FString::Printf(TEXT("LoginNoticeBolt%d"),Index+1)),Bolt,FLinearColor::White);
		Pin->SetVisibility(ESlateVisibility::HitTestInvisible);
		Place(Canvas,Pin,Index%2?1676:217,Index<2?133:802,28,34);
	}
	UTextBlock* Title=Text(Tree,TEXT("LoginTitle"),TEXT("ProjectMW"),64); Title->SetJustification(ETextJustify::Center); Place(Canvas,Title,400,270,1120,100);
	UTextBlock* Hint=Text(Tree,TEXT("LoginHint"),TEXT("Epic Games 계정으로 로그인"),28); Hint->SetJustification(ETextJustify::Center); Place(Canvas,Hint,400,385,1120,60);
	// Keep the existing objects/names and every EventGraph node; only Designer properties change.
	for (UWidget* Existing:{static_cast<UWidget*>(Login),static_cast<UWidget*>(Status),static_cast<UWidget*>(Error)}) { Existing->RemoveFromParent(); }
	FButtonStyle Style=Login->GetStyle(); Style.Normal=Brush(Sign,Yellow,106.5f/512.f); Style.Hovered=Brush(Sign,Blue,106.5f/512.f); Style.Pressed=Style.Hovered; Style.Disabled=Brush(Sign,FLinearColor(.35f,.35f,.35f,1),106.5f/512.f); Login->SetStyle(Style);
	Login->SetRenderTransform(FWidgetTransform());
	Caption->SetText(FText::FromString(TEXT("EOS 로그인"))); Caption->SetFont(TextFont(38)); Caption->SetColorAndOpacity(FSlateColor(Ink)); Caption->SetJustification(ETextJustify::Center);
	Place(Canvas,Login,690,485,540,100);
	Status->SetFont(TextFont(28)); Status->SetColorAndOpacity(FSlateColor(Ink)); Status->SetJustification(ETextJustify::Center); Status->SetAutoWrapText(true); Place(Canvas,Status,340,625,1240,70);
	Error->SetFont(TextFont(23)); Error->SetColorAndOpacity(FSlateColor(Ink)); Error->SetJustification(ETextJustify::Center); Error->SetAutoWrapText(true); Place(Canvas,Error,340,710,1240,120);
	if (!Compile(Blueprint) || Before!=ExportWidgetLogic(Blueprint->GetPathName())) { UE_LOG(LogTemp,Error,TEXT("Login Designer change did not preserve EventGraph; refusing save.")); return false; }
	return Save(Blueprint);
}

bool UMWFrontendAuthoringLibrary::BuildNewWidgets()
{
	using namespace MWFrontendAuthoring;
	Font = LoadObject<UFont>(nullptr, TEXT("/Game/UI/Common/F_NotoSansKR_Bold_Font.F_NotoSansKR_Bold_Font"));
	Sign = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/Common/T_MW_MenuSign.T_MW_MenuSign"));
	PanelTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/Common/T_MW_MenuOverlay.T_MW_MenuOverlay"));
	PlayerSign=LoadObject<UTexture2D>(nullptr,TEXT("/Game/UI/Common/T_MW_PlayerSlotSign.T_MW_PlayerSlotSign"));
	LobbyOutline=LoadObject<UTexture2D>(nullptr,TEXT("/Game/UI/Common/T_MW_LobbySlotBorder.T_MW_LobbySlotBorder"));
	ModeSign=LoadObject<UTexture2D>(nullptr,TEXT("/Game/UI/Common/T_MW_BattleModeSign.T_MW_BattleModeSign"));
	ModeOutline=LobbyOutline; // Both original border PNGs and sprite borders are byte-identical.
	Bolt=LoadObject<UTexture2D>(nullptr,TEXT("/Game/UI/Common/T_MW_SignBolt.T_MW_SignBolt"));
	PadIcon=LoadObject<UTexture2D>(nullptr,TEXT("/Game/UI/Common/T_MW_GenericConsole.T_MW_GenericConsole"));
	if (!Font || !Sign || !PanelTexture || !PlayerSign || !LobbyOutline || !ModeSign || !ModeOutline || !Bolt || !PadIcon) { UE_LOG(LogTemp, Error, TEXT("Original frontend font or textures missing; authoring cancelled.")); return false; }
	return BuildButton() && BuildRow() && BuildRoot();
}

bool UMWFrontendAuthoringLibrary::BuildTestPreviewWidget()
{
	UWidgetBlueprint* Preview = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/Developers/FrontendTests/WBP_FrontendPreview.WBP_FrontendPreview"));
	if (!Preview)
	{
		UWidgetBlueprint* Source = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/UI/Frontend/WBP_FrontendRoot.WBP_FrontendRoot"));
		if (!Source) { return false; }
		Preview = Cast<UWidgetBlueprint>(FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().DuplicateAsset(TEXT("WBP_FrontendPreview"), TEXT("/Game/Developers/FrontendTests"), Source));
		if (!Preview) { return false; }
		Preview->ParentClass = UMWFrontendPreviewWidget::StaticClass();
		// Fixture-only canvas colour for the empty test map. Production stays transparent.
		if (UCanvasPanel* Canvas=Cast<UCanvasPanel>(Preview->WidgetTree->RootWidget))
		{
			UBorder* Backdrop=Preview->WidgetTree->ConstructWidget<UBorder>(); Backdrop->SetBrushColor(FLinearColor::FromSRGBColor(FColor(127,223,248)));
			UCanvasPanelSlot* BackdropSlot=Canvas->AddChildToCanvas(Backdrop); BackdropSlot->SetAnchors(FAnchors(0,0,1,1)); BackdropSlot->SetOffsets(FMargin(0)); BackdropSlot->SetZOrder(-1);
		}
		if (!MWFrontendAuthoring::Compile(Preview) || !MWFrontendAuthoring::Save(Preview)) { return false; }
	}
	return true;
}

UUserWidget* UMWFrontendAuthoringLibrary::CreateTransientTestWidget(APlayerController* Controller)
{
	if (!Controller || !Controller->IsLocalController()) { return nullptr; }
	UWidgetBlueprint* Source=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/UI/Frontend/WBP_FrontendRoot.WBP_FrontendRoot"));
	if (!Source) { return nullptr; }
	TStrongObjectPtr<UWidgetBlueprint> Preview(DuplicateObject<UWidgetBlueprint>(Source,GetTransientPackage(),MakeUniqueObjectName(GetTransientPackage(),UWidgetBlueprint::StaticClass(),TEXT("WBP_FrontendPreviewTransient"))));
	if (!Preview.IsValid()) { return nullptr; }
	Preview->SetFlags(RF_Transient);
	Preview->ClearFlags(RF_Public|RF_Standalone);
	Preview->ParentClass=UMWFrontendPreviewWidget::StaticClass();
	if (UCanvasPanel* Canvas=Cast<UCanvasPanel>(Preview->WidgetTree->RootWidget))
	{
		UBorder* Backdrop=Preview->WidgetTree->ConstructWidget<UBorder>();
		Backdrop->SetBrushColor(FLinearColor::FromSRGBColor(FColor(127,223,248)));
		UCanvasPanelSlot* Slot=Canvas->AddChildToCanvas(Backdrop);
		Slot->SetAnchors(FAnchors(0,0,1,1)); Slot->SetOffsets(FMargin(0)); Slot->SetZOrder(-1);
	}
	if (!MWFrontendAuthoring::Compile(Preview.Get())) { return nullptr; }
	return CreateWidget<UMWFrontendWidget>(Controller,Preview->GeneratedClass.Get());
}

UUserWidget* UMWFrontendAuthoringLibrary::ShowTestPreview(APlayerController* Controller)
{
	UMWFrontendWidget* Widget=Cast<UMWFrontendWidget>(CreateTransientTestWidget(Controller));
	if (!Widget) { return nullptr; }
	for (TObjectIterator<UMWFrontendWidget> It; It; ++It)
	{
		if (It->GetWorld() == Controller->GetWorld() && It->IsInViewport()) { It->RemoveFromParent(); }
	}
	UMWFrontendPreviewConnection* Connection = NewObject<UMWFrontendPreviewConnection>(Widget);
	Connection->PrepareLobby(Controller);
	Connection->bCompleteSearchImmediately = true;
	Widget->SetConnection(Connection);
	Widget->AddToViewport(100);
	if (UTextBlock* Heading = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Heading"))))
	{
		MWFrontendAuthoring::Font = LoadObject<UFont>(nullptr, TEXT("/Game/UI/Common/F_NotoSansKR_Bold_Font.F_NotoSansKR_Bold_Font"));
		Heading->SetText(FText::FromString(TEXT("UI TEST ONLY · 실제 EOS 연결 아님")));
		Heading->SetFont(MWFrontendAuthoring::TextFont(30));
	}
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Widget->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Controller->SetInputMode(Mode);
	Controller->bShowMouseCursor = true;
	return Widget;
}

void UMWFrontendAuthoringLibrary::SetTestPreviewSize(APlayerController* Controller, int32 Width, int32 Height)
{
    if (!Controller || !Controller->GetLocalPlayer() || Width < 640 || Height < 360) { return; }
    for (const TSharedRef<SWindow>& Candidate : FSlateApplication::Get().GetTopLevelWindows()) { if (Candidate->GetTitle().ToString().Contains(TEXT("MW Preview"))) { Candidate->Resize(FVector2D(Width, Height)); Candidate->MoveWindowTo(FVector2D(0, 0)); } }
    if (UGameViewportClient* ViewportClient = Controller->GetLocalPlayer()->ViewportClient)
    {
        if (TSharedPtr<SViewport> Widget = ViewportClient->GetGameViewportWidget()) { if (TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef())) { Window->Resize(FVector2D(Width, Height)); } }
    }
}

void UMWFrontendAuthoringLibrary::BeginTestPreview()
{
 FRequestPlaySessionParams Params; Params.WorldType=EPlaySessionWorldType::PlayInEditor; GUnrealEd->RequestPlaySession(Params);
}
