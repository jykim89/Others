// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	 GestureRecognizer - handles detecting when gestures happen
=============================================================================*/

#include "EnginePrivate.h"

void FGestureRecognizer::DetectGestures(const FVector (&Touches)[EKeys::NUM_TOUCH_KEYS], UPlayerInput* PlayerInput, float DeltaTime)
{
	// how many fingers currently held down?
	int32 TouchCount = 0;
	for (int32 TouchIndex = 0; TouchIndex < EKeys::NUM_TOUCH_KEYS; TouchIndex++)
	{
		if (Touches[TouchIndex].Z != 0)
		{
			TouchCount++;
		}
	}

	// place new anchor points
	for (int32 Index = 0; Index < ARRAY_COUNT(AnchorPoints); Index++)
	{
		if (PreviousTouchCount < Index + 1 && TouchCount >= Index + 1)
		{
			AnchorPoints[Index] = FVector2D(Touches[Index].X, Touches[Index].Y);
		}
	}

	// handle different types of swipes
	if (TouchCount >= 2)
	{
		float* CurrentAlpha = CurrentGestureValues.Find(EKeys::Gesture_Pinch);
		if (CurrentAlpha == NULL)
		{
			// remember the starting spots
			AnchorDistance = (AnchorPoints[1] - AnchorPoints[0]).Size();

			// alpha of 1 is initial pinch anchor distance
			CurrentGestureValues.Add(EKeys::Gesture_Pinch, 1.0f);

			HandleGesture(PlayerInput, EKeys::Gesture_Pinch, true, false);
		}
		else
		{
			// calculate current alpha
			float NewDistance = (FVector2D(Touches[1].X, Touches[1].Y) - FVector2D(Touches[0].X, Touches[0].Y)).Size();
			*CurrentAlpha = NewDistance / AnchorDistance;

			HandleGesture(PlayerInput, EKeys::Gesture_Pinch, false, false);
		}
	}

	// cancel any 2 finger gestures
	if (PreviousTouchCount >= 2 && TouchCount < 2)
	{
		HandleGesture(PlayerInput, EKeys::Gesture_TwoFingerSwipeLeftRight, false, true);
		HandleGesture(PlayerInput, EKeys::Gesture_TwoFingerSwipeUpDown, false, true);
		HandleGesture(PlayerInput, EKeys::Gesture_Pinch, false, true);
	}

	if (PreviousTouchCount == 0 && TouchCount == 1)
	{
		// initialize the flick
		FlickTime = 0.0f;
	}
	else if (PreviousTouchCount == 1 && TouchCount == 1)
	{
		// remember the position for when we let go
		FlickCurrent = FVector2D(Touches[0].X, Touches[0].Y);
		FlickTime += DeltaTime;
	}
	else if (PreviousTouchCount >= 1 && TouchCount == 0)
	{
		// must be a fast flick
		if (FlickTime < 0.25f && (FlickCurrent - AnchorPoints[0]).Size() > 100.f)
		{
			// this is the angle from +X in screen space, meaning right is 0, up is 90, left is 180, down is 270
			float Angle = FMath::Atan2(-(FlickCurrent.Y - AnchorPoints[0].Y), FlickCurrent.X - AnchorPoints[0].X) * 180.f / PI;
			Angle = FRotator::ClampAxis(Angle);

			// flicks are one-shot, so we start and end in the same frame
			CurrentGestureValues.Add(EKeys::Gesture_Flick, Angle);
			HandleGesture(PlayerInput, EKeys::Gesture_Flick, true, true);
		}
	}

	if (PreviousTouchCount >= 1 && TouchCount == 0)
	{
		// cancel other 1 finger gestures
		HandleGesture(PlayerInput, EKeys::Gesture_SwipeLeftRight, false, true);
		HandleGesture(PlayerInput, EKeys::Gesture_SwipeUpDown, false, true);
	}

	// remember for next frame
	PreviousTouchCount = TouchCount;
}

void FGestureRecognizer::HandleGesture(UPlayerInput* PlayerInput, FKey Gesture, bool bStarted, bool bEnded)
{
	float* Value = CurrentGestureValues.Find(Gesture);
	if (Value)
	{
		const EInputEvent GestureEvent = (bStarted ? IE_Pressed : bEnded ? IE_Released : IE_Repeat);
		PlayerInput->InputGesture(Gesture, GestureEvent, *Value);

		// remove it if the gesture is complete
		if (bEnded)
		{
			CurrentGestureValues.Remove(Gesture);
		}
	}
}

