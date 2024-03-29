// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LevelEditorViewportSettings.h: Declares the ULevelEditorViewportSettings class.
=============================================================================*/

#pragma once

#include "Viewports.h"
#include "LevelEditorViewportSettings.generated.h"


/**
 * Implements the Level Editor's per-instance view port settings.
 */
USTRUCT()
struct UNREALED_API FLevelEditorViewportInstanceSettings
{
	GENERATED_USTRUCT_BODY()

	FLevelEditorViewportInstanceSettings()
		: ViewportType(LVT_Perspective)
		, PerspViewModeIndex(VMI_Lit)
		, OrthoViewModeIndex(VMI_BrushWireframe)
		, EditorShowFlagsString()
		, GameShowFlagsString()
		, BufferVisualizationMode()
		, ExposureSettings()
		, FOVAngle(EditorViewportDefs::DefaultPerspectiveFOVAngle)
		, bIsRealtime(false)
		, bShowFPS_DEPRECATED(false)
		, bShowStats(false)
	{ }

	/**
	 * The viewport type
	 */
	UPROPERTY(config)
	TEnumAsByte<ELevelViewportType> ViewportType;

	/* 
	 * View mode to set when this viewport is of type LVT_Perspective 
	 */
	UPROPERTY(config)
	TEnumAsByte<EViewModeIndex> PerspViewModeIndex;

	/* 
	 * View mode to set when this viewport is not of type LVT_Perspective 
	 */
	UPROPERTY(config)
	TEnumAsByte<EViewModeIndex> OrthoViewModeIndex;

	/**
	 * A set of flags that determines visibility for various scene elements (FEngineShowFlags), converted to string form
	 * These have to be saved as strings since FEngineShowFlags is too complex for UHT to parse correctly
	 */
	UPROPERTY(config)
	FString EditorShowFlagsString;

	/**
	 * A set of flags that determines visibility for various scene elements (FEngineShowFlags), converted to string form
	 * These have to be saved as strings since FEngineShowFlags is too complex for UHT to parse correctly
	 */
	UPROPERTY(config)
	FString GameShowFlagsString;

	/**
	 * The buffer visualization mode for the viewport
	 */
	UPROPERTY(config)
	FName BufferVisualizationMode;

	/**
	 * Setting to allow designers to override the automatic expose
	 */
	UPROPERTY(config)
	FExposureSettings ExposureSettings;

	/*
	 * Field of view angle for the viewport
	 */
	UPROPERTY(config)
	float FOVAngle;

	/*
	 * Is this viewport updating in real-time?
	 */
	UPROPERTY(config)
	bool bIsRealtime;

	/*
	 * Should this viewport show an FPS count?
	 */
	UPROPERTY(config)
	bool bShowFPS_DEPRECATED;

	/*
	 * Should this viewport show statistics?
	 */
	UPROPERTY(config)
	bool bShowStats;

	/*
	 * Should this viewport have any stats enabled by default?
	 */
	UPROPERTY(config)
	TArray<FString> EnabledStats;
};


/**
 * Implements a key -> value pair for the per-instance view port settings
 */
USTRUCT()
struct UNREALED_API FLevelEditorViewportInstanceSettingsKeyValuePair
{
	GENERATED_USTRUCT_BODY()

	/* 
	 * Name identifying this config
	 */
	UPROPERTY(config)
	FString ConfigName;

	/* 
	 * Settings for this config
	 */
	UPROPERTY(config)
	FLevelEditorViewportInstanceSettings ConfigSettings;
};


/**
 * Implements the Level Editor's view port settings.
 */
UCLASS(config=EditorUserSettings)
class UNREALED_API ULevelEditorViewportSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()

	/**
	 * Enable the use of flight camera controls under various circumstances.
	 */
	UPROPERTY(EditAnywhere, config, Category=Controls)
	TEnumAsByte<enum EWASDType> FlightCameraControlType;

	/**
	 * If true, moves the canvas and shows the mouse.  If false, uses original camera movement.
	 */
	UPROPERTY(EditAnywhere, config, Category=Controls, meta=(DisplayName = "Grab and Drag to Move Orthographic Cameras"), AdvancedDisplay)
	uint32 bPanMovesCanvas:1;

	/**
	 * If checked, in orthographic view ports zooming will center on the mouse position.  If unchecked, the zoom is around the center of the viewport.
	 */
	UPROPERTY(EditAnywhere, config, Category=Controls, meta=(DisplayName = "Orthographic Zoom to Cursor Position"))
	uint32 bCenterZoomAroundCursor:1;

	/** Allow translate/rotate widget */
	UPROPERTY(EditAnywhere, config, Category=Controls, meta=( DisplayName = "Enable Combined Translate/Rotate Widget" ))
	uint32 bAllowTranslateRotateZWidget:1;

	/** If true, Clicking a BSP selects the brush and ctrl+shift+click selects the surface. If false, vice versa */
	UPROPERTY(EditAnywhere, config, Category=Controls, meta=( DisplayName = "Clicking BSP Enables Brush" ))
	uint32 bClickBSPSelectsBrush:1;

	/**
	 * How fast the perspective camera moves when flying through the world.
	 */
	UPROPERTY(config, meta=(UIMin = "1", UIMax = "8", ClampMin="1", ClampMax="8"))
	int32 CameraSpeed;

	/**
	 * How fast the perspective camera moves through the world when using mouse scroll.
	 */
	UPROPERTY(EditAnywhere, config, Category=Controls, meta=(UIMin = "1", UIMax = "8", ClampMin="1", ClampMax="8"))
	int32 MouseScrollCameraSpeed;

	/**
	 * The sensitivity of mouse movement when rotating the camera.
	 */
	UPROPERTY(EditAnywhere, config, Category=Controls, meta=(ClampMin="0.0",ClampMax="1.0") )
	float MouseSensitivty;
	
	/**
	 * Whether or not to invert the direction of middle mouse panning in viewports
	 */
	UPROPERTY(EditAnywhere, config, Category=Controls)
	bool bInvertMiddleMousePan;

	/**
	 * Whether to use mouse position as direct widget position.
	 */
	UPROPERTY(EditAnywhere, config, Category=Controls, AdvancedDisplay)
	uint32 bUseAbsoluteTranslation:1;

	/**
	 * If enabled, the viewport will stream in levels automatically when the camera is moved.
	 */
	UPROPERTY(EditAnywhere, config, Category=Controls, meta=(DisplayName = "Stream in Levels Automatically when Camera is Moved"), aDvancedDisplay)
	bool bLevelStreamingVolumePrevis;

	/** When checked, orbit the camera by using the L or U keys when unchecked, Alt and Left Mouse Drag will orbit around the look at point */
	UPROPERTY(EditAnywhere, config, Category=Controls, meta=(DisplayName="Use UE3 Orbit Controls"), AdvancedDisplay)
	bool bUseUE3OrbitControls;

public:

	/** If enabled will use power of 2 grid settings (e.g, 1,2,4,8,16,...,1024) instead of decimal grid sizes */
	UPROPERTY(EditAnywhere, config, Category=GridSnapping, meta=(DisplayName = "User Power of Two Snap Size"))
	bool bUsePowerOf2SnapSize;

	/** Decimal grid sizes (for translation snapping and grid rendering) */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category=GridSnapping)
	TArray<float> DecimalGridSizes;

	/** The number of lines between each major line interval for decimal grids */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category=GridSnapping)
	TArray<float> DecimalGridIntervals;	

	/** Power of 2 grid sizes (for translation snapping and grid rendering) */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category=GridSnapping)
	TArray<float> Pow2GridSizes;

	/** The number of lines between each major line interval for pow2 grids */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category=GridSnapping)
	TArray<float> Pow2GridIntervals;

	/** User defined grid intervals for rotations */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category=GridSnapping)
	TArray<float> CommonRotGridSizes;

	/** Preset grid intervals for rotations */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category=GridSnapping)
	TArray<float> DivisionsOf360RotGridSizes;

	/** Grid sizes for scaling */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category=GridSnapping)
	TArray<float> ScalingGridSizes;

	/** If enabled, actor positions will snap to the grid. */
	UPROPERTY(EditAnywhere, config, Category=GridSnapping, meta=(DisplayName = "Enable Grid Snapping"))
	uint32 GridEnabled:1;
	
	/** If enabled, actor rotations will snap to the grid. */
	UPROPERTY(EditAnywhere, config, Category=GridSnapping, meta=(DisplayName = "Enable Rotation Snapping"))
	uint32 RotGridEnabled:1;

	/** If enabled, actor sizes will snap to the grid. */
	UPROPERTY(EditAnywhere, config, Category=GridSnapping, meta=(DisplayName = "Enable Scale Snapping"))
	uint32 SnapScaleEnabled:1;

	/** If enabled the when dragging new objects out of the content browser, it will snap the objects Z coordinate to the floor below it (if any) instead of the Z grid snapping location */
	UPROPERTY(EditAnywhere, config, Category=GridSnapping)
	bool bSnapNewObjectsToFloor;

private:

	/** If enabled, use the old-style multiplicative/percentage scaling method instead of the new additive/fraction method */
	UPROPERTY(EditAnywhere, config, Category=GridSnapping)
	uint32 bUsePercentageBasedScaling:1;

public:

	/** If true actor snap will be enabled in the editor **/
	UPROPERTY(config, Category=GridSnapping, VisibleDefaultsOnly)
	uint32 bEnableActorSnap:1;

	/** Global actor snap scale for the editor */
	UPROPERTY(config, Category=GridSnapping, VisibleDefaultsOnly)
	float ActorSnapScale;

	/** Global actor snap distance setting for the editor */
	UPROPERTY(config)
	float ActorSnapDistance;

	UPROPERTY(config)
	bool bSnapVertices;
 
	UPROPERTY(config)
	float SnapDistance;

	UPROPERTY(config)
	int32 CurrentPosGridSize;

	UPROPERTY(config)
	int32 CurrentRotGridSize;

	UPROPERTY(config)
	int32 CurrentScalingGridSize;

	UPROPERTY(config)
	bool PreserveNonUniformScale;

	/** Controls which array of rotation grid values we are using */
	UPROPERTY(config)
	TEnumAsByte<ERotationGridMode> CurrentRotGridMode;

public:

	/** How to constrain perspective view port FOV */
	UPROPERTY(EditAnywhere, config, Category=LookAndFeel)
	TEnumAsByte<enum EAspectRatioAxisConstraint> AspectRatioAxisConstraint;

	/** Enables real-time hover feedback when mousing over objects in editor view ports */
	UPROPERTY(EditAnywhere, config, Category=LookAndFeel, meta=(DisplayName = "Highlight Objects Under Mouse Cursor"))
	uint32 bEnableViewportHoverFeedback:1;

	/** If enabled, selected objects will be highlighted with brackets in all modes rather than a special highlight color. */
	UPROPERTY(EditAnywhere, config, Category=LookAndFeel, meta=(DisplayName = "Highlight Selected Objects with Brackets"))
	uint32 bHighlightWithBrackets:1;

	/**
	 * If checked all orthographic view ports are linked to the same position and move together.
	 */
	UPROPERTY(EditAnywhere, config, Category=LookAndFeel, meta=(DisplayName = "Link Orthographic Viewport Movement"))
	uint32 bUseLinkedOrthographicViewports:1;

	/** True if viewport box selection requires objects to be fully encompassed by the selection box to be selected */
	UPROPERTY(config)
	uint32 bStrictBoxSelection:1;

	/** Whether to show selection outlines for selected Actors */
	UPROPERTY(EditAnywhere, config, Category=LookAndFeel, meta=(DisplayName = "Use Selection Outline"))
	uint32 bUseSelectionOutline:1;

	/** Sets the intensity of the overlay displayed when an object is selected */
	UPROPERTY(EditAnywhere, config, Category=LookAndFeel, meta=(DisplayName = "Selection Highlight Intensity" ,ClampMin = "0", UIMin = "0", UIMax = "1"))
	float SelectionHighlightIntensity;

	/** Sets the intensity of the overlay displayed when an object is selected */
	UPROPERTY(EditAnywhere, config, Category=LookAndFeel, meta=(DisplayName = "BSP Surface Highlight Intensity" ,ClampMin = "0", UIMin = "0", UIMax = "1"))
	float BSPSelectionHighlightIntensity;

	/** Sets the intensity of the overlay displayed when an object is hovered */
	UPROPERTY(EditAnywhere, config, Category=LookAndFeel, meta=(DisplayName = "Hover Highlight Intensity" ,ClampMin = "0", UIMin = "0", UIMax = "20"))
	float HoverHighlightIntensity;

	/** Enables the editor perspective camera to be dropped at the last PlayInViewport cam position */
	UPROPERTY(EditAnywhere, config, Category=LookAndFeel, meta=(DisplayName = "Use Camera Location from Play-In-Viewport"))
	uint32 bEnableViewportCameraToUpdateFromPIV:1;

	/** When enabled, selecting a camera actor will display a live 'picture in picture' preview from the camera's perspective within the current editor view port.  This can be used to easily tweak camera positioning, post-processing and other settings without having to possess the camera itself.  This feature may reduce application performance when enabled. */
	UPROPERTY(EditAnywhere, config, Category=LookAndFeel)
	uint32 bPreviewSelectedCameras:1;

	/** Affects the size of 'picture in picture' previews if they are enabled */
	UPROPERTY(EditAnywhere, config, Category=LookAndFeel, meta=(ClampMin = "1", UIMin = "1", UIMax = "10"))
	float CameraPreviewSize;

	/** Distance from the camera to place actors which are dropped on nothing in the view port. */
	UPROPERTY(EditAnywhere, config, Category=LookAndFeel, meta=(DisplayName = "Background Drop Distance"))
	float BackgroundDropDistance;

	/** A list of meshes that can be used as preview mesh in the editor view port by holding down the backslash key */
	UPROPERTY(EditAnywhere, config, Category=Preview, meta=(AllowedClasses = "StaticMesh"))
	TArray<FStringAssetReference> PreviewMeshes;

	UPROPERTY(EditAnywhere, config, Category=LookAndFeel, meta=(ClampMin = "0.01", UIMin = "0.01", UIMax = "5"))
	float BillboardScale;

	/**
	 * The size adjustment to apply to the translate/rotate/scale widgets (in Unreal units).
	 */
	UPROPERTY(EditAnywhere, config, Category=LookAndFeel, meta=(ClampMin="-10",ClampMax="150") )
	int32 TransformWidgetSizeAdjustment;

	/** When enabled, engine stats that are enabled in level viewports are preserved between editor sessions */
	UPROPERTY(EditAnywhere, config, Category = LookAndFeel)
	uint32 bSaveEngineStats : 1;

private:

	// Per-instance viewport settings.
	UPROPERTY(config)
	TArray<FLevelEditorViewportInstanceSettingsKeyValuePair> PerInstanceSettings;

public:

	/**
	 * @return The instance settings for the given viewport; null if no settings were found for this viewport
	 */
	const FLevelEditorViewportInstanceSettings* GetViewportInstanceSettings( const FString& InConfigName ) const
	{
		for(auto It = PerInstanceSettings.CreateConstIterator(); It; ++It)
		{
			const FLevelEditorViewportInstanceSettingsKeyValuePair& ConfigData = *It;
			if(ConfigData.ConfigName == InConfigName)
			{
				return &ConfigData.ConfigSettings;
			}
		}

		return nullptr;
	}

	/**
	 * Set the instance settings for the given viewport
	 */
	void SetViewportInstanceSettings( const FString& InConfigName, const FLevelEditorViewportInstanceSettings& InConfigSettings )
	{
		check(!InConfigName.IsEmpty());

		bool bWasFound = false;
		for(auto It = PerInstanceSettings.CreateIterator(); It; ++It)
		{
			FLevelEditorViewportInstanceSettingsKeyValuePair& ConfigData = *It;
			if(ConfigData.ConfigName == InConfigName)
			{
				ConfigData.ConfigSettings = InConfigSettings;
				bWasFound = true;
				break;
			}
		}

		if(!bWasFound)
		{
			FLevelEditorViewportInstanceSettingsKeyValuePair ConfigData;
			ConfigData.ConfigName = InConfigName;
			ConfigData.ConfigSettings = InConfigSettings;
			PerInstanceSettings.Add(ConfigData);
		}

		PostEditChange();
	}

	/**
	 * Checks whether percentage based scaling should be used for view ports.
	 *
	 * @return true if percentage based scaling is enabled, false otherwise.
	 */
	bool UsePercentageBasedScaling( ) const
	{
		return bUsePercentageBasedScaling;
	}

public:

	/**
	 * Returns an event delegate that is executed when a setting has changed.
	 *
	 * @return The delegate.
	 */
	DECLARE_EVENT_OneParam(ULevelEditorViewportSettings, FSettingChangedEvent, FName /*PropertyName*/);
	FSettingChangedEvent& OnSettingChanged( ) { return SettingChangedEvent; }

protected:

	// Begin UObject overrides

	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent ) OVERRIDE;

	// End UObject overrides

private:

	// Holds an event delegate that is executed when a setting has changed.
	FSettingChangedEvent SettingChangedEvent;
};
