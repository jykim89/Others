
#include "ALAudioDevice.h"
#include "AudioEffect.h"
#include "Engine.h"
#include "IAudioFormat.h"
#include "AudioDecompress.h"

/*------------------------------------------------------------------------------------
	FALSoundBuffer.
------------------------------------------------------------------------------------*/
/** 
 * Constructor
 *
 * @param AudioDevice	audio device this sound buffer is going to be attached to.
 */
FALSoundBuffer::FALSoundBuffer( FALAudioDevice* InAudioDevice )
{
	AudioDevice	= InAudioDevice;
}

/**
 * Destructor 
 * 
 * Frees wave data and detaches itself from audio device.
 */
FALSoundBuffer::~FALSoundBuffer( void )
{
	if( ResourceID )
	{
		AudioDevice->WaveBufferMap.Remove( ResourceID );
	}

	// Delete AL buffers.
	alDeleteBuffers( 1, BufferIds );
}

/**
 * Static function used to create a buffer.
 *
 * @param InWave		USoundNodeWave to use as template and wave source
 * @param AudioDevice	audio device to attach created buffer to
 * @param	bIsPrecacheRequest	Whether this request is for precaching or not
 * @return	FALSoundBuffer pointer if buffer creation succeeded, NULL otherwise
 */

FALSoundBuffer* FALSoundBuffer::Init(  FALAudioDevice* AudioDevice ,USoundWave* InWave )
{
	// Can't create a buffer without any source data
	if (InWave == NULL || InWave->NumChannels == 0)
	{
		return NULL;
	}

	FALSoundBuffer *Buffer = NULL;

	switch (static_cast<EDecompressionType>(InWave->DecompressionType))
	{
	case DTYPE_Setup:
		// Has circumvented pre-cache mechanism - pre-cache now
		AudioDevice->Precache(InWave, true, false);

		// Recall this function with new decompression type
		return Init(AudioDevice, InWave);

	case DTYPE_Native:
		if (InWave->ResourceID)
		{
			Buffer = static_cast<FALSoundBuffer*>(AudioDevice->WaveBufferMap.FindRef(InWave->ResourceID));
		}

		if (!Buffer)
		{
			Buffer = CreateNativeBuffer(AudioDevice, InWave);
		}
		break;

	case DTYPE_Invalid:
	case DTYPE_Preview:
	case DTYPE_Procedural:
	case DTYPE_RealTime:
	default:
		// Invalid will be set if the wave cannot be played
		break;
	}

	return Buffer;

}

FALSoundBuffer* FALSoundBuffer::CreateNativeBuffer( FALAudioDevice* AudioDevice, USoundWave* Wave)
{
	SCOPE_CYCLE_COUNTER( STAT_AudioResourceCreationTime );

	// Check to see if thread has finished decompressing on the other thread
	if (Wave->AudioDecompressor != NULL)
	{
		Wave->AudioDecompressor->EnsureCompletion();

		// Remove the decompressor
		delete Wave->AudioDecompressor;
		Wave->AudioDecompressor = NULL;
	}

	// Can't create a buffer without any source data
	if( Wave == NULL || Wave->NumChannels == 0 )
	{
		return( NULL );
	}
	FWaveModInfo WaveInfo;

	Wave->InitAudioResource(AudioDevice->GetRuntimeFormat());

	FALSoundBuffer* Buffer = NULL;
	
	// Find the existing buffer if any
	if( Wave->ResourceID )
	{
		Buffer = AudioDevice->WaveBufferMap.FindRef( Wave->ResourceID );
	}

	if( Buffer == NULL )
	{
		// Create new buffer.
		Buffer = new FALSoundBuffer( AudioDevice );

		alGenBuffers( 1, Buffer->BufferIds );

		AudioDevice->alError( TEXT( "RegisterSound" ) );

		// Allocate new resource ID and assign to USoundNodeWave. A value of 0 (default) means not yet registered.
		int ResourceID = AudioDevice->NextResourceID++;
		Buffer->ResourceID = ResourceID;
		Wave->ResourceID = ResourceID;

		AudioDevice->Buffers.Add( Buffer );
		AudioDevice->WaveBufferMap.FindOrAdd(ResourceID) =  Buffer ;

		// Keep track of associated resource name.
		Buffer->ResourceName = Wave->GetPathName();

		Buffer->InternalFormat = AudioDevice->GetInternalFormat( Wave->NumChannels );		
		Buffer->NumChannels = Wave->NumChannels;
		Buffer->SampleRate = Wave->SampleRate;

		if (Wave->RawPCMData)
		{
			// upload it
			Buffer->BufferSize = Wave->RawPCMDataSize;
			alBufferData( Buffer->BufferIds[0], Buffer->InternalFormat, Wave->RawPCMData, Wave->RawPCMDataSize, Buffer->SampleRate );

			// Free up the data if necessary
			if( Wave->bDynamicResource )
			{
				FMemory::Free( Wave->RawPCMData );
				Wave->RawPCMData = NULL;
				Wave->bDynamicResource = false;
			}
		}
		else
		{
			// get the raw data
			uint8* SoundData = ( uint8* )Wave->RawData.Lock( LOCK_READ_ONLY );
			// it's (possibly) a pointer to a wave file, so skip over the header
			int SoundDataSize = Wave->RawData.GetBulkDataSize();

			// is there a wave header?
			FWaveModInfo WaveInfo;
			if (WaveInfo.ReadWaveInfo(SoundData, SoundDataSize))
			{
				// if so, modify the location and size of the sound data based on header
				SoundData = WaveInfo.SampleDataStart;
				SoundDataSize = WaveInfo.SampleDataSize;
			}
			// let the Buffer know the final size
			Buffer->BufferSize = SoundDataSize;

			// upload it
			alBufferData( Buffer->BufferIds[0], Buffer->InternalFormat, SoundData, Buffer->BufferSize, Buffer->SampleRate );
			// unload it
			Wave->RawData.Unlock();
		}

		if( AudioDevice->alError( TEXT( "RegisterSound (buffer data)" ) ) || ( Buffer->BufferSize == 0 ) )
		{
			Buffer->InternalFormat = 0;
		}

		if( Buffer->InternalFormat == 0 )
		{
			UE_LOG ( LogAudio, Log,TEXT( "Audio: sound format not supported for '%s' (%d)" ), *Wave->GetName(), Wave->NumChannels );
			delete Buffer;
			Buffer = NULL;
		}
	}

	return Buffer;
}
