// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/**
 *
 * Group for controlling properties of a 'player' in the game. This includes switching the player view between different cameras etc.
 */

#pragma once
#include "InterpGroupDirector.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UInterpGroupDirector : public UInterpGroup
{
	GENERATED_UCLASS_BODY()

	// Begin UInterpGroup Interface
	virtual AActor* SelectGroupActor( class UInterpGroupInst* GrInst, bool bDeselectActors ) OVERRIDE;
	virtual AActor* DeselectGroupActor( class UInterpGroupInst* GrInst ) OVERRIDE;
	// End UInterpGroup Interface

	/** @return the director track inside this Director group - if present. */
	ENGINE_API class UInterpTrackDirector* GetDirectorTrack();

	/** @return the fade track inside this Director group - if present. */
	ENGINE_API class UInterpTrackFade* GetFadeTrack();

	/** @return the slomo track inside this Director group - if present. */
	ENGINE_API class UInterpTrackSlomo* GetSlomoTrack();

	/** @return this director group's Color Scale track, if it has one */
	ENGINE_API class UInterpTrackColorScale* GetColorScaleTrack();

	/** @return this director group's Audio Master track, if it has one */
	class UInterpTrackAudioMaster* GetAudioMasterTrack();
};



