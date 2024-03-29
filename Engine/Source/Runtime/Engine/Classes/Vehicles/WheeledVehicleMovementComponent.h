// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VehicleWheel.h"
#include "WheeledVehicleMovementComponent.generated.h"

#if WITH_PHYSX
namespace physx
{
	class PxVehicleDrive;
	class PxVehicleWheels;
	class PxVehicleWheelsSimData;
}
#endif

/**
 * Values passed from PhysX to generate tire forces
 */
struct FTireShaderInput
{
	// Friction value of the tire contact.
	float TireFriction;

	// Longitudinal slip of the tire
	float LongSlip;

	// Lateral slip of the tire.
	float LatSlip;

	// Rotational speed of the wheel, in radians.
	float WheelOmega;

	// The distance from the tire surface to the center of the wheel.
	float WheelRadius;

	// 1 / WheelRadius
	float RecipWheelRadius;

	// How much force (weight) is pushing on the tire when the vehicle is at rest.
	float RestTireLoad;

	// How much force (weight) is pushing on the tire right now.
	float TireLoad;

	// RestTireLoad / TireLoad
	float NormalizedTireLoad;

	// Acceleration due to gravity
	float Gravity;

	// 1 / Gravity
	float RecipGravity;
};

/**
 * Generated tire forces to pass back to PhysX
 */
struct FTireShaderOutput
{
	// The torque to be applied to the wheel around the wheel axle. Opposes the engine torque on the wheel
	float WheelTorque;

	// The magnitude of the longitudinal tire force to be applied to the vehicle's rigid body.
	float LongForce;

	// The magnitude of the lateral tire force to be applied to the vehicle's rigid body.
	float LatForce;

	FTireShaderOutput() {}

	FTireShaderOutput(float f)
		: WheelTorque(f)
		, LongForce(f)
		, LatForce(f)
	{
	}
};

/**
 * Vehicle-specific wheel setup
 */
USTRUCT()
struct FWheelSetup
{
	GENERATED_USTRUCT_BODY()

	// The wheel class to use
	UPROPERTY(EditAnywhere, Category=WheelSetup)
	TSubclassOf<UVehicleWheel> WheelClass;

	// Bone name on mesh to create wheel at
	UPROPERTY(EditAnywhere, Category=WheelSetup)
	FName BoneName;

	// Additional offset to give the wheels for this axle.
	UPROPERTY(EditAnywhere, Category=WheelSetup)
	FVector AdditionalOffset;

	FWheelSetup()
		: WheelClass(UVehicleWheel::StaticClass())
		, BoneName(NAME_None)
		, AdditionalOffset(0.0f)
	{
	}
};

USTRUCT()
struct FReplicatedVehicleState
{
	GENERATED_USTRUCT_BODY()

	// input replication: steering
	UPROPERTY()
	float SteeringInput;

	// input replication: throttle
	UPROPERTY()
	float ThrottleInput;

	// input replication: brake
	UPROPERTY()
	float BrakeInput;

	// input replication: handbrake
	UPROPERTY()
	float HandbrakeInput;

	// state replication: current gear
	UPROPERTY()
	int32 CurrentGear;
};

USTRUCT()
struct FVehicleInputRate
{
	GENERATED_USTRUCT_BODY()

	// Rate at which the input value rises
	UPROPERTY(EditAnywhere, Category=VehicleInputRate)
	float RiseRate;

	// Rate at which the input value falls
	UPROPERTY(EditAnywhere, Category=VehicleInputRate)
	float FallRate;

	FVehicleInputRate() : RiseRate(5.0f), FallRate(5.0f) { }

	/** Change an output value using max rise and fall rates */
	float InterpInputValue( float DeltaTime, float CurrentValue, float NewValue ) const
	{
		const float DeltaValue = NewValue - CurrentValue;
		const bool bRising = ( DeltaValue > 0.0f ) == ( CurrentValue > 0.0f );
		const float MaxDeltaValue = DeltaTime * ( bRising ? RiseRate : FallRate );
		const float ClampedDeltaValue = FMath::Clamp( DeltaValue, -MaxDeltaValue, MaxDeltaValue );
		return CurrentValue + ClampedDeltaValue;
	}
};

/**
 * Component to handle the vehicle simulation for an actor.
 */
UCLASS(Abstract, dependson=UCurveBase, hidecategories=(PlanarMovement, "Components|Movement|Planar", Activation, "Components|Activation"))
class ENGINE_API UWheeledVehicleMovementComponent : public UPawnMovementComponent
{
	GENERATED_UCLASS_BODY()

	/** Wheels to create */
	UPROPERTY(EditAnywhere, Category=VehicleSetup)
	TArray<FWheelSetup> WheelSetups;

	/** Mass to set the vehicle chassis to. It's much easier to tweak vehicle settings when
	 * the mass doesn't change due to tweaks with the physics asset. [kg] */
	UPROPERTY(EditAnywhere, Category = VehicleSetup, meta = (ClampMin = "0.01", UIMin = "0.01"))
	float Mass;

	/** DragCoefficient of the vehicle chassis. */
	UPROPERTY(EditAnywhere, Category = VehicleSetup)
	float DragCoefficient;

	/** Chassis width used for drag force computation (cm)*/
	UPROPERTY(EditAnywhere, Category = VehicleSetup, meta = (ClampMin = "0.01", UIMin = "0.01"))
	float ChassisWidth;

	/** Chassis height used for drag force computation (cm)*/
	UPROPERTY(EditAnywhere, Category = VehicleSetup, meta = (ClampMin = "0.01", UIMin = "0.01"))
	float ChassisHeight;

	// Drag area in cm^2
	UPROPERTY(transient)
	float DragArea;

	// Estimated mad speed for engine
	UPROPERTY(transient)
	float EstimatedMaxEngineSpeed;

	// Max RPM for engine
	UPROPERTY(transient)
	float MaxEngineRPM;

	// Debug drag magnitude last applied
	UPROPERTY(transient)
	float DebugDragMagnitude;

	/** When vehicle is created we want to compute some helper data like drag area, etc.... Derived classes should use this to properly compute things like engine RPM */
	virtual void ComputeConstants();

	/** Override center of mass offset, makes tweaking easier [uu] */
	UPROPERTY(EditAnywhere, Category=VehicleSetup, AdvancedDisplay)
	FVector COMOffset;

	/** Scales the vehicle's inertia in each direction (forward, right, up) */
	UPROPERTY(EditAnywhere, Category=VehicleSetup, AdvancedDisplay)
	FVector InertiaTensorScale;

	/** Clamp normalized tire load to this value */
	UPROPERTY(EditAnywhere, Category=VehicleSetup, AdvancedDisplay)
	float MinNormalizedTireLoad;

	/** Clamp normalized tire load to this value */
	UPROPERTY(EditAnywhere, Category = VehicleSetup, AdvancedDisplay)
	float MinNormalizedTireLoadFiltered;

	/** Clamp normalized tire load to this value */
	UPROPERTY(EditAnywhere, Category=VehicleSetup, AdvancedDisplay)
	float MaxNormalizedTireLoad;

	/** Clamp normalized tire load to this value */
	UPROPERTY(EditAnywhere, Category = VehicleSetup, AdvancedDisplay)
	float MaxNormalizedTireLoadFiltered;

	// Our instanced wheels
	UPROPERTY(transient, duplicatetransient, BlueprintReadOnly, Category=Vehicle)
	TArray<class UVehicleWheel*> Wheels;

	// The value of PhysXVehicleManager::VehicleSetupTag when this vehicle created its physics state.
	// Used to recreate the physics if the blueprint changes.
	uint32 VehicleSetupTag;

	bool CheckSlipThreshold(float AbsLongSlipThreshold, float AbsLatSlipThreshold) const;
	float GetMaxSpringForce() const;

#if WITH_PHYSX

	// The instanced PhysX vehicle
	physx::PxVehicleWheels* PVehicle;
	physx::PxVehicleDrive* PVehicleDrive;

	/** Compute the forces generates from a spinning tire */
	virtual void GenerateTireForces(class UVehicleWheel* Wheel, const FTireShaderInput& Input, FTireShaderOutput& Output);

	/** Return true if we are ready to create a vehicle */
	virtual bool CanCreateVehicle() const;

	/** Create and setup the physx vehicle */
	virtual void CreateVehicle();

	/** Tick this vehicle sim right before input is sent to the vehicle system  */
	virtual void TickVehicle( float DeltaTime );

	/** Updates the vehicle tuning and other state such as user input. */
	virtual void PreTick(float DeltaTime);

	/** Updates the forces of drag acting on the vehicle */
	virtual void UpdateDrag( float DeltaTime );

	/** Used to create any physics engine information for this component */
	virtual void CreatePhysicsState() OVERRIDE;

	/** Used to shut down and pysics engine structure for this component */
	virtual void DestroyPhysicsState() OVERRIDE;

	virtual bool ShouldCreatePhysicsState() const OVERRIDE;

	virtual bool HasValidPhysicsState() const OVERRIDE;

	/** Draw debug text for the wheels and suspension */
	virtual void DrawDebug(UCanvas* Canvas, float& YL, float& YPos);

	/** Draw debug lines for the wheels and suspension */
	virtual void DrawDebugLines();

	/** Skeletal mesh needs some special handling in the vehicle case */
	virtual void FixupSkeletalMesh();

#if WITH_EDITOR
	/** Respond to a property change in editor */
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
#endif //WITH_EDITOR

#endif // WITH_PHYSX

	/** Set the user input for the vehicle throttle */
	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	void SetThrottleInput(float Throttle);
	
	/** Set the user input for the vehicle steering */
	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	void SetSteeringInput(float Steering);

	/** Set the user input for handbrake */
	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	void SetHandbrakeInput(bool bNewHandbrake);

	/** Set the user input for gear up */
	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	void SetGearUp(bool bNewGearUp);

	/** Set the user input for gear down */
	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	void SetGearDown(bool bNewGearDown);

	/** Set the user input for gear (-1 reverse, 0 neutral, 1+ forward)*/
	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	void SetTargetGear(int32 GearNum, bool bImmediate);

	/** Set the flag that will be used to select auto-gears */
	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	void SetUseAutoGears(bool bUseAuto);

	/** How fast the vehicle is moving forward */
	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	float GetForwardSpeed() const;

	/** Get current engine's rotation speed */
	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	float GetEngineRotationSpeed() const;

	/** Get current engine's max rotation speed */
	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	float GetEngineMaxRotationSpeed() const;

	/** Get current gear */
	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	int32 GetCurrentGear() const;

	/** Get target gear */
	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	int32 GetTargetGear() const;

	/** Are gears being changed automatically? */
	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	bool GetUseAutoGears() const;

protected:

	// replicated state of vehicle 
	UPROPERTY(Transient, Replicated)
	FReplicatedVehicleState ReplicatedState;

	// accumulator for RB replication errors 
	float AngErrorAccumulator;

	// What the player has the steering set to. Range -1...1
	UPROPERTY(Transient)
	float RawSteeringInput;

	// What the player has the accelerator set to. Range -1...1
	UPROPERTY(Transient)
	float RawThrottleInput;

	// True if the player is holding the handbrake
	UPROPERTY(Transient)
	uint32 bRawHandbrakeInput : 1;

	// True if the player is holding gear up
	UPROPERTY(Transient)
	uint32 bRawGearUpInput : 1;

	// True if the player is holding gear down
	UPROPERTY(Transient)
	uint32 bRawGearDownInput : 1;

	// Steering output to physics system. Range -1...1
	UPROPERTY(Transient)
	float SteeringInput;

	// Accelerator output to physics system. Range 0...1
	UPROPERTY(Transient)
	float ThrottleInput;

	// Brake output to physics system. Range 0...1
	UPROPERTY(Transient)
	float BrakeInput;

	// Handbrake output to physics system. Range 0...1
	UPROPERTY(Transient)
	float HandbrakeInput;

	// How much to press the brake when the player has release throttle
	UPROPERTY(EditAnywhere, Category=VehicleInput)
	float IdleBrakeInput;

	// Auto-brake when absolute vehicle forward speed is less than this (cm/s)
	UPROPERTY(EditAnywhere, Category=VehicleInput)
	float StopThreshold;

	// Auto-brake when vehicle forward speed is opposite of player input by at least this much (cm/s)
	UPROPERTY(EditAnywhere, Category = VehicleInput)
	float WrongDirectionThreshold;

	// Rate at which input throttle can rise and fall
	UPROPERTY(EditAnywhere, Category=VehicleInput, AdvancedDisplay)
	FVehicleInputRate ThrottleInputRate;

	// Rate at which input brake can rise and fall
	UPROPERTY(EditAnywhere, Category=VehicleInput, AdvancedDisplay)
	FVehicleInputRate BrakeInputRate;

	// Rate at which input handbrake can rise and fall
	UPROPERTY(EditAnywhere, Category=VehicleInput, AdvancedDisplay)
	FVehicleInputRate HandbrakeInputRate;

	// Rate at which input steering can rise and fall
	UPROPERTY(EditAnywhere, Category=VehicleInput, AdvancedDisplay)
	FVehicleInputRate SteeringInputRate;

	/** Compute steering input */
	float CalcSteeringInput();

	/** Compute brake input */
	float CalcBrakeInput();

	/** Compute handbrake input */
	float CalcHandbrakeInput();

	/** Compute throttle input */
	virtual float CalcThrottleInput();

	/** Clear all interpolated inputs to default values */
	virtual void ClearInput();

	/** Read current state for simulation */
	void UpdateState(float DeltaTime);

	/** Pass current state to server */
	UFUNCTION(reliable, server, WithValidation)
	void ServerUpdateState(float InSteeringInput, float InThrottleInput, float InBrakeInput, float InHandbrakeInput, int32 CurrentGear);

#if WITH_PHYSX

	int32 GearToPhysXGear(const int32 Gear) const;

	int32 PhysXGearToGear(const int32 PhysXGear) const;

	/** Pass input values to vehicle simulation */
	virtual void UpdateSimulation( float DeltaTime );

	/** Allocate and setup the PhysX vehicle */
	virtual void SetupVehicle() {};

	/** Do some final setup after the physx vehicle gets created */
	virtual void PostSetupVehicle();

	/** Set up the chassis and wheel shapes */
	virtual void SetupVehicleShapes();

	/** Adjust the PhysX actor's mass */
	virtual void SetupVehicleMass();

	/** Set up the wheel data */
	virtual void SetupWheels(physx::PxVehicleWheelsSimData* PWheelsSimData);

	/** Instantiate and setup our wheel objects */
	virtual void CreateWheels();

	/** Release our wheel objects */
	virtual void DestroyWheels();

	/** Get the local position of the wheel at rest */
	virtual FVector GetWheelRestingPosition(const FWheelSetup& WheelSetup);

	/** Get the local COM offset */
	virtual FVector GetCOMOffset();

	/** Get the mesh this vehicle is tied to */
	class USkinnedMeshComponent* GetMesh();

#endif // WITH_PHYSX
	

};

//some helper functions for converting units

//rev per minute to rad/s
inline float RPMToOmega(float RPM)
{
	return RPM * PI / 30.f;
}

//rad/s to rev per minute
inline float OmegaToRPM(float Omega)
{
	return Omega * 30.f / PI;
}

//km/h to cm/s
inline float KmHToCmS(float KmH)
{
	return KmH * 100000.f / 3600.f;
}

inline float CmSToKmH(float CmS)
{
	return CmS * 3600.f / 100000.f;
}

inline float M2ToCm2(float M2)
{
	return M2 * 100.f * 100.f;
}

inline float Cm2ToM2(float Cm2)
{
	return Cm2 / (100.f * 100.f);
}