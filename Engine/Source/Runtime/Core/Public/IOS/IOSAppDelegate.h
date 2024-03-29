// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#import <UIKit/UIKit.h>
#import <AVFoundation/AVAudioSession.h>
#import <GameKit/GKGameCenterViewController.h>

@class EAGLView;
@class IOSViewController;
@class SlateOpenGLESViewController;
@class IOSAppDelegate;

DECLARE_LOG_CATEGORY_EXTERN(LogIOSAudioSession, Log, All);

namespace FAppEntry
{
	void PlatformInit();
	void PreInit(IOSAppDelegate* AppDelegate, UIApplication* Application);
	void Init();
	void Tick();
    void SuspendTick();
	void Shutdown();
    void Suspend();
    void Resume();
}

@interface IOSAppDelegate : UIResponder <UIApplicationDelegate,
#if !UE_BUILD_SHIPPING
	UIGestureRecognizerDelegate,
#endif
	GKGameCenterControllerDelegate,
UITextFieldDelegate, AVAudioSessionDelegate>

/** Window object */
@property (strong, retain, nonatomic) UIWindow *Window;

/** Main GL View */
@property (retain) EAGLView *GLView;

/** The controller to handle rotation of the view */
@property (retain) IOSViewController* IOSController;

/** The view controlled by the auto-rotating controller */
@property (retain) UIView* RootView;

/** The controller to handle rotation of the view */
@property (retain) SlateOpenGLESViewController* SlateController;

/** The value of the alert response (atomically set since main thread and game thread use it */
@property (assign) int AlertResponse;

/** Version of the OS we are running on (NOT compiled with) */
@property (readonly) float OSVersion;

@property bool bDeviceInPortraitMode;

#if !UE_BUILD_SHIPPING
	/** Properties for managing the console */
	@property (nonatomic, retain) UIAlertView*		ConsoleAlert;
	@property (nonatomic, retain) NSMutableArray*	ConsoleHistoryValues;
	@property (nonatomic, assign) int				ConsoleHistoryValuesIndex;
#endif

/** True if the engine has been initialized */
@property (readonly) bool bEngineInit;

/** Delays game initialization slightly in case we have a URL launch to handle */
@property (retain) NSTimer* CommandLineParseTimer;
@property (atomic) bool bCommandLineReady;

/** True if we need to reset the idle timer */
@property (readonly) bool bResetIdleTimer;
/**
 * @return the single app delegate object
 */
+ (IOSAppDelegate*)GetDelegate;


-(void) ParseCommandLineOverrides;

/** TRUE if the device is playing background music and we want to allow that */
@property (assign) bool bUsingBackgroundMusic;

- (void)InitializeAudioSession;
- (void)ToggleAudioSession:(bool)bActive;
- (bool)IsBackgroundAudioPlaying;

@property (atomic) bool bAudioActive;

@property (atomic) bool bIsSuspended;
@property (atomic) bool bHasSuspended;
@property (atomic) bool bHasStarted;
- (void)ToggleSuspend:(bool)bSuspend;

static void interruptionListener(void* ClientData, UInt32 Interruption);

@end

void InstallSignalHandlers();
