// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AsyncWork.h: Definition of queued work classes
=============================================================================*/

#pragma once

/**
	FAutoDeleteAsyncTask - template task for jobs that delete themselves when complete

	Sample code:

	class ExampleAutoDeleteAsyncTask : public FNonAbandonableTask
	{
		friend class FAutoDeleteAsyncTask<ExampleAutoDeleteAsyncTask>;

		int32 ExampleData;

		ExampleAutoDeleteAsyncTask(int32 InExampleData)
		 : ExampleData(InExampleData)
		{
		}

		void DoWork()
		{
			... do the work here
		}

		static const TCHAR *Name()
		{
			return TEXT("ExampleAutoDeleteAsyncTask");
		}
	};

	start an example job

	(new FAutoDeleteAsyncTask<ExampleAutoDeleteAsyncTask>(5)->StartBackgroundTask();

	do an example job now, on this thread

	(new FAutoDeleteAsyncTask<ExampleAutoDeleteAsyncTask>(5)->StartSynchronousTask();

**/
template<typename TTask>
class FAutoDeleteAsyncTask : private FQueuedWork
{
	/** User job embedded in this task */ 
	TTask Task;

	/* Generic start function, not called directly
		* @param bForceSynchronous if true, this job will be started synchronously, now, on this thread
	**/
	void Start(bool bForceSynchronous)
	{
		FPlatformMisc::MemoryBarrier();
		FQueuedThreadPool* QueuedPool = GThreadPool;
		if (bForceSynchronous)
		{
			QueuedPool = 0;
		}
		if (QueuedPool)
		{
			QueuedPool->AddQueuedWork(this);
		}
		else 
		{
			// we aren't doing async stuff
			DoWork();
		}
	}

	/* 
	* Tells the user job to do the work, sometimes called synchronously, sometimes from the thread pool. Calls the event tracker.
	**/
	void DoWork()
	{		
		Task.DoWork();		
		delete this;
	}

	/* 
	* Always called from the thread pool. Just passes off to DoWork
	**/
	virtual void DoThreadedWork()
	{
		DoWork();
	}

	/**
	 * Always called from the thread pool. Called if the task is removed from queue before it has started which might happen at exit.
	 * If the user job can abandon, we do that, otherwise we force the work to be done now (doing nothing would not be safe).
	 */
	virtual void Abandon(void)
	{
		if (Task.CanAbandon())
		{
			Task.Abandon();
			delete this;
		}
		else
		{
			DoWork();
		}
	}

public:
	/** Default constructor. */
	FAutoDeleteAsyncTask( )
	{
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T>
	FAutoDeleteAsyncTask( T Arg )
		: Task(Arg)
	{
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2>
	FAutoDeleteAsyncTask( T1 Arg1, T2 Arg2 )
		: Task(Arg1,Arg2)
	{
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3>
	FAutoDeleteAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3 )
		: Task(Arg1,Arg2,Arg3)
	{
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3, typename T4>
	FAutoDeleteAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3, T4 Arg4 )
		: Task(Arg1,Arg2,Arg3,Arg4)
	{
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3, typename T4, typename T5>
	FAutoDeleteAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3, T4 Arg4, T5 Arg5 )
		: Task(Arg1,Arg2,Arg3,Arg4,Arg5)
	{
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3, typename T4, typename T5, typename T6>
	FAutoDeleteAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3, T4 Arg4, T5 Arg5, T6 Arg6 )
		: Task(Arg1,Arg2,Arg3,Arg4,Arg5,Arg6)
	{
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
	FAutoDeleteAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3, T4 Arg4, T5 Arg5, T6 Arg6, T7 Arg7 )
		: Task(Arg1,Arg2,Arg3,Arg4,Arg5,Arg6,Arg7)
	{
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
	FAutoDeleteAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3, T4 Arg4, T5 Arg5, T6 Arg6, T7 Arg7, T8 Arg8 )
		: Task(Arg1,Arg2,Arg3,Arg4,Arg5,Arg6,Arg7,Arg8)
	{
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>
	FAutoDeleteAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3, T4 Arg4, T5 Arg5, T6 Arg6, T7 Arg7, T8 Arg8, T9 Arg9 )
		: Task(Arg1,Arg2,Arg3,Arg4,Arg5,Arg6,Arg7,Arg8,Arg9)
	{
	}


	/* 
	* Run this task on this thread, now. Will end up destroying myself, so it is not safe to use this object after this call.
	**/
	void StartSynchronousTask()
	{
		Start(true);
	}

	/* 
	* Run this task on the lo priority thread pool. It is not safe to use this object after this call.
	**/
	void StartBackgroundTask()
	{
		Start(false);
	}

};


/**
	FAsyncTask - template task for jobs queued to thread pools

	Sample code:

	class ExampleAsyncTask : public FNonAbandonableTask
	{
		friend class FAsyncTask<ExampleAsyncTask>;

		int32 ExampleData;

		ExampleAsyncTask(int32 InExampleData)
		 : ExampleData(InExampleData)
		{
		}

		void DoWork()
		{
			... do the work here
		}

		static const TCHAR *Name()
		{
			return TEXT("ExampleAsyncTask");
		}
	};

	start an example job

	FAutoDeleteAsyncTask<ExampleAutoDeleteAsyncTask> MyTask(5);
	
	MyTask.StartBackgroundTask();

	-- or -- 

	MyTask.StartSynchronousTask();

	to just do it now on this thread

	Check if the task is done:

	if (MyTask.IsDone())
	{
	}

	Spinning on IsDone is not acceptable (see EnsureCompletion), but it is ok to check once a frame.

	Ensure the task is done, doing the task on the current thread if it has not been started, waiting until completion in all cases.

	Task.EnsureCompletion();


**/
template<typename TTask>
class FAsyncTask : private FQueuedWork
{
	/** User job embedded in this task */ 
	TTask Task;
	/** Thread safe counter that indicates WORK completion, no necessarily finalization of the job */
	FThreadSafeCounter	WorkNotFinishedCounter;
	/** If we aren't doing the work synchrnously, this will hold the completion event */
	FEvent*				DoneEvent;
	/** Pool we are queued into, maintained by the calling thread */
	FQueuedThreadPool*	QueuedPool;

	/* Internal function to destroy the completion event
	**/
	void DestroyEvent()
	{
		delete DoneEvent;
		DoneEvent = NULL;
	}

	/* Generic start function, not called directly
		* @param bForceSynchronous if true, this job will be started synchronously, now, on this thread
	**/
	void Start(bool bForceSynchronous)
	{
		FPlatformMisc::MemoryBarrier();
		CheckIdle();  // can't start a job twice without it being completed first
		WorkNotFinishedCounter.Increment();
		QueuedPool = GThreadPool;
		if (bForceSynchronous)
		{
			QueuedPool = 0;
		}
		if (QueuedPool)
		{
			if (!DoneEvent)
			{
				DoneEvent = FPlatformProcess::CreateSynchEvent(true);
			}
			DoneEvent->Reset();
			QueuedPool->AddQueuedWork(this);
		}
		else 
		{
			// we aren't doing async stuff
			DestroyEvent();
			DoWork();
		}
	}

	/* 
	* Tells the user job to do the work, sometimes called synchronously, sometimes from the thread pool. Calls the event tracker.
	**/
	void DoWork()
	{		
		Task.DoWork();		
		check(WorkNotFinishedCounter.GetValue() == 1);
		WorkNotFinishedCounter.Decrement();
	}

	/* 
	* Triggers the work completion event, only called from a pool thread
	**/
	void FinishThreadedWork()
	{
		check(QueuedPool);
		if (DoneEvent)
		{
			DoneEvent->Trigger();
		}
	}

	/* 
	* Performs the work, this is only called from a pool thread.
	**/
	virtual void DoThreadedWork()
	{
		DoWork();
		FinishThreadedWork();
	}

	/**
	 * Always called from the thread pool. Called if the task is removed from queue before it has started which might happen at exit.
	 * If the user job can abandon, we do that, otherwise we force the work to be done now (doing nothing would not be safe).
	 */
	virtual void Abandon(void)
	{
		if (Task.CanAbandon())
		{
			Task.Abandon();
			check(WorkNotFinishedCounter.GetValue() == 1);
			WorkNotFinishedCounter.Decrement();
		}
		else
		{
			DoWork();
		}
		FinishThreadedWork();
	}

	/* 
	* Internal call to assert that we are idle
	**/
	void CheckIdle() const
	{
		check(WorkNotFinishedCounter.GetValue() == 0);
		check(!QueuedPool);
	}

	/* 
	* Internal call to synchronize completion between threads, never called from a pool thread
	**/
	void SyncCompletion()
	{
		FPlatformMisc::MemoryBarrier();
		if (QueuedPool)
		{
			check(DoneEvent); // if it is not done yet, we must have an event
			DoneEvent->Wait();
			QueuedPool = 0;
		}
		CheckIdle();
	}

	/* 
	* Internal call to intialize internal variables
	**/
	void Init()
	{
		DoneEvent = 0;
		QueuedPool = 0;
	}

public:
	/** Default constructor. */
	FAsyncTask( )
	{
		Init();
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T>
	FAsyncTask( T Arg )
		: Task(Arg)
	{
		Init();
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2>
	FAsyncTask( T1 Arg1, T2 Arg2 )
		: Task(Arg1,Arg2)
	{
		Init();
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3>
	FAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3 )
		: Task(Arg1,Arg2,Arg3)
	{
		Init();
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3, typename T4>
	FAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3, T4 Arg4 )
		: Task(Arg1,Arg2,Arg3,Arg4)
	{
		Init();
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3, typename T4, typename T5>
	FAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3, T4 Arg4, T5 Arg5 )
		: Task(Arg1,Arg2,Arg3,Arg4,Arg5)
	{
		Init();
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3, typename T4, typename T5, typename T6>
	FAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3, T4 Arg4, T5 Arg5, T6 Arg6 )
		: Task(Arg1,Arg2,Arg3,Arg4,Arg5,Arg6)
	{
		Init();
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
	FAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3, T4 Arg4, T5 Arg5, T6 Arg6, T7 Arg7 )
		: Task(Arg1,Arg2,Arg3,Arg4,Arg5,Arg6,Arg7)
	{
		Init();
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
	FAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3, T4 Arg4, T5 Arg5, T6 Arg6, T7 Arg7, T8 Arg8 )
		: Task(Arg1,Arg2,Arg3,Arg4,Arg5,Arg6,Arg7,Arg8)
	{
		Init();
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>
	FAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3, T4 Arg4, T5 Arg5, T6 Arg6, T7 Arg7, T8 Arg8, T9 Arg9 )
		: Task(Arg1,Arg2,Arg3,Arg4,Arg5,Arg6,Arg7,Arg8,Arg9)
	{
		Init();
	}

	/** Destructor, not legal when a task is in process */
	~FAsyncTask()
	{
		// destroying an unfinished task is a bug
		CheckIdle();
		DestroyEvent();
	}

	/* Retrieve embedded user job, not legal to call while a job is in process
		* @return reference to embedded user job 
	**/
	TTask &GetTask()
	{
		CheckIdle();  // can't modify a job without it being completed first
		return Task;
	}

	/* Retrieve embedded user job, not legal to call while a job is in process
		* @return reference to embedded user job 
	**/
	const TTask &GetTask() const
	{
		CheckIdle();  // could be safe, but I won't allow it anyway because the data could be changed while it is being read
		return Task;
	}

	/* 
	* Run this task on this thread
	* @param bDoNow if true then do the job now instead of at EnsureCompletion
	**/
	void StartSynchronousTask()
	{
		Start(true);
	}

	/* 
	* Queue this task for processing by the background thread pool
	**/
	void StartBackgroundTask()
	{
		Start(false);
	}

	/* 
	* Wait until the job is complete
	* @param bDoWorkOnThisThreadIfNotStarted if true and the work has not been started, retract the async task and do it now on this thread
	**/
	void EnsureCompletion(bool bDoWorkOnThisThreadIfNotStarted = true)
	{
		bool DoSyncCompletion = true;
		if (bDoWorkOnThisThreadIfNotStarted)
		{
			if (QueuedPool)
			{
				if (QueuedPool->RetractQueuedWork(this))
				{
					// we got the job back, so do the work now and no need to synchronize
					DoSyncCompletion = false;
					DoWork(); 
					FinishThreadedWork();
					QueuedPool = 0;
				}
			}
			else if (WorkNotFinishedCounter.GetValue())  // in the synchronous case, if we haven't done it yet, do it now
			{
				DoWork(); 
			}
		}
		if (DoSyncCompletion)
		{
			SyncCompletion();
		}
		CheckIdle(); // Must have had bDoWorkOnThisThreadIfNotStarted == false and needed it to be true for a synchronous job
	}

	/** Returns true if the work and TASK has completed, false while it's still in progress. 
	 * prior to returning true, it synchronizes so the task can be destroyed or reused
	*/
	bool IsDone()
	{
		if (!IsWorkDone())
		{
			return false;
		}
		SyncCompletion();
		return true;
	}

	/** Returns true if the work has completed, false while it's still in progress. 
	 * This does not block and if true, you can use the results.
	 * But you can't destroy or reuse the task without IsDone() being true or EnsureCompletion()
	*/
	bool IsWorkDone() const
	{
		if (WorkNotFinishedCounter.GetValue())
		{
			return false;
		}
		return true;
	}

	/** Returns true if the work has not been started or has been completed. NOT to be used for synchronization, but great for check()'s */
	bool IsIdle() const
	{
		return WorkNotFinishedCounter.GetValue() == 0 && QueuedPool == 0;
	}

};

/**
 * Stub class to use a base class for tasks that cannot be abandoned
 */
class FNonAbandonableTask
{
public:
	bool CanAbandon()
	{
		return false;
	}
	void Abandon()
	{
	}
};

/**
 * Asynchronous decompression, used for decompressing chunks of memory in the background
 */
class FAsyncUncompress : public FNonAbandonableTask
{
	/** Buffer containing uncompressed data					*/
	void*	UncompressedBuffer;
	/** Size of uncompressed data in bytes					*/
	int32		UncompressedSize;
	/** Buffer compressed data is going to be written to	*/
	void*	CompressedBuffer;
	/** Size of CompressedBuffer data in bytes				*/
	int32		CompressedSize;
	/** Flags to control decompression						*/
	ECompressionFlags Flags;
	/** Whether the source memory is padded with a full cache line at the end */
	bool	bIsSourceMemoryPadded;

public:
	/**
	 * Initializes the data and creates the event.
	 *
	 * @param	Flags				Flags to control what method to use for decompression
	 * @param	UncompressedBuffer	Buffer containing uncompressed data
	 * @param	UncompressedSize	Size of uncompressed data in bytes
	 * @param	CompressedBuffer	Buffer compressed data is going to be written to
	 * @param	CompressedSize		Size of CompressedBuffer data in bytes
	 * @param	bIsSourcePadded		Whether the source memory is padded with a full cache line at the end
	 */
	FAsyncUncompress( 
		ECompressionFlags InFlags, 
		void* InUncompressedBuffer, 
		int32 InUncompressedSize, 
		void* InCompressedBuffer, 
		int32 InCompressedSize, 
		bool bIsSourcePadded = false)
	:	UncompressedBuffer( InUncompressedBuffer )
		,	UncompressedSize( InUncompressedSize )
		,	CompressedBuffer( InCompressedBuffer )
		,	CompressedSize( InCompressedSize )
		,	Flags( InFlags )
		,	bIsSourceMemoryPadded( bIsSourcePadded )
	{

	}
	/**
	 * Performs the async decompression
	 */
	void DoWork()
	{
		// Uncompress from memory to memory.
		verify( FCompression::UncompressMemory( Flags, UncompressedBuffer, UncompressedSize, CompressedBuffer, CompressedSize, bIsSourceMemoryPadded ) );
	}
	/** Give the name for external event viewers
	* @return	the name to display in external event viewers
	*/
	static const TCHAR *Name()
	{
		return TEXT("FAsyncUncompress");
	}
};


