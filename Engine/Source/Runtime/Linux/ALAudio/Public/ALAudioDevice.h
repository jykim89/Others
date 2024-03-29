/*=============================================================================
	ALAudioDevice.h: Unreal OpenAL audio interface object.
	Copyright 1998-2012 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#pragma  once 

/*------------------------------------------------------------------------------------
	Dependencies, helpers & forward declarations.
------------------------------------------------------------------------------------*/

class FALAudioDevice;

#include "Engine.h"
#include "SoundDefinitions.h"
#include "AudioEffect.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack (push,8)
#endif
#define AL_NO_PROTOTYPES 1
#define ALC_NO_PROTOTYPES 1
#include "AL/al.h"
#include "AL/alc.h"
#if SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif 

DECLARE_LOG_CATEGORY_EXTERN(LogALAudio, Log, All);

/**
 * OpenAL implementation of FSoundBuffer, containing the wave data and format information.
 */
class FALSoundBuffer : public FSoundBuffer 
{
public:
	/** 
	 * Constructor
	 *
	 * @param AudioDevice	audio device this sound buffer is going to be attached to.
	 */
	FALSoundBuffer( FALAudioDevice* AudioDevice );
	
	/**
	 * Destructor 
	 * 
	 * Frees wave data and detaches itself from audio device.
 	 */
	~FALSoundBuffer( void );

	/**
	 * Static function used to create a buffer.
	 *
	 * @param	InWave				USoundWave to use as template and wave source
	 * @param	AudioDevice			Audio device to attach created buffer to
	 * @return	FALSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FALSoundBuffer* Init(  FALAudioDevice* ,USoundWave* );

		/**
	 * Static function used to create a IOSAudio buffer and upload decompressed ogg vorbis data to.
	 *
	 * @param Wave			USoundWave to use as template and wave source
	 * @param AudioDevice	audio device to attach created buffer to
	 * @return FIOSAudioSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FALSoundBuffer* CreateNativeBuffer(FALAudioDevice* , USoundWave* );


	/**
	 * Returns the size of this buffer in bytes.
	 *
	 * @return Size in bytes
	 */
	int GetSize( void ) 
	{ 
		return( BufferSize ); 
	}

	/** 
	 * Returns the number of channels for this buffer
	 */
	int GetNumChannels( void ) 
	{ 
		return( NumChannels ); 
	}
		
	/** Audio device this buffer is attached to */
	FALAudioDevice*			AudioDevice;
	/** Array of buffer ids used to reference the data stored in AL. */
	ALuint					BufferIds[2];
	/** Resource ID of associated USoundeWave */
	int						ResourceID;
	/** Human readable name of resource, most likely name of UObject associated during caching. */
	FString					ResourceName;
	/** Format of the data internal to OpenAL */
	ALuint					InternalFormat;

	/** Number of bytes stored in OpenAL, or the size of the ogg vorbis data */
	int						BufferSize;
	/** The number of channels in this sound buffer - should be directly related to InternalFormat */
	int						NumChannels;
	/** Sample rate of the ogg vorbis data - typically 44100 or 22050 */
	int						SampleRate;
};

/**
 * OpenAL implementation of FSoundSource, the interface used to play, stop and update sources
 */
class FALSoundSource : public FSoundSource
{
public:
	/**
	 * Constructor
	 *
	 * @param	InAudioDevice	audio device this source is attached to
	 */
	FALSoundSource( class FAudioDevice* InAudioDevice )
	:	FSoundSource( InAudioDevice ),
		SourceId( 0 ),
		Buffer( NULL )
	{}

	/** 
	 * Destructor
	 */
	~FALSoundSource( void );

	/**
	 * Initializes a source with a given wave instance and prepares it for playback.
	 *
	 * @param	WaveInstance	wave instance being primed for playback
	 * @return	TRUE			if initialization was successful, FALSE otherwise
	 */
	virtual bool Init( FWaveInstance* WaveInstance );

	/**
	 * Updates the source specific parameter like e.g. volume and pitch based on the associated
	 * wave instance.	
	 */
	virtual void Update( void );

	/**
	 * Plays the current wave instance.	
	 */
	virtual void Play( void );

	/**
	 * Stops the current wave instance and detaches it from the source.	
	 */
	virtual void Stop( void );

	/**
	 * Pauses playback of current wave instance.
	 */
	virtual void Pause( void );

	/**
	 * Queries the status of the currently associated wave instance.
	 *
	 * @return	TRUE if the wave instance/ source has finished playback and FALSE if it is 
	 *			currently playing or paused.
	 */
	virtual bool IsFinished( void );

	/** 
	 * Access function for the source id
	 */
	ALuint GetSourceId( void ) const { return( SourceId ); }

	/** 
	 * Returns TRUE if an OpenAL source has finished playing
	 */
	bool IsSourceFinished( void );

	/** 
	 * Handle dequeuing and requeuing of a single buffer
	 */
	void HandleQueuedBuffer( void );

protected:
	/** OpenAL source voice associated with this source/ channel. */
	ALuint				SourceId;
	/** Cached sound buffer associated with currently bound wave instance. */
	FALSoundBuffer*		Buffer;

	friend class FALAudioDevice;
};

/**
 * OpenAL implementation of an Unreal audio device.
 */
class FALAudioDevice : public FAudioDevice
{
public: 
	FALAudioDevice() {} 
	virtual ~FALAudioDevice() {} 

	virtual FName GetRuntimeFormat() OVERRIDE
	{
		static FName NAME_OGG(TEXT("OGG"));
		return NAME_OGG;
	}

	/** Starts up any platform specific hardware/APIs */
	virtual bool InitializeHardware() OVERRIDE;
 
	/**
	 * Tears down audio device by stopping all sounds, removing all buffers,  destroying all sources, ... Called by both Destroy and ShutdownAfterError
	 * to perform the actual tear down.
	 */
	virtual void TeardownHardware() OVERRIDE;

	/**
	 * Update the audio device and calculates the cached inverse transform later
	 * on used for spatialization.
	 *
	 * @param	Realtime	whether we are paused or not
	 */
	virtual void Update( bool bGameTicking );

	/** 
	 * Lists all the loaded sounds and their memory footprint
	 */
	virtual void ListSounds( const TCHAR* Cmd, FOutputDevice& Ar );

	/**
	 * Frees the bulk resource data associated with this SoundNodeWave.
	 *
	 * @param	SoundNodeWave	wave object to free associated bulk data
	 */
	virtual void FreeResource( USoundWave* SoundWave );

	virtual bool HasCompressedAudioInfoClass(USoundWave* SoundWave) OVERRIDE;

	virtual class ICompressedAudioInfo* CreateCompressedAudioInfo(USoundWave* SoundWave) OVERRIDE;

	void FindProcs( bool AllowExt );

	// Error checking.
	bool alError( const TCHAR* Text, bool Log = true );

protected:

	virtual FSoundSource* CreateSoundSource() OVERRIDE; 
	/** Returns the enum for the internal format for playing a sound with this number of channels. */
	ALuint GetInternalFormat( int NumChannels );

	// Dynamic binding.
	void FindProc( void*& ProcAddress, char* Name, char* SupportName, bool& Supports, bool AllowExt );
	bool FindExt( const TCHAR* Name );


	// Variables.

	/** All loaded resident buffers */
	TArray<FALSoundBuffer*>						Buffers;
	/** Map from resource ID to sound buffer */
	TMap<int, FALSoundBuffer*>					WaveBufferMap;
	/** Next resource ID value used for registering USoundWave objects */
	int											NextResourceID;

	// AL specific

	/** Device/context used to play back sounds (static so it can be initialized early) */
	static ALCdevice*							HardwareDevice;
	static ALCcontext*							SoundContext;
	void*										DLLHandle;
	/** Formats for multichannel sounds */
	ALenum										Surround40Format;
	ALenum										Surround51Format;
	ALenum										Surround61Format;
	ALenum										Surround71Format;

	friend class FALSoundBuffer;
};

