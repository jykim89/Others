// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LightSceneInfo.cpp: Light scene info implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"

void FLightSceneInfoCompact::Init(FLightSceneInfo* InLightSceneInfo)
{
	LightSceneInfo = InLightSceneInfo;
	FSphere BoundingSphere(
		InLightSceneInfo->Proxy->GetOrigin(),
		InLightSceneInfo->Proxy->GetRadius() > 0.0f ?
			InLightSceneInfo->Proxy->GetRadius() :
			FLT_MAX
		);
	FMemory::Memcpy(&BoundingSphereVector,&BoundingSphere,sizeof(BoundingSphereVector));
	Color = InLightSceneInfo->Proxy->GetColor();
	LightType = InLightSceneInfo->Proxy->GetLightType();

	bCastDynamicShadow = InLightSceneInfo->Proxy->CastsDynamicShadow();
	bCastStaticShadow = InLightSceneInfo->Proxy->CastsStaticShadow();
	bStaticLighting = InLightSceneInfo->Proxy->HasStaticLighting();
}

FLightSceneInfo::FLightSceneInfo(FLightSceneProxy* InProxy, bool InbVisible)
	: Proxy(InProxy)
	, DynamicPrimitiveList(NULL)
	, Id(INDEX_NONE)
	, bPrecomputedLightingIsValid(InProxy->GetLightComponent()->bPrecomputedLightingIsValid)
	, bVisible(InbVisible)
	, bEnableLightShaftBloom(InProxy->GetLightComponent()->bEnableLightShaftBloom)
	, BloomScale(InProxy->GetLightComponent()->BloomScale)
	, BloomThreshold(InProxy->GetLightComponent()->BloomThreshold)
	, BloomTint(InProxy->GetLightComponent()->BloomTint)
	, NumUnbuiltInteractions(0)
	, bCreatePerObjectShadowsForDynamicObjects(Proxy->ShouldCreatePerObjectShadowsForDynamicObjects())
	, Scene(InProxy->GetLightComponent()->GetScene()->GetRenderScene())
{
	// Only visible lights can be added in game
	check(bVisible || GIsEditor);

	if (!bPrecomputedLightingIsValid)
	{
		Proxy->InvalidatePrecomputedLighting(InProxy->GetLightComponent()->GetScene()->IsEditorScene());
	}

	BeginInitResource(this);

	for(uint32 LightTypeIndex = 0;LightTypeIndex < LightType_MAX;++LightTypeIndex)
	{
		for(uint32 A = 0;A < 2;++A)
		{
			for(uint32 B = 0;B < 2;++B)
			{
				for(uint32 C = 0;C < 2;++C)
				{
					TranslucentInjectCachedShaderMaps[LightTypeIndex][A][B][C] = NULL;
				}
			}
		}
	}
}

FLightSceneInfo::~FLightSceneInfo()
{
	ReleaseResource();
}

void FLightSceneInfo::AddToScene()
{
	const FLightSceneInfoCompact& LightSceneInfoCompact = Scene->Lights[Id];

	// Only need to create light interactions for lights that can cast a shadow, 
	// As deferred shading doesn't need to know anything about the primitives that a light affects
	if (Proxy->CastsDynamicShadow() 
		|| Proxy->CastsStaticShadow() 
		// Lights that should be baked need to check for interactions to track unbuilt state correctly
		|| Proxy->HasStaticLighting())
	{
		// Add the light to the scene's light octree.
		Scene->LightOctree.AddElement(LightSceneInfoCompact);

		// TODO: Special case directional lights, no need to traverse the octree.

		// Find primitives that the light affects in the primitive octree.
		FMemMark MemStackMark(FMemStack::Get());
		for(FScenePrimitiveOctree::TConstElementBoxIterator<SceneRenderingAllocator> PrimitiveIt(
				Scene->PrimitiveOctree,
				GetBoundingBox()
				);
			PrimitiveIt.HasPendingElements();
			PrimitiveIt.Advance())
		{
			CreateLightPrimitiveInteraction(LightSceneInfoCompact, PrimitiveIt.GetCurrentElement());
		}
	}
}

/**
 * If the light affects the primitive, create an interaction, and process children 
 * 
 * @param LightSceneInfoCompact Compact representation of the light
 * @param PrimitiveSceneInfoCompact Compact representation of the primitive
 */
void FLightSceneInfo::CreateLightPrimitiveInteraction(const FLightSceneInfoCompact& LightSceneInfoCompact, const FPrimitiveSceneInfoCompact& PrimitiveSceneInfoCompact)
{
	if(	LightSceneInfoCompact.AffectsPrimitive(PrimitiveSceneInfoCompact))
	{
		// create light interaction and add to light/primitive lists
		FLightPrimitiveInteraction::Create(this,PrimitiveSceneInfoCompact.PrimitiveSceneInfo);
	}
}


void FLightSceneInfo::RemoveFromScene()
{
	if (OctreeId.IsValidId())
	{
		// Remove the light from the octree.
		Scene->LightOctree.RemoveElement(OctreeId);
	}

	// Detach the light from the primitives it affects.
	Detach();
}

void FLightSceneInfo::Detach()
{
	check(IsInRenderingThread());

	// implicit linked list. The destruction will update this "head" pointer to the next item in the list.
	while(DynamicPrimitiveList)
	{
		FLightPrimitiveInteraction::Destroy(DynamicPrimitiveList);
	}
}

bool FLightSceneInfo::ShouldRenderLight(const FViewInfo& View) const
{
	// Only render the light if it is in the view frustum
	bool bLocalVisible = bVisible ? View.VisibleLightInfos[Id].bInViewFrustum : true;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	ELightComponentType Type = (ELightComponentType)Proxy->GetLightType();

	switch(Type)
	{
		case LightType_Directional:
			if(!View.Family->EngineShowFlags.DirectionalLights) 
			{
				bLocalVisible = false;
			}
			break;
		case LightType_Point:
			if(!View.Family->EngineShowFlags.PointLights) 
			{
				bLocalVisible = false;
			}
			break;
		case LightType_Spot:
			if(!View.Family->EngineShowFlags.SpotLights)
			{
				bLocalVisible = false;
			}
			break;
	}
#endif

	return bLocalVisible
		// Only render lights with static shadowing for reflection captures, since they are only captured at edit time
		&& (!View.bIsReflectionCapture || Proxy->HasStaticShadowing());
}

void FLightSceneInfo::ReleaseRHI()
{
	for(uint32 LightTypeIndex = 0;LightTypeIndex < LightType_MAX;++LightTypeIndex)
	{
		for(uint32 A = 0;A < 2;++A)
		{
			for(uint32 B = 0;B < 2;++B)
			{
				for(uint32 C = 0;C < 2;++C)
				{
					TranslucentInjectBoundShaderState[LightTypeIndex][A][B][C].SafeRelease();
					TranslucentInjectCachedShaderMaps[LightTypeIndex][A][B][C] = NULL;
				}
			}
		}
	}
}

/** Determines whether two bounding spheres intersect. */
FORCEINLINE bool AreSpheresNotIntersecting(
	const VectorRegister& A_XYZ,
	const VectorRegister& A_Radius,
	const VectorRegister& B_XYZ,
	const VectorRegister& B_Radius
	)
{
	const VectorRegister DeltaVector = VectorSubtract(A_XYZ,B_XYZ);
	const VectorRegister DistanceSquared = VectorDot3(DeltaVector,DeltaVector);
	const VectorRegister MaxDistance = VectorAdd(A_Radius,B_Radius);
	const VectorRegister MaxDistanceSquared = VectorMultiply(MaxDistance,MaxDistance);
	return !!VectorAnyGreaterThan(DistanceSquared,MaxDistanceSquared);
}

/**
* Tests whether this light affects the given primitive.  This checks both the primitive and light settings for light relevance
* and also calls AffectsBounds.
*
* @param CompactPrimitiveSceneInfo - The primitive to test.
* @return True if the light affects the primitive.
*/
bool FLightSceneInfoCompact::AffectsPrimitive(const FPrimitiveSceneInfoCompact& CompactPrimitiveSceneInfo) const
{
	// Check if the light's bounds intersect the primitive's bounds.
	if(AreSpheresNotIntersecting(
		BoundingSphereVector,
		VectorReplicate(BoundingSphereVector,3),
		VectorLoadFloat3(&CompactPrimitiveSceneInfo.Bounds.Origin),
		VectorLoadFloat1(&CompactPrimitiveSceneInfo.Bounds.SphereRadius)
		))
	{
		return false;
	}

	// Cull based on information in the full scene infos.

	if(!LightSceneInfo->Proxy->AffectsBounds(CompactPrimitiveSceneInfo.Bounds))
	{
		return false;
	}

	return true;
}
