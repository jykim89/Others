// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "InterpTrackBoolProp.generated.h"

/** Information for one event in the track. */
USTRUCT()
struct FBoolTrackKey
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	float Time;

	UPROPERTY(EditAnywhere, Category=BoolTrackKey)
	uint32 Value:1;


	FBoolTrackKey()
		: Time(0)
		, Value(false)
	{
	}

};

UCLASS(MinimalAPI, meta=( DisplayName = "Bool Property Track" ) )
class UInterpTrackBoolProp : public UInterpTrack
{
	GENERATED_UCLASS_BODY()

	/** Array of booleans to set. */
	UPROPERTY()
	TArray<struct FBoolTrackKey> BoolTrack;

	/** Name of property in Group  AActor  which this track will modify over time. */
	UPROPERTY(Category=InterpTrackBoolProp, VisibleAnywhere)
	FName PropertyName;


	// Begin UInterpTrack Interface
	virtual int32 GetNumKeyframes() const OVERRIDE;
	virtual float GetTrackEndTime() const OVERRIDE;
	virtual float GetKeyframeTime( int32 KeyIndex ) const OVERRIDE;
	virtual int32 GetKeyframeIndex( float KeyTime ) const OVERRIDE;
	virtual void GetTimeRange( float& StartTime, float& EndTime ) const OVERRIDE;
	virtual int32 SetKeyframeTime( int32 KeyIndex, float NewKeyTime, bool bUpdateOrder = true ) OVERRIDE;
	virtual void RemoveKeyframe( int32 KeyIndex ) OVERRIDE;
	virtual int32 DuplicateKeyframe( int32 KeyIndex, float NewKeyTime, UInterpTrack* ToTrack = NULL ) OVERRIDE;
	virtual bool GetClosestSnapPosition( float InPosition, TArray<int32>& IgnoreKeys, float& OutPosition ) OVERRIDE;
	virtual int32 AddKeyframe( float Time, UInterpTrackInst* TrackInst, EInterpCurveMode InitInterpMode ) OVERRIDE;
	virtual bool CanAddKeyframe( UInterpTrackInst* TrackInst ) OVERRIDE;
	virtual void UpdateKeyframe( int32 KeyIndex, UInterpTrackInst* TrackInst ) OVERRIDE;
	virtual void PreviewUpdateTrack( float NewPosition, UInterpTrackInst* TrackInst ) OVERRIDE;
	virtual void UpdateTrack( float NewPosition, UInterpTrackInst* TrackInst, bool bJump ) OVERRIDE;
	virtual bool AllowStaticActors() OVERRIDE { return true; }
	virtual const FString GetEdHelperClassName() const OVERRIDE;
	virtual const FString GetSlateHelperClassName() const OVERRIDE;
	virtual class UTexture2D* GetTrackIcon() const OVERRIDE;
	// End UInterpTrack Interface
};



