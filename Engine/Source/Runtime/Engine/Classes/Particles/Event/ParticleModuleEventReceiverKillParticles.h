// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "ParticleModuleEventReceiverKillParticles.generated.h"

UCLASS(editinlinenew, hidecategories=Object, meta=(DisplayName = "EventReceiver Kill All"))
class UParticleModuleEventReceiverKillParticles : public UParticleModuleEventReceiverBase
{
	GENERATED_UCLASS_BODY()

	/** If true, stop this emitter from spawning as well. */
	UPROPERTY(EditAnywhere, Category=ParticleModuleEventReceiverKillParticles)
	uint32 bStopSpawning:1;


	// Begin UParticleModuleEventBase Interface
	virtual bool ProcessParticleEvent(FParticleEmitterInstance* Owner, FParticleEventData& InEvent, float DeltaTime) OVERRIDE;
	// End UParticleModuleEventBase Interface
};



