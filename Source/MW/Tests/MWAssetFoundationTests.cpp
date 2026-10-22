#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Data/MWAssetData.h"
#include "Data/MWInputData.h"
#include "Data/MWVehicleDefinition.h"
#include "System/MWAssetManager.h"
#include "MWGameplayTags.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace MWAssetTests
{
template<typename PropertyType>
PropertyType* Property(UObject* Object, const FName Name)
{
	PropertyType* Result = FindFProperty<PropertyType>(Object->GetClass(), Name);
	check(Result);
	return Result;
}

template<typename Element>
void SetArray(UObject* Object, const FName Name, const TArray<Element>& Values)
{
	*Property<FArrayProperty>(Object, Name)->template ContainerPtrToValuePtr<TArray<Element>>(Object) = Values;
}

void SetObject(UObject* Object, const FName Name, UObject* Value)
{
	Property<FObjectPropertyBase>(Object, Name)->SetObjectPropertyValue_InContainer(Object, Value);
}

FMWAssetEntry Entry(const FGameplayTag Id, UObject* Asset)
{
	FMWAssetEntry Result;
	Result.AssetId = Id;
	Result.Asset = Asset;
	Result.Labels.AddTag(MWGameplayTags::AssetLabel_Preload);
	return Result;
}

UMWInputData* InputData()
{
	UMWInputData* Data = NewObject<UMWInputData>();
	SetObject(Data, TEXT("InputMappingContext"), NewObject<UInputMappingContext>(Data));
	FMWInputAction Action;
	Action.InputTag = MWGameplayTags::Input_Throttle;
	Action.InputAction = NewObject<UInputAction>(Data);
	SetArray(Data, TEXT("InputActions"), TArray<FMWInputAction>{ Action });
	return Data;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWCatalogTest, "MW.Foundation.CatalogIdentityAndLifetime",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMWCatalogTest::RunTest(const FString&)
{
	using namespace MWAssetTests;
	TStrongObjectPtr<UMWAssetManager> Manager(NewObject<UMWAssetManager>());
	TStrongObjectPtr<UMWAssetData> Catalog(NewObject<UMWAssetData>());
	const FString Base = TEXT("/Temp/MWFoundation/") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	UDataAsset* First = NewObject<UMWAssetData>(CreatePackage(*(Base + TEXT("/One"))), TEXT("Shared"), RF_Public);
	UDataAsset* Second = NewObject<UMWAssetData>(CreatePackage(*(Base + TEXT("/Two"))), TEXT("Shared"), RF_Public);
	const FSoftObjectPath FirstPath(First);
	SetArray(Catalog.Get(), TEXT("Entries"), TArray<FMWAssetEntry>{
		Entry(MWGameplayTags::Asset_Input_Driving, First),
		Entry(MWGameplayTags::Asset_Input_Construction, Second),
		Entry(MWGameplayTags::Asset_Vehicle_Default, First)
	});
	TArray<FText> Errors;
	TestTrue(TEXT("Valid catalog initializes"), Manager->InitializeCatalog(Catalog.Get(), Errors));
	TestNull(TEXT("Lookup never loads implicitly"), Manager->FindLoadedAsset(MWGameplayTags::Asset_Input_Driving));
	TestEqual(TEXT("Lookup preserves full package path"), Manager->FindAssetPath(MWGameplayTags::Asset_Input_Driving), FirstPath);
	TestTrue(TEXT("Label preloads all members"), Manager->LoadAssetsByLabelSynchronous(MWGameplayTags::AssetLabel_Preload));
	TestEqual(TEXT("First same-name asset"), Manager->FindLoadedAsset(MWGameplayTags::Asset_Input_Driving), static_cast<UObject*>(First));
	TestEqual(TEXT("Second same-name asset is distinct"), Manager->FindLoadedAsset(MWGameplayTags::Asset_Input_Construction), static_cast<UObject*>(Second));
	TestNull(TEXT("Wrong typed lookup fails"), Manager->FindLoadedAsset<UInputAction>(MWGameplayTags::Asset_Input_Driving));

	TWeakObjectPtr<UDataAsset> WeakFirst(First);
	TWeakObjectPtr<UDataAsset> WeakSecond(Second);
	First = nullptr;
	Second = nullptr;
	CollectGarbage(RF_NoFlags);
	TestTrue(TEXT("Manager cache retains loaded objects across GC"), WeakFirst.IsValid() && WeakSecond.IsValid());
	Manager->ReleaseAsset(MWGameplayTags::Asset_Input_Driving);
	CollectGarbage(RF_NoFlags);
	TestTrue(TEXT("Releasing one alias retains the other alias"), WeakFirst.IsValid());
	Manager->ReleaseAsset(MWGameplayTags::Asset_Vehicle_Default);
	CollectGarbage(RF_NoFlags);
	TestFalse(TEXT("Released object is collectible"), WeakFirst.IsValid());
	TestTrue(TEXT("Other same-name object remains retained"), WeakSecond.IsValid());

	TStrongObjectPtr<UMWAssetData> Invalid(NewObject<UMWAssetData>());
	SetArray(Invalid.Get(), TEXT("Entries"), TArray<FMWAssetEntry>{
		Entry(MWGameplayTags::Asset_Input_Construction, WeakSecond.Get()),
		Entry(MWGameplayTags::Asset_Input_Construction, WeakSecond.Get())
	});
	Errors.Reset();
	TestFalse(TEXT("Duplicate IDs reject the entire new catalog"), Manager->InitializeCatalog(Invalid.Get(), Errors));
	TestFalse(TEXT("Duplicate IDs return a diagnostic"), Errors.IsEmpty());
	TestNull(TEXT("Ambiguous lookup does not choose first"), Invalid->FindEntry(MWGameplayTags::Asset_Input_Construction));
	TestEqual(TEXT("Failed catalog swap preserves existing cache"),
		Manager->FindLoadedAsset(MWGameplayTags::Asset_Input_Construction), static_cast<UObject*>(WeakSecond.Get()));
	TestEqual(TEXT("Failed catalog swap preserves existing paths"),
		Manager->FindAssetPath(MWGameplayTags::Asset_Input_Driving), FirstPath);
	Errors.Reset();
	TestFalse(TEXT("Null catalog rejected"), Manager->InitializeCatalog(nullptr, Errors));

	// The manager uses an initialization snapshot, not an editor-mutated definition.
	SetArray(Catalog.Get(), TEXT("Entries"), TArray<FMWAssetEntry>{});
	TestEqual(TEXT("Authored edits do not silently retarget runtime paths"),
		Manager->FindAssetPath(MWGameplayTags::Asset_Input_Driving), FirstPath);
	Manager->ReleaseAllAssets();
	CollectGarbage(RF_NoFlags);
	TestFalse(TEXT("ReleaseAll drops remaining manager references"), WeakSecond.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWCatalogValidationTest, "MW.Foundation.CatalogValidation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMWCatalogValidationTest::RunTest(const FString&)
{
	using namespace MWAssetTests;
	TStrongObjectPtr<UMWAssetData> Catalog(NewObject<UMWAssetData>());
	FMWAssetEntry Empty;
	SetArray(Catalog.Get(), TEXT("Entries"), TArray<FMWAssetEntry>{ Empty });
	TArray<FText> Errors;
	TestFalse(TEXT("Empty ID and missing reference rejected"), Catalog->ValidateCatalog(Errors));
	TestEqual(TEXT("Both authoring issues are reported"), Errors.Num(), 2);
	TestNull(TEXT("Invalid tag cannot select an entry"), Catalog->FindEntry(FGameplayTag()));
	TestTrue(TEXT("Invalid label selects no entries"), Catalog->GetAssetIdsByLabel(FGameplayTag()).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWInputDefinitionTest, "MW.Foundation.InputAndVehicleDefinition",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMWInputDefinitionTest::RunTest(const FString&)
{
	using namespace MWAssetTests;
	TStrongObjectPtr<UMWInputData> Input(InputData());
	TArray<FText> Errors;
	TestTrue(TEXT("Input mode validates"), Input->ValidateInputData(Errors));
	TestNotNull(TEXT("Exact input tag resolves"), Input->FindInputActionByTag(MWGameplayTags::Input_Throttle));
	TestNull(TEXT("Missing input does not fall back"), Input->FindInputActionByTag(MWGameplayTags::Input_UseItem));
	TestNull(TEXT("Invalid input tag fails safely"), Input->FindInputActionByTag(FGameplayTag()));

	TStrongObjectPtr<UMWVehicleDefinition> Vehicle(NewObject<UMWVehicleDefinition>());
	TestFalse(TEXT("Unconfigured vehicle is rejected"), Vehicle->ValidateDefinition(Errors));
	Property<FNameProperty>(Vehicle.Get(), TEXT("VehicleId"))->SetPropertyValue_InContainer(Vehicle.Get(), FName(TEXT("Default")));
	SetObject(Vehicle.Get(), TEXT("PawnClass"), APawn::StaticClass());
	SetObject(Vehicle.Get(), TEXT("DrivingInputData"), Input.Get());
	Errors.Reset();
	TestTrue(TEXT("Definition needs no GAS, AI or character class"), Vehicle->ValidateDefinition(Errors));
	TestEqual(TEXT("Definition retains approved pawn class"), Vehicle->GetPawnClass().Get(), APawn::StaticClass());

	FMWInputAction Duplicate;
	Duplicate.InputTag = MWGameplayTags::Input_Throttle;
	Duplicate.InputAction = const_cast<UInputAction*>(Input->FindInputActionByTag(MWGameplayTags::Input_Throttle));
	SetArray(Input.Get(), TEXT("InputActions"), TArray<FMWInputAction>{ Duplicate, Duplicate });
	Errors.Reset();
	TestFalse(TEXT("Duplicate input tags rejected"), Input->ValidateInputData(Errors));
	TestNull(TEXT("Duplicate input lookup is ambiguous"), Input->FindInputActionByTag(MWGameplayTags::Input_Throttle));
	Errors.Reset();
	TestFalse(TEXT("Vehicle propagates invalid input configuration"), Vehicle->ValidateDefinition(Errors));

	SetObject(Input.Get(), TEXT("InputMappingContext"), nullptr);
	Duplicate.InputAction = nullptr;
	SetArray(Input.Get(), TEXT("InputActions"), TArray<FMWInputAction>{ Duplicate });
	Errors.Reset();
	TestFalse(TEXT("Missing mapping/action rejected"), Input->ValidateInputData(Errors));
	TestEqual(TEXT("Mapping and action issues are reported"), Errors.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWAssetManagerStartupTest, "MW.Foundation.StartupConfiguration",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMWAssetManagerStartupTest::RunTest(const FString&)
{
	UMWAssetManager* Manager = Cast<UMWAssetManager>(UAssetManager::GetIfInitialized());
	TestNotNull(TEXT("Engine instantiates MWAssetManager from config"), Manager);
	if (Manager) TestEqual(TEXT("Get returns the configured singleton"), &UMWAssetManager::Get(), Manager);
	return true;
}
#endif
