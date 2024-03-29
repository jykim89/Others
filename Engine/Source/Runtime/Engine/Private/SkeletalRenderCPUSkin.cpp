// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalRenderCPUSkin.cpp: CPU skinned skeletal mesh rendering code.

	This code contains embedded portions of source code from dqconv.c Conversion routines between (regular quaternion, translation) and dual quaternion, Version 1.0.0, Copyright ?2006-2007 University of Dublin, Trinity College, All Rights Reserved, which have been altered from their original version.

	The following terms apply to dqconv.c Conversion routines between (regular quaternion, translation) and dual quaternion, Version 1.0.0:

	This software is provided 'as-is', without any express or implied warranty.  In no event will the author(s) be held liable for any damages arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
	claim that you wrote the original software. If you use this software
	in a product, an acknowledgment in the product documentation would be
	appreciated but is not required.
	2. Altered source versions must be plainly marked as such, and must not be
	misrepresented as being the original software.
	3. This notice may not be removed or altered from any source distribution.
=============================================================================*/

#include "EnginePrivate.h"
#include "SkeletalRenderCPUSkin.h"

template<typename BaseVertexType, typename VertexType>
static void SkinVertices( FFinalSkinVertex* DestVertex, FMatrix* ReferenceToLocal, int32 LODIndex, FStaticLODModel& LOD, TArray<FActiveVertexAnim>& ActiveVertexAnims  );

#define INFLUENCE_0		0
#define INFLUENCE_1		1
#define INFLUENCE_2		2
#define INFLUENCE_3		3
#define INFLUENCE_4		4
#define INFLUENCE_5		5
#define INFLUENCE_6		6
#define INFLUENCE_7		7

/** 
 * Modify the vertex buffer to store bone weights in the UV coordinates for rendering 
 * @param DestVertex - already filled out vertex buffer from SkinVertices
 * @param LOD - LOD model corresponding to DestVertex 
 * @param BonesOfInterest - array of bones we want to display
 */
static void CalculateBoneWeights(FFinalSkinVertex* DestVertex, FStaticLODModel& LOD, const TArray<int32>& BonesOfInterest);

/*-----------------------------------------------------------------------------
	FFinalSkinVertexBuffer
-----------------------------------------------------------------------------*/

/** 
 * Initialize the dynamic RHI for this rendering resource 
 */
void FFinalSkinVertexBuffer::InitDynamicRHI()
{
	// all the vertex data for a single LOD of the skel mesh
	FStaticLODModel& LodModel = SkeletalMeshResource->LODModels[LODIdx];

	if (LodModel.DoesVertexBufferHaveExtraBoneInfluences())
	{
		InitVertexData<true>(LodModel);
	}
	else
	{
		InitVertexData<false>(LodModel);
	}
}

template <bool bExtraBoneInfluencesT>
void FFinalSkinVertexBuffer::InitVertexData(FStaticLODModel& LodModel)
{
	// Create the buffer rendering resource
	uint32 Size = LodModel.NumVertices * sizeof(FFinalSkinVertex);

	VertexBufferRHI = RHICreateVertexBuffer(Size,NULL,BUF_Dynamic);

	// Lock the buffer.
	void* Buffer = RHILockVertexBuffer(VertexBufferRHI,0,Size,RLM_WriteOnly);

	// Initialize the vertex data
	// All chunks are combined into one (rigid first, soft next)
	check(LodModel.VertexBufferGPUSkin.GetNumVertices() == LodModel.NumVertices);

	FFinalSkinVertex* DestVertex = (FFinalSkinVertex*)Buffer;
	for( uint32 VertexIdx=0; VertexIdx < LodModel.NumVertices; VertexIdx++ )
	{
		const TGPUSkinVertexBase<bExtraBoneInfluencesT>* SrcVertex = LodModel.VertexBufferGPUSkin.GetVertexPtr<bExtraBoneInfluencesT>(VertexIdx);

		DestVertex->Position = LodModel.VertexBufferGPUSkin.GetVertexPositionFast<bExtraBoneInfluencesT>(VertexIdx);
		DestVertex->TangentX = SrcVertex->TangentX;
		// w component of TangentZ should already have sign of the tangent basis determinant
		DestVertex->TangentZ = SrcVertex->TangentZ;

		FVector2D UVs = LodModel.VertexBufferGPUSkin.GetVertexUVFast<bExtraBoneInfluencesT>(VertexIdx,0);
		DestVertex->U = UVs.X;
		DestVertex->V = UVs.Y;

		DestVertex++;
	}

	// Unlock the buffer.
	RHIUnlockVertexBuffer(VertexBufferRHI);
}

void FFinalSkinVertexBuffer::ReleaseDynamicRHI()
{
	VertexBufferRHI.SafeRelease();
}


/*-----------------------------------------------------------------------------
	FSkeletalMeshObjectCPUSkin
-----------------------------------------------------------------------------*/


FSkeletalMeshObjectCPUSkin::FSkeletalMeshObjectCPUSkin(USkinnedMeshComponent* InMeshComponent, FSkeletalMeshResource* InSkeletalMeshResource) 
:	FSkeletalMeshObject(InMeshComponent,InSkeletalMeshResource)
,	DynamicData(NULL)
,	CachedVertexLOD(INDEX_NONE)
,	bRenderBoneWeight(false)
{
	// create LODs to match the base mesh
	for( int32 LODIndex=0;LODIndex < SkeletalMeshResource->LODModels.Num();LODIndex++ )
	{
		new(LODs) FSkeletalMeshObjectLOD(SkeletalMeshResource,LODIndex);
	}

	InitResources();
}


FSkeletalMeshObjectCPUSkin::~FSkeletalMeshObjectCPUSkin()
{
	delete DynamicData;
}


void FSkeletalMeshObjectCPUSkin::InitResources()
{
	for( int32 LODIndex=0;LODIndex < LODs.Num();LODIndex++ )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs[LODIndex];
		SkelLOD.InitResources();
	}
}


void FSkeletalMeshObjectCPUSkin::ReleaseResources()
{
	for( int32 LODIndex=0;LODIndex < LODs.Num();LODIndex++ )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs[LODIndex];
		SkelLOD.ReleaseResources();
	}
}

void FSkeletalMeshObjectCPUSkin::EnableBlendWeightRendering(bool bEnabled, const TArray<int32>& InBonesOfInterest)
{
	bRenderBoneWeight = bEnabled;

	BonesOfInterest.Empty(InBonesOfInterest.Num());
	BonesOfInterest.Append(InBonesOfInterest);
}

void FSkeletalMeshObjectCPUSkin::Update(int32 LODIndex,USkinnedMeshComponent* InMeshComponent,const TArray<FActiveVertexAnim>& ActiveVertexAnims)
{
	// create the new dynamic data for use by the rendering thread
	// this data is only deleted when another update is sent
	FDynamicSkelMeshObjectData* NewDynamicData = new FDynamicSkelMeshObjectDataCPUSkin(InMeshComponent,SkeletalMeshResource,LODIndex,ActiveVertexAnims);

	// queue a call to update this data
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		SkelMeshObjectUpdateDataCommand,
		FSkeletalMeshObject*, MeshObject, this,
		FDynamicSkelMeshObjectData*, NewDynamicData, NewDynamicData,
	{
		FScopeCycleCounter Context(MeshObject->GetStatId());
		MeshObject->UpdateDynamicData_RenderThread(NewDynamicData);
	}
	);

	if( GIsEditor )
	{
		// this does not need thread-safe update
		ProgressiveDrawingFraction = InMeshComponent->ProgressiveDrawingFraction;
		CustomSortAlternateIndexMode = (ECustomSortAlternateIndexMode)InMeshComponent->CustomSortAlternateIndexMode;
	}
}

void FSkeletalMeshObjectCPUSkin::UpdateDynamicData_RenderThread(FDynamicSkelMeshObjectData* InDynamicData)
{
	// we should be done with the old data at this point
	delete DynamicData;
	// update with new data
	DynamicData = (FDynamicSkelMeshObjectDataCPUSkin*)InDynamicData;	
	check(DynamicData);

	// update vertices using the new data
	CacheVertices(DynamicData->LODIndex,true);
}

static bool ComputeTangent(FVector &t,
							const FVector &p0, const FVector2D &c0,
							const FVector &p1, const FVector2D &c1,
							const FVector &p2, const FVector2D &c2)
{
	const float epsilon = 0.0001f;
	bool   Ret = false;
	FVector dp1 = p1 - p0;
	FVector dp2 = p2 - p0;
	float   du1 = c1.X - c0.X;
	float   dv1 = c1.Y - c0.Y;
	if(FMath::Abs(dv1) < epsilon && FMath::Abs(du1) >= epsilon)
	{
		t = dp1 / du1;
		Ret = true;
	}
	else
	{
		float du2 = c2.X - c0.X;
		float dv2 = c2.Y - c0.Y;
		float det = dv1*du2 - dv2*du1;
		if(FMath::Abs(det) >= epsilon)
		{
			t = (dp2*dv1-dp1*dv2)/det;
			Ret = true;
		}
	}
	return Ret;
}


void FSkeletalMeshObjectCPUSkin::CacheVertices(int32 LODIndex, bool bForce) const
{
	SCOPE_CYCLE_COUNTER( STAT_CPUSkinUpdateRTTime);

	// Source skel mesh and static lod model
	FStaticLODModel& LOD = SkeletalMeshResource->LODModels[LODIndex];

	// Get the destination mesh LOD.
	const FSkeletalMeshObjectLOD& MeshLOD = LODs[LODIndex];

	// only recache if lod changed
	if ( (LODIndex != CachedVertexLOD || bForce) &&
		DynamicData && 
		IsValidRef(MeshLOD.VertexBuffer.VertexBufferRHI) )
	{
		const FSkelMeshObjectLODInfo& MeshLODInfo = LODInfo[LODIndex];

		// bone matrices
		FMatrix* ReferenceToLocal = DynamicData->ReferenceToLocal.GetTypedData();

		int32 CachedFinalVerticesNum = LOD.NumVertices;
		CachedFinalVertices.Empty(CachedFinalVerticesNum);
		CachedFinalVertices.AddUninitialized(CachedFinalVerticesNum);

		// final cached verts
		FFinalSkinVertex* DestVertex = CachedFinalVertices.GetTypedData();

		if (DestVertex)
		{
			check(GIsEditor || LOD.VertexBufferGPUSkin.GetNeedsCPUAccess());
			SCOPE_CYCLE_COUNTER(STAT_SkinningTime);
			if (LOD.VertexBufferGPUSkin.GetUseFullPrecisionUVs())
			{
				// do actual skinning
				if (LOD.DoesVertexBufferHaveExtraBoneInfluences())
				{
					SkinVertices< TGPUSkinVertexBase<true>, TGPUSkinVertexFloat32Uvs<1, true> >( DestVertex, ReferenceToLocal, DynamicData->LODIndex, LOD, DynamicData->ActiveVertexAnims );
				}
				else
				{
					SkinVertices< TGPUSkinVertexBase<false>, TGPUSkinVertexFloat32Uvs<1, false> >( DestVertex, ReferenceToLocal, DynamicData->LODIndex, LOD, DynamicData->ActiveVertexAnims );
				}
			}
			else
			{
				// do actual skinning
				if (LOD.DoesVertexBufferHaveExtraBoneInfluences())
				{
					SkinVertices< TGPUSkinVertexBase<true>, TGPUSkinVertexFloat16Uvs<1, true> >( DestVertex, ReferenceToLocal, DynamicData->LODIndex, LOD, DynamicData->ActiveVertexAnims  );
				}
				else
				{
					SkinVertices< TGPUSkinVertexBase<false>, TGPUSkinVertexFloat16Uvs<1, false> >( DestVertex, ReferenceToLocal, DynamicData->LODIndex, LOD, DynamicData->ActiveVertexAnims  );
				}
			}

			if (bRenderBoneWeight)
			{
				//Transfer bone weights we're interested in to the UV channels
				CalculateBoneWeights(DestVertex, LOD, BonesOfInterest);
			}
		}

		// set lod level currently cached
		CachedVertexLOD = LODIndex;

		check(LOD.NumVertices == CachedFinalVertices.Num());
		MeshLOD.UpdateFinalSkinVertexBuffer( CachedFinalVertices.GetTypedData(), LOD.NumVertices * sizeof(FFinalSkinVertex) );
	}
}

const FVertexFactory* FSkeletalMeshObjectCPUSkin::GetVertexFactory(int32 LODIndex,int32 /*ChunkIdx*/) const
{
	check( LODs.IsValidIndex(LODIndex) );
	return &LODs[LODIndex].VertexFactory;
}

/** 
 * Init rendering resources for this LOD 
 */
void FSkeletalMeshObjectCPUSkin::FSkeletalMeshObjectLOD::InitResources()
{
	// upload vertex buffer
	BeginInitResource(&VertexBuffer);

	// update vertex factory components and sync it
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		InitSkeletalMeshCPUSkinVertexFactory,
		FLocalVertexFactory*,VertexFactory,&VertexFactory,
		FVertexBuffer*,VertexBuffer,&VertexBuffer,
		{
			FLocalVertexFactory::DataType Data;

			// position
			Data.PositionComponent = FVertexStreamComponent(
				VertexBuffer,STRUCT_OFFSET(FFinalSkinVertex,Position),sizeof(FFinalSkinVertex),VET_Float3);
			// tangents
			Data.TangentBasisComponents[0] = FVertexStreamComponent(
				VertexBuffer,STRUCT_OFFSET(FFinalSkinVertex,TangentX),sizeof(FFinalSkinVertex),VET_PackedNormal);
			Data.TangentBasisComponents[1] = FVertexStreamComponent(
				VertexBuffer,STRUCT_OFFSET(FFinalSkinVertex,TangentZ),sizeof(FFinalSkinVertex),VET_PackedNormal);
			// uvs
			Data.TextureCoordinates.Add(FVertexStreamComponent(
				VertexBuffer,STRUCT_OFFSET(FFinalSkinVertex,U),sizeof(FFinalSkinVertex),VET_Float2));

			VertexFactory->SetData(Data);
		});
	BeginInitResource(&VertexFactory);

	bResourcesInitialized = true;
}

/** 
 * Release rendering resources for this LOD 
 */
void FSkeletalMeshObjectCPUSkin::FSkeletalMeshObjectLOD::ReleaseResources()
{	
	BeginReleaseResource(&VertexFactory);
	BeginReleaseResource(&VertexBuffer);

	bResourcesInitialized = false;
}

/** 
 * Update the contents of the vertex buffer with new data
 * @param	NewVertices - array of new vertex data
 * @param	Size - size of new vertex data aray 
 */
void FSkeletalMeshObjectCPUSkin::FSkeletalMeshObjectLOD::UpdateFinalSkinVertexBuffer(void* NewVertices, uint32 Size) const
{
	void* Buffer = RHILockVertexBuffer(VertexBuffer.VertexBufferRHI,0,Size,RLM_WriteOnly);
	FMemory::Memcpy(Buffer,NewVertices,Size);
	RHIUnlockVertexBuffer(VertexBuffer.VertexBufferRHI);
}

TArray<FTransform>* FSkeletalMeshObjectCPUSkin::GetSpaceBases() const
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if(DynamicData)
	{
		return &(DynamicData->MeshSpaceBases);
	}
	else
#endif
	{
		return NULL;
	}
}

/**
 * Get the origin and direction vectors for TRISORT_CustomLeftRight sections
 */
const FTwoVectors& FSkeletalMeshObjectCPUSkin::GetCustomLeftRightVectors(int32 SectionIndex) const
{
	if( DynamicData && DynamicData->CustomLeftRightVectors.IsValidIndex(SectionIndex) )
	{
		return DynamicData->CustomLeftRightVectors[SectionIndex];
	}
	else
	{
		static FTwoVectors Bad( FVector::ZeroVector, FVector(1.f,0.f,0.f) );
		return Bad;
	}
}

void FSkeletalMeshObjectCPUSkin::DrawVertexElements(FPrimitiveDrawInterface* PDI, const FTransform& ToWorldSpace, bool bDrawNormals, bool bDrawTangents, bool bDrawBinormals) const
{
	uint32 NumIndices = CachedFinalVertices.Num();

	FMatrix LocalToWorldInverseTranspose = ToWorldSpace.ToMatrixWithScale().Inverse().GetTransposed();

	for (uint32 i = 0; i < NumIndices; i++)
	{
		FFinalSkinVertex& Vert = CachedFinalVertices[i];

		const FVector WorldPos = ToWorldSpace.TransformPosition( Vert.Position );

		const FVector Normal = Vert.TangentZ;
		const FVector Tangent = Vert.TangentX;
		const FVector Binormal = FVector(Normal) ^ FVector(Tangent);

		const float Len = 1.0f;

		if( bDrawNormals )
		{
			PDI->DrawLine( WorldPos, WorldPos+LocalToWorldInverseTranspose.TransformVector( (FVector)(Normal) ).SafeNormal() * Len, FLinearColor( 0.0f, 1.0f, 0.0f), SDPG_World );
		}

		if( bDrawTangents )
		{
			PDI->DrawLine( WorldPos, WorldPos+LocalToWorldInverseTranspose.TransformVector( Tangent ).SafeNormal() * Len, FLinearColor( 1.0f, 0.0f, 0.0f), SDPG_World );
		}

		if( bDrawBinormals )
		{
			PDI->DrawLine( WorldPos, WorldPos+LocalToWorldInverseTranspose.TransformVector( Binormal ).SafeNormal() * Len, FLinearColor( 0.0f, 0.0f, 1.0f), SDPG_World );
		}
	}
}

/*-----------------------------------------------------------------------------
FDynamicSkelMeshObjectDataCPUSkin
-----------------------------------------------------------------------------*/

FDynamicSkelMeshObjectDataCPUSkin::FDynamicSkelMeshObjectDataCPUSkin(
	USkinnedMeshComponent* InMeshComponent,
	FSkeletalMeshResource* InSkeletalMeshResource,
	int32 InLODIndex,
	const TArray<FActiveVertexAnim>& InActiveVertexAnims
	)
:	LODIndex(InLODIndex)
,	ActiveVertexAnims(InActiveVertexAnims)
{
	UpdateRefToLocalMatrices( ReferenceToLocal, InMeshComponent, InSkeletalMeshResource, LODIndex );

	UpdateCustomLeftRightVectors( CustomLeftRightVectors, InMeshComponent, InSkeletalMeshResource, LODIndex );

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	MeshSpaceBases = InMeshComponent->SpaceBases;
#endif
}

/*-----------------------------------------------------------------------------
	FSkeletalMeshObjectCPUSkin - morph target blending implementation
-----------------------------------------------------------------------------*/

/** Struct used to hold temporary info during vertex animation blending */
struct FVertexAnimEvalInfo
{
	/** Info about anim to blend */
	FActiveVertexAnim			ActiveVertexAnim;
	/** Index of next delta to try applying. This prevents us looking at every delta for every vertex. */
	int32						NextDeltaIndex;
	/** Array of deltas to apply to mesh, sorted based on the index of the base mesh vert that they affect. */
	FVertexAnimDelta*			Deltas;
	/** How many deltas are in array */
	int32						NumDeltas;
	/** Temporary state allocated by the vertex anim object, cleaned up after we are finished with Deltas. */
	FVertexAnimEvalStateBase*	EvalState;
};

/**
 *	Init set of info structs to hold temporary state while blending vertex animations in.
 * @return							number of active morphs that are valid
 */
uint32 InitEvalInfos(const TArray<FActiveVertexAnim>& ActiveVertexAnims, int32 LODIndex, TArray<FVertexAnimEvalInfo>& OutEvalInfos)
{
	uint32 NumValidVertexAnims=0;

	for( int32 AnimIdx=0; AnimIdx < ActiveVertexAnims.Num(); AnimIdx++ )
	{
		FVertexAnimEvalInfo NewInfo;

		const FActiveVertexAnim& ActiveAnim = ActiveVertexAnims[AnimIdx];
		if( ActiveAnim.VertAnim != NULL &&
			ActiveAnim.Weight >= MinVertexAnimBlendWeight && 
			ActiveAnim.Weight <= MaxVertexAnimBlendWeight &&				
			ActiveAnim.VertAnim->HasDataForLOD(LODIndex) )
		{
			// start at the first vertex since they affect base mesh verts in ascending order
			NewInfo.ActiveVertexAnim = ActiveAnim;
			NewInfo.NextDeltaIndex = 0;
			NewInfo.EvalState = ActiveAnim.VertAnim->InitEval();
			NewInfo.Deltas = ActiveAnim.VertAnim->GetDeltasAtTime(0.f, LODIndex, NewInfo.EvalState, NewInfo.NumDeltas);

			NumValidVertexAnims++;
		}
		else
		{
			// invalidate the indices for any invalid morph models
			NewInfo.ActiveVertexAnim = FActiveVertexAnim();
			NewInfo.NextDeltaIndex = INDEX_NONE;
			NewInfo.EvalState = NULL;
			NewInfo.Deltas = NULL;
			NewInfo.NumDeltas = 0;
		}			

		OutEvalInfos.Add(NewInfo);
	}
	return NumValidVertexAnims;
}

/** Release any state for the vertex animations being evaluated */
void TermEvalInfos(TArray<FVertexAnimEvalInfo>& EvalInfos)
{
	for( int32 InfoIdx=0; InfoIdx < EvalInfos.Num(); InfoIdx++ )
	{
		FVertexAnimEvalInfo& Info = EvalInfos[InfoIdx];
		if(Info.ActiveVertexAnim.VertAnim != NULL)
		{
			Info.ActiveVertexAnim.VertAnim->TermEval(Info.EvalState);
		}
	}

	EvalInfos.Empty();
}

/** 
* Derive the tanget/binormal using the new normal and the base tangent vectors for a vertex 
*/
template<typename VertexType>
FORCEINLINE void RebuildTangentBasis( VertexType& DestVertex )
{
	// derive the new tangent by orthonormalizing the new normal against
	// the base tangent vector (assuming these are normalized)
	FVector Tangent( DestVertex.TangentX );
	FVector Normal( DestVertex.TangentZ );
	Tangent = Tangent - ((Tangent | Normal) * Normal);
	Tangent.Normalize();
	DestVertex.TangentX = Tangent;
}

/**
* Applies the vertex deltas to a vertex.
*/
template<typename VertexType>
FORCEINLINE void ApplyMorphBlend( VertexType& DestVertex, const FVertexAnimDelta& SrcMorph, float Weight )
{
	// Add position offset 
	DestVertex.Position += SrcMorph.PositionDelta * Weight;

	// Save W before = operator. That overwrites W to be 127.
	uint8 W = DestVertex.TangentZ.Vector.W;
	// add normal offset. can only apply normal deltas up to a weight of 1
	DestVertex.TangentZ = FVector(FVector(DestVertex.TangentZ) + SrcMorph.TangentZDelta * FMath::Min(Weight,1.0f)).UnsafeNormal();
	// Recover W
	DestVertex.TangentZ.Vector.W = W;
} 

/**
* Blends the source vertex with all the active morph targets.
*/
template<typename VertexType>
FORCEINLINE void UpdateMorphedVertex( VertexType& MorphedVertex, VertexType& SrcVertex, int32 CurBaseVertIdx, int32 LODIndex, TArray<FVertexAnimEvalInfo>& EvalInfos )
{
	MorphedVertex = SrcVertex;

	// iterate over all active morphs
	for( int32 AnimIdx=0; AnimIdx < EvalInfos.Num(); AnimIdx++ )
	{
		FVertexAnimEvalInfo& Info = EvalInfos[AnimIdx];

		// if the next delta to use matches the current vertex, apply it
		if( Info.NextDeltaIndex != INDEX_NONE &&
			Info.NextDeltaIndex < Info.NumDeltas &&
			Info.Deltas[Info.NextDeltaIndex].SourceIdx == CurBaseVertIdx )
		{
			ApplyMorphBlend( MorphedVertex, Info.Deltas[Info.NextDeltaIndex], Info.ActiveVertexAnim.Weight );
			// Update 'next delta to use'
			Info.NextDeltaIndex += 1;
		}
	}

	// rebuild orthonormal tangents
	RebuildTangentBasis( MorphedVertex );
}



/*-----------------------------------------------------------------------------
	FSkeletalMeshObjectCPUSkin - optimized skinning code
-----------------------------------------------------------------------------*/

MSVC_PRAGMA(warning(push))
MSVC_PRAGMA(warning(disable : 4730)) //mixing _m64 and floating point expressions may result in incorrect code


const VectorRegister		VECTOR_PACK_127_5		= DECLARE_VECTOR_REGISTER(127.5f, 127.5f, 127.5f, 0.f);
const VectorRegister		VECTOR4_PACK_127_5		= DECLARE_VECTOR_REGISTER(127.5f, 127.5f, 127.5f, 127.5f);

const VectorRegister		VECTOR_INV_127_5		= DECLARE_VECTOR_REGISTER(1.f / 127.5f, 1.f / 127.5f, 1.f / 127.5f, 0.f);
const VectorRegister		VECTOR4_INV_127_5		= DECLARE_VECTOR_REGISTER(1.f / 127.5f, 1.f / 127.5f, 1.f / 127.5f, 1.f / 127.5f);

const VectorRegister		VECTOR_UNPACK_MINUS_1	= DECLARE_VECTOR_REGISTER(-1.f, -1.f, -1.f, 0.f);
const VectorRegister		VECTOR4_UNPACK_MINUS_1	= DECLARE_VECTOR_REGISTER(-1.f, -1.f, -1.f, -1.f);

const VectorRegister		VECTOR_0001				= DECLARE_VECTOR_REGISTER(0.f, 0.f, 0.f, 1.f);

template<int32 MaxBoneInfluences, typename BaseVertexType, typename VertexType>
static void SkinVertexChunk( FFinalSkinVertex*& DestVertex, TArray<FVertexAnimEvalInfo>& AnimEvalInfos, const FSkelMeshChunk& Chunk, const FStaticLODModel &LOD, int32 VertexBufferBaseIndex, uint32 NumValidMorphs, int32 &CurBaseVertIdx, int32 LODIndex, int32 RigidInfluenceIndex, const FMatrix* RESTRICT ReferenceToLocal )
{
	const bool bExtraBoneInfluences = (MaxBoneInfluences > 4);

	// VertexCopy for morph. Need to allocate right struct
	// To avoid re-allocation, create 2 statics, and assign right struct
	VertexType  VertexCopy;

	// Prefetch all bone indices
	const FBoneIndexType* BoneMap = Chunk.BoneMap.GetTypedData();
	FPlatformMisc::Prefetch( BoneMap );
	FPlatformMisc::Prefetch( BoneMap, CACHE_LINE_SIZE );

	VertexType* SrcRigidVertex = NULL;
	const int32 NumRigidVertices = Chunk.GetNumRigidVertices();
	if (NumRigidVertices > 0)
	{
		INC_DWORD_STAT_BY(STAT_CPUSkinVertices,NumRigidVertices);

		// Prefetch first vertex
		FPlatformMisc::Prefetch( LOD.VertexBufferGPUSkin.GetVertexPtr<bExtraBoneInfluences>(Chunk.GetRigidVertexBufferIndex()) );

		for(int32 VertexIndex = VertexBufferBaseIndex;VertexIndex < NumRigidVertices;VertexIndex++,DestVertex++)
		{
			int32 VertexBufferIndex = Chunk.GetRigidVertexBufferIndex() + VertexIndex;
			SrcRigidVertex = (VertexType*)LOD.VertexBufferGPUSkin.GetVertexPtr<(bExtraBoneInfluences)>(VertexBufferIndex);
			FPlatformMisc::Prefetch( SrcRigidVertex, CACHE_LINE_SIZE );	// Prefetch next vertices
			VertexType* MorphedVertex = SrcRigidVertex;
			if( NumValidMorphs ) 
			{
				MorphedVertex = &VertexCopy;
				UpdateMorphedVertex<VertexType>( *MorphedVertex, *SrcRigidVertex, CurBaseVertIdx, LODIndex, AnimEvalInfos );
			}

			VectorRegister SrcNormals[3];
			VectorRegister DstNormals[3];
			const FVector VertexPosition = LOD.VertexBufferGPUSkin.GetVertexPositionFast((const BaseVertexType*)MorphedVertex);
			SrcNormals[0] = VectorLoadFloat3_W1( &VertexPosition );
			SrcNormals[1] = Unpack3( &MorphedVertex->TangentX.Vector.Packed );
			SrcNormals[2] = Unpack4( &MorphedVertex->TangentZ.Vector.Packed );
			VectorResetFloatRegisters(); // Need to call this to be able to use regular floating point registers again after Unpack().

			uint8 BoneIndex = MorphedVertex->InfluenceBones[RigidInfluenceIndex];

			const FMatrix BoneMatrix = ReferenceToLocal[BoneMap[BoneIndex]];
			VectorRegister M00	= VectorLoadAligned( &BoneMatrix.M[0][0] );
			VectorRegister M10	= VectorLoadAligned( &BoneMatrix.M[1][0] );
			VectorRegister M20	= VectorLoadAligned( &BoneMatrix.M[2][0] );
			VectorRegister M30	= VectorLoadAligned( &BoneMatrix.M[3][0] );

			VectorRegister N_xxxx = VectorReplicate( SrcNormals[0], 0 );
			VectorRegister N_yyyy = VectorReplicate( SrcNormals[0], 1 );
			VectorRegister N_zzzz = VectorReplicate( SrcNormals[0], 2 );
			DstNormals[0] = VectorMultiplyAdd( N_xxxx, M00, VectorMultiplyAdd( N_yyyy, M10, VectorMultiplyAdd( N_zzzz, M20, M30 ) ) );

			N_xxxx = VectorReplicate( SrcNormals[1], 0 );
			N_yyyy = VectorReplicate( SrcNormals[1], 1 );
			N_zzzz = VectorReplicate( SrcNormals[1], 2 );
			DstNormals[1] = VectorMultiplyAdd( N_xxxx, M00, VectorMultiplyAdd( N_yyyy, M10, VectorMultiply( N_zzzz, M20 ) ) );

			N_xxxx = VectorReplicate( SrcNormals[2], 0 );
			N_yyyy = VectorReplicate( SrcNormals[2], 1 );
			N_zzzz = VectorReplicate( SrcNormals[2], 2 );
			DstNormals[2] = VectorMultiplyAdd( N_xxxx, M00, VectorMultiplyAdd( N_yyyy, M10, VectorMultiply( N_zzzz, M20 ) ) );

			// carry over the W component (sign of basis determinant) 
			DstNormals[2] = VectorMultiplyAdd( VECTOR_0001, SrcNormals[2], DstNormals[2] );

			// Write to 16-byte aligned memory:
			VectorStore( DstNormals[0], &DestVertex->Position );
			Pack3( DstNormals[1], &DestVertex->TangentX.Vector.Packed );
			Pack4( DstNormals[2], &DestVertex->TangentZ.Vector.Packed );
			VectorResetFloatRegisters(); // Need to call this to be able to use regular floating point registers again after Pack().

			// Copy UVs.
			FVector2D UVs = LOD.VertexBufferGPUSkin.GetVertexUVFast<bExtraBoneInfluences>(VertexBufferIndex,0);
			DestVertex->U = UVs.X;
			DestVertex->V = UVs.Y;

			CurBaseVertIdx++;
		}
	}

	VertexType* SrcSoftVertex = NULL;
	const int32 NumSoftVertices = Chunk.GetNumSoftVertices();
	if (NumSoftVertices > 0)
	{
		INC_DWORD_STAT_BY(STAT_CPUSkinVertices,NumSoftVertices);

		// Prefetch first vertex
		FPlatformMisc::Prefetch( LOD.VertexBufferGPUSkin.GetVertexPtr<(bExtraBoneInfluences)>(Chunk.GetSoftVertexBufferIndex()) );

		for(int32 VertexIndex = VertexBufferBaseIndex;VertexIndex < NumSoftVertices;VertexIndex++,DestVertex++)
		{
			const int32 VertexBufferIndex = Chunk.GetSoftVertexBufferIndex() + VertexIndex;
			SrcSoftVertex = (VertexType*)LOD.VertexBufferGPUSkin.GetVertexPtr<(bExtraBoneInfluences)>(VertexBufferIndex);
			FPlatformMisc::Prefetch( SrcSoftVertex, CACHE_LINE_SIZE );	// Prefetch next vertices
			VertexType* MorphedVertex = SrcSoftVertex;
			if( NumValidMorphs ) 
			{
				MorphedVertex = &VertexCopy;
				UpdateMorphedVertex<VertexType>( *MorphedVertex, *SrcSoftVertex, CurBaseVertIdx, LODIndex, AnimEvalInfos );
			}

			const uint8* RESTRICT BoneIndices = MorphedVertex->InfluenceBones;
			const uint8* RESTRICT BoneWeights = MorphedVertex->InfluenceWeights;

			static VectorRegister	SrcNormals[3];
			VectorRegister			DstNormals[3];
			const FVector VertexPosition = LOD.VertexBufferGPUSkin.GetVertexPositionFast((const BaseVertexType*)MorphedVertex);
			SrcNormals[0] = VectorLoadFloat3_W1( &VertexPosition );
			SrcNormals[1] = Unpack3( &MorphedVertex->TangentX.Vector.Packed );
			SrcNormals[2] = Unpack4( &MorphedVertex->TangentZ.Vector.Packed );
			VectorRegister Weights = VectorMultiply( VectorLoadByte4(BoneWeights), VECTOR_INV_255 );
			VectorRegister ExtraWeights;
			if (bExtraBoneInfluences)
			{
				ExtraWeights = VectorMultiply( VectorLoadByte4(&BoneWeights[MAX_INFLUENCES_PER_STREAM]), VECTOR_INV_255 );
			}
			VectorResetFloatRegisters(); // Need to call this to be able to use regular floating point registers again after Unpack and VectorLoadByte4.

			const FMatrix BoneMatrix0 = ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_0]]];
			VectorRegister Weight0 = VectorReplicate( Weights, INFLUENCE_0 );
			VectorRegister M00	= VectorMultiply( VectorLoadAligned( &BoneMatrix0.M[0][0] ), Weight0 );
			VectorRegister M10	= VectorMultiply( VectorLoadAligned( &BoneMatrix0.M[1][0] ), Weight0 );
			VectorRegister M20	= VectorMultiply( VectorLoadAligned( &BoneMatrix0.M[2][0] ), Weight0 );
			VectorRegister M30	= VectorMultiply( VectorLoadAligned( &BoneMatrix0.M[3][0] ), Weight0 );

			if ( MaxBoneInfluences > 1 )
			{
				const FMatrix BoneMatrix1 = ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_1]]];
				VectorRegister Weight1 = VectorReplicate( Weights, INFLUENCE_1 );
				M00	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix1.M[0][0] ), Weight1, M00 );
				M10	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix1.M[1][0] ), Weight1, M10 );
				M20	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix1.M[2][0] ), Weight1, M20 );
				M30	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix1.M[3][0] ), Weight1, M30 );

				if ( MaxBoneInfluences > 2 )
				{
					const FMatrix BoneMatrix2 = ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_2]]];
					VectorRegister Weight2 = VectorReplicate( Weights, INFLUENCE_2 );
					M00	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix2.M[0][0] ), Weight2, M00 );
					M10	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix2.M[1][0] ), Weight2, M10 );
					M20	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix2.M[2][0] ), Weight2, M20 );
					M30	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix2.M[3][0] ), Weight2, M30 );

					if ( MaxBoneInfluences > 3 )
					{
						const FMatrix BoneMatrix3 = ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_3]]];
						VectorRegister Weight3 = VectorReplicate( Weights, INFLUENCE_3 );
						M00	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix3.M[0][0] ), Weight3, M00 );
						M10	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix3.M[1][0] ), Weight3, M10 );
						M20	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix3.M[2][0] ), Weight3, M20 );
						M30	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix3.M[3][0] ), Weight3, M30 );
					}

					if (MaxBoneInfluences > 4)
					{
						const FMatrix BoneMatrix4 = ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_4]]];
						VectorRegister Weight4 = VectorReplicate( ExtraWeights, INFLUENCE_4 - INFLUENCE_4 );
						M00	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix4.M[0][0] ), Weight4, M00 );
						M10	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix4.M[1][0] ), Weight4, M10 );
						M20	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix4.M[2][0] ), Weight4, M20 );
						M30	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix4.M[3][0] ), Weight4, M30 );

						if (MaxBoneInfluences > 5)
						{
							const FMatrix BoneMatrix5 = ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_5]]];
							VectorRegister Weight5 = VectorReplicate( ExtraWeights, INFLUENCE_5 - INFLUENCE_4 );
							M00	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix5.M[0][0] ), Weight5, M00 );
							M10	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix5.M[1][0] ), Weight5, M10 );
							M20	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix5.M[2][0] ), Weight5, M20 );
							M30	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix5.M[3][0] ), Weight5, M30 );

							if (MaxBoneInfluences > 6)
							{
								const FMatrix BoneMatrix6 = ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_6]]];
								VectorRegister Weight6 = VectorReplicate( ExtraWeights, INFLUENCE_6 - INFLUENCE_4 );
								M00	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix6.M[0][0] ), Weight6, M00 );
								M10	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix6.M[1][0] ), Weight6, M10 );
								M20	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix6.M[2][0] ), Weight6, M20 );
								M30	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix6.M[3][0] ), Weight6, M30 );

								if (MaxBoneInfluences > 7)
								{
									const FMatrix BoneMatrix7 = ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_7]]];
									VectorRegister Weight7 = VectorReplicate( ExtraWeights, INFLUENCE_7 - INFLUENCE_4 );
									M00	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix7.M[0][0] ), Weight7, M00 );
									M10	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix7.M[1][0] ), Weight7, M10 );
									M20	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix7.M[2][0] ), Weight7, M20 );
									M30	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix7.M[3][0] ), Weight7, M30 );
								}
							}
						}
					}
				}
			}

			VectorRegister N_xxxx = VectorReplicate( SrcNormals[0], 0 );
			VectorRegister N_yyyy = VectorReplicate( SrcNormals[0], 1 );
			VectorRegister N_zzzz = VectorReplicate( SrcNormals[0], 2 );
			DstNormals[0] = VectorMultiplyAdd( N_xxxx, M00, VectorMultiplyAdd( N_yyyy, M10, VectorMultiplyAdd( N_zzzz, M20, M30 ) ) );

			DstNormals[1] = VectorZero();
			N_xxxx = VectorReplicate( SrcNormals[1], 0 );
			N_yyyy = VectorReplicate( SrcNormals[1], 1 );
			N_zzzz = VectorReplicate( SrcNormals[1], 2 );
			DstNormals[1] = VectorNormalize(VectorMultiplyAdd( N_xxxx, M00, VectorMultiplyAdd( N_yyyy, M10, VectorMultiply( N_zzzz, M20 ) ) ));

			N_xxxx = VectorReplicate( SrcNormals[2], 0 );
			N_yyyy = VectorReplicate( SrcNormals[2], 1 );
			N_zzzz = VectorReplicate( SrcNormals[2], 2 );
			DstNormals[2] = VectorZero();
			DstNormals[2] = VectorNormalize(VectorMultiplyAdd( N_xxxx, M00, VectorMultiplyAdd( N_yyyy, M10, VectorMultiply( N_zzzz, M20 ) ) ));


			// carry over the W component (sign of basis determinant) 
			DstNormals[2] = VectorMultiplyAdd( VECTOR_0001, SrcNormals[2], DstNormals[2] );

			// Write to 16-byte aligned memory:
			VectorStore( DstNormals[0], &DestVertex->Position );
			Pack3( DstNormals[1], &DestVertex->TangentX.Vector.Packed );
			Pack4( DstNormals[2], &DestVertex->TangentZ.Vector.Packed );
			VectorResetFloatRegisters(); // Need to call this to be able to use regular floating point registers again after Pack().

			// Copy UVs.
			FVector2D UVs = LOD.VertexBufferGPUSkin.GetVertexUVFast<bExtraBoneInfluences>(Chunk.GetSoftVertexBufferIndex()+VertexIndex,0);
			DestVertex->U = UVs.X;
			DestVertex->V = UVs.Y;

			CurBaseVertIdx++;
		}
	}
}

template<typename BaseVertexType, typename VertexType>
static void SkinVertices( FFinalSkinVertex* DestVertex, FMatrix* ReferenceToLocal, int32 LODIndex, FStaticLODModel& LOD, TArray<FActiveVertexAnim>& ActiveVertexAnims )
{
	uint32 StatusRegister = VectorGetControlRegister();
	VectorSetControlRegister( StatusRegister | VECTOR_ROUND_TOWARD_ZERO );

	// Create array to track state during vertex anim blending
	TArray<FVertexAnimEvalInfo> AnimEvalInfos;
	uint32 NumValidMorphs = InitEvalInfos(ActiveVertexAnims, LODIndex, AnimEvalInfos);

	static const auto MaxBonesVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("Compat.MAX_GPUSKIN_BONES"));
	const int32 MaxGPUSkinBones = MaxBonesVar->GetValueOnAnyThread();

	// Prefetch all matrices
	for ( int32 MatrixIndex=0; MatrixIndex < MaxGPUSkinBones; MatrixIndex+=2 )
	{
		FPlatformMisc::Prefetch( ReferenceToLocal + MatrixIndex );
	}

	int32 CurBaseVertIdx = 0;

	const int32 RigidInfluenceIndex = SkinningTools::GetRigidInfluenceIndex();
	int32 VertexBufferBaseIndex=0;

	for(int32 SectionIndex= 0;SectionIndex< LOD.Sections.Num();SectionIndex++)
	{
		FSkelMeshSection& Section = LOD.Sections[SectionIndex];
		FSkelMeshChunk& Chunk = LOD.Chunks[Section.ChunkIndex];

		switch (Chunk.MaxBoneInfluences)
		{
		case 1: SkinVertexChunk<1, BaseVertexType, VertexType>(DestVertex, AnimEvalInfos, Chunk, LOD, VertexBufferBaseIndex, NumValidMorphs, CurBaseVertIdx, LODIndex, RigidInfluenceIndex, ReferenceToLocal); break;
		case 2: SkinVertexChunk<2, BaseVertexType, VertexType>(DestVertex, AnimEvalInfos, Chunk, LOD, VertexBufferBaseIndex, NumValidMorphs, CurBaseVertIdx, LODIndex, RigidInfluenceIndex, ReferenceToLocal); break;
		case 3: SkinVertexChunk<3, BaseVertexType, VertexType>(DestVertex, AnimEvalInfos, Chunk, LOD, VertexBufferBaseIndex, NumValidMorphs, CurBaseVertIdx, LODIndex, RigidInfluenceIndex, ReferenceToLocal); break;
		case 4: SkinVertexChunk<4, BaseVertexType, VertexType>(DestVertex, AnimEvalInfos, Chunk, LOD, VertexBufferBaseIndex, NumValidMorphs, CurBaseVertIdx, LODIndex, RigidInfluenceIndex, ReferenceToLocal); break;
		case 5: SkinVertexChunk<5, BaseVertexType, VertexType>(DestVertex, AnimEvalInfos, Chunk, LOD, VertexBufferBaseIndex, NumValidMorphs, CurBaseVertIdx, LODIndex, RigidInfluenceIndex, ReferenceToLocal); break;
		case 6: SkinVertexChunk<6, BaseVertexType, VertexType>(DestVertex, AnimEvalInfos, Chunk, LOD, VertexBufferBaseIndex, NumValidMorphs, CurBaseVertIdx, LODIndex, RigidInfluenceIndex, ReferenceToLocal); break;
		case 7: SkinVertexChunk<7, BaseVertexType, VertexType>(DestVertex, AnimEvalInfos, Chunk, LOD, VertexBufferBaseIndex, NumValidMorphs, CurBaseVertIdx, LODIndex, RigidInfluenceIndex, ReferenceToLocal); break;
		case 8: SkinVertexChunk<8, BaseVertexType, VertexType>(DestVertex, AnimEvalInfos, Chunk, LOD, VertexBufferBaseIndex, NumValidMorphs, CurBaseVertIdx, LODIndex, RigidInfluenceIndex, ReferenceToLocal); break;
		default: check(0);
		}
	}

	VectorSetControlRegister( StatusRegister );
}

/**
 * Convert FPackedNormal to 0-1 FVector4
 */
FVector4 GetTangetToColor(FPackedNormal Tangent)
{
	VectorRegister VectorToUnpack = Tangent.GetVectorRegister();
	// Write to FVector and return it.
	FVector4 UnpackedVector;
	VectorStoreAligned( VectorToUnpack, &UnpackedVector );

	FVector4 Src = UnpackedVector;
	Src = Src + FVector4(1.f, 1.f, 1.f, 1.f);
	Src = Src/2.f;
	return Src;
}

/** 
 * Modify the vertex buffer to store bone weights in the UV coordinates for rendering 
 * @param DestVertex - already filled out vertex buffer from SkinVertices
 * @param LOD - LOD model corresponding to DestVertex 
 * @param BonesOfInterest - array of bones we want to display
 */
template <bool bExtraBoneInfluencesT>
static FORCEINLINE void CalculateChunkBoneWeights(FFinalSkinVertex*& DestVertex, FSkeletalMeshVertexBuffer& VertexBufferGPUSkin, FSkelMeshChunk& Chunk, const TArray<int32>& BonesOfInterest)
{
	const float INV255 = 1.f/255.f;

	const int32 RigidInfluenceIndex = SkinningTools::GetRigidInfluenceIndex();

	int32 VertexBufferBaseIndex = 0;

	//array of bone mapping
	FBoneIndexType* BoneMap = Chunk.BoneMap.GetTypedData();

	for(int32 VertexIndex = VertexBufferBaseIndex;VertexIndex < Chunk.GetNumRigidVertices();VertexIndex++,DestVertex++)
	{
		int32 VertexBufferIndex = Chunk.GetRigidVertexBufferIndex() + VertexIndex;
		auto* SrcRigidVertex = VertexBufferGPUSkin.GetVertexPtr<bExtraBoneInfluencesT>(VertexBufferIndex);

		uint8 BoneIndex = SrcRigidVertex->InfluenceBones[RigidInfluenceIndex];

		if (BonesOfInterest.Contains(BoneMap[BoneIndex]))
		{
			DestVertex->U = 1.f; 
			DestVertex->V = 1.f; 
		}
		else
		{
			DestVertex->U = 0.0f;
			DestVertex->V = 0.0f;
		}
	}

	for(int32 VertexIndex = VertexBufferBaseIndex;VertexIndex < Chunk.GetNumSoftVertices();VertexIndex++,DestVertex++)
	{
		const int32 VertexBufferIndex = Chunk.GetSoftVertexBufferIndex() + VertexIndex;
		auto* SrcSoftVertex = VertexBufferGPUSkin.GetVertexPtr<bExtraBoneInfluencesT>(VertexBufferIndex);

		//Zero out the UV coords
		DestVertex->U = 0.0f;
		DestVertex->V = 0.0f;

		const uint8* RESTRICT BoneIndices = SrcSoftVertex->InfluenceBones;
		const uint8* RESTRICT BoneWeights = SrcSoftVertex->InfluenceWeights;

		for (int32 i = 0; i < TGPUSkinVertexBase<bExtraBoneInfluencesT>::NumInfluences; i++)
		{
			if (BonesOfInterest.Contains(BoneMap[BoneIndices[i]]))
			{
				DestVertex->U += BoneWeights[i] * INV255; 
				DestVertex->V += BoneWeights[i] * INV255;
			}
		}
	}
}

/** 
 * Modify the vertex buffer to store bone weights in the UV coordinates for rendering 
 * @param DestVertex - already filled out vertex buffer from SkinVertices
 * @param LOD - LOD model corresponding to DestVertex 
 * @param BonesOfInterest - array of bones we want to display
 */
static void CalculateBoneWeights(FFinalSkinVertex* DestVertex, FStaticLODModel& LOD, const TArray<int32>& BonesOfInterest)
{
	const float INV255 = 1.f/255.f;

	const int32 RigidInfluenceIndex = SkinningTools::GetRigidInfluenceIndex();

	int32 VertexBufferBaseIndex = 0;

	for(int32 SectionIndex= 0;SectionIndex< LOD.Sections.Num();SectionIndex++)
	{
		FSkelMeshSection& Section = LOD.Sections[SectionIndex];
		FSkelMeshChunk& Chunk = LOD.Chunks[Section.ChunkIndex];

		if (Chunk.HasExtraBoneInfluences())
		{
			CalculateChunkBoneWeights<true>(DestVertex, LOD.VertexBufferGPUSkin, Chunk, BonesOfInterest);
		}
		else
		{
			CalculateChunkBoneWeights<false>(DestVertex, LOD.VertexBufferGPUSkin, Chunk, BonesOfInterest);
		}
	}
}

MSVC_PRAGMA(warning(pop))
