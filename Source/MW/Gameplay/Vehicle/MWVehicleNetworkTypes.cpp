#include "Gameplay/Vehicle/MWVehicleNetworkTypes.h"

bool FMWVehicleDriveSnapshot::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Ar << StreamId;
	Ar << Revision;
	Ar << LastProcessedMoveId;
	uint8 Allowed = bDriveAllowed ? 1 : 0;
	Ar.SerializeBits(&Allowed, 1);
	if (Ar.IsLoading())
	{
		bDriveAllowed = Allowed != 0;
	}
	uint8 FinishCollisionDisabled = bFinishCollisionDisabled ? 1 : 0;
	Ar.SerializeBits(&FinishCollisionDisabled, 1);
	if (Ar.IsLoading())
	{
		bFinishCollisionDisabled = FinishCollisionDisabled != 0;
	}
	Ar << RoundEpoch;
	bool bLocationSuccess = false;
	bool bVelocitySuccess = false;
	Location.NetSerialize(Ar, Map, bLocationSuccess);
	Rotation.SerializeCompressedShort(Ar);
	Velocity.NetSerialize(Ar, Map, bVelocitySuccess);
	uint8 Grounded = bGrounded ? 1 : 0;
	Ar.SerializeBits(&Grounded, 1);
	if (Ar.IsLoading())
	{
		bGrounded = Grounded != 0;
	}
	bool bGroundNormalSuccess = false;
	GroundNormal.NetSerialize(Ar, Map, bGroundNormalSuccess);
	Ar << ImpactYawRate;
	Ar << ServerTimeSeconds;
	bOutSuccess = bLocationSuccess && bVelocitySuccess && bGroundNormalSuccess && !Ar.IsError();
	return true;
}
