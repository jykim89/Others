// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneCore.h: Core scene definitions.
=============================================================================*/

#pragma once

// Forward declarations.
class FStaticMesh;
class FScene;
class FPrimitiveSceneInfo;
class FLightSceneInfo;

/**
 * An interaction between a light and a primitive.
 */
class FLightPrimitiveInteraction
{
public:

	/** Creates an interaction for a light-primitive pair. */
	static void InitializeMemoryPool();
	static void Create(FLightSceneInfo* LightSceneInfo,FPrimitiveSceneInfo* PrimitiveSceneInfo);
	static void Destroy(FLightPrimitiveInteraction* LightPrimitiveInteraction);

	/** Returns current size of memory pool */
	static uint32 GetMemoryPoolSize();

	// Accessors.
	bool HasShadow() const { return bCastShadow; }
	bool IsLightMapped() const { return bLightMapped; }
	bool IsDynamic() const { return bIsDynamic; }
	bool IsShadowMapped() const { return bIsShadowMapped; }
	bool IsUncachedStaticLighting() const { return bUncachedStaticLighting; }
	bool HasTranslucentObjectShadow() const { return bHasTranslucentObjectShadow; }
	bool HasInsetObjectShadow() const { return bHasInsetObjectShadow; }
	FLightSceneInfo* GetLight() const { return LightSceneInfo; }
	int32 GetLightId() const { return LightId; }
	FPrimitiveSceneInfo* GetPrimitiveSceneInfo() const { return PrimitiveSceneInfo; }
	FLightPrimitiveInteraction* GetNextPrimitive() const { return NextPrimitive; }
	FLightPrimitiveInteraction* GetNextLight() const { return NextLight; }

	/** Hash function required for TMap support */
	friend uint32 GetTypeHash( const FLightPrimitiveInteraction* Interaction )
	{
		return (uint32)Interaction->LightId;
	}

	/** Custom new/delete */
	void* operator new(size_t Size);
	void operator delete(void *RawMemory);

private:
	/** The index into Scene->Lights of the light which affects the primitive. */
	int32 LightId;

	/** The light which affects the primitive. */
	FLightSceneInfo* LightSceneInfo;

	/** The primitive which is affected by the light. */
	FPrimitiveSceneInfo* PrimitiveSceneInfo;

	/** True if the primitive casts a shadow from the light. */
	uint32 bCastShadow : 1;

	/** True if the primitive has a light-map containing the light. */
	uint32 bLightMapped : 1;

	/** True if the interaction is dynamic. */
	uint32 bIsDynamic : 1;

	/** Whether the light's shadowing is contained in the primitive's static shadow map. */
	uint32 bIsShadowMapped : 1;

	/** True if the interaction is an uncached static lighting interaction. */
	uint32 bUncachedStaticLighting : 1;

	/** True if the interaction has a translucent per-object shadow. */
	uint32 bHasTranslucentObjectShadow : 1;

	/** True if the interaction has an inset per-object shadow. */
	uint32 bHasInsetObjectShadow : 1;

	/** A pointer to the NextPrimitive member of the previous interaction in the light's interaction list. */
	FLightPrimitiveInteraction** PrevPrimitiveLink;

	/** The next interaction in the light's interaction list. */
	FLightPrimitiveInteraction* NextPrimitive;

	/** A pointer to the NextLight member of the previous interaction in the primitive's interaction list. */
	FLightPrimitiveInteraction** PrevLightLink;

	/** The next interaction in the primitive's interaction list. */
	FLightPrimitiveInteraction* NextLight;


	/** Initialization constructor. */
	FLightPrimitiveInteraction(FLightSceneInfo* InLightSceneInfo,FPrimitiveSceneInfo* InPrimitiveSceneInfo,
		bool bIsDynamic,bool bInLightMapped,bool bInIsShadowMapped, bool bInHasTranslucentObjectShadow, bool bInHasInsetObjectShadow);

	/** Hide dtor */
	~FLightPrimitiveInteraction();

};

/**
 * A mesh which is defined by a primitive at scene segment construction time and never changed.
 * Lights are attached and detached as the segment containing the mesh is added or removed from a scene.
 */
class FStaticMesh : public FMeshBatch
{
public:

	/**
	 * An interface to a draw list's reference to this static mesh.
	 * used to remove the static mesh from the draw list without knowing the draw list type.
	 */
	class FDrawListElementLink : public FRefCountedObject
	{
	public:
		virtual bool IsInDrawList(const class FStaticMeshDrawListBase* DrawList) const = 0;
		virtual void Remove() = 0;
	};

	/** The screen space size to draw this primitive at */
	float ScreenSize;

	/** The render info for the primitive which created this mesh. */
	FPrimitiveSceneInfo* PrimitiveSceneInfo;

	/** The ID of the hit proxy which represents this static mesh. */
	FHitProxyId HitProxyId;

	/** The index of the mesh in the scene's static meshes array. */
	int32 Id;

	/** If true this static mesh should only be rendered during shadow depth passes. */
	bool bShadowOnly;

	// Constructor/destructor.
	FStaticMesh(
		FPrimitiveSceneInfo* InPrimitiveSceneInfo,
		const FMeshBatch& InMesh,
		float InScreenSize,
		bool bInShadowOnly,
		FHitProxyId InHitProxyId
		):
		FMeshBatch(InMesh),
		ScreenSize(InScreenSize),
		PrimitiveSceneInfo(InPrimitiveSceneInfo),
		HitProxyId(InHitProxyId),
		Id(INDEX_NONE),
		bShadowOnly(bInShadowOnly)
	{}
	~FStaticMesh();

	/** Adds a link from the mesh to its entry in a draw list. */
	void LinkDrawList(FDrawListElementLink* Link);

	/** Removes a link from the mesh to its entry in a draw list. */
	void UnlinkDrawList(FDrawListElementLink* Link);

	/** Adds the static mesh to the appropriate draw lists in a scene. */
	void AddToDrawLists(FScene* Scene);

	/** Removes the static mesh from all draw lists. */
	void RemoveFromDrawLists();

	/** Returns true if the mesh is linked to the given draw list. */
	bool IsLinkedToDrawList(const FStaticMeshDrawListBase* DrawList) const;

private:
	/** Links to the draw lists this mesh is an element of. */
	TArray<TRefCountPtr<FDrawListElementLink> > DrawListLinks;

	/** Private copy constructor. */
	FStaticMesh(const FStaticMesh& InStaticMesh):
		FMeshBatch(InStaticMesh),
		ScreenSize(InStaticMesh.ScreenSize),
		PrimitiveSceneInfo(InStaticMesh.PrimitiveSceneInfo),
		HitProxyId(InStaticMesh.HitProxyId),
		Id(InStaticMesh.Id)
	{}
};

/** The properties of a exponential height fog layer which are used for rendering. */
class FExponentialHeightFogSceneInfo
{
public:

	/** The fog component the scene info is for. */
	const UExponentialHeightFogComponent* Component;
	float FogHeight;
	float FogDensity;
	float FogHeightFalloff;
	float FogMaxOpacity;
	float StartDistance;
	float LightTerminatorAngle;
	FLinearColor FogColor;
	float DirectionalInscatteringExponent;
	float DirectionalInscatteringStartDistance;
	FLinearColor DirectionalInscatteringColor;

	/** Initialization constructor. */
	FExponentialHeightFogSceneInfo(const UExponentialHeightFogComponent* InComponent);
};

/** Returns true if the indirect lighting cache can be used at all. */
extern bool IsIndirectLightingCacheAllowed(ERHIFeatureLevel::Type InFeatureLevel);

/** Returns true if the indirect lighting cache can use the volume texture atlas on this feature level. */
extern bool CanIndirectLightingCacheUseVolumeTexture(ERHIFeatureLevel::Type InFeatureLevel);
