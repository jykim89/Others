// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineAchievementsInterface.h"

/**
 *	IOnlineAchievements - Interface class for acheivements
 */
class FOnlineAchievementsIOS : public IOnlineAchievements
{
private:

	/** Reference to the main GameCenter subsystem */
	class FOnlineSubsystemIOS* IOSSubsystem;

	/** hide the default constructor, we need a reference to our OSS */
	FOnlineAchievementsIOS() {};

	// IOS only supports loading achievements for local player. This is where they are cached.
	TArray< FOnlineAchievement > Achievements;

	// Cached achievement descriptions for an Id
	TMap< FString, FOnlineAchievementDesc > AchievementDescriptions;


public:

	// Begin IOnlineAchievements interface
	virtual void WriteAchievements(const FUniqueNetId& PlayerId, FOnlineAchievementsWriteRef& WriteObject, const FOnAchievementsWrittenDelegate& Delegate = FOnAchievementsWrittenDelegate()) OVERRIDE;
	virtual void QueryAchievements(const FUniqueNetId & PlayerId, const FOnQueryAchievementsCompleteDelegate & Delegate = FOnQueryAchievementsCompleteDelegate()) OVERRIDE;
	virtual void QueryAchievementDescriptions( const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate & Delegate = FOnQueryAchievementsCompleteDelegate() ) OVERRIDE;
	virtual EOnlineCachedResult::Type GetCachedAchievement(const FUniqueNetId& PlayerId, const FString& AchievementId, FOnlineAchievement& OutAchievement) OVERRIDE;
	virtual EOnlineCachedResult::Type GetCachedAchievements(const FUniqueNetId& PlayerId, TArray<FOnlineAchievement> & OutAchievements) OVERRIDE;
	virtual EOnlineCachedResult::Type GetCachedAchievementDescription(const FString& AchievementId, FOnlineAchievementDesc& OutAchievementDesc) OVERRIDE;
#if !UE_BUILD_SHIPPING
	virtual bool ResetAchievements( const FUniqueNetId& PlayerId ) OVERRIDE;
#endif // !UE_BUILD_SHIPPING
	// End IOnlineAchievements interface


	/**
	 * Constructor
	 *
	 * @param InSubsystem - A reference to the owning subsystem
	 */
	FOnlineAchievementsIOS( class FOnlineSubsystemIOS* InSubsystem );

	
	/**
	 * Default destructor
	 */
	virtual ~FOnlineAchievementsIOS() {}
};

typedef TSharedPtr<FOnlineAchievementsIOS, ESPMode::ThreadSafe> FOnlineAchievementsIOSPtr;