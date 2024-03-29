// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "ParticleModuleSubUVMovie.generated.h"

UCLASS(editinlinenew, hidecategories=Object, hidecategories=SubUV, meta=(DisplayName = "SubUV Movie"))
class UParticleModuleSubUVMovie : public UParticleModuleSubUV
{
	GENERATED_UCLASS_BODY()

	/**
	 *	If true, use the emitter time to look up the frame rate.
	 *	If false (default), use the particle relative time.
	 */
	UPROPERTY(EditAnywhere, Category=Flipbook)
	uint32 bUseEmitterTime:1;

	/**
	 *	The frame rate the SubUV images should be 'flipped' thru at.
	 
	 */
	UPROPERTY(EditAnywhere, Category=Flipbook)
	struct FRawDistributionFloat FrameRate;

	/**
	 *	The starting image index for the SubUV (1 = the first frame).
	 *	Assumes order of Left->Right, Top->Bottom
	 *	If greater than the last frame, it will clamp to the last one.
	 *	If 0, then randomly selects a starting frame.
	 */
	UPROPERTY(EditAnywhere, Category=Flipbook)
	int32 StartingFrame;

	/** Initializes the default values for this property */
	void InitializeDefaults();

	//Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
#endif // WITH_EDITOR
	virtual void PostInitProperties() OVERRIDE;
	virtual void Serialize(FArchive& Ar) OVERRIDE;
	//End UObject Interface

	// Begin UParticleModule Interface
	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) OVERRIDE;
	virtual uint32 RequiredBytes(FParticleEmitterInstance* Owner = NULL) OVERRIDE;
	virtual void SetToSensibleDefaults(UParticleEmitter* Owner) OVERRIDE;
	virtual void GetCurveObjects(TArray<FParticleCurvePair>& OutCurves) OVERRIDE;
	// End UParticleModule Interface
	
	// Begin UParticleModuleSubUV Interface
	virtual float DetermineImageIndex(FParticleEmitterInstance* Owner, int32 Offset, FBaseParticle* Particle, 
		EParticleSubUVInterpMethod InterpMethod, FFullSubUVPayload& SubUVPayload, float DeltaTime) OVERRIDE;
	// End UParticleModuleSubUV Interface
	
};



