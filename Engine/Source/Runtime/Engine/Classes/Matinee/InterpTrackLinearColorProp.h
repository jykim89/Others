// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "InterpTrackLinearColorProp.generated.h"

UCLASS(MinimalAPI, meta=( DisplayName = "LinearColor Property Track" ) )
class UInterpTrackLinearColorProp : public UInterpTrackLinearColorBase
{
	GENERATED_UCLASS_BODY()

	/** Name of property in Group  AActor  which this track mill modify over time. */
	UPROPERTY(Category=InterpTrackLinearColorProp, VisibleAnywhere)
	FName PropertyName;


	// Begin UInterpTrack interface.
	virtual int32 AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode) OVERRIDE;
	virtual bool CanAddKeyframe( UInterpTrackInst* TrackInst ) OVERRIDE;
	virtual void UpdateKeyframe(int32 KeyIndex, UInterpTrackInst* TrInst) OVERRIDE;
	virtual void PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst) OVERRIDE;
	virtual void UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump) OVERRIDE;
	virtual const FString	GetEdHelperClassName() const OVERRIDE;
	virtual const FString	GetSlateHelperClassName() const OVERRIDE;
	virtual class UTexture2D* GetTrackIcon() const OVERRIDE;
	// End UInterpTrack interface.
};



