// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SpotLightComponent.cpp: LightComponent implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "PointLightSceneProxy.h"


/**
 * The spot light policy for TMeshLightingDrawingPolicy.
 */
class FSpotLightPolicy
{
public:
	typedef class FSpotLightSceneProxy SceneInfoType;
};

/**
 * The scene info for a spot light.
 */
class FSpotLightSceneProxy : public TPointLightSceneProxy<FSpotLightPolicy>
{
public:

	/** Outer cone angle in radians, clamped to a valid range. */
	float OuterConeAngle;

	/** Cosine of the spot light's inner cone angle. */
	float CosInnerCone;

	/** Cosine of the spot light's outer cone angle. */
	float CosOuterCone;

	/** 1 / (CosInnerCone - CosOuterCone) */
	float InvCosConeDifference;

	/** Sine of the spot light's outer cone angle. */
	float SinOuterCone;

	/** 1 / Tangent of the spot light's outer cone angle. */
	float InvTanOuterCone;

	/** Cosine of the spot light's outer light shaft cone angle. */
	float CosLightShaftConeAngle;

	/** 1 / (appCos(ClampedInnerLightShaftConeAngle) - CosLightShaftConeAngle) */
	float InvCosLightShaftConeDifference;

	/** Initialization constructor. */
	FSpotLightSceneProxy(const USpotLightComponent* Component):
		TPointLightSceneProxy<FSpotLightPolicy>(Component)
	{
		const float ClampedInnerConeAngle = FMath::Clamp(Component->InnerConeAngle,0.0f,89.0f) * (float)PI / 180.0f;
		const float ClampedOuterConeAngle = FMath::Clamp(Component->OuterConeAngle * (float)PI / 180.0f,ClampedInnerConeAngle + 0.001f,89.0f * (float)PI / 180.0f + 0.001f);
		OuterConeAngle = ClampedOuterConeAngle;
		CosOuterCone = FMath::Cos(ClampedOuterConeAngle);
		SinOuterCone = FMath::Sin(ClampedOuterConeAngle);
		CosInnerCone = FMath::Cos(ClampedInnerConeAngle);
		InvCosConeDifference = 1.0f / (CosInnerCone - CosOuterCone);
		InvTanOuterCone = 1.0f / FMath::Tan(ClampedOuterConeAngle);
		const float ClampedOuterLightShaftConeAngle = FMath::Clamp(Component->LightShaftConeAngle * (float)PI / 180.0f, 0.001f, 89.0f * (float)PI / 180.0f + 0.001f);
		// Use half the outer light shaft cone angle as the inner angle to provide a nice falloff
		// Not exposing the inner light shaft cone angle as it is probably not needed
		const float ClampedInnerLightShaftConeAngle = .5f * ClampedOuterLightShaftConeAngle;
		CosLightShaftConeAngle = FMath::Cos(ClampedOuterLightShaftConeAngle);
		InvCosLightShaftConeDifference = 1.0f / (FMath::Cos(ClampedInnerLightShaftConeAngle) - CosLightShaftConeAngle);
	}

	/** Accesses parameters needed for rendering the light. */
	virtual void GetParameters(FVector4& LightPositionAndInvRadius, FVector4& LightColorAndFalloffExponent, FVector& NormalizedLightDirection, FVector2D& SpotAngles, float& LightSourceRadius, float& LightSourceLength, float& LightMinRoughness) const
	{
		LightPositionAndInvRadius = FVector4(
			GetOrigin(),
			InvRadius);

		LightColorAndFalloffExponent = FVector4(
			GetColor().R,
			GetColor().G,
			GetColor().B,
			FalloffExponent);

		NormalizedLightDirection = -GetDirection();
		SpotAngles = FVector2D(CosOuterCone, InvCosConeDifference);
		LightSourceRadius = SourceRadius;
		LightSourceLength = SourceLength;
		LightMinRoughness = MinRoughness;
	}

	// FLightSceneInfo interface.
	virtual bool AffectsBounds(const FBoxSphereBounds& Bounds) const
	{
		if(!TPointLightSceneProxy<FSpotLightPolicy>::AffectsBounds(Bounds))
		{
			return false;
		}

		FVector	U = GetOrigin() - (Bounds.SphereRadius / SinOuterCone) * GetDirection(),
				D = Bounds.Origin - U;
		float	dsqr = D | D,
				E = GetDirection() | D;
		if(E > 0.0f && E * E >= dsqr * FMath::Square(CosOuterCone))
		{
			D = Bounds.Origin - GetOrigin();
			dsqr = D | D;
			E = -(GetDirection() | D);
			if(E > 0.0f && E * E >= dsqr * FMath::Square(SinOuterCone))
				return dsqr <= FMath::Square(Bounds.SphereRadius);
			else
				return true;
		}

		return false;
	}
	
	/**
	 * Sets up a projected shadow initializer for shadows from the entire scene.
	 * @return True if the whole-scene projected shadow should be used.
	 */
	virtual bool GetWholeSceneProjectedShadowInitializer(const FSceneViewFamily& ViewFamily, TArray<FWholeSceneProjectedShadowInitializer, TInlineAllocator<6> >& OutInitializers) const
	{
		FWholeSceneProjectedShadowInitializer& OutInitializer = *new(OutInitializers) FWholeSceneProjectedShadowInitializer;
		OutInitializer.bDirectionalLight = false;
		OutInitializer.bOnePassPointLightShadow = false;
		OutInitializer.PreShadowTranslation = -GetLightToWorld().GetOrigin();
		OutInitializer.WorldToLight = GetWorldToLight().RemoveTranslation();
		OutInitializer.Scales = FVector(1.0f,InvTanOuterCone,InvTanOuterCone);
		OutInitializer.FaceDirection = FVector(1,0,0);
		OutInitializer.SubjectBounds = FBoxSphereBounds(
			GetLightToWorld().RemoveTranslation().TransformPosition(FVector(Radius / 2.0f,0,0)),
			FVector(Radius/2.0f,Radius/2.0f,Radius/2.0f),
			Radius / 2.0f
			);
		OutInitializer.WAxis = FVector4(0,0,1,0);
		OutInitializer.MinLightW = 0.1f;
		OutInitializer.MaxDistanceToCastInLightW = Radius;
		OutInitializer.SplitIndex = INDEX_NONE;
		return true;
	}

	virtual float GetOuterConeAngle() const { return OuterConeAngle; }

	virtual FVector2D GetLightShaftConeParams() const OVERRIDE { return FVector2D(CosLightShaftConeAngle, InvCosLightShaftConeDifference); }

	virtual FSphere GetBoundingSphere() const
	{
		// Use the law of cosines to find the distance to the furthest edge of the spotlight cone from a position that is halfway down the spotlight direction
		const float BoundsRadius = FMath::Sqrt(1.25f * Radius * Radius - Radius * Radius * CosOuterCone);
		return FSphere(GetOrigin() + .5f * GetDirection() * Radius, BoundsRadius);
	}
};

USpotLightComponent::USpotLightComponent(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
		static ConstructorHelpers::FObjectFinder<UTexture2D> StaticTexture(TEXT("/Engine/EditorResources/LightIcons/S_LightSpot"));
		static ConstructorHelpers::FObjectFinder<UTexture2D> DynamicTexture(TEXT("/Engine/EditorResources/LightIcons/S_LightSpotMove"));

		StaticEditorTexture = StaticTexture.Object;
		StaticEditorTextureScale = 0.5f;
		DynamicEditorTexture = DynamicTexture.Object;
		DynamicEditorTextureScale = 0.5f;
	}
#endif

	InnerConeAngle = 0.0f;
	OuterConeAngle = 44.0f;
}

void USpotLightComponent::SetInnerConeAngle(float NewInnerConeAngle)
{
	if (!(IsRegistered() && (Mobility == EComponentMobility::Static || Mobility == EComponentMobility::Stationary))
		&& NewInnerConeAngle != InnerConeAngle)
	{
		InnerConeAngle = NewInnerConeAngle;
		MarkRenderStateDirty();
	}
}

void USpotLightComponent::SetOuterConeAngle(float NewOuterConeAngle)
{
	if (!(IsRegistered() && (Mobility == EComponentMobility::Static || Mobility == EComponentMobility::Stationary))
		&& NewOuterConeAngle != OuterConeAngle)
	{
		OuterConeAngle = NewOuterConeAngle;
		MarkRenderStateDirty();
	}
}

// Disable for now
//void USpotLightComponent::SetLightShaftConeAngle(float NewLightShaftConeAngle)
//{
//	if (NewLightShaftConeAngle != LightShaftConeAngle)
//	{
//		LightShaftConeAngle = NewLightShaftConeAngle;
//		MarkRenderStateDirty();
//	}
//}

FLightSceneProxy* USpotLightComponent::CreateSceneProxy() const
{
	return new FSpotLightSceneProxy(this);
}

FSphere USpotLightComponent::GetBoundingSphere() const
{
	float ClampedInnerConeAngle = FMath::Clamp(InnerConeAngle,0.0f,89.0f) * (float)PI / 180.0f;
	float ClampedOuterConeAngle = FMath::Clamp(OuterConeAngle * (float)PI / 180.0f,ClampedInnerConeAngle + 0.001f,89.0f * (float)PI / 180.0f + 0.001f);

	float CosOuterCone = FMath::Cos(ClampedOuterConeAngle);

	// Use the law of cosines to find the distance to the furthest edge of the spotlight cone from a position that is halfway down the spotlight direction
	const float BoundsRadius = FMath::Sqrt(1.25f * AttenuationRadius * AttenuationRadius - AttenuationRadius * AttenuationRadius * CosOuterCone);
	return FSphere(ComponentToWorld.GetLocation() + .5f * GetDirection() * AttenuationRadius, BoundsRadius);
}

bool USpotLightComponent::AffectsBounds(const FBoxSphereBounds& Bounds) const
{
	if(!Super::AffectsBounds(Bounds))
	{
		return false;
	}

	float	ClampedInnerConeAngle = FMath::Clamp(InnerConeAngle,0.0f,89.0f) * (float)PI / 180.0f,
			ClampedOuterConeAngle = FMath::Clamp(OuterConeAngle * (float)PI / 180.0f,ClampedInnerConeAngle + 0.001f,89.0f * (float)PI / 180.0f + 0.001f);

	float	Sin = FMath::Sin(ClampedOuterConeAngle),
			Cos = FMath::Cos(ClampedOuterConeAngle);

	FVector	U = GetComponentLocation() - (Bounds.SphereRadius / Sin) * GetDirection(),
			D = Bounds.Origin - U;
	float	dsqr = D | D,
			E = GetDirection() | D;
	if(E > 0.0f && E * E >= dsqr * FMath::Square(Cos))
	{
		D = Bounds.Origin - GetComponentLocation();
		dsqr = D | D;
		E = -(GetDirection() | D);
		if(E > 0.0f && E * E >= dsqr * FMath::Square(Sin))
			return dsqr <= FMath::Square(Bounds.SphereRadius);
		else
			return true;
	}

	return false;
}

/**
* @return ELightComponentType for the light component class 
*/
ELightComponentType USpotLightComponent::GetLightType() const
{
	return LightType_Spot;
}
