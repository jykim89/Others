// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include <pthread.h>

#if PLATFORM_LINUX
#define PTHREAD_NULL -1
#else
#define PTHREAD_NULL NULL
#endif

/**
 * This is the base interface for all runnable thread classes. It specifies the
 * methods used in managing its life cycle.
 */
class FRunnableThreadPThread : public FRunnableThread
{

protected:

	/**
	 * The thread handle for the thread
	 */
	pthread_t Thread;

	/**
	 * The runnable object to execute on this thread
	 */
	FRunnable* Runnable;

	/** 
	 * Sync event to make sure that Init() has been completed before allowing the main thread to continue
	 */
	FEvent* ThreadInitSyncEvent;

	/** 
	 * Sync event to make sure that CreateInteral() has been completed before allowing the thread to be auto-deleted
	 */
	FEvent* ThreadCreatedSyncEvent;

	/** 
	 * Flag used when the thread is waiting for the caller to finish setting it up before it can delete itself.
	 */
	FThreadSafeCounter WantsToDeleteSelf;

	/**
	 * Whether we should delete ourselves on thread exit
	 */
	bool bShouldDeleteSelf;

	/**
	 * Whether we should delete the runnable on thread exit
	 */
	bool bShouldDeleteRunnable;

	/**
	 * The priority to run the thread at
	 */
	EThreadPriority ThreadPriority;

	/**
	* ID set during thread creation
	*/
	uint32 ThreadID;

	/**
	 * The name of this thread
	 */
	FString ThreadName;

	/**
	 * If true, the thread is ready to be joined.
	 */
	volatile bool ThreadIsRunning;

	typedef void *(*PthreadEntryPoint)(void *arg);

	/**
	 * Converts an EThreadPriority to a value that can be used in pthread_setschedparam. Virtual
	 * so that platforms can override priority values
	 */
	virtual int32 TranslateThreadPriority(EThreadPriority Priority)
	{
		// these are some default priorities
		switch (Priority)
		{
			// 0 is the lowest, 31 is the highest possible priority for pthread
			case TPri_AboveNormal: return 25;
			case TPri_Normal: return 15;
			case TPri_BelowNormal: return 5;
			default: UE_LOG(LogHAL, Fatal, TEXT("Unknown Priroty passed to FRunnableThreadPThread::TranslateThreadPriority()"));
		}
	}

	virtual void SetThreadPriority(pthread_t InThread, EThreadPriority NewPriority)
	{
		// initialize the structure
		struct sched_param Sched;
		FMemory::Memzero(&Sched, sizeof(struct sched_param));
		
		// set the priority appropriately
		Sched.sched_priority = TranslateThreadPriority(NewPriority);
		pthread_setschedparam(InThread, SCHED_RR, &Sched);
	}

	/**
	 * Wrapper for pthread_create that takes a name
	 *
	 * Allows a subclass to override this function to create a thread and give it a name, if
	 * the platform supports it
	 *
	 * Takes the same arguments as pthread_create
	 */
	virtual int CreateThreadWithName(pthread_t* HandlePtr, pthread_attr_t* AttrPtr, PthreadEntryPoint Proc, void* Arg, const ANSICHAR* /*Name*/)
	{
		// by default, we ignore the name since it's not in the standard pthreads
		return pthread_create(HandlePtr, AttrPtr, Proc, Arg);
	}

	/**
	 * Allows platforms to choose a default stack size for when a StackSize of 0 is given
	 */
	virtual int GetDefaultStackSize()
	{
		// Some information on default stack sizes, selected when given 0:
		// - On Windows, all threads get 1MB: http://msdn.microsoft.com/en-us/library/windows/desktop/ms686774(v=vs.85).aspx
		// - On Mac, main thread gets 8MB,
		// - all other threads get 512 kB when created through pthread or NSThread, and only 4kB when through MPTask()
		//	( http://developer.apple.com/library/mac/#qa/qa1419/_index.html )
		return 0;
	}

	/**
	 * Allows platforms to adjust stack size
	 */
	virtual uint32 AdjustStackSize(uint32 InStackSize)
	{
		// allow the platform to override default stack size
		if (InStackSize == 0)
		{
			InStackSize = GetDefaultStackSize();
		}
		return InStackSize;
	}

	bool SpinPthread(pthread_t *HandlePtr, PthreadEntryPoint Proc, uint32 InStackSize, void *Arg)
	{
		bool ThreadCreated = false;
		pthread_attr_t *AttrPtr = NULL;
		pthread_attr_t StackAttr;

		// allow platform to adjust stack size
		InStackSize = AdjustStackSize(InStackSize);

		if (InStackSize != 0)
		{
			if (pthread_attr_init(&StackAttr) == 0)
			{
				// we'll use this the attribute if this succeeds, otherwise, we'll wing it without it.
				const size_t StackSize = (size_t) InStackSize;
				if (pthread_attr_setstacksize(&StackAttr, StackSize) == 0)
				{
					AttrPtr = &StackAttr;
				}
			}

			if (AttrPtr == NULL)
			{
				UE_LOG(LogHAL, Log, TEXT("Failed to change pthread stack size to %d bytes"), (int) InStackSize);
			}
		}

		const int ThreadErrno = CreateThreadWithName(HandlePtr, AttrPtr, Proc, Arg, TCHAR_TO_ANSI(*ThreadName));
		ThreadCreated = (ThreadErrno == 0);
		if (AttrPtr != NULL)
		{
			pthread_attr_destroy(AttrPtr);
		}

		// Move the thread to the specified processors if requested
		if (!ThreadCreated)
		{
			UE_LOG(LogHAL, Log, TEXT("Failed to create thread! (err=%d, %s)"), ThreadErrno, UTF8_TO_TCHAR(strerror(ThreadErrno)));
		}

		return ThreadCreated;
	}

	/**
	 * The thread entry point. Simply forwards the call on to the right
	 * thread main function
	 */
	static void *STDCALL _ThreadProc(void *pThis)
	{
		check(pThis);

		FRunnableThreadPThread* ThisThread = (FRunnableThreadPThread*)pThis;

		// cache the thread ID for this thread (defined by the platform)
		ThisThread->ThreadID = FPlatformTLS::GetCurrentThreadId();

		// run the thread!
		ThisThread->PreRun();
		ThisThread->Run();
		ThisThread->PostRun();

		pthread_exit(NULL);
		return NULL;
	}

	virtual PthreadEntryPoint GetThreadEntryPoint()
	{
		return _ThreadProc;
	}

	/**
	 * Allows a platform subclass to setup anything needed on the thread before running the Run function
	 */
	virtual void PreRun()
	{
	}

	/**
	 * Allows a platform subclass to teardown anything needed on the thread after running the Run function
	 */
	virtual void PostRun()
	{
		if (bShouldDeleteSelf == true)
		{
			// Make sure the caller knows we want to delete this thread if we're still int CreateInternal.
			WantsToDeleteSelf.Increment();
			// Wait until the caller has finished setting up this thread in case Runnable execution was very short.
			ThreadCreatedSyncEvent->Wait();
			// Now clean up the thread handle so we don't leak
			Thread = PTHREAD_NULL;
			delete this;
		}
	}

	/**
	 * The real thread entry point. It calls the Init/Run/Exit methods on
	 * the runnable object
	 */
	uint32 Run()
	{
		ThreadIsRunning = true;
		// Assume we'll fail init
		uint32 ExitCode = 1;
		check(Runnable);

		// Initialize the runnable object
		if (Runnable->Init() == true)
		{
			// Initialization has completed, release the sync event
			ThreadInitSyncEvent->Trigger();
			// Now run the task that needs to be done
			ExitCode = Runnable->Run();
			// Allow any allocated resources to be cleaned up
			Runnable->Exit();
		}
		else
		{
			// Initialization has failed, release the sync event
			ThreadInitSyncEvent->Trigger();
		}

		// Should we delete the runnable?
		if (bShouldDeleteRunnable == true)
		{
			delete Runnable;
			Runnable = NULL;
		}
		// Clean ourselves up without waiting
		ThreadIsRunning = false;
		return ExitCode;
	}

public:
	FRunnableThreadPThread()
		: Thread(PTHREAD_NULL)
		, Runnable(NULL)
		, ThreadInitSyncEvent(NULL)
		, ThreadCreatedSyncEvent(NULL)
		, WantsToDeleteSelf(0)
		, bShouldDeleteSelf(false)
		, bShouldDeleteRunnable(false)
		, ThreadPriority(TPri_Normal)
		, ThreadID(0)
		, ThreadIsRunning(false)
	{
	}

	~FRunnableThreadPThread()
	{
		// Clean up our thread if it is still active
		if (Thread != PTHREAD_NULL)
		{
			Kill(true);
		}
		FRunnableThread::GetThreadRegistry().Remove(ThreadID);
		ThreadID = 0;
		delete ThreadCreatedSyncEvent;
	}

	virtual void SetThreadPriority(EThreadPriority NewPriority) OVERRIDE
	{
		// Don't bother calling the OS if there is no need
		if (NewPriority != ThreadPriority)
		{
			ThreadPriority = NewPriority;
			SetThreadPriority(Thread, NewPriority);
		}
	}

	virtual void Suspend(bool bShouldPause = 1) OVERRIDE
	{
		check(Thread);
		// Impossible in pthreads!
	}

	virtual bool Kill(bool bShouldWait = false) OVERRIDE
	{
		check(Thread && "Did you forget to call Create()?");
		bool bDidExitOK = true;
		// Let the runnable have a chance to stop without brute force killing
		if (Runnable)
		{
			Runnable->Stop();
		}
		// If waiting was specified, wait the amount of time. If that fails,
		// brute force kill that thread. Very bad as that might leak.
		if (bShouldWait == true)
		{
			while (ThreadIsRunning)
			{
				FPlatformProcess::Sleep(0.001f);
			}
		}

		// It's not really safe to kill a pthread. So don't do it.

		Thread = PTHREAD_NULL;

		// delete the runnable if requested and we didn't shut down gracefully already.
		if (Runnable && bShouldDeleteRunnable == true)
		{
			delete Runnable;
			Runnable = NULL;
		}
		// Delete ourselves if requested and we didn't shut down gracefully already.
		// This check prevents a double-delete of self when we shut down gracefully.
		if (!bDidExitOK && bShouldDeleteSelf == true)
		{
			delete this;
		}
		return bDidExitOK;
	}

	virtual void WaitForCompletion() OVERRIDE
	{
		// Block until this thread exits
		while (ThreadIsRunning)
		{
			FPlatformProcess::Sleep(0.001f);
		}
	}

	virtual uint32 GetThreadID() OVERRIDE
	{
		return ThreadID;
	}

	virtual FString GetThreadName() OVERRIDE
	{
		return ThreadName;
	}

	virtual void SetThreadAffinityMask(uint64 AffinityMask) OVERRIDE
	{
	}

protected:

	virtual bool CreateInternal(FRunnable* InRunnable, const TCHAR* InThreadName,
		bool bAutoDeleteSelf = 0,bool bAutoDeleteRunnable = 0,uint32 InStackSize = 0,
		EThreadPriority InThreadPri = TPri_Normal, uint64 InThreadAffinityMask = 0) OVERRIDE
	{
		check(InRunnable);
		Runnable = InRunnable;
		bShouldDeleteSelf = bAutoDeleteSelf;
		bShouldDeleteRunnable = bAutoDeleteRunnable;

		// Create a sync event to guarantee the Init() function is called first
		ThreadInitSyncEvent	= FPlatformProcess::CreateSynchEvent(true);
		// Create a sync event to guarantee the thread will not delete itself until it has been fully set up.
		ThreadCreatedSyncEvent = FPlatformProcess::CreateSynchEvent(true);
		// A name for the thread in for debug purposes. _ThreadProc will set it.
		ThreadName = InThreadName ? InThreadName : TEXT("Unnamed UE4");
		// Create the new thread
		bool ThreadCreated = SpinPthread(&Thread, GetThreadEntryPoint(), InStackSize, this);
		// If it fails, clear all the vars
		if (ThreadCreated)
		{
			pthread_detach(Thread);  // we can't join on these, since we can't determine when they'll die.

			// Let the thread start up, then set the name for debug purposes.
			ThreadInitSyncEvent->Wait((uint32)-1); // infinite wait

			// set the priority
			SetThreadPriority(InThreadPri);

			// set the affinity
			SetThreadAffinityMask(InThreadAffinityMask);
		}
		else // If it fails, clear all the vars
		{
			if (bAutoDeleteRunnable == true)
			{
				delete InRunnable;
			}
			Runnable = NULL;
		}

		// Cleanup the sync event
		delete ThreadInitSyncEvent;
		ThreadInitSyncEvent = NULL;
		return Thread != PTHREAD_NULL;
	}

	virtual bool NotifyCreated() OVERRIDE
	{
		const bool bHasFinished = !!WantsToDeleteSelf.GetValue();
		// It's ok to delete this thread if it wants to delete self.
		ThreadCreatedSyncEvent->Trigger();
		return bHasFinished;
	}
};

