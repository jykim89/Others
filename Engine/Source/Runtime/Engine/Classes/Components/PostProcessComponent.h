// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "PostProcessComponent.generated.h"

/**
 *  PostProcessComponent. Enables Post process controls for blueprints.
 *	Will use a parent UShapeComponent to provide volume data if available.
 */

UCLASS(ClassGroup = Rendering, collapsecategories, hidecategories = (Object), editinlinenew, meta = (BlueprintSpawnableComponent), MinimalAPI)
class UPostProcessComponent : public USceneComponent, public IInterface_PostProcessVolume
{
	GENERATED_UCLASS_BODY()

	/** Post process settings to use for this volume. */
	UPROPERTY(interp, Category = PostProcessVolume, meta = (ShowOnlyInnerProperties))
	struct FPostProcessSettings Settings;

	/**
	 * Priority of this volume. In the case of overlapping volumes the one with the highest priority
	 * overrides the lower priority ones. The order is undefined if two or more overlapping volumes have the same priority.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PostProcessVolume)
	float Priority;

	/** World space radius around the volume that is used for blending (only if not unbound).			*/
	UPROPERTY(interp, Category = PostProcessVolume, meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "6000.0"))
	float BlendRadius;

	/** 0:no effect, 1:full effect */
	UPROPERTY(interp, Category = PostProcessVolume, BlueprintReadWrite, meta = (UIMin = "0.0", UIMax = "1.0"))
	float BlendWeight;

	/** Whether this volume is enabled or not.															*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PostProcessVolume)
	uint32 bEnabled: 1;

	/** set this to false to use the parent shape component as volume bounds. True affects the whole world regardless.		*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PostProcessVolume)
	uint32 bUnbound: 1;
 
	// Begin IInterface_PostProcessVolume Interface
	ENGINE_API virtual bool EncompassesPoint(FVector Point, float SphereRadius/*=0.f*/, float* OutDistanceToPoint) OVERRIDE;
	ENGINE_API virtual FPostProcessVolumeProperties GetProperties() const OVERRIDE
	{
		FPostProcessVolumeProperties Ret;
		Ret.bIsEnabled = bEnabled != 0;
		Ret.bIsUnbound = bUnbound != 0 || Cast<UShapeComponent>(AttachParent) == nullptr;
		Ret.BlendRadius = BlendRadius;
		Ret.BlendWeight = BlendWeight;
		Ret.Priority = Priority;
		Ret.Settings = &Settings;
		return Ret;
	}
	// End IInterface_PostProcessVolume Interface

protected:

	virtual void OnRegister();
	virtual void OnUnregister();
};



