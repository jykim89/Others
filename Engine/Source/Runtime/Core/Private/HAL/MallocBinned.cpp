// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MallocBinned.cpp: Binned memory allocator
=============================================================================*/

#include "CorePrivate.h"

#include "MallocBinned.h"
#include "MemoryMisc.h"

/** Malloc binned allocator specific stats. */
DEFINE_STAT(STAT_Binned_OsCurrent);
DEFINE_STAT(STAT_Binned_OsPeak);
DEFINE_STAT(STAT_Binned_WasteCurrent);
DEFINE_STAT(STAT_Binned_WastePeak);
DEFINE_STAT(STAT_Binned_UsedCurrent);
DEFINE_STAT(STAT_Binned_UsedPeak);
DEFINE_STAT(STAT_Binned_CurrentAllocs);
DEFINE_STAT(STAT_Binned_TotalAllocs);
DEFINE_STAT(STAT_Binned_SlackCurrent);

void FMallocBinned::GetAllocatorStats( FGenericMemoryStats& out_Stats )
{
	FMalloc::GetAllocatorStats( out_Stats );

#if	STATS
	SIZE_T	LocalOsCurrent = 0;
	SIZE_T	LocalOsPeak = 0;
	SIZE_T	LocalWasteCurrent = 0;
	SIZE_T	LocalWastePeak = 0;
	SIZE_T	LocalUsedCurrent = 0;
	SIZE_T	LocalUsedPeak = 0;
	SIZE_T	LocalCurrentAllocs = 0;
	SIZE_T	LocalTotalAllocs = 0;
	SIZE_T	LocalSlackCurrent = 0;

	{
#ifdef USE_INTERNAL_LOCKS
		FScopeLock ScopedLock( &AccessGuard );
#endif

		UpdateSlackStat();

		// Copy memory stats.
		LocalOsCurrent = OsCurrent;
		LocalOsPeak = OsPeak;
		LocalWasteCurrent = WasteCurrent;
		LocalWastePeak = WastePeak;
		LocalUsedCurrent = UsedCurrent;
		LocalUsedPeak = UsedPeak;
		LocalCurrentAllocs = CurrentAllocs;
		LocalTotalAllocs = TotalAllocs;
		LocalSlackCurrent = SlackCurrent;
	}

	// Malloc binned stats.
	out_Stats.Add( GET_STATFNAME( STAT_Binned_OsCurrent ), LocalOsCurrent );
	out_Stats.Add( GET_STATFNAME( STAT_Binned_OsPeak ), LocalOsPeak );
	out_Stats.Add( GET_STATFNAME( STAT_Binned_WasteCurrent ), LocalWasteCurrent );
	out_Stats.Add( GET_STATFNAME( STAT_Binned_WastePeak ), LocalWastePeak );
	out_Stats.Add( GET_STATFNAME( STAT_Binned_UsedCurrent ), LocalUsedCurrent );
	out_Stats.Add( GET_STATFNAME( STAT_Binned_UsedPeak ), LocalUsedPeak );
	out_Stats.Add( GET_STATFNAME( STAT_Binned_CurrentAllocs ), LocalCurrentAllocs );
	out_Stats.Add( GET_STATFNAME( STAT_Binned_TotalAllocs ), LocalTotalAllocs );
	out_Stats.Add( GET_STATFNAME( STAT_Binned_SlackCurrent ), LocalSlackCurrent );
#endif // STATS
}