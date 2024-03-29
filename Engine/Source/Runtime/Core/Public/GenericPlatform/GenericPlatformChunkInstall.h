// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	GenericPlatformChunkInstall.h: Generic platform chunk based install classes.
==============================================================================================*/

#pragma once

DECLARE_LOG_CATEGORY_EXTERN(LogChunkInstaller, Log, All);

namespace EChunkLocation
{
	enum Type
	{
		// note: higher quality locations should have higher enum values, we sort by these in AssetRegistry.cpp
		DoesNotExist,	// chunk does not exist
		NotAvailable,	// chunk has not been installed yet
		LocalSlow,		// chunk is on local slow media (optical)
		LocalFast,		// chunk is on local fast media (HDD)

		BestLocation=LocalFast
	};
}


namespace EChunkInstallSpeed
{
	enum Type
	{
		Paused,					// chunk installation is paused
		Slow,					// installation is lower priority than Game IO
		Fast					// installation is higher priority than Game IO
	};
}


namespace EChunkProgressReportingType
{
	enum Type
	{
		ETA,					// time remaining in seconds
		PercentageComplete		// percentage complete in 99.99 format
	};
}


/**
* Interface for platform specific chunk based install
**/
class CORE_API IPlatformChunkInstall
{
public:

	/** Virtual destructor */
	virtual ~IPlatformChunkInstall() {}

	/**
	 * Get the current location of a chunk.
	 * @param ChunkID		The id of the chunk to check.
	 * @return				Enum specifying whether the chunk is available to use, waiting to install, or does not exist.
	 **/
	virtual EChunkLocation::Type GetChunkLocation( uint32 ChunkID ) = 0;

	/** 
	 * Check if a given reporting type is supported.
	 * @param ReportType	Enum specifying how progress is reported.
	 * @return				true if reporting type is supported on the current platform.
	 **/
	virtual bool GetProgressReportingTypeSupported(EChunkProgressReportingType::Type ReportType) = 0;		

	/**
	 * Get the current install progress of a chunk.  Let the user specify report type for platforms that support more than one.
	 * @param ChunkID		The id of the chunk to check.
	 * @param ReportType	The type of progress report you want.
	 * @return				A value whose meaning is dependent on the ReportType param.
	 **/
	virtual float GetChunkProgress( uint32 ChunkID, EChunkProgressReportingType::Type ReportType ) = 0;

	/**
	 * Inquire about the priority of chunk installation vs. game IO.
	 * @return				Paused, low or high priority.
	 **/
	virtual EChunkInstallSpeed::Type GetInstallSpeed() = 0;
	/**
	 * Specify the priority of chunk installation vs. game IO.
	 * @param InstallSpeed	Pause, low or high priority.
	 * @return				false if the operation is not allowed, otherwise true.
	 **/
	virtual bool SetInstallSpeed( EChunkInstallSpeed::Type InstallSpeed ) = 0;
	
	/**
	 * Hint to the installer that we would like to prioritize a specific chunk (moves it to the head of the list.)
	 * @param ChunkID		The id of the chunk to prioritize.
	 * @return				false if the operation is not allowed or the chunk doesn't exist, otherwise true.
	 **/
	virtual bool PrioritizeChunk( uint32 ChunkID ) = 0;

	/**
	 * For platforms that support emulation of the Chunk install.  Starts transfer of the next chunk.
	 * Does nothing in a shipping build.
	 * @return				true if the opreation succeeds.
	 **/
	virtual bool DebugStartNextChunk() = 0;
};


/**
* Generic implementation of chunk based install
**/
class CORE_API FGenericPlatformChunkInstall : public IPlatformChunkInstall
{
public:
	/**
	 * Get the current location of a chunk.
	 * @param ChunkID		The id of the chunk to check.
	 * @return				Enum specifying whether the chunk is available to use, waiting to install, or does not exist.
	 **/
	virtual EChunkLocation::Type GetChunkLocation( uint32 ChunkID ) OVERRIDE
	{
		return EChunkLocation::LocalFast;
	}

	/** 
	 * Check if a given reporting type is supported.
	 * @param ReportType	Enum specifying how progress is reported.
	 * @return				true if reporting type is supported on the current platform.
	 **/
	virtual bool GetProgressReportingTypeSupported(EChunkProgressReportingType::Type ReportType) OVERRIDE
	{
		if (ReportType == EChunkProgressReportingType::PercentageComplete)
		{
			return true;
		}

		return false;
	}

	/**
	 * Get the current install progress of a chunk.  Let the user specify report type for platforms that support more than one.
	 * @param ChunkID		The id of the chunk to check.
	 * @param ReportType	The type of progress report you want.
	 * @return				A value whose meaning is dependent on the ReportType param.
	 **/
	virtual float GetChunkProgress( uint32 ChunkID, EChunkProgressReportingType::Type ReportType ) OVERRIDE
	{
		if (ReportType == EChunkProgressReportingType::PercentageComplete)
		{
			return 100.0f;
		}		
		return 0.0f;
	}

	/**
	 * Inquire about the priority of chunk installation vs. game IO.
	 * @return				Paused, low or high priority.
	 **/
	virtual EChunkInstallSpeed::Type GetInstallSpeed() OVERRIDE
	{
		return EChunkInstallSpeed::Paused;
	}

	/**
	 * Specify the priority of chunk installation vs. game IO.
	 * @param InstallSpeed	Pause, low or high priority.
	 * @return				false if the operation is not allowed, otherwise true.
	 **/
	virtual bool SetInstallSpeed( EChunkInstallSpeed::Type InstallSpeed ) OVERRIDE
	{
		return false;
	}
	
	/**
	 * Hint to the installer that we would like to prioritize a specific chunk (moves it to the head of the list.)
	 * @param ChunkID		The id of the chunk to prioritize.
	 * @return				false if the operation is not allowed or the chunk doesn't exist, otherwise true.
	 **/
	virtual bool PrioritizeChunk( uint32 ChunkID ) OVERRIDE
	{
		return false;
	}

	/**
	 * For platforms that support emulation of the Chunk install.  Starts transfer of the next chunk.
	 * Does nothing in a shipping build.
	 * @return				true if the opreation succeeds.
	 **/
	virtual bool DebugStartNextChunk()
	{
		return true;
	}
};
