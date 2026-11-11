#include "MWTitleUIEditorLibrary.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphUtilities.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/World.h"
#include "Editor.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "UnrealClient.h"
#include "Widgets/SWindow.h"
#include "K2Node_ConstructObjectFromClass.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "WidgetBlueprint.h"

UWidgetTree* UMWTitleUIEditorLibrary::GetWidgetTree(UWidgetBlueprint* Blueprint)
{
	return IsValid(Blueprint) ? Blueprint->WidgetTree : nullptr;
}

bool UMWTitleUIEditorLibrary::SetWidgetTreeRoot(UWidgetBlueprint* Blueprint, UWidget* Root)
{
	UWidgetTree* Tree = GetWidgetTree(Blueprint);
	if (!Tree || !IsValid(Root) || Root->GetOuter() != Tree || Root->GetParent())
	{
		return false;
	}
	Blueprint->Modify();
	Tree->Modify();
	Tree->RootWidget = Root;
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	return true;
}

UWidget* UMWTitleUIEditorLibrary::AddDesignerWidget(UWidgetBlueprint* Blueprint,
	TSubclassOf<UWidget> WidgetClass, FName WidgetName)
{
	UWidgetTree* Tree = GetWidgetTree(Blueprint);
	if (!Tree || !WidgetClass || WidgetName.IsNone() || Tree->FindWidget(WidgetName))
	{
		return nullptr;
	}
	Blueprint->Modify();
	Tree->Modify();
	UWidget* Widget = Tree->ConstructWidget<UWidget>(WidgetClass, WidgetName);
	Blueprint->WidgetVariableNameToGuidMap.FindOrAdd(WidgetName, FGuid::NewGuid());
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	return Widget;
}

void UMWTitleUIEditorLibrary::SynchronizeDesignerWidgetGuids(UWidgetBlueprint* Blueprint)
{
	UWidgetTree* Tree = GetWidgetTree(Blueprint);
	if (!Tree) return;
	Blueprint->Modify();
	TArray<UWidget*> Widgets;
	Tree->GetAllWidgets(Widgets);
	TSet<FName> Names;
	for (UWidget* Widget : Widgets)
	{
		Names.Add(Widget->GetFName());
		Blueprint->WidgetVariableNameToGuidMap.FindOrAdd(Widget->GetFName(), FGuid::NewGuid());
	}
	// RemoveFromParent만으로는 Blueprint의 이름/GUID 참조가 제거되지 않는다.
	// 애니메이션의 GUID는 위젯 삭제와 무관하므로 유지한다.
	for (const UWidgetAnimation* Animation : Blueprint->Animations)
	{
		if (Animation) Names.Add(Animation->GetFName());
	}
	for (auto It = Blueprint->WidgetVariableNameToGuidMap.CreateIterator(); It; ++It)
	{
		if (!Names.Contains(It.Key())) It.RemoveCurrent();
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
}

UBlueprint* UMWTitleUIEditorLibrary::GetLevelBlueprint(UWorld* World)
{
	return IsValid(World) && World->PersistentLevel
		? World->PersistentLevel->GetLevelScriptBlueprint(true) : nullptr;
}

int32 UMWTitleUIEditorLibrary::ReplaceWidgetClassPins(UBlueprint* Blueprint,
	UClass* PreviousClass, UClass* ReplacementClass)
{
	if (!IsValid(Blueprint) || !PreviousClass || !ReplacementClass || PreviousClass == ReplacementClass)
	{
		return 0;
	}
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	int32 Count = 0;
	for (UEdGraph* Graph : Graphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_ConstructObjectFromClass* Create = Cast<UK2Node_ConstructObjectFromClass>(Node))
			{
				UEdGraphPin* Pin = Create->GetClassPin();
				if (Pin && Pin->LinkedTo.IsEmpty() && Pin->DefaultObject == PreviousClass)
				{
					Blueprint->Modify();
					Create->Modify();
					Pin->DefaultObject = ReplacementClass;
					Create->PinDefaultValueChanged(Pin);
					++Count;
				}
			}
		}
	}
	if (Count > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}
	return Count;
}

FString UMWTitleUIEditorLibrary::ExportGraphs(UBlueprint* Blueprint)
{
	FString Result;
	if (!IsValid(Blueprint))
	{
		return Result;
	}
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		TSet<UObject*> Nodes;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			Nodes.Add(Node);
		}
		FString Text;
		FEdGraphUtilities::ExportNodesToText(Nodes, Text);
		Result += Graph->GetName() + LINE_TERMINATOR + Text;
	}
	return Result;
}

bool UMWTitleUIEditorLibrary::CompileBlueprint(UBlueprint* Blueprint)
{
	if (!IsValid(Blueprint))
	{
		return false;
	}
	if (GEditor && GEditor->PlayWorld)
	{
		return false;
	}
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	return Blueprint->Status == BS_UpToDate || Blueprint->Status == BS_UpToDateWithWarnings;
}

bool UMWTitleUIEditorLibrary::SetSingleTypefaceFace(UFont* Font, UFontFace* Face)
{
	if (!IsValid(Font) || !IsValid(Face))
	{
		return false;
	}
	FCompositeFont& Composite = Font->GetMutableInternalCompositeFont();
	if (Composite.DefaultTypeface.Fonts.Num() != 1 || !Composite.SubTypefaces.IsEmpty())
	{
		return false;
	}
	Font->Modify();
	Composite.DefaultTypeface.Fonts[0].Font = FFontData(Face);
	Font->MarkPackageDirty();
	return true;
}

bool UMWTitleUIEditorLibrary::ResizePlayWindow(APlayerController* Controller, FIntPoint ViewportSize)
{
	ULocalPlayer* Player = IsValid(Controller) ? Controller->GetLocalPlayer() : nullptr;
	UGameViewportClient* Client = Player ? Player->ViewportClient : nullptr;
	if (!Client || !Client->Viewport || ViewportSize.X <= 0 || ViewportSize.Y <= 0)
	{
		return false;
	}
	if (TSharedPtr<SWindow> Window = Client->GetWindow())
	{
		const FVector2D Difference = FVector2D(ViewportSize) - FVector2D(Client->Viewport->GetSizeXY());
		Window->Resize(FVector2D(Window->GetClientSizeInScreen()) + Difference);
		return true;
	}
	return false;
}
