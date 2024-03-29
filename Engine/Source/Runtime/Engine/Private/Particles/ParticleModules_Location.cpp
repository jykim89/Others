// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ParticleModules_Location.cpp: 
	Location-related particle module implementations.
=============================================================================*/

#include "EnginePrivate.h"
#include "ParticleDefinitions.h"
#include "../DistributionHelpers.h"

UParticleModuleLocationBase::UParticleModuleLocationBase(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}


/*-----------------------------------------------------------------------------
	Abstract base modules used for categorization.
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
	UParticleModuleLocation implementation.
-----------------------------------------------------------------------------*/

UParticleModuleLocation::UParticleModuleLocation(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	bSpawnModule = true;
	bSupported3DDrawMode = true;
	DistributeOverNPoints = 0.0f;
}

void UParticleModuleLocation::InitializeDefaults()
{
	if (!StartLocation.Distribution)
	{
		StartLocation.Distribution = NewNamedObject<UDistributionVectorUniform>(this, TEXT("DistributionStartLocation"));
	}
}

void UParticleModuleLocation::PostInitProperties()
{
	Super::PostInitProperties();
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad))
	{
		InitializeDefaults();
	}
}

void UParticleModuleLocation::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_MOVE_DISTRIBUITONS_TO_POSTINITPROPS)
	{
		FDistributionHelpers::RestoreDefaultUniform(StartLocation.Distribution, TEXT("DistributionStartLocation"), FVector::ZeroVector, FVector::ZeroVector);
	}
}

#if WITH_EDITOR
void UParticleModuleLocation::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	InitializeDefaults();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

void UParticleModuleLocation::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	SpawnEx(Owner, Offset, SpawnTime, NULL, ParticleBase);
}

void UParticleModuleLocation::SpawnEx(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, class FRandomStream* InRandomStream, FBaseParticle* ParticleBase)
{
	SPAWN_INIT;
	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	FVector LocationOffset;

	// Avoid divide by zero.
	if ((DistributeOverNPoints != 0.0f) && (DistributeOverNPoints != 1.f))
	{
		float RandomNum = FMath::SRand() * FMath::Fractional(Owner->EmitterTime);

		if(RandomNum > DistributeThreshold)
		{
			LocationOffset = StartLocation.GetValue(Owner->EmitterTime, Owner->Component, 0, InRandomStream);
		}
		else
		{
			FVector Min, Max;
			StartLocation.Distribution->GetRange(Min, Max);
			FVector Lerped = FMath::Lerp(Min, Max, FMath::TruncToFloat((FMath::SRand() * (DistributeOverNPoints - 1.0f)) + 0.5f)/(DistributeOverNPoints - 1.0f));
			LocationOffset.Set(Lerped.X, Lerped.Y, Lerped.Z);
		}
	}
	else
	{
		LocationOffset = StartLocation.GetValue(Owner->EmitterTime, Owner->Component, 0, InRandomStream);
	}

	LocationOffset = Owner->EmitterToSimulation.TransformVector(LocationOffset);
	Particle.Location += LocationOffset;
}

void UParticleModuleLocation::Render3DPreview(FParticleEmitterInstance* Owner, const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
#if WITH_EDITOR
	// Draw the location as a wire star
	FVector Position(0.0f);

	FMatrix LocalToWorld = FMatrix::Identity;
	if (Owner != NULL)
	{
		LocalToWorld = Owner->EmitterToSimulation * Owner->SimulationToWorld;
	}

	if (StartLocation.Distribution)
	{
		// Nothing else to do if it is constant...
		if (StartLocation.Distribution->IsA(UDistributionVectorUniform::StaticClass()))
		{
			// Draw a box showing the min/max extents
			UDistributionVectorUniform* Uniform = CastChecked<UDistributionVectorUniform>(StartLocation.Distribution);
			
			Position = (Uniform->GetMaxValue() + Uniform->GetMinValue()) / 2.0f;

			FVector MinValue = Uniform->GetMinValue();
			FVector MaxValue = Uniform->GetMaxValue();
			FVector Extent = (MaxValue - MinValue) / 2.0f;
			FVector Offset = (MaxValue + MinValue) / 2.0f;
			// We just want to rotate the offset
			Offset = LocalToWorld.TransformVector(Offset);
			DrawOrientedWireBox(PDI, LocalToWorld.GetOrigin() + Offset, 
				LocalToWorld.GetScaledAxis( EAxis::X ), LocalToWorld.GetScaledAxis( EAxis::Y ), LocalToWorld.GetScaledAxis( EAxis::Z ), 
				Extent, ModuleEditorColor, SDPG_World);
		}
		else if (StartLocation.Distribution->IsA(UDistributionVectorConstantCurve::StaticClass()))
		{
			// Draw a box showing the min/max extents
			UDistributionVectorConstantCurve* Curve = CastChecked<UDistributionVectorConstantCurve>(StartLocation.Distribution);

			//Curve->
			Position = StartLocation.GetValue(0.0f, Owner->Component);
		}
		else if (StartLocation.Distribution->IsA(UDistributionVectorConstant::StaticClass()))
		{
			Position = StartLocation.GetValue(0.0f, Owner->Component);
		}
	}

	Position = LocalToWorld.TransformPosition(Position);
	DrawWireStar(PDI, Position, 10.0f, ModuleEditorColor, SDPG_World);
#endif	//#if WITH_EDITOR
}

/*-----------------------------------------------------------------------------
	UParticleModuleLocation_Seeded implementation.
-----------------------------------------------------------------------------*/
UParticleModuleLocation_Seeded::UParticleModuleLocation_Seeded(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	bSpawnModule = true;
	bSupportsRandomSeed = true;
	bRequiresLoopingNotification = true;
}

void UParticleModuleLocation_Seeded::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	FParticleRandomSeedInstancePayload* Payload = (FParticleRandomSeedInstancePayload*)(Owner->GetModuleInstanceData(this));
	SpawnEx(Owner, Offset, SpawnTime, (Payload != NULL) ? &(Payload->RandomStream) : NULL, ParticleBase);
}

uint32 UParticleModuleLocation_Seeded::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	return RandomSeedInfo.GetInstancePayloadSize();
}

uint32 UParticleModuleLocation_Seeded::PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData)
{
	return PrepRandomSeedInstancePayload(Owner, (FParticleRandomSeedInstancePayload*)InstData, RandomSeedInfo);
}

void UParticleModuleLocation_Seeded::EmitterLoopingNotify(FParticleEmitterInstance* Owner)
{
	if (RandomSeedInfo.bResetSeedOnEmitterLooping == true)
	{
		FParticleRandomSeedInstancePayload* Payload = (FParticleRandomSeedInstancePayload*)(Owner->GetModuleInstanceData(this));
		PrepRandomSeedInstancePayload(Owner, Payload, RandomSeedInfo);
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleLocationWorldOffset implementation.
-----------------------------------------------------------------------------*/
UParticleModuleLocationWorldOffset::UParticleModuleLocationWorldOffset(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

void UParticleModuleLocationWorldOffset::SpawnEx(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, class FRandomStream* InRandomStream, FBaseParticle* ParticleBase)
{
	SPAWN_INIT;
	UParticleLODLevel* LODLevel = Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	if (LODLevel->RequiredModule->bUseLocalSpace == false)
	{
		// Nothing to do here... the distribution value is already being in world space
		Particle.Location += StartLocation.GetValue(Owner->EmitterTime, Owner->Component, 0, InRandomStream);
	}
	else
	{
		// We need to inverse transform the location so that the bUseLocalSpace transform uses the proper value
		FMatrix InvMat = Owner->Component->ComponentToWorld.ToMatrixWithScale().Inverse();
		FVector StartLoc = StartLocation.GetValue(Owner->EmitterTime, Owner->Component, 0, InRandomStream);
		Particle.Location += InvMat.TransformVector(StartLoc);
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleLocationWorldOffset_Seeded implementation.
-----------------------------------------------------------------------------*/
UParticleModuleLocationWorldOffset_Seeded::UParticleModuleLocationWorldOffset_Seeded(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	bSpawnModule = true;
	bSupportsRandomSeed = true;
	bRequiresLoopingNotification = true;
}

void UParticleModuleLocationWorldOffset_Seeded::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	FParticleRandomSeedInstancePayload* Payload = (FParticleRandomSeedInstancePayload*)(Owner->GetModuleInstanceData(this));
	SpawnEx(Owner, Offset, SpawnTime, (Payload != NULL) ? &(Payload->RandomStream) : NULL, ParticleBase);
}

uint32 UParticleModuleLocationWorldOffset_Seeded::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	return RandomSeedInfo.GetInstancePayloadSize();
}

uint32 UParticleModuleLocationWorldOffset_Seeded::PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData)
{
	return PrepRandomSeedInstancePayload(Owner, (FParticleRandomSeedInstancePayload*)InstData, RandomSeedInfo);
}

void UParticleModuleLocationWorldOffset_Seeded::EmitterLoopingNotify(FParticleEmitterInstance* Owner)
{
	if (RandomSeedInfo.bResetSeedOnEmitterLooping == true)
	{
		FParticleRandomSeedInstancePayload* Payload = (FParticleRandomSeedInstancePayload*)(Owner->GetModuleInstanceData(this));
		PrepRandomSeedInstancePayload(Owner, Payload, RandomSeedInfo);
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleLocationDirect implementation.
-----------------------------------------------------------------------------*/
UParticleModuleLocationDirect::UParticleModuleLocationDirect(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	bSpawnModule = true;
	bUpdateModule = true;
}

void UParticleModuleLocationDirect::InitializeDefaults()
{
	if (!Location.Distribution)
	{
		Location.Distribution = NewNamedObject<UDistributionVectorUniform>(this, TEXT("DistributionLocation"));
	}

	if (!LocationOffset.Distribution)
	{
		UDistributionVectorConstant* DistributionLocationOffset = NewNamedObject<UDistributionVectorConstant>(this, TEXT("DistributionLocationOffset"));
		DistributionLocationOffset->Constant = FVector(0.0f, 0.0f, 0.0f);
		LocationOffset.Distribution = DistributionLocationOffset;
	}

	if (!Direction.Distribution)
	{
		UDistributionVectorConstant* DistributionScaleFactor = NewNamedObject<UDistributionVectorConstant>(this, TEXT("DistributionScaleFactor"));
		DistributionScaleFactor->Constant = FVector(1.0f, 1.0f, 1.0f);
		ScaleFactor.Distribution = DistributionScaleFactor;

		Direction.Distribution = NewNamedObject<UDistributionVectorUniform>(this, TEXT("DistributionDirection"));
	}
}

void UParticleModuleLocationDirect::PostInitProperties()
{
	Super::PostInitProperties();
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad))
	{
		InitializeDefaults();
	}
}

void UParticleModuleLocationDirect::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_MOVE_DISTRIBUITONS_TO_POSTINITPROPS)
	{
		FDistributionHelpers::RestoreDefaultUniform(Location.Distribution, TEXT("DistributionLocation"), FVector::ZeroVector, FVector::ZeroVector);
		FDistributionHelpers::RestoreDefaultConstant(LocationOffset.Distribution, TEXT("DistributionLocationOffset"), FVector::ZeroVector);
		FDistributionHelpers::RestoreDefaultConstant(ScaleFactor.Distribution, TEXT("DistributionScaleFactor"), FVector(1.0f, 1.0f, 1.0f));
		FDistributionHelpers::RestoreDefaultUniform(Direction.Distribution, TEXT("DistributionDirection"), FVector::ZeroVector, FVector::ZeroVector);
	}
}

#if WITH_EDITOR
void UParticleModuleLocationDirect::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	InitializeDefaults();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

void UParticleModuleLocationDirect::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	SPAWN_INIT;

	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	if (LODLevel->RequiredModule->bUseLocalSpace)
	{
		Particle.Location = Location.GetValue(Particle.RelativeTime, Owner->Component);
	}
	else
	{
		FVector StartLoc	= Location.GetValue(Particle.RelativeTime, Owner->Component);
		StartLoc = Owner->Component->ComponentToWorld.TransformPosition(StartLoc);
		Particle.Location	= StartLoc;
	}

	PARTICLE_ELEMENT(FVector, LocOffset);
	LocOffset	= LocationOffset.GetValue(Owner->EmitterTime, Owner->Component);
	Particle.Location += LocOffset;
}

void UParticleModuleLocationDirect::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	BEGIN_UPDATE_LOOP;
	{
		FVector	NewLoc;
		UParticleLODLevel* LODLevel = Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
		check(LODLevel);
		if (LODLevel->RequiredModule->bUseLocalSpace)
		{
			NewLoc = Location.GetValue(Particle.RelativeTime, Owner->Component);
		}
		else
		{
			FVector Loc			= Location.GetValue(Particle.RelativeTime, Owner->Component);
			Loc = Owner->Component->ComponentToWorld.TransformPosition(Loc);
			NewLoc	= Loc;
		}

		FVector	Scale	= ScaleFactor.GetValue(Particle.RelativeTime, Owner->Component);

		PARTICLE_ELEMENT(FVector, LocOffset);
		NewLoc += LocOffset;

		FVector	Diff		 = (NewLoc - Particle.Location);
		FVector	ScaleDiffA	 = Diff * Scale.X;
		FVector	ScaleDiffB	 = Diff * (1.0f - Scale.X);
		float InvDeltaTime = (DeltaTime > 0.0f) ? 1.0f / DeltaTime : 0.0f;
		Particle.Velocity	 = ScaleDiffA * InvDeltaTime;
		Particle.Location	+= ScaleDiffB;
	}
	END_UPDATE_LOOP;
}

uint32 UParticleModuleLocationDirect::RequiredBytes(FParticleEmitterInstance* Owner)
{
	return sizeof(FVector);
}

/*-----------------------------------------------------------------------------
	UParticleModuleLocationEmitter implementation.
-----------------------------------------------------------------------------*/
UParticleModuleLocationEmitter::UParticleModuleLocationEmitter(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FName NAME_None;
		FConstructorStatics()
			: NAME_None(TEXT("None"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	bSpawnModule = true;
	SelectionMethod = ELESM_Random;
	EmitterName = ConstructorStatics.NAME_None;
	InheritSourceVelocity = false;
	InheritSourceVelocityScale = 1.0f;
	bInheritSourceRotation = false;
	InheritSourceRotationScale = 1.0f;
}

void UParticleModuleLocationEmitter::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	// We need to look up the emitter instance...
	// This may not need to be done every Spawn, but in the short term, it will to be safe.
	// (In the editor, the source emitter may be deleted, etc...)
	FParticleEmitterInstance* LocationEmitterInst = NULL;
	if (EmitterName != NAME_None)
	{
		for (int32 ii = 0; ii < Owner->Component->EmitterInstances.Num(); ii++)
		{
			FParticleEmitterInstance* pkEmitInst = Owner->Component->EmitterInstances[ii];
			if (pkEmitInst && (pkEmitInst->SpriteTemplate->EmitterName == EmitterName))
			{
				LocationEmitterInst = pkEmitInst;
				break;
			}
		}
	}

	if (LocationEmitterInst == NULL)
	{
		// No source emitter, so we don't spawn??
		return;
	}

	check(LocationEmitterInst->CurrentLODLevel);
	check(LocationEmitterInst->CurrentLODLevel->RequiredModule);
	check(Owner->CurrentLODLevel);
	check(Owner->CurrentLODLevel->RequiredModule);
	bool bSourceIsInLocalSpace = LocationEmitterInst->CurrentLODLevel->RequiredModule->bUseLocalSpace;
	bool bInLocalSpace = Owner->CurrentLODLevel->RequiredModule->bUseLocalSpace;

	SPAWN_INIT;
		{
			int32 Index = 0;

			switch (SelectionMethod)
			{
			case ELESM_Random:
				{
					Index = FMath::TruncToInt(FMath::SRand() * LocationEmitterInst->ActiveParticles);
					if (Index >= LocationEmitterInst->ActiveParticles)
					{
						Index = LocationEmitterInst->ActiveParticles - 1;
					}
				}
				break;
			case ELESM_Sequential:
				{
					FLocationEmitterInstancePayload* Payload = 
						(FLocationEmitterInstancePayload*)(Owner->GetModuleInstanceData(this));
					if (Payload != NULL)
					{
						Index = ++(Payload->LastSelectedIndex);
						if (Index >= LocationEmitterInst->ActiveParticles)
						{
							Index = 0;
							Payload->LastSelectedIndex = Index;
						}
					}
					else
					{
						// There was an error...
						//@todo.SAS. How to resolve this situation??
					}
				}
				break;
			}
					
			// Grab a particle from the location emitter instance
			FBaseParticle* pkParticle = LocationEmitterInst->GetParticle(Index);
			if (pkParticle)
			{
				if ((pkParticle->RelativeTime == 0.0f) && (pkParticle->Location == FVector::ZeroVector))
				{
					if (bInLocalSpace == false)
					{
						Particle.Location = LocationEmitterInst->Component->GetComponentLocation();
					}
					else
					{
						Particle.Location = FVector::ZeroVector;
					}
				}
				else
				{
					if (bSourceIsInLocalSpace == bInLocalSpace)
					{
						// Just copy it directly
						Particle.Location = pkParticle->Location;
					}
					else if ((bSourceIsInLocalSpace == true) && (bInLocalSpace == false))
					{
						// We need to transform it into world space
						Particle.Location = LocationEmitterInst->Component->ComponentToWorld.TransformPosition(pkParticle->Location);
					}
					else //if ((bSourceIsInLocalSpace == false) && (bInLocalSpace == true))
					{
						// We need to transform it into local space
						Particle.Location = LocationEmitterInst->Component->ComponentToWorld.InverseTransformPosition(pkParticle->Location);
					}
				}
				if (InheritSourceVelocity)
				{
					Particle.BaseVelocity	+= pkParticle->Velocity * InheritSourceVelocityScale;
					Particle.Velocity		+= pkParticle->Velocity * InheritSourceVelocityScale;
				}

				if (bInheritSourceRotation)
				{
					// If the ScreenAlignment of the source emitter is PSA_Velocity, 
					// and that of the local is not, then the rotation will NOT be correct!
					Particle.Rotation		+= pkParticle->Rotation * InheritSourceRotationScale;
				}
			}
		}
} 

uint32 UParticleModuleLocationEmitter::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	return sizeof(FLocationEmitterInstancePayload);
}

/*-----------------------------------------------------------------------------
	UParticleModuleLocationEmitterDirect implementation.
-----------------------------------------------------------------------------*/
UParticleModuleLocationEmitterDirect::UParticleModuleLocationEmitterDirect(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FName NAME_None;
		FConstructorStatics()
			: NAME_None(TEXT("None"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	bSpawnModule = true;
	bUpdateModule = true;
	EmitterName = ConstructorStatics.NAME_None;
}

void UParticleModuleLocationEmitterDirect::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	// We need to look up the emitter instance...
	// This may not need to be done every Spawn, but in the short term, it will to be safe.
	// (In the editor, the source emitter may be deleted, etc...)
	FParticleEmitterInstance* LocationEmitterInst = NULL;
	if (EmitterName != NAME_None)
	{
		for (int32 ii = 0; ii < Owner->Component->EmitterInstances.Num(); ii++)
		{
			FParticleEmitterInstance* pkEmitInst = Owner->Component->EmitterInstances[ii];
			if (pkEmitInst && (pkEmitInst->SpriteTemplate->EmitterName == EmitterName))
			{
				LocationEmitterInst = pkEmitInst;
				break;
			}
		}
	}

	if (LocationEmitterInst == NULL)
	{
		// No source emitter, so we don't spawn??
		return;
	}

	SPAWN_INIT;
		int32 Index = Owner->ActiveParticles;

		// Grab a particle from the location emitter instance
		FBaseParticle* pkParticle = LocationEmitterInst->GetParticle(Index);
		if (pkParticle)
		{
			Particle.Location		= pkParticle->Location;
			Particle.OldLocation	= pkParticle->OldLocation;
			Particle.Velocity		= pkParticle->Velocity;
			Particle.RelativeTime	= pkParticle->RelativeTime;
		}
} 

void UParticleModuleLocationEmitterDirect::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	// We need to look up the emitter instance...
	// This may not need to be done every Spawn, but in the short term, it will to be safe.
	// (In the editor, the source emitter may be deleted, etc...)
	FParticleEmitterInstance* LocationEmitterInst = NULL;
	if (EmitterName != NAME_None)
	{
		for (int32 ii = 0; ii < Owner->Component->EmitterInstances.Num(); ii++)
		{
			FParticleEmitterInstance* pkEmitInst = Owner->Component->EmitterInstances[ii];
			if (pkEmitInst && (pkEmitInst->SpriteTemplate->EmitterName == EmitterName))
			{
				LocationEmitterInst = pkEmitInst;
				break;
			}
		}
	}

	if (LocationEmitterInst == NULL)
	{
		// No source emitter, so we don't spawn??
		return;
	}

	BEGIN_UPDATE_LOOP;
		{
			// Grab a particle from the location emitter instance
			FBaseParticle* pkParticle = LocationEmitterInst->GetParticle(i);
			if (pkParticle)
			{
				Particle.Location		= pkParticle->Location;
				Particle.OldLocation	= pkParticle->OldLocation;
				Particle.Velocity		= pkParticle->Velocity;
				Particle.RelativeTime	= pkParticle->RelativeTime;
			}
		}
	END_UPDATE_LOOP;
}

/*-----------------------------------------------------------------------------
	UParticleModuleLocationPrimitiveBase implementation.
-----------------------------------------------------------------------------*/
UParticleModuleLocationPrimitiveBase::UParticleModuleLocationPrimitiveBase(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	bSpawnModule = true;
	Positive_X = true;
	Positive_Y = true;
	Positive_Z = true;
	Negative_X = true;
	Negative_Y = true;
	Negative_Z = true;
	SurfaceOnly = false;
	Velocity = false;
}

void UParticleModuleLocationPrimitiveBase::InitializeDefaults()
{
	if (!VelocityScale.Distribution)
	{
		UDistributionFloatConstant* DistributionVelocityScale = NewNamedObject<UDistributionFloatConstant>(this, TEXT("DistributionVelocityScale"));
		DistributionVelocityScale->Constant = 1.0f;
		VelocityScale.Distribution = DistributionVelocityScale;
	}

	if (!StartLocation.Distribution)
	{
		UDistributionVectorConstant* DistributionStartLocation = NewNamedObject<UDistributionVectorConstant>(this, TEXT("DistributionStartLocation"));
		DistributionStartLocation->Constant = FVector(0.0f, 0.0f, 0.0f);
		StartLocation.Distribution = DistributionStartLocation;
	}
}

void UParticleModuleLocationPrimitiveBase::PostInitProperties()
{
	Super::PostInitProperties();
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad))
	{
		InitializeDefaults();
	}
}

void UParticleModuleLocationPrimitiveBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_MOVE_DISTRIBUITONS_TO_POSTINITPROPS)
	{
		FDistributionHelpers::RestoreDefaultConstant(VelocityScale.Distribution, TEXT("DistributionVelocityScale"), 1.0f);
		FDistributionHelpers::RestoreDefaultConstant(StartLocation.Distribution, TEXT("DistributionStartLocation"), FVector(0.0f, 0.0f, 0.0f));
	}
}

#if WITH_EDITOR
void UParticleModuleLocationPrimitiveBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	InitializeDefaults();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

void UParticleModuleLocationPrimitiveBase::DetermineUnitDirection(FParticleEmitterInstance* Owner, FVector& vUnitDir, class FRandomStream* InRandomStream)
{
	FVector vRand;

	// Grab 3 random numbers for the axes
	if (InRandomStream == NULL)
	{
		vRand.X	= FMath::SRand();
		vRand.Y = FMath::SRand();
		vRand.Z = FMath::SRand();
	}
	else
	{
		vRand.X	= InRandomStream->GetFraction();
		vRand.Y = InRandomStream->GetFraction();
		vRand.Z = InRandomStream->GetFraction();
	}

	// Set the unit dir
	if (Positive_X && Negative_X)
	{
		vUnitDir.X = vRand.X * 2 - 1;
	}
	else if (Positive_X)
	{
		vUnitDir.X = vRand.X;
	}
	else if (Negative_X)
	{
		vUnitDir.X = -vRand.X;
	}
	else
	{
		vUnitDir.X = 0.0f;
	}

	if (Positive_Y && Negative_Y)
	{
		vUnitDir.Y = vRand.Y * 2 - 1;
	}
	else if (Positive_Y)
	{
		vUnitDir.Y = vRand.Y;
	}
	else if (Negative_Y)
	{
		vUnitDir.Y = -vRand.Y;
	}
	else
	{
		vUnitDir.Y = 0.0f;
	}

	if (Positive_Z && Negative_Z)
	{
		vUnitDir.Z = vRand.Z * 2 - 1;
	}
	else if (Positive_Z)
	{
		vUnitDir.Z = vRand.Z;
	}
	else if (Negative_Z)
	{
		vUnitDir.Z = -vRand.Z;
	}
	else
	{
		vUnitDir.Z = 0.0f;
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleLocationPrimitiveTriangle implementation.
-----------------------------------------------------------------------------*/
UParticleModuleLocationPrimitiveTriangle::UParticleModuleLocationPrimitiveTriangle(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	bSupported3DDrawMode = true;
	bSpawnModule = true;
}

void UParticleModuleLocationPrimitiveTriangle::InitializeDefaults()
{
	if (!StartOffset.Distribution)
	{
		UDistributionVectorConstant* DistributionOffset = NewNamedObject<UDistributionVectorConstant>(this, TEXT("DistributionOffset"));
		DistributionOffset->Constant = FVector::ZeroVector;
		StartOffset.Distribution = DistributionOffset;
	}

	if (!Height.Distribution)
	{
		UDistributionFloatConstant* DistributionHeight = NewNamedObject<UDistributionFloatConstant>(this, TEXT("DistributionHeight"));
		DistributionHeight->Constant = 50.0f;
		Height.Distribution = DistributionHeight;
	}

	if (!Angle.Distribution)
	{
		UDistributionFloatConstant* DistributionAngle = NewNamedObject<UDistributionFloatConstant>(this, TEXT("DistributionAngle"));
		DistributionAngle->Constant = 90.0f;
		Angle.Distribution = DistributionAngle;
	}

	if (!Thickness.Distribution)
	{
		UDistributionFloatConstant* DistributionThickness = NewNamedObject<UDistributionFloatConstant>(this, TEXT("DistributionThickness"));
		DistributionThickness->Constant = 0.0f;
		Thickness.Distribution = DistributionThickness;
	}
}

void UParticleModuleLocationPrimitiveTriangle::PostInitProperties()
{
	Super::PostInitProperties();
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad))
	{
		InitializeDefaults();
	}
}

void UParticleModuleLocationPrimitiveTriangle::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_MOVE_DISTRIBUITONS_TO_POSTINITPROPS)
	{
		FDistributionHelpers::RestoreDefaultConstant(StartOffset.Distribution, TEXT("DistributionOffset"), FVector::ZeroVector);
		FDistributionHelpers::RestoreDefaultConstant(Height.Distribution, TEXT("DistributionHeight"), 50.0f);
		FDistributionHelpers::RestoreDefaultConstant(Angle.Distribution, TEXT("DistributionAngle"), 90.0f);
		FDistributionHelpers::RestoreDefaultConstant(Thickness.Distribution, TEXT("DistributionThickness"), 0.0f);
	}
}

#if WITH_EDITOR
void UParticleModuleLocationPrimitiveTriangle::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	InitializeDefaults();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

void UParticleModuleLocationPrimitiveTriangle::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	SpawnEx(Owner, Offset, SpawnTime, NULL, ParticleBase);
}

void UParticleModuleLocationPrimitiveTriangle::SpawnEx(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, class FRandomStream* InRandomStream, FBaseParticle* ParticleBase)
{
	SPAWN_INIT;
	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);

	FVector TriOffset = StartOffset.GetValue(Owner->EmitterTime, Owner->Component, 0, InRandomStream);
	float TriHeight = Height.GetValue(Owner->EmitterTime, Owner->Component, InRandomStream);
	float TriAngle = Angle.GetValue(Owner->EmitterTime, Owner->Component, InRandomStream);
	float TriThickness = Thickness.GetValue(Owner->EmitterTime, Owner->Component, InRandomStream);
	float BaseLength = TriHeight * FMath::Tan(0.5f * TriAngle * PI / 180.0f);

	FVector Corners[3];
	Corners[0] = TriOffset + FVector(+TriHeight * 0.5f, 0.0f, 0.0f);
	Corners[1] = TriOffset + FVector(-TriHeight * 0.5f, +BaseLength, 0.0f);
	Corners[2] = TriOffset + FVector(-TriHeight * 0.5f, -BaseLength, 0.0f);

	float BarycentricCoords[3] = {0};
	float ZPos = 0.0f;
	if (InRandomStream)
	{
		BarycentricCoords[0] = InRandomStream->GetFraction();
		BarycentricCoords[1] = InRandomStream->GetFraction();
		BarycentricCoords[2] = InRandomStream->GetFraction();
		ZPos = InRandomStream->GetFraction();
	}
	else
	{
		BarycentricCoords[0] = FMath::SRand();
		BarycentricCoords[1] = FMath::SRand();
		BarycentricCoords[2] = FMath::SRand();
		ZPos = FMath::SRand();
	}

	FVector LocationOffset = FVector::ZeroVector;
	float Sum = FMath::Max<float>(KINDA_SMALL_NUMBER, BarycentricCoords[0] + BarycentricCoords[1] + BarycentricCoords[2]);
	for (int32 i = 0; i < 3; i++)
	{
		LocationOffset += (BarycentricCoords[i] / Sum) * Corners[i];
	}
	LocationOffset.Z = ZPos * TriThickness - 0.5f * TriThickness;
	LocationOffset = Owner->EmitterToSimulation.TransformVector(LocationOffset);

	Particle.Location += LocationOffset;
}

void UParticleModuleLocationPrimitiveTriangle::Render3DPreview(FParticleEmitterInstance* Owner, const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
#if WITH_EDITOR
	FMatrix LocalToWorld = FMatrix::Identity;
	if (Owner != NULL)
	{
		LocalToWorld = Owner->EmitterToSimulation * Owner->SimulationToWorld;
	}

	if (StartOffset.Distribution && Height.Distribution && Angle.Distribution && Thickness.Distribution)
	{
		FVector TriOffset = StartOffset.GetValue(0.0f, NULL, 0, NULL);
		float TriHeight = Height.GetValue(0.0f, NULL, NULL);
		float TriAngle = Angle.GetValue(0.0f, NULL, NULL);
		float TriThickness = Thickness.GetValue(0.0f, NULL, NULL);
		float BaseLength = TriHeight * FMath::Tan(0.5f * TriAngle * PI / 180.0f);

		FVector Corners[3];
		Corners[0] = TriOffset + FVector(+TriHeight * 0.5f, 0.0f, 0.0f);
		Corners[1] = TriOffset + FVector(-TriHeight * 0.5f, +BaseLength, 0.0f);
		Corners[2] = TriOffset + FVector(-TriHeight * 0.5f, -BaseLength, 0.0f);
		
		for (int32 i = 0; i < 3; ++i)
		{
			Corners[i] = LocalToWorld.TransformPosition(Corners[i]);
		}
		FVector ThicknessDir(0.0f, 0.0f, 0.5f * TriThickness);
		ThicknessDir = LocalToWorld.TransformVector(ThicknessDir);
		
		FVector CenterPos = Corners[0] / 3.0f + Corners[1] / 3.0f + Corners[2] / 3.0f;
		DrawWireStar(PDI, CenterPos, 10.0f, ModuleEditorColor, SDPG_World);	

		for (int32 i = 0; i < 3; ++i)
		{
			PDI->DrawLine(
				Corners[i] + ThicknessDir,
				Corners[(i+1)%3] + ThicknessDir,
				ModuleEditorColor,
				SDPG_World
				);
			PDI->DrawLine(
				Corners[i] - ThicknessDir,
				Corners[(i+1)%3] - ThicknessDir,
				ModuleEditorColor,
				SDPG_World
				);
			PDI->DrawLine(
				Corners[i] + ThicknessDir,
				Corners[i] - ThicknessDir,
				ModuleEditorColor,
				SDPG_World
				);
		}
	}
#endif	//#if WITH_EDITOR
}

/*-----------------------------------------------------------------------------
	UParticleModuleLocationPrimitiveCylinder implementation.
-----------------------------------------------------------------------------*/
UParticleModuleLocationPrimitiveCylinder::UParticleModuleLocationPrimitiveCylinder(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	RadialVelocity = true;
	bSupported3DDrawMode = true;
	HeightAxis = PMLPC_HEIGHTAXIS_Z;
}

void UParticleModuleLocationPrimitiveCylinder::InitializeDefaults()
{
	if (!StartRadius.Distribution)
	{
		UDistributionFloatConstant* DistributionStartRadius = NewNamedObject<UDistributionFloatConstant>(this, TEXT("DistributionStartRadius"));
		DistributionStartRadius->Constant = 50.0f;
		StartRadius.Distribution = DistributionStartRadius;
	}

	if (!StartHeight.Distribution)
	{
		UDistributionFloatConstant* DistributionStartHeight = NewNamedObject<UDistributionFloatConstant>(this, TEXT("DistributionStartHeight"));
		DistributionStartHeight->Constant = 50.0f;
		StartHeight.Distribution = DistributionStartHeight;
	}
}

void UParticleModuleLocationPrimitiveCylinder::PostInitProperties()
{
	Super::PostInitProperties();
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad))
	{
		InitializeDefaults();
	}
}

void UParticleModuleLocationPrimitiveCylinder::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_MOVE_DISTRIBUITONS_TO_POSTINITPROPS)
	{
		FDistributionHelpers::RestoreDefaultConstant(StartRadius.Distribution, TEXT("DistributionStartRadius"), 50.0f);
		FDistributionHelpers::RestoreDefaultConstant(StartHeight.Distribution, TEXT("DistributionStartHeight"), 50.0f);
	}
}

#if WITH_EDITOR
void UParticleModuleLocationPrimitiveCylinder::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	InitializeDefaults();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

void UParticleModuleLocationPrimitiveCylinder::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	SpawnEx(Owner, Offset, SpawnTime, NULL, ParticleBase);
}

void UParticleModuleLocationPrimitiveCylinder::SpawnEx(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, class FRandomStream* InRandomStream, FBaseParticle* ParticleBase)
{
	SPAWN_INIT;

	int32	RadialIndex0	= 0;	//X
	int32	RadialIndex1	= 1;	//Y
	int32	HeightIndex		= 2;	//Z

	switch (HeightAxis)
	{
	case PMLPC_HEIGHTAXIS_X:
		RadialIndex0	= 1;	//Y
		RadialIndex1	= 2;	//Z
		HeightIndex		= 0;	//X
		break;
	case PMLPC_HEIGHTAXIS_Y:
		RadialIndex0	= 0;	//X
		RadialIndex1	= 2;	//Z
		HeightIndex		= 1;	//Y
		break;
	case PMLPC_HEIGHTAXIS_Z:
		break;
	}

	// Determine the start location for the sphere
	FVector vStartLoc = StartLocation.GetValue(Owner->EmitterTime, Owner->Component, 0, InRandomStream);

	FVector vOffset(0.0f);
	float	fStartRadius	= StartRadius.GetValue(Owner->EmitterTime, Owner->Component, InRandomStream);
	float	fStartHeight	= StartHeight.GetValue(Owner->EmitterTime, Owner->Component, InRandomStream) / 2.0f;


	// Determine the unit direction
	FVector vUnitDir, vUnitDirTemp;

	bool bFoundValidValue = false;
	int32 NumberOfAttempts = 0;
	float RadiusSquared = fStartRadius * fStartRadius;
	while (!bFoundValidValue)
	{
		DetermineUnitDirection(Owner, vUnitDirTemp, InRandomStream);
		vUnitDir[RadialIndex0]	= vUnitDirTemp[RadialIndex0];
		vUnitDir[RadialIndex1]	= vUnitDirTemp[RadialIndex1];
		vUnitDir[HeightIndex]	= vUnitDirTemp[HeightIndex];

		FVector2D CheckVal(vUnitDir[RadialIndex0] * fStartRadius, vUnitDir[RadialIndex1] * fStartRadius);
		if (CheckVal.SizeSquared() <= RadiusSquared)
		{
			bFoundValidValue = true;
		}
		else if (NumberOfAttempts >= 50)
		{
			// Just pass the value thru. 
			// It will clamp to the 'circle' but we tried...
			bFoundValidValue = true;
		}
		NumberOfAttempts++;
	}

	FVector vNormalizedDir = vUnitDir;
	vNormalizedDir.Normalize();

	FVector2D vUnitDir2D(vUnitDir[RadialIndex0], vUnitDir[RadialIndex1]);
	FVector2D vNormalizedDir2D = vUnitDir2D.SafeNormal();

	// Determine the position
	// Always want Z in the [-Height, Height] range
	vOffset[HeightIndex] = vUnitDir[HeightIndex] * fStartHeight;

	vNormalizedDir[RadialIndex0] = vNormalizedDir2D.X;
	vNormalizedDir[RadialIndex1] = vNormalizedDir2D.Y;

	if (SurfaceOnly)
	{
		// Clamp the X,Y to the outer edge...
		if (FMath::IsNearlyZero(FMath::Abs(vOffset[HeightIndex]) - fStartHeight))
		{
			// On the caps, it can be anywhere within the 'circle'
			vOffset[RadialIndex0] = vUnitDir[RadialIndex0] * fStartRadius;
			vOffset[RadialIndex1] = vUnitDir[RadialIndex1] * fStartRadius;
		}
		else
		{
			// On the sides, it must be on the 'circle'
			vOffset[RadialIndex0] = vNormalizedDir[RadialIndex0] * fStartRadius;
			vOffset[RadialIndex1] = vNormalizedDir[RadialIndex1] * fStartRadius;
		}
	}
	else
	{
		vOffset[RadialIndex0] = vUnitDir[RadialIndex0] * fStartRadius;
		vOffset[RadialIndex1] = vUnitDir[RadialIndex1] * fStartRadius;
	}

	// Clamp to the radius...
	FVector	vMax;

	vMax[RadialIndex0]	= FMath::Abs(vNormalizedDir[RadialIndex0]) * fStartRadius;
	vMax[RadialIndex1]	= FMath::Abs(vNormalizedDir[RadialIndex1]) * fStartRadius;
	vMax[HeightIndex]	= fStartHeight;

	vOffset[RadialIndex0]	= FMath::Clamp<float>(vOffset[RadialIndex0], -vMax[RadialIndex0], vMax[RadialIndex0]);
	vOffset[RadialIndex1]	= FMath::Clamp<float>(vOffset[RadialIndex1], -vMax[RadialIndex1], vMax[RadialIndex1]);
	vOffset[HeightIndex]	= FMath::Clamp<float>(vOffset[HeightIndex],  -vMax[HeightIndex],  vMax[HeightIndex]);

	// Add in the start location
	vOffset[RadialIndex0]	+= vStartLoc[RadialIndex0];
	vOffset[RadialIndex1]	+= vStartLoc[RadialIndex1];
	vOffset[HeightIndex]	+= vStartLoc[HeightIndex];

	Particle.Location += Owner->EmitterToSimulation.TransformVector(vOffset);

	if (Velocity)
	{
		FVector vVelocity;
		vVelocity[RadialIndex0]	= vOffset[RadialIndex0]	- vStartLoc[RadialIndex0];
		vVelocity[RadialIndex1]	= vOffset[RadialIndex1]	- vStartLoc[RadialIndex1];
		vVelocity[HeightIndex]	= vOffset[HeightIndex]	- vStartLoc[HeightIndex];

		if (RadialVelocity)
		{
			vVelocity[HeightIndex]	= 0.0f;
		}
		vVelocity	*= VelocityScale.GetValue(Owner->EmitterTime, Owner->Component, InRandomStream);
		vVelocity = Owner->EmitterToSimulation.TransformVector(vVelocity);

		Particle.Velocity		+= vVelocity;
		Particle.BaseVelocity	+= vVelocity;
	}
}

void UParticleModuleLocationPrimitiveCylinder::Render3DPreview(FParticleEmitterInstance* Owner, const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
#if WITH_EDITOR
	// Draw the location as a wire star
	FVector Position = FVector::ZeroVector;
	FVector OwnerScale = FVector(1.0f);
	FMatrix LocalToWorld = FMatrix::Identity;
	if (Owner != NULL)
	{
		LocalToWorld = Owner->EmitterToSimulation * Owner->SimulationToWorld;
		OwnerScale = LocalToWorld.GetScaleVector();
	}

	Position = LocalToWorld.TransformPosition(Position);
	DrawWireStar(PDI, Position, 10.0f, ModuleEditorColor, SDPG_World);

	if (StartLocation.Distribution)
	{
		if (StartLocation.Distribution->IsA(UDistributionVectorConstant::StaticClass()))
		{
			UDistributionVectorConstant* pkConstant = CastChecked<UDistributionVectorConstant>(StartLocation.Distribution);
			Position = pkConstant->Constant;
		}
		else
		if (StartLocation.Distribution->IsA(UDistributionVectorUniform::StaticClass()))
		{
			// Draw at the avg. of the min/max extents
			UDistributionVectorUniform* pkUniform = CastChecked<UDistributionVectorUniform>(StartLocation.Distribution);
			Position = (pkUniform->GetMaxValue() + pkUniform->GetMinValue()) / 2.0f;
		}
		else
		if (StartLocation.Distribution->IsA(UDistributionVectorConstantCurve::StaticClass()))
		{
			// Draw at the avg. of the min/max extents
			UDistributionVectorConstantCurve* pkCurve = CastChecked<UDistributionVectorConstantCurve>(StartLocation.Distribution);

			//pkCurve->
			Position = StartLocation.GetValue(0.0f, Owner->Component);
		}
	}

	// Draw a wire start at the center position
	Position = LocalToWorld.TransformPosition(Position);
	DrawWireStar(PDI,Position, 10.0f, ModuleEditorColor, SDPG_World);

	float fStartRadius = 1.0f;
	float fStartHeight = 1.0f;
	if(Owner && Owner->Component)
	{
		fStartRadius = StartRadius.GetValue(Owner->EmitterTime, Owner->Component);
		fStartHeight = StartHeight.GetValue(Owner->EmitterTime, Owner->Component) / 2.0f;
	}

	FVector	TransformedAxis[3];
	FVector	Axis[3];

	TransformedAxis[0] = LocalToWorld.TransformVector(FVector(1.0f, 0.0f, 0.0f)).SafeNormal();
	TransformedAxis[1] = LocalToWorld.TransformVector(FVector(0.0f, 1.0f, 0.0f)).SafeNormal();
	TransformedAxis[2] = LocalToWorld.TransformVector(FVector(0.0f, 0.0f, 1.0f)).SafeNormal();

	switch (HeightAxis)
	{
	case PMLPC_HEIGHTAXIS_X:
		Axis[0]	= TransformedAxis[1];	//Y
		Axis[1]	= TransformedAxis[2];	//Z
		Axis[2]	= TransformedAxis[0];	//X
		fStartHeight *= OwnerScale.X;
		fStartRadius *= FMath::Max<float>(OwnerScale.Y, OwnerScale.Z);
		break;
	case PMLPC_HEIGHTAXIS_Y:
		Axis[0]	= TransformedAxis[0];	//X
		Axis[1]	= TransformedAxis[2];	//Z
		Axis[2]	= TransformedAxis[1];	//Y
		fStartHeight *= OwnerScale.Y;
		fStartRadius *= FMath::Max<float>(OwnerScale.X, OwnerScale.Z);
		break;
	case PMLPC_HEIGHTAXIS_Z:
		Axis[0]	= TransformedAxis[0];	//X
		Axis[1]	= TransformedAxis[1];	//Y
		Axis[2]	= TransformedAxis[2];	//Z
		fStartHeight *= OwnerScale.Z;
		fStartRadius *= FMath::Max<float>(OwnerScale.X, OwnerScale.Y);
		break;
	}

	DrawWireCylinder(PDI,Position, Axis[0], Axis[1], Axis[2], 
		ModuleEditorColor, fStartRadius, fStartHeight, 16, SDPG_World);
#endif	//#if WITH_EDITOR
}

/*-----------------------------------------------------------------------------
	UParticleModuleLocationPrimitiveCylinder_Seeded implementation.
-----------------------------------------------------------------------------*/
UParticleModuleLocationPrimitiveCylinder_Seeded::UParticleModuleLocationPrimitiveCylinder_Seeded(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	bSpawnModule = true;
	bSupportsRandomSeed = true;
	bRequiresLoopingNotification = true;
}

void UParticleModuleLocationPrimitiveCylinder_Seeded::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	FParticleRandomSeedInstancePayload* Payload = (FParticleRandomSeedInstancePayload*)(Owner->GetModuleInstanceData(this));
	SpawnEx(Owner, Offset, SpawnTime, (Payload != NULL) ? &(Payload->RandomStream) : NULL, ParticleBase);
}

uint32 UParticleModuleLocationPrimitiveCylinder_Seeded::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	return RandomSeedInfo.GetInstancePayloadSize();
}

uint32 UParticleModuleLocationPrimitiveCylinder_Seeded::PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData)
{
	return PrepRandomSeedInstancePayload(Owner, (FParticleRandomSeedInstancePayload*)InstData, RandomSeedInfo);
}

void UParticleModuleLocationPrimitiveCylinder_Seeded::EmitterLoopingNotify(FParticleEmitterInstance* Owner)
{
	if (RandomSeedInfo.bResetSeedOnEmitterLooping == true)
	{
		FParticleRandomSeedInstancePayload* Payload = (FParticleRandomSeedInstancePayload*)(Owner->GetModuleInstanceData(this));
		PrepRandomSeedInstancePayload(Owner, Payload, RandomSeedInfo);
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleLocationPrimitiveSphere implementation.
-----------------------------------------------------------------------------*/
UParticleModuleLocationPrimitiveSphere::UParticleModuleLocationPrimitiveSphere(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	bSupported3DDrawMode = true;
}

void UParticleModuleLocationPrimitiveSphere::InitializeDefaults()
{
	if (!StartRadius.Distribution)
	{
		UDistributionFloatConstant* DistributionStartRadius = NewNamedObject<UDistributionFloatConstant>(this, TEXT("DistributionStartRadius"));
		DistributionStartRadius->Constant = 50.0f;
		StartRadius.Distribution = DistributionStartRadius;
	}
}

void UParticleModuleLocationPrimitiveSphere::PostInitProperties()
{
	Super::PostInitProperties();
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad))
	{
		InitializeDefaults();
	}
}

void UParticleModuleLocationPrimitiveSphere::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_MOVE_DISTRIBUITONS_TO_POSTINITPROPS)
	{
		FDistributionHelpers::RestoreDefaultConstant(StartRadius.Distribution, TEXT("DistributionStartRadius"), 50.0f);
	}
}

#if WITH_EDITOR
void UParticleModuleLocationPrimitiveSphere::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	InitializeDefaults();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

void UParticleModuleLocationPrimitiveSphere::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	SpawnEx(Owner, Offset, SpawnTime, NULL, ParticleBase);
}

void UParticleModuleLocationPrimitiveSphere::SpawnEx(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, class FRandomStream* InRandomStream, FBaseParticle* ParticleBase)
{
	SPAWN_INIT;

	// Determine the start location for the sphere
	FVector vStartLoc = StartLocation.GetValue(Owner->EmitterTime, Owner->Component, 0, InRandomStream);

	// Determine the unit direction
	FVector vUnitDir;
	DetermineUnitDirection(Owner, vUnitDir, InRandomStream);

	FVector vNormalizedDir = vUnitDir;
	vNormalizedDir.Normalize();

	// If we want to cover just the surface of the sphere...
	if (SurfaceOnly)
	{
		vUnitDir.Normalize();
	}

	// Determine the position
	float	fStartRadius	= StartRadius.GetValue(Owner->EmitterTime, Owner->Component, InRandomStream);
	FVector vStartRadius	= FVector(fStartRadius);
	FVector vOffset			= vUnitDir * vStartRadius;

	// Clamp to the radius...
	FVector	vMax;

	vMax.X	= FMath::Abs(vNormalizedDir.X) * fStartRadius;
	vMax.Y	= FMath::Abs(vNormalizedDir.Y) * fStartRadius;
	vMax.Z	= FMath::Abs(vNormalizedDir.Z) * fStartRadius;

	if (Positive_X || Negative_X)
	{
		vOffset.X = FMath::Clamp<float>(vOffset.X, -vMax.X, vMax.X);
	}
	else
	{
		vOffset.X = 0.0f;
	}
	if (Positive_Y || Negative_Y)
	{
		vOffset.Y = FMath::Clamp<float>(vOffset.Y, -vMax.Y, vMax.Y);
	}
	else
	{
		vOffset.Y = 0.0f;
	}
	if (Positive_Z || Negative_Z)
	{
		vOffset.Z = FMath::Clamp<float>(vOffset.Z, -vMax.Z, vMax.Z);
	}
	else
	{
		vOffset.Z = 0.0f;
	}

	vOffset += vStartLoc;
	Particle.Location += Owner->EmitterToSimulation.TransformVector(vOffset);

	if (Velocity)
	{
		FVector vVelocity		 = (vOffset - vStartLoc) * VelocityScale.GetValue(Owner->EmitterTime, Owner->Component, InRandomStream);
		vVelocity = Owner->EmitterToSimulation.TransformVector(vVelocity);
		Particle.Velocity		+= vVelocity;
		Particle.BaseVelocity	+= vVelocity;
	}
}

void UParticleModuleLocationPrimitiveSphere::Render3DPreview(FParticleEmitterInstance* Owner, const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
#if WITH_EDITOR
	FVector Position(0.0f);

	// Draw the location as a wire star
	if ((Owner != NULL) && (Owner->Component != NULL))
	{
		Position = Owner->SimulationToWorld.TransformPosition(Owner->EmitterToSimulation.GetOrigin());
	}
	DrawWireStar(PDI, Position, 10.0f, ModuleEditorColor, SDPG_World);

	if (StartLocation.Distribution)
	{
		if (StartLocation.Distribution->IsA(UDistributionVectorConstant::StaticClass()))
		{
			UDistributionVectorConstant* pkConstant = CastChecked<UDistributionVectorConstant>(StartLocation.Distribution);
			Position = pkConstant->Constant;
		}
		else
		if (StartLocation.Distribution->IsA(UDistributionVectorUniform::StaticClass()))
		{
			// Draw at the avg. of the min/max extents
			UDistributionVectorUniform* pkUniform = CastChecked<UDistributionVectorUniform>(StartLocation.Distribution);
			Position = (pkUniform->GetMaxValue() + pkUniform->GetMinValue()) / 2.0f;
		}
		else
		if (StartLocation.Distribution->IsA(UDistributionVectorConstantCurve::StaticClass()))
		{
			// Draw at the avg. of the min/max extents
			UDistributionVectorConstantCurve* pkCurve = CastChecked<UDistributionVectorConstantCurve>(StartLocation.Distribution);

			//pkCurve->
			Position = StartLocation.GetValue(0.0f, Owner->Component);
		}
	}

	if ((Owner != NULL) && (Owner->Component != NULL))
	{
		Position = Owner->EmitterToSimulation.TransformPosition(Position);
		Position = Owner->SimulationToWorld.TransformPosition(Position);
	}

	// Draw a wire start at the center position
	DrawWireStar(PDI,Position, 10.0f, ModuleEditorColor, SDPG_World);

	float	fRadius		= 1.0f; 
	int32		iNumSides	= 32;
	FVector	vAxis[3];

	if ((Owner != NULL) && (Owner->Component != NULL))
	{
		fRadius = StartRadius.GetValue(Owner->EmitterTime, Owner->Component);
		vAxis[0]	= Owner->SimulationToWorld.TransformVector(Owner->EmitterToSimulation.GetScaledAxis( EAxis::X ));
		vAxis[1]	= Owner->SimulationToWorld.TransformVector(Owner->EmitterToSimulation.GetScaledAxis( EAxis::Y ));
		vAxis[2]	= Owner->SimulationToWorld.TransformVector(Owner->EmitterToSimulation.GetScaledAxis( EAxis::Z ));
	}

	DrawCircle(PDI,Position, vAxis[0], vAxis[1], ModuleEditorColor, fRadius, iNumSides, SDPG_World);
	DrawCircle(PDI,Position, vAxis[0], vAxis[2], ModuleEditorColor, fRadius, iNumSides, SDPG_World);
	DrawCircle(PDI,Position, vAxis[1], vAxis[2], ModuleEditorColor, fRadius, iNumSides, SDPG_World);
#endif	//#if WITH_EDITOR
}

/*-----------------------------------------------------------------------------
	UParticleModuleLocationPrimitiveSphere_Seeded implementation.
-----------------------------------------------------------------------------*/
UParticleModuleLocationPrimitiveSphere_Seeded::UParticleModuleLocationPrimitiveSphere_Seeded(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	bSpawnModule = true;
	bSupportsRandomSeed = true;
	bRequiresLoopingNotification = true;
}

void UParticleModuleLocationPrimitiveSphere_Seeded::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	FParticleRandomSeedInstancePayload* Payload = (FParticleRandomSeedInstancePayload*)(Owner->GetModuleInstanceData(this));
	SpawnEx(Owner, Offset, SpawnTime, (Payload != NULL) ? &(Payload->RandomStream) : NULL, ParticleBase);
}

uint32 UParticleModuleLocationPrimitiveSphere_Seeded::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	return RandomSeedInfo.GetInstancePayloadSize();
}

uint32 UParticleModuleLocationPrimitiveSphere_Seeded::PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData)
{
	return PrepRandomSeedInstancePayload(Owner, (FParticleRandomSeedInstancePayload*)InstData, RandomSeedInfo);
}

void UParticleModuleLocationPrimitiveSphere_Seeded::EmitterLoopingNotify(FParticleEmitterInstance* Owner)
{
	if (RandomSeedInfo.bResetSeedOnEmitterLooping == true)
	{
		FParticleRandomSeedInstancePayload* Payload = (FParticleRandomSeedInstancePayload*)(Owner->GetModuleInstanceData(this));
		PrepRandomSeedInstancePayload(Owner, Payload, RandomSeedInfo);
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleLocationBoneSocket implementation.
-----------------------------------------------------------------------------*/
UParticleModuleLocationBoneSocket::UParticleModuleLocationBoneSocket(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FName NAME_BoneSocketActor;
		FConstructorStatics()
			: NAME_BoneSocketActor(TEXT("BoneSocketActor"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	bSpawnModule = true;
	bUpdateModule = true;
	bFinalUpdateModule = true;
	bUpdateForGPUEmitter = true;
	bSupported3DDrawMode = true;
	SourceType = BONESOCKETSOURCE_Sockets;
	SkelMeshActorParamName = ConstructorStatics.NAME_BoneSocketActor;
	bOrientMeshEmitters = true;
}

void UParticleModuleLocationBoneSocket::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	FModuleLocationBoneSocketInstancePayload* InstancePayload = 
		(FModuleLocationBoneSocketInstancePayload*)(Owner->GetModuleInstanceData(this));
	if (InstancePayload == NULL)
	{
		return;
	}

	if (!InstancePayload->SourceComponent.IsValid())
	{
		// Setup the source skeletal mesh component...
		USkeletalMeshComponent* SkeletalMeshComponent = GetSkeletalMeshComponentSource(Owner);
		if (SkeletalMeshComponent != NULL)
		{
			InstancePayload->SourceComponent = SkeletalMeshComponent;
		}
		else
		{
			return;
		}
	}

	// Early out if source component is still invalid 
	if (!InstancePayload->SourceComponent.IsValid())
	{
		return;
	}
	USkeletalMeshComponent* SourceComponent = InstancePayload->SourceComponent.Get();

	// Determine the bone/socket to spawn at
	int32 SourceIndex = -1;
	if (SelectionMethod == BONESOCKETSEL_Sequential)
	{
		// Simply select the next socket
		SourceIndex = InstancePayload->LastSelectedIndex++;
		if (InstancePayload->LastSelectedIndex >= SourceLocations.Num())
		{
			InstancePayload->LastSelectedIndex = 0;
		}
	}
	else if (SelectionMethod == BONESOCKETSEL_Random)
	{
		// Note: This can select the same socket over and over...
		SourceIndex = FMath::TruncToInt(FMath::SRand() * (SourceLocations.Num() - 1));
		InstancePayload->LastSelectedIndex = SourceIndex;
	}

	if (SourceIndex == -1)
	{
		// Failed to select a socket?
		return;
	}
	if (SourceIndex >= SourceLocations.Num())
	{
		return;
	}

	FVector SourceLocation;
	FQuat RotationQuat;
	const int32 MeshRotationOffset = Owner->GetMeshRotationOffset();
	const bool bMeshRotationActive = MeshRotationOffset > 0 && Owner->IsMeshRotationActive();
	FQuat* SourceRotation = (bMeshRotationActive) ? NULL : &RotationQuat;
	if (GetParticleLocation(Owner, SourceComponent, SourceIndex, SourceLocation, SourceRotation) == true)
	{
		SPAWN_INIT
		{
			FModuleLocationBoneSocketParticlePayload* ParticlePayload = (FModuleLocationBoneSocketParticlePayload*)((uint8*)&Particle + Offset);
			ParticlePayload->SourceIndex = SourceIndex;
			Particle.Location = SourceLocation;
			if(bInheritBoneVelocity)
			{
				// Set the base velocity for this particle.
				int32 BoneIndex = SourceComponent->GetBoneIndex(SourceLocations[SourceIndex].BoneSocketName);
				if (BoneIndex != INDEX_NONE)
				{
					Particle.BaseVelocity = InstancePayload->BoneSocketVelocities[SourceIndex];
				}
			}
			if (bMeshRotationActive)
			{
				FMeshRotationPayloadData* PayloadData = (FMeshRotationPayloadData*)((uint8*)&Particle + MeshRotationOffset);
				PayloadData->Rotation = RotationQuat.Euler();
				if (Owner->CurrentLODLevel->RequiredModule->bUseLocalSpace == true)
				{
					PayloadData->Rotation = Owner->Component->ComponentToWorld.InverseTransformVectorNoScale(PayloadData->Rotation);
				}
			}
		}
	}
}

void UParticleModuleLocationBoneSocket::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	FModuleLocationBoneSocketInstancePayload* InstancePayload = 
		(FModuleLocationBoneSocketInstancePayload*)(Owner->GetModuleInstanceData(this));
	if (!InstancePayload->SourceComponent.IsValid())
	{
		return;
	}

	USkeletalMeshComponent* SourceComponent = InstancePayload->SourceComponent.Get();

	if(bInheritBoneVelocity)
	{
		const float InvDeltaTime = (DeltaTime > 0.0f) ? 1.0f / DeltaTime : 0.0f;

		// Calculate velocities to be used when spawning particles later this frame
		for(int32 SourceIndex = 0; SourceIndex < SourceLocations.Num(); ++SourceIndex)
		{
			int32 BoneIndex = SourceComponent->GetBoneIndex(SourceLocations[SourceIndex].BoneSocketName);
			if (BoneIndex != INDEX_NONE)
			{
				// Calculate the velocity
				const FMatrix WorldBoneTM = SourceComponent->GetBoneMatrix(BoneIndex);
				const FVector Diff = WorldBoneTM.GetOrigin() - InstancePayload->PrevFrameBoneSocketPositions[SourceIndex];
				InstancePayload->BoneSocketVelocities[SourceIndex] = Diff * InvDeltaTime;
			}
		}
	}

	if (bUpdatePositionEachFrame == false)
	{
		return;
	}

	// Particle Data will not exist for GPU sprite emitters.
	if(Owner->ParticleData == NULL)
	{
		return;
	}

	FVector SourceLocation;

	FQuat RotationQuat;
	const int32 MeshRotationOffset = Owner->GetMeshRotationOffset();
	const bool bMeshRotationActive = MeshRotationOffset > 0 && Owner->IsMeshRotationActive();
	FQuat* SourceRotation = (bMeshRotationActive) ? NULL : &RotationQuat;
	BEGIN_UPDATE_LOOP;
	{
		FModuleLocationBoneSocketParticlePayload* ParticlePayload = (FModuleLocationBoneSocketParticlePayload*)((uint8*)&Particle + Offset);
		if (GetParticleLocation(Owner, SourceComponent, ParticlePayload->SourceIndex, SourceLocation, SourceRotation) == true)
		{
			Particle.Location = SourceLocation;
			if (bMeshRotationActive)
			{
				FMeshRotationPayloadData* PayloadData = (FMeshRotationPayloadData*)((uint8*)&Particle + MeshRotationOffset);
				PayloadData->Rotation = RotationQuat.Euler();
				if (Owner->CurrentLODLevel->RequiredModule->bUseLocalSpace == true)
				{
					PayloadData->Rotation = Owner->Component->ComponentToWorld.InverseTransformVectorNoScale(PayloadData->Rotation);
				}
			}
		}
	}
	END_UPDATE_LOOP;
}

void UParticleModuleLocationBoneSocket::FinalUpdate(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	Super::FinalUpdate(Owner, Offset, DeltaTime);

	FModuleLocationBoneSocketInstancePayload* InstancePayload = 
		(FModuleLocationBoneSocketInstancePayload*)(Owner->GetModuleInstanceData(this));
	if (!InstancePayload->SourceComponent.IsValid())
	{
		//@todo. Should we setup the source skeletal mesh component here too??
		return;
	}

	USkeletalMeshComponent* SourceComponent = InstancePayload->SourceComponent.Get();

	if(bInheritBoneVelocity)
	{
		// Store bone positions to be used to calculate velocity on the next frame
		for(int32 SourceIndex = 0; SourceIndex < SourceLocations.Num(); ++SourceIndex)
		{
			int32 BoneIndex = InstancePayload->SourceComponent->GetBoneIndex(SourceLocations[SourceIndex].BoneSocketName);
			if (BoneIndex != INDEX_NONE)
			{
				const FMatrix WorldBoneTM = SourceComponent->GetBoneMatrix(BoneIndex);
				InstancePayload->PrevFrameBoneSocketPositions[SourceIndex] = WorldBoneTM.GetOrigin();
			}
		}
	}

	// Particle Data will not exist for GPU sprite emitters.
	if(Owner->ParticleData == NULL)
	{
		return;
	}

	bool bHaveDeadParticles = false;
	BEGIN_UPDATE_LOOP;
	{
		FModuleLocationBoneSocketParticlePayload* ParticlePayload = (FModuleLocationBoneSocketParticlePayload*)((uint8*)&Particle + Offset);
		if (SourceType == BONESOCKETSOURCE_Sockets)
		{
			if (SourceComponent && SourceComponent->SkeletalMesh)
			{
				USkeletalMeshSocket const* Socket = SourceComponent->SkeletalMesh->FindSocket(SourceLocations[ParticlePayload->SourceIndex].BoneSocketName);
				if (Socket)
				{
					//@todo. Can we make this faster???
					int32 BoneIndex = SourceComponent->GetBoneIndex(Socket->BoneName);
					if (BoneIndex != INDEX_NONE)
					{
						if ((SourceComponent->IsBoneHidden(BoneIndex)) || 
							(SourceComponent->GetBoneTransform(BoneIndex).GetScale3D() == FVector::ZeroVector))
						{
							// Kill it
							Particle.RelativeTime = 1.1f;
							bHaveDeadParticles = true;
						}
					}
				}
			}
		}
	}
	END_UPDATE_LOOP;

	if (bHaveDeadParticles == true)
	{
		Owner->KillParticles();
	}
}

uint32 UParticleModuleLocationBoneSocket::RequiredBytes(FParticleEmitterInstance* Owner)
{
	return sizeof(FModuleLocationBoneSocketParticlePayload);
}

uint32 UParticleModuleLocationBoneSocket::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
    // Memory in addition to the struct size is reserved for the PrevFrameBonePositions and BoneVelocity arrays. 
    // The size of these arrays are fixed to SourceLocations.Num(). FModuleLocationBoneSocketInstancePayload contains
    // an interface to access each array which are setup in PrepPerInstanceBlock to the respective offset into the instance buffer.

	const uint32 ArraySize = SourceLocations.Num();

	// Size to allocate for PrevFrameBoneSocketPositions and BoneSocketVelocity arrays
	const uint32 BoneArraySize = ArraySize * sizeof(FVector) * 2;

	return sizeof(FModuleLocationBoneSocketInstancePayload) + BoneArraySize;
}

uint32 UParticleModuleLocationBoneSocket::PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData)
{
	FModuleLocationBoneSocketInstancePayload* Payload = (FModuleLocationBoneSocketInstancePayload*)InstData;
	if (Payload)
	{
		FMemory::Memzero(Payload, sizeof(FModuleLocationBoneSocketInstancePayload));
		Payload->InitArrayProxies(SourceLocations.Num());
		return 0;
	}

	return 0xffffffff;
}

void UParticleModuleLocationBoneSocket::AutoPopulateInstanceProperties(UParticleSystemComponent* PSysComp)
{
	bool bFound = false;
	for (int32 ParamIdx = 0; ParamIdx < PSysComp->InstanceParameters.Num(); ParamIdx++)
	{
		FParticleSysParam* Param = &(PSysComp->InstanceParameters[ParamIdx]);
		if (Param->Name == SkelMeshActorParamName)
		{
			bFound = true;
			break;
		}
	}

	if (bFound == false)
	{
		int32 NewParamIndex = PSysComp->InstanceParameters.AddZeroed();
		PSysComp->InstanceParameters[NewParamIndex].Name = SkelMeshActorParamName;
		PSysComp->InstanceParameters[NewParamIndex].ParamType = PSPT_Actor;
		PSysComp->InstanceParameters[NewParamIndex].Actor = NULL;
	}
}

#if WITH_EDITOR
int32 UParticleModuleLocationBoneSocket::GetNumberOfCustomMenuOptions() const
{
	return 1;
}

bool UParticleModuleLocationBoneSocket::GetCustomMenuEntryDisplayString(int32 InEntryIndex, FString& OutDisplayString) const
{
	if (InEntryIndex == 0)
	{
		OutDisplayString = NSLOCTEXT("UnrealEd", "Module_LocationBoneSocket_AutoFill", "Auto-fill Bone/Socket Names").ToString();
		return true;
	}
	return false;
}

bool UParticleModuleLocationBoneSocket::PerformCustomMenuEntry(int32 InEntryIndex)
{
	if (GIsEditor == true)
	{
		if (InEntryIndex == 0)
		{
			// Fill in the socket names array with the skeletal mesh 
			if (EditorSkelMesh != NULL)
			{
				if (SourceType == BONESOCKETSOURCE_Sockets)
				{
					const TArray<USkeletalMeshSocket*>& Sockets = EditorSkelMesh->GetActiveSocketList();

					// Retrieve all the sockets
					if (Sockets.Num() > 0)
					{
						SourceLocations.Empty();
						SourceLocations.AddZeroed( Sockets.Num() );

						for (int32 SocketIdx = 0; SocketIdx < Sockets.Num(); SocketIdx++)
						{
							FLocationBoneSocketInfo& Info = SourceLocations[SocketIdx];
							USkeletalMeshSocket* Socket = Sockets[SocketIdx];
							if (Socket != NULL)
							{
								Info.BoneSocketName = Socket->SocketName;
							}
							else
							{
								Info.BoneSocketName = NAME_None;
							}
						}
						return true;
					}
					else
					{
						FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Module_LocationBoneSocket_EditorMeshNoSockets", "Editor mesh has no sockets.") );
					}
				}
				else //BONESOCKETSOURCE_Bones
				{
					// Retrieve all the bones
					if (EditorSkelMesh->RefSkeleton.GetNum() > 0)
					{
						SourceLocations.Empty();
						for (int32 BoneIdx = 0; BoneIdx < EditorSkelMesh->RefSkeleton.GetNum(); BoneIdx++)
						{
							int32 NewItemIdx = SourceLocations.AddZeroed();
							FLocationBoneSocketInfo& Info = SourceLocations[NewItemIdx];
							Info.BoneSocketName = EditorSkelMesh->RefSkeleton.GetBoneName(BoneIdx);
						}
						return true;
					}
					else
					{
						FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Module_LocationBoneSocket_EditorMeshNoBones", "Editor mesh has no bones.") );
					}
				}
			}
			else
			{
				FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Module_LocationBoneSocket_NoEditorMesh", "No editor mesh is set.") );
			}
		}
	}
	return false;
}
#endif

USkeletalMeshComponent* UParticleModuleLocationBoneSocket::GetSkeletalMeshComponentSource(FParticleEmitterInstance* Owner)
{
	if (Owner == NULL)
	{
		return NULL;
	}

	UParticleSystemComponent* PSysComp = Owner->Component;
	if (PSysComp == NULL)
	{
		return NULL;
	}

	AActor* Actor;
	if (PSysComp->GetActorParameter(SkelMeshActorParamName, Actor) == true)
	{
		ASkeletalMeshActor* SkelMeshActor = Cast<ASkeletalMeshActor>(Actor);
		if (SkelMeshActor != NULL)
		{
			return SkelMeshActor->SkeletalMeshComponent;
		}
		else if (Actor)
		{
			USkeletalMeshComponent* SkeletalMeshComponent = Actor->FindComponentByClass<USkeletalMeshComponent>();
			if (SkeletalMeshComponent)
			{
				return SkeletalMeshComponent;
			}
			//@todo. Warn about this...
		}
	}
	return NULL;
}

bool UParticleModuleLocationBoneSocket::GetParticleLocation(FParticleEmitterInstance* Owner, 
	USkeletalMeshComponent* InSkelMeshComponent, int32 InBoneSocketIndex, 
	FVector& OutPosition, FQuat* OutRotation)
{
	check(InSkelMeshComponent);

	if (SourceType == BONESOCKETSOURCE_Sockets)
	{
		if (InSkelMeshComponent->SkeletalMesh)
		{
			USkeletalMeshSocket const* Socket = InSkelMeshComponent->SkeletalMesh->FindSocket(SourceLocations[InBoneSocketIndex].BoneSocketName);
			if (Socket)
			{
				FVector SocketOffset = SourceLocations[InBoneSocketIndex].Offset + UniversalOffset;
				FRotator SocketRotator(0,0,0);
				FMatrix SocketMatrix;
 				if (Socket->GetSocketMatrixWithOffset(SocketMatrix, InSkelMeshComponent, SocketOffset, SocketRotator) == false)
 				{
 					return false;
 				}
 				OutPosition = SocketMatrix.GetOrigin();
 				if (OutRotation != NULL)
 				{
 					SocketMatrix.RemoveScaling();
 					*OutRotation = SocketMatrix.ToQuat();
 				}
			}
			else
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}
	else	//BONESOCKETSOURCE_Bones
	{
		int32 BoneIndex = InSkelMeshComponent->GetBoneIndex(SourceLocations[InBoneSocketIndex].BoneSocketName);
		if (BoneIndex != INDEX_NONE)
		{
			FVector SocketOffset = SourceLocations[InBoneSocketIndex].Offset + UniversalOffset;
			FMatrix WorldBoneTM = InSkelMeshComponent->GetBoneMatrix(BoneIndex);
			FTranslationMatrix OffsetMatrix(SocketOffset);
			FMatrix ResultMatrix = OffsetMatrix * WorldBoneTM;
			OutPosition = ResultMatrix.GetOrigin();
			if (OutRotation != NULL)
			{
				ResultMatrix.RemoveScaling();
				*OutRotation = ResultMatrix.ToQuat();
			}
		}
		else
		{
			return false;
		}
	}

	if (Owner->CurrentLODLevel->RequiredModule->bUseLocalSpace == true)
	{
		OutPosition = Owner->Component->ComponentToWorld.InverseTransformPosition(OutPosition);
	}

	return true;
}

/*-----------------------------------------------------------------------------
	UParticleModuleLocationVertSurface implementation.
-----------------------------------------------------------------------------*/
UParticleModuleLocationSkelVertSurface::UParticleModuleLocationSkelVertSurface(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FName NAME_VertSurfaceActor;
		FConstructorStatics()
			: NAME_VertSurfaceActor(TEXT("VertSurfaceActor"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	bSpawnModule = true;
	bUpdateModule = true;
	bUpdateForGPUEmitter=true;
	bFinalUpdateModule = true;
	bSupported3DDrawMode = true;
	SourceType = VERTSURFACESOURCE_Vert;
	SkelMeshActorParamName = ConstructorStatics.NAME_VertSurfaceActor;
	bOrientMeshEmitters = true;
	bEnforceNormalCheck = false;
}

DEFINE_STAT(STAT_ParticleSkelMeshSurfTime);


void UParticleModuleLocationSkelVertSurface::PostLoad()
{
	Super::PostLoad();

	if(NormalCheckToleranceDegrees > 180.0f)
	{
		NormalCheckToleranceDegrees = 180.0f;
	}
	else if(NormalCheckToleranceDegrees < 0.0f)
	{
		NormalCheckToleranceDegrees = 0.0f;
	}

	NormalCheckTolerance = ((1.0f-(NormalCheckToleranceDegrees/180.0f))*2.0f)-1.0f;
}

#if WITH_EDITOR
void UParticleModuleLocationSkelVertSurface::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if(PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetName() == "NormalCheckToleranceDegrees")
	{
		if(NormalCheckToleranceDegrees > 180.0f)
		{
			NormalCheckToleranceDegrees = 180.0f;
		}
		else if(NormalCheckToleranceDegrees < 0.0f)
		{
			NormalCheckToleranceDegrees = 0.0f;
		}

		NormalCheckTolerance = ((1.0f-(NormalCheckToleranceDegrees/180.0f))*2.0f)-1.0f;
	}
}
#endif // WITH_EDITOR

void UParticleModuleLocationSkelVertSurface::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleSkelMeshSurfTime);
	FModuleLocationVertSurfaceInstancePayload* InstancePayload = 
		(FModuleLocationVertSurfaceInstancePayload*)(Owner->GetModuleInstanceData(this));
	if (InstancePayload == NULL)
	{
		return;
	}

	if (!InstancePayload->SourceComponent.IsValid())
	{
		// Setup the source skeletal mesh component...
		USkeletalMeshComponent* SkeletalMeshComponent = GetSkeletalMeshComponentSource(Owner);
		if (SkeletalMeshComponent != NULL)
		{
			InstancePayload->SourceComponent = SkeletalMeshComponent;
		}
		else
		{
			return;
		}
	}

	// Early out if source component is still invalid 
	if (!InstancePayload->SourceComponent.IsValid())
	{
		return;
	}
	USkeletalMeshComponent* SourceComponent = InstancePayload->SourceComponent.Get();
	FSkeletalMeshResource* SkelMeshResource = SourceComponent ? SourceComponent->GetSkeletalMeshResource() : NULL;
	if (SkelMeshResource == NULL)
	{
		return;
	}

	// Determine the bone/socket to spawn at
	int32 SourceIndex = -1;
	int32 ActiveBoneIndex = -1;
	if (SourceType == VERTSURFACESOURCE_Vert)
	{
		int32 SourceLocationsCount(SkelMeshResource->LODModels[0].VertexBufferGPUSkin.GetNumVertices());

		SourceIndex = FMath::TruncToInt(FMath::SRand() * ((float)SourceLocationsCount) - 1);
		InstancePayload->VertIndex = SourceIndex;

		if(SourceIndex != -1)
		{
			if(!VertInfluencedByActiveBone(Owner, SourceComponent, SourceIndex, &ActiveBoneIndex))
			{
				SPAWN_INIT
				{
					Particle.RelativeTime = 1.1f;
				}

				return;
			}
		}
	}
	else if(SourceType == VERTSURFACESOURCE_Surface)
	{
		FStaticLODModel& LODModel = SkelMeshResource->LODModels[0];
		int32 SectionCount = LODModel.Sections.Num();
		int32 RandomSection = FMath::RoundToInt(FMath::SRand() * ((float)SectionCount-1));

		SourceIndex = LODModel.Sections[RandomSection].BaseIndex +
			(FMath::TruncToInt(FMath::SRand() * ((float)LODModel.Sections[RandomSection].NumTriangles))*3);

		InstancePayload->VertIndex = SourceIndex;

		if(SourceIndex != -1)
		{
			int32 VertIndex[3];

			VertIndex[0] = LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Get( SourceIndex );
			VertIndex[1] = LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Get( SourceIndex+1 );
			VertIndex[2] = LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Get( SourceIndex+2 );

			int32 BoneIndex1, BoneIndex2, BoneIndex3;
			BoneIndex1 = BoneIndex2 = BoneIndex3 = INDEX_NONE;
			if(!VertInfluencedByActiveBone(Owner, SourceComponent, VertIndex[0], &BoneIndex1) &&
			   !VertInfluencedByActiveBone(Owner, SourceComponent, VertIndex[1], &BoneIndex2) && 
			   !VertInfluencedByActiveBone(Owner, SourceComponent, VertIndex[2], &BoneIndex3))
			{
				SPAWN_INIT
				{
					Particle.RelativeTime = 1.1f;
				}

				return;
			}

			// Attempt to retrieve a valid bone index for any of the three verts.
			ActiveBoneIndex = FMath::Max3(BoneIndex1, BoneIndex2, BoneIndex3);
		}
	}

	if (SourceIndex == -1)
	{
		// Failed to select a vert/face?
		return;
	}

	FVector SourceLocation;
	FQuat RotationQuat;
	const int32 MeshRotationOffset = Owner->GetMeshRotationOffset();
	const bool bMeshRotationActive = MeshRotationOffset > 0 && Owner->IsMeshRotationActive();
	FQuat* SourceRotation = (bMeshRotationActive) ? NULL : &RotationQuat;
	if (GetParticleLocation(Owner, SourceComponent, SourceIndex, SourceLocation, SourceRotation, true) == true)
	{
		SPAWN_INIT
		{
			FModuleLocationVertSurfaceParticlePayload* ParticlePayload = (FModuleLocationVertSurfaceParticlePayload*)((uint8*)&Particle + Offset);
			ParticlePayload->SourceIndex = SourceIndex;
			Particle.Location = SourceLocation;
			
			// Set the base velocity
			if(bInheritBoneVelocity && ActiveBoneIndex != INDEX_NONE)
			{
				const int32 VelocityIndex = InstancePayload->ValidAssociatedBoneIndices.Find(ActiveBoneIndex);
				if(VelocityIndex != INDEX_NONE)
				{
					Particle.BaseVelocity = InstancePayload->BoneVelocities[VelocityIndex];
				}
			}

			if (bMeshRotationActive)
			{
				FMeshRotationPayloadData* PayloadData = (FMeshRotationPayloadData*)((uint8*)&Particle + MeshRotationOffset);
				PayloadData->Rotation = RotationQuat.Euler();
				if (Owner->CurrentLODLevel->RequiredModule->bUseLocalSpace == true)
				{
					PayloadData->Rotation = Owner->Component->ComponentToWorld.InverseTransformVectorNoScale(PayloadData->Rotation);
				}
			}
		}
	}
	else
	{
		SPAWN_INIT
		{
			Particle.RelativeTime = 1.1f;
		}
	}
}

void UParticleModuleLocationSkelVertSurface::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleSkelMeshSurfTime);
	
	FModuleLocationVertSurfaceInstancePayload* InstancePayload = 
		(FModuleLocationVertSurfaceInstancePayload*)(Owner->GetModuleInstanceData(this));
	if (!InstancePayload->SourceComponent.IsValid())
	{
		//@todo. Should we setup the source skeletal mesh component here too??
		return;
	}

	USkeletalMeshComponent* SourceComponent = InstancePayload->SourceComponent.Get();

	if(bInheritBoneVelocity)
	{
		const float InvDeltaTime = (DeltaTime > 0.0f) ? 1.0f / DeltaTime : 0.0f;

		// Calculate velocities to be used when spawning particles later this frame
		for(int32 ValidBoneIndex = 0; ValidBoneIndex < InstancePayload->NumValidAssociatedBoneIndices; ++ValidBoneIndex)
		{
			int32 BoneIndex = InstancePayload->ValidAssociatedBoneIndices[ValidBoneIndex];
			if (BoneIndex != INDEX_NONE)
			{
				const FMatrix WorldBoneTM = SourceComponent->GetBoneMatrix(BoneIndex);
				const FVector Diff = WorldBoneTM.GetOrigin() - InstancePayload->PrevFrameBonePositions[ValidBoneIndex];
				InstancePayload->BoneVelocities[ValidBoneIndex] = Diff * InvDeltaTime;
			}
		}
	}

	if (bUpdatePositionEachFrame == false)
	{
		return;
	}

	// Particle Data will not exist for GPU sprite emitters.
	if(Owner->ParticleData == NULL)
	{
		return;
	}

	FVector SourceLocation;
	FQuat RotationQuat;
	const int32 MeshRotationOffset = Owner->GetMeshRotationOffset();
	const bool bMeshRotationActive = MeshRotationOffset > 0 && Owner->IsMeshRotationActive();
	FQuat* SourceRotation = (bMeshRotationActive) ? NULL : &RotationQuat;
	BEGIN_UPDATE_LOOP;
	{
		FModuleLocationVertSurfaceParticlePayload* ParticlePayload = (FModuleLocationVertSurfaceParticlePayload*)((uint8*)&Particle + Offset);
		if (GetParticleLocation(Owner, SourceComponent, ParticlePayload->SourceIndex, SourceLocation, SourceRotation) == true)
		{
			Particle.Location = SourceLocation;
			if (bMeshRotationActive)
			{
				FMeshRotationPayloadData* PayloadData = (FMeshRotationPayloadData*)((uint8*)&Particle + MeshRotationOffset);
				PayloadData->Rotation = RotationQuat.Euler();
				if (Owner->CurrentLODLevel->RequiredModule->bUseLocalSpace == true)
				{
					PayloadData->Rotation = Owner->Component->ComponentToWorld.InverseTransformVectorNoScale(PayloadData->Rotation);
				}
			}
		}
	}
	END_UPDATE_LOOP;
}

void UParticleModuleLocationSkelVertSurface::FinalUpdate(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	Super::FinalUpdate(Owner, Offset, DeltaTime);

	FModuleLocationVertSurfaceInstancePayload* InstancePayload = 
		(FModuleLocationVertSurfaceInstancePayload*)(Owner->GetModuleInstanceData(this));
	if (!InstancePayload->SourceComponent.IsValid())
	{
		return;
	}

	USkeletalMeshComponent* SourceComponent = InstancePayload->SourceComponent.Get();

	if(bInheritBoneVelocity)
	{
		// Save bone positions to be used to calculate velocity on the next frame
		for(int32 ValidBoneIndex = 0; ValidBoneIndex < InstancePayload->NumValidAssociatedBoneIndices; ++ValidBoneIndex)
		{
		
			const int32 BoneIndex = InstancePayload->ValidAssociatedBoneIndices[ValidBoneIndex];
			if (BoneIndex != INDEX_NONE)
			{
				const FMatrix WorldBoneTM = SourceComponent->GetBoneMatrix(BoneIndex);
				InstancePayload->PrevFrameBonePositions[ValidBoneIndex] = WorldBoneTM.GetOrigin();
			}
		}
	}
}

uint32 UParticleModuleLocationSkelVertSurface::PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData)
{
	FModuleLocationVertSurfaceInstancePayload* Payload = (FModuleLocationVertSurfaceInstancePayload*)InstData;
	if (Payload)
	{
		Payload->InitArrayProxies(ValidAssociatedBones.Num());
	}

	UpdateBoneIndicesList(Owner);

	return Super::PrepPerInstanceBlock(Owner, InstData);
}


void UParticleModuleLocationSkelVertSurface::UpdateBoneIndicesList(FParticleEmitterInstance* Owner)
{
	FModuleLocationVertSurfaceInstancePayload* InstancePayload = 
		(FModuleLocationVertSurfaceInstancePayload*)(Owner->GetModuleInstanceData(this));

	AActor* ActorInst = NULL;

	if (Owner->Component->GetActorParameter(SkelMeshActorParamName, ActorInst) && (ActorInst != NULL))
	{
		ASkeletalMeshActor* SkeletalMeshActor = Cast<ASkeletalMeshActor>(ActorInst);

		if ( SkeletalMeshActor != NULL )
		{
			if ( SkeletalMeshActor->SkeletalMeshComponent.IsValid() && (SkeletalMeshActor->SkeletalMeshComponent->SkeletalMesh != NULL) )
			{
				int32 InsertionIndex = 0;
				for (int32 FindBoneIdx = 0; FindBoneIdx < ValidAssociatedBones.Num(); FindBoneIdx++)
				{
					const int32 BoneIdx = SkeletalMeshActor->SkeletalMeshComponent->SkeletalMesh->RefSkeleton.FindBoneIndex(ValidAssociatedBones[FindBoneIdx]);
					if (BoneIdx != INDEX_NONE)
					{
						InstancePayload->ValidAssociatedBoneIndices[InsertionIndex++] = BoneIdx;
					}
				}
				// Cache the number of bone indices on the payload
				InstancePayload->NumValidAssociatedBoneIndices = InsertionIndex;
			}
		}
		// if we have a pawn
		else 
		{
			APawn* Pawn = Cast<APawn>(ActorInst);
			if( Pawn != NULL )
			{
				TArray<USkeletalMeshComponent*> Components;
				Pawn->GetComponents(Components);

				int32 InsertionIndex = 0;
				// look over all of the components looking for a SkelMeshComp and then if we find one we look at it to see if the bones match
				for( int32 CompIdx = 0; CompIdx < Components.Num(); ++CompIdx )
				{
					USkeletalMeshComponent* SkelComp = Components[ CompIdx ];

					if( ( SkelComp->SkeletalMesh != NULL ) && SkelComp->IsRegistered() )
					{
						for (int32 FindBoneIdx = 0; FindBoneIdx < ValidAssociatedBones.Num(); FindBoneIdx++)
						{
							const int32 BoneIdx = SkelComp->SkeletalMesh->RefSkeleton.FindBoneIndex(ValidAssociatedBones[FindBoneIdx]);
							if (BoneIdx != INDEX_NONE)
							{
								InstancePayload->ValidAssociatedBoneIndices[InsertionIndex++] = BoneIdx;
							}
						}
					}
				}
                // Cache the number of bone indices on the payload
				InstancePayload->NumValidAssociatedBoneIndices = InsertionIndex;
			}
		}
	}
}


uint32 UParticleModuleLocationSkelVertSurface::RequiredBytes(FParticleEmitterInstance* Owner)
{
	return sizeof(FModuleLocationVertSurfaceParticlePayload);
}


uint32 UParticleModuleLocationSkelVertSurface::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	// Memory in addition to the struct size is reserved for the ValidAssociatedBoneIndices, PrevFrameBonePositions and BoneVelocity arrays. 
    // The size of these arrays are fixed to ValidAssociatedBones.Num(). Proxys are setup in PrepPerInstanceBlock 
	// to access these arrays
 
	const uint32 ArraySize = ValidAssociatedBones.Num();
	// Allocation size to reserve for ValidAssociatedBonesIndices array
	const uint32 ValidAssociatedBonesIndiceSize = ArraySize * sizeof(int32);
	// Allocation size to reserve for PrevFrameBonePositions, and BoneVelocity arrays
	const uint32 BoneArraySize = ArraySize * sizeof(FVector) * 2;
	return sizeof(FModuleLocationVertSurfaceInstancePayload) + ValidAssociatedBonesIndiceSize + BoneArraySize;
}


void UParticleModuleLocationSkelVertSurface::AutoPopulateInstanceProperties(UParticleSystemComponent* PSysComp)
{
	bool bFound = false;
	for (int32 ParamIdx = 0; ParamIdx < PSysComp->InstanceParameters.Num(); ParamIdx++)
	{
		FParticleSysParam* Param = &(PSysComp->InstanceParameters[ParamIdx]);
		if (Param->Name == SkelMeshActorParamName)
		{
			bFound = true;
			break;
		}
	}

	if (bFound == false)
	{
		int32 NewParamIndex = PSysComp->InstanceParameters.AddZeroed();
		PSysComp->InstanceParameters[NewParamIndex].Name = SkelMeshActorParamName;
		PSysComp->InstanceParameters[NewParamIndex].ParamType = PSPT_Actor;
		PSysComp->InstanceParameters[NewParamIndex].Actor = NULL;
	}
}

#if WITH_EDITOR

int32 UParticleModuleLocationSkelVertSurface::GetNumberOfCustomMenuOptions() const
{
	return 1;
}


bool UParticleModuleLocationSkelVertSurface::GetCustomMenuEntryDisplayString(int32 InEntryIndex, FString& OutDisplayString) const
{
	if (InEntryIndex == 0)
	{
		OutDisplayString = NSLOCTEXT("UnrealEd", "Module_LocationVertSurface_AutoFill", "Auto-fill Bone Names").ToString();
		return true;
	}
	return false;
}


bool UParticleModuleLocationSkelVertSurface::PerformCustomMenuEntry(int32 InEntryIndex)
{
	if (GIsEditor == true)
	{
		if (InEntryIndex == 0)
		{
			// Fill in the socket names array with the skeletal mesh 
			if (EditorSkelMesh != NULL)
			{
				// Retrieve all the bones
				if (EditorSkelMesh->RefSkeleton.GetNum() > 0)
				{
					ValidAssociatedBones.Empty();
					for (int32 BoneIdx = 0; BoneIdx < EditorSkelMesh->RefSkeleton.GetNum(); BoneIdx++)
					{
						int32 NewItemIdx = ValidAssociatedBones.AddZeroed();
						ValidAssociatedBones[NewItemIdx] = EditorSkelMesh->RefSkeleton.GetBoneName(BoneIdx);
					}
				}
				else
				{
					FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Module_LocationBoneSocket_EditorMeshNoBones", "Editor mesh has no bones.") );
				}
			}
			else
			{
				FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Module_LocationBoneSocket_NoEditorMesh", "No editor mesh is set.") );
			}
		}
	}
	return false;
}
#endif

USkeletalMeshComponent* UParticleModuleLocationSkelVertSurface::GetSkeletalMeshComponentSource(FParticleEmitterInstance* Owner)
{
	if (Owner == NULL)
	{
		return NULL;
	}

	UParticleSystemComponent* PSysComp = Owner->Component;
	if (PSysComp == NULL)
	{
		return NULL;
	}

	AActor* Actor;
	if (PSysComp->GetActorParameter(SkelMeshActorParamName, Actor) == true)
	{
		if(Actor == NULL)
		{
			return NULL;
		}
		
		ASkeletalMeshActor* SkelMeshActor = Cast<ASkeletalMeshActor>(Actor);
		if (SkelMeshActor != NULL)
		{
			return SkelMeshActor->SkeletalMeshComponent;
		}
		else
		{
			USkeletalMeshComponent* SkeletalMeshComponent = Actor->FindComponentByClass<USkeletalMeshComponent>();
			if (SkeletalMeshComponent)
			{
				return SkeletalMeshComponent;
			}
			//@todo. Warn about this...
		}
	}

	return NULL;
}


bool UParticleModuleLocationSkelVertSurface::GetParticleLocation(FParticleEmitterInstance* Owner, 
	USkeletalMeshComponent* InSkelMeshComponent, int32 InPrimaryVertexIndex, 
	FVector& OutPosition, FQuat* OutRotation, bool bSpawning /* = false*/)
{
	check(InSkelMeshComponent);
	FSkeletalMeshResource* SkelMeshResource = InSkelMeshComponent->GetSkeletalMeshResource();

	if (SkelMeshResource)
	{
		if (SourceType == VERTSURFACESOURCE_Vert)
		{
			FVector VertPos = InSkelMeshComponent->GetSkinnedVertexPosition(InPrimaryVertexIndex);
			OutPosition = InSkelMeshComponent->ComponentToWorld.TransformPosition(VertPos);
			if (OutRotation != NULL)
			{
				*OutRotation = FQuat::Identity;
			}
		}
		else if (SourceType == VERTSURFACESOURCE_Surface)
		{
			FVector Verts[3];
			int32 VertIndex[3];
			FStaticLODModel& LODModel = SkelMeshResource->LODModels[0];

			VertIndex[0] = LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Get( InPrimaryVertexIndex );
			VertIndex[1] = LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Get( InPrimaryVertexIndex+1 );
			VertIndex[2] = LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Get( InPrimaryVertexIndex+2 );
			Verts[0] = InSkelMeshComponent->ComponentToWorld.TransformPosition(InSkelMeshComponent->GetSkinnedVertexPosition(VertIndex[0]));
			Verts[1] = InSkelMeshComponent->ComponentToWorld.TransformPosition(InSkelMeshComponent->GetSkinnedVertexPosition(VertIndex[1]));
			Verts[2] = InSkelMeshComponent->ComponentToWorld.TransformPosition(InSkelMeshComponent->GetSkinnedVertexPosition(VertIndex[2]));

			if(bEnforceNormalCheck && bSpawning)
			{

				FVector Direction = (Verts[2]-Verts[0]) ^ (Verts[1]-Verts[0]);
				Direction.Normalize();

				float Dot = Direction | NormalToCompare;

				if(Dot < ((2.0f*NormalCheckTolerance)-1.0f))
				{
					return false;
				}

				OutPosition = (Verts[0] + Verts[1] + Verts[2]) / 3.0f;
			}
			else
			{
				FVector VertPos;

				OutPosition = (Verts[0] + Verts[1] + Verts[2]) / 3.0f;
			}

			if (OutRotation != NULL)
			{
				*OutRotation = FQuat::Identity;
			}
		}
	}

	if (Owner->CurrentLODLevel->RequiredModule->bUseLocalSpace == true)
	{
		OutPosition = Owner->Component->ComponentToWorld.InverseTransformPosition(OutPosition);
	}

	OutPosition += UniversalOffset;

	return true;
}


bool UParticleModuleLocationSkelVertSurface::VertInfluencedByActiveBone(FParticleEmitterInstance* Owner, USkeletalMeshComponent* InSkelMeshComponent, int32 InVertexIndex, int32* OutBoneIndex)
{
	FSkeletalMeshResource* SkelMeshResource = InSkelMeshComponent->GetSkeletalMeshResource();
	if (SkelMeshResource)
	{
		FStaticLODModel& Model = SkelMeshResource->LODModels[0];

		FModuleLocationVertSurfaceInstancePayload* InstancePayload = 
			(FModuleLocationVertSurfaceInstancePayload*)(Owner->GetModuleInstanceData(this));

		// Find the chunk and vertex within that chunk, and skinning type, for this vertex.
		int32 ChunkIndex;
		int32 VertIndex;
		bool bSoftVertex;
		bool bHasExtraBoneInfluences;
		Model.GetChunkAndSkinType(InVertexIndex, ChunkIndex, VertIndex, bSoftVertex, bHasExtraBoneInfluences);

		check(ChunkIndex < Model.Chunks.Num());

		if (ValidMaterialIndices.Num() > 0)
		{
			for (int32 SectIdx = 0; SectIdx < Model.Sections.Num(); SectIdx++)
			{
				FSkelMeshSection& Section = Model.Sections[SectIdx];
				if (Section.ChunkIndex == ChunkIndex)
				{
					// Does the material match one of the valid ones
					bool bFound = false;
					for (int32 ValidIdx = 0; ValidIdx < ValidMaterialIndices.Num(); ValidIdx++)
					{
						if (ValidMaterialIndices[ValidIdx] == Section.MaterialIndex)
						{
							bFound = true;
							break;
						}
					}

					if (!bFound)
					{
						// Material wasn't in the valid list...
						return false;
					}
				}
			}
		}

		const FSkelMeshChunk& Chunk = Model.Chunks[ChunkIndex];

		return Chunk.HasExtraBoneInfluences()
			? VertInfluencedByActiveBoneTyped<true>(bSoftVertex, Model, Chunk, VertIndex, InSkelMeshComponent, InstancePayload, OutBoneIndex)
			: VertInfluencedByActiveBoneTyped<false>(bSoftVertex, Model, Chunk, VertIndex, InSkelMeshComponent, InstancePayload, OutBoneIndex);
	}
	return false;
}

template<bool bExtraBoneInfluencesT>
bool UParticleModuleLocationSkelVertSurface::VertInfluencedByActiveBoneTyped(bool bSoftVertex, FStaticLODModel& Model, const FSkelMeshChunk& Chunk, int32 VertIndex, USkeletalMeshComponent* InSkelMeshComponent, FModuleLocationVertSurfaceInstancePayload* InstancePayload, int32* OutBoneIndex)
{
	// Do soft skinning for this vertex.
	if(bSoftVertex)
	{
		const TGPUSkinVertexBase<bExtraBoneInfluencesT>* SrcSoftVertex = Model.VertexBufferGPUSkin.GetVertexPtr<bExtraBoneInfluencesT>(Chunk.GetSoftVertexBufferIndex()+VertIndex);

#if !PLATFORM_LITTLE_ENDIAN
		// uint8[] elements in LOD.VertexBufferGPUSkin have been swapped for VET_UBYTE4 vertex stream use
		for(int32 InfluenceIndex = MAX_INFLUENCES-1;InfluenceIndex >=  MAX_INFLUENCES-Chunk.MaxBoneInfluences;InfluenceIndex--)
#else
		for(int32 InfluenceIndex = 0;InfluenceIndex < Chunk.MaxBoneInfluences;InfluenceIndex++)
#endif
		{
			int32 BoneIndex = Chunk.BoneMap[SrcSoftVertex->InfluenceBones[InfluenceIndex]];
			if(InSkelMeshComponent->MasterPoseComponent.IsValid())
			{		
				check(InSkelMeshComponent->MasterBoneMap.Num() == InSkelMeshComponent->SkeletalMesh->RefSkeleton.GetNum());
				BoneIndex = InSkelMeshComponent->MasterBoneMap[BoneIndex];
			}

			if(!InstancePayload->NumValidAssociatedBoneIndices || InstancePayload->ValidAssociatedBoneIndices.Contains(BoneIndex))
			{
				if(OutBoneIndex)
				{
					*OutBoneIndex = BoneIndex;
				}

				return true;
			}
		}
	}
	// Do rigid (one-influence) skinning for this vertex.
	else
	{
		const int32 RigidInfluenceIndex = SkinningTools::GetRigidInfluenceIndex();
		const TGPUSkinVertexBase<bExtraBoneInfluencesT>* SrcRigidVertex = Model.VertexBufferGPUSkin.GetVertexPtr<bExtraBoneInfluencesT>(Chunk.GetRigidVertexBufferIndex()+VertIndex);

		int32 BoneIndex = Chunk.BoneMap[SrcRigidVertex->InfluenceBones[RigidInfluenceIndex]];
		if(InSkelMeshComponent->MasterPoseComponent.IsValid())
		{
			check(InSkelMeshComponent->MasterBoneMap.Num() == InSkelMeshComponent->SkeletalMesh->RefSkeleton.GetNum());
			BoneIndex = InSkelMeshComponent->MasterBoneMap[BoneIndex];
		}

		if(!InstancePayload->NumValidAssociatedBoneIndices || InstancePayload->ValidAssociatedBoneIndices.Contains(BoneIndex))
		{
			if(OutBoneIndex)
			{
				*OutBoneIndex = BoneIndex;
			}
			return true;
		}
	}

	return false;
}
