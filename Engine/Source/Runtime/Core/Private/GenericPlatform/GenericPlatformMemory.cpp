// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GenericPlatformMemory.cpp: Generic implementations of memory platform functions
=============================================================================*/

#include "CorePrivate.h"
#include "MallocAnsi.h"
#include "GenericPlatformMemoryPoolStats.h"
#include "MemoryMisc.h"


DEFINE_STAT(MCR_Physical);
DEFINE_STAT(MCR_GPU);
DEFINE_STAT(MCR_TexturePool);

DECLARE_MEMORY_STAT(TEXT("Total Physical"),		STAT_TotalPhysical,STATGROUP_MemoryPlatform);
DECLARE_MEMORY_STAT(TEXT("Total Virtual"),		STAT_TotalVirtual,STATGROUP_MemoryPlatform);
DECLARE_MEMORY_STAT(TEXT("PageSize"),			STAT_PageSize,STATGROUP_MemoryPlatform);
DECLARE_MEMORY_STAT(TEXT("Total Physical GB"),	STAT_TotalPhysicalGB,STATGROUP_MemoryPlatform);

DECLARE_MEMORY_STAT(TEXT("AvailablePhysical"),	STAT_AvailablePhysical,STATGROUP_MemoryPlatform);
DECLARE_MEMORY_STAT(TEXT("AvailableVirtual"),	STAT_AvailableVirtual,STATGROUP_MemoryPlatform);
DECLARE_MEMORY_STAT(TEXT("UsedPhysical"),		STAT_UsedPhysical,STATGROUP_MemoryPlatform);
DECLARE_MEMORY_STAT(TEXT("PeakUsedPhysical"),	STAT_PeakUsedPhysical,STATGROUP_MemoryPlatform);
DECLARE_MEMORY_STAT(TEXT("UsedVirtual"),		STAT_UsedVirtual,STATGROUP_MemoryPlatform);
DECLARE_MEMORY_STAT(TEXT("PeakUsedVirtual"),	STAT_PeakUsedVirtual,STATGROUP_MemoryPlatform);

FGenericPlatformMemoryStats::FGenericPlatformMemoryStats()
	: FGenericPlatformMemoryConstants( FPlatformMemory::GetConstants() )
	, AvailablePhysical( 0 )
	, AvailableVirtual( 0 )
	, UsedPhysical( 0 )
	, PeakUsedPhysical( 0 )
	, UsedVirtual( 0 )
	, PeakUsedVirtual( 0 )
{}

void FGenericPlatformMemory::SetupMemoryPools()
{
	SET_MEMORY_STAT(MCR_Physical, 0); // "unlimited" physical memory, we still need to make this call to set the short name, etc
	SET_MEMORY_STAT(MCR_GPU, 0); // "unlimited" GPU memory, we still need to make this call to set the short name, etc
	SET_MEMORY_STAT(MCR_TexturePool, 0); // "unlimited" Texture memory, we still need to make this call to set the short name, etc
}

void FGenericPlatformMemory::Init()
{
	SetupMemoryPools();
	UE_LOG(LogMemory, Warning, TEXT("FGenericPlatformMemory::Init not implemented on this platform"));
}

void FGenericPlatformMemory::OnOutOfMemory(uint64 Size, uint32 Alignment)
{
	UE_LOG(LogMemory, Fatal, TEXT("Ran out of memory allocating %llu bytes with alignment %u"), Size, Alignment);
}

FMalloc* FGenericPlatformMemory::BaseAllocator()
{
	return new FMallocAnsi();
}

FPlatformMemoryStats FGenericPlatformMemory::GetStats()
{
	UE_LOG(LogMemory, Warning, TEXT("FGenericPlatformMemory::GetStats not implemented on this platform"));
	return FPlatformMemoryStats();
}

void FGenericPlatformMemory::GetStatsForMallocProfiler( FGenericMemoryStats& out_Stats )
{
#if	STATS
	FPlatformMemoryStats Stats = FPlatformMemory::GetStats();

	// Base common stats for all platforms.
	out_Stats.Add( GET_STATFNAME( STAT_TotalPhysical ), Stats.TotalPhysical );
	out_Stats.Add( GET_STATFNAME( STAT_TotalVirtual ), Stats.TotalVirtual );
	out_Stats.Add( GET_STATFNAME( STAT_PageSize ), Stats.PageSize );
	out_Stats.Add( GET_STATFNAME( STAT_TotalPhysicalGB ), (SIZE_T)Stats.TotalPhysicalGB );
	out_Stats.Add( GET_STATFNAME( STAT_AvailablePhysical ), Stats.AvailablePhysical );
	out_Stats.Add( GET_STATFNAME( STAT_AvailableVirtual ), Stats.AvailableVirtual );
	out_Stats.Add( GET_STATFNAME( STAT_UsedPhysical ), Stats.UsedPhysical );
	out_Stats.Add( GET_STATFNAME( STAT_PeakUsedPhysical ), Stats.PeakUsedPhysical );
	out_Stats.Add( GET_STATFNAME( STAT_UsedVirtual ), Stats.UsedVirtual );
	out_Stats.Add( GET_STATFNAME( STAT_PeakUsedVirtual ), Stats.PeakUsedVirtual );
#endif // STATS
}

const FPlatformMemoryConstants& FGenericPlatformMemory::GetConstants()
{
	UE_LOG(LogMemory, Warning, TEXT("FGenericPlatformMemory::GetConstants not implemented on this platform"));
	static FPlatformMemoryConstants MemoryConstants;
	return MemoryConstants;
}

uint32 FGenericPlatformMemory::GetPhysicalGBRam()
{
	return FPlatformMemory::GetConstants().TotalPhysicalGB;
}

void FGenericPlatformMemory::UpdateStats()
{
	FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();

	SET_MEMORY_STAT(STAT_TotalPhysical,MemoryStats.TotalPhysical);
	SET_MEMORY_STAT(STAT_TotalVirtual,MemoryStats.TotalVirtual);
	SET_MEMORY_STAT(STAT_PageSize,MemoryStats.PageSize);
	SET_MEMORY_STAT(STAT_TotalPhysicalGB,MemoryStats.TotalPhysicalGB);

	SET_MEMORY_STAT(STAT_AvailablePhysical,MemoryStats.AvailablePhysical);
	SET_MEMORY_STAT(STAT_AvailableVirtual,MemoryStats.AvailableVirtual);
	SET_MEMORY_STAT(STAT_UsedPhysical,MemoryStats.UsedPhysical);
	SET_MEMORY_STAT(STAT_PeakUsedPhysical,MemoryStats.PeakUsedPhysical);
	SET_MEMORY_STAT(STAT_UsedVirtual,MemoryStats.UsedVirtual);
	SET_MEMORY_STAT(STAT_PeakUsedVirtual,MemoryStats.PeakUsedVirtual);
}

void* FGenericPlatformMemory::BinnedAllocFromOS( SIZE_T Size )
{
	UE_LOG(LogMemory, Error, TEXT("FGenericPlatformMemory::BinnedAllocFromOS not implemented on this platform"));
	return nullptr;
}

void FGenericPlatformMemory::BinnedFreeToOS( void* Ptr )
{
	UE_LOG(LogMemory, Error, TEXT("FGenericPlatformMemory::BinnedFreeToOS not implemented on this platform"));
}

void FGenericPlatformMemory::DumpStats( class FOutputDevice& Ar )
{
	const float InvMB = 1.0f / 1024.0f / 1024.0f;
	FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
#if !NO_LOGGING
	const FName CategoryName(LogMemory.GetCategoryName());
#else
	const FName CategoryName(TEXT("LogMemory"));
#endif
	Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Platform Memory Stats for %s"), ANSI_TO_TCHAR(FPlatformProperties::PlatformName()));
	Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Process Physical Memory: %.2f MB used, %.2f MB peak"), MemoryStats.UsedPhysical*InvMB, MemoryStats.PeakUsedPhysical*InvMB);
	Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Process Virtual Memory: %.2f MB used, %.2f MB peak"), MemoryStats.UsedVirtual*InvMB, MemoryStats.PeakUsedVirtual*InvMB);

	Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Physical Memory: %.2f MB used, %.2f MB total"), (MemoryStats.TotalPhysical - MemoryStats.AvailablePhysical)*InvMB, MemoryStats.TotalPhysical*InvMB);
	Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Virtual Memory: %.2f MB used, %.2f MB total"), (MemoryStats.TotalVirtual - MemoryStats.AvailableVirtual)*InvMB, MemoryStats.TotalVirtual*InvMB);

}

void FGenericPlatformMemory::DumpPlatformAndAllocatorStats( class FOutputDevice& Ar )
{
	FPlatformMemory::DumpStats( Ar );
	GMalloc->DumpAllocatorStats( Ar );
}

void FGenericPlatformMemory::Memswap( void* Ptr1, void* Ptr2, SIZE_T Size )
{
	void* Temp = FMemory_Alloca(Size);
	FPlatformMemory::Memcpy( Temp, Ptr1, Size );
	FPlatformMemory::Memcpy( Ptr1, Ptr2, Size );
	FPlatformMemory::Memcpy( Ptr2, Temp, Size );
}

FGenericPlatformMemory::FSharedMemoryRegion::FSharedMemoryRegion(const FString & InName, uint32 InAccessMode, void * InAddress, SIZE_T InSize)
	:	AccessMode(InAccessMode)
	,	Address(InAddress)
	,	Size(InSize)
{
	FCString::Strcpy(Name, sizeof(Name) - 1, *InName);
}

FGenericPlatformMemory::FSharedMemoryRegion * FGenericPlatformMemory::MapNamedSharedMemoryRegion(const FString & Name, bool bCreate, uint32 AccessMode, SIZE_T Size)
{
	UE_LOG(LogHAL, Error, TEXT("FGenericPlatformMemory::MapNamedSharedMemoryRegion not implemented on this platform"));
	return NULL;
}

bool FGenericPlatformMemory::UnmapNamedSharedMemoryRegion(FSharedMemoryRegion * MemoryRegion)
{
	UE_LOG(LogHAL, Error, TEXT("FGenericPlatformMemory::UnmapNamedSharedMemoryRegion not implemented on this platform"));
	return false;
}
