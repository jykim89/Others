// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

AInteractiveFoliageActor::AInteractiveFoliageActor(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP
		.SetDefaultSubobjectClass<UInteractiveFoliageComponent>("StaticMeshComponent0"))
{

	UInteractiveFoliageComponent* FoliageMeshComponent = CastChecked<UInteractiveFoliageComponent>(StaticMeshComponent);
	FoliageMeshComponent->BodyInstance.bEnableCollision_DEPRECATED = false;
	FoliageMeshComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	FoliageMeshComponent->Mobility = EComponentMobility::Static;

	CapsuleComponent = PCIP.CreateDefaultSubobject<UCapsuleComponent>(this, TEXT("CollisionCylinder"));
	CapsuleComponent->InitCapsuleSize(60.0f, 200.0f);
	CapsuleComponent->BodyInstance.bEnableCollision_DEPRECATED = true;
	static FName CollisionProfileName(TEXT("OverlapAllDynamic"));
	CapsuleComponent->SetCollisionProfileName(CollisionProfileName);
	CapsuleComponent->Mobility = EComponentMobility::Static;

	RootComponent = CapsuleComponent;

	PrimaryActorTick.bCanEverTick = true;
	bCanBeDamaged = true;
	bCollideWhenPlacing = true;
	FoliageDamageImpulseScale = 20.0f;
	FoliageTouchImpulseScale = 10.0f;
	FoliageStiffness = 10.0f;
	FoliageStiffnessQuadratic = 0.3f;
	FoliageDamping = 2.0f;
	MaxDamageImpulse = 100000.0f;
	MaxTouchImpulse = 1000.0f;
	MaxForce = 100000.0f;
	Mass = 1.0f;
}
