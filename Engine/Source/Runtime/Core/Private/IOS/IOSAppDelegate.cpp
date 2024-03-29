// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "CorePrivate.h"
#include "IOSAppDelegate.h"
#include "IOSCommandLineHelper.h"
#include "ExceptionHandling.h"
#include "ModuleBoilerplate.h"
#include "CallbackDevice.h"

#include <AudioToolbox/AudioToolbox.h>
#include <AVFoundation/AVAudioSession.h>

// this is the size of the game thread stack, it must be a multiple of 4k
#define GAME_THREAD_STACK_SIZE 1024 * 1024 

DEFINE_LOG_CATEGORY(LogIOSAudioSession);
DEFINE_LOG_CATEGORY_STATIC(LogEngine, Log, All);

static void SignalHandler(int32 Signal, struct __siginfo* Info, void* Context)
{
	static int32 bHasEntered = 0;
	if (FPlatformAtomics::InterlockedCompareExchange(&bHasEntered, 1, 0) == 0)
	{
		const SIZE_T StackTraceSize = 65535;
		ANSICHAR* StackTrace = (ANSICHAR*)FMemory::Malloc(StackTraceSize);
		StackTrace[0] = 0;
		
		// Walk the stack and dump it to the allocated memory.
		FPlatformStackWalk::StackWalkAndDump(StackTrace, StackTraceSize, 0, (ucontext_t*)Context);
		UE_LOG(LogEngine, Error, TEXT("%s"), ANSI_TO_TCHAR(StackTrace));
		FMemory::Free(StackTrace);
		
		GError->HandleError();
		FPlatformMisc::RequestExit(true);
	}
}

void InstallSignalHandlers()
{
	struct sigaction Action;
	FMemory::Memzero(&Action, sizeof(struct sigaction));
	
	Action.sa_sigaction = SignalHandler;
	sigemptyset(&Action.sa_mask);
	Action.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
	
	sigaction(SIGQUIT, &Action, NULL);
	sigaction(SIGILL, &Action, NULL);
	sigaction(SIGEMT, &Action, NULL);
	sigaction(SIGFPE, &Action, NULL);
	sigaction(SIGBUS, &Action, NULL);
	sigaction(SIGSEGV, &Action, NULL);
	sigaction(SIGSYS, &Action, NULL);
}

@implementation IOSAppDelegate

#if !UE_BUILD_SHIPPING
	@synthesize ConsoleAlert;
	@synthesize ConsoleHistoryValues;
	@synthesize ConsoleHistoryValuesIndex;
#endif

@synthesize AlertResponse;
@synthesize bDeviceInPortraitMode;
@synthesize bEngineInit;
@synthesize OSVersion;
@synthesize bResetIdleTimer;

@synthesize Window;
@synthesize GLView;
@synthesize IOSController;
@synthesize SlateController;

-(void) ParseCommandLineOverrides
{
	//// Check for device type override
	//FString IOSDeviceName;
	//if ( Parse( appCmdLine(), TEXT("-IOSDevice="), IOSDeviceName) )
	//{
	//	for ( int32 DeviceTypeIndex = 0; DeviceTypeIndex < IOS_Unknown; ++DeviceTypeIndex )
	//	{
	//		if ( IOSDeviceName == IPhoneGetDeviceTypeString( (EIOSDevice) DeviceTypeIndex) )
	//		{
	//			GOverrideIOSDevice = (EIOSDevice) DeviceTypeIndex;
	//		}
	//	}
	//}

	//// Check for iOS version override
	//FLOAT IOSVersion = 0.0f;
	//if ( Parse( appCmdLine(), TEXT("-IOSVersion="), IOSVersion) )
	//{
	//	GOverrideIOSVersion = IOSVersion;
	//}

	// check to see if we are using the network file system, if so, disable the idle timer
	bResetIdleTimer = false;
	FString HostIP;
	if (FParse::Value(FCommandLine::Get(), TEXT("-FileHostIP="), HostIP))
	{
		bResetIdleTimer = true;
		[[UIApplication sharedApplication] setIdleTimerDisabled: YES];
	}
}

-(void)MainAppThread:(NSDictionary*)launchOptions
{
    self.bHasStarted = true;
	GIsGuarded = false;
	GStartTime = FPlatformTime::Seconds();

	// make sure this thread has an auto release pool setup 
	NSAutoreleasePool* AutoreleasePool = [[NSAutoreleasePool alloc] init];

	while(!self.bCommandLineReady)
	{
		FPlatformProcess::Sleep(.01f);
	}


	// Look for overrides specified on the command-line
	[self ParseCommandLineOverrides];

	FAppEntry::Init();

	bEngineInit = true;

	while( !GIsRequestingExit ) 
	{
        if (self.bIsSuspended)
        {
            FAppEntry::SuspendTick();
            
            self.bHasSuspended = true;
        }
        else
        {
            FAppEntry::Tick();
        
            // free any autoreleased objects every once in awhile to keep memory use down (strings, splash screens, etc)
            if (((GFrameCounter) & 31) == 0)
            {
                [AutoreleasePool release];
                AutoreleasePool = [[NSAutoreleasePool alloc] init];
            }
        }

        // drain the async task queue from the game thread
        [FIOSAsyncTask ProcessAsyncTasks];
	}

	if (bResetIdleTimer)
	{
		[[UIApplication sharedApplication] setIdleTimerDisabled: NO];
		bResetIdleTimer = false;
	}

	[AutoreleasePool release];
	FAppEntry::Shutdown();
    
    self.bHasStarted = false;
}

- (void)NoUrlCommandLine
{
	//Since it is non-repeating, the timer should kill itself.
	self.bCommandLineReady = true;
}

- (void)InitializeAudioSession
{
	// get notified about interruptions
	[[AVAudioSession sharedInstance] setDelegate:self];
	
	self.bUsingBackgroundMusic = [self IsBackgroundAudioPlaying];
	if (!self.bUsingBackgroundMusic)
	{
		NSError* ActiveError = nil;
		[[AVAudioSession sharedInstance] setActive:YES error:&ActiveError];
		if (ActiveError)
		{
			UE_LOG(LogIOSAudioSession, Error, TEXT("Failed to set audio session as active!"));
		}
		ActiveError = nil;
		
		[[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategorySoloAmbient error:&ActiveError];
		if (ActiveError)
		{
			UE_LOG(LogIOSAudioSession, Error, TEXT("Failed to set audio session category to AVAudioSessionCategorySoloAmbient!"));
		}
		ActiveError = nil;
	}
	else
	{
		// Allow iPod music to continue playing in the background
		NSError* ActiveError = nil;
		[[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryAmbient error:&ActiveError];
		if (ActiveError)
		{
			UE_LOG(LogIOSAudioSession, Error, TEXT("Failed to set audio session category to AVAudioSessionCategoryAmbient!"));
		}
		ActiveError = nil;
 	}
    self.bAudioActive = true;

	/* TODO::JTM - Jan 16, 2013 06:22PM - Music player support */
}

- (void)ToggleAudioSession:(bool)bActive
{
	if (bActive)
	{
        if (!self.bAudioActive)
        {
		bool bWasUsingBackgroundMusic = self.bUsingBackgroundMusic;
		self.bUsingBackgroundMusic = [self IsBackgroundAudioPlaying];

		if (bWasUsingBackgroundMusic != self.bUsingBackgroundMusic)
		{
			if (!self.bUsingBackgroundMusic)
			{
				NSError* ActiveError = nil;
				[[AVAudioSession sharedInstance] setActive:YES error:&ActiveError];
				if (ActiveError)
				{
					UE_LOG(LogIOSAudioSession, Error, TEXT("Failed to set audio session as active!"));
				}
				ActiveError = nil;

				[[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategorySoloAmbient error:&ActiveError];
				if (ActiveError)
				{
					UE_LOG(LogIOSAudioSession, Error, TEXT("Failed to set audio session category to AVAudioSessionCategorySoloAmbient!"));
				}
				ActiveError = nil;

				/* TODO::JTM - Jan 16, 2013 05:05PM - Music player support */
			}
			else
			{
				/* TODO::JTM - Jan 16, 2013 05:05PM - Music player support */

				// Allow iPod music to continue playing in the background
				NSError* ActiveError = nil;
				[[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryAmbient error:&ActiveError];
				if (ActiveError)
				{
					UE_LOG(LogIOSAudioSession, Error, TEXT("Failed to set audio session category to AVAudioSessionCategoryAmbient!"));
				}
				ActiveError = nil;
			}
		}
		else if (!self.bUsingBackgroundMusic)
		{
			NSError* ActiveError = nil;
			[[AVAudioSession sharedInstance] setActive:YES error:&ActiveError];
			if (ActiveError)
			{
				UE_LOG(LogIOSAudioSession, Error, TEXT("Failed to set audio session as active!"));
			}
			ActiveError = nil;
			
			[[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategorySoloAmbient error:&ActiveError];
			if (ActiveError)
			{
				UE_LOG(LogIOSAudioSession, Error, TEXT("Failed to set audio session category to AVAudioSessionCategorySoloAmbient!"));
			}
			ActiveError = nil;
		}
        }
	}
	else if (self.bAudioActive && !self.bUsingBackgroundMusic)
	{
		NSError* ActiveError = nil;
		[[AVAudioSession sharedInstance] setActive:NO error:&ActiveError];
		if (ActiveError)
		{
			UE_LOG(LogIOSAudioSession, Error, TEXT("Failed to set audio session as inactive!"));
		}
		ActiveError = nil;
        
		// Necessary to prevent audio from getting killing when setup for background iPod audio playback
		[[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryAmbient error:&ActiveError];
		if (ActiveError)
		{
			UE_LOG(LogIOSAudioSession, Error, TEXT("Failed to set audio session category to AVAudioSessionCategoryAmbient!"));
		}
		ActiveError = nil;
 	}
    self.bAudioActive = bActive;
}

- (bool)IsBackgroundAudioPlaying
{
	AVAudioSession* Session = [AVAudioSession sharedInstance];
	return Session.otherAudioPlaying;
}

/**
 * @return the single app delegate object
 */
+ (IOSAppDelegate*)GetDelegate
{
	return (IOSAppDelegate*)[UIApplication sharedApplication].delegate;
}

- (void)ToggleSuspend:(bool)bSuspend
{
    self.bHasSuspended = !bSuspend;
    self.bIsSuspended = bSuspend;
    
    while(!self.bHasSuspended)
    {
        FPlatformProcess::Sleep(0.05f);
    }
}

- (BOOL)application:(UIApplication *)application willFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
	self.bDeviceInPortraitMode = false;
	bEngineInit = false;

	return YES;
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions
{
	// use the status bar orientation to properly determine landscape vs portrait
	self.bDeviceInPortraitMode = UIInterfaceOrientationIsPortrait([[UIApplication sharedApplication] statusBarOrientation]);
	printf("========= This app is in %s mode\n", self.bDeviceInPortraitMode ? "PORTRAIT" : "LANDSCAPE");

		// check OS version to make sure we have the API
	OSVersion = [[[UIDevice currentDevice] systemVersion] floatValue];
	if (!FPlatformMisc::IsDebuggerPresent() || GAlwaysReportCrash)
	{
		InstallSignalHandlers();
	}

	// create the main landscape window object
	CGRect MainFrame = [[UIScreen mainScreen] bounds];
	self.Window = [[UIWindow alloc] initWithFrame:MainFrame];
	self.Window.screen = [UIScreen mainScreen];

	//Make this the primary window, and show it.
	[self.Window makeKeyAndVisible];

	FAppEntry::PreInit(self, application);

	// create a new thread (the pointer will be retained forever)
	NSThread* GameThread = [[NSThread alloc] initWithTarget:self selector:@selector(MainAppThread:) object:launchOptions];
	[GameThread setStackSize:GAME_THREAD_STACK_SIZE];
	[GameThread start];

	self.CommandLineParseTimer = [NSTimer scheduledTimerWithTimeInterval:0.01f target:self selector:@selector(NoUrlCommandLine) userInfo:nil repeats:NO];
#if !UE_BUILD_SHIPPING
	self.ConsoleHistoryValues = [[NSMutableArray alloc] init];
	self.ConsoleHistoryValuesIndex = -1;
#endif

	[self InitializeAudioSession];
	return YES;
}

- (BOOL)application:(UIApplication *)application handleOpenURL:(NSURL *)url
{
#if !NO_LOGGING
	NSLog(@"%s", "IOSAppDelegate handleOpenURL\n");
#endif

	NSString* EncdodedURLString = [url absoluteString];
	NSString* URLString = [EncdodedURLString stringByReplacingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
	FString CommandLineParameters(URLString);

	// Strip the "URL" part of the URL before treating this like args. It comes in looking like so:
	// "MyGame://arg1 arg2 arg3 ..."
	// So, we're going to make it look like:
	// "arg1 arg2 arg3 ..."
	int32 URLTerminator = CommandLineParameters.Find( TEXT("://"));
	if ( URLTerminator > -1 )
	{
		CommandLineParameters = CommandLineParameters.RightChop(URLTerminator + 3);
	}

	FIOSCommandLineHelper::InitCommandArgs(CommandLineParameters);
	self.bCommandLineReady = true;
	[self.CommandLineParseTimer invalidate];
	self.CommandLineParseTimer = nil;
	
	return YES;
}

- (void)beginInterruption
{
    FAppEntry::Suspend();

	[self ToggleAudioSession:false];
 }

- (void)endInterruption
{
	[self ToggleAudioSession:true];

    FAppEntry::Resume();
}

- (void)applicationWillResignActive:(UIApplication *)application
{
	/*
	 Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
	 Use this method to pause ongoing tasks, disable timers, and throttle down OpenGL ES frame rates. Games should use this method to pause the game.
	 */

	FCoreDelegates::ApplicationWillDeactivateDelegate.Broadcast();
	[self ToggleAudioSession:false];
    [self ToggleSuspend:true];
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
	/*
	 Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later. 
	 If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
	 */
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
	/*
	 Called as part of the transition from the background to the inactive state; here you can undo many of the changes made on entering the background.
	 */
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
	/*
	 Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
	 */

    [self ToggleSuspend:false];
	[self ToggleAudioSession:true];
	FCoreDelegates::ApplicationHasReactivatedDelegate.Broadcast();
}

- (void)applicationWillTerminate:(UIApplication *)application
{
	/*
	 Called when the application is about to terminate.
	 Save data if appropriate.
	 See also applicationDidEnterBackground:.
	 */
	FCoreDelegates::ApplicationWillTerminateDelegate.Broadcast();
    
    // note that we are shutting down
    GIsRequestingExit = true;
    
    // wait for the game thread to shut down
    while (self.bHasStarted == true)
    {
        usleep(3);
    }
}

- (void)applicationDidReceiveMemoryWarning:(UIApplication *)application
{
	/*
	Tells the delegate when the application receives a memory warning from the system
	*/
	FPlatformMisc::HandleLowMemoryWarning();
}

/**
 * Shows the given Game Center supplied controller on the screen
 *
 * @param Controller The Controller object to animate onto the screen
 */
-(void)ShowController:(UIViewController*)Controller
{
	// slide it onto the screen
	[[IOSAppDelegate GetDelegate].IOSController presentViewController : Controller animated : YES completion : nil];
	
	// stop drawing the 3D world for faster UI speed
	//FViewport::SetGameRenderingEnabled(false);
}

/**
 * Hides the given Game Center supplied controller from the screen, optionally controller
 * animation (sliding off)
 *
 * @param Controller The Controller object to animate off the screen
 * @param bShouldAnimate YES to slide down, NO to hide immediately
 */
-(void)HideController:(UIViewController*)Controller Animated : (BOOL)bShouldAnimate
{
	// slide it off
	[Controller dismissViewControllerAnimated : bShouldAnimate completion : nil];

	// stop drawing the 3D world for faster UI speed
	//FViewport::SetGameRenderingEnabled(true);
}

/**
 * Hides the given Game Center supplied controller from the screen
 *
 * @param Controller The Controller object to animate off the screen
 */
-(void)HideController:(UIViewController*)Controller
{
	// call the other version with default animation of YES
	[self HideController : Controller Animated : YES];
}

-(void)gameCenterViewControllerDidFinish:(GKGameCenterViewController*)GameCenterDisplay
{
	// close the view 
	[self HideController : GameCenterDisplay];
}

/**
 * Show the leaderboard interface (call from iOS main thread)
 */
-(void)ShowLeaderboard:(NSString*)Category
{
	// create the leaderboard display object 
	GKGameCenterViewController* GameCenterDisplay = [[[GKGameCenterViewController alloc] init] autorelease];
	GameCenterDisplay.viewState = GKGameCenterViewControllerStateLeaderboards;
	GameCenterDisplay.leaderboardCategory = Category;
	GameCenterDisplay.gameCenterDelegate = self;

	// show it 
	[self ShowController : GameCenterDisplay];
}

/**
 * Show the achievements interface (call from iOS main thread)
 */
-(void)ShowAchievements
{
	// create the leaderboard display object 
	GKGameCenterViewController* GameCenterDisplay = [[[GKGameCenterViewController alloc] init] autorelease];
	GameCenterDisplay.viewState = GKGameCenterViewControllerStateAchievements;
	GameCenterDisplay.gameCenterDelegate = self;

	// show it 
	[self ShowController : GameCenterDisplay];
}

/**
 * Show the leaderboard interface (call from game thread)
 */
CORE_API bool IOSShowLeaderboardUI(const FString& CategoryName)
{
	// route the function to iOS thread, passing the category string along as the object
	NSString* CategoryToShow = [NSString stringWithFString:CategoryName];
	[[IOSAppDelegate GetDelegate] performSelectorOnMainThread:@selector(ShowLeaderboard:) withObject:CategoryToShow waitUntilDone : NO];

	return true;
}

/**
* Show the achievements interface (call from game thread)
*/
CORE_API bool IOSShowAchievementsUI()
{
	// route the function to iOS thread
	[[IOSAppDelegate GetDelegate] performSelectorOnMainThread:@selector(ShowAchievements) withObject:nil waitUntilDone : NO];

	return true;
}

@end
