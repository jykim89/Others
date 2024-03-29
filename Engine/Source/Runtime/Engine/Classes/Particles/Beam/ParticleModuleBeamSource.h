// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/**
 *	ParticleModuleBeamSource
 *
 *	This module implements a single source for a beam emitter.
 *
 */

#pragma once
#include "ParticleModuleBeamSource.generated.h"

UCLASS(editinlinenew, hidecategories=Object, MinimalAPI, meta=(DisplayName = "Source"))
class UParticleModuleBeamSource : public UParticleModuleBeamBase
{
	GENERATED_UCLASS_BODY()

	/** The method flag. */
	UPROPERTY(EditAnywhere, Category=Source)
	TEnumAsByte<enum Beam2SourceTargetMethod> SourceMethod;

	/** The strength of the tangent from the source point for each beam. */
	UPROPERTY(EditAnywhere, Category=Source)
	FName SourceName;

	/** Whether to treat the as an absolute position in world space. */
	UPROPERTY(EditAnywhere, Category=Source)
	uint32 bSourceAbsolute:1;

	/** Default source-point to use. */
	UPROPERTY(EditAnywhere, Category=Source)
	struct FRawDistributionVector Source;

	/** Whether to lock the source to the life of the particle. */
	UPROPERTY(EditAnywhere, Category=Source)
	uint32 bLockSource:1;

	/** The method to use for the source tangent. */
	UPROPERTY(EditAnywhere, Category=Source)
	TEnumAsByte<enum Beam2SourceTargetTangentMethod> SourceTangentMethod;

	/** The tangent for the source point for each beam. */
	UPROPERTY(EditAnywhere, Category=Source)
	struct FRawDistributionVector SourceTangent;

	/** Whether to lock the source to the life of the particle. */
	UPROPERTY(EditAnywhere, Category=Source)
	uint32 bLockSourceTangent:1;

	/** The strength of the tangent from the source point for each beam. */
	UPROPERTY(EditAnywhere, Category=Source)
	struct FRawDistributionFloat SourceStrength;

	/** Whether to lock the source to the life of the particle. */
	UPROPERTY(EditAnywhere, Category=Source)
	uint32 bLockSourceStength:1;

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
	virtual void AutoPopulateInstanceProperties(UParticleSystemComponent* PSysComp) OVERRIDE;
	virtual void GetParticleSysParamsUtilized(TArray<FString>& ParticleSysParamList) OVERRIDE;
	//End UParticleModule Interface


	// @todo document
	void GetDataPointers(FParticleEmitterInstance* Owner, const uint8* ParticleBase, 
				int32& CurrentOffset, 
				FBeamParticleSourceTargetPayloadData*& ParticleSource,
				FBeamParticleSourceBranchPayloadData*& BranchSource);

	// @todo document
	void GetDataPointerOffsets(FParticleEmitterInstance* Owner, const uint8* ParticleBase, 
				int32& CurrentOffset, int32& ParticleSourceOffset, int32& BranchSourceOffset);
						

	// @todo document
	bool ResolveSourceData(FParticleBeam2EmitterInstance* BeamInst, 
				FBeam2TypeDataPayload* BeamData, const uint8* ParticleBase, 
				int32& CurrentOffset, int32	ParticleIndex, bool bSpawning,
				FBeamParticleModifierPayloadData* ModifierData);
};



