// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "InterpTrackParticleReplay.generated.h"

/**
 *
 * 
 *	This track implements support for creating and playing back captured particle system data
 */



















































/** Data for a single key in this track */
USTRUCT()
struct FParticleReplayTrackKey
{
	GENERATED_USTRUCT_BODY()

	/** Position along timeline */
	UPROPERTY()
	float Time;

	/** Time length this clip should be captured/played for */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ParticleReplayTrackKey)
	float Duration;

	/** Replay clip ID number that identifies the clip we should capture to or playback from */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ParticleReplayTrackKey)
	int32 ClipIDNumber;


	FParticleReplayTrackKey()
		: Time(0)
		, Duration(0)
		, ClipIDNumber(0)
	{
	}

};

UCLASS(MinimalAPI, meta=( DisplayName = "Particle Replay Track" ) )
class UInterpTrackParticleReplay : public UInterpTrack
{
	GENERATED_UCLASS_BODY()

	/** Array of keys */
	UPROPERTY(editinline)
	TArray<struct FParticleReplayTrackKey> TrackKeys;

#if WITH_EDITORONLY_DATA
	/** True in the editor if track should be used to capture replay frames instead of play them back */
	UPROPERTY(transient)
	uint32 bIsCapturingReplay:1;

	/** Current replay fixed time quantum between frames (one over frame rate) */
	UPROPERTY(transient)
	float FixedTimeStep;

#endif // WITH_EDITORONLY_DATA

	// Begin UInterpTrack interface.
	virtual int32 GetNumKeyframes() const OVERRIDE;
	virtual void GetTimeRange(float& StartTime, float& EndTime) const OVERRIDE;
	virtual float GetTrackEndTime() const OVERRIDE;
	virtual float GetKeyframeTime(int32 KeyIndex) const OVERRIDE;
	virtual int32 GetKeyframeIndex( float KeyTime ) const OVERRIDE;
	virtual int32 AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode) OVERRIDE;
	virtual int32 SetKeyframeTime(int32 KeyIndex, float NewKeyTime, bool bUpdateOrder=true) OVERRIDE;
	virtual void RemoveKeyframe(int32 KeyIndex) OVERRIDE;
	virtual int32 DuplicateKeyframe(int32 KeyIndex, float NewKeyTime, UInterpTrack* ToTrack = NULL) OVERRIDE;
	virtual bool GetClosestSnapPosition(float InPosition, TArray<int32> &IgnoreKeys, float& OutPosition) OVERRIDE;
	virtual void PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst) OVERRIDE;
	virtual void UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump) OVERRIDE;
	virtual const FString	GetEdHelperClassName() const OVERRIDE;
	virtual const FString	GetSlateHelperClassName() const OVERRIDE;
	virtual class UTexture2D* GetTrackIcon() const OVERRIDE;
	virtual bool AllowStaticActors() OVERRIDE { return true; }
	virtual void DrawTrack( FCanvas* Canvas, UInterpGroup* Group, const FInterpTrackDrawParams& Params );
	// End UInterpTrack interface.

	// Begin FInterpEdInputInterface Interface
	virtual void BeginDrag(FInterpEdInputData &InputData) OVERRIDE;
	virtual void EndDrag(FInterpEdInputData &InputData) OVERRIDE;
	virtual EMouseCursor::Type GetMouseCursor(FInterpEdInputData &InputData) OVERRIDE;
	virtual void ObjectDragged(FInterpEdInputData& InputData) OVERRIDE;
	// End FInterpEdInputInterface Interface

};



