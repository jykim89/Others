// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "ParticleModuleLocationSkelVertSurface.generated.h"

UENUM()
enum ELocationSkelVertSurfaceSource
{
	VERTSURFACESOURCE_Vert UMETA(DisplayName="Verticies"),
	VERTSURFACESOURCE_Surface UMETA(DisplayName="Surfaces"),
	VERTSURFACESOURCE_MAX,
};

UCLASS(editinlinenew, hidecategories=Object, meta=(DisplayName = "Skel Vert/Surf Location"))
class UParticleModuleLocationSkelVertSurface : public UParticleModuleLocationBase
{
	GENERATED_UCLASS_BODY()

	/**
	 *	Whether the module uses Verts or Surfaces for locations.
	 *
	 *	VERTSURFACESOURCE_Vert		- Use Verts as the source locations.
	 *	VERTSURFACESOURCE_Surface	- Use Surfaces as the source locations.
	 */
	UPROPERTY(EditAnywhere, Category=VertSurface)
	TEnumAsByte<enum ELocationSkelVertSurfaceSource> SourceType;

	/** An offset to apply to each vert/surface */
	UPROPERTY(EditAnywhere, Category=VertSurface)
	FVector UniversalOffset;

	/** If true, update the particle locations each frame with that of the vert/surface */
	UPROPERTY(EditAnywhere, Category=VertSurface)
	uint32 bUpdatePositionEachFrame:1;

	/** If true, rotate mesh emitter meshes to orient w/ the vert/surface */
	UPROPERTY(EditAnywhere, Category=VertSurface)
	uint32 bOrientMeshEmitters:1;

	/** If true, particles inherit the associated bone velocity when spawned */
	UPROPERTY(EditAnywhere, Category=VertSurface)
	uint32 bInheritBoneVelocity:1;

	/**
	 *	The parameter name of the skeletal mesh actor that supplies the SkelMeshComponent for in-game.
	 */
	UPROPERTY(EditAnywhere, Category=VertSurface)
	FName SkelMeshActorParamName;

#if WITH_EDITORONLY_DATA
	/** The name of the skeletal mesh to use in the editor */
	UPROPERTY(EditAnywhere, Category=VertSurface)
	class USkeletalMesh* EditorSkelMesh;

#endif // WITH_EDITORONLY_DATA
	/** This module will only spawn from verts or surfaces associated with the bones in this list */
	UPROPERTY(EditAnywhere, Category=VertSurface)
	TArray<FName> ValidAssociatedBones;

	/** When true use the RestrictToNormal and NormalTolerance values to check surface normals */
	UPROPERTY(EditAnywhere, Category=VertSurface)
	uint32 bEnforceNormalCheck:1;

	/** Use this normal to restrict spawning locations */
	UPROPERTY(EditAnywhere, Category=VertSurface)
	FVector NormalToCompare;

	/** Normal tolerance.  0 degrees means it must be an exact match, 180 degrees means it can be any angle. */
	UPROPERTY(EditAnywhere, Category=VertSurface)
	float NormalCheckToleranceDegrees;

	/** Normal tolerance.  Value between 1.0 and -1.0 with 1.0 being exact match, 0.0 being everything up to
		perpendicular and -1.0 being any direction or don't restrict at all. */
	UPROPERTY()
	float NormalCheckTolerance;

	/**
	 *	Array of material indices that are valid materials to spawn from.
	 *	If empty, any material will be considered valid
	 */
	UPROPERTY(EditAnywhere, Category=VertSurface)
	TArray<int32> ValidMaterialIndices;


	// Begin UObject Interface
	virtual void PostLoad() OVERRIDE;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
#endif // WITH_EDITOR
	// End UObject Interface

	//Begin UParticleModule Interface
	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) OVERRIDE;
	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) OVERRIDE;
	virtual void FinalUpdate(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) OVERRIDE;
	virtual uint32	PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData) OVERRIDE;
	virtual uint32	RequiredBytes(FParticleEmitterInstance* Owner = NULL) OVERRIDE;
	virtual uint32	RequiredBytesPerInstance(FParticleEmitterInstance* Owner = NULL) OVERRIDE;
	virtual bool	TouchesMeshRotation() const OVERRIDE { return true; }
	virtual void	AutoPopulateInstanceProperties(UParticleSystemComponent* PSysComp) OVERRIDE;
	virtual bool CanTickInAnyThread() OVERRIDE
	{
		return false;
	}
#if WITH_EDITOR
	virtual int32 GetNumberOfCustomMenuOptions() const OVERRIDE;
	virtual bool GetCustomMenuEntryDisplayString(int32 InEntryIndex, FString& OutDisplayString) const OVERRIDE;
	virtual bool PerformCustomMenuEntry(int32 InEntryIndex) OVERRIDE;
#endif
	//End UParticleModule Interface

	/**
	 *	Retrieve the skeletal mesh component source to use for the current emitter instance.
	 *
	 *	@param	Owner						The particle emitter instance that is being setup
	 *
	 *	@return	USkeletalMeshComponent*		The skeletal mesh component to use as the source
	 */
	USkeletalMeshComponent* GetSkeletalMeshComponentSource(FParticleEmitterInstance* Owner);

	/**
	 *	Retrieve the position for the given socket index.
	 *
	 *	@param	Owner					The particle emitter instance that is being setup
	 *	@param	InSkelMeshComponent		The skeletal mesh component to use as the source
	 *	@param	InPrimaryVertexIndex	The index of the only vertex (vert mode) or the first vertex (surface mode)
	 *	@param	OutPosition				The position for the particle location
	 *	@param	OutRotation				Optional orientation for the particle (mesh emitters)
	 *  @param  bSpawning				When true and when using normal check on surfaces, will return false if the check fails.
	 *	
	 *	@return	bool					true if successful, false if not
	 */
	bool GetParticleLocation(FParticleEmitterInstance* Owner, USkeletalMeshComponent* InSkelMeshComponent, int32 InPrimaryVertexIndex, FVector& OutPosition, FQuat* OutRotation, bool bSpawning = false);

	/**
	 *  Check to see if the vert is influenced by a bone on our approved list.
	 *
	 *	@param	Owner					The particle emitter instance that is being setup
	 *	@param	InSkelMeshComponent		The skeletal mesh component to use as the source
	 *  @param  InVertexIndex			The vertex index of the vert to check.
	 *  @param	OutBoneIndex			Optional return of matching bone index
	 *
	 *  @return bool					true if it is influenced by an approved bone, false otherwise.
	 */
	bool VertInfluencedByActiveBone(FParticleEmitterInstance* Owner, USkeletalMeshComponent* InSkelMeshComponent, int32 InVertexIndex, int32* OutBoneIndex = NULL);

	/**
	 *	Updates the indices list with the bone index for each named bone in the editor exposed values.
	 *	
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 */
	void UpdateBoneIndicesList(FParticleEmitterInstance* Owner);

private:
	/** Helper function for concrete types. */
	template<bool bExtraBoneInfluencesT>
	bool VertInfluencedByActiveBoneTyped(bool bSoftVertex, FStaticLODModel& Model, const FSkelMeshChunk& Chunk, int32 VertIndex, USkeletalMeshComponent* InSkelMeshComponent, FModuleLocationVertSurfaceInstancePayload* InstancePayload, int32* OutBoneIndex);
};
