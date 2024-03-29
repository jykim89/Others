// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnParticleEmitterInstances.h: 
	Particle emitter instance definitions/ macros.
=============================================================================*/

#pragma once

#include "Engine.h"
#include "ParticleHelper.h"

/*-----------------------------------------------------------------------------
	Forward declarations
-----------------------------------------------------------------------------*/
//	Emitter and module types
class UParticleEmitter;
class UParticleSpriteEmitter;
class UParticleModule;
// Data types
class UParticleModuleTypeDataMesh;
class UParticleModuleTypeDataBeam2;
class UParticleModuleTypeDataRibbon;
class UParticleModuleTypeDataAnimTrail;

class UStaticMeshComponent;
class UParticleSystemComponent;

class UParticleModuleBeamSource;
class UParticleModuleBeamTarget;
class UParticleModuleBeamNoise;

class UParticleModuleTrailSource;
class UParticleModuleTrailSpawn;
class UParticleModuleTrailTaper;

class UParticleModuleSpawnPerUnit;

class UParticleModuleOrientationAxisLock;

class UParticleLODLevel;

class FParticleSystemSceneProxy;
class FParticleDynamicData;
struct FDynamicBeam2EmitterData;
struct FDynamicTrail2EmitterData;

struct FLODBurstFired
{
	TArray<bool> Fired;
};

/*-----------------------------------------------------------------------------
	Information compiled from modules to build runtime emitter data.
-----------------------------------------------------------------------------*/

struct FParticleEmitterBuildInfo
{
	/** The required module. */
	class UParticleModuleRequired* RequiredModule;
	/** The spawn module. */
	class UParticleModuleSpawn* SpawnModule;
	/** The spawn-per-unit module. */
	class UParticleModuleSpawnPerUnit* SpawnPerUnitModule;
	/** List of spawn modules that need to be invoked at runtime. */
	TArray<class UParticleModule*> SpawnModules;

	/** The accumulated orbit offset. */
	FComposableVectorDistribution OrbitOffset;
	/** The accumulated orbit initial rotation. */
	FComposableVectorDistribution OrbitInitialRotation;
	/** The accumulated orbit rotation rate. */
	FComposableVectorDistribution OrbitRotationRate;

	/** The color scale of a particle over time. */
	FComposableVectorDistribution ColorScale;
	/** The alpha scale of a particle over time. */
	FComposableFloatDistribution AlphaScale;

	/** An additional color scale for allowing parameters to be used for ColorOverLife modules. */
	FRawDistributionVector DynamicColor;
	/** An additional alpha scale for allowing parameters to be used for ColorOverLife modules. */
	FRawDistributionFloat DynamicAlpha;

	/** An additional color scale for allowing parameters to be used for ColorScaleOverLife modules. */
	FRawDistributionVector DynamicColorScale;
	/** An additional alpha scale for allowing parameters to be used for ColorScaleOverLife modules. */
	FRawDistributionFloat DynamicAlphaScale;
	
	/** How to scale a particle's size over time. */
	FComposableVectorDistribution SizeScale;
	/** The maximum size of a particle. */
	FVector2D MaxSize;
	/** How much to scale a particle's size based on its speed. */
	FVector2D SizeScaleBySpeed;
	/** The maximum amount by which to scale a particle based on its speed. */
	FVector2D MaxSizeScaleBySpeed;

	/** The sub-image index over the particle's life time. */
	FComposableFloatDistribution SubImageIndex;

	/** Drag coefficient. */
	FComposableFloatDistribution DragCoefficient;
	/** Drag scale over life. */
	FComposableFloatDistribution DragScale;

	/** Enable collision? */
	bool bEnableCollision;
	/** How particles respond to collision. */
	//EParticleCollisionResponse::Type CollisionResponse;
	uint8 CollisionResponse;
	/** Radius scale applied to friction. */
	float CollisionRadiusScale;
	/** Bias applied to the collision radius. */
	float CollisionRadiusBias;
	/** Friction. */
	float Friction;
	/** Collision damping factor. */
	FComposableFloatDistribution Resilience;
	/** Collision damping factor scale over life. */
	FComposableFloatDistribution ResilienceScaleOverLife;

	/** Location of a point source attractor. */
	FVector PointAttractorPosition;
	/** Radius of the point source attractor. */
	float PointAttractorRadius;
	/** Strength of the point attractor. */
	FComposableFloatDistribution PointAttractorStrength;

	/** The per-particle vector field scale. */
	FComposableFloatDistribution VectorFieldScale;
	/** The per-particle vector field scale-over-life. */
	FComposableFloatDistribution VectorFieldScaleOverLife;
	/** Global vector field scale. */
	float GlobalVectorFieldScale;
	/** Global vector field tightness. */
	float GlobalVectorFieldTightness;

	/** Local vector field. */
	class UVectorField* LocalVectorField;
	/** Local vector field transform. */
	FTransform LocalVectorFieldTransform;
	/** Local vector field intensity. */
	float LocalVectorFieldIntensity;
	/** Tightness tweak for local vector fields. */
	float LocalVectorFieldTightness;
	/** Minimum initial rotation applied to local vector fields. */
	FVector LocalVectorFieldMinInitialRotation;
	/** Maximum initial rotation applied to local vector fields. */
	FVector LocalVectorFieldMaxInitialRotation;
	/** Local vector field rotation rate. */
	FVector LocalVectorFieldRotationRate;

	/** Constant acceleration to apply to particles. */
	FVector ConstantAcceleration;

	/** The maximum lifetime of any particle that will spawn. */
	float MaxLifetime;
	/** The maximum rotation rate of particles. */
	float MaxRotationRate;
	/** The estimated maximum number of particles for this emitter. */
	int32 EstimatedMaxActiveParticleCount;

	int32 ScreenAlignment;

	/** An offset in UV space for the positioning of a sprites verticies. */
	FVector2D PivotOffset;

	/** If true, local vector fields ignore the component transform. */
	uint32 bLocalVectorFieldIgnoreComponentTransform : 1;
	/** Tile vector field in x axis? */
	uint32 bLocalVectorFieldTileX : 1;
	/** Tile vector field in y axis? */
	uint32 bLocalVectorFieldTileY : 1;
	/** Tile vector field in z axis? */
	uint32 bLocalVectorFieldTileZ : 1;

	/** Default constructor. */
	FParticleEmitterBuildInfo();
};

/*-----------------------------------------------------------------------------
	FParticleEmitterInstance
-----------------------------------------------------------------------------*/
struct ENGINE_API FParticleEmitterInstance
{
public:
	/** The maximum DeltaTime allowed for updating PeakActiveParticle tracking.
	 *	Any delta time > this value will not impact active particle tracking.
	 */
	static const float PeakActiveParticleUpdateDelta;

	/** The template this instance is based on.							*/
	UParticleSpriteEmitter* SpriteTemplate;
	/** The component who owns it.										*/
	UParticleSystemComponent* Component;
	/** The index of the currently set LOD level.						*/
	int32 CurrentLODLevelIndex;
	/** The currently set LOD level.									*/
	UParticleLODLevel* CurrentLODLevel;
	/** The offset to the TypeData payload in the particle data.		*/
	int32 TypeDataOffset;
	/** The offset to the TypeData instance payload.					*/
	int32 TypeDataInstanceOffset;
	/** The offset to the SubUV payload in the particle data.			*/
	int32 SubUVDataOffset;
	/** The offset to the dynamic parameter payload in the particle data*/
	int32 DynamicParameterDataOffset;
	/** Offset to the light module data payload.						*/
	int32 LightDataOffset;
	/** The offset to the Orbit module payload in the particle data.	*/
	int32 OrbitModuleOffset;
	/** The offset to the Camera payload in the particle data.			*/
	int32 CameraPayloadOffset;
	/** The location of the emitter instance							*/
	FVector Location;
	/** Transform from emitter local space to simulation space.			*/
	FMatrix EmitterToSimulation;
	/** Transform from simulation space to world space.					*/
	FMatrix SimulationToWorld;
	/** If true, kill this emitter instance when it is deactivated.		*/
	uint32 bKillOnDeactivate:1;
	/** if true, kill this emitter instance when it has completed.		*/
	uint32 bKillOnCompleted:1;
	/** Whether this emitter requires sorting as specified by artist.	*/
	uint32 bRequiresSorting:1;
	/** If true, halt spawning for this instance.						*/
	uint32 bHaltSpawning:1;
	/** If true, the emitter has modules that require loop notification.*/
	uint32 bRequiresLoopNotification:1;
	/** If true, the emitter ignores the component's scale. (Mesh emitters only). */
	uint32 bIgnoreComponentScale:1;
	/** Hack: Make sure this is a Beam type to avoid casting from/to wrong types. */
	uint32 bIsBeam:1;
	/** The sort mode to use for this emitter as specified by artist.	*/
	int32 SortMode;
	/** Pointer to the particle data array.								*/
	uint8* ParticleData;
	/** Pointer to the particle index array.							*/
	uint16* ParticleIndices;
	/** Map module pointers to their offset into the particle data.		*/
	TMap<UParticleModule*,uint32> ModuleOffsetMap;
	/** Pointer to the instance data array.								*/
	uint8* InstanceData;
	/** The size of the Instance data array.							*/
	int32 InstancePayloadSize;
	/** Map module pointers to their offset into the instance data.		*/
	TMap<UParticleModule*,uint32> ModuleInstanceOffsetMap;
	/** The offset to the particle data.								*/
	int32 PayloadOffset;
	/** The total size of a particle (in bytes).						*/
	int32 ParticleSize;
	/** The stride between particles in the ParticleData array.			*/
	int32 ParticleStride;
	/** The number of particles currently active in the emitter.		*/
	int32 ActiveParticles;
	/** Monotonically increasing counter. */
	uint32 ParticleCounter;
	/** The maximum number of active particles that can be held in 
	 *	the particle data array.
	 */
	int32 MaxActiveParticles;
	/** The fraction of time left over from spawning.					*/
	float SpawnFraction;
	/** The number of seconds that have passed since the instance was
	 *	created.
	 */
	float SecondsSinceCreation;
	/** */
	float EmitterTime;
	/** The amount of time simulated in the previous time step. */
	float LastDeltaTime;
	/** The previous location of the instance.							*/
	FVector OldLocation;
	/** The bounding box for the particles.								*/
	FBox ParticleBoundingBox;
	/** The BurstFire information.										*/
	TArray<struct FLODBurstFired> BurstFired;
	/** The number of loops completed by the instance.					*/
	int32 LoopCount;
	/** Flag indicating if the render data is dirty.					*/
	int32 IsRenderDataDirty;
	/** The AxisLock module - to avoid finding each Tick.				*/
	UParticleModuleOrientationAxisLock* Module_AxisLock;
	/** The current duration fo the emitter instance.					*/
	float EmitterDuration;
	/** The emitter duration at each LOD level for the instance.		*/
	TArray<float> EmitterDurations;
	/** The emitter's delay for the current loop		*/
	float CurrentDelay;


	/** The number of triangles to render								*/
	int32	TrianglesToRender;
	int32 MaxVertexIndex;

	/** The material to render this instance with.						*/
	UMaterialInterface* CurrentMaterial;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** Number of events this emitter has generated... */
	int32 EventCount;
	int32 MaxEventCount;
#endif	//#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	/** Position offset for each particle. Will be reset to zero at the end of the tick	*/
	FVector PositionOffsetThisTick;
	
	/** The PivotOffset applied to the vertex positions 			*/
	FVector2D PivotOffset;

	/** Constructor	*/
	FParticleEmitterInstance();

	/** Destructor	*/
	virtual ~FParticleEmitterInstance();

#if STATS
	void PreDestructorCall();
#endif

	//
	virtual void InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent, bool bClearResources = true);
	virtual void Init();

	/** @return The world that the component that owns this instance is in */
	UWorld* GetWorld() const;

	/**
	 * Ensures enough memory is allocated for the requested number of particles.
	 *
	 * @param NewMaxActiveParticles		The number of particles for which memory must be allocated.
	 * @param bSetMaxActiveCount		If true, update the peak active particles for this LOD.
	 * @returns bool					true if memory is allocated for at least NewMaxActiveParticles.
	 */
	virtual bool Resize(int32 NewMaxActiveParticles, bool bSetMaxActiveCount = true);

	virtual void Tick(float DeltaTime, bool bSuppressSpawning);
	/**
	 *	Tick sub-function that handles EmitterTime setup, looping, etc.
	 *
	 *	@param	DeltaTime			The current time slice
	 *	@param	CurrentLODLevel		The current LOD level for the instance
	 *
	 *	@return	float				The EmitterDelay
	 */
	virtual float Tick_EmitterTimeSetup(float DeltaTime, UParticleLODLevel* CurrentLODLevel);
	/**
	 *	Tick sub-function that handles spawning of particles
	 *
	 *	@param	DeltaTime			The current time slice
	 *	@param	CurrentLODLevel		The current LOD level for the instance
	 *	@param	bSuppressSpawning	true if spawning has been supressed on the owning particle system component
	 *	@param	bFirstTime			true if this is the first time the instance has been ticked
	 *
	 *	@return	float				The SpawnFraction remaining
	 */
	virtual float Tick_SpawnParticles(float DeltaTime, UParticleLODLevel* CurrentLODLevel, bool bSuppressSpawning, bool bFirstTime);
	/**
	 *	Tick sub-function that handles module updates
	 *
	 *	@param	DeltaTime			The current time slice
	 *	@param	CurrentLODLevel		The current LOD level for the instance
	 */
	virtual void Tick_ModuleUpdate(float DeltaTime, UParticleLODLevel* CurrentLODLevel);
	/**
	 *	Tick sub-function that handles module post updates
	 *
	 *	@param	DeltaTime			The current time slice
	 *	@param	CurrentLODLevel		The current LOD level for the instance
	 */
	virtual void Tick_ModulePostUpdate(float DeltaTime, UParticleLODLevel* CurrentLODLevel);
	/**
	 *	Tick sub-function that handles module FINAL updates
	 *
	 *	@param	DeltaTime			The current time slice
	 *	@param	CurrentLODLevel		The current LOD level for the instance
	 */
	virtual void Tick_ModuleFinalUpdate(float DeltaTime, UParticleLODLevel* CurrentLODLevel);

	/**
	 *	Set the LOD to the given index
	 *
	 *	@param	InLODIndex			The index of the LOD to set as current
	 *	@param	bInFullyProcess		If true, process burst lists, etc.
	 */
	virtual void SetCurrentLODIndex(int32 InLODIndex, bool bInFullyProcess);

	virtual void Rewind();
	virtual FBox GetBoundingBox();
	virtual void UpdateBoundingBox(float DeltaTime);
	virtual void ForceUpdateBoundingBox();
	virtual uint32 RequiredBytes();
	virtual uint8* GetModuleInstanceData(UParticleModule* Module);
	virtual uint8* GetTypeDataModuleInstanceData();
	virtual uint32 CalculateParticleStride(uint32 ParticleSize);
	virtual void ResetBurstList();
	virtual float GetCurrentBurstRateOffset(float& DeltaTime, int32& Burst);
	virtual void ResetParticleParameters(float DeltaTime);
	void CalculateOrbitOffset(FOrbitChainModuleInstancePayload& Payload, 
		FVector& AccumOffset, FVector& AccumRotation, FVector& AccumRotationRate, 
		float DeltaTime, FVector& Result, FMatrix& RotationMat);
	virtual void UpdateOrbitData(float DeltaTime);
	virtual void ParticlePrefetch();

	/**
	 *	Spawn particles for this emitter instance
	 *	@param	DeltaTime		The time slice to spawn over
	 *	@return	float			The leftover fraction of spawning
	 */
	virtual float Spawn(float DeltaTime);

	/**
	 * Spawn the indicated number of particles.
	 *
	 * @param Count The number of particles to spawn.
	 * @param StartTime			The local emitter time at which to begin spawning particles.
	 * @param Increment			The time delta between spawned particles.
	 * @param InitialLocation	The initial location of spawned particles.
	 * @param InitialVelocity	The initial velocity of spawned particles.
	 * @param EventPayload		Event generator payload if events should be triggered.
	 */
	void SpawnParticles( int32 Count, float StartTime, float Increment, const FVector& InitialLocation, const FVector& InitialVelocity, struct FParticleEventInstancePayload* EventPayload );

	/**
	 *	Spawn/burst the given particles...
	 *
	 *	@param	DeltaTime		The time slice to spawn over.
	 *	@param	InSpawnCount	The number of particles to forcibly spawn.
	 *	@param	InBurstCount	The number of particles to forcibly burst.
	 *	@param	InLocation		The location to spawn at.
	 *	@param	InVelocity		OPTIONAL velocity to have the particle inherit.
	 *
	 */
	virtual void ForceSpawn(float DeltaTime, int32 InSpawnCount, int32 InBurstCount, FVector& InLocation, FVector& InVelocity);
	void CheckSpawnCount(int32 InNewCount, int32 InMaxCount);

	/**
	 * Handle any pre-spawning actions required for particles
	 *
	 * @param Particle			The particle being spawned.
	 * @param InitialLocation	The initial location of the particle.
	 * @param InitialVelocity	The initial velocity of the particle.
	 */
	virtual void PreSpawn(FBaseParticle* Particle, const FVector& InitialLocation, const FVector& InitialVelocity);

	/**
	 * Handle any post-spawning actions required by the instance
	 *
	 * @param	Particle					The particle that was spawned
	 * @param	InterpolationPercentage		The percentage of the time slice it was spawned at
	 * @param	SpawnTIme					The time it was spawned at
	 */
	virtual void PostSpawn(FBaseParticle* Particle, float InterpolationPercentage, float SpawnTime);

	virtual bool HasCompleted();
	virtual void KillParticles();
	/**
	 *	Kill the particle at the given instance
	 *
	 *	@param	Index		The index of the particle to kill.
	 */
	virtual void KillParticle(int32 Index);

	/**
	 *	Force kill all particles in the emitter.
	 *
	 *	@param	bFireEvents		If true, fire events for the particles being killed.
	 */
	virtual void KillParticlesForced(bool bFireEvents = false);

	/** Set the HaltSpawning flag */
	virtual void SetHaltSpawning(bool bInHaltSpawning)
	{
		bHaltSpawning = bInHaltSpawning;
	}

	/** Get the offset of the orbit payload. */
	int32 GetOrbitPayloadOffset();

	/** Get the position of the particle taking orbit in to account. */
	FVector GetParticleLocationWithOrbitOffset(FBaseParticle* Particle);

	virtual FBaseParticle* GetParticle(int32 Index);
	/**
	 *	Get the physical index of the particle at the given index
	 *	(ie, the contents of ParticleIndices[InIndex])
	 *
	 *	@param	InIndex			The index of the particle of interest
	 *
	 *	@return	int32				The direct index of the particle
	 */
	FORCEINLINE int32 GetParticleDirectIndex(int32 InIndex)
	{
		if (InIndex < MaxActiveParticles)
		{
			return ParticleIndices[InIndex];
		}
		return -1;
	}
	/**
	 *	Get the particle at the given direct index
	 *
	 *	@param	InDirectIndex		The direct index of the particle of interest
	 *
	 *	@return	FBaseParticle*		The particle, if valid index was given; NULL otherwise
	 */
	virtual FBaseParticle* GetParticleDirect(int32 InDirectIndex);

	/**
	 *	Calculates the emitter duration for the instance.
	 */
	void SetupEmitterDuration();
	
	/**
	 * Returns whether the system has any active particles.
	 *
	 * @return true if there are active particles, false otherwise.
	 */
	bool HasActiveParticles()
	{
		return ActiveParticles > 0;
	}

	/**
	 *	Checks some common values for GetDynamicData validity
	 *
	 *	@return	bool		true if GetDynamicData should continue, false if it should return NULL
	 */
	virtual bool IsDynamicDataRequired(UParticleLODLevel* CurrentLODLevel);

	/**
	 *	Retrieves the dynamic data for the emitter
	 */
	virtual FDynamicEmitterDataBase* GetDynamicData(bool bSelected)
	{
		return NULL;
	}

	/**
	 *	Updates the dynamic data for the instance
	 *
	 *	@param	DynamicData		The dynamic data to fill in
	 *	@param	bSelected		true if the particle system component is selected
	 */
	virtual bool UpdateDynamicData(FDynamicEmitterDataBase* DynamicData, bool bSelected)
	{
		// Base class does nothing...
		return false;
	}

	/**
	 *	Retrieves replay data for the emitter
	 *
	 *	@return	The replay data, or NULL on failure
	 */
	virtual FDynamicEmitterReplayDataBase* GetReplayData()
	{
		return NULL;
	}

	/**
	 *	Retrieve the allocated size of this instance.
	 *
	 *	@param	OutNum			The size of this instance
	 *	@param	OutMax			The maximum size of this instance
	 */
	virtual void GetAllocatedSize(int32& OutNum, int32& OutMax)
	{
		OutNum = 0;
		OutMax = 0;
	}
	
	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 * @param	Mode	Specifies which resource size should be displayed. ( see EResourceSizeMode::Type )
	 * @return  Size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode)
	{
		return 0;
	}

	/**
	 *	Process received events.
	 */
	virtual void ProcessParticleEvents(float DeltaTime, bool bSuppressSpawning);

	/**
	 *	Called when the particle system is deactivating...
	 */
	virtual void OnDeactivateSystem()
	{
	}

	/**
	 * Returns the offset to the mesh rotation payload, if any.
	 */
	virtual int32 GetMeshRotationOffset() const
	{
		return 0;
	}

	/**
	 * Returns true if mesh rotation is active.
	 */
	virtual bool IsMeshRotationActive() const
	{
		return false;
	}

	/**
	 * Sets the materials with which mesh particles should be rendered.
	 * @param InMaterials - The materials.
	 */
	virtual void SetMeshMaterials( const TArray<UMaterialInterface*>& InMaterials )
	{
	}

	/**
	 * Gathers material relevance flags for this emitter instance.
	 * @param OutMaterialRelevance - Pointer to where material relevance flags will be stored.
	 * @param LODLevel - The LOD level for which to compute material relevance flags.
	 */
	virtual void GatherMaterialRelevance( FMaterialRelevance* OutMaterialRelevance, const UParticleLODLevel* LODLevel ) const;

	/**
	 * When an emitter is killed, this will check other emitters and clean up anything pointing to this one
	 */
	virtual void OnEmitterInstanceKilled(FParticleEmitterInstance* Instance)
	{

	}

	// Beam interface
	virtual void SetBeamEndPoint(FVector NewEndPoint) {};
	virtual void SetBeamSourcePoint(FVector NewSourcePoint,int32 SourceIndex) {};
	virtual void SetBeamSourceTangent(FVector NewTangentPoint,int32 SourceIndex) {};
	virtual void SetBeamSourceStrength(float NewSourceStrength,int32 SourceIndex) {};
	virtual void SetBeamTargetPoint(FVector NewTargetPoint,int32 TargetIndex) {};
	virtual void SetBeamTargetTangent(FVector NewTangentPoint,int32 TargetIndex) {};
	virtual void SetBeamTargetStrength(float NewTargetStrength,int32 TargetIndex) {};

	// Called on world origin changes
	virtual void ApplyWorldOffset(FVector InOffset, bool bWorldShift);
		
	virtual bool IsTrailEmitter()const{ return false; }
protected:

	/**
	 * Captures dynamic replay data for this particle system.
	 *
	 * @param	OutData		[Out] Data will be copied here
	 *
	 * @return Returns true if successful
	 */
	virtual bool FillReplayData( FDynamicEmitterReplayDataBase& OutData );

	/**
	 * Updates all internal transforms.
	 */
	void UpdateTransforms();

	/**
	 * Retrieves the current LOD level and asserts that it is valid.
	 */
	class UParticleLODLevel* GetCurrentLODLevelChecked();

	/**
	 * Get the current material to render with.
	 */
	UMaterialInterface* GetCurrentMaterial();

};

/*-----------------------------------------------------------------------------
	ParticleSpriteEmitterInstance
-----------------------------------------------------------------------------*/
struct FParticleSpriteEmitterInstance : public FParticleEmitterInstance
{
	/** Constructor	*/
	FParticleSpriteEmitterInstance();

	/** Destructor	*/
	virtual ~FParticleSpriteEmitterInstance();

	/**
	 *	Retrieves the dynamic data for the emitter
	 */
	virtual FDynamicEmitterDataBase* GetDynamicData(bool bSelected);

	/**
	 *	Updates the dynamic data for the instance
	 *
	 *	@param	DynamicData		The dynamic data to fill in
	 *	@param	bSelected		true if the particle system component is selected
	 */
	virtual bool UpdateDynamicData(FDynamicEmitterDataBase* DynamicData, bool bSelected);

	/**
	 *	Retrieves replay data for the emitter
	 *
	 *	@return	The replay data, or NULL on failure
	 */
	virtual FDynamicEmitterReplayDataBase* GetReplayData();

	/**
	 *	Retrieve the allocated size of this instance.
	 *
	 *	@param	OutNum			The size of this instance
	 *	@param	OutMax			The maximum size of this instance
	 */
	virtual void GetAllocatedSize(int32& OutNum, int32& OutMax);

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 * @param	Mode	Specifies which resource size should be displayed. ( see EResourceSizeMode::Type )
	 * @return  Size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode) OVERRIDE;

protected:

	/**
	 * Captures dynamic replay data for this particle system.
	 *
	 * @param	OutData		[Out] Data will be copied here
	 *
	 * @return Returns true if successful
	 */
	virtual bool FillReplayData( FDynamicEmitterReplayDataBase& OutData );

};

/*-----------------------------------------------------------------------------
	ParticleMeshEmitterInstance
-----------------------------------------------------------------------------*/
struct ENGINE_API FParticleMeshEmitterInstance : public FParticleEmitterInstance
{
	UParticleModuleTypeDataMesh* MeshTypeData;
	bool MeshRotationActive;
	int32 MeshRotationOffset;

	/** The materials to render this instance with.	*/
	TArray<UMaterialInterface*> CurrentMaterials;

	/** Constructor	*/
	FParticleMeshEmitterInstance();

	virtual void InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent, bool bClearResources = true);
	virtual void Init();
	virtual bool Resize(int32 NewMaxActiveParticles, bool bSetMaxActiveCount = true);
	virtual void Tick(float DeltaTime, bool bSuppressSpawning);
	virtual void UpdateBoundingBox(float DeltaTime);
	virtual uint32 RequiredBytes();
	virtual void PostSpawn(FBaseParticle* Particle, float InterpolationPercentage, float SpawnTime);
	virtual FDynamicEmitterDataBase* GetDynamicData(bool bSelected);
	virtual bool IsDynamicDataRequired(UParticleLODLevel* CurrentLODLevel) OVERRIDE;

	/**
	 *	Updates the dynamic data for the instance
	 *
	 *	@param	DynamicData		The dynamic data to fill in
	 *	@param	bSelected		true if the particle system component is selected
	 */
	virtual bool UpdateDynamicData(FDynamicEmitterDataBase* DynamicData, bool bSelected);

	/**
	 *	Retrieves replay data for the emitter
	 *
	 *	@return	The replay data, or NULL on failure
	 */
	virtual FDynamicEmitterReplayDataBase* GetReplayData();

	/**
	 *	Retrieve the allocated size of this instance.
	 *
	 *	@param	OutNum			The size of this instance
	 *	@param	OutMax			The maximum size of this instance
	 */
	virtual void GetAllocatedSize(int32& OutNum, int32& OutMax);

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 * @param	Mode	Specifies which resource size should be displayed. ( see EResourceSizeMode::Type )
	 * @return  Size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode) OVERRIDE;

	/**
	 * Returns the offset to the mesh rotation payload, if any.
	 */
	virtual int32 GetMeshRotationOffset() const OVERRIDE
	{
		return MeshRotationOffset;
	}

	/**
	 * Returns true if mesh rotation is active.
	 */
	virtual bool IsMeshRotationActive() const OVERRIDE
	{
		return MeshRotationActive;
	}

	/**
	 * Sets the materials with which mesh particles should be rendered.
	 * @param InMaterials - The materials.
	 */
	virtual void SetMeshMaterials( const TArray<UMaterialInterface*>& InMaterials ) OVERRIDE;

	/**
	 * Gathers material relevance flags for this emitter instance.
	 * @param OutMaterialRelevance - Pointer to where material relevance flags will be stored.
	 * @param LODLevel - The LOD level for which to compute material relevance flags.
	 */
	virtual void GatherMaterialRelevance( FMaterialRelevance* OutMaterialRelevance, const UParticleLODLevel* LODLevel ) const OVERRIDE;

	/**
	 * Gets the materials applied to each section of a mesh.
	 */
	void GetMeshMaterials(TArray<UMaterialInterface*,TInlineAllocator<2> >& OutMaterials, const UParticleLODLevel* LODLevel) const;

protected:

	/**
	 * Captures dynamic replay data for this particle system.
	 *
	 * @param	OutData		[Out] Data will be copied here
	 *
	 * @return Returns true if successful
	 */
	virtual bool FillReplayData( FDynamicEmitterReplayDataBase& OutData );
};

/*-----------------------------------------------------------------------------
	ParticleBeam2EmitterInstance
-----------------------------------------------------------------------------*/
struct FParticleBeam2EmitterInstance : public FParticleEmitterInstance
{
	UParticleModuleTypeDataBeam2*	BeamTypeData;
	UParticleModuleBeamSource*		BeamModule_Source;
	UParticleModuleBeamTarget*		BeamModule_Target;
	UParticleModuleBeamNoise*		BeamModule_Noise;
	UParticleModuleBeamModifier*	BeamModule_SourceModifier;
	int32								BeamModule_SourceModifier_Offset;
	UParticleModuleBeamModifier*	BeamModule_TargetModifier;
	int32								BeamModule_TargetModifier_Offset;

	TArray<UParticleModuleTypeDataBeam2*>	LOD_BeamTypeData;
	TArray<UParticleModuleBeamSource*>		LOD_BeamModule_Source;
	TArray<UParticleModuleBeamTarget*>		LOD_BeamModule_Target;
	TArray<UParticleModuleBeamNoise*>		LOD_BeamModule_Noise;
	TArray<UParticleModuleBeamModifier*>	LOD_BeamModule_SourceModifier;
	TArray<UParticleModuleBeamModifier*>	LOD_BeamModule_TargetModifier;

	bool							FirstEmission;
	int32								TickCount;
	int32								ForceSpawnCount;
	/** The method to utilize when forming the beam.							*/
	int32								BeamMethod;
	/** How many times to tile the texture along the beam.						*/
	TArray<int32>						TextureTiles;
	/** The number of live beams												*/
	int32								BeamCount;
	/** The actor to get the source point from.									*/
	AActor*							SourceActor;
	/** The emitter to get the source point from.								*/
	FParticleEmitterInstance*		SourceEmitter;
	/** User set Source points of each beam - primarily for weapon effects.		*/
	TArray<FVector>					UserSetSourceArray;
	/** User set Source tangents of each beam - primarily for weapon effects.	*/
	TArray<FVector>					UserSetSourceTangentArray;
	/** User set Source strengths of each beam - primarily for weapon effects.	*/
	TArray<float>					UserSetSourceStrengthArray;
	/** The distance of each beam, if utilizing the distance method.			*/
	TArray<float>					DistanceArray;
	/** The target point of each beam, when using the end point method.			*/
	TArray<FVector>					TargetPointArray;
	/** The target tangent of each beam, when using the end point method.		*/
	TArray<FVector>					TargetTangentArray;
	/** User set Target strengths of each beam - primarily for weapon effects.	*/
	TArray<float>					UserSetTargetStrengthArray;
	/** The actor to get the target point from.									*/
	AActor*							TargetActor;
	/** The emitter to get the Target point from.								*/
	FParticleEmitterInstance*		TargetEmitter;
	/** The target point sources of each beam, when using the end point method.	*/
	TArray<FName>					TargetPointSourceNames;
	/** User set target points of each beam - primarily for weapon effects.		*/
	TArray<FVector>					UserSetTargetArray;
	/** User set target tangents of each beam - primarily for weapon effects.	*/
	TArray<FVector>					UserSetTargetTangentArray;

	/** The number of vertices and triangles, for rendering						*/
	int32								VertexCount;
	int32								TriangleCount;
	TArray<int32>						BeamTrianglesPerSheet;

	/** Constructor	*/
	FParticleBeam2EmitterInstance();

	/** Destructor	*/
	virtual ~FParticleBeam2EmitterInstance();

	// Beam interface.
	virtual void SetBeamEndPoint(FVector NewEndPoint) OVERRIDE;
	virtual void SetBeamSourcePoint(FVector NewSourcePoint,int32 SourceIndex) OVERRIDE;
	virtual void SetBeamSourceTangent(FVector NewTangentPoint,int32 SourceIndex) OVERRIDE;
	virtual void SetBeamSourceStrength(float NewSourceStrength,int32 SourceIndex) OVERRIDE;
	virtual void SetBeamTargetPoint(FVector NewTargetPoint,int32 TargetIndex) OVERRIDE;
	virtual void SetBeamTargetTangent(FVector NewTangentPoint,int32 TargetIndex) OVERRIDE;
	virtual void SetBeamTargetStrength(float NewTargetStrength,int32 TargetIndex) OVERRIDE;
	virtual void ApplyWorldOffset(FVector InOffset, bool bWorldShift) OVERRIDE;
	
	//
	virtual void InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent, bool bClearResources = true);
	virtual void Init();
	virtual void Tick(float DeltaTime, bool bSuppressSpawning);
	/**
	 *	Tick sub-function that handles module post updates
	 *
	 *	@param	DeltaTime			The current time slice
	 *	@param	CurrentLODLevel		The current LOD level for the instance
	 */
	virtual void Tick_ModulePostUpdate(float DeltaTime, UParticleLODLevel* CurrentLODLevel);

	/**
	 *	Set the LOD to the given index
	 *
	 *	@param	InLODIndex			The index of the LOD to set as current
	 *	@param	bInFullyProcess		If true, process burst lists, etc.
	 */
	virtual void SetCurrentLODIndex(int32 InLODIndex, bool bInFullyProcess);

	/**
	 * Handle any post-spawning actions required by the instance
	 *
	 * @param	Particle					The particle that was spawned
	 * @param	InterpolationPercentage		The percentage of the time slice it was spawned at
	 * @param	SpawnTIme					The time it was spawned at
	 */
	virtual void PostSpawn(FBaseParticle* Particle, float InterpolationPercentage, float SpawnTime) OVERRIDE;

	virtual void UpdateBoundingBox(float DeltaTime);
	virtual void ForceUpdateBoundingBox() OVERRIDE;
	virtual uint32 RequiredBytes();
	float SpawnBeamParticles(float OldLeftover, float Rate, float DeltaTime, int32 Burst = 0, float BurstTime = 0.0f);
	virtual void KillParticles();
	void SetupBeamModules();
	void SetupBeamModifierModules();
	/**
	 *	Setup the offsets to the BeamModifier modules...
	 *	This must be done after the base Init call as that inserts modules into the offset map.
	 */
	void SetupBeamModifierModulesOffsets();
	void ResolveSource();
	void ResolveTarget();
	void DetermineVertexAndTriangleCount();

	/**
	 *	Retrieves the dynamic data for the emitter
	 */
	virtual FDynamicEmitterDataBase* GetDynamicData(bool bSelected);

	/**
	 *	Updates the dynamic data for the instance
	 *
	 *	@param	DynamicData		The dynamic data to fill in
	 *	@param	bSelected		true if the particle system component is selected
	 */
	virtual bool UpdateDynamicData(FDynamicEmitterDataBase* DynamicData, bool bSelected);

	/**
	 *	Retrieves replay data for the emitter
	 *
	 *	@return	The replay data, or NULL on failure
	 */
	virtual FDynamicEmitterReplayDataBase* GetReplayData();

	/**
	 *	Retrieve the allocated size of this instance.
	 *
	 *	@param	OutNum			The size of this instance
	 *	@param	OutMax			The maximum size of this instance
	 */
	virtual void GetAllocatedSize(int32& OutNum, int32& OutMax);

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 * @param	Mode	Specifies which resource size should be displayed. ( see EResourceSizeMode::Type )
	 * @return	Size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode) OVERRIDE;

	/**
	 * When an emitter is killed, this will check other emitters and clean up anything pointing to this one
	 */
	virtual void OnEmitterInstanceKilled(FParticleEmitterInstance* Instance) OVERRIDE
	{
		if (SourceEmitter == Instance)
		{
			SourceEmitter = NULL;
		}
	}

protected:
	/**
	 * Get the current material to render with.
	 */
	UMaterialInterface* GetCurrentMaterial();

	/**
	 * Captures dynamic replay data for this particle system.
	 *
	 * @param	OutData		[Out] Data will be copied here
	 *
	 * @return Returns true if successful
	 */
	virtual bool FillReplayData( FDynamicEmitterReplayDataBase& OutData );

};

/*-----------------------------------------------------------------------------
	FParticleRibbonEmitterInstance.
-----------------------------------------------------------------------------*/
struct FParticleTrailsEmitterInstance_Base : public FParticleEmitterInstance
{
	/** The vertex count for this emitter */
	int32 VertexCount;
	/** The triangle count for this emitter */
	int32 TriangleCount;
	/** The number of active trails in this emitter */
	int32 TrailCount;
	/** The max number of trails this emitter is allowed to have */
	int32 MaxTrailCount;
	/** The running time for this instance w/ ActiveParticles > 0 */
	float RunningTime;
	/** The last time the emitter instance was ticked */
	float LastTickTime;
	/** If true, mark trails dead on deactivate */
	uint32 bDeadTrailsOnDeactivate:1;

	/** The Spawn times for each trail in this emitter */
	TArray<float> TrailSpawnTimes;
	/** The last time a spawn happened for each trail in this emitter */
	TArray<float> LastSpawnTime;
	/** The distance traveled by each source of each trail in this emitter */
	TArray<float> SourceDistanceTraveled;
	/** The distance traveled by each source of each trail in this emitter */
	TArray<float> TiledUDistanceTraveled;
	/** If true, this emitter has not been updated yet... */
	uint32 bFirstUpdate:1;
	/**
	 *	If true, when the system checks for particles to kill, it
	 *	will use elapsed gametime to make the determination. This
	 *	will result in emitters that were inactive due to not being
	 *	rendered killing off old particles.
	 */
	uint32 bEnableInactiveTimeTracking:1;

	/** Constructor	*/
	FParticleTrailsEmitterInstance_Base() :
		  FParticleEmitterInstance()
		, VertexCount(0)
		, TriangleCount(0)
		, TrailCount(0)
		, MaxTrailCount(0)
		, RunningTime(0.0f)
		, LastTickTime(0.0f)
		, bDeadTrailsOnDeactivate(false)
		, bFirstUpdate(true)
		, bEnableInactiveTimeTracking(false)
	{
		TrailSpawnTimes.Empty();
		LastSpawnTime.Empty();
		SourceDistanceTraveled.Empty();
		TiledUDistanceTraveled.Empty();
	}

	/** Destructor	*/
	virtual ~FParticleTrailsEmitterInstance_Base()
	{
	}

	virtual void Init();
	virtual void InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent, bool bClearResources = true);
	virtual void Tick(float DeltaTime, bool bSuppressSpawning);

	/**
	 *	Tick sub-function that handles recalculation of tangents
	 *
	 *	@param	DeltaTime			The current time slice
	 *	@param	CurrentLODLevel		The current LOD level for the instance
	 */
	virtual void Tick_RecalculateTangents(float DeltaTime, UParticleLODLevel* CurrentLODLevel);
	virtual void UpdateBoundingBox(float DeltaTime);
	virtual void ForceUpdateBoundingBox() OVERRIDE;
	virtual void KillParticles();

	/**
	 *	Kill the given number of particles from the end of the trail.
	 *
	 *	@param	InTrailIdx		The trail to kill particles in.
	 *	@param	InKillCount		The number of particles to kill off.
	 */
	virtual void KillParticles(int32 InTrailIdx, int32 InKillCount);

	virtual void SetupTrailModules() {};
	virtual void UpdateSourceData(float DeltaTime, bool bFirstTime);
	// Update the start particles of the trail, if needed
//	virtual void UpdateStartParticles(float DeltaTime, bool bFirstTime) {}

	/**
	 *	Called when the particle system is deactivating...
	 */
	virtual void OnDeactivateSystem();

protected:
	/**
	 * Get the current material to render with.
	 */
	UMaterialInterface* GetCurrentMaterial();

	enum EGetTrailDirection
	{
		GET_Prev,
		GET_Next
	};
	enum EGetTrailParticleOption
	{
		GET_Any,			// Grab the prev/next particle
		GET_Spawned,		// Grab the first prev/next particle that was true spawned
		GET_Interpolated,	// Grab the first prev/next particle that was interpolation spawned
		GET_Start,			// Grab the start particle for the trail the particle is in
		GET_End,			// Grab the end particle for the trail the particle is in
	};

	/**
	 *	Retrieve the particle in the trail that meets the given criteria
	 *
	 *	@param	bSkipStartingParticle		If true, don't check the starting particle for meeting the criteria
	 *	@param	InStartingFromParticle		The starting point for the search.
	 *	@param	InStartingTrailData			The trail data for the starting point.
	 *	@param	InGetDirection				Direction to search the trail.
	 *	@param	InGetOption					Options for defining the type of particle.
	 *	@param	OutParticle					The particle that meets the criteria.
	 *	@param	OutTrailData				The trail data of the particle that meets the criteria.
	 *
	 *	@return	bool						true if found, false if not.
	 */
	bool GetParticleInTrail(
		bool bSkipStartingParticle,
		FBaseParticle* InStartingFromParticle,
		FTrailsBaseTypeDataPayload* InStartingTrailData,
		EGetTrailDirection InGetDirection,
		EGetTrailParticleOption InGetOption,
		FBaseParticle*& OutParticle,
		FTrailsBaseTypeDataPayload*& OutTrailData);

	/** Prints out info for a single particle. */
	virtual void PrintParticleData(FBaseParticle* Particle, FTrailsBaseTypeDataPayload* TrailData, int32 CurrentIndex, int32 TrailIndex){}
	/** Prints out info for all active particles. */
	virtual void PrintAllActiveParticles(){}
	/** Traverses all trails and prints out debugging info. */
	virtual void PrintTrails(){}
};

struct FParticleRibbonEmitterInstance : public FParticleTrailsEmitterInstance_Base
{
	/** The TypeData module for this trail emitter */
	UParticleModuleTypeDataRibbon*	TrailTypeData;

	/** SpawnPerUnit module (hijacking it for trails here) */
	UParticleModuleSpawnPerUnit*	SpawnPerUnitModule;

	/** Source module */
	UParticleModuleTrailSource*		SourceModule;
	/** Payload offset for source module */
	int32								TrailModule_Source_Offset;

	/** The current source position for each trail in this emitter */
	TArray<FVector> CurrentSourcePosition;
	/** The current source rotation for each trail in this emitter */
	TArray<FQuat> CurrentSourceRotation;
	/** The current source up for each trail in this emitter */
	TArray<FVector> CurrentSourceUp;
	/** The current source tangent for each trail in this emitter */
	TArray<FVector> CurrentSourceTangent;
	/** The current source tangent strength for each trail in this emitter */
	TArray<float> CurrentSourceTangentStrength;
	/** The previous source position for each trail in this emitter */
	TArray<FVector> LastSourcePosition;
	/** The last source rotation for each trail in this emitter */
	TArray<FQuat> LastSourceRotation;
	/** The previous source up for each trail in this emitter */
	TArray<FVector> LastSourceUp;
	/** The previous source tangent for each trail in this emitter */
	TArray<FVector> LastSourceTangent;
	/** The previous source tangent strength for each trail in this emitter */
	TArray<float> LastSourceTangentStrength;
	/** If the source is an actor, this is it */
	AActor* SourceActor;
	/** The offset from the source for each trail in this emitter */
	TArray<FVector> SourceOffsets;
	/** If the source is an emitter, this is it */
	FParticleEmitterInstance* SourceEmitter;
	/** The last selected source index (for sequential selection) */
	int32 LastSelectedParticleIndex;
	/** The indices for the source of each trail (if required) */
	TArray<int32> SourceIndices;
	/** The time of the last partice source update */
	TArray<float> SourceTimes;
	/** The time of the last partice source update */
	TArray<float> LastSourceTimes;
	/** The lifetime to use for each ribbon */
	TArray<float> CurrentLifetimes;
	/** The size to use for each ribbon */
	TArray<float> CurrentSizes;
	/** The direct index of the particle that is the start of each ribbon */
//	TArray<int32> CurrentStartIndices;
	/** If true, then do not render the source (a real spawn just happened) */
//	TArray<bool> SkipSourceSegment;

	/** The number of "head only" active particles */
	int32 HeadOnlyParticles;

	/** Constructor	*/
	FParticleRibbonEmitterInstance();

	/** Destructor	*/
	virtual ~FParticleRibbonEmitterInstance();

	virtual void InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent, bool bClearResources = true);

	/**
	 *	Tick sub-function that handles recalculation of tangents
	 *
	 *	@param	DeltaTime			The current time slice
	 *	@param	CurrentLODLevel		The current LOD level for the instance
	 */
	virtual void Tick_RecalculateTangents(float DeltaTime, UParticleLODLevel* CurrentLODLevel);

	virtual bool GetSpawnPerUnitAmount(float DeltaTime, int32 InTrailIdx, int32& OutCount, float& OutRate);

	/**
	 *	Get the lifetime and size for a particle being added to the given trail
	 *	
	 *	@param	InTrailIdx				The index of the trail the particle is being added to
	 *	@param	InParticle				The particle that is being added
	 *	@param	bInNoLivingParticles	true if there are no particles in the trail, false if there already are
	 *	@param	OutOneOverMaxLifetime	The OneOverMaxLifetime value to use for the particle
	 *	@param	OutSize					The Size value to use for the particle
	 */
	void GetParticleLifetimeAndSize(int32 InTrailIdx, const FBaseParticle* InParticle, bool bInNoLivingParticles, float& OutOneOverMaxLifetime, float& OutSize);

	/**
	 *	Spawn particles for this emitter instance
	 *	@param	DeltaTime		The time slice to spawn over
	 *	@return	float			The leftover fraction of spawning
	 */
	virtual float Spawn(float DeltaTime);

	/**
	 *	Spawn source-based ribbon particles.
	 *
	 *	@param	DeltaTime			The current time slice
	 *
	 *	@return	bool				true if SpawnRate should be processed.
	 */
	bool Spawn_Source(float DeltaTime);
	/**
	 *	Spawn ribbon particles from SpawnRate and Burst settings.
	 *
	 *	@param	DeltaimTime			The current time slice
	 *	
	 *	@return	float				The spawnfraction left over from this time slice
	 */
	float Spawn_RateAndBurst(float DeltaTime);
	
	virtual void SetupTrailModules();
	void ResolveSource();
	virtual void UpdateSourceData(float DeltaTime, bool bFirstTime);
	// Update the start particles of the trail, if needed
//	virtual void UpdateStartParticles(float DeltaTime, bool bFirstTime);
	bool ResolveSourcePoint(int32 InTrailIdx, FVector& OutPosition, FQuat& OutRotation, FVector& OutUp, FVector& OutTangent, float& OutTangentStrength);

	/** Determine the number of vertices and triangles in each trail */
	void DetermineVertexAndTriangleCount();

	/**
	 *	Checks some common values for GetDynamicData validity
	 *
	 *	@return	bool		true if GetDynamicData should continue, false if it should return NULL
	 */
	virtual bool IsDynamicDataRequired(UParticleLODLevel* CurrentLODLevel);

	/**
	 *	Retrieves the dynamic data for the emitter
	 */
	virtual FDynamicEmitterDataBase* GetDynamicData(bool bSelected);

	/**
	 *	Updates the dynamic data for the instance
	 *
	 *	@param	DynamicData		The dynamic data to fill in
	 *	@param	bSelected		true if the particle system component is selected
	 */
	virtual bool UpdateDynamicData(FDynamicEmitterDataBase* DynamicData, bool bSelected);

	/**
	 *	Retrieves replay data for the emitter
	 *
	 *	@return	The replay data, or NULL on failure
	 */
	virtual FDynamicEmitterReplayDataBase* GetReplayData();

	/**
	 *	Retrieve the allocated size of this instance.
	 *
	 *	@param	OutNum			The size of this instance
	 *	@param	OutMax			The maximum size of this instance
	 */
	virtual void GetAllocatedSize(int32& OutNum, int32& OutMax);

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 * @param	Mode	Specifies which resource size should be displayed. ( see EResourceSizeMode::Type )
	 * @return  Size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode) OVERRIDE;

	/**
	 * When an emitter is killed, this will check other emitters and clean up anything pointing to this one
	 */
	virtual void OnEmitterInstanceKilled(FParticleEmitterInstance* Instance) OVERRIDE
	{
		if (SourceEmitter == Instance)
		{
			SourceEmitter = NULL;
		}
	}

protected:

	/**
	 * Captures dynamic replay data for this particle system.
	 *
	 * @param	OutData		[Out] Data will be copied here
	 *
	 * @return Returns true if successful
	 */
	virtual bool FillReplayData( FDynamicEmitterReplayDataBase& OutData );
};

/*-----------------------------------------------------------------------------
	FParticleAnimTrailEmitterInstance.
-----------------------------------------------------------------------------*/

struct FParticleAnimTrailEmitterInstance : public FParticleTrailsEmitterInstance_Base
{
	/** The TypeData module for this trail emitter */
	UParticleModuleTypeDataAnimTrail*	TrailTypeData;

	/** SpawnPerUnit module (hijacking it for trails here) */
	UParticleModuleSpawnPerUnit*		SpawnPerUnitModule;

	/**
	 *	The name of the socket that supplies the first edge for this emitter.
	 */
	FName FirstSocketName;

	/**
	 *	The name of the socket that supplies the second edge for this emitter.
	 */
	FName SecondSocketName;

	/**
	*	The width of the trail.
	*/
	float Width;

	/**
	*	How the width is applied to the trail.
	*/
	ETrailWidthMode WidthMode;

	/**
	 *	The anim notify state that spawned this emitter.
	 */
	class UAnimNotifyState* AnimNotifyState;

	/**
	*	When set, the current trail will be marked as dead in the next tick.
	*/
	bool bTagTrailAsDead;

	/**
	*	Whether new particles should be spawned.
	*/
	bool bTrailEnabled;

	/** Editor only variables controlling the debug rendering for trails.*/
#if WITH_EDITORONLY_DATA
	uint32 bRenderGeometry : 1;
	uint32 bRenderSpawnPoints : 1;
	uint32 bRenderTangents : 1;
	uint32 bRenderTessellation : 1;
#endif

	/** The number of particles in the trail which are either marked */
	int32 HeadOnlyParticles;

	/** Constructor	*/
	FParticleAnimTrailEmitterInstance();

	/** Destructor	*/
	virtual ~FParticleAnimTrailEmitterInstance();

	virtual void InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent, bool bClearResources = true);

	/**
	 *	Helper function for recalculating tangents and the spline interpolation parameter...
	 *
	 *	@param	PrevParticle		The previous particle in the trail
	 *	@param	PrevTrailData		The payload of the previous particle in the trail
	 *	@param	CurrParticle		The current particle in the trail
	 *	@param	CurrTrailData		The payload of the current particle in the trail
	 *	@param	NextParticle		The next particle in the trail
	 *	@param	NextTrailData		The payload of the next particle in the trail
	 */
	virtual void RecalculateTangentAndInterpolationParam(
		FBaseParticle* PrevParticle, FAnimTrailTypeDataPayload* PrevTrailData, 
		FBaseParticle* CurrParticle, FAnimTrailTypeDataPayload* CurrTrailData, 
		FBaseParticle* NextParticle, FAnimTrailTypeDataPayload* NextTrailData);

	/**
	 *	Tick sub-function that handles recalculation of tangents
	 *
	 *	@param	DeltaTime			The current time slice
	 *	@param	CurrentLODLevel		The current LOD level for the instance
	 */
	virtual void Tick_RecalculateTangents(float DeltaTime, UParticleLODLevel* CurrentLODLevel);

	virtual bool GetSpawnPerUnitAmount(float DeltaTime, int32 InTrailIdx, int32& OutCount, float& OutRate);

	/**
	 *	Spawn particles for this emitter instance
	 *	@param	DeltaTime		The time slice to spawn over
	 *	@return	float			The leftover fraction of spawning
	 */
	virtual float Spawn(float DeltaTime);

	virtual void SetupTrailModules();
	void ResolveSource();
	virtual void UpdateSourceData(float DeltaTime, bool bFirstTime);

	void UpdateBoundingBox(float DeltaTime);
	void ForceUpdateBoundingBox();

	/** Determine the number of vertices and triangles in each trail */
	void DetermineVertexAndTriangleCount();

	/**
	 *	Retrieves the dynamic data for the emitter
	 */
	virtual FDynamicEmitterDataBase* GetDynamicData(bool bSelected);

	/**
	 *	Updates the dynamic data for the instance
	 *
	 *	@param	DynamicData		The dynamic data to fill in
	 *	@param	bSelected		true if the particle system component is selected
	 */
	virtual bool UpdateDynamicData(FDynamicEmitterDataBase* DynamicData, bool bSelected);

	/**
	 *	Retrieves replay data for the emitter
	 *
	 *	@return	The replay data, or NULL on failure
	 */
	virtual FDynamicEmitterReplayDataBase* GetReplayData();

	/**
	 *	Retrieve the allocated size of this instance.
	 *
	 *	@param	OutNum			The size of this instance
	 *	@param	OutMax			The maximum size of this instance
	 */
	virtual void GetAllocatedSize(int32& OutNum, int32& OutMax);

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 * @param	Mode	Specifies which resource size should be displayed. ( see EResourceSizeMode::Type )
	 * @return  Size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode) OVERRIDE;

	virtual bool IsTrailEmitter()const{ return true; }

	/**
	 * Begins the trail.
	 *
	 * @param	InAnimNotifyState	The AnimNotifyState that created (or is creating) this trail.
	 */
	void BeginTrail(UAnimNotifyState* InAnimNotifyState);	
	
	/**
	 * Ends the trail.
	 *
	 * @return  True if this was a trail ended by this notify, false otherwise.
	 */
	void EndTrail();

	/**
	* Sets the date that defines this trail.
	*
	* @param	InFirstSocketName	The name of the first socket for the trail.
	* @param	InSecondSocketName	The name of the second socket for the trail.
	* @param	InWidthMode			How the width value is applied to the trail.
	* @param	InWidth				The width of the trail.
	*/
	void SetTrailSourceData( FName InFirstSocketName, FName InSecondSocketName, ETrailWidthMode InWidthMode, float InWidth );

	bool IsTrailActive()const;

#if WITH_EDITORONLY_DATA
	/**
	* Sets various debug variables for trails.
	*
	* @param	bRenderGeometry		When true, the trails geometry is rendered.
	* @param	bRenderSpawnPoints	When true, the trails spawn points are rendered.
	* @param	bRenderTessellation	When true, the trails tessellated points are rendered.
	* @param	bRenderTangents		When true, the trails tangents are rendered.
	*/
	void SetTrailDebugData(bool bInRenderGeometry, bool bInRenderSpawnPoints, bool bInRenderTessellation, bool bInRenderTangents);
#endif

protected:

	/**
	 * Captures dynamic replay data for this particle system.
	 *
	 * @param	OutData		[Out] Data will be copied here
	 *
	 * @return Returns true if successful
	 */
	virtual bool FillReplayData( FDynamicEmitterReplayDataBase& OutData );

	/** 
	*	Helper to spawn a trail particle during SpawnParticles(). 
	*	@param	StartParticleIndex	Index of the particle at the start of the trail. Is altered in the function.
	*	@param	Params			Various parameters for spawning.
	*/
	void SpawnParticle( int32& StartParticleIndex, const struct FAnimTrailParticleSpawnParams& Params );

	/** Prints out info for a single particle. */
	virtual void PrintParticleData(FBaseParticle* Particle, FTrailsBaseTypeDataPayload* TrailData, int32 CurrentIndex, int32 TrailIndex);
	/** Prints out info for all active particles. */
	virtual void PrintAllActiveParticles();
	/** Traverses all trails and prints out debugging info. */
	virtual void PrintTrails();
};
