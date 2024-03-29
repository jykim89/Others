// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
 	CoreAudioDevice.cpp: Unreal CoreAudio audio interface object.
 =============================================================================*/

/*------------------------------------------------------------------------------------
 Audio includes.
 ------------------------------------------------------------------------------------*/

#include "CoreAudioDevice.h"
#include "AudioDecompress.h"
#include "AudioEffect.h"
#include "CoreAudioEffects.h"
#include "Engine.h"

DEFINE_LOG_CATEGORY(LogCoreAudio);

class FCoreAudioDeviceModule : public IAudioDeviceModule
{
public:

	/** Creates a new instance of the audio device implemented by the module. */
	virtual FAudioDevice* CreateAudioDevice() OVERRIDE
	{
		return new FCoreAudioDevice;
	}
};

IMPLEMENT_MODULE(FCoreAudioDeviceModule, CoreAudio);

/*------------------------------------------------------------------------------------
 FAudioDevice Interface.
 ------------------------------------------------------------------------------------*/

/**
 * Initializes the audio device and creates sources.
 *
 * @return true if initialization was successful, false otherwise
 */
bool FCoreAudioDevice::InitializeHardware()
{
	if (IsRunningDedicatedServer())
	{
		return false;
	}

	// Load ogg and vorbis dlls if they haven't been loaded yet
	LoadVorbisLibraries();

	for( SInt32 Index = 0; Index < MAX_AUDIOCHANNELS; ++Index )
	{
		Mixer3DInputStatus[ Index ] = false;
	}

	for( SInt32 Index = 0; Index < MAX_MULTICHANNEL_AUDIOCHANNELS; ++Index )
	{
		MatrixMixerInputStatus[ Index ] = false;
	}

	// Make sure the output audio device exists
	AudioDeviceID HALDevice;
	UInt32 Size = sizeof( AudioDeviceID );
	AudioObjectPropertyAddress PropertyAddress;
	PropertyAddress.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
	PropertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
	PropertyAddress.mElement = kAudioObjectPropertyElementMaster;

	OSStatus Status = AudioObjectGetPropertyData( kAudioObjectSystemObject, &PropertyAddress, 0, NULL, &Size, &HALDevice );
	if( Status != noErr )
	{
		UE_LOG(LogInit, Log, TEXT( "No audio devices found!" ) );
		return false;
	}

	Status = NewAUGraph( &AudioUnitGraph );
	if( Status != noErr )
	{
		UE_LOG(LogInit, Log, TEXT( "Failed to create audio unit graph!" ) );
		return false;
	}

	AudioComponentDescription Desc;
	Desc.componentFlags = 0;
	Desc.componentFlagsMask = 0;
	Desc.componentType = kAudioUnitType_Output;
	Desc.componentSubType = kAudioUnitSubType_DefaultOutput;
	Desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	Status = AUGraphAddNode( AudioUnitGraph, &Desc, &OutputNode );
	if( Status == noErr )
	{
		Status = AUGraphOpen( AudioUnitGraph );
		if( Status == noErr )
		{
			Status = AUGraphNodeInfo( AudioUnitGraph, OutputNode, NULL, &OutputUnit );
			if( Status == noErr )
			{
				Status = AudioUnitInitialize( OutputUnit );
			}
		}
	}

	if( Status != noErr )
	{
		UE_LOG(LogInit, Log, TEXT( "Failed to initialize audio output unit!" ) );
		Teardown();
		return false;
	}

	Desc.componentFlags = 0;
	Desc.componentFlagsMask = 0;
	Desc.componentType = kAudioUnitType_Mixer;
	Desc.componentSubType = kAudioUnitSubType_3DMixer;
	Desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	
	Status = AUGraphAddNode( AudioUnitGraph, &Desc, &Mixer3DNode );
	if( Status == noErr )
	{
		Status = AUGraphNodeInfo( AudioUnitGraph, Mixer3DNode, NULL, &Mixer3DUnit );
		if( Status == noErr )
		{
			Status = AudioUnitInitialize( Mixer3DUnit );
		}
	}
	
	if( Status != noErr )
	{
		UE_LOG(LogInit, Log, TEXT( "Failed to initialize audio 3D mixer unit!" ) );
		Teardown();
		return false;
	}
	
	Desc.componentFlags = 0;
	Desc.componentFlagsMask = 0;
	Desc.componentType = kAudioUnitType_Mixer;
	Desc.componentSubType = kAudioUnitSubType_MatrixMixer;
	Desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	
	Status = AUGraphAddNode( AudioUnitGraph, &Desc, &MatrixMixerNode );
	if( Status == noErr )
	{
		Status = AUGraphNodeInfo( AudioUnitGraph, MatrixMixerNode, NULL, &MatrixMixerUnit );
		
		// Set number of buses for input
		uint32 NumBuses = MAX_MULTICHANNEL_AUDIOCHANNELS;
		Size = sizeof( NumBuses );
		
		Status = AudioUnitSetProperty( MatrixMixerUnit, kAudioUnitProperty_ElementCount, kAudioUnitScope_Input, 0, &NumBuses, Size );
		if( Status == noErr )
		{
			// Set number fo buses for output
			NumBuses = 1;
			Status = AudioUnitSetProperty(	MatrixMixerUnit, kAudioUnitProperty_ElementCount, kAudioUnitScope_Output, 0, &NumBuses, Size );
		}

		if( Status != noErr )
		{
			UE_LOG(LogInit, Log, TEXT( "Failed to setup audio matrix mixer unit!" ) );
			Teardown();
			return false;
		}
		
		// Get default input stream format
		Size = sizeof( AudioStreamBasicDescription );
		Status = AudioUnitGetProperty( MatrixMixerUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &MatrixMixerInputFormat, &Size );
		
		// Set output stream format to SPEAKER_COUT (6 channels)
		MatrixMixerInputFormat.mChannelsPerFrame = SPEAKER_COUNT;
		MatrixMixerInputFormat.mFramesPerPacket = 1;
		MatrixMixerInputFormat.mBytesPerPacket = MatrixMixerInputFormat.mBytesPerFrame;
		MatrixMixerInputFormat.mFormatFlags |= kAudioFormatFlagIsNonInterleaved;
		
		for( int32 Index = 0; Index < MAX_MULTICHANNEL_AUDIOCHANNELS; Index++ )
		{
			Status = AudioUnitSetProperty( MatrixMixerUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, Index, &MatrixMixerInputFormat, Size );

			if( Status != noErr )
			{
				UE_LOG(LogInit, Log, TEXT( "Failed to setup audio matrix mixer unit input format!" ) );
				Teardown();
				return false;
			}
		}
		
		// Set output stream format
		Size = sizeof( AudioStreamBasicDescription );
		Status = AudioUnitGetProperty( MatrixMixerUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &MatrixMixerOutputFormat, &Size );
		
		if( Status != noErr )
		{
			UE_LOG(LogInit, Log, TEXT( "Failed to setup audio matrix mixer unit output format!" ) );
			Teardown();
			return false;
		}
		
		// Initialize Matrix Mixer unit
		Status = AudioUnitInitialize( MatrixMixerUnit );
		
		if( Status != noErr )
		{
			UE_LOG(LogInit, Log, TEXT( "Failed to initialize audio matrix mixer unit!" ) );
			Teardown();
			return false;
		}
		
		// Enable Output
		AudioUnitSetParameter( MatrixMixerUnit, kMatrixMixerParam_Enable, kAudioUnitScope_Output, 0, 1.0, 0 );
		
		// Set Output volume
		AudioUnitSetParameter( MatrixMixerUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Output, 0, 1.0, 0 );
		AudioUnitSetParameter( MatrixMixerUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Output, 1, 1.0, 0 );
		
		// Set Master volume
		AudioUnitSetParameter( MatrixMixerUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Global, 0xFFFFFFFF, 1.0, 0 );
	}
	
	Size = sizeof( AudioStreamBasicDescription );
	Status = AudioUnitGetProperty( Mixer3DUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &Mixer3DFormat, &Size );
	if( Status == noErr )
	{
		// Connect 3D Mixer to Output node
		Status = AUGraphConnectNodeInput( AudioUnitGraph, Mixer3DNode, 0, OutputNode, 0 );
	}

	if( Status != noErr )
	{
		UE_LOG(LogInit, Log, TEXT( "Failed to start audio graph!" ) );
		Teardown();
		return false;
	}
	
	// Connect Matrix Mixer to 3D Mixer node
	Status = AUGraphConnectNodeInput( AudioUnitGraph, MatrixMixerNode, 0, Mixer3DNode, 0 );
	Mixer3DInputStatus[0] = true;
	
	if( Status != noErr )
	{
		UE_LOG(LogInit, Log, TEXT( "Failed to start audio graph!" ) );
		Teardown();
		return false;
	}

	Status = AUGraphInitialize( AudioUnitGraph );
	if( Status == noErr )
	{
		Status = AUGraphStart( AudioUnitGraph );
	}

	if( Status != noErr )
	{
		UE_LOG(LogInit, Log, TEXT( "Failed to start audio graph!" ) );
		Teardown();
		return false;
	}

	return true;
}

void FCoreAudioDevice::TeardownHardware()
{
	if( AudioUnitGraph )
	{
		AUGraphStop( AudioUnitGraph );
		DisposeAUGraph( AudioUnitGraph );
		AudioUnitGraph = NULL;
		OutputNode = 0;
		OutputUnit = NULL;
		Mixer3DNode = 0;
		Mixer3DUnit = NULL;
		MatrixMixerNode = 0;
		MatrixMixerUnit = NULL;
	}
	
	for( int32 Index = 0; Index < MAX_AUDIOCHANNELS; ++Index )
	{
		Mixer3DInputStatus[ Index ] = false;
	}
	
	for( int32 Index = 0; Index < MAX_MULTICHANNEL_AUDIOCHANNELS; ++Index )
	{
		MatrixMixerInputStatus[ Index ] = false;
	}
}

void FCoreAudioDevice::UpdateHardware()
{
	// Caches the matrix used to transform a sounds position into local space so we can just look
	// at the Y component after normalization to determine spatialization.
	const FVector Up = Listeners[0].GetUp();
	const FVector Right = Listeners[0].GetFront();
	InverseTransform = FMatrix(Up, Right, Up ^ Right, Listeners[0].Transform.GetTranslation()).Inverse();
}

FAudioEffectsManager* FCoreAudioDevice::CreateEffectsManager()
{
	// Create the effects subsystem (reverb, EQ, etc.)
	return new FCoreAudioEffectsManager(this);
}

FSoundSource* FCoreAudioDevice::CreateSoundSource()
{
	// create source source object
	return new FCoreAudioSoundSource(this);
}

void FCoreAudioDevice::SetupMatrixMixerInput( int32 Input, bool bIs6ChannelOGG )
{
	check( Input < MAX_MULTICHANNEL_AUDIOCHANNELS );
	
	uint32 InputOffset = Input * MatrixMixerInputFormat.mChannelsPerFrame;

	uint32 FLInputOffset	= 0;
	uint32 FRInputOffset	= 1;
	uint32 FCInputOffset	= 2;
	uint32 LFEInputOffset	= 3;
	uint32 SLInputOffset	= 4;
	uint32 SRInputOffset	= 5;

	// Channel ordering is different for 6 channel OGG files
	if (bIs6ChannelOGG)
	{
		FLInputOffset	= 0;
		FCInputOffset	= 1;
		FRInputOffset	= 2;
		SLInputOffset	= 3;
		SRInputOffset	= 4;
		LFEInputOffset	= 5;
	}

	// Enable input 0
	AudioUnitSetParameter( MatrixMixerUnit, kMatrixMixerParam_Enable, kAudioUnitScope_Input, Input, 1.0, 0 );
	
	// Set Matrix Input volume
	SetMatrixMixerInputVolume( Input, 1.0 );
	
	// FL channel to left output
	uint32 element = CalculateMatrixElement( InputOffset + FLInputOffset, 0 );
	AudioUnitSetParameter( MatrixMixerUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Global, element, 1.0, 0 );
	
	// FR channel to right output
	element = CalculateMatrixElement( InputOffset + FRInputOffset, 1 );
	AudioUnitSetParameter( MatrixMixerUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Global, element, 1.0, 0 );
	
	// FC channel to left & right output
	element = CalculateMatrixElement( InputOffset + FCInputOffset, 0 );
	AudioUnitSetParameter( MatrixMixerUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Global, element, 0.5, 0 );

	element = CalculateMatrixElement( InputOffset + FCInputOffset, 1 );
	AudioUnitSetParameter( MatrixMixerUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Global, element, 0.5, 0 );
	
	// LFE channel to left & right output
	element = CalculateMatrixElement( InputOffset + LFEInputOffset, 0 );
	AudioUnitSetParameter( MatrixMixerUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Global, element, 0.5, 0 );
	
	element = CalculateMatrixElement( InputOffset + LFEInputOffset, 1 );
	AudioUnitSetParameter( MatrixMixerUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Global, element, 0.5, 0 );
	
	// SL channel to left output
	element = CalculateMatrixElement( InputOffset + SLInputOffset, 0 );
	AudioUnitSetParameter( MatrixMixerUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Global, element, 1.0, 0 );
	
	// SR channel to right output
	element = CalculateMatrixElement( InputOffset + SRInputOffset, 1 );
	AudioUnitSetParameter( MatrixMixerUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Global, element, 1.0, 0 );
}

void FCoreAudioDevice::SetMatrixMixerInputVolume( int32 Input, float Volume )
{
	check( Input < MAX_MULTICHANNEL_AUDIOCHANNELS );
	
	uint32 InputOffset = Input * MatrixMixerInputFormat.mChannelsPerFrame;
	
	// Set Input channel 0 volume
	uint32 element = CalculateMatrixElement( InputOffset + 0, -1 );
	AudioUnitSetParameter( MatrixMixerUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Global, element, Volume, 0 );
	
	// Set Input channel 1 volume
	element = CalculateMatrixElement( InputOffset + 1, -1 );
	AudioUnitSetParameter( MatrixMixerUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Global, element, Volume, 0 );
	
	// Set Input channel 2 volume
	element = CalculateMatrixElement( InputOffset + 2, -1 );
	AudioUnitSetParameter( MatrixMixerUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Global, element, Volume, 0 );
	
	// Set Input channel 3 volume
	element = CalculateMatrixElement( InputOffset + 3, -1 );
	AudioUnitSetParameter( MatrixMixerUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Global, element, Volume, 0 );
	
	// Set Input channel 4 volume
	element = CalculateMatrixElement( InputOffset + 4, -1 );
	AudioUnitSetParameter( MatrixMixerUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Global, element, Volume, 0 );
	
	// Set Input channel 5 volume
	element = CalculateMatrixElement( InputOffset + 5, -1 );
	AudioUnitSetParameter( MatrixMixerUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Global, element, Volume, 0 );
}

int32 FCoreAudioDevice::GetFreeMixer3DInput()
{
	for( int32 Index = 0; Index < MAX_AUDIOCHANNELS; ++Index )
	{
		if( Mixer3DInputStatus[ Index ] == false )
		{
			Mixer3DInputStatus[ Index ] = true;
			return Index;
		}
	}
	
	return -1;
}

void FCoreAudioDevice::SetFreeMixer3DInput( int32 Input )
{
	Mixer3DInputStatus[ Input ] = false;
}

int32 FCoreAudioDevice::GetFreeMatrixMixerInput()
{
	for( int32 Index = 0; Index < MAX_MULTICHANNEL_AUDIOCHANNELS; ++Index )
	{
		if( MatrixMixerInputStatus[ Index ] == false )
		{
			MatrixMixerInputStatus[ Index ] = true;
			return Index;
		}
	}
	
	return -1;
}

void FCoreAudioDevice::SetFreeMatrixMixerInput( int32 Input )
{
	MatrixMixerInputStatus[ Input ] = false;
}

bool FCoreAudioDevice::HasCompressedAudioInfoClass(USoundWave* SoundWave)
{
#if WITH_OGGVORBIS
	return true;
#else
	return false;
#endif
}

class ICompressedAudioInfo* FCoreAudioDevice::CreateCompressedAudioInfo(USoundWave* SoundWave)
{
#if WITH_OGGVORBIS
	return new FVorbisAudioInfo();
#else
	return NULL;
#endif
}
