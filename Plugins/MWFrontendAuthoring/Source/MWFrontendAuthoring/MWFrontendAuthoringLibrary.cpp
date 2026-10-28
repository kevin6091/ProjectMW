#include "MWFrontendAuthoringLibrary.h"
#include "MWFrontendPreview.h"

#include "Frontend/MWFrontendWidget.h"
#include "Frontend/MWRoadButtonWidget.h"
#include "Frontend/MWRoomRowWidget.h"
#include "Animation/WidgetAnimation.h"
#include "Animation/WidgetAnimationBinding.h"
#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
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
#include "GameFramework/PlayerController.h"
#include "UObject/UObjectIterator.h"

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
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FCompilerResultsLog Results;
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Results);
		return Results.NumErrors == 0 && Blueprint->Status != BS_Error && Save(Blueprint);
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
		Style.SetPressed(Brush(Sign, Blue * .75f, 106.5f / 512.f));
		Style.SetDisabled(Brush(Sign, FLinearColor(.42f, .45f, .45f, .65f), 106.5f / 512.f));
		Style.SetNormalPadding(FMargin(26.f, 6.f));
		Style.SetPressedPadding(FMargin(26.f, 9.f, 26.f, 3.f));
		Action->SetStyle(Style);
		UTextBlock* Label = Text(Tree, TEXT("Label"), TEXT("경기 생성"), 27);
		Label->SetAutoWrapText(false);
		Label->SetJustification(ETextJustify::Center);
		Action->AddChild(Label);
		if (!Compile(Blueprint)) { return false; }
		ButtonClass = Blueprint->GeneratedClass;
		return true;
	}

	bool BuildRow()
	{
		UWidgetBlueprint* Blueprint = NewBlueprint(TEXT("/Game/UI/Frontend"), TEXT("WBP_RoomRow"), UMWRoomRowWidget::StaticClass());
		if (!Blueprint) { return false; }
		UWidgetTree* Tree = Blueprint->WidgetTree;
		UBorder* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SelectionBorder"));
		Tree->RootWidget = Border;
		Border->SetBrush(Brush(Sign, FLinearColor::White, 106.5f / 512.f));
		Border->SetBrushColor(Yellow);
		Border->SetPadding(FMargin(24.f, 14.f));
		Border->AddChild(Text(Tree, TEXT("RoomLabel"), TEXT("방 이름    |    인원    |    테마"), 22));
		if (!Compile(Blueprint)) { return false; }
		RowClass = Blueprint->GeneratedClass;
		return true;
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

	bool BuildRoot()
	{
		UWidgetBlueprint* Blueprint = NewBlueprint(TEXT("/Game/UI/Frontend"), TEXT("WBP_FrontendRoot"), UMWFrontendWidget::StaticClass());
		if (!Blueprint) { return false; }
		UWidgetTree* Tree = Blueprint->WidgetTree;
		UCanvasPanel* Screen = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Screen"));
		Tree->RootWidget = Screen;
		UBorder* Background = Tree->ConstructWidget<UBorder>();
		Background->SetBrushColor(FLinearColor::FromSRGBColor(FColor(127, 223, 248)));
		UCanvasPanelSlot* BgSlot = Screen->AddChildToCanvas(Background);
		BgSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); BgSlot->SetOffsets(FMargin(0.f));
		UScaleBox* Scale = Tree->ConstructWidget<UScaleBox>();
		Scale->SetStretch(EStretch::ScaleToFit);
		UCanvasPanelSlot* ScaleSlot = Screen->AddChildToCanvas(Scale);
		ScaleSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); ScaleSlot->SetOffsets(FMargin(36.f));
		USizeBox* Design = Tree->ConstructWidget<USizeBox>();
		Design->SetWidthOverride(1100.f); Design->SetHeightOverride(900.f); Scale->AddChild(Design);
		UBorder* Frame = Tree->ConstructWidget<UBorder>();
		Frame->SetBrush(Brush(PanelTexture, FLinearColor::White, .16f));
		Frame->SetBrushColor(Yellow);
		Frame->SetPadding(FMargin(52.f, 26.f)); Design->AddChild(Frame);
		UVerticalBox* Layout = Panel(Tree, TEXT("Layout")); Frame->AddChild(Layout);
		Add(Layout, Text(Tree, TEXT("Heading"), TEXT("PROJECT MW"), 43), 8.f);
		UWidgetSwitcher* Pages = Tree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("Pages"));
		UVerticalBoxSlot* PagesSlot = Layout->AddChildToVerticalBox(Pages);
		PagesSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		UVerticalBox* Login = Panel(Tree, TEXT("LoginHost")); Pages->AddChild(Login);
		UVerticalBox* Choice = Panel(Tree, TEXT("MatchChoice")); Pages->AddChild(Choice);
		Add(Choice, Text(Tree, TEXT("ChoiceTitle"), TEXT("함께 달릴 경기를 선택하세요"), 31), 38.f);
		Button(Tree, Choice, TEXT("OpenCreateButton"), TEXT("경기 생성"));
		Button(Tree, Choice, TEXT("OpenBrowserButton"), TEXT("경기 참가"));

		UVerticalBox* Create = Panel(Tree, TEXT("CreateRoom")); Pages->AddChild(Create);
		Add(Create, Text(Tree, TEXT("CreateTitle"), TEXT("경기 생성"), 32));
		Add(Create, Text(Tree, TEXT("RoomNameCaption"), TEXT("방 이름  ·  1~32자"), 23), 4.f);
		UEditableTextBox* Input = Tree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("RoomNameInput"));
		Input->SetHintText(FText::FromString(TEXT("방 이름을 입력하세요"))); FEditableTextBoxStyle InputStyle = Input->GetWidgetStyle(); InputStyle.SetFont(TextFont(26)); InputStyle.SetForegroundColor(Ink); InputStyle.TextStyle.SetColorAndOpacity(FSlateColor(Ink)); Input->SetWidgetStyle(InputStyle);
		Input->SetSelectAllTextWhenFocused(false); Input->SetClearKeyboardFocusOnCommit(false);
		Add(Create, Input, 14.f);
		Add(Create, Text(Tree, TEXT("CapacityCaption"), TEXT("최대 인원  ·  호스트 포함"), 23), 4.f);
		UComboBoxString* Capacity = Tree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("CapacityInput"));
		const FSlateFontInfo ComboFont = TextFont(25); FindFProperty<FStructProperty>(UComboBoxString::StaticClass(), TEXT("Font"))->CopyCompleteValue(FindFProperty<FStructProperty>(UComboBoxString::StaticClass(), TEXT("Font"))->ContainerPtrToValuePtr<void>(Capacity), &ComboFont); Capacity->AddOption(TEXT("2")); Capacity->AddOption(TEXT("3")); Capacity->AddOption(TEXT("4")); Capacity->SetSelectedOption(TEXT("3"));
		Add(Create, Capacity, 14.f);
		StyleCombo(Capacity);
		Add(Create, Text(Tree, TEXT("ThemeCaption"), TEXT("맵 테마"), 23), 4.f);
		UComboBoxString* Theme = Tree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("ThemeInput")); FindFProperty<FStructProperty>(UComboBoxString::StaticClass(), TEXT("Font"))->CopyCompleteValue(FindFProperty<FStructProperty>(UComboBoxString::StaticClass(), TEXT("Font"))->ContainerPtrToValuePtr<void>(Theme), &ComboFont); Add(Create, Theme, 20.f);
		StyleCombo(Theme);
		Button(Tree, Create, TEXT("CreateButton"), TEXT("생성")); Button(Tree, Create, TEXT("CreateBackButton"), TEXT("뒤로"));

		UVerticalBox* Browser = Panel(Tree, TEXT("RoomBrowser")); Pages->AddChild(Browser);
		Add(Browser, Text(Tree, TEXT("BrowserTitle"), TEXT("경기 참가"), 32));
		Add(Browser, Text(Tree, TEXT("RoomCountLabel"), TEXT("검색 결과"), 22));
		UListView* List = Tree->ConstructWidget<UListView>(UListView::StaticClass(), TEXT("RoomList"));
		FClassProperty* EntryClass = FindFProperty<FClassProperty>(UListView::StaticClass(), TEXT("EntryWidgetClass"));
		if (!EntryClass) { return false; } EntryClass->SetObjectPropertyValue_InContainer(List, RowClass);
		UVerticalBoxSlot* ListSlot = Browser->AddChildToVerticalBox(List); ListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); ListSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
		Button(Tree, Browser, TEXT("RefreshButton"), TEXT("새로고침")); Button(Tree, Browser, TEXT("JoinButton"), TEXT("참가")); Button(Tree, Browser, TEXT("BrowserBackButton"), TEXT("뒤로"));

		UVerticalBox* Lobby = Panel(Tree, TEXT("CommonLobby")); Pages->AddChild(Lobby);
		Add(Lobby, Text(Tree, TEXT("LobbyTitle"), TEXT("공통 로비"), 30));
		UTextBlock* Players = Text(Tree, TEXT("ParticipantLabel"), TEXT("서버 참가자 정보를 기다리는 중"), 27);
		UVerticalBoxSlot* PlayerSlot = Lobby->AddChildToVerticalBox(Players); PlayerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Button(Tree, Lobby, TEXT("ReadyButton"), TEXT("준비 완료")); Button(Tree, Lobby, TEXT("StartButton"), TEXT("게임 시작")); Button(Tree, Lobby, TEXT("LeaveButton"), TEXT("나가기"));

		UTextBlock* Status = Text(Tree, TEXT("StatusLabel"), TEXT(""), 22);
		USizeBox* StatusBox = Tree->ConstructWidget<USizeBox>(); StatusBox->SetHeightOverride(68.f); StatusBox->AddChild(Status); Add(Layout, StatusBox, 0.f);
		AddTransition(Blueprint);
		if (!Compile(Blueprint)) { return false; }
		FClassProperty* LoginClass = FindFProperty<FClassProperty>(UMWFrontendWidget::StaticClass(), TEXT("LoginWidgetClass"));
		UClass* ExistingLogin = LoadClass<UUserWidget>(nullptr, TEXT("/Game/UI/Lobby/WBP_LoginTest.WBP_LoginTest_C"));
		if (!LoginClass || !ExistingLogin) { return false; }
		LoginClass->SetObjectPropertyValue_InContainer(Blueprint->GeneratedClass->GetDefaultObject(), ExistingLogin);
		return Save(Blueprint);
	}
}

bool UMWFrontendAuthoringLibrary::BuildNewWidgets()
{
	using namespace MWFrontendAuthoring;
	Font = LoadObject<UFont>(nullptr, TEXT("/Game/UI/Common/F_NotoSansKR_Bold_Font.F_NotoSansKR_Bold_Font"));
	Sign = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/Common/T_MW_MenuSign.T_MW_MenuSign"));
	PanelTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/Common/T_MW_MenuOverlay.T_MW_MenuOverlay"));
	if (!Font || !Sign || !PanelTexture) { UE_LOG(LogTemp, Error, TEXT("Original frontend font or textures missing; authoring cancelled.")); return false; }
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
		if (!MWFrontendAuthoring::Compile(Preview)) { return false; }
	}
	return true;
}

UUserWidget* UMWFrontendAuthoringLibrary::ShowTestPreview(APlayerController* Controller)
{
	if (!Controller || !Controller->IsLocalController() || !BuildTestPreviewWidget()) { return nullptr; }
	UWidgetBlueprint* Preview = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/Developers/FrontendTests/WBP_FrontendPreview.WBP_FrontendPreview"));
	for (TObjectIterator<UMWFrontendWidget> It; It; ++It)
	{
		if (It->GetWorld() == Controller->GetWorld() && It->IsInViewport()) { It->RemoveFromParent(); }
	}
	UMWFrontendWidget* Widget = CreateWidget<UMWFrontendWidget>(Controller, Preview->GeneratedClass.Get());
	if (!Widget) { return nullptr; }
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
