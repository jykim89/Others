// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MallocProfilerEx.h: Extended memory profiling support.
=============================================================================*/

#ifndef INC_MALLOCPROFILEREX_H
#define INC_MALLOCPROFILEREX_H

#if USE_MALLOC_PROFILER

#include "MallocProfiler.h"

/** 
 * Extended version of malloc profiler, implements engine side functions that are not available in the core
 */
class FMallocProfilerEx : public FMallocProfiler
{
public:
	/**
	 * Constructor, initializing all member variables and potentially loading symbols.
	 *
	 * @param	InMalloc	The allocator wrapped by FMallocProfiler that will actually do the allocs/deallocs.
	 */
	FMallocProfilerEx( FMalloc* InMalloc );

	/** 
	 * Writes names of currently loaded levels. 
	 * Only to be called from within the mutex / scope lock of the FMallocProfiler.
	 *
	 * @param	InWorld		World Context.
	 */
	virtual void WriteLoadedLevels( UWorld* InWorld ) OVERRIDE;

	/** 
	 * Gather texture memory stats. 
	 */
	virtual void GetTexturePoolSize( FGenericMemoryStats& out_Stats ) OVERRIDE;
};

#endif // USE_MALLOC_PROFILER

#endif // INC_MALLOCPROFILEREX_H
