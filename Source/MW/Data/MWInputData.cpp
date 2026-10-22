#include "Data/MWInputData.h"

#include "InputAction.h"
#include "InputMappingContext.h"

const UInputAction* UMWInputData::FindInputActionByTag(const FGameplayTag InputTag) const
{
	if (!InputTag.IsValid()) return nullptr;
	const UInputAction* Match = nullptr;
	bool bFound = false;
	for (const FMWInputAction& Entry : InputActions)
	{
		if (Entry.InputTag == InputTag)
		{
			if (bFound) return nullptr;
			bFound = true;
			Match = Entry.InputAction;
		}
	}
	return Match;
}

bool UMWInputData::ValidateInputData(TArray<FText>& OutErrors) const
{
	const int32 InitialErrors = OutErrors.Num();
	if (!InputMappingContext)
	{
		OutErrors.Add(FText::FromString(TEXT("InputMappingContext is required.")));
	}
	if (InputActions.IsEmpty())
	{
		OutErrors.Add(FText::FromString(TEXT("At least one InputAction is required.")));
	}
	TSet<FGameplayTag> SeenTags;
	for (int32 Index = 0; Index < InputActions.Num(); ++Index)
	{
		const FMWInputAction& Entry = InputActions[Index];
		if (!Entry.InputTag.IsValid() || SeenTags.Contains(Entry.InputTag))
		{
			OutErrors.Add(FText::FromString(FString::Printf(TEXT("InputActions[%d]: missing or duplicate tag '%s'."),
				Index, *Entry.InputTag.ToString())));
		}
		SeenTags.Add(Entry.InputTag);
		if (!Entry.InputAction)
		{
			OutErrors.Add(FText::FromString(FString::Printf(TEXT("InputActions[%d]: InputAction is required."), Index)));
		}
	}
	return OutErrors.Num() == InitialErrors;
}
