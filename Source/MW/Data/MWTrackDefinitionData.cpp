#include "MWTrackDefinitionData.h"

#include "GameFramework/Actor.h"
#include "UObject/Class.h"

bool UMWTrackDefinitionData::ValidateDefinition(FText& OutError) const
{
	OutError = FText::GetEmpty();

	const UClass* ActorClass = TrackActorClass.Get();

	if (!ActorClass)
	{
		OutError = FText::FromString(
			TEXT("트랙 Actor 클래스가 지정되지 않았습니다."));
		return false;
	}

	if (ActorClass->HasAnyClassFlags(CLASS_Abstract))
	{
		OutError = FText::FromString(
			TEXT("추상 클래스는 트랙으로 생성할 수 없습니다."));
		return false;
	}

	return true;
}