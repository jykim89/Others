// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	StaticMeshDrawList.h: Static mesh draw list definition.
=============================================================================*/

#ifndef __STATICMESHDRAWLIST_H__
#define __STATICMESHDRAWLIST_H__

/** Base class of the static draw list, used when comparing draw lists and the drawing policy type is not necessary. */
class FStaticMeshDrawListBase
{
public:

	static SIZE_T TotalBytesUsed;
};

/**
 * Statistics for a static mesh draw list.
 */
struct FDrawListStats
{
	int32 NumMeshes;
	int32 NumDrawingPolicies;
	int32 MedianMeshesPerDrawingPolicy;
	int32 MaxMeshesPerDrawingPolicy;
	int32 NumSingleMeshDrawingPolicies;
};

/** Fields in the key used to sort mesh elements in a draw list. */
struct FDrawListSortKeyFields
{
	uint64 MeshElementIndex : 16;
	uint64 DepthBits : 16;
	uint64 DrawingPolicyIndex : 16;
	uint64 DrawingPolicyDepthBits : 15;
	uint64 bBackground : 1;
};

/** Key for sorting mesh elements. */
union FDrawListSortKey
{
	FDrawListSortKeyFields Fields;
	uint64 PackedInt;
};

FORCEINLINE bool operator<(FDrawListSortKey A, FDrawListSortKey B)
{
	return A.PackedInt < B.PackedInt;
}

/** Builds a sort key. */
inline FDrawListSortKey GetSortKey(bool bBackground, float BoundsRadius, float DrawingPolicyDistance, int32 DrawingPolicyIndex, float Distance, int32 MeshElementIndex)
{
	union FFloatToInt { float F; uint32 I; };
	FFloatToInt F2I;

	FDrawListSortKey Key;
	Key.Fields.bBackground = bBackground || BoundsRadius > HALF_WORLD_MAX/4.0f;
	F2I.F = Distance;
	Key.Fields.DrawingPolicyDepthBits = ((-int32(F2I.I >> 31) | 0x80000000) ^ F2I.I) >> 17;
	Key.Fields.DrawingPolicyIndex = DrawingPolicyIndex;
	F2I.F = Distance;
	Key.Fields.DepthBits = ((-int32(F2I.I >> 31) | 0x80000000) ^ F2I.I) >> 16;
	Key.Fields.MeshElementIndex = MeshElementIndex;
	return Key;
}

/**
 * A set of static meshs, each associated with a mesh drawing policy of a particular type.
 * @param DrawingPolicyType - The drawing policy type used to draw mesh in this draw list.
 * @param HashSize - The number of buckets to use in the drawing policy hash.
 */
template<typename DrawingPolicyType>
class TStaticMeshDrawList : public FStaticMeshDrawListBase, public FRenderResource
{
public:
	typedef typename DrawingPolicyType::ElementDataType ElementPolicyDataType;

private:

	/** A handle to an element in the draw list.  Used by FStaticMesh to keep track of draw lists containing the mesh. */
	class FElementHandle : public FStaticMesh::FDrawListElementLink
	{
	public:

		/** Initialization constructor. */
		FElementHandle(TStaticMeshDrawList* InStaticMeshDrawList,FSetElementId InSetId,int32 InElementIndex):
		  StaticMeshDrawList(InStaticMeshDrawList)
		  ,SetId(InSetId)
		  ,ElementIndex(InElementIndex)
		{
		}

		virtual bool IsInDrawList(const FStaticMeshDrawListBase* DrawList) const
		{
			return DrawList == StaticMeshDrawList;
		}
		// FAbstractDrawListElementLink interface.
		virtual void Remove();

	private:
		TStaticMeshDrawList* StaticMeshDrawList;
		FSetElementId SetId;
		int32 ElementIndex;
	};

	/**
	 * This structure stores the info needed for visibility culling a static mesh element.
	 * Stored separately to avoid bringing the other info about non-visible meshes into the cache.
	 */
	struct FElementCompact
	{
		int32 MeshId;
		FElementCompact() {}
		FElementCompact(int32 InMeshId)
		: MeshId(InMeshId)
		{}
	};

	struct FElement
	{
		ElementPolicyDataType PolicyData;
		FStaticMesh* Mesh;
		FBoxSphereBounds Bounds;
		bool bBackground;
		TRefCountPtr<FElementHandle> Handle;

		/** Default constructor. */
		FElement():
			Mesh(NULL)
		{}

		/** Minimal initialization constructor. */
		FElement(
			FStaticMesh* InMesh,
			const ElementPolicyDataType& InPolicyData,
			TStaticMeshDrawList* StaticMeshDrawList,
			FSetElementId SetId,
			int32 ElementIndex
			):
			PolicyData(InPolicyData),
			Mesh(InMesh),
			Handle(new FElementHandle(StaticMeshDrawList,SetId,ElementIndex))
		{
			// Cache bounds so we can use them for sorting quickly, without having to dereference the proxy
			Bounds = Mesh->PrimitiveSceneInfo->Proxy->GetBounds();
			bBackground = Mesh->PrimitiveSceneInfo->Proxy->TreatAsBackgroundForOcclusion();
		}

		/** Destructor. */
		~FElement()
		{
			if(Mesh)
			{
				Mesh->UnlinkDrawList(Handle);
			}
		}
	};

	/** A set of draw list elements with the same drawing policy. */
	struct FDrawingPolicyLink
	{
		/** The elements array and the compact elements array are always synchronized */
		TArray<FElementCompact>		CompactElements; 
		TArray<FElement>			Elements;
		DrawingPolicyType			DrawingPolicy;
		FBoundShaderStateRHIRef		BoundShaderState;
		ERHIFeatureLevel::Type		FeatureLevel;

		/** Used when sorting policy links */
		FSphere						CachedBoundingSphere;

		/** The id of this link in the draw list's set of drawing policy links. */
		FSetElementId SetId;

		TStaticMeshDrawList* DrawList;

		/** Initialization constructor. */
		FDrawingPolicyLink(TStaticMeshDrawList* InDrawList, const DrawingPolicyType& InDrawingPolicy, ERHIFeatureLevel::Type InFeatureLevel) :
			DrawingPolicy(InDrawingPolicy),
			FeatureLevel(InFeatureLevel),
			DrawList(InDrawList)
		{
			CreateBoundShaderState();
		}

		SIZE_T GetSizeBytes() const
		{
			return sizeof(*this) + CompactElements.GetAllocatedSize() + Elements.GetAllocatedSize();
		}

		void ReleaseBoundShaderState()
		{
			BoundShaderState.SafeRelease();
		}

		void CreateBoundShaderState()
		{
			BoundShaderState = DrawingPolicy.CreateBoundShaderState(FeatureLevel);
		}
	};

	/** Functions to extract the drawing policy from FDrawingPolicyLink as a key for TSet. */
	struct FDrawingPolicyKeyFuncs : BaseKeyFuncs<FDrawingPolicyLink,DrawingPolicyType>
	{
		static const DrawingPolicyType& GetSetKey(const FDrawingPolicyLink& Link)
		{
			return Link.DrawingPolicy;
		}

		static bool Matches(const DrawingPolicyType& A,const DrawingPolicyType& B)
		{
			return A.Matches(B);
		}

		static uint32 GetKeyHash(const DrawingPolicyType& DrawingPolicy)
		{
			return DrawingPolicy.GetTypeHash();
		}
	};

	/**
	* Draws a single FElement
	* @param View - The view of the meshes to render.
	* @param Element - The mesh element
	* @param BatchElementMask - Visibility bitmask for element's batch elements.
	* @param DrawingPolicyLink - the drawing policy link
	* @param bDrawnShared - determines whether to draw shared 
	*/
	void DrawElement(const FViewInfo& View, const FElement& Element, uint64 BatchElementMask, FDrawingPolicyLink* DrawingPolicyLink, bool &bDrawnShared);

public:

	/**
	 * Adds a mesh to the draw list.
	 * @param Mesh - The mesh to add.
	 * @param PolicyData - The drawing policy data for the mesh.
	 * @param InDrawingPolicy - The drawing policy to use to draw the mesh.
	 * @param InFeatureLevel - The feature level of the scene we're rendering
	 */
	void AddMesh(
		FStaticMesh* Mesh,
		const ElementPolicyDataType& PolicyData,
		const DrawingPolicyType& InDrawingPolicy,
		ERHIFeatureLevel::Type InFeatureLevel
		);

	/**
	 * Draws only the static meshes which are in the visibility map.
	 * @param View - The view of the meshes to render.
	 * @param StaticMeshVisibilityMap - An map from FStaticMesh::Id to visibility state.
	 * @return True if any static meshes were drawn.
	 */
	bool DrawVisible(const FViewInfo& View, const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap);

	/**
	 * Draws only the static meshes which are in the visibility map.
	 * @param View - The view of the meshes to render.
	 * @param StaticMeshVisibilityMap - An map from FStaticMesh::Id to visibility state.
	 * @param BatchVisibilityArray - An array of batch element visibility bitmasks.
	 * @return True if any static meshes were drawn.
	 */
	bool DrawVisible(const FViewInfo& View, const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap, const TArray<uint64,SceneRenderingAllocator>& BatchVisibilityArray);

	/**
	 * Draws only the static meshes which are in the visibility map, sorted front-to-back.
	 * @param View - The view of the meshes to render.
	 * @param StaticMeshVisibilityMap - An map from FStaticMesh::Id to visibility state.
	 * @param BatchVisibilityArray - An array of batch element visibility bitmasks.
	 * @param MaxToDraw - The maximum number of meshes to be drawn.
	 * @return The number of static meshes drawn.
	 */
	int32 DrawVisibleFrontToBack(const FViewInfo& View, const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap, const TArray<uint64,SceneRenderingAllocator>& BatchVisibilityArray, int32 MaxToDraw);

	/** Sorts OrderedDrawingPolicies front to back. */
	void SortFrontToBack(FVector ViewPosition);

	/** Builds a list of primitives that use the given materials in this static draw list. */
	void GetUsedPrimitivesBasedOnMaterials(const TArray<const FMaterial*>& Materials, TArray<FPrimitiveSceneInfo*>& PrimitivesToUpdate);

	/**
	 * Shifts all meshes bounds by an arbitrary delta.
	 * Called on world origin changes
	 * @param InOffset - The delta to shift by
	 */
	void ApplyWorldOffset(FVector InOffset);

	/**
	 * @return total number of meshes in all draw policies
	 */
	int32 NumMeshes() const;

	TStaticMeshDrawList();
	~TStaticMeshDrawList();

	// FRenderResource interface.
	virtual void ReleaseRHI();

	/** Sorts OrderedDrawingPolicies front to back.  Relies on static variables SortDrawingPolicySet and SortViewPosition being set. */
	static int32 Compare(FSetElementId A, FSetElementId B);

	/** Computes statistics for this draw list. */
	FDrawListStats GetStats() const;

private:
	/** All drawing policies in the draw list, in rendering order. */
    TArray<FSetElementId> OrderedDrawingPolicies;
	
	typedef TSet<FDrawingPolicyLink,FDrawingPolicyKeyFuncs> TDrawingPolicySet;
	/** All drawing policy element sets in the draw list, hashed by drawing policy. */
	TDrawingPolicySet DrawingPolicySet;

	/** 
	 * Static variables for getting data into the Compare function.
	 * Ideally Sort would accept a non-static member function which would avoid having to go through globals.
	 */ 
	static TDrawingPolicySet* SortDrawingPolicySet;
	static FVector SortViewPosition;
};

/** Helper stuct for sorting */
template<typename DrawingPolicyType>
struct TCompareStaticMeshDrawList
{
	FORCEINLINE bool operator()( const FSetElementId& A, const FSetElementId& B ) const
	{
		// Use static Compare from TStaticMeshDrawList
		return TStaticMeshDrawList<DrawingPolicyType>::Compare( A, B ) < 0;
	}
};

#include "StaticMeshDrawList.inl"

#endif
