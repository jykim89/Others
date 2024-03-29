// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ThrottleManager.cpp: Implements the FSlateThrottleManager class.
=============================================================================*/

#include "SlateCorePrivatePCH.h"


/* FSlateThrottleManager structors
 *****************************************************************************/

FSlateThrottleManager::FSlateThrottleManager( )
	: CVarAllowThrottle(TEXT("Slate.bAllowThrottling"), bShouldThrottle, TEXT("Allow Slate to throttle parts of the engine to ensure the UI is responsive") )
	, bShouldThrottle(1)
	, ThrottleCount(0)
{ }


/* FSlateThrottleManager interface
 *****************************************************************************/

FThrottleRequest FSlateThrottleManager::EnterResponsiveMode( )
{
	// Increase the number of active throttle requests
	++ThrottleCount;

	// Create a new handle for the request and return it to the user so they can close it later
	FThrottleRequest NewHandle;
	NewHandle.Index = ThrottleCount;

	return NewHandle;
}


bool FSlateThrottleManager::IsAllowingExpensiveTasks( ) const
{
	// Expensive tasks are allowed if the number of active throttle requests is zero
	// or we always always allow it due to a CVar
	return (ThrottleCount == 0) || !bShouldThrottle;
}


void FSlateThrottleManager::LeaveResponsiveMode( FThrottleRequest& InHandle )
{
	if( InHandle.IsValid() )
	{
		// Decrement throttle count. If it becomes zero we are no longer throttling
		--ThrottleCount;
		check(ThrottleCount >= 0);

		InHandle.Index = INDEX_NONE;
	}
}


/* FSlateThrottleManager static functions
 *****************************************************************************/

FSlateThrottleManager& FSlateThrottleManager::Get( )
{
	static FSlateThrottleManager* Instance = nullptr;

	if (Instance == nullptr)
	{
		Instance = new FSlateThrottleManager;
	}

	return *Instance;
}
