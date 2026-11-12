#include "Gameplay/Vehicle/MWVehicleNetworkTypes.h"

bool FMWVehicleDriveSnapshot::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Ar << StreamId;
	Ar << Revision;
	Ar << LastProcessedMoveId;
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
	Ar << ImpactYawRate;
	Ar << ServerTimeSeconds;
	bOutSuccess = bLocationSuccess && bVelocitySuccess && !Ar.IsError();
	return true;
}
