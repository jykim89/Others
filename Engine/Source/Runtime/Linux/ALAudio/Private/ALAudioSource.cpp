/*------------------------------------------------------------------------------------
	FALSoundSource.
------------------------------------------------------------------------------------*/

#include "ALAudioDevice.h"
#include "Engine.h"
/**
 * Initializes a source with a given wave instance and prepares it for playback.
 *
 * @param	WaveInstance	wave instace being primed for playback
 * @return	TRUE if initialization was successful, FALSE otherwise
 */
bool FALSoundSource::Init( FWaveInstance* InWaveInstance )
{
	if (InWaveInstance->OutputTarget != EAudioOutputTarget::Controller)
	{
		// Find matching buffer.
		Buffer = FALSoundBuffer::Init( (FALAudioDevice*)AudioDevice, InWaveInstance->WaveData  );
		if( Buffer )
		{
			SCOPE_CYCLE_COUNTER( STAT_AudioSourceInitTime );

			WaveInstance = InWaveInstance;

			// Enable/disable spatialization of sounds
			alSourcei( SourceId, AL_SOURCE_RELATIVE, WaveInstance->bUseSpatialization ? AL_FALSE : AL_TRUE );

			// Setting looping on a real time decompressed source suppresses the buffers processed message
			alSourcei( SourceId, AL_LOOPING, ( WaveInstance->LoopingMode == LOOP_Forever ) ? AL_TRUE : AL_FALSE );

			// Always queue up the first buffer
			alSourceQueueBuffers( SourceId, 1, Buffer->BufferIds );	
			if( WaveInstance->LoopingMode == LOOP_WithNotification )
			{		
				// We queue the sound twice for wave instances that use seamless looping so we can have smooth 
				// loop transitions. The downside is that we might play at most one frame worth of audio from the 
				// beginning of the wave after the wave stops looping.
				alSourceQueueBuffers( SourceId, 1, Buffer->BufferIds );
			}

			Update();

			// Initialization was successful.
			return true ;
		}
	}

	// Failed to initialize source.
	return false ;
}

/**
 * Clean up any hardware referenced by the sound source
 */
FALSoundSource::~FALSoundSource( void )
{
	// @todo openal: What do we do here
	/// AudioDevice->DestroyEffect( this );

	alDeleteSources( 1, &SourceId );
}

/**
 * Updates the source specific parameter like e.g. volume and pitch based on the associated
 * wave instance.	
 */
void FALSoundSource::Update( void )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioUpdateSources );

	if( !WaveInstance || Paused )
	{
		return;
	}

	float Volume = WaveInstance->Volume * WaveInstance->VolumeMultiplier;
	if( SetStereoBleed() )
	{
		// Emulate the bleed to rear speakers followed by stereo fold down
		Volume *= 1.25f;
	}
	Volume *= GVolumeMultiplier;

 			Volume	= 	FMath::Clamp(Volume, 0.0f, MAX_VOLUME ); 
	float	Pitch	=	FMath::Clamp(WaveInstance->Pitch, MIN_PITCH, MAX_PITCH ); 


	// Set whether to apply reverb
	SetReverbApplied( true );

	// Set the HighFrequencyGain value
	SetHighFrequencyGain();

	FVector Location;
	FVector	Velocity;

	// See file header for coordinate system explanation.
	Location.X = WaveInstance->Location.X;
	Location.Y = WaveInstance->Location.Z; // Z/Y swapped on purpose, see file header
	Location.Z = WaveInstance->Location.Y; // Z/Y swapped on purpose, see file header
	
	Velocity.X = WaveInstance->Velocity.X;
	Velocity.Y = WaveInstance->Velocity.Z; // Z/Y swapped on purpose, see file header
	Velocity.Z = WaveInstance->Velocity.Y; // Z/Y swapped on purpose, see file header

	// Convert to meters.
	Location *= AUDIO_DISTANCE_FACTOR;
	Velocity *= AUDIO_DISTANCE_FACTOR;

	// We're using a relative coordinate system for un- spatialized sounds.
	if( !WaveInstance->bUseSpatialization )
	{
		Location = FVector( 0.f, 0.f, 0.f );
	}

	alSourcef( SourceId, AL_GAIN, Volume );	
	alSourcef( SourceId, AL_PITCH, Pitch );		

	alSourcefv( SourceId, AL_POSITION, ( ALfloat * )&Location );
	alSourcefv( SourceId, AL_VELOCITY, ( ALfloat * )&Velocity );

	// Platform dependent call to update the sound output with new parameters
	// @todo openal: Is this no longer needed?
	/// AudioDevice->UpdateEffect( this );
}

/**
 * Plays the current wave instance.	
 */
void FALSoundSource::Play( void )
{
	if( WaveInstance )
	{
		alSourcePlay( SourceId );
		Paused = false;
		Playing = true;
	}
}

/**
 * Stops the current wave instance and detaches it from the source.	
 */
void FALSoundSource::Stop( void )
{
	if( WaveInstance )
	{
		alSourceStop( SourceId );
		// This clears out any pending buffers that may or may not be queued or played
		alSourcei( SourceId, AL_BUFFER, 0 );
		Paused = false;
		Playing = false;
		Buffer = NULL;
	}

	FSoundSource::Stop();
}

/**
 * Pauses playback of current wave instance.
 */
void FALSoundSource::Pause( void )
{
	if( WaveInstance )
	{
		alSourcePause( SourceId );
		Paused = true;
	}
}

/** 
 * Returns TRUE if an OpenAL source has finished playing
 */
bool FALSoundSource::IsSourceFinished( void )
{
	ALint State = AL_STOPPED;

	// Check the source for data to continue playing
	alGetSourcei( SourceId, AL_SOURCE_STATE, &State );
	if( State == AL_PLAYING || State == AL_PAUSED )
	{
		return( false );
	}

	return( true );
}

/** 
 * Handle dequeuing and requeuing of a single buffer
 */
void FALSoundSource::HandleQueuedBuffer( void )
{
	ALuint	DequeuedBuffer;

	// Unqueue the processed buffers
	alSourceUnqueueBuffers( SourceId, 1, &DequeuedBuffer );

	// Notify the wave instance that the current (native) buffer has finished playing.
	WaveInstance->NotifyFinished();

	// Queue the same packet again for looping
	alSourceQueueBuffers( SourceId, 1, Buffer->BufferIds );
}

/**
 * Queries the status of the currently associated wave instance.
 *
 * @return	TRUE if the wave instance/ source has finished playback and FALSE if it is 
 *			currently playing or paused.
 */
bool FALSoundSource::IsFinished( void )
{
	if( WaveInstance )
	{
		// Check for a non starved, stopped source
		if( IsSourceFinished() )
		{
			// Notify the wave instance that it has finished playing.
			WaveInstance->NotifyFinished();
			return( true );
		}
		else 
		{
			// Check to see if any complete buffers have been processed
			ALint BuffersProcessed;
			alGetSourcei( SourceId, AL_BUFFERS_PROCESSED, &BuffersProcessed );

			switch( BuffersProcessed )
			{
			case 0:
				// No buffers need updating
				break;

			case 1:
				// Standard case of 1 buffer expired which needs repopulating
				HandleQueuedBuffer();
				break;

			case 2:
				// Starvation case when the source has stopped 
				HandleQueuedBuffer();
				HandleQueuedBuffer();

				// Restart the source
				alSourcePlay( SourceId );
				break;
			}
		}

		return( false  );
	}

	return(  true );
}
