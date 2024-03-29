// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

UPostProcessComponent::UPostProcessComponent(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	bEnabled = true;
	BlendRadius = 100.0f;
	BlendWeight = 1.0f;
	Priority = 0;
	bUnbound = 1;
}

bool UPostProcessComponent::EncompassesPoint(FVector Point, float SphereRadius/*=0.f*/, float* OutDistanceToPoint)
{
	UShapeComponent* ParentShape = Cast<UShapeComponent>(AttachParent);
	if (ParentShape != nullptr)
	{
#if WITH_PHYSX
		FVector ClosestPoint;
		float Distance = ParentShape->GetDistanceToCollision(Point, ClosestPoint);
#else
		FBoxSphereBounds Bounds = ParentShape->CalcBounds(ParentShape->ComponentToWorld);
		float Distance = 0;
		if (ParentShape->IsA<USphereComponent>())
		{
			const FSphere& Sphere = Bounds.GetSphere();
			const FVector& Dist = Sphere.Center - Point;
			Distance = FMath::Max(0.0f, Dist.Size() - Sphere.W);
		}
		else // UBox or UCapsule shape (approx).
		{
			Distance = FMath::Sqrt(Bounds.GetBox().ComputeSquaredDistanceToPoint(Point));
		}
#endif

		if (OutDistanceToPoint)
		{
			*OutDistanceToPoint = Distance;
		}

		return Distance >= 0.f && Distance <= SphereRadius;
	}
	if (OutDistanceToPoint != nullptr)
	{
		*OutDistanceToPoint = 0;
	}
	return true;
}