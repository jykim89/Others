// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// Emitter actor class.
//=============================================================================

#pragma once
#include "Emitter.generated.h"

/** Fires when a particle is spawned
	* @param EventName - Custom event name for the Spawn Event.
	* @param EmitterTime - The emitter time when the event occured.
	* @param Location - Location at which the particle was spawned.
	* @param Velocity - Initial velocity of the spawned particle.
	*/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams( FParticleSpawnSignature, FName, EventName, float, EmitterTime, FVector, Location, FVector, Velocity);

/** Fires when a particle system bursts
	* @param EventName - Custom event name for the Burst Event
	* @param EmitterTime - The emitter time when the event occured.
	* @param ParticleCount - The number of particles spawned in the burst.
	*/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams( FParticleBurstSignature, FName, EventName, float, EmitterTime, int32, ParticleCount);

/** Fires when a particle dies
	* @param EventName - Custom event name for the Death Event.
	* @param EmitterTime - The emitter time when the event occured.
	* @param ParticleTime - How long the particle had been alive at the time of the event.
	* @param Location - Location the particle was at when it died.
	* @param Velocity - Velocity of the particle when it died.
	* @param Direction - Direction of the particle when it died.
	*/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_SixParams( FParticleDeathSignature, FName, EventName, float, EmitterTime, int32, ParticleTime, FVector, Location, FVector, Velocity, FVector, Direction);

/** Fires when a particle dies
	* @param EventName - Custom event name for the Collision Event.
	* @param EmitterTime - The emitter time when the event occured.
	* @param ParticleTime - How long the particle had been alive at the time of the event.
	* @param Location - Location of the collision.
	* @param Velocity - Velocity of the particle at the point of collision.
	* @param Direction - Direction of the particle at the point of collision.
	* @param Normal - Normal to the surface with which collision occurred.
	* @param BoneName- Name of the bone that the particle collided with. (Only valid if collision was with a Skeletal Mesh)
	*/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_EightParams( FParticleCollisionSignature, FName, EventName, float, EmitterTime, int32, ParticleTime, FVector, Location, FVector, Velocity, FVector, Direction, FVector, Normal, FName, BoneName);

UCLASS(hidecategories=(Activation,"Components|Activation",Input,Collision), showcategories=("Input|MouseInput", "Input|TouchInput"))
class ENGINE_API AEmitter : public AActor
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(Category=Emitter, VisibleAnywhere, BlueprintReadOnly, meta=(ExposeFunctionCategories="Particles|Beam,Particles|Parameters,Particles,Effects|Components|ParticleSystem,Rendering,Activation,Components|Activation"))
	TSubobjectPtr<class UParticleSystemComponent> ParticleSystemComponent;

	UPROPERTY()
	uint32 bDestroyOnSystemFinish:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Emitter)
	uint32 bPostUpdateTickGroup:1;

	/** used to update status of toggleable level placed emitters on clients */
	UPROPERTY(replicatedUsing=OnRep_bCurrentlyActive)
	uint32 bCurrentlyActive:1;

	UPROPERTY(BlueprintAssignable)
	FParticleSpawnSignature OnParticleSpawn;

	UPROPERTY(BlueprintAssignable)
	FParticleBurstSignature OnParticleBurst;

	UPROPERTY(BlueprintAssignable)
	FParticleDeathSignature OnParticleDeath;

	UPROPERTY(BlueprintAssignable)
	FParticleCollisionSignature OnParticleCollide;

#if WITH_EDITORONLY_DATA

	UPROPERTY()
	TSubobjectPtr<class UBillboardComponent> SpriteComponent;

	UPROPERTY()
	TSubobjectPtr<class UArrowComponent> ArrowComponent;

#endif

	UFUNCTION()
	virtual void OnParticleSystemFinished(class UParticleSystemComponent* FinishedComponent);

	/** Replication Notification Callbacks */
	UFUNCTION()
	virtual void OnRep_bCurrentlyActive();

	// BEGIN DEPRECATED (use component functions now in level script)
	UFUNCTION(BlueprintCallable, Category=Particles, meta=(DeprecatedFunction))
	void Activate();
	UFUNCTION(BlueprintCallable, Category=Particles, meta=(DeprecatedFunction))
	void Deactivate();
	UFUNCTION(BlueprintCallable, Category=Particles, meta=(DeprecatedFunction))
	void ToggleActive();
	UFUNCTION(BlueprintCallable, Category=Particles, meta=(DeprecatedFunction))
	bool IsActive() const;
	UFUNCTION(BlueprintCallable, Category=Particles, meta=(DeprecatedFunction))
	virtual void SetTemplate(class UParticleSystem* NewTemplate);
	UFUNCTION(BlueprintCallable, Category="Particles|Parameters", meta=(DeprecatedFunction))
	void SetFloatParameter(FName ParameterName, float Param);
	UFUNCTION(BlueprintCallable, Category="Particles|Parameters", meta=(DeprecatedFunction))
	void SetVectorParameter(FName ParameterName, FVector Param);
	UFUNCTION(BlueprintCallable, Category="Particles|Parameters", meta=(DeprecatedFunction))
	void SetColorParameter(FName ParameterName, FLinearColor Param);
	UFUNCTION(BlueprintCallable, Category="Particles|Parameters", meta=(DeprecatedFunction))
	void SetActorParameter(FName ParameterName, class AActor* Param);
	UFUNCTION(BlueprintCallable, Category="Particles|Parameters", meta=(DeprecatedFunction))
	void SetMaterialParameter(FName ParameterName, class UMaterialInterface* Param);
	// END DEPRECATED


	void AutoPopulateInstanceProperties();

	// Begin UObject Interface
	virtual FString GetDetailedInfoInternal() const OVERRIDE;
	// End UObject Interface


	// Begin AActor interface
	virtual void PostActorCreated() OVERRIDE;
	virtual void PostInitializeComponents() OVERRIDE;
#if WITH_EDITOR
	virtual void CheckForErrors() OVERRIDE;
	virtual bool GetReferencedContentObjects( TArray<UObject*>& Objects ) const OVERRIDE;
	// End AActor interface

	/**
	 *	Called to reset the emitter actor in the level.
	 *	Intended for use in editor only
	 */
	void ResetInLevel();
#endif
};



