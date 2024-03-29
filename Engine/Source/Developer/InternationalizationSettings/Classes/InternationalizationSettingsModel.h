// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	InternationalizationSettingsModel.h: Declares the UInternationalizationSettingsModel class.
=============================================================================*/

#pragma once

#include "InternationalizationSettingsModel.generated.h"

/**
 * Implements loading and saving of internationalization settings.
 */
UCLASS()
class INTERNATIONALIZATIONSETTINGS_API UInternationalizationSettingsModel
	:	public UObject
{
	GENERATED_UCLASS_BODY()

public:
	void SaveDefaults();
	void ResetToDefault();
	FString GetCultureName() const;
	void SetCultureName(const FString& CultureName);
	bool ShouldLoadLocalizedPropertyNames() const;
	void ShouldLoadLocalizedPropertyNames(const bool Value);

public:
	/**
	 * Returns an event delegate that is executed when a setting has changed.
	 *
	 * @return The delegate.
	 */
	DECLARE_EVENT(UInternationalizationSettingsModel, FSettingChangedEvent);
	FSettingChangedEvent& OnSettingChanged( ) { return SettingChangedEvent; }

private:
	// Holds an event delegate that is executed when a setting has changed.
	FSettingChangedEvent SettingChangedEvent;
};