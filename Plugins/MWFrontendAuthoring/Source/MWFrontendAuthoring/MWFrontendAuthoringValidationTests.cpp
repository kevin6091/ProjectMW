#include "MWFrontendAuthoringValidation.h"
#include "Frontend/MWFrontendWidget.h"
#include "Frontend/MWRoomRowWidget.h"
#include "Components/ListView.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWFrontendAuthoringDependencyTest, "MW.Frontend.AuthoringDependencyValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMWFrontendAuthoringDependencyTest::RunTest(const FString& Parameters)
{
	using namespace MWFrontendAuthoringValidation;
	FClassProperty* Entry = FindFProperty<FClassProperty>(UListView::StaticClass(), TEXT("EntryWidgetClass"));
	FClassProperty* Login = FindFProperty<FClassProperty>(UMWFrontendWidget::StaticClass(), TEXT("LoginWidgetClass"));
	FString Error;
	TestTrue(TEXT("Real row property accepts the existing row class"), CheckWidgetClass(Entry, UMWRoomRowWidget::StaticClass(), UUserWidget::StaticClass(), Error));
	UClass* LoginClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/UI/Lobby/WBP_LoginTest.WBP_LoginTest_C"));
	TestTrue(TEXT("Existing login class remains assignable"), CheckWidgetClass(Login, LoginClass, UUserWidget::StaticClass(), Error));
	TestFalse(TEXT("Missing reflected property fails safely"), CheckWidgetClass(nullptr, LoginClass, UUserWidget::StaticClass(), Error));
	TestFalse(TEXT("Wrong property type is rejected by reflection"), CheckWidgetClass(FindFProperty<FClassProperty>(UListView::StaticClass(), TEXT("bIsFocusable")), LoginClass, UUserWidget::StaticClass(), Error));
	TestFalse(TEXT("Missing login load result fails safely"), CheckWidgetClass(Login, nullptr, UUserWidget::StaticClass(), Error));
	TestFalse(TEXT("Non-widget class is rejected"), CheckWidgetClass(Login, UObject::StaticClass(), UUserWidget::StaticClass(), Error));
	TestFalse(TEXT("Missing generated defaults fail safely"), CheckDefaults(nullptr, UMWFrontendWidget::StaticClass(), Error));
	TestFalse(TEXT("Wrong generated defaults fail safely"), CheckDefaults(GetTransientPackage(), UMWFrontendWidget::StaticClass(), Error));
	TestFalse(TEXT("Failure reason is populated"), Error.IsEmpty());
	UClass* Root = LoadClass<UMWFrontendWidget>(nullptr, TEXT("/Game/UI/Frontend/WBP_FrontendRoot.WBP_FrontendRoot_C"));
	if (!TestNotNull(TEXT("Production root loads"), Root)) { return false; }
	TestTrue(TEXT("Compiled root default object is valid"), CheckDefaults(Root->GetDefaultObject(), UMWFrontendWidget::StaticClass(), Error));
	TestEqual(TEXT("Generated root retains original login class"), Login->GetObjectPropertyValue_InContainer(Root->GetDefaultObject()), static_cast<UObject*>(LoginClass));
	return true;
}
#endif
