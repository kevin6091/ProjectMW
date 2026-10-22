#include "Data/MWInputData.h"

#include "InputAction.h"
#include "InputMappingContext.h"

const UInputAction* UMWInputData::FindInputActionByTag(const FGameplayTag InputTag) const
{
	if (!InputTag.IsValid()) return nullptr;
	const UInputAction* Match = nullptr;
	// 액션이 비어 있는 항목도 태그를 찾은 것으로 간주. 같은 태그가 다시 나오면 중복으로 거부해야 함.
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
	// 기존 오류는 지우지 않고, 이번 입력 설정을 검사하면서 추가한 오류만 성공 여부에 반영함.
	const int32 InitialErrors = OutErrors.Num();
	// 사용할 키 연결 설정과 입력 액션이 하나 이상 있어야 입력 모드로 사용할 수 있음.
	if (!InputMappingContext)
	{
		OutErrors.Add(FText::FromString(TEXT("InputMappingContext is required.")));
	}
	if (InputActions.IsEmpty())
	{
		OutErrors.Add(FText::FromString(TEXT("At least one InputAction is required.")));
	}
	// 태그가 중복되면 어떤 액션을 써야 할지 정할 수 없으므로 설정 오류로 처리함.
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
