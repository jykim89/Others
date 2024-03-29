// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "ParticleModuleVelocityCone.generated.h"

UCLASS(editinlinenew, hidecategories=Object, meta=(DisplayName = "Velocity Cone"))
class UParticleModuleVelocityCone : public UParticleModuleVelocityBase
{
	GENERATED_UCLASS_BODY()

	/** The Min value represents the inner cone angle value and the Max value represents the outer cone angle value. */
	UPROPERTY(EditAnywhere, Category=Cone)
	struct FRawDistributionFloat Angle;

	/** The initial velocity of the particles. */
	UPROPERTY(EditAnywhere, Category=Cone)
	struct FRawDistributionFloat Velocity;

	/** The direction FVector of the cone. */
	UPROPERTY(EditAnywhere, Category=Cone)
	FVector Direction;

	/** Initializes the default values for this property */
	void InitializeDefaults();

	// Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
#endif // WITH_EDITOR
	virtual void PostInitProperties() OVERRIDE;
	virtual void Serialize(FArchive& Ar) OVERRIDE;
	// End UObject Interface

	// Begin UParticleModule Interface
	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) OVERRIDE;
	virtual void	Render3DPreview(FParticleEmitterInstance* Owner, const FSceneView* View,FPrimitiveDrawInterface* PDI) OVERRIDE;
	// Begin UParticleModule Interface
	
	/**
	 *	Extended version of spawn, allows for using a random stream for distribution value retrieval
	 *
	 *	@param	Owner				The particle emitter instance that is spawning
	 *	@param	Offset				The offset to the modules payload data
	 *	@param	SpawnTime			The time of the spawn
	 *	@param	InRandomStream		The random stream to use for retrieving random values
	 */
	void			SpawnEx(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, class FRandomStream* InRandomStream, FBaseParticle* ParticleBase);

};



