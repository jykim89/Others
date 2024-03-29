// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Audio.h: Unreal base audio.
=============================================================================*/

#pragma once
#include "Sound/SoundClass.h"

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogAudio, Warning, All);

/** 
 * Maximum number of channels that can be set using the ini setting
 */
#define MAX_AUDIOCHANNELS				64

/** 
 * Number of ticks an inaudible source remains alive before being stopped
 */
#define AUDIOSOURCE_TICK_LONGEVITY		60

/** 
 * Length of sound in seconds to be considered as looping forever
 */
#define INDEFINITELY_LOOPING_DURATION	10000.0f

/*
 * Some defaults to help cross platform consistency
 */
#define SPEAKER_COUNT					6

#define DEFAULT_LOW_FREQUENCY			600.0f
#define DEFAULT_MID_FREQUENCY			1000.0f
#define DEFAULT_HIGH_FREQUENCY			2000.0f

#define MAX_VOLUME						4.0f
#define MIN_PITCH						0.4f
#define MAX_PITCH						2.0f

/**
 * Some filters don't work properly with extreme values, so these are the limits 
 */
#define MIN_FILTER_GAIN					0.126f
#define MAX_FILTER_GAIN					7.94f

#define MIN_FILTER_FREQUENCY			20.0f
#define MAX_FILTER_FREQUENCY			20000.0f

#define MIN_FILTER_BANDWIDTH			0.1f
#define MAX_FILTER_BANDWIDTH			2.0f

/**
 * Audio stats
 */
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Audio Update Time" ), STAT_AudioUpdateTime, STATGROUP_Audio , );
DECLARE_DWORD_COUNTER_STAT_EXTERN( TEXT( "Active Sounds" ), STAT_ActiveSounds, STATGROUP_Audio , );
DECLARE_DWORD_COUNTER_STAT_EXTERN( TEXT( "Audio Sources" ), STAT_AudioSources, STATGROUP_Audio , );
DECLARE_DWORD_COUNTER_STAT_EXTERN( TEXT( "Wave Instances" ), STAT_WaveInstances, STATGROUP_Audio , );
DECLARE_DWORD_COUNTER_STAT_EXTERN( TEXT( "Wave Instances Dropped" ), STAT_WavesDroppedDueToPriority, STATGROUP_Audio , );
DECLARE_DWORD_COUNTER_STAT_EXTERN( TEXT( "Audible Wave Instances Dropped" ), STAT_AudibleWavesDroppedDueToPriority, STATGROUP_Audio , );
DECLARE_DWORD_COUNTER_STAT_EXTERN( TEXT( "Finished delegates called" ), STAT_AudioFinishedDelegatesCalled, STATGROUP_Audio , );
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Finished delegates time" ), STAT_AudioFinishedDelegates, STATGROUP_Audio , );
DECLARE_MEMORY_STAT_EXTERN( TEXT( "Audio Memory Used" ), STAT_AudioMemorySize, STATGROUP_Audio , );
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN( TEXT( "Audio Buffer Time" ), STAT_AudioBufferTime, STATGROUP_Audio , );
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN( TEXT( "Audio Buffer Time (w/ Channels)" ), STAT_AudioBufferTimeChannels, STATGROUP_Audio , );
DECLARE_DWORD_COUNTER_STAT_EXTERN( TEXT( "CPU Decompressed Wave Instances" ), STAT_OggWaveInstances, STATGROUP_Audio , );
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Gathering WaveInstances" ), STAT_AudioGatherWaveInstances, STATGROUP_Audio , );
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Processing Sources" ), STAT_AudioStartSources, STATGROUP_Audio , );
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Updating Sources" ), STAT_AudioUpdateSources, STATGROUP_Audio , ENGINE_API);
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Updating Effects" ), STAT_AudioUpdateEffects, STATGROUP_Audio , );
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Source Init" ), STAT_AudioSourceInitTime, STATGROUP_Audio , ENGINE_API);
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Source Create" ), STAT_AudioSourceCreateTime, STATGROUP_Audio , ENGINE_API);
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Submit Buffers" ), STAT_AudioSubmitBuffersTime, STATGROUP_Audio , ENGINE_API);
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Decompress Audio" ), STAT_AudioDecompressTime, STATGROUP_Audio , );
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Decompress Vorbis" ), STAT_VorbisDecompressTime, STATGROUP_Audio , );
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Prepare Audio Decompression" ), STAT_AudioPrepareDecompressionTime, STATGROUP_Audio , );
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Prepare Vorbis Decompression" ), STAT_VorbisPrepareDecompressionTime, STATGROUP_Audio , );
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Finding Nearest Location" ), STAT_AudioFindNearestLocation, STATGROUP_Audio , );
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Decompress Opus" ), STAT_OpusDecompressTime, STATGROUP_Audio , );

/**
 * Channel definitions for multistream waves
 *
 * These are in the sample order OpenAL expects for a 7.1 sound
 * 
 */
enum EAudioSpeakers
{							//	4.0	5.1	6.1	7.1
	SPEAKER_FrontLeft,		//	*	*	*	*
	SPEAKER_FrontRight,		//	*	*	*	*
	SPEAKER_FrontCenter,	//		*	*	*
	SPEAKER_LowFrequency,	//		*	*	*
	SPEAKER_LeftSurround,	//	*	*	*	*
	SPEAKER_RightSurround,	//	*	*	*	*
	SPEAKER_LeftBack,		//			*	*		If there is no BackRight channel, this is the BackCenter channel
	SPEAKER_RightBack,		//				*
	SPEAKER_Count
};

// Forward declarations.
class UAudioComponent;
class USoundNode;
struct FWaveInstance;
struct FReverbSettings;
struct FSampleLoop;

enum ELoopingMode
{
	/** One shot sound */
	LOOP_Never,
	/** Call the user callback on each loop for dynamic control */
	LOOP_WithNotification,
	/** Loop the sound forever */
	LOOP_Forever
};

struct FNotifyBufferFinishedHooks
{
	void AddNotify(USoundNode* NotifyNode, UPTRINT WaveInstanceHash);
	UPTRINT GetHashForNode(USoundNode* NotifyNode) const;
	void AddReferencedObjects( FReferenceCollector& Collector );
	void DispatchNotifies(FWaveInstance* WaveInstance, const bool bStopped);

	friend FArchive& operator<<( FArchive& Ar, FNotifyBufferFinishedHooks& WaveInstance );

private:

	struct FNotifyBufferDetails
	{
		USoundNode* NotifyNode;
		UPTRINT NotifyNodeWaveInstanceHash;

		FNotifyBufferDetails()
			: NotifyNode(NULL)
			, NotifyNodeWaveInstanceHash(0)
		{
		}

		FNotifyBufferDetails(USoundNode* InNotifyNode, UPTRINT InHash)
			: NotifyNode(InNotifyNode)
			, NotifyNodeWaveInstanceHash(InHash)
		{
		}
	};


	TArray<FNotifyBufferDetails> Notifies;
};

/**
 * Structure encapsulating all information required to play a USoundWave on a channel/source. This is required
 * as a single USoundWave object can be used in multiple active cues or multiple times in the same cue.
 */
struct ENGINE_API FWaveInstance
{
	/** Static helper to create good unique type hashes */
	static uint32 TypeHashCounter;

	/** Wave data */
	class USoundWave*	WaveData;
	/** Sound class */
	class USoundClass*  SoundClass;
	/** Sound nodes to notify when the current audio buffer finishes */
	FNotifyBufferFinishedHooks NotifyBufferFinishedHooks;

	/** Active Sound this wave instance belongs to */
	struct FActiveSound* ActiveSound;


	/** Current volume */
	float				Volume;
	/** Current volume multiplier - used to zero the volume without stopping the source */
	float				VolumeMultiplier;
	/** Current priority */
	float				PlayPriority;
	/** Voice center channel volume */
	float				VoiceCenterChannelVolume;
	/** Volume of the radio filter effect */
	float				RadioFilterVolume;
	/** The volume at which the radio filter kicks in */
	float				RadioFilterVolumeThreshold;
	/** The amount of stereo sounds to bleed to the rear speakers */
	float				StereoBleed;
	/** The amount of a sound to bleed to the LFE channel */
	float				LFEBleed;

	/** Looping mode - None, loop with notification, forever */
	ELoopingMode		LoopingMode;

	float				StartTime;

	/** Set to true if the sound nodes state that the radio filter should be applied */
	uint32				bApplyRadioFilter:1;

	/** Whether wave instanced has been started */
	uint32				bIsStarted:1;
	/** Whether wave instanced is finished */
	uint32				bIsFinished:1;
	/** Whether the notify finished hook has been called since the last update/parsenodes */
	uint32				bAlreadyNotifiedHook:1;
	/** Whether to use spatialization */
	uint32				bUseSpatialization:1;

	/** Whether to apply audio effects */
	uint32				bEQFilterApplied:1;
	/** Whether or not this sound plays when the game is paused in the UI */
	uint32				bIsUISound:1;
	/** Whether or not this wave is music */
	uint32				bIsMusic:1;
	/** Whether or not this wave has reverb applied */
	uint32				bReverb:1;
	/** Whether or not this sound class forces sounds to the center channel */
	uint32				bCenterChannelOnly:1;
	/** Prevent spamming of spatialization of surround sounds by tracking if the warning has already been emitted */
	uint32				bReportedSpatializationWarning:1;
	/** Which output target the sound should play to */
	EAudioOutputTarget::Type OutputTarget;
	/** Low pass filter setting */
	float				HighFrequencyGain;
	/** Current pitch */
	float				Pitch;
	/** Current velocity */
	FVector				Velocity;
	/** Current location */
	FVector				Location;
	/** At what distance we start transforming into omnidirectional soundsource */
	float				OmniRadius;
	/** Cached type hash */
	uint32				TypeHash;
	/** Hash value for finding the wave instance based on the path through the cue to get to it */
	UPTRINT				WaveInstanceHash;
	/** User / Controller index that owns the sound */
	uint8				UserIndex;

	/**
	 * Constructor, initializing all member variables.
	 *
	 * @param InAudioComponent	Audio component this wave instance belongs to.
	 */
	FWaveInstance( FActiveSound* ActiveSound );

	/**
	 * Stops the wave instance without notifying NotifyWaveInstanceFinishedHook. This will NOT stop wave instance
	 * if it is set up to loop indefinitely or set to remain active.
 	 */
	void StopWithoutNotification( void );

	/**
	 * Notifies the wave instance that the current playback buffer has finished.
	 */
	void NotifyFinished( const bool bStopped = false );

	/**
	 * Friend archive function used for serialization.
	 */
	friend FArchive& operator<<( FArchive& Ar, FWaveInstance* WaveInstance );

	/**
	 * Function used by the GC.
	 */
	void AddReferencedObjects( FReferenceCollector& Collector );

	/** Returns the actual volume the wave instance will play at */
	float GetActualVolume() const;
};

inline uint32 GetTypeHash( FWaveInstance* A ) { return A->TypeHash; }

/*-----------------------------------------------------------------------------
	FSoundBuffer.
-----------------------------------------------------------------------------*/

class FSoundBuffer
{
public:
	FSoundBuffer()
		: ResourceID(0)
		, NumChannels(0)
		, bAllocationInPermanentPool(false)
	{

	}

	ENGINE_API virtual ~FSoundBuffer();

	virtual int32 GetSize() PURE_VIRTUAL(FSoundBuffer::GetSize,return 0;);

	/**
	 * Describe the buffer (platform can override to add to the description, but should call the base class version)
	 * 
	 * @param bUseLongNames If TRUE, this will print out the full path of the sound resource, otherwise, it will show just the object name
	 */
	ENGINE_API virtual FString Describe(bool bUseLongName);

	/**
	 * Return the name of the sound class for this buffer 
	 */
	FName GetSoundClassName();

	/**
	 * Turn the number of channels into a string description
	 */
	FString GetChannelsDesc();

	/** Unique ID that ties this buffer to a USoundWave */
	int32	ResourceID;
	/** Cumulative channels from all streams */
	int32	NumChannels;
	/** Human readable name of resource, most likely name of UObject associated during caching.	*/
	FString	ResourceName;
	/** Whether memory for this buffer has been allocated from permanent pool. */
	bool	bAllocationInPermanentPool;
};

/*-----------------------------------------------------------------------------
	FSoundSource.
-----------------------------------------------------------------------------*/

class FSoundSource
{
public:
	// Constructor/ Destructor.
	FSoundSource(class FAudioDevice* InAudioDevice)
		: AudioDevice(InAudioDevice)
		, WaveInstance(NULL)
		, Buffer(NULL)
		, Playing(false)
		, Paused(false)
		, bReverbApplied(false)
		, StereoBleed(0.0f)
		, LFEBleed(0.5f)
		, HighFrequencyGain(1.0f)
		, LastUpdate(0)
	{
	}

	virtual ~FSoundSource( void )
	{
	}

	// Initialization & update.
	virtual bool Init( FWaveInstance* WaveInstance ) = 0;
	virtual void Update( void ) = 0;

	// Playback.
	virtual void Play( void ) = 0;
	ENGINE_API virtual void Stop( void );
	virtual void Pause( void ) = 0;

	// Query.
	virtual	bool IsFinished( void ) = 0;

	/**
	 * Returns a string describing the source (subclass can override, but it should call the base and append)
	 */
	ENGINE_API virtual FString Describe(bool bUseLongName);

	/**
	 * Returns whether the buffer associated with this source is using CPU decompression.
	 *
	 * @return true if decompressed on the CPU, false otherwise
	 */
	virtual bool UsesCPUDecompression( void )
	{
		return( false );
	}

	/**
	 * Returns whether associated audio component is an ingame only component, aka one that will
	 * not play unless we're in game mode (not paused in the UI)
	 *
	 * @return false if associated component has bIsUISound set, true otherwise
	 */
	bool IsGameOnly( void );

	/** 
	 * @return	The wave instance associated with the sound. 
	 */
	const FWaveInstance* GetWaveInstance( void ) const
	{
		return( WaveInstance );
	}

	/** 
	 * @return		true if the sound is playing, false otherwise. 
	 */
	bool IsPlaying( void ) const
	{
		return( Playing );
	}

	/** 
	 * @return		true if the sound is paused, false otherwise. 
	 */
	bool IsPaused( void ) const
	{
		return( Paused );
	}

	/** 
	 * Returns true if reverb should be applied
	 */
	bool IsReverbApplied( void ) const 
	{	
		return( bReverbApplied ); 
	}

	/** 
	 * Returns true if EQ should be applied
	 */
	bool IsEQFilterApplied( void ) const 
	{ 
		return( WaveInstance->bEQFilterApplied ); 
	}

	/**
	 * Set the bReverbApplied variable
	 */
	ENGINE_API bool SetReverbApplied( bool bHardwareAvailable );

	/**
	 * Set the StereoBleed variable
	 */
	ENGINE_API float SetStereoBleed( void );

	/**
	 * Set the LFEBleed variable
	 */
	ENGINE_API float SetLFEBleed( void );

	/**
	 * Set the HighFrequencyGain value
	 */
	ENGINE_API void SetHighFrequencyGain( void );

protected:
	// Variables.	
	class FAudioDevice*		AudioDevice;
	struct FWaveInstance*	WaveInstance;

	/** Cached sound buffer associated with currently bound wave instance. */
	class FSoundBuffer*		Buffer;

	/** Cached status information whether we are playing or not. */
	uint32				Playing:1;
	/** Cached status information whether we are paused or not. */
	uint32				Paused:1;
	/** Cached sound mode value used to detect when to switch outputs. */
	uint32				bReverbApplied:1;

	/** The amount of stereo sounds to bleed to the rear speakers */
	float				StereoBleed;
	/** The amount of a sound to bleed to the LFE speaker */
	float				LFEBleed;
	/** Low pass filter setting */
	float				HighFrequencyGain;

	/** Last tick when this source was active */
	int32					LastUpdate;
	/** Last tick when this source was active *and* had a hearable volume */
	int32					LastHeardUpdate;


	friend class FAudioDevice;
};


/*-----------------------------------------------------------------------------
	FWaveModInfo. 
-----------------------------------------------------------------------------*/

//
// Structure for in-memory interpretation and modification of WAVE sound structures.
//
class FWaveModInfo
{
public:

	// Pointers to variables in the in-memory WAVE file.
	uint32* pSamplesPerSec;
	uint32* pAvgBytesPerSec;
	uint16* pBlockAlign;
	uint16* pBitsPerSample;
	uint16* pChannels;
	uint16* pFormatTag;

	uint32  OldBitsPerSample;

	uint32* pWaveDataSize;
	uint32* pMasterSize;
	uint8*  SampleDataStart;
	uint8*  SampleDataEnd;
	uint32  SampleDataSize;
	uint8*  WaveDataEnd;

	uint32  NewDataSize;

	// Constructor.
	FWaveModInfo()
	{
	}
	
	// 16-bit padding.
	uint32 Pad16Bit( uint32 InDW )
	{
		return ((InDW + 1)& ~1);
	}

	// Read headers and load all info pointers in WaveModInfo. 
	// Returns 0 if invalid data encountered.
	ENGINE_API bool ReadWaveInfo( uint8* WaveData, int32 WaveDataSize, FString* ErrorMessage = NULL );
	/**
	 * Read a wave file header from bulkdata
	 */
	ENGINE_API bool ReadWaveHeader(uint8* RawWaveData, int32 Size, int32 Offset );

	ENGINE_API void ReportImportFailure() const;
};

/**
 * Brings loaded sounds up to date for the given platforms (or all platforms), and also sets persistent variables to cover any newly loaded ones.
 *
 * @param	Platform				Name of platform to cook for, or NULL if all platforms
 */
ENGINE_API void SetCompressedAudioFormatsToBuild(const TCHAR* Platform = NULL);

/**
 * Brings loaded sounds up to date for the given platforms (or all platforms), and also sets persistent variables to cover any newly loaded ones.
 *
 * @param	Platform				Name of platform to cook for, or NULL if all platforms
 */
ENGINE_API const TArray<FName>& GetCompressedAudioFormatsToBuild();
