// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AndroidRuntimeSettings.generated.h"

UENUM()
namespace EAndroidScreenOrientation
{
	enum Type
	{
		// Portrait orientation (the display is taller than it is wide)
		Portrait UMETA(ManifestValue = "portrait"),

		// Portrait orientation rotated 180 degrees
		ReversePortrait UMETA(ManifestValue = "reversePortrait"),

		// Use either portrait or reverse portrait orientation, based on the device orientation sensor
		SensorPortrait UMETA(ManifestValue = "sensorPortrait"),

		// Landscape orientation (the display is wider than it is tall)
		Landscape UMETA(ManifestValue = "landscape"),

		// Landscape orientation rotated 180 degrees
		ReverseLandscape UMETA(ManifestValue = "reverseLandscape"),

		// Use either landscape or reverse landscape orientation, based on the device orientation sensor
		SensorLandscape UMETA(ManifestValue = "sensorLandscape"),

		// Use any orientation the device normally supports, based on the device orientation sensor
		Sensor UMETA(ManifestValue = "sensor"),

		// Use any orientation (including ones the device wouldn't choose in Sensor mode), based on the device orientation sensor
		FullSensor UMETA(ManifestValue = "fullSensor"),
	};
}

/**
 * Holds the game-specific achievement name and corresponding ID from Google Play services.
 */
USTRUCT()
struct FGooglePlayAchievementMapping
{
	GENERATED_USTRUCT_BODY()

	// The game-specific achievement name (the one passed in to WriteAchievement calls)
	UPROPERTY(EditAnywhere, Category = GooglePlayServices)
	FString Name;

	// The ID of the corresponding achievement, generated by the Google Play developer console.
	UPROPERTY(EditAnywhere, Category = GooglePlayServices)
	FString AchievementID;
};

/**
 * Holds the game-specific leaderboard name and corresponding ID from Google Play services.
 */
USTRUCT()
struct FGooglePlayLeaderboardMapping
{
	GENERATED_USTRUCT_BODY()

	// The game-specific leaderboard name (the one passed in to WriteLeaderboards calls)
	UPROPERTY(EditAnywhere, Category = GooglePlayServices)
	FString Name;

	// The ID of the corresponding leaderboard, generated by the Google Play developer console.
	UPROPERTY(EditAnywhere, Category = GooglePlayServices)
	FString LeaderboardID;
};

/**
 * Implements the settings for the Android runtime platform.
 */
UCLASS(config=Engine, defaultconfig)
class ANDROIDRUNTIMESETTINGS_API UAndroidRuntimeSettings : public UObject
{
public:
	GENERATED_UCLASS_BODY()

	// The permitted orientation or orientations of the application on the device
	UPROPERTY(GlobalConfig, EditAnywhere, Category = AppManifest)
	TEnumAsByte<EAndroidScreenOrientation::Type> Orientation;

	// Should Google Play support be enabled?
	UPROPERTY(GlobalConfig, EditAnywhere, Category = GooglePlayServices)
	bool bEnableGooglePlaySupport;

	// The app id obtained from the Google Play Developer Console
	UPROPERTY(GlobalConfig, EditAnywhere, Category = GooglePlayServices)
	FString GamesAppID;

	// Mapping of game achievement names to IDs generated by Google Play.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = GooglePlayServices)
	TArray<FGooglePlayAchievementMapping> AchievementMap;

	// Mapping of game leaderboard names to IDs generated by Google Play.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = GooglePlayServices)
	TArray<FGooglePlayLeaderboardMapping> LeaderboardMap;

	// The unique identifier for the ad obtained from AdMob.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = GooglePlayServices)
	FString AdMobAdUnitID;
};
