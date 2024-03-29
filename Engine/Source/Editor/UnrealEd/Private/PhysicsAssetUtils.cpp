// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "PhysicsAssetUtils.h"
#include "Developer/MeshUtilities/Public/MeshUtilities.h"
#include "Editor/UnrealEd/Private/ConvexDecompTool.h"


void FPhysAssetCreateParams::Initialize()
{
	MinBoneSize = 5.0f;
	GeomType = EFG_Sphyl;
	VertWeight = EVW_DominantWeight;
	bAlignDownBone = true;
	bCreateJoints = true;
	bWalkPastSmall = true;
	bBodyForAll = false;
	AngularConstraintMode = ACM_Limited;
}

namespace FPhysicsAssetUtils
{

	static const float	DefaultPrimSize = 15.0f;
	static const float	MinPrimSize = 0.5f;

/** Returns INDEX_NONE if no children in the visual asset or if more than one parent */
static int32 GetChildIndex(int32 BoneIndex, USkeletalMesh* SkelMesh, const TArray<FBoneVertInfo>& Infos)
{
	int32 ChildIndex = INDEX_NONE;

	for(int32 i=0; i<SkelMesh->RefSkeleton.GetNum(); i++)
	{
		int32 ParentIndex = SkelMesh->RefSkeleton.GetParentIndex(i);

		if(ParentIndex == BoneIndex)
		{
			if(ChildIndex != INDEX_NONE)
			{
				return INDEX_NONE; // if we already have a child, this bone has more than one so return INDEX_NONE.
			}
			else if (Infos[i].Positions.Num() > 0)
			{
				ChildIndex = i;
			}
		}
	}

	return ChildIndex;
}

static float CalcBoneInfoLength(const FBoneVertInfo& Info)
{
	FBox BoneBox(0);
	for(int32 j=0; j<Info.Positions.Num(); j++)
	{
		BoneBox += Info.Positions[j];
	}

	if(BoneBox.IsValid)
	{
		FVector BoxExtent = BoneBox.GetExtent();
		return BoxExtent.Size();
	}
	else
	{
		return 0.f;
	}
}

/**
 * For all bones below the give bone index, find each ones minimum box dimension, and return the maximum over those bones.
 * This is used to decide if we should create physics for a bone even if its small, because there are good-sized bones below it.
 */
static float GetMaximalMinSizeBelow(int32 BoneIndex, USkeletalMesh* SkelMesh, const TArray<FBoneVertInfo>& Infos)
{
	check( Infos.Num() == SkelMesh->RefSkeleton.GetNum() );

	UE_LOG(LogPhysics, Log, TEXT("-------------------------------------------------"));

	float MaximalMinBoxSize = 0.f;

	// For all bones that are children of the supplied one...
	for(int32 i=BoneIndex; i<SkelMesh->RefSkeleton.GetNum(); i++)
	{
		if( SkelMesh->RefSkeleton.BoneIsChildOf(i, BoneIndex) )
		{
			float MinBoneDim = CalcBoneInfoLength( Infos[i] );
			
			UE_LOG(LogPhysics, Log,  TEXT("Parent: %s Bone: %s Size: %f"), *SkelMesh->RefSkeleton.GetBoneName(BoneIndex).ToString(), *SkelMesh->RefSkeleton.GetBoneName(i).ToString(), MinBoneDim );

			MaximalMinBoxSize = FMath::Max(MaximalMinBoxSize, MinBoneDim);
		}
	}

	return MaximalMinBoxSize;
}

bool CreateFromSkeletalMeshInternal(UPhysicsAsset* PhysicsAsset, USkeletalMesh* SkelMesh, FPhysAssetCreateParams& Params)
{
	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");

	// For each bone, get the vertices most firmly attached to it.
	TArray<FBoneVertInfo> Infos;
	MeshUtilities.CalcBoneVertInfos(SkelMesh, Infos, (Params.VertWeight == EVW_DominantWeight));
	check(Infos.Num() == SkelMesh->RefSkeleton.GetNum());

	bool bHitRoot = false;

	// Iterate over each graphics bone creating body/joint.
	for(int32 i=0; i<SkelMesh->RefSkeleton.GetNum(); i++)
	{
		FName BoneName = SkelMesh->RefSkeleton.GetBoneName(i);

		int32 ParentIndex = INDEX_NONE;
		FName ParentName = NAME_None;
		int32 ParentBodyIndex = INDEX_NONE;

		// If we have already found the 'physics root', we expect a parent.
		if(bHitRoot)
		{
			ParentIndex = SkelMesh->RefSkeleton.GetParentIndex(i);
			ParentName = SkelMesh->RefSkeleton.GetBoneName(ParentIndex);
			ParentBodyIndex = PhysicsAsset->FindBodyIndex(ParentName);

			// Ignore bones with no physical parent (except root)
			if(ParentBodyIndex == INDEX_NONE)
			{
				continue;
			}
		}

		// Determine if we should create a physics body for this bone
		bool bMakeBone = false;

		// If desired - make a body for EVERY bone
		if(Params.bBodyForAll)
		{
			bMakeBone = true;
		}
		// If we have passed the physics 'root', and this bone has no physical parent, ignore it.
		else if(!(bHitRoot && ParentBodyIndex == INDEX_NONE))
		{
			// If bone is big enough - create physics.
			if(CalcBoneInfoLength(Infos[i]) > Params.MinBoneSize)
			{
				bMakeBone = true;
			}

			// If its too small, and we have set the option, see if it has any large children.
			if(!bMakeBone && Params.bWalkPastSmall)
			{
				if(GetMaximalMinSizeBelow(i, SkelMesh, Infos) > Params.MinBoneSize)
				{
					bMakeBone = true;
				}
			}
		}

		if(bMakeBone)
		{
			// Go ahead and make this bone physical.
			int32 NewBodyIndex = CreateNewBody(PhysicsAsset, BoneName);
			UBodySetup* bs = PhysicsAsset->BodySetup[NewBodyIndex];
			check(bs->BoneName == BoneName);

			// Fill in collision info for this bone.
			CreateCollisionFromBone(bs, SkelMesh, i, Params, Infos);

			// If not root - create joint to parent body.
			if(bHitRoot && Params.bCreateJoints)
			{
				int32 NewConstraintIndex = CreateNewConstraint(PhysicsAsset, BoneName);
				UPhysicsConstraintTemplate* CS = PhysicsAsset->ConstraintSetup[NewConstraintIndex];

				// Transform of child from parent is just child ref-pose entry.
				FMatrix RelTM = SkelMesh->GetRefPoseMatrix(i);

				// set angular constraint mode
				CS->DefaultInstance.AngularSwing1Motion = Params.AngularConstraintMode;
				CS->DefaultInstance.AngularSwing2Motion = Params.AngularConstraintMode;
				CS->DefaultInstance.AngularTwistMotion = Params.AngularConstraintMode;

				// Place joint at origin of child
				CS->DefaultInstance.ConstraintBone1 = BoneName;
				CS->DefaultInstance.Pos1 = FVector::ZeroVector;
				CS->DefaultInstance.PriAxis1 = FVector(1, 0, 0);
				CS->DefaultInstance.SecAxis1 = FVector(0, 1, 0);

				CS->DefaultInstance.ConstraintBone2 = ParentName;
				CS->DefaultInstance.Pos2 = RelTM.GetOrigin();
				CS->DefaultInstance.PriAxis2 = RelTM.GetScaledAxis(EAxis::X);
				CS->DefaultInstance.SecAxis2 = RelTM.GetScaledAxis(EAxis::Y);

				// Disable collision between constrained bodies by default.
				PhysicsAsset->DisableCollision(NewBodyIndex, ParentBodyIndex);
			}

			bHitRoot = true;
		}
	}

	return PhysicsAsset->BodySetup.Num() > 0;
}

bool CreateFromSkeletalMesh(UPhysicsAsset* PhysicsAsset, USkeletalMesh* SkelMesh, FPhysAssetCreateParams& Params, FText & OutErrorMessage)
{
	PhysicsAsset->PreviewSkeletalMesh = SkelMesh;

	bool bSuccess = CreateFromSkeletalMeshInternal(PhysicsAsset, SkelMesh, Params);
	if (bSuccess)
	{
		// sets physics asset here now, so whoever creates 
		// new physics asset from skeletalmesh will set properly here
		SkelMesh->PhysicsAsset = PhysicsAsset;
		SkelMesh->MarkPackageDirty();
	}
	else
	{
		// try lower minimum bone size 
		Params.MinBoneSize = 1.f;

		bSuccess = CreateFromSkeletalMeshInternal(PhysicsAsset, SkelMesh, Params);
		if(bSuccess)
		{
			// sets physics asset here now, so whoever creates 
			// new physics asset from skeletalmesh will set properly here
			SkelMesh->PhysicsAsset = PhysicsAsset;
			SkelMesh->MarkPackageDirty();
		}
		else
		{
			OutErrorMessage = FText::Format(NSLOCTEXT("CreatePhysicsAsset", "CreatePhysicsAssetLinkFailed", "The bone size is too small to create Physics Asset '{0}' from Skeletal Mesh '{1}'. You will have to create physics asset manually."), FText::FromString(PhysicsAsset->GetName()), FText::FromString(SkelMesh->GetName()));
		}
	}

	return bSuccess;
}


void CreateCollisionFromBone( UBodySetup* bs, USkeletalMesh* skelMesh, int32 BoneIndex, FPhysAssetCreateParams& Params, const TArray<FBoneVertInfo>& Infos )
{
#if WITH_EDITOR
	// Empty any existing collision.
	bs->RemoveSimpleCollision();
#endif // WITH_EDITOR

	// Calculate orientation of to use for collision primitive.
	FMatrix ElemTM;

	if(Params.bAlignDownBone)
	{
		int32 ChildIndex = GetChildIndex(BoneIndex, skelMesh, Infos);
		if(ChildIndex != INDEX_NONE)
		{
			// Get position of child relative to parent.
			FMatrix RelTM = skelMesh->GetRefPoseMatrix(ChildIndex);
			FVector ChildPos = RelTM.GetOrigin();

			// Check that child is not on top of parent. If it is - we can't make an orientation
			if(ChildPos.Size() > KINDA_SMALL_NUMBER)
			{
				// ZAxis for collision geometry lies down axis to child bone.
				FVector ZAxis = ChildPos.SafeNormal();

				// Then we pick X and Y randomly. 
				// JTODO: Should project all the vertices onto ZAxis plane and fit a bounding box using calipers or something...
				FVector XAxis, YAxis;
				ZAxis.FindBestAxisVectors( YAxis, XAxis );

				ElemTM = FMatrix( XAxis, YAxis, ZAxis, FVector(0) );	
			}
			else
			{
				ElemTM = FMatrix::Identity;
			}
		}
		else
		{
			ElemTM = FMatrix::Identity;
		}
	}
	else
	{
		ElemTM = FMatrix::Identity;
	}

	// Get the (Unreal scale) bounding box for this bone using the rotation.
	const FBoneVertInfo* BoneInfo = &Infos[BoneIndex];
	FBox BoneBox(0);
	for(int32 j=0; j<BoneInfo->Positions.Num(); j++)
	{
		BoneBox += ElemTM.InverseTransformPosition( BoneInfo->Positions[j]  );
	}

	FVector BoxCenter(0,0,0), BoxExtent(0,0,0);

	if( BoneBox.IsValid )
		BoneBox.GetCenterAndExtents(BoxCenter, BoxExtent);

	float MinRad = BoxExtent.GetMin();
	float MinAllowedSize = MinPrimSize;

	// If the primitive is going to be too small - just use some default numbers and let the user tweak.
	if( MinRad < MinAllowedSize )
	{
		BoxExtent = FVector(DefaultPrimSize, DefaultPrimSize, DefaultPrimSize);
	}

	FVector BoneOrigin = ElemTM.TransformPosition( BoxCenter );
	ElemTM.SetOrigin( BoneOrigin );

	if(Params.GeomType == EFG_Box)
	{
		// Add a new box geometry to this body the size of the bounding box.
		int32 ex = bs->AggGeom.BoxElems.AddZeroed();
		FKBoxElem* be = &bs->AggGeom.BoxElems[ex];

		be->SetTransform( FTransform( ElemTM ) );

		be->X = BoxExtent.X * 2.0f * 1.01f; // Side Lengths (add 1% to avoid graphics glitches)
		be->Y = BoxExtent.Y * 2.0f * 1.01f;
		be->Z = BoxExtent.Z * 2.0f * 1.01f;	
	}
	else if (Params.GeomType == EFG_Sphere)
	{
		int32 sx = bs->AggGeom.SphereElems.AddZeroed();
		FKSphereElem* se = &bs->AggGeom.SphereElems[sx];

		se->Center = ElemTM.GetOrigin();
		se->Radius = BoxExtent.GetMax() * 1.01f;
	}
	// Deal with creating a single convex hull
	else if (Params.GeomType == EFG_SingleConvexHull)
	{
		int32 ex = bs->AggGeom.ConvexElems.Add(FKConvexElem());
		FKConvexElem* ce = &bs->AggGeom.ConvexElems[ex];

		// Add all of the vertices for this bone to the convex element
		for( int32 index=0; index<BoneInfo->Positions.Num(); index++ )
		{
			ce->VertexData.Add( BoneInfo->Positions[index] );
		}

		ce->UpdateElemBox();		
	}
	else if (Params.GeomType == EFG_MultiConvexHull)
	{
		// Just feed in all of the verts which are affected by this bone
		int32 ChunkIndex;
		int32 VertIndex;
		bool bSoftVertex;
		bool bHasExtraInfluences;

		// Storage for the hull generation
		TArray<uint32> Indices;
		TArray<FVector> Verts;
		TMap<uint32, uint32> IndexMap;

		// Get the static LOD from the skeletal mesh and loop through the chunks		
		FStaticLODModel& LODModel = skelMesh->GetSourceModel();
		TArray<uint32> indexBufferInOrder;
		LODModel.MultiSizeIndexContainer.GetIndexBuffer( indexBufferInOrder );
		uint32 indexBufferSize = indexBufferInOrder.Num();
		uint32 currentIndex = 0;

		// Add all of the verts and indices to a list I can loop over
		for ( uint32 index = 0; index < indexBufferSize; index++ )
		{
			LODModel.GetChunkAndSkinType(indexBufferInOrder[index], ChunkIndex, VertIndex, bSoftVertex, bHasExtraInfluences);
			const FSkelMeshChunk& Chunk = LODModel.Chunks[ChunkIndex];
			if ( bSoftVertex )
			{
				// We dont want to support soft verts, only rigid
				UE_LOG(LogPhysics, Log, TEXT("Unable to create physics asset with a multi convex hull due to the presence of soft vertices!"));
				return;
			}

			// Using the same code in GetSkinnedVertexPosition
			const FRigidSkinVertex& RigidVert = Chunk.RigidVertices[VertIndex];
			const int LocalBoneIndex = Chunk.BoneMap[RigidVert.Bone];
			const FVector& VertPosition = skelMesh->RefBasesInvMatrix[LocalBoneIndex].TransformPosition(RigidVert.Position);

			if ( LocalBoneIndex == BoneIndex )
			{
				if ( IndexMap.Contains( VertIndex ) )
				{
					Indices.Add( *IndexMap.Find( VertIndex ) );
				}
				else
				{
					Indices.Add( currentIndex );
					IndexMap.Add( VertIndex, currentIndex++ );
					Verts.Add( VertPosition );					
				}
			}
		}

		if ( Params.GeomType == EFG_MultiConvexHull )
		{
			// Create the convex hull from the data we got from the skeletal mesh
			DecomposeMeshToHulls( bs, Verts, Indices, Params.MaxHullCount, Params.MaxHullVerts );
		}
		else
		{
			//Support triangle mesh soon
		}		
	}
	else
	{
		int32 sx = bs->AggGeom.SphylElems.AddZeroed();
		FKSphylElem* se = &bs->AggGeom.SphylElems[sx];

		se->SetTransform( FTransform( ElemTM ) );

		se->Radius = FMath::Max(BoxExtent.X, BoxExtent.Y) * 1.01f;
		se->Length = BoxExtent.Z * 1.01f;
	}
}


void WeldBodies(UPhysicsAsset* PhysAsset, int32 BaseBodyIndex, int32 AddBodyIndex, USkeletalMeshComponent* SkelComp)
{
	if(BaseBodyIndex == INDEX_NONE || AddBodyIndex == INDEX_NONE)
		return;

	if (SkelComp == NULL || SkelComp->SkeletalMesh == NULL)
	{
		return;
	}

	UBodySetup* Body1 = PhysAsset->BodySetup[BaseBodyIndex];
	int32 Bone1Index = SkelComp->SkeletalMesh->RefSkeleton.FindBoneIndex(Body1->BoneName);
	check(Bone1Index != INDEX_NONE);
	FTransform Bone1TM = SkelComp->GetBoneTransform(Bone1Index);
	Bone1TM.RemoveScaling();
	FTransform InvBone1TM = Bone1TM.InverseSafe();

	UBodySetup* Body2 = PhysAsset->BodySetup[AddBodyIndex];
	int32 Bone2Index = SkelComp->SkeletalMesh->RefSkeleton.FindBoneIndex(Body2->BoneName);
	check(Bone2Index != INDEX_NONE);
	FTransform Bone2TM = SkelComp->GetBoneTransform(Bone2Index);
	Bone2TM.RemoveScaling();

	FTransform Bone2ToBone1TM = Bone2TM * InvBone1TM;

	// First copy all collision info over.
	for(int32 i=0; i<Body2->AggGeom.SphereElems.Num(); i++)
	{
		int32 NewPrimIndex = Body1->AggGeom.SphereElems.Add( Body2->AggGeom.SphereElems[i] );
		Body1->AggGeom.SphereElems[NewPrimIndex].Center = Bone2ToBone1TM.TransformPosition( Body2->AggGeom.SphereElems[i].Center ); // Make transform relative to body 1 instead of body 2
	}

	for(int32 i=0; i<Body2->AggGeom.BoxElems.Num(); i++)
	{
		int32 NewPrimIndex = Body1->AggGeom.BoxElems.Add( Body2->AggGeom.BoxElems[i] );
		Body1->AggGeom.BoxElems[NewPrimIndex].SetTransform( Body2->AggGeom.BoxElems[i].GetTransform() * Bone2ToBone1TM );
	}

	for(int32 i=0; i<Body2->AggGeom.SphylElems.Num(); i++)
	{
		int32 NewPrimIndex = Body1->AggGeom.SphylElems.Add( Body2->AggGeom.SphylElems[i] );
		Body1->AggGeom.SphylElems[NewPrimIndex].SetTransform( Body2->AggGeom.SphylElems[i].GetTransform() * Bone2ToBone1TM );
	}

	for(int32 i=0; i<Body2->AggGeom.ConvexElems.Num(); i++)
	{
		// No matrix here- we transform all the vertices into the new ref frame instead.
		int32 NewPrimIndex = Body1->AggGeom.ConvexElems.Add( Body2->AggGeom.ConvexElems[i] );
		FKConvexElem* cElem= &Body1->AggGeom.ConvexElems[NewPrimIndex];

		for(int32 j=0; j<cElem->VertexData.Num(); j++)
		{
			cElem->VertexData[j] = Bone2ToBone1TM.TransformPosition( cElem->VertexData[j] );
		}

		// Update face data.
		cElem->UpdateElemBox();
	}

	// We need to update the collision disable table to shift any pairs that included body2 to include body1 instead.
	// We remove any pairs that include body2 & body1.

	for(int32 i=0; i<PhysAsset->BodySetup.Num(); i++)
	{
		if(i == AddBodyIndex) 
			continue;

		FRigidBodyIndexPair Key(i, AddBodyIndex);

		if( PhysAsset->CollisionDisableTable.Find(Key) )
		{
			PhysAsset->CollisionDisableTable.Remove(Key);

			// Only re-add pair if its not between 'base' and 'add' bodies.
			if(i != BaseBodyIndex)
			{
				FRigidBodyIndexPair NewKey(i, BaseBodyIndex);
				PhysAsset->CollisionDisableTable.Add(NewKey, 0);
			}
		}
	}

	// Make a sensible guess for the other flags
	ECollisionEnabled::Type NewCollisionEnabled = FMath::Min(Body1->DefaultInstance.GetCollisionEnabled(), Body2->DefaultInstance.GetCollisionEnabled());
	Body1->DefaultInstance.SetCollisionEnabled(NewCollisionEnabled);

	// if different
	if (Body1->PhysicsType != Body2->PhysicsType)
	{
		// i don't think this is necessarily good, but I think better than default
		Body1->PhysicsType = FMath::Max(Body1->PhysicsType, Body2->PhysicsType);
	}

	// Then deal with any constraints.

	TArray<int32>	Body2Constraints;
	PhysAsset->BodyFindConstraints(AddBodyIndex, Body2Constraints);

	while( Body2Constraints.Num() > 0 )
	{
		int32 ConstraintIndex = Body2Constraints[0];
		FConstraintInstance& Instance = PhysAsset->ConstraintSetup[ConstraintIndex]->DefaultInstance;

		FName OtherBodyName;
		if( Instance.ConstraintBone1 == Body2->BoneName )
			OtherBodyName = Instance.ConstraintBone2;
		else
			OtherBodyName = Instance.ConstraintBone1;

		// If this is a constraint between the two bodies we are welding, we just destroy it.
		if(OtherBodyName == Body1->BoneName)
		{
			DestroyConstraint(PhysAsset, ConstraintIndex);
		}
		else // Otherwise, we reconnect it to body1 (the 'base' body) instead of body2 (the 'weldee').
		{
			if(Instance.ConstraintBone2 == Body2->BoneName)
			{
				Instance.ConstraintBone2 = Body1->BoneName;

				FTransform ConFrame = Instance.GetRefFrame(EConstraintFrame::Frame2);
				Instance.SetRefFrame(EConstraintFrame::Frame2, ConFrame * FTransform(Bone2ToBone1TM));
			}
			else
			{
				Instance.ConstraintBone1 = Body1->BoneName;

				FTransform ConFrame = Instance.GetRefFrame(EConstraintFrame::Frame1);
				Instance.SetRefFrame(EConstraintFrame::Frame1, ConFrame * FTransform(Bone2ToBone1TM));
			}
		}

		// See if we have any more constraints to body2.
		PhysAsset->BodyFindConstraints(AddBodyIndex, Body2Constraints);
	}

	// Finally remove the body
	DestroyBody(PhysAsset, AddBodyIndex);
}

int32 CreateNewConstraint(UPhysicsAsset* PhysAsset, FName InConstraintName, UPhysicsConstraintTemplate* InConstraintSetup)
{
	// constraintClass must be a subclass of UPhysicsConstraintTemplate
	int32 ConstraintIndex = PhysAsset->FindConstraintIndex(InConstraintName);
	if(ConstraintIndex != INDEX_NONE)
	{
		return ConstraintIndex;
	}

	UPhysicsConstraintTemplate* NewConstraintSetup = ConstructObject<UPhysicsConstraintTemplate>( UPhysicsConstraintTemplate::StaticClass(), PhysAsset, NAME_None, RF_Transactional );
	if(InConstraintSetup)
	{
		NewConstraintSetup->DefaultInstance.CopyConstraintParamsFrom( &InConstraintSetup->DefaultInstance );
	}

	int32 ConstraintSetupIndex = PhysAsset->ConstraintSetup.Add( NewConstraintSetup );
	NewConstraintSetup->DefaultInstance.JointName = InConstraintName;

	return ConstraintSetupIndex;
}

void DestroyConstraint(UPhysicsAsset* PhysAsset, int32 ConstraintIndex)
{
	check(PhysAsset);
	PhysAsset->ConstraintSetup.RemoveAt(ConstraintIndex);
}


int32 CreateNewBody(UPhysicsAsset* PhysAsset, FName InBodyName)
{
	check(PhysAsset);

	int32 BodyIndex = PhysAsset->FindBodyIndex(InBodyName);
	if(BodyIndex != INDEX_NONE)
	{
		return BodyIndex; // if we already have one for this name - just return that.
	}

	UBodySetup* NewBodySetup = ConstructObject<UBodySetup>( UBodySetup::StaticClass(), PhysAsset, NAME_None, RF_Transactional );
	// make default to be use complex as simple 
	NewBodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	// newly created bodies default to simulating
	NewBodySetup->PhysicsType = PhysType_Default;

	int32 BodySetupIndex = PhysAsset->BodySetup.Add( NewBodySetup );
	NewBodySetup->BoneName = InBodyName;

	PhysAsset->UpdateBodySetupIndexMap();
	PhysAsset->UpdateBoundsBodiesArray();

	// Return index of new body.
	return BodySetupIndex;
}

void DestroyBody(UPhysicsAsset* PhysAsset, int32 bodyIndex)
{
	check(PhysAsset);

	// First we must correct the CollisionDisableTable.
	// All elements which refer to bodyIndex are removed.
	// All elements which refer to a body with index >bodyIndex are adjusted. 

	TMap<FRigidBodyIndexPair,bool> NewCDT;
	for(int32 i=1; i<PhysAsset->BodySetup.Num(); i++)
	{
		for(int32 j=0; j<i; j++)
		{
			FRigidBodyIndexPair Key(j,i);

			// If there was an entry for this pair, and it doesn't refer to the removed body, we need to add it to the new CDT.
			if( PhysAsset->CollisionDisableTable.Find(Key) )
			{
				if(i != bodyIndex && j != bodyIndex)
				{
					int32 NewI = (i > bodyIndex) ? i-1 : i;
					int32 NewJ = (j > bodyIndex) ? j-1 : j;

					FRigidBodyIndexPair NewKey(NewJ, NewI);
					NewCDT.Add(NewKey, 0);
				}
			}
		}
	}

	PhysAsset->CollisionDisableTable = NewCDT;

	// Now remove any constraints that were attached to this body.
	// This is a bit yuck and slow...
	TArray<int32> Constraints;
	PhysAsset->BodyFindConstraints(bodyIndex, Constraints);

	while(Constraints.Num() > 0)
	{
		DestroyConstraint( PhysAsset, Constraints[0] );
		PhysAsset->BodyFindConstraints(bodyIndex, Constraints);
	}

	// Remove pointer from array. Actual objects will be garbage collected.
	PhysAsset->BodySetup.RemoveAt(bodyIndex);

	PhysAsset->UpdateBodySetupIndexMap();
	// Update body indices.
	PhysAsset->UpdateBoundsBodiesArray();
}

}; // namespace FPhysicsAssetUtils
