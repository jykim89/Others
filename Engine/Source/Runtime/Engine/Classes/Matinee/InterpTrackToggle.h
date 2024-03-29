// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "InterpTrackToggle.generated.h"

/**
 *	A track containing toggle actions that are triggered as its played back. 
 */




/** Enumeration indicating toggle action	*/
UENUM()
enum ETrackToggleAction
{
	ETTA_Off,
	ETTA_On,
	ETTA_Toggle,
	ETTA_Trigger,
	ETTA_MAX,
};

/** Information for one toggle in the track. */
USTRUCT()
struct FToggleTrackKey
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	float Time;

	UPROPERTY(EditAnywhere, Category=ToggleTrackKey)
	TEnumAsByte<enum ETrackToggleAction> ToggleAction;


	FToggleTrackKey()
		: Time(0)
		, ToggleAction(0)
	{
	}

};

UCLASS(MinimalAPI, meta=( DisplayName = "Toggle Track" ) )
class UInterpTrackToggle : public UInterpTrack
{
	GENERATED_UCLASS_BODY()

	/** Array of events to fire off. */
	UPROPERTY()
	TArray<struct FToggleTrackKey> ToggleTrack;

	/** 
	 *	If true, the track will call ActivateSystem on the emitter each update (the old 'incorrect' behavior).
	 *	If false (the default), the System will only be activated if it was previously inactive.
	 */
	UPROPERTY(EditAnywhere, Category=InterpTrackToggle)
	uint32 bActivateSystemEachUpdate:1;

	/** 
	 *	If true, the track will activate the system w/ the 'Just Attached' flag.
	 */
	UPROPERTY(EditAnywhere, Category=InterpTrackToggle)
	uint32 bActivateWithJustAttachedFlag:1;

	/** If events should be fired when passed playing the sequence forwards. */
	UPROPERTY(EditAnywhere, Category=InterpTrackToggle)
	uint32 bFireEventsWhenForwards:1;

	/** If events should be fired when passed playing the sequence backwards. */
	UPROPERTY(EditAnywhere, Category=InterpTrackToggle)
	uint32 bFireEventsWhenBackwards:1;

	/** If true, events on this track are fired even when jumping forwads through a sequence - for example, skipping a cinematic. */
	UPROPERTY(EditAnywhere, Category=InterpTrackToggle)
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
	// Begin UInterpTrack interface.
};



