// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LandscapeEditorDetailCustomization_Base.h"


/**
 * Slate widgets customizer for the smaller tools that need little customization
 */

class FLandscapeEditorDetailCustomization_MiscTools : public FLandscapeEditorDetailCustomization_Base
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) OVERRIDE;

protected:
	// Component Selection Tool
	static EVisibility GetClearComponentSelectionVisibility();
	static FReply OnClearComponentSelectionButtonClicked();

	// Mask Tool
	static EVisibility GetClearRegionSelectionVisibility();
	static FReply OnClearRegionSelectionButtonClicked();

	// Splines Tool
	static FReply OnApplyAllSplinesButtonClicked();
	static FReply OnApplySelectedSplinesButtonClicked();
	void OnbUseAutoRotateControlPointChanged(ESlateCheckBoxState::Type NewState);
	ESlateCheckBoxState::Type GetbUseAutoRotateControlPoint() const;

	// Ramp Tool
	static FReply OnApplyRampButtonClicked();
	static bool GetApplyRampButtonIsEnabled();
	static FReply OnResetRampButtonClicked();
};
