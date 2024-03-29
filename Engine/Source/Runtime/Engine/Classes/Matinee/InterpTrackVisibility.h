// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "InterpTrackVisibility.generated.h"

/**
 *	This track implements support for setting or toggling the visibility of the associated actor
 */



/** Visibility track actions */
UENUM()
enum EVisibilityTrackAction
{
	/** Hides the object */
	EVTA_Hide,
	/** Shows the object */
	EVTA_Show,
	/** Toggles visibility of the object */
	EVTA_Toggle,
	EVTA_MAX,
};

/** Required condition for firing this event */
UENUM()
enum EVisibilityTrackCondition
{
	/** Always play this event */
	EVTC_Always,
	/** Only play this event when extreme content (gore) is enabled */
	EVTC_GoreEnabled,
	/** Only play this event when extreme content (gore) is disabled */
	EVTC_GoreDisabled,
	EVTC_MAX,
};

/** Information for one toggle in the track. */
USTRUCT()
struct FVisibilityTrackKey
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	float Time;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=VisibilityTrackKey)
	TEnumAsByte<enum EVisibilityTrackAction> Action;

	/** Condition that must be satisfied for this key event to fire */
	UPROPERTY()
	TEnumAsByte<enum EVisibilityTrackCondition> ActiveCondition;


	FVisibilityTrackKey()
		: Time(0)
		, Action(0)
		, ActiveCondition(0)
	{
	}

};

UCLASS(MinimalAPI, meta=( DisplayName = "Visibility Track" ) )
class UInterpTrackVisibility : public UInterpTrack
{
	GENERATED_UCLASS_BODY()

	/** Array of events to fire off. */
	UPROPERTY()
	TArray<struct FVisibilityTrackKey> VisibilityTrack;

	/** If events should be fired when passed playing the sequence forwards. */
	UPROPERTY(EditAnywhere, Category=InterpTrackVisibility)
	uint32 bFireEventsWhenForwards:1;

	/** If events should be fired when passed playing the sequence backwards. */
	UPROPERTY(EditAnywhere, Category=InterpTrackVisibility)
	uint32 bFireEventsWhenBackwards:1;

	/** If true, events on this track are fired even when jumping forwads through a sequence - for example, skipping a cinematic. */
	UPROPERTY(EditAnywhere, Category=InterpTrackVisibility)
	uint32 bFireEventsWhenJumpingForwards:1;


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
	virtual void DrawTrack( FCanvas* Canvas, UInterpGroup* Group, const FInterpTrackDrawParams& Params ) OVERRIDE;
	// End UInterpTrack interface.

	/** Shows or hides the actor */
	void HideActor( AActor* Actor, bool bHidden );
};



