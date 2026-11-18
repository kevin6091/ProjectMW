#include "UI/Common/MWParticipantPalette.h"

FLinearColor UMWParticipantPalette::GetColorForMember(int32 MemberId) const
{
    return MemberId > 0 && !Colors.IsEmpty() ? GetColorForSlot((MemberId - 1) % Colors.Num()) : FallbackColor;
}

FLinearColor UMWParticipantPalette::GetColorForSlot(int32 SlotIndex) const
{
    return Colors.IsValidIndex(SlotIndex) ? Colors[SlotIndex] : FallbackColor;
}
