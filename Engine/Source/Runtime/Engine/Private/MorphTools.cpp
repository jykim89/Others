// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MorphTools.cpp: Morph target creation helper classes.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "MeshBuild.h"
#include "PhysicsEngine/PhysXSupport.h"

/** compare based on base mesh source vertex indices */
struct FCompareVertexAnimDeltas
{
	FORCEINLINE bool operator()( const FVertexAnimDelta& A, const FVertexAnimDelta& B ) const
	{
		return ((int32)A.SourceIdx - (int32)B.SourceIdx) < 0 ? true : false;
	}
};

FVertexAnimDelta* UMorphTarget::GetDeltasAtTime(float Time, int32 LODIndex, FVertexAnimEvalStateBase* State, int32& OutNumDeltas)
{
	if(LODIndex < MorphLODModels.Num())
	{
		FMorphTargetLODModel& MorphModel = MorphLODModels[LODIndex];
		OutNumDeltas = MorphModel.Vertices.Num();
		return MorphModel.Vertices.GetData();
	}

	return NULL;
}

bool UMorphTarget::HasDataForLOD(int32 LODIndex) 
{
	// If we have an entry for this LOD, and it has verts
	return (MorphLODModels.IsValidIndex(LODIndex) && MorphLODModels[LODIndex].Vertices.Num() > 0);
}


void UMorphTarget::PostProcess( USkeletalMesh * NewMesh, const FMorphMeshRawSource& BaseSource, const FMorphMeshRawSource& TargetSource, int32 LODIndex )
{
	// @todo anim: update BaseSkelMesh with my information
	NewMesh->RegisterMorphTarget(this);

	CreateMorphMeshStreams( BaseSource, TargetSource, LODIndex );

	MarkPackageDirty();
}


void UMorphTarget::CreateMorphMeshStreams( const FMorphMeshRawSource& BaseSource, const FMorphMeshRawSource& TargetSource, int32 LODIndex )
{
	check(BaseSource.IsValidTarget(TargetSource));

	const float CLOSE_TO_ZERO_DELTA = THRESH_POINTS_ARE_SAME * 4.f;

	// create the LOD entry if it doesn't already exist
	if( LODIndex == MorphLODModels.Num() )
	{
		new(MorphLODModels) FMorphTargetLODModel();
	}

	// morph mesh data to modify
	FMorphTargetLODModel& MorphModel = MorphLODModels[LODIndex];
	// copy the wedge point indices
	// for now just keep every thing 

	// set the original number of vertices
	MorphModel.NumBaseMeshVerts = BaseSource.Vertices.Num();

	// empty morph mesh vertices first
	MorphModel.Vertices.Empty();

	// array to mark processed base vertices
	TArray<bool> WasProcessed;
	WasProcessed.Empty(BaseSource.Vertices.Num());
	WasProcessed.AddZeroed(BaseSource.Vertices.Num());


	TMap<uint32,uint32> WedgePointToVertexIndexMap;
	// Build a mapping of wedge point indices to vertex indices for fast lookup later.
	for( int32 Idx=0; Idx < TargetSource.WedgePointIndices.Num(); Idx++ )
	{
		WedgePointToVertexIndexMap.Add( TargetSource.WedgePointIndices[Idx], Idx );
	}

	// iterate over all the base mesh indices
	for( int32 Idx=0; Idx < BaseSource.Indices.Num(); Idx++ )
	{
		uint32 BaseVertIdx = BaseSource.Indices[Idx];

		// check for duplicate processing
		if( !WasProcessed[BaseVertIdx] )
		{
			// mark this base vertex as already processed
			WasProcessed[BaseVertIdx] = true;

			// get base mesh vertex using its index buffer
			const FMorphMeshVertexRaw& VBase = BaseSource.Vertices[BaseVertIdx];
			
			// clothing can add extra verts, and we won't have source point, so we ignore those
			if (BaseSource.WedgePointIndices.IsValidIndex(BaseVertIdx))
			{
				// get the base mesh's original wedge point index
				uint32 BasePointIdx = BaseSource.WedgePointIndices[BaseVertIdx];

				// find the matching target vertex by searching for one
				// that has the same wedge point index
				uint32* TargetVertIdx = WedgePointToVertexIndexMap.Find( BasePointIdx );

				// only add the vertex if the source point was found
				if( TargetVertIdx != NULL )
				{
					// get target mesh vertex using its index buffer
					const FMorphMeshVertexRaw& VTarget = TargetSource.Vertices[*TargetVertIdx];

					// change in position from base to target
					FVector PositionDelta( VTarget.Position - VBase.Position );
					FVector NormalDeltaZ (VTarget.TanZ - VBase.TanZ);

					// check if position actually changed much
					if( PositionDelta.Size() > CLOSE_TO_ZERO_DELTA  || NormalDeltaZ.Size() > 0.1f)
					{
						// create a new entry
						FVertexAnimDelta NewVertex;
						// position delta
						NewVertex.PositionDelta = PositionDelta;
						// normal delta
						NewVertex.TangentZDelta = NormalDeltaZ;
						// index of base mesh vert this entry is to modify
						NewVertex.SourceIdx = BaseVertIdx;

						// add it to the list of changed verts
						MorphModel.Vertices.Add( NewVertex );				
					}
				}	
			}
		}
	}

	// sort the array of vertices for this morph target based on the base mesh indices 
	// that each vertex is associated with. This allows us to sequentially traverse the list
	// when applying the morph blends to each vertex.
	MorphModel.Vertices.Sort(FCompareVertexAnimDeltas());

	// remove array slack
	MorphModel.Vertices.Shrink();
}

void UMorphTarget::RemapVertexIndices( USkeletalMesh* InBaseMesh, const TArray< TArray<uint32> > & BasedWedgePointIndices )
{
	// make sure base wedge point indices have more than what this morph target has
	// any morph target import needs base mesh (correct LOD index if it belongs to LOD)
	check ( BasedWedgePointIndices.Num() >= MorphLODModels.Num() );
	check ( InBaseMesh );

	// for each LOD
	FSkeletalMeshResource* ImportedResource = InBaseMesh->GetImportedResource();
	for ( int32 LODIndex=0; LODIndex<MorphLODModels.Num(); ++LODIndex )
	{
		FStaticLODModel & BaseLODModel = ImportedResource->LODModels[LODIndex];
		FMorphTargetLODModel& MorphLODModel = MorphLODModels[LODIndex];
		const TArray<uint32> & LODWedgePointIndices = BasedWedgePointIndices[LODIndex];
		TArray<uint32> NewWedgePointIndices;

		// If the LOD has been simplified, don't remap vertex indices else the data will be useless if the mesh is unsimplified.
		check( LODIndex < InBaseMesh->LODInfo.Num() );
		if ( InBaseMesh->LODInfo[ LODIndex ].bHasBeenSimplified  )
		{
			continue;
		}

		// copy the wedge point indices - it makes easy to find
		if( BaseLODModel.RawPointIndices.GetBulkDataSize() )
		{
			NewWedgePointIndices.Empty( BaseLODModel.RawPointIndices.GetElementCount() );
			NewWedgePointIndices.AddUninitialized( BaseLODModel.RawPointIndices.GetElementCount() );
			FMemory::Memcpy( NewWedgePointIndices.GetData(), BaseLODModel.RawPointIndices.Lock(LOCK_READ_ONLY), BaseLODModel.RawPointIndices.GetBulkDataSize() );
			BaseLODModel.RawPointIndices.Unlock();
		
			// Source Indices used : Save it so that you don't use it twice
			TArray<uint32> SourceIndicesUsed;
			SourceIndicesUsed.Empty(MorphLODModel.Vertices.Num());

			// go through all vertices
			for ( int32 VertIdx=0; VertIdx<MorphLODModel.Vertices.Num(); ++VertIdx )
			{	
				// Get Old Base Vertex ID
				uint32 OldVertIdx = MorphLODModel.Vertices[VertIdx].SourceIdx;
				// find PointIndices from the old list
				uint32 BasePointIndex = LODWedgePointIndices[OldVertIdx];

				// Find the PointIndices from new list
				int32 NewVertIdx = NewWedgePointIndices.Find(BasePointIndex);
				// See if it's already used
				if ( SourceIndicesUsed.Find(NewVertIdx) != INDEX_NONE )
				{
					// if used look for next right vertex index
					for ( int32 Iter = NewVertIdx + 1; Iter<NewWedgePointIndices.Num(); ++Iter )
					{
						// found one
						if (NewWedgePointIndices[Iter] == BasePointIndex)
						{
							// see if used again
							if (SourceIndicesUsed.Find(Iter) == INDEX_NONE)
							{
								// if not, this slot is available 
								// update new value
								MorphLODModel.Vertices[VertIdx].SourceIdx = Iter;
								SourceIndicesUsed.Add(Iter);									
								break;
							}
						}
					}
				}
				else
				{
					// update new value
					MorphLODModel.Vertices[VertIdx].SourceIdx = NewVertIdx;
					SourceIndicesUsed.Add(NewVertIdx);
				}
			}

			MorphLODModel.Vertices.Sort(FCompareVertexAnimDeltas());
		}
	}
}
/**
* Constructor. 
* Converts a skeletal mesh to raw vertex data
* needed for creating a morph target mesh
*
* @param	SrcMesh - source skeletal mesh to convert
* @param	LODIndex - level of detail to use for the geometry
*/
FMorphMeshRawSource::FMorphMeshRawSource( USkeletalMesh* SrcMesh, int32 LODIndex ) :
	SourceMesh(SrcMesh)
{
	check(SrcMesh);
	FSkeletalMeshResource* ImportedResource = SrcMesh->GetImportedResource();
	check(ImportedResource->LODModels.IsValidIndex(LODIndex));

	// get the mesh data for the given lod
	FStaticLODModel& LODModel = ImportedResource->LODModels[LODIndex];

	// vertices are packed in this order iot stay consistent
	// with the indexing used by the FStaticLODModel vertex buffer
	//
	//	Chunk0
	//		Rigid0
	//		Rigid1
	//		Soft0
	//		Soft1
	//	Chunk1
	//		Rigid0
	//		Rigid1
	//		Soft0
	//		Soft1

	// iterate over the chunks for the skeletal mesh
	for( int32 ChunkIdx=0; ChunkIdx < LODModel.Chunks.Num(); ChunkIdx++ )
	{
		// each chunk has both rigid and smooth vertices
		const FSkelMeshChunk& Chunk = LODModel.Chunks[ChunkIdx];
		// rigid vertices should always be added first so that we are
		// consistent with the way vertices are packed in the FStaticLODModel vertex buffer
		for( int32 VertexIdx=0; VertexIdx < Chunk.RigidVertices.Num(); VertexIdx++ )
		{
			const FRigidSkinVertex& SourceVertex = Chunk.RigidVertices[VertexIdx];
			FMorphMeshVertexRaw RawVertex = 
			{
				SourceVertex.Position,
				SourceVertex.TangentX,
				SourceVertex.TangentY,
				SourceVertex.TangentZ
			};
			Vertices.Add( RawVertex );			
		}
		// smooth vertices are added next. The resulting Vertices[] array should
		// match the FStaticLODModel vertex buffer when indexing vertices
		for( int32 VertexIdx=0; VertexIdx < Chunk.SoftVertices.Num(); VertexIdx++ )
		{
			const FSoftSkinVertex& SourceVertex = Chunk.SoftVertices[VertexIdx];
			FMorphMeshVertexRaw RawVertex = 
			{
				SourceVertex.Position,
				SourceVertex.TangentX,
				SourceVertex.TangentY,
				SourceVertex.TangentZ
			};
			Vertices.Add( RawVertex );			
		}		
	}

	// Copy the indices manually, since the LODModel's index buffer may have a different alignment.
	Indices.Empty(LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Num());
	for(int32 Index = 0;Index < LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Num();Index++)
	{
		Indices.Add(LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Get(Index));
	}

	// copy the wedge point indices
	if( LODModel.RawPointIndices.GetBulkDataSize() )
	{
		WedgePointIndices.Empty( LODModel.RawPointIndices.GetElementCount() );
		WedgePointIndices.AddUninitialized( LODModel.RawPointIndices.GetElementCount() );
		FMemory::Memcpy( WedgePointIndices.GetData(), LODModel.RawPointIndices.Lock(LOCK_READ_ONLY), LODModel.RawPointIndices.GetBulkDataSize() );
		LODModel.RawPointIndices.Unlock();
	}
}

/**
* Constructor. 
* Converts a static mesh to raw vertex data
* needed for creating a morph target mesh
*
* @param	SrcMesh - source static mesh to convert
* @param	LODIndex - level of detail to use for the geometry
*/
FMorphMeshRawSource::FMorphMeshRawSource( UStaticMesh* SrcMesh, int32 LODIndex ) :
	SourceMesh(SrcMesh)
{
	// @todo - not implemented
	// not sure if we will support static mesh morphing yet
}

/**
* Return true if current vertex data can be morphed to the target vertex data
* 
*/
bool FMorphMeshRawSource::IsValidTarget( const FMorphMeshRawSource& Target ) const
{
	//@todo sz -
	// heuristic is to check for the same number of original points
	//return( WedgePointIndices.Num() == Target.WedgePointIndices.Num() );
	return true;
}

