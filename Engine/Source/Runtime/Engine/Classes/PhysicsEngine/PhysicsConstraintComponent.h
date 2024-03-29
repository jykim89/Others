// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "PhysicsConstraintComponent.generated.h"

/** Dynamic delegate to use by components that want to route the broken-event into blueprints */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FConstraintBrokenSignature, int32, ConstraintIndex);

/**
 *	This is effectively a joint that allows you to connect 2 rigid bodies together. You can create different types of joints using the various parameters of this component.
 */
UCLASS(ClassGroup=Physics, dependson(ConstraintInstance), MinimalAPI, meta=(BlueprintSpawnableComponent), HideCategories=(Activation,"Components|Activation", Physics, Mobility), ShowCategories=("Physics|Components|PhysicsConstraint"))
class UPhysicsConstraintComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	/** Pointer to first Actor to constrain.  */
	UPROPERTY(EditInstanceOnly, Category=Constraint)
	AActor* ConstraintActor1;

	/** 
	 *	Name of first component property to constrain. If Actor1 is NULL, will look within Owner.
	 *	If this is NULL, will use RootComponent of Actor1
	 */
	UPROPERTY(EditAnywhere, Category=Constraint)
	FConstrainComponentPropName ComponentName1;


	/** Pointer to second Actor to constrain. */
	UPROPERTY(EditInstanceOnly, Category=Constraint)
	AActor* ConstraintActor2;

	/** 
	 *	Name of second component property to constrain. If Actor2 is NULL, will look within Owner. 
	 *	If this is NULL, will use RootComponent of Actor2
	 */
	UPROPERTY(EditAnywhere, Category=Constraint)
	FConstrainComponentPropName ComponentName2;


	/** Allows direct setting of first component to constraint. */
	TWeakObjectPtr<UPrimitiveComponent> OverrideComponent1;

	/** Allows direct setting of second component to constraint. */
	TWeakObjectPtr<UPrimitiveComponent> OverrideComponent2;


	UPROPERTY(instanced)
	class UPhysicsConstraintTemplate* ConstraintSetup_DEPRECATED;

	/** Notification when constraint is broken. */
	UPROPERTY(BlueprintAssignable)
	FConstraintBrokenSignature OnConstraintBroken;

public:
	/** All constraint settings */
	UPROPERTY(EditAnywhere, Category=ConstraintComponent, meta=(ShowOnlyInnerProperties))
	FConstraintInstance			ConstraintInstance;

	//Begin UObject Interface
	virtual void BeginDestroy() OVERRIDE;
	virtual void PostLoad() OVERRIDE;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
#endif // WITH_EDITOR
	//End UObject interface

	//Begin ActorComponent interface
#if WITH_EDITOR
	virtual void CheckForErrors() OVERRIDE;
#endif // WITH_EDITOR
	virtual void OnRegister() OVERRIDE;
	virtual void OnUnregister() OVERRIDE;
	virtual void InitializeComponent() OVERRIDE;
	//End ActorComponent interface

	// Begin SceneComponent interface
#if WITH_EDITOR
	virtual void PostEditComponentMove(bool bFinished) OVERRIDE;
#endif // WITH_EDITOR
	// End SceneComponent interface

	/** Get the body frame. Works without constraint being created */
	ENGINE_API FTransform GetBodyTransform(EConstraintFrame::Type Frame) const;
	
	/** Get body bounding box. Works without constraint being created */
	ENGINE_API FBox GetBodyBox(EConstraintFrame::Type Frame) const;

	/** Initialize the frames and creates constraint */
	void InitComponentConstraint();

	/** Break the constraint */
	void TermComponentConstraint();

	/** Directly specify component to connect. Will update frames based on current position. */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	ENGINE_API void SetConstrainedComponents(UPrimitiveComponent* Component1, FName BoneName1, UPrimitiveComponent* Component2, FName BoneName2);

	/** Break this constraint */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	ENGINE_API void BreakConstraint();

	/** Enables/Disables linear position drive 
	 *	
	 *	@param bEnableDriveX	Indicates whether the drive for the X-Axis should be enabled
	 *	@param bEnableDriveY	Indicates whether the drive for the Y-Axis should be enabled
	 *	@param bEnableDriveZ	Indicates whether the drive for the Z-Axis should be enabled
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	ENGINE_API void SetLinearPositionDrive(bool bEnableDriveX, bool bEnableDriveY, bool bEnableDriveZ);

	/** Enables/Disables linear position drive 
	 *	
	 *	@param bEnableDriveX	Indicates whether the drive for the X-Axis should be enabled
	 *	@param bEnableDriveY	Indicates whether the drive for the Y-Axis should be enabled
	 *	@param bEnableDriveZ	Indicates whether the drive for the Z-Axis should be enabled
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	ENGINE_API void SetLinearVelocityDrive(bool bEnableDriveX, bool bEnableDriveY, bool bEnableDriveZ);

	/** Enables/Disables angular orientation drive 
	 *	
	 *	@param bEnableSwingDrive	Indicates whether the drive for the swing axis should be enabled
	 *	@param bEnableTwistDrive	Indicates whether the drive for the twist axis should be enabled
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	ENGINE_API void SetAngularOrientationDrive(bool bEnableSwingDrive, bool bEnableTwistDrive);

	/** Enables/Disables angular velocity drive 
	 *	
	 *	@param bEnableSwingDrive	Indicates whether the drive for the swing axis should be enabled
	 *	@param bEnableTwistDrive	Indicates whether the drive for the twit axis should be enabled
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	ENGINE_API void SetAngularVelocityDrive(bool bEnableSwingDrive, bool bEnableTwistDrive);

	/** Sets the target position for the linear drive. 
	 *	@param InPosTarget		Target position
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	ENGINE_API void	SetLinearPositionTarget(const FVector& InPosTarget);
	
	/** Sets the target velocity for the linear drive. 
	 *	@param InVelTarget		Target velocity
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	ENGINE_API void	SetLinearVelocityTarget(const FVector& InVelTarget);

	/** Sets the drive params for the linear drive. 
	 *	@param InSpring		Spring force for the drive
	 *	@param InDamping	Damping of the drive
	 *	@param InForceLimit	Max force applied by the drive
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	ENGINE_API void	SetLinearDriveParams(float InSpring, float InDamping, float InForceLimit);

	/** Sets the target orientation for the angular drive. 
	 *	@param InPosTarget		Target orientation
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	ENGINE_API void	SetAngularOrientationTarget(const FQuat& InPosTarget);

	
	/** Sets the target velocity for the angular drive. 
	 *	@param InVelTarget		Target velocity
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	ENGINE_API void	SetAngularVelocityTarget(const FVector& InVelTarget);

	/** Sets the drive params for the angular drive. 
	 *	@param InSpring		Spring force for the drive
	 *	@param InDamping	Damping of the drive
	 *	@param InForceLimit	Max force applied by the drive
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	ENGINE_API void	SetAngularDriveParams(float InSpring, float InDamping, float InForceLimit);

	/** 
	 *	Update the reference frames held inside the constraint that indicate the joint location in the reference frame 
	 *	of the two connected bodies. You should call this whenever the constraint or either Component moves, or if you change
	 *	the connected Components. This function does nothing though once the joint has been initialized.
	 */
	ENGINE_API void UpdateConstraintFrames();

#if WITH_EDITORONLY_DATA
	UPROPERTY(transient)
	class UBillboardComponent* SpriteComponent;

	ENGINE_API void UpdateSpriteTexture();
#endif

protected:
	/** Get the body instance that we want to constrain to */
	FBodyInstance* GetBodyInstance(EConstraintFrame::Type Frame) const;

	/** Internal util to get body transform from actor/component name/bone name information */
	FTransform GetBodyTransformInternal(EConstraintFrame::Type Frame, FName InBoneName) const;
	/** Internal util to get body box from actor/component name/bone name information */
	FBox GetBodyBoxInternal(EConstraintFrame::Type Frame, FName InBoneName) const;
	/** Internal util to get component from actor/component name */
	UPrimitiveComponent* GetComponentInternal(EConstraintFrame::Type Frame) const;

	/** Routes the FConstraint callback to the dynamic delegate */
	void OnConstraintBrokenHandler(FConstraintInstance* BrokenConstraint);
};

