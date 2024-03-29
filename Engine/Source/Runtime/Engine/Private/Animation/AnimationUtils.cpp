// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimationUtils.cpp: Skeletal mesh animation utilities.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "AnimationUtils.h"
#include "AnimationCompression.h"
#include "AnimEncoding.h"

/** Array to keep track of SkeletalMeshes we have built metadata for, and log out the results just once. */
//static TArray<USkeleton*> UniqueSkeletonsMetadataArray;

void FAnimationUtils::BuildSkeletonMetaData(USkeleton * Skeleton, TArray<FBoneData>& OutBoneData)
{
	// Disable logging by default. Except if we deal with a new Skeleton. Then we log out its details. (just once).
	bool bEnableLogging = false;
// Uncomment to enable.
// 	if( UniqueSkeletonsMetadataArray.FindItemIndex(Skeleton) == INDEX_NONE )
// 	{
// 		bEnableLogging = true;
// 		UniqueSkeletonsMetadataArray.AddItem(Skeleton);
// 	}

	const FReferenceSkeleton & RefSkeleton = Skeleton->GetReferenceSkeleton();
	const TArray<FTransform> & SkeletonRefPose = Skeleton->GetRefLocalPoses();
	const int32 NumBones = RefSkeleton.GetNum();

	// Assemble bone data.
	OutBoneData.Empty();
	OutBoneData.AddZeroed( NumBones );

	TArray<FString> KeyEndEffectorsMatchNameArray;
	GConfig->GetArray( TEXT("AnimationCompression"), TEXT("KeyEndEffectorsMatchName"), KeyEndEffectorsMatchNameArray, GEngineIni );

	for (int32 BoneIndex = 0 ; BoneIndex<NumBones; ++BoneIndex )
	{
		FBoneData& BoneData = OutBoneData[BoneIndex];

		// Copy over data from the skeleton.
		const FTransform& SrcTransform = SkeletonRefPose[BoneIndex];

		BoneData.Orientation = SrcTransform.GetRotation();
		BoneData.Position = SrcTransform.GetTranslation();
		BoneData.Name = RefSkeleton.GetBoneName(BoneIndex);

		if ( BoneIndex > 0 )
		{
			// Compute ancestry.
			int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
			BoneData.BonesToRoot.Add( ParentIndex );
			while ( ParentIndex > 0 )
			{
				ParentIndex = RefSkeleton.GetParentIndex(ParentIndex);
				BoneData.BonesToRoot.Add( ParentIndex );
			}
		}

		// See if a Socket is attached to that bone
		BoneData.bHasSocket = false;
		// @todo anim: socket isn't moved to Skeleton yet, but this code needs better testing
		for(int32 SocketIndex=0; SocketIndex<Skeleton->Sockets.Num(); SocketIndex++)
		{
			USkeletalMeshSocket* Socket = Skeleton->Sockets[SocketIndex];
			if( Socket && Socket->BoneName == RefSkeleton.GetBoneName(BoneIndex) )
			{
				BoneData.bHasSocket = true;
				break;
			}
		}
	}

	// Enumerate children (bones that refer to this bone as parent).
	for ( int32 BoneIndex = 0 ; BoneIndex < OutBoneData.Num() ; ++BoneIndex )
	{
		FBoneData& BoneData = OutBoneData[BoneIndex];
		// Exclude the root bone as it is the child of nothing.
		for ( int32 BoneIndex2 = 1 ; BoneIndex2 < OutBoneData.Num() ; ++BoneIndex2 )
		{
			if ( OutBoneData[BoneIndex2].GetParent() == BoneIndex )
			{
				BoneData.Children.Add(BoneIndex2);
			}
		}
	}

	// Enumerate end effectors.  For each end effector, propagate its index up to all ancestors.
	if( bEnableLogging )
	{
		UE_LOG(LogAnimation, Warning, TEXT("Enumerate End Effectors for %s"), *Skeleton->GetFName().ToString());
	}
	for ( int32 BoneIndex = 0 ; BoneIndex < OutBoneData.Num() ; ++BoneIndex )
	{
		FBoneData& BoneData = OutBoneData[BoneIndex];
		if ( BoneData.IsEndEffector() )
		{
			// End effectors have themselves as an ancestor.
			BoneData.EndEffectors.Add( BoneIndex );
			// Add the end effector to the list of end effectors of all ancestors.
			for ( int32 i = 0 ; i < BoneData.BonesToRoot.Num() ; ++i )
			{
				const int32 AncestorIndex = BoneData.BonesToRoot[i];
				OutBoneData[AncestorIndex].EndEffectors.Add( BoneIndex );
			}

			for(int32 MatchIndex=0; MatchIndex<KeyEndEffectorsMatchNameArray.Num(); MatchIndex++)
			{
				// See if this bone has been defined as a 'key' end effector
				FString BoneString(BoneData.Name.ToString());
				if( BoneString.Contains(KeyEndEffectorsMatchNameArray[MatchIndex]) )
				{
					BoneData.bKeyEndEffector = true;
					break;
				}
			}
			if( bEnableLogging )
			{
				UE_LOG(LogAnimation, Warning, TEXT("\t %s bKeyEndEffector: %d"), *BoneData.Name.ToString(), BoneData.bKeyEndEffector);
			}
		}
	}
#if 0
	UE_LOG(LogAnimation, Log,  TEXT("====END EFFECTORS:") );
	int32 NumEndEffectors = 0;
	for ( int32 BoneIndex = 0 ; BoneIndex < OutBoneData.Num() ; ++BoneIndex )
	{
		const FBoneData& BoneData = OutBoneData(BoneIndex);
		if ( BoneData.IsEndEffector() )
		{
			FString Message( FString::Printf(TEXT("%s(%i): "), *BoneData.Name, BoneData.GetDepth()) );
			for ( int32 i = 0 ; i < BoneData.BonesToRoot.Num() ; ++i )
			{
				const int32 AncestorIndex = BoneData.BonesToRoot(i);
				Message += FString::Printf( TEXT("%s "), *OutBoneData(AncestorIndex).Name );
			}
			UE_LOG(LogAnimation, Log,  *Message );
			++NumEndEffectors;
		}
	}
	UE_LOG(LogAnimation, Log,  TEXT("====NUM EFFECTORS %i(%i)"), NumEndEffectors, OutBoneData(0).Children.Num() );
	UE_LOG(LogAnimation, Log,  TEXT("====NON END EFFECTORS:") );
	for ( int32 BoneIndex = 0 ; BoneIndex < OutBoneData.Num() ; ++BoneIndex )
	{
		const FBoneData& BoneData = OutBoneData(BoneIndex);
		if ( !BoneData.IsEndEffector() )
		{
			FString Message( FString::Printf(TEXT("%s(%i): "), *BoneData.Name, BoneData.GetDepth()) );
			Message += TEXT("Children: ");
			for ( int32 i = 0 ; i < BoneData.Children.Num() ; ++i )
			{
				const int32 ChildIndex = BoneData.Children(i);
				Message += FString::Printf( TEXT("%s "), *OutBoneData(ChildIndex).Name );
			}
			Message += TEXT("  EndEffectors: ");
			for ( int32 i = 0 ; i < BoneData.EndEffectors.Num() ; ++i )
			{
				const int32 EndEffectorIndex = BoneData.EndEffectors(i);
				Message += FString::Printf( TEXT("%s "), *OutBoneData(EndEffectorIndex).Name );
				check( OutBoneData(EndEffectorIndex).IsEndEffector() );
			}
			UE_LOG(LogAnimation, Log,  *Message );
		}
	}
	UE_LOG(LogAnimation, Log,  TEXT("===================") );
#endif
}

/**
* Builds the local-to-component matrix for the specified bone.
*/
void FAnimationUtils::BuildComponentSpaceTransform(FTransform& OutTransform,
											   int32 BoneIndex,
											   const TArray<FTransform>& LocalAtoms,
											   const TArray<FBoneData>& BoneData)
{
	// Put root-to-component in OutTransform.
	OutTransform = LocalAtoms[0];

	if ( BoneIndex > 0 )
	{
		const FBoneData& Bone = BoneData[BoneIndex];

		checkSlow( Bone.BonesToRoot.Num()-1 == 0 );

		// Compose BoneData.BonesToRoot down.
		for ( int32 i = Bone.BonesToRoot.Num()-2 ; i >=0 ; --i )
		{
			const int32 AncestorIndex = Bone.BonesToRoot[i];
			OutTransform = LocalAtoms[AncestorIndex]*OutTransform;
		}

		// Finally, include the bone's local-to-parent.
		OutTransform = LocalAtoms[BoneIndex]*OutTransform;
	}
}

/**
 * Utility function to measure the accuracy of a compressed animation. Each end-effector is checked for 
 * world-space movement as a result of compression.
 *
 * @param	AnimSet		The animset to calculate compression error for.
 * @param	BoneData	BoneData array describing the hierarchy of the animated skeleton
 * @param	ErrorStats	Output structure containing the final compression error values
 * @return				None.
 */
void FAnimationUtils::ComputeCompressionError(const UAnimSequence* AnimSeq, const TArray<FBoneData>& BoneData, AnimationErrorStats& ErrorStats)
{
	ErrorStats.AverageError = 0.0f;
	ErrorStats.MaxError = 0.0f;
	ErrorStats.MaxErrorBone = 0;
	ErrorStats.MaxErrorTime = 0.0f;
	int32 MaxErrorTrack = -1;

	if (AnimSeq->NumFrames > 0)
	{
		const float TimeStep = (float)AnimSeq->SequenceLength/(float)AnimSeq->NumFrames;
		const int32 NumBones = BoneData.Num();
		
		float ErrorCount = 0.0f;
		float ErrorTotal = 0.0f;

		USkeleton * Skeleton = AnimSeq->GetSkeleton();
		check ( Skeleton );

		const TArray<FTransform>& RefPose	= Skeleton->GetRefLocalPoses();

		TArray<FTransform> RawAtoms;
		TArray<FTransform> NewAtoms;
		TArray<FTransform> RawTransforms;
		TArray<FTransform> NewTransforms;

		RawAtoms.AddZeroed(NumBones);
		NewAtoms.AddZeroed(NumBones);
		RawTransforms.AddZeroed(NumBones);
		NewTransforms.AddZeroed(NumBones);

		FTransform const DummyBone(FQuat::Identity, FVector(END_EFFECTOR_SOCKET_DUMMY_BONE_SIZE, END_EFFECTOR_SOCKET_DUMMY_BONE_SIZE, END_EFFECTOR_SOCKET_DUMMY_BONE_SIZE));

		// for each whole increment of time (frame stepping)
		for( float Time = 0.0f; Time < AnimSeq->SequenceLength; Time+= TimeStep )
		{
			// get the raw and compressed atom for each bone
			for( int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex )
			{
				int32 const TrackIndex = Skeleton->GetAnimationTrackIndex(BoneIndex, AnimSeq);

				if( TrackIndex == INDEX_NONE )
				{
					// No track for the bone was found, so use the reference pose.
					RawAtoms[BoneIndex]	= RefPose[BoneIndex];
					NewAtoms[BoneIndex] = RawAtoms[BoneIndex];
				}
				else
				{
					AnimSeq->GetBoneTransform(RawAtoms[BoneIndex], TrackIndex, Time, false, true);
					AnimSeq->GetBoneTransform(NewAtoms[BoneIndex], TrackIndex, Time, false, false);


					bool bSkipTranslationTrack = false;
					// If we forcibly reduced the translation track to one key, make sure we don't introduce error if it was animated previously.
					// So short-circuit RAW data for error measuring past that first key.
					bool bReducedTranslationTrack = false;

					// If we don't care about this translation track, because it's going to get skipped, then use RefSkel translation for error measurement.
#if( SKIP_FORCEMESHTRANSLATION_TRACKS || SKIP_ANIMROTATIONONLY_TRACKS || REDUCE_ANIMROTATIONONLY_TRACKS )		
					const bool bUseRefPoseTranslation = (Skeleton->GetBoneTranslationRetargetingMode(BoneIndex) == EBoneTranslationRetargetingMode::Skeleton);
	#if( SKIP_FORCEMESHTRANSLATION_TRACKS || SKIP_ANIMROTATIONONLY_TRACKS )	
					bSkipTranslationTrack = bUseRefPoseTranslation;
	#endif
	#if( REDUCE_ANIMROTATIONONLY_TRACKS )
					bReducedTranslationTrack = bUseRefPoseTranslation && (Time > 0.f);
	#endif

#endif
					// bAnimRotationOnly tracks - ignore translation data always use Ref Skeleton.
					if( bSkipTranslationTrack || bReducedTranslationTrack )
					{
						RawAtoms[BoneIndex].SetTranslation( RefPose[BoneIndex].GetTranslation() );
						NewAtoms[BoneIndex].SetTranslation( RefPose[BoneIndex].GetTranslation() );
					}
				}

				RawTransforms[BoneIndex] = RawAtoms[BoneIndex];
				NewTransforms[BoneIndex] = NewAtoms[BoneIndex];

				// For all bones below the root, final component-space transform is relative transform * component-space transform of parent.
				if( BoneIndex > 0 )
				{
					const int32 ParentIndex = Skeleton->GetReferenceSkeleton().GetParentIndex(BoneIndex);

					// Check the precondition that parents occur before children in the RequiredBones array.
					check( ParentIndex != INDEX_NONE );
					check( ParentIndex < BoneIndex );

					RawTransforms[BoneIndex] *= RawTransforms[ParentIndex];
					NewTransforms[BoneIndex] *= NewTransforms[ParentIndex];
				}
				
				if( BoneData[BoneIndex].IsEndEffector() )
				{
					// If this is an EndEffector with a Socket attached to it, add an extra bone, to measure error introduced by effector rotation compression.
					if( BoneData[BoneIndex].bHasSocket || BoneData[BoneIndex].bKeyEndEffector )
					{
						RawTransforms[BoneIndex] = DummyBone * RawTransforms[BoneIndex];
						NewTransforms[BoneIndex] = DummyBone * NewTransforms[BoneIndex];
					}

					float Error = (RawTransforms[BoneIndex].GetLocation() - NewTransforms[BoneIndex].GetLocation()).Size();

					ErrorTotal += Error;
					ErrorCount += 1.0f;

					if( Error > ErrorStats.MaxError )
					{
						ErrorStats.MaxError		= Error;
						ErrorStats.MaxErrorBone = BoneIndex;
						MaxErrorTrack = TrackIndex;
						ErrorStats.MaxErrorTime = Time;
					}
				}
			}
		}

		if (ErrorCount > 0.0f)
		{
			ErrorStats.AverageError = ErrorTotal / ErrorCount;
		}

#if 0		
		// That's a big error, log out some information!
 		if( ErrorStats.MaxError > 10.f )
 		{
			UE_LOG(LogAnimation, Log, TEXT("!!! Big error found: %f, Time: %f, BoneIndex: %d, Track: %d, CompressionScheme: %s, additive: %d"), 
 				ErrorStats.MaxError,
				ErrorStats.MaxErrorTime,
				ErrorStats.MaxErrorBone,
				MaxErrorTrack,
				AnimSeq->CompressionScheme ? *AnimSeq->CompressionScheme->GetFName().ToString() : TEXT("NULL"),
				AnimSeq->bIsAdditive );
 			UE_LOG(LogAnimation, Log, TEXT("   RawOrigin: %s, NormalOrigin: %s"), *RawTransforms(ErrorStats.MaxErrorBone).GetOrigin().ToString(), *NewTransforms(ErrorStats.MaxErrorBone).GetOrigin().ToString());
 
 			// We shouldn't have a big error with no compression.
 			check( AnimSeq->CompressionScheme != NULL );
 		}	
#endif
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Default animation compression algorithm.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {

/**
 * @return		A new instance of the default animation compression algorithm singleton, attached to the root set.
 */
static inline UAnimCompress* ConstructDefaultCompressionAlgorithm()
{
	// Algorithm.
	FString DefaultCompressionAlgorithm( UAnimCompress_BitwiseCompressOnly::StaticClass()->GetName() );
	GConfig->GetString( TEXT("AnimationCompression"), TEXT("DefaultCompressionAlgorithm"), DefaultCompressionAlgorithm, GEngineIni );

	// Rotation compression format.
	AnimationCompressionFormat RotationCompressionFormat = ACF_Float96NoW;
	GConfig->GetInt( TEXT("AnimationCompression"), TEXT("RotationCompressionFormat"), (int32&)RotationCompressionFormat, GEngineIni );
	RotationCompressionFormat = FMath::Clamp( RotationCompressionFormat, ACF_None, (AnimationCompressionFormat)(ACF_MAX-1) );

	// Translation compression format.
	AnimationCompressionFormat TranslationCompressionFormat = ACF_None;
	GConfig->GetInt( TEXT("AnimationCompression"), TEXT("TranslationCompressionFormat"), (int32&)TranslationCompressionFormat, GEngineIni );
	TranslationCompressionFormat = FMath::Clamp( TranslationCompressionFormat, ACF_None, (AnimationCompressionFormat)(ACF_MAX-1) );

	// Find a class that inherits
	UClass* CompressionAlgorithmClass = NULL;
	for ( TObjectIterator<UClass> It ; It ; ++It )
	{
		UClass* Class = *It;
		if( !Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated) )
		{
			if ( Class->IsChildOf(UAnimCompress::StaticClass()) && DefaultCompressionAlgorithm == Class->GetName() )
			{
				CompressionAlgorithmClass = Class;
				break;

			}
		}
	}

	if ( !CompressionAlgorithmClass )
	{
		UE_LOG(LogAnimation, Fatal, TEXT("Couldn't find animation compression algorithm named %s"), *DefaultCompressionAlgorithm );
	}

	UAnimCompress* NewAlgorithm = ConstructObject<UAnimCompress>( CompressionAlgorithmClass );
	NewAlgorithm->RotationCompressionFormat = RotationCompressionFormat;
	NewAlgorithm->TranslationCompressionFormat = TranslationCompressionFormat;
	NewAlgorithm->AddToRoot();
	return NewAlgorithm;
}

} // namespace

/**
 * @return		The default animation compression algorithm singleton, instantiating it if necessary.
 */
UAnimCompress* FAnimationUtils::GetDefaultAnimationCompressionAlgorithm()
{
	static UAnimCompress* SAlgorithm = ConstructDefaultCompressionAlgorithm();
	return SAlgorithm;
}

/**
 * Determines the current setting for world-space error tolerance in the animation compressor.
 * When requested, animation being compressed will also consider an alternative compression
 * method if the end result of that method produces less error than the AlternativeCompressionThreshold.
 * The default tolerance value is 0.0f (no alternatives allowed) but may be overridden using a field in the base engine INI file.
 *
 * @return				World-space error tolerance for considering an alternative compression method
 */
float FAnimationUtils::GetAlternativeCompressionThreshold()
{
	// Allow the Engine INI file to provide a new override
	float AlternativeCompressionThreshold = 0.0f;
	GConfig->GetFloat( TEXT("AnimationCompression"), TEXT("AlternativeCompressionThreshold"), (float&)AlternativeCompressionThreshold, GEngineIni );

	return AlternativeCompressionThreshold;
}

/**
 * Determines the current setting for recompressing all animations upon load. The default value 
 * is False, but may be overridden by an optional field in the base engine INI file. 
 *
 * @return				true if the engine settings request that all animations be recompiled
 */
bool FAnimationUtils::GetForcedRecompressionSetting()
{
	// Allow the Engine INI file to provide a new override
	bool ForcedRecompressionSetting = false;
	GConfig->GetBool( TEXT("AnimationCompression"), TEXT("ForceRecompression"), (bool&)ForcedRecompressionSetting, GEngineIni );

	return ForcedRecompressionSetting;
}

#if !WITH_EDITOR
	#define TRYCOMPRESSION_INNER(compressionname,winningcompressor_count,winningcompressor_error,winningcompressor_margin,compressionalgorithm)	
#else
	#define TRYCOMPRESSION_INNER(compressionname,winningcompressor_count,winningcompressor_error,winningcompressor_margin,compressionalgorithm)				\
{																																							\
	/* try the alternative compressor	*/																													\
	(compressionalgorithm)->Reduce( AnimSeq, bOutput );																				\
	const SIZE_T NewSize = AnimSeq->GetResourceSize(EResourceSizeMode::Exclusive);																												\
																																							\
	/* compute the savings and compression error*/																											\
	const SIZE_T MemorySavingsFromOriginal = OriginalSize - NewSize;																									\
	const SIZE_T MemorySavingsFromPrevious = CurrentSize - NewSize;																									\
	PctSaving = 0.f;																																		\
	/* figure out our new compression error*/																												\
	FAnimationUtils::ComputeCompressionError(AnimSeq, BoneData, NewErrorStats);														\
																																							\
	const bool bLowersError = NewErrorStats.MaxError < WinningCompressorError;																				\
	const bool bErrorUnderThreshold = NewErrorStats.MaxError <= MasterTolerance;																			\
																																							\
	/* keep it if it we want to force the error below the threshold and it reduces error */																	\
	bKeepNewCompressionMethod = false;																												\
	bKeepNewCompressionMethod |= (bLowersError && (WinningCompressorError > MasterTolerance) && bForceBelowThreshold);										\
	/* or if has an acceptable error and saves space  */																									\
	bKeepNewCompressionMethod |= bErrorUnderThreshold && (MemorySavingsFromPrevious > 0);																	\
	/* or if saves the same amount and an acceptable error that is lower than the previous best */															\
	bKeepNewCompressionMethod |= bErrorUnderThreshold && bLowersError && (MemorySavingsFromPrevious >= 0);													\
																																							\
	if (bKeepNewCompressionMethod)																															\
	{																																						\
		WinningCompressorMarginalSavings = MemorySavingsFromPrevious;																						\
		WinningCompressorCounter = &(winningcompressor_count);																							\
		WinningCompressorErrorSum = &(winningcompressor_error);																							\
		WinningCompressorMarginalSavingsSum = &(winningcompressor_margin);																				\
		WinningCompressorName = compressionname;																											\
		CurrentSize = NewSize;																																\
		WinningCompressorSavings = MemorySavingsFromOriginal;																								\
		WinningCompressorError = NewErrorStats.MaxError;																									\
	}																																						\
																																							\
	PctSaving = OriginalSize > 0 ? 100.f - (100.f * float(NewSize) / float(OriginalSize)) : 0.f;															\
	UE_LOG(LogAnimation, Warning, TEXT("- %s - bytes saved: %i (%3.1f%% saved), maxdiff: %f %s"),																					\
	compressionname, MemorySavingsFromOriginal, PctSaving, NewErrorStats.MaxError, bKeepNewCompressionMethod ? TEXT("(**Best so far**)") : TEXT(""));		\
																																							\
	if( !bKeepNewCompressionMethod )																														\
	{																																						\
		/* revert back to the old method by copying back the data we cached */																				\
		AnimSeq->TranslationData = SavedTranslationData;																									\
		AnimSeq->RotationData = SavedRotationData;																											\
		AnimSeq->CompressionScheme = SavedCompressionScheme;																								\
		AnimSeq->TranslationCompressionFormat = SavedTranslationCompressionFormat;																			\
		AnimSeq->RotationCompressionFormat = SavedRotationCompressionFormat;																				\
		AnimSeq->KeyEncodingFormat = SavedKeyEncodingFormat;																								\
		AnimSeq->CompressedTrackOffsets = SavedCompressedTrackOffsets;																						\
		AnimSeq->CompressedByteStream = SavedCompressedByteStream;																							\
		AnimSeq->CompressedScaleOffsets = SavedCompressedScaleOffsets;																						\
		AnimSeq->TranslationCodec = SavedTranslationCodec;																									\
		AnimSeq->RotationCodec = SavedRotationCodec;																										\
		AnimSeq->ScaleCodec = SavedScaleCodec;																												\
		AnimationFormat_SetInterfaceLinks(*AnimSeq);																										\
																																							\
		const SIZE_T RestoredSize = AnimSeq->GetResourceSize(EResourceSizeMode::Exclusive);																										\
		check(RestoredSize == CurrentSize);																													\
	}																																						\
	else																																					\
	{																																						\
		/* backup key information from the sequence */																										\
		SavedTranslationData				= AnimSeq->TranslationData;																						\
		SavedRotationData					= AnimSeq->RotationData;																						\
		SavedCompressionScheme				= AnimSeq->CompressionScheme;																					\
		SavedTranslationCompressionFormat	= AnimSeq->TranslationCompressionFormat;																		\
		SavedRotationCompressionFormat		= AnimSeq->RotationCompressionFormat;																			\
		SavedKeyEncodingFormat				= AnimSeq->KeyEncodingFormat;																					\
		SavedCompressedTrackOffsets			= AnimSeq->CompressedTrackOffsets;																				\
		SavedCompressedScaleOffsets			= AnimSeq->CompressedScaleOffsets;																				\
		SavedCompressedByteStream			= AnimSeq->CompressedByteStream;																				\
		SavedTranslationCodec				= AnimSeq->TranslationCodec;																					\
		SavedRotationCodec					= AnimSeq->RotationCodec;																						\
		SavedScaleCodec						= AnimSeq->ScaleCodec;																							\
	}																																						\
}
#endif

#define TRYCOMPRESSION(Name, CompressionAlgorithm) TRYCOMPRESSION_INNER(TEXT(#Name), Name ## CompressorWins, Name ## CompressorSumError, Name ## CompressorWinMargin, CompressionAlgorithm)

#define WARN_COMPRESSION_STATUS(Name) \
	UE_LOG(LogAnimation, Warning, TEXT("\t\tWins for '%32s': %4i\t\t%f\t%i bytes"), TEXT(#Name), Name ## CompressorWins, (Name ## CompressorWins > 0) ? Name ## CompressorSumError / Name ## CompressorWins : 0.0f, Name ## CompressorWinMargin)

#define DECLARE_ANIM_COMP_ALGORITHM(Algorithm) \
	static int32 Algorithm ## CompressorWins = 0; \
	static float Algorithm ## CompressorSumError = 0.0f; \
	static int32 Algorithm ## CompressorWinMargin = 0

/** Control animation recompression upon load. */
bool GDisableAnimationRecompression	= false;

/**
 * Utility function to compress an animation. If the animation is currently associated with a codec, it will be used to 
 * compress the animation. Otherwise, the default codec will be used. If AllowAlternateCompressor is true, an
 * alternative compression codec will also be tested. If the alternative codec produces better compression and 
 * the accuracy of the compressed animation remains within tolerances, the alternative codec will be used. 
 * See GetAlternativeCompressionThreshold for information on the tolerance value used.
 *
 * @param	AnimSet		The animset to compress.
 * @param	AllowAlternateCompressor	true if an alternative compressor is permitted.
 * @param	bOutput		If false don't generate output or compute memory savings.
 * @return				None.
 */
void FAnimationUtils::CompressAnimSequence(UAnimSequence* AnimSeq,  bool bAllowAlternateCompressor, bool bOutput)
{
	if (FPlatformProperties::HasEditorOnlyData())
	{
		// the underlying code won't work right without skeleton. 
		if ( !AnimSeq->GetSkeleton() )
		{
			return;
		}

		// get the master tolerance we will use to guide recompression
		float MasterTolerance = GetAlternativeCompressionThreshold(); 

		bool bOnlyCheckForMissingSkeletalMeshes = false;
		GConfig->GetBool( TEXT("AnimationCompression"), TEXT("bOnlyCheckForMissingSkeletalMeshes"), (bool&)bOnlyCheckForMissingSkeletalMeshes, GEngineIni );

		if (bOnlyCheckForMissingSkeletalMeshes)
		{
			TestForMissingMeshes(AnimSeq);
		}
		else
		{
			bool bForceBelowThreshold = false;
			bool bFirstRecompressUsingCurrentOrDefault = true;
			bool bRaiseMaxErrorToExisting = false;
			GConfig->GetBool( TEXT("AnimationCompression"), TEXT("bForceBelowThreshold"), (bool&)bForceBelowThreshold, GEngineIni );
			GConfig->GetBool( TEXT("AnimationCompression"), TEXT("bFirstRecompressUsingCurrentOrDefault"), (bool&)bFirstRecompressUsingCurrentOrDefault, GEngineIni );
			// If we don't allow alternate compressors, and just want to recompress with default/existing, then make sure we do so.
			if( !bAllowAlternateCompressor )
			{
				bFirstRecompressUsingCurrentOrDefault = true;
			}
			GConfig->GetBool( TEXT("AnimationCompression"), TEXT("bRaiseMaxErrorToExisting"), (bool&)bRaiseMaxErrorToExisting, GEngineIni );

			bool bTryFixedBitwiseCompression = true;
			bool bTryPerTrackBitwiseCompression = true;
			bool bTryLinearKeyRemovalCompression = true;
			bool bTryIntervalKeyRemoval = true;
			GConfig->GetBool( TEXT("AnimationCompression"), TEXT("bTryFixedBitwiseCompression"), bTryFixedBitwiseCompression, GEngineIni );
			GConfig->GetBool( TEXT("AnimationCompression"), TEXT("bTryPerTrackBitwiseCompression"), bTryPerTrackBitwiseCompression, GEngineIni );
			GConfig->GetBool( TEXT("AnimationCompression"), TEXT("bTryLinearKeyRemovalCompression"), bTryLinearKeyRemovalCompression, GEngineIni );
			GConfig->GetBool( TEXT("AnimationCompression"), TEXT("bTryIntervalKeyRemoval"), bTryIntervalKeyRemoval, GEngineIni );

			CompressAnimSequenceExplicit(
				AnimSeq,
				bAllowAlternateCompressor ? MasterTolerance : 0.0f,
				bOutput,
				bFirstRecompressUsingCurrentOrDefault,
				bForceBelowThreshold,
				bRaiseMaxErrorToExisting,
				bTryFixedBitwiseCompression,
				bTryPerTrackBitwiseCompression,
				bTryLinearKeyRemovalCompression,
				bTryIntervalKeyRemoval);
		}
	}
}

/**
 * Utility function to compress an animation. If the animation is currently associated with a codec, it will be used to 
 * compress the animation. Otherwise, the default codec will be used. If AllowAlternateCompressor is true, an
 * alternative compression codec will also be tested. If the alternative codec produces better compression and 
 * the accuracy of the compressed animation remains within tolerances, the alternative codec will be used. 
 * See GetAlternativeCompressionThreshold for information on the tolerance value used.
 *
 * @param	AnimSet		The animset to compress.
 * @param	MasterTolerance	The alternate error threshold (0.0 means don't try anything other than the current / default scheme)
 * @param	bOutput		If false don't generate output or compute memory savings.
 * @return				None.
 */
void FAnimationUtils::CompressAnimSequenceExplicit(
	UAnimSequence* AnimSeq,
	float MasterTolerance,
	bool bOutput,
	bool bFirstRecompressUsingCurrentOrDefault,
	bool bForceBelowThreshold,
	bool bRaiseMaxErrorToExisting,
	bool bTryFixedBitwiseCompression,
	bool bTryPerTrackBitwiseCompression,
	bool bTryLinearKeyRemovalCompression,
	bool bTryIntervalKeyRemoval)
{
#if WITH_EDITORONLY_DATA
	if( GDisableAnimationRecompression )
	{
		return;
	}

	DECLARE_ANIM_COMP_ALGORITHM(BitwiseACF_Float96);
	DECLARE_ANIM_COMP_ALGORITHM(BitwiseACF_Fixed48);
	DECLARE_ANIM_COMP_ALGORITHM(BitwiseACF_IntervalFixed32);
	DECLARE_ANIM_COMP_ALGORITHM(BitwiseACF_Fixed32);

	DECLARE_ANIM_COMP_ALGORITHM(HalfOddACF_Float96);
	DECLARE_ANIM_COMP_ALGORITHM(HalfOddACF_Fixed48);
	DECLARE_ANIM_COMP_ALGORITHM(HalfOddACF_IntervalFixed32);
	DECLARE_ANIM_COMP_ALGORITHM(HalfOddACF_Fixed32);

	DECLARE_ANIM_COMP_ALGORITHM(HalfEvenACF_Float96);
	DECLARE_ANIM_COMP_ALGORITHM(HalfEvenACF_Fixed48);
	DECLARE_ANIM_COMP_ALGORITHM(HalfEvenACF_IntervalFixed32);
	DECLARE_ANIM_COMP_ALGORITHM(HalfEvenACF_Fixed32);

	DECLARE_ANIM_COMP_ALGORITHM(LinearACF_Float96);
	DECLARE_ANIM_COMP_ALGORITHM(LinearACF_Fixed48);
	DECLARE_ANIM_COMP_ALGORITHM(LinearACF_IntervalFixed32);
	DECLARE_ANIM_COMP_ALGORITHM(LinearACF_Fixed32);

	DECLARE_ANIM_COMP_ALGORITHM(Progressive_PerTrack);
	DECLARE_ANIM_COMP_ALGORITHM(Bitwise_PerTrack);
	DECLARE_ANIM_COMP_ALGORITHM(Linear_PerTrack);
	DECLARE_ANIM_COMP_ALGORITHM(Adaptive1_LinPerTrack);
	DECLARE_ANIM_COMP_ALGORITHM(Adaptive1_LinPerTrackNoRT);

	DECLARE_ANIM_COMP_ALGORITHM(Adaptive2_PerTrack);
	DECLARE_ANIM_COMP_ALGORITHM(Adaptive2_LinPerTrack);
	DECLARE_ANIM_COMP_ALGORITHM(Adaptive2_LinPerTrackNoRT);

	DECLARE_ANIM_COMP_ALGORITHM(Downsample20Hz_PerTrack);
	DECLARE_ANIM_COMP_ALGORITHM(Downsample15Hz_PerTrack);
	DECLARE_ANIM_COMP_ALGORITHM(Downsample10Hz_PerTrack);
	DECLARE_ANIM_COMP_ALGORITHM(Downsample5Hz_PerTrack);

	DECLARE_ANIM_COMP_ALGORITHM(Adaptive1_15Hz_LinPerTrack);
	DECLARE_ANIM_COMP_ALGORITHM(Adaptive1_10Hz_LinPerTrack);
	DECLARE_ANIM_COMP_ALGORITHM(Adaptive1_5Hz_LinPerTrack);

	DECLARE_ANIM_COMP_ALGORITHM(Adaptive2_15Hz_LinPerTrack);
	DECLARE_ANIM_COMP_ALGORITHM(Adaptive2_10Hz_LinPerTrack);

	DECLARE_ANIM_COMP_ALGORITHM(Linear_PerTrackExp1);
	DECLARE_ANIM_COMP_ALGORITHM(Linear_PerTrackExp2);

	check(AnimSeq != NULL);

	// attempt to find the default skeletal mesh associated with this sequence
	USkeleton * Skeleton = AnimSeq->GetSkeleton();
	check (Skeleton);

	static int32 TotalRecompressions = 0;

	static int32 TotalNoWinnerRounds = 0;

	static int32 AlternativeCompressorLossesFromSize = 0;
	static int32 AlternativeCompressorLossesFromError = 0;
	static int32 AlternativeCompressorSavings = 0;
	static int64 TotalSizeBefore = 0;
	static int64 TotalSizeNow = 0;
	static int64 TotalUncompressed = 0;

	// we must have raw data to continue
	if( AnimSeq->RawAnimationData.Num() > 0 )
	{
		// See if we're trying alternate compressors
		bool const bTryAlternateCompressor = MasterTolerance > 0.0f;

		// Get the current size
		int32 OriginalSize = AnimSeq->GetResourceSize(EResourceSizeMode::Exclusive);
		TotalSizeBefore += OriginalSize;

		// Estimate total uncompressed
		TotalUncompressed += ((sizeof(FVector) + sizeof(FQuat)) * AnimSeq->RawAnimationData.Num() * AnimSeq->NumFrames);

		// Filter RAW data to get rid of mismatched tracks (translation/rotation data with a different number of keys than there are frames)
		// No trivial key removal is done at this point (impossible error metrics of -1), since all of the techniques will perform it themselves
		AnimSeq->CompressRawAnimData(-1.0f, -1.0f);

		// start with the current technique, or the default if none exists.
		// this will serve as our fallback if no better technique can be found
		int32 OriginalKeyEncodingFormat = AnimSeq->KeyEncodingFormat;
		int32 OriginalTranslationFormat = AnimSeq->TranslationCompressionFormat;
		int32 OriginalRotationFormat = AnimSeq->RotationCompressionFormat;

		AnimationErrorStats OriginalErrorStats;
		AnimationErrorStats TrueOriginalErrorStats;
		TArray<FBoneData> BoneData;

		// Build skeleton metadata to use during the key reduction.
		FAnimationUtils::BuildSkeletonMetaData( Skeleton, BoneData );
		FAnimationUtils::ComputeCompressionError(AnimSeq, BoneData, TrueOriginalErrorStats);

		int32 AfterOriginalRecompression = 0;
		if( (bFirstRecompressUsingCurrentOrDefault && !bTryAlternateCompressor) )
		{
			UAnimCompress* OriginalCompressionAlgorithm = AnimSeq->CompressionScheme ? AnimSeq->CompressionScheme : FAnimationUtils::GetDefaultAnimationCompressionAlgorithm();

			if( OriginalCompressionAlgorithm->IsA(UDEPRECATED_AnimCompress_RevertToRaw::StaticClass()) )
			{
				UE_LOG(LogAnimation, Warning, TEXT("FAnimationUtils::CompressAnimSequence %s (%s) Not allowed to revert to RAW. Using default compression scheme."), *AnimSeq->GetName(), *AnimSeq->GetFullName());
				OriginalCompressionAlgorithm = FAnimationUtils::GetDefaultAnimationCompressionAlgorithm();
			}
			else if( OriginalCompressionAlgorithm->IsA(UAnimCompress_LeastDestructive::StaticClass()) )
			{
				UE_LOG(LogAnimation, Warning, TEXT("FAnimationUtils::CompressAnimSequence %s (%s) Not allowed to least destructive. Using default compression scheme."), *AnimSeq->GetName(), *AnimSeq->GetFullName());
				OriginalCompressionAlgorithm = FAnimationUtils::GetDefaultAnimationCompressionAlgorithm();
			}

			OriginalCompressionAlgorithm->Reduce( AnimSeq, bOutput );
			AfterOriginalRecompression = AnimSeq->GetResourceSize(EResourceSizeMode::Exclusive);

			// figure out our current compression error
			FAnimationUtils::ComputeCompressionError(AnimSeq, BoneData, OriginalErrorStats);
		}
		else
		{
			AfterOriginalRecompression = AnimSeq->GetResourceSize(EResourceSizeMode::Exclusive);
			OriginalErrorStats = TrueOriginalErrorStats;
		}
 
		// Check for global permission to try an alternative compressor
		if( bTryAlternateCompressor && !AnimSeq->bDoNotOverrideCompression )
		{
			AnimationErrorStats NewErrorStats = OriginalErrorStats;
			if (bRaiseMaxErrorToExisting)
			{
				if (NewErrorStats.MaxError > MasterTolerance)
				{
					UE_LOG(LogAnimation, Warning, TEXT("  Boosting MasterTolerance to %f, as existing MaxDiff was higher than %f and bRaiseMaxErrorToExisting=true"), NewErrorStats.MaxError, MasterTolerance);
					MasterTolerance = NewErrorStats.MaxError;
				}
			}

			{
				// backup key information from the sequence
				TArray<struct FTranslationTrack> SavedTranslationData = AnimSeq->TranslationData;
				TArray<struct FRotationTrack> SavedRotationData = AnimSeq->RotationData;
				class UAnimCompress* SavedCompressionScheme = AnimSeq->CompressionScheme;
				AnimationCompressionFormat SavedTranslationCompressionFormat = AnimSeq->TranslationCompressionFormat;
				AnimationCompressionFormat SavedRotationCompressionFormat = AnimSeq->RotationCompressionFormat;
				AnimationKeyFormat SavedKeyEncodingFormat = AnimSeq->KeyEncodingFormat;
				TArray<int32> SavedCompressedTrackOffsets = AnimSeq->CompressedTrackOffsets;
				TArray<uint8> SavedCompressedByteStream = AnimSeq->CompressedByteStream;
				FCompressedOffsetData SavedCompressedScaleOffsets = AnimSeq->CompressedScaleOffsets;
				AnimEncoding* SavedTranslationCodec = AnimSeq->TranslationCodec;
				AnimEncoding* SavedRotationCodec = AnimSeq->RotationCodec;
				AnimEncoding* SavedScaleCodec = AnimSeq->ScaleCodec;

				// count all attempts for debugging
				++TotalRecompressions;

				// Prepare to compress
				int32 CurrentSize = AnimSeq->GetResourceSize(EResourceSizeMode::Exclusive);
				int32* WinningCompressorCounter = NULL;
				float* WinningCompressorErrorSum = NULL;
				int32* WinningCompressorMarginalSavingsSum = NULL;
				int32 WinningCompressorMarginalSavings = 0;
				FString WinningCompressorName;
				int32 WinningCompressorSavings = 0;
				float PctSaving = 0.f;
				float WinningCompressorError = OriginalErrorStats.MaxError;
				bool bKeepNewCompressionMethod = false;

				UE_LOG(LogAnimation, Warning, TEXT("Compressing %s (%s)\n\tSkeleton: %s\n\tOriginal Size: %i   MaxDiff: %f"),
					*AnimSeq->GetName(),
					*AnimSeq->GetFullName(),
					Skeleton ? *Skeleton->GetFName().ToString() : TEXT("NULL - Not all compression techniques can be used!"),
					OriginalSize,
					TrueOriginalErrorStats.MaxError);

				UE_LOG(LogAnimation, Warning, TEXT("Original Key Encoding: %s\n\tOriginal Rotation Format: %s\n\tOriginal Translation Format: %s\n\tNumFrames: %i\n\tSequenceLength: %f (%2.1f fps)"),
					*GetAnimationKeyFormatString(static_cast<AnimationKeyFormat>(OriginalKeyEncodingFormat)),
					*FAnimationUtils::GetAnimationCompressionFormatString(static_cast<AnimationCompressionFormat>(OriginalRotationFormat)),
					*FAnimationUtils::GetAnimationCompressionFormatString(static_cast<AnimationCompressionFormat>(OriginalTranslationFormat)),
					AnimSeq->NumFrames,
					AnimSeq->SequenceLength,
					(AnimSeq->NumFrames > 1) ? AnimSeq->NumFrames / AnimSeq->SequenceLength : DEFAULT_SAMPLERATE);

				if (bFirstRecompressUsingCurrentOrDefault)
				{
					UE_LOG(LogAnimation, Warning, TEXT("Recompressed using current/default\n\tRecompress Size: %i   MaxDiff: %f\n\tRecompress Scheme: %s"),
						AfterOriginalRecompression,
						OriginalErrorStats.MaxError,
						AnimSeq->CompressionScheme ? *AnimSeq->CompressionScheme->GetClass()->GetName() : TEXT("NULL"));
				}

				// Progressive Algorithm
				if( bTryPerTrackBitwiseCompression )
				{
					UAnimCompress_PerTrackCompression* PerTrackCompressor = ConstructObject<UAnimCompress_PerTrackCompression>(UAnimCompress_PerTrackCompression::StaticClass());

					// Start not too aggressive
					PerTrackCompressor->MaxPosDiffBitwise /= 10.f;
					PerTrackCompressor->MaxAngleDiffBitwise /= 10.f;
					PerTrackCompressor->MaxScaleDiffBitwise /= 10.f;
					PerTrackCompressor->bUseAdaptiveError2 = true;

					// Try default compressor first
					TRYCOMPRESSION(Progressive_PerTrack, PerTrackCompressor);

					if( NewErrorStats.MaxError >= MasterTolerance )
					{
						UE_LOG(LogAnimation, Warning, TEXT("\tStandard bitwise compressor too aggressive, lower default settings."));
					}
					else
					{
						// First, start by finding most downsampling factor.
						if( bTryIntervalKeyRemoval && (AnimSeq->NumFrames >= PerTrackCompressor->MinKeysForResampling) )
						{
							PerTrackCompressor->bResampleAnimation = true;
				
							// Try PerTrackCompression, down sample to 5 Hz
							PerTrackCompressor->ResampledFramerate = 5.0f;
							UE_LOG(LogAnimation, Warning, TEXT("\tResampledFramerate: %f"), PerTrackCompressor->ResampledFramerate);
							TRYCOMPRESSION(Progressive_PerTrack, PerTrackCompressor);

							// If too much error, try 6Hz
							if( NewErrorStats.MaxError >= MasterTolerance )
							{
								PerTrackCompressor->ResampledFramerate = 6.0f;
								UE_LOG(LogAnimation, Warning, TEXT("\tResampledFramerate: %f"), PerTrackCompressor->ResampledFramerate);
								TRYCOMPRESSION(Progressive_PerTrack, PerTrackCompressor);

								// if too much error go 10Hz, 15Hz, 20Hz.
								if( NewErrorStats.MaxError >= MasterTolerance )
								{
									PerTrackCompressor->ResampledFramerate = 5.0f;
									// Keep trying until we find something that works (or we just don't downsample)
									while( PerTrackCompressor->ResampledFramerate < 20.f && NewErrorStats.MaxError >= MasterTolerance )
									{
										PerTrackCompressor->ResampledFramerate += 5.f;
										UE_LOG(LogAnimation, Warning, TEXT("\tResampledFramerate: %f"), PerTrackCompressor->ResampledFramerate);
										TRYCOMPRESSION(Progressive_PerTrack, PerTrackCompressor);
									}
								}
							}
							
							// Give up downsampling if it didn't work.
							if( NewErrorStats.MaxError >= MasterTolerance )
							{
								UE_LOG(LogAnimation, Warning, TEXT("\tDownsampling didn't work."));
								PerTrackCompressor->bResampleAnimation = false;
							}
						}
						
						// Now do Linear Key Removal
						if( AnimSeq->NumFrames > 1 )
						{
							PerTrackCompressor->bActuallyFilterLinearKeys = true;
							PerTrackCompressor->bRetarget = true;
							
							int32 const TestSteps = 16;
							float const MaxScale = 2^(TestSteps);

							// Start with the least aggressive first. if that one doesn't succeed, don't bother going through all the steps.
							PerTrackCompressor->MaxPosDiff /= MaxScale;
							PerTrackCompressor->MaxAngleDiff /= MaxScale;
							PerTrackCompressor->MaxScaleDiff /= MaxScale;
							PerTrackCompressor->MaxEffectorDiff /= MaxScale;
							PerTrackCompressor->MinEffectorDiff /= MaxScale;
							PerTrackCompressor->EffectorDiffSocket /= MaxScale;
							UE_LOG(LogAnimation, Warning, TEXT("\tLinearKeys. MaxPosDiff: %f, MaxAngleDiff: %f, MaxScaleDiff : %f"), PerTrackCompressor->MaxPosDiff, PerTrackCompressor->MaxAngleDiff, PerTrackCompressor->MaxScaleDiff);
							TRYCOMPRESSION(Progressive_PerTrack, PerTrackCompressor);							
							PerTrackCompressor->MaxPosDiff *= MaxScale;
							PerTrackCompressor->MaxAngleDiff *= MaxScale;
							PerTrackCompressor->MaxScaleDiff *= MaxScale;
							PerTrackCompressor->MaxEffectorDiff *= MaxScale;
							PerTrackCompressor->MinEffectorDiff *= MaxScale;
							PerTrackCompressor->EffectorDiffSocket *= MaxScale;

							if( NewErrorStats.MaxError < MasterTolerance )
							{
								// Start super aggressive, and go down until we find something that works.
								UE_LOG(LogAnimation, Warning, TEXT("\tLinearKeys. MaxPosDiff: %f, MaxAngleDiff: %f, MaxScaleDiff : %f"), PerTrackCompressor->MaxPosDiff, PerTrackCompressor->MaxAngleDiff, PerTrackCompressor->MaxScaleDiff);
								TRYCOMPRESSION(Progressive_PerTrack, PerTrackCompressor);

								for(int32 Step=0; Step<TestSteps && (NewErrorStats.MaxError >= MasterTolerance); Step++)
								{
									PerTrackCompressor->MaxPosDiff /= 2.f;
									PerTrackCompressor->MaxAngleDiff /= 2.f;
									PerTrackCompressor->MaxScaleDiff /= 2.f;
									PerTrackCompressor->MaxEffectorDiff /= 2.f;
									PerTrackCompressor->MinEffectorDiff /= 2.f;
									PerTrackCompressor->EffectorDiffSocket /= 2.f;
									UE_LOG(LogAnimation, Warning, TEXT("\tLinearKeys. MaxPosDiff: %f, MaxAngleDiff: %f, MaxScaleDiff : %f"), PerTrackCompressor->MaxPosDiff, PerTrackCompressor->MaxAngleDiff, PerTrackCompressor->MaxScaleDiff);
									TRYCOMPRESSION(Progressive_PerTrack, PerTrackCompressor);
								}
							}

							// Give up Linear Key Compression if it didn't work
							if( NewErrorStats.MaxError >= MasterTolerance )
							{
								PerTrackCompressor->bActuallyFilterLinearKeys = false;
								PerTrackCompressor->bRetarget = false;
							}
						}

						// Finally tighten up bitwise compression
						PerTrackCompressor->MaxPosDiffBitwise *= 10.f;
						PerTrackCompressor->MaxAngleDiffBitwise *= 10.f;
						PerTrackCompressor->MaxScaleDiffBitwise *= 10.f;
						{
							int32 const TestSteps = 16;
							float const MaxScale = 2^(TestSteps/2);

							PerTrackCompressor->MaxPosDiffBitwise *= MaxScale;
							PerTrackCompressor->MaxAngleDiffBitwise *= MaxScale;
							PerTrackCompressor->MaxScaleDiffBitwise *= MaxScale;
							UE_LOG(LogAnimation, Warning, TEXT("\tBitwise. MaxPosDiffBitwise: %f, MaxAngleDiffBitwise: %f, MaxScaleDiffBitwise: %f"), PerTrackCompressor->MaxPosDiffBitwise, PerTrackCompressor->MaxAngleDiffBitwise, PerTrackCompressor->MaxScaleDiffBitwise);
							TRYCOMPRESSION(Progressive_PerTrack, PerTrackCompressor);
							PerTrackCompressor->MaxPosDiffBitwise /= 2.f;
							PerTrackCompressor->MaxAngleDiffBitwise /= 2.f;
							PerTrackCompressor->MaxScaleDiffBitwise /= 2.f;
							for(int32 Step=0; Step<TestSteps && (NewErrorStats.MaxError >= MasterTolerance) && (PerTrackCompressor->MaxPosDiffBitwise >= PerTrackCompressor->MaxZeroingThreshold); Step++)
							{
								UE_LOG(LogAnimation, Warning, TEXT("\tBitwise. MaxPosDiffBitwise: %f, MaxAngleDiffBitwise: %f, MaxScaleDiffBitwise: %f"), PerTrackCompressor->MaxPosDiffBitwise, PerTrackCompressor->MaxAngleDiffBitwise, PerTrackCompressor->MaxScaleDiffBitwise);
								TRYCOMPRESSION(Progressive_PerTrack, PerTrackCompressor);
								PerTrackCompressor->MaxPosDiffBitwise /= 2.f;
								PerTrackCompressor->MaxAngleDiffBitwise /= 2.f;
								PerTrackCompressor->MaxScaleDiffBitwise /= 2.f;
							}
						}
					}
				}

				// Start with Bitwise Compress only
				if( bTryFixedBitwiseCompression )
				{
					UAnimCompress_BitwiseCompressOnly* BitwiseCompressor = ConstructObject<UAnimCompress_BitwiseCompressOnly>( UAnimCompress_BitwiseCompressOnly::StaticClass() );

					// Try ACF_Float96NoW
					BitwiseCompressor->RotationCompressionFormat = ACF_Float96NoW;
					BitwiseCompressor->TranslationCompressionFormat = ACF_None;
					TRYCOMPRESSION(BitwiseACF_Float96, BitwiseCompressor);

					// Try ACF_Fixed48NoW
					BitwiseCompressor->RotationCompressionFormat = ACF_Fixed48NoW;
					BitwiseCompressor->TranslationCompressionFormat = ACF_None;
					TRYCOMPRESSION(BitwiseACF_Fixed48,BitwiseCompressor);

// 32bits currently unusable due to creating too much error
// 					// Try ACF_IntervalFixed32NoW
// 					BitwiseCompressor->RotationCompressionFormat = ACF_IntervalFixed32NoW;
// 					BitwiseCompressor->TranslationCompressionFormat = ACF_None;
// 					TRYCOMPRESSION(BitwiseACF_IntervalFixed32,BitwiseCompressor);
// 
// 					// Try ACF_Fixed32NoW
// 					BitwiseCompressor->RotationCompressionFormat = ACF_Fixed32NoW;
// 					BitwiseCompressor->TranslationCompressionFormat = ACF_None;
// 					TRYCOMPRESSION(BitwiseACF_Fixed32,BitwiseCompressor);
				}

				// Start with Bitwise Compress only
				// this compressor has a minimum number of frames requirement. So no need to go there if we don't meet that...
				if( bTryFixedBitwiseCompression && bTryIntervalKeyRemoval )
				{
					UAnimCompress_RemoveEverySecondKey* RemoveEveryOtherKeyCompressor = ConstructObject<UAnimCompress_RemoveEverySecondKey>( UAnimCompress_RemoveEverySecondKey::StaticClass() );
					if( AnimSeq->NumFrames > RemoveEveryOtherKeyCompressor->MinKeys )
					{
						RemoveEveryOtherKeyCompressor->bStartAtSecondKey = false;
						{
							// Try ACF_Float96NoW
							RemoveEveryOtherKeyCompressor->RotationCompressionFormat = ACF_Float96NoW;	
							RemoveEveryOtherKeyCompressor->TranslationCompressionFormat = ACF_None;		
							TRYCOMPRESSION(HalfOddACF_Float96, RemoveEveryOtherKeyCompressor);

							// Try ACF_Fixed48NoW
							RemoveEveryOtherKeyCompressor->RotationCompressionFormat = ACF_Fixed48NoW;	
							RemoveEveryOtherKeyCompressor->TranslationCompressionFormat = ACF_None;		
							TRYCOMPRESSION(HalfOddACF_Fixed48, RemoveEveryOtherKeyCompressor);

// 32bits currently unusable due to creating too much error
// 							// Try ACF_IntervalFixed32NoW
// 							RemoveEveryOtherKeyCompressor->RotationCompressionFormat = ACF_IntervalFixed32NoW;	
// 							RemoveEveryOtherKeyCompressor->TranslationCompressionFormat = ACF_None;		
// 							TRYCOMPRESSION(HalfOddACF_IntervalFixed32, RemoveEveryOtherKeyCompressor);
// 
// 							// Try ACF_Fixed32NoW
// 							RemoveEveryOtherKeyCompressor->RotationCompressionFormat = ACF_Fixed32NoW;	
// 							RemoveEveryOtherKeyCompressor->TranslationCompressionFormat = ACF_None;		
// 							TRYCOMPRESSION(HalfOddACF_Fixed32, RemoveEveryOtherKeyCompressor);
						}
						RemoveEveryOtherKeyCompressor->bStartAtSecondKey = true;
						{
							// Try ACF_Float96NoW
							RemoveEveryOtherKeyCompressor->RotationCompressionFormat = ACF_Float96NoW;	
							RemoveEveryOtherKeyCompressor->TranslationCompressionFormat = ACF_None;		
							TRYCOMPRESSION(HalfEvenACF_Float96,RemoveEveryOtherKeyCompressor);

							// Try ACF_Fixed48NoW
							RemoveEveryOtherKeyCompressor->RotationCompressionFormat = ACF_Fixed48NoW;	
							RemoveEveryOtherKeyCompressor->TranslationCompressionFormat = ACF_None;		
							TRYCOMPRESSION(HalfEvenACF_Fixed48,RemoveEveryOtherKeyCompressor);

// 32bits currently unusable due to creating too much error
// 							// Try ACF_IntervalFixed32NoW
// 							RemoveEveryOtherKeyCompressor->RotationCompressionFormat = ACF_IntervalFixed32NoW;	
// 							RemoveEveryOtherKeyCompressor->TranslationCompressionFormat = ACF_None;		
// 							TRYCOMPRESSION(HalfEvenACF_IntervalFixed32,RemoveEveryOtherKeyCompressor);
// 
// 							// Try ACF_Fixed32NoW
// 							RemoveEveryOtherKeyCompressor->RotationCompressionFormat = ACF_Fixed32NoW;	
// 							RemoveEveryOtherKeyCompressor->TranslationCompressionFormat = ACF_None;		
// 							TRYCOMPRESSION(HalfEvenACF_Fixed32,RemoveEveryOtherKeyCompressor);
						}
					}
				}

				// construct the proposed compressor		
				if( bTryLinearKeyRemovalCompression && AnimSeq->NumFrames > 1 )
				{
					UAnimCompress_RemoveLinearKeys* LinearKeyRemover = ConstructObject<UAnimCompress_RemoveLinearKeys>( UAnimCompress_RemoveLinearKeys::StaticClass() );
					{
						// Try ACF_Float96NoW
						LinearKeyRemover->RotationCompressionFormat = ACF_Float96NoW;
						LinearKeyRemover->TranslationCompressionFormat = ACF_None;	
						TRYCOMPRESSION(LinearACF_Float96,LinearKeyRemover);

						// Try ACF_Fixed48NoW
						LinearKeyRemover->RotationCompressionFormat = ACF_Fixed48NoW;
						LinearKeyRemover->TranslationCompressionFormat = ACF_None;	
						TRYCOMPRESSION(LinearACF_Fixed48,LinearKeyRemover);

// Error is too bad w/ 32bits
// 						// Try ACF_IntervalFixed32NoW
// 						LinearKeyRemover->RotationCompressionFormat = ACF_IntervalFixed32NoW;
// 						LinearKeyRemover->TranslationCompressionFormat = ACF_None;
// 						TRYCOMPRESSION(LinearACF_IntervalFixed32,LinearKeyRemover);
// 
// 						// Try ACF_Fixed32NoW
// 						LinearKeyRemover->RotationCompressionFormat = ACF_Fixed32NoW;
// 						LinearKeyRemover->TranslationCompressionFormat = ACF_None;
// 						TRYCOMPRESSION(LinearACF_Fixed32,LinearKeyRemover);
					}
				}

				if( bTryPerTrackBitwiseCompression )
				{
					UAnimCompress_PerTrackCompression* PerTrackCompressor = ConstructObject<UAnimCompress_PerTrackCompression>(UAnimCompress_PerTrackCompression::StaticClass());

					// Straight PerTrackCompression, no key decimation and no linear key removal
					TRYCOMPRESSION(Bitwise_PerTrack, PerTrackCompressor);
					PerTrackCompressor->bUseAdaptiveError = true;

					// Full blown linear
					PerTrackCompressor->bActuallyFilterLinearKeys = true;
					PerTrackCompressor->bRetarget = true;
					TRYCOMPRESSION(Linear_PerTrack, PerTrackCompressor);

					// Adaptive retargetting based on height within the skeleton
					PerTrackCompressor->bActuallyFilterLinearKeys = true;
					PerTrackCompressor->bRetarget = false;
					PerTrackCompressor->ParentingDivisor = 2.0f;
					PerTrackCompressor->ParentingDivisorExponent = 1.6f;
					TRYCOMPRESSION(Adaptive1_LinPerTrackNoRT, PerTrackCompressor);
					PerTrackCompressor->ParentingDivisor = 1.0f;
					PerTrackCompressor->ParentingDivisorExponent = 1.0f;

					PerTrackCompressor->bActuallyFilterLinearKeys = true;
					PerTrackCompressor->bRetarget = true;
					PerTrackCompressor->ParentingDivisor = 2.0f;
					PerTrackCompressor->ParentingDivisorExponent = 1.6f;
					TRYCOMPRESSION(Adaptive1_LinPerTrack, PerTrackCompressor);
					PerTrackCompressor->ParentingDivisor = 1.0f;
					PerTrackCompressor->ParentingDivisorExponent = 1.0f;
				}


				if( bTryPerTrackBitwiseCompression )
				{
					UAnimCompress_PerTrackCompression* PerTrackCompressor = ConstructObject<UAnimCompress_PerTrackCompression>(UAnimCompress_PerTrackCompression::StaticClass());
					PerTrackCompressor->bUseAdaptiveError = true;

					if ( AnimSeq->NumFrames > 1 )
					{
						PerTrackCompressor->bActuallyFilterLinearKeys = true;
						PerTrackCompressor->bRetarget = true;

						PerTrackCompressor->MaxPosDiff = 0.1;
// 						PerTrackCompressor->MaxAngleDiff = 0.1;
						PerTrackCompressor->MaxScaleDiff = 0.00001;
						PerTrackCompressor->ParentingDivisor = 2.0f;
						PerTrackCompressor->ParentingDivisorExponent = 1.0f;
						TRYCOMPRESSION(Linear_PerTrackExp1, PerTrackCompressor);

						PerTrackCompressor->MaxPosDiff = 0.01;
// 						PerTrackCompressor->MaxAngleDiff = 0.025;
						PerTrackCompressor->MaxScaleDiff = 0.000001;
						PerTrackCompressor->ParentingDivisor = 2.0f;
						PerTrackCompressor->ParentingDivisorExponent = 1.0f;
						TRYCOMPRESSION(Linear_PerTrackExp2, PerTrackCompressor);

						PerTrackCompressor->bRetarget = false;
						PerTrackCompressor->MaxPosDiff = 0.1;
// 						PerTrackCompressor->MaxAngleDiff = 0.025;
						PerTrackCompressor->MaxScaleDiff = 0.00001;
						PerTrackCompressor->ParentingDivisor = 1.0f;
						PerTrackCompressor->ParentingDivisorExponent = 1.0f;
					}
				}

				if( bTryPerTrackBitwiseCompression )
				{
					UAnimCompress_PerTrackCompression* PerTrackCompressor = ConstructObject<UAnimCompress_PerTrackCompression>(UAnimCompress_PerTrackCompression::StaticClass());
					PerTrackCompressor->bUseAdaptiveError = true;

					// Try the decimation algorithms
					if (bTryIntervalKeyRemoval && (AnimSeq->NumFrames >= PerTrackCompressor->MinKeysForResampling))
					{
						PerTrackCompressor->bActuallyFilterLinearKeys = false;
						PerTrackCompressor->bRetarget = false;
						PerTrackCompressor->bUseAdaptiveError = false;
						PerTrackCompressor->bResampleAnimation = true;

						// Try PerTrackCompression, downsample to 20 Hz
						PerTrackCompressor->ResampledFramerate = 20.0f;
						TRYCOMPRESSION(Downsample20Hz_PerTrack, PerTrackCompressor);

						// Try PerTrackCompression, downsample to 15 Hz
						PerTrackCompressor->ResampledFramerate = 15.0f;
						TRYCOMPRESSION(Downsample15Hz_PerTrack, PerTrackCompressor);

						// Try PerTrackCompression, downsample to 10 Hz
						PerTrackCompressor->ResampledFramerate = 10.0f;
						TRYCOMPRESSION(Downsample10Hz_PerTrack, PerTrackCompressor);

						// Try PerTrackCompression, downsample to 5 Hz
						PerTrackCompressor->ResampledFramerate = 5.0f;
						TRYCOMPRESSION(Downsample5Hz_PerTrack, PerTrackCompressor);


						// Downsampling with linear key removal and adaptive error metrics
						PerTrackCompressor->bActuallyFilterLinearKeys = true;
						PerTrackCompressor->bRetarget = false;
						PerTrackCompressor->bUseAdaptiveError = true;
						PerTrackCompressor->ParentingDivisor = 2.0f;
						PerTrackCompressor->ParentingDivisorExponent = 1.6f;

						PerTrackCompressor->ResampledFramerate = 15.0f;
						TRYCOMPRESSION(Adaptive1_15Hz_LinPerTrack, PerTrackCompressor);

						PerTrackCompressor->ResampledFramerate = 10.0f;
						TRYCOMPRESSION(Adaptive1_10Hz_LinPerTrack, PerTrackCompressor);

						PerTrackCompressor->ResampledFramerate = 5.0f;
						TRYCOMPRESSION(Adaptive1_5Hz_LinPerTrack, PerTrackCompressor);
					}
				}


				if( bTryPerTrackBitwiseCompression && bTryIntervalKeyRemoval)
				{
					// Try the decimation algorithms
					if (AnimSeq->NumFrames >= 3)
					{
						UAnimCompress_PerTrackCompression* NewPerTrackCompressor = ConstructObject<UAnimCompress_PerTrackCompression>(UAnimCompress_PerTrackCompression::StaticClass());

						// Downsampling with linear key removal and adaptive error metrics v2
						NewPerTrackCompressor->MinKeysForResampling = 3;
						NewPerTrackCompressor->bUseAdaptiveError2 = true;
						NewPerTrackCompressor->MaxPosDiffBitwise = 0.05;
						NewPerTrackCompressor->MaxAngleDiffBitwise = 0.02;
						NewPerTrackCompressor->MaxScaleDiffBitwise = 0.00005;
						NewPerTrackCompressor->bActuallyFilterLinearKeys = true;
						NewPerTrackCompressor->bRetarget = true;

						NewPerTrackCompressor->ResampledFramerate = 15.0f;
						TRYCOMPRESSION(Adaptive2_15Hz_LinPerTrack, NewPerTrackCompressor);

						NewPerTrackCompressor->ResampledFramerate = 10.0f;
						TRYCOMPRESSION(Adaptive2_10Hz_LinPerTrack, NewPerTrackCompressor);
					}
				}


				if( bTryPerTrackBitwiseCompression)
				{
					// Adaptive error through probing the effect of perturbations at each track
					UAnimCompress_PerTrackCompression* NewPerTrackCompressor = ConstructObject<UAnimCompress_PerTrackCompression>(UAnimCompress_PerTrackCompression::StaticClass());
					NewPerTrackCompressor->bUseAdaptiveError2 = true;
					NewPerTrackCompressor->MaxPosDiffBitwise = 0.05;
					NewPerTrackCompressor->MaxAngleDiffBitwise = 0.02;
					NewPerTrackCompressor->MaxScaleDiffBitwise = 0.00005;

					TRYCOMPRESSION(Adaptive2_PerTrack, NewPerTrackCompressor);

					NewPerTrackCompressor->bActuallyFilterLinearKeys = true;
					NewPerTrackCompressor->bRetarget = true;
					TRYCOMPRESSION(Adaptive2_LinPerTrack, NewPerTrackCompressor);

					NewPerTrackCompressor->bActuallyFilterLinearKeys = true;
					NewPerTrackCompressor->bRetarget = false;
					TRYCOMPRESSION(Adaptive2_LinPerTrackNoRT, NewPerTrackCompressor);
				}

				// Increase winning compressor.
				if( CurrentSize != OriginalSize )
				{
					int32 SizeDecrease = OriginalSize - CurrentSize;
					if (WinningCompressorCounter)
					{
						++(*WinningCompressorCounter);
						(*WinningCompressorErrorSum) += WinningCompressorError;
						AlternativeCompressorSavings += WinningCompressorSavings;
						*WinningCompressorMarginalSavingsSum += WinningCompressorMarginalSavings;
						checkf(WinningCompressorSavings == SizeDecrease);

					UE_LOG(LogAnimation, Warning, TEXT("  Recompressing '%s' with compressor '%s' saved %i bytes (%i -> %i -> %i) (max diff=%f)\n"),
							*AnimSeq->GetName(),
							*WinningCompressorName,
							SizeDecrease,
							OriginalSize, AfterOriginalRecompression, CurrentSize,
							WinningCompressorError);
					}
					else
					{
						TotalNoWinnerRounds++;
						UE_LOG(LogAnimation, Warning, TEXT("  Recompressing '%s' with original/default compressor saved %i bytes (%i -> %i -> %i) (max diff=%f)\n"), 
							*AnimSeq->GetName(),
							SizeDecrease,
							OriginalSize, AfterOriginalRecompression, CurrentSize,
							WinningCompressorError);
					}


					// Update the memory stats
#if STATS
					if( IsRunningGame() )
					{
						if (SizeDecrease > 0)
						{
							DEC_DWORD_STAT_BY( STAT_AnimationMemory, SizeDecrease );
						}
						else
						{
							INC_DWORD_STAT_BY( STAT_AnimationMemory, -SizeDecrease );
						}
					}
#endif
				}

				// Make sure we got that right.
				check(CurrentSize == AnimSeq->GetResourceSize(EResourceSizeMode::Exclusive));
				TotalSizeNow += CurrentSize;

				PctSaving = TotalSizeBefore > 0 ? 100.f - (100.f * float(TotalSizeNow) / float(TotalSizeBefore)) : 0.f;
				UE_LOG(LogAnimation, Warning, TEXT("Compression Stats Summary [%i total, %i Bytes saved, %i before, %i now, %3.1f%% savings. Uncompressed: %i TotalRatio: %i:1]"), 
				TotalRecompressions,
				AlternativeCompressorSavings,
				TotalSizeBefore, 
				TotalSizeNow,
				PctSaving,
				TotalUncompressed,
				(TotalUncompressed / TotalSizeNow));

				UE_LOG(LogAnimation, Warning, TEXT("\t\tDefault compressor wins:                      %i"), TotalNoWinnerRounds);

				if (bTryFixedBitwiseCompression)
				{
					WARN_COMPRESSION_STATUS(BitwiseACF_Float96);
					WARN_COMPRESSION_STATUS(BitwiseACF_Fixed48);
// 					WARN_COMPRESSION_STATUS(BitwiseACF_IntervalFixed32);
// 					WARN_COMPRESSION_STATUS(BitwiseACF_Fixed32);
				}

				if (bTryFixedBitwiseCompression && bTryIntervalKeyRemoval)
				{
					WARN_COMPRESSION_STATUS(HalfOddACF_Float96);
					WARN_COMPRESSION_STATUS(HalfOddACF_Fixed48);
// 					WARN_COMPRESSION_STATUS(HalfOddACF_IntervalFixed32);
// 					WARN_COMPRESSION_STATUS(HalfOddACF_Fixed32);

					WARN_COMPRESSION_STATUS(HalfEvenACF_Float96);
					WARN_COMPRESSION_STATUS(HalfEvenACF_Fixed48);
// 					WARN_COMPRESSION_STATUS(HalfEvenACF_IntervalFixed32);
// 					WARN_COMPRESSION_STATUS(HalfEvenACF_Fixed32);
				}

				if (bTryLinearKeyRemovalCompression)
				{
					WARN_COMPRESSION_STATUS(LinearACF_Float96);
					WARN_COMPRESSION_STATUS(LinearACF_Fixed48);
// 					WARN_COMPRESSION_STATUS(LinearACF_IntervalFixed32);
// 					WARN_COMPRESSION_STATUS(LinearACF_Fixed32);
				}

				if (bTryPerTrackBitwiseCompression)
				{
					WARN_COMPRESSION_STATUS(Progressive_PerTrack);
					WARN_COMPRESSION_STATUS(Bitwise_PerTrack);
					WARN_COMPRESSION_STATUS(Linear_PerTrack);
					WARN_COMPRESSION_STATUS(Adaptive1_LinPerTrackNoRT);
					WARN_COMPRESSION_STATUS(Adaptive1_LinPerTrack);

					WARN_COMPRESSION_STATUS(Linear_PerTrackExp1);
					WARN_COMPRESSION_STATUS(Linear_PerTrackExp2);
				}

				if (bTryPerTrackBitwiseCompression && bTryIntervalKeyRemoval)
				{
					WARN_COMPRESSION_STATUS(Downsample20Hz_PerTrack);
					WARN_COMPRESSION_STATUS(Downsample15Hz_PerTrack);
					WARN_COMPRESSION_STATUS(Downsample10Hz_PerTrack);
					WARN_COMPRESSION_STATUS(Downsample5Hz_PerTrack);

					WARN_COMPRESSION_STATUS(Adaptive1_15Hz_LinPerTrack);
					WARN_COMPRESSION_STATUS(Adaptive1_10Hz_LinPerTrack);
					WARN_COMPRESSION_STATUS(Adaptive1_5Hz_LinPerTrack);

					WARN_COMPRESSION_STATUS(Adaptive2_15Hz_LinPerTrack);
					WARN_COMPRESSION_STATUS(Adaptive2_10Hz_LinPerTrack);
				}

				if (bTryPerTrackBitwiseCompression)
				{
					WARN_COMPRESSION_STATUS(Adaptive2_PerTrack);
					WARN_COMPRESSION_STATUS(Adaptive2_LinPerTrack);
					WARN_COMPRESSION_STATUS(Adaptive2_LinPerTrackNoRT);
				}
			}
		}
		// Do not recompress - Still take into account size for stats.
		else
		{
			TotalSizeNow += AnimSeq->GetResourceSize(EResourceSizeMode::Exclusive);
		}
	}
	else
	{
		UE_LOG(LogAnimation, Warning, TEXT("Compression Requested for Empty Animation %s"), *AnimSeq->GetName() );
	}
#endif // WITH_EDITORONLY_DATA
}


void FAnimationUtils::TestForMissingMeshes(UAnimSequence* AnimSeq)
{
	if (FPlatformProperties::HasEditorOnlyData())
	{
		check(AnimSeq != NULL);

		USkeleton * Skeleton = AnimSeq->GetSkeleton();
		check( Skeleton );

		static int32 MissingSkeletonCount = 0;
		static TArray<FString> MissingSkeletonArray;
	}
}

static void GetBindPoseAtom(FTransform &OutBoneAtom, int32 BoneIndex, USkeleton *Skeleton)
{
	OutBoneAtom = Skeleton->GetRefLocalPoses()[BoneIndex];
// #if DEBUG_ADDITIVE_CREATION
// 	UE_LOG(LogAnimation, Log, TEXT("GetBindPoseAtom BoneIndex: %d, OutBoneAtom: %s"), BoneIndex, *OutBoneAtom.ToString());
// #endif
}

/** Get default Outer for AnimSequences contained in this AnimSet.
	*  The intent is to use that when Constructing new AnimSequences to put into that set.
	*  The Outer will be Package.<AnimSetName>_Group.
	*  @param bCreateIfNotFound if true, Group will be created. This is only in the editor.
	*/
UObject* FAnimationUtils::GetDefaultAnimSequenceOuter(UAnimSet* InAnimSet, bool bCreateIfNotFound)
{
	check( InAnimSet );

#if WITH_EDITORONLY_DATA
	for(int32 i=0; i<InAnimSet->Sequences.Num(); i++)
	{
		UAnimSequence* TestAnimSeq = InAnimSet->Sequences[i];
		// Make sure outer is not current AnimSet, but they should be in the same package.
		if( TestAnimSeq && TestAnimSeq->GetOuter() != InAnimSet && TestAnimSeq->GetOutermost() == InAnimSet->GetOutermost() )
		{
			return TestAnimSeq->GetOuter();
		}
	}
#endif	//#if WITH_EDITORONLY_DATA

	// Otherwise go ahead and create a new one if we should.
	if( bCreateIfNotFound )
	{
		// We can only create Group if we are within the editor.
		check(GIsEditor);

		UPackage* AnimSetPackage = InAnimSet->GetOutermost();
		// Make sure package is fully loaded.
		AnimSetPackage->FullyLoad();

		// Try to create a new package with Group named <AnimSetName>_Group.
		FString NewPackageString = FString::Printf(TEXT("%s.%s_Group"), *AnimSetPackage->GetFName().ToString(), *InAnimSet->GetFName().ToString());
		UPackage* NewPackage = CreatePackage( NULL, *NewPackageString );

		// New Outer to use
		return NewPackage;
	}

	return NULL;
}


/**
 * Converts an animation compression type into a human readable string
 *
 * @param	InFormat	The compression format to convert into a string
 * @return				The format as a string
 */
FString FAnimationUtils::GetAnimationCompressionFormatString(AnimationCompressionFormat InFormat)
{
	switch(InFormat)
	{
	case ACF_None:
		return FString(TEXT("ACF_None"));
	case ACF_Float96NoW:
		return FString(TEXT("ACF_Float96NoW"));
	case ACF_Fixed48NoW:
		return FString(TEXT("ACF_Fixed48NoW"));
	case ACF_IntervalFixed32NoW:
		return FString(TEXT("ACF_IntervalFixed32NoW"));
	case ACF_Fixed32NoW:
		return FString(TEXT("ACF_Fixed32NoW"));
	case ACF_Float32NoW:
		return FString(TEXT("ACF_Float32NoW"));
	case ACF_Identity:
		return FString(TEXT("ACF_Identity"));
	default:
		UE_LOG(LogAnimation, Warning, TEXT("AnimationCompressionFormat was not found:  %i"), static_cast<int32>(InFormat) );
	}

	return FString(TEXT("Unknown"));
}

/**
 * Converts an animation codec format into a human readable string
 *
 * @param	InFormat	The format to convert into a string
 * @return				The format as a string
 */
FString FAnimationUtils::GetAnimationKeyFormatString(AnimationKeyFormat InFormat)
{
	switch(InFormat)
	{
	case AKF_ConstantKeyLerp:
		return FString(TEXT("AKF_ConstantKeyLerp"));
	case AKF_VariableKeyLerp:
		return FString(TEXT("AKF_VariableKeyLerp"));
	case AKF_PerTrackCompression:
		return FString(TEXT("AKF_PerTrackCompression"));
	default:
		UE_LOG(LogAnimation, Warning, TEXT("AnimationKeyFormat was not found:  %i"), static_cast<int32>(InFormat) );
	}

	return FString(TEXT("Unknown"));
}


/**
 * Computes the 'height' of each track, relative to a given animation linkup.
 *
 * The track height is defined as the minimal number of bones away from an end effector (end effectors are 0, their parents are 1, etc...)
 *
  * @param BoneData				The bone data to check
 * @param NumTracks				The number of tracks
 * @param TrackHeights [OUT]	The computed track heights
 *
 */
void FAnimationUtils::CalculateTrackHeights(UAnimSequence* AnimSeq, const TArray<FBoneData>& BoneData, int32 NumTracks, TArray<int32>& TrackHeights)
{
	TrackHeights.Empty();
	TrackHeights.AddUninitialized(NumTracks);
	for (int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
	{
		TrackHeights[TrackIndex] = 0;
	}

	USkeleton * Skeleton = AnimSeq->GetSkeleton();
	check(Skeleton);

	// Populate the bone 'height' table (distance from closest end effector, with 0 indicating an end effector)
	// setup the raw bone transformation and find all end effectors
	for (int32 BoneIndex = 0; BoneIndex < BoneData.Num(); ++BoneIndex)
	{
		// also record all end-effectors we find
		const FBoneData& Bone = BoneData[BoneIndex];
		if (Bone.IsEndEffector())
		{
			const FBoneData& EffectorBoneData = BoneData[BoneIndex];

			for (int32 FamilyIndex = 0; FamilyIndex < EffectorBoneData.BonesToRoot.Num(); ++FamilyIndex)
			{
				const int32 NextParentBoneIndex = EffectorBoneData.BonesToRoot[FamilyIndex];
				const int32 NextParentTrackIndex = Skeleton->GetAnimationTrackIndex(NextParentBoneIndex, AnimSeq);
				if (NextParentTrackIndex != INDEX_NONE)
				{
					const int32 CurHeight = TrackHeights[NextParentTrackIndex];
					TrackHeights[NextParentTrackIndex] = (CurHeight > 0) ? FMath::Min<int32>(CurHeight, (FamilyIndex+1)) : (FamilyIndex+1);
				}
			}
		}
	}
}

/**
 * Checks a set of key times to see if the spacing is uniform or non-uniform.
 * Note: If there are as many times as frames, they are automatically assumed to be uniformly spaced.
 * Note: If there are two or fewer times, they are automatically assumed to be uniformly spaced.
 *
 * @param AnimSeq		The animation sequence the Times array is associated with
 * @param Times			The array of key times
 *
 * @return				true if the keys are uniformly spaced (or one of the trivial conditions is detected).  false if any key spacing is greater than 1e-4 off.
 */
bool FAnimationUtils::HasUniformKeySpacing(UAnimSequence* AnimSeq, const TArray<float>& Times)
{
	if ((Times.Num() <= 2) || (Times.Num() == AnimSeq->NumFrames))
	{
		return true;
	}

	float FirstDelta = Times[1] - Times[0];
	for (int32 i = 2; i < Times.Num(); ++i)
	{
		float DeltaTime = Times[i] - Times[i-1];

		if (fabs(DeltaTime - FirstDelta) > KINDA_SMALL_NUMBER)
		{
			return false;
		}
	}

	return false;
}

/**
 * Perturbs the bone(s) associated with each track in turn, measuring the maximum error introduced in end effectors as a result
 */
void FAnimationUtils::TallyErrorsFromPerturbation(
	const UAnimSequence* AnimSeq,
	int32 NumTracks,
	const TArray<FBoneData>& BoneData,
	const FVector& PositionNudge,
	const FQuat& RotationNudge,
	const FVector& ScaleNudge,
	TArray<FAnimPerturbationError>& InducedErrors)
{
	const float TimeStep = (float)AnimSeq->SequenceLength / (float)AnimSeq->NumFrames;
	const int32 NumBones = BoneData.Num();


	USkeleton * Skeleton = AnimSeq->GetSkeleton();
	check ( Skeleton );

	const TArray<FTransform>& RefPose = Skeleton->GetRefLocalPoses();

	TArray<FTransform> RawAtoms;
	TArray<FTransform> NewAtomsT;
	TArray<FTransform> NewAtomsR;
	TArray<FTransform> NewAtomsS;
	TArray<FTransform> RawTransforms;
	TArray<FTransform> NewTransformsT;
	TArray<FTransform> NewTransformsR;
	TArray<FTransform> NewTransformsS;

	RawAtoms.AddZeroed(NumBones);
	NewAtomsT.AddZeroed(NumBones);
	NewAtomsR.AddZeroed(NumBones);
	NewAtomsS.AddZeroed(NumBones);
	RawTransforms.AddZeroed(NumBones);
	NewTransformsT.AddZeroed(NumBones);
	NewTransformsR.AddZeroed(NumBones);
	NewTransformsS.AddZeroed(NumBones);

	InducedErrors.AddUninitialized(NumTracks);

	FTransform Perturbation(RotationNudge, PositionNudge, ScaleNudge);

	for (int32 TrackUnderTest = 0; TrackUnderTest < NumTracks; ++TrackUnderTest)
	{
		float MaxErrorT_DueToT = 0.0f;
		float MaxErrorR_DueToT = 0.0f;
		float MaxErrorS_DueToT = 0.0f;
		float MaxErrorT_DueToR = 0.0f;
		float MaxErrorR_DueToR = 0.0f;
		float MaxErrorS_DueToR = 0.0f;
		float MaxErrorT_DueToS = 0.0f;
		float MaxErrorR_DueToS = 0.0f;
		float MaxErrorS_DueToS = 0.0f;
		
		// for each whole increment of time (frame stepping)
		for (float Time = 0.0f; Time < AnimSeq->SequenceLength; Time += TimeStep)
		{
			// get the raw and compressed atom for each bone
			for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
			{
				const int32 TrackIndex = Skeleton->GetAnimationTrackIndex(BoneIndex, AnimSeq);

				if (TrackIndex == INDEX_NONE)
				{
					// No track for the bone was found, so use the reference pose.
					RawAtoms[BoneIndex]	= RefPose[BoneIndex];
					NewAtomsT[BoneIndex] = RawAtoms[BoneIndex];
					NewAtomsR[BoneIndex] = RawAtoms[BoneIndex];
					NewAtomsS[BoneIndex] = RawAtoms[BoneIndex];
				}
				else
				{
					AnimSeq->GetBoneTransform(RawAtoms[BoneIndex], TrackIndex, Time, false, true);

					NewAtomsT[BoneIndex] = RawAtoms[BoneIndex];
					NewAtomsR[BoneIndex] = RawAtoms[BoneIndex];
					NewAtomsS[BoneIndex] = RawAtoms[BoneIndex];

					// Perturb the bone under test
					if (TrackIndex == TrackUnderTest)
					{
						NewAtomsT[BoneIndex].AddToTranslation(PositionNudge);

						FQuat NewR = NewAtomsR[BoneIndex].GetRotation();
						NewR += RotationNudge;
						NewR.Normalize();
						NewAtomsR[BoneIndex].SetRotation(NewR);

						FVector Scale3D = NewAtomsS[BoneIndex].GetScale3D();
						NewAtomsS[BoneIndex].SetScale3D( Scale3D + ScaleNudge);
					}
				}

				RawTransforms[BoneIndex] = RawAtoms[BoneIndex];
				NewTransformsT[BoneIndex] = NewAtomsT[BoneIndex];
				NewTransformsR[BoneIndex] = NewAtomsR[BoneIndex];
				NewTransformsS[BoneIndex] = NewAtomsS[BoneIndex];

				// For all bones below the root, final component-space transform is relative transform * component-space transform of parent.
				if ( BoneIndex > 0 )
				{
					const int32 ParentIndex = Skeleton->GetReferenceSkeleton().GetParentIndex(BoneIndex);

					// Check the precondition that parents occur before children in the RequiredBones array.
					check( ParentIndex != INDEX_NONE );
					check( ParentIndex < BoneIndex );

					RawTransforms[BoneIndex] *= RawTransforms[ParentIndex];
					NewTransformsT[BoneIndex] *= NewTransformsT[ParentIndex];
					NewTransformsR[BoneIndex] *= NewTransformsR[ParentIndex];
					NewTransformsS[BoneIndex] *= NewTransformsS[ParentIndex];
				}

				// Only look at the error that occurs in end effectors
				if (BoneData[BoneIndex].IsEndEffector())
				{
					MaxErrorT_DueToT = FMath::Max(MaxErrorT_DueToT, (RawTransforms[BoneIndex].GetLocation() - NewTransformsT[BoneIndex].GetLocation()).Size());
					MaxErrorT_DueToR = FMath::Max(MaxErrorT_DueToR, (RawTransforms[BoneIndex].GetLocation() - NewTransformsR[BoneIndex].GetLocation()).Size());
					MaxErrorT_DueToS = FMath::Max(MaxErrorT_DueToS, (RawTransforms[BoneIndex].GetLocation() - NewTransformsS[BoneIndex].GetLocation()).Size());
					MaxErrorR_DueToT = FMath::Max(MaxErrorR_DueToT, FQuat::ErrorAutoNormalize(RawTransforms[BoneIndex].GetRotation(), NewTransformsT[BoneIndex].GetRotation()));
					MaxErrorR_DueToR = FMath::Max(MaxErrorR_DueToR, FQuat::ErrorAutoNormalize(RawTransforms[BoneIndex].GetRotation(), NewTransformsR[BoneIndex].GetRotation()));
					MaxErrorR_DueToS = FMath::Max(MaxErrorR_DueToS, FQuat::ErrorAutoNormalize(RawTransforms[BoneIndex].GetRotation(), NewTransformsS[BoneIndex].GetRotation()));
					MaxErrorS_DueToT = FMath::Max(MaxErrorS_DueToT, (RawTransforms[BoneIndex].GetScale3D() - NewTransformsT[BoneIndex].GetScale3D()).Size());
					MaxErrorS_DueToR = FMath::Max(MaxErrorS_DueToR, (RawTransforms[BoneIndex].GetScale3D() - NewTransformsR[BoneIndex].GetScale3D()).Size());
					MaxErrorS_DueToS = FMath::Max(MaxErrorS_DueToS, (RawTransforms[BoneIndex].GetScale3D() - NewTransformsS[BoneIndex].GetScale3D()).Size());
				}
			} // for each bone
		} // for each time

		// Save the worst errors
		FAnimPerturbationError& TrackError = InducedErrors[TrackUnderTest];
		TrackError.MaxErrorInTransDueToTrans = MaxErrorT_DueToT;
		TrackError.MaxErrorInRotDueToTrans = MaxErrorR_DueToT;
		TrackError.MaxErrorInScaleDueToTrans = MaxErrorS_DueToT;
		TrackError.MaxErrorInTransDueToRot = MaxErrorT_DueToR;
		TrackError.MaxErrorInRotDueToRot = MaxErrorR_DueToR;
		TrackError.MaxErrorInScaleDueToRot = MaxErrorS_DueToR;
		TrackError.MaxErrorInTransDueToScale = MaxErrorT_DueToR;
		TrackError.MaxErrorInRotDueToScale = MaxErrorR_DueToR;
		TrackError.MaxErrorInScaleDueToScale = MaxErrorS_DueToR;
	}
}
