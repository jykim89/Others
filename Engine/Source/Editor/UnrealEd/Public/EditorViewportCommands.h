// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once 

/**
 * Class containing commands for editor viewport actions common to all viewports
 */
class UNREALED_API FEditorViewportCommands : public TCommands<FEditorViewportCommands>
{
public:
	FEditorViewportCommands() 
		: TCommands<FEditorViewportCommands>
		(
			TEXT("EditorViewport"), // Context name for fast lookup
			NSLOCTEXT("Contexts", "EditorViewportCommands", "Common Viewport Commands"), // Localized context name for displaying
			TEXT("LevelEditor"),
			FEditorStyle::GetStyleSetName() // Icon Style Set
		)
	{
	}
	
	/** Changes the viewport to perspective view */
	TSharedPtr< FUICommandInfo > Perspective;

	/** Changes the viewport to top view */
	TSharedPtr< FUICommandInfo > Top;

	/** Changes the viewport to side view */
	TSharedPtr< FUICommandInfo > Side;

	/** Changes the viewport to front view */
	TSharedPtr< FUICommandInfo > Front;

	/** Changes the viewport to wireframe */
	TSharedPtr< FUICommandInfo > WireframeMode;

	/** Changes the viewport to unlit mode */
	TSharedPtr< FUICommandInfo > UnlitMode;

	/** Changes the viewport to lit mode */
	TSharedPtr< FUICommandInfo > LitMode;

	/** Changes the viewport to detail lighting mode */
	TSharedPtr< FUICommandInfo > DetailLightingMode;

	/** Changes the viewport to reflection override mode */
	TSharedPtr< FUICommandInfo > ReflectionOverrideMode;

	/** Changes the viewport to lighting only */
	TSharedPtr< FUICommandInfo > LightingOnlyMode;

	/** Changes the viewport to light complextiy mode */
	TSharedPtr< FUICommandInfo > LightComplexityMode;

	/** Changes the viewport to shader complexity mode */
	TSharedPtr< FUICommandInfo > ShaderComplexityMode;

	/** Changes the viewport to stationary light overlap mode */
	TSharedPtr< FUICommandInfo > StationaryLightOverlapMode;

	/** Changes the viewport to lightmap density mode */
	TSharedPtr< FUICommandInfo > LightmapDensityMode;

	/** Changes the viewport to visualize the buffer content */
	TSharedPtr< FUICommandInfo > VisualizeBufferMode;

	/** Collision Draw Mode */
	TSharedPtr< FUICommandInfo > CollisionPawn;
	TSharedPtr< FUICommandInfo > CollisionVisibility;

	/** Toggles realtime rendering in the viewport */
	TSharedPtr< FUICommandInfo > ToggleRealTime;

	/** Toggles showing stats in the viewport */
	TSharedPtr< FUICommandInfo > ToggleStats;

	/** Toggles showing fps in the viewport */
	TSharedPtr< FUICommandInfo > ToggleFPS;

	/** Allows the grid size setting to by changed by one */
	TSharedPtr< FUICommandInfo > IncrementPositionGridSize;

	/** Allows the grid size setting to by changed by one */
	TSharedPtr< FUICommandInfo > DecrementPositionGridSize;

	/** Allows the rotation grid size setting to by changed by one */
	TSharedPtr< FUICommandInfo > IncrementRotationGridSize;

	/** Allows the rotation grid size setting to by changed by one */
	TSharedPtr< FUICommandInfo > DecrementRotationGridSize;

	/** Command to capture screen */
	TSharedPtr< FUICommandInfo > ScreenCapture;

	/** Captures the viewport and updates the project thumbnail png file */
	TSharedPtr< FUICommandInfo > ScreenCaptureForProjectThumbnail;

	/** Translate Mode */
	TSharedPtr< FUICommandInfo > TranslateMode;

	/** Rotate Mode */
	TSharedPtr< FUICommandInfo > RotateMode;

	/** Scale Mode */
	TSharedPtr< FUICommandInfo > ScaleMode;

	/** TranslateRotate Mode */
	TSharedPtr< FUICommandInfo > TranslateRotateMode;

	/** Shrink the level editor transform widget */
	TSharedPtr< FUICommandInfo > ShrinkTransformWidget;

	/** Expand the level editor transform widget */
	TSharedPtr< FUICommandInfo > ExpandTransformWidget;

	/** World relative coordinate system */
	TSharedPtr< FUICommandInfo > RelativeCoordinateSystem_World;

	/** Local relative coordinate system */
	TSharedPtr< FUICommandInfo > RelativeCoordinateSystem_Local;

	TSharedPtr< FUICommandInfo > CycleTransformGizmos;
	TSharedPtr< FUICommandInfo > CycleTransformGizmoCoordSystem;

	TSharedPtr< FUICommandInfo > FocusViewportToSelection;

	/** Toggle automatic exposure */
	TSharedPtr< FUICommandInfo > ToggleAutoExposure;

	/** Magic ID to differentiate the auto expose setting from the fixed exposure settings */
	static const int32 AutoExposureRadioID = 999;

	/** Exposure Commands */
	TSharedPtr< FUICommandInfo > FixedExposure4m;
	TSharedPtr< FUICommandInfo > FixedExposure3m;
	TSharedPtr< FUICommandInfo > FixedExposure2m;
	TSharedPtr< FUICommandInfo > FixedExposure1m;
	TSharedPtr< FUICommandInfo > FixedExposure0;
	TSharedPtr< FUICommandInfo > FixedExposure1p;
	TSharedPtr< FUICommandInfo > FixedExposure2p;
	TSharedPtr< FUICommandInfo > FixedExposure3p;
	TSharedPtr< FUICommandInfo > FixedExposure4p;

	/**
	 * Grid commands
	 */

	/** Enables or disables snapping to the grid when dragging objects around */
	TSharedPtr< FUICommandInfo > LocationGridSnap;

	/** Enables or disables snapping to a rotational grid while rotating objects */
	TSharedPtr< FUICommandInfo > RotationGridSnap;


	/** Enables or disables snapping to a scaling grid while scaling objects */
	TSharedPtr< FUICommandInfo > ScaleGridSnap;

public:
	/** Registers our commands with the binding system */
	virtual void RegisterCommands() OVERRIDE;
};


