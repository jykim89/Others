// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "ParticleModuleAccelerationBase.generated.h"

UCLASS(editinlinenew, hidecategories=Object, meta=(DisplayName = "Acceleration"), abstract)
class UParticleModuleAccelerationBase : public UParticleModule
{
	GENERATED_UCLASS_BODY()

	/**
	 *	If true, then treat the acceleration as world-space
	 */
	UPROPERTY(EditAnywhere, Category=Acceleration)
	uint32 bAlwaysInWorldSpace:1;


	// Begin UParticleModule Interface
	virtual void SetToSensibleDefaults(UParticleEmitter* Owner) OVERRIDE;
	// End UParticleModule Interface
};

