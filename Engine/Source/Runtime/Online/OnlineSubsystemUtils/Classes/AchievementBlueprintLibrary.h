// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AchievementBlueprintLibrary.generated.h"

// Library of synchronous achievement calls
UCLASS()
class ONLINESUBSYSTEMUTILS_API UAchievementBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/**
	 * Gets the status of an achievement ID (you must call CacheAchievements first to cache them)
	 *
	 * @param AchievementID - The id of the achievement we are looking up
	 * @param bFoundID - If the ID was found in the cache (if not, none of the other values are meaningful)
	 * @param Progress - The progress amount of the achievement
	 */
	UFUNCTION(BlueprintCallable, Category = "Online|Achievements")
	static void GetCachedAchievementProgress(APlayerController* PlayerController, FName AchievementID, /*out*/ bool& bFoundID, /*out*/ float& Progress);

	/**
	 * Get the description for an achievement ID (you must call CacheAchievementDescriptions first to cache them)
	 *
	 * @param AchievementID - The id of the achievement we are searching for data of
	 * @param bFoundID - If the ID was found in the cache (if not, none of the other values are meaningful)
	 * @param Title - The localized title of the achievement
	 * @param LockedDescription - The localized locked description of the achievement
	 * @param UnlockedDescription - The localized unlocked description of the achievement
	 * @param bHidden - Whether the achievement is hidden
	 */
	UFUNCTION(BlueprintCallable, Category="Online|Achievements")
	static void GetCachedAchievementDescription(APlayerController* PlayerController, FName AchievementID, /*out*/ bool& bFoundID, /*out*/ FText& Title, /*out*/ FText& LockedDescription, /*out*/ FText& UnlockedDescription, /*out*/ bool& bHidden);
};
