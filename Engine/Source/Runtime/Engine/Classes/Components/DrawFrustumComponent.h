// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "DrawFrustumComponent.generated.h"

/**
 *	Utility component for drawing a view frustum. Origin is at the component location, frustum points down position X axis.
 */ 

UCLASS(collapsecategories, hidecategories=Object, editinlinenew, MinimalAPI)
class UDrawFrustumComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()
	
	/** Color to draw the wireframe frustum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DrawFrustumComponent)
	FColor FrustumColor;

	/** Angle of longest dimension of view shape. 
	  * If the angle is 0 then an orthographic projection is used */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DrawFrustumComponent)
	float FrustumAngle;

	/** Ratio of horizontal size over vertical size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DrawFrustumComponent)
	float FrustumAspectRatio;

	/** Distance from origin to start drawing the frustum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DrawFrustumComponent)
	float FrustumStartDist;

	/** Distance from origin to stop drawing the frustum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DrawFrustumComponent)
	float FrustumEndDist;

	/** optional texture to show on the near plane */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DrawFrustumComponent)
	class UTexture* Texture;


	// Begin UPrimitiveComponent interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() OVERRIDE;
	virtual FBoxSphereBounds CalcBounds(const FTransform & LocalToWorld) const OVERRIDE;
	// End UPrimitiveComponent interface.
};



