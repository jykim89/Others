// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "InterpTrackInstVectorMaterialParam.generated.h"

UCLASS()
class UInterpTrackInstVectorMaterialParam : public UInterpTrackInst
{
	GENERATED_UCLASS_BODY()

	/** MIDs we're using to set the desired parameter. */
	UPROPERTY()
	TArray<class UMaterialInstanceDynamic*> MaterialInstances;

	/** Saved values for restoring state when exiting Matinee. */
	UPROPERTY()
	TArray<FVector> ResetVectors;

	/** Primitive components on which materials have been overridden. */
	UPROPERTY()
	TArray<struct FPrimitiveMaterialRef> PrimitiveMaterialRefs;

	/** Track we are an instance of - used in the editor to propagate changes to the track's Materials array immediately. */
	UPROPERTY()
	class UInterpTrackVectorMaterialParam* InstancedTrack;


	// Begin UInterpTrackInst Instance
	virtual void InitTrackInst(UInterpTrack* Track) OVERRIDE;
	virtual void TermTrackInst(UInterpTrack* Track) OVERRIDE;
	virtual void SaveActorState(UInterpTrack* Track) OVERRIDE;
	virtual void RestoreActorState(UInterpTrack* Track) OVERRIDE;
	// End UInterpTrackInst Instance
};

