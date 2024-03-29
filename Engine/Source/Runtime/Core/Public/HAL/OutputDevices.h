// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OutputDevices.h: Collection of FOutputDevice subclasses
=============================================================================*/

#pragma once
#if !PLATFORM_DESKTOP

// don't support colorized text on consoles
#define SET_WARN_COLOR(Color)
#define SET_WARN_COLOR_AND_BACKGROUND(Color, Bkgrnd)
#define CLEAR_WARN_COLOR() 

#else
/*-----------------------------------------------------------------------------
	Colorized text.

	To use colored text from a commandlet, you use the SET_WARN_COLOR macro with
	one of the following standard colors. Then use CLEAR_WARN_COLOR to return 
	to default.

	To use the standard colors, you simply do this:
	SET_WARN_COLOR(COLOR_YELLOW);
	
	You can specify a background color by appending it to the foreground:
	SET_WARN_COLOR, COLOR_YELLOW COLOR_DARK_RED);

	This will have bright yellow text on a dark red background.

	Or you can make your own in the format:
	
	ForegroundRed | ForegroundGreen | ForegroundBlue | ForegroundBright | BackgroundRed | BackgroundGreen | BackgroundBlue | BackgroundBright
	where each value is either 0 or 1 (can leave off trailing 0's), so 
	blue on bright yellow is "00101101" and red on black is "1"
	
	An empty string reverts to the normal gray on black.
-----------------------------------------------------------------------------*/
// putting them in a namespace to protect against future name conflicts
namespace OutputDeviceColor
{
const TCHAR* const COLOR_BLACK			= TEXT("0000");

const TCHAR* const COLOR_DARK_RED		= TEXT("1000");
const TCHAR* const COLOR_DARK_GREEN		= TEXT("0100");
const TCHAR* const COLOR_DARK_BLUE		= TEXT("0010");
const TCHAR* const COLOR_DARK_YELLOW	= TEXT("1100");
const TCHAR* const COLOR_DARK_CYAN		= TEXT("0110");
const TCHAR* const COLOR_DARK_PURPLE	= TEXT("1010");
const TCHAR* const COLOR_DARK_WHITE		= TEXT("1110");
const TCHAR* const COLOR_GRAY			= COLOR_DARK_WHITE;

const TCHAR* const COLOR_RED			= TEXT("1001");
const TCHAR* const COLOR_GREEN			= TEXT("0101");
const TCHAR* const COLOR_BLUE			= TEXT("0011");
const TCHAR* const COLOR_YELLOW			= TEXT("1101");
const TCHAR* const COLOR_CYAN			= TEXT("0111");
const TCHAR* const COLOR_PURPLE			= TEXT("1011");
const TCHAR* const COLOR_WHITE			= TEXT("1111");

const TCHAR* const COLOR_NONE			= TEXT("");

}
using namespace OutputDeviceColor;

// let a console or (UE_BUILD_SHIPPING || UE_BUILD_TEST) define it to nothing
#ifndef SET_WARN_COLOR

/**
 * Set the console color with Color or a Color and Background color
 */
#define SET_WARN_COLOR(Color) \
	UE_LOG(LogHAL, SetColor, TEXT("%s"), Color);
#define SET_WARN_COLOR_AND_BACKGROUND(Color, Bkgrnd) \
	UE_LOG(LogHAL, SetColor, TEXT("%s%s"), Color, Bkgrnd);

/**
 * Return color to it's default
 */
#define CLEAR_WARN_COLOR() \
	UE_LOG(LogHAL, SetColor, TEXT("%s"), COLOR_NONE);

#endif
#endif

/*-----------------------------------------------------------------------------
	Log Suppression
-----------------------------------------------------------------------------*/

/** Interface to the log suppression system **/
class FLogSuppressionInterface
{
public:
	/** Singleton, returns a reference the global log suppression implementation. **/
	static CORE_API FLogSuppressionInterface& Get();
	/** Used by FLogCategoryBase to register itself with the global category table **/
	virtual void AssociateSuppress(struct FLogCategoryBase* Destination) = 0;
	/** Used by FLogCategoryBase to unregister itself from the global category table **/
	virtual void DisassociateSuppress(struct FLogCategoryBase* Destination) = 0;
	/** Called by appInit once the config files and commandline are set up. The log suppression system uses these to setup the boot time defaults. **/
	virtual void ProcessConfigAndCommandLine() = 0;
};

/** Base class for all log categories. **/
struct CORE_API FLogCategoryBase
{
	/** 
	 * Constructor, registers with the log suppression system and sets up the default values.
	 * @param CategoryName, name of the category
	 * @param InDefaultVerbosity, default verbosity for the category, may ignored and overrridden by many other mechanisms
	 * @param InCompileTimeVerbosity, mostly used to keep the working verbosity in bounds, macros elsewhere actually do the work of stripping compiled out things.
	**/
	FLogCategoryBase(const TCHAR *CategoryName, ELogVerbosity::Type InDefaultVerbosity, ELogVerbosity::Type InCompileTimeVerbosity);

	/** Destructor, unregisters from the log suppression system **/
	~FLogCategoryBase();
	/** Should not generally be used directly. Tests the runtime verbosity and maybe triggers a debug break, etc. **/
	FORCEINLINE bool IsSuppressed(ELogVerbosity::Type VerbosityLevel)
	{
		if ((VerbosityLevel & ELogVerbosity::VerbosityMask) <= Verbosity)
		{
			return false;
		}
		return true;
	}
	/** Called just after a logging statement being allow to print. Checks a few things and maybe breaks into the debugger. **/
	void PostTrigger(ELogVerbosity::Type VerbosityLevel);
	FORCEINLINE FName GetCategoryName()
	{
		return CategoryFName;
	}

	/** Sets up the working verbosity and clamps to the compile time verbosity. **/
	void SetVerbosity(ELogVerbosity::Type Verbosity);

private:
	friend class FLogSuppressionImplementation;
	friend class FLogScopedVerbosityOverride;
	/** Internal call to get the working verbosity. **/
	ELogVerbosity::Type GetVerbosity() const	{ return (ELogVerbosity::Type)Verbosity; }
	/** Internal call to set up the working verbosity from the boot time default. **/
	void ResetFromDefault();
protected:

	/** Holds the current suppression state **/
	uint8 Verbosity;
	/** Holds the break flag **/
	bool DebugBreakOnLog;
	/** Holds default supression **/
	uint8 DefaultVerbosity;
	/** Holds compile time supression **/
	uint8 CompileTimeVerbosity;
	/** FName for this category **/
	FName CategoryFName;
};

/** Template for log categories that transfers the compile-time constant default and compile time verbosity to the FLogCategoryBase constructor. **/
template<uint8 InDefaultVerbosity, uint8 InCompileTimeVerbosity>
struct FLogCategory : public FLogCategoryBase
{
	checkAtCompileTime((InDefaultVerbosity & ELogVerbosity::VerbosityMask) < ELogVerbosity::NumVerbosity, bogus_default_verbosity);
	checkAtCompileTime(InCompileTimeVerbosity < ELogVerbosity::NumVerbosity, bogus_compile_time_verbosity);
	enum
	{
		CompileTimeVerbosity = InCompileTimeVerbosity
	};

	FORCEINLINE FLogCategory(const TCHAR *CategoryName)
		: FLogCategoryBase(CategoryName, ELogVerbosity::Type(InDefaultVerbosity), ELogVerbosity::Type(CompileTimeVerbosity))
	{
	}
};

/** 
 * Helper class that uses thread local storage to set up the default category and verbosity for the low level logging functions.
 * This is what allow a UE_LOG(LogHAL, Log, TEXT("...")); within a UE_LOG statement to know what the category and verbosity is.
 * When one of these goes out of scope, it restores the previous values.
**/
class CORE_API FScopedCategoryAndVerbosityOverride
{
public:
	/** STructure to aggregate a category and verbosity pair **/
	struct FOverride
	{
		ELogVerbosity::Type Verbosity;
		FName Category;
		FOverride()
			: Verbosity(ELogVerbosity::Log)
		{
		}
		FOverride(FName InCategory, ELogVerbosity::Type InVerbosity)
			: Verbosity(InVerbosity)
			, Category(InCategory)
		{
		}
		void operator=(const FOverride& Other)
		{
			Verbosity = Other.Verbosity;
			Category = Other.Category;
		}
	};
private:
	/** Backup of the category, verbosity pairs that was present when we were constructed **/
	FOverride Backup;
public:
	/** Back up the existing category and varbosity pair, then sets them.*/
	FScopedCategoryAndVerbosityOverride(FName Category, ELogVerbosity::Type Verbosity);

	/** Restore the category and verbosity overrides to the previous value.*/
	~FScopedCategoryAndVerbosityOverride();

	/** Manages a TLS slot with the current overrides for category and verbosity. **/
	static FOverride* GetTLSCurrent();
};

/*-----------------------------------------------------------------------------
	FLogScopedVerbosityOverride
-----------------------------------------------------------------------------*/
/** 
 * Helper class that allows setting scoped verbosity for log category. 
 * Saved what was previous verbosity for the category, and recovers it when it goes out of scope. 
 * Use Macro LOG_SCOPE_VERBOSITY_OVERRIDE for this
 **/
class CORE_API FLogScopedVerbosityOverride
{
private:
	/** Backup of the category, verbosity pairs that was present when we were constructed **/
	FLogCategoryBase * SavedCategory;
	ELogVerbosity::Type SavedVerbosity;

public:
	/** Back up the existing verbosity for the category then sets new verbosity.*/
	FLogScopedVerbosityOverride(FLogCategoryBase * Category, ELogVerbosity::Type Verbosity);

	/** Restore the verbosity overrides for the category to the previous value.*/
	~FLogScopedVerbosityOverride();
};


/*-----------------------------------------------------------------------------
	FOutputDeviceRedirector.
-----------------------------------------------------------------------------*/

/** The type of lines buffered by secondary threads. */
struct FBufferedLine
{
	const FString Data;
	const ELogVerbosity::Type Verbosity;
	const FName Category;

	/** Initialization constructor. */
	FBufferedLine( const TCHAR* InData, ELogVerbosity::Type InVerbosity, const class FName& InCategory )
		: Data( InData )
		, Verbosity( InVerbosity )
		, Category( InCategory )
	{}
};

/**
 * Class used for output redirection to allow logs to show
 */
class CORE_API FOutputDeviceRedirector : public FOutputDevice
{
private:
	/** A FIFO of lines logged by non-master threads. */
	TArray<FBufferedLine> BufferedLines;

	/** A FIFO backlog of messages logged before the editor had a chance to intercept them. */
	TArray<FBufferedLine> BacklogLines;

	/** Array of output devices to redirect to */
	TArray<FOutputDevice*> OutputDevices;

	/** The master thread ID.  Logging from other threads will be buffered for processing by the master thread. */
	uint32 MasterThreadID;

	/** Whether backlogging is enabled. */
	bool bEnableBacklog;

	/** Object used for synchronization via a scoped lock */
	FCriticalSection	SynchronizationObject;

	/**
	 * The unsynchronized version of FlushThreadedLogs.
	 * Assumes that the caller holds a lock on SynchronizationObject.
	 * @param bUseAllDevices - if true this method will use all output devices
	 */
	void UnsynchronizedFlushThreadedLogs( bool bUseAllDevices );

public:

	/** Initialization constructor. */
	FOutputDeviceRedirector();

	/**
	 * Get the GLog singleton
	 */
	static FOutputDeviceRedirector* Get();

	/**
	 * Adds an output device to the chain of redirections.	
	 *
	 * @param OutputDevice	output device to add
	 */
	void AddOutputDevice( FOutputDevice* OutputDevice );

	/**
	 * Removes an output device from the chain of redirections.	
	 *
	 * @param OutputDevice	output device to remove
	 */
	void RemoveOutputDevice( FOutputDevice* OutputDevice );

	/**
	 * Returns whether an output device is currently in the list of redirectors.
	 *
	 * @param	OutputDevice	output device to check the list against
	 * @return	true if messages are currently redirected to the the passed in output device, false otherwise
	 */
	bool IsRedirectingTo( FOutputDevice* OutputDevice );

	/** Flushes lines buffered by secondary threads. */
	virtual void FlushThreadedLogs();

	/**
	 *	Flushes lines buffered by secondary threads.
	 *	Only used if a background thread crashed and we needed to push the callstack into the log. 
	 */
	virtual void PanicFlushThreadedLogs();

	/**
	 * Serializes the current backlog to the specified output device.
	 * @param OutputDevice	- Output device that will receive the current backlog
	 */
	virtual void SerializeBacklog( FOutputDevice* OutputDevice );

	/**
	 * Enables or disables the backlog.
	 * @param bEnable	- Starts saving a backlog if true, disables and discards any backlog if false
	 */
	virtual void EnableBacklog( bool bEnable );

	/**
	 * Sets the current thread to be the master thread that prints directly
	 * (isn't queued up)
	 */
	virtual void SetCurrentThreadAsMasterThread();

	/**
	 * Serializes the passed in data via all current output devices.
	 *
	 * @param	Data	Text to log
	 * @param	Event	Event name used for suppression purposes
	 */
	virtual void Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category) OVERRIDE;

	/**
	 * Passes on the flush request to all current output devices.
	 */
	void Flush();

	/**
	 * Closes output device and cleans up. This can't happen in the destructor
	 * as we might have to call "delete" which cannot be done for static/ global
	 * objects.
	 */
	void TearDown();
};

/*-----------------------------------------------------------------------------
	FOutputDevice subclasses.
-----------------------------------------------------------------------------*/

/** string added to the filename of timestamped backup log files */
#define BACKUP_LOG_FILENAME_POSTFIX TEXT("-backup-")

/**
 * File output device.
 */
class CORE_API FOutputDeviceFile : public FOutputDevice
{
public:
	/** 
	 * Constructor, initializing member variables.
	 *
	 * @param InFilename	Filename to use, can be NULL
	 * @param bDisableBackup If true, existing files will not be backed up
	 */
	FOutputDeviceFile( const TCHAR* InFilename = NULL, bool bDisableBackup=false );

	/** Sets the filename that the output device writes to.  If the output device was already writing to a file, closes that file. */
	void SetFilename(const TCHAR* InFilename);

	/**
	 * Closes output device and cleans up. This can't happen in the destructor
	 * as we have to call "delete" which cannot be done for static/ global
	 * objects.
	 */
	void TearDown();

	/**
	 * Flush the write cache so the file isn't truncated in case we crash right
	 * after calling this function.
	 */
	void Flush();

	virtual void Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category ) OVERRIDE;

	virtual bool CanBeUsedOnAnyThread() const OVERRIDE
	{
		return true;
	}

private:
	FArchive*	LogAr;
	TCHAR		Filename[1024];
	bool		Opened;
	bool		Dead;

	/** If true, existing files will not be backed up */
	bool		bDisableBackup;
	
	void WriteRaw( const TCHAR* C );
};

// Null output device.
class CORE_API FOutputDeviceNull : public FOutputDevice
{
public:
	/**
	 * NULL implementation of Serialize.
	 *
	 * @param	Data	unused
	 * @param	Event	unused
	 */
	virtual void Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category ) OVERRIDE
	{}
};

class CORE_API FOutputDeviceDebug : public FOutputDevice
{
public:
	/**
	 * Serializes the passed in data unless the current event is suppressed.
	 *
	 * @param	Data	Text to log
	 * @param	Event	Event name used for suppression purposes
	 */
	virtual void Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category ) OVERRIDE;

	virtual bool CanBeUsedOnAnyThread() const OVERRIDE
	{
		return true;
	}
};

/** Buffered output device. */
class FBufferedOutputDevice : public FOutputDevice
{
	TArray<FBufferedLine> BufferedLines;

public:
	virtual void Serialize( const TCHAR* InData, ELogVerbosity::Type Verbosity, const class FName& Category ) OVERRIDE
	{
		new(BufferedLines)FBufferedLine( InData, Verbosity, Category );
	}

	/** Pushes buffered lines into the specified output device. */
	void RedirectTo( class FOutputDevice& Ar )
	{
		for( const FBufferedLine& BufferedLine : BufferedLines )
		{
			Ar.Serialize( *BufferedLine.Data, BufferedLine.Verbosity, BufferedLine.Category );
		}
	}
};

/*-----------------------------------------------------------------------------
	FOutputDeviceError subclasses.
-----------------------------------------------------------------------------*/

class FOutputDeviceAnsiError : public FOutputDeviceError
{
public:
	/** Constructor, initializing member variables */
	FOutputDeviceAnsiError();

	/**
	 * Serializes the passed in data unless the current event is suppressed.
	 *
	 * @param	Data	Text to log
	 * @param	Event	Event name used for suppression purposes
	 */
	virtual void Serialize( const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category ) OVERRIDE;

	virtual bool CanBeUsedOnAnyThread() const OVERRIDE
	{
		return true;
	}

	/**
	 * Error handling function that is being called from within the system wide global
	 * error handler, e.g. using structured exception handling on the PC.
	 */
	void HandleError();

private:

	int32		ErrorPos;
};

CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogHAL, Log, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogMac, Log, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogIOS, Log, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogAndroid, Log, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogPS4, Log, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogWindows, Log, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogSerialization, Log, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogUnrealMath, Log, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogUnrealMatrix, Log, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogContentComparisonCommandlet, Log, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogNetPackageMap, Warning, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogNetSerialization, Warning, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogMemory, Log, All);

// Temporary log category, generally you should not check things in that use this
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogTemp, Log, All);

