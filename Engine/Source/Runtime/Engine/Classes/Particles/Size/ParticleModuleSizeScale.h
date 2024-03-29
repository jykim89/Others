// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "ParticleModuleSizeScale.generated.h"

UCLASS(editinlinenew, hidecategories=Object, MinimalAPI, meta=(DisplayName = "Size Scale"))
class UParticleModuleSizeScale : public UParticleModuleSizeBase
{
	GENERATED_UCLASS_BODY()

	/**
	 *	The amount the BaseSize should be scaled before being used as the size of the particle. 
	 *	The value is retrieved using the RelativeTime of the particle during its update.
	 *	NOTE: this module overrides any size adjustments made prior to this module in that frame.
	 */
	UPROPERTY(EditAnywhere, Category=ParticleModuleSizeScale)
	struct FRawDistributionVector SizeScale;

	/** Ignored */
	UPROPERTY(EditAnywhere, Category=ParticleModuleSizeScale)
	uint32 EnableX:1;

	/** Ignored */
	UPROPERTY(EditAnywhere, Category=ParticleModuleSizeScale)
	uint32 EnableY:1;

	/** Ignored */
	UPROPERTY(EditAnywhere, Category=ParticleModuleSizeScale)
	uint32 EnableZ:1;

	/** Initializes the default values for this property */
	void InitializeDefaults();

	//Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
#endif // WITH_EDITOR
	virtual void PostInitProperties() OVERRIDE;
	virtual void Serialize(FArchive& Ar) OVERRIDE;
	// End UObject Interface

	// Begin UParticleModule Interface
	virtual void CompileModule( struct FParticleEmitterBuildInfo& EmitterInfo ) OVERRIDE;
	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) OVERRIDE;
	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) OVERRIDE;
	virtual void SetToSensibleDefaults(UParticleEmitter* Owner) OVERRIDE;

#if WITH_EDITOR
	virtual bool IsValidForLODLevel(UParticleLODLevel* LODLevel, FString& OutErrorString) OVERRIDE;
#endif

	// End UParticleModule Interface

protected:
	friend class FParticleModuleSizeScaleDetails;
};



