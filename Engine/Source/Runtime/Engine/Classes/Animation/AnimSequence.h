// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * One animation sequence of keyframes. Contains a number of tracks of data.
 *
 */

#include "AnimSequenceBase.h"
#include "AnimSequence.generated.h"

typedef TArray<FTransform> FTransformArrayA2;

class USkeletalMesh;

/**
 * Indicates animation data compression format.
 */
UENUM()
enum AnimationCompressionFormat
{
	ACF_None,
	ACF_Float96NoW,
	ACF_Fixed48NoW,
	ACF_IntervalFixed32NoW,
	ACF_Fixed32NoW,
	ACF_Float32NoW,
	ACF_Identity,
	ACF_MAX,
};

/**
 * Indicates animation data compression format.
 */
UENUM()
enum AnimationKeyFormat
{
	AKF_ConstantKeyLerp,
	AKF_VariableKeyLerp,
	AKF_PerTrackCompression,
	AKF_MAX,
};

/** 
 * AdditiveAnimationType
 * 
 * This dictates if this is additive or not, If it is, what kind of additive
 */
UENUM()
enum EAdditiveAnimationType
{
	/** No additive */
	AAT_None  UMETA(DisplayName="No additive"),
	/* Create Additive from LocalSpace Base */
	AAT_LocalSpaceBase UMETA(DisplayName="Local Space"),
	/* Create Additive from MeshSpace Rotation Only, Translation still will be LocalSpace */
	AAT_RotationOffsetMeshSpace UMETA(DisplayName="Mesh Space"),
	AAT_MAX,
};

/** 
 * What kind of additive animation
 */
UENUM()
enum EAdditiveBasePoseType
{
	// will be deprecated
	ABPT_None,
	// use ref pose of Skeleton as base
	ABPT_RefPose,
	// use whole animation as a base pose. Need BasePoseSeq.
	ABPT_AnimScaled,
	// use animation as a base pose. Need BasePoseSeq and RefFrameIndex (will clamp if outside).
	ABPT_AnimFrame,
	ABPT_MAX,
};

/**
 * Raw keyframe data for one track.  Each array will contain either NumFrames elements or 1 element.
 * One element is used as a simple compression scheme where if all keys are the same, they'll be
 * reduced to 1 key that is constant over the entire sequence.
 */
USTRUCT()
struct FRawAnimSequenceTrack
{
	GENERATED_USTRUCT_BODY()

	/** Position keys. */
	UPROPERTY()
	TArray<FVector> PosKeys;

	/** Rotation keys. */
	UPROPERTY()
	TArray<FQuat> RotKeys;

	/** Scale keys. */
	UPROPERTY()
	TArray<FVector> ScaleKeys;

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FRawAnimSequenceTrack& T )
	{
		T.PosKeys.BulkSerialize(Ar);
		T.RotKeys.BulkSerialize(Ar);

		if (Ar.UE4Ver() >= VER_UE4_ANIM_SUPPORT_NONUNIFORM_SCALE_ANIMATION)
		{
			T.ScaleKeys.BulkSerialize(Ar);
		}

		return Ar;
	}
};

/**
 *	@note we have plan to support skeleton hierarchy. 
 *	when that happens, we'd like to keep skeleton indexing
 */
USTRUCT()
struct FTrackToSkeletonMap
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	int32 SkeletonIndex_DEPRECATED;    // 0 as current Skeleton (the one above), and N is the N'th parent

	UPROPERTY()
	int32 BoneTreeIndex;    // Index of Skeleton.BoneTree this Track belongs to


	FTrackToSkeletonMap()
		: BoneTreeIndex(0)
	{
	}

	FTrackToSkeletonMap(int32 InBoneTreeIndex)
		: BoneTreeIndex(InBoneTreeIndex)
	{
	}
};

/**
 * Keyframe position data for one track.  Pos(i) occurs at Time(i).  Pos.Num() always equals Time.Num().
 */
USTRUCT()
struct FTranslationTrack
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FVector> PosKeys;

	UPROPERTY()
	TArray<float> Times;
};

/**
 * Keyframe rotation data for one track.  Rot(i) occurs at Time(i).  Rot.Num() always equals Time.Num().
 */
USTRUCT()
struct FRotationTrack
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FQuat> RotKeys;

	UPROPERTY()
	TArray<float> Times;
};

/**
 * Keyframe scale data for one track.  Scale(i) occurs at Time(i).  Rot.Num() always equals Time.Num().
 */
USTRUCT()
struct FScaleTrack
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FVector> ScaleKeys;

	UPROPERTY()
	TArray<float> Times;
};


/**
 * Key frame curve data for one track
 * CurveName: Morph Target Name
 * CurveWeights: List of weights for each frame
 */
USTRUCT()
struct FCurveTrack
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName CurveName;

	UPROPERTY()
	TArray<float> CurveWeights;

	/** Returns true if valid curve weight exists in the array*/
	ENGINE_API bool IsValidCurveTrack();
	
	/** This is very simple cut to 1 key method if all is same since I see so many redundant same value in every frame 
	 *  Eventually this can get more complicated 
	 *  Will return true if compressed to 1. Return false otherwise **/
	bool CompressCurveWeights();
};

USTRUCT()
struct FCompressedTrack
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<uint8> ByteStream;

	UPROPERTY()
	TArray<float> Times;

	UPROPERTY()
	float Mins[3];

	UPROPERTY()
	float Ranges[3];


	FCompressedTrack()
	{
		for (int32 ElementIndex = 0; ElementIndex < 3; ElementIndex++)
		{
			Mins[ElementIndex] = 0;
		}
		for (int32 ElementIndex = 0; ElementIndex < 3; ElementIndex++)
		{
			Ranges[ElementIndex] = 0;
		}
	}

};

USTRUCT()
struct FCompressedOffsetData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<int32> OffsetData;

	UPROPERTY()
	int32 StripSize;

	FCompressedOffsetData( int32 InStripSize=2 )
		: StripSize (InStripSize)
		{}

	void SetStripSize(int32 InStripSize)
	{
		ensure (InStripSize > 0 );
		StripSize = InStripSize;
	}

	const int32 GetOffsetData(int32 StripIndex, int32 Offset) const
	{
		checkSlow(OffsetData.IsValidIndex(StripIndex * StripSize + Offset));

		return OffsetData[StripIndex * StripSize + Offset];
	}

	void SetOffsetData(int32 StripIndex, int32 Offset, int32 Value)
	{
		checkSlow(OffsetData.IsValidIndex(StripIndex * StripSize + Offset));
		OffsetData[StripIndex * StripSize + Offset] = Value;
	}

	void AddUninitialized(int32 NumOfTracks)
	{
		OffsetData.AddUninitialized(NumOfTracks*StripSize);
	}

	void Empty(int32 NumOfTracks=0)
	{
		OffsetData.Empty(NumOfTracks*StripSize);
	}

	int32 GetMemorySize() const
	{
		return sizeof(int32)*OffsetData.Num() + sizeof(int32);
	}

	int32 GetNumTracks() const
	{
		return OffsetData.Num()/StripSize;
	}

	bool IsValid() const
	{
		return (OffsetData.Num() > 0);
	}
};

UCLASS(config=Engine, hidecategories=(UObject, Length), MinimalAPI, BlueprintType)
class UAnimSequence : public UAnimSequenceBase
{
	GENERATED_UCLASS_BODY()

	/** Number of raw frames in this sequence (not used by engine - just for informational purposes). */
	UPROPERTY(AssetRegistrySearchable)
	int32 NumFrames;

	/**
	 * if true, enable interpolation between last and first frame when looping.
	 */
	UPROPERTY(EditAnywhere, Category=Animation)
	uint32 bLoopingInterpolation:1;

	/**
	 * In the future, maybe keeping RawAnimSequenceTrack + TrackMap as one would be good idea to avoid inconsistent array size
	 * TrackToSkeletonMapTable(i) should contains  track mapping data for RawAnimationData(i). 
	 */
	UPROPERTY()
	TArray<struct FTrackToSkeletonMap> TrackToSkeletonMapTable;

	/**
	 * Raw uncompressed keyframe data. 
	 */
	TArray<struct FRawAnimSequenceTrack> RawAnimationData;

#if WITH_EDITORONLY_DATA
	/**
	 * This is name of tracks for editoronly - if we lose skeleton, we'll need relink them
	 */
	UPROPERTY()
	TArray<FName> AnimationTrackNames;
#endif // WITH_EDITORONLY_DATA

	/**
	 * Translation data post keyframe reduction.  TranslationData.Num() is zero if keyframe reduction
	 * has not yet been applied.
	 */
	UPROPERTY(transient)
	TArray<struct FTranslationTrack> TranslationData;

	/**
	 * Rotation data post keyframe reduction.  RotationData.Num() is zero if keyframe reduction
	 * has not yet been applied.
	 */
	UPROPERTY(transient)
	TArray<struct FRotationTrack> RotationData;


	/**
	 * Scale data post keyframe reduction.  ScaleData.Num() is zero if keyframe reduction
	 * has not yet been applied.
	 */
	UPROPERTY(transient)
	TArray<struct FScaleTrack> ScaleData;


	/*
	 * Curve data - no compression yet                                                                       
	 */
	UPROPERTY()
	TArray<struct FCurveTrack> CurveData_DEPRECATED;

#if WITH_EDITORONLY_DATA
	/**
	 * The compression scheme that was most recently used to compress this animation.
	 * May be NULL.
	 */
	UPROPERTY(EditInline, Category=Compression, VisibleAnywhere)
	class UAnimCompress* CompressionScheme;
#endif // WITH_EDITORONLY_DATA

	/** The compression format that was used to compress translation tracks. */
	UPROPERTY()
	TEnumAsByte<enum AnimationCompressionFormat> TranslationCompressionFormat;

	/** The compression format that was used to compress rotation tracks. */
	UPROPERTY()
	TEnumAsByte<enum AnimationCompressionFormat> RotationCompressionFormat;

	/** The compression format that was used to compress rotation tracks. */
	UPROPERTY()
	TEnumAsByte<enum AnimationCompressionFormat> ScaleCompressionFormat;

	/**
	 * An array of 4*NumTrack ints, arranged as follows: - PerTrack is 2*NumTrack, so this isn't true any more
	 *   [0] Trans0.Offset
	 *   [1] Trans0.NumKeys
	 *   [2] Rot0.Offset
	 *   [3] Rot0.NumKeys
	 *   [4] Trans1.Offset
	 *   . . .
	 */
	UPROPERTY()
	TArray<int32> CompressedTrackOffsets;

	/**
	 * An array of 2*NumTrack ints, arranged as follows: 
		if identity, it is offset
		if not, it is num of keys 
	 *   [0] Scale0.Offset or NumKeys
	 *   [1] Scale1.Offset or NumKeys

	 * @TODO NOTE: first implementation is offset is [0], numkeys [1]
	 *   . . .
	 */
	UPROPERTY()
	FCompressedOffsetData CompressedScaleOffsets;

	/**
	 * ByteStream for compressed animation data.
	 * All keys are currently stored at evenly-spaced intervals (ie no explicit key times).
	 *
	 * For a translation track of n keys, data is packed as n uncompressed float[3]:
	 *
	 * For a rotation track of n>1 keys, the first 24 bytes are reserved for compression info
	 * (eg Fixed32 stores float Mins[3]; float Ranges[3]), followed by n elements of the compressed type.
	 * For a rotation track of n=1 keys, the single key is packed as an FQuatFloat96NoW.
	 */
	TArray<uint8> CompressedByteStream;

	UPROPERTY()
	TEnumAsByte<enum AnimationKeyFormat> KeyEncodingFormat;

	/**
	 * The runtime interface to decode and byte swap the compressed animation
	 * May be NULL. Set at runtime - does not exist in editor
	 */
	class AnimEncoding* TranslationCodec;

	class AnimEncoding* RotationCodec;

	class AnimEncoding* ScaleCodec;

	/** Additive animation type **/
	UPROPERTY(EditAnywhere, Category=AdditiveSettings, AssetRegistrySearchable)
	TEnumAsByte<enum EAdditiveAnimationType> AdditiveAnimType;

	/* Additive refrerence pose type. Refer above enum type */
	UPROPERTY(EditAnywhere, Category=AdditiveSettings)
	TEnumAsByte<enum EAdditiveBasePoseType> RefPoseType;

	/* Additive reference animation if it's relevant - i.e. AnimScaled or AnimFrame **/
	UPROPERTY(EditAnywhere, Category=AdditiveSettings)
	class UAnimSequence* RefPoseSeq;

	/* Additve reference frame if RefPoseType == AnimFrame **/
	UPROPERTY(EditAnywhere, Category=AdditiveSettings)
	int32 RefFrameIndex;

	// Versioning Support
	/** The version of the global encoding package used at the time of import */
	UPROPERTY()
	int32 EncodingPkgVersion;

	UPROPERTY(EditAnywhere, AssetRegistrySearchable, Category=Animation)
	FName RetargetSource;

#if WITH_EDITORONLY_DATA
	/** Saved version number with CompressAnimations commandlet. To help with doing it in multiple passes. */
	UPROPERTY()
	int32 CompressCommandletVersion;

	/**
	 * Do not attempt to override compression scheme when running CompressAnimations commandlet.
	 * Some high frequency animations are too sensitive and shouldn't be changed.
	 */
	UPROPERTY(EditAnywhere, Category=Compression)
	uint32 bDoNotOverrideCompression:1;

	/**
	 * Used to track whether, or not, this sequence was compressed with it's full translation tracks
	 */
	UPROPERTY()
	uint32 bWasCompressedWithoutTranslations:1;

	/** Importing data and options used for this mesh */
	UPROPERTY(EditAnywhere, editinline, Category=Reimport)
	class UAssetImportData* AssetImportData;

	/***  for Reimport **/
	/** Path to the resource used to construct this skeletal mesh */
	UPROPERTY()
	FString SourceFilePath_DEPRECATED;

	/** Date/Time-stamp of the file from the last import */
	UPROPERTY()
	FString SourceFileTimestamp_DEPRECATED;

#endif // WITH_EDITORONLY_DATA

	// Begin UObject interface
	ENGINE_API virtual void Serialize(FArchive& Ar) OVERRIDE;
	ENGINE_API virtual void PostLoad() OVERRIDE;
#if WITH_EDITOR
	ENGINE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
#endif // WITH_EDITOR
	ENGINE_API virtual void BeginDestroy() OVERRIDE;
	ENGINE_API virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode) OVERRIDE;
	// End of UObject interface

	// Begin UAnimationAsset interface
	ENGINE_API virtual bool IsValidAdditive() const OVERRIDE;
#if WITH_EDITOR
	ENGINE_API virtual bool GetAllAnimationSequencesReferred(TArray<UAnimSequence*>& AnimationSequences) OVERRIDE;
	ENGINE_API virtual int32 GetNumberOfFrames() OVERRIDE { return NumFrames; }
#endif
	// End of UAnimationAsset interface

	// Begin UAnimSequenceBase interface
	ENGINE_API virtual void ExtractRootTrack(float Pos, FTransform & RootTransform, const FBoneContainer * RequiredBones) const OVERRIDE;
	ENGINE_API virtual void UpgradeMorphTargetCurves() OVERRIDE;
	// End UAnimSequenceBase interface

	// Begin Transform related functions 

	/** 
	 * Get Bone Transform of the Time given, relative to Parent for all RequiredBones
	 * This returns different transform based on additive or not. Or what kind of additive.
	 * 
	 * @param	OutAtoms			[out] Array of output bone transforms
	 * @param	RequiredBones		Array of Desired Tracks
	 * @param	ExtractionContext	Extraction Context (position, looping, root motion, etc.)
	 */
	void GetAnimationPose( FTransformArrayA2& OutAtoms, const FBoneContainer & RequiredBones, const FAnimExtractContext & ExtractionContext) const;

	/** 
	 * Get Bone Transform of the animation for the Time given, relative to Parent for all RequiredBones 
	 * 
	 * @param	OutAtoms			[out] Array of output bone transforms
	 * @param	RequiredBones		Array of Desired Tracks
	 * @param	ExtractionContext	Extraction Context (position, looping, root motion, etc.)
	 */
	ENGINE_API void GetBonePose( FTransformArrayA2& OutAtoms, const FBoneContainer & RequiredBones, const FAnimExtractContext & ExtractionContext) const;

private:
	/** 
	 * Utility function.
	 * When extracting root motion, this function resets the root bone to the first frame of the animation.
	 * 
	 * @param	BoneTransforms		Bone Transforms to read/write root bone transform.
	 * @param	RequiredBones		BoneContainer
	 * @param	ExtractionContext	Extraction Context to access root motion extraction information.
	 */
	void ResetRootBoneForRootMotion(FTransformArrayA2 & BoneTransforms, const FBoneContainer & RequiredBones, const FAnimExtractContext & ExtractionContext) const;
	
	/** 
	 * Retarget a single bone transform, to apply right after extraction.
	 * 
	 * @param	BoneTransform		BoneTransform to read/write from.
	 * @param	SkeletonBoneIndex	Bone Index in USkeleton.
	 * @param	PoseBoneIndex		Bone Index in Bone Transform array.
	 * @param	RequiredBones		BoneContainer
	 */	
	void RetargetBoneTransform(FTransform & BoneTransform, const int32 & SkeletonBoneIndex, const int32 & PoseBoneIndex, const FBoneContainer & RequiredBones) const;

public:
	/** 
	 * Get Bone Transform of the additive animation for the Time given, relative to Parent for all RequiredBones 
	 * 
	 * @param	OutAtoms			[out] Array of output bone transforms
	 * @param	RequiredBones		Array of Desired Tracks
	 * @param	ExtractionContext	Extraction Context (position, looping, root motion, etc.)
	 */
	ENGINE_API void GetBonePose_Additive( FTransformArrayA2& OutAtoms, const FBoneContainer & RequiredBones, const FAnimExtractContext & ExtractionContext) const;

	/** 
	 * Get Bone Transform of the base (reference) pose of the additive animation for the Time given, relative to Parent for all RequiredBones 
	 * 
	 * @param	OutAtoms			[out] Array of output bone transforms
	 * @param	RequiredBones		Array of Desired Tracks
	 * @param	ExtractionContext	Extraction Context (position, looping, root motion, etc.)
	 */
	ENGINE_API void GetAdditiveBasePose(FTransformArrayA2 & OutAtoms, const FBoneContainer & RequiredBones, const FAnimExtractContext & ExtractionContext) const;

	/**
	 * Get Bone Transform of the Time given, relative to Parent for the Track Given
	 *
	 * @param	OutAtom			[out] Output bone transform.
	 * @param	TrackIndex		Index of track to interpolate.
	 * @param	Time			Time on track to interpolate to.
	 * @param	bLooping		true if the animation is looping.
	 * @param	bUseRawData		If true, use raw animation data instead of compressed data.
	 */
	ENGINE_API void GetBoneTransform(FTransform& OutAtom, int32 TrackIndex, float Time, bool bLooping, bool bUseRawData) const;
	
	// End Transform related functions 

	// Begin Memory related functions
	/**
	 * @return		The approximate size of raw animation data.
	 */
	ENGINE_API int32 GetApproxRawSize() const;

	/**
	 * @return		The approximate size of key-reduced animation data.
	 */
	int32 GetApproxReducedSize() const;

	/**
	 * @return		The approximate size of compressed animation data.
	 */
	ENGINE_API int32 GetApproxCompressedSize() const;

	/** 
	 *  Utility function for lossless compression of a FRawAnimSequenceTrack 
	 *  @return true if keys were removed.
	 **/
	bool CompressRawAnimSequenceTrack(FRawAnimSequenceTrack& RawTrack, float MaxPosDiff, float MaxAngleDiff);

	/**
	 * Removes trivial frames -- frames of tracks when position or orientation is constant
	 * over the entire animation -- from the raw animation data.  If both position and rotation
	 * go down to a single frame, the time is stripped out as well.
	 * @return true if keys were removed.
	 */
	ENGINE_API bool CompressRawAnimData(float MaxPosDiff, float MaxAngleDiff);

	/**
	 * Removes trivial frames -- frames of tracks when position or orientation is constant
	 * over the entire animation -- from the raw animation data.  If both position and rotation
	 * go down to a single frame, the time is stripped out as well.
	 * @return true if keys were removed.
	 */
	ENGINE_API bool CompressRawAnimData();
	// End Memory related functions

	// Begin Utility functions
	/**
	 * Get Skeleton Bone Index from Track Index
	 *
	 * @param	TrackIndex		Track Index
	 */
	int32 GetSkeletonIndexFromTrackIndex(const int32 & TrackIndex) const 
	{ 
		return TrackToSkeletonMapTable[TrackIndex].BoneTreeIndex; 
	}

	
	/**
	 * Crops the raw anim data either from Start to CurrentTime or CurrentTime to End depending on
	 * value of bFromStart.  Can't be called against cooked data.
	 *
	 * @param	CurrentTime		marker for cropping (either beginning or end)
	 * @param	bFromStart		whether marker is begin or end marker
	 * @return					true if the operation was successful.
	 */
	ENGINE_API bool CropRawAnimData( float CurrentTime, bool bFromStart );

	/** Clears any data in the AnimSequence, so it can be recycled when importing a new animation with same name over it. */
	ENGINE_API void RecycleAnimSequence();

	/** 
	 * Utility function to copy all UAnimSequence properties from Source to Destination.
	 * Does not copy however RawAnimData, CompressedAnimData and AdditiveBasePose.
	 */
	static bool CopyAnimSequenceProperties(UAnimSequence* SourceAnimSeq, UAnimSequence* DestAnimSeq, bool bSkipCopyingNotifies=false);

	/** 
	 * Copy AnimNotifies from one UAnimSequence to another.
	 */
	ENGINE_API static bool CopyNotifies(UAnimSequence* SourceAnimSeq, UAnimSequence* DestAnimSeq);

	/**
	 * Flip Rotation's W For NonRoot items, and compress it again if SkelMesh exists
	 */
	ENGINE_API void FlipRotationWForNonRoot(USkeletalMesh * SkelMesh);

	/** 
	 * Return number of tracks in this animation
	 */
	int32 GetNumberOfTracks() const;

	/**
	 * Fix for new additive type - Mesh Rotation Only allowed
	 */
	ENGINE_API void FixAdditiveType();

	// End Utility functions
#if WITH_EDITOR
	/**
	 * After imported or any other change is made, call this to apply post process
	 */
	ENGINE_API void PostProcessSequence();
#endif

private:
	/** 
	 * Get Bone Transform of the animation for the Time given, relative to Parent for all RequiredBones 
	 * This return mesh rotation only additive pose
	 * 
	 * @param	OutAtoms			[out] Array of output bone transforms
	 * @param	RequiredBones		Array of Desired Tracks
	 * @param	ExtractionContext	Extraction Context (position, looping, root motion, etc.)
	 */
	void GetBonePose_AdditiveMeshRotationOnly( FTransformArrayA2& OutAtoms, const FBoneContainer & RequiredBones, const FAnimExtractContext & ExtractionContext) const;

#if WITH_EDITOR
	/**
	 * Remap Tracks to New Skeleton
	 */
	void RemapTracksToNewSkeleton( USkeleton * NewSkeleton );
	/**
	 * Remap NaN tracks from the RawAnimation data and recompress
	 */	
	void RemoveNaNTracks();
#endif

	/*
	 * Utility function that helps to remove track, you can't just remove RawAnimationData
	 */
	void RemoveTrack(int32 TrackIndex);
	friend class UAnimationAsset;
};



