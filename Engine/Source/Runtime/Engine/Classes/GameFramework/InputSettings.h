// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "PlayerInput.h"
#include "InputSettings.generated.h"


UCLASS(config=Input, defaultconfig)
class ENGINE_API UInputSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()

	/** List of Axis to be defined in ini file. These are put into AxisProperties for game use. */
	UPROPERTY(config, EditAnywhere, EditFixedSize, Category="Bindings", meta=(ToolTip="List of Axis Properties"), AdvancedDisplay)
	TArray<struct FInputAxisConfigEntry> AxisConfig;

	// Allow mouse to be used for touch
	UPROPERTY(config, EditAnywhere, Category="MouseProperties")
	uint32 bUseMouseForTouch:1;

	// Mouse smoothing control
	UPROPERTY(config, EditAnywhere, Category="MouseProperties", AdvancedDisplay)
	uint32 bEnableMouseSmoothing:1;

	// Scale the mouse based on the player camera manager's field of view
	UPROPERTY(config, EditAnywhere, Category="MouseProperties", AdvancedDisplay)
	uint32 bEnableFOVScaling:1;

	// The scaling value to multiply the field of view by
	UPROPERTY(config, EditAnywhere, Category="MouseProperties", AdvancedDisplay, meta=(editcondition="bEnableFOVScaling"))
	float FOVScale;

	/** If a key is pressed twice in this amount of time, it's considered a "double click" */
	UPROPERTY(config, EditAnywhere, Category="MouseProperties", AdvancedDisplay)
	float DoubleClickTime;

	/** List of Action Mappings */
	UPROPERTY(config, EditAnywhere, Category="Bindings")
	TArray<struct FInputActionKeyMapping> ActionMappings;

	/** List of Axis Mappings */
	UPROPERTY(config, EditAnywhere, Category="Bindings")
	TArray<struct FInputAxisKeyMapping> AxisMappings;

	/** Should the touch input interface be shown always, or only when the platform has a touch screen? */
	UPROPERTY(config, EditAnywhere, Category="Mobile")
	bool bAlwaysShowTouchInterface;

	/** The default on-screen touch input interface for the game (can be null to disable the onscreen interface) */
	UPROPERTY(config, EditAnywhere, Category="Mobile", meta=(AllowedClasses="TouchInterface"))
	FStringAssetReference DefaultTouchInterface;

	/** The key which opens the console. */
	UPROPERTY(config, EditAnywhere, Category="Console")
	FKey ConsoleKey;

	// UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) OVERRIDE;
#endif
	// End of UObject interface

	void AddActionMapping(const FInputActionKeyMapping& KeyMapping);
	void RemoveActionMapping(const FInputActionKeyMapping& KeyMapping);

	void AddAxisMapping(const FInputAxisKeyMapping& KeyMapping);
	void RemoveAxisMapping(const FInputAxisKeyMapping& KeyMapping);

	void SaveKeyMappings();

	void GetActionNames(TArray<FName>& ActionNames) const;
	void GetAxisNames(TArray<FName>& AxisNames) const;
};
