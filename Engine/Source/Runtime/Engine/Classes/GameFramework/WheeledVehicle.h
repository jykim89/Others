// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "WheeledVehicle.generated.h"

UCLASS(abstract, config=Game, dependson=(AController), BlueprintType)
class ENGINE_API AWheeledVehicle : public APawn
{
	GENERATED_UCLASS_BODY()

	/**  The main skeletal mesh associated with this Vehicle */
	UPROPERTY(Category=Vehicle, VisibleDefaultsOnly, BlueprintReadOnly)
	TSubobjectPtr<class USkeletalMeshComponent> Mesh;

	/** vehicle simulation component */
	UPROPERTY(Category=Vehicle, VisibleDefaultsOnly, BlueprintReadOnly)
	TSubobjectPtr<class UWheeledVehicleMovementComponent> VehicleMovement;

	/** Name of the MeshComponent. Use this name if you want to prevent creation of the component (with PCIP.DoNotCreateDefaultSubobject). */
	static FName VehicleMeshComponentName;

	/** Name of the VehicleMovement. Use this name if you want to use a different class (with PCIP.SetDefaultSubobjectClass). */
	static FName VehicleMovementComponentName;

	/** Util to get the wheeled vehicle movement component */
	class UWheeledVehicleMovementComponent* GetVehicleMovementComponent() const 
	{ 
		return VehicleMovement; 
	}

	// Begin AActor interface
	virtual void DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) OVERRIDE;
	// End Actor interface
};
