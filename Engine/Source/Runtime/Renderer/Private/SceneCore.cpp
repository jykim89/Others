// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneCore.cpp: Core scene implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "AllocatorFixedSizeFreeList.h"

static TAutoConsoleVariable<int32> CVarWholeSceneShadowUnbuiltInteractionThreshold(
	TEXT("r.Shadow.WholeSceneShadowUnbuiltInteractionThreshold"),
	500,
	TEXT("How many unbuilt light-primitive interactions there can be for a light before the light switches to whole scene shadows"),
	ECVF_RenderThreadSafe);

/**
 * Fixed Size pool allocator for FLightPrimitiveInteractions
 */
#define FREE_LIST_GROW_SIZE ( 16384 / sizeof(FLightPrimitiveInteraction) )
TAllocatorFixedSizeFreeList<sizeof(FLightPrimitiveInteraction), FREE_LIST_GROW_SIZE> GLightPrimitiveInteractionAllocator;

uint32 FRendererModule::GetNumDynamicLightsAffectingPrimitive(const FPrimitiveSceneInfo* PrimitiveSceneInfo,const FLightCacheInterface* LCI)
{
	uint32 NumDynamicLights = 0;

	FLightPrimitiveInteraction *LightList = PrimitiveSceneInfo->LightList;
	while ( LightList )
	{
		const FLightSceneInfo* LightSceneInfo = LightList->GetLight();

		// Determine the interaction type between the mesh and the light.
		FLightInteraction LightInteraction = FLightInteraction::Dynamic();
		if(LCI)
		{
			LightInteraction = LCI->GetInteraction(LightSceneInfo->Proxy);
		}

		// Don't count light-mapped or irrelevant lights.
		if(LightInteraction.GetType() != LIT_CachedIrrelevant && LightInteraction.GetType() != LIT_CachedLightMap)
		{
			++NumDynamicLights;
		}

		LightList = LightList->GetNextLight();
	}

	return NumDynamicLights;
}

/*-----------------------------------------------------------------------------
	FLightPrimitiveInteraction
-----------------------------------------------------------------------------*/

/**
 * Custom new
 */
void* FLightPrimitiveInteraction::operator new(size_t Size)
{
	// doesn't support derived classes with a different size
	checkSlow(Size == sizeof(FLightPrimitiveInteraction));
	return GLightPrimitiveInteractionAllocator.Allocate();
	//return FMemory::Malloc(Size);
}

/**
 * Custom delete
 */
void FLightPrimitiveInteraction::operator delete(void *RawMemory)
{
	GLightPrimitiveInteractionAllocator.Free(RawMemory);
	//FMemory::Free(RawMemory);
}	

/**
 * Initialize the memory pool with a default size from the ini file.
 * Called at render thread startup. Since the render thread is potentially
 * created/destroyed multiple times, must make sure we only do it once.
 */
void FLightPrimitiveInteraction::InitializeMemoryPool()
{
	static bool bAlreadyInitialized = false;
	if (!bAlreadyInitialized)
	{
		bAlreadyInitialized = true;
		int32 InitialBlockSize = 0;
		GConfig->GetInt(TEXT("MemoryPools"), TEXT("FLightPrimitiveInteractionInitialBlockSize"), InitialBlockSize, GEngineIni);
		GLightPrimitiveInteractionAllocator.Grow(InitialBlockSize);
	}
}

/**
* Returns current size of memory pool
*/
uint32 FLightPrimitiveInteraction::GetMemoryPoolSize()
{
	return GLightPrimitiveInteractionAllocator.GetAllocatedSize();
}

void FLightPrimitiveInteraction::Create(FLightSceneInfo* LightSceneInfo,FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	// Attach the light to the primitive's static meshes.
	bool bDynamic = true;
	bool bRelevant = false;
	bool bLightMapped = true;
	bool bShadowMapped = false;

	// Determine the light's relevance to the primitive.
	check(PrimitiveSceneInfo->Proxy);
	PrimitiveSceneInfo->Proxy->GetLightRelevance(LightSceneInfo->Proxy, bDynamic, bRelevant, bLightMapped, bShadowMapped);

	if (bRelevant && bDynamic
		// Don't let lights with static shadowing or static lighting affect primitives that should use static lighting, but don't have valid settings (lightmap res 0, etc)
		// This prevents those components with invalid lightmap settings from causing lighting to remain unbuilt after a build
		&& !(LightSceneInfo->Proxy->HasStaticShadowing() && PrimitiveSceneInfo->Proxy->HasStaticLighting() && !PrimitiveSceneInfo->Proxy->HasValidSettingsForStaticLighting()))
	{
		const bool bTranslucentObjectShadow = LightSceneInfo->Proxy->CastsTranslucentShadows() && PrimitiveSceneInfo->Proxy->CastsVolumetricTranslucentShadow();
		const bool bInsetObjectShadow = 
			// Currently only supporting inset shadows on directional lights, but could be made to work with any whole scene shadows
			LightSceneInfo->Proxy->GetLightType() == LightType_Directional
			&& PrimitiveSceneInfo->Proxy->CastsInsetShadow();

		// Movable directional lights determine shadow relevance dynamically based on the view and CSM settings. Interactions are only required for per-object cases.
		if (LightSceneInfo->Proxy->GetLightType() != LightType_Directional || LightSceneInfo->Proxy->HasStaticShadowing() || bTranslucentObjectShadow || bInsetObjectShadow)
		{
			// Create the light interaction.
			FLightPrimitiveInteraction* Interaction = new FLightPrimitiveInteraction(LightSceneInfo, PrimitiveSceneInfo, bDynamic, bLightMapped, bShadowMapped, bTranslucentObjectShadow, bInsetObjectShadow);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			// Treat the light as completely unbuilt if it has more unbuilt interactions than the threshold.
			// This will result in the light using whole scene shadows instead of many per-object shadows, 
			// Which prevents poor performance when many per-object shadows are created for previewing unbuilt lighting.
			if (LightSceneInfo->NumUnbuiltInteractions >= CVarWholeSceneShadowUnbuiltInteractionThreshold.GetValueOnRenderThread() && LightSceneInfo->bPrecomputedLightingIsValid)
			{
				LightSceneInfo->bPrecomputedLightingIsValid = false;
				LightSceneInfo->Proxy->InvalidatePrecomputedLighting(true);
			}
#endif
		}
	}
}

void FLightPrimitiveInteraction::Destroy(FLightPrimitiveInteraction* LightPrimitiveInteraction)
{
	delete LightPrimitiveInteraction;
}

FLightPrimitiveInteraction::FLightPrimitiveInteraction(
	FLightSceneInfo* InLightSceneInfo,
	FPrimitiveSceneInfo* InPrimitiveSceneInfo,
	bool bInIsDynamic,
	bool bInLightMapped,
	bool bInIsShadowMapped,
	bool bInHasTranslucentObjectShadow,
	bool bInHasInsetObjectShadow
	):
	LightId(InLightSceneInfo->Id),
	LightSceneInfo(InLightSceneInfo),
	PrimitiveSceneInfo(InPrimitiveSceneInfo),
	bLightMapped(bInLightMapped),
	bIsDynamic(bInIsDynamic),
	bIsShadowMapped(bInIsShadowMapped),
	bUncachedStaticLighting(false),
	bHasTranslucentObjectShadow(bInHasTranslucentObjectShadow),
	bHasInsetObjectShadow(bInHasInsetObjectShadow)
{
	// Determine whether this light-primitive interaction produces a shadow.
	if(PrimitiveSceneInfo->Proxy->HasStaticLighting())
	{
		const bool bHasStaticShadow =
			LightSceneInfo->Proxy->HasStaticShadowing() &&
			LightSceneInfo->Proxy->CastsStaticShadow() &&
			PrimitiveSceneInfo->Proxy->CastsStaticShadow();
		const bool bHasDynamicShadow =
			!LightSceneInfo->Proxy->HasStaticLighting() &&
			LightSceneInfo->Proxy->CastsDynamicShadow() &&
			PrimitiveSceneInfo->Proxy->CastsDynamicShadow();
		bCastShadow = bHasStaticShadow || bHasDynamicShadow;
	}
	else
	{
		bCastShadow = LightSceneInfo->Proxy->CastsDynamicShadow() && PrimitiveSceneInfo->Proxy->CastsDynamicShadow();
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if(bCastShadow && bIsDynamic)
	{
		// Determine the type of dynamic shadow produced by this light.
		if (PrimitiveSceneInfo->Proxy->HasStaticLighting()
			&& PrimitiveSceneInfo->Proxy->CastsStaticShadow()
			&& (LightSceneInfo->Proxy->HasStaticLighting() || (LightSceneInfo->Proxy->HasStaticShadowing() && !bInIsShadowMapped)))
		{
			// Update the game thread's counter of number of uncached static lighting interactions.
			bUncachedStaticLighting = true;
			LightSceneInfo->NumUnbuiltInteractions++;

			FPlatformAtomics::InterlockedIncrement(&PrimitiveSceneInfo->Scene->NumUncachedStaticLightingInteractions);

#if WITH_EDITOR
			PrimitiveSceneInfo->Proxy->NumUncachedStaticLightingInteractions++;
#endif
		}
	}
#endif

	if (bIsDynamic)
	{
		// Add the interaction to the light's interaction list.
		PrevPrimitiveLink = &LightSceneInfo->DynamicPrimitiveList;
	}

	NextPrimitive = *PrevPrimitiveLink;
	if(*PrevPrimitiveLink)
	{
		(*PrevPrimitiveLink)->PrevPrimitiveLink = &NextPrimitive;
	}
	*PrevPrimitiveLink = this;

	// Add the interaction to the primitive's interaction list.
	PrevLightLink = &PrimitiveSceneInfo->LightList;
	NextLight = *PrevLightLink;
	if(*PrevLightLink)
	{
		(*PrevLightLink)->PrevLightLink = &NextLight;
	}
	*PrevLightLink = this;
}

FLightPrimitiveInteraction::~FLightPrimitiveInteraction()
{
	check(IsInRenderingThread());

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Update the game thread's counter of number of uncached static lighting interactions.
	if(bUncachedStaticLighting)
	{
		LightSceneInfo->NumUnbuiltInteractions--;
		FPlatformAtomics::InterlockedDecrement(&PrimitiveSceneInfo->Scene->NumUncachedStaticLightingInteractions);
#if WITH_EDITOR
		PrimitiveSceneInfo->Proxy->NumUncachedStaticLightingInteractions--;
#endif
	}
#endif

	// Remove the interaction from the light's interaction list.
	if(NextPrimitive)
	{
		NextPrimitive->PrevPrimitiveLink = PrevPrimitiveLink;
	}
	*PrevPrimitiveLink = NextPrimitive;

	// Remove the interaction from the primitive's interaction list.
	if(NextLight)
	{
		NextLight->PrevLightLink = PrevLightLink;
	}
	*PrevLightLink = NextLight;
}

/*-----------------------------------------------------------------------------
	FStaticMesh
-----------------------------------------------------------------------------*/

void FStaticMesh::LinkDrawList(FStaticMesh::FDrawListElementLink* Link)
{
	check(IsInRenderingThread());
	check(!DrawListLinks.Contains(Link));
	DrawListLinks.Add(Link);
}

void FStaticMesh::UnlinkDrawList(FStaticMesh::FDrawListElementLink* Link)
{
	check(IsInRenderingThread());
	verify(DrawListLinks.RemoveSingleSwap(Link) == 1);
}

void FStaticMesh::AddToDrawLists(FScene* Scene)
{
	if (Scene->GetFeatureLevel() >= ERHIFeatureLevel::SM3)
	{
		if (CastShadow)
		{
			FShadowDepthDrawingPolicyFactory::AddStaticMesh(Scene,this);
		}

		if (!bShadowOnly && PrimitiveSceneInfo->Proxy->ShouldRenderInMainPass())
		{
			// not all platforms need this
			const bool bRequiresHitProxies = Scene->RequiresHitProxies();
			if ( bRequiresHitProxies && PrimitiveSceneInfo->Proxy->IsSelectable() )
			{
				// Add the static mesh to the DPG's hit proxy draw list.
				FHitProxyDrawingPolicyFactory::AddStaticMesh(Scene,this);
			}

			if(!IsTranslucent())
			{
				extern TAutoConsoleVariable<int32> CVarEarlyZPass;
				int32 EarlyZPass = CVarEarlyZPass.GetValueOnRenderThread();

				extern int32 GEarlyZPassMovable;

				// Render non-masked materials in the depth only pass
				if (PrimitiveSceneInfo->Proxy->ShouldUseAsOccluder() 
					&& (!IsMasked() || EarlyZPass == 2)
					&& (!PrimitiveSceneInfo->Proxy->IsMovable() || GEarlyZPassMovable))
				{
					FDepthDrawingPolicyFactory::AddStaticMesh(Scene,this);
				}

				// Add the static mesh to the DPG's base pass draw list.
				FBasePassOpaqueDrawingPolicyFactory::AddStaticMesh(Scene,this);

				FVelocityDrawingPolicyFactory::AddStaticMesh(Scene, this);
			}
		}
	}
	else
	{
		if (!bShadowOnly && !IsTranslucent())
		{
			// Add the static mesh to the DPG's base pass draw list.
			FBasePassForwardOpaqueDrawingPolicyFactory::AddStaticMesh(Scene,this);
		}
	}
}

void FStaticMesh::RemoveFromDrawLists()
{
	// Remove the mesh from all draw lists.
	while(DrawListLinks.Num())
	{
		FStaticMesh::FDrawListElementLink* Link = DrawListLinks[0];
		const int32 OriginalNumLinks = DrawListLinks.Num();
		// This will call UnlinkDrawList.
		Link->Remove();
		check(DrawListLinks.Num() == OriginalNumLinks - 1);
		if(DrawListLinks.Num())
		{
			check(DrawListLinks[0] != Link);
		}
	}
}

/** Returns true if the mesh is linked to the given draw list. */
bool FStaticMesh::IsLinkedToDrawList(const FStaticMeshDrawListBase* DrawList) const
{
	for (int32 i = 0; i < DrawListLinks.Num(); i++)
	{
		if (DrawListLinks[i]->IsInDrawList(DrawList))
		{
			return true;
		}
	}
	return false;
}

FStaticMesh::~FStaticMesh()
{
	// Remove this static mesh from the scene's list.
	PrimitiveSceneInfo->Scene->StaticMeshes.RemoveAt(Id);

	RemoveFromDrawLists();
}

/** Initialization constructor. */
FExponentialHeightFogSceneInfo::FExponentialHeightFogSceneInfo(const UExponentialHeightFogComponent* InComponent):
	Component(InComponent),
	FogHeight(InComponent->GetComponentLocation().Z),
	// Scale the densities back down to their real scale
	// Artists edit the densities scaled up so they aren't entering in minuscule floating point numbers
	FogDensity(InComponent->FogDensity / 1000.0f),
	FogHeightFalloff(InComponent->FogHeightFalloff / 1000.0f),
	FogMaxOpacity(InComponent->FogMaxOpacity),
	StartDistance(InComponent->StartDistance),
	LightTerminatorAngle(0),
	DirectionalInscatteringExponent(InComponent->DirectionalInscatteringExponent),
	DirectionalInscatteringStartDistance(InComponent->DirectionalInscatteringStartDistance),
	DirectionalInscatteringColor(InComponent->DirectionalInscatteringColor)
{
	FogColor = InComponent->FogInscatteringColor;
}
