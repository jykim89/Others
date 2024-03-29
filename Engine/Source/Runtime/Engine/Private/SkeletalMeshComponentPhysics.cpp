// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "SkeletalRender.h"
#include "SkeletalRenderPublic.h"

#include "MessageLog.h"

#if WITH_PHYSX
	#include "PhysicsEngine/PhysXSupport.h"
	#include "Collision/PhysXCollision.h"
#endif

#include "Collision/CollisionDebugDrawing.h"

#if WITH_APEX
	#include "NxParamUtils.h"
	#include "NxApex.h"

#if WITH_APEX_CLOTHING
	#include "NxClothingAsset.h"
	#include "NxClothingActor.h"
	#include "NxClothingCollision.h"
#endif// #if WITH_APEX_CLOTHING

#endif//#if WITH_APEX

#define LOCTEXT_NAMESPACE "SkeletalMeshComponentPhysics"


void FSkeletalMeshComponentPreClothTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	if ((TickType == LEVELTICK_All) && Target && !Target->HasAnyFlags(RF_PendingKill | RF_Unreachable))
	{
		Target->PreClothTick(DeltaTime);
	}
}

FString FSkeletalMeshComponentPreClothTickFunction::DiagnosticMessage()
{
	return TEXT("FSkeletalMeshComponentPreClothTickFunction");
}


#if WITH_APEX_CLOTHING
//
//	FClothingActor
//
void FClothingActor::Clear(bool bReleaseResource)
{
	if(bReleaseResource)
	{
		GPhysCommandHandler->DeferredRelease(ApexClothingActor);
	}

	ParentClothingAsset = NULL;
	ApexClothingActor = NULL;
}

//
//	USkeletalMesh methods for clothing
//
void USkeletalMesh::LoadClothCollisionVolumes(int32 AssetIndex, NxClothingAsset* ApexClothingAsset)
{
	if(AssetIndex >= ClothingAssets.Num())
	{
		return;
	}

	FClothingAssetData& Asset = ClothingAssets[AssetIndex];

	check(ApexClothingAsset);

	const NxParameterized::Interface* AssetParams = ApexClothingAsset->getAssetNxParameterized();

	// load bone actors
	physx::PxI32 NumBoneActors;
	verify(NxParameterized::getParamArraySize(*AssetParams, "boneActors", NumBoneActors));

	Asset.ClothCollisionVolumes.Empty(NumBoneActors);

	char ParameterName[MAX_SPRINTF];

	physx::PxMat44 PxLocalPose;
	for(int32 i=0; i<NumBoneActors; i++)
	{
		FApexClothCollisionVolumeData& CollisionData = Asset.ClothCollisionVolumes[Asset.ClothCollisionVolumes.AddZeroed()];

		FCStringAnsi::Sprintf(ParameterName, "boneActors[%d].boneIndex", i);
		verify(NxParameterized::getParamI32(*AssetParams, ParameterName, CollisionData.BoneIndex));
		FCStringAnsi::Sprintf(ParameterName, "boneActors[%d].convexVerticesCount", i);
		verify(NxParameterized::getParamU32(*AssetParams, ParameterName, CollisionData.ConvexVerticesCount));
		if(CollisionData.ConvexVerticesCount > 0)
		{
			FCStringAnsi::Sprintf(ParameterName, "boneActors[%d].convexVerticesStart", i);
			verify(NxParameterized::getParamU32(*AssetParams, ParameterName, CollisionData.ConvexVerticesStart));
		}
		else
		{
			FCStringAnsi::Sprintf(ParameterName, "boneActors[%d].capsuleRadius", i);
			verify(NxParameterized::getParamF32(*AssetParams, ParameterName, CollisionData.CapsuleRadius));
			FCStringAnsi::Sprintf(ParameterName, "boneActors[%d].capsuleHeight", i);
			verify(NxParameterized::getParamF32(*AssetParams, ParameterName, CollisionData.CapsuleHeight));
		}

		FCStringAnsi::Sprintf(ParameterName, "boneActors[%d].localPose", i);
		verify(NxParameterized::getParamMat34(*AssetParams, ParameterName, PxLocalPose));

		CollisionData.LocalPose = P2UMatrix(PxLocalPose);
	}

	// load convex data
	physx::PxI32 NumConvexes;
	verify(NxParameterized::getParamArraySize(*AssetParams, "collisionConvexes", NumConvexes));

	Asset.ClothCollisionConvexPlaneIndices.Empty(NumConvexes);

	uint32 PlaneIndex;
	for(int32 i=0; i<NumConvexes; i++)
	{
		FCStringAnsi::Sprintf(ParameterName, "collisionConvexes[%d]", i);
		verify(NxParameterized::getParamU32(*AssetParams, ParameterName, PlaneIndex));
		Asset.ClothCollisionConvexPlaneIndices.Add(PlaneIndex);
	}

	// load plane data
	physx::PxI32 NumPlanes;
	verify(NxParameterized::getParamArraySize(*AssetParams, "bonePlanes", NumPlanes));

	physx::PxVec3 PlaneNormal;
	float		  PlaneDist;

	PxReal PlaneData[4];
	Asset.ClothCollisionVolumePlanes.Empty(NumPlanes);

	for(int32 PlaneIdx=0; PlaneIdx<NumPlanes; PlaneIdx++)
	{
		FClothBonePlane BonePlane;
		FCStringAnsi::Sprintf(ParameterName, "bonePlanes[%d].boneIndex", PlaneIdx);
		verify(NxParameterized::getParamI32(*AssetParams, ParameterName, BonePlane.BoneIndex));
		FCStringAnsi::Sprintf(ParameterName, "bonePlanes[%d].n", PlaneIdx);
		verify(NxParameterized::getParamVec3(*AssetParams, ParameterName, PlaneNormal));
		FCStringAnsi::Sprintf(ParameterName, "bonePlanes[%d].d", PlaneIdx);
		verify(NxParameterized::getParamF32(*AssetParams, ParameterName, PlaneDist));

		for(int i=0; i<3; i++)
		{
			PlaneData[i] = PlaneNormal[i];
		}

		PlaneData[3] = PlaneDist;
		BonePlane.PlaneData = P2UPlane(PlaneData);
		Asset.ClothCollisionVolumePlanes.Add(BonePlane);
	}

	// load bone spheres
	physx::PxI32 NumBoneSpheres;
	verify(NxParameterized::getParamArraySize(*AssetParams, "boneSpheres", NumBoneSpheres));

	Asset.ClothBoneSpheres.Empty(NumBoneSpheres);

	physx::PxVec3 LocalPosForBoneSphere;

	for(int32 i=0; i<NumBoneSpheres; i++)
	{
		FApexClothBoneSphereData& BoneSphere = Asset.ClothBoneSpheres[Asset.ClothBoneSpheres.AddZeroed()];

		FCStringAnsi::Sprintf(ParameterName, "boneSpheres[%d].boneIndex", i);
		verify(NxParameterized::getParamI32(*AssetParams, ParameterName, BoneSphere.BoneIndex));
		FCStringAnsi::Sprintf(ParameterName, "boneSpheres[%d].radius", i);
		verify(NxParameterized::getParamF32(*AssetParams, ParameterName, BoneSphere.Radius));
		FCStringAnsi::Sprintf(ParameterName, "boneSpheres[%d].localPos", i);		
		verify(NxParameterized::getParamVec3(*AssetParams, ParameterName, LocalPosForBoneSphere));
		BoneSphere.LocalPos = P2UVector(LocalPosForBoneSphere);
	}

	// load bone sphere connections, 2 bone spheres become a capsule by this connection info
	physx::PxI32 NumBoneSphereConnections;
	verify(NxParameterized::getParamArraySize(*AssetParams, "boneSphereConnections", NumBoneSphereConnections));

	Asset.BoneSphereConnections.Empty(NumBoneSphereConnections);

	for(int32 i=0; i<NumBoneSphereConnections; i++)
	{
		uint16 &ConnectionIndex = Asset.BoneSphereConnections[Asset.BoneSphereConnections.AddZeroed()];
		FCStringAnsi::Sprintf(ParameterName, "boneSphereConnections[%d]", i);
		verify(NxParameterized::getParamU16(*AssetParams, ParameterName, ConnectionIndex));
	}
}

bool USkeletalMesh::HasClothSections(int32 LODIndex, int AssetIndex)
{
	FSkeletalMeshResource* Resource = GetImportedResource();
	check(Resource->LODModels.IsValidIndex(LODIndex));

	FStaticLODModel& LODModel = Resource->LODModels[LODIndex];
	int32 NumSections = LODModel.Sections.Num();

	for(int32 SecIdx=0; SecIdx < NumSections; SecIdx++)
	{
		uint16 ChunkIdx = LODModel.Sections[SecIdx].ChunkIndex;

		if(LODModel.Chunks[ChunkIdx].CorrespondClothAssetIndex == AssetIndex)
		{
			return true;
		}
	}

	return false;
}

void USkeletalMesh::GetClothSectionIndices(int32 LODIndex, int32 AssetIndex, TArray<uint32>& OutSectionIndices)
{
	FSkeletalMeshResource* Resource = GetImportedResource();

	OutSectionIndices.Empty();

	check(Resource->LODModels.IsValidIndex(LODIndex));

	FStaticLODModel& LODModel = Resource->LODModels[LODIndex];
	int32 NumSections = LODModel.Sections.Num();

	for(int32 SecIdx=0; SecIdx < NumSections; SecIdx++)
	{
		if(LODModel.Chunks[LODModel.Sections[SecIdx].ChunkIndex].CorrespondClothAssetIndex == AssetIndex)
		{
			//add cloth sections
			OutSectionIndices.Add(SecIdx);
		}
	}
}

void USkeletalMesh::GetOriginSectionIndicesWithCloth(int32 LODIndex, TArray<uint32>& OutSectionIndices)
{
	FSkeletalMeshResource* Resource = GetImportedResource();

	OutSectionIndices.Empty();

	check(Resource->LODModels.IsValidIndex(LODIndex));

	FStaticLODModel& LODModel = Resource->LODModels[LODIndex];
	int32 NumSections = LODModel.Sections.Num();

	for(int32 SecIdx=0; SecIdx < NumSections; SecIdx++)
	{
		if(LODModel.Sections[SecIdx].CorrespondClothSectionIndex >= 0)
		{
			//add original sections
			OutSectionIndices.Add(SecIdx);
		}
	}
}

void USkeletalMesh::GetOriginSectionIndicesWithCloth(int32 LODIndex, int32 AssetIndex, TArray<uint32>& OutSectionIndices)
{
	FSkeletalMeshResource* Resource = GetImportedResource();

	OutSectionIndices.Empty();

	check(Resource->LODModels.IsValidIndex(LODIndex));

	FStaticLODModel& LODModel = Resource->LODModels[LODIndex];
	int32 NumSections = LODModel.Sections.Num();

	for(int32 SecIdx=0; SecIdx < NumSections; SecIdx++)
	{
		if(LODModel.Chunks[LODModel.Sections[SecIdx].ChunkIndex].CorrespondClothAssetIndex == AssetIndex)
		{
			//add original sections
			OutSectionIndices.Add(LODModel.Sections[SecIdx].CorrespondClothSectionIndex);
		}
	}
}

bool USkeletalMesh::IsEnabledClothLOD(int32 AssetIndex)
{
	FSkeletalMeshResource* Resource = GetImportedResource();

	FStaticLODModel& LODModel = Resource->LODModels[0];

	TArray<uint32> SectionIndices;
	GetOriginSectionIndicesWithCloth(0, AssetIndex, SectionIndices);

	//if found more than 1 section enabled, return true
	for(int32 i=0; i < SectionIndices.Num(); i++)
	{
		if(LODModel.Sections[SectionIndices[i]].bEnableClothLOD)
		{
			return true;
		}
	}

	return false;
}

int32 USkeletalMesh::GetClothAssetIndex(int32 SectionIndex)
{
	FSkeletalMeshResource* Resource = GetImportedResource();

	FStaticLODModel& LODModel = Resource->LODModels[0];

	//no sections
	if(!LODModel.Sections.IsValidIndex(SectionIndex))
	{
		return -1;
	}
	int16 ClothSecIdx = LODModel.Sections[SectionIndex].CorrespondClothSectionIndex;

	//no mapping
	if(ClothSecIdx < 0)
	{
		return -1;
	}

	int16 ChunkIdx = LODModel.Sections[ClothSecIdx].ChunkIndex;
	return LODModel.Chunks[ChunkIdx].CorrespondClothAssetIndex;
}
#endif//#if WITH_APEX_CLOTHING

//
//	USkeletalMeshComponent
//
UBodySetup* USkeletalMeshComponent::GetBodySetup()
{
	UPhysicsAsset * const PhysicsAsset = GetPhysicsAsset();
	if(SkeletalMesh && PhysicsAsset)
	{
		for(int32 i=0; i<SkeletalMesh->RefSkeleton.GetNum(); i++)
		{
			int32 BodyIndex = PhysicsAsset->FindBodyIndex( SkeletalMesh->RefSkeleton.GetBoneName(i) );
			if(BodyIndex != INDEX_NONE)
			{
				return PhysicsAsset->BodySetup[BodyIndex];
			}
		}
	}	

	return NULL;
}

void USkeletalMeshComponent::SetSimulatePhysics(bool bSimulate)
{
	if ( !bEnablePhysicsOnDedicatedServer && IsRunningDedicatedServer() )
	{
		return;
	}

	// skeletalmeshcomponent BodyInstance is data class
	// we don't instantiate BodyInstance but Bodies
	// however this information is used for Owner data
	BodyInstance.bSimulatePhysics = bSimulate;

	// enable blending physics
	bBlendPhysics = bSimulate;

	for(int32 i=0; i<Bodies.Num(); i++)
	{
		Bodies[i]->UpdateInstanceSimulatePhysics();
	}
}

void USkeletalMeshComponent::OnComponentCollisionSettingsChanged()
{
	for(int32 i=0; i<Bodies.Num(); i++)
	{
		Bodies[i]->UpdatePhysicsFilterData();
	}

	if (SceneProxy)
	{
		((FSkeletalMeshSceneProxy*)SceneProxy)->SetCollisionEnabled_GameThread(IsCollisionEnabled());
	}
}

void USkeletalMeshComponent::AddRadialImpulse(FVector Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff, bool bVelChange)
{
	if(bIgnoreRadialImpulse)
	{
		return;
	}

	for(int32 i=0; i<Bodies.Num(); i++)
	{
		Bodies[i]->AddRadialImpulseToBody(Origin, Radius, Strength, Falloff, bVelChange);
	}
}



void USkeletalMeshComponent::AddRadialForce(FVector Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff)
{
	if(bIgnoreRadialForce)
	{
		return;
	}

	for(int32 i=0; i<Bodies.Num(); i++)
	{
		Bodies[i]->AddRadialForceToBody(Origin, Radius, Strength, Falloff);
	}

}

void USkeletalMeshComponent::WakeAllRigidBodies()
{
	for (int32 i=0; i < Bodies.Num(); i++)
	{
		FBodyInstance* BI = Bodies[i];
		check(BI);
		BI->WakeInstance();
	}
}

void USkeletalMeshComponent::PutAllRigidBodiesToSleep()
{
	for (int32 i=0; i < Bodies.Num(); i++)
	{
		FBodyInstance* BI = Bodies[i];
		check(BI);
		BI->PutInstanceToSleep();
	}
}


bool USkeletalMeshComponent::IsAnyRigidBodyAwake()
{
	bool bAwake = false;

	// ..iterate over each body to find any that are awak
	for(int32 i=0; i<Bodies.Num(); i++)
	{
		FBodyInstance* BI = Bodies[i];
		check(BI);
		if(BI->IsInstanceAwake())
		{
			// Found an awake one - so mesh is considered 'awake'
			bAwake = true;
			continue;
		}
	}

	return bAwake;
}


void USkeletalMeshComponent::SetAllPhysicsLinearVelocity(FVector NewVel, bool bAddToCurrent)
{
	for (int32 i=0; i < Bodies.Num(); i++)
	{
		FBodyInstance* BodyInstance = Bodies[i];
		check(BodyInstance);
		BodyInstance->SetLinearVelocity(NewVel, bAddToCurrent);
	}
}

void USkeletalMeshComponent::SetAllPhysicsAngularVelocity(FVector const& NewAngVel, bool bAddToCurrent)
{
	if(RootBodyIndex < Bodies.Num())
	{
		// Find the root actor. We use its location as the center of the rotation.
		FBodyInstance* RootBodyInst = Bodies[ RootBodyIndex ];
		check(RootBodyInst);
		FTransform RootTM = RootBodyInst->GetUnrealWorldTransform();

		FVector RootPos = RootTM.GetLocation();

		// Iterate over each bone, updating its velocity
		for (int32 i = 0; i < Bodies.Num(); i++)
		{
			FBodyInstance* const BI = Bodies[i];
			check(BI);

			BI->SetAngularVelocity(NewAngVel, bAddToCurrent);
		}
	}
}

void USkeletalMeshComponent::SetAllPhysicsPosition(FVector NewPos)
{
	if(RootBodyIndex < Bodies.Num())
	{
		// calculate the deltas to get the root body to NewPos
		FBodyInstance* RootBI = Bodies[RootBodyIndex];
		check(RootBI);
		if(RootBI->IsValidBodyInstance())
		{
			// move the root body
			FTransform RootBodyTM = RootBI->GetUnrealWorldTransform();
			FVector DeltaLoc = NewPos - RootBodyTM.GetLocation();
			RootBodyTM.SetTranslation(NewPos);
			RootBI->SetBodyTransform(RootBodyTM, true);

#if DO_CHECK
			FVector RelativeVector = (RootBI->GetUnrealWorldTransform().GetLocation() - NewPos);
			check(RelativeVector.SizeSquared() < 1.f);
#endif

			// apply the delta to all the other bodies
			for (int32 i = 0; i < Bodies.Num(); i++)
			{
				if (i != RootBodyIndex)
				{
					FBodyInstance* BI = Bodies[i];
					check(BI);

					FTransform BodyTM = BI->GetUnrealWorldTransform();
					BodyTM.SetTranslation(BodyTM.GetTranslation() + DeltaLoc);
					BI->SetBodyTransform(BodyTM, true);
				}
			}

			// Move component to new physics location
			SyncComponentToRBPhysics();
		}
	}
}

void USkeletalMeshComponent::SetAllPhysicsRotation(FRotator NewRot)
{
	if(RootBodyIndex < Bodies.Num())
	{
		// calculate the deltas to get the root body to NewRot
		FBodyInstance* RootBI = Bodies[RootBodyIndex];
		check(RootBI);
		if(RootBI->IsValidBodyInstance())
		{
			// move the root body
			FQuat NewRotQuat = NewRot.Quaternion();
			FTransform RootBodyTM = RootBI->GetUnrealWorldTransform();
			FQuat DeltaQuat = RootBodyTM.GetRotation().Inverse() * NewRotQuat;
			RootBodyTM.SetRotation(NewRotQuat);
			RootBI->SetBodyTransform(RootBodyTM, true);

			// apply the delta to all the other bodies
			for (int32 i = 0; i < Bodies.Num(); i++)
			{
				if (i != RootBodyIndex)
				{
					FBodyInstance* BI = Bodies[i];
					check(BI);

					FTransform BodyTM = BI->GetUnrealWorldTransform();
					BodyTM.SetRotation(BodyTM.GetRotation() * DeltaQuat);
					BI->SetBodyTransform( BodyTM, true );
				}
			}

			// Move component to new physics location
			SyncComponentToRBPhysics();
		}
	}
}

void USkeletalMeshComponent::SetPhysMaterialOverride(UPhysicalMaterial* NewPhysMaterial)
{
	// Single-body case - just use PrimComp code.
	UPrimitiveComponent::SetPhysMaterialOverride(NewPhysMaterial);

	// Now update any child bodies
	for( int32 i = 0; i < Bodies.Num(); i++ )
	{
		FBodyInstance* BI = Bodies[i];
		BI->UpdatePhysicalMaterials();
	}
}

void USkeletalMeshComponent::InitArticulated(FPhysScene* PhysScene)
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();

	if(PhysScene == NULL || PhysicsAsset == NULL || SkeletalMesh == NULL)
	{
		return;
	}

	if(Bodies.Num() > 0)
	{
		UE_LOG(LogSkeletalMesh, Log, TEXT("InitArticulated: Bodies already created (%s) - call TermArticulated first."), *GetPathName());
		return;
	}

	FVector Scale3D = ComponentToWorld.GetScale3D();
	float Scale = Scale3D.X;

	// Find root physics body
	RootBodyIndex = INDEX_NONE;
	for(int32 i=0; i<SkeletalMesh->RefSkeleton.GetNum(); i++)
	{
		int32 BodyInstIndex = PhysicsAsset->FindBodyIndex( SkeletalMesh->RefSkeleton.GetBoneName(i) );
		if(BodyInstIndex != INDEX_NONE)
		{
			RootBodyIndex = BodyInstIndex;
			break;
		}
	}

	if(RootBodyIndex == INDEX_NONE)
	{
		UE_LOG(LogSkeletalMesh, Log, TEXT("UPhysicsAssetInstance::InitInstance : Could not find root physics body: %s"), *GetName() );
		return;
	}

	// Set up the map from skelmeshcomp ID to collision disable table
#if WITH_PHYSX
	uint32 SkelMeshCompID = GetUniqueID();
	PhysScene->DeferredAddCollisionDisableTable(SkelMeshCompID, &PhysicsAsset->CollisionDisableTable);

	if(Aggregate == NULL && Bodies.Num() > AggregatePhysicsAssetThreshold)
	{
		Aggregate = GPhysXSDK->createAggregate(AggregateMaxSize, true);
	}
#endif //WITH_PHYSX

	// Create all the bodies.
	check(Bodies.Num() == 0);
	int32 NumBodies = PhysicsAsset->BodySetup.Num();
	Bodies.AddZeroed(NumBodies);
	for(int32 i=0; i<NumBodies; i++)
	{
		UBodySetup* BodySetup = PhysicsAsset->BodySetup[i];
		Bodies[i] = new FBodyInstance;
		FBodyInstance* BodyInst = Bodies[i];
		check(BodyInst);

		// Get transform of bone by name.
		int32 BoneIndex = GetBoneIndex( BodySetup->BoneName );
		if(BoneIndex != INDEX_NONE)
		{
			// Copy body setup default instance properties
			BodyInst->CopyBodyInstancePropertiesFrom(&BodySetup->DefaultInstance);
			// we don't allow them to use this in editor. For physics asset, this set up is overriden by Physics Type. 
			// but before we hide in the detail customization, we saved with this being true, causing the simulate always happens for some bodies
			// so adding initialization here to disable this. 
			// to check, please check BodySetupDetails.cpp, if (ChildProperty->GetProperty()->GetName() == TEXT("bSimulatePhysics"))
			// we hide this property, so it should always be false initially. 
			// this is not true for all other BodyInstance, but for physics assets it is true. 
			BodyInst->bSimulatePhysics = false;
			BodyInst->InstanceBodyIndex = i; // Set body index 
#if WITH_PHYSX
			// Create physics body instance.
			FTransform BoneTransform = GetBoneTransform( BoneIndex );
			BodyInst->InitBody( BodySetup, BoneTransform, this, PhysScene, Aggregate);
#endif //WITH_PHYSX
		}
	}

#if WITH_PHYSX
	// add Aggregate into the scene
	if(Aggregate && Aggregate->getNbActors() > 0 && PhysScene)
	{
		// Get the scene type from the SkeletalMeshComponent's BodyInstance
		const uint32 SceneType = BodyInstance.UseAsyncScene() ? PST_Async : PST_Sync;
		PhysScene->GetPhysXScene(SceneType)->addAggregate(*Aggregate);
	}
#endif //WITH_PHYSX

	// Create all the constraints.
	check(Constraints.Num() == 0);
	int32 NumConstraints = PhysicsAsset->ConstraintSetup.Num();
	Constraints.AddZeroed(NumConstraints);
	for(int32 i=0; i<NumConstraints; i++)
	{
		UPhysicsConstraintTemplate* ConstraintSetup = PhysicsAsset->ConstraintSetup[i]; 
		Constraints[i] = new FConstraintInstance;
		FConstraintInstance* ConInst = Constraints[i];
		check( ConInst );
		ConInst->ConstraintIndex = i; // Set the ConstraintIndex property in the ConstraintInstance.
		ConInst->CopyConstraintParamsFrom(&ConstraintSetup->DefaultInstance);

		// Get bodies we want to joint
		FBodyInstance* Body1 = GetBodyInstance(ConInst->ConstraintBone1);
		FBodyInstance* Body2 = GetBodyInstance(ConInst->ConstraintBone2);

		// If we have 2, joint 'em
		if(Body1 != NULL && Body2 != NULL)
		{
			ConInst->InitConstraint(this, Body1, Body2, Scale);
		}
	}

	// Update Flag
	ResetAllBodiesSimulatePhysics();

#if WITH_APEX_CLOTHING
	PrevRootBoneMatrix = GetBoneMatrix(0); // save the root bone transform
	// pre-compute cloth teleport thresholds for performance
	ClothTeleportCosineThresholdInRad = FMath::Cos(FMath::DegreesToRadians(TeleportRotationThreshold));
	ClothTeleportDistThresholdSquared = TeleportDistanceThreshold * TeleportDistanceThreshold;
#endif // #if WITH_APEX_CLOTHING
}


void USkeletalMeshComponent::TermArticulated()
{
#if WITH_PHYSX
	uint32 SkelMeshCompID = GetUniqueID();
	FPhysScene * PhysScene = GetWorld()->GetPhysicsScene();
	if (PhysScene)
	{
		PhysScene->DeferredRemoveCollisionDisableTable(SkelMeshCompID);
	}
#endif	//#if WITH_PHYSX

	// We shut down the physics for each body and constraint here. 
	// The actual UObjects will get GC'd

	for(int32 i=0; i<Constraints.Num(); i++)
	{
		check( Constraints[i] );
		Constraints[i]->TermConstraint();
		delete Constraints[i];
	}

	Constraints.Empty();

	for(int32 i=0; i<Bodies.Num(); i++)
	{
		check( Bodies[i] );
		Bodies[i]->TermBody();
		delete Bodies[i];
	}

	Bodies.Empty();

#if WITH_PHYSX
	// releasing Aggregate, it shouldn't contain any Bodies now, because they are released above
	if(Aggregate)
	{
		check(!Aggregate->getNbActors());
		Aggregate->release();
		Aggregate = NULL;
	}
#endif //WITH_PHYSX
}

void USkeletalMeshComponent::TermBodiesBelow(FName ParentBoneName)
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if(PhysicsAsset && SkeletalMesh && Bodies.Num() > 0)
	{
		check(Bodies.Num() == PhysicsAsset->BodySetup.Num());

		// Get index of parent bone
		int32 ParentBoneIndex = GetBoneIndex(ParentBoneName);
		if(ParentBoneIndex == INDEX_NONE)
		{
			UE_LOG(LogSkeletalMesh, Log, TEXT("TermBodiesBelow: ParentBoneName '%s' is invalid"), *ParentBoneName.ToString());
			return;
		}

		// First terminate any constraints at below this bone
		for(int32 i=0; i<Constraints.Num(); i++)
		{
			// Get bone index of constraint
			FName JointName = Constraints[i]->JointName;
			int32 JointBoneIndex = GetBoneIndex(JointName);

			// If constraint has bone in mesh, and is either the parent or child of it, term it
			if(	JointBoneIndex != INDEX_NONE && (JointName == ParentBoneName ||	SkeletalMesh->RefSkeleton.BoneIsChildOf(JointBoneIndex, ParentBoneIndex)) )
			{
				Constraints[i]->TermConstraint();
			}
		}

		// Then iterate over bodies looking for any which are children of supplied parent
		for(int32 i=0; i<Bodies.Num(); i++)
		{
			// Get bone index of body
			if (Bodies[i]->IsValidBodyInstance())
			{
				FName BodyName = Bodies[i]->BodySetup->BoneName;
				int32 BodyBoneIndex = GetBoneIndex(BodyName);

				// If body has bone in mesh, and is either the parent or child of it, term it
				if(	BodyBoneIndex != INDEX_NONE && (BodyName == ParentBoneName ||	SkeletalMesh->RefSkeleton.BoneIsChildOf(BodyBoneIndex, ParentBoneIndex)) )
				{
					Bodies[i]->TermBody();
				}
			}
		}
	}
}

float USkeletalMeshComponent::GetTotalMassBelowBone(FName InBoneName)
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if(!PhysicsAsset || !SkeletalMesh)
	{
		return 0.f;
	}

	// if physics state is invalid - i.e. collision is disabled - or it does not have a valid bodies, this will crash right away
	if (!IsPhysicsStateCreated()  || !bHasValidBodies)
	{
		return 0.f;
	}

	TArray<int32> BodyIndices;
	PhysicsAsset->GetBodyIndicesBelow(BodyIndices, InBoneName, SkeletalMesh);

	float TotalMass = 0.f;
	for(int32 i=0; i<BodyIndices.Num(); i++)
	{
		TotalMass += Bodies[BodyIndices[i]]->GetBodyMass();
	}

	return TotalMass;
}

void USkeletalMeshComponent::SetAllBodiesSimulatePhysics(bool bNewSimulate)
{
	for(int32 i=0; i<Bodies.Num(); i++)
	{
		Bodies[i]->SetInstanceSimulatePhysics(bNewSimulate);
	}
}


void USkeletalMeshComponent::SetAllBodiesCollisionObjectType(ECollisionChannel NewChannel)
{
	for(int32 i=0; i<Bodies.Num(); i++)
	{
		Bodies[i]->SetObjectType(NewChannel);
	}
}

void USkeletalMeshComponent::SetAllBodiesNotifyRigidBodyCollision(bool bNewNotifyRigidBodyCollision)
{
	for(int32 i=0; i<Bodies.Num(); i++)
	{
		Bodies[i]->SetInstanceNotifyRBCollision(bNewNotifyRigidBodyCollision);
	}
}


void USkeletalMeshComponent::SetAllBodiesBelowSimulatePhysics( const FName& InBoneName, bool bNewSimulate  )
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if( !PhysicsAsset || !SkeletalMesh )
	{
		return;
	}

	// if physics state is invalid - i.e. collision is disabled - or it does not have a valid bodies, this will crash right away
	if (!IsPhysicsStateCreated()  || !bHasValidBodies)
	{
		FMessageLog("PIE").Warning(LOCTEXT("InvalidBodies", "Invalid Bodies : Make sure collision is enabled or root bone has body in PhysicsAsset."));
		return;
	}

	TArray<int32> BodyIndices;
	PhysicsAsset->GetBodyIndicesBelow(BodyIndices, InBoneName, SkeletalMesh);

	for(int32 i=0; i<BodyIndices.Num(); i++)
	{
		//UE_LOG(LogSkeletalMesh, Warning, TEXT( "ForceAllBodiesBelowUnfixed %s" ), *InAsset->BodySetup(BodyIndices(i))->BoneName.ToString() );
		Bodies[BodyIndices[i]]->SetInstanceSimulatePhysics( bNewSimulate );
	}
}


void USkeletalMeshComponent::SetAllMotorsAngularPositionDrive(bool bEnableSwingDrive, bool bEnableTwistDrive, bool bSkipCustomPhysicsType)
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if( !PhysicsAsset )
	{
		return;
	}

	for(int32 i=0; i<Constraints.Num(); i++)
	{
		if( bSkipCustomPhysicsType )
		{
			int32 BodyIndex = PhysicsAsset->FindBodyIndex(Constraints[i]->JointName);
			if( BodyIndex != INDEX_NONE && PhysicsAsset->BodySetup[BodyIndex]->PhysicsType != PhysType_Default)
			{
				continue;
			}
		}

		Constraints[i]->SetAngularPositionDrive(bEnableSwingDrive, bEnableTwistDrive);
	}
}

void USkeletalMeshComponent::SetNamedMotorsAngularPositionDrive(bool bEnableSwingDrive, bool bEnableTwistDrive, const TArray<FName>& BoneNames, bool bSetOtherBodiesToComplement)
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if( !PhysicsAsset )
	{
		return;
	}

	for(int32 i=0; i<Constraints.Num(); i++)
	{
		FConstraintInstance* Instance = Constraints[i];
		if( BoneNames.Contains(Instance->JointName) )
		{
			Constraints[i]->SetAngularPositionDrive(bEnableSwingDrive, bEnableTwistDrive);
		}
		else if( bSetOtherBodiesToComplement )
		{
			Constraints[i]->SetAngularPositionDrive(!bEnableSwingDrive, !bEnableTwistDrive);
		}
	}
}

void USkeletalMeshComponent::SetNamedMotorsAngularVelocityDrive(bool bEnableSwingDrive, bool bEnableTwistDrive, const TArray<FName>& BoneNames, bool bSetOtherBodiesToComplement)
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if( !PhysicsAsset )
	{
		return;
	}

	for(int32 i=0; i<Constraints.Num(); i++)
	{
		FConstraintInstance* Instance = Constraints[i];
		if( BoneNames.Contains(Instance->JointName) )
		{
			Constraints[i]->SetAngularVelocityDrive(bEnableSwingDrive, bEnableTwistDrive);
		}
		else if( bSetOtherBodiesToComplement )
		{
			Constraints[i]->SetAngularVelocityDrive(!bEnableSwingDrive, !bEnableTwistDrive);
		}
	}
}

void USkeletalMeshComponent::SetAllMotorsAngularVelocityDrive(bool bEnableSwingDrive, bool bEnableTwistDrive, bool bSkipCustomPhysicsType)
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if( !PhysicsAsset )
	{
		return;
	}

	for(int32 i=0; i<Constraints.Num(); i++)
	{
		if( bSkipCustomPhysicsType )
		{
			int32 BodyIndex = PhysicsAsset->FindBodyIndex(Constraints[i]->JointName);
			if( BodyIndex != INDEX_NONE && PhysicsAsset->BodySetup[BodyIndex]->PhysicsType != PhysType_Default )
			{
				continue;
			}
		}

		Constraints[i]->SetAngularVelocityDrive(bEnableSwingDrive, bEnableTwistDrive);
	}
}


void USkeletalMeshComponent::SetAllMotorsAngularDriveParams(float InSpring, float InDamping, float InForceLimit, bool bSkipCustomPhysicsType)
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if( !PhysicsAsset )
	{
		return;
	}

	for(int32 i=0; i<Constraints.Num(); i++)
	{
		if( bSkipCustomPhysicsType )
		{
			int32 BodyIndex = PhysicsAsset->FindBodyIndex(Constraints[i]->JointName);
			if( BodyIndex != INDEX_NONE && PhysicsAsset->BodySetup[BodyIndex]->PhysicsType != PhysType_Default )
			{
				continue;
			}
		}
		Constraints[i]->SetAngularDriveParams(InSpring, InDamping, InForceLimit);
	}
}

void USkeletalMeshComponent::ResetAllBodiesSimulatePhysics()
{
	if ( !bEnablePhysicsOnDedicatedServer && IsRunningDedicatedServer() )
	{
		return;
	}

	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if( !PhysicsAsset )
	{
		return;
	}

	// Fix / Unfix bones
	for(int32 i=0; i<Bodies.Num(); i++)
	{
		FBodyInstance*	BodyInst	= Bodies[i];
		UBodySetup*	BodySetup	= BodyInst->BodySetup.Get();

		// Set fixed on any bodies with bAlwaysFullAnimWeight set to true
		if(BodySetup && BodySetup->PhysicsType != PhysType_Default)
		{
			if (BodySetup->PhysicsType == PhysType_Simulated)
			{
				BodyInst->SetInstanceSimulatePhysics(true);
			}
			else
			{
				BodyInst->SetInstanceSimulatePhysics(false);
			}
		}
	}
}

void USkeletalMeshComponent::SetEnablePhysicsBlending(bool bNewBlendPhysics)
{
	bBlendPhysics = bNewBlendPhysics;
}

void USkeletalMeshComponent::SetPhysicsBlendWeight(float PhysicsBlendWeight)
{
	bool bShouldSimulate = PhysicsBlendWeight > 0.f;
	if (bShouldSimulate != IsSimulatingPhysics())
	{
		SetSimulatePhysics(bShouldSimulate);
	}

	// if blend weight is not 1, set manual weight
	if ( PhysicsBlendWeight < 1.f )
	{
		bBlendPhysics = false;
		SetAllBodiesPhysicsBlendWeight (PhysicsBlendWeight, true);
	}
}

void USkeletalMeshComponent::SetAllBodiesPhysicsBlendWeight(float PhysicsBlendWeight, bool bSkipCustomPhysicsType )
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if( !PhysicsAsset )
	{
		return;
	}

	// Fix / Unfix bones
	for(int32 i=0; i<Bodies.Num(); i++)
	{
		FBodyInstance*	BodyInst	= Bodies[i];
		UBodySetup*	BodySetup	= BodyInst->BodySetup.Get();

		// Set fixed on any bodies with bAlwaysFullAnimWeight set to true
		if(BodySetup && (!bSkipCustomPhysicsType || BodySetup->PhysicsType == PhysType_Default) )
		{
			BodyInst->PhysicsBlendWeight = PhysicsBlendWeight;
		}
	}
}


void USkeletalMeshComponent::SetAllBodiesBelowPhysicsBlendWeight( const FName& InBoneName, float PhysicsBlendWeight, bool bSkipCustomPhysicsType )
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if( !PhysicsAsset || !SkeletalMesh )
	{
		return;
	}

	// if physics state is invalid - i.e. collision is disabled - or it does not have a valid bodies, this will crash right away
	if (!IsPhysicsStateCreated()  || !bHasValidBodies)
	{
		FMessageLog("PIE").Warning(LOCTEXT("InvalidBodies", "Invalid Bodies : Make sure collision is enabled or root bone has body in PhysicsAsset."));
		return;
	}

	TArray<int32> BodyIndices;
	PhysicsAsset->GetBodyIndicesBelow(BodyIndices, InBoneName, SkeletalMesh);

	for(int32 i=0; i<BodyIndices.Num(); i++)
	{
		Bodies[BodyIndices[i]]->PhysicsBlendWeight = PhysicsBlendWeight;
	}
}


void USkeletalMeshComponent::AccumulateAllBodiesBelowPhysicsBlendWeight( const FName& InBoneName, float PhysicsBlendWeight, bool bSkipCustomPhysicsType )
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if( !PhysicsAsset || !SkeletalMesh )
	{
		return;
	}

	// if physics state is invalid - i.e. collision is disabled - or it does not have a valid bodies, this will crash right away
	if (!IsPhysicsStateCreated()  || !bHasValidBodies)
	{
		FMessageLog("PIE").Warning(LOCTEXT("InvalidBodies", "Invalid Bodies : Make sure collision is enabled or root bone has body in PhysicsAsset."));
		return;
	}

	TArray<int32> BodyIndices;
	PhysicsAsset->GetBodyIndicesBelow(BodyIndices, InBoneName, SkeletalMesh);

	for(int32 i=0; i<BodyIndices.Num(); i++)
	{
		Bodies[BodyIndices[i]]->PhysicsBlendWeight = FMath::Min(Bodies[BodyIndices[i]]->PhysicsBlendWeight + PhysicsBlendWeight, 1.f);
	}
}

FConstraintInstance* USkeletalMeshComponent::FindConstraintInstance(FName ConName)
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if(PhysicsAsset && PhysicsAsset->ConstraintSetup.Num() == Constraints.Num())
	{
		int32 ConIndex = PhysicsAsset->FindConstraintIndex(ConName);
		if(ConIndex != INDEX_NONE)
		{
			return Constraints[ConIndex];
		}
	}

	return NULL;
}

void USkeletalMeshComponent::OnUpdateTransform(bool bSkipPhysicsMove)
{
	// We are handling the physics move below, so don't handle it at higher levels
	Super::OnUpdateTransform(true);

	// Always send new transform to physics
	if(bPhysicsStateCreated && !bSkipPhysicsMove )
	{
		UpdateKinematicBonesToPhysics(false);
	}

#if WITH_APEX_CLOTHING
	if(ClothingActors.Num() > 0)
	{
		// Updates cloth animation states because transform is updated
		UpdateClothTransform();
	}
#endif //#if WITH_APEX_CLOTHING
}

void USkeletalMeshComponent::UpdateOverlaps(TArray<FOverlapInfo> const* PendingOverlaps, bool bDoNotifies, const TArray<FOverlapInfo>* OverlapsAtEndLocation)
{
		UPrimitiveComponent::UpdateOverlaps(PendingOverlaps, bDoNotifies, OverlapsAtEndLocation);
	}


void USkeletalMeshComponent::CreatePhysicsState()
{
	//	UE_LOG(LogSkeletalMesh, Warning, TEXT("Creating Physics State (%s : %s)"), *GetNameSafe(GetOuter()),  *GetName());
	// Init physics
	InitArticulated(World->GetPhysicsScene());

	USceneComponent::CreatePhysicsState(); // Need to route CreatePhysicsState, skip PrimitiveComponent
}


void USkeletalMeshComponent::DestroyPhysicsState()
{
	TermArticulated();

	Super::DestroyPhysicsState();
}



#if 0 && !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#define DEBUGBROKENCONSTRAINTUPDATE(x) { ##x }
#else
#define DEBUGBROKENCONSTRAINTUPDATE(x)
#endif


void USkeletalMeshComponent::UpdateMeshForBrokenConstraints()
{
	UPhysicsAsset * const PhysicsAsset = GetPhysicsAsset();
	// Needs to have a SkeletalMesh, and PhysicsAsset.
	if( !SkeletalMesh || !PhysicsAsset )
	{
		return;
	}

	DEBUGBROKENCONSTRAINTUPDATE(UE_LOG(LogSkeletalMesh, Log, TEXT("%3.3f UpdateMeshForBrokenConstraints"), GetWorld()->GetTimeSeconds());)

	// Iterate through list of constraints in the physics asset
	for(int32 ConstraintInstIndex = 0; ConstraintInstIndex < Constraints.Num(); ConstraintInstIndex++)
	{
		// See if we can find a constraint that has been terminated (broken)
		FConstraintInstance* ConstraintInst = Constraints[ConstraintInstIndex];
		if( ConstraintInst && ConstraintInst->IsTerminated() )
		{
			// Get the associated joint bone index.
			int32 JointBoneIndex = GetBoneIndex(ConstraintInst->JointName);
			if( JointBoneIndex == INDEX_NONE )
			{
				continue;
			}

			DEBUGBROKENCONSTRAINTUPDATE(UE_LOG(LogSkeletalMesh, Log, TEXT("  Found Broken Constraint: (%d) %s"), JointBoneIndex, *PhysicsAsset->ConstraintSetup(ConstraintInstIndex)->JointName.ToString());)

			// Get child bodies of this joint
			for(int32 BodySetupIndex = 0; BodySetupIndex < PhysicsAsset->BodySetup.Num(); BodySetupIndex++)
			{
				UBodySetup* BodySetup = PhysicsAsset->BodySetup[BodySetupIndex];
				int32 BoneIndex = GetBoneIndex(BodySetup->BoneName);
				if( BoneIndex != INDEX_NONE && 
					(BoneIndex == JointBoneIndex || SkeletalMesh->RefSkeleton.BoneIsChildOf(BoneIndex, JointBoneIndex)) )
				{
					DEBUGBROKENCONSTRAINTUPDATE(UE_LOG(LogSkeletalMesh, Log, TEXT("    Found Child Bone: (%d) %s"), BoneIndex, *BodySetup->BoneName.ToString());)

					FBodyInstance* ChildBodyInst = Bodies[BodySetupIndex];
					if( ChildBodyInst )
					{
						// Unfix Body so, it is purely physical, not kinematic.
						if( !ChildBodyInst->IsInstanceSimulatingPhysics() )
						{
							DEBUGBROKENCONSTRAINTUPDATE(UE_LOG(LogSkeletalMesh, Log, TEXT("      Unfixing body."));)
							ChildBodyInst->SetInstanceSimulatePhysics(true);
						}
					}

					FConstraintInstance* ChildConstraintInst = FindConstraintInstance(BodySetup->BoneName);
					if( ChildConstraintInst )
					{
						if( ChildConstraintInst->bLinearPositionDrive )
						{
							DEBUGBROKENCONSTRAINTUPDATE(UE_LOG(LogSkeletalMesh, Log, TEXT("      Turning off LinearPositionDrive."));)
							ChildConstraintInst->SetLinearPositionDrive(false, false, false);
						}
						if( ChildConstraintInst->bLinearVelocityDrive )
						{
							DEBUGBROKENCONSTRAINTUPDATE(UE_LOG(LogSkeletalMesh, Log, TEXT("      Turning off LinearVelocityDrive."));)
							ChildConstraintInst->SetLinearVelocityDrive(false, false, false);
						}
						if( ChildConstraintInst->bAngularOrientationDrive )
						{
							DEBUGBROKENCONSTRAINTUPDATE(UE_LOG(LogSkeletalMesh, Log, TEXT("      Turning off AngularPositionDrive."));)
							ChildConstraintInst->SetAngularPositionDrive(false, false);
						}
						if( ChildConstraintInst->bAngularVelocityDrive )
						{
							DEBUGBROKENCONSTRAINTUPDATE(UE_LOG(LogSkeletalMesh, Log, TEXT("      Turning off AngularVelocityDrive."));)
							ChildConstraintInst->SetAngularVelocityDrive(false, false);
						}
					}
				}
			}
		}
	}
}


int32 USkeletalMeshComponent::FindConstraintIndex( FName ConstraintName )
{
	UPhysicsAsset * const PhysicsAsset = GetPhysicsAsset();
	return PhysicsAsset ? PhysicsAsset->FindConstraintIndex(ConstraintName) : INDEX_NONE;
}


FName USkeletalMeshComponent::FindConstraintBoneName( int32 ConstraintIndex )
{
	UPhysicsAsset * const PhysicsAsset = GetPhysicsAsset();
	return PhysicsAsset ? PhysicsAsset->FindConstraintBoneName(ConstraintIndex) : NAME_None;
}


FBodyInstance* USkeletalMeshComponent::GetBodyInstance(FName BoneName) const
{
	UPhysicsAsset * const PhysicsAsset = GetPhysicsAsset();
	FBodyInstance* BodyInst = NULL;

	if(PhysicsAsset != NULL)
	{
		// A name of NAME_None indicates 'root body'
		if(BoneName == NAME_None)
		{
			if(Bodies.IsValidIndex(RootBodyIndex))
			{
				BodyInst = Bodies[RootBodyIndex];
			}
		}
		// otherwise, look for the body
		else
		{
			int32 BodyIndex = PhysicsAsset->FindBodyIndex(BoneName);
			if(Bodies.IsValidIndex(BodyIndex))
			{
				BodyInst = Bodies[BodyIndex];
			}
		}

	}

	return BodyInst;
}


void USkeletalMeshComponent::BreakConstraint(FVector Impulse, FVector HitLocation, FName InBoneName)
{
	// you can enable/disable the instanced weights by calling
	int32 ConstraintIndex = FindConstraintIndex(InBoneName);
	if( ConstraintIndex == INDEX_NONE )
	{
		return;
	}

	FConstraintInstance* Constraint = Constraints[ConstraintIndex];
	// If already broken, our job has already been done. Bail!
	if( Constraint->IsTerminated() )
	{
		return;
	}

	UPhysicsAsset * const PhysicsAsset = GetPhysicsAsset();

	// Figure out if Body is fixed or not
	FBodyInstance* Body = GetBodyInstance(Constraint->JointName);

	if( Body != NULL && Body->IsInstanceSimulatingPhysics() )
	{
		// Unfix body so it can be broken.
		Body->SetInstanceSimulatePhysics(true);
	}

	// Break Constraint
	Constraint->TermConstraint();
	// Make sure child bodies and constraints are released and turned to physics.
	UpdateMeshForBrokenConstraints();
	// Add impulse to broken limb
	AddImpulseAtLocation(Impulse, HitLocation, InBoneName);
}


void USkeletalMeshComponent::SetPhysicsAsset(UPhysicsAsset* InPhysicsAsset, bool bForceReInit)
{
	// If this is different from what we have now, or we should have an instance but for whatever reason it failed last time, teardown/recreate now.
	if(bForceReInit || InPhysicsAsset != GetPhysicsAsset())
	{
		// SkelComp had a physics instance, then terminate it.
		TermArticulated();

		// Need to update scene proxy, because it keeps a ref to the PhysicsAsset.
		Super::SetPhysicsAsset(InPhysicsAsset, bForceReInit);
		MarkRenderStateDirty();

		// Update bHasValidBodies flag
		UpdateHasValidBodies();

		// Component should be re-attached here, so create physics.
		if( SkeletalMesh )
		{
			// Because we don't know what bones the new PhysicsAsset might want, we have to force an update to _all_ bones in the skeleton.
			RequiredBones.Reset(SkeletalMesh->RefSkeleton.GetNum());
			RequiredBones.AddUninitialized( SkeletalMesh->RefSkeleton.GetNum() );
			for(int32 i=0; i<SkeletalMesh->RefSkeleton.GetNum(); i++)
			{
				RequiredBones[i] = (FBoneIndexType)i;
			}
			RefreshBoneTransforms();

			// Initialize new Physics Asset
			if(World->GetPhysicsScene() != NULL && ShouldCreatePhysicsState())
			{
			//	UE_LOG(LogSkeletalMesh, Warning, TEXT("Creating Physics State (%s : %s)"), *GetNameSafe(GetOuter()),  *GetName());			
				InitArticulated(World->GetPhysicsScene());
			}
		}
		else
		{
			// If PhysicsAsset hasn't been instanced yet, just update the template.
			Super::SetPhysicsAsset(InPhysicsAsset, bForceReInit);

			// Update bHasValidBodies flag
			UpdateHasValidBodies();
		}

		// Indicate that 'required bones' array will need to be recalculated.
		bRequiredBonesUpToDate = false;
	}
}


void USkeletalMeshComponent::UpdateHasValidBodies()
{
	// First clear out old data
	bHasValidBodies = false;

	const UPhysicsAsset * const PhysicsAsset = GetPhysicsAsset();

	// If we have a physics asset..
	if(PhysicsAsset != NULL)
	{
		// For each body in physics asset..
		for( int32 BodyIndex = 0; BodyIndex < PhysicsAsset->BodySetup.Num(); BodyIndex++ )
		{
			// .. find the matching graphics bone index
			int32 BoneIndex = GetBoneIndex( PhysicsAsset->BodySetup[ BodyIndex ]->BoneName );

			// If we found a valid graphics bone, set the 'valid' flag
			if(BoneIndex != INDEX_NONE)
			{
				bHasValidBodies = true;
				break;
			}
		}
	}
}

void USkeletalMeshComponent::UpdatePhysicsToRBChannels()
{
	// Iterate over each bone/body.
	for (int32 i = 0; i < Bodies.Num(); i++)
	{
		FBodyInstance* BI = Bodies[i];
		check(BI);
		BI->UpdatePhysicsFilterData();
	}
}

//////////////////////////////////////////////////////////////////////////
// COLLISION

extern float DebugLineLifetime;

bool USkeletalMeshComponent::LineTraceComponent(struct FHitResult& OutHit, const FVector Start, const FVector End, const struct FCollisionQueryParams& Params)
{
	UPhysicsAsset * const PhysicsAsset = GetPhysicsAsset();
	bool bHaveHit = false;

	float MinTime = MAX_FLT;
	FHitResult Hit;
	for (int32 BodyIdx=0; BodyIdx < Bodies.Num(); ++BodyIdx)
	{
		if (Bodies[BodyIdx]->LineTrace(Hit, Start, End, Params.bTraceComplex, Params.bReturnPhysicalMaterial))
		{
			bHaveHit = true;
			if(MinTime > Hit.Time)
			{
				MinTime = Hit.Time;
				OutHit = Hit;
			}
		}
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if((GetWorld()->DebugDrawTraceTag != NAME_None) && (GetWorld()->DebugDrawTraceTag == Params.TraceTag))
	{
		TArray<FHitResult> Hits;
		if (bHaveHit)
		{
			Hits.Add(OutHit);
		}
		DrawLineTraces(GetWorld(), Start, End, Hits, DebugLineLifetime);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	return bHaveHit;
}

bool USkeletalMeshComponent::SweepComponent( FHitResult& OutHit, const FVector Start, const FVector End, const FCollisionShape& CollisionShape, bool bTraceComplex)
{
	bool bHaveHit = false;

	for (int32 BodyIdx=0; BodyIdx < Bodies.Num(); ++BodyIdx)
	{
		if (Bodies[BodyIdx]->Sweep(OutHit, Start, End, CollisionShape, bTraceComplex))
		{
			bHaveHit = true;
		}
	}

	return bHaveHit;
}

bool USkeletalMeshComponent::ComponentOverlapComponent(class UPrimitiveComponent* PrimComp,const FVector Pos,const FRotator Rot,const struct FCollisionQueryParams& Params)
{
#if WITH_PHYSX
	// will have to do per component - default single body or physicsinstance 
	const PxRigidActor* TargetRigidBody = (PrimComp)? PrimComp->BodyInstance.GetPxRigidActor():NULL;
	if (TargetRigidBody==NULL || TargetRigidBody->getNbShapes()==0)
	{
		return false;
	}

	// if target is skeletalmeshcomponent and do not support singlebody physics
	USkeletalMeshComponent * OtherComp = Cast<USkeletalMeshComponent>(PrimComp);
	if (OtherComp)
	{
		UE_LOG(LogCollision, Log, TEXT("ComponentOverlapComponent : (%s) Does not support skeletalmesh with Physics Asset"), *PrimComp->GetPathName());
		return false;
	}

	// calculate the test global pose of the actor
	PxTransform PTestGlobalPose = U2PTransform(FTransform(Rot, Pos));

	// Get all the shapes from the actor
	TArray<PxShape*> PTargetShapes;
	PTargetShapes.AddZeroed(TargetRigidBody->getNbShapes());
	int32 NumTargetShapes = TargetRigidBody->getShapes(PTargetShapes.GetData(), PTargetShapes.Num());

	bool bHaveOverlap = false;

	for (int32 TargetShapeIdx=0; TargetShapeIdx<PTargetShapes.Num(); ++TargetShapeIdx)
	{
		const PxShape * PTargetShape = PTargetShapes[TargetShapeIdx];
		check (PTargetShape);

		// Calc shape global pose
		PxTransform PShapeGlobalPose = PTestGlobalPose.transform(PTargetShape->getLocalPose());

		GET_GEOMETRY_FROM_SHAPE(PGeom, PTargetShape);

		if(PGeom != NULL)
		{
			for (int32 BodyIdx=0; BodyIdx < Bodies.Num(); ++BodyIdx)
			{
				bHaveOverlap = Bodies[BodyIdx]->Overlap(*PGeom, PShapeGlobalPose);
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				if((GetWorld()->DebugDrawTraceTag != NAME_None) && (GetWorld()->DebugDrawTraceTag == Params.TraceTag))
				{
					TArray<FOverlapResult> Overlaps;
					if (bHaveOverlap)
					{
						FOverlapResult Result;
						Result.Component = PrimComp;
						Result.Actor = PrimComp->GetOwner();
						Result.bBlockingHit = true;
						Overlaps.Add(Result);
					}

					DrawGeomOverlaps(GetWorld(), *PGeom, PShapeGlobalPose, Overlaps, DebugLineLifetime);
				}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				if (bHaveOverlap)
				{
					break;
				}
			}
		}
		return bHaveOverlap;
	}
#endif //WITH_PHYSX
	return false;
}

bool USkeletalMeshComponent::OverlapComponent(const FVector& Pos, const FQuat& Rot, const FCollisionShape& CollisionShape)
{

#if WITH_PHYSX
	PxSphereGeometry PSphereGeom;
	PxBoxGeometry PBoxGeom;
	PxCapsuleGeometry PCapsuleGeom;
	PxGeometry * PGeom = NULL;
	PxTransform PShapePose;

	switch (CollisionShape.ShapeType)
	{
	case ECollisionShape::Sphere:
		{
			PSphereGeom = PxSphereGeometry(CollisionShape.GetSphereRadius()) ;
			// calculate the test global pose of the actor
			PShapePose = U2PTransform(FTransform(Rot, Pos));
			PGeom = &PSphereGeom;
			break;
		}
	case ECollisionShape::Box:
		{
			PBoxGeom = PxBoxGeometry(U2PVector(CollisionShape.GetBox()));
			PShapePose = U2PTransform(FTransform(Rot, Pos));
			PGeom = &PBoxGeom;
			break;
		}
	case ECollisionShape::Capsule:
		{
			PCapsuleGeom = PxCapsuleGeometry( CollisionShape.GetCapsuleRadius(), CollisionShape.GetCapsuleAxisHalfLength() );
			PShapePose = ConvertToPhysXCapsulePose( FTransform(Rot,Pos) );
			PGeom = &PCapsuleGeom;
			break;
		}
	default:
		// invalid point
		ensure(false);
	}

	if (PGeom)
	{
		for (int32 BodyIdx=0; BodyIdx < Bodies.Num(); ++BodyIdx)
		{
			if (Bodies[BodyIdx]->Overlap(*PGeom, PShapePose))
			{
				return true;
			}
		}
	}
#endif //WITH_PHYSX

	return false;
}

bool USkeletalMeshComponent::ComponentOverlapMulti(TArray<struct FOverlapResult>& OutOverlaps, const UWorld* World, const FVector& Pos, const FRotator& Rot, ECollisionChannel TestChannel, const struct FComponentQueryParams& Params, const struct FCollisionObjectQueryParams& ObjectQueryParams) const
{
#if WITH_PHYSX

	OutOverlaps.Reset();

	if(!Bodies.IsValidIndex(RootBodyIndex))
	{
		return false;
	}

	// calculate the test global pose of the actor
	// because we're using skeletal mesh we need to use relative transform to root body
	PxTransform PTestGlobalPose = U2PTransform(FTransform(Rot, Pos));
	check(Bodies[RootBodyIndex]);
	PxTransform PRootGlobalPoseInv = U2PTransform(Bodies[RootBodyIndex]->GetUnrealWorldTransform()).getInverse();
	PxTransform PGlobalRootSpacePose = PTestGlobalPose.transform(PRootGlobalPoseInv);

	bool bHaveBlockingHit = false;

	for(int32 BodyIdx = 0; BodyIdx < Bodies.Num(); ++BodyIdx)
	{
		const FBodyInstance * BodyInstance = Bodies[BodyIdx];
		PxRigidActor* PRigidActor = BodyInstance->GetPxRigidActor();

		if(PRigidActor == NULL)
		{
			UE_LOG(LogCollision, Log, TEXT("ComponentOverlapMulti : (%s) No physics data"), *GetPathName());
			return false;
		}

		// Get all the shapes from the actor
		{
			SCOPED_SCENE_READ_LOCK(PRigidActor->getScene());

			TArray<PxShape*, TInlineAllocator<8>> PShapes;
			PShapes.AddZeroed(PRigidActor->getNbShapes());
			int32 NumShapes = PRigidActor->getShapes(PShapes.GetData(), PShapes.Num());

			// Iterate over each shape
			for(int32 ShapeIdx=0; ShapeIdx<PShapes.Num(); ShapeIdx++)
			{
				PxShape* PShape = PShapes[ShapeIdx];
				check(PShape);
				TArray<struct FOverlapResult> Overlaps;
				// Calc shape global pose
				PxTransform PLocalPose = PShape->getLocalPose();
				PxTransform PBodyPoseGlobal = U2PTransform(BodyInstance->GetUnrealWorldTransform());
				PxTransform PShapeGlobalPose = PGlobalRootSpacePose.transform(PBodyPoseGlobal.transform(PLocalPose));

				GET_GEOMETRY_FROM_SHAPE(PGeom, PShape);

				if(PGeom != NULL)
				{
					// if object query param isn't valid, it will use trace channel
					if (GeomOverlapMulti(World, *PGeom, PShapeGlobalPose, Overlaps, TestChannel, Params, FCollisionResponseParams(GetCollisionResponseToChannels()), ObjectQueryParams))
					{
						bHaveBlockingHit = true;
					}

					OutOverlaps.Append(Overlaps);
				}
			}
		}
	}

	return bHaveBlockingHit;
#endif //WITH_PHYSX
	return false;
}


#if WITH_APEX_CLOTHING
void USkeletalMeshComponent::AddClothingBounds(FBoxSphereBounds& InOutBounds) const
{
	int32 NumAssets = ClothingActors.Num();

	for(int32 i=0; i < NumAssets; i++)
	{
		if(IsValidClothingActor(i))
		{
			NxClothingActor* Actor = ClothingActors[i].ApexClothingActor;

			if(Actor)
			{
				physx::PxBounds3 ApexClothingBounds = Actor->getBounds();

				if (!ApexClothingBounds.isEmpty())
				{
					FBoxSphereBounds BoxBounds = FBox( P2UVector(ApexClothingBounds.minimum), P2UVector(ApexClothingBounds.maximum) );
					InOutBounds = InOutBounds + BoxBounds;
				}
			}
		}
	}
}

bool USkeletalMeshComponent::HasValidClothingActors()
{
	for(int32 i=0; i < ClothingActors.Num(); i++)
	{
		if(IsValidClothingActor(i))
		{
			return true;
		}
	}

	return false;
}

//if any changes in clothing assets, will create new actors 
void USkeletalMeshComponent::ValidateClothingActors()
{
	//newly spawned component's tick group could be "specified tick group + 1" for the first few frames
	//but it comes back to original tick group soon
	if(PrimaryComponentTick.GetActualTickGroup() != PrimaryComponentTick.TickGroup)
	{
		return;
	}

	if(!SkeletalMesh)
	{
		return;
	}

	int32 NumAssets = SkeletalMesh->ClothingAssets.Num();

	//if clothing assets are added or removed, re-create after removing all
	if(ClothingActors.Num() != NumAssets)
	{
		RemoveAllClothingActors();

		if(NumAssets > 0)
		{
			ClothingActors.Empty(NumAssets);
			ClothingActors.AddZeroed(NumAssets);
		}
	}

	bool bNeedUpdateLOD = false;

	for(int32 AssetIdx=0; AssetIdx < NumAssets; AssetIdx++)
	{
		FClothingAssetData& ClothAsset = SkeletalMesh->ClothingAssets[AssetIdx];

		// if there exist mapped sections, create a clothing actor
		if(SkeletalMesh->HasClothSections(0, AssetIdx))
		{
			if(CreateClothingActor(AssetIdx, ClothAsset.ApexClothingAsset))
			{
				bNeedUpdateLOD = true;
			}
		}
		else 
		{	//don't have cloth sections but clothing actor is alive
			if(IsValidClothingActor(AssetIdx))
			{
				//clear because mapped sections are removed
				ClothingActors[AssetIdx].Clear(true);
			}
		}
	}

	if(bNeedUpdateLOD)
	{
		SetClothingLOD(PredictedLODLevel);
	}
}

/** 
 * APEX clothing actor is created from APEX clothing asset for cloth simulation 
 * If this is invalid, re-create actor , but if valid ,just skip to create
*/
bool USkeletalMeshComponent::CreateClothingActor(int32 AssetIndex, TSharedPtr<FClothingAssetWrapper> ClothingAssetWrapper)
{	
	int32 NumActors = ClothingActors.Num();
	int32 ActorIndex = -1;

	//check we need to expand ClothingActors array 
	//actor is totally corresponding to asset, 1-on-1, so asset index should be same as actor index
	if(AssetIndex < NumActors)
	{
		ActorIndex = AssetIndex;

		if(IsValidClothingActor(ActorIndex))
		{
			// a valid actor already exists
			return false;
		}
		else
		{
			//removed or changed or not-created yet
			ClothingActors[ActorIndex].Clear();
		}
	}

	if(ActorIndex < 0)
	{
		ActorIndex = ClothingActors.AddZeroed();
	}
	 
	NxClothingAsset* ClothingAsset = ClothingAssetWrapper->GetAsset();
	// Get the (singleton!) default actor descriptor.
	NxParameterized::Interface* ActorDesc = ClothingAsset->getDefaultActorDesc();
	PX_ASSERT(ActorDesc != NULL);

	// Run Cloth on the GPU
	verify(NxParameterized::setParamBool(*ActorDesc, "useHardwareCloth", true));
	verify(NxParameterized::setParamBool(*ActorDesc, "updateStateWithGlobalMatrices", true));

	FVector ScaleVector = ComponentToWorld.GetScale3D();

	//support only uniform scale
	verify(NxParameterized::setParamF32(*ActorDesc, "actorScale", ScaleVector.X));

	bool bUseInternalBoneOrder = true;

	verify(NxParameterized::setParamBool(*ActorDesc,"useInternalBoneOrder",bUseInternalBoneOrder));

	verify(NxParameterized::setParamF32(*ActorDesc,"maxDistanceBlendTime",1.0f));
	verify(NxParameterized::setParamF32(*ActorDesc,"lodWeights.maxDistance",10000));
	verify(NxParameterized::setParamF32(*ActorDesc,"lodWeights.distanceWeight",1.0f));
	verify(NxParameterized::setParamF32(*ActorDesc,"lodWeights.bias",0));
	verify(NxParameterized::setParamF32(*ActorDesc,"lodWeights.benefitsBias",0));

	// Initialize the global pose

	FMatrix UGlobalPose = ComponentToWorld.ToMatrixWithScale();

	physx::PxMat44 PxGlobalPose = U2PMatrix(UGlobalPose);

	PxGlobalPose = physx::PxMat44::createIdentity();

	verify(NxParameterized::setParamMat44(*ActorDesc, "globalPose", PxGlobalPose));

	// set max distance scale 
	// if set "maxDistanceScale.Multipliable" to true, scaled result looks more natural
	// @TODO : need to expose "Multipliable"?
	verify(NxParameterized::setParamBool(*ActorDesc, "maxDistanceScale.Multipliable", true));
	verify(NxParameterized::setParamF32(*ActorDesc, "maxDistanceScale.Scale", ClothMaxDistanceScale));

	FPhysScene* PhysScene = NULL;

	if(GetWorld())
	{
		PhysScene = GetWorld()->GetPhysicsScene();
	}

	//this clothing actor would be crated later because this becomes invalid
	if(!PhysScene)
	{
		return false;
	}

	physx::apex::NxApexScene* ScenePtr = PhysScene->GetApexScene(PST_Cloth);

	if(!ScenePtr)
	{
		// can't create clothing actor
		UE_LOG(LogSkeletalMesh, Log, TEXT("CreateClothingActor: Failed to create an actor becauase PhysX Scene doesn't exist"));
		return false;
	}

	physx::apex::NxApexActor* apexActor = ClothingAsset->createApexActor(*ActorDesc, *ScenePtr);
	physx::apex::NxClothingActor* ClothingActor = static_cast<physx::apex::NxClothingActor*>(apexActor);

	ClothingActors[ActorIndex].ApexClothingActor = ClothingActor;
	ClothingActors[ActorIndex].SceneType = PST_Cloth;
	ClothingActors[ActorIndex].PhysScene = PhysScene;

	if(!ClothingActor)
	{
		UE_LOG(LogSkeletalMesh, Log, TEXT("CreateClothingActor: Failed to create an clothing actor (%s)"), ANSI_TO_TCHAR(ClothingAsset->getName()));
		return false;
	}

	//set parent pointer to verify later whether became invalid or not
	ClothingActors[ActorIndex].ParentClothingAsset = ClothingAssetWrapper;

	// budget is millisecond units
	ScenePtr->setLODResourceBudget(100); // for temporary, 100ms

	ClothingActor->setGraphicalLOD(PredictedLODLevel);

	// 0 means no simulation
	ClothingActor->forcePhysicalLod(1); // 1 will be changed to "GetActivePhysicalLod()" later
	ClothingActor->setFrozen(false);

	// process clothing collisions once for the case that this component doesn't move
	if(bCollideWithEnvironment)
	{
		ProcessClothCollisionWithEnvironment();
	}

	return true;
}

void USkeletalMeshComponent::SetClothingLOD(int32 LODIndex)
{
	int32 NumActors = ClothingActors.Num();

	bool bFrozen = false;

	for(int32 i=0; i<NumActors; i++)
	{
		if(IsValidClothingActor(i))
		{			
			int32 CurLODIndex = (int32)ClothingActors[i].ApexClothingActor->getGraphicalLod();

			int32 NumClothLODs = ClothingActors[i].ParentClothingAsset->GetAsset()->getNumGraphicalLodLevels();

			bool bEnabledLOD = SkeletalMesh->IsEnabledClothLOD(i);

			//Change Clothing LOD if new LOD index is different from current index
			if(CurLODIndex != LODIndex)
			{
				//physical LOD is changed by graphical LOD
				ClothingActors[i].ApexClothingActor->setGraphicalLOD(LODIndex);

				if(ClothingActors[i].ApexClothingActor->isFrozen())
				{
					bFrozen = true;
				}
			}

			//decide whether enable or disable
			if( (LODIndex > 0  && !bEnabledLOD) ||
			    (LODIndex >= NumClothLODs) )
			{
				//disable clothing simulation
				ClothingActors[i].ApexClothingActor->forcePhysicalLod(0);
			}
			else
			{	
				int32 CurPhysLOD = ClothingActors[i].ApexClothingActor->getActivePhysicalLod();
				//enable clothing simulation
				if(CurPhysLOD == 0)
				{
					ClothingActors[i].ApexClothingActor->forcePhysicalLod(1);
				}
			}
		}
	}

	if(bFrozen)
	{
#if WITH_EDITOR
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("SkelmeshComponent", "Warning_FrozenCloth", "Clothing will be melted from frozen state"));
#endif
		//melt it because rendering mesh is broken if frozen when changing LODs
		FreezeClothSection(false);
	}

}

void USkeletalMeshComponent::RemoveAllClothingActors()
{
	int32 NumActors = ClothingActors.Num();

	for(int32 i=0; i<NumActors; i++)
	{
		ClothingActors[i].Clear(IsValidClothingActor(i));
	}

	ClothingActors.Empty();
}

void USkeletalMeshComponent::ReleaseAllClothingResources()
{
#if WITH_CLOTH_COLLISION_DETECTION
	ReleaseAllParentCollisions();
	ReleaseAllChildrenCollisions();
	// should be called before removing clothing actors
	RemoveAllOverlappedComponentMap();
#endif // #if WITH_CLOTH_COLLISION_DETECTION

#if WITH_APEX_CLOTHING
	RemoveAllClothingActors();
#endif// #if WITH_APEX_CLOTHING
}

void USkeletalMeshComponent::ApplyWindForCloth(FClothingActor& ClothingActor)
{
	//to convert from normalized value( usually 0.0 to 1.0 ) to Apex clothing wind value
	const float WindUnitAmout = 2500.0f;

	NxClothingActor* ApexClothingActor = ClothingActor.ApexClothingActor;

	if(!ApexClothingActor)
	{
		return;
	}

	if(World && World->Scene)
	{
		// set wind
		if(IsWindEnabled())
		{
			FVector Position = ComponentToWorld.GetTranslation();

			FVector4 WindParam = World->Scene->GetWindParameters(Position);

			physx::PxVec3 WindVelocity(WindParam.X, WindParam.Y, WindParam.Z);

			WindVelocity *= WindUnitAmout;
			float WindAdaption = rand()%20 * 0.1f; // make range from 0 to 2

			NxParameterized::Interface* ActorDesc = ApexClothingActor->getActorDesc();

			if(ActorDesc)
			{
				verify(NxParameterized::setParamVec3(*ActorDesc, "windParams.Velocity", WindVelocity));
				verify(NxParameterized::setParamF32(*ActorDesc, "windParams.Adaption", WindAdaption));
			}
		}
		else
		{
			physx::PxVec3 WindVelocity(0.0f);
			NxParameterized::Interface* ActorDesc = ApexClothingActor->getActorDesc();
			if(ActorDesc)
			{
				verify(NxParameterized::setParamVec3(*ActorDesc, "windParams.Velocity", WindVelocity));
				// if turned off the wind, will do fast adaption
				verify(NxParameterized::setParamF32(*ActorDesc, "windParams.Adaption", 2.0f));
			}
		}
	}
}

#endif// #if WITH_APEX_CLOTHING


#if WITH_CLOTH_COLLISION_DETECTION

void USkeletalMeshComponent::DrawDebugConvexFromPlanes(FClothCollisionPrimitive& CollisionPrimitive, FColor& Color, bool bDrawWithPlanes)
{
	int32 NumPlanes = CollisionPrimitive.ConvexPlanes.Num();

	//draw with planes
	if(bDrawWithPlanes)
	{
		for(int32 PlaneIdx=0; PlaneIdx < NumPlanes; PlaneIdx++)
		{
			FPlane& Plane = CollisionPrimitive.ConvexPlanes[PlaneIdx];
			DrawDebugSolidPlane(GetWorld(), Plane, CollisionPrimitive.Origin, 10, Color);
		}
	}
	else
	{
		FVector UniquePoint;
		TArray<FVector> UniqueIntersectPoints;

		TArray<FPlane>& Planes = CollisionPrimitive.ConvexPlanes;

		//find all unique intersected points
		for(int32 i=0; i < NumPlanes; i++)
		{
			FPlane Plane1 = Planes[i];

			for(int32 j=i+1; j < NumPlanes; j++)
			{
				FPlane Plane2 = Planes[j];

				for(int32 k=j+1; k < NumPlanes; k++)
				{
					FPlane Plane3 = Planes[k];
					
					if(FMath::IntersectPlanes3(UniquePoint, Plane1, Plane2, Plane3))
					{
						UniqueIntersectPoints.Add(UniquePoint);
					}
				}
			}
		}

		int32 NumPts = UniqueIntersectPoints.Num();

		//shows all connected lines for just debugging
		for(int32 i=0; i < NumPts; i++)
		{
			for(int32 j=i+1; j < NumPts; j++)
			{
				DrawDebugLine(GetWorld(), UniqueIntersectPoints[i], UniqueIntersectPoints[j], Color, false, -1, SDPG_World, 2.0f);
			}
		}
	}
}

void USkeletalMeshComponent::DrawDebugClothCollisions()
{
	FColor Colors[6] = { FColor::Red, FColor::Green, FColor::Blue, FColor::Cyan, FColor::Yellow, FColor::Magenta };

	for( auto It = ClothOverlappedComponentsMap.CreateConstIterator(); It; ++It )
	{
		TWeakObjectPtr<UPrimitiveComponent> PrimComp = It.Key();

		TArray<FClothCollisionPrimitive> CollisionPrims;
		ECollisionChannel Channel = PrimComp->GetCollisionObjectType();
		if (Channel == ECollisionChannel::ECC_WorldStatic)
		{
			if (!GetClothCollisionDataFromStaticMesh(PrimComp.Get(), CollisionPrims))
		{
			return;
		}

		for(int32 PrimIndex=0; PrimIndex < CollisionPrims.Num(); PrimIndex++)
		{
			switch(CollisionPrims[PrimIndex].PrimType)
			{
			case FClothCollisionPrimitive::SPHERE:
				DrawDebugSphere(GetWorld(), CollisionPrims[PrimIndex].Origin, CollisionPrims[PrimIndex].Radius, 10, FColor::Red);
				break;

			case FClothCollisionPrimitive::CAPSULE:
				{
					FVector DiffVec = CollisionPrims[PrimIndex].SpherePos2 - CollisionPrims[PrimIndex].SpherePos1;
					float HalfHeight = DiffVec.Size() * 0.5f;
					FQuat Rotation = PrimComp->ComponentToWorld.GetRotation();
					DrawDebugCapsule(GetWorld(), CollisionPrims[PrimIndex].Origin, HalfHeight, CollisionPrims[PrimIndex].Radius, Rotation, FColor::Red );
				}

				break;
			case FClothCollisionPrimitive::CONVEX:	
				DrawDebugConvexFromPlanes(CollisionPrims[PrimIndex], Colors[PrimIndex%6]);
				break;
			}

			//draw a bounding box for double checking
			DrawDebugBox(GetWorld(), PrimComp->Bounds.Origin, PrimComp->Bounds.BoxExtent, FColor::Red );
		}
	}
		else if (Channel == ECollisionChannel::ECC_PhysicsBody) // supports an interaction between the character and other clothing such as a curtain
		{
			bool bCreatedCollisions = false;
			// if a component is a skeletal mesh component with clothing, add my collisions to the component
			USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(PrimComp.Get());
			if (SkelMeshComp && SkelMeshComp->SkeletalMesh)
			{
				if (SkelMeshComp->ClothingActors.Num() > 0)
				{
					TArray<FApexClothCollisionVolumeData> NewCollisions;
					FindClothCollisions(NewCollisions);

					for (int32 ColIdx = 0; ColIdx < NewCollisions.Num(); ColIdx++)
					{
						if (NewCollisions[ColIdx].IsCapsule())
						{
							FVector Origin = NewCollisions[ColIdx].LocalPose.GetOrigin();
							// apex uses y-axis as the up-axis of capsule
							FVector UpAxis = NewCollisions[ColIdx].LocalPose.GetScaledAxis(EAxis::Y);
							float Radius = NewCollisions[ColIdx].CapsuleRadius*UpAxis.Size();
							float HalfHeight = NewCollisions[ColIdx].CapsuleHeight*0.5f;

							FMatrix Pose = NewCollisions[ColIdx].LocalPose;
							FMatrix RotMat(Pose.GetScaledAxis(EAxis::X), Pose.GetScaledAxis(EAxis::Z), Pose.GetScaledAxis(EAxis::Y), FVector(0,0,0));
							FQuat Rotation = RotMat.ToQuat();
							DrawDebugCapsule(GetWorld(), Origin, HalfHeight, Radius, Rotation, FColor::Red);
						}
					}
				}
			}
		}
	}
	//draw this skeletal mesh component's bounding box

	DrawDebugBox(GetWorld(), Bounds.Origin, Bounds.BoxExtent, FColor::Red);

}

bool USkeletalMeshComponent::GetClothCollisionDataFromStaticMesh(UPrimitiveComponent* PrimComp, TArray<FClothCollisionPrimitive>& ClothCollisionPrimitives)
{
	//make sure Num of collisions should be 0 in the case this returns false
	ClothCollisionPrimitives.Empty();

	if(!PrimComp)
	{
		return false;
	}

	ECollisionChannel Channel = PrimComp->GetCollisionObjectType();
	// accept only a static mesh
	if (Channel != ECollisionChannel::ECC_WorldStatic)
	{
		return false;
	}

	if (!PrimComp->BodyInstance.IsValidBodyInstance())
	{
		return false;
	}

	int32 NumSyncShapes = 0;

	TArray<PxShape*> AllShapes = PrimComp->BodyInstance.GetAllShapes(NumSyncShapes);

	if(NumSyncShapes == 0 || NumSyncShapes > 3) //skipping complicated object because of collision limitation
	{
		return false;
	}

	FVector Center = PrimComp->Bounds.Origin;
	FTransform Transform = PrimComp->ComponentToWorld;
	FMatrix TransMat = Transform.ToMatrixWithScale();

	for(int32 ShapeIdx=0; ShapeIdx < NumSyncShapes; ShapeIdx++)
	{
		PxGeometryType::Enum GeomType = AllShapes[ShapeIdx]->getGeometryType();

		switch(GeomType)
		{
		case PxGeometryType::eSPHERE:
			{
				FClothCollisionPrimitive ClothPrimData;

				PxSphereGeometry SphereGeom;
				AllShapes[ShapeIdx]->getSphereGeometry(SphereGeom);

				ClothPrimData.Origin = Center;
				ClothPrimData.Radius = SphereGeom.radius;
				ClothPrimData.PrimType = FClothCollisionPrimitive::SPHERE;

				ClothCollisionPrimitives.Add(ClothPrimData);
			}
			break;

		case PxGeometryType::eCAPSULE:
			{
				FClothCollisionPrimitive ClothPrimData;

				PxCapsuleGeometry CapsuleGeom;
				AllShapes[ShapeIdx]->getCapsuleGeometry(CapsuleGeom);

				ClothPrimData.Origin = Center;
				ClothPrimData.Radius = CapsuleGeom.radius;

				FVector ZAxis = TransMat.GetUnitAxis(EAxis::Z);
				float Radius = CapsuleGeom.radius;
				float HalfHeight = CapsuleGeom.halfHeight;
				ClothPrimData.SpherePos1 = Center + (HalfHeight * ZAxis);
				ClothPrimData.SpherePos2 = Center - (HalfHeight * ZAxis);

				ClothPrimData.PrimType = FClothCollisionPrimitive::CAPSULE;

				ClothCollisionPrimitives.Add(ClothPrimData);
			}
			break;
		case PxGeometryType::eBOX:
			{
				FClothCollisionPrimitive ClothPrimData;
				ClothPrimData.Origin = Center;
				ClothPrimData.Radius = 0;

				PxBoxGeometry BoxGeom;

				AllShapes[ShapeIdx]->getBoxGeometry(BoxGeom);

				ClothPrimData.ConvexPlanes.Empty(6); //box has 6 planes

				FPlane UPlane1(1,0,0,Center.X + BoxGeom.halfExtents.x);
				UPlane1 = UPlane1.TransformBy(TransMat);
				ClothPrimData.ConvexPlanes.Add(UPlane1);

				FPlane UPlane2(-1,0,0,Center.X - BoxGeom.halfExtents.x);
				UPlane2 = UPlane2.TransformBy(TransMat);
				ClothPrimData.ConvexPlanes.Add(UPlane2);

				FPlane UPlane3(0,1,0,Center.Y + BoxGeom.halfExtents.y);
				UPlane3 = UPlane3.TransformBy(TransMat);
				ClothPrimData.ConvexPlanes.Add(UPlane3);

				FPlane UPlane4(0,-1,0,Center.Y - BoxGeom.halfExtents.y);
				UPlane4 = UPlane4.TransformBy(TransMat);
				ClothPrimData.ConvexPlanes.Add(UPlane4);

				FPlane UPlane5(0,0,1,Center.Z + BoxGeom.halfExtents.z);
				UPlane5 = UPlane5.TransformBy(TransMat);
				ClothPrimData.ConvexPlanes.Add(UPlane5);

				FPlane UPlane6(0,0,-1,Center.Z - BoxGeom.halfExtents.z);
				UPlane6 = UPlane6.TransformBy(TransMat);
				ClothPrimData.ConvexPlanes.Add(UPlane6);

				ClothPrimData.PrimType = FClothCollisionPrimitive::CONVEX;
				ClothCollisionPrimitives.Add(ClothPrimData);
			}
			break;
		case PxGeometryType::eCONVEXMESH:
			{
				PxConvexMeshGeometry ConvexGeom;

				AllShapes[ShapeIdx]->getConvexMeshGeometry(ConvexGeom);

				if(ConvexGeom.convexMesh)
				{
					FClothCollisionPrimitive ClothPrimData;
					ClothPrimData.Origin = Center;
					ClothPrimData.Radius = 0;

					uint32 NumPoly = ConvexGeom.convexMesh->getNbPolygons();

					ClothPrimData.ConvexPlanes.Empty(NumPoly);

					for(uint32 Poly=0; Poly < NumPoly; Poly++)
					{
						PxHullPolygon HullData;
						ConvexGeom.convexMesh->getPolygonData(Poly, HullData);						
						physx::PxPlane PPlane(HullData.mPlane[0],HullData.mPlane[1],HullData.mPlane[2],HullData.mPlane[3]);
						FPlane UPlane = P2UPlane(PPlane);
						UPlane = UPlane.TransformBy(TransMat);
						ClothPrimData.ConvexPlanes.Add(UPlane);
					}

					ClothPrimData.PrimType = FClothCollisionPrimitive::CONVEX;
					ClothCollisionPrimitives.Add(ClothPrimData);
				}
			}
			break;
		}
	}
	return true;
}

void USkeletalMeshComponent::FindClothCollisions(TArray<FApexClothCollisionVolumeData>& OutCollisions)
{
	if(!SkeletalMesh)
	{
		return;
	}

	int32 NumAssets = SkeletalMesh->ClothingAssets.Num();

	// find all new collisions passing to children
	for(int32 AssetIdx=0; AssetIdx < NumAssets; AssetIdx++)
	{
		FClothingAssetData& Asset = SkeletalMesh->ClothingAssets[AssetIdx];
		int32 NumCollisions = Asset.ClothCollisionVolumes.Num();

		for(int32 ColIdx=0; ColIdx < NumCollisions; ColIdx++)
		{
			FApexClothCollisionVolumeData& Collision = Asset.ClothCollisionVolumes[ColIdx];

			if(Collision.BoneIndex < 0)
			{
				continue;
			}

			FName BoneName = Asset.ApexClothingAsset->GetConvertedBoneName(Collision.BoneIndex);

			int32 BoneIndex = GetBoneIndex(BoneName);

			if(BoneIndex < 0)
			{
				continue;
			}

			FMatrix BoneMat = GetBoneMatrix(BoneIndex);

			FMatrix LocalToWorld = Collision.LocalPose * BoneMat;

			// support only capsule now 
			if(Collision.IsCapsule())
			{
				FApexClothCollisionVolumeData NewCollision = Collision;
				NewCollision.LocalPose = LocalToWorld;
				OutCollisions.Add(NewCollision);
			}
			}
		}
	}

void USkeletalMeshComponent::CreateInternalClothCollisions(TArray<FApexClothCollisionVolumeData>& InCollisions, TArray<physx::apex::NxClothingCollision*>& OutCollisions)
	{
	int32 NumCollisions = InCollisions.Num();

	const int32 MaxNumCapsules = 16;
	// because sphere number can not be larger than 32 and 1 capsule takes 2 spheres 
	NumCollisions = FMath::Min(NumCollisions, MaxNumCapsules);
	int32 NumActors = ClothingActors.Num();

		for(int32 ActorIdx=0; ActorIdx < NumActors; ActorIdx++)
		{
		if (!IsValidClothingActor(ActorIdx))
			{
				continue;
			}

		NxClothingActor* Actor = ClothingActors[ActorIdx].ApexClothingActor;
		int32 NumCurrentCapsules = SkeletalMesh->ClothingAssets[ActorIdx].ClothCollisionVolumes.Num(); // # of capsules 

			for(int32 ColIdx=0; ColIdx < NumCollisions; ColIdx++)
			{
				// support only capsules now 
			if (InCollisions[ColIdx].IsCapsule() && NumCurrentCapsules < MaxNumCapsules)
				{
				FVector Origin = InCollisions[ColIdx].LocalPose.GetOrigin();
					// apex uses y-axis as the up-axis of capsule
				FVector UpAxis = InCollisions[ColIdx].LocalPose.GetScaledAxis(EAxis::Y);
					
				float Radius = InCollisions[ColIdx].CapsuleRadius*UpAxis.Size();

				float HalfHeight = InCollisions[ColIdx].CapsuleHeight*0.5f;
					const FVector TopEnd = Origin + (HalfHeight * UpAxis);
					const FVector BottomEnd = Origin - (HalfHeight * UpAxis);

					NxClothingSphere* Sphere1 = Actor->createCollisionSphere(U2PVector(TopEnd), Radius);
					NxClothingSphere* Sphere2 = Actor->createCollisionSphere(U2PVector(BottomEnd), Radius);

					NxClothingCapsule* Capsule = Actor->createCollisionCapsule(*Sphere1, *Sphere2);

				OutCollisions.Add(Capsule);
				NumCurrentCapsules++;
				}
			}
		}
	}

void USkeletalMeshComponent::CopyClothCollisionsToChildren()
{
	// 3 steps
	// 1. release all previous parent collisions
	// 2. find new collisions from parent(this class)
	// 3. add new collisions to children

	int32 NumChildren = AttachChildren.Num();
	TArray<USkeletalMeshComponent*> ClothChildren;

	for(int32 i=0; i < NumChildren; i++)
	{
		USkeletalMeshComponent* pChild = Cast<USkeletalMeshComponent>(AttachChildren[i]);
		if(pChild)
		{
			int32 NumActors = pChild->ClothingActors.Num();
			if(NumActors > 0)
			{
				ClothChildren.Add(pChild);
				// release all parent collisions
				pChild->ReleaseAllParentCollisions();
			}
		}
	}

	int32 NumClothChildren = ClothChildren.Num();

	if(NumClothChildren == 0)
	{
		return;
	}

	TArray<FApexClothCollisionVolumeData> NewCollisions;

	FindClothCollisions(NewCollisions);

	for(int32 ChildIdx=0; ChildIdx < NumClothChildren; ChildIdx++)
	{
		ClothChildren[ChildIdx]->CreateInternalClothCollisions(NewCollisions, ClothChildren[ChildIdx]->ParentCollisions);
	}
}

void USkeletalMeshComponent::ReleaseAllChildrenCollisions()
{
	for(int32 i=0; i<ChildrenCollisions.Num(); i++)
	{
		ReleaseClothingCollision(ChildrenCollisions[i]);
	}

	ChildrenCollisions.Empty();
}

// children's collisions can affect to parent's cloth reversely
void USkeletalMeshComponent::CopyChildrenClothCollisionsToParent()
{
	// 3 steps
	// 1. release all previous children collisions
	// 2. find new collisions from children
	// 3. add new collisions to parent (this component)

	ReleaseAllChildrenCollisions();

	int32 NumChildren = AttachChildren.Num();
	TArray<USkeletalMeshComponent*> ClothCollisionChildren;

	TArray<FApexClothCollisionVolumeData> NewCollisions;

	for(int32 i=0; i < NumChildren; i++)
	{
		USkeletalMeshComponent* pChild = Cast<USkeletalMeshComponent>(AttachChildren[i]);
		if(pChild)
		{
			pChild->FindClothCollisions(NewCollisions);
		}
	}

	CreateInternalClothCollisions(NewCollisions, ChildrenCollisions);
}

void USkeletalMeshComponent::ReleaseClothingCollision(NxClothingCollision* Collision)
{
	switch(Collision->getType())
	{
	case NxClothingCollisionType::Capsule:
		{
			NxClothingCapsule* Capsule = static_cast<NxClothingCapsule*>(Collision);

			check(Capsule);

			Capsule->releaseWithSpheres();
		}
		break;

	case NxClothingCollisionType::Convex:
		{
			NxClothingConvex* Convex = static_cast<NxClothingConvex*>(Collision);

			check(Convex);

			Convex->releaseWithPlanes();
		}
		break;

	default:
		Collision->release();
		break;
	}
}

FApexClothCollisionInfo* USkeletalMeshComponent::CreateNewClothingCollsions(UPrimitiveComponent* PrimitiveComponent)
{
	FApexClothCollisionInfo NewInfo;

	TArray<FClothCollisionPrimitive> CollisionPrims;

	ECollisionChannel Channel = PrimitiveComponent->GetCollisionObjectType();

	// crate new clothing collisions for a static mesh
	if (Channel == ECollisionChannel::ECC_WorldStatic)
	{
		if (!GetClothCollisionDataFromStaticMesh(PrimitiveComponent, CollisionPrims))
	{
		return NULL;
	}

		NewInfo.OverlapCompType = FApexClothCollisionInfo::OCT_STATIC;

	int32 NumActors = ClothingActors.Num();

	for(int32 ActorIdx=0; ActorIdx < NumActors; ActorIdx++)
	{
		NxClothingActor* Actor = ClothingActors[ActorIdx].ApexClothingActor;

		if(Actor)
		{	
			for(int32 PrimIndex=0; PrimIndex < CollisionPrims.Num(); PrimIndex++)
			{
				NxClothingCollision* ClothCol = NULL;

				switch(CollisionPrims[PrimIndex].PrimType)
				{
				case FClothCollisionPrimitive::SPHERE:
					ClothCol = Actor->createCollisionSphere(U2PVector(CollisionPrims[PrimIndex].Origin), CollisionPrims[PrimIndex].Radius);
					if(ClothCol)
					{
						NewInfo.ClothingCollisions.Add(ClothCol);
					}
					break;
				case FClothCollisionPrimitive::CAPSULE:
					{
						float Radius = CollisionPrims[PrimIndex].Radius;
						NxClothingSphere* Sphere1 = Actor->createCollisionSphere(U2PVector(CollisionPrims[PrimIndex].SpherePos1), Radius);
						NxClothingSphere* Sphere2 = Actor->createCollisionSphere(U2PVector(CollisionPrims[PrimIndex].SpherePos2), Radius);

						ClothCol = Actor->createCollisionCapsule(*Sphere1, *Sphere2);
						if(ClothCol)
						{
							NewInfo.ClothingCollisions.Add(ClothCol);
						}
					}
					break;
				case FClothCollisionPrimitive::CONVEX:

					int32 NumPlanes = CollisionPrims[PrimIndex].ConvexPlanes.Num();

					TArray<NxClothingPlane*> ClothingPlanes;

					//can not exceed 32 planes
					NumPlanes = FMath::Min(NumPlanes, 32);

					ClothingPlanes.AddUninitialized(NumPlanes);

					for(int32 PlaneIdx=0; PlaneIdx < NumPlanes; PlaneIdx++)
					{
						PxPlane PPlane = U2PPlane(CollisionPrims[PrimIndex].ConvexPlanes[PlaneIdx]);

						ClothingPlanes[PlaneIdx] = Actor->createCollisionPlane(PPlane);
					}

					ClothCol = Actor->createCollisionConvex(ClothingPlanes.GetTypedData(), ClothingPlanes.Num());

					if(ClothCol)
					{
						NewInfo.ClothingCollisions.Add(ClothCol);
					}

					break;
				}
			}
		}
	}
	}
	else if (Channel == ECollisionChannel::ECC_PhysicsBody) // creates new clothing collisions for clothing objects
	{
		bool bCreatedCollisions = false;
		// if a component is a skeletal mesh component with clothing, add my collisions to the component
		USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(PrimitiveComponent);

		if (SkelMeshComp && SkelMeshComp->SkeletalMesh)
		{
			// if a casted skeletal mesh component is myself, then skip
			if (SkelMeshComp == this)
			{
				return NULL;
			}

			if (SkelMeshComp->ClothingActors.Num() > 0)
			{
				TArray<FApexClothCollisionVolumeData> NewCollisions;
				// get collision infos computed by current bone transforms
				FindClothCollisions(NewCollisions);
				
				if (NewCollisions.Num() > 0)
				{
					NewInfo.OverlapCompType = FApexClothCollisionInfo::OCT_CLOTH;
					SkelMeshComp->CreateInternalClothCollisions(NewCollisions, NewInfo.ClothingCollisions);
					bCreatedCollisions = true;
				}
			}
		}

		if (bCreatedCollisions == false)
		{
			return NULL;
		}
	}

	return &ClothOverlappedComponentsMap.Add(PrimitiveComponent, NewInfo);
}

void USkeletalMeshComponent::RemoveAllOverlappedComponentMap()
{
	for( auto It = ClothOverlappedComponentsMap.CreateConstIterator(); It; ++It )
	{
		const FApexClothCollisionInfo& Info = It.Value();

		for(int32 i=0; i < Info.ClothingCollisions.Num(); i++)
		{
			ReleaseClothingCollision(Info.ClothingCollisions[i]);
		}

		ClothOverlappedComponentsMap.Remove(It.Key());
	}

	ClothOverlappedComponentsMap.Empty();
}

void USkeletalMeshComponent::ReleaseAllParentCollisions()
{
	for(int32 i=0; i<ParentCollisions.Num(); i++)
	{
		ReleaseClothingCollision(ParentCollisions[i]);
	}

	ParentCollisions.Empty();
}

void USkeletalMeshComponent::UpdateOverlappedComponent(UPrimitiveComponent* PrimComp, FApexClothCollisionInfo* Info)
{
	if (Info->OverlapCompType == FApexClothCollisionInfo::OCT_CLOTH)
	{
		int32 NumCollisions = Info->ClothingCollisions.Num();
		for (int32 i = 0; i < NumCollisions; i++)
		{
			ReleaseClothingCollision(Info->ClothingCollisions[i]);
		}

		Info->ClothingCollisions.Empty(NumCollisions);

		TArray<FApexClothCollisionVolumeData> NewCollisions;
		FindClothCollisions(NewCollisions);

		if (NewCollisions.Num() > 0)
		{
			USkeletalMeshComponent *SkelMeshComp = Cast<USkeletalMeshComponent>(PrimComp);
			if (SkelMeshComp)
			{
				SkelMeshComp->CreateInternalClothCollisions(NewCollisions, Info->ClothingCollisions);
			}
		}
	}
}

void USkeletalMeshComponent::ProcessClothCollisionWithEnvironment()
{
	// don't handle collision detection if this component is in editor
	if(!GetWorld()->IsGameWorld())
	{
		return;
	}

	// Increment the revision number
	ClothingCollisionRevision++;

	TArray<FOverlapResult> Overlaps;

	FCollisionObjectQueryParams ObjectParams;

	ObjectParams.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldStatic);
	// to collide with other clothing objects
	ObjectParams.AddObjectTypesToQuery(ECollisionChannel::ECC_PhysicsBody);
	
	static FName ClothOverlapComponentsName(TEXT("ClothOverlapComponents"));
	FCollisionQueryParams Params(ClothOverlapComponentsName, false);

	GetWorld()->OverlapMulti(Overlaps, Bounds.Origin, FQuat::Identity, FCollisionShape::MakeBox(Bounds.BoxExtent), Params, ObjectParams);

	for (int32 OverlapIdx=0; OverlapIdx<Overlaps.Num(); ++OverlapIdx)
	{
		const TWeakObjectPtr<UPrimitiveComponent>& Component = Overlaps[OverlapIdx].Component;
		if (Component.IsValid())
		{ 
			// add intersected cloth collision
			FApexClothCollisionInfo* Info = ClothOverlappedComponentsMap.Find(Component);

			if(!Info)
			{
				Info = CreateNewClothingCollsions(Component.Get());
			}

			// update new or valid entries to the current revision number
			if(Info)
			{
				Info->Revision = ClothingCollisionRevision;
				// if this component needs update every tick 
				UpdateOverlappedComponent(Component.Get(), Info);
			}
		}
	}

	// releases all invalid collisions in overlapped list 
	for( auto It = ClothOverlappedComponentsMap.CreateConstIterator(); It; ++It )
	{
		const FApexClothCollisionInfo& Info = It.Value();

		// Anything not updated above will have an old revision number.
		if( Info.Revision != ClothingCollisionRevision )
		{
			for(int32 i=0; i < Info.ClothingCollisions.Num(); i++)
			{
				ReleaseClothingCollision(Info.ClothingCollisions[i]);
			}

			ClothOverlappedComponentsMap.Remove(It.Key());
		}
	}
}

#endif// #if WITH_CLOTH_COLLISION_DETECTION

void USkeletalMeshComponent::PreClothTick(float DeltaTime)
{
	// if physics is disabled on dedicated server, no reason to be here. 
	if (!bEnablePhysicsOnDedicatedServer && IsRunningDedicatedServer())
	{
		return;
	}

	if (IsRegistered() && IsSimulatingPhysics())
	{
		SyncComponentToRBPhysics();
	}

	// this used to not run if not rendered, but that causes issues such as bounds not updated
	// causing it to not rendered, at the end, I think we should blend body positions
	// for example if you're only simulating, this has to happen all the time
	// whether looking at it or not, otherwise
	// @todo better solution is to check if it has moved by changing SyncComponentToRBPhysics to return true if anything modified
	// and run this if that is true or rendered
	// that will at least reduce the chance of mismatch
	// generally if you move your actor position, this has to happen to approximately match their bounds
	if (Bodies.Num() > 0 && IsRegistered())
	{
		BlendInPhysics();
	}

#if WITH_APEX_CLOTHING
	// if skeletal mesh has clothing assets, call TickClothing
	if (SkeletalMesh && SkeletalMesh->ClothingAssets.Num() > 0)
	{
		TickClothing(DeltaTime + SkippedTickDeltaTime);
	}
#endif
}

#if WITH_APEX_CLOTHING

void USkeletalMeshComponent::UpdateClothTransform()
{
	int32 NumActors = ClothingActors.Num();

#if WITH_CLOTH_COLLISION_DETECTION

	if(bCollideWithAttachedChildren)
	{
		CopyClothCollisionsToChildren();
	}

	//check the environment when only transform is updated
	if(bCollideWithEnvironment && NumActors > 0)
	{
		ProcessClothCollisionWithEnvironment();
	}
#endif // WITH_CLOTH_COLLISION_DETECTION

	physx::PxMat44 PxGlobalPose = U2PMatrix(ComponentToWorld.ToMatrixWithScale());

	for(int32 ActorIdx=0; ActorIdx<NumActors; ActorIdx++)
	{
		// skip if ClothingActor is NULL or invalid
		if(!IsValidClothingActor(ActorIdx))
		{
			continue;
		}

		NxClothingActor* ClothingActor = ClothingActors[ActorIdx].ApexClothingActor;

		check(ClothingActor);

		NxParameterized::Interface* ActorDesc = ClothingActor->getActorDesc();

		verify(NxParameterized::setParamMat44(*ActorDesc, "globalPose", PxGlobalPose));
	}
}

void USkeletalMeshComponent::CheckClothTeleport(float DeltaTime)
{
	// Get the root bone transform
	FMatrix CurRootBoneMat = GetBoneMatrix(0);

	if(bNeedTeleportAndResetOnceMore)
	{
		ForceClothNextUpdateTeleportAndReset();
		bNeedTeleportAndResetOnceMore = false;
	}

	float DeltaTimeThreshold = 0.2f;
	// clothing simulation could be broken if frame-rate goes under 5 fps
	if(DeltaTime > DeltaTimeThreshold)
	{
		ForceClothNextUpdateTeleportAndReset();
	}

	// distance check 
	// TeleportDistanceThreshold is greater than Zero and not teleported yet
	if(TeleportDistanceThreshold > 0 && ClothTeleportMode == FClothingActor::Continuous)
	{
		float DistSquared = FVector::DistSquared(PrevRootBoneMatrix.GetOrigin(), CurRootBoneMat.GetOrigin());
		if ( DistSquared > ClothTeleportDistThresholdSquared ) // if it has traveled too far
		{
			ClothTeleportMode = bResetAfterTeleport ? FClothingActor::TeleportAndReset : FClothingActor::Teleport;
			// clothing reset is needed once more to avoid clothing pop up when moved the component too far
			bNeedTeleportAndResetOnceMore = true;
		}
	}

	// rotation check
	// if TeleportRotationThreshold is greater than Zero and the user didn't do force teleport
	if(TeleportRotationThreshold > 0 && ClothTeleportMode == FClothingActor::Continuous)
	{
		// Detect whether teleportation is needed or not
		// Rotation matrix's transpose means an inverse but can't use a transpose because this matrix includes scales
		FMatrix AInvB = CurRootBoneMat * PrevRootBoneMatrix.Inverse();
		float Trace = AInvB.M[0][0] + AInvB.M[1][1] + AInvB.M[2][2];
		float CosineTheta = (Trace - 1.0f) / 2.0f; // trace = 1+2cos(��) for a 3��3 matrix

		if ( CosineTheta < ClothTeleportCosineThresholdInRad ) // has the root bone rotated too much
		{
			ClothTeleportMode = bResetAfterTeleport ? FClothingActor::TeleportAndReset : FClothingActor::Teleport;
		}
	}

	PrevRootBoneMatrix = CurRootBoneMat;
}

void USkeletalMeshComponent::UpdateClothState(float DeltaTime)
{
	int32 NumActors = ClothingActors.Num();

#if WITH_CLOTH_COLLISION_DETECTION

	if(bCollideWithAttachedChildren)
	{
		CopyClothCollisionsToChildren();
		CopyChildrenClothCollisionsToParent();
	}
#endif // WITH_CLOTH_COLLISION_DETECTION

	if(NumActors == 0)
	{
		return;
	}

	TArray<FTransform>* BoneTransforms = &SpaceBases;

	if(MasterPoseComponent.IsValid())
	{
		BoneTransforms = &(MasterPoseComponent.Get()->SpaceBases);
	}

	if(BoneTransforms->Num() == 0)
	{
		return;
	}

	physx::PxMat44 PxGlobalPose = U2PMatrix(ComponentToWorld.ToMatrixWithScale());

	CheckClothTeleport(DeltaTime);

	// convert teleport mode to apex clothing teleport enum
	physx::apex::ClothingTeleportMode::Enum CurTeleportMode = (physx::apex::ClothingTeleportMode::Enum)ClothTeleportMode;

	for(int32 ActorIdx=0; ActorIdx<NumActors; ActorIdx++)
	{
		// skip if ClothingActor is NULL or invalid
		if(!IsValidClothingActor(ActorIdx))
		{
			continue;
		}

		ApplyWindForCloth(ClothingActors[ActorIdx]);

		TArray<physx::PxMat44> BoneMatrices;

		NxClothingAsset* ClothingAsset = ClothingActors[ActorIdx].ParentClothingAsset->GetAsset();

		uint32 NumUsedBones = ClothingAsset->getNumUsedBones();

		BoneMatrices.Empty(NumUsedBones);
		BoneMatrices.AddUninitialized(NumUsedBones);

		for(uint32 Index=0; Index < NumUsedBones; Index++)
		{
		   FName BoneName = ClothingActors[ActorIdx].ParentClothingAsset->GetConvertedBoneName(Index);

		   int32 BoneIndex = GetBoneIndex(BoneName);

			if(MasterPoseComponent.IsValid())
			{
				int32 TempBoneIndex = BoneIndex;
				BoneIndex = INDEX_NONE;
				if(TempBoneIndex < MasterBoneMap.Num())
				{
					int32 MasterBoneIndex = MasterBoneMap[TempBoneIndex];
 
					// If ParentBoneIndex is valid, grab matrix from MasterPoseComponent.
					if( MasterBoneIndex != INDEX_NONE && 
						MasterBoneIndex < MasterPoseComponent->SpaceBases.Num())
					{
						BoneIndex = MasterBoneIndex;
					}
				}
			}

		   if(BoneIndex != INDEX_NONE)
		   {
			   BoneMatrices[Index] = U2PMatrix((*BoneTransforms)[BoneIndex].ToMatrixWithScale());
		   }
		   else
		   {
			   BoneMatrices[Index] = PxMat44::createIdentity();
		   }
		}

		NxClothingActor* ClothingActor = ClothingActors[ActorIdx].ApexClothingActor;

		check(ClothingActor);

		// if bUseInternalboneOrder is set, "NumUsedBones" works, otherwise have to use "getNumBones" 
		ClothingActor->updateState(
			PxGlobalPose, 
			BoneMatrices.GetTypedData(), 
			sizeof(physx::PxMat44), 
			NumUsedBones,
			CurTeleportMode);
	}

	// reset to Continuous
	ClothTeleportMode = FClothingActor::Continuous;
}
#endif// #if WITH_APEX_CLOTHING

void USkeletalMeshComponent::TickClothing(float DeltaTime)
{
#if WITH_APEX_CLOTHING
	// animated but bone transforms were not updated because it was not rendered
	if(bPoseTicked && !bRecentlyRendered)
	{
		ForceClothNextUpdateTeleportAndReset();
	}
	else
	{
	ValidateClothingActors();
		UpdateClothState(DeltaTime);
	}
#if 0 //if turn on this flag, you can check which primitive objects are activated for collision detection
	DrawDebugClothCollisions();
#endif 
#endif// #if WITH_APEX_CLOTHING
}

void USkeletalMeshComponent::GetUpdateClothSimulationData(TArray<FClothSimulData>& OutClothSimData)
{
#if WITH_APEX_CLOTHING

	int32 NumClothingActors = ClothingActors.Num();

	if(NumClothingActors == 0 || bDisableClothSimulation)
	{
		OutClothSimData.Empty();
		return;
	}

	if(OutClothSimData.Num() != NumClothingActors)
	{
		OutClothSimData.Empty(NumClothingActors);
		OutClothSimData.AddZeroed(NumClothingActors);
	}

	bool bSimulated = false;

	for(int32 ActorIndex=0; ActorIndex<NumClothingActors; ActorIndex++)
	{
		if(!IsValidClothingActor(ActorIndex))
		{
			OutClothSimData[ActorIndex].ClothSimulPositions.Empty();
			OutClothSimData[ActorIndex].ClothSimulNormals.Empty();
			continue;
		}

		NxClothingActor* ClothingActor = ClothingActors[ActorIndex].ApexClothingActor;

		// update simulation positions & normals
		if(ClothingActor)
		{
			uint32 NumSimulVertices = ClothingActor->getNumSimulationVertices();

			if(NumSimulVertices > 0)
			{
				bSimulated = true;

				FClothSimulData& ClothData = OutClothSimData[ActorIndex];

				if(ClothData.ClothSimulPositions.Num() != NumSimulVertices)
				{
					ClothData.ClothSimulPositions.Empty(NumSimulVertices);
					ClothData.ClothSimulPositions.AddUninitialized(NumSimulVertices);
					ClothData.ClothSimulNormals.Empty(NumSimulVertices);
					ClothData.ClothSimulNormals.AddUninitialized(NumSimulVertices);
				}

				const physx::PxVec3* Vertices = ClothingActor->getSimulationPositions();
				const physx::PxVec3* Normals = ClothingActor->getSimulationNormals();

				for(uint32 VertexIndex=0; VertexIndex<NumSimulVertices; VertexIndex++)
				{
					ClothData.ClothSimulPositions[VertexIndex] = P2UVector(Vertices[VertexIndex]);
					ClothData.ClothSimulNormals[VertexIndex] = P2UVector(Normals[VertexIndex]);
				}
			}
		}
	}
	//no simulated vertices 
	if(!bSimulated)
	{
		OutClothSimData.Empty();
	}
#endif// #if WITH_APEX_CLOTHING
}

void USkeletalMeshComponent::FreezeClothSection(bool bFreeze)
{
#if WITH_APEX_CLOTHING

	int32 NumActors = ClothingActors.Num();

	for(int32 ActorIdx=0; ActorIdx<NumActors; ActorIdx++)
	{
		NxClothingActor* ClothingActor = ClothingActors[ActorIdx].ApexClothingActor;

		if (ClothingActor)
		{
			ClothingActor->setFrozen(bFreeze);
		}
	}
#endif// #if WITH_APEX_CLOTHING
}

bool USkeletalMeshComponent::IsValidClothingActor(int32 ActorIndex) const
{
#if WITH_APEX_CLOTHING

	//false if ActorIndex is out-range
	if(ActorIndex >= SkeletalMesh->ClothingAssets.Num()
	|| ActorIndex >= ClothingActors.Num())
	{
		return false;
	}

	if(ClothingActors[ActorIndex].ApexClothingActor
	&& SkeletalMesh->ClothingAssets[ActorIndex].ApexClothingAsset->IsValid())
	{
		return true;
	}

	return false;
	
#else
	return false;
#endif// #if WITH_APEX_CLOTHING

}

void USkeletalMeshComponent::DrawClothingNormals(FPrimitiveDrawInterface* PDI)
{
#if WITH_APEX_CLOTHING
	if (!SkeletalMesh) 
	{
		return;
	}

	int32 NumActors = ClothingActors.Num();

	for(int32 ActorIdx=0; ActorIdx<NumActors; ActorIdx++)
	{
		NxClothingActor* ClothingActor = ClothingActors[ActorIdx].ApexClothingActor;

		if(!IsValidClothingActor(ActorIdx))
		{
			continue;
		}

		if (ClothingActor)
		{
			uint32 NumSimulVertices = ClothingActor->getNumSimulationVertices();

			// simulation is enabled and there exist simulated vertices
			if(!bDisableClothSimulation && NumSimulVertices > 0)
			{
				const physx::PxVec3* Vertices = ClothingActor->getSimulationPositions();
				const physx::PxVec3* Normals = ClothingActor->getSimulationNormals();

				FLinearColor LineColor = FColor::Red;
				FVector Start, End;

				for(uint32 i=0; i<NumSimulVertices; i++)
				{
					Start = P2UVector(Vertices[i]);
					End = P2UVector(Normals[i]); 
					End *= 5.0f;

					End = Start + End;

					PDI->DrawLine(Start, End, LineColor, SDPG_World);
				}
			}
			else // otherwise, draws initial values loaded from the asset file
			{
				if(SkeletalMesh->ClothingAssets[ActorIdx].ClothVisualizationInfos.Num() == 0)
				{
					LoadClothingVisualizationInfo(ActorIdx);
				}

				int32 PhysicalMeshLOD = PredictedLODLevel;

				// if this asset doesn't have current physical mesh LOD, then skip to visualize
				if(!SkeletalMesh->ClothingAssets[ActorIdx].ClothVisualizationInfos.IsValidIndex(PhysicalMeshLOD))
				{
					continue;
				}

				FClothVisualizationInfo& VisualInfo = SkeletalMesh->ClothingAssets[ActorIdx].ClothVisualizationInfos[PhysicalMeshLOD];				

				uint32 NumVertices = VisualInfo.ClothPhysicalMeshVertices.Num();

				FLinearColor LineColor = FColor::Red;
				FVector Start, End;

				for(uint32 i=0; i<NumVertices; i++)
				{
					Start = VisualInfo.ClothPhysicalMeshVertices[i];
					End = VisualInfo.ClothPhysicalMeshNormals[i]; 

					End *= 5.0f;

					End = Start + End;

					PDI->DrawLine(Start, End, LineColor, SDPG_World);
				}
			}
		}
	}
#endif // #if WITH_APEX_CLOTHING
}

void USkeletalMeshComponent::DrawClothingTangents(FPrimitiveDrawInterface* PDI)
{
#if WITH_APEX_CLOTHING

	if (!SkeletalMesh || !MeshObject)
	{
		return;
	}

	int32 NumActors = ClothingActors.Num();

	for(int32 ActorIdx=0; ActorIdx<NumActors; ActorIdx++)
	{
		if(!IsValidClothingActor(ActorIdx))
		{
			continue;
		}

		NxClothingActor* ClothingActor = ClothingActors[ActorIdx].ApexClothingActor;

		if (ClothingActor)
		{
			uint32 NumSimulVertices = ClothingActor->getNumSimulationVertices();

			// simulation is enabled and there exist simulated vertices
			if(!bDisableClothSimulation && NumSimulVertices > 0)
			{
				uint16 ChunkIndex = 0;

				FStaticLODModel& LODModel = MeshObject->GetSkeletalMeshResource().LODModels[PredictedLODLevel];

				TArray<uint32> SectionIndices;
				SkeletalMesh->GetClothSectionIndices(PredictedLODLevel, ActorIdx, SectionIndices);

				int32 NumSections = SectionIndices.Num();

				for(int32 SecIdx=0; SecIdx <NumSections; SecIdx++)
				{
					uint16 SectionIndex = SectionIndices[SecIdx];

					ChunkIndex = LODModel.Sections[SectionIndex].ChunkIndex;

					FSkelMeshChunk& Chunk = LODModel.Chunks[ChunkIndex];

					const physx::PxVec3* SimulVertices = ClothingActor->getSimulationPositions();
					const physx::PxVec3* SimulNormals = ClothingActor->getSimulationNormals();

					uint32 NumMppingData = Chunk.ApexClothMappingData.Num();

					FVector Start, End;		

					for(uint32 MappingIndex=0; MappingIndex < NumMppingData; MappingIndex++)
					{
						FVector4 BaryCoordPos = Chunk.ApexClothMappingData[MappingIndex].PositionBaryCoordsAndDist;
						FVector4 BaryCoordNormal = Chunk.ApexClothMappingData[MappingIndex].NormalBaryCoordsAndDist;
						FVector4 BaryCoordTangent = Chunk.ApexClothMappingData[MappingIndex].TangentBaryCoordsAndDist;
						uint16*  SimulIndices = Chunk.ApexClothMappingData[MappingIndex].SimulMeshVertIndices;

						bool bFixed = (SimulIndices[3] == 0xFFFF);

						if(bFixed)
						{
							//if met a fixed vertex, skip to draw simulated vertices
							continue;
						}

						check(SimulIndices[0] < NumSimulVertices && SimulIndices[1] < NumSimulVertices && SimulIndices[2] < NumSimulVertices);

						PxVec3 a = SimulVertices[SimulIndices[0]];
						PxVec3 b = SimulVertices[SimulIndices[1]];
						PxVec3 c = SimulVertices[SimulIndices[2]];

						PxVec3 na = SimulNormals[SimulIndices[0]];
						PxVec3 nb = SimulNormals[SimulIndices[1]];
						PxVec3 nc = SimulNormals[SimulIndices[2]];

						FVector Position = P2UVector( 
							BaryCoordPos.X*(a + BaryCoordPos.W*na)
						  + BaryCoordPos.Y*(b + BaryCoordPos.W*nb)
						  + BaryCoordPos.Z*(c + BaryCoordPos.W*nc));

						FVector Normal = P2UVector(  
							  BaryCoordNormal.X*(a + BaryCoordNormal.W*na)
							+ BaryCoordNormal.Y*(b + BaryCoordNormal.W*nb) 
							+ BaryCoordNormal.Z*(c + BaryCoordNormal.W*nc));

						FVector Tangent = P2UVector(  
							  BaryCoordTangent.X*(a + BaryCoordTangent.W*na) 
							+ BaryCoordTangent.Y*(b + BaryCoordTangent.W*nb)  
							+ BaryCoordTangent.Z*(c + BaryCoordTangent.W*nc));

						Normal -= Position;
						Normal.Normalize();

						Tangent -= Position;
						Tangent.Normalize();

						FVector BiNormal = FVector::CrossProduct(Normal, Tangent);
						BiNormal.Normalize();

						Start = Position;
						End = Normal; 
						End *= 5.0f;
						End = Start + End;

						PDI->DrawLine(Start, End, FColor::Green, SDPG_World);

						End = Tangent; 
						End *= 5.0f;
						End = Start + End;

						PDI->DrawLine(Start, End, FColor::Red, SDPG_World);

						End = BiNormal; 
						End *= 5.0f;
						End = Start + End;

						PDI->DrawLine(Start, End, FColor::Blue, SDPG_World);
					}
				}
			}
			else
			{
				if(SkeletalMesh->ClothingAssets[ActorIdx].ClothVisualizationInfos.Num() == 0)
				{
					LoadClothingVisualizationInfo(ActorIdx);
				}

				int32 PhysicalMeshLOD = PredictedLODLevel;

				// if this asset doesn't have current physical mesh LOD, then skip to visualize
				if(!SkeletalMesh->ClothingAssets[ActorIdx].ClothVisualizationInfos.IsValidIndex(PhysicalMeshLOD))
				{
					continue;
				}

				FClothVisualizationInfo& VisualInfo = SkeletalMesh->ClothingAssets[ActorIdx].ClothVisualizationInfos[PhysicalMeshLOD];				

				NumSimulVertices = VisualInfo.ClothPhysicalMeshVertices.Num();

				uint16 ChunkIndex = 0;

				FStaticLODModel& LODModel = MeshObject->GetSkeletalMeshResource().LODModels[PredictedLODLevel];

				TArray<uint32> SectionIndices;
				SkeletalMesh->GetClothSectionIndices(PredictedLODLevel, ActorIdx, SectionIndices);

				int32 NumSections = SectionIndices.Num();

				for(int32 SecIdx=0; SecIdx <NumSections; SecIdx++)
				{
					uint16 SectionIndex = SectionIndices[SecIdx];

					ChunkIndex = LODModel.Sections[SectionIndex].ChunkIndex;

					FSkelMeshChunk& Chunk = LODModel.Chunks[ChunkIndex];

					const TArray<FVector>& SimulVertices = VisualInfo.ClothPhysicalMeshVertices;
					const TArray<FVector>& SimulNormals = VisualInfo.ClothPhysicalMeshNormals;

					uint32 NumMppingData = Chunk.ApexClothMappingData.Num();

					FVector Start, End;		

					for(uint32 MappingIndex=0; MappingIndex < NumMppingData; MappingIndex++)
					{
						FVector4 BaryCoordPos = Chunk.ApexClothMappingData[MappingIndex].PositionBaryCoordsAndDist;
						FVector4 BaryCoordNormal = Chunk.ApexClothMappingData[MappingIndex].NormalBaryCoordsAndDist;
						FVector4 BaryCoordTangent = Chunk.ApexClothMappingData[MappingIndex].TangentBaryCoordsAndDist;
						uint16*  SimulIndices = Chunk.ApexClothMappingData[MappingIndex].SimulMeshVertIndices;

						bool bFixed = (SimulIndices[3] == 0xFFFF);

						check(SimulIndices[0] < NumSimulVertices && SimulIndices[1] < NumSimulVertices && SimulIndices[2] < NumSimulVertices);

						FVector a = SimulVertices[SimulIndices[0]];
						FVector b = SimulVertices[SimulIndices[1]];
						FVector c = SimulVertices[SimulIndices[2]];

						FVector na = SimulNormals[SimulIndices[0]];
						FVector nb = SimulNormals[SimulIndices[1]];
						FVector nc = SimulNormals[SimulIndices[2]];

						FVector Position = 
							BaryCoordPos.X*(a + BaryCoordPos.W*na)
							+ BaryCoordPos.Y*(b + BaryCoordPos.W*nb)
							+ BaryCoordPos.Z*(c + BaryCoordPos.W*nc);

						FVector Normal = 
							BaryCoordNormal.X*(a + BaryCoordNormal.W*na)
							+ BaryCoordNormal.Y*(b + BaryCoordNormal.W*nb) 
							+ BaryCoordNormal.Z*(c + BaryCoordNormal.W*nc);

						FVector Tangent = 
							BaryCoordTangent.X*(a + BaryCoordTangent.W*na) 
							+ BaryCoordTangent.Y*(b + BaryCoordTangent.W*nb)  
							+ BaryCoordTangent.Z*(c + BaryCoordTangent.W*nc);

						Normal -= Position;
						Normal.Normalize();

						Tangent -= Position;
						Tangent.Normalize();

						FVector BiNormal = FVector::CrossProduct(Normal, Tangent);
						BiNormal.Normalize();

						Start = Position;
						End = Normal; 
						End *= 5.0f;
						End = Start + End;

						PDI->DrawLine(Start, End, FColor::Green, SDPG_World);

						End = Tangent; 
						End *= 5.0f;
						End = Start + End;

						PDI->DrawLine(Start, End, FColor::Red, SDPG_World);

						End = BiNormal; 
						End *= 5.0f;
						End = Start + End;

						PDI->DrawLine(Start, End, FColor::Blue, SDPG_World);
					}
					}
				}
			}
	}

#endif // #if WITH_APEX_CLOTHING
}

void USkeletalMeshComponent::DrawClothingCollisionVolumes(FPrimitiveDrawInterface* PDI)
{
#if WITH_APEX_CLOTHING
	if (!SkeletalMesh
	|| SkeletalMesh->ClothingAssets.Num() == 0) 
	{
		return;
	}

	FColor Colors[3] = { FColor::Red, FColor::Green, FColor::Blue };
	FColor GrayColor(50, 50, 50); // dark gray to represent ignored collisions

	const int32 MaxSphereCollisions = 32; // Apex clothing supports up to 16 capsules or 32 spheres because 1 capsule takes 2 spheres

	for(int32 AssetIdx=0; AssetIdx < SkeletalMesh->ClothingAssets.Num(); AssetIdx++)
	{
		TArray<FApexClothCollisionVolumeData>& Collisions = SkeletalMesh->ClothingAssets[AssetIdx].ClothCollisionVolumes;
		int32 NumCollisions = Collisions.Num();
		int32 SphereCount = 0;

		for(int32 i=0; i<NumCollisions; i++)
		{
			if(Collisions[i].BoneIndex < 0)
			{
				continue;
			}

			FName BoneName = SkeletalMesh->ClothingAssets[AssetIdx].ApexClothingAsset->GetConvertedBoneName(Collisions[i].BoneIndex);
			
			int32 BoneIndex = GetBoneIndex(BoneName);

			if(BoneIndex < 0)
			{
				continue;
			}

			FMatrix BoneMat = GetBoneMatrix(BoneIndex);

			FMatrix LocalToWorld = Collisions[i].LocalPose * BoneMat;

			if(Collisions[i].IsCapsule())
			{
				FColor CapsuleColor = Colors[AssetIdx % 3];
				if (SphereCount >= MaxSphereCollisions)
				{
					CapsuleColor = GrayColor; // Draw in gray to show that these collisions are ignored
				}

				const int32 CapsuleSides = FMath::Clamp<int32>(Collisions[i].CapsuleRadius/4.f, 16, 64);
				float CapsuleHalfHeight = (Collisions[i].CapsuleHeight*0.5f+Collisions[i].CapsuleRadius);
				float CapsuleRadius = Collisions[i].CapsuleRadius*2.f;
				// swapped Y-axis and Z-axis to convert apex coordinate to UE coordinate
				DrawWireCapsule(PDI, LocalToWorld.GetOrigin(), LocalToWorld.GetUnitAxis(EAxis::X), LocalToWorld.GetUnitAxis(EAxis::Z), LocalToWorld.GetUnitAxis(EAxis::Y), CapsuleColor, Collisions[i].CapsuleRadius, CapsuleHalfHeight, CapsuleSides, SDPG_World);
				SphereCount += 2; // 1 capsule takes 2 spheres
			}
			}

		// draw bone spheres
		TArray<FApexClothBoneSphereData>& Spheres = SkeletalMesh->ClothingAssets[AssetIdx].ClothBoneSpheres;
		TArray<FVector> SpherePositions;

		int32 NumSpheres = Spheres.Num();

		SpherePositions.AddUninitialized(NumSpheres);

		for(int32 i=0; i<NumSpheres; i++)
		{
			if(Spheres[i].BoneIndex < 0)
			{
				continue;
			}

			FName BoneName = SkeletalMesh->ClothingAssets[AssetIdx].ApexClothingAsset->GetConvertedBoneName(Spheres[i].BoneIndex);

			int32 BoneIndex = GetBoneIndex(BoneName);

			if(BoneIndex < 0)
			{
				continue;
			}

			FMatrix BoneMat = GetBoneMatrix(BoneIndex);

			FVector SpherePos = BoneMat.TransformPosition(Spheres[i].LocalPos);

			SpherePositions[i] = SpherePos;
			FTransform SphereTransform(FQuat::Identity, SpherePos);

			FColor SphereColor = Colors[AssetIdx % 3];
			if (SphereCount >= MaxSphereCollisions)
			{
				SphereColor = GrayColor; // Draw in gray to show that these collisions are ignored
			}

			DrawWireSphere(PDI, SphereTransform, SphereColor, Spheres[i].Radius, 10, SDPG_World);
			SphereCount++;
		}

		// draw connections between bone spheres, this makes 2 sphere to a capsule
		TArray<uint16>& Connections = SkeletalMesh->ClothingAssets[AssetIdx].BoneSphereConnections;

		int32 NumConnections = Connections.Num();

		for(int32 i=0; i<NumConnections; i+=2)
		{
			uint16 Index1 = Connections[i];
			uint16 Index2 = Connections[i+1];

			DrawDebugLine(GetWorld(), SpherePositions[Index1], SpherePositions[Index2], FColor::Magenta, false, -1.0f, SDPG_Foreground);
		}
	}
#endif // #if WITH_APEX_CLOTHING
}

void USkeletalMeshComponent::DrawClothingFixedVertices(FPrimitiveDrawInterface* PDI)
{
#if WITH_APEX_CLOTHING

	if (!SkeletalMesh || !MeshObject)
	{
		return;
	}

	int32 NumActors = ClothingActors.Num();

	TArray<FMatrix> RefToLocals;
	// get local matrices for skinning
	UpdateRefToLocalMatrices(RefToLocals, this, GetSkeletalMeshResource(), 0, NULL);
	const float Inv_255 = 1.f/255.f;

	for(int32 ActorIdx=0; ActorIdx<NumActors; ActorIdx++)
	{
		if(!IsValidClothingActor(ActorIdx))
		{
			continue;
		}

		NxClothingActor* ClothingActor = ClothingActors[ActorIdx].ApexClothingActor;

		if (ClothingActor)
		{
			uint32 NumSimulVertices = ClothingActor->getNumSimulationVertices();

			if(SkeletalMesh->ClothingAssets[ActorIdx].ClothVisualizationInfos.Num() == 0)
			{
				LoadClothingVisualizationInfo(ActorIdx);
			}

			int32 PhysicalMeshLOD = PredictedLODLevel;

			// if this asset doesn't have current physical mesh LOD, then skip to visualize
			if(!SkeletalMesh->ClothingAssets[ActorIdx].ClothVisualizationInfos.IsValidIndex(PhysicalMeshLOD))
			{
				continue;
			}

			FClothVisualizationInfo& VisualInfo = SkeletalMesh->ClothingAssets[ActorIdx].ClothVisualizationInfos[PhysicalMeshLOD];				

			NumSimulVertices = VisualInfo.ClothPhysicalMeshVertices.Num();

			uint16 ChunkIndex = 0;

			FStaticLODModel& LODModel = MeshObject->GetSkeletalMeshResource().LODModels[PredictedLODLevel];

			TArray<uint32> SectionIndices;
			SkeletalMesh->GetClothSectionIndices(PredictedLODLevel, ActorIdx, SectionIndices);

			int32 NumSections = SectionIndices.Num();

			for(int32 SecIdx=0; SecIdx <NumSections; SecIdx++)
			{
				uint16 SectionIndex = SectionIndices[SecIdx];

				ChunkIndex = LODModel.Sections[SectionIndex].ChunkIndex;

				FSkelMeshChunk& Chunk = LODModel.Chunks[ChunkIndex];

				const TArray<FVector>& SimulVertices = VisualInfo.ClothPhysicalMeshVertices;
				const TArray<FVector>& SimulNormals = VisualInfo.ClothPhysicalMeshNormals;

				uint32 NumMppingData = Chunk.ApexClothMappingData.Num();

				FVector Start, End;		

				for(uint32 MappingIndex=0; MappingIndex < NumMppingData; MappingIndex++)
				{
					FVector4 BaryCoordPos = Chunk.ApexClothMappingData[MappingIndex].PositionBaryCoordsAndDist;
					FVector4 BaryCoordNormal = Chunk.ApexClothMappingData[MappingIndex].NormalBaryCoordsAndDist;
					FVector4 BaryCoordTangent = Chunk.ApexClothMappingData[MappingIndex].TangentBaryCoordsAndDist;
					uint16*  SimulIndices = Chunk.ApexClothMappingData[MappingIndex].SimulMeshVertIndices;

					bool bFixed = (SimulIndices[3] == 0xFFFF);

					if(!bFixed)
					{
						continue;
					}

					check(SimulIndices[0] < NumSimulVertices && SimulIndices[1] < NumSimulVertices && SimulIndices[2] < NumSimulVertices);
					
					FVector a = SimulVertices[SimulIndices[0]];
					FVector b = SimulVertices[SimulIndices[1]];
					FVector c = SimulVertices[SimulIndices[2]];

					FVector na = SimulNormals[SimulIndices[0]];
					FVector nb = SimulNormals[SimulIndices[1]];
					FVector nc = SimulNormals[SimulIndices[2]];

					FVector Position = 
						BaryCoordPos.X*(a + BaryCoordPos.W*na)
						+ BaryCoordPos.Y*(b + BaryCoordPos.W*nb)
						+ BaryCoordPos.Z*(c + BaryCoordPos.W*nc);

					FSoftSkinVertex& SoftVert = Chunk.SoftVertices[MappingIndex];

					// skinning, APEX clothing supports up to 4 bone influences for now
					const uint8* BoneIndices = SoftVert.InfluenceBones;
					const uint8* BoneWeights = SoftVert.InfluenceWeights;
				
					FMatrix SkinningMat(ForceInitToZero);

					for(int32 BoneWeightIdx=0; BoneWeightIdx < Chunk.MaxBoneInfluences; BoneWeightIdx++)
					{
						float Weight = BoneWeights[BoneWeightIdx]*Inv_255;
						SkinningMat += RefToLocals[Chunk.BoneMap[BoneIndices[BoneWeightIdx]]]*Weight;
					}

					FVector SkinnedPosition = SkinningMat.TransformPosition(Position);
					// draws a skinned fixed vertex
					PDI->DrawPoint(SkinnedPosition, FColor::Yellow, 2, SDPG_World);
				}
			}
		}
	}

#endif // #if WITH_APEX_CLOTHING
}

void USkeletalMeshComponent::LoadClothingVisualizationInfo(int32 AssetIndex)
{
#if WITH_APEX_CLOTHING

	if (!SkeletalMesh || !SkeletalMesh->ClothingAssets.IsValidIndex(AssetIndex)) 
	{
		return;
	}


	FClothingAssetData& AssetData = SkeletalMesh->ClothingAssets[AssetIndex];
	NxClothingAsset* ApexClothingAsset = AssetData.ApexClothingAsset->GetAsset();
	const NxParameterized::Interface* AssetParams = ApexClothingAsset->getAssetNxParameterized();

	int32 NumPhysicalLODs;
	NxParameterized::getParamArraySize(*AssetParams, "physicalMeshes", NumPhysicalLODs);

	check(NumPhysicalLODs == ApexClothingAsset->getNumGraphicalLodLevels());

	// prepares to load data for each LOD
	AssetData.ClothVisualizationInfos.Empty(NumPhysicalLODs);
	AssetData.ClothVisualizationInfos.AddZeroed(NumPhysicalLODs);

	for(int32 LODIndex=0; LODIndex<NumPhysicalLODs; LODIndex++)
	{
		FClothVisualizationInfo& VisualInfo = AssetData.ClothVisualizationInfos[LODIndex];

		char ParameterName[MAX_SPRINTF];
		FCStringAnsi::Sprintf(ParameterName, "physicalMeshes[%d]", LODIndex);

		NxParameterized::Interface* PhysicalMeshParams;
		uint32 NumVertices = 0;
		uint32 NumIndices = 0;
		// physical mesh vertices & normals
		if (NxParameterized::getParamRef(*AssetParams, ParameterName, PhysicalMeshParams))
		{
			if(PhysicalMeshParams != NULL)
			{
				verify(NxParameterized::getParamU32(*PhysicalMeshParams, "physicalMesh.numVertices", NumVertices));

				// physical mesh vertices
				physx::PxI32 VertexCount = 0;
				if (NxParameterized::getParamArraySize(*PhysicalMeshParams, "physicalMesh.vertices", VertexCount))
				{
					check(VertexCount == NumVertices);
					VisualInfo.ClothPhysicalMeshVertices.Empty(NumVertices);

					for (uint32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
					{
						FCStringAnsi::Sprintf( ParameterName, "physicalMesh.vertices[%d]", VertexIndex );
						NxParameterized::Handle MeshVertexHandle(*PhysicalMeshParams);
						if (NxParameterized::findParam(*PhysicalMeshParams, ParameterName, MeshVertexHandle) != NULL)
						{
							physx::PxVec3  Vertex;
							MeshVertexHandle.getParamVec3(Vertex);
							VisualInfo.ClothPhysicalMeshVertices.Add(P2UVector(Vertex));
						}
					}
				}

				// bone weights & bone indices

				physx::PxI32 BoneWeightsCount = 0;
				if (NxParameterized::getParamArraySize(*PhysicalMeshParams, "physicalMesh.boneWeights", BoneWeightsCount))
				{
					VisualInfo.ClothPhysicalMeshBoneWeightsInfo.AddZeroed(VertexCount);

					int32 MaxBoneWeights = BoneWeightsCount / VertexCount;
					VisualInfo.NumMaxBoneInfluences = MaxBoneWeights;

					physx::PxI32 BoneIndicesCount = 0;
					verify(NxParameterized::getParamArraySize(*PhysicalMeshParams, "physicalMesh.boneIndices", BoneIndicesCount));
					check(BoneIndicesCount == BoneWeightsCount);

					NxParameterized::Handle BoneWeightHandle(*PhysicalMeshParams);
					for (int32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
					{
						for(uint8 WeightIndex=0; WeightIndex < MaxBoneWeights; ++WeightIndex)
						{
							uint32 CurBoneWeightIndex = VertexIndex*MaxBoneWeights + WeightIndex;

							NxParameterized::Handle BoneIndexHandle(*PhysicalMeshParams);
							FCStringAnsi::Sprintf( ParameterName, "physicalMesh.boneIndices[%d]", CurBoneWeightIndex );
							verify(NxParameterized::findParam(*PhysicalMeshParams, ParameterName, BoneIndexHandle));
							uint16 BoneIndex;
							BoneIndexHandle.getParamU16(BoneIndex);
							VisualInfo.ClothPhysicalMeshBoneWeightsInfo[VertexIndex].Indices[WeightIndex] = BoneIndex;

							NxParameterized::Handle BoneWeightHandle(*PhysicalMeshParams);
							FCStringAnsi::Sprintf( ParameterName, "physicalMesh.boneWeights[%d]", CurBoneWeightIndex );
							verify(NxParameterized::findParam(*PhysicalMeshParams, ParameterName, BoneWeightHandle));
							float BoneWeight;
							BoneWeightHandle.getParamF32(BoneWeight);
							VisualInfo.ClothPhysicalMeshBoneWeightsInfo[VertexIndex].Weights[WeightIndex] = BoneWeight;
						}
					}
				}

				// physical mesh normals
				physx::PxI32 NormalCount = 0;
				if (NxParameterized::getParamArraySize(*PhysicalMeshParams, "physicalMesh.normals", NormalCount))
				{
					check(NormalCount == NumVertices);
					VisualInfo.ClothPhysicalMeshNormals.Empty(NormalCount);

					for (int32 NormalIndex = 0; NormalIndex < NormalCount; ++NormalIndex)
					{
						FCStringAnsi::Sprintf( ParameterName, "physicalMesh.normals[%d]", NormalIndex );
						NxParameterized::Handle MeshNormalHandle(*PhysicalMeshParams);
						if (NxParameterized::findParam(*PhysicalMeshParams, ParameterName, MeshNormalHandle) != NULL)
						{
							physx::PxVec3  PxNormal;
							MeshNormalHandle.getParamVec3(PxNormal);
							VisualInfo.ClothPhysicalMeshNormals.Add(P2UVector(PxNormal));
						}
					}
				}
				// physical mesh indices
				verify(NxParameterized::getParamU32(*PhysicalMeshParams, "physicalMesh.numIndices", NumIndices));
				physx::PxI32 IndexCount = 0;
				if (NxParameterized::getParamArraySize(*PhysicalMeshParams, "physicalMesh.indices", IndexCount))
				{
					check(IndexCount == NumIndices);
					VisualInfo.ClothPhysicalMeshIndices.Empty(NumIndices);

					for (uint32 IndexIdx = 0; IndexIdx < NumIndices; ++IndexIdx)
					{
						FCStringAnsi::Sprintf( ParameterName, "physicalMesh.indices[%d]", IndexIdx );
						NxParameterized::Handle MeshIndexHandle(*PhysicalMeshParams);
						if (NxParameterized::findParam(*PhysicalMeshParams, ParameterName, MeshIndexHandle) != NULL)
						{
							uint32 IndexParam;
							MeshIndexHandle.getParamU32(IndexParam);
							VisualInfo.ClothPhysicalMeshIndices.Add(IndexParam);
						}
					}
				}

				// constraint coefficient parameters (max distances & backstop data)
				verify(NxParameterized::getParamF32(*PhysicalMeshParams, "physicalMesh.maximumMaxDistance", VisualInfo.MaximumMaxDistance));

				physx::PxI32  ConstraintCoeffCount = 0;
				if (NxParameterized::getParamArraySize(*PhysicalMeshParams, "physicalMesh.constrainCoefficients", ConstraintCoeffCount))
				{
					check(ConstraintCoeffCount == NumVertices);
					VisualInfo.ClothConstrainCoeffs.Empty(ConstraintCoeffCount);
					VisualInfo.ClothConstrainCoeffs.AddZeroed(ConstraintCoeffCount);

					for (uint32 ConstCoeffIdx = 0; ConstCoeffIdx < NumIndices; ++ConstCoeffIdx)
					{
						// max distances
						FCStringAnsi::Sprintf( ParameterName, "physicalMesh.constrainCoefficients[%d].maxDistance", ConstCoeffIdx );
						NxParameterized::Handle MeshConstCoeffHandle(*PhysicalMeshParams);
						if (NxParameterized::findParam(*PhysicalMeshParams, ParameterName, MeshConstCoeffHandle) != NULL)
						{
							float MaxDistance;
							MeshConstCoeffHandle.getParamF32(MaxDistance);
							VisualInfo.ClothConstrainCoeffs[ConstCoeffIdx].ClothMaxDistance = MaxDistance;
						}

						// backstop data
						FCStringAnsi::Sprintf( ParameterName, "physicalMesh.constrainCoefficients[%d].collisionSphereRadius", ConstCoeffIdx );
						if (NxParameterized::findParam(*PhysicalMeshParams, ParameterName, MeshConstCoeffHandle) != NULL)
						{
							float BackstopCollisionSphereRadius;
							MeshConstCoeffHandle.getParamF32(BackstopCollisionSphereRadius);
							VisualInfo.ClothConstrainCoeffs[ConstCoeffIdx].ClothBackstopRadius = BackstopCollisionSphereRadius;
						}

						FCStringAnsi::Sprintf( ParameterName, "physicalMesh.constrainCoefficients[%d].collisionSphereDistance", ConstCoeffIdx );
						if (NxParameterized::findParam(*PhysicalMeshParams, ParameterName, MeshConstCoeffHandle) != NULL)
						{
							float BackstopCollisionSphereDistance;
							MeshConstCoeffHandle.getParamF32(BackstopCollisionSphereDistance);
							VisualInfo.ClothConstrainCoeffs[ConstCoeffIdx].ClothBackstopDistance = BackstopCollisionSphereDistance;
						}
					}
				}
			}
		}
	}

#endif // #if WITH_APEX_CLOTHING
}

void USkeletalMeshComponent::LoadAllClothingVisualizationInfos()
{
#if WITH_APEX_CLOTHING

	if (!SkeletalMesh || SkeletalMesh->ClothingAssets.Num() == 0) 
	{
		return;
	}

	int32 NumAssets = SkeletalMesh->ClothingAssets.Num();

	for(int32 AssetIdx=0; AssetIdx<NumAssets; AssetIdx++)
	{
		LoadClothingVisualizationInfo(AssetIdx);
	}
#endif // #if WITH_APEX_CLOTHING
}

void USkeletalMeshComponent::DrawClothingMaxDistances(FPrimitiveDrawInterface* PDI)
{
#if WITH_APEX_CLOTHING
	if (!SkeletalMesh
		|| SkeletalMesh->ClothingAssets.Num() == 0) 
	{
		return;
	}

	int32 PhysicalMeshLOD = PredictedLODLevel;

	int32 NumAssets = SkeletalMesh->ClothingAssets.Num();

	for(int32 AssetIdx=0; AssetIdx < NumAssets; AssetIdx++)
	{
		if(SkeletalMesh->ClothingAssets[AssetIdx].ClothVisualizationInfos.Num() == 0)
		{
			LoadClothingVisualizationInfo(AssetIdx);
		}

		// if this asset doesn't have current physical mesh LOD, then skip to visualize
		if(!SkeletalMesh->ClothingAssets[AssetIdx].ClothVisualizationInfos.IsValidIndex(PhysicalMeshLOD))
		{
			continue;
		}

		FClothVisualizationInfo& VisualInfo = SkeletalMesh->ClothingAssets[AssetIdx].ClothVisualizationInfos[PhysicalMeshLOD];

		int32 NumMaxDistances = VisualInfo.ClothConstrainCoeffs.Num();
		for(int32 MaxDistIdx=0; MaxDistIdx < NumMaxDistances; MaxDistIdx++)
		{
			float MaxDistance = VisualInfo.ClothConstrainCoeffs[MaxDistIdx].ClothMaxDistance;
			if(MaxDistance > 0.0f)
			{
				FVector LineStart = VisualInfo.ClothPhysicalMeshVertices[MaxDistIdx];
				FVector LineEnd = LineStart + (VisualInfo.ClothPhysicalMeshNormals[MaxDistIdx] * MaxDistance);

				// calculates gray scale for this line color
				uint8 GrayLevel = (uint8)((MaxDistance / VisualInfo.MaximumMaxDistance)*255);
				PDI->DrawLine(LineStart, LineEnd, FColor(GrayLevel, GrayLevel, GrayLevel), SDPG_World);
			}
			else // fixed vertices
			{
				FVector FixedPoint = VisualInfo.ClothPhysicalMeshVertices[MaxDistIdx];
				PDI->DrawPoint(FixedPoint, FColor::Blue, 2, SDPG_World);
			}
		}
	}	

#endif // #if WITH_APEX_CLOTHING
}

void USkeletalMeshComponent::DrawClothingBackstops(FPrimitiveDrawInterface* PDI)
{
#if WITH_APEX_CLOTHING
	if (!SkeletalMesh
		|| SkeletalMesh->ClothingAssets.Num() == 0) 
	{
		return;
	}

	int32 PhysicalMeshLOD = PredictedLODLevel;

	int32 NumAssets = SkeletalMesh->ClothingAssets.Num();

	for(int32 AssetIdx=0; AssetIdx < NumAssets; AssetIdx++)
	{
		if(SkeletalMesh->ClothingAssets[AssetIdx].ClothVisualizationInfos.Num() == 0)
		{
			LoadClothingVisualizationInfo(AssetIdx);
		}

		// if this asset doesn't have current physical mesh LOD, then skip to visualize
		if(!SkeletalMesh->ClothingAssets[AssetIdx].ClothVisualizationInfos.IsValidIndex(PhysicalMeshLOD))
		{
			continue;
		}

		FClothVisualizationInfo& VisualInfo = SkeletalMesh->ClothingAssets[AssetIdx].ClothVisualizationInfos[PhysicalMeshLOD];

		int32 NumBackstopSpheres = VisualInfo.ClothConstrainCoeffs.Num();
		for(int32 BackstopIdx=0; BackstopIdx < NumBackstopSpheres; BackstopIdx++)
		{
			float Distance = VisualInfo.ClothConstrainCoeffs[BackstopIdx].ClothBackstopDistance;
			float MaxDistance = VisualInfo.ClothConstrainCoeffs[BackstopIdx].ClothMaxDistance;

			FColor FixedColor = FColor::White;
			if(Distance > MaxDistance) // Backstop is disabled if the value is bigger than its Max Distance value
			{
				Distance = 0.0f;
				FixedColor = FColor::Black;
			}

			if(Distance > 0.0f)
			{
				FVector LineStart = VisualInfo.ClothPhysicalMeshVertices[BackstopIdx];
				FVector LineEnd = LineStart + (VisualInfo.ClothPhysicalMeshNormals[BackstopIdx] * Distance);
				PDI->DrawLine(LineStart, LineEnd, FColor::Red, SDPG_World);
			}
			else
			if(Distance < 0.0f)
			{
				FVector LineStart = VisualInfo.ClothPhysicalMeshVertices[BackstopIdx];
				FVector LineEnd = LineStart + (VisualInfo.ClothPhysicalMeshNormals[BackstopIdx] * Distance);
				PDI->DrawLine(LineStart, LineEnd, FColor::Blue, SDPG_World);
			}
			else // White means collision distance is set to 0
			{
				FVector FixedPoint = VisualInfo.ClothPhysicalMeshVertices[BackstopIdx];
				PDI->DrawPoint(FixedPoint, FixedColor, 2, SDPG_World);
			}
		}
	}

#endif // #if WITH_APEX_CLOTHING
}

void USkeletalMeshComponent::DrawClothingPhysicalMeshWire(FPrimitiveDrawInterface* PDI)
{
#if WITH_APEX_CLOTHING
	if (!SkeletalMesh
		|| SkeletalMesh->ClothingAssets.Num() == 0) 
	{
		return;
	}

	int32 PhysicalMeshLOD = PredictedLODLevel;

	int32 NumAssets = SkeletalMesh->ClothingAssets.Num();

	TArray<FMatrix> RefToLocals;
	// get local matrices for skinning
	UpdateRefToLocalMatrices(RefToLocals, this, GetSkeletalMeshResource(), 0, NULL);

	for(int32 AssetIdx=0; AssetIdx < NumAssets; AssetIdx++)
	{
		if(SkeletalMesh->ClothingAssets[AssetIdx].ClothVisualizationInfos.Num() == 0)
		{
			LoadClothingVisualizationInfo(AssetIdx);
		}

		// if this asset doesn't have current physical mesh LOD, then skip to visualize
		if(!SkeletalMesh->ClothingAssets[AssetIdx].ClothVisualizationInfos.IsValidIndex(PhysicalMeshLOD))
		{
			continue;
		}

		FClothVisualizationInfo& VisualInfo = SkeletalMesh->ClothingAssets[AssetIdx].ClothVisualizationInfos[PhysicalMeshLOD];

		uint32 NumPhysicalMeshVerts = VisualInfo.ClothPhysicalMeshVertices.Num();
		const physx::PxVec3* SimulVertices = NULL;

		bool bUseSimulatedResult = false;
		// skinning for fixed vertices
		TArray<FVector> SimulatedPhysicalMeshVertices;
		// if cloth simulation is enabled and there exist a clothing actor and simulation vertices, 
		// then draws wire frame using simulated vertices
		if(!bDisableClothSimulation && ClothingActors.IsValidIndex(AssetIdx))
		{
			NxClothingActor* ClothingActor = ClothingActors[AssetIdx].ApexClothingActor;

			if(ClothingActor)
			{
				uint32 NumSimulVertices = ClothingActor->getNumSimulationVertices();

				if(NumSimulVertices > 0)
				{
					bUseSimulatedResult = true;
					SimulatedPhysicalMeshVertices.AddUninitialized(NumPhysicalMeshVerts);

					const physx::PxVec3* SimulVertices = ClothingActor->getSimulationPositions();
					for(uint32 SimulVertIdx=0; SimulVertIdx < NumSimulVertices; SimulVertIdx++)
					{
						SimulatedPhysicalMeshVertices[SimulVertIdx] = P2UVector(SimulVertices[SimulVertIdx]);
					}

					// skinning for fixed vertices
					for(uint32 FixedVertIdx=NumSimulVertices; FixedVertIdx < NumPhysicalMeshVerts; FixedVertIdx++)
					{
						FMatrix SkinningMat(ForceInitToZero);

						for(uint32 BoneWeightIdx=0; BoneWeightIdx < VisualInfo.NumMaxBoneInfluences; BoneWeightIdx++)
						{
							uint16 ApexBoneIndex = VisualInfo.ClothPhysicalMeshBoneWeightsInfo[FixedVertIdx].Indices[BoneWeightIdx];

							FName BoneName = SkeletalMesh->ClothingAssets[AssetIdx].ApexClothingAsset->GetConvertedBoneName(ApexBoneIndex);

							int32 BoneIndex = GetBoneIndex(BoneName);

							if(BoneIndex < 0)
							{
								continue;
							}

							float Weight = VisualInfo.ClothPhysicalMeshBoneWeightsInfo[FixedVertIdx].Weights[BoneWeightIdx];
							SkinningMat += RefToLocals[BoneIndex]*Weight;
						}

						SimulatedPhysicalMeshVertices[FixedVertIdx] = SkinningMat.TransformPosition(VisualInfo.ClothPhysicalMeshVertices[FixedVertIdx]);
					}
				}
			}
		}

		TArray<FVector>* PhysicalMeshVertices = &VisualInfo.ClothPhysicalMeshVertices;
		if(bUseSimulatedResult)
		{
			PhysicalMeshVertices = &SimulatedPhysicalMeshVertices;
		}

		uint32 NumIndices = VisualInfo.ClothPhysicalMeshIndices.Num();
		check(NumIndices % 3 == 0); // check triangles count

		for(uint32 IndexIdx=0; IndexIdx < NumIndices; IndexIdx+=3)
		{
			// draw a triangle
			uint32 Index0 = VisualInfo.ClothPhysicalMeshIndices[IndexIdx];
			uint32 Index1 = VisualInfo.ClothPhysicalMeshIndices[IndexIdx + 1];
			uint32 Index2 = VisualInfo.ClothPhysicalMeshIndices[IndexIdx + 2];

			// If index is greater than Num of vertices, then skip 
			if(Index0 >= NumPhysicalMeshVerts 
			|| Index1 >= NumPhysicalMeshVerts 
			|| Index2 >= NumPhysicalMeshVerts)
			{
				continue;
			}

			FVector V[3];
			V[0] = (*PhysicalMeshVertices)[Index0];
			V[1] = (*PhysicalMeshVertices)[Index1];
			V[2] = (*PhysicalMeshVertices)[Index2];

			float MaxDists[3];
			MaxDists[0] = VisualInfo.ClothConstrainCoeffs[Index0].ClothMaxDistance;
			MaxDists[1] = VisualInfo.ClothConstrainCoeffs[Index1].ClothMaxDistance;
			MaxDists[2] = VisualInfo.ClothConstrainCoeffs[Index2].ClothMaxDistance;

			for(int32 i=0; i<3; i++)
			{
				uint32 Index0 = i;
				uint32 Index1 = i+1;
				if(Index1 >= 3)
				{
					Index1 = 0;
				}

				// calculates gray scaled color
				uint8 GrayLevel0 = (uint8)((MaxDists[Index0] / VisualInfo.MaximumMaxDistance)*255.0f);
				uint8 GrayLevel1 = (uint8)((MaxDists[Index1] / VisualInfo.MaximumMaxDistance)*255.0f);

				uint8 GrayMidColor = (uint8)(((uint32)GrayLevel0 + (uint32)GrayLevel1)*0.5);
				FColor LineColor(GrayMidColor, GrayMidColor, GrayMidColor);
				if(GrayMidColor == 0)
				{
					LineColor = FColor::Magenta;
				}
				else
				{
					LineColor = FColor::White;
				}

				PDI->DrawLine(V[Index0], V[Index1], LineColor, SDPG_World);
			}
		}
	}

#endif // #if WITH_APEX_CLOTHING
}

float USkeletalMeshComponent::GetMass() const
{
	float Mass = 0.0f;
	for (int32 i=0; i < Bodies.Num(); ++i)
	{
		FBodyInstance* BI = Bodies[i];

		if (BI->IsValidBodyInstance())
		{
			Mass += BI->GetBodyMass();
		}
	}
	return Mass;
}

// blueprint callable methods 
float USkeletalMeshComponent::GetClothMaxDistanceScale()
{
#if WITH_APEX_CLOTHING
	return ClothMaxDistanceScale;
#else
	return 1.0f;
#endif// #if WITH_APEX_CLOTHING
}

void USkeletalMeshComponent::SetClothMaxDistanceScale(float Scale)
{
#if WITH_APEX_CLOTHING

	//this scale parameter is also used when new clothing actor is created
	ClothMaxDistanceScale = Scale;

	int32 NumActors = ClothingActors.Num();

	for(int32 ActorIdx=0; ActorIdx<NumActors; ActorIdx++)
	{
		// skip if ClothingActor is NULL or invalid
		if(!IsValidClothingActor(ActorIdx))
		{
			continue;
		}

		NxClothingActor* ClothingActor = ClothingActors[ActorIdx].ApexClothingActor;

		check(ClothingActor);

		NxParameterized::Interface* ActorDesc = ClothingActor->getActorDesc();

		verify(NxParameterized::setParamF32(*ActorDesc, "maxDistanceScale.Scale", Scale));
	}
#endif// #if WITH_APEX_CLOTHING
}


void USkeletalMeshComponent::ResetClothTeleportMode()
{
#if WITH_APEX_CLOTHING
	ClothTeleportMode = FClothingActor::Continuous;
#endif// #if WITH_APEX_CLOTHING
}

void USkeletalMeshComponent::ForceClothNextUpdateTeleport()
{
#if WITH_APEX_CLOTHING
	ClothTeleportMode = FClothingActor::Teleport;
#endif// #if WITH_APEX_CLOTHING
}

void USkeletalMeshComponent::ForceClothNextUpdateTeleportAndReset()
{
#if WITH_APEX_CLOTHING
	ClothTeleportMode = FClothingActor::TeleportAndReset;
#endif// #if WITH_APEX_CLOTHING
}


#undef LOCTEXT_NAMESPACE
