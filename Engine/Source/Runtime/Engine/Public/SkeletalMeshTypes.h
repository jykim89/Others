// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalMesh.h: Unreal skeletal mesh objects.
=============================================================================*/

/*-----------------------------------------------------------------------------
	USkinnedMeshComponent.
-----------------------------------------------------------------------------*/

#pragma once

#include "Components.h"
#include "GPUSkinPublicDefs.h"
#include "Components/PrimitiveComponent.h"
#include "PrimitiveSceneProxy.h"

// Define that controls showing chart of distance factors for skel meshes during entire run of the game on exit.
#define CHART_DISTANCE_FACTORS 0

class FRawStaticIndexBuffer16or32Interface;

/** 
* A pair of bone indices
*/
struct FBoneIndexPair
{
	int32 BoneIdx[2];

	bool operator==(const FBoneIndexPair& Src) const
	{
		return (BoneIdx[0] == Src.BoneIdx[0]) && (BoneIdx[1] == Src.BoneIdx[1]);
	}

	friend FORCEINLINE uint32 GetTypeHash( const FBoneIndexPair& BonePair )
	{
		return FCrc::MemCrc_DEPRECATED(&BonePair, sizeof(FBoneIndexPair));
	}

	/**
	* Serialize to Archive
	*/
	friend FArchive& operator<<( FArchive& Ar, FBoneIndexPair& W )
	{
		return Ar << W.BoneIdx[0] << W.BoneIdx[1];
	}
};

/** Which set of indices to select for TRISORT_CustomLeftRight sections. */
enum ECustomSortAlternateIndexMode
{
	CSAIM_Auto = 0,
	CSAIM_Left = 1,
	CSAIM_Right = 2,
};


class USkeletalMesh;


/*-----------------------------------------------------------------------------
	USkeletalMesh.
-----------------------------------------------------------------------------*/

struct FMeshWedge
{
	uint32			iVertex;			// Vertex index.
	FVector2D		UVs[MAX_TEXCOORDS];	// UVs.
	FColor			Color;			// Vertex color.
	friend FArchive &operator<<( FArchive& Ar, FMeshWedge& T )
	{
		Ar << T.iVertex;
		for( int32 UVIdx = 0; UVIdx < MAX_TEXCOORDS; ++UVIdx )
		{
			Ar << T.UVs[UVIdx];
		}
		Ar << T.Color;
		return Ar;
	}
};
template <> struct TIsPODType<FMeshWedge> { enum { Value = true }; };

struct FMeshFace
{
	uint32		iWedge[3];			// Textured Vertex indices.
	uint16		MeshMaterialIndex;	// Source Material (= texture plus unique flags) index.

	FVector	TangentX[3];
	FVector	TangentY[3];
	FVector	TangentZ[3];
	
};
template <> struct TIsPODType<FMeshFace> { enum { Value = true }; };

// A bone: an orientation, and a position, all relative to their parent.
struct VJointPos
{
	FTransform	Transform;

	float       Length;       //  For collision testing / debugging drawing...
	float       XSize;
	float       YSize;
	float       ZSize;
};

template <> struct TIsPODType<VJointPos> { enum { Value = true }; };

// This contains Reference-skeleton related info
// Bone transform is saved as FTransform array
struct FMeshBoneInfo
{
	// Bone's name.
	FName Name;			
	// 0/NULL if this is the root bone.  
	int32 ParentIndex;	

	FMeshBoneInfo() : Name(NAME_None), ParentIndex(INDEX_NONE) {}

	FMeshBoneInfo(const FName & InName, int32 InParentIndex)
	:	Name(InName)
	,	ParentIndex(InParentIndex)
	{}

	FMeshBoneInfo(const FMeshBoneInfo & Other)
		:	Name(Other.Name)
		,	ParentIndex(Other.ParentIndex)
	{}

	bool operator==( const FMeshBoneInfo& B ) const
	{
		return( Name == B.Name );
	}

	friend FArchive &operator<<( FArchive& Ar, FMeshBoneInfo& F);
};

/** Reference Skeleton **/
struct FReferenceSkeleton
{
private:
	/** Reference bone related info to be serialized **/
	TArray<FMeshBoneInfo>	RefBoneInfo;
	/** Reference bone transform **/
	TArray<FTransform>		RefBonePose;
	/** TMap to look up bone index from bone name. */
	TMap<FName, int32>		NameToIndexMap;

	/** Removes the specified bone, so long as it has no children. Returns whether we removed the bone or not */
	bool RemoveIndividualBone(int32 BoneIndex, TArray<int32>& OutBonesRemoved)
	{
		bool bRemoveThisBone = true;

		// Make sure we have no children
		for(int32 CurrBoneIndex=BoneIndex+1; CurrBoneIndex < GetNum(); CurrBoneIndex++)
		{
			if( RefBoneInfo[CurrBoneIndex].ParentIndex == BoneIndex )
			{
				bRemoveThisBone = false;
				break;
			}
		}

		if(bRemoveThisBone)
		{
			// Update parent indices of bones further through the array
			for(int32 CurrBoneIndex=BoneIndex+1; CurrBoneIndex < GetNum(); CurrBoneIndex++)
			{
				FMeshBoneInfo& Bone = RefBoneInfo[CurrBoneIndex];
				if( Bone.ParentIndex > BoneIndex )
				{
					Bone.ParentIndex -= 1;
				}
			}

			OutBonesRemoved.Add(BoneIndex);
			RefBonePose.RemoveAt(BoneIndex, 1);
			RefBoneInfo.RemoveAt(BoneIndex, 1);
		}
		return bRemoveThisBone;
	}

public:
	void Allocate(int32 Size)
	{
		NameToIndexMap.Empty(Size);
		RefBoneInfo.Empty(Size);
		RefBonePose.Empty(Size);
	}

	/** Returns number of bones in Skeleton. */
	int32 GetNum() const
	{
		return RefBoneInfo.Num();
	}

	/** Accessor to private data. Const so it can't be changed recklessly. */
	const TArray<FMeshBoneInfo> & GetRefBoneInfo() const
	{
		return RefBoneInfo;
	}

	/** Accessor to private data. Const so it can't be changed recklessly. */
	const TArray<FTransform> & GetRefBonePose() const
	{
		return RefBonePose;
	}

	void UpdateRefPoseTransform(const int32 & BoneIndex, const FTransform & BonePose)
	{
		RefBonePose[BoneIndex] = BonePose;
	}

	/** Add a new bone. 
	 * BoneName must not already exist! ParentIndex must be valid. */
	void Add(const FMeshBoneInfo & BoneInfo, const FTransform & BonePose)
	{
		// Adding a bone that already exists is illegal
		check(FindBoneIndex(BoneInfo.Name) == INDEX_NONE);
	
		// Make sure our arrays are in sync.
		checkSlow( (RefBoneInfo.Num() == RefBonePose.Num()) && (RefBoneInfo.Num() == NameToIndexMap.Num()) );

		const int32 BoneIndex = RefBoneInfo.Add(BoneInfo);
		RefBonePose.Add(BonePose);
		NameToIndexMap.Add(BoneInfo.Name, BoneIndex);

		// Normalize Quaternion to be safe.
		RefBonePose[BoneIndex].NormalizeRotation();

		// Parent must be valid. Either INDEX_NONE for Root, or before us.
		check( ((BoneIndex == 0) && (BoneInfo.ParentIndex == INDEX_NONE)) || ((BoneIndex > 0) && RefBoneInfo.IsValidIndex(BoneInfo.ParentIndex)) );
	}

	/** Insert a new bone
	* BoneName must not already exist! ParentIndex must be valid. */
	void Insert(int32 BoneIndex, const FMeshBoneInfo & BoneInfo, const FTransform & BonePose)
	{
		// Make sure our arrays are in sync.
		checkSlow( (RefBoneInfo.Num() == RefBonePose.Num()) && (RefBoneInfo.Num() == NameToIndexMap.Num()) );

		// Inserting a bone that already exists is illegal
		check(FindBoneIndex(BoneInfo.Name) == INDEX_NONE);

		// Parent must be valid. Either INDEX_NONE for Root, or before us.
		check( ((BoneIndex == 0) && (BoneInfo.ParentIndex == INDEX_NONE)) || ((BoneIndex > 0) && RefBoneInfo.IsValidIndex(BoneInfo.ParentIndex)) );

		// Make sure our bone transform is valid.
		check( BonePose.GetRotation().IsNormalized() );

		RefBoneInfo.Insert(BoneInfo, BoneIndex);
		RefBonePose.Insert(BonePose, BoneIndex);
		NameToIndexMap.Add(BoneInfo.Name, BoneIndex);

		// Normalize Quaternion to be safe.
		RefBonePose[BoneIndex].NormalizeRotation();

		// Now we need to fix all the parent indices that pointed to bones after this in the array
		// These must be after this point in the array.
		for(int32 j=BoneIndex+1; j<GetNum(); j++)
		{
			if( GetParentIndex(j) >= BoneIndex )
			{
				RefBoneInfo[j].ParentIndex += 1;
			}
		}
	}

	void Empty()
	{
		RefBoneInfo.Empty();
		RefBonePose.Empty();
		NameToIndexMap.Empty();
	}

	/** Find Bone Index from BoneName. Precache as much as possible in speed critical sections! */
	int32 FindBoneIndex(const FName & BoneName) const
	{
		checkSlow(RefBoneInfo.Num() == NameToIndexMap.Num());
		int32 BoneIndex = INDEX_NONE;
		if( BoneName != NAME_None )
		{
			const int32* IndexPtr = NameToIndexMap.Find(BoneName);
			if( IndexPtr )
			{
				BoneIndex = *IndexPtr;
			}
		}
		return BoneIndex;
	}

	FName GetBoneName(const int32 & BoneIndex) const
	{
		return RefBoneInfo[BoneIndex].Name;
	}

	int32 GetParentIndex(const int32 & BoneIndex) const
	{
		// Parent must be valid. Either INDEX_NONE for Root, or before us.
		checkSlow( ((BoneIndex == 0) && (RefBoneInfo[BoneIndex].ParentIndex == INDEX_NONE)) || ((BoneIndex > 0) && RefBoneInfo.IsValidIndex(RefBoneInfo[BoneIndex].ParentIndex)) );
		return RefBoneInfo[BoneIndex].ParentIndex;
	}

	bool IsValidIndex(int32 Index) const
	{
		return (RefBoneInfo.IsValidIndex(Index));
	}

	/** 
	 * Returns # of Depth from BoneIndex to ParentBoneIndex
	 * This will return 0 if BoneIndex == ParentBoneIndex;
	 * This will return -1 if BoneIndex isn't child of ParentBoneIndex
	 */
	int32 GetDepthBetweenBones(const int32 & BoneIndex, const int32 & ParentBoneIndex) const
	{
		if (BoneIndex >= ParentBoneIndex)
		{
			int32 CurBoneIndex = BoneIndex;
			int32 Depth = 0;

			do
			{
				// if same return;
				if (CurBoneIndex == ParentBoneIndex)
				{
					return Depth;
				}

				CurBoneIndex = RefBoneInfo[CurBoneIndex].ParentIndex;
				++Depth;

			} while (CurBoneIndex!=INDEX_NONE);
		}

		return INDEX_NONE;
	}

	bool BoneIsChildOf(const int32 & ChildBoneIndex, const int32 & ParentBoneIndex) const
	{
		// Bones are in strictly increasing order.
		// So child must have an index greater than his parent.
		if( ChildBoneIndex > ParentBoneIndex )
		{
			int32 BoneIndex = GetParentIndex(ChildBoneIndex);
			do
			{
				if( BoneIndex == ParentBoneIndex )
				{
					return true;
				}
				BoneIndex = GetParentIndex(BoneIndex);

			} while (BoneIndex != INDEX_NONE);
		}

		return false;
	}

	void RemoveDuplicateBones(const UObject * Requester, TArray<FBoneIndexType> & DuplicateBones)
	{
		const int32 NumBones = GetNum();
		DuplicateBones.Empty();

		TMap<FName, int32> BoneNameCheck;
		bool bRemovedBones = false;
		for(int32 BoneIndex=NumBones-1; BoneIndex>=0; BoneIndex--)
		{
			const FName & BoneName = GetBoneName(BoneIndex);
			const int32 * FoundBoneIndexPtr = BoneNameCheck.Find(BoneName);

			// Not a duplicate bone, track it.
			if( FoundBoneIndexPtr == NULL )
			{
				BoneNameCheck.Add(BoneName, BoneIndex);
			}
			else 
			{
				const int32 DuplicateBoneIndex = *FoundBoneIndexPtr;
				DuplicateBones.Add(DuplicateBoneIndex);

				UE_LOG(LogAnimation, Warning, TEXT("RemoveDuplicateBones: duplicate bone name (%s) detected for (%s)! Indices: %d and %d. Removing the latter."), 
					*BoneName.ToString(), *GetNameSafe(Requester), DuplicateBoneIndex, BoneIndex);

				// Remove duplicate bone index, which was added later as a mistake.
				RefBonePose.RemoveAt(DuplicateBoneIndex, 1);
				RefBoneInfo.RemoveAt(DuplicateBoneIndex, 1);

				// Now we need to fix all the parent indices that pointed to bones after this in the array
				// These must be after this point in the array.
				for(int32 j=DuplicateBoneIndex; j<GetNum(); j++)
				{
					if( GetParentIndex(j) >= DuplicateBoneIndex )
					{
						RefBoneInfo[j].ParentIndex -= 1;
					}
				}

				// Update entry in case problem bones were added multiple times.
				BoneNameCheck.Add(BoneName, BoneIndex);

				// We need to make sure that any bone that has this old bone as a parent is fixed up
				bRemovedBones = true;
			}
		}

		// If we've removed bones, we need to rebuild our name table.
		if( bRemovedBones || (NameToIndexMap.Num() == 0) )
		{
			RebuildNameToIndexMap();
		}

		// Make sure our arrays are in sync.
		checkSlow( (RefBoneInfo.Num() == RefBonePose.Num()) && (RefBoneInfo.Num() == NameToIndexMap.Num()) );

		// Additionally normalize all quaternions to be safe.
		for(int32 BoneIndex=0; BoneIndex<GetNum(); BoneIndex++)
		{
			RefBonePose[BoneIndex].NormalizeRotation();
		}
	}

	/** Removes the supplied bones from the skeleton, unless they have children that aren't also going to be removed */
	TArray<int32> RemoveBonesByName(const TArray<FName>& BonesToRemove)
	{
		TArray<int32> BonesRemoved;

		const int32 NumBones = GetNum();
		for(int32 BoneIndex=NumBones-1; BoneIndex>=0; BoneIndex--)
		{
			FMeshBoneInfo& Bone = RefBoneInfo[BoneIndex];

			if(BonesToRemove.Contains(Bone.Name))
			{
				RemoveIndividualBone(BoneIndex, BonesRemoved);
			}
		}
		RebuildNameToIndexMap();
		return BonesRemoved;
	}

	void RebuildNameToIndexMap()
	{
		// Start by clearing the current map.
		NameToIndexMap.Empty();

		// Then iterate over each bone, adding the name and bone index.
		const int32 NumBones = GetNum();
		for(int32 BoneIndex=0; BoneIndex<NumBones; BoneIndex++)
		{
			const FName & BoneName = GetBoneName(BoneIndex);
			if( BoneName != NAME_None )
			{
				NameToIndexMap.Add(BoneName, BoneIndex);
			}
			else
			{
				UE_LOG(LogAnimation, Warning, TEXT("RebuildNameToIndexMap: Bone with no name detected for index: %d"), BoneIndex);
			}
		}

		// Make sure we don't have duplicate bone names. This would be very bad.
		checkSlow(NameToIndexMap.Num() == NumBones);
	}

	friend FArchive & operator<<(FArchive & Ar, FReferenceSkeleton & F);
};

// Textured triangle.
struct VTriangle
{
	uint32   WedgeIndex[3];	 // Point to three vertices in the vertex list.
	uint8    MatIndex;	     // Materials can be anything.
	uint8    AuxMatIndex;     // Second material from exporter (unused)
	uint32   SmoothingGroups; // 32-bit flag for smoothing groups.

	FVector	TangentX[3];
	FVector	TangentY[3];
	FVector	TangentZ[3];


	VTriangle& operator=( const VTriangle& Other)
	{
		this->AuxMatIndex = Other.AuxMatIndex;
		this->MatIndex        =  Other.MatIndex;
		this->SmoothingGroups =  Other.SmoothingGroups;
		this->WedgeIndex[0]   =  Other.WedgeIndex[0];
		this->WedgeIndex[1]   =  Other.WedgeIndex[1];
		this->WedgeIndex[2]   =  Other.WedgeIndex[2];
		this->TangentX[0]   =  Other.TangentX[0];
		this->TangentX[1]   =  Other.TangentX[1];
		this->TangentX[2]   =  Other.TangentX[2];

		this->TangentY[0]   =  Other.TangentY[0];
		this->TangentY[1]   =  Other.TangentY[1];
		this->TangentY[2]   =  Other.TangentY[2];

		this->TangentZ[0]   =  Other.TangentZ[0];
		this->TangentZ[1]   =  Other.TangentZ[1];
		this->TangentZ[2]   =  Other.TangentZ[2];

		return *this;
	}
};
template <> struct TIsPODType<VTriangle> { enum { Value = true }; };

struct FVertInfluence 
{
	float Weight;
	uint32 VertIndex;
	FBoneIndexType BoneIndex;
	friend FArchive &operator<<( FArchive& Ar, FVertInfluence& F )
	{
		Ar << F.Weight << F.VertIndex << F.BoneIndex;
		return Ar;
	}
};
template <> struct TIsPODType<FVertInfluence> { enum { Value = true }; };

/**
* Data needed for importing an extra set of vertex influences
*/
struct FSkelMeshExtraInfluenceImportData
{
	FReferenceSkeleton		RefSkeleton;
	TArray<FVertInfluence> Influences;
	TArray<FMeshWedge> Wedges;
	TArray<FMeshFace> Faces;
	TArray<FVector> Points;
	int32 MaxBoneCountPerChunk;
};

//
//	FSoftSkinVertex
//

struct FSoftSkinVertex
{
	FVector			Position;
	FPackedNormal	TangentX,	// Tangent, U-direction
					TangentY,	// Binormal, V-direction
					TangentZ;	// Normal
	FVector2D		UVs[MAX_TEXCOORDS]; // UVs
	FColor			Color;		// VertexColor
	uint8			InfluenceBones[MAX_TOTAL_INFLUENCES];
	uint8			InfluenceWeights[MAX_TOTAL_INFLUENCES];

	/**
	* Serializer
	*
	* @param Ar - archive to serialize with
	* @param V - vertex to serialize
	* @return archive that was used
	*/
	friend FArchive& operator<<(FArchive& Ar,FSoftSkinVertex& V);
};

//
//	FRigidSkinVertex
//

struct FRigidSkinVertex
{
	FVector			Position;
	FPackedNormal	TangentX,	// Tangent, U-direction
					TangentY,	// Binormal, V-direction
					TangentZ;	// Normal
	FVector2D		UVs[MAX_TEXCOORDS]; // UVs
	FColor			Color;		// Vertex color.
	uint8			Bone;

	/**
	* Serializer
	*
	* @param Ar - archive to serialize with
	* @param V - vertex to serialize
	* @return archive that was used
	*/
	friend FArchive& operator<<(FArchive& Ar,FRigidSkinVertex& V);
};

/**
 * A structure for holding the APEX cloth physical-to-render mapping data
 */
struct FApexClothPhysToRenderVertData
{
	/**
	\brief xyz : Barycentric coordinates of the graphical vertex relative to the simulated triangle.
		   w : distance from the mesh

	\note PX_MAX_F32 values represent invalid coordinates.
	*/

	FVector4 PositionBaryCoordsAndDist;
	FVector4 NormalBaryCoordsAndDist;
	FVector4 TangentBaryCoordsAndDist;
	uint16	 SimulMeshVertIndices[4];
	//dummy for alignment 16 bytes
	uint32	 Padding[2];

	/**
	 * Serializer
	 *
	 * @param Ar - archive to serialize with
	 * @param V - vertex to serialize
	 * @return archive that was used
	 */
	friend FArchive& operator<<(FArchive& Ar, FApexClothPhysToRenderVertData& V)
	{
		Ar	<< V.PositionBaryCoordsAndDist 
			<< V.NormalBaryCoordsAndDist
			<< V.TangentBaryCoordsAndDist
			<< V.SimulMeshVertIndices[0]
			<< V.SimulMeshVertIndices[1]
			<< V.SimulMeshVertIndices[2]
			<< V.SimulMeshVertIndices[3]
			<< V.Padding[0]
			<< V.Padding[1];
		return Ar;
	}
};

/**
 * A structure for holding the APEX cloth collision volumes data
 */
struct FApexClothCollisionVolumeData
{
	/**
	\brief structure for presenting collision volume data

	\note contains either capsule data or convex data.
	*/

	int32	BoneIndex;
	uint32	ConvexVerticesCount;
	uint32	ConvexVerticesStart;
	float	CapsuleRadius;
	float	CapsuleHeight;
	FMatrix LocalPose;

	FApexClothCollisionVolumeData()
	{
		BoneIndex = -1;
		ConvexVerticesCount = 0;
		ConvexVerticesStart = 0;
		CapsuleRadius = 0.0f;
		CapsuleHeight = 0.0f;
		LocalPose.SetIdentity();
	};

	bool IsCapsule()
	{
		return (ConvexVerticesCount == 0);
	}
};

/**
 * \brief A structure for holding bone sphere ( one of the APEX cloth collision volumes data )
 * \note 2 bone spheres present a capsule
 */
struct FApexClothBoneSphereData
{
	int32	BoneIndex;
	float	Radius;
	FVector LocalPos;
};

/**
 * A set of the skeletal mesh vertices which use the same set of <MAX_GPUSKIN_BONES bones.
 * In practice, there is a 1:1 mapping between chunks and sections, but for meshes which
 * were imported before chunking was implemented, there will be a single chunk for all
 * sections.
 */
struct FSkelMeshChunk
{
	/** The offset into the LOD's vertex buffer of this chunk's vertices. */
	uint32 BaseVertexIndex;

	/** The rigid vertices of this chunk. */
	TArray<FRigidSkinVertex> RigidVertices;

	/** The soft vertices of this chunk. */
	TArray<FSoftSkinVertex> SoftVertices;

	/** The extra vertex data for mapping to an APEX clothing simulation mesh. */
	TArray<FApexClothPhysToRenderVertData> ApexClothMappingData;

	/** The physical mesh vertices imported from the APEX file. */
	TArray<FVector> PhysicalMeshVertices;

	/** The physical mesh normals imported from the APEX file. */
	TArray<FVector> PhysicalMeshNormals;

	/** The bones which are used by the vertices of this chunk. Indices of bones in the USkeletalMesh::RefSkeleton array */
	TArray<FBoneIndexType> BoneMap;

	/** The number of rigid vertices in this chunk */
	int32 NumRigidVertices;
	/** The number of soft vertices in this chunk */
	int32 NumSoftVertices;

	/** max # of bones used to skin the vertices in this chunk */
	int32 MaxBoneInfluences;

	int16 CorrespondClothAssetIndex;
	int16 ClothAssetSubmeshIndex;

	FSkelMeshChunk()
		: BaseVertexIndex(0)
		, NumRigidVertices(0)
		, NumSoftVertices(0)
		, MaxBoneInfluences(4)
		, CorrespondClothAssetIndex(-1)
		, ClothAssetSubmeshIndex(-1)
	{}

	FSkelMeshChunk(const FSkelMeshChunk& Other)
	{
		BaseVertexIndex = Other.BaseVertexIndex;
		RigidVertices = Other.RigidVertices;
		SoftVertices = Other.SoftVertices;
		ApexClothMappingData = Other.ApexClothMappingData;
		PhysicalMeshVertices = Other.PhysicalMeshVertices;
		PhysicalMeshNormals = Other.PhysicalMeshNormals;
		BoneMap = Other.BoneMap;
		NumRigidVertices = Other.NumRigidVertices;
		NumSoftVertices = Other.NumSoftVertices;
		MaxBoneInfluences = Other.MaxBoneInfluences;
		CorrespondClothAssetIndex = Other.CorrespondClothAssetIndex;
		ClothAssetSubmeshIndex = Other.ClothAssetSubmeshIndex;
	}

	/**
	* @return total num rigid verts for this chunk
	*/
	FORCEINLINE int32 GetNumRigidVertices() const
	{
		return NumRigidVertices;
	}

	/**
	* @return total num soft verts for this chunk
	*/
	FORCEINLINE int32 GetNumSoftVertices() const
	{
		return NumSoftVertices;
	}

	/**
	* @return total number of soft and rigid verts for this chunk
	*/
	FORCEINLINE int32 GetNumVertices() const
	{
		return GetNumRigidVertices() + GetNumSoftVertices();
	}

	/**
	* @return starting index for rigid verts for this chunk in the LOD vertex buffer
	*/
	FORCEINLINE int32 GetRigidVertexBufferIndex() const
	{
		return BaseVertexIndex;
	}

	/**
	* @return starting index for soft verts for this chunk in the LOD vertex buffer
	*/
	FORCEINLINE int32 GetSoftVertexBufferIndex() const
	{
		return BaseVertexIndex + NumRigidVertices;
	}

	/**
	* @return TRUE if we have cloth data for this chunk
	*/
	FORCEINLINE bool HasApexClothData() const
	{
		return (ApexClothMappingData.Num() > 0);
	}

	FORCEINLINE void SetClothSubmeshIndex(int16 AssetIndex, int16 AssetSubmeshIndex)
	{
		CorrespondClothAssetIndex = AssetIndex;
		ClothAssetSubmeshIndex = AssetSubmeshIndex;
	}

	/**
	* Calculate max # of bone influences used by this skel mesh chunk
	*/
	ENGINE_API void CalcMaxBoneInfluences();

	FORCEINLINE bool HasExtraBoneInfluences() const
	{
		return MaxBoneInfluences > MAX_INFLUENCES_PER_STREAM;
	}

	/**
	* Serialize this class
	* @param Ar - archive to serialize to
	* @param C - skel mesh chunk to serialize
	*/
	friend FArchive& operator<<(FArchive& Ar,FSkelMeshChunk& C);
};


/** Helper to convert the above enum to string */
static const TCHAR* TriangleSortOptionToString(ETriangleSortOption Option)
{
	switch(Option)
	{
		case TRISORT_CenterRadialDistance:
			return TEXT("CenterRadialDistance");
		case TRISORT_Random:
			return TEXT("Random");
		case TRISORT_MergeContiguous:
			return TEXT("MergeContiguous");
		case TRISORT_Custom:
			return TEXT("Custom");
		case TRISORT_CustomLeftRight:
			return TEXT("CustomLeftRight");
		default:
			return TEXT("None");
	}
}

/**
 * A set of skeletal mesh triangles which use the same material and chunk.
 */
struct FSkelMeshSection
{
	/** Material (texture) used for this section. */
	uint16 MaterialIndex;

	/** The chunk that vertices for this section are from. */
	uint16 ChunkIndex;

	/** The offset of this section's indices in the LOD's index buffer. */
	uint32 BaseIndex;

	/** The number of triangles in this section. */
	uint32 NumTriangles;

	/** Current triangle sorting method */
	TEnumAsByte<ETriangleSortOption> TriangleSorting;

	/** Is this mesh selected? */
	uint8 bSelected:1;

	/** This Section can be disabled for cloth simulation and corresponding Cloth Section will be enabled*/
	bool bDisabled;
	/** Corresponding Section Index will be enabled when this section is disabled 
		because corresponding cloth section will be showed instead of this
		or disabled section index when this section is enabled for cloth simulation
	*/
	int16 CorrespondClothSectionIndex;

	/** Decide whether enabling clothing LOD for this section or not, just using skelmesh LOD_0's one to decide */
	uint8 bEnableClothLOD;

	FSkelMeshSection()
		: MaterialIndex(0)
		, ChunkIndex(0)
		, BaseIndex(0)
		, NumTriangles(0)
		, TriangleSorting(0)
		, bSelected(false)
		, bDisabled(false)
		, CorrespondClothSectionIndex(-1)
		, bEnableClothLOD(true)
	{}

	// Serialization.
	friend FArchive& operator<<(FArchive& Ar, FSkelMeshSection& S);
};
template <> struct TIsPODType<FSkelMeshSection> { enum { Value = true }; };

/**
* Base vertex data for GPU skinned skeletal meshes
*	make sure to update GpuSkinCacheCommon.usf if the member sizes/order change!
*/
template <bool bExtraBoneInfluences>
struct TGPUSkinVertexBase
{
	enum
	{
		NumInfluences = bExtraBoneInfluences ? MAX_TOTAL_INFLUENCES : MAX_INFLUENCES_PER_STREAM,
	};
	FPackedNormal	TangentX,	// Tangent, U-direction
					TangentZ;	// Normal	
	uint8			InfluenceBones[NumInfluences];
	uint8			InfluenceWeights[NumInfluences];

	/**
	* Serializer
	*
	* @param Ar - archive to serialize with
	*/
	void Serialize(FArchive& Ar);
	void Serialize(FArchive& Ar, FVector & OutPosition)
	{
		Serialize(Ar);
	}
};

/** 
* 16 bit UV version of skeletal mesh vertex
*	make sure to update GpuSkinCacheCommon.usf if the member sizes/order change!
*/
template<uint32 NumTexCoords, bool bExtraBoneInfluencesT>
struct TGPUSkinVertexFloat16Uvs : public TGPUSkinVertexBase<bExtraBoneInfluencesT>
{
	/** full float position **/
	FVector			Position;
	/** half float UVs */
	FVector2DHalf	UVs[NumTexCoords];

	/**
	* Serializer
	*
	* @param Ar - archive to serialize with
	* @param V - vertex to serialize
	* @return archive that was used
	*/
	friend FArchive& operator<<(FArchive& Ar,TGPUSkinVertexFloat16Uvs& V)
	{
		V.Serialize(Ar);
		Ar << V.Position;

		for(uint32 UVIndex = 0;UVIndex < NumTexCoords;UVIndex++)
		{
			Ar << V.UVs[UVIndex];
		}
		return Ar;
	}
};

/** 
* 32 bit UV version of skeletal mesh vertex
*	make sure to update GpuSkinCacheCommon.usf if the member sizes/order change!
*/
template<uint32 NumTexCoords, bool bExtraBoneInfluencesT>
struct TGPUSkinVertexFloat32Uvs : public TGPUSkinVertexBase<bExtraBoneInfluencesT>
{
	/** full float position **/
	FVector			Position;
	/** full float UVs */
	FVector2D UVs[NumTexCoords];

	/**
	* Serializer
	*
	* @param Ar - archive to serialize with
	* @param V - vertex to serialize
	* @return archive that was used
	*/
	friend FArchive& operator<<(FArchive& Ar,TGPUSkinVertexFloat32Uvs& V)
	{
		V.Serialize(Ar);
		Ar << V.Position;

		for(uint32 UVIndex = 0;UVIndex < NumTexCoords;UVIndex++)
		{
			Ar << V.UVs[UVIndex];
		}
		return Ar;
	}
};

/**
 * A structure for holding a skeletal mesh vertex color
 */
struct FGPUSkinVertexColor
{
	/** VertexColor */
	FColor VertexColor;

	/**
	 * Serializer
	 *
	 * @param Ar - archive to serialize with
	 * @param V - vertex to serialize
	 * @return archive that was used
	 */
	friend FArchive& operator<<(FArchive& Ar, FGPUSkinVertexColor& V)
	{
		Ar << V.VertexColor;
		return Ar;
	}
};

/** An interface to the skel-mesh vertex data storage type. */
class FSkeletalMeshVertexDataInterface
{
public:

	/** Virtual destructor. */
	virtual ~FSkeletalMeshVertexDataInterface() {}

	/**
	* Resizes the vertex data buffer, discarding any data which no longer fits.
	* @param NumVertices - The number of vertices to allocate the buffer for.
	*/
	virtual void ResizeBuffer(uint32 NumVertices) = 0;

	/** @return The stride of the vertex data in the buffer. */
	virtual uint32 GetStride() const = 0;

	/** @return A pointer to the data in the buffer. */
	virtual uint8* GetDataPointer() = 0;

	/** @return number of vertices in the buffer */
	virtual uint32 GetNumVertices() = 0;

	/** @return A pointer to the FResourceArrayInterface for the vertex data. */
	virtual FResourceArrayInterface* GetResourceArray() = 0;

	/** Serializer. */
	virtual void Serialize(FArchive& Ar) = 0;
};


/** The implementation of the skeletal mesh vertex data storage type. */
template<typename VertexDataType>
class TSkeletalMeshVertexData :
	public FSkeletalMeshVertexDataInterface,
	public TResourceArray<VertexDataType,VERTEXBUFFER_ALIGNMENT>
{
public:
	typedef TResourceArray<VertexDataType,VERTEXBUFFER_ALIGNMENT> ArrayType;

	/**
	* Constructor
	* @param InNeedsCPUAccess - true if resource array data should be CPU accessible
	*/
	TSkeletalMeshVertexData(bool InNeedsCPUAccess=false)
		:	TResourceArray<VertexDataType,VERTEXBUFFER_ALIGNMENT>(InNeedsCPUAccess)
	{
	}
	
	/**
	* Resizes the vertex data buffer, discarding any data which no longer fits.
	*
	* @param NumVertices - The number of vertices to allocate the buffer for.
	*/
	virtual void ResizeBuffer(uint32 NumVertices)
	{
		if((uint32)ArrayType::Num() < NumVertices)
		{
			// Enlarge the array.
			ArrayType::AddUninitialized(NumVertices - ArrayType::Num());
		}
		else if((uint32)ArrayType::Num() > NumVertices)
		{
			// Shrink the array.
			ArrayType::RemoveAt(NumVertices,ArrayType::Num() - NumVertices);
		}
	}
	/**
	* @return stride of the vertex type stored in the resource data array
	*/
	virtual uint32 GetStride() const
	{
		return sizeof(VertexDataType);
	}
	/**
	* @return uint8 pointer to the resource data array
	*/
	virtual uint8* GetDataPointer()
	{
		return (uint8*)&(*this)[0];
	}
	/**
	* @return number of vertices stored in the resource data array
	*/
	virtual uint32 GetNumVertices()
	{
		return ArrayType::Num();
	}
	/**
	* @return resource array interface access
	*/
	virtual FResourceArrayInterface* GetResourceArray()
	{
		return this;
	}
	/**
	* Serializer for this class
	*
	* @param Ar - archive to serialize to
	* @param B - data to serialize
	*/
	virtual void Serialize(FArchive& Ar)
	{
		ArrayType::BulkSerialize(Ar);
	}
	/**
	* Assignment operator. This is currently the only method which allows for 
	* modifying an existing resource array
	*/
	TSkeletalMeshVertexData<VertexDataType>& operator=(const TArray<VertexDataType>& Other)
	{
		ArrayType::operator=(TArray<VertexDataType,TAlignedHeapAllocator<VERTEXBUFFER_ALIGNMENT> >(Other));
		return *this;
	}
};

/** 
* Vertex buffer with static lod chunk vertices for use with GPU skinning 
*/
class FSkeletalMeshVertexBuffer : public FVertexBuffer
{
public:
	/**
	* Constructor
	*/
	ENGINE_API FSkeletalMeshVertexBuffer();

	/**
	* Destructor
	*/
	ENGINE_API virtual ~FSkeletalMeshVertexBuffer();

	/**
	* Assignment. Assumes that vertex buffer will be rebuilt 
	*/
	ENGINE_API FSkeletalMeshVertexBuffer& operator=(const FSkeletalMeshVertexBuffer& Other);
	/**
	* Constructor (copy)
	*/
	ENGINE_API FSkeletalMeshVertexBuffer(const FSkeletalMeshVertexBuffer& Other);

	/** 
	* Delete existing resources 
	*/
	void CleanUp();

	/** 
	* @return true is VertexData is valid 
	*/
	bool IsVertexDataValid() const 
	{ 
		return VertexData != NULL; 
	}

	/**
	* Initializes the buffer with the given vertices.
	* @param InVertices - The vertices to initialize the buffer with.
	*/
	void Init(const TArray<FSoftSkinVertex>& InVertices);

	/**
	* Serializer for this class
	* @param Ar - archive to serialize to
	* @param B - data to serialize
	*/
	friend FArchive& operator<<(FArchive& Ar,FSkeletalMeshVertexBuffer& VertexBuffer);

	// FRenderResource interface.

	/**
	* Initialize the RHI resource for this vertex buffer
	*/
	virtual void InitRHI();

	/**
	* @return text description for the resource type
	*/
	virtual FString GetFriendlyName() const;

	// Vertex data accessors.

	/** 
	* Const access to entry in vertex data array
	*
	* @param VertexIndex - index into the vertex buffer
	* @return pointer to vertex data cast to base vertex type
	*/
	template <bool bExtraBoneInfluencesT>
	FORCEINLINE const TGPUSkinVertexBase<bExtraBoneInfluencesT>* GetVertexPtr(uint32 VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		return (TGPUSkinVertexBase<bExtraBoneInfluencesT>*)(Data + VertexIndex * Stride);
	}
	/** 
	* Non=Const access to entry in vertex data array
	*
	* @param VertexIndex - index into the vertex buffer
	* @return pointer to vertex data cast to base vertex type
	*/
	template <bool bExtraBoneInfluencesT>
	FORCEINLINE TGPUSkinVertexBase<bExtraBoneInfluencesT>* GetVertexPtr(uint32 VertexIndex)
	{
		checkSlow(VertexIndex < GetNumVertices());
		return (TGPUSkinVertexBase<bExtraBoneInfluencesT>*)(Data + VertexIndex * Stride);
	}

	/**
	* Get the vertex UV values at the given index in the vertex buffer.
	*
	* @param VertexIndex - index into the vertex buffer
	* @param UVIndex - [0,MAX_TEXCOORDS] value to index into UVs array
	* @return 2D UV values
	*/
	template <bool bExtraBoneInfluencesT>
	FORCEINLINE FVector2D GetVertexUVFast(uint32 VertexIndex,uint32 UVIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		if( !bUseFullPrecisionUVs )
		{
			return ((TGPUSkinVertexFloat16Uvs<MAX_TEXCOORDS, bExtraBoneInfluencesT>*)(Data + VertexIndex * Stride))->UVs[UVIndex];
		}
		else
		{
			return ((TGPUSkinVertexFloat32Uvs<MAX_TEXCOORDS, bExtraBoneInfluencesT>*)(Data + VertexIndex * Stride))->UVs[UVIndex];
		}		
	}	

	/**
	* Get the vertex UV values at the given index in the vertex buffer.
	*
	* @param VertexIndex - index into the vertex buffer
	* @param UVIndex - [0,MAX_TEXCOORDS] value to index into UVs array
	* @return 2D UV values
	*/
	FORCEINLINE FVector2D GetVertexUV(uint32 VertexIndex,uint32 UVIndex) const
	{
		return bExtraBoneInfluences
			? GetVertexUVFast<true>(VertexIndex, UVIndex)
			: GetVertexUVFast<false>(VertexIndex, UVIndex);
	}

	/**
	* Get the vertex XYZ values at the given index in the vertex buffer
	*
	* @param VertexIndex - index into the vertex buffer
	* @return FVector 3D position
	*/
	FORCEINLINE FVector GetVertexPositionSlow(uint32 VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		return bExtraBoneInfluences
			? GetVertexPositionFast<true>((const TGPUSkinVertexBase<true>*)(Data + VertexIndex * Stride))
			: GetVertexPositionFast<false>((const TGPUSkinVertexBase<false>*)(Data + VertexIndex * Stride));
	}

	/**
	* Get the vertex XYZ values of the given SrcVertex
	*
	* @param TGPUSkinVertexBase *
	* @return FVector 3D position
	*/
	template <bool bExtraBoneInfluencesT>
	FORCEINLINE FVector GetVertexPositionFast(const TGPUSkinVertexBase<bExtraBoneInfluencesT>* SrcVertex) const
	{
		if( !bUseFullPrecisionUVs )
		{
			return ((TGPUSkinVertexFloat16Uvs<MAX_TEXCOORDS, bExtraBoneInfluencesT>*)(SrcVertex))->Position;
		}
		else
		{
			return ((TGPUSkinVertexFloat32Uvs<MAX_TEXCOORDS, bExtraBoneInfluencesT>*)(SrcVertex))->Position;
		}		
	}

	/**
	* Get the vertex XYZ values of the given SrcVertex
	*
	* @param TGPUSkinVertexBase *
	* @return FVector 3D position
	*/
	template <bool bExtraBoneInfluencesT>
	FORCEINLINE FVector GetVertexPositionFast(uint32 VertexIndex) const
	{
		return GetVertexPositionFast<bExtraBoneInfluencesT>((const TGPUSkinVertexBase<bExtraBoneInfluencesT>*)(Data + VertexIndex * Stride));
	}

	// Other accessors.

	/** 
	* @return true if using 32 bit floats for UVs 
	*/
	FORCEINLINE bool GetUseFullPrecisionUVs() const
	{
		return bUseFullPrecisionUVs;
	}
	/** 
	* @param UseFull - set to true if using 32 bit floats for UVs 
	*/
	FORCEINLINE void SetUseFullPrecisionUVs(bool UseFull)
	{
		bUseFullPrecisionUVs = UseFull;
	}
	/** 
	* @return number of vertices in this vertex buffer
	*/
	FORCEINLINE uint32 GetNumVertices() const
	{
		return NumVertices;
	}
	/** 
	* @return cached stride for vertex data type for this vertex buffer
	*/
	FORCEINLINE uint32 GetStride() const
	{
		return Stride;
	}
	/** 
	* @return total size of data in resource array
	*/
	FORCEINLINE uint32 GetVertexDataSize() const
	{
		return NumVertices * Stride;
	}
	/** 
	* @return Mesh Origin 
	*/
	FORCEINLINE const FVector& GetMeshOrigin() const 		
	{ 
		return MeshOrigin; 
	}

	/** 
	* @return Mesh Extension
	*/
	FORCEINLINE const FVector& GetMeshExtension() const
	{ 
		return MeshExtension;
	}

	/**
	 * @return the number of texture coordinate sets in this buffer
	 */
	FORCEINLINE uint32 GetNumTexCoords() const 
	{
		return NumTexCoords;
	}

	/** 
	* @param bInNeedsCPUAccess - set to true if the CPU needs access to this vertex buffer
	*/
	void SetNeedsCPUAccess(bool bInNeedsCPUAccess);
	bool GetNeedsCPUAccess() const { return bNeedsCPUAccess; }

	/** 
	* @param bInHasExtraBoneInfluences - set to true if this will have extra streams for bone indices & weights.
	*/
	FORCEINLINE void SetHasExtraBoneInfluences(bool bInHasExtraBoneInfluences)
	{
		bExtraBoneInfluences = bInHasExtraBoneInfluences;
	}

	FORCEINLINE bool HasExtraBoneInfluences() const
	{
		return bExtraBoneInfluences;
	}

	/**
	 * @param InNumTexCoords	The number of texture coordinate sets that should be in this mesh
	 */
	FORCEINLINE void SetNumTexCoords( uint32 InNumTexCoords ) 
	{
		NumTexCoords = InNumTexCoords;
	}
	
	/**
	 * Assignment operator. 
	 */
	template <uint32 NumTexCoordsT, bool bExtraBoneInfluencesT>
	FSkeletalMeshVertexBuffer& operator=(const TArray< TGPUSkinVertexFloat16Uvs<NumTexCoordsT, bExtraBoneInfluencesT> >& InVertices)
	{
		check(!bUseFullPrecisionUVs);
		check(bExtraBoneInfluences == bExtraBoneInfluencesT);
		AllocateData();

		*(TSkeletalMeshVertexData< TGPUSkinVertexFloat16Uvs<NumTexCoordsT, bExtraBoneInfluencesT> >*)VertexData = InVertices;


		Data = VertexData->GetDataPointer();
		Stride = VertexData->GetStride();
		NumVertices = VertexData->GetNumVertices();

		return *this;
	}

	/**
	 * Assignment operator.  
	 */
	template <uint32 NumTexCoordsT, bool bExtraBoneInfluencesT>
	FSkeletalMeshVertexBuffer& operator=(const TArray< TGPUSkinVertexFloat32Uvs<NumTexCoordsT, bExtraBoneInfluencesT> >& InVertices)
	{
		check(bUseFullPrecisionUVs);
		check(bExtraBoneInfluences == bExtraBoneInfluencesT);
		AllocateData();

		*(TSkeletalMeshVertexData< TGPUSkinVertexFloat32Uvs<NumTexCoordsT, bExtraBoneInfluencesT> >*)VertexData = InVertices;

		Data = VertexData->GetDataPointer();
		Stride = VertexData->GetStride();
		NumVertices = VertexData->GetNumVertices();

		return *this;
	}

	/**
	* Convert the existing data in this mesh from 16 bit to 32 bit UVs.
	* Without rebuilding the mesh (loss of precision)
	*/
	template<uint32 NumTexCoordsT>
	void ConvertToFullPrecisionUVs()
	{
		if (bExtraBoneInfluences)
		{
			ConvertToFullPrecisionUVsTyped<NumTexCoordsT, true>();
		}
		else
		{
			ConvertToFullPrecisionUVsTyped<NumTexCoordsT, false>();
		}
	}

private:
	/** InfluenceBones/InfluenceWeights byte order has been swapped */
	bool bInfluencesByteSwapped;
	/** Corresponds to USkeletalMesh::bUseFullPrecisionUVs. if true then 32 bit UVs are used */
	bool bUseFullPrecisionUVs;
	/** true if this vertex buffer will be used with CPU skinning. Resource arrays are set to cpu accessible if this is true */
	bool bNeedsCPUAccess;
	/** Position data has already been packed. Used during cooking to avoid packing twice. */
	bool bProcessedPackedPositions;
	/** Has extra bone influences per Vertex, which means using a different TGPUSkinVertexBase */
	bool bExtraBoneInfluences;
	/** The vertex data storage type */
	FSkeletalMeshVertexDataInterface* VertexData;
	/** The cached vertex data pointer. */
	uint8* Data;
	/** The cached vertex stride. */
	uint32 Stride;
	/** The cached number of vertices. */
	uint32 NumVertices;
	/** The number of unique texture coordinate sets in this buffer */
	uint32 NumTexCoords;

	/** The origin of Mesh **/
	FVector MeshOrigin;
	/** The scale of Mesh **/
	FVector MeshExtension;

	/** 
	* Allocates the vertex data storage type. Based on UV precision needed
	*/
	void AllocateData();	

	/** 
	* Copy the contents of the source vertex to the destination vertex in the buffer 
	*
	* @param VertexIndex - index into the vertex buffer
	* @param SrcVertex - source vertex to copy from
	*/
	void SetVertexSlow(uint32 VertexIndex,const FSoftSkinVertex& SrcVertex)
	{
		if (bExtraBoneInfluences)
		{
			SetVertexFast<true>(VertexIndex, SrcVertex);
		}
		else
		{
			SetVertexFast<false>(VertexIndex, SrcVertex);
		}
	}

	/** Helper for concrete types */
	template <bool bUsesExtraBoneInfluences>
	void SetVertexFast(uint32 VertexIndex,const FSoftSkinVertex& SrcVertex);

	/** Helper for concrete types */
	template<uint32 NumTexCoordsT, bool bUsesExtraBoneInfluences>
	void ConvertToFullPrecisionUVsTyped();
};

/** 
 * A vertex buffer for holding skeletal mesh per vertex color information only. 
 * This buffer sits along side FSkeletalMeshVertexBuffer in each skeletal mesh lod
 */
class FSkeletalMeshVertexColorBuffer : public FVertexBuffer
{
public:
	/**
	 * Constructor
	 */
	ENGINE_API FSkeletalMeshVertexColorBuffer();

	/**
	 * Destructor
	 */
	ENGINE_API virtual ~FSkeletalMeshVertexColorBuffer();

	/**
	 * Assignment. Assumes that vertex buffer will be rebuilt 
	 */
	ENGINE_API FSkeletalMeshVertexColorBuffer& operator=(const FSkeletalMeshVertexColorBuffer& Other);
	
	/**
	 * Constructor (copy)
	 */
	ENGINE_API FSkeletalMeshVertexColorBuffer(const FSkeletalMeshVertexColorBuffer& Other);

	/** 
	 * Delete existing resources 
	 */
	void CleanUp();

	/**
	 * Initializes the buffer with the given vertices.
	 * @param InVertices - The vertices to initialize the buffer with.
	 */
	void Init(const TArray<FSoftSkinVertex>& InVertices);

	/**
	 * Serializer for this class
	 * @param Ar - archive to serialize to
	 * @param B - data to serialize
	 */
	friend FArchive& operator<<(FArchive& Ar,FSkeletalMeshVertexColorBuffer& VertexBuffer);

	// FRenderResource interface.

	/**
	 * Initialize the RHI resource for this vertex buffer
	 */
	virtual void InitRHI();

	/**
	 * @return text description for the resource type
	 */
	virtual FString GetFriendlyName() const;

	/** 
	 * @return number of vertices in this vertex buffer
	 */
	FORCEINLINE uint32 GetNumVertices() const
	{
		return NumVertices;
	}

	/** 
	* @return cached stride for vertex data type for this vertex buffer
	*/
	FORCEINLINE uint32 GetStride() const
	{
		return Stride;
	}
	/** 
	* @return total size of data in resource array
	*/
	FORCEINLINE uint32 GetVertexDataSize() const
	{
		return NumVertices * Stride;
	}

	/**
	 * @return the vertex color for the specified index
	 */
	FORCEINLINE const FColor& VertexColor( uint32 VertexIndex ) const
	{
		checkSlow( VertexIndex < GetNumVertices() );
		uint8* VertBase = Data + VertexIndex * Stride;
		return ((FGPUSkinVertexColor*)(VertBase))->VertexColor;
	}
private:
	/** The vertex data storage type */
	FSkeletalMeshVertexDataInterface* VertexData;
	/** The cached vertex data pointer. */
	uint8* Data;
	/** The cached vertex stride. */
	uint32 Stride;
	/** The cached number of vertices. */
	uint32 NumVertices;

	/** 
	 * Allocates the vertex data storage type
	 */
	void AllocateData();	

	/** 
	 * Copy the contents of the source color to the destination vertex in the buffer 
	 *
	 * @param VertexIndex - index into the vertex buffer
	 * @param SrcColor - source color to copy from
	 */
	void SetColor(uint32 VertexIndex,const FColor& SrcColor);
};

/** 
 * A vertex buffer for holding skeletal mesh per APEX cloth information only. 
 * This buffer sits along side FSkeletalMeshVertexBuffer in each skeletal mesh lod
 */
class FSkeletalMeshVertexAPEXClothBuffer : public FVertexBuffer
{
public:
	/**
	 * Constructor
	 */
	ENGINE_API FSkeletalMeshVertexAPEXClothBuffer();

	/**
	 * Destructor
	 */
	ENGINE_API virtual ~FSkeletalMeshVertexAPEXClothBuffer();

	/**
	 * Assignment. Assumes that vertex buffer will be rebuilt 
	 */
	ENGINE_API FSkeletalMeshVertexAPEXClothBuffer& operator=(const FSkeletalMeshVertexAPEXClothBuffer& Other);
	
	/**
	 * Constructor (copy)
	 */
	ENGINE_API FSkeletalMeshVertexAPEXClothBuffer(const FSkeletalMeshVertexAPEXClothBuffer& Other);

	/** 
	 * Delete existing resources 
	 */
	void CleanUp();

	/**
	 * Initializes the buffer with the given vertices.
	 * @param InVertices - The vertices to initialize the buffer with.
	 */
	void Init(const TArray<FApexClothPhysToRenderVertData>& InMappingData);

	/**
	 * Serializer for this class
	 * @param Ar - archive to serialize to
	 * @param B - data to serialize
	 */
	friend FArchive& operator<<(FArchive& Ar,FSkeletalMeshVertexAPEXClothBuffer& VertexBuffer);

	// FRenderResource interface.

	/**
	 * Initialize the RHI resource for this vertex buffer
	 */
	virtual void InitRHI();

	/**
	 * @return text description for the resource type
	 */
	virtual FString GetFriendlyName() const;

	// Vertex data accessors.
	FORCEINLINE FApexClothPhysToRenderVertData& MappingData(uint32 VertexIndex)
	{
		checkSlow(VertexIndex < GetNumVertices());
		return *((FApexClothPhysToRenderVertData*)(Data + VertexIndex * Stride));
	}
	FORCEINLINE const FApexClothPhysToRenderVertData& MappingData(uint32 VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		return *((FApexClothPhysToRenderVertData*)(Data + VertexIndex * Stride));
	}

	/** 
	 * @return number of vertices in this vertex buffer
	 */
	FORCEINLINE uint32 GetNumVertices() const
	{
		return NumVertices;
	}

	/** 
	* @return cached stride for vertex data type for this vertex buffer
	*/
	FORCEINLINE uint32 GetStride() const
	{
		return Stride;
	}
	/** 
	* @return total size of data in resource array
	*/
	FORCEINLINE uint32 GetVertexDataSize() const
	{
		return NumVertices * Stride;
	}

private:
	/** The vertex data storage type */
	FSkeletalMeshVertexDataInterface* VertexData;
	/** The cached vertex data pointer. */
	uint8* Data;
	/** The cached vertex stride. */
	uint32 Stride;
	/** The cached number of vertices. */
	uint32 NumVertices;

	/** 
	 * Allocates the vertex data storage type
	 */
	void AllocateData();	
};

//////////////////////////////////////////////////////////////////////////
// DEPRECATED (can remove when min ue4 version > VER_UE4_REMOVE_EXTRA_SKELMESH_VERTEX_INFLUENCES)

struct FInfluenceWeights_DEPRECATED
{
	uint32 InfluenceWeightsDWORD; 

	friend FArchive& operator<<( FArchive& Ar, FInfluenceWeights_DEPRECATED& W )
	{
		return Ar << W.InfluenceWeightsDWORD;
	}
};

struct FInfluenceBones_DEPRECATED
{
	uint32 InfluenceBonesDWORD; 

	friend FArchive& operator<<( FArchive& Ar, FInfluenceBones_DEPRECATED& W )
	{
		return Ar << W.InfluenceBonesDWORD;
	}

};

struct FVertexInfluence_DEPRECATED
{
	FInfluenceWeights_DEPRECATED Weights;
	FInfluenceBones_DEPRECATED Bones;

	friend FArchive& operator<<( FArchive& Ar, FVertexInfluence_DEPRECATED& W )
	{
		return Ar << W.Weights << W.Bones;
	}
};

class FSkeletalMeshVertexInfluences_DEPRECATED : public FVertexBuffer
{
public:
	TResourceArray<FVertexInfluence_DEPRECATED, VERTEXBUFFER_ALIGNMENT> Influences;
	TMap<struct FBoneIndexPair, TArray<uint32> > VertexInfluenceMapping;
	TArray<FSkelMeshSection> Sections;
	TArray<FSkelMeshChunk> Chunks;
	TArray<FBoneIndexType> RequiredBones;
	TArray<int32> CustomLeftRightSectionMap;

	FSkeletalMeshVertexInfluences_DEPRECATED() : Influences(true) {}


	friend FArchive& operator<<( FArchive& Ar, FSkeletalMeshVertexInfluences_DEPRECATED& W )
	{
		Ar << W.Influences;

		Ar << W.VertexInfluenceMapping;
		Ar << W.Sections;
		Ar << W.Chunks;
		Ar << W.RequiredBones;
		uint8 Usage = 0;
		Ar << Usage;

		return Ar;
	}
};

//////////////////////////////////////////////////////////////////////////


struct FMultiSizeIndexContainerData
{
	TArray<uint32> Indices;
	uint32 DataTypeSize;
};

/**
 * Skeletal mesh index buffers are 16 bit by default and 32 bit when called for.
 * This class adds a level of abstraction on top of the index buffers so that we can treat them all as 32 bit.
 */
class FMultiSizeIndexContainer
{
public:
	FMultiSizeIndexContainer()
	: DataTypeSize(sizeof(uint16))
	, IndexBuffer(NULL)
	{
	}

	ENGINE_API ~FMultiSizeIndexContainer();
	
	/**
	 * Initialize the index buffer's render resources.
	 */
	void InitResources();

	/**
	 * Releases the index buffer's render resources.
	 */	
	void ReleaseResources();

	/**
	 * Serialization.
	 * @param Ar				The archive with which to serialize.
	 * @param bNeedsCPUAccess	If true, the loaded data will remain in CPU memory
	 *							even after the RHI index buffer has been initialized.
	 */
	void Serialize(FArchive& Ar, bool bNeedsCPUAccess);

	/**
	 * Creates a new index buffer
	 */
	ENGINE_API void CreateIndexBuffer(uint8 DataTypeSize);

	/**
	 * Repopulates the index buffer
	 */
	ENGINE_API void RebuildIndexBuffer( const FMultiSizeIndexContainerData& InData );

	/**
	 * Returns a 32 bit version of the index buffer
	 */
	ENGINE_API void GetIndexBuffer( TArray<uint32>& OutArray ) const;

	/**
	 * Populates the index buffer with a new set of indices
	 */
	ENGINE_API void CopyIndexBuffer(const TArray<uint32>& NewArray);

	bool IsIndexBufferValid() const { return IndexBuffer != NULL; }

	/**
	 * Accessors
	 */
	uint8 GetDataTypeSize() const { return DataTypeSize; }
	FRawStaticIndexBuffer16or32Interface* GetIndexBuffer() 
	{ 
		check( IndexBuffer != NULL );
		return IndexBuffer; 
	}
	const FRawStaticIndexBuffer16or32Interface* GetIndexBuffer() const
	{ 
		check( IndexBuffer != NULL );
		return IndexBuffer; 
	}
	
#if WITH_EDITOR
	/**
	 * Retrieves index buffer related data
	 */
	ENGINE_API void GetIndexBufferData( FMultiSizeIndexContainerData& OutData ) const;
	
	ENGINE_API FMultiSizeIndexContainer(const FMultiSizeIndexContainer& Other);
	ENGINE_API FMultiSizeIndexContainer& operator=(const FMultiSizeIndexContainer& Buffer);
#endif

	friend FArchive& operator<<(FArchive& Ar, FMultiSizeIndexContainer& Buffer);

private:
	/** Size of the index buffer's index type (should be 2 or 4 bytes) */
	uint8 DataTypeSize;
	/** The vertex index buffer */
	FRawStaticIndexBuffer16or32Interface* IndexBuffer;
};

/**
* All data to define a certain LOD model for a skeletal mesh.
* All necessary data to render smooth-parts is in SkinningStream, SmoothVerts, SmoothSections and SmoothIndexbuffer.
* For rigid parts: RigidVertexStream, RigidIndexBuffer, and RigidSections.
*/
class FStaticLODModel
{
public:
	/** Sections. */
	TArray<FSkelMeshSection> Sections;

	/** The vertex chunks which make up this LOD. */
	TArray<FSkelMeshChunk> Chunks;

	/** 
	* Bone hierarchy subset active for this chunk.
	* This is a map between the bones index of this LOD (as used by the vertex structs) and the bone index in the reference skeleton of this SkeletalMesh.
	*/
	TArray<FBoneIndexType> ActiveBoneIndices;  
	
	/** 
	* Bones that should be updated when rendering this LOD. This may include bones that are not required for rendering.
	* All parents for bones in this array should be present as well - that is, a complete path from the root to each bone.
	* For bone LOD code to work, this array must be in strictly increasing order, to allow easy merging of other required bones.
	*/
	TArray<FBoneIndexType> RequiredBones;

	/** 
	* Rendering data.
	*/
	FMultiSizeIndexContainer	MultiSizeIndexContainer; 
	uint32						Size;
	uint32						NumVertices;
	/** The number of unique texture coordinate sets in this lod */
	uint32						NumTexCoords;

	/** Resources needed to render the model using PN-AEN */
	FMultiSizeIndexContainer	AdjacencyMultiSizeIndexContainer;

	/** static vertices from chunks for skinning on GPU */
	FSkeletalMeshVertexBuffer	VertexBufferGPUSkin;
	
	/** A buffer for vertex colors */
	FSkeletalMeshVertexColorBuffer	ColorVertexBuffer;

	/** A buffer for APEX cloth mesh-mesh mapping */
	FSkeletalMeshVertexAPEXClothBuffer	APEXClothVertexBuffer;

	/** Editor only data: array of the original point (wedge) indices for each of the vertices in a FStaticLODModel */
	FIntBulkData				RawPointIndices;
	FWordBulkData				LegacyRawPointIndices;

	/** Mapping from final mesh vertex index to raw import vertex index. Needed for vertex animation, which only stores positions for import verts. */
	TArray<int32>				MeshToImportVertexMap;
	/** The max index in MeshToImportVertexMap, ie. the number of imported (raw) verts. */
	int32						MaxImportVertex;

	/**
	* Initialize the LOD's render resources.
	*
	* @param Parent Parent mesh
	*/
	void InitResources(bool bNeedsVertexColors);

	/**
	* Releases the LOD's render resources.
	*/
	ENGINE_API void ReleaseResources();

	/**
	* Releases the LOD's CPU render resources.
	*/
	void ReleaseCPUResources();

	/** Constructor (default) */
	FStaticLODModel()
	:	Size(0)
	,	NumVertices(0)
	{
	}

	/**
	 * Special serialize function passing the owning UObject along as required by FUnytpedBulkData
	 * serialization.
	 *
	 * @param	Ar		Archive to serialize with
	 * @param	Owner	UObject this structure is serialized within
	 * @param	Idx		Index of current array entry being serialized
	 */
	void Serialize( FArchive& Ar, UObject* Owner, int32 Idx );

	/**
	* Fill array with vertex position and tangent data from skel mesh chunks.
	*
	* @param Vertices Array to fill.
	*/
	ENGINE_API void GetVertices(TArray<FSoftSkinVertex>& Vertices) const;

	/**
	* Fill array with APEX cloth mapping data.
	*
	* @param MappingData Array to fill.
	*/
	void GetApexClothMappingData(TArray<FApexClothPhysToRenderVertData>& MappingData) const;

	/** Flags used when building vertex buffers. */
	struct EVertexFlags
	{
		enum
		{
			None = 0x0,
			UseFullPrecisionUVs = 0x1,
			HasVertexColors = 0x2
		};
	};

	/**
	 * Initialize vertex buffers from skel mesh chunks.
	 * @param BuildFlags See EVertexFlags.
	 */
	void BuildVertexBuffers(uint32 VertexFlags);

	/** Utility function for returning total number of faces in this LOD. */
	ENGINE_API int32 GetTotalFaces() const;

	/** Utility for finding the chunk that a particular vertex is in. */
	ENGINE_API void GetChunkAndSkinType(int32 InVertIndex, int32& OutChunkIndex, int32& OutVertIndex, bool& bOutSoftVert, bool& bOutHasExtraBoneInfluences) const;

	/** Sort the triangles with the specified sorting method */
	ENGINE_API void SortTriangles( FVector SortCenter, bool bUseSortCenter, int32 SectionIndex, ETriangleSortOption NewTriangleSorting );

	/**
	* @return true if any chunks have cloth data.
	*/
	bool HasApexClothData() const
	{
		for( int32 ChunkIdx=0;ChunkIdx<Chunks.Num();ChunkIdx++ )
		{
			if( Chunks[ChunkIdx].HasApexClothData() )
			{
				return true;
			}
		}
		return false;
	}

	int32 GetApexClothCunkIndex(TArray<int32>& ChunkIndices) const
	{
		ChunkIndices.Empty();

		uint32 Count = 0;
		for( int32 ChunkIdx=0;ChunkIdx<Chunks.Num();ChunkIdx++ )
		{
			if( Chunks[ChunkIdx].HasApexClothData() )
			{
				ChunkIndices.Add(ChunkIdx);
				Count++;
			}
		}
		return Count;
	}

	bool HasApexClothData(int32 SectionIndex) const
	{
		return Chunks[Sections[SectionIndex].ChunkIndex].HasApexClothData();
	}

	int32 NumNonClothingSections() const
	{
		int32 NumSections = Sections.Num();

		for(int32 i=0; i < NumSections; i++)
		{
			//if found the start section of clothing, return that index which means # of non-clothing
			if((Sections[i].bDisabled == false)
			&& (Sections[i].CorrespondClothSectionIndex >= 0))
			{
				return i;
			}
		}

		return NumSections;
	}

	bool DoesVertexBufferHaveExtraBoneInfluences() const
	{
		return VertexBufferGPUSkin.HasExtraBoneInfluences();
	}

	bool DoChunksNeedExtraBoneInfluences() const
	{
		for (int32 ChunkIdx = 0; ChunkIdx < Chunks.Num(); ++ChunkIdx)
		{
			if (Chunks[ChunkIdx].HasExtraBoneInfluences())
			{
				return true;
			}
		}

		return false;
	}
};

/**
 * Resources required to render a skeletal mesh.
 */
class FSkeletalMeshResource
{
public:
	/** Per-LOD render data. */
	TIndirectArray<FStaticLODModel> LODModels;

	/** Default constructor. */
	FSkeletalMeshResource();

	/** Initializes rendering resources. */
	void InitResources(bool bNeedsVertexColors);
	
	/** Releases rendering resources. */
	ENGINE_API void ReleaseResources();

	/** Serialize to/from the specified archive.. */
	void Serialize(FArchive& Ar, USkeletalMesh* Owner);

	/**
	 * Computes the maximum number of bones per chunk used to render this mesh.
	 */
	int32 GetMaxBonesPerChunk() const;

	/** Returns true if this resource must be skinned on the CPU for the given feature level. */
	bool RequiresCPUSkinning(ERHIFeatureLevel::Type FeatureLevel) const;

	/** Returns true if there are more than MAX_INFLUENCES_PER_STREAM influences per vertex. */
	bool HasExtraBoneInfluences() const;

private:
	/** True if the resource has been initialized. */
	bool bInitialized;
};

/**
 *	Contains the vertices that are most dominated by that bone. Vertices are in Bone space.
 *	Not used at runtime, but useful for fitting physics assets etc.
 */
struct FBoneVertInfo
{
	// Invariant: Arrays should be same length!
	TArray<FVector>	Positions;
	TArray<FVector>	Normals;
};



/*-----------------------------------------------------------------------------
FSkeletalMeshSceneProxy
-----------------------------------------------------------------------------*/
class USkinnedMeshComponent;

/**
 * A skeletal mesh component scene proxy.
 */
class ENGINE_API FSkeletalMeshSceneProxy : public FPrimitiveSceneProxy
{
public:
	/** 
	 * Constructor. 
	 * @param	Component - skeletal mesh primitive being added
	 */
	FSkeletalMeshSceneProxy(const USkinnedMeshComponent* Component, FSkeletalMeshResource* InSkelMeshResource);

	// FPrimitiveSceneProxy interface.
#if WITH_EDITOR
	virtual HHitProxy* CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies) OVERRIDE;
#endif
	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI, const FSceneView* View) OVERRIDE;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) OVERRIDE;
	virtual bool CanBeOccluded() const OVERRIDE;
	virtual void PreRenderView(const FSceneViewFamily* ViewFamily, const uint32 VisibilityMap, int32 FrameNumber) OVERRIDE;
	
	/**
	* Draw only the section of the material ID given of the scene proxy as a dynamic element
	*
	* @param	PDI - draw interface to render to
	* @param	View - current view
	* @param 	ForceLOD - Force this LOD. If -1, use current LOD of mesh. 
	* @param	InMaterial - which material section to draw
	*/
	virtual void DrawDynamicElementsByMaterial(FPrimitiveDrawInterface* PDI,const FSceneView* View, int32 ForceLOD, int32 InMaterial);

	/**
	 * Returns the world transform to use for drawing.
	 * @param View - Current view
	 * @param OutLocalToWorld - Will contain the local-to-world transform when the function returns.
	 * @param OutWorldToLocal - Will contain the world-to-local transform when the function returns.
	 */
	virtual void GetWorldMatrices( const FSceneView* View, FMatrix& OutLocalToWorld, FMatrix& OutWorldToLocal );

	/** Util for getting LOD index currently used by this SceneProxy. */
	int32 GetCurrentLODIndex();


	/** 
	 * Render a coordinate system indicator
	 */
	void RenderAxisGizmo(FPrimitiveDrawInterface* PDI, FTransform& Transform);

	/** 
	 * Render physics asset for debug display
	 */
	void DebugDrawPhysicsAsset(FPrimitiveDrawInterface* PDI,const FSceneView* View);

	virtual uint32 GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
	uint32 GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() + LODSections.GetAllocatedSize() ); }

	/**
	* Updates morph material usage for materials referenced by each LOD entry
	*
	* @param bNeedsMorphUsage - true if the materials used by this skeletal mesh need morph target usage
	*/
	void UpdateMorphMaterialUsage_GameThread(bool bNeedsMorphUsage);

	friend class FSkeletalMeshSectionIter;

protected:
	AActor* Owner;
	class FSkeletalMeshObject* MeshObject;
	class FSkeletalMeshResource* SkelMeshResource;

	/** The points to the skeletal mesh and physics assets are purely for debug purposes. Access is NOT thread safe! */
	const USkeletalMesh* SkeletalMeshForDebug;
	class UPhysicsAsset* PhysicsAssetForDebug;

	/** data copied for rendering */
	FColor LevelColor;
	FColor PropertyColor;
	uint32 bForceWireframe : 1;
	uint32 bIsCPUSkinned : 1;
	uint32 bCanHighlightSelectedSections : 1;
	FMaterialRelevance MaterialRelevance;

	/** info for section element in an LOD */
	struct FSectionElementInfo
	{
		FSectionElementInfo(UMaterialInterface* InMaterial, bool bInEnableShadowCasting, int32 InUseMaterialIndex)
		: Material( InMaterial )
		, bEnableShadowCasting( bInEnableShadowCasting )
		, UseMaterialIndex( InUseMaterialIndex )
#if WITH_EDITOR
		, HitProxy(NULL)
#endif
		{}
		
		UMaterialInterface* Material;
		
		/** Whether shadow casting is enabled for this section. */
		bool bEnableShadowCasting;
		
		/** Index into the materials array of the skel mesh or the component after LOD mapping */
		int32 UseMaterialIndex;

#if WITH_EDITOR
		/** The editor needs to be able to individual sub-mesh hit detection, so we store a hit proxy on each mesh. */
		HHitProxy* HitProxy;
#endif
	};

	/** Section elements for a particular LOD */
	struct FLODSectionElements
	{
		TArray<FSectionElementInfo> SectionElements;
	};
	
	/** Array of section elements for each LOD */
	TArray<FLODSectionElements> LODSections;

	/** Set of materials used by this scene proxy, safe to access from the game thread. */
	TSet<UMaterialInterface*> MaterialsInUse_GameThread;
	bool bMaterialsNeedMorphUsage_GameThread;
	
	/** The color used by the wireframe mesh overlay mode */
	FColor WireframeOverlayColor;

	/**
	* Draw only the section of the scene proxy as a dynamic element
	* This is to avoid redundant code of two functions (DrawDynamicElementsByMaterial & DrawDynamicElements)
	* 
	* @param	PDI - draw interface to render to
	* @param	View - current view
	* @param	const FStaticLODModel& LODModel - LODModel 
	* @param	const FSkelMeshSection& Section - Section
	* @param	const FSkelMeshChunk& Chunk - Chunk
	* @param	const FSectionElementInfo& SectionElementInfo - SectionElementInfo - material ID
	*/
	void DrawDynamicElementsSection(FPrimitiveDrawInterface* PDI,const FSceneView* View,
		const FStaticLODModel& LODModel, const int32 LODIndex, const FSkelMeshSection& Section, 
		const FSkelMeshChunk& Chunk, const FSectionElementInfo& SectionElementInfo, const FTwoVectors& CustomLeftRightVectors );

};
