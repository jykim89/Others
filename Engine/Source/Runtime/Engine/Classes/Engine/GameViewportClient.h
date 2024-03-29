// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/**
 * A game viewport (FViewport) is a high-level abstract interface for the
 * platform specific rendering, audio, and input subsystems.
 * GameViewportClient is the engine's interface to a game viewport.
 * Exactly one GameViewportClient is created for each instance of the game.  The
 * only case (so far) where you might have a single instance of Engine, but
 * multiple instances of the game (and thus multiple GameViewportClients) is when
 * you have more than one PIE window running.
 *
 * Responsibilities:
 * propagating input events to the global interactions list
 *
 */
#pragma once
#include "GameViewportClient.generated.h"

/**
 * Enum of the different splitscreen types
 */
namespace ESplitScreenType
{
	enum Type
	{
		// No split
		None,
		// 2 player horizontal split
		TwoPlayer_Horizontal,
		// 2 player vertical split
		TwoPlayer_Vertical,
		// 3 Player split with 1 player on top and 2 on bottom
		ThreePlayer_FavorTop,
		// 3 Player split with 1 player on bottom and 2 on top
		ThreePlayer_FavorBottom,
		// 4 Player split
		FourPlayer,

		SplitTypeCount
	};
}

/**
 * Stereoscopic rendering passes.  FULL implies stereoscopic rendering isn't enabled for this pass
 */
enum EStereoscopicPass
{
	eSSP_FULL,
	eSSP_LEFT_EYE,
	eSSP_RIGHT_EYE
};

/**
 * The 4 different kinds of safezones
 */
enum ESafeZoneType
{
	eSZ_TOP,
	eSZ_BOTTOM,
	eSZ_LEFT,
	eSZ_RIGHT,
	eSZ_MAX,
};

/** Max/Recommended screen viewable extents as a percentage */
struct FTitleSafeZoneArea
{
	float MaxPercentX;
	float MaxPercentY;
	float RecommendedPercentX;
	float RecommendedPercentY;

	FTitleSafeZoneArea()
		: MaxPercentX(0)
		, MaxPercentY(0)
		, RecommendedPercentX(0)
		, RecommendedPercentY(0)
	{
	}

};

/** Structure to store splitscreen data. */
struct FPerPlayerSplitscreenData
{
	float SizeX;
	float SizeY;
	float OriginX;
	float OriginY;


	FPerPlayerSplitscreenData()
		: SizeX(0)
		, SizeY(0)
		, OriginX(0)
		, OriginY(0)
	{
	}

	FPerPlayerSplitscreenData(float NewSizeX, float NewSizeY, float NewOriginX, float NewOriginY)
		: SizeX(NewSizeX)
		, SizeY(NewSizeY)
		, OriginX(NewOriginX)
		, OriginY(NewOriginY)
	{
	}

};

/** Structure containing all the player splitscreen datas per splitscreen configuration. */
struct FSplitscreenData
{
	TArray<struct FPerPlayerSplitscreenData> PlayerData;
};

/** debug property display functionality
 * to interact with this, use "display", "displayall", "displayclear"
 */
USTRUCT()
struct FDebugDisplayProperty
{
	GENERATED_USTRUCT_BODY()

	/** the object whose property to display. If this is a class, all objects of that class are drawn. */
	UPROPERTY()
	class UObject* Obj;

	/** if Obj is a class and WithinClass is not NULL, further limit the display to objects that have an Outer of WithinClass */
	UPROPERTY()
	TSubclassOf<class UObject>  WithinClass;

	/** name of the property to display */
	FName PropertyName;

	/** whether PropertyName is a "special" value not directly mapping to a real property (e.g. state name) */
	uint32 bSpecialProperty:1;


	FDebugDisplayProperty()
		: Obj(NULL)
		, WithinClass(NULL)
		, bSpecialProperty(false)
	{
	}

};

/**
 * Delegate type for when a screenshot has been captured
 *
 * The first parameter is the width.
 * The second parameter is the height.
 * The third parameter is the array of bitmap data.
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnScreenshotCaptured, int32 /*Width*/, int32 /*Height*/, const TArray<FColor>& /*Colors*/);

/**
 * Delegate type for when a png screenshot has been captured
 *
 * The first parameter is the width.
 * The second parameter is the height.
 * The third parameter is the array of bitmap data.
 * The fourth parameter is the screen shot filename.
 */
DECLARE_DELEGATE_FourParams(FOnPNGScreenshotCaptured, int32, int32, const TArray<FColor>&, const FString&);

UCLASS(Within=Engine, transient, config=Engine)
class ENGINE_API UGameViewportClient : public UScriptViewportClient, public FExec
{
	GENERATED_UCLASS_BODY()

public:
	virtual ~UGameViewportClient();

	/** The viewport's console.   Might be null on consoles */
	UPROPERTY()
	class UConsole* ViewportConsole;

	/** @todo document */
	UPROPERTY()
	TArray<struct FDebugDisplayProperty> DebugProperties;

	/** border of safe area */
	struct FTitleSafeZoneArea TitleSafeZone;

	/** Array of the screen data needed for all the different splitscreen configurations */
	TArray<struct FSplitscreenData> SplitscreenInfo;

	int32 MaxSplitscreenPlayers;

	/** if true then the title safe border is drawn */
	uint32 bShowTitleSafeZone:1;

	/** If true, this viewport is a play in editor viewport */
	uint32 bIsPlayInEditorViewport:1;

	/** set to disable world rendering */
	uint32 bDisableWorldRendering:1;

protected:
	/**
	 * The splitscreen type that is actually being used; takes into account the number of players and other factors (such as cinematic mode)
	 * that could affect the splitscreen mode that is actually used.
	 */
	TEnumAsByte<ESplitScreenType::Type> ActiveSplitscreenType;

	UPROPERTY()
	UWorld* World;

	/** If true will suppress the blue transition text messages. */
	bool bSuppressTransitionMessage;

public:
	/** @todo document */
	float ProgressFadeTime;

	/** see enum EViewModeIndex */
	int32 ViewModeIndex;

	/**
	 * Debug console command to create a player.
	 * @param ControllerId - The controller ID the player should accept input from.
	 */
	UFUNCTION(exec)
	virtual void DebugCreatePlayer(int32 ControllerId);

	/** Rotates controller ids among gameplayers, useful for testing splitscreen with only one controller. */
	UFUNCTION(exec)
	virtual void SSSwapControllers();

	/**
	 * Debug console command to remove the player with a given controller ID.
	 * @param ControllerId - The controller ID to search for.
	 */
	UFUNCTION(exec)
	virtual void DebugRemovePlayer(int32 ControllerId);

	/** Exec for toggling the display of the title safe area */
	UFUNCTION(exec)
	virtual void ShowTitleSafeArea();

	/** Sets the player which console commands will be executed in the context of. */
	UFUNCTION(exec)
	virtual void SetConsoleTarget(int32 PlayerIndex);

	/** Returns a relative world context for this viewport.	 */
	virtual UWorld* GetWorld() const OVERRIDE;

	virtual void SetReferenceToWorldContext(struct FWorldContext& WorldContext);

	class FSceneViewport* GetGameViewport();

public:
	// Begin UObject Interface
	virtual void PostInitProperties() OVERRIDE;
	virtual void BeginDestroy() OVERRIDE;
	// End UObject Interface

	// FViewportClient interface.
	virtual void RedrawRequested(FViewport* InViewport) OVERRIDE {}
	virtual bool InputKey(FViewport* Viewport, int32 ControllerId, FKey Key, EInputEvent EventType, float AmountDepressed=1.f, bool bGamepad=false) OVERRIDE;
	virtual bool InputAxis(FViewport* Viewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime, int32 NumSamples=1, bool bGamepad=false) OVERRIDE;
	virtual bool InputChar(FViewport* Viewport,int32 ControllerId, TCHAR Character) OVERRIDE;
	virtual bool InputTouch(FViewport* Viewport, int32 ControllerId, uint32 Handle, ETouchType::Type Type, const FVector2D& TouchLocation, FDateTime DeviceTimestamp, uint32 TouchpadIndex) OVERRIDE;
	virtual bool InputMotion(FViewport* Viewport, int32 ControllerId, const FVector& Tilt, const FVector& RotationRate, const FVector& Gravity, const FVector& Acceleration) OVERRIDE;
	virtual EMouseCursor::Type GetCursor(FViewport* Viewport, int32 X, int32 Y ) OVERRIDE;
	virtual void Precache() OVERRIDE;
	virtual void Draw(FViewport* Viewport,FCanvas* SceneCanvas) OVERRIDE;
	virtual void ProcessScreenShots(FViewport* Viewport) OVERRIDE;
	virtual void LostFocus(FViewport* Viewport) OVERRIDE;
	virtual void ReceivedFocus(FViewport* Viewport) OVERRIDE;
	virtual bool IsFocused(FViewport* Viewport) OVERRIDE;
	virtual void CloseRequested(FViewport* Viewport) OVERRIDE;
	virtual bool RequiresHitProxyStorage() OVERRIDE { return 0; }
	virtual bool IsOrtho() const OVERRIDE;
	virtual void MouseEnter(FViewport* Viewport, int32 x, int32 y) OVERRIDE;
	virtual void MouseLeave(FViewport* Viewport) OVERRIDE;
	virtual void SetIsSimulateInEditorViewport(bool bInIsSimulateInEditorViewport) OVERRIDE;
	// End of FViewportClient interface.

	// Begin FExec interface.
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd,FOutputDevice& Ar) OVERRIDE;
	// End of FExec interface.

	/**
	 * Exec command handlers
	 */
	bool HandleForceFullscreenCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleShowCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );
	bool HandleViewModeCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );
	bool HandleNextViewModeCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );
	bool HandlePrevViewModeCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );
#if WITH_EDITOR
	bool HandleShowMouseCursorCommand( const TCHAR* Cmd, FOutputDevice& Ar );
#endif // WITH_EDITOR
	bool HandlePreCacheCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleToggleFullscreenCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleSetResCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleHighresScreenshotCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleHighresScreenshotUICommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleScreenshotCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleBugScreenshotwithHUDInfoCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleBugScreenshotCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleKillParticlesCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleForceSkelLODCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );
	bool HandleDisplayCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleDisplayAllCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleDisplayAllLocationCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleDisplayAllRotationCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleDisplayClearCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleTextureDefragCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleToggleMIPFadeCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandlePauseRenderClockCommand( const TCHAR* Cmd, FOutputDevice& Ar );

	/**
	 * Adds a widget to the Slate viewport's overlay (i.e for in game UI or tools) at the specified Z-order
	 * 
	 * @param  ViewportContent	The widget to add.  Must be valid.
	 * @param  ZOrder  The Z-order index for this widget.  Larger values will cause the widget to appear on top of widgets with lower values.
	 */
	void AddViewportWidgetContent( TSharedRef<class SWidget> ViewportContent, const int32 ZOrder = 0 );

	/**
	 * Removes a previously-added widget from the Slate viewport
	 *
	 * @param	ViewportContent  The widget to remove.  Must be valid.
	 */
	void RemoveViewportWidgetContent( TSharedRef<class SWidget> ViewportContent );


	/**
	 * This function removes all widgets from the viewport overlay
	 */
	void RemoveAllViewportWidgets();

	/**
	 * Cleans up all rooted or referenced objects created or managed by the GameViewportClient.  This method is called
	 * when this GameViewportClient has been disassociated with the game engine (i.e. is no longer the engine's GameViewport).
	 */
	virtual void DetachViewportClient();

	/**
	 * Called every frame to allow the game viewport to update time based state.
	 * @param	DeltaTime	The time since the last call to Tick.
	 */
	void Tick( float DeltaTime );

	/**
	 * Determines whether this viewport client should receive calls to InputAxis() if the game's window is not currently capturing the mouse.
	 * Used by the UI system to easily receive calls to InputAxis while the viewport's mouse capture is disabled.
	 */
	virtual bool RequiresUncapturedAxisInput() const;


	/**
	 * Set this GameViewportClient's viewport and viewport frame to the viewport specified
	 */
	virtual void SetViewportFrame( FViewportFrame* InViewportFrame );

	/**
	 * Set this GameViewportClient's viewport to the viewport specified
	 */
	virtual void SetViewport( FViewport* InViewportFrame );

	/** Assigns the viewport overlay widget to use for this viewport client.  Should only be called when first created */
	void SetViewportOverlayWidget( TSharedPtr< class SWindow > InWindow, TSharedRef<class SOverlay> InViewportOverlayWidget )
	{
		Window = InWindow;
		ViewportOverlayWidget = InViewportOverlayWidget;
	}

	/** Returns access to this viewport's Slate window */
	TSharedPtr< class SWindow > GetWindow()
	{
	 	 return Window.Pin();
	}
	 
	/** sets bDropDetail and other per-frame detail level flags on the current WorldSettings
	 * @param DeltaSeconds - amount of time passed since last tick
	 */
	virtual void SetDropDetail(float DeltaSeconds);

	/**
	 * Process Console Command
	 *
	 * @param	Command		command to process
	 */
	virtual FString ConsoleCommand( const FString& Command );

	/**
	 * Retrieve the size of the main viewport.
	 *
	 * @param	out_ViewportSize	[out] will be filled in with the size of the main viewport
	 */
	void GetViewportSize( FVector2D& ViewportSize );

	/** @return Whether or not the main viewport is fullscreen or windowed. */
	bool IsFullScreenViewport();

	/** @return mouse position in game viewport coordinates (does not account for splitscreen) */
	FVector2D GetMousePosition();

	/**
	 * Determine whether a fullscreen viewport should be used in cases where there are multiple players.
	 *
	 * @return	true to use a fullscreen viewport; false to allow each player to have their own area of the viewport.
	 */
	bool ShouldForceFullscreenViewport() const;

	/**
	 * Adds a new player.
	 * @param ControllerId - The controller ID the player should accept input from.
	 * @param OutError - If no player is returned, OutError will contain a string describing the reason.
	 * @param SpawnActor - True if an actor should be spawned for the new player.
	 * @return The player which was created.
	 */
	virtual class ULocalPlayer* CreatePlayer(int32 ControllerId, FString& OutError, bool bSpawnActor);

	/**
	 * Removes a player.
	 * @param Player - The player to remove.
	 * @return whether the player was successfully removed. Removal is not allowed while connected to a server.
	 */
	virtual bool RemovePlayer(class ULocalPlayer* ExPlayer);

	/**
	 * Initialize the game viewport.
	 * @param OutError - If an error occurs, returns the error description.
	 * @return False if an error occurred, true if the viewport was initialized successfully.
	 */
	virtual ULocalPlayer* Init(FString& OutError);

	/**
	 * Create the game's initial player at startup.  
	 * Creates a player with a ControllerId of 0.
	 *
	 * @param	OutError	receives the error string if an error occurs while creating the player.
	 *
	 * @return	player that was created (NULL if failed).
	 */
	virtual ULocalPlayer* CreateInitialPlayer( FString& OutError );

	/** @return Returns the splitscreen type that is currently being used */
	FORCEINLINE ESplitScreenType::Type GetCurrentSplitscreenConfiguration() const
	{
		return ActiveSplitscreenType;
	}

	/**
	 * Sets the value of ActiveSplitscreenConfiguration based on the desired split-screen layout type, current number of players, and any other
	 * factors that might affect the way the screen should be layed out.
	 */
	virtual void UpdateActiveSplitscreenType();

	/** Called before rendering to allow the game viewport to allocate subregions to players. */
	virtual void LayoutPlayers();

	/** called before rending subtitles to allow the game viewport to determine the size of the subtitle area
	 * @param Min top left bounds of subtitle region (0 to 1)
	 * @param Max bottom right bounds of subtitle region (0 to 1)
	 */
	virtual void GetSubtitleRegion(FVector2D& MinPos, FVector2D& MaxPos);

	/**
	* Convert a LocalPlayer to it's index in the GamePlayer array
	* Returns -1 if the index could not be found.
	*/
	int32 ConvertLocalPlayerToGamePlayerIndex( class ULocalPlayer* LPlayer );

	/** Whether the player at LocalPlayerIndex's viewport has a "top of viewport" safezone or not. */
	bool HasTopSafeZone( int32 LocalPlayerIndex );

	/** Whether the player at LocalPlayerIndex's viewport has a "bottom of viewport" safezone or not.*/
	bool HasBottomSafeZone( int32 LocalPlayerIndex );

	/** Whether the player at LocalPlayerIndex's viewport has a "left of viewport" safezone or not. */
	bool HasLeftSafeZone( int32 LocalPlayerIndex );

	/** Whether the player at LocalPlayerIndex's viewport has a "right of viewport" safezone or not. */
	bool HasRightSafeZone( int32 LocalPlayerIndex );

	/**
	* Get the total pixel size of the screen.
	* This is different from the pixel size of the viewport since we could be in splitscreen
	*/
	void GetPixelSizeOfScreen( float& Width, float& Height, class UCanvas* Canvas, int32 LocalPlayerIndex );

	/** Calculate the amount of safezone needed for a single side for both vertical and horizontal dimensions*/
	void CalculateSafeZoneValues( float& Horizontal, float& Vertical, class UCanvas* Canvas, int32 LocalPlayerIndex, bool bUseMaxPercent );

	/**
	* pixel size of the deadzone for all sides (right/left/top/bottom) based on which local player it is
	* @return true if the safe zone exists
	*/
	bool CalculateDeadZoneForAllSides( class ULocalPlayer* LPlayer, class UCanvas* Canvas, float& fTopSafeZone, float& fBottomSafeZone, float& fLeftSafeZone, float& fRightSafeZone, bool bUseMaxPercent = false );

	/**  Draw the safe area using the current TitleSafeZone settings. */
	virtual void DrawTitleSafeArea( class UCanvas* Canvas );

	/**
	 * Called after rendering the player views and HUDs to render menus, the console, etc.
	 * This is the last rendering call in the render loop
	 * @param Canvas - The canvas to use for rendering.
	 */
	virtual void PostRender(class UCanvas* Canvas);

	/**
	 * Displays the transition screen.
	 * @param Canvas - The canvas to use for rendering.
	 */
	virtual void DrawTransition(class UCanvas* Canvas);

	/** Print a centered transition message with a drop shadow. */
	virtual void DrawTransitionMessage(class UCanvas* Canvas,const FString& Message);

	/**
	 * Notifies all interactions that a new player has been added to the list of active players.
	 *
	 * @param	PlayerIndex		the index [into the GamePlayers array] where the player was inserted
	 * @param	AddedPlayer		the player that was added
	 */
	virtual void NotifyPlayerAdded( int32 PlayerIndex, class ULocalPlayer* AddedPlayer );

	/**
	 * Notifies all interactions that a new player has been added to the list of active players.
	 *
	 * @param	PlayerIndex		the index [into the GamePlayers array] where the player was located
	 * @param	RemovedPlayer	the player that was removed
	 */
	void NotifyPlayerRemoved( int32 PlayerIndex, class ULocalPlayer* RemovedPlayer );

	/**
	 * Adds a LocalPlayer to the local and global list of Players.
	 *
	 * @param	NewPlayer	the player to add
	 * @param	ControllerId id of the controller associated with the player
	 */
	int32 AddLocalPlayer( class ULocalPlayer* NewPlayer, int32 ControllerId );

	/**
	 * Removes a LocalPlayer from the local and global list of Players.
	 *
	 * @param	ExistingPlayer	the player to remove
	 */
	int32 RemoveLocalPlayer( class ULocalPlayer* ExistingPlayer );

	/**
	 * Notification of server travel error messages, generally network connection related (package verification, client server handshaking, etc) 
	 * generally not expected to handle the failure here, but provide notification to the user
	 *
	 * @param	FailureType	the type of error
	 * @param	ErrorString	additional string detailing the error
	 */
	virtual void PeekTravelFailureMessages(UWorld* InWorld, enum ETravelFailure::Type FailureType, const FString& ErrorString);

	/**
	 * Notification of network error messages
	 * generally not expected to handle the failure here, but provide notification to the user
	 *
	 * @param	FailureType	the type of error
	 * @param	NetDriverName name of the network driver generating the error
	 * @param	ErrorString	additional string detailing the error
	 */
	virtual void PeekNetworkFailureMessages(UWorld *World, UNetDriver *NetDriver, enum ENetworkFailure::Type FailureType, const FString& ErrorString);

	/**
	 * Retrieves a reference to a LocalPlayer.
	 *
	 * @param	PlayerIndex		if specified, returns the player at this index in the GamePlayers array.  Otherwise, returns
	 *							the player associated with the owner scene.
	 * @return	the player that owns this scene or is located in the specified index of the GamePlayers array.
	 */
	virtual void OnPrimaryPlayerSwitch(class ULocalPlayer* OldPrimaryPlayer, class ULocalPlayer* NewPrimaryPlayer);


	/** Make sure all navigation objects have appropriate path rendering components set.  Called when EngineShowFlags.Navigation is set. */
	virtual void VerifyPathRenderingComponents();

	/** Accessor for delegate called when a png screenshot is captured */
	FOnPNGScreenshotCaptured& OnPNGScreenshotCaptured()
	{
		return PNGScreenshotCapturedDelegate;
	}

	/** Accessor for delegate called when a screenshot is captured */
	static FOnScreenshotCaptured& OnScreenshotCaptured()
	{
		return ScreenshotCapturedDelegate;
	}

	/** Return the engine show flags for this viewport */
	virtual FEngineShowFlags* GetEngineShowFlags() 
	{ 
		return &EngineShowFlags; 
	}

public:
	/** The show flags used by the viewport's players. */
	FEngineShowFlags EngineShowFlags;

	/** The platform-specific viewport which this viewport client is attached to. */
	FViewport* Viewport;

	/** The platform-specific viewport frame which this viewport is contained by. */
	FViewportFrame* ViewportFrame;

	/*
 	 * Controls supression of the blue transition text messages 
	 * 
	 * @param bSuppress	Pass true to supress messages
	 */
	void SetSuppressTransitionMessage( bool bSuppress )
	{
		bSuppressTransitionMessage = bSuppress;
	}

	/**
	 * Get a ptr to the stat unit data for this viewport
	 */
	virtual FStatUnitData* GetStatUnitData() const OVERRIDE
	{
		return StatUnitData;
	}

	/**
	 * Get a ptr to the stat unit data for this viewport
	 */
	virtual FStatHitchesData* GetStatHitchesData() const OVERRIDE
	{
		return StatHitchesData;
	}

	/**
	 * Get a ptr to the enabled stats list
	 */
	virtual const TArray<FString>* GetEnabledStats() const OVERRIDE
	{
		return &EnabledStats;
	}

	/**
	 * Sets all the stats that should be enabled for the viewport
	 */
	virtual void SetEnabledStats(const TArray<FString>& InEnabledStats) OVERRIDE
	{
		EnabledStats = InEnabledStats;
	}

	/**
	 * Check whether a specific stat is enabled for this viewport
	 */
	virtual bool IsStatEnabled(const TCHAR* InName) const OVERRIDE
	{
		return EnabledStats.Contains(InName);
	}

	/**
	 * Get the sound stat flags enabled for this viewport
	 */
	virtual ESoundShowFlags::Type GetSoundShowFlags() const OVERRIDE
	{ 
		return SoundShowFlags;
	}

	/**
	 * Set the sound stat flags enabled for this viewport
	 */
	virtual void SetSoundShowFlags(const ESoundShowFlags::Type InSoundShowFlags) OVERRIDE
	{
		SoundShowFlags = InSoundShowFlags;
	}

private:
	/**
	 * Set a specific stat to either enabled or disabled (returns the number of remaining enabled stats)
	 */
	int32 SetStatEnabled(const TCHAR* InName, const bool bEnable, const bool bAll = false)
	{
		if (bEnable)
		{
			check(!bAll);	// Not possible to enable all
			EnabledStats.AddUnique(InName);
		}
		else
		{
			if (bAll)
			{
				EnabledStats.Empty();
			}
			else
			{
				EnabledStats.Remove(InName);
			}
		}
		return EnabledStats.Num();
	}

	/** Delegate handler to see if a stat is enabled on this viewport */
	void HandleViewportStatCheckEnabled(const TCHAR* InName, bool& bOutCurrentEnabled, bool& bOutOthersEnabled);

	/** Delegate handler for when stats are enabled in a viewport */
	void HandleViewportStatEnabled(const TCHAR* InName);

	/** Delegate handler for when stats are disabled in a viewport */
	void HandleViewportStatDisabled(const TCHAR* InName);

	/** Delegate handler for when all stats are disabled in a viewport */
	void HandleViewportStatDisableAll(const bool bInAnyViewport);

private:
	/** Slate window associated with this viewport client.  The same window may host more than one viewport client. */
	TWeakPtr<class SWindow> Window;

	/** Overlay widget that contains widgets to draw on top of the game viewport */
	TWeakPtr<class SOverlay> ViewportOverlayWidget;

	/** Current buffer visualization mode for this game viewport */
	FName CurrentBufferVisualizationMode;

	/** Weak pointer to the highres screenshot dialog if it's open */
	TWeakPtr<class SWindow> HighResScreenshotDialog;

	// Function that handles bug screen-shot requests w/ or w/o extra HUD info (project-specific)
	bool RequestBugScreenShot(const TCHAR* Cmd, bool bDisplayHUDInfo);

	/** Applies requested changes to display configuration 
	* @param	Dimensions - Pointer to new dimensions of the display. NULL for no change.
	* @param	WindowMode - What window mode do we want to st the display to.
	*/
	bool SetDisplayConfiguration( const FIntPoint* Dimensions, EWindowMode::Type WindowMode);

	/** Delegate called at the end of the frame when a screenshot is captured and a .png is requested */
	FOnPNGScreenshotCaptured PNGScreenshotCapturedDelegate;

	/** Delegate called at the end of the frame when a screenshot is captured */
	static FOnScreenshotCaptured ScreenshotCapturedDelegate;

	/** Data needed to display perframe stat tracking when STAT UNIT is enabled */
	FStatUnitData* StatUnitData;

	/** Data needed to display perframe stat tracking when STAT HITCHES is enabled */
	FStatHitchesData* StatHitchesData;

	/** A list of all the stat names which are enabled for this viewport (static so they persist between runs) */
	static TArray<FString> EnabledStats;

	/** Those sound stat flags which are enabled on this viewport */
	static ESoundShowFlags::Type SoundShowFlags;
};



