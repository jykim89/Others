// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AnimNotifyState_TimedParticleEffect.generated.h"

// Timed Particle Effect Notify
// Allows a looping particle effect to be played in an animation that will activate
// at the beginning of the notify and deactivate at the end.
UCLASS(MinimalAPI, Blueprintable, meta = (DisplayName = "Timed Particle Effect"))
class UAnimNotifyState_TimedParticleEffect : public UAnimNotifyState
{
	GENERATED_UCLASS_BODY()

	// The particle system template to use when spawning the particle component
	UPROPERTY(EditAnywhere, Category = ParticleSystem, meta = (ToolTip = "The particle system to spawn for the notify state"))
	UParticleSystem* PSTemplate;

	// The socket within our mesh component to attach to when we spawn the particle componnet
	UPROPERTY(EditAnywhere, Category = ParticleSystem, meta = (ToolTip = "The socket or bone to attach the system to"))
	FName SocketName;

	// Offset from the socket / bone location
	UPROPERTY(EditAnywhere, Category = ParticleSystem, meta = (ToolTip = "Offset from the socket or bone to place the particle system"))
	FVector LocationOffset;

	// Offset from the socket / bone rotation
	UPROPERTY(EditAnywhere, Category = ParticleSystem, meta = (ToolTip = "Rotation offset from the socket or bone for the particle system"))
	FRotator RotationOffset;

	// Whether or not we destroy the component at the end of the notify or instead just stop
	// the emitters.
	UPROPERTY(EditAnywhere, Category = ParticleSystem, meta = (DisplayName = "Destroy Immediately", ToolTip = "Whether the particle system should be immediately destroyed at the end of the notify state or be allowed to finish"))
	bool bDestroyAtEnd;

#if WITH_EDITORONLY_DATA
	// The following arrays are used to handle property changes during a state. Because we can't
	// store any stateful data here we can't know which emitter is ours. The best metric we have
	// is an emitter on our Mesh Component with the same template and socket name we have defined.
	// Because these can change at any time we need to track previous versions when we are in an
	// editor build. Refactor when stateful data is possible, tracking our component instead.
	UPROPERTY(transient)
	TArray<UParticleSystem*> PreviousPSTemplates;

	UPROPERTY(transient)
	TArray<FName> PreviousSocketNames;

	virtual void PreEditChange(UProperty* PropertyAboutToChange) OVERRIDE;
#endif

	virtual void NotifyBegin(class USkeletalMeshComponent * MeshComp, class UAnimSequence * AnimSeq) OVERRIDE;
	virtual void NotifyTick(class USkeletalMeshComponent * MeshComp, class UAnimSequence * AnimSeq, float FrameDeltaTime) OVERRIDE;
	virtual void NotifyEnd(class USkeletalMeshComponent * MeshComp, class UAnimSequence * AnimSeq) OVERRIDE;

private:
	bool ValidateParameters(USkeletalMeshComponent* MeshComp);
};