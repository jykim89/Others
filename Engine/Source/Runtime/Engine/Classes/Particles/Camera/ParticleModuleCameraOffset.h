// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "ParticleModuleCameraOffset.generated.h"

/**
 *	The update method for the offset
 */
UENUM()
enum EParticleCameraOffsetUpdateMethod
{
	EPCOUM_DirectSet UMETA(DisplayName="Direct Set"),
	EPCOUM_Additive UMETA(DisplayName="Additive"),
	EPCOUM_Scalar UMETA(DisplayName="Scalar"),
	EPCOUM_MAX,
};

UCLASS(editinlinenew, hidecategories=Object, meta=(DisplayName = "Camera Offset"))
class UParticleModuleCameraOffset : public UParticleModuleCameraBase
{
	GENERATED_UCLASS_BODY()

	/** 
	 *	The camera-relative offset to apply to sprite location
	 */
	UPROPERTY(EditAnywhere, Category=Camera)
	struct FRawDistributionFloat CameraOffset;

	/** If true, the offset will only be processed at spawn time */
	UPROPERTY(EditAnywhere, Category=Camera)
	uint32 bSpawnTimeOnly:1;

	/**
	 * How to update the offset for this module.
	 * DirectSet - Set the value directly (overwrite any previous setting)
	 * Additive  - Add the offset of this module to the existing offset
	 * Scalar    - Scale the existing offset by the value of this module
	 */
	UPROPERTY(EditAnywhere, Category=Camera)
	TEnumAsByte<enum EParticleCameraOffsetUpdateMethod> UpdateMethod;

	/** Initializes the default values for this property */
	void InitializeDefaults();

	//Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
#endif // WITH_EDITOR
	virtual void PostInitProperties() OVERRIDE;
	virtual void Serialize(FArchive& Ar) OVERRIDE;
	//End UObject Interface

	//Begin UParticleModule Interface
	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) OVERRIDE;
	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) OVERRIDE;
	virtual uint32 RequiredBytes(FParticleEmitterInstance* Owner = NULL) OVERRIDE;
	//End UParticleModule Interface
};

