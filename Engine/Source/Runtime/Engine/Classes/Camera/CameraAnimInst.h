 // Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/**
 *	A CameraAnimInst is an active instance of a CameraAnim.
 */

#pragma once
#include "CameraAnimInst.generated.h"

UCLASS(notplaceable, BlueprintType, transient)
class ENGINE_API UCameraAnimInst : public UObject
{
	GENERATED_UCLASS_BODY()

	/** which CameraAnim this is an instance of */
	UPROPERTY()
	class UCameraAnim* CamAnim;

protected:
	/** the UInterpGroupInst used to do the interpolation */
	UPROPERTY(instanced)
	TSubobjectPtr<class UInterpGroupInst> InterpGroupInst;

public:
	/** Current time for the animation */
	float CurTime;

protected:
	/** True if the animation should loop, false otherwise. */
	uint32 bLooping:1;

public:
	/** True if the animation has finished, false otherwise. */
	uint32 bFinished:1;

	/** True if it's ok for the system to auto-release this instance upon completion. */
	uint32 bAutoReleaseWhenFinished:1;

protected:
	/** Time to interpolate in from zero, for smooth starts. */
	float BlendInTime;

	/** Time to interpolate out to zero, for smooth finishes. */
	float BlendOutTime;

	/** True if currently blending in. */
	uint32 bBlendingIn:1;

	/** True if currently blending out. */
	uint32 bBlendingOut:1;

	/** Current time for the blend-in.  I.e. how long we have been blending. */
	float CurBlendInTime;

	/** Current time for the blend-out.  I.e. how long we have been blending. */
	float CurBlendOutTime;

public:
	/** Multiplier for playback rate.  1.0 = normal. */
	UPROPERTY(BlueprintReadWrite, Category=CameraAnimInst)
	float PlayRate;

	/** "Intensity" value used to scale keyframe values. */
	float BasePlayScale;

	/** A supplemental scale factor, allowing external systems to scale this anim as necessary.  This is reset to 1.f each frame. */
	float TransientScaleModifier;

	/* Number in range [0..1], controlling how much this influence this instance should have. */
	float CurrentBlendWeight;

protected:
	/** How much longer to play the anim, if a specific duration is desired.  Has no effect if 0.  */
	float RemainingTime;


public:
	/** cached movement track from the currently playing anim so we don't have to go find it every frame */
	UPROPERTY(transient)
	class UInterpTrackMove* MoveTrack;

	UPROPERTY(transient)
	class UInterpTrackInstMove* MoveInst;

	UPROPERTY()
	TEnumAsByte<ECameraAnimPlaySpace::Type> PlaySpace;

	/** The user-defined space for UserDefined PlaySpace */
	FMatrix UserPlaySpaceMatrix;

	/** Camera Anim debug variable to trace back to previous location **/
	FVector LastCameraLoc;

	/** transform of initial anim key, used for treating anim keys as offsets from initial key */
	FTransform InitialCamToWorld;

	/** FOV of the initial anim key, used for treating fov keys as offsets from initial key. */
	float InitialFOV;

	/**
	 * Starts this instance playing the specified CameraAnim.
	 *
	 * @param CamAnim			The animation that should play on this instance.
	 * @param CamActor			The  AActor  that will be modified by this animation.
	 * @param InRate			How fast to play the animation.  1.f is normal.
	 * @param InScale			How intense to play the animation.  1.f is normal.
	 * @param InBlendInTime		Time over which to linearly ramp in.
	 * @param InBlendOutTime	Time over which to linearly ramp out.
	 * @param bInLoop			Whether or not to loop the animation.
	 * @param bRandomStartTime	Whether or not to choose a random time to start playing.  Only really makes sense for bLoop = true;
	 * @param Duration			optional specific playtime for this animation.  This is total time, including blends.
	 */
	void Play(class UCameraAnim* Anim, class AActor* CamActor, float InRate, float InScale, float InBlendInTime, float InBlendOutTime, bool bInLoop, bool bRandomStartTime, float Duration = 0.f);
	
	/** Updates this active instance with new parameters. */
	void Update(float NewRate, float NewScale, float NewBlendInTime, float NewBlendOutTime, float NewDuration = 0.f);
	
	/** advances the animation by the specified time - updates any modified interp properties, moves the group actor, etc */
	void AdvanceAnim(float DeltaTime, bool bJump);
	
	/** Stops this instance playing whatever animation it is playing. */
	UFUNCTION(BlueprintCallable, Category = CameraAnimInst)
	void Stop(bool bImmediate = false);
	
	/** Applies given scaling factor to the playing animation for the next update only. */
	void ApplyTransientScaling(float Scalar);
	
	/** Sets this anim to play in an alternate playspace */
	void SetPlaySpace(ECameraAnimPlaySpace::Type NewSpace, FRotator UserPlaySpace = FRotator::ZeroRotator);

	/** Changes the running duration of this active anim, while maintaining playback position. */
	UFUNCTION(BlueprintCallable, Category=CameraAnimInst)
	void SetDuration(float NewDuration);

	
};



