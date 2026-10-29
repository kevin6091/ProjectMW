#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UObject/UnrealType.h"

namespace MWFrontendAuthoringValidation
{
	// Generation dependencies must be valid before a new root asset is constructed or saved.
	inline bool CheckWidgetClass(const FClassProperty* Property, UClass* Value, UClass* RequiredBase, FString& Error)
	{
		if (!Property || !Property->MetaClass)
		{
			Error = TEXT("Required widget class property is missing or has an invalid reflected type");
			return false;
		}
		if (!IsValid(Value) || !RequiredBase || !Value->IsChildOf(RequiredBase) || !Value->IsChildOf(Property->MetaClass))
		{
			Error = FString::Printf(TEXT("%s has a missing or incompatible widget class: %s"), *Property->GetName(), *GetNameSafe(Value));
			return false;
		}
		return true;
	}

	inline bool CheckDefaults(UObject* Defaults, UClass* RequiredBase, FString& Error)
	{
		if (!IsValid(Defaults) || !Defaults->HasAnyFlags(RF_ClassDefaultObject) || !RequiredBase || !Defaults->IsA(RequiredBase))
		{
			Error = TEXT("Generated Blueprint default object is missing or incompatible");
			return false;
		}
		return true;
	}
}
