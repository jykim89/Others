// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "OnlineLeaderboardInterface.h"

/**
 * Class used to test the friends interface
 */
 class FTestLeaderboardInterface : public FTickerObjectBase
 {
	/** The subsystem that was requested to be tested or the default if empty */
	const FString Subsystem;

	/** Keep track of success across all functions and callbacks */
	bool bOverallSuccess;

	/** Logged in UserId */
	TSharedPtr<FUniqueNetId> UserId;

	/** Convenient access to the leaderboard interfaces */
	IOnlineLeaderboardsPtr Leaderboards;

	/** Leaderboard read object */
	FOnlineLeaderboardReadPtr ReadObject;

	/** Delegate called when leaderboard data has been successfully committed to the backend service */ 
	FOnLeaderboardFlushCompleteDelegate LeaderboardFlushDelegate;
	/** Delegate called when a leaderboard has been successfully read */
	FOnLeaderboardReadCompleteDelegate LeaderboardReadCompleteDelegate;

	/** Current phase of testing */
	int32 TestPhase;
	/** Last phase of testing triggered */
	int32 LastTestPhase;

	/** Hidden on purpose */
	FTestLeaderboardInterface()
		: Subsystem()
	{
	}

	/**
	 *	Write out some test data to a leaderboard
	 */
	void WriteLeaderboards();

	/**
	 *	Delegate called when leaderboard data has been successfully committed to the backend service
	 */
	void OnLeaderboardFlushComplete(FName SessionName, bool bWasSuccessful);

	/**
	 *	Commit the leaderboard writes to the backend service
	 */
	void FlushLeaderboards();

	/**
	 *	Delegate called when a leaderboard has been successfully read
	 */
	void OnLeaderboardReadComplete(bool bWasSuccessful);

	/**
	 *	Read in some predefined data from a leaderboard
	 */
	void ReadLeaderboards();

 public:
	/**
	 * Sets the subsystem name to test
	 *
	 * @param InSubsystem the subsystem to test
	 */
	FTestLeaderboardInterface(const FString& InSubsystem) :
		Subsystem(InSubsystem),
		bOverallSuccess(true),
		Leaderboards(NULL),
		TestPhase(0),
		LastTestPhase(-1)
	{
	}

	~FTestLeaderboardInterface()
	{
		Leaderboards = NULL;
	}

	// FTickerObjectBase

	bool Tick( float DeltaTime ) OVERRIDE;

	// FTestLeaderboardInterface

	/**
	 * Kicks off all of the testing process
	 */
	void Test(class UWorld* InWorld);
 };
