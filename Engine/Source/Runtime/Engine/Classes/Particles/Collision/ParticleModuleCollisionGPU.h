// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ParticleModuleCollisionGPU.generated.h"

/**
 * How particles respond to collision events.
 */
UENUM()
namespace EParticleCollisionResponse
{
	enum Type
	{
		/** The particle will bounce off of the surface. */
		Bounce,
		/** The particle will stop on the surface. */
		Stop,
		/** The particle will be killed. */
		Kill
	};
}

UCLASS(editinlinenew, hidecategories=Object, MinimalAPI, meta=(DisplayName = "Collision (Scene Depth)"))
class UParticleModuleCollisionGPU : public UParticleModuleCollisionBase
{
	GENERATED_UCLASS_BODY()

	/**
	 * Dampens the velocity of a particle in the direction normal to the
	 * collision plane.
	 */
	UPROPERTY(EditAnywhere, Category=Collision, meta=(ToolTip="The bounciness of the particle."))
	struct FRawDistributionFloat Resilience;

	/**
	 * Modulates the resilience of the particle over its lifetime.
	 */
	UPROPERTY(EditAnywhere, Category=Collision, meta=(ToolTip="Scales the bounciness of the particle over its life."))
	struct FRawDistributionFloat ResilienceScaleOverLife;

	/** 
	 * Friction applied to all particles during a collision or while moving
	 * along a surface.
	 */
	UPROPERTY(EditAnywhere, Category=Collision)
	float Friction;

	/**
	 * Scale applied to the size of the particle to obtain the collision radius.
	 */
	UPROPERTY(EditAnywhere, Category=Collision)
	float RadiusScale;

	/**
	 * Bias applied to the collision radius.
	 */
	UPROPERTY(EditAnywhere, Category=Collision)
	float RadiusBias;

	/**
	 * How particles respond to a collision event.
	 */
	UPROPERTY(EditAnywhere, Category=Collision)
	TEnumAsByte<EParticleCollisionResponse::Type> Response;

	/** Initializes the default values for this property */
	void InitializeDefaults();

	//Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
#endif // WITH_EDITOR
	virtual void PostInitProperties() OVERRIDE;
	//End UObject Interface

	//Begin UParticleModule Interface
	virtual void SetToSensibleDefaults(UParticleEmitter* Owner) OVERRIDE;
	virtual void CompileModule(struct FParticleEmitterBuildInfo& EmitterInfo) OVERRIDE;
#if WITH_EDITOR
	virtual bool IsValidForLODLevel(UParticleLODLevel* LODLevel, FString& OutErrorString) OVERRIDE;
#endif
	//End UParticleModule Interface

protected:
	friend class FParticleModuleCollisionGPUDetails;
};



