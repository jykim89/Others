// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*
 * Component to handle the vehicle simulation for an actor
 */

#pragma once
#include "VehicleWheel.generated.h"

UCLASS(BlueprintType, Blueprintable)
class ENGINE_API UVehicleWheel : public UObject
{
	GENERATED_UCLASS_BODY()

	/** 
	 * Static mesh with collision setup for wheel, will be used to create wheel shape
	 * (if empty, sphere will be added as wheel shape, check bDontCreateShape flag)
	 */
	UPROPERTY(EditDefaultsOnly, Category=Shape)
	class UStaticMesh*								CollisionMesh;

	/** If set, shape won't be created, but mapped from chassis mesh */
	UPROPERTY(EditDefaultsOnly, Category=Shape)
	bool											bDontCreateShape;

	/** 
	 *	If true, ShapeRadius and ShapeWidth will be used to automatically scale collision taken from CollisionMesh to match wheel size.
	 *	If false, size of CollisionMesh won't be changed. Use if you want to scale wheels manually.
	 */
	UPROPERTY(EditAnywhere, Category=Shape)
	bool											bAutoAdjustCollisionSize;

	/** 
	 * If BoneName is specified, offset the wheel from the bone's location.
	 * Otherwise this offsets the wheel from the vehicle's origin.
	 */
	UPROPERTY(EditAnywhere, Category=Wheel)
	FVector											Offset;

	/** Radius of the wheel */
	UPROPERTY(EditAnywhere, Category = Wheel, meta = (ClampMin = "0.01", UIMin = "0.01"))
	float											ShapeRadius;

	/** Width of the wheel */
	UPROPERTY(EditAnywhere, Category = Wheel, meta = (ClampMin = "0.01", UIMin = "0.01"))
	float											ShapeWidth;

	/** Mass of this wheel */
	UPROPERTY(EditAnywhere, Category = Wheel, meta = (ClampMin = "0.01", UIMin = "0.01"))
	float											Mass;

	/** Damping rate for this wheel (Kgm^2/s) */
	UPROPERTY(EditAnywhere, Category = Wheel, meta = (ClampMin = "0.01", UIMin = "0.01"))
	float											DampingRate;

	// steer angle in degrees for this wheel
	UPROPERTY(EditAnywhere, Category = WheelsSetup, meta = (ClampMin = "0", UIMin = "0"))
	float SteerAngle;

	/** Whether handbrake should affect this wheel */
	UPROPERTY(EditAnywhere, Category=Wheel)
	bool											bAffectedByHandbrake;

	/** Tire type for the wheel. Determines friction */
	UPROPERTY(EditAnywhere, Category=Tire)
	class UTireType*								TireType;

	/** Max normalized tire load at which the tire can deliver no more lateral stiffness no matter how much extra load is applied to the tire. */
	UPROPERTY(EditAnywhere, Category=Tire, meta=(ClampMin = "0.01", UIMin = "0.01"))
	float											LatStiffMaxLoad;

	/** How much lateral stiffness to have given lateral slip */
	UPROPERTY(EditAnywhere, Category=Tire, meta=(ClampMin = "0.01", UIMin = "0.01"))
	float											LatStiffValue;

	/** How much longitudinal stiffness to have given longitudinal slip */
	UPROPERTY(EditAnywhere, Category=Tire)
	float											LongStiffValue;

	/** Vertical offset from vehicle center of mass where suspension forces are applied */
	UPROPERTY(EditAnywhere, Category=Suspension)
	float											SuspensionForceOffset;

	/** How far the wheel can go above the resting position */
	UPROPERTY(EditAnywhere, Category = Suspension)
	float											SuspensionMaxRaise;

	/** How far the wheel can drop below the resting position */
	UPROPERTY(EditAnywhere, Category=Suspension)
	float											SuspensionMaxDrop;
	
	/** Oscillation frequency of suspension. Standard cars have values between 5 and 10 */
	UPROPERTY(EditAnywhere, Category=Suspension)
	float											SuspensionNaturalFrequency;

	/**
	 *	The rate at which energy is dissipated from the spring. Standard cars have values between 0.8 and 1.2.
	 *	values < 1 are more sluggish, values > 1 or more twitchy
	 */
	UPROPERTY(EditAnywhere, Category=Suspension)
	float											SuspensionDampingRatio;

	/** max brake torque for this wheel (Nm) */
	UPROPERTY(EditAnywhere, Category=Brakes)
	float											MaxBrakeTorque;

	/** 
	 *	Max handbrake brake torque for this wheel (Nm). A handbrake should have a stronger brake torque
	 *	than the brake. This will be ignored for wheels that are not affected by the handbrake. 
	 */
	UPROPERTY(EditAnywhere, Category=Brakes)
	float											MaxHandBrakeTorque;


	/** The vehicle that owns us */
	UPROPERTY(transient)
	class UWheeledVehicleMovementComponent*			VehicleSim;

	// Our index in the vehicle's (and setup's) wheels array
	UPROPERTY(transient)
	int32											WheelIndex;

	// Longitudinal slip experienced by the wheel
	UPROPERTY(transient)
	float											DebugLongSlip;

	// Lateral slip experienced by the wheel
	UPROPERTY(transient)
	float											DebugLatSlip;

	// How much force the tire experiences at rest devided by how much force it is experiencing now
	UPROPERTY(transient)
	float											DebugNormalizedTireLoad;

	// Wheel torque
	UPROPERTY(transient)
	float											DebugWheelTorque;

	// Longitudinal force the wheel is applying to the chassis
	UPROPERTY(transient)
	float											DebugLongForce;

	// Lateral force the wheel is applying to the chassis
	UPROPERTY(transient)
	float											DebugLatForce;

	// Worldspace location of this wheel
	UPROPERTY(transient)
	FVector											Location;

	// Worldspace location of this wheel last frame
	UPROPERTY(transient)
	FVector											OldLocation;

	// Current velocity of the wheel center (change in location over time)
	UPROPERTY(transient)
	FVector											Velocity;

	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	float GetSteerAngle();

	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	float GetRotationAngle();

	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	float GetSuspensionOffset();

#if WITH_PHYSX

	// Our wheelshape
	physx::PxShape*									WheelShape;

	/**
	 * Initialize this wheel instance
	 */
	virtual void Init( class UWheeledVehicleMovementComponent* InVehicleSim, int32 InWheelIndex );

	/**
	 * Notify this wheel it will be removed from the scene
	 */
	virtual void Shutdown();

	/**
	 * Get the wheel setup we were created from
	 */
	struct FWheelSetup& GetWheelSetup();

	/**
	 * Tick this wheel when the vehicle ticks
	 */
	virtual void Tick( float DeltaTime );

#if WITH_EDITOR

	/**
	 * Respond to a property change in editor
	 */
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent ) OVERRIDE;

#endif //WITH_EDITOR

protected:

	/**
	 * Get the wheel's location in physics land
	 */
	FVector GetPhysicsLocation();

#endif // WITH_PHYSX

public:

	/** Get contact surface material */
	UPhysicalMaterial* GetContactSurfaceMaterial();
};
