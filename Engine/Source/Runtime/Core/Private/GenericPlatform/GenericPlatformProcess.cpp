// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GenericPlatformProcess.cpp: Generic implementations of Process platform functions
=============================================================================*/

#include "CorePrivate.h"

#if PLATFORM_HAS_BSD_TIME 
#include <unistd.h> // for usleep
#include <sched.h>  // for sched_yield
#endif

void* FGenericPlatformProcess::GetDllHandle( const TCHAR* Filename )
{
	UE_LOG(LogHAL, Fatal, TEXT("FPlatformProcess::GetDllHandle not implemented on this platform"));
	return NULL;
}

void FGenericPlatformProcess::FreeDllHandle( void* DllHandle )
{
	UE_LOG(LogHAL, Fatal, TEXT("FPlatformProcess::FreeDllHandle not implemented on this platform"));
}

void* FGenericPlatformProcess::GetDllExport( void* DllHandle, const TCHAR* ProcName )
{
	UE_LOG(LogHAL, Fatal, TEXT("FPlatformProcess::GetDllExport not implemented on this platform"));
	return NULL;
}

FBinaryFileVersion FGenericPlatformProcess::GetBinaryFileVersion( const TCHAR* Filename )
{
	UE_LOG(LogHAL, Fatal, TEXT("FPlatformProcess::GetBinaryFileVersion not implemented on this platform"));
	return FBinaryFileVersion(0, 0, 0, 0);
}

uint32 FGenericPlatformProcess::GetCurrentProcessId()
{
	// for single-process platforms (consoles, etc), just use 0
	return 0;
}

void FGenericPlatformProcess::SetThreadAffinityMask( uint64 AffinityMask )
{
	// Not implemented cross-platform. Each platform may or may not choose to implement this.
}

const TCHAR* FGenericPlatformProcess::UserDir()
{
	// default to the root directory
	return FPlatformMisc::RootDir();
}

const TCHAR *FGenericPlatformProcess::UserSettingsDir()
{
	// default to the root directory
	return FPlatformMisc::RootDir();
}

const TCHAR* FGenericPlatformProcess::ApplicationSettingsDir()
{
	// default to the root directory
	return FPlatformMisc::RootDir();
}

const TCHAR* FGenericPlatformProcess::ComputerName()
{
	return TEXT("GenericComputer");
}

const TCHAR* FGenericPlatformProcess::UserName(bool bOnlyAlphaNumeric/* = true*/)
{
	return TEXT("GenericUser");
}

static FString Generic_ShaderDir;

const TCHAR* FGenericPlatformProcess::ShaderDir()
{
	if (Generic_ShaderDir.Len() == 0)
	{
		Generic_ShaderDir = FPaths::Combine(*(FPaths::EngineDir()), TEXT("Shaders"));
	}
	return *Generic_ShaderDir;
}

void FGenericPlatformProcess::SetShaderDir(const TCHAR*Where)
{
	if ((Where != NULL) && (FCString::Strlen(Where) != 0))
	{
		Generic_ShaderDir = Where;
	}
	else
	{
		Generic_ShaderDir = TEXT("");
	}
}

/**
 *	Get the shader working directory
 */
const FString FGenericPlatformProcess::ShaderWorkingDir()
{
	return (FPaths::GameIntermediateDir() / TEXT("Shaders/WorkingDirectory/"));
}

/**
 *	Clean the shader working directory
 */
void FGenericPlatformProcess::CleanShaderWorkingDir()
{
	// Path to the working directory where files are written for multi-threaded compilation
	FString ShaderWorkingDirectory = ShaderWorkingDir();

	IFileManager::Get().DeleteDirectory(*ShaderWorkingDirectory, false, true);
}

const TCHAR* FGenericPlatformProcess::ExecutableName(bool bRemoveExtension)
{
	UE_LOG(LogHAL, Fatal, TEXT("FGenericPlatformProcess::ExecutableName not implemented on this platform"));
	return NULL;
}

FString FGenericPlatformProcess::GenerateApplicationPath( const FString& AppName, EBuildConfigurations::Type BuildConfiguration)
{
	UE_LOG(LogHAL, Fatal, TEXT("FGenericPlatformProcess::GenerateApplicationPath not implemented on this platform"));
	return FString();
}

const TCHAR* FGenericPlatformProcess::GetModuleExtension()
{
	return TEXT("dll");
}

const TCHAR* FGenericPlatformProcess::GetBinariesSubdirectory()
{
	return TEXT("");
}

const FString FGenericPlatformProcess::GetModulesDirectory()
{
	return FPaths::Combine(*FPaths::EngineDir(), TEXT("Binaries"), FPlatformProcess::GetBinariesSubdirectory());
}

void FGenericPlatformProcess::LaunchURL( const TCHAR* URL, const TCHAR* Parms, FString* Error )
{
	UE_LOG(LogHAL, Fatal, TEXT("FGenericPlatformProcess::LaunchURL not implemented on this platform"));
}

FProcHandle FGenericPlatformProcess::CreateProc( const TCHAR* URL, const TCHAR* Parms, bool bLaunchDetached, bool bLaunchHidden, bool bLaunchReallyHidden, uint32* OutProcessID, int32 PriorityModifier, const TCHAR* OptionalWorkingDirectory, void* PipeWrite )
{
	UE_LOG(LogHAL, Fatal, TEXT("FGenericPlatformProcess::CreateProc not implemented on this platform"));
	return FProcHandle();
}

bool FGenericPlatformProcess::IsProcRunning( FProcHandle & ProcessHandle )
{
	UE_LOG(LogHAL, Fatal, TEXT("FGenericPlatformProcess::IsProcRunning not implemented on this platform"));
	return false;
}

void FGenericPlatformProcess::WaitForProc( FProcHandle & ProcessHandle )
{
	UE_LOG(LogHAL, Fatal, TEXT("FGenericPlatformProcess::WaitForProc not implemented on this platform"));
}

void FGenericPlatformProcess::TerminateProc( FProcHandle & ProcessHandle, bool KillTree )
{
	UE_LOG(LogHAL, Fatal, TEXT("FGenericPlatformProcess::TerminateProc not implemented on this platform"));
}


bool FGenericPlatformProcess::GetProcReturnCode( FProcHandle & ProcHandle, int32* ReturnCode )
{
	UE_LOG(LogHAL, Fatal, TEXT("FGenericPlatformProcess::GetProcReturnCode not implemented on this platform"));
	return false;
}

bool FGenericPlatformProcess::IsApplicationRunning( uint32 ProcessId )
{
	UE_LOG(LogHAL, Fatal, TEXT("FGenericPlatformProcess::IsApplicationRunning not implemented on this platform"));
	return false;
}

bool FGenericPlatformProcess::IsApplicationRunning( const TCHAR* ProcName )
{
	UE_LOG(LogHAL, Fatal, TEXT("FGenericPlatformProcess::IsApplicationRunning not implemented on this platform"));
	return false;
}

FString FGenericPlatformProcess::GetApplicationName( uint32 ProcessId )
{
	UE_LOG(LogHAL, Fatal, TEXT("FGenericPlatformProcess::GetApplicationName not implemented on this platform"));
	return FString(TEXT(""));
}

bool FGenericPlatformProcess::IsThisApplicationForeground()
{
	UE_LOG(LogHAL, Fatal, TEXT("FGenericPlatformProcess::IsThisApplicationForeground not implemented on this platform"));
	return false;
}

bool FGenericPlatformProcess::ExecProcess( const TCHAR* URL, const TCHAR* Params, int32* OutReturnCode, FString* OutStdOut, FString* OutStdErr )
{
	UE_LOG(LogHAL, Fatal, TEXT("FGenericPlatformProcess::ExecProcess not implemented on this platform"));
	return false;
}

bool FGenericPlatformProcess::ExecElevatedProcess(const TCHAR* URL, const TCHAR* Params, int32* OutReturnCode)
{
	return FPlatformProcess::ExecProcess(URL, Params, OutReturnCode, NULL, NULL);
}

void FGenericPlatformProcess::LaunchFileInDefaultExternalApplication( const TCHAR* FileName, const TCHAR* Parms, ELaunchVerb::Type Verb )
{
	UE_LOG(LogHAL, Fatal, TEXT("FGenericPlatformProcess::LaunchFileInDefaultExternalApplication not implemented on this platform"));
}

void FGenericPlatformProcess::ExploreFolder( const TCHAR* FilePath )
{
	UE_LOG(LogHAL, Fatal, TEXT("FGenericPlatformProcess::ExploreFolder not implemented on this platform"));
}



#if PLATFORM_HAS_BSD_TIME
DECLARE_CYCLE_STAT(TEXT("CPU Stall - Sleep"),STAT_Sleep,STATGROUP_CPUStalls);

void FGenericPlatformProcess::Sleep( float Seconds )
{
	SCOPE_CYCLE_COUNTER(STAT_Sleep);
	FThreadIdleStats::FScopeIdle Scope;
	const int32 usec = FPlatformMath::TruncToInt(Seconds * 1000000.0f);
	if (usec > 0)
	{
		usleep(usec);
	}
	else
	{
		sched_yield();
	}
}

void FGenericPlatformProcess::SleepInfinite()
{
	// stop this thread forever
	pause();
}

#endif // PLATFORM_HAS_BSD_TIME 

#if PLATFORM_USE_PTHREADS

#include "PThreadEvent.h"

DECLARE_CYCLE_STAT(TEXT("CPU Stall - Wait For Event"),STAT_EventWait,STATGROUP_CPUStalls);

bool FPThreadEvent::Wait(uint32 WaitTime)
{
	SCOPE_CYCLE_COUNTER(STAT_EventWait);
	FThreadIdleStats::FScopeIdle Scope;

	check(bInitialized);

	struct timeval StartTime;

	// We need to know the start time if we're going to do a timed wait.
	if ( (WaitTime > 0) && (WaitTime != ((uint32)-1)) )  // not polling and not infinite wait.
	{
		gettimeofday(&StartTime, NULL);
	}

	LockEventMutex();

	bool bRetVal = false;

	// loop in case we fall through the Condition signal but someone else claims the event.
	do
	{
		// See what state the event is in...we may not have to wait at all...

		// One thread should be released. We saw it first, so we'll take it.
		if (Triggered == TRIGGERED_ONE)
		{
			Triggered = TRIGGERED_NONE;  // dibs!
			bRetVal = true;
		}

		// manual reset that is still signaled. Every thread goes.
		else if (Triggered == TRIGGERED_ALL)
		{
			bRetVal = true;
		}

		// No event signalled yet.
		else if (WaitTime != 0)  // not just polling, wait on the condition variable.
		{
			WaitingThreads++;
			if (WaitTime == ((uint32)-1)) // infinite wait?
			{
				int rc = pthread_cond_wait(&Condition, &Mutex);  // unlocks Mutex while blocking...
				check(rc == 0);
			}
			else  // timed wait.
			{
				struct timespec TimeOut;
				const uint32 ms = (StartTime.tv_usec / 1000) + WaitTime;
				TimeOut.tv_sec = StartTime.tv_sec + (ms / 1000);
				TimeOut.tv_nsec = (ms % 1000) * 1000000;  // remainder of milliseconds converted to nanoseconds.
				int rc = pthread_cond_timedwait(&Condition, &Mutex, &TimeOut);    // unlocks Mutex while blocking...
				check((rc == 0) || (rc == ETIMEDOUT));

				// Update WaitTime and StartTime in case we have to go again...
				struct timeval Now, Difference;
				gettimeofday(&Now, NULL);
				SubtractTimevals(&Now, &StartTime, &Difference);
				const int32 DifferenceMS = ((Difference.tv_sec * 1000) + (Difference.tv_usec / 1000));
				WaitTime = ((DifferenceMS >= WaitTime) ? 0 : (WaitTime - DifferenceMS));
				StartTime = Now;
			}
			WaitingThreads--;
			check(WaitingThreads >= 0);
		}

	} while ((!bRetVal) && (WaitTime != 0));

	UnlockEventMutex();
	return bRetVal;
}

#endif

FEvent* FGenericPlatformProcess::CreateSynchEvent(bool bIsManualReset)
{
#if PLATFORM_USE_PTHREADS
	FEvent* Event = NULL;
	if (FPlatformProcess::SupportsMultithreading())
	{
		// Allocate the new object
		Event = new FPThreadEvent();
	}
	else
	{
		// Fake event.
		Event = new FSingleThreadEvent();
	}
	// If the internal create fails, delete the instance and return NULL
	if (!Event->Create(bIsManualReset))
	{
		delete Event;
		Event = NULL;
	}
	return Event;
#else
	UE_LOG(LogHAL, Fatal, TEXT("FGenericPlatformProcess::CreateSynchEvent not implemented on this platform"));
	return NULL;
#endif
}




#if PLATFORM_USE_PTHREADS
	#include "../HAL/PThreadRunnableThread.h"
#endif

FRunnableThread* FGenericPlatformProcess::CreateRunnableThread()
{
#if PLATFORM_USE_PTHREADS
	return new FRunnableThreadPThread();
#else
	UE_LOG(LogHAL, Fatal, TEXT("FGenericPlatformProcess::CreateThread not implemented on this platform"));
	return NULL;
#endif
}

void FGenericPlatformProcess::ClosePipe( void* ReadPipe, void* WritePipe )
{
	UE_LOG(LogHAL, Fatal, TEXT("FGenericPlatformProcess::ClosePipe not implemented on this platform"));
}

bool FGenericPlatformProcess::CreatePipe( void*& ReadPipe, void*& WritePipe )
{
	UE_LOG(LogHAL, Fatal, TEXT("FGenericPlatformProcess::CreatePipe not implemented on this platform"));
	return false;
}

FString FGenericPlatformProcess::ReadPipe( void* ReadPipe )
{
	UE_LOG(LogHAL, Fatal, TEXT("FGenericPlatformProcess::ReadPipe not implemented on this platform"));
	return FString();
}

bool FGenericPlatformProcess::SupportsMultithreading()
{
	static bool bSupportsMultithreading = !FParse::Param(FCommandLine::Get(), TEXT("nothreading"));
	return bSupportsMultithreading;
}

void FBinaryFileVersion::ToString(FString& OutString) const
{
	OutString = FString::Printf(TEXT("%d %d %d %d"), A, B, C, D);
}

FGenericPlatformProcess::FSemaphore::FSemaphore(const FString & InName)
{
	FCString::Strcpy(Name, sizeof(Name)-1, *InName);
}

FGenericPlatformProcess::FSemaphore * FGenericPlatformProcess::NewInterprocessSynchObject(const FString & Name, bool bCreate, uint32 MaxLocks)
{
	UE_LOG(LogHAL, Fatal, TEXT("FGenericPlatformProcess::NewInterprocessSynchObject not implemented on this platform"));
	return NULL;
}

bool FGenericPlatformProcess::DeleteInterprocessSynchObject(FSemaphore * Object)
{
	UE_LOG(LogHAL, Fatal, TEXT("FGenericPlatformProcess::DeleteInterprocessSynchObject not implemented on this platform"));
	return false;
}

bool FGenericPlatformProcess::Daemonize()
{
	UE_LOG(LogHAL, Fatal, TEXT("FGenericPlatformProcess::Daemonize not implemented on this platform"));
	return false;
}
