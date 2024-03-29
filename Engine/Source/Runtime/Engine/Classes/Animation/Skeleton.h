// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/** 
 * This is the definition for a skeleton, used to animate USkeletalMesh
 *
 */

#pragma once
#include "Core.h"
#include "Delegate.h"
#include "PreviewAssetAttachComponent.h"
#include "Skeleton.generated.h"


/** This is a mapping table between bone in a particular skeletal mesh and bone of this skeleton set. */
USTRUCT()
struct FSkeletonToMeshLinkup
{
	GENERATED_USTRUCT_BODY()

	/** 
	 * Mapping table. Size must be same as size of bone tree (not Mesh Ref Pose). 
	 * No index should be more than the number of bones in this skeleton
	 * -1 indicates no match for this bone - will be ignored.
	 */
	UPROPERTY()
	TArray<int32> SkeletonToMeshTable;

	/** 
	 * Mapping table. Size must be same as size of ref pose (not bone tree). 
	 * No index should be more than the number of bones in this skeletalmesh
	 * -1 indicates no match for this bone - will be ignored.
	 */
	UPROPERTY()
	TArray<int32> MeshToSkeletonTable;

};

/** Bone translation retargeting mode. */
UENUM()
namespace EBoneTranslationRetargetingMode
{
	enum Type
	{
		/** Use translation from animation data. */
		Animation,
		/** Use fixed translation from Skeleton. */
		Skeleton,
		/** Use Translation from animation, but scale length by Skeleton's proportions. */
		AnimationScaled,
	};
}

/** Each Bone node in BoneTree **/
USTRUCT()
struct FBoneNode
{
	GENERATED_USTRUCT_BODY()

	/** Name of bone, this is the search criteria to match with mesh bone. This will be NAME_None if deleted **/
	UPROPERTY()
	FName Name_DEPRECATED;

	/** Parent Index. -1 if not used. The root has 0 as its parent. Do not delete the element but set this to -1. If it is revived by other reason, fix up this link. **/
	UPROPERTY()
	int32 ParentIndex_DEPRECATED;

	/** Retargeting Mode for Translation Component */
	UPROPERTY(EditAnywhere, Category=BoneNode)
	TEnumAsByte<EBoneTranslationRetargetingMode::Type> TranslationRetargetingMode;

	FBoneNode()
		: ParentIndex_DEPRECATED(INDEX_NONE)
		, TranslationRetargetingMode(EBoneTranslationRetargetingMode::Animation)
	{
	}

	FBoneNode(FName InBoneName, int32 InParentIndex)
		: Name_DEPRECATED(InBoneName)
		, ParentIndex_DEPRECATED(InParentIndex)
		, TranslationRetargetingMode(EBoneTranslationRetargetingMode::Animation)
	{
	}
};

/** This is a mapping table between bone in a particular skeletal mesh and bone of this skeleton set. */
USTRUCT()
struct FReferencePose
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName	PoseName;

	UPROPERTY()
	TArray<FTransform>	ReferencePose;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	USkeletalMesh*	ReferenceMesh;
#endif

	/**
	 * Serializes the bones
	 *
	 * @param Ar - The archive to serialize into.
	 * @param Rect - The bone container to serialize.
	 *
	 * @return Reference to the Archive after serialization.
	 */
	friend FArchive& operator<<(FArchive& Ar, FReferencePose & P);
};

USTRUCT()
struct FBoneReductionSetting
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FName> BonesToRemove;

	bool Add(FName BoneName)
	{
		if ( BoneName!=NAME_None && !BonesToRemove.Contains(BoneName) )
		{
			BonesToRemove.Add(BoneName);
			return true;
		}

		return false;
	}

	void Remove(FName BoneName)
	{
		BonesToRemove.Remove(BoneName);
	}

	bool Contains(FName BoneName)
	{
		return (BonesToRemove.Contains(BoneName));
	}
};
/**
 *	USkeleton : that links between mesh and animation
 *		- Bone hierarchy for animations
 *		- Bone/track linkup between mesh and animation
 *		- Retargetting related
 *		- Mirror table
 */
UCLASS(hidecategories=Object, MinimalAPI)
class USkeleton : public UObject
{
	GENERATED_UCLASS_BODY()

private:
	/** Skeleton bone tree - each contains name and parent index**/
	UPROPERTY(VisibleAnywhere, Category=Skeleton)
	TArray<struct FBoneNode> BoneTree;

	/** Reference skeleton poses in local space */
	UPROPERTY()
	TArray<FTransform> RefLocalPoses_DEPRECATED;

	/** Reference Skeleton */
	FReferenceSkeleton ReferenceSkeleton;

	/** Guid for skeleton */
	FGuid Guid;

	/** Conversion function. Remove when VER_UE4_REFERENCE_SKELETON_REFACTOR is removed. */
	void ConvertToFReferenceSkeleton();

public:
	/** Accessor to Reference Skeleton to make data read only */
	const FReferenceSkeleton & GetReferenceSkeleton() const
	{
		return ReferenceSkeleton;
	}

	/** Non-serialised cache of linkups between different skeletal meshes and this Skeleton. */
	UPROPERTY(transient)
	TArray<struct FSkeletonToMeshLinkup> LinkupCache;

	/** 
	 *	Array of named socket locations, set up in editor and used as a shortcut instead of specifying 
	 *	everything explicitly to AttachComponent in the SkeletalMeshComponent.
	 */
	UPROPERTY()
	TArray<class USkeletalMeshSocket*> Sockets;

	/** Serializable retarget sources for this skeleton **/
	TMap< FName, FReferencePose > AnimRetargetSources;

#if WITH_EDITORONLY_DATA
private:
	/** The default skeletal mesh to use when previewing this skeleton */
	UPROPERTY(duplicatetransient, AssetRegistrySearchable)
	TAssetPtr<class USkeletalMesh> PreviewSkeletalMesh;

public:

	/** AnimNotifiers that has been created. Right now there is no delete step for this, but in the future we'll supply delete**/
	UPROPERTY()
	TArray<FName> AnimationNotifies;

	/* Attached assets component for this skeleton */
	UPROPERTY()
	FPreviewAssetAttachContainer PreviewAttachedAssetContainer;

	UPROPERTY()
	TArray< struct FBoneReductionSetting > BoneReductionSettingsForLODs;
#endif // WITH_EDITORONLY_DATA

private:
	DECLARE_MULTICAST_DELEGATE( FOnRetargetSourceChangedMulticaster )
	FOnRetargetSourceChangedMulticaster OnRetargetSourceChanged;

public:
	typedef FOnRetargetSourceChangedMulticaster::FDelegate FOnRetargetSourceChanged;

	/** Registers a delegate to be called after the preview animation has been changed */
	void RegisterOnRetargetSourceChanged(const FOnRetargetSourceChanged& Delegate)
	{
		OnRetargetSourceChanged.Add(Delegate);
	}

	const FGuid GetGuid() const
	{
		return Guid;
	}
	/** Unregisters a delegate to be called after the preview animation has been changed */
	void UnregisterOnRetargetSourceChanged(const FOnRetargetSourceChanged& Delegate)
	{
		OnRetargetSourceChanged.Remove(Delegate);
	}

	void CallbackRetargetSourceChanged()
	{
		OnRetargetSourceChanged.Broadcast();
	}


	typedef TArray<FBoneNode> FBoneTreeType;

	/** Runtime built mapping table between SkeletalMeshes, and LinkupCache array indices. */
	TMap<TAutoWeakObjectPtr<class USkeletalMesh>, int32> SkelMesh2LinkupCache;

#if WITH_EDITORONLY_DATA

	// @todo document
	void CollectAnimationNotifies();

	// @todo document
	ENGINE_API void AddNewAnimationNotify(FName NewAnimNotifyName);

	/** Returns the skeletons preview mesh, loading it if necessary */
	ENGINE_API USkeletalMesh* GetPreviewMesh(bool bFindIfNotSet=false);

	/** Returns the skeletons preview mesh, loading it if necessary */
	ENGINE_API void SetPreviewMesh(USkeletalMesh* PreviewMesh, bool bMarkAsDirty=true);

	/**
	 * Makes sure all attached objects are valid and removes any that aren't.
	 *
	 * @return		NumberOfBrokenAssets
	 */
	ENGINE_API int32 ValidatePreviewAttachedObjects();

	/**
	 * Get List of Child Bones of the ParentBoneIndex
	 *
	 * @param	Parent Bone Index
	 * @param	(out) List of Direct Children
	 */
	ENGINE_API int32 GetChildBones(int32 ParentBoneIndex, TArray<int32> & Children) const;

	/**
	 * Remove Bone from the LOD
	 *
	 * @param	LODIndex	LOD to remove from
	 * @param 	BoneIndex	Bone to remove
	 */
	ENGINE_API int32 RemoveBoneFromLOD(int32 LODIndex, int32 BoneIndex);
	
	/**
	 * Return true if this bone is included in LOD. 
	 * In other words, this returns true if it's not in BoneReductionSettingsForLODs
	 *
	 * @param	LODIndex	LOD to check
	 * @param 	BoneIndex	Bone to check
	 */
	ENGINE_API bool IsBoneIncludedInLOD(int32 LODIndex, int32 BoneIndex);

	/**
	 * Add Bone back to the LOD
	 *
	 * @param	LODIndex	LOD to add back to 
	 * @param 	BoneIndex	Bone to add back
	 */	
	ENGINE_API void AddBoneToLOD(int32 LODIndex, int32 BoneIndex);

#endif

	/**
	 *	Check if this skeleton may be used with other skeleton
	 */
	ENGINE_API bool IsCompatible(USkeleton const * InSkeleton) const { return (InSkeleton && this == InSkeleton); }

	/** 
	 * Indexing naming convention
	 * 
	 * Since this code has indexing to very two distinct array but it can be confusing so I am making it consistency for naming
	 * 
	 * First index is SkeletalMesh->RefSkeleton index - I call this RefBoneIndex
	 * Second index is BoneTree index in USkeleton - I call this TreeBoneIndex
	 */

	/**
	 * Verify to see if we can match this skeleton with the provided SkeletalMesh.
	 * 
	 * Returns true 
	 *		- if bone hierarchy matches (at least needs to have matching parent) 
	 *		- and if parent chain matches - meaning if bone tree has A->B->C and if ref pose has A->C, it will fail
	 *		- and if more than 50 % of bones matches
	 *  
	 * @param	InSkelMesh	SkeletalMesh to compare the Skeleton against.
	 * 
	 * @return				true if animation set can play on supplied SkeletalMesh, false if not.
	 */
	ENGINE_API bool IsCompatibleMesh(USkeletalMesh * InSkelMesh) const;

	/** Clears all cache data **/
	void ClearCacheData();

	/** 
	 * Find a mesh linkup table (mapping of skeleton bone tree indices to refpose indices) for a particular SkeletalMesh
	 * If one does not already exist, create it now.
	 */
	ENGINE_API int32 GetMeshLinkupIndex(const USkeletalMesh* InSkelMesh);

	/** 
	 * Merge Bones (RequiredBones from InSkelMesh) to BoneTrees if not exists
	 * 
	 * Note that this bonetree can't ever clear up because doing so will corrupt all animation data that was imported based on this
	 * If nothing exists, it will build new bone tree 
	 * 
	 * @param InSkelMesh			: Mesh to build from. 
	 * @param RequiredRefBones		: RequiredBones are subset of list of bones (index to InSkelMesh->RefSkeleton)
									Most of cases, you don't like to add all bones to skeleton, so you'll have choice of cull out some
	 * 
	 * @return true if success
	 */
	ENGINE_API bool MergeBonesToBoneTree(USkeletalMesh* InSkeletalMesh, const TArray<int32> &RequiredRefBones);

	/** 
	 * Merge all Bones to BoneTrees if not exists
	 * 
	 * Note that this bonetree can't ever clear up because doing so will corrupt all animation data that was imported based on this
	 * If nothing exists, it will build new bone tree 
	 * 
	 * @param InSkelMesh			: Mesh to build from. 
	 * 
	 * @return true if success
	 */
	ENGINE_API bool MergeAllBonesToBoneTree(USkeletalMesh* InSkelMesh);

	/** 
	 * Merge has failed, then Recreate BoneTree
	 * 
	 * This will invalidate all animations that were linked before, but this is needed 
	 * 
	 * @param InSkelMesh			: Mesh to build from. 
	 * 
	 * @return true if success
	 */
	ENGINE_API bool RecreateBoneTree(USkeletalMesh* InSkelMesh);

	/** This is const accessor for BoneTree
	 *  Understand there will be a lot of need to access BoneTree, but 
	 *	Anybody modifying BoneTree will corrupt animation data, so will need to make sure it's not modifiable outside of Skeleton
	 *	You can add new BoneNode but you can't modify current list. The index will be referenced by Animation data.
	 */
	const TArray<FBoneNode> & GetBoneTree()	const
	{ 
		return BoneTree;	
	}

	// @todo document
	const TArray<FTransform> & GetRefLocalPoses( FName RetargetSource = NAME_None ) const 
	{
		if ( RetargetSource != NAME_None ) 
		{
			const FReferencePose * FoundRetargetSource = AnimRetargetSources.Find(RetargetSource);
			if (FoundRetargetSource)
			{
				return FoundRetargetSource->ReferencePose;
			}
		}

		return ReferenceSkeleton.GetRefBonePose();	
	}

	/** 
	 * Rebuild reference pose from the list of mesh 
	 * Priority goes from 0->N, where 0 is highest
	 * Sequential index will be used to fill up bones not found from previous index
	 */
	ENGINE_API void RebuildReferencePose(TArray<USkeletalMesh*> InSkelMeshList);

	/** 
	 * Get Track index of InAnimSeq for the BoneTreeIndex of BoneTree
	 * this is slow, and it's not supposed to be used heavily
	 * @param	InBoneTreeIdx	BoneTree Index
	 * @param	InAnimSeq		Animation Sequence to get track index for 
	 *
	 * @return	Index of Track of Animation Sequence
	 */
	ENGINE_API int32 GetAnimationTrackIndex(const int32 & InSkeletonBoneIndex, const UAnimSequence * InAnimSeq);

	/** 
	 * Get Bone Tree Index from Reference Bone Index
	 * @param	InSkelMesh	SkeletalMesh for the ref bone idx
	 * @param	InRefBoneIdx	Reference Bone Index to look for - index of USkeletalMesh.RefSkeleton
	 * @return	Index of BoneTree Index
	 */
	ENGINE_API int32 GetSkeletonBoneIndexFromMeshBoneIndex(const USkeletalMesh * InSkelMesh, const int32 & MeshBoneIndex);

	/** 
	 * Get Reference Bone Index from Bone Tree Index
	 * @param	InSkelMesh	SkeletalMesh for the ref bone idx
	 * @param	InBoneTreeIdx	Bone Tree Index to look for - index of USkeleton.BoneTree
	 * @return	Index of BoneTree Index
	 */
	ENGINE_API int32 GetMeshBoneIndexFromSkeletonBoneIndex(const USkeletalMesh * InSkelMesh, const int32 & SkeletonBoneIndex);

	EBoneTranslationRetargetingMode::Type GetBoneTranslationRetargetingMode(const int32 & BoneTreeIdx) const
	{
		return BoneTree[BoneTreeIdx].TranslationRetargetingMode;
	}

	ENGINE_API FString GetRetargetingModeString(const EBoneTranslationRetargetingMode::Type & RetargetingMode) const;
	
	/** 
	 * Rebuild Look up between SkelMesh to BoneTree - this should only get called when SkelMesh is re-imported or so, where the mapping may be no longer valid
	 *
	 * @param	InSkelMesh	: SkeletalMesh to build look up for
	 */
	void RebuildLinkup(const USkeletalMesh* InSkelMesh);

	ENGINE_API void SetBoneTranslationRetargetingMode(const int32 & BoneIndex, EBoneTranslationRetargetingMode::Type NewRetargetingMode, bool bChildrenToo=false);

	virtual void PostLoad() OVERRIDE;
	virtual void PostDuplicate(bool bDuplicateForPIE) OVERRIDE;
	virtual void PostInitProperties() OVERRIDE;
	virtual void Serialize(FArchive& Ar) OVERRIDE;

	/** 
	 * Create RefLocalPoses from InSkelMesh
	 * 
	 * If bClearAll is false, it will overwrite ref pose of bones that are found in InSkelMesh
	 * If bClearAll is true, it will reset all Reference Poses 
	 * Note that this means it will remove transforms of extra bones that might not be found in this skeletalmesh
	 *
	 * @return true if successful. false if skeletalmesh wasn't compatible with the bone hierarchy
	 */
	ENGINE_API void UpdateReferencePoseFromMesh(const USkeletalMesh * InSkelMesh);

#if WITH_EDITORONLY_DATA
	/**
	 * Update Retarget Source with given name
	 *
	 * @param Name	Name of pose to update
	 */
	ENGINE_API void UpdateRetargetSource( const FName Name );
#endif
protected:
	/** 
	 * Check if Parent Chain Matches between BoneTree, and SkelMesh 
	 * Meaning if BoneTree has A->B->C (top to bottom) and if SkelMesh has A->C
	 * It will fail since it's missing B
	 * We ensure this chain matches to play animation properly
	 *
	 * @param StartBoneIndex	: BoneTreeIndex to start from in BoneTree 
	 * @param InSkelMesh		: SkeletalMesh to compare
	 *
	 * @return true if matches till root. false if not. 
	 */
	bool DoesParentChainMatch(int32 StartBoneTreeIndex, USkeletalMesh* InSkelMesh) const;

	/** 
	 * Build Look up between SkelMesh to BoneTree
	 *
	 * @param	InSkelMesh	: SkeletalMesh to build look up for
	 * @return	Index of LinkupCache that this SkelMesh is linked to 
	 */
	int32 BuildLinkup(const USkeletalMesh* InSkelMesh);

#if WITH_EDITORONLY_DATA
	/**
	 * Refresh All Retarget Sources
	 */
	void RefreshAllRetargetSources();
#endif
	/**
	 * Create Reference Skeleton From the given Mesh 
	 * 
	 * @param InSkeletonMesh	SkeletalMesh that this Skeleton is based on
	 * @param RequiredRefBones	List of required bones to create skeleton from
	 *
	 * @return true if successful
	 */
	bool CreateReferenceSkeletonFromMesh(const USkeletalMesh * InSkeletalMesh, const TArray<int32> & RequiredRefBones);

#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE( FOnSkeletonHierarchyChangedMulticaster );
	FOnSkeletonHierarchyChangedMulticaster OnSkeletonHierarchyChanged;

	/** Call this when the skeleton has changed to fix dependent assets */
	void HandleSkeletonHierarchyChange();

public:
	typedef FOnSkeletonHierarchyChangedMulticaster::FDelegate FOnSkeletonHierarchyChanged;

	/** Registers a delegate to be called after notification has changed*/
	ENGINE_API void RegisterOnSkeletonHierarchyChanged(const FOnSkeletonHierarchyChanged& Delegate);
	ENGINE_API void UnregisterOnSkeletonHierarchyChanged(void * Unregister);

	/** Removes the supplied bones from the skeleton */
	ENGINE_API void RemoveBonesFromSkeleton(const TArray<FName>& BonesToRemove, bool bRemoveChildBones);

	static const FName AnimNotifyTag;
	static const TCHAR AnimNotifyTagDeliminator;
#endif
private:
	void RegenerateGuid();
};

