// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "SoundDefinitions.h"

/*-----------------------------------------------------------------------------
	USoundNodeDistanceCrossFade implementation.
-----------------------------------------------------------------------------*/
USoundNodeDistanceCrossFade::USoundNodeDistanceCrossFade(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

float USoundNodeDistanceCrossFade::MaxAudibleDistance( float CurrentMaxDistance )
{
	float Retval = 0.0f;

	for( int32 CrossFadeIndex = 0; CrossFadeIndex < CrossFadeInput.Num(); CrossFadeIndex++ )
	{
		float FadeInDistanceMax = CrossFadeInput[ CrossFadeIndex ].FadeInDistanceEnd;
		float FadeOutDistanceMax = CrossFadeInput[ CrossFadeIndex ].FadeOutDistanceEnd;

		if( FadeInDistanceMax > Retval )
		{
			Retval = FadeInDistanceMax;
		}

		if( FadeOutDistanceMax > Retval )
		{
			Retval = FadeOutDistanceMax;
		}
	}

	return( Retval );
}


void USoundNodeDistanceCrossFade::ParseNodes( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances )
{
	FSoundParseParameters UpdatedParams = ParseParams;
	for( int32 ChildNodeIndex = 0; ChildNodeIndex < ChildNodes.Num(); ChildNodeIndex++ )
	{
		if( ChildNodes[ ChildNodeIndex ] != NULL )
		{
			// get the various distances for this input so we can fade in/out the volume correctly
			const float FadeInDistanceMin = CrossFadeInput[ ChildNodeIndex ].FadeInDistanceStart;
			const float FadeInDistanceMax = CrossFadeInput[ ChildNodeIndex ].FadeInDistanceEnd;

			const float FadeOutDistanceMin = CrossFadeInput[ ChildNodeIndex ].FadeOutDistanceStart;
			const float FadeOutDistanceMax = CrossFadeInput[ ChildNodeIndex ].FadeOutDistanceEnd;

			// watch out here.  If one is playing the sound on the PlayerController then this will not update correctly as PlayerControllers don't move in normal play
			const float Distance = GetCurrentDistance(AudioDevice, ActiveSound, ParseParams);

			// determine the volume amount we should set the component to before "playing"
			float VolumeToSet = 1.0f;
			//UE_LOG(LogAudio, Log,  TEXT("  USoundNodeDistanceCrossFade.  Distance: %f ChildNodeIndex: %d CurrLoc: %s  ListenerLoc: %s"), Distance, ChildNodeIndex, *AudioComponent->CurrentLocation.ToString(), *AudioComponent->Listener->Location.ToString() );

			// Ignore distance calculations for preview components as they are undefined
			if( !ActiveSound.bLocationDefined )
			{
				VolumeToSet = CrossFadeInput[ ChildNodeIndex ].Volume;
			}
			else if( ( Distance >= FadeInDistanceMin ) && ( Distance <= FadeInDistanceMax ) )
			{
				VolumeToSet = (FadeInDistanceMax > 0.f ? CrossFadeInput[ ChildNodeIndex ].Volume * ( 0.0f + ( Distance - FadeInDistanceMin ) / ( FadeInDistanceMax - FadeInDistanceMin ) ) : 1.f);
				//UE_LOG(LogAudio, Log,  TEXT("     FadeIn.  Distance: %f,  VolumeToSet: %f"), Distance, VolumeToSet );
			}
			// else if we are inside the FadeOut edge
			else if( ( Distance >= FadeOutDistanceMin ) && ( Distance <= FadeOutDistanceMax ) )
			{
				VolumeToSet = (FadeInDistanceMax > 0.f ? CrossFadeInput[ ChildNodeIndex ].Volume * ( 1.0f - ( Distance - FadeOutDistanceMin ) / ( FadeOutDistanceMax - FadeOutDistanceMin ) ) : 0.f);
				//UE_LOG(LogAudio, Log,  TEXT("     FadeOut.  Distance: %f,  VolumeToSet: %f"), Distance, VolumeToSet );
			}
			// else we are in between the fading edges of the CrossFaded sound and we should play the
			// sound at the CrossFadeInput's specified volume
			else if( ( Distance >= FadeInDistanceMax ) && ( Distance <= FadeOutDistanceMin ) )
			{
				VolumeToSet = CrossFadeInput[ ChildNodeIndex ].Volume;
				//UE_LOG(LogAudio, Log,  TEXT("     In Between.  Distance: %f,  VolumeToSet: %f"), Distance, VolumeToSet );
			}
			// else we are outside of the range of this CrossFadeInput and should not play anything
			else
			{
				//UE_LOG(LogAudio, Log,  TEXT("     OUTSIDE!!!" ));
				VolumeToSet = 0.f; //CrossFadeInput( ChildNodeIndex ).Volume;
			}

			UpdatedParams.Volume = ParseParams.Volume * VolumeToSet;

			// "play" the rest of the tree
			ChildNodes[ ChildNodeIndex ]->ParseNodes( AudioDevice, GetNodeWaveInstanceHash(NodeWaveInstanceHash, ChildNodes[ChildNodeIndex], ChildNodeIndex), ActiveSound, UpdatedParams, WaveInstances );
		}
	}
}


void USoundNodeDistanceCrossFade::CreateStartingConnectors()
{
	// Mixers default with two connectors.
	InsertChildNode( ChildNodes.Num() );
	InsertChildNode( ChildNodes.Num() );
}


void USoundNodeDistanceCrossFade::InsertChildNode( int32 Index )
{
	Super::InsertChildNode( Index );
	CrossFadeInput.InsertZeroed( Index );

	CrossFadeInput[ Index ].Volume = 1.0f;
}


void USoundNodeDistanceCrossFade::RemoveChildNode( int32 Index )
{
	Super::RemoveChildNode( Index );
	CrossFadeInput.RemoveAt( Index );
}

#if WITH_EDITOR
void USoundNodeDistanceCrossFade::SetChildNodes(TArray<USoundNode*>& InChildNodes)
{
	Super::SetChildNodes(InChildNodes);

	if (CrossFadeInput.Num() < ChildNodes.Num())
	{
		const int32 OldSize = CrossFadeInput.Num();
		const int32 NumToAdd = ChildNodes.Num() - OldSize;
		CrossFadeInput.AddZeroed(NumToAdd);
		for (int32 NewIndex = OldSize; NewIndex < CrossFadeInput.Num(); ++NewIndex)
		{
			CrossFadeInput[NewIndex].Volume = 1.0f;
		}
	}
	else if (CrossFadeInput.Num() > ChildNodes.Num())
	{
		const int32 NumToRemove = CrossFadeInput.Num() - ChildNodes.Num();
		CrossFadeInput.RemoveAt(CrossFadeInput.Num() - NumToRemove, NumToRemove);
	}
}
#endif //WITH_EDITOR

float USoundNodeDistanceCrossFade::GetCurrentDistance(FAudioDevice* AudioDevice, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams) const
{
	return ActiveSound.bLocationDefined ? FVector::Dist( ParseParams.Transform.GetTranslation(), AudioDevice->Listeners[0].Transform.GetTranslation() ) : 0.f;
}

FString USoundNodeDistanceCrossFade::GetUniqueString() const
{
	return TEXT( "DistanceCrossFadeComplex/" );
}
