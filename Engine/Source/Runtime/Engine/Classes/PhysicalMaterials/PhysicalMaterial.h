// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PhysicalMaterial.generated.h"

namespace physx
{
	class PxMaterial;
}

/** Pairs desired tire friction scale with tire type */
USTRUCT()
struct FTireFrictionScalePair
{
	GENERATED_USTRUCT_BODY()

	/** Tire type */
	UPROPERTY(EditAnywhere, Category=TireFrictionScalePair)
	class UTireType*				TireType;

	/** Friction scale for this type of tire */
	UPROPERTY(EditAnywhere, Category=TireFrictionScalePair)
	float							FrictionScale;

	FTireFrictionScalePair()
		: TireType(NULL)
		, FrictionScale(1.0f)
	{
	}
};

/**
 * Physical materials are used to define the response of a physical object when interacting dynamically with the world.
 */
UCLASS(collapsecategories, hidecategories=Object)
class ENGINE_API UPhysicalMaterial : public UObject
{
	GENERATED_UCLASS_BODY()

	//
	// Surface properties.
	//
	
	/** Friction value of surface, controls how easily things can slide on this surface */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PhysicalMaterial)
	float Friction;

	/** Resitution or 'bouncyness' of this surface */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PhysicalMaterial)
	float Restitution;

	//
	// Object properties.
	//
	
	/** Used with the shape of the object to calculate its mass properties. The higher the number, the heavier the object. g per cubic cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PhysicalMaterial)
	float Density;

	/** 
	 *	Used to adjust the way that mass increases as objects get larger. This is applied to the mass as calculated based on a 'solid' object. 
	 *	In actuality, larger objects do not tend to be solid, and become more like 'shells' (e.g. a car is not a solid piece of metal).
	 *	Values are clamped to 1 or less.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Advanced)
	float RaiseMassToPower;

	/** How much to scale the damage threshold by on any destructible we are applied to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Destruction)
	float DestructibleDamageThresholdScale;

	UPROPERTY(/*deprecated*/)
	class UDEPRECATED_PhysicalMaterialPropertyBase* PhysicalMaterialProperty;

	/**
	 * To edit surface type for your project, use ProjectSettings/Physics/PhysicalSurface section
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PhysicalProperties)
	TEnumAsByte<EPhysicalSurface> SurfaceType;

	/** Overall tire friction scalar for every type of tire. This value is multiplied against our parents' values. */
	UPROPERTY(EditAnywhere, Category=Vehicles)
	float TireFrictionScale;

	/** Tire friction scales for specific types of tires. These values are multiplied against our parents' values. */
	UPROPERTY(EditAnywhere, Category=Vehicles)
	TArray<FTireFrictionScalePair> TireFrictionScales;

public:
#if WITH_PHYSX
	/** Internal pointer to PhysX material object */
	physx::PxMaterial* PMaterial;

	FPhysxUserData PhysxUserData;
#endif

	// Begin UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
#endif // WITH_EDITOR
	virtual void PostLoad() OVERRIDE;
	virtual void FinishDestroy() OVERRIDE;
	// End UObject interface

#if WITH_PHYSX
	physx::PxMaterial* GetPhysXMaterial();
#endif // WITH_PHYSX

	/** Update the PhysX material from this objects properties */
	void UpdatePhysXMaterial();

	/** Get the tire friction scale for a type of tire */
	virtual float GetTireFrictionScale( TWeakObjectPtr<class UTireType> TireType );

	/** Determine Material Type from input PhysicalMaterial **/
	static EPhysicalSurface DetermineSurfaceType(UPhysicalMaterial const* PhysicalMaterial);
};



