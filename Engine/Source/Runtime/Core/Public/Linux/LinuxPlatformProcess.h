// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	LinuxPlatformProcess.h: Linux platform Process functions
==============================================================================================*/

#pragma once

/** Wrapper around Linux pid_t. */
struct FProcHandle : public TProcHandle<pid_t, -1>
{
	typedef TProcHandle<pid_t, -1> Parent;

	/** Default constructor. */
	FORCEINLINE FProcHandle()
		:	TProcHandle()
		,	bIsRunning( false )
		,	bHasBeenWaitedFor( false )
		,	ReturnCode( -1 )
	{
	}

	/** Initialization constructor. */
	FORCEINLINE explicit FProcHandle( HandleType Other )
		:	TProcHandle( Other )
		,	bIsRunning( true )	// assume it is
		,	bHasBeenWaitedFor( false )
		,	ReturnCode( -1 )
	{
	}

	/** Copy constructor. */
	FORCEINLINE FProcHandle( const FProcHandle & Other )
		:	TProcHandle( Other )
		,	bIsRunning( Other.bIsRunning )	// assume it is
		,	bHasBeenWaitedFor( Other.bHasBeenWaitedFor )
		,	ReturnCode( Other.ReturnCode )
	{
	}

	/** Assignment operator. */
	FORCEINLINE FProcHandle& operator=( const FProcHandle& Other )
	{
		Parent::operator=( Other );

		if( this != &Other )
		{
			bIsRunning = Other.bIsRunning;
			bHasBeenWaitedFor = Other.bHasBeenWaitedFor;
			ReturnCode = Other.ReturnCode;
		}

		return *this;
	}


protected:	// the below is not a public API!

	friend struct FLinuxPlatformProcess;

	/** 
	 * Returns whether this process is running.
	 *
	 * @return true if running
	 */
	bool	IsRunning();

	/** 
	 * Returns child's return code (only valid to call if not running)
	 *
	 * @param ReturnCode set to return code if not NULL (use the value only if method returned true)
	 *
	 * @return return whether we have the return code (we don't if it crashed)
	 */
	bool	GetReturnCode(int32* ReturnCodePtr);

	/** 
	 * Waits for the process to end.
	 * Has a side effect (stores child's return code).
	 */
	void	Wait();

	// -------------------------

	/** Whether the process has finished or not (cached) */
	bool	bIsRunning;

	/** Whether the process's return code has been collected */
	bool	bHasBeenWaitedFor;

	/** Return code of the process (if negative, means that process did not finish gracefully but was killed/crashed*/
	int32	ReturnCode;
};

/** Wrapper around Linux file descriptors */
struct FPipeHandle
{
	FPipeHandle(int Fd)
		:	PipeDesc(Fd)
	{
	}

	~FPipeHandle();

	/**
	 * Reads until EOF.
	 */
	FString Read();

	/**
	 * Returns raw file handle.
	 */
	int GetHandle() const
	{
		return PipeDesc;
	}

protected:

	int	PipeDesc;
};

/**
 * Linux implementation of the Process OS functions
 */
struct CORE_API FLinuxPlatformProcess : public FGenericPlatformProcess
{
	static void* GetDllHandle( const TCHAR* Filename );
	static void FreeDllHandle( void* DllHandle );
	static void* GetDllExport( void* DllHandle, const TCHAR* ProcName );
	static const TCHAR* ComputerName();
	static const TCHAR* BaseDir();
	static FString GetApplicationName( uint32 ProcessId );
	static bool SetProcessLimits(EProcessResource::Type Resource, uint64 Limit);
	static const TCHAR* ExecutableName(bool bRemoveExtension = true);
	static void ClosePipe( void* ReadPipe, void* WritePipe );
	static bool CreatePipe( void*& ReadPipe, void*& WritePipe );
	static FString ReadPipe( void* ReadPipe );
	static class FRunnableThread* CreateRunnableThread();
	static void LaunchURL(const TCHAR* URL, const TCHAR* Parms, FString* Error);
	static FProcHandle CreateProc(const TCHAR* URL, const TCHAR* Parms, bool bLaunchDetached, bool bLaunchHidden, bool bLaunchReallyHidden, uint32* OutProcessID, int32 PriorityModifier, const TCHAR* OptionalWorkingDirectory, void* PipeWrite);
	static bool IsProcRunning( FProcHandle & ProcessHandle );
	static void WaitForProc( FProcHandle & ProcessHandle );
	static void TerminateProc( FProcHandle & ProcessHandle, bool KillTree = false );
	static bool GetProcReturnCode( FProcHandle & ProcHandle, int32* ReturnCode );
	static bool Daemonize();
};

typedef FLinuxPlatformProcess FPlatformProcess;
