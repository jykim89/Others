// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "ParticleModuleRotation_Seeded.generated.h"

UCLASS(editinlinenew, hidecategories=Object, meta=(DisplayName = "Init Rotation (Seed)"))
class UParticleModuleRotation_Seeded : public UParticleModuleRotation
{
	GENERATED_UCLASS_BODY()

	/** The random seed(s) to use for looking up values in StartLocation */
	UPROPERTY(EditAnywhere, Category=RandomSeed)
	struct FParticleRandomSeedInfo RandomSeedInfo;


	//Begin UParticleModule Interface
	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) OVERRIDE;
	virtual uint32	RequiredBytesPerInstance(FParticleEmitterInstance* Owner = NULL) OVERRIDE;
	virtual uint32	PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData) OVERRIDE;
	virtual FParticleRandomSeedInfo* GetRandomSeedInfo() OVERRIDE
	{
		return &RandomSeedInfo;
	}
	virtual void EmitterLoopingNotify(FParticleEmitterInstance* Owner) OVERRIDE;
	//End UParticleModule Interface
};



