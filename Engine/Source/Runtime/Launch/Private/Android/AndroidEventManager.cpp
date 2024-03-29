// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "LaunchPrivatePCH.h"
#include "AndroidEventManager.h"
#include "AndroidApplication.h"
#include "AudioDevice.h"
#include <android/native_window.h> 
#include <android/native_window_jni.h> 


DEFINE_LOG_CATEGORY(LogAndroidEvents);


FAppEventManager* FAppEventManager::sInstance = NULL;

FAppEventManager* FAppEventManager::GetInstance()
{
	if(!sInstance)
	{
		sInstance = new FAppEventManager();
	}

	return sInstance;
}


void FAppEventManager::Tick()
{
	while (!Queue.IsEmpty())
	{
		FAppEventData Event = DequeueAppEvent();

		switch (Event.State)
		{
		case APP_EVENT_STATE_WINDOW_CREATED:
			check(FirstInitialized);
			bCreateWindow = true;
			PendingWindow = (ANativeWindow*)Event.Data;
			break;
		case APP_EVENT_STATE_WINDOW_RESIZED:
			bWindowChanged = true;
			break;
		case APP_EVENT_STATE_WINDOW_CHANGED:
			bWindowChanged = true;
			break;
		case APP_EVENT_STATE_SAVE_STATE:
			bSaveState = true; //todo android: handle save state.
			break;
		case APP_EVENT_STATE_WINDOW_DESTROYED:
			//AndroidEGL::GetInstance()->UnBind()
			FAndroidAppEntry::DestroyWindow();
			FPlatformMisc::SetHardwareWindow(NULL);
			bHaveWindow = false;
			break;
		case APP_EVENT_STATE_ON_START:
			//doing nothing here
			break;
		case APP_EVENT_STATE_ON_DESTROY:
			GIsRequestingExit = true; //destroy immediately. Game will shutdown.
			break;
		case APP_EVENT_STATE_ON_STOP:
			bHaveGame = false;
			break;
		case APP_EVENT_STATE_ON_PAUSE:
			bHaveGame = false;
			break;
		case APP_EVENT_STATE_ON_RESUME:
			bHaveGame = true;
			break;

		// window focus events that follow their own  heirarchy, and might or might not respect App main events heirarchy

		case APP_EVENT_STATE_WINDOW_GAINED_FOCUS: 
			bWindowInFocus = true;
			break;
		case APP_EVENT_STATE_WINDOW_LOST_FOCUS:
			bWindowInFocus = false;
			break;

		default:
			UE_LOG(LogAndroidEvents, Display, TEXT("Application Event : %u  not handled. "), Event.State);
		}

		if (bCreateWindow)
		{
			// wait until activity is in focus.
			if (bWindowInFocus) 
			{
				ExecWindowCreated();
				bCreateWindow = false;
				bHaveWindow = true;
			}
		}

		if (bWindowChanged)
		{
			// wait until window is valid and created. 
			if(FPlatformMisc::GetHardwareWindow() != NULL)
			{
				if(GEngine && GEngine->GameViewport && GEngine->GameViewport->ViewportFrame)
				{
					ExecWindowChanged();
					bWindowChanged = false;
					bHaveWindow = true;
				}
			}
		}

		if (!bRunning && bHaveWindow && bHaveGame)
		{
			ResumeRendering();
			ResumeAudio();
			bRunning = true;
		}
		else if (bRunning && (!bHaveWindow || !bHaveGame))
		{
			PauseRendering();
			PauseAudio();
			bRunning = false;
		}
	}
}

FAppEventManager::FAppEventManager():
	FirstInitialized(false)
	,bCreateWindow(false)
	,bWindowChanged(false)
	,bWindowInFocus(false)
	,bSaveState(false)
	,bAudioPaused(false)
	,PendingWindow(NULL)
	,bHaveWindow(false)
	,bHaveGame(false)
	,bRunning(false)
{
	pthread_mutex_init(&MainMutex, NULL);
	pthread_mutex_init(&QueueMutex, NULL);
}

void FAppEventManager::HandleWindowCreated(void* InWindow)
{
	int rc = pthread_mutex_lock(&MainMutex);
	check(rc == 0);
	bool AlreadyInited = FirstInitialized;
	rc = pthread_mutex_unlock(&MainMutex);
	check(rc == 0);

	if(AlreadyInited)
	{
		EnqueueAppEvent(APP_EVENT_STATE_WINDOW_CREATED, InWindow );
	}
	else
	{
		//This cannot wait until first tick. 

		rc = pthread_mutex_lock(&MainMutex);
		check(rc == 0);
		
		check(FPlatformMisc::GetHardwareWindow() == NULL);
		FPlatformMisc::SetHardwareWindow(InWindow);
		FirstInitialized = true;

		rc = pthread_mutex_unlock(&MainMutex);
		check(rc == 0);

		EnqueueAppEvent(APP_EVENT_STATE_WINDOW_CREATED, InWindow );
	}
}

void FAppEventManager::PauseRendering()
{
	if(GUseThreadedRendering )
	{
		if (GIsThreadedRendering)
		{
			StopRenderingThread(); 
		}
	}
	else
	{
		RHIReleaseThreadOwnership();
	}
}

void FAppEventManager::ResumeRendering()
{
	if( GUseThreadedRendering )
	{
		if (!GIsThreadedRendering)
		{
			StartRenderingThread();
		}
	}
	else
	{
		RHIAcquireThreadOwnership();
	}
}

void FAppEventManager::ExecWindowCreated()
{
	UE_LOG(LogAndroidEvents, Display, TEXT("ExecWindowCreated"));
	check(PendingWindow)

	FPlatformMisc::SetHardwareWindow(PendingWindow);
	PendingWindow = NULL;
	
	FAndroidAppEntry::ReInitWindow();
}

void FAppEventManager::ExecWindowChanged()
{
	FPlatformRect ScreenRect = FAndroidWindow::GetScreenRect();
	UE_LOG(LogAndroidEvents, Display, TEXT("ExecWindowChanged : width: %u, height:  %u"), ScreenRect.Right, ScreenRect.Bottom);

	if(GEngine && GEngine->GameViewport && GEngine->GameViewport->ViewportFrame)
	{
		GEngine->GameViewport->ViewportFrame->ResizeFrame(ScreenRect.Right, ScreenRect.Bottom, EWindowMode::Fullscreen);
	}
}

void FAppEventManager::PauseAudio()
{
	bAudioPaused = true;

	if (GEngine->GetAudioDevice())
	{
		GEngine->GetAudioDevice()->Suspend(false);
	}
}

void FAppEventManager::ResumeAudio()
{
	bAudioPaused = false;

	if (GEngine->GetAudioDevice())
	{
		GEngine->GetAudioDevice()->Suspend(true);
	}
}

void FAppEventManager::EnqueueAppEvent(EAppEventState InState, void* InData)
{
	FAppEventData Event;
	Event.State = InState;
	Event.Data = InData;

	int rc = pthread_mutex_lock(&QueueMutex);
	check(rc == 0);
	Queue.Enqueue(Event);
	 rc = pthread_mutex_unlock(&QueueMutex);
	check(rc == 0);

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("LogAndroidEvents: EnqueueAppEvent : %u, %u"), InState, (uintptr_t)InData);
}

FAppEventData FAppEventManager::DequeueAppEvent()
{
	int rc = pthread_mutex_lock(&QueueMutex);
	check(rc == 0);

	FAppEventData OutData;
	Queue.Dequeue( OutData );

	rc = pthread_mutex_unlock(&QueueMutex);
	check(rc == 0);

	UE_LOG(LogAndroidEvents, Display, TEXT("DequeueAppEvent : %u, %u"), OutData.State, (uintptr_t)OutData.Data)

	return OutData;
}

bool FAppEventManager::IsGamePaused()
{
	return !bRunning;
}
