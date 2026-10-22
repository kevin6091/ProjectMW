#include "CoreMinimal.h"

// 개발용 자동화 검사를 켠 빌드에만 포함합니다. 검사 이름은 MW.Foundation 아래에 모입니다.
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Data/MWAssetData.h"
#include "Data/MWInputData.h"
#include "Data/MWVehicleDefinitionData.h"
#include "System/MWAssetManager.h"
#include "MWGameplayTags.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

// 실제 게임 코드에 테스트용 수정 함수를 추가하지 않고, 여기서만 설정 데이터를 만들어 검사합니다.
namespace MWAssetTests
{
/** 비공개 설정값도 테스트에서 바꿀 수 있도록, 언리얼에 등록된 변수 정보를 찾습니다. */
template<typename PropertyType>
PropertyType* Property(UObject* Object, const FName Name)
{
	PropertyType* Result = FindFProperty<PropertyType>(Object->GetClass(), Name);
	check(Result);
	return Result;
}

/** 언리얼에 등록된 배열 변수에 테스트용 항목들을 넣습니다. */
template<typename Element>
void SetArray(UObject* Object, const FName Name, const TArray<Element>& Values)
{
	*Property<FArrayProperty>(Object, Name)->template ContainerPtrToValuePtr<TArray<Element>>(Object) = Values;
}

/** 언리얼에 등록된 객체 참조 변수에 테스트용 객체나 클래스를 넣습니다. */
void SetObject(UObject* Object, const FName Name, UObject* Value)
{
	Property<FObjectPropertyBase>(Object, Name)->SetObjectPropertyValue_InContainer(Object, Value);
}

/** 지정한 ID와 에셋으로 항목을 만들고, 미리 불러오기 분류에 넣습니다. */
FMWAssetEntry Entry(const FGameplayTag Id, UObject* Asset)
{
	FMWAssetEntry Result;
	Result.AssetId = Id;
	Result.Asset = Asset;
	Result.Labels.AddTag(MWGameplayTags::AssetLabel_Preload);
	return Result;
}

/** 입력 매핑 설정과 가속 액션 하나를 갖춘, 검사에 통과할 수 있는 입력 데이터를 만듭니다. */
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

/**
 * 이름이 같은 에셋의 구분, 같은 에셋을 가리키는 여러 ID, 메모리 보관과 해제를 검사합니다.
 * 잘못된 새 목록을 거부할 때 기존 목록과 에셋이 유지되는지도 확인합니다.
 */
bool FMWCatalogTest::RunTest(const FString&)
{
	using namespace MWAssetTests;
	TStrongObjectPtr<UMWAssetManager> Manager(NewObject<UMWAssetManager>());
	TStrongObjectPtr<UMWAssetData> Catalog(NewObject<UMWAssetData>());
	// 검사마다 다른 임시 경로를 만들고, 파일명은 같지만 전체 경로는 다른 에셋 두 개를 준비합니다.
	const FString Base = TEXT("/Temp/MWFoundation/") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	UDataAsset* First = NewObject<UMWAssetData>(CreatePackage(*(Base + TEXT("/One"))), TEXT("Shared"), RF_Public);
	UDataAsset* Second = NewObject<UMWAssetData>(CreatePackage(*(Base + TEXT("/Two"))), TEXT("Shared"), RF_Public);
	const FSoftObjectPath FirstPath(First);
	// 첫 번째 에셋에는 ID를 두 개 붙여, 하나의 ID를 해제해도 다른 참조가 남는지 확인합니다.
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

	// 약한 참조로 생존 여부만 확인합니다. 메모리 정리 후에도 관리자 참조가 객체를 유지해야 합니다.
	TWeakObjectPtr<UDataAsset> WeakFirst(First);
	TWeakObjectPtr<UDataAsset> WeakSecond(Second);
	First = nullptr;
	Second = nullptr;
	CollectGarbage(RF_NoFlags);
	TestTrue(TEXT("Manager cache retains loaded objects across GC"), WeakFirst.IsValid() && WeakSecond.IsValid());
	// 같은 객체를 가리키는 두 ID를 차례로 해제해서, 마지막 참조가 없어질 때만 정리되는지 확인합니다.
	Manager->ReleaseAsset(MWGameplayTags::Asset_Input_Driving);
	CollectGarbage(RF_NoFlags);
	TestTrue(TEXT("Releasing one alias retains the other alias"), WeakFirst.IsValid());
	Manager->ReleaseAsset(MWGameplayTags::Asset_Vehicle_Default);
	CollectGarbage(RF_NoFlags);
	TestFalse(TEXT("Released object is collectible"), WeakFirst.IsValid());
	TestTrue(TEXT("Other same-name object remains retained"), WeakSecond.IsValid());

	// 중복 ID가 있는 새 목록은 통째로 거부하고, 기존 경로와 보관 중인 에셋은 유지해야 합니다.
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

	// 관리자는 초기화할 때 복사한 값을 사용합니다. 원본을 편집해도 다시 초기화하기 전에는 바뀌지 않아야
	// 합니다.
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

/** ID와 에셋 참조가 빠진 항목을 거부하고, 잘못된 태그 조회가 안전하게 빈 결과를 주는지 검사합니다. */
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

/** 입력 태그 조회, 중복·누락 검사, 차량 정의와 연결된 입력 설정의 검사를 함께 확인합니다. */
bool FMWInputDefinitionTest::RunTest(const FString&)
{
	using namespace MWAssetTests;
	TStrongObjectPtr<UMWInputData> Input(InputData());
	TArray<FText> Errors;
	TestTrue(TEXT("Input mode validates"), Input->ValidateInputData(Errors));
	TestNotNull(TEXT("Exact input tag resolves"), Input->FindInputActionByTag(MWGameplayTags::Input_Throttle));
	TestNull(TEXT("Missing input does not fall back"), Input->FindInputActionByTag(MWGameplayTags::Input_UseItem));
	TestNull(TEXT("Invalid input tag fails safely"), Input->FindInputActionByTag(FGameplayTag()));

	// 실제 차량 구현 없이 기본 Pawn 클래스로 검사해, 특정 캐릭터나 능력 시스템에 의존하지 않는지 확인합니다.
	TStrongObjectPtr<UMWVehicleDefinitionData> Vehicle(NewObject<UMWVehicleDefinitionData>());
	TestFalse(TEXT("Unconfigured vehicle is rejected"), Vehicle->ValidateDefinition(Errors));
	Property<FNameProperty>(Vehicle.Get(), TEXT("VehicleId"))->SetPropertyValue_InContainer(Vehicle.Get(), FName(TEXT("Default")));
	SetObject(Vehicle.Get(), TEXT("PawnClass"), APawn::StaticClass());
	SetObject(Vehicle.Get(), TEXT("DrivingInputData"), Input.Get());
	Errors.Reset();
	TestTrue(TEXT("Definition needs no GAS, AI or character class"), Vehicle->ValidateDefinition(Errors));
	TestEqual(TEXT("Definition retains approved pawn class"), Vehicle->GetPawnClass().Get(), APawn::StaticClass());

	// 입력 태그를 중복시켜, 입력 데이터뿐 아니라 이를 사용하는 차량 정의도 거부하는지 확인합니다.
	FMWInputAction Duplicate;
	Duplicate.InputTag = MWGameplayTags::Input_Throttle;
	Duplicate.InputAction = const_cast<UInputAction*>(Input->FindInputActionByTag(MWGameplayTags::Input_Throttle));
	SetArray(Input.Get(), TEXT("InputActions"), TArray<FMWInputAction>{ Duplicate, Duplicate });
	Errors.Reset();
	TestFalse(TEXT("Duplicate input tags rejected"), Input->ValidateInputData(Errors));
	TestNull(TEXT("Duplicate input lookup is ambiguous"), Input->FindInputActionByTag(MWGameplayTags::Input_Throttle));
	Errors.Reset();
	TestFalse(TEXT("Vehicle propagates invalid input configuration"), Vehicle->ValidateDefinition(Errors));

	// 키 연결 설정과 액션이 모두 빠졌을 때, 두 오류를 빠짐없이 모으는지 확인합니다.
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

/** 엔진 설정으로 MWAssetManager가 생성됐고, Get이 바로 그 관리자를 반환하는지 검사합니다. */
bool FMWAssetManagerStartupTest::RunTest(const FString&)
{
	UMWAssetManager* Manager = Cast<UMWAssetManager>(UAssetManager::GetIfInitialized());
	TestNotNull(TEXT("Engine instantiates MWAssetManager from config"), Manager);
	if (Manager) TestEqual(TEXT("Get returns the configured singleton"), &UMWAssetManager::Get(), Manager);
	return true;
}
#endif
