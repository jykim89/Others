// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LightSceneInfo.h: Light scene info definitions.
=============================================================================*/

#ifndef __LIGHTSCENEINFO_H__
#define __LIGHTSCENEINFO_H__

#include "StaticArray.h"

/**
 * The information needed to cull a light-primitive interaction.
 */
class FLightSceneInfoCompact
{
public:

	// must not be 0
	FLightSceneInfo* LightSceneInfo;
	// XYZ: origin, W:sphere radius
	VectorRegister BoundingSphereVector;
	FLinearColor Color;
	uint32 bCastDynamicShadow : 1;
	uint32 bCastStaticShadow : 1;
	uint32 bStaticLighting : 1;
	// e.g. LightType_Directional, LightType_Point or LightType_Spot
	uint32 LightType : LightType_NumBits;	

	/** Initializes the compact scene info from the light's full scene info. */
	void Init(FLightSceneInfo* InLightSceneInfo);

	/** Default constructor. */
	FLightSceneInfoCompact():
		LightSceneInfo(NULL)
	{}

	/** Initialization constructor. */
	FLightSceneInfoCompact(FLightSceneInfo* InLightSceneInfo)
	{
		Init(InLightSceneInfo);
	}

	/**
	 * Tests whether this light affects the given primitive.  This checks both the primitive and light settings for light relevance
	 * and also calls AffectsBounds.
	 *
	 * @param CompactPrimitiveSceneInfo - The primitive to test.
	 * @return True if the light affects the primitive.
	 */
	bool AffectsPrimitive(const FPrimitiveSceneInfoCompact& CompactPrimitiveSceneInfo) const;
};

/** Information for sorting lights. */
struct FSortedLightSceneInfo
{
	union
	{
		struct
		{
			// Note: the order of these members controls the light sort order!
			// Currently bShadowed is the MSB and LightType is LSB
			/** The type of light. */
			uint32 LightType : LightType_NumBits;
			/** Whether the light has a texture profile. */
			uint32 bTextureProfile : 1;
			/** Whether the light uses a light function. */
			uint32 bLightFunction : 1;
			/** Whether the light casts shadows. */
			uint32 bShadowed : 1;
		} Fields;
		/** Sort key bits packed into an integer. */
		int32 Packed;
	} SortKey;
	/** The compact light scene info. */
	FLightSceneInfoCompact SceneInfo;

	/** Initialization constructor. */
	explicit FSortedLightSceneInfo(const FLightSceneInfoCompact& InSceneInfo)
		: SceneInfo(InSceneInfo)
	{
		SortKey.Packed = 0;
	}
};

/** The type of the octree used by FScene to find lights. */
typedef TOctree<FLightSceneInfoCompact,struct FLightOctreeSemantics> FSceneLightOctree;

/**
 * The information used to render a light.  This is the rendering thread's mirror of the game thread's ULightComponent.
 */
class FLightSceneInfo : public FRenderResource
{
public:
	/** The light's scene proxy. */
	FLightSceneProxy* Proxy;

	/** The list of dynamic primitives affected by the light. */
	FLightPrimitiveInteraction* DynamicPrimitiveList;

	/** If bVisible == true, this is the index of the primitive in Scene->Lights. */
	int32 Id;

	/** The identifier for the primitive in Scene->PrimitiveOctree. */
	FOctreeElementId OctreeId;

	/** 
	 * Bound shader state used for rendering this light's contribution to the translucent lighting volume.
	 * This is mutable because it is cached on first use, possibly when const 
	 */
	mutable FBoundShaderStateRHIRef TranslucentInjectBoundShaderState[LightType_MAX][2][2][2];

	/** 
	 * Tracks the shader map that was used when the bound shader state was cached.
	 * This is needed to detect when the bound shader state should be invalidated due to a shader map switch,
	 * Which happens during async shader compiling. 
	 */
	mutable const FMaterialShaderMap* TranslucentInjectCachedShaderMaps[LightType_MAX][2][2][2];

	/** True if the light is built. */
	uint32 bPrecomputedLightingIsValid : 1;

	/** 
	 * True if the light is visible.  
	 * False if the light is invisible but still needed for previewing, which can only happen in the editor.
	 */
	uint32 bVisible : 1;

	/** 
	 * Whether to render light shaft bloom from this light. 
	 * For directional lights, the color around the light direction will be blurred radially and added back to the scene.
	 * for point lights, the color on pixels closer than the light's SourceRadius will be blurred radially and added back to the scene.
	 */
	uint32 bEnableLightShaftBloom : 1;

	/** Scales the additive color. */
	float BloomScale;

	/** Scene color must be larger than this to create bloom in the light shafts. */
	float BloomThreshold;

	/** Multiplies against scene color to create the bloom color. */
	FColor BloomTint;

	/** Number of dynamic interactions with statically lit primitives. */
	int32 NumUnbuiltInteractions;

	/** Cached value from the light proxy's virtual function, since it is checked many times during shadow setup. */
	bool bCreatePerObjectShadowsForDynamicObjects;

	/** The scene the light is in. */
	FScene* Scene;

	/** Initialization constructor. */
	FLightSceneInfo(FLightSceneProxy* InProxy, bool InbVisible);
	virtual ~FLightSceneInfo();

	/** Adds the light to the scene. */
	void AddToScene();

	/**
	 * If the light affects the primitive, create an interaction, and process children 
	 * @param LightSceneInfoCompact Compact representation of the light
	 * @param PrimitiveSceneInfoCompact Compact representation of the primitive
	 */
	void CreateLightPrimitiveInteraction(const FLightSceneInfoCompact& LightSceneInfoCompact, const FPrimitiveSceneInfoCompact& PrimitiveSceneInfoCompact);

	/** Removes the light from the scene. */
	void RemoveFromScene();

	/** Detaches the light from the primitives it affects. */
	void Detach();

	/** Octree bounds setup. */
	FORCEINLINE FBoxCenterAndExtent GetBoundingBox() const
	{
		const float Extent = Proxy->GetRadius();
		return FBoxCenterAndExtent(
			Proxy->GetOrigin(),
			FVector(Extent,Extent,Extent)
			);
	}

	bool ShouldRenderLight(const FViewInfo& View) const;

	/** Hash function. */
	friend uint32 GetTypeHash(const FLightSceneInfo* LightSceneInfo)
	{
		return (uint32)LightSceneInfo->Id;
	}

	// FRenderResource interface.
	virtual void ReleaseRHI();
};

/** Defines how the light is stored in the scene's light octree. */
struct FLightOctreeSemantics
{
	enum { MaxElementsPerLeaf = 16 };
	enum { MinInclusiveElementsPerNode = 7 };
	enum { MaxNodeDepth = 12 };

	typedef TInlineAllocator<MaxElementsPerLeaf> ElementAllocator;

	FORCEINLINE static FBoxCenterAndExtent GetBoundingBox(const FLightSceneInfoCompact& Element)
	{
		return Element.LightSceneInfo->GetBoundingBox();
	}

	FORCEINLINE static bool AreElementsEqual(const FLightSceneInfoCompact& A,const FLightSceneInfoCompact& B)
	{
		return A.LightSceneInfo == B.LightSceneInfo;
	}
	
	FORCEINLINE static void SetElementId(const FLightSceneInfoCompact& Element,FOctreeElementId Id)
	{
		Element.LightSceneInfo->OctreeId = Id;
	}

	FORCEINLINE static void ApplyOffset(FLightSceneInfoCompact& Element, FVector Offset)
	{
		VectorRegister OffsetReg = VectorLoadFloat3_W0(&Offset);
		Element.BoundingSphereVector = VectorAdd(Element.BoundingSphereVector, OffsetReg);
	}
};

#endif
