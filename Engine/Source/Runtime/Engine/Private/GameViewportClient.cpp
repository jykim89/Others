// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "ParticleDefinitions.h"
#include "SoundDefinitions.h"
#include "FXSystem.h"
#include "SubtitleManager.h"
#include "ImageUtils.h"

#include "RenderCore.h"
#include "ColorList.h"
#include "Slate.h"
#include "SceneViewExtension.h"
#include "IHeadMountedDisplay.h"
#include "SVirtualJoystick.h"
#include "SceneViewport.h"

#include "HighResScreenshot.h"

/** This variable allows forcing full screen of the first player controller viewport, even if there are multiple controllers plugged in and no cinematic playing. */
bool GForceFullscreen = false;

/** Whether to visualize the lightmap selected by the Debug Camera. */
extern ENGINE_API bool GShowDebugSelectedLightmap;
/** The currently selected component in the actor. */
extern ENGINE_API UPrimitiveComponent* GDebugSelectedComponent;
/** The lightmap used by the currently selected component, if it's a static mesh component. */
extern ENGINE_API FLightMap2D* GDebugSelectedLightmap;

/** Delegate called at the end of the frame when a screenshot is captured */
FOnScreenshotCaptured UGameViewportClient::ScreenshotCapturedDelegate;

/** A list of all the stat names which are enabled for this viewport (static so they persist between runs) */
TArray<FString> UGameViewportClient::EnabledStats;

/** Those sound stat flags which are enabled on this viewport */
FViewportClient::ESoundShowFlags::Type UGameViewportClient::SoundShowFlags = FViewportClient::ESoundShowFlags::Disabled;

DEFINE_STAT(STAT_UIDrawingTime);

/**
 * Draw debug info on a game scene view.
 */
class FGameViewDrawer : public FViewElementDrawer
{
public:
	/**
	 * Draws debug info using the given draw interface.
	 */
	virtual void Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Draw a wireframe sphere around the selected lightmap, if requested.
		if ( GShowDebugSelectedLightmap && GDebugSelectedComponent && GDebugSelectedLightmap )
		{
			float Radius = GDebugSelectedComponent->Bounds.SphereRadius;
			int32 Sides = FMath::Clamp<int32>( FMath::TruncToInt(Radius*Radius*4.0f*PI/(80.0f*80.0f)), 8, 200 );
			DrawWireSphere( PDI, GDebugSelectedComponent->Bounds.Origin, FColor(255,130,0), GDebugSelectedComponent->Bounds.SphereRadius, Sides, SDPG_Foreground );
		}
#endif
	}
};

UGameViewportClient::UGameViewportClient(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	, EngineShowFlags(ESFIM_Game)
	, CurrentBufferVisualizationMode(NAME_None)
	, HighResScreenshotDialog(NULL)
{

	TitleSafeZone.MaxPercentX = 0.9f;
	TitleSafeZone.MaxPercentY = 0.9f;
	TitleSafeZone.RecommendedPercentX = 0.8f;
	TitleSafeZone.RecommendedPercentY = 0.8f;

	bIsPlayInEditorViewport = false;
	ProgressFadeTime = 1.0f;
	ViewModeIndex = VMI_Lit;

	SplitscreenInfo.Init(FSplitscreenData(), ESplitScreenType::SplitTypeCount);

	SplitscreenInfo[ESplitScreenType::None].PlayerData.Add(FPerPlayerSplitscreenData(1.0f, 1.0f, 0.0f, 0.0f));

	SplitscreenInfo[ESplitScreenType::TwoPlayer_Horizontal].PlayerData.Add(FPerPlayerSplitscreenData(1.0f, 0.5f, 0.0f, 0.0f));
	SplitscreenInfo[ESplitScreenType::TwoPlayer_Horizontal].PlayerData.Add(FPerPlayerSplitscreenData(1.0f, 0.5f, 0.0f, 0.5f));

	SplitscreenInfo[ESplitScreenType::TwoPlayer_Vertical].PlayerData.Add(FPerPlayerSplitscreenData(0.5f, 1.0f, 0.0f, 0.0f));
	SplitscreenInfo[ESplitScreenType::TwoPlayer_Vertical].PlayerData.Add(FPerPlayerSplitscreenData(0.5f, 1.0f, 0.5f, 0.0f));

	SplitscreenInfo[ESplitScreenType::ThreePlayer_FavorTop].PlayerData.Add(FPerPlayerSplitscreenData(1.0f, 0.5f, 0.0f, 0.0f));
	SplitscreenInfo[ESplitScreenType::ThreePlayer_FavorTop].PlayerData.Add(FPerPlayerSplitscreenData(0.5f, 0.5f, 0.0f, 0.5f));
	SplitscreenInfo[ESplitScreenType::ThreePlayer_FavorTop].PlayerData.Add(FPerPlayerSplitscreenData(0.5f, 0.5f, 0.5f, 0.5f));

	SplitscreenInfo[ESplitScreenType::ThreePlayer_FavorBottom].PlayerData.Add(FPerPlayerSplitscreenData(0.5f, 0.5f, 0.0f, 0.0f));
	SplitscreenInfo[ESplitScreenType::ThreePlayer_FavorBottom].PlayerData.Add(FPerPlayerSplitscreenData(0.5f, 0.5f, 0.5f, 0.0f));
	SplitscreenInfo[ESplitScreenType::ThreePlayer_FavorBottom].PlayerData.Add(FPerPlayerSplitscreenData(1.0f, 0.5f, 0.0f, 0.5f));

	SplitscreenInfo[ESplitScreenType::FourPlayer].PlayerData.Add(FPerPlayerSplitscreenData(0.5f, 0.5f, 0.0f, 0.0f));
	SplitscreenInfo[ESplitScreenType::FourPlayer].PlayerData.Add(FPerPlayerSplitscreenData(0.5f, 0.5f, 0.5f, 0.0f));
	SplitscreenInfo[ESplitScreenType::FourPlayer].PlayerData.Add(FPerPlayerSplitscreenData(0.5f, 0.5f, 0.0f, 0.5f));
	SplitscreenInfo[ESplitScreenType::FourPlayer].PlayerData.Add(FPerPlayerSplitscreenData(0.5f, 0.5f, 0.5f, 0.5f));

	MaxSplitscreenPlayers = 4;
	bSuppressTransitionMessage = false;

	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		StatUnitData = new FStatUnitData();
		StatHitchesData = new FStatHitchesData();
		FCoreDelegates::StatCheckEnabled.AddUObject(this, &UGameViewportClient::HandleViewportStatCheckEnabled);
		FCoreDelegates::StatEnabled.AddUObject(this, &UGameViewportClient::HandleViewportStatEnabled);
		FCoreDelegates::StatDisabled.AddUObject(this, &UGameViewportClient::HandleViewportStatDisabled);
		FCoreDelegates::StatDisableAll.AddUObject(this, &UGameViewportClient::HandleViewportStatDisableAll);
	}
}


UGameViewportClient::~UGameViewportClient()
{
	FCoreDelegates::StatCheckEnabled.RemoveAll(this);
	FCoreDelegates::StatEnabled.RemoveAll(this);
	FCoreDelegates::StatDisabled.RemoveAll(this);
	FCoreDelegates::StatDisableAll.RemoveAll(this);
	if (StatHitchesData)
	{
		delete StatHitchesData;
		StatHitchesData = NULL;
	}
	if (StatUnitData)
	{
		delete StatUnitData;
		StatUnitData = NULL;
	}
}


void UGameViewportClient::PostInitProperties()
{
	Super::PostInitProperties();
	EngineShowFlags = FEngineShowFlags(ESFIM_Game);
}

void UGameViewportClient::BeginDestroy()
{
	RemoveAllViewportWidgets();
	Super::BeginDestroy();
}


void UGameViewportClient::DetachViewportClient()
{
	ViewportConsole = NULL;
	RemoveFromRoot();
}

FSceneViewport* UGameViewportClient::GetGameViewport()
{
	return static_cast<FSceneViewport*>(Viewport);
}


void UGameViewportClient::Tick( float DeltaTime )
{
}

FString UGameViewportClient::ConsoleCommand( const FString& Command)
{
	FString TruncatedCommand = Command.Left(1000);
	FConsoleOutputDevice ConsoleOut(ViewportConsole);
	Exec( GetWorld(), *TruncatedCommand,ConsoleOut);
	return *ConsoleOut;
}

void UGameViewportClient::SetReferenceToWorldContext(struct FWorldContext& WorldContext)
{
	WorldContext.AddRef(World);
}

UWorld* UGameViewportClient::GetWorld() const
{
	return World;
}

bool UGameViewportClient::InputKey(FViewport* InViewport, int32 ControllerId, FKey Key, EInputEvent EventType, float AmountDepressed, bool bGamepad)
{
	if (InViewport->IsPlayInEditorViewport() && Key.IsGamepadKey())
	{
		GEngine->RemapGamepadControllerIdForPIE(this, ControllerId);
	}

	// route to subsystems that care
	bool bResult = (ViewportConsole ? ViewportConsole->InputKey(ControllerId, Key, EventType, AmountDepressed, bGamepad) : false);
	if (!bResult)
	{
		ULocalPlayer* const TargetPlayer = GEngine->GetLocalPlayerFromControllerId(this, ControllerId);
		if (TargetPlayer && TargetPlayer->PlayerController)
		{
			bResult = TargetPlayer->PlayerController->InputKey(Key, EventType, AmountDepressed, bGamepad);
		}

		// A gameviewport is always considered to have responded to a mouse buttons to avoid throttling
		if (!bResult && Key.IsMouseButton())
		{
			bResult = true;
		}
	}

	// For PIE, let the next PIE window handle the input if we didn't
	// (this allows people to use multiple controllers to control each window)
	if (!bResult && ControllerId > 0 && InViewport->IsPlayInEditorViewport())
	{
		UGameViewportClient *NextViewport = GEngine->GetNextPIEViewport(this);
		if (NextViewport)
		{
			bResult = NextViewport->InputKey(InViewport, ControllerId-1, Key, EventType, AmountDepressed, bGamepad);
		}
	}

	return bResult;
}


bool UGameViewportClient::InputAxis(FViewport* InViewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime, int32 NumSamples, bool bGamepad)
{
	bool bResult = false;

	if (InViewport->IsPlayInEditorViewport() && Key.IsGamepadKey())
	{
		GEngine->RemapGamepadControllerIdForPIE(this, ControllerId);
	}

	// Don't allow mouse/joystick input axes while in PIE and the console has forced the cursor to be visible.  It's
	// just distracting when moving the mouse causes mouse look while you are trying to move the cursor over a button
	// in the editor!
	if( !( InViewport->IsSlateViewport() && InViewport->IsPlayInEditorViewport() ) || ViewportConsole == NULL || !ViewportConsole->ConsoleActive() )
	{
		// route to subsystems that care
		if (ViewportConsole != NULL)
		{
			bResult = ViewportConsole->InputAxis(ControllerId, Key, Delta, DeltaTime, NumSamples, bGamepad);
		}
		if (!bResult)
		{
			ULocalPlayer* const TargetPlayer = GEngine->GetLocalPlayerFromControllerId(this, ControllerId);
			if (TargetPlayer && TargetPlayer->PlayerController)
			{
				bResult = TargetPlayer->PlayerController->InputAxis(Key, Delta, DeltaTime, NumSamples, bGamepad);
			}
		}

		// For PIE, let the next PIE window handle the input if we didn't
		// (this allows people to use multiple controllers to control each window)
		if (!bResult && ControllerId > 0 && InViewport->IsPlayInEditorViewport())
		{
			UGameViewportClient *NextViewport = GEngine->GetNextPIEViewport(this);
			if (NextViewport)
			{
				bResult = NextViewport->InputAxis(InViewport, ControllerId-1, Key, Delta, DeltaTime, NumSamples, bGamepad);
			}
		}

		if( InViewport->IsSlateViewport() && InViewport->IsPlayInEditorViewport() )
		{
			// Absorb all keys so game input events are not routed to the Slate editor frame
			bResult = true;
		}
	}

	return bResult;
}


bool UGameViewportClient::InputChar(FViewport* InViewport, int32 ControllerId, TCHAR Character)
{
	// should probably just add a ctor to FString that takes a TCHAR
	FString CharacterString;
	CharacterString += Character;

	// route to subsystems that care
	bool bResult = (ViewportConsole ? ViewportConsole->InputChar(ControllerId, CharacterString) : false);

	if( InViewport->IsSlateViewport() && InViewport->IsPlayInEditorViewport() )
	{
		// Absorb all keys so game input events are not routed to the Slate editor frame
		bResult = true;
	}

	return bResult;
}

bool UGameViewportClient::InputTouch(FViewport* InViewport, int32 ControllerId, uint32 Handle, ETouchType::Type Type, const FVector2D& TouchLocation, FDateTime DeviceTimestamp, uint32 TouchpadIndex)
{
	// route to subsystems that care
	bool bResult = (ViewportConsole ? ViewportConsole->InputTouch(ControllerId, Handle, Type, TouchLocation, DeviceTimestamp, TouchpadIndex) : false);
	if (!bResult)
	{
		ULocalPlayer* const TargetPlayer = GEngine->GetLocalPlayerFromControllerId(this, ControllerId);
		if (TargetPlayer && TargetPlayer->PlayerController)
		{
			bResult = TargetPlayer->PlayerController->InputTouch(Handle, Type, TouchLocation, DeviceTimestamp, TouchpadIndex);
		}
	}

	return bResult;
}

bool UGameViewportClient::InputMotion(FViewport* InViewport, int32 ControllerId, const FVector& Tilt, const FVector& RotationRate, const FVector& Gravity, const FVector& Acceleration)
{
	// route to subsystems that care
	bool bResult = false;

	ULocalPlayer* const TargetPlayer = GEngine->GetLocalPlayerFromControllerId(this, ControllerId);
	if (TargetPlayer && TargetPlayer->PlayerController)
	{
		bResult = TargetPlayer->PlayerController->InputMotion(Tilt, RotationRate, Gravity, Acceleration);
	}

	return bResult;
}

void UGameViewportClient::SetIsSimulateInEditorViewport(bool bInIsSimulateInEditorViewport)
{
	if (GetDefault<UInputSettings>()->bUseMouseForTouch)
	{
		FSlateApplication::Get().SetGameIsFakingTouchEvents(!bInIsSimulateInEditorViewport);
	}

	for (ULocalPlayer* LocalPlayer : GetOuterUEngine()->GetGamePlayers(this))
	{
		if (LocalPlayer->PlayerController)
		{
			if (bInIsSimulateInEditorViewport)
			{
				LocalPlayer->PlayerController->CleanupGameViewport();
			}
			else
			{
				LocalPlayer->PlayerController->CreateTouchInterface();
			}
		}
	}
}

void UGameViewportClient::MouseEnter(FViewport* InViewport, int32 x, int32 y)
{
	Super::MouseEnter(InViewport, x, y);

	if (GetDefault<UInputSettings>()->bUseMouseForTouch && !GetGameViewport()->GetPlayInEditorIsSimulate())
	{
		FSlateApplication::Get().SetGameIsFakingTouchEvents(true);
	}
}

void UGameViewportClient::MouseLeave(FViewport* InViewport)
{
	Super::MouseLeave(InViewport);

	if (GetDefault<UInputSettings>()->bUseMouseForTouch)
	{
		UE_LOG(LogTemp, Log, TEXT("MouseLeave"));
		FIntPoint LastViewportCursorPos;
		Viewport->GetMousePos(LastViewportCursorPos, false);
		const FVector2D CurrentCursorPos = FSlateApplication::Get().GetCursorPos();
		FSlateApplication::Get().SetCursorPos(FVector2D(LastViewportCursorPos.X,LastViewportCursorPos.Y));
		FSlateApplication::Get().SetGameIsFakingTouchEvents(false);
		FSlateApplication::Get().SetCursorPos(CurrentCursorPos);
	}
}

FVector2D UGameViewportClient::GetMousePosition()
{
	if (Viewport == NULL)
	{
		return FVector2D(0.f, 0.f);
	}
	else
	{
		FIntPoint MousePos;
		Viewport->GetMousePos(MousePos);
		return FVector2D(MousePos);
	}
}


bool UGameViewportClient::RequiresUncapturedAxisInput() const
{
	bool bRequired = false;
	if (Viewport != NULL && Viewport->HasFocus())
	{
		if (ViewportConsole && ViewportConsole->ConsoleActive())
		{
			bRequired = true;
		}
		else if (GetWorld() && GetWorld()->GetFirstPlayerController())
		{
			bRequired = GetWorld()->GetFirstPlayerController()->ShouldShowMouseCursor();
		}
	}

	return bRequired;
}


EMouseCursor::Type UGameViewportClient::GetCursor(FViewport* InViewport, int32 X, int32 Y )
{
	bool bIsPlayingMovie = false;//GetMoviePlayer()->IsMovieCurrentlyPlaying();

#if !PLATFORM_WINDOWS
	bool bIsWithinTitleBar = false;
#else
	POINT CursorPos = { X, Y };
	RECT WindowRect;

	bool bIsWithinWindow = true;

	// For Slate based windows the viewport doesnt have access to the OS window handle and shouln't need it
	bool bIsWithinTitleBar = false;
	if( InViewport->GetWindow() )
	{
		ClientToScreen( (HWND)InViewport->GetWindow(), &CursorPos );
		GetWindowRect( (HWND)InViewport->GetWindow(), &WindowRect );
		bIsWithinWindow = ( CursorPos.x >= WindowRect.left && CursorPos.x <= WindowRect.right &&
			CursorPos.y >= WindowRect.top && CursorPos.y <= WindowRect.bottom );

		// The user is mousing over the title bar if Y is less than zero and within the window rect
		bIsWithinTitleBar = Y < 0 && bIsWithinWindow;
	}

#endif

	if ( !InViewport->HasFocus() || (ViewportConsole && ViewportConsole->ConsoleActive() ) )
	{
		return EMouseCursor::Default;
	}
	else if ( (!bIsPlayingMovie) && (InViewport->IsFullscreen() || !bIsWithinTitleBar) )
	{
		if (GetWorld() && GetWorld()->GetFirstPlayerController())
		{
			return GetWorld()->GetFirstPlayerController()->GetMouseCursor();
		}

		return EMouseCursor::None;
	}

	return FViewportClient::GetCursor(InViewport, X, Y);
}


void UGameViewportClient::SetDropDetail(float DeltaSeconds)
{
	if (GEngine)
	{
		float FrameTime = 0.0f;
		if (FPlatformProperties::SupportsWindowedMode() == false)
		{
			FrameTime	= FPlatformTime::ToSeconds(FMath::Max3<uint32>( GRenderThreadTime, GGameThreadTime, GGPUFrameTime ));
			// If DeltaSeconds is bigger than 34 ms we can take it into account as we're not VSYNCing in that case.
			if( DeltaSeconds > 0.034 )
			{
				FrameTime = FMath::Max( FrameTime, DeltaSeconds );
			}
		}
		else
		{
			FrameTime = DeltaSeconds;
		}
		const float FrameRate	= FrameTime > 0 ? 1 / FrameTime : 0;

		// Drop detail if framerate is below threshold.
		GetWorld()->bDropDetail		= FrameRate < FMath::Clamp(GEngine->MinDesiredFrameRate, 1.f, 100.f) && !FApp::IsBenchmarking() && !FApp::UseFixedTimeStep();
		GetWorld()->bAggressiveLOD	= FrameRate < FMath::Clamp(GEngine->MinDesiredFrameRate - 5.f, 1.f, 100.f) && !FApp::IsBenchmarking() && !FApp::UseFixedTimeStep();

		// this is slick way to be able to do something based on the frametime and whether we are bound by one thing or another
#if 0 
		// so where we check to see if we are above some threshold and below 150 ms (any thing above that is usually blocking loading of some sort)
		// also we don't want to do the auto trace when we are blocking on async loading
		if( ( 0.070 < FrameTime ) && ( FrameTime < 0.150 ) && GIsAsyncLoading == false && GetWorld()->bRequestedBlockOnAsyncLoading == false && (GetWorld()->GetTimeSeconds() > 30.0f )  )
		{
			// now check to see if we have done a trace in the last 30 seconds otherwise we will just trace ourselves to death
			static float LastTraceTime = -9999.0f;
			if( (LastTraceTime+30.0f < GetWorld()->GetTimeSeconds()))
			{
				LastTraceTime = GetWorld()->GetTimeSeconds();
				UE_LOG(LogPlayerManagement, Warning, TEXT("Auto Trace initiated!! FrameTime: %f"), FrameTime );

				// do what ever action you want here (e.g. trace <type>, GShouldLogOutAFrameOfMoveActor = true, c.f. LevelTick.cpp for more)
				//GShouldLogOutAFrameOfMoveActor = true;

#if !WITH_EDITORONLY_DATA
				UE_LOG(LogPlayerManagement, Warning, TEXT("    GGameThreadTime: %d GRenderThreadTime: %d "), GGameThreadTime, GRenderThreadTime );
#endif // WITH_EDITORONLY_DATA
			}
		}
#endif // 0 
	}
}


void UGameViewportClient::SetViewportFrame( FViewportFrame* InViewportFrame )
{
	ViewportFrame = InViewportFrame;
	SetViewport( ViewportFrame ? ViewportFrame->GetViewport() : NULL );
}


void UGameViewportClient::SetViewport( FViewport* InViewport )
{
	FViewport* PreviousViewport = Viewport;
	Viewport = InViewport;

	if ( PreviousViewport == NULL && Viewport != NULL )
	{
		// ensure that the player's Origin and Size members are initialized the moment we get a viewport
		LayoutPlayers();
	}
}



void UGameViewportClient::GetViewportSize( FVector2D& out_ViewportSize )
{
	if ( Viewport != NULL )
	{
		out_ViewportSize.X = Viewport->GetSizeXY().X;
		out_ViewportSize.Y = Viewport->GetSizeXY().Y;
	}
}

bool UGameViewportClient::IsFullScreenViewport()
{
	return Viewport->IsFullscreen();
}



bool UGameViewportClient::ShouldForceFullscreenViewport() const
{
	bool bResult = false;
	if ( GForceFullscreen )
	{
		bResult = true;
	}
	else if ( GetOuterUEngine()->GetNumGamePlayers(this) == 0 )
	{
		bResult = true;
	}
	else if ( GetWorld() )
	{
		if ( GetWorld()->bIsDefaultLevel )
		{
			bResult = true;
		}
		else
		{
			APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
			if( ( PlayerController ) && ( PlayerController->bCinematicMode ) )
			{
				bResult = true;
			}
		}
	}
	return bResult;
}

/** Util to find named canvas in transient package, and create if not found */
static UCanvas* GetCanvasByName(FName CanvasName)
{
	// Cache to avoid FString/FName conversions/compares
	static TMap<FName, UCanvas*> CanvasMap;
	UCanvas** FoundCanvas = CanvasMap.Find(CanvasName);
	if (!FoundCanvas)
	{
		UCanvas* CanvasObject = FindObject<UCanvas>(GetTransientPackage(),*CanvasName.ToString());
		if( !CanvasObject )
		{
			CanvasObject = NewNamedObject<UCanvas>(GetTransientPackage(), CanvasName);
			CanvasObject->AddToRoot();
		}

		CanvasMap.Add(CanvasName, CanvasObject);
		return CanvasObject;
	}

	return *FoundCanvas;
}

void UGameViewportClient::Draw(FViewport* InViewport, FCanvas* SceneCanvas)
{
	// Allow HMD to modify screen settings
	if (GEngine->HMDDevice.IsValid() && GEngine->IsStereoscopic3D())
	{
		GEngine->HMDDevice->UpdateScreenSettings(Viewport);
	}

	FCanvas* DebugCanvas = InViewport->GetDebugCanvas();

	// Create a temporary canvas if there isn't already one.
	static FName CanvasObjectName(TEXT("CanvasObject"));
	UCanvas* CanvasObject = GetCanvasByName(CanvasObjectName);
	CanvasObject->Canvas = SceneCanvas;	

	// Create temp debug canvas object
	static FName DebugCanvasObjectName(TEXT("DebugCanvasObject"));
	UCanvas* DebugCanvasObject = GetCanvasByName(DebugCanvasObjectName);
	DebugCanvasObject->Canvas = DebugCanvas;	
	DebugCanvasObject->Init(InViewport->GetSizeXY().X, InViewport->GetSizeXY().Y, NULL);

	bool bUIDisableWorldRendering = false;
	FGameViewDrawer GameViewDrawer;

	if (EngineShowFlags.VisualizeBuffer)
	{
		// Process the buffer visualization console command
		FName NewBufferVisualizationMode = NAME_None;
		static IConsoleVariable* ICVar = IConsoleManager::Get().FindConsoleVariable(FBufferVisualizationData::GetVisualizationTargetConsoleCommandName());
		if (ICVar)
		{
			static const FName OverviewName = TEXT("Overview");
			FString ModeNameString = ICVar->GetString();
			FName ModeName = *ModeNameString;
			if (ModeNameString.IsEmpty() || ModeName == OverviewName || ModeName == NAME_None)
			{
				NewBufferVisualizationMode = NAME_None;
			}
			else
			{
				if (GetBufferVisualizationData().GetMaterial(ModeName) == NULL)
				{
					// Mode is out of range, so display a message to the user, and reset the mode back to the previous valid one
					UE_LOG(LogConsoleResponse, Warning, TEXT("Buffer visualization mode '%s' does not exist"), *ModeNameString);
					NewBufferVisualizationMode = CurrentBufferVisualizationMode;
					ICVar->Set(*NewBufferVisualizationMode.GetPlainNameString());
				}
				else
				{
					NewBufferVisualizationMode = ModeName;
				}
			}
		}

		if (NewBufferVisualizationMode != CurrentBufferVisualizationMode)
		{
			CurrentBufferVisualizationMode = NewBufferVisualizationMode;
		}
	}

	// create the view family for rendering the world scene to the viewport's render target
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues( 	
		InViewport,
		GetWorld()->Scene,
		EngineShowFlags)
		.SetRealtimeUpdate(true));

	// Allow HMD to modify the view later, just before rendering
	if (GEngine->HMDDevice.IsValid() && GEngine->IsStereoscopic3D())
	{
		ISceneViewExtension* HmdViewExt = GEngine->HMDDevice->GetViewExtension();
		if (HmdViewExt)
		{
			ViewFamily.ViewExtensions.Add(HmdViewExt);
			HmdViewExt->ModifyShowFlags(ViewFamily.EngineShowFlags);
		}
	}


	ESplitScreenType::Type SplitScreenConfig = GetCurrentSplitscreenConfiguration();
	EngineShowFlagOverride(ESFIM_Game, (EViewModeIndex)ViewModeIndex, ViewFamily.EngineShowFlags, NAME_None, SplitScreenConfig != ESplitScreenType::None);

	TMap<ULocalPlayer*,FSceneView*> PlayerViewMap;

	FAudioDevice* AudioDevice = GEngine->GetAudioDevice();
	bool bReverbSettingsFound = false;
	FReverbSettings ReverbSettings;
	class AReverbVolume* ReverbVolume = NULL;

	for( FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator )
	{
		APlayerController* PlayerController = *Iterator;
		if( PlayerController )
		{
			ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(PlayerController->Player);
			if (LocalPlayer)
			{
				const bool bEnableStereo = GEngine->IsStereoscopic3D();
				int32 NumViews = bEnableStereo ? 2 : 1;

				for( int i = 0; i < NumViews; ++i )
				{
					// Calculate the player's view information.
					FVector		ViewLocation;
					FRotator	ViewRotation;

					EStereoscopicPass PassType = !bEnableStereo ? eSSP_FULL : ((i == 0) ? eSSP_LEFT_EYE : eSSP_RIGHT_EYE);

					FSceneView* View = LocalPlayer->CalcSceneView( &ViewFamily, ViewLocation, ViewRotation, InViewport, &GameViewDrawer, PassType );

					if (View)
					{
						// Add depth of field override regions.
						if (PlayerController->MyHUD)
						{
							if( !PlayerController->bCinematicMode )
							{
								View->UIBlurOverrideRectangles = PlayerController->MyHUD->GetUIBlurRectangles();
							}
							PlayerController->MyHUD->ClearUIBlurOverrideRects();
						}
					
						if (View->Family->EngineShowFlags.Wireframe)
						{
							// Wireframe color is emissive-only, and mesh-modifying materials do not use material substitution, hence...
							View->DiffuseOverrideParameter = FVector4(0.f, 0.f, 0.f, 0.f);
							View->SpecularOverrideParameter = FVector4(0.f, 0.f, 0.f, 0.f);
						}
						else if (View->Family->EngineShowFlags.OverrideDiffuseAndSpecular)
						{
							View->DiffuseOverrideParameter = FVector4(GEngine->LightingOnlyBrightness.R, GEngine->LightingOnlyBrightness.G, GEngine->LightingOnlyBrightness.B, 0.0f);
							View->SpecularOverrideParameter = FVector4(.1f, .1f, .1f, 0.0f);
						}
						else if (View->Family->EngineShowFlags.ReflectionOverride)
						{
							View->DiffuseOverrideParameter = FVector4(0.f, 0.f, 0.f, 0.f);
							View->SpecularOverrideParameter = FVector4(1, 1, 1, 0.0f);
							View->NormalOverrideParameter = FVector4(0, 0, 1, 0.0f);
							View->RoughnessOverrideParameter = FVector2D(0.0f, 0.0f);
						}


						if (!View->Family->EngineShowFlags.Diffuse)
						{
							View->DiffuseOverrideParameter = FVector4(0.f, 0.f, 0.f, 0.f);
						}

						if (!View->Family->EngineShowFlags.Specular)
						{
							View->SpecularOverrideParameter = FVector4(0.f, 0.f, 0.f, 0.f);
						}

						View->CurrentBufferVisualizationMode = CurrentBufferVisualizationMode;

						View->CameraConstrainedViewRect = View->UnscaledViewRect;

						// If this is the primary drawing pass, update things that depend on the view location
						if( i == 0 )
						{
							// Save the location of the view.
							LocalPlayer->LastViewLocation = ViewLocation;

							PlayerViewMap.Add(LocalPlayer,View);

							// Update the listener.
							if (AudioDevice != NULL)
							{
								FVector Location;
								FVector ProjFront;
								FVector ProjRight;
								PlayerController->GetAudioListenerPosition(/*out*/ Location, /*out*/ ProjFront, /*out*/ ProjRight);

								FTransform ListenerTransform(FRotationMatrix::MakeFromXY(ProjFront, ProjRight));
								ListenerTransform.SetTranslation(Location);
								ListenerTransform.NormalizeRotation();

								bReverbSettingsFound = true;

								FReverbSettings PlayerReverbSettings;
								FInteriorSettings PlayerInteriorSettings;
								class AReverbVolume* PlayerReverbVolume = GetWorld()->GetAudioSettings( ViewLocation, &PlayerReverbSettings, &PlayerInteriorSettings );

								if (ReverbVolume == NULL || (PlayerReverbVolume != NULL && PlayerReverbVolume->Priority > ReverbVolume->Priority))
								{
									ReverbVolume = PlayerReverbVolume;
									ReverbSettings = PlayerReverbSettings;
								}

								uint32 ViewportIndex = PlayerViewMap.Num()-1;
								AudioDevice->SetListener(ViewportIndex, ListenerTransform, (View->bCameraCut ? 0.f : GetWorld()->GetDeltaSeconds()), PlayerReverbVolume, PlayerInteriorSettings);
							}
					
						}
						
						// Add view information for resource streaming.
						IStreamingManager::Get().AddViewInformation( View->ViewMatrices.ViewOrigin, View->ViewRect.Width(), View->ViewRect.Width() * View->ViewMatrices.ProjMatrix.M[0][0] );
						GetWorld()->ViewLocationsRenderedLastFrame.Add(View->ViewMatrices.ViewOrigin);
					}
				}
			}
		}
	}

	if (bReverbSettingsFound)
	{
		AudioDevice->SetReverbSettings( ReverbVolume, ReverbSettings );
	}

	// Update level streaming.
	GetWorld()->UpdateLevelStreaming( &ViewFamily );

	// Draw the player views.
	if (!bDisableWorldRendering && !bUIDisableWorldRendering && PlayerViewMap.Num() > 0)
	{
		GetRendererModule().BeginRenderingViewFamily(SceneCanvas,&ViewFamily);
	}

	// Clear areas of the rendertarget (backbuffer) that aren't drawn over by the views.
	{
		// Find largest rectangle bounded by all rendered views.
		uint32 MinX=InViewport->GetSizeXY().X, MinY=InViewport->GetSizeXY().Y, MaxX=0, MaxY=0;
		uint32 TotalArea = 0;
		for( int32 ViewIndex = 0; ViewIndex < ViewFamily.Views.Num(); ++ViewIndex )
		{
			const FSceneView* View = ViewFamily.Views[ViewIndex];

			FIntRect UpscaledViewRect = View->UnscaledViewRect;

			MinX = FMath::Min<uint32>(UpscaledViewRect.Min.X, MinX);
			MinY = FMath::Min<uint32>(UpscaledViewRect.Min.Y, MinY);
			MaxX = FMath::Max<uint32>(UpscaledViewRect.Max.X, MaxX);
			MaxY = FMath::Max<uint32>(UpscaledViewRect.Max.Y, MaxY);
			TotalArea += FMath::TruncToInt(UpscaledViewRect.Width()) * FMath::TruncToInt(UpscaledViewRect.Height());
		}

		// To draw black borders around the rendered image (prevents artifacts from post processing passes that read outside of the image e.g. PostProcessAA)
		{
			static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.BlackBorders"));
			int32 BlackBorders = FMath::Clamp(CVar->GetValueOnGameThread(), 0, 10);

			if(ViewFamily.Views.Num() == 1 && BlackBorders)
			{
				MinX += BlackBorders;
				MinY += BlackBorders;
				MaxX -= BlackBorders;
				MaxY -= BlackBorders;
				TotalArea = (MaxX - MinX) * (MaxY - MinY);
			}
		}

		// If the views don't cover the entire bounding rectangle, clear the entire buffer.
		if ( ViewFamily.Views.Num() == 0 || TotalArea != (MaxX-MinX)*(MaxY-MinY) || bDisableWorldRendering )
		{
			SceneCanvas->DrawTile(0,0,InViewport->GetSizeXY().X,InViewport->GetSizeXY().Y,0.0f,0.0f,1.0f,1.f,FLinearColor::Black,NULL,false);
		}
		else
		{
			// clear left
			if( MinX > 0 )
			{
				SceneCanvas->DrawTile(0,0,MinX,InViewport->GetSizeXY().Y,0.0f,0.0f,1.0f,1.f,FLinearColor::Black,NULL,false);
			}
			// clear right
			if( MaxX < (uint32)InViewport->GetSizeXY().X )
			{
				SceneCanvas->DrawTile(MaxX,0,InViewport->GetSizeXY().X,InViewport->GetSizeXY().Y,0.0f,0.0f,1.0f,1.f,FLinearColor::Black,NULL,false);
			}
			// clear top
			if( MinY > 0 )
			{
				SceneCanvas->DrawTile(MinX,0,MaxX,MinY,0.0f,0.0f,1.0f,1.f,FLinearColor::Black,NULL,false);
			}
			// clear bottom
			if( MaxY < (uint32)InViewport->GetSizeXY().Y )
			{
				SceneCanvas->DrawTile(MinX,MaxY,MaxX,InViewport->GetSizeXY().Y,0.0f,0.0f,1.0f,1.f,FLinearColor::Black,NULL,false);
			}
		}
	}
	
	// Remove temporary debug lines.
	if (GetWorld()->LineBatcher != NULL)
	{
		GetWorld()->LineBatcher->Flush();
	}

	if (GetWorld()->ForegroundLineBatcher != NULL)
	{
		GetWorld()->ForegroundLineBatcher->Flush();
	}

	// Draw FX debug information.
	GetWorld()->FXSystem->DrawDebug( DebugCanvas );	

	// Render the UI.
	{
		SCOPE_CYCLE_COUNTER(STAT_UIDrawingTime);

		// render HUD
		bool bDisplayedSubtitles = false;
		for( FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator )
		{
			APlayerController* PlayerController = *Iterator;
			if (PlayerController)
			{
				ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(PlayerController->Player);
				if( LocalPlayer )
				{
					FSceneView* View = PlayerViewMap.FindRef(LocalPlayer);
					if (View != NULL)
					{
						// rendering to directly to viewport target
						FVector CanvasOrigin(FMath::TruncToFloat(View->UnscaledViewRect.Min.X), FMath::TruncToInt(View->UnscaledViewRect.Min.Y), 0.f);
												
						CanvasObject->Init(View->UnscaledViewRect.Width(), View->UnscaledViewRect.Height(), View);

						// Set the canvas transform for the player's view rectangle.
						SceneCanvas->PushAbsoluteTransform(FTranslationMatrix(CanvasOrigin));						
						CanvasObject->ApplySafeZoneTransform();

						// Render the player's HUD.
						if( PlayerController->MyHUD )
						{
							SCOPE_CYCLE_COUNTER(STAT_HudTime);

							DebugCanvasObject->SceneView = View;
							PlayerController->MyHUD->SetCanvas(CanvasObject, DebugCanvasObject);
							PlayerController->MyHUD->PostRender();
							
							// Put these pointers back as if a blueprint breakpoint hits during HUD PostRender they can
							// have been changed
							CanvasObject->Canvas = SceneCanvas;
							DebugCanvasObject->Canvas = DebugCanvas;

							// A side effect of PostRender is that the playercontroller could be destroyed
							if (!PlayerController->IsPendingKill())
							{
								PlayerController->MyHUD->SetCanvas(NULL, NULL);
							}
						}

						if (DebugCanvas != NULL )
						{
							DebugCanvas->PushAbsoluteTransform(FTranslationMatrix(CanvasOrigin));
							UDebugDrawService::Draw(ViewFamily.EngineShowFlags, InViewport, View, DebugCanvas);
							DebugCanvas->PopTransform();
						}

						CanvasObject->PopSafeZoneTransform();
						SceneCanvas->PopTransform();

						// draw subtitles
						if (!bDisplayedSubtitles)
						{
							FVector2D MinPos(0.f, 0.f);
							FVector2D MaxPos(1.f, 1.f);
							GetSubtitleRegion(MinPos, MaxPos);

							uint32 SizeX = SceneCanvas->GetRenderTarget()->GetSizeXY().X;
							uint32 SizeY = SceneCanvas->GetRenderTarget()->GetSizeXY().Y;
							FIntRect SubtitleRegion(FMath::TruncToInt(SizeX * MinPos.X), FMath::TruncToInt(SizeY * MinPos.Y), FMath::TruncToInt(SizeX * MaxPos.X), FMath::TruncToInt(SizeY * MaxPos.Y));
							// We need a world to do this
							FSubtitleManager::GetSubtitleManager()->DisplaySubtitles( SceneCanvas, SubtitleRegion, GetWorld()->GetAudioTimeSeconds() );
						}
					}
				}
			}
		}

		//ensure canvas has been flushed before rendering UI
		SceneCanvas->Flush();
		if (DebugCanvas != NULL)
		{
			DebugCanvas->Flush();
		}
		// Allow the viewport to render additional stuff
		PostRender(DebugCanvasObject);
		
		// Render the console.
		if (ViewportConsole)
		{
			if (GEngine->IsStereoscopic3D())
			{
				GEngine->StereoRenderingDevice->PushViewportCanvas(eSSP_LEFT_EYE, DebugCanvas, DebugCanvasObject, Viewport);
				ViewportConsole->PostRender_Console(DebugCanvasObject);
				DebugCanvas->PopTransform();

				GEngine->StereoRenderingDevice->PushViewportCanvas(eSSP_RIGHT_EYE, DebugCanvas, DebugCanvasObject, Viewport);
				ViewportConsole->PostRender_Console(DebugCanvasObject);
				if (DebugCanvas != NULL)
				{
					DebugCanvas->PopTransform();
				}

				// Reset the canvas for rendering to the full viewport.
				DebugCanvasObject->Reset();
				DebugCanvasObject->SizeX = Viewport->GetSizeXY().X;
				DebugCanvasObject->SizeY = Viewport->GetSizeXY().Y;
				DebugCanvasObject->SetView(NULL);
				DebugCanvasObject->Update();
			}
			else
			{
				ViewportConsole->PostRender_Console(DebugCanvasObject);
			}
		}
	}


	// Grab the player camera location and orientation so we can pass that along to the stats drawing code.
	FVector PlayerCameraLocation = FVector::ZeroVector;
	FRotator PlayerCameraRotation = FRotator::ZeroRotator;
	{
		for( FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator )
		{
			(*Iterator)->GetPlayerViewPoint( PlayerCameraLocation, PlayerCameraRotation );
		}
	}

	if (GEngine->IsStereoscopic3D())
	{
		GEngine->StereoRenderingDevice->PushViewportCanvas(eSSP_LEFT_EYE, DebugCanvas, DebugCanvasObject, InViewport);
		DrawStatsHUD(GetWorld(), InViewport, DebugCanvas, DebugCanvasObject, DebugProperties, PlayerCameraLocation, PlayerCameraRotation);
		DebugCanvas->PopTransform();

		GEngine->StereoRenderingDevice->PushViewportCanvas(eSSP_RIGHT_EYE, DebugCanvas, DebugCanvasObject, InViewport);
		DrawStatsHUD(GetWorld(), InViewport, DebugCanvas, DebugCanvasObject, DebugProperties, PlayerCameraLocation, PlayerCameraRotation);
		DebugCanvas->PopTransform();

		// Reset the canvas for rendering to the full viewport.
		DebugCanvasObject->Reset();
		DebugCanvasObject->SizeX = Viewport->GetSizeXY().X;
		DebugCanvasObject->SizeY = Viewport->GetSizeXY().Y;
		DebugCanvasObject->SetView(NULL);
		DebugCanvasObject->Update();
	}
	else
	{
		DrawStatsHUD( GetWorld(), InViewport, DebugCanvas, DebugCanvasObject, DebugProperties, PlayerCameraLocation, PlayerCameraRotation );
	}
}

void UGameViewportClient::ProcessScreenShots(FViewport* InViewport)
{
	if (GIsDumpingMovie || FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot)
	{
		TArray<FColor> Bitmap;

		TSharedPtr<SWindow> WindowPtr = GetWindow();
		if (!GIsDumpingMovie && (FScreenshotRequest::ShouldShowUI() && WindowPtr.IsValid()))
		{
			TSharedRef<SWindow> WindowRef = WindowPtr.ToSharedRef();
			FSlateApplication::Get().ForceRedrawWindow(WindowRef);
		}

		if (GetViewportScreenShot(InViewport, Bitmap))
		{
			if (ScreenshotCapturedDelegate.IsBound())
			{
				ScreenshotCapturedDelegate.Broadcast(InViewport->GetSizeXY().X, InViewport->GetSizeXY().Y, Bitmap);
			}
			else
			{
				FString ScreenShotName = FScreenshotRequest::GetFilename();
				if (GIsDumpingMovie && ScreenShotName.IsEmpty())
				{
					// Request a new screenshot with a formatted name
					FScreenshotRequest::RequestScreenshot(false);
					ScreenShotName = FScreenshotRequest::GetFilename();
				}

				if (PNGScreenshotCapturedDelegate.IsBound() && FPaths::GetExtension(ScreenShotName).ToLower() == TEXT("png"))
				{
					PNGScreenshotCapturedDelegate.Execute(InViewport->GetSizeXY().X, InViewport->GetSizeXY().Y, Bitmap, ScreenShotName);
				}
				else
				{
					// Save the contents of the array to a bitmap file.
					bool bWriteAlpha = false;
					FIntRect SourceRect(0, 0, GScreenshotResolutionX, GScreenshotResolutionY);
					if (GIsHighResScreenshot)
					{
						bWriteAlpha = GetHighResScreenshotConfig().MergeMaskIntoAlpha(Bitmap);
						SourceRect = GetHighResScreenshotConfig().CaptureRegion;
					}
					FFileHelper::CreateBitmap(*ScreenShotName, InViewport->GetSizeXY().X, InViewport->GetSizeXY().Y, Bitmap.GetTypedData(), &SourceRect, &IFileManager::Get(), NULL, bWriteAlpha);
				}
			}
		}

		FScreenshotRequest::Reset();
		// Reeanble screen messages - if we are NOT capturing a movie
		GAreScreenMessagesEnabled = GScreenMessagesRestoreState;
	}
}

void UGameViewportClient::Precache()
{
	if(!GIsEditor)
	{
		// Precache sounds...
		FAudioDevice* AudioDevice = GEngine ? GEngine->GetAudioDevice() : NULL;
		if (AudioDevice != NULL)
		{
			UE_LOG(LogPlayerManagement, Log, TEXT("Precaching sounds..."));
			for(TObjectIterator<USoundWave> It;It;++It)
			{
				USoundWave* SoundWave = *It;
				AudioDevice->Precache( SoundWave );
			}
			UE_LOG(LogPlayerManagement, Log, TEXT("Precaching sounds completed..."));
		}
	}

	// Log time till first precache is finished.
	static bool bIsFirstCallOfFunction = true;
	if( bIsFirstCallOfFunction )
	{
		UE_LOG(LogPlayerManagement, Log, TEXT("%5.2f seconds passed since startup."),FPlatformTime::Seconds()-GStartTime);
		bIsFirstCallOfFunction = false;
	}
}

void UGameViewportClient::LostFocus(FViewport* InViewport)
{
	// We need to reset some key inputs, since keyup events will sometimes not be processed (such as going into immersive/maximized mode).  
	// Resetting them will prevent them from "sticking"
	for( FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator )
	{
		APlayerController* PlayerController = *Iterator;
		if (PlayerController)
		{
			PlayerController->FlushPressedKeys();
		}
	}
}

void UGameViewportClient::ReceivedFocus(FViewport* InViewport)
{
	InViewport->CaptureJoystickInput(true);

	if (GetDefault<UInputSettings>()->bUseMouseForTouch && !GetGameViewport()->GetPlayInEditorIsSimulate())
	{
		FSlateApplication::Get().SetGameIsFakingTouchEvents(true);
	}
}

bool UGameViewportClient::IsFocused(FViewport* InViewport)
{
	return InViewport->HasFocus() || InViewport->HasMouseCapture();
}

void UGameViewportClient::CloseRequested(FViewport* InViewport)
{
	check(InViewport == Viewport);

	FSlateApplication::Get().SetGameIsFakingTouchEvents(false);

	SetViewportFrame(NULL);

	// If this viewport has a high res screenshot window attached to it, close it
	if (HighResScreenshotDialog.IsValid())
	{
		HighResScreenshotDialog.Pin()->RequestDestroyWindow();
		HighResScreenshotDialog = NULL;
	}
}

bool UGameViewportClient::IsOrtho() const
{
	return false;
}

void UGameViewportClient::PostRender(UCanvas* Canvas)
{
	if( bShowTitleSafeZone )
	{
		DrawTitleSafeArea(Canvas);
	}

	// Draw the transition screen.
	DrawTransition(Canvas);
}

void UGameViewportClient::PeekTravelFailureMessages(UWorld* InWorld, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogNet, Warning, TEXT("Travel Failure: [%s]: %s"), ETravelFailure::ToString(FailureType), *ErrorString);
}

void UGameViewportClient::PeekNetworkFailureMessages(UWorld *World, UNetDriver *NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogNet, Warning, TEXT("Network Failure: %s[%s]: %s"), NetDriver ? *NetDriver->NetDriverName.ToString() : TEXT("NULL"), ENetworkFailure::ToString(FailureType), *ErrorString);
}

ULocalPlayer* UGameViewportClient::CreatePlayer(int32 ControllerId, FString& OutError, bool bSpawnActor)
{
	checkf(GetOuterUEngine()->LocalPlayerClass != NULL);

	ULocalPlayer* NewPlayer = NULL;
	int32 InsertIndex = INDEX_NONE;

	if (GetOuterUEngine()->GetLocalPlayerFromControllerId(this, ControllerId) != NULL)
	{
		OutError = FString::Printf(TEXT("A local player already exists for controller ID %d,"), ControllerId);
	}
	else if (GetOuterUEngine()->GetNumGamePlayers(this) < MaxSplitscreenPlayers)
	{
		// If the controller ID is not specified then find the first available
		if (ControllerId < 0)
		{
			for (ControllerId = 0; ControllerId < MaxSplitscreenPlayers; ++ControllerId)
			{
				if (GetOuterUEngine()->GetLocalPlayerFromControllerId(this, ControllerId) == NULL)
				{
					break;
				}
			}
			check(ControllerId < MaxSplitscreenPlayers);
		}
		else if (ControllerId >= MaxSplitscreenPlayers)
		{
			UE_LOG(LogPlayerManagement, Warning, TEXT("Controller ID (%d) is unlikely to map to any physical device, so this player will not receive input"), ControllerId);
		}

		NewPlayer = CastChecked<ULocalPlayer>(StaticConstructObject(GetOuterUEngine()->LocalPlayerClass, GetOuter()));
		InsertIndex = AddLocalPlayer(NewPlayer, ControllerId);
		if ( bSpawnActor && InsertIndex != INDEX_NONE )
		{
			if (World->GetNetMode() != NM_Client)
			{
				// server; spawn a new PlayerController immediately
				if (!NewPlayer->SpawnPlayActor("", OutError, World ))
				{
					RemoveLocalPlayer(NewPlayer);
					NewPlayer = NULL;
				}
			}
			else
			{
				// client; ask the server to let the new player join
				NewPlayer->SendSplitJoin();
			}
		}
	}
	else
	{
		OutError = FString::Printf(TEXT("Maximum number of players (%d) already created.  Unable to create more."), MaxSplitscreenPlayers);
	}

	if (OutError != TEXT(""))
	{
		UE_LOG(LogPlayerManagement, Log, TEXT("UPlayer* creation failed with error: %s"), *OutError);
	}
	else
	{
		if ( NewPlayer != NULL && InsertIndex != INDEX_NONE )
		{
			NotifyPlayerAdded(InsertIndex, NewPlayer);
		}
	}
	return NewPlayer;
}


bool UGameViewportClient::RemovePlayer(ULocalPlayer* ExPlayer)
{
	//Mapping of: index into array is new index, value is old index

	// can't destroy viewports while connected to a server
	if (ExPlayer->PlayerController->Role == ROLE_Authority)
	{
		UE_LOG(LogPlayerManagement, Log, TEXT("Removing player %s  with ControllerId %s at index %s (%s existing players)"), ExPlayer, ExPlayer->ControllerId, GetOuterUEngine()->GetGamePlayers(this).Find(ExPlayer), GetOuterUEngine()->GetNumGamePlayers(this));
		
		if ( ExPlayer->PlayerController != NULL )
		{
			// Destroy the player's actors.
			ExPlayer->PlayerController->Destroy();
		}

		// Remove the player from the global and viewport lists of players.
		int32 OldIndex = RemoveLocalPlayer(ExPlayer);
		if ( OldIndex != INDEX_NONE )
		{
			NotifyPlayerRemoved(OldIndex, ExPlayer);
		}

		// Disassociate this viewport client from the player.
		// Do this after notifications, as some of them require the ViewportClient.
		ExPlayer->ViewportClient = NULL;

		UE_LOG(LogPlayerManagement, Log, TEXT("Finished removing player  %s  with ControllerId %i at index %i (%i remaining players)"), *ExPlayer->GetName(), ExPlayer->ControllerId, OldIndex, GetOuterUEngine()->GetNumGamePlayers(this));
		return true;
	}
	else
	{
		UEnum* NetRoleEnum = FindObject<UEnum>( NULL, TEXT( "ENetRole" ) );
		UE_LOG(LogPlayerManagement, Log, TEXT("Not removing player %s  with ControllerId %i because UPlayer* does not have appropriate role (%s"), *ExPlayer->GetName(), ExPlayer->ControllerId, *NetRoleEnum->GetEnum(ExPlayer->PlayerController->Role).ToString());
		return false;
	}
}

void UGameViewportClient::DebugCreatePlayer(int32 ControllerId)
{
#if !UE_BUILD_SHIPPING
	FString Error;
	CreatePlayer(ControllerId, Error, true);
	if (Error.Len() > 0)
	{
		UE_LOG(LogPlayerManagement, Error, TEXT("Failed to DebugCreatePlayer: %s"), *Error);
	}
#endif
}

void UGameViewportClient::SSSwapControllers()
{
#if !UE_BUILD_SHIPPING
	int32 TmpControllerID = GetOuterUEngine()->GetFirstGamePlayer(this)->ControllerId;

	for (int32 Idx=0; Idx<GetOuterUEngine()->GetNumGamePlayers(this)-1; ++Idx)
	{
		GetOuterUEngine()->GetGamePlayer(this, Idx)->ControllerId = GetOuterUEngine()->GetGamePlayer(this, Idx+1)->ControllerId;
	}
	GetOuterUEngine()->GetGamePlayer(this, GetOuterUEngine()->GetNumGamePlayers(this)-1)->ControllerId = TmpControllerID;
#endif
}


void UGameViewportClient::DebugRemovePlayer(int32 ControllerId)
{
#if !UE_BUILD_SHIPPING
	ULocalPlayer* const ExPlayer = GEngine->GetLocalPlayerFromControllerId(this, ControllerId);
	if (ExPlayer != NULL)
	{
		RemovePlayer(ExPlayer);
	}
#endif
}

void UGameViewportClient::ShowTitleSafeArea()
{
#if !UE_BUILD_SHIPPING
	bShowTitleSafeZone = !bShowTitleSafeZone;
#endif
}


void UGameViewportClient::SetConsoleTarget(int32 PlayerIndex)
{
#if !UE_BUILD_SHIPPING
	if (ViewportConsole)
	{
		if(PlayerIndex >= 0 && PlayerIndex < GetOuterUEngine()->GetNumGamePlayers(this))
		{
			ViewportConsole->ConsoleTargetPlayer = GetOuterUEngine()->GetGamePlayer(this, PlayerIndex);
		}
		else
		{
			ViewportConsole->ConsoleTargetPlayer = NULL;
		}
	}
#endif
}


ULocalPlayer* UGameViewportClient::Init(FString& OutError)
{
	checkf(GetOuterUEngine()->ConsoleClass != NULL);

	ActiveSplitscreenType = ESplitScreenType::None;

#if !UE_BUILD_SHIPPING
	// Create the viewport's console.
	ViewportConsole = Cast<UConsole>(StaticConstructObject(GetOuterUEngine()->ConsoleClass, this));
	// register console to get all log messages
	GLog->AddOutputDevice(ViewportConsole);
#endif // !UE_BUILD_SHIPPING

	// Keep an eye on any network or server travel failures
	GEngine->OnTravelFailure().AddUObject(this, &UGameViewportClient::PeekTravelFailureMessages);
	GEngine->OnNetworkFailure().AddUObject(this, &UGameViewportClient::PeekNetworkFailureMessages);

	// create the initial player - this is necessary or we can't render anything in-game.
	return CreateInitialPlayer(OutError);
}


ULocalPlayer* UGameViewportClient::CreateInitialPlayer( FString& OutError )
{
	return CreatePlayer(0, OutError, false);
}

void UGameViewportClient::UpdateActiveSplitscreenType()
{
	ESplitScreenType::Type SplitType = ESplitScreenType::None;
	const int32 NumPlayers = GEngine->GetNumGamePlayers(GetWorld());
	const UGameMapsSettings* Settings = GetDefault<UGameMapsSettings>();

	if (Settings->bUseSplitscreen)
	{
		switch (NumPlayers)
		{
		case 0:
		case 1:
			SplitType = ESplitScreenType::None;
			break;

		case 2:
			switch (Settings->TwoPlayerSplitscreenLayout)
			{
			case ETwoPlayerSplitScreenType::Horizontal:
				SplitType = ESplitScreenType::TwoPlayer_Horizontal;
				break;

			case ETwoPlayerSplitScreenType::Vertical:
				SplitType = ESplitScreenType::TwoPlayer_Vertical;
				break;

			default:
				check(0);
			}
			break;

		case 3:
			switch (Settings->ThreePlayerSplitscreenLayout)
			{
			case EThreePlayerSplitScreenType::FavorTop:
				SplitType = ESplitScreenType::ThreePlayer_FavorTop;
				break;

			case EThreePlayerSplitScreenType::FavorBottom:
				SplitType = ESplitScreenType::ThreePlayer_FavorBottom;
				break;

			default:
				check(0);
			}
			break;

		default:
			ensure(NumPlayers == 4);
			SplitType = ESplitScreenType::FourPlayer;
			break;
		}
	}
	else
	{
		SplitType = ESplitScreenType::None;
	}

	ActiveSplitscreenType = SplitType;
}

void UGameViewportClient::LayoutPlayers()
{
	UpdateActiveSplitscreenType();
	const ESplitScreenType::Type SplitType = GetCurrentSplitscreenConfiguration();
	
	// Initialize the players
	const TArray<ULocalPlayer*>& PlayerList = GetOuterUEngine()->GetGamePlayers(this);

	for ( int32 PlayerIdx = 0; PlayerIdx < PlayerList.Num(); PlayerIdx++ )
	{
		if ( SplitType < SplitscreenInfo.Num() && PlayerIdx < SplitscreenInfo[SplitType].PlayerData.Num() )
		{
			PlayerList[PlayerIdx]->Size.X =	SplitscreenInfo[SplitType].PlayerData[PlayerIdx].SizeX;
			PlayerList[PlayerIdx]->Size.Y =	SplitscreenInfo[SplitType].PlayerData[PlayerIdx].SizeY;
			PlayerList[PlayerIdx]->Origin.X =	SplitscreenInfo[SplitType].PlayerData[PlayerIdx].OriginX;
			PlayerList[PlayerIdx]->Origin.Y =	SplitscreenInfo[SplitType].PlayerData[PlayerIdx].OriginY;
		}
		else
		{
			PlayerList[PlayerIdx]->Size.X =	0.f;
			PlayerList[PlayerIdx]->Size.Y =	0.f;
			PlayerList[PlayerIdx]->Origin.X =	0.f;
			PlayerList[PlayerIdx]->Origin.Y =	0.f;
		}
	}
}


void UGameViewportClient::GetSubtitleRegion(FVector2D& MinPos, FVector2D& MaxPos)
{
	MaxPos.X = 1.0f;
	MaxPos.Y = (GetOuterUEngine()->GetNumGamePlayers(this) == 1) ? 0.9f : 0.5f;
}


int32 UGameViewportClient::ConvertLocalPlayerToGamePlayerIndex( ULocalPlayer* LPlayer )
{
	return GetOuterUEngine()->GetGamePlayers(this).Find( LPlayer );
}

bool UGameViewportClient::HasTopSafeZone( int32 LocalPlayerIndex )
{
	switch ( GetCurrentSplitscreenConfiguration() )
	{
	case ESplitScreenType::None:
	case ESplitScreenType::TwoPlayer_Vertical:
		return true;

	case ESplitScreenType::TwoPlayer_Horizontal:
	case ESplitScreenType::ThreePlayer_FavorTop:
		return (LocalPlayerIndex == 0) ? true : false;

	case ESplitScreenType::ThreePlayer_FavorBottom:
	case ESplitScreenType::FourPlayer:
		return (LocalPlayerIndex < 2) ? true : false;
	}

	return false;
}

bool UGameViewportClient::HasBottomSafeZone( int32 LocalPlayerIndex )
{
	switch ( GetCurrentSplitscreenConfiguration() )
	{
	case ESplitScreenType::None:
	case ESplitScreenType::TwoPlayer_Vertical:
		return true;

	case ESplitScreenType::TwoPlayer_Horizontal:
	case ESplitScreenType::ThreePlayer_FavorTop:
		return (LocalPlayerIndex == 0) ? false : true;

	case ESplitScreenType::ThreePlayer_FavorBottom:
	case ESplitScreenType::FourPlayer:
		return (LocalPlayerIndex > 1) ? true : false;
	}

	return false;
}

bool UGameViewportClient::HasLeftSafeZone( int32 LocalPlayerIndex )
{
	switch ( GetCurrentSplitscreenConfiguration() )
	{
	case ESplitScreenType::None:
	case ESplitScreenType::TwoPlayer_Horizontal:
		return true;

	case ESplitScreenType::TwoPlayer_Vertical:
		return (LocalPlayerIndex == 0) ? true : false;

	case ESplitScreenType::ThreePlayer_FavorTop:
		return (LocalPlayerIndex < 2) ? true : false;

	case ESplitScreenType::ThreePlayer_FavorBottom:
	case ESplitScreenType::FourPlayer:
		return (LocalPlayerIndex == 0 || LocalPlayerIndex == 2) ? true : false;
	}

	return false;
}

bool UGameViewportClient::HasRightSafeZone( int32 LocalPlayerIndex )
{
	switch ( GetCurrentSplitscreenConfiguration() )
	{
	case ESplitScreenType::None:
	case ESplitScreenType::TwoPlayer_Horizontal:
		return true;

	case ESplitScreenType::TwoPlayer_Vertical:
	case ESplitScreenType::ThreePlayer_FavorBottom:
		return (LocalPlayerIndex > 0) ? true : false;

	case ESplitScreenType::ThreePlayer_FavorTop:
		return (LocalPlayerIndex == 1) ? false : true;

	case ESplitScreenType::FourPlayer:
		return (LocalPlayerIndex == 0 || LocalPlayerIndex == 2) ? false : true;
	}

	return false;
}


void UGameViewportClient::GetPixelSizeOfScreen( float& Width, float& Height, UCanvas* Canvas, int32 LocalPlayerIndex )
{
	switch ( GetCurrentSplitscreenConfiguration() )
	{
	case ESplitScreenType::None:
		Width = Canvas->ClipX;
		Height = Canvas->ClipY;
		return;
	case ESplitScreenType::TwoPlayer_Horizontal:
		Width = Canvas->ClipX;
		Height = Canvas->ClipY * 2;
		return;
	case ESplitScreenType::TwoPlayer_Vertical:
		Width = Canvas->ClipX * 2;
		Height = Canvas->ClipY;
		return;
	case ESplitScreenType::ThreePlayer_FavorTop:
		if (LocalPlayerIndex == 0)
		{
			Width = Canvas->ClipX;
		}
		else
		{
			Width = Canvas->ClipX * 2;
		}
		Height = Canvas->ClipY * 2;
		return;
	case ESplitScreenType::ThreePlayer_FavorBottom:
		if (LocalPlayerIndex == 2)
		{
			Width = Canvas->ClipX;
		}
		else
		{
			Width = Canvas->ClipX * 2;
		}
		Height = Canvas->ClipY * 2;
		return;
	case ESplitScreenType::FourPlayer:
		Width = Canvas->ClipX * 2;
		Height = Canvas->ClipY * 2;
		return;
	}
}

void UGameViewportClient::CalculateSafeZoneValues( float& Horizontal, float& Vertical, UCanvas* Canvas, int32 LocalPlayerIndex, bool bUseMaxPercent )
{

	float XSafeZoneToUse = bUseMaxPercent ? TitleSafeZone.MaxPercentX : TitleSafeZone.RecommendedPercentX;
	float YSafeZoneToUse = bUseMaxPercent ? TitleSafeZone.MaxPercentY : TitleSafeZone.RecommendedPercentY;

	float ScreenWidth, ScreenHeight;
	GetPixelSizeOfScreen( ScreenWidth, ScreenHeight, Canvas, LocalPlayerIndex );
	Horizontal = (ScreenWidth * (1 - XSafeZoneToUse) / 2.0f);
	Vertical = (ScreenHeight * (1 - YSafeZoneToUse) / 2.0f);
}


bool UGameViewportClient::CalculateDeadZoneForAllSides( ULocalPlayer* LPlayer, UCanvas* Canvas, float& fTopSafeZone, float& fBottomSafeZone, float& fLeftSafeZone, float& fRightSafeZone, bool bUseMaxPercent )
{
	// save separate - if the split screen is in bottom right, then

	if ( LPlayer != NULL )
	{
		int32 LocalPlayerIndex = ConvertLocalPlayerToGamePlayerIndex( LPlayer );

		if ( LocalPlayerIndex != -1 )
		{
			// see if this player should have a safe zone for any particular zonetype
			bool bHasTopSafeZone = HasTopSafeZone( LocalPlayerIndex );
			bool bHasBottomSafeZone = HasBottomSafeZone( LocalPlayerIndex );
			bool bHasLeftSafeZone = HasLeftSafeZone( LocalPlayerIndex );
			bool bHasRightSafeZone = HasRightSafeZone( LocalPlayerIndex );

			// if they need a safezone, then calculate it and save it
			if ( bHasTopSafeZone || bHasBottomSafeZone || bHasLeftSafeZone || bHasRightSafeZone)
			{
				// calculate the safezones
				float HorizSafeZoneValue, VertSafeZoneValue;
				CalculateSafeZoneValues( HorizSafeZoneValue, VertSafeZoneValue, Canvas, LocalPlayerIndex, bUseMaxPercent );

				if (bHasTopSafeZone)
				{
					fTopSafeZone = VertSafeZoneValue;
				}
				else
				{
					fTopSafeZone = 0.f;
				}

				if (bHasBottomSafeZone)
				{
					fBottomSafeZone = VertSafeZoneValue;
				}
				else
				{
					fBottomSafeZone = 0.f;
				}

				if (bHasLeftSafeZone)
				{
					fLeftSafeZone = HorizSafeZoneValue;
				}
				else
				{
					fLeftSafeZone = 0.f;
				}

				if (bHasRightSafeZone)
				{
					fRightSafeZone = HorizSafeZoneValue;
				}
				else
				{
					fRightSafeZone = 0.f;
				}

				return true;
			}
		}
	}
	return false;
}

void UGameViewportClient::DrawTitleSafeArea( UCanvas* Canvas )
{	
	// red colored max safe area box
	Canvas->SetDrawColor(255,0,0,255);
	float X = Canvas->ClipX * (1 - TitleSafeZone.MaxPercentX) / 2.0f;
	float Y = Canvas->ClipY * (1 - TitleSafeZone.MaxPercentY) / 2.0f;
	FCanvasBoxItem BoxItem( FVector2D( X, Y ), FVector2D( Canvas->ClipX * TitleSafeZone.MaxPercentX, Canvas->ClipY * TitleSafeZone.MaxPercentY ) );
	BoxItem.SetColor( FLinearColor::Red );
	Canvas->DrawItem( BoxItem );
		
	// yellow colored recommended safe area box
	X = Canvas->ClipX * (1 - TitleSafeZone.RecommendedPercentX) / 2.0f;
	Y = Canvas->ClipY * (1 - TitleSafeZone.RecommendedPercentY) / 2.0f;
	BoxItem.SetColor( FLinearColor::Yellow );
	BoxItem.Size = FVector2D( Canvas->ClipX * TitleSafeZone.RecommendedPercentX, Canvas->ClipY * TitleSafeZone.RecommendedPercentY );
	Canvas->DrawItem( BoxItem, X, Y );
}

void UGameViewportClient::DrawTransition(UCanvas* Canvas)
{
	if (bSuppressTransitionMessage == false)
	{
		switch (GetOuterUEngine()->TransitionType)
		{
		case TT_Loading:
			DrawTransitionMessage(Canvas, NSLOCTEXT("GameViewportClient", "LoadingMessage", "LOADING").ToString());
			break;
		case TT_Saving:
			DrawTransitionMessage(Canvas, NSLOCTEXT("GameViewportClient", "SavingMessage", "SAVING").ToString());
			break;
		case TT_Connecting:
			DrawTransitionMessage(Canvas, NSLOCTEXT("GameViewportClient", "ConnectingMessage", "CONNECTING").ToString());
			break;
		case TT_Precaching:
			DrawTransitionMessage(Canvas, NSLOCTEXT("GameViewportClient", "PrecachingMessage", "PRECACHING").ToString());
			break;
		case TT_Paused:
			DrawTransitionMessage(Canvas, NSLOCTEXT("GameViewportClient", "PausedMessage", "PAUSED").ToString());
			break;
		case TT_WaitingToConnect:
			DrawTransitionMessage(Canvas, TEXT("Waiting to connect...")); // Temp - localization of the FString messages is broke atm. Loc this when its fixed.
			break;
		}
	}
}

void UGameViewportClient::DrawTransitionMessage(UCanvas* Canvas,const FString& Message)
{
	UFont* Font = GEngine->GetLargeFont();
	FCanvasTextItem TextItem( FVector2D::ZeroVector, FText::GetEmpty(), Font, FLinearColor::Blue);
	TextItem.EnableShadow( FLinearColor::Black );
	TextItem.Text = FText::FromString(Message);
	float XL, YL;
	Canvas->StrLen( Font , Message, XL, YL );
	Canvas->DrawItem( TextItem, 0.5f * (Canvas->ClipX - XL), 0.66f * Canvas->ClipY - YL * 0.5f );	
}


void UGameViewportClient::NotifyPlayerAdded( int32 PlayerIndex, ULocalPlayer* AddedPlayer )
{
	LayoutPlayers();
}

void UGameViewportClient::NotifyPlayerRemoved( int32 PlayerIndex, ULocalPlayer* RemovedPlayer )
{
	LayoutPlayers();
}


int32 UGameViewportClient::AddLocalPlayer( ULocalPlayer* NewPlayer, int32 ControllerId )
{
	int32 InsertIndex = INDEX_NONE;
	if ( NewPlayer != NULL )
	{
		// add to list
		NewPlayer->PlayerAdded(this, ControllerId);
		InsertIndex = GetOuterUEngine()->GetNumGamePlayers(this);
		GetOuterUEngine()->AddGamePlayer(this, NewPlayer);
	}
	return InsertIndex;
}

int32 UGameViewportClient::RemoveLocalPlayer( ULocalPlayer* ExistingPlayer )
{
	int32 Index = GetOuterUEngine()->GetGamePlayers(this).Find(ExistingPlayer);
	if ( Index != INDEX_NONE )
	{
		ExistingPlayer->PlayerRemoved();
		GetOuterUEngine()->RemoveGamePlayer(this, Index);
	}

	return Index;
}

void UGameViewportClient::AddViewportWidgetContent( TSharedRef<SWidget> ViewportContent, const int32 ZOrder )
{
	TSharedPtr< SOverlay > PinnedViewportOverlayWidget( ViewportOverlayWidget.Pin() );
	if( ensure( PinnedViewportOverlayWidget.IsValid() ) )
	{
		// NOTE: Returns FSimpleSlot but we're ignoring here.  Could be used for alignment though.
		PinnedViewportOverlayWidget->AddSlot( ZOrder )
			[
				ViewportContent
			];
	}
}

void UGameViewportClient::RemoveViewportWidgetContent( TSharedRef<SWidget> ViewportContent )
{
	TSharedPtr< SOverlay > PinnedViewportOverlayWidget( ViewportOverlayWidget.Pin() );
	if( PinnedViewportOverlayWidget.IsValid() )
	{
		PinnedViewportOverlayWidget->RemoveSlot( ViewportContent );
	}
}

void UGameViewportClient::RemoveAllViewportWidgets()
{
	TSharedPtr< SOverlay > PinnedViewportOverlayWidget( ViewportOverlayWidget.Pin() );
	if( PinnedViewportOverlayWidget.IsValid() )
	{
		while(PinnedViewportOverlayWidget->GetNumWidgets())
		{
			PinnedViewportOverlayWidget->RemoveSlot();
		}
	}
}

void UGameViewportClient::OnPrimaryPlayerSwitch(ULocalPlayer* OldPrimaryPlayer, ULocalPlayer* NewPrimaryPlayer) {}

void UGameViewportClient::VerifyPathRenderingComponents()
{
	const bool bShowPaths = !!EngineShowFlags.Navigation;

	// make sure nav mesh has a rendering component
	ANavigationData* NavData = GetWorld()->GetNavigationSystem() != NULL ? GetWorld()->GetNavigationSystem()->GetMainNavData(NavigationSystem::DontCreate)
		: NULL;

	if(NavData && NavData->RenderingComp == NULL)
	{
		NavData->RenderingComp = NavData->ConstructRenderingComponent();
		if (NavData->RenderingComp)
		{
			NavData->RenderingComp->SetVisibility(bShowPaths);
			NavData->RenderingComp->RegisterComponent();
		}
	}

	if(NavData == NULL)
	{
		UE_LOG(LogPlayerManagement, Warning, TEXT("No NavData found when calling UGameViewportClient::VerifyPathRenderingComponents()"));
	}
}



bool UGameViewportClient::Exec( UWorld* InWorld, const TCHAR* Cmd,FOutputDevice& Ar)
{
	if ( FParse::Command(&Cmd,TEXT("FORCEFULLSCREEN")) )
	{
		return HandleForceFullscreenCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("SHOW")) )
	{
		return HandleShowCommand( Cmd, Ar, InWorld );
	}
	else if (FParse::Command(&Cmd,TEXT("VIEWMODE")))
	{
		return HandleViewModeCommand( Cmd, Ar, InWorld );
	}
	else if (FParse::Command(&Cmd, TEXT("NEXTVIEWMODE")))
	{
		return HandleNextViewModeCommand( Cmd, Ar, InWorld );
	}
	else if (FParse::Command(&Cmd, TEXT("PREVVIEWMODE")))
	{
		return HandlePrevViewModeCommand( Cmd, Ar, InWorld );
	}
#if WITH_EDITOR
	else if( FParse::Command( &Cmd, TEXT("ShowMouseCursor") ) )
	{
		return HandleShowMouseCursorCommand( Cmd, Ar );
	}
#endif
	else if( FParse::Command(&Cmd,TEXT("PRECACHE")) )
	{
		return HandlePreCacheCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("TOGGLE_FULLSCREEN")) || FParse::Command(&Cmd,TEXT("FULLSCREEN")) )
	{
		return HandleToggleFullscreenCommand( Cmd, Ar );
	}	
	else if( FParse::Command(&Cmd,TEXT("SETRES")) )
	{
		return HandleSetResCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("HighResShot")) )
	{
		return HandleHighresScreenshotCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("HighResShotUI")) )
	{
		return HandleHighresScreenshotUICommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("SHOT")) || FParse::Command(&Cmd,TEXT("SCREENSHOT")) )
	{
		return HandleScreenshotCommand( Cmd, Ar );	
	}
	else if ( FParse::Command(&Cmd, TEXT("BUGSCREENSHOTWITHHUDINFO")) )
	{
		return HandleBugScreenshotwithHUDInfoCommand( Cmd, Ar );
	}
	else if ( FParse::Command(&Cmd,TEXT("BUGSCREENSHOT")) )
	{
		return HandleBugScreenshotCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("KILLPARTICLES")) )
	{
		return HandleKillParticlesCommand( Cmd, Ar );	
	}
	else if( FParse::Command(&Cmd,TEXT("FORCESKELLOD")) )
	{
		return HandleForceSkelLODCommand( Cmd, Ar, InWorld );
	}
	else if (FParse::Command(&Cmd, TEXT("DISPLAY")))
	{
		return HandleDisplayCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("DISPLAYALL")))
	{
		return HandleDisplayAllCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("DISPLAYALLLOCATION")))
	{
		return HandleDisplayAllLocationCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("DISPLAYALLROTATION")))
	{
		return HandleDisplayAllRotationCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("DISPLAYCLEAR")))
	{
		return HandleDisplayClearCommand( Cmd, Ar );
	}
	else if(FParse::Command(&Cmd, TEXT("TEXTUREDEFRAG")))
	{
		return HandleTextureDefragCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("TOGGLEMIPFADE")))
	{
		return HandleToggleMIPFadeCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("PAUSERENDERCLOCK")))
	{
		return HandlePauseRenderClockCommand( Cmd, Ar );
	}

	if(ProcessConsoleExec(Cmd,Ar,NULL))
	{
		return true;
	}
	else if( GEngine->Exec( InWorld, Cmd,Ar) )
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool UGameViewportClient::HandleForceFullscreenCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GForceFullscreen = !GForceFullscreen;
	return true;
}

bool UGameViewportClient::HandleShowCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
#if UE_BUILD_SHIPPING
	// don't allow show flags in net games, but on con
	if ( InWorld->GetNetMode() != NM_Standalone || (GEngine->GetWorldContextFromWorldChecked(InWorld).PendingNetGame != NULL) )
	{
		return true;
	}
	// the effects of this cannot be easily reversed, so prevent the user from playing network games without restarting to avoid potential exploits
	GDisallowNetworkTravel = true;
#endif // UE_BUILD_SHIPPING

	// First, look for skeletal mesh show commands

	bool bUpdateSkelMeshCompDebugFlags = false;
	static bool bShowSkelBones = false;
	static bool bShowPrePhysSkelBones = false;

	if(FParse::Command(&Cmd,TEXT("BONES")))
	{
		bShowSkelBones = !bShowSkelBones;
		bUpdateSkelMeshCompDebugFlags = true;
	}
	else if(FParse::Command(&Cmd,TEXT("PREPHYSBONES")))
	{
		bShowPrePhysSkelBones = !bShowPrePhysSkelBones;
		bUpdateSkelMeshCompDebugFlags = true;
	}

	// If we changed one of the skel mesh debug show flags, set it on each of the components in the World.
	if(bUpdateSkelMeshCompDebugFlags)
	{
		for (TObjectIterator<USkeletalMeshComponent> It; It; ++It)
		{
			USkeletalMeshComponent* SkelComp = *It;
			if( SkelComp->GetScene() == InWorld->Scene )
			{
				SkelComp->bDisplayBones = bShowSkelBones;
				SkelComp->bShowPrePhysBones = bShowPrePhysSkelBones;
				SkelComp->MarkRenderStateDirty();
			}
		}

		// Now we are done.
		return true;
	}

	// EngineShowFlags
	{
		int32 FlagIndex = FEngineShowFlags::FindIndexByName(Cmd);

		if(FlagIndex != -1)
		{
			bool bCanBeToggled = true;

			if(GIsEditor)
			{
				if(!FEngineShowFlags::CanBeToggledInEditor(Cmd))
				{
					bCanBeToggled = false;
				}
			}

			bool bIsACollisionFlag = FEngineShowFlags::IsNameThere(Cmd, TEXT("Collision"));

			if(bCanBeToggled)
				{
					bool bOldState = EngineShowFlags.GetSingleFlag(FlagIndex);

					if(bIsACollisionFlag && !bOldState)
					{
						// we only want one active at a time
						EngineShowFlags.Collision = 0;
					}

					EngineShowFlags.SetSingleFlag(FlagIndex, !bOldState);

					if(FEngineShowFlags::IsNameThere(Cmd, TEXT("Navigation,Cover")))
					{
						VerifyPathRenderingComponents();
					}
					
					if(FEngineShowFlags::IsNameThere(Cmd, TEXT("Volumes")))
					{
						if (AllowDebugViewmodes())
						{
							// Iterate over all brushes
							for( TObjectIterator<UBrushComponent> It; It; ++It )
							{
								UBrushComponent* BrushComponent = *It;
								AVolume* Owner = Cast<AVolume>( BrushComponent->GetOwner() );

								// Only bother with volume brushes that belong to the world's scene
								if( Owner && BrushComponent->GetScene() == GetWorld()->Scene )
								{
									// We're expecting this to be in the game at this point
									check( Owner->GetWorld()->IsGameWorld() );

									// Toggle visibility of this volume
									if( BrushComponent->IsVisible() )
									{
										Owner->bHidden = true;
										BrushComponent->SetVisibility( false );
									}
									else
									{
										Owner->bHidden = false;
										BrushComponent->SetVisibility( true );
									}
								}
							}
						}
						else
						{
							Ar.Logf(TEXT("Debug viewmodes not allowed on consoles by default.  See AllowDebugViewmodes()."));
						}
					}
				}

			if(bIsACollisionFlag)
			{
				// special case: for the Engine.Collision flag, we need to un-hide any primitive components that collide so their collision geometry gets rendered

				/** Contains the previous state of a primitive before turning on collision visibility */
				struct CollVisibilityState
				{
					bool bHiddenInGame;
					bool bVisible;

					CollVisibilityState(bool InHidden, bool InVisible) :
					bHiddenInGame(InHidden),
						bVisible(InVisible)
					{
					}
				};

				static TMap< TWeakObjectPtr<UPrimitiveComponent>, CollVisibilityState> Mapping;

				{
					// Restore state to any object touched above
					for (TMap< TWeakObjectPtr<UPrimitiveComponent>, CollVisibilityState>::TIterator It(Mapping); It; ++It)
					{
						TWeakObjectPtr<UPrimitiveComponent>& PrimitiveComponent = It.Key();
						if (PrimitiveComponent.IsValid())
						{
							const CollVisibilityState& VisState = It.Value();
							PrimitiveComponent->SetHiddenInGame(VisState.bHiddenInGame);
							PrimitiveComponent->SetVisibility(VisState.bVisible);
						}
					}
					Mapping.Empty();
				}

				if (EngineShowFlags.Collision)
					{
						for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
						{
							UPrimitiveComponent* PrimitiveComponent = *It;
							if( !PrimitiveComponent->IsVisible() && PrimitiveComponent->IsCollisionEnabled() && PrimitiveComponent->GetScene() == GetWorld()->Scene )
							{
								check( PrimitiveComponent->GetOwner() && PrimitiveComponent->GetOwner()->GetWorld() && PrimitiveComponent->GetOwner()->GetWorld()->IsGameWorld() );
								
								// Save state before modifying the collision visibility
								Mapping.Add(PrimitiveComponent, CollVisibilityState(PrimitiveComponent->bHiddenInGame, PrimitiveComponent->bVisible));
								PrimitiveComponent->SetHiddenInGame(false);
								PrimitiveComponent->SetVisibility(true);
							}
						}
					}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				if( EngineShowFlags.Collision )
				{
					for (FLocalPlayerIterator It((UEngine*)GetOuter(), InWorld); It; ++It)
					{
						APlayerController* PC = It->PlayerController;
						if( PC != NULL && PC->GetPawn() != NULL )
						{
							PC->ClientMessage( FString::Printf(TEXT("!!!! Player Pawn %s Collision Info !!!!"), *PC->GetPawn()->GetName()) );
							if (PC->GetPawn()->GetMovementBase())
							{
								PC->ClientMessage( FString::Printf(TEXT("Base %s"), *PC->GetPawn()->GetMovementBase()->GetName() ));
							}
							TArray<AActor*> Touching;
							PC->GetPawn()->GetOverlappingActors(Touching);
							for( int32 i = 0; i < Touching.Num(); i++ )
							{
								PC->ClientMessage( FString::Printf(TEXT("Touching %d: %s"), i, *Touching[i]->GetName() ));
							}
						}
					}
				}
#endif
			}


			return true;
		}
	}

	// create a sorted list of showflags
	TSet<FString> LinesToSort;
	{
		struct FIterSink
		{
			FIterSink(TSet<FString>& InLinesToSort, const FEngineShowFlags InEngineShowFlags) : LinesToSort(InLinesToSort), EngineShowFlags(InEngineShowFlags)
			{
			}

			bool OnEngineShowFlag(uint32 InIndex, const FString& InName)
			{
				FString Value = FString::Printf(TEXT("%s=%d"), *InName, EngineShowFlags.GetSingleFlag(InIndex) ? 1 : 0);
				LinesToSort.Add(Value);
				return true;
			}

			TSet<FString>& LinesToSort;
			const FEngineShowFlags EngineShowFlags;
		};

		FIterSink Sink(LinesToSort, EngineShowFlags);

		FEngineShowFlags::IterateAllFlags(Sink);
	}	

	LinesToSort.Sort( TLess<FString>() );

	for(TSet<FString>::TConstIterator It(LinesToSort); It; ++It)
	{
		const FString Value = *It;

		Ar.Logf(TEXT("%s"), *Value);
	}

	return true;
}

bool UGameViewportClient::HandleViewModeCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
#if !UE_BUILD_DEBUG
	// If there isn't a cheat manager, exit out
	bool bCheatsEnabled = false;
	for (FLocalPlayerIterator It((UEngine*)GetOuter(), InWorld); It; ++It)
	{
		if (It->PlayerController != NULL && It->PlayerController->CheatManager != NULL)
		{
			bCheatsEnabled = true;
			break;
		}
	}
	if (!bCheatsEnabled)
	{
		return true;
	}
#endif
	FString ViewModeName = FParse::Token(Cmd, 0);

	if(!ViewModeName.IsEmpty())
	{
		uint32 i = 0;
		for(; i < VMI_Max; ++i)
		{
			if(ViewModeName == GetViewModeName((EViewModeIndex)i))
			{
				ViewModeIndex = i;
				Ar.Logf(TEXT("Set new viewmode: %s"), GetViewModeName((EViewModeIndex)ViewModeIndex));
				break;
			}
		}
		if(i == VMI_Max)
		{
			Ar.Logf(TEXT("Error: view mode not recognized: %s"), *ViewModeName);
		}
	}
	else
	{
		Ar.Logf(TEXT("Current view mode: %s"), GetViewModeName((EViewModeIndex)ViewModeIndex));

		FString ViewModes;
		for(uint32 i = 0; i < VMI_Max; ++i)
		{
			if(i != 0)
			{
				ViewModes += TEXT(", ");
			}
			ViewModes += GetViewModeName((EViewModeIndex)i);
		}
		Ar.Logf(TEXT("Available view modes: %s"), *ViewModes);
	}

	if (ViewModeIndex == VMI_StationaryLightOverlap)
	{
		Ar.Logf(TEXT("This view mode is currently not supported in game."));
		ViewModeIndex = VMI_Lit;
	}

	if (FPlatformProperties::SupportsWindowedMode() == false)
	{
		if(ViewModeIndex == VMI_Unlit
			|| ViewModeIndex == VMI_ShaderComplexity
			|| ViewModeIndex == VMI_StationaryLightOverlap
			|| ViewModeIndex == VMI_Lit_DetailLighting
			|| ViewModeIndex == VMI_ReflectionOverride)
		{
			Ar.Logf(TEXT("This view mode is currently not supported on consoles."));
			ViewModeIndex = VMI_Lit;
		}
	}
	if (ViewModeIndex != VMI_Lit && !AllowDebugViewmodes())
	{
		Ar.Logf(TEXT("Debug viewmodes not allowed on consoles by default.  See AllowDebugViewmodes()."));
		ViewModeIndex = VMI_Lit;
	}

	ApplyViewMode((EViewModeIndex)ViewModeIndex, true, EngineShowFlags);

	return true;
}

bool UGameViewportClient::HandleNextViewModeCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
#if !UE_BUILD_DEBUG
	// If there isn't a cheat manager, exit out
	bool bCheatsEnabled = false;
	for (FLocalPlayerIterator It((UEngine*)GetOuter(), InWorld); It; ++It)
	{
		if (It->PlayerController != NULL && It->PlayerController->CheatManager != NULL)
		{
			bCheatsEnabled = true;
			break;
		}
	}
	if (!bCheatsEnabled)
	{
		return true;
	}
#endif
	ViewModeIndex = ViewModeIndex + 1;

	// wrap around
	if(ViewModeIndex == VMI_Max)
	{
		ViewModeIndex = 0;
	}

	Ar.Logf(TEXT("New view mode: %s"), GetViewModeName((EViewModeIndex)ViewModeIndex));
	ApplyViewMode((EViewModeIndex)ViewModeIndex, true, EngineShowFlags);
	return true;
}

bool UGameViewportClient::HandlePrevViewModeCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
#if !UE_BUILD_DEBUG
	// If there isn't a cheat manager, exit out
	bool bCheatsEnabled = false;
	for (FLocalPlayerIterator It((UEngine*)GetOuter(), InWorld); It; ++It)
	{
		if (It->PlayerController != NULL && It->PlayerController->CheatManager != NULL)
		{
			bCheatsEnabled = true;
			break;
		}
	}
	if (!bCheatsEnabled)
	{
		return true;
	}
#endif
	ViewModeIndex = ViewModeIndex - 1;

	// wrap around
	if(ViewModeIndex < 0)
	{
		ViewModeIndex = VMI_Max - 1;
	}

	Ar.Logf(TEXT("New view mode: %s"), GetViewModeName((EViewModeIndex)ViewModeIndex));
	ApplyViewMode((EViewModeIndex)ViewModeIndex, true, EngineShowFlags);
	return true;
}

#if WITH_EDITOR
bool UGameViewportClient::HandleShowMouseCursorCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FSlateApplication::Get().ClearKeyboardFocus( EKeyboardFocusCause::SetDirectly );
	FSlateApplication::Get().ResetToDefaultInputSettings();
	return true;
}
#endif // WITH_EDITOR

bool UGameViewportClient::HandlePreCacheCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	Precache();
	return true;
}

bool UGameViewportClient::SetDisplayConfiguration(const FIntPoint* Dimensions, EWindowMode::Type WindowMode)
{
	if (Viewport == NULL || ViewportFrame == NULL)
	{
		return true;
	}

	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
	
	if (GameEngine)
	{
		UGameUserSettings* UserSettings = GameEngine->GetGameUserSettings();

		UserSettings->SetFullscreenMode(WindowMode);

		if (Dimensions)
		{
			UserSettings->SetScreenResolution(*Dimensions);
		}

		UserSettings->ApplySettings();
	}
	else
	{
		int32 NewX = GSystemResolution.ResX;
		int32 NewY = GSystemResolution.ResY;

		if (Dimensions)
		{
			NewX = Dimensions->X;
			NewY = Dimensions->Y;
		}
	
		FSystemResolution::RequestResolutionChange(NewX, NewY, WindowMode);
	}

	return true;
}

bool UGameViewportClient::HandleToggleFullscreenCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	return SetDisplayConfiguration(NULL, Viewport->IsFullscreen() ? EWindowMode::Windowed : EWindowMode::Fullscreen);
}

bool UGameViewportClient::HandleSetResCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if(Viewport && ViewportFrame)
	{
		int32 X=FCString::Atoi(Cmd);
		const TCHAR* CmdTemp = FCString::Strchr(Cmd,'x') ? FCString::Strchr(Cmd,'x')+1 : FCString::Strchr(Cmd,'X') ? FCString::Strchr(Cmd,'X')+1 : TEXT("");
		int32 Y=FCString::Atoi(CmdTemp);
		Cmd = CmdTemp;
		EWindowMode::Type WindowMode = Viewport->IsFullscreen() ? EWindowMode::Fullscreen : EWindowMode::Windowed;
		if(FCString::Strchr(Cmd,'w') || FCString::Strchr(Cmd,'W'))
		{
			if(FCString::Strchr(Cmd, 'f') || FCString::Strchr(Cmd, 'F'))
			{
				WindowMode = EWindowMode::WindowedFullscreen;
			}
			else
			{
				WindowMode = EWindowMode::Windowed;
			}
			
		}
		else if(FCString::Strchr(Cmd,'f') || FCString::Strchr(Cmd,'F'))
		{
			WindowMode = EWindowMode::Fullscreen;
		}
		if( X && Y )
		{
			FIntPoint Dims = FIntPoint(X,Y);
			return SetDisplayConfiguration(&Dims, WindowMode);
		}
	}
	return true;
}

bool UGameViewportClient::HandleHighresScreenshotCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if(Viewport)
	{
		if (GetHighResScreenshotConfig().ParseConsoleCommand(Cmd, Ar))
		{
			Viewport->TakeHighResScreenShot();
		}
	}
	return true;
}

bool UGameViewportClient::HandleHighresScreenshotUICommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Open the highres screenshot UI. When the capture region editing works properly, we can pass CaptureRegionWidget through
	// HighResScreenshotDialog = SHighResScreenshotDialog::OpenDialog(GetWorld(), Viewport, NULL /*CaptureRegionWidget*/);
	// Disabled until mouse specification UI can be used correctly
	return true;
}


bool UGameViewportClient::HandleScreenshotCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if(Viewport)
	{
		const bool bShowUI = FParse::Command(&Cmd, TEXT("SHOWUI"));
		FScreenshotRequest::RequestScreenshot( bShowUI );

		GScreenMessagesRestoreState = GAreScreenMessagesEnabled;
		GAreScreenMessagesEnabled = false;
	}
	return true;
}

bool UGameViewportClient::HandleBugScreenshotwithHUDInfoCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	return RequestBugScreenShot(Cmd, true);
}

bool UGameViewportClient::HandleBugScreenshotCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	return RequestBugScreenShot(Cmd, false);
}

bool UGameViewportClient::HandleKillParticlesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Don't kill in the Editor to avoid potential content clobbering.
	if( !GIsEditor )
	{
		extern bool GIsAllowingParticles;
		// Deactivate system and kill existing particles.
		for( TObjectIterator<UParticleSystemComponent> It; It; ++It )
		{
			UParticleSystemComponent* ParticleSystemComponent = *It;
			ParticleSystemComponent->DeactivateSystem();
			ParticleSystemComponent->KillParticlesForced();
		}
		// No longer initialize particles from here on out.
		GIsAllowingParticles = false;
	}
	return true;
}

bool UGameViewportClient::HandleForceSkelLODCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	int32 ForceLod = 0;
	if(FParse::Value(Cmd,TEXT("LOD="),ForceLod))
	{
		ForceLod++;
	}

	for (TObjectIterator<USkeletalMeshComponent> It; It; ++It)
	{
		USkeletalMeshComponent* SkelComp = *It;
		if( SkelComp->GetScene() == InWorld->Scene && !SkelComp->IsTemplate())
		{
			SkelComp->ForcedLodModel = ForceLod;
		}
	}
	return true;
}

bool UGameViewportClient::HandleDisplayCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	TCHAR ObjectName[256];
	TCHAR PropStr[256];
	if ( FParse::Token(Cmd, ObjectName, ARRAY_COUNT(ObjectName), true) &&
		FParse::Token(Cmd, PropStr, ARRAY_COUNT(PropStr), true) )
	{
		UObject* Obj = FindObject<UObject>(ANY_PACKAGE, ObjectName);
		if (Obj != NULL)
		{
			FName PropertyName(PropStr, FNAME_Find);
			if (PropertyName != NAME_None && FindField<UProperty>(Obj->GetClass(), PropertyName) != NULL)
			{
				FDebugDisplayProperty& NewProp = DebugProperties[DebugProperties.AddZeroed()];
				NewProp.Obj = Obj;
				NewProp.PropertyName = PropertyName;
			}
			else
			{
				Ar.Logf(TEXT("Property '%s' not found on object '%s'"), PropStr, *Obj->GetName());
			}
		}
		else
		{
			Ar.Logf(TEXT("Object not found"));
		}
	}

	return true;
}

bool UGameViewportClient::HandleDisplayAllCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	TCHAR ClassName[256];
	TCHAR PropStr[256];
	if (FParse::Token(Cmd, ClassName, ARRAY_COUNT(ClassName), true))
	{
		bool bValidClassToken = true;
		UClass* WithinClass = NULL;
		{
			FString ClassStr(ClassName);
			int32 DotIndex = ClassStr.Find(TEXT("."));
			if (DotIndex != INDEX_NONE)
			{
				// first part is within class
				WithinClass = FindObject<UClass>(ANY_PACKAGE, *ClassStr.Left(DotIndex));
				if (WithinClass == NULL)
				{
					Ar.Logf(TEXT("Within class not found"));
					bValidClassToken = false;
				}
				else
				{
					FCString::Strncpy(ClassName, *ClassStr.Right(ClassStr.Len() - DotIndex - 1), 256);
					bValidClassToken = FCString::Strlen(ClassName) > 0;
				}
			}
		}
		if (bValidClassToken)
		{
			FParse::Token(Cmd, PropStr, ARRAY_COUNT(PropStr), true);
			UClass* Cls = FindObject<UClass>(ANY_PACKAGE, ClassName);
			if (Cls != NULL)
			{
				FName PropertyName(PropStr, FNAME_Find);
				UProperty* Prop = PropertyName != NAME_None ? FindField<UProperty>(Cls, PropertyName) : NULL;
				{
					// add all un-GCable things immediately as that list is static
					// so then we only have to iterate over dynamic things each frame
					for (TObjectIterator<UObject> It; It; ++It)
					{
						if (!GUObjectArray.IsDisregardForGC(*It))
						{
							break;
						}
						else if (It->IsA(Cls) && !It->IsTemplate() && (WithinClass == NULL || (It->GetOuter() != NULL && It->GetOuter()->GetClass()->IsChildOf(WithinClass))))
						{
							FDebugDisplayProperty& NewProp = DebugProperties[DebugProperties.AddZeroed()];
							NewProp.Obj = *It;
							NewProp.PropertyName = PropertyName;
							if (!Prop)
							{
								NewProp.bSpecialProperty = true;
							}
						}
					}
					FDebugDisplayProperty& NewProp = DebugProperties[DebugProperties.AddZeroed()];
					NewProp.Obj = Cls;
					NewProp.WithinClass = WithinClass;
					NewProp.PropertyName = PropertyName;
					if (!Prop)
					{
						NewProp.bSpecialProperty = true;
					}
				}
			}
			else
			{
				Ar.Logf(TEXT("Object not found"));
			}
		}
	}

	return true;
}

bool UGameViewportClient::HandleDisplayAllLocationCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	TCHAR ClassName[256];
	if (FParse::Token(Cmd, ClassName, ARRAY_COUNT(ClassName), true))
	{
		UClass* Cls = FindObject<UClass>(ANY_PACKAGE, ClassName);
		if (Cls != NULL)
		{
			// add all un-GCable things immediately as that list is static
			// so then we only have to iterate over dynamic things each frame
			for (TObjectIterator<UObject> It(true); It; ++It)
			{
				if (!GUObjectArray.IsDisregardForGC(*It))
				{
					break;
				}
				else if (It->IsA(Cls))
				{
					FDebugDisplayProperty& NewProp = DebugProperties[DebugProperties.AddZeroed()];
					NewProp.Obj = *It;
					NewProp.PropertyName = NAME_Location;
					NewProp.bSpecialProperty = true;
				}
			}
			FDebugDisplayProperty& NewProp = DebugProperties[DebugProperties.AddZeroed()];
			NewProp.Obj = Cls;
			NewProp.PropertyName = NAME_Location;
			NewProp.bSpecialProperty = true;
		}
		else
		{
			Ar.Logf(TEXT("Object not found"));
		}
	}

	return true;
}

bool UGameViewportClient::HandleDisplayAllRotationCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	TCHAR ClassName[256];
	if (FParse::Token(Cmd, ClassName, ARRAY_COUNT(ClassName), true))
	{
		UClass* Cls = FindObject<UClass>(ANY_PACKAGE, ClassName);
		if (Cls != NULL)
		{
			// add all un-GCable things immediately as that list is static
			// so then we only have to iterate over dynamic things each frame
			for (TObjectIterator<UObject> It(true); It; ++It)
			{
				if (!GUObjectArray.IsDisregardForGC(*It))
				{
					break;
				}
				else if (It->IsA(Cls))
				{
					FDebugDisplayProperty& NewProp = DebugProperties[DebugProperties.AddZeroed()];
					NewProp.Obj = *It;
					NewProp.PropertyName = NAME_Rotation;
					NewProp.bSpecialProperty = true;
				}
			}
			FDebugDisplayProperty& NewProp = DebugProperties[DebugProperties.AddZeroed()];
			NewProp.Obj = Cls;
			NewProp.PropertyName = NAME_Rotation;
			NewProp.bSpecialProperty = true;
		}
		else
		{
			Ar.Logf(TEXT("Object not found"));
		}
	}

	return true;
}

bool UGameViewportClient::HandleDisplayClearCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	DebugProperties.Empty();

	return true;
}

bool UGameViewportClient::HandleTextureDefragCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	extern void appDefragmentTexturePool();
	appDefragmentTexturePool();
	return true;
}

bool UGameViewportClient::HandleToggleMIPFadeCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GEnableMipLevelFading = (GEnableMipLevelFading >= 0.0f) ? -1.0f : 1.0f;
	Ar.Logf(TEXT("Mip-fading is now: %s"), (GEnableMipLevelFading >= 0.0f) ? TEXT("ENABLED") : TEXT("DISABLED"));
	return true;
}

bool UGameViewportClient::HandlePauseRenderClockCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GPauseRenderingRealtimeClock = !GPauseRenderingRealtimeClock;
	Ar.Logf(TEXT("The global realtime rendering clock is now: %s"), GPauseRenderingRealtimeClock ? TEXT("PAUSED") : TEXT("RUNNING"));
	return true;
}


bool UGameViewportClient::RequestBugScreenShot(const TCHAR* Cmd, bool bDisplayHUDInfo)
{
	// find where these are really defined
	const static int32 MaxFilenameLen = 100;

	if( Viewport != NULL )
	{
		TCHAR File[MAX_SPRINTF] = TEXT("");
		for( int32 TestBitmapIndex = 0; TestBitmapIndex < 9; ++TestBitmapIndex )
		{ 
			const FString DescPlusExtension = FString::Printf( TEXT("%s%i.bmp"), Cmd, TestBitmapIndex );
			const FString SSFilename = CreateProfileFilename( DescPlusExtension, false );

			const FString OutputDir = FPaths::BugItDir() + FString::Printf( TEXT("%s"), Cmd) + TEXT("/");

			//UE_LOG(LogPlayerManagement, Warning, TEXT( "BugIt Looking: %s" ), *(OutputDir + SSFilename) );
			FCString::Sprintf( File, TEXT("%s"), *(OutputDir + SSFilename) );
			if( IFileManager::Get().FileSize(File) == INDEX_NONE )
			{
				if (bDisplayHUDInfo)
				{
					for( FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator )
					{
						APlayerController* PlayerController = *Iterator;
						if (PlayerController && PlayerController->GetHUD() )
						{
							PlayerController->GetHUD()->HandleBugScreenShot();
						}
					}
				}

				GScreenshotBitmapIndex = TestBitmapIndex; // this is safe as the UnMisc.cpp ScreenShot code will test each number before writing a file

				const bool bShowUI = true;
				FScreenshotRequest::RequestScreenshot( File, bShowUI );
				break;
			}
		}
	}

	return true;
}

void UGameViewportClient::HandleViewportStatCheckEnabled(const TCHAR* InName, bool& bOutCurrentEnabled, bool& bOutOthersEnabled)
{
	// Check to see which viewports have this enabled (current, non-current)
	const bool bEnabled = IsStatEnabled(InName);
	if (GStatProcessingViewportClient == this && GEngine->GameViewport == this)
	{
		bOutCurrentEnabled = bEnabled;
	}
	else
	{
		bOutOthersEnabled |= bEnabled;
	}
}

void UGameViewportClient::HandleViewportStatEnabled(const TCHAR* InName)
{
	// Just enable this on the active viewport
	if (GStatProcessingViewportClient == this && GEngine->GameViewport == this)
	{
		SetStatEnabled(InName, true);
	}
}

void UGameViewportClient::HandleViewportStatDisabled(const TCHAR* InName)
{
	// Just disable this on the active viewport
	if (GStatProcessingViewportClient == this && GEngine->GameViewport == this)
	{
		SetStatEnabled(InName, false);
	}
}

void UGameViewportClient::HandleViewportStatDisableAll(const bool bInAnyViewport)
{
	// Disable all on either all or the current viewport (depending on the flag)
	if (bInAnyViewport || (GStatProcessingViewportClient == this && GEngine->GameViewport == this))
	{
		SetStatEnabled(NULL, false, true);
	}
}

