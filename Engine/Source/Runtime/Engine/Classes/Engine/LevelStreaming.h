// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "LatentActions.h"

// Stream Level Action
class FStreamLevelAction : public FPendingLatentAction
{
public:
	bool			bLoading;
	bool			bMakeVisibleAfterLoad;
	bool			bShouldBlockOnLoad;
	ULevelStreaming* Level;
	FName			LevelName;

	FLatentActionInfo LatentInfo;

	FStreamLevelAction(bool bIsLoading, const FName & InLevelName, bool bIsMakeVisibleAfterLoad, bool bIsShouldBlockOnLoad, const FLatentActionInfo& InLatentInfo, UWorld* World);

	/**
	 * Given a level name, returns a short level name that will work with Play on Editor or Play on Console
	 *
	 * @param	InLevelName		Raw level name (no UEDPIE or UED<console> prefix)
	 */
	static FString MakeSafeShortLevelName( const FName& InLevelName, UWorld* InWorld );

	/**
	 * Helper function to potentially find a level streaming object by name and cache the result
	 *
	 * @param	LevelName							Name of level to search streaming object for in case Level is NULL
	 * @return	level streaming object or NULL if none was found
	 */
	static ULevelStreaming* FindAndCacheLevelStreamingObject( const FName LevelName, UWorld* InWorld );

	/**
	 * Handles "Activated" for single ULevelStreaming object.
	 *
	 * @param	LevelStreamingObject	LevelStreaming object to handle "Activated" for.
	 */
	void ActivateLevel( ULevelStreaming* LevelStreamingObject );
	/**
	 * Handles "UpdateOp" for single ULevelStreaming object.
	 *
	 * @param	LevelStreamingObject	LevelStreaming object to handle "UpdateOp" for.
	 *
	 * @return true if operation has completed, false if still in progress
	 */
	bool UpdateLevel( ULevelStreaming* LevelStreamingObject );

	virtual void UpdateOperation(FLatentResponse& Response) OVERRIDE;

#if WITH_EDITOR
	// Returns a human readable description of the latent operation's current state
	virtual FString GetDescription() const OVERRIDE;
#endif
};

#include "LevelStreaming.generated.h"

// Delegate signatures
DECLARE_DYNAMIC_MULTICAST_DELEGATE( FLevelStreamingLoadedStatus );
DECLARE_DYNAMIC_MULTICAST_DELEGATE( FLevelStreamingVisibilityStatus );

/**
 * LevelStreaming
 *
 * Abstract base class of container object encapsulating data required for streaming and providing 
 * interface for when a level should be streamed in and out of memory.
 *
 */
UCLASS(abstract, editinlinenew,MinimalAPI)
class ULevelStreaming : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Name of the level package name used for loading.																		*/
	UPROPERTY(Category=LevelStreaming, VisibleAnywhere, BlueprintReadOnly)
	FName PackageName;

	/** If this isn't Name_None, then we load from this package on disk to the new package named PackageName					*/
	UPROPERTY()
	FName PackageNameToLoad;

	/** LOD versions of this level																								*/
	UPROPERTY()
	TArray<FName> LODPackageNames;

	/** LOD package names on disk																								*/
	TArray<FName> LODPackageNamesToLoad;

	/** Transform applied to actors after loading.                                                                              */
	UPROPERTY(EditAnywhere, Category=LevelStreaming, BlueprintReadWrite)
	FTransform LevelTransform;

	/** Whether we currently have a load request pending.																		*/
	uint32 bHasLoadRequestPending:1;

	/** This streaming level was not found																						*/
	uint32 bFailedToLoad:1;

	/** Whether this level should be visible in the Editor																		*/
	UPROPERTY()
	uint32 bShouldBeVisibleInEditor:1;

	/** Whether this level is locked; that is, its actors are read-only.														*/
	UPROPERTY()
	uint32 bLocked:1;

	/** Whether the level should be loaded																						*/
	UPROPERTY(Category=LevelStreaming, BlueprintReadWrite)
	uint32 bShouldBeLoaded:1;

	/** Whether the level should be visible if it is loaded																		*/
	UPROPERTY(Category=LevelStreaming, BlueprintReadWrite)
	uint32 bShouldBeVisible:1;

	/** Whether we want to force a blocking load																				*/
	UPROPERTY(Category=LevelStreaming, BlueprintReadWrite)
	uint32 bShouldBlockOnLoad:1;

	/** Whether this level streaming object's level should be unloaded and the object be removed from the level list.			*/
	uint32 bIsRequestingUnloadAndRemoval:1;

	/** If true, will be drawn on the 'level streaming status' map (STAT LEVELMAP console command) */
	UPROPERTY(EditAnywhere, Category=LevelStreaming)
	uint32 bDrawOnLevelStatusMap:1;

	/** The level's color; used to make the level easily identifiable in the level browser, for actor level visulization, etc.	*/
	UPROPERTY(EditAnywhere, Category=LevelStreaming)
	FColor DrawColor;

	/** The level streaming volumes bound to this level.																		*/
	UPROPERTY(EditAnywhere, Category=LevelStreaming, meta=(DisplayName = "Streaming Volumes"))
	TArray<class ALevelStreamingVolume*> EditorStreamingVolumes;

	/** Cooldown time in seconds between volume-based unload requests.  Used in preventing spurious unload requests.			*/
	UPROPERTY(EditAnywhere, Category=LevelStreaming, meta=(ClampMin = "0", UIMin = "0", UIMax = "10"))
	float MinTimeBetweenVolumeUnloadRequests;

	/** Time of last volume unload request.  Used in preventing spurious unload requests.										*/
	float LastVolumeUnloadRequestTime;

	/** List of keywords to filter on in the level browser */
	UPROPERTY()
	TArray<FString> Keywords;

	// Begin UObject Interface
	virtual void PostLoad() OVERRIDE;
	virtual void Serialize( FArchive& Ar ) OVERRIDE;
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) OVERRIDE;
	
	/** Remove duplicates in EditorStreamingVolumes list*/
	void RemoveStreamingVolumeDuplicates();
#endif
	// End UObject Interface

	bool HasLoadedLevel() const
	{
		return (LoadedLevel || PendingUnloadLevel);
	}

	/** Rename package name to PIE appropriate name */
	ENGINE_API void RenameForPIE(int PIEInstanceID);

	/**
	 * Return whether this level should be present in memory which in turn tells the 
	 * streaming code to stream it in. Please note that a change in value from false 
	 * to true only tells the streaming code that it needs to START streaming it in 
	 * so the code needs to return true an appropriate amount of time before it is 
	 * needed.
	 *
	 * @param ViewLocation	Location of the viewer
	 * @return true if level should be loaded/ streamed in, false otherwise
	 */
	virtual bool ShouldBeLoaded( const FVector& ViewLocation );

	/**
	 * Return whether this level should be visible/ associated with the world if it is
	 * loaded.
	 * 
	 * @param ViewLocation	Location of the viewer
	 * @return true if the level should be visible, false otherwise
	 */
	virtual bool ShouldBeVisible( const FVector& ViewLocation );

	virtual bool ShouldBeAlwaysLoaded() const { return false; }
	
	/** Get a bounding box around the streaming volumes associated with this LevelStreaming object */
	FBox GetStreamingVolumeBounds();

	/** Gets a pointer to the LoadedLevel value */
	class ULevel* GetLoadedLevel() const {	return LoadedLevel; }
	
	/** Sets the LoadedLevel value to NULL */
	void ClearLoadedLevel() { SetLoadedLevel(NULL); }
	
	/** Gets current streaming level LOD index  */
	int32 GetLODIndex(UWorld* PersistentWorld) const;	

	/** Sets current streaming level LOD index  */
	void SetLODIndex(UWorld* PersistentWorld, int32 LODIndex);	
		
#if WITH_EDITOR
	/** Override Pre/PostEditUndo functions to handle editor transform */
	virtual void PreEditUndo();
	virtual void PostEditUndo();
#endif
	
	/** Matcher for searching streaming levels by PackageName */
	struct FPackageNameMatcher
	{
		FPackageNameMatcher( const FName& InPackageName )
			: PackageName( InPackageName )
		{
		}

		bool Matches( const ULevelStreaming* Candidate ) const
		{
			return Candidate->PackageName == PackageName;
		}

		FName PackageName;
	};

	UWorld* GetWorld() const;

	/** Returns whether streaming level is visible */
	UFUNCTION(BlueprintCallable, Category="Game")
	bool IsLevelVisible() const;

	/** Returns whether streaming level is loaded */
	UFUNCTION(BlueprintCallable, Category="Game")
	bool IsLevelLoaded() const;

	/** Creates a new instance of this streaming level with a provided unique instance name */
	UFUNCTION(BlueprintCallable, Category="Game")
	ULevelStreaming* CreateInstance(FString UniqueInstanceName);

	//==============================================================================================
	// Delegates
	
	/** Called when level is streamed in  */
	UPROPERTY(BlueprintAssignable)
	FLevelStreamingLoadedStatus			OnLevelLoaded;
	
	/** Called when level is streamed out  */
	UPROPERTY(BlueprintAssignable)
	FLevelStreamingLoadedStatus			OnLevelUnloaded;
	
	/** Called when level is added to the world  */
	UPROPERTY(BlueprintAssignable)
	FLevelStreamingVisibilityStatus		OnLevelShown;
	
	/** Called when level is removed from the world  */
	UPROPERTY(BlueprintAssignable)
	FLevelStreamingVisibilityStatus		OnLevelHidden;

	/** 
	 * Traverses all streaming level objects in the persistent world and in all inner worlds and calls appropriate delegate for streaming objects that refer specified level 
	 *
	 * @param PersistentWorld	World to traverse
	 * @param LevelPackageName	Level which loaded status was changed
	 * @param bLoaded			Whether level was loaded or unloaded
	 */
	static void BroadcastLevelLoadedStatus(UWorld* PersistentWorld, FName LevelPackageName, bool bLoaded);
	
	/** 
	 * Traverses all streaming level objects in the persistent world and in all inner worlds and calls appropriate delegate for streaming objects that refer specified level 
	 *
	 * @param PersistentWorld	World to traverse
	 * @param LevelPackageName	Level which visibility status was changed
	 * @param bVisible			Whether level become visible or not
	 */
	static void BroadcastLevelVisibleStatus(UWorld* PersistentWorld, FName LevelPackageName, bool bVisible);
	
private:
	/** @return Name of the LOD level package used for loading.																		*/
	FName GetLODPackageName(UWorld* PersistentWorld) const;

	/** @return Name of the LOD package on disk to load to the new package named PackageName, Name_None otherwise					*/
	FName GetLODPackageNameToLoad(UWorld* PersistentWorld) const;

	/** 
	 * Try to find loaded level in memory, issue a loading request otherwise
	 *
	 * @param	PersistentWorld			Persistent world
	 * @param	bAllowLevelLoadRequests	Whether to allow level load requests
	 * @param	bBlockOnLoad			Whether loading operation should block
	 * @return							true if the load request was issued or a package was already loaded
	 */
	bool RequestLevel(UWorld* PersistentWorld, bool bAllowLevelLoadRequests, bool bBlockOnLoad);
	
	/** Sets the value of LoadedLevel */
	void SetLoadedLevel(class ULevel* Level)
	{ 
		// Pending level should be unloaded or hidden at this point
		check(PendingUnloadLevel == nullptr || PendingUnloadLevel->bIsVisible == false || Level == PendingUnloadLevel);

		SetPendingUnloadLevel(LoadedLevel);

		if (LoadedLevel)
		{
			LoadedLevel->DecStreamingLevelRefs();
		}

		LoadedLevel = Level;

		if (LoadedLevel)
		{
			LoadedLevel->IncStreamingLevelRefs();
		}
	}

	/** Sets the value of PendingUnloadLevel */
	void SetPendingUnloadLevel(class ULevel* Level)
	{ 
		if (PendingUnloadLevel)
		{
			PendingUnloadLevel->DecStreamingLevelRefs();
		}

		PendingUnloadLevel = Level;

		if (PendingUnloadLevel)
		{
			PendingUnloadLevel->IncStreamingLevelRefs();
		}
	}

	void DiscardPendingUnloadLevel(UWorld* PersistentWorld);

	/** 
	 * Handler for level async loading completion 
	 *
	 * @param LevelPackage	Loaded level package
	 */
	void AsyncLevelLoadComplete( const FString& PackageName, UPackage* LevelPackage );

	/** Pointer to Level object if currently loaded/ streamed in.																*/
	UPROPERTY(transient)
	class ULevel* LoadedLevel;

	/** Pointer to replaced loaded Level object */
	UPROPERTY(transient)
	class ULevel* PendingUnloadLevel;
	
	/** Friend classes to allow access to SetLoadedLevel */
	friend class UEngine;
	friend class UEditorEngine;
	friend class UWorld;
};



