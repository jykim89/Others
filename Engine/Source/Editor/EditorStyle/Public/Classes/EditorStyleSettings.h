// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EditorStyleSettings.h: Declares the UEditorStyleSettings class.
=============================================================================*/

#pragma once


#include "EditorStyleSettings.generated.h"


/**
 * Enumerates color vision deficiency types.
 */
UENUM()
enum EColorVisionDeficiency
{
	CVD_NormalVision UMETA(DisplayName="Normal Vision"),
	CVD_Deuteranomly UMETA(DisplayName="Deuteranomly (6% of males, 0.4% of females)"),
	CVD_Deuteranopia UMETA(DisplayName="Deuteranopia (1% of males)"),
	CVD_Protanomly UMETA(DisplayName="Protanomly (1% of males, 0.01% of females)"),
	CVD_Protanopia UMETA(DisplayName="Protanopia (1% of males)"),
	CVD_Tritanomaly UMETA(DisplayName="Tritanomaly (0.01% of males and females)"),
	CVD_Tritanopia UMETA(DisplayName="Tritanopia (1% of males and females)"),
	CVD_Achromatopsia UMETA(DisplayName="Achromatopsia (Extremely Rare)"),
};


/**
 * Implements the Editor style settings.
 */
UCLASS(config=EditorUserSettings)
class EDITORSTYLE_API UEditorStyleSettings
	: public UObject
{
public:

	GENERATED_UCLASS_BODY()

public:

	/** The color used to represent selection */
	UPROPERTY(EditAnywhere, config, Category=Colors, meta=(DisplayName="Selection Color"))
	FLinearColor SelectionColor;

	/** The color used to represent a pressed item */
	UPROPERTY(EditAnywhere, config, Category=Colors, meta=(DisplayName="Pressed Selection Color"))
	FLinearColor PressedSelectionColor;

	/** The color used to represent selected items that are currently inactive */
	UPROPERTY(EditAnywhere, config, Category=Colors, meta=(DisplayName="Inactive Selection Color"))
	FLinearColor InactiveSelectionColor;

	/** The color used to represent keyboard input selection focus */
	UPROPERTY(EditAnywhere, config, Category=Colors, meta=(DisplayName="Keyboard Focus Color"), AdvancedDisplay)
	FLinearColor KeyboardFocusColor;

	/** Applies a color vision deficiency filter to the entire editor */
	UPROPERTY(EditAnywhere, config, Category=Colors)
	TEnumAsByte<EColorVisionDeficiency> ColorVisionDeficiencyPreviewType;

public:

	/** Whether to use small toolbar icons without labels or not. */
	UPROPERTY(EditAnywhere, config, Category=UserInterface)
	uint32 bUseSmallToolBarIcons:1;

	/** Enables animated transitions for certain menus and pop-up windows.  Note that animations may be automatically disabled at low frame rates in order to improve responsiveness. */
	UPROPERTY(EditAnywhere, config, Category=UserInterface)
	uint32 bEnableWindowAnimations:1;

	/** When enabled, the C++ names for properties and functions will be displayed in a format that is easier to read */
	UPROPERTY(EditAnywhere, config, Category=UserInterface, meta=(DisplayName="Show Friendly Variable Names"))
	uint32 bShowFriendlyNames:1;

	/** When enabled, the Editor Preferences and Project Settings menu items in the main menu will be expanded with sub-menus for each settings section. */
	UPROPERTY(EditAnywhere, config, Category=UserInterface, AdvancedDisplay)
	uint32 bExpandConfigurationMenus:1;

public:

	/**
	 * Returns an event delegate that is executed when a setting has changed.
	 *
	 * @return The delegate.
	 */
	DECLARE_EVENT_OneParam(UEditorStyleSettings, FSettingChangedEvent, FName /*PropertyName*/);
	FSettingChangedEvent& OnSettingChanged( ) { return SettingChangedEvent; }

protected:

	// Begin UObject overrides

#if WITH_EDITOR
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent ) OVERRIDE;
#endif

	// End UObject overrides

private:

	// Holds an event delegate that is executed when a setting has changed.
	FSettingChangedEvent SettingChangedEvent;
};