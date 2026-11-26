#include "Data/MWItemDefinitionData.h"

#include "System/MWAssetManager.h"

UMWItemDefinitionData* UMWItemDefinitionData::FindById(EMWItemId Id)
{
	if (Id == EMWItemId::None)
	{
		return nullptr;
	}
	const FString Name = StaticEnum<EMWItemId>()->GetNameStringByValue(static_cast<int64>(Id));
	if (Name.IsEmpty())
	{
		return nullptr;
	}

	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*FString::Printf(TEXT("Asset.Item.%s"), *Name)), false);
	UMWItemDefinitionData* Definition = Tag.IsValid()
		? UMWAssetManager::Get().LoadAssetSynchronous<UMWItemDefinitionData>(Tag) : nullptr;
	return Definition && Definition->ItemId == Id && Definition->HasValidDefinition() ? Definition : nullptr;
}

bool UMWItemDefinitionData::HasValidDefinition() const
{
	const auto IsPositive = [](float Value) { return FMath::IsFinite(Value) && Value > 0.f; };
	const bool bHasMountedSequence = !Presentation.VisualClass.IsNull() && !Presentation.Attach.IsNull()
		&& !Presentation.Use.IsNull() && !Presentation.Detach.IsNull();
	switch (ItemId)
	{
	case EMWItemId::Booster:
		return IsPositive(SpeedBoost.Duration)
			&& FMath::IsFinite(SpeedBoost.SpeedMultiplier) && SpeedBoost.SpeedMultiplier >= 1.f
			&& FMath::IsFinite(SpeedBoost.AccelerationMultiplier) && SpeedBoost.AccelerationMultiplier >= 1.f;
	case EMWItemId::Hammer:
		return bHasMountedSequence && IsPositive(AreaHit.ImpactDelay) && IsPositive(AreaHit.UseDuration)
			&& AreaHit.UseDuration >= AreaHit.ImpactDelay
			&& IsPositive(AreaHit.Radius) && IsPositive(AreaHit.HalfHeight) && IsPositive(AreaHit.KnockbackSpeed)
			&& FMath::IsFinite(AreaHit.ControlLockDuration) && AreaHit.ControlLockDuration >= 0.f
			&& FMath::IsFinite(AreaHit.SpinDegrees) && AreaHit.SpinDegrees >= 0.f;
	case EMWItemId::None:
		return false;
	default:
		return bHasMountedSequence;
	}
}
