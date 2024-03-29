// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

/*=============================================================================
	World.h: UWorld definition.
=============================================================================*/

#include "CollisionQueryParams.h"
#include "WorldCollision.h"
#include "PendingNetGame.h"
#include "../GameFramework/MusicTrackDataStructures.h"
#include "World.generated.h"

class FPhysScene;

template<typename,typename> class TOctree;

/**
 * Misc. Iterator types
 *
 */
typedef TArray<TAutoWeakObjectPtr<AController> >::TConstIterator FConstControllerIterator;
typedef TArray<TAutoWeakObjectPtr<APlayerController> >::TConstIterator FConstPlayerControllerIterator;
typedef TArray<TAutoWeakObjectPtr<APawn> >::TConstIterator FConstPawnIterator;	
typedef TArray<class ULevel*>::TConstIterator FConstLevelIterator;

DECLARE_LOG_CATEGORY_EXTERN(LogSpawn, Warning, All);

DECLARE_MULTICAST_DELEGATE_OneParam(FOnActorSpawned, AActor*);


/** Proxy class that allows verification on GWorld accesses. */
class UWorldProxy
{
public:

	UWorldProxy() :
		World(NULL)
	{}

	inline UWorld* operator->()
	{
		// GWorld is changed often on the game thread when in PIE, accessing on any other thread is going to be a race condition
		// In general, the rendering thread should not dereference UObjects, unless there is a mechanism in place to make it safe
#if !EXPERIMENTAL_PARALLEL_CODE		
		checkSlow(IsInGameThread());
#endif								
		return World;
	}

	inline const UWorld* operator->() const
	{
#if !EXPERIMENTAL_PARALLEL_CODE
		checkSlow(IsInGameThread());
#endif
		return World;
	}

	inline UWorld& operator*()
	{
#if !EXPERIMENTAL_PARALLEL_CODE
		checkSlow(IsInGameThread());
#endif
		return *World;
	}

	inline const UWorld& operator*() const
	{
#if !EXPERIMENTAL_PARALLEL_CODE
		checkSlow(IsInGameThread());
#endif
		return *World;
	}

	inline UWorldProxy& operator=(UWorld* InWorld)
	{
		World = InWorld;
		return *this;
	}

	inline UWorldProxy& operator=(const UWorldProxy& InProxy)
	{
		World = InProxy.World;
		return *this;
	}

	inline bool operator==(const UWorldProxy& Other) const
	{
		return World == Other.World;
	}

	inline operator UWorld*() const
	{
#if !EXPERIMENTAL_PARALLEL_CODE
		checkSlow(IsInGameThread());
#endif
		return World;
	}

	inline UWorld* GetReference() 
	{
#if !EXPERIMENTAL_PARALLEL_CODE
		checkSlow(IsInGameThread());
#endif
		return World;
	}

private:

	UWorld* World;
};

/** class that encapsulates seamless world traveling */
class FSeamlessTravelHandler
{
private:
	/** set when a transition is in progress */
	bool bTransitionInProgress;
	/** URL we're traveling to */
	FURL PendingTravelURL;
	/** Guid of the destination map (for finding it in the package cache if autodownloaded) */
	FGuid PendingTravelGuid;
	/** whether or not we've transitioned to the entry level and are now moving on to the specified map */
	bool bSwitchedToDefaultMap;
	/** set to the loaded package once loading is complete. Transition to it is performed in the next tick where it's safe to perform the required operations */
	UObject* LoadedPackage;
	/** the world we are travelling from */
	UWorld* CurrentWorld;
	/** set to the loaded world object inside that package. This is added to the root set (so that if a GC gets in between it won't break loading) */
	UWorld* LoadedWorld;
	/** while set, pause at midpoint (after loading transition level, before loading final destination) */
	bool bPauseAtMidpoint;
	/** set when we started a new travel in the middle of a previous one and still need to clean up that previous attempt */
	bool bNeedCancelCleanUp;
	/** The context we are running in. Can be used to get the FWorldContext from Engine*/
	FName WorldContextHandle;

	/** copy data between the old world and the new world */
	void CopyWorldData();

	/** callback sent to async loading code to inform us when the level package is complete */
	void SeamlessTravelLoadCallback(const FString& PackageName, UPackage* LevelPackage);

	void SetHandlerLoadedData(UObject* InLevelPackage, UWorld* InLoadedWorld);

	/** called to kick off async loading of the destination map and any other packages it requires */
	void StartLoadingDestination();

public:
	FSeamlessTravelHandler()
		: bTransitionInProgress(false)
		, PendingTravelURL(NoInit)
		, PendingTravelGuid(0, 0, 0, 0)
		, bSwitchedToDefaultMap(false)
		, LoadedPackage(NULL)
		, CurrentWorld(NULL)
		, LoadedWorld(NULL)
		, bPauseAtMidpoint(false)
		, bNeedCancelCleanUp(false)
	{}

	/** starts traveling to the given URL. The required packages will be loaded async and Tick() will perform the transition once we are ready
	 * @param InURL the URL to travel to
	 * @param InGuid the GUID of the destination map package
	 * @return whether or not we succeeded in starting the travel
	 */
	bool StartTravel(UWorld* InCurrentWorld, const FURL& InURL, const FGuid& InGuid);

	/** @return whether a transition is already in progress */
	FORCEINLINE bool IsInTransition()
	{
		return bTransitionInProgress;
	}
	/** @return if current transition has switched to the default map; returns false if no transition is in progress */
	FORCEINLINE bool HasSwitchedToDefaultMap()
	{
		return IsInTransition() && bSwitchedToDefaultMap;
	}

	inline FString GetDestinationMapName()
	{
		return (IsInTransition() ? PendingTravelURL.Map : TEXT(""));
	}

	/** cancels transition in progress */
	void CancelTravel();

	/** turns on/off pausing after loading the transition map
	 * only valid during travel, before we've started loading the final destination
	 * @param bNowPaused - whether the transition should now be paused
	 */
	void SetPauseAtMidpoint(bool bNowPaused);

	/** 
	 * Ticks the transition; handles performing the world switch once the required packages have been loaded 
	 *
	 * @returns	The new primary world if the world has changed, null if it has not
	 */
	ENGINE_API UWorld* Tick();
};


/**
 * Helper structure encapsulating functionality used to defer marking actors and their components as pending
 * kill till right before garbage collection by registering a callback.
 */
struct ENGINE_API FLevelStreamingGCHelper
{
	/** Called when streamed out levels are going to be garbage collected  */
	DECLARE_MULTICAST_DELEGATE(FOnGCStreamedOutLevelsEvent);
	static FOnGCStreamedOutLevelsEvent OnGCStreamedOutLevels;

	/**
	 * Register with the garbage collector to receive callbacks pre and post garbage collection
	 */
	static void AddGarbageCollectorCallback();

	/**
	 * Request to be unloaded.
	 *
	 * @param InLevel	Level that should be unloaded
	 */
	static void RequestUnload( ULevel* InLevel );

	/**
	 * Cancel any pending unload requests for passed in Level.
	 */
	static void CancelUnloadRequest( ULevel* InLevel );

	/** 
	 * Prepares levels that are marked for unload for the GC call by marking their actors and components as
	 * pending kill.
	 */
	static void PrepareStreamedOutLevelsForGC();

	/**
	 * Verify that the level packages are no longer around.
	 */
	static void VerifyLevelsGotRemovedByGC();
	
	/**
	 * @return	The number of levels pending a purge by the garbage collector
	 */
	static int32 GetNumLevelsPendingPurge();
	
private:
	/** Static array of levels that should be unloaded */
	static TArray<TWeakObjectPtr<ULevel> > LevelsPendingUnload;
	/** Static array of level packages that have been marked by PrepareStreamedOutLevelsForGC */
	static TArray<FName> LevelPackageNames;
};

struct ENGINE_API FLevelViewportInfo
{
	FVector CamPosition;
	FRotator CamRotation;
	float CamOrthoZoom;
	bool CamUpdated;

	FLevelViewportInfo()
	{
		CamPosition = FVector::ZeroVector;
		CamRotation = FRotator::ZeroRotator;
		CamOrthoZoom = DEFAULT_ORTHOZOOM;
		CamUpdated = false;
	}

	FLevelViewportInfo(const FVector& InCamPosition, const FRotator& InCamRotation, float InCamOrthoZoom)
	{
		CamPosition = InCamPosition;
		CamRotation = InCamRotation;
		CamOrthoZoom = InCamOrthoZoom;
		CamUpdated = false;
	}

	friend FArchive& operator<<( FArchive& Ar, FLevelViewportInfo& I )
	{
		if ( Ar.IsLoading() )
		{
			I.CamUpdated = true;
		}

		Ar << I.CamPosition;
		Ar << I.CamRotation;
		Ar << I.CamOrthoZoom;

		if ( Ar.IsLoading() )
		{
			if ( I.CamOrthoZoom == 0 )
			{
				I.CamOrthoZoom = DEFAULT_ORTHOZOOM;
			}
		}

		return Ar;
	}
};

/** 
* Tick function that starts the physics tick
**/
USTRUCT()
struct FStartPhysicsTickFunction : public FTickFunction
{
	GENERATED_USTRUCT_BODY()

	/** World this tick function belongs to **/
	class UWorld*	Target;

	/** 
		* Abstract function actually execute the tick. 
		* @param DeltaTime - frame time to advance, in seconds
		* @param TickType - kind of tick for this frame
		* @param CurrentThread - thread we are executing on, useful to pass along as new tasks are created
		* @param MyCompletionGraphEvent - completion event for this task. Useful for holding the completetion of this task until certain child tasks are complete.
	**/
	virtual void ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) OVERRIDE;
	/** Abstract function to describe this tick. Used to print messages about illegal cycles in the dependency graph **/
	virtual FString DiagnosticMessage();
};

/** 
* Tick function that ends the physics tick
**/
USTRUCT()
struct FEndPhysicsTickFunction : public FTickFunction
{
	GENERATED_USTRUCT_BODY()

	/** World this tick function belongs to **/
	class UWorld*	Target;

	/** 
		* Abstract function actually execute the tick. 
		* @param DeltaTime - frame time to advance, in seconds
		* @param TickType - kind of tick for this frame
		* @param CurrentThread - thread we are executing on, useful to pass along as new tasks are created
		* @param MyCompletionGraphEvent - completion event for this task. Useful for holding the completetion of this task until certain child tasks are complete.
	**/
	virtual void ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) OVERRIDE;
	/** Abstract function to describe this tick. Used to print messages about illegal cycles in the dependency graph **/
	virtual FString DiagnosticMessage();
};

/**
* Tick function that starts the cloth tick
**/
USTRUCT()
struct FStartClothSimulationFunction : public FTickFunction
{
	GENERATED_USTRUCT_BODY()

	/** World this tick function belongs to **/
	class UWorld*	Target;

	/**
	* Abstract function actually execute the tick.
	* @param DeltaTime - frame time to advance, in seconds
	* @param TickType - kind of tick for this frame
	* @param CurrentThread - thread we are executing on, useful to pass along as new tasks are created
	* @param MyCompletionGraphEvent - completion event for this task. Useful for holding the completetion of this task until certain child tasks are complete.
	**/
	virtual void ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) OVERRIDE;
	/** Abstract function to describe this tick. Used to print messages about illegal cycles in the dependency graph **/
	virtual FString DiagnosticMessage();
};

/**
* Tick function that ends the cloth tick
**/
USTRUCT()
struct FEndClothSimulationFunction : public FTickFunction
{
	GENERATED_USTRUCT_BODY()

	/** World this tick function belongs to **/
	class UWorld*	Target;

	/**
	* Abstract function actually execute the tick.
	* @param DeltaTime - frame time to advance, in seconds
	* @param TickType - kind of tick for this frame
	* @param CurrentThread - thread we are executing on, useful to pass along as new tasks are created
	* @param MyCompletionGraphEvent - completion event for this task. Useful for holding the completetion of this task until certain child tasks are complete.
	**/
	virtual void ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) OVERRIDE;
	/** Abstract function to describe this tick. Used to print messages about illegal cycles in the dependency graph **/
	virtual FString DiagnosticMessage();
};

struct ENGINE_API FActorSpawnParameters
{
	FActorSpawnParameters()
		:	Name(NAME_None)
		,	Template(NULL)
		,	Owner(NULL)
		,	Instigator(NULL)
		,	OverrideLevel(NULL)
		,	bNoCollisionFail(false)
		,	bRemoteOwned(false)
		,	bNoFail(false)
		,	bDeferConstruction(false)		
		,	bAllowDuringConstructionScript(false)
		,	ObjectFlags(RF_Transactional)
	{
	}

	/* A name to assign as the Name of the Actor being spawned. If no value is specified, the name of the spawned Actor will be automatically generated using the form [Class]_[Number]. */
	FName Name;

	/* An Actor to use as a template when spawning the new Actor. The spawned Actor will be initialized using the property values of the template Actor. If left NULL the class default object (CDO) will be used to initialize the spawned Actor. */
	AActor* Template;

	/* The Actor that spawned this Actor. (Can be left as NULL). */
	AActor* Owner;

	/* The APawn that is responsible for damage done by the spawned Actor. (Can be left as NULL). */
	APawn*	Instigator;

	/* The ULevel to spawn the Actor in, i.e. the Outer of the Actor. If left as NULL the Outer of the Owner is used. If the Owner is NULL the persistent level is used. */
	class	ULevel* OverrideLevel;

	/* Determines whether a collision test will be performed when spawning the Actor. If true, no collision test will be performed when spawning the Actor regardless of the collision settings of the root component or template Actor. */
	uint32	bNoCollisionFail:1;

	/* Is the actor remotely owned. */
	uint32	bRemoteOwned:1;

	/* Determines whether spawning will not fail if certain conditions are not met. If true, spawning will not fail because the class being spawned is `bStatic=true` or because the class of the template Actor is not the same as the class of the Actor being spawned. */
	uint32	bNoFail:1;

	/* Determines whether the construction script will be run. If true, the construction script will not be run on the spawned Actor. Only applicable if the Actor is being spawned from a Blueprint. */
	uint32	bDeferConstruction:1;
	
	/* Determines whether or not the actor may be spawned when running a construction script. If true spawning will fail if a construction script is being run. */
	uint32	bAllowDuringConstructionScript:1;
	
	/* Flags used to describe the spawned actor/object instance. */
	EObjectFlags ObjectFlags;		
};

struct ENGINE_API FWorldAsyncTraceState
{
	FWorldAsyncTraceState();

	FORCEINLINE AsyncTraceData& GetBufferForFrame        (int32 Frame) { return DataBuffer[ Frame             % 2]; }
	FORCEINLINE AsyncTraceData& GetBufferForCurrentFrame ()            { return DataBuffer[ CurrentFrame      % 2]; }
	FORCEINLINE AsyncTraceData& GetBufferForPreviousFrame()            { return DataBuffer[(CurrentFrame + 1) % 2]; }

	/** Async Trace Data Buffer **/
	AsyncTraceData DataBuffer[2];

	/** Used as counter for Buffer swap for DataBuffer
	 *	Right now it's only 2, but it can change
	 */
	int32 CurrentFrame;

	/** Next available index for each pool - used as ID for each trace query **/
	int32 NextAvailableTraceIndex;
	int32 NextAvailableOverlapIndex;
};

UCLASS(customConstructor, config=Engine)
class ENGINE_API UWorld : public UObject, public FNetworkNotify
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	// List of the layer names which were checked when the level was last saved
	UPROPERTY(config) 
	FString VisibleLayers_DEPRECATED;
	
	/** List of all the layers referenced by the world's actors */
	UPROPERTY()
	TArray< class ULayer* > Layers; 
#endif // WITH_EDITORONLY_DATA

	/** Persistent level containing the world info, default brush and actors spawned during gameplay among other things			*/
	UPROPERTY(Transient)
	class ULevel*								PersistentLevel;

	/** Reference to last save game info used for serialization. The only time this is non NULL is during UEngine::SaveGame(..) */
	UPROPERTY(Transient)
	class UDEPRECATED_SaveGameSummary*			SaveGameSummary_DEPRECATED;

	/** The NAME_GameNetDriver game connection(s) for client/server communication */
	UPROPERTY(Transient)
	class UNetDriver*							NetDriver;

	/** Line Batchers. All lines to be drawn in the world. */
	UPROPERTY(Transient)
	class ULineBatchComponent*					LineBatcher;

	/** Persistent Line Batchers. They don't get flushed every frame.  */
	UPROPERTY(Transient)
	class ULineBatchComponent*					PersistentLineBatcher;

	/** Foreground Line Batchers. This can't be Persistent.  */
	UPROPERTY(Transient)
	class ULineBatchComponent*					ForegroundLineBatcher;

	/** The replicated actor which contains game state information that can be accessible to clients */
	UPROPERTY(Transient)
	class AGameState*							GameState;

	/** @todo document */
	UPROPERTY(Transient)
	class AGameNetworkManager*					NetworkManager;

	/** Instance of UPhysicsCollisionHandler */
	UPROPERTY(Transient)
	class UPhysicsCollisionHandler*				PhysicsCollisionHandler;

	/** Audio component used for playing music tracks via SeqAct_PlayMusicTrack */
	UPROPERTY(Transient)
	class UAudioComponent*						MusicComp;

	/** Array of any additional objects that need to be referenced by this world, to make sure they aren't GC'd */
	UPROPERTY(Transient)
	TArray<UObject*>							ExtraReferencedObjects;

	/** Level collection. ULevels are referenced by FName (Package name) to avoid serialized references. Also contains offsets in world units */
	UPROPERTY(Transient)
	TArray<class ULevelStreaming*>				StreamingLevels;

	/** Prefix we used to rename streaming levels, non empty in PIE and standalone preview */
	UPROPERTY()
	FString										StreamingLevelsPrefix;
	
	/** Pointer to the current level in the queue to be made visible, NULL if none are pending.									*/
	UPROPERTY(Transient)
	class ULevel*								CurrentLevelPendingVisibility;
	
	/** Particle event manager **/
	UPROPERTY()
	class AParticleEventManager*				MyParticleEventManager;

	/** DefaultPhysicsVolume used for whole game **/
	UPROPERTY()
	APhysicsVolume*								DefaultPhysicsVolume;

	/** View locations rendered in the previous frame, if any. */
	TArray<FVector>								ViewLocationsRenderedLastFrame;

	/** set for one tick after completely loading and initializing a new world
	 * (regardless of whether it's LoadMap() or seamless travel)
	 */
	UPROPERTY(transient)
	uint32 bWorldWasLoadedThisTick:1;

	/**
	 * Triggers a call to PostLoadMap() the next Tick, turns off loading movie if LoadMap() has been called.
	 */
	UPROPERTY(transient)
	uint32 bTriggerPostLoadMap:1;

private:
	/** The world's navmesh */
	UPROPERTY(Transient)
	class UNavigationSystem*					NavigationSystem;

	/** The current GameMode, valid only on the server */
	UPROPERTY(Transient)
	class AGameMode*							AuthorityGameMode;

	/** Behavior tree manager used by game */
	UPROPERTY(Transient)
	class UBehaviorTreeManager*					BehaviorTreeManager;

	/** Environment query manager used by game */
	UPROPERTY(Transient)
	class UEnvQueryManager*						EnvironmentQueryManager;

	/** RVO avoidance manager used by game */
	UPROPERTY(Transient)
	class UAvoidanceManager*					AvoidanceManager;

	/** Array of levels currently in this world. Not serialized to disk to avoid hard references.								*/
	UPROPERTY(Transient)
	TArray<class ULevel*>						Levels;

public:
	/** Array of actors that are candidates for sending over the network */
	TArray< AActor * >							NetworkActors;

private:

	/** Pointer to the current level being edited. Level has to be in the Levels array and == PersistentLevel in the game.		*/
	UPROPERTY(Transient)
	class ULevel*								CurrentLevel;

	/** Parameter collection instances that hold parameter overrides for this world. */
	UPROPERTY(Transient)
	TArray<class UMaterialParameterCollectionInstance*> ParameterCollectionInstances;

public:
	/** Set the pointer to the Navgation system. */
	void SetNavigationSystem( UNavigationSystem* InNavigationSystem);

	/** The interface to the scene manager for this world. */
	FSceneInterface*							Scene;

	/** Saved editor viewport states - one for each view type. Indexed using ELevelViewportType above.							*/
	FLevelViewportInfo							EditorViews[4];

	/** 
	 * Set the CurrentLevel for this world. 
	 * @return true if the current level changed.
	 */
	bool SetCurrentLevel( class ULevel* InLevel );
	
	/** Get the CurrentLevel for this world. **/
	class ULevel* GetCurrentLevel() const;

	/** A static map that is populated before loading a world from a package. This is so UWorld can look up its WorldType in ::PostLoad */
	static TMap<FName, EWorldType::Type> WorldTypePreLoadMap;

	/** Map of blueprints that are bieng debugged and the object instance they are debugging. */
	typedef TMap<TWeakObjectPtr<class UBlueprint>, TWeakObjectPtr<UObject> > FBlueprintToDebuggedObjectMap;

	/** Return the array of objects currently bieng debugged. */
	const FBlueprintToDebuggedObjectMap& GetBlueprintObjectsBeingDebugged() const{ return BlueprintObjectsBeingDebugged; };

private:
	/** List of all the controllers in the world. */
	TArray<TAutoWeakObjectPtr<class AController> >	ControllerList;

	/** List of all the player controllers in the world. */
	TArray<TAutoWeakObjectPtr<class APlayerController> >	PlayerControllerList;

	/** List of all the pawns in the world. */
	TArray<TAutoWeakObjectPtr<class APawn> >			PawnList;	

	/** Physics scene for this world. */
	FPhysScene*									PhysicsScene;

	/** Set of components that need updates at the end of the frame */
	TSet<TWeakObjectPtr<class UActorComponent> > ComponentsThatNeedEndOfFrameUpdate;

	/** Set of components that need recreates at the end of the frame */
	TSet<TWeakObjectPtr<class UActorComponent> > ComponentsThatNeedEndOfFrameUpdate_OnGameThread;

#if EXPERIMENTAL_PARALLEL_CODE
	/** Sync object for ComponentsThatNeedEndOfFrameUpdate */
	FCriticalSection	ComponentsThatNeedEndOfFrameUpdateSynchronizationObject;
#endif

	/** The state of async tracing - abstracted into its own object for easier reference */
	FWorldAsyncTraceState AsyncTraceState;

	/**	Objects currently being debugged in Kismet	*/
	FBlueprintToDebuggedObjectMap BlueprintObjectsBeingDebugged;

	bool bRequiresHitProxies;

	/** a delegate that broadcasts a notification whenever an actor is spawned */
	FOnActorSpawned OnActorSpawned;

	/** Reset Async Trace Buffer **/
	void ResetAsyncTrace();

	/** Wait for all Async Trace Buffer to be done **/
	void WaitForAllAsyncTraceTasks();

	/** Finish Async Trace Buffer **/
	void FinishAsyncTrace();

	/** Gameplay timers. */
	class FTimerManager* TimerManager;

	/** Latent action manager. */
	struct FLatentActionManager LatentActionManager;

	/** Whether we have a pending call to BuildStreamingData(). */
	uint32 bStreamingDataDirty:1;

	/** Timestamp (in FPlatformTime::Seconds) when the next call to BuildStreamingData() should be made, if bDirtyStreamingData is true. */
	double BuildStreamingDataTimer;

	DECLARE_EVENT_OneParam(UWorld, FOnNetTickEvent, float);
	DECLARE_EVENT(UWorld, FOnTickFlushEvent);
	/** Event to gather up all net drivers and call TickDispatch at once */
	FOnNetTickEvent TickDispatchEvent;

	/** Event to gather up all net drivers and call TickFlush at once */
	FOnNetTickEvent TickFlushEvent;
	
	/** Event to gather up all net drivers and call PostTickFlush at once */
	FOnTickFlushEvent PostTickFlushEvent;

	/** All registered net drivers TickDispatch() */
	void BroadcastTickDispatch(float DeltaTime)
	{
		TickDispatchEvent.Broadcast(DeltaTime);
	}
	/** All registered net drivers TickFlush() */
	void BroadcastTickFlush(float DeltaTime)
	{
		TickFlushEvent.Broadcast(DeltaTime);
	}
	/** All registered net drivers PostTickFlush() */
	void BroadcastPostTickFlush(float DeltaTime)
	{
		PostTickFlushEvent.Broadcast();
	}

	/** Called when the number of levels changes. */
	DECLARE_EVENT(UWorld, FOnLevelsChangedEvent);
	
	/** Broadcasts whenever the number of levels changes */
	FOnLevelsChangedEvent LevelsChangedEvent;

#if WITH_EDITOR

	/** Broadcasts that selected levels have changed. */
	void BroadcastSelectedLevelsChanged();

#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
	/** Called when selected level list changes. */
	DECLARE_EVENT( UWorld, FOnSelectedLevelsChangedEvent);

	/** Broadcasts whenever selected level list changes. */
	FOnSelectedLevelsChangedEvent				SelectedLevelsChangedEvent;

	/** Array of selected levels currently in this world. Not serialized to disk to avoid hard references.	*/
	UPROPERTY(Transient)
	TArray<class ULevel*>						SelectedLevels;

	/** Disables the broadcasting of level selection change. Internal use only. */
	uint32 bBroadcastSelectionChange:1;
#endif //WITH_EDITORONLY_DATA
public:
	FURL										URL;

	/** Interface to the FX system managing particles and related effects for this world.										*/
	class FFXSystemInterface*					FXSystem;

	/** Data structures for holding the tick functions that are associated with the world (line batcher, etc) **/
	class FTickTaskLevel*						TickTaskLevel;

	/** Whether we are in the middle of ticking actors/components or not														*/
	bool										bInTick;

    /** Whether we have already built the collision tree or not                                                                 */
    bool                                        bIsBuilt;
    
	/** We are in the middle of actor ticking, so add tasks for newly spawned actors											*/
	bool										bTickNewlySpawned;

	/** The current ticking group																								*/
	ETickingGroup								TickGroup;

	/** Tick function for starting physics																						*/
	FStartPhysicsTickFunction StartPhysicsTickFunction;
	/** Tick function for ending physics																						*/
	FEndPhysicsTickFunction EndPhysicsTickFunction;

	/** Tick function for starting cloth simulation																				*/
	FStartClothSimulationFunction StartClothTickFunction;
	/** Tick function for ending cloth simulation																				*/
	FEndClothSimulationFunction EndClothTickFunction;

	/** 
	 * Indicates that during world ticking we are doing the final component update of dirty components 
	 * (after PostAsyncWork and effect physics scene has run. 
	 */
	bool										bPostTickComponentUpdate;

	/** Counter for allocating game- unique controller player numbers															*/
	int32											PlayerNum;

	/** Time in seconds (game time so we respect time dilation) since the last time we purged references to pending kill objects */
	float										TimeSinceLastPendingKillPurge;

	/** Whether a full purge has been triggered, so that the next GarbageCollect will do a full purge no matter what.			*/
	bool										FullPurgeTriggered;
	
	/** Whether we should delay GC for one frame to finish some pending operation												*/
	bool										bShouldDelayGarbageCollect;

	/** Whether world object has been initialized via Init()																	*/
	bool										bIsWorldInitialized;
	
	/** Override, forcing level load requests to be allowed. < 0 == not allowed, 0 == have code choose, > 1 == force allow.		 */
	int32											AllowLevelLoadOverride;

	/** Number of frames to delay Streaming Volume updating, useful if you preload a bunch of levels but the camera hasn't caught up yet (INDEX_NONE for infinite) */
	int32											StreamingVolumeUpdateDelay;

	/** Is level streaming currently frozen?																					*/
	bool										bIsLevelStreamingFrozen;

	/** Is forcibly unloading streaming levels?																					*/
	bool										bShouldForceUnloadStreamingLevels;

	/** Is forcibly making streaming levels visible?																			*/
	bool										bShouldForceVisibleStreamingLevels;

	/** True we want to execute a call to UpdateCulledTriggerVolumes during Tick */
	bool										bDoDelayedUpdateCullDistanceVolumes;

	/** If true, this is a preview world used for editor tools, and not an actual loaded map world */
	TEnumAsByte<EWorldType::Type>				WorldType;

	/** If true this world is in the process of running the construction script for an actor */
	bool										bIsRunningConstructionScript;

	/** If true this world will tick physics to simulate. This isn't same as having Physics Scene. 
	 *  You need Physics Scene if you'd like to trace. This flag changed ticking */
	bool										bShouldSimulatePhysics;

#if WITH_EDITOR
	/** this is special flag to enable collision by default for components that are not Volume
	 * currently only used by editor level viewport world, and do not use this for in-game scene
	 */
	bool										bEnableTraceCollision;
#endif

	/** When non-'None', all line traces where the TraceTag match this will be drawn */
	FName    DebugDrawTraceTag;

	/*****************************************************************************************************/
	/** Moved from WorldSettings properties - START 														**/
	/*****************************************************************************************************/
		
	/** An array of post processing volumes, sorted in ascending order of priority.					*/
	TArray< IInterface_PostProcessVolume * > PostProcessVolumes;

	/** Linked list of reverb volumes, sorted in descending order of priority.							*/
	TAutoWeakObjectPtr<class AReverbVolume> HighestPriorityReverbVolume;

	/** Time in FPlatformTime::Seconds unbuilt time was last encountered. 0 means not yet.							*/
	double LastTimeUnbuiltLightingWasEncountered;

	/**  Time in seconds since level began play, but IS paused when the game is paused, and IS dilated/clamped. */
	float TimeSeconds;

	/** Time in seconds since level began play, but is NOT paused when the game is paused, and is NOT dilated/clamped. */
	float RealTimeSeconds;

	/** Time in seconds since level began play, but IS paused when the game is paused, and is NOT dilated/clamped. */
	float AudioTimeSeconds;

	/** Frame delta time in seconds adjusted by e.g. time dilation. */
	float DeltaTimeSeconds;

	/** time at which to start pause **/
	float PauseDelay;

	/** Offset for all levels from world origin */
	FIntPoint GlobalOriginOffset;

	/** Requested new world origin offset */
	FIntPoint RequestedGlobalOriginOffset;
	
	/** All levels information from which our world is composed */
	UPROPERTY(Transient)
	class UWorldComposition* WorldComposition;
	
	/** Streaming level package name to LOD index. 
	 *	LOD index stored in the persistent world to support consistent LOD between nested streaming levels which could be loaded during gameplay
	 *  LOD changes affects all streaming levels referring the same level package
	 */
	TMap<FName, int32>		StreamingLevelsLOD;

	/** Whether we currently flushing level streaming state */ 
	bool bFlushingLevelStreaming;

public:
	/** The type of travel to perform next when doing a server travel */
	ETravelType			NextTravelType;
	
	/** @todo document */
	FString NextURL;

	/** Amount of time to wait before traveling to next map, gives clients time to receive final RPCs @see ServerTravelPause */
	float NextSwitchCountdown;

	/** array of levels that were loaded into this map via PrepareMapChange() / CommitMapChange() (to inform newly joining clients) */
	TArray<FName> PreparingLevelNames;

	/** @todo document */
	FName CommittedPersistentLevelName;

	/** Param information for the currently playing MusicComp */
	struct FMusicTrackStruct CurrentMusicTrack;
	
#if WITH_EDITORONLY_DATA
	/** Map of LandscapeInfos for all loaded levels, valid in the editor only */
	TMap< FGuid, class ULandscapeInfo* > LandscapeInfoMap;
#endif // WITH_EDITORONLY_DATA

	/** frame rate is below DesiredFrameRate, so drop high detail actors */
	uint32 bDropDetail:1;

	/** frame rate is well below DesiredFrameRate, so make LOD more aggressive */
	uint32 bAggressiveLOD:1;

	/** That map is default map or not **/
	uint32 bIsDefaultLevel:1;
	
	/** Whether it was requested that the engine bring up a loading screen and block on async loading. */   
	uint32 bRequestedBlockOnAsyncLoading:1;

	/** Whether actors have been initialized for play */
	uint32 bActorsInitialized:1;

	/** Whether BeginPlay has been called on actors */
	uint32 bBegunPlay:1;

	/** Whether the match has been started */
	uint32 bMatchStarted:1;

	/**  Only update players. */
	uint32 bPlayersOnly:1;

	/** Only update players.  Next frame will set bPlayersOnly */
	uint32 bPlayersOnlyPending:1;

	/** Starting gameplay. */
	uint32 bStartup:1;

	/**
	 * This is a int on the level which is set when a light that needs to have lighting rebuilt
	 * is moved.  This is then checked in CheckMap for errors to let you know that this level should
	 * have lighting rebuilt.
	 **/
	uint32 NumLightingUnbuiltObjects;

	/**
	 * This is a bool that indicates that one or more blueprints in the level (blueprint instances, level script, etc)
	 * have compile errors that could not be automatically resolved.
	 */
	uint32 bKismetScriptError:1;

	// Kismet debugging flags - they can be only editor only, but they're uint32, so it doens't make much difference
	uint32 bDebugPauseExecution:1;

	/** @todo document */
	uint32 bDebugStepExecution:1;

	/** Indicates this scene always allows audio playback. */
	uint32 bAllowAudioPlayback:1;

	/** When set, will tell us to pause simulation after one tick.  If a breakpoint is encountered before tick is complete we will stop there instead. */
	uint32 bDebugFrameStepExecution:1;

	/** Keeps track whether actors moved via PostEditMove and therefore constraint syncup should be performed. */
	UPROPERTY(transient)
	uint32 bAreConstraintsDirty:1;

	/*****************************************************************************************************/
	/** Moved from WorldSettings - END 																		**/
	/*****************************************************************************************************/

	/**
	 * UWorld default constructor
	 */
	UWorld( const class FPostConstructInitializeProperties& PCIP );

	/**
	 * UWorld constructor called at game startup and when creating a new world in the Editor.
	 * Please note that this constructor does NOT get called when a world is loaded from disk.
	 *
	 * @param	InURL	URL associated with this world.
	 */
	UWorld( const class FPostConstructInitializeProperties& PCIP,const FURL& InURL );
	
	// LINE TRACE

	/**
	 *  Trace a ray against the world and return if a blocking hit is found.
	 *  @param  Start           Start location of the ray
	 *  @param  End             End location of the ray
	 *  @param  TraceChannel    The 'channel' that this ray is in, used to determine which components to hit
	 *  @param  Params          Additional parameters used for the trace
	 * 	@param 	ResponseParam	ResponseContainer to be used for this trace
	 *  @return TRUE if a blocking hit is found
	 */
	bool LineTraceTest(const FVector& Start,const FVector& End,ECollisionChannel TraceChannel,const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam) const;

	/**
	 *  Trace a ray against the world and return if a blocking hit is found.
	 *  @param  Start           	Start location of the ray
	 *  @param  End             	End location of the ray
	 *  @param  Params          	Additional parameters used for the trace
	 *	@param	ObjectQueryParams	List of object types it's looking for
	 *  @return TRUE if any hit is found
	 */
	bool LineTraceTest(const FVector& Start,const FVector& End,const struct FCollisionQueryParams& Params, const struct FCollisionObjectQueryParams& ObjectQueryParams) const;

	/**
	 *  Trace a ray against the world and return the first blocking hit
	 *  @param  OutHit          First blocking hit found
	 *  @param  Start           Start location of the ray
	 *  @param  End             End location of the ray
	 *  @param  TraceChannel    The 'channel' that this ray is in, used to determine which components to hit
	 *  @param  Params          Additional parameters used for the trace
	 * 	@param 	ResponseParam	ResponseContainer to be used for this trace	 
	 *  @return TRUE if a blocking hit is found
	 */
	bool LineTraceSingle(struct FHitResult& OutHit,const FVector& Start,const FVector& End,ECollisionChannel TraceChannel,const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam) const;

	/**
	 *  Trace a ray against the world and return the first blocking hit
	 *  @param  OutHit          First blocking hit found
	 *  @param  Start           Start location of the ray
	 *  @param  End             End location of the ray
	 *  @param  Params          	Additional parameters used for the trace
	 *	@param	ObjectQueryParams	List of object types it's looking for
	 *  @return TRUE if any hit is found
	 */
	bool LineTraceSingle(struct FHitResult& OutHit,const FVector& Start,const FVector& End,const struct FCollisionQueryParams& Params, const struct FCollisionObjectQueryParams& ObjectQueryParams) const;

	/**
	 *  Trace a ray against the world and return overlapping hits and then first blocking hit
	 *  Results are sorted, so a blocking hit (if found) will be the last element of the array
	 *  Only the single closest blocking result will be generated, no tests will be done after that
	 *  @param  OutHits         Array of hits found between ray and the world
	 *  @param  Start           Start location of the ray
	 *  @param  End             End location of the ray
	 *  @param  TraceChannel    The 'channel' that this ray is in, used to determine which components to hit
	 *  @param  Params          Additional parameters used for the trace
	 * 	@param 	ResponseParam	ResponseContainer to be used for this trace	 
	 *  @return TRUE if OutHits contains any blocking hit entries
	 */
	bool LineTraceMulti(TArray<struct FHitResult>& OutHits,const FVector& Start,const FVector& End,ECollisionChannel TraceChannel,const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam) const;

	/**
	 *  Trace a ray against the world and return overlapping hits and then first blocking hit
	 *  Results are sorted, so a blocking hit (if found) will be the last element of the array
	 *  Only the single closest blocking result will be generated, no tests will be done after that
	 *  @param  OutHits         Array of hits found between ray and the world
	 *  @param  Start           Start location of the ray
	 *  @param  End             End location of the ray
	 *  @param  Params          	Additional parameters used for the trace
	 *	@param	ObjectQueryParams	List of object types it's looking for
	 *  @return TRUE if any hit is found
	 */
	bool LineTraceMulti(TArray<struct FHitResult>& OutHits,const FVector& Start,const FVector& End,const struct FCollisionQueryParams& Params, const struct FCollisionObjectQueryParams& ObjectQueryParams) const;

	/**
	 *  Sweep a sphere against the world and return if a blocking hit is found.
	 *  @param  Start           Start location of the sphere
	 *  @param  End             End location of the sphere
	 *  @param  TraceChannel    The 'channel' that this trace uses, used to determine which components to hit
	 *  @param	CollisionShape		CollisionShape - supports Box, Sphere, Capsule
	 *  @param  Params          Additional parameters used for the trace
	 * 	@param 	ResponseParam	ResponseContainer to be used for this trace	 	 
	 *  @return TRUE if a blocking hit is found
	 */
	bool SweepTest(const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const struct FCollisionShape & CollisionShape, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam) const;

	/**
	 *  Sweep a sphere against the world and return if a blocking hit is found.
	 *  @param  Start           Start location of the sphere
	 *  @param  End             End location of the sphere
	 *  @param	CollisionShape		CollisionShape - supports Box, Sphere, Capsule
	 *  @param  Params          	Additional parameters used for the trace
	 *	@param	ObjectQueryParams	List of object types it's looking for
	 *  @return TRUE if any hit is found
	 */
	bool SweepTest(const FVector& Start, const FVector& End, const FQuat& Rot, const struct FCollisionShape & CollisionShape, const struct FCollisionQueryParams& Params, const struct FCollisionObjectQueryParams& ObjectQueryParams) const;

	/**
	 *  Sweep a sphere against the world and return the first blocking hit
	 *  @param  OutHit          First blocking hit found
	 *  @param  Start           Start location of the sphere
	 *  @param  End             End location of the sphere
	 *  @param  TraceChannel    The 'channel' that this trace is in, used to determine which components to hit
	 *  @param	CollisionShape		CollisionShape - supports Box, Sphere, Capsule
	 *  @param  Params          Additional parameters used for the trace
	 * 	@param 	ResponseParam	ResponseContainer to be used for this trace	 
	 *  @return TRUE if OutHits contains any blocking hit entries
	 */
	bool SweepSingle(struct FHitResult& OutHit,const FVector& Start,const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel,const struct FCollisionShape & CollisionShape, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam) const;

	/**
	 *  Sweep a sphere against the world and return the first blocking hit
	 *  @param  OutHit          First blocking hit found
	 *  @param  Start           Start location of the sphere
	 *  @param  End             End location of the sphere
	 *  @param	CollisionShape		CollisionShape - supports Box, Sphere, Capsule
	 *  @param  Params          	Additional parameters used for the trace
	 *	@param	ObjectQueryParams	List of object types it's looking for
	 *  @return TRUE if any hit is found
	 */
	bool SweepSingle(struct FHitResult& OutHit,const FVector& Start,const FVector& End, const FQuat& Rot, const struct FCollisionShape & CollisionShape, const struct FCollisionQueryParams& Params, const struct FCollisionObjectQueryParams& ObjectQueryParams) const;

	/**
	 *  Sweep a sphere against the world and return all initial overlaps (including blocking) if requested, then overlapping hits and then first blocking hit
	 *  Results are sorted, so a blocking hit (if found) will be the last element of the array
	 *  Only the single closest blocking result will be generated, no tests will be done after that
	 *  @param  OutHits         Array of hits found between ray and the world
	 *  @param  Start           Start location of the sphere
	 *  @param  End             End location of the sphere
	 *  @param  TraceChannel    The 'channel' that this ray is in, used to determine which components to hit
	 *  @param	CollisionShape		CollisionShape - supports Box, Sphere, Capsule
	 *  @param  Params          Additional parameters used for the trace
	 * 	@param 	ResponseParam	ResponseContainer to be used for this trace	 
	 *  @return TRUE if OutHits contains any blocking hit entries
	 */
	bool SweepMulti(TArray<struct FHitResult>& OutHits,const FVector& Start,const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel,const struct FCollisionShape & CollisionShape, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam) const;

	/**
	 *  Sweep a sphere against the world and return all initial overlaps (including blocking) if requested, then overlapping hits and then first blocking hit
	 *  Results are sorted, so a blocking hit (if found) will be the last element of the array
	 *  Only the single closest blocking result will be generated, no tests will be done after that
	 *  @param  OutHits         Array of hits found between ray and the world
	 *  @param  Start           Start location of the sphere
	 *  @param  End             End location of the sphere
	 *  @param	CollisionShape		CollisionShape - supports Box, Sphere, Capsule
	 *  @param  Params          	Additional parameters used for the trace
	 *	@param	ObjectQueryParams	List of object types it's looking for
	 *  @return TRUE if any hit is found
	 */
	bool SweepMulti(TArray<struct FHitResult>& OutHits,const FVector& Start,const FVector& End, const FQuat& Rot, const struct FCollisionShape & CollisionShape, const struct FCollisionQueryParams& Params, const struct FCollisionObjectQueryParams& ObjectQueryParams) const;

	/**
	 *  Test the collision of an AABB at the supplied location, and return if any blocking overlap is found
	 *  @param  Pos             Location of center of box to test against the world
	 *  @param  TraceChannel    The 'channel' that this query is in, used to determine which components to hit
	 *  @param	CollisionShape		CollisionShape - supports Box, Sphere, Capsule
	 *  @param  Params          Additional parameters used for the trace
	 * 	@param 	ResponseParam	ResponseContainer to be used for this trace	 
	 *  @return TRUE if any blocking results are found
	 */
	bool OverlapTest(const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const struct FCollisionShape & CollisionShape, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam) const;

	/**
	 *  Test the collision of an AABB at the supplied location, and return if any blocking overlap is found
	 *  @param  Pos             Location of center of box to test against the world
	 *  @param	CollisionShape		CollisionShape - supports Box, Sphere, Capsule
	 *  @param  Params          	Additional parameters used for the trace
	 *	@param	ObjectQueryParams	List of object types it's looking for
	 *  @return TRUE if any hit is found
	 */
	bool OverlapTest(const FVector& Pos, const FQuat& Rot, const struct FCollisionShape & CollisionShape, const struct FCollisionQueryParams& Params, const struct FCollisionObjectQueryParams& ObjectQueryParams) const;

	/**
	 *  Test the collision of a sphere at the supplied location, and determine the set of components that it overlaps
	 *  @param  OutOverlaps     Array of components found to overlap supplied box
	 *  @param  Pos             Location of center of sphere to test against the world
	 *  @param  TraceChannel    The 'channel' that this query is in, used to determine which components to hit
	 *  @param	CollisionShape		CollisionShape - supports Box, Sphere, Capsule
	 *  @param  Params          Additional parameters used for the trace
	 * 	@param 	ResponseParam	ResponseContainer to be used for this trace
	 *  @return TRUE if OutOverlaps contains any blocking results
	 */
	bool OverlapSingle(struct FOverlapResult& OutOverlap,const FVector& Pos, const FQuat& Rot,ECollisionChannel TraceChannel,const struct FCollisionShape & CollisionShape, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam) const;

	/**
	 *  Test the collision of a sphere at the supplied location, and determine the set of components that it overlaps
	 *  @param  OutOverlaps     Array of components found to overlap supplied box
	 *  @param  Pos             Location of center of sphere to test against the world
	 *  @param	CollisionShape		CollisionShape - supports Box, Sphere, Capsule
	 *  @param  Params          	Additional parameters used for the trace
	 *	@param	ObjectQueryParams	List of object types it's looking for
	 *  @return TRUE if any hit is found
	 */
	bool OverlapSingle(struct FOverlapResult& OutOverlap,const FVector& Pos, const FQuat& Rot, const struct FCollisionShape & CollisionShape, const struct FCollisionQueryParams& Params, const struct FCollisionObjectQueryParams& ObjectQueryParams) const;

	/**
	 *  Test the collision of a sphere at the supplied location, and determine the set of components that it overlaps
	 *  @param  OutOverlaps     Array of components found to overlap supplied box
	 *  @param  Pos             Location of center of sphere to test against the world
	 *  @param  TraceChannel    The 'channel' that this query is in, used to determine which components to hit
	 *  @param	CollisionShape		CollisionShape - supports Box, Sphere, Capsule
	 *  @param  Params          Additional parameters used for the trace
	 * 	@param 	ResponseParam	ResponseContainer to be used for this trace
	 *  @return TRUE if OutOverlaps contains any blocking results
	 */
	bool OverlapMulti(TArray<struct FOverlapResult>& OutOverlaps,const FVector& Pos, const FQuat& Rot,ECollisionChannel TraceChannel,const struct FCollisionShape & CollisionShape, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam) const;

	/**
	 *  Test the collision of a sphere at the supplied location, and determine the set of components that it overlaps
	 *  @param  OutOverlaps     Array of components found to overlap supplied box
	 *  @param  Pos             Location of center of sphere to test against the world
	 *  @param	CollisionShape		CollisionShape - supports Box, Sphere, Capsule
	 *  @param  Params          	Additional parameters used for the trace
	 *	@param	ObjectQueryParams	List of object types it's looking for
	 *  @return TRUE if any hit is found
	 */
	bool OverlapMulti(TArray<struct FOverlapResult>& OutOverlaps,const FVector& Pos, const FQuat& Rot, const struct FCollisionShape & CollisionShape, const struct FCollisionQueryParams& Params, const struct FCollisionObjectQueryParams& ObjectQueryParams) const;
	// COMPONENT SWEEP

	/**
	 *  Sweep this component against the world and return the first blocking hit
	 *  @param  OutHit          First blocking hit found
	 *	@param	PrimComp		Component to use geometry from to test against the world. Transform of this component is ignored
	 *  @param  Start           Start location of the sphere
	 *  @param  End             End location of the sphere
	 *  @param  Params          Additional parameters used for the trace
	 *  @return TRUE if OutHits contains any blocking hit entries
	 */
	bool ComponentSweepSingle(struct FHitResult& OutHit,class UPrimitiveComponent* PrimComp, const FVector& Start,const FVector& End, const FRotator& Rot, const struct FComponentQueryParams& Params) const;

	/**
	 *  Sweep the geometry of the supplied component, and determine the set of components that it hits
	 *  @param  OutHits         Array of hits found between ray and the world
	 *  @param  PrimComp        Component to use geometry from to test against the world. Transform of this component is ignored
	 *  @param  Start           Start location of the trace
	 *  @param  End             End location of the trace
	 *  @param  Rot             Rotation to place PrimComp geometry at to test against the world (rotation remains constant over trace)
	 *  @param  Params          Additional parameters used for the trace
	 *  @return TRUE if OutHits contains any blocking hit entries
	 */
	bool ComponentSweepMulti(TArray<struct FHitResult>& OutHits,class UPrimitiveComponent* PrimComp, const FVector& Start,const FVector& End, const FRotator& Rot, const struct FComponentQueryParams& Params)  const;

	// COMPONENT OVERLAP
	/**
	 *  Test the collision of the supplied component at the supplied location/rotation, and determine the set of components that it overlaps
	 *  @param  PrimComp        Component to use geometry from to test against the world. Transform of this component is ignored
	 *  @param  Pos             Location to place PrimComp geometry at to test against the world
	 *  @param  Rot             Rotation to place PrimComp geometry at to test against the world
	 *  @return TRUE if OutOverlaps contains any blocking results
	 */
	bool ComponentOverlapTest(class UPrimitiveComponent* PrimComp, const FVector& Pos, const FRotator& Rot, const struct FComponentQueryParams& Params) const;

	/**
	 *  Test the collision of the supplied component at the supplied location/rotation, and determine the set of components that it overlaps
	 *  @param  OutOverlaps     Array of overlaps found between component in specified pose and the world
	 *  @param  PrimComp        Component to use geometry from to test against the world. Transform of this component is ignored
	 *  @param  Pos             Location to place PrimComp geometry at to test against the world
	 *  @param  Rot             Rotation to place PrimComp geometry at to test against the world
	 *	@param	ObjectQueryParams	List of object types it's looking for. When this enters, we do object query with component shape
	 *  @return TRUE if OutOverlaps contains any blocking results
	 */
	bool ComponentOverlapMulti(TArray<struct FOverlapResult>& OutOverlaps, const class UPrimitiveComponent* PrimComp, const FVector& Pos, const FRotator& Rot, const struct FComponentQueryParams& Params, const struct FCollisionObjectQueryParams& ObjectQueryParams=FCollisionObjectQueryParams::DefaultObjectQueryParam) const;

	/**
	 *  Test the collision of the supplied component at the supplied location/rotation, and determine the set of components that it overlaps
	 *  @param  OutOverlaps     Array of overlaps found between component in specified pose and the world
	 *  @param  PrimComp        Component to use geometry from to test against the world. Transform of this component is ignored
	 *  @param  Pos             Location to place PrimComp geometry at to test against the world
	 *  @param  Rot             Rotation to place PrimComp geometry at to test against the world
	 *  @param  TestChannel    The 'channel' that this ray is in, used to determine which components to hit
	 *  @return TRUE if OutOverlaps contains any blocking results
	 */
	bool ComponentOverlapMulti(TArray<struct FOverlapResult>& OutOverlaps, const class UPrimitiveComponent* PrimComp, const FVector& Pos, const FRotator& Rot, ECollisionChannel TestChannel, const struct FComponentQueryParams& Params, const struct FCollisionObjectQueryParams& ObjectQueryParams=FCollisionObjectQueryParams::DefaultObjectQueryParam) const;

	/**
	 * Interface for Async. Pretty much same parameter set except you can optional set delegate to be called when execution is completed and you can set UserData if you'd like
	 * if no delegate, you can query trace data using QueryTraceData or QueryOverlapData
	 * the data is available only in the next frame after request is made - in other words, if request is made in frame X, you can get the result in frame (X+1)
	 *
	 *  @param  Start           Start location of the ray
	 *  @param  End             End location of the ray
	 *  @param  TraceChannel    The 'channel' that this ray is in, used to determine which components to hit
	 *  @param  Params          Additional parameters used for the trace
	 * 	@param 	ResponseParam	ResponseContainer to be used for this trace
	 *	@param	InDeleagte		Delegate function to be called - to see example, search FTraceDelegate
	 *							Example can be void MyActor::TraceDone(const FTraceHandle & TraceHandle, FTraceDatum & TraceData)
	 *							Before sending to the function, 
	 *						
	 *							FTraceDelegate TraceDelegate;
	 *							TraceDelegate.BindRaw(this, &MyActor::TraceDone);
	 * 
	 *	@param	UserData		UserData
	 *	@param bMultiTrace		true if you'd like to get continuous result from the trace, false if you want single
	 */ 
	FTraceHandle	AsyncLineTrace(const FVector& Start,const FVector& End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam, FTraceDelegate * InDelegate=NULL, uint32 UserData = 0, bool bMultiTrace=false );

	/**
	 * Interface for Async. Pretty much same parameter set except you can optional set delegate to be called when execution is completed and you can set UserData if you'd like
	 * if no delegate, you can query trace data using QueryTraceData or QueryOverlapData
	 * the data is available only in the next frame after request is made - in other words, if request is made in frame X, you can get the result in frame (X+1)
	 *
	 *  @param  Start           Start location of the ray
	 *  @param  End             End location of the ray
	 *  @param  Params          Additional parameters used for the trace
	 *	@param	ObjectQueryParams	List of object types it's looking for
	 *	@param	InDeleagte		Delegate function to be called - to see example, search FTraceDelegate
	 *							Example can be void MyActor::TraceDone(const FTraceHandle & TraceHandle, FTraceDatum & TraceData)
	 *							Before sending to the function, 
	 *						
	 *							FTraceDelegate TraceDelegate;
	 *							TraceDelegate.BindRaw(this, &MyActor::TraceDone);
	 * 
	 *	@param	UserData		UserData
	 *	@param bMultiTrace		true if you'd like to get continuous result from the trace, false if you want single
	 */ 
	FTraceHandle	AsyncLineTrace(const FVector& Start,const FVector& End, const struct FCollisionQueryParams& Params, const struct FCollisionObjectQueryParams& ObjectQueryParams, FTraceDelegate * InDelegate=NULL, uint32 UserData = 0, bool bMultiTrace=false );

	/**
	 * Interface for Async trace
	 * Pretty much same parameter set except you can optional set delegate to be called when execution is completed and you can set UserData if you'd like
	 * if no delegate, you can query trace data using QueryTraceData or QueryOverlapData
	 * the data is available only in the next frame after request is made - in other words, if request is made in frame X, you can get the result in frame (X+1)
	 *
	 *  @param  Start           Start location of the sphere
	 *  @param  End             End location of the sphere
	 *  @param  TraceChannel    The 'channel' that this trace is in, used to determine which components to hit
	 *  @param	CollisionShape		CollisionShape - supports Box, Sphere, Capsule
	 *  @param  Params          Additional parameters used for the trace
	 * 	@param 	ResponseParam	ResponseContainer to be used for this trace	 
	 *	@param	InDeleagte		Delegate function to be called - to see example, search FTraceDelegate
	 *							Example can be void MyActor::TraceDone(const FTraceHandle & TraceHandle, FTraceDatum & TraceData)
	 *							Before sending to the function, 
	 *						
	 *							FTraceDelegate TraceDelegate;
	 *							TraceDelegate.BindRaw(this, &MyActor::TraceDone);
	 * 
	 *	@param	UserData		UserData
	 *	@param bMultiTrace		true if you'd like to get continuous result from the trace, false if you want single
	 */ 
	FTraceHandle	AsyncSweep(const FVector& Start,const FVector& End,ECollisionChannel TraceChannel, const struct FCollisionShape & CollisionShape, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam, FTraceDelegate * InDelegate=NULL, uint32 UserData = 0, bool bMultiTrace=false );

	/**
	 * Interface for Async trace
	 * Pretty much same parameter set except you can optional set delegate to be called when execution is completed and you can set UserData if you'd like
	 * if no delegate, you can query trace data using QueryTraceData or QueryOverlapData
	 * the data is available only in the next frame after request is made - in other words, if request is made in frame X, you can get the result in frame (X+1)
	 *
	 *  @param  Start           Start location of the sphere
	 *  @param  End             End location of the sphere
	 *  @param	CollisionShape		CollisionShape - supports Box, Sphere, Capsule
	 *  @param  Params          Additional parameters used for the trace
	 *	@param	ObjectQueryParams	List of object types it's looking for
	 *	@param	InDeleagte		Delegate function to be called - to see example, search FTraceDelegate
	 *							Example can be void MyActor::TraceDone(const FTraceHandle & TraceHandle, FTraceDatum & TraceData)
	 *							Before sending to the function, 
	 *						
	 *							FTraceDelegate TraceDelegate;
	 *							TraceDelegate.BindRaw(this, &MyActor::TraceDone);
	 * 
	 *	@param	UserData		UserData
	 *	@param bMultiTrace		true if you'd like to get continuous result from the trace, false if you want single
	 */ 
	FTraceHandle	AsyncSweep(const FVector& Start,const FVector& End,const struct FCollisionShape & CollisionShape, const struct FCollisionQueryParams& Params, const struct FCollisionObjectQueryParams& ObjectQueryParams, FTraceDelegate * InDelegate=NULL, uint32 UserData = 0, bool bMultiTrace=false );
	// overlap functions

	/**
	 * Interface for Async trace
	 * Pretty much same parameter set except you can optional set delegate to be called when execution is completed and you can set UserData if you'd like
	 * if no delegate, you can query trace data using QueryTraceData or QueryOverlapData
	 * the data is available only in the next frame after request is made - in other words, if request is made in frame X, you can get the result in frame (X+1)
	 *
	 *  @param  Pos             Location of center of sphere to test against the world
	 *	@param	bMultiTrace		true if you'd like to do multi trace, or false otherwise
	 *  @param  TraceChannel    The 'channel' that this query is in, used to determine which components to hit
	 *  @param	CollisionShape		CollisionShape - supports Box, Sphere, Capsule
	 *  @param  Params          Additional parameters used for the trace
	 * 	@param 	ResponseParam	ResponseContainer to be used for this trace
	 *	@param	InDeleagte		Delegate function to be called - to see example, search FTraceDelegate
	 *							Example can be void MyActor::TraceDone(const FTraceHandle & TraceHandle, FTraceDatum & TraceData)
	 *							Before sending to the function, 
	 *						
	 *							FTraceDelegate TraceDelegate;
	 *							TraceDelegate.BindRaw(this, &MyActor::TraceDone);
	 * 
	 *	@param UserData			UserData
	 *	@param bMultiTrace		true if you'd like to get continuous result from the trace, false if you want single
	 */ 
	FTraceHandle	AsyncOverlap(const FVector & Pos, const FQuat & Rot, ECollisionChannel TraceChannel, const struct FCollisionShape & CollisionShape, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam, FOverlapDelegate * InDelegate=NULL, uint32 UserData = 0, bool bMultiTrace=false);

	/**
	 * Interface for Async trace
	 * Pretty much same parameter set except you can optional set delegate to be called when execution is completed and you can set UserData if you'd like
	 * if no delegate, you can query trace data using QueryTraceData or QueryOverlapData
	 * the data is available only in the next frame after request is made - in other words, if request is made in frame X, you can get the result in frame (X+1)
	 *
	 *  @param  Pos             Location of center of sphere to test against the world
	 *  @param  TraceChannel    The 'channel' that this query is in, used to determine which components to hit
	 *  @param	CollisionShape		CollisionShape - supports Box, Sphere, Capsule
	 *  @param  Params          Additional parameters used for the trace
	 * 	@param 	ResponseParam	ResponseContainer to be used for this trace
	 *	@param	InDeleagte		Delegate function to be called - to see example, search FTraceDelegate
	 *							Example can be void MyActor::TraceDone(const FTraceHandle & TraceHandle, FTraceDatum & TraceData)
	 *							Before sending to the function, 
	 *						
	 *							FTraceDelegate TraceDelegate;
	 *							TraceDelegate.BindRaw(this, &MyActor::TraceDone);
	 * 
	 *	@param UserData			UserData
	 *	@param bMultiTrace		true if you'd like to get continuous result from the trace, false if you want single
	 */ 
	FTraceHandle	AsyncOverlap(const FVector & Pos, const FQuat & Rot, const struct FCollisionShape & CollisionShape, const struct FCollisionQueryParams& Params, const struct FCollisionObjectQueryParams& ObjectQueryParams, FOverlapDelegate * InDelegate=NULL, uint32 UserData = 0, bool bMultiTrace=false);

	/**
	 * Query function 
	 * return true if already done and returning valid result - can be hit or no hit
	 * return false if either expired or not yet evaluated or invalid
	 * Use IsTraceHandleValid to find out if valid and to be evaluated
	 */
	bool QueryTraceData(const FTraceHandle & Handle, FTraceDatum & OutData);

	/**
	 * Query function 
	 * return true if already done and returning valid result - can be hit or no hit
	 * return false if either expired or not yet evaluated or invalid
	 * Use IsTraceHandleValid to find out if valid and to be evaluated
	 */
	bool QueryOverlapData(const FTraceHandle & Handle, FOverlapDatum & OutData);
	/** 
	 * See if TraceHandle is still valid or not
	 *
	 * @param	Handle			TraceHandle that was returned when request Trace
	 * @param	bOverlapTrace	true if this is overlap test Handle, not trace test handle
	 * 
	 * return true if it will be evaluated OR it has valid result 
	 * return false if it already has expired Or not valid 
	 */
	bool IsTraceHandleValid(const FTraceHandle & Handle, bool bOverlapTrace);

	/** NavigationSystem getter */
	FORCEINLINE UNavigationSystem* GetNavigationSystem() { return NavigationSystem; }
	/** NavigationSystem const getter */
	FORCEINLINE const UNavigationSystem* GetNavigationSystem() const { return NavigationSystem; }

	/** Behavior tree manager getter */
	FORCEINLINE class UBehaviorTreeManager* GetBehaviorTreeManager() { return BehaviorTreeManager; }
	/** Behavior tree manager const getter */
	FORCEINLINE const class UBehaviorTreeManager* GetBehaviorTreeManager() const { return BehaviorTreeManager; }

	/** Behavior tree manager getter */
	FORCEINLINE class UEnvQueryManager* GetEnvironmentQueryManager() { return EnvironmentQueryManager; }
	/** Behavior tree manager const getter */
	FORCEINLINE const class UEnvQueryManager* GetEnvironmentQueryManager() const { return EnvironmentQueryManager; }

	/** Behavior tree manager getter */
	FORCEINLINE class UAvoidanceManager* GetAvoidanceManager() { return AvoidanceManager; }
	/** Behavior tree manager const getter */
	FORCEINLINE const class UAvoidanceManager* GetAvoidanceManager() const { return AvoidanceManager; }

	/** Returns an iterator for the controller list. */
	FConstControllerIterator GetControllerIterator() const;

	/** Returns an iterator for the pawn list. */
	FConstPawnIterator GetPawnIterator() const;
	
	/** Returns an iterator for the player controller list. */
	FConstPlayerControllerIterator GetPlayerControllerIterator() const;
	
	/** Get the first player controller, or NULL if there is not one. */	
	APlayerController* GetFirstPlayerController() const;

	/*
	 *	Get the first valid local player via the first player controller.
	 *
	 *  @return Pointer to the first valid ULocalPlayer, or NULL if there is not one.
	 */	
	ULocalPlayer* GetFirstLocalPlayerFromController() const;
	
	UGameViewportClient* GetGameViewport() const;

	/** Returns the default brush. */
	ABrush* GetBrush() const;

	/** Returns true if the actors have been initialized and are ready to start play */
	bool AreActorsInitialized() const;

	/** Returns true if gameplay has already started, false otherwise. */
	bool HasBegunPlay() const;

	/**
	 * Returns time in seconds since world was brought up for play, IS stopped when game pauses, IS dilated/clamped
	 *
	 * @return time in seconds since world was brought up for play
	 */
	float GetTimeSeconds() const;

	/**
	* Returns time in seconds since world was brought up for play, does NOT stop when game pauses, NOT dilated/clamped
	*
	* @return time in seconds since world was brought up for play
	*/
	float GetRealTimeSeconds() const;

	/**
	* Returns time in seconds since world was brought up for play, IS stopped when game pauses, NOT dilated/clamped
	*
	* @return time in seconds since world was brought up for play
	*/
	float GetAudioTimeSeconds() const;

	/**
	 * Returns the frame delta time in seconds adjusted by e.g. time dilation.
	 *
	 * @return frame delta time in seconds adjusted by e.g. time dilation
	 */
	float GetDeltaSeconds() const;
	
	/** Helper for getting the time since a certain time. */
	float TimeSince( float Time ) const;

	FPhysScene* GetPhysicsScene() const { return PhysicsScene; }

	/** Set the physics scene to use by this world */
	void SetPhysicsScene(FPhysScene* InScene);

	/**
	 * Returns the default physics volume and creates it if necessary.
	 * 
	 * @return default physics volume
	 */
	APhysicsVolume* GetDefaultPhysicsVolume() const;

	/**
	 * Returns the current (or specified) level's level scripting actor
	 *
	 * @param	OwnerLevel	the level to get the level scripting actor for.  Must correspond to one of the levels in GWorld's Levels array;
	 *						Thus, only applicable when editing a multi-level map.  Defaults to the level currently being edited.
	 *
	 * @return	A pointer to the level scripting actor, if any, for the specified level, or NULL if no level scripting actor is available
	 */
	class ALevelScriptActor* GetLevelScriptActor( class ULevel* OwnerLevel=NULL ) const;

	/**
	 * Returns the AWorldSettings actor associated with this world.
	 *
	 * @return AWorldSettings actor associated with this world
	 */
	AWorldSettings* GetWorldSettings( bool bCheckStreamingPesistent = false, bool bChecked = true ) const;

	/**
	 * Returns the current levels BSP model.
	 *
	 * @return BSP UModel
	 */
	UModel* GetModel() const;

	/**
	 * Returns the Z component of the current world gravity.
	 *
	 * @return Z component of current world gravity.
	 */
	float GetGravityZ() const;

	/**
	 * Returns the Z component of the default world gravity.
	 *
	 * @return Z component of the default world gravity.
	 */
	float GetDefaultGravityZ() const;

	/**
	 * Returns the name of the current map, taking into account using a dummy persistent world
	 * and loading levels into it via PrepareMapChange.
	 *
	 * @return	name of the current map
	 */
	const FString GetMapName() const;
	
	/** Accessor for bRequiresHitProxies. */
	bool RequiresHitProxies() const 
	{
		return bRequiresHitProxies;
	}

	/**
	 * Inserts the passed in controller at the front of the linked list of controllers.
	 *
	 * @param	Controller	Controller to insert, use NULL to clear list
	 */
	void AddController( AController* Controller );
	
	/**
	 * Removes the passed in controller from the linked list of controllers.
	 *
	 * @param	Controller	Controller to remove
	 */
	void RemoveController( AController* Controller );

	/**
	 * Inserts the passed in pawn at the front of the linked list of pawns.
	 *
	 * @param	Pawn	Pawn to insert, use NULL to clear list
	 */
	void AddPawn( APawn* Pawn );
	
	/**
	 * Removes the passed in pawn from the linked list of pawns.
	 *
	 * @param	Pawn	Pawn to remove
	 */
	void RemovePawn( APawn* Pawn );

	/**
	 * Adds the passed in actor to the special network actor list
	 * This list is used to specifically single out actors that are relevant for networking without having to scan the much large list
	 * @param	Actor	Actor to add
	 */
	void AddNetworkActor( AActor * Actor );
	
	/**
	 * Removes the passed in actor to from special network actor list
	 * @param	Actor	Actor to remove
	 */
	void RemoveNetworkActor( AActor * Actor );

	/** Add a listener for OnActorSpawned events */
	void AddOnActorSpawnedHandler( const FOnActorSpawned::FDelegate& InHandler );

	/** Remove a listener for OnActorSpawned events */
	void RemoveOnActorSpawnedHandler( const FOnActorSpawned::FDelegate& InHandler );

	/**
	 * Returns whether the passed in actor is part of any of the loaded levels actors array.
	 * Warning: Will return true for pending kill actors!
	 *
	 * @param	Actor	Actor to check whether it is contained by any level
	 *	
	 * @return	true if actor is contained by any of the loaded levels, false otherwise
	 */
	bool ContainsActor( AActor* Actor );

	/**
	 * Returns whether audio playback is allowed for this scene.
	 *
	 * @return true if current world is GWorld, false otherwise
	 */
	virtual bool AllowAudioPlayback();

	// Begin UObject Interface
	virtual void Serialize( FArchive& Ar ) OVERRIDE;
	virtual void FinishDestroy() OVERRIDE;
	virtual void PostLoad() OVERRIDE;
	virtual bool PreSaveRoot(const TCHAR* Filename, TArray<FString>& AdditionalPackagesToCook) OVERRIDE;
	virtual void PostSaveRoot( bool bCleanupIsRequired ) OVERRIDE;
	virtual UWorld* GetWorld() const OVERRIDE;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	virtual void PostDuplicate(bool bDuplicateForPIE) OVERRIDE;

	// End UObject Interface
	
	/**
	 * Clears all level components and world components like e.g. line batcher.
	 */
	void ClearWorldComponents();

	/**
	 * Updates world components like e.g. line batcher and all level components.
	 *
	 * @param	bRerunConstructionScripts	If we should rerun construction scripts on actors
	 * @param	bCurrentLevelOnly			If true, affect only the current level.
	 */
	void UpdateWorldComponents(bool bRerunConstructionScripts, bool bCurrentLevelOnly);

	/**
	 * Updates all cull distance volumes.
	 */
	void UpdateCullDistanceVolumes();

	/**
	 * Cleans up components, streaming data and assorted other intermediate data.
	 * @param bSessionEnded whether to notify the viewport that the game session has ended
	 */
	void CleanupWorld(bool bSessionEnded = true, bool bCleanupResources = true);
	
	/**
	 * Invalidates the cached data used to render the levels' UModel.
	 *
	 * @param	InLevel		Level to invalidate. If this is NULL it will affect ALL levels
	 */
	void InvalidateModelGeometry( ULevel* InLevel );

	/**
	 * Discards the cached data used to render the levels' UModel.  Assumes that the
	 * faces and vertex positions haven't changed, only the applied materials.
	 *
	 * @param	bCurrentLevelOnly		If true, affect only the current level.
	 */
	void InvalidateModelSurface(bool bCurrentLevelOnly);

	/**
	 * Commits changes made to the surfaces of the UModels of all levels.
	 */
	void CommitModelSurfaces();

	/** Purges all reflection capture cached derived data and forces a re-render of captured scene data. */
	void UpdateAllReflectionCaptures();

	/** Purges all sky capture cached derived data and forces a re-render of captured scene data. */
	void UpdateAllSkyCaptures();

	/**
	 * Associates the passed in level with the world. The work to make the level visible is spread across several frames and this
	 * function has to be called till it returns true for the level to be visible/ associated with the world and no longer be in
	 * a limbo state.
	 *
	 * @param Level				Level object we should add
	 * @param LevelTransform	Transformation to apply to each actor in the level
	 */
	void AddToWorld( ULevel* Level, const FTransform& LevelTransform = FTransform::Identity );

	/** 
	 * Dissociates the passed in level from the world. The removal is blocking.
	 *
	 * @param Level			Level object we should remove
	 */
	void RemoveFromWorld( ULevel* Level );

	/**
	 * Updates the all the visible worlds based on the current view location of the player and sets level LODs accordingly.	 
	 *
	 * @param ViewFamily	Optional collection of views to take into account
	 */
	void UpdateLevelStreaming( FSceneViewFamily* ViewFamily = NULL );

private:
	/**
	 * Updates the world based on the current view location of the player and sets level LODs accordingly.	 
	 * @param PersistentWorld	Persistent world
	 * @param ViewFamily		Optional collection of views to take into account
	 */
	void UpdateLevelStreamingInner( UWorld* PersistentWorld, FSceneViewFamily* ViewFamily );

	/**
	 * Evaluates current world origin location against provided view information
	 * Might issue request for world origin shifting
	 *
	 * @param ViewFamily	Collection of views to take into account
	 */
	void EvaluateWorldOriginLocation( const FSceneViewFamily& ViewFamily );

public:

	/**
	 * Keeps streaming levels list updated according to camera view
	 *
	 * @param ViewFamily Collection of views to take into account
	 */
	void UpdateWorldStreamingState(const FSceneViewFamily& ViewFamily);
	
	/**
	 * Flushes level streaming in blocking fashion and returns when all levels are loaded/ visible/ hidden
	 * so further calls to UpdateLevelStreaming won't do any work unless state changes. Basically blocks
	 * on all async operation like updating components.
	 *
	 * @param ViewFamily				Optional collection of views to take into account
	 * @param bOnlyFlushVisibility		Whether to only flush level visibility operations (optional)
	 * @param ExcludeType				Exclude packages of this type from flushing
	 */
	void FlushLevelStreaming( FSceneViewFamily* ViewFamily = NULL, bool bOnlyFlushVisibility = false, FName ExcludeType = NAME_None);

	/**
	 * Triggers a call to ULevel::BuildStreamingData(this,NULL,NULL) within a few seconds.
	 */
	void TriggerStreamingDataRebuild();

	/**
	 * Calls ULevel::BuildStreamingData(this,NULL,NULL) if it has been triggered within the last few seconds.
	 */
	void ConditionallyBuildStreamingData();

	/** @return whether there is at least one level with a pending visibility request */
	bool IsVisibilityRequestPending() const;

	/** Returns whether all the 'always loaded' levels are loaded. */
	bool AreAlwaysLoadedLevelsLoaded() const;

	/**
	 * Returns whether the level streaming code is allowed to issue load requests.
	 *
	 * @return true if level load requests are allowed, false otherwise.
	 */
	bool AllowLevelLoadRequests();

	/** Creates instances for each parameter collection in memory.  Called when a world is created. */
	void SetupParameterCollectionInstances();

	/** Adds a new instance of the given collection, or overwrites an existing instance if there is one. */
	void AddParameterCollectionInstance(class UMaterialParameterCollection* Collection, bool bUpdateScene);

	/** Gets this world's instance for a given collection. */
	UMaterialParameterCollectionInstance* GetParameterCollectionInstance(const UMaterialParameterCollection* Collection);

	/** Updates this world's scene with the list of instances, and optionally updates each instance's uniform buffer. */
	void UpdateParameterCollectionInstances(bool bUpdateInstanceUniformBuffers);

	struct InitializationValues
	{
		InitializationValues()
			: bInitializeScenes(true)
			, bAllowAudioPlayback(true)
			, bRequiresHitProxies(true)
			, bCreatePhysicsScene(true)
			, bCreateNavigation(false)
			, bShouldSimulatePhysics(true)
			, bEnableTraceCollision(false)
			, bTransactional(true)
			, bCreateWorldComposition(false)
		{
		}

		uint32 bInitializeScenes:1;
		uint32 bAllowAudioPlayback:1;
		uint32 bRequiresHitProxies:1;
		uint32 bCreatePhysicsScene:1;
		uint32 bCreateNavigation:1;
		uint32 bShouldSimulatePhysics:1;
		uint32 bEnableTraceCollision:1;
		uint32 bTransactional:1;
		uint32 bCreateWorldComposition:1;


		InitializationValues& InitializeScenes(const bool bInitialize) { bInitializeScenes = bInitialize; return *this; }
		InitializationValues& AllowAudioPlayback(const bool bAllow) { bAllowAudioPlayback = bAllow; return *this; }
		InitializationValues& RequiresHitProxies(const bool bRequires) { bRequiresHitProxies = bRequires; return *this; }
		InitializationValues& CreatePhysicsScene(const bool bCreate) { bCreatePhysicsScene = bCreate; return *this; }
		InitializationValues& CreateNavigation(const bool bCreate) { bCreateNavigation = bCreate; return *this; }
		InitializationValues& ShouldSimulatePhysics(const bool bInShouldSimulatePhysics) { bShouldSimulatePhysics = bInShouldSimulatePhysics; return *this; }
		InitializationValues& EnableTraceCollision(const bool bInEnableTraceCollision) { bEnableTraceCollision = bInEnableTraceCollision; return *this; }
		InitializationValues& SetTransactional(const bool bInTransactional) { bTransactional = bInTransactional; return *this; }
		InitializationValues& CreateWorldComposition(const bool bCreate) { bCreateWorldComposition = bCreate; return *this; }
	};

	/**
	 * Initializes the world, associates the persistent level and sets the proper zones.
	 */
	void InitWorld(const InitializationValues IVS = InitializationValues());

	/**
	 * Initializes a newly created world.
	 */
	void InitializeNewWorld(const InitializationValues IVS = InitializationValues());
	
	/**
	 * Static function that creates a new UWorld and returns a pointer to it
	 */
	static UWorld* CreateWorld( const EWorldType::Type InWorldType, bool bInformEngineOfWorld, FName WorldName = NAME_None, UPackage* InWorldPackage = NULL, bool bAddToRoot = true );

	/** 
	 * Destroy this World instance 
	 */
	void DestroyWorld( bool bInformEngineOfWorld );

	/**
	 *  Interface to allow WorldSettings to request immediate garbage collection
	 */
	void PerformGarbageCollectionAndCleanupActors();

	/**
	 *  Requests a one frame delay of Garbage Collection
	 */
	void DelayGarbageCollection();

protected:

	/**
	 *	Remove NULL entries from actor list. Only does so for dynamic actors to avoid resorting. 
	 *	In theory static actors shouldn't be deleted during gameplay.
	 */
	void CleanupActors();	

public:

	/** Get the event that broadcasts TickDispatch */
	FOnNetTickEvent& OnTickDispatch() { return TickDispatchEvent; }
	/** Get the event that broadcasts TickFlush */
	FOnNetTickEvent& OnTickFlush() { return TickFlushEvent; }
	/** Get the event that broadcasts TickFlush */
	FOnTickFlushEvent& OnPostTickFlush() { return PostTickFlushEvent; }

	/**
	 * Update the level after a variable amount of time, DeltaSeconds, has passed.
	 * All child actors are ticked after their owners have been ticked.
	 */
	void Tick( ELevelTick TickType, float DeltaSeconds );

	/**
	 * Set up the physics tick function if they aren't already
	 */
	void SetupPhysicsTickFunctions(float DeltaSeconds);

	/**
	 * Run a tick group, ticking all actors and components
	 * @param Group - Ticking group to run
	 * @param bBlockTillComplete - if true, do not return until all ticks are complete
	 */
	void RunTickGroup(ETickingGroup Group, bool bBlockTillComplete);

	/**
	 * Mark a component as needing an end of frame update
	 * @param Component - Component to update at the end of the frame
	 * @param bForceGameThread - if true, force this to happen on the game thread
	 */
	void MarkActorComponentForNeededEndOfFrameUpdate(class UActorComponent* Component, bool bForceGameThread);

	/**
	 * Send all render updates to the rendering thread.
	 * @param OutCompletion - all async updates are added to this array, they typically need to be completed before GC or anything else
	 */
	void SendAllEndOfFrameUpdates(FGraphEventArray* OutCompletion = NULL);


	/** @todo document */
	void TickNetClient( float DeltaSeconds );

	/**
	 * Issues level streaming load/unload requests based on whether
	 * local players are inside/outside level streaming volumes.
	 *
	 * @param OverrideViewLocation Optional position used to override the location used to calculate current streaming volumes
	 */
	void ProcessLevelStreamingVolumes(FVector* OverrideViewLocation=NULL);

	/**
	 * Transacts the specified level -- the correct way to modify a level
	 * as opposed to calling Level->Modify.
	 */
	void ModifyLevel(ULevel* Level);

	/**
	 * Ensures that the collision detection tree is fully built. This should be called after the full level reload to make sure
	 * the first traces are not abysmally slow.
	 */
	void EnsureCollisionTreeIsBuilt();

#if WITH_EDITOR	
	/** Returns the SelectedLevelsChangedEvent member. */
	FOnSelectedLevelsChangedEvent& OnSelectedLevelsChanged() { return SelectedLevelsChangedEvent; }

	/**
	 * Flag a level as selected.
	 */
	void SelectLevel( ULevel* InLevel );
	
	/**
	 * Flag a level as not selected.
	 */
	void DeSelectLevel( ULevel* InLevel );

	/**
	 * Query whether or not a level is selected.
	 */
	bool IsLevelSelected( ULevel* InLevel ) const;

	/**
	 * Set the selected levels from the given array (Clears existing selections)
	 */
	void SetSelectedLevels( const TArray<class ULevel*>& InLevels );

	/**
	 * Return the number of levels in this world.
	 */
	int32 GetNumSelectedLevels() const;
	
	/**
	 * Return the selected level with the given index.
	 */
	ULevel* GetSelectedLevel( int32 InLevelIndex ) const;

	/**
	 * Return the list of selected levels in this world.
	 */
	TArray<class ULevel*>& GetSelectedLevels();

	/** @todo document */
	void ShrinkLevel();
#endif // WITH_EDITOR
	
	/**
	 * Returns an iterator for the level list.
	 */
	FConstLevelIterator		GetLevelIterator() const;

	/**
	 * Return the level with the given index.
	 */
	ULevel* GetLevel( int32 InLevelIndex ) const;

	/**
	 * Does the level list contain the given level.
	 */
	bool ContainsLevel( ULevel* InLevel ) const;

	/**
	 * Return the number of levels in this world.
	 */
	int32 GetNumLevels() const;

	/**
	 * Return the list of levels in this world.
	 */
	const TArray<class ULevel*>& GetLevels() const;

	/**
	 * Add a level to the level list.
	 */
	bool AddLevel( ULevel* InLevel );
	
	/**
	 * Remove a level from the level list.
	 */
	bool RemoveLevel( ULevel* InLevel );

	/** @todo document */
	bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar=*GLog );

	/**
	 * Exec command handlers
	 */
	bool HandleTraceTagCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleFlushPersistentDebugLinesCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleLogActorCountsCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );
	
	// Start listening for connections.
	bool Listen( FURL& InURL );

	/** @return true if this level is a client */
	bool IsClient();

	/** @return true if this level is a server */
	bool IsServer();

	/** @todo document */
	bool IsPaused();

	/**
	 * Wrapper for DestroyActor() that should be called in the editor.
	 *
	 * @param	bShouldModifyLevel		If true, Modify() the level before removing the actor.
	 */
	bool EditorDestroyActor( AActor* Actor, bool bShouldModifyLevel );

	/**
	 * Removes the actor from its level's actor list and generally cleans up the engine's internal state.
	 * What this function does not do, but is handled via garbage collection instead, is remove references
	 * to this actor from all other actors, and kill the actor's resources.  This function is set up so that
	 * no problems occur even if the actor is being destroyed inside its recursion stack.
	 *
	 * @param	ThisActor				Actor to remove.
	 * @param	bNetForce				[opt] Ignored unless called during play.  Default is false.
	 * @param	bShouldModifyLevel		[opt] If true, Modify() the level before removing the actor.  Default is true.
	 * @return							true if destroy, false if actor couldn't be destroyed.
	 */
	bool DestroyActor( AActor* Actor, bool bNetForce=false, bool bShouldModifyLevel=true );

	/**
	 * Removes the passed in actor from the actor lists. Please note that the code actually doesn't physically remove the
	 * index but rather clears it so other indices are still valid and the actors array size doesn't change.
	 *
	 * @param	Actor					Actor to remove.
	 * @param	bShouldModifyLevel		If true, Modify() the level before removing the actor if in the editor.
	 */
	void RemoveActor( AActor* Actor, bool bShouldModifyLevel );
	
	
	AActor* SpawnActor( UClass* Class, FVector const* Location=NULL, FRotator const* Rotation=NULL, const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters() );

	// Templated version of SpawnActor that allows you to specify a class type
	template< class T >
	T* SpawnActor( const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters() )
	{
		return CastChecked<T>(SpawnActor(T::StaticClass(), NULL, NULL, SpawnParameters),ECastCheckedType::NullAllowed);
	};

	
	template< class T >
	T* SpawnActor( FVector const& Location, FRotator const& Rotation, const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters() )
	{
		return CastChecked<T>(SpawnActor(T::StaticClass(), &Location, &Rotation, SpawnParameters),ECastCheckedType::NullAllowed);
	};
	
	template< class T >
	T* SpawnActor( UClass* Class, const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters() )
	{
		return CastChecked<T>(SpawnActor(Class, NULL, NULL, SpawnParameters),ECastCheckedType::NullAllowed);
	};

	template< class T >
	T* SpawnActor( UClass* Class, FVector const& Location, FRotator const& Rotation, const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters() )
	{
		return CastChecked<T>(SpawnActor(Class, &Location, &Rotation, SpawnParameters),ECastCheckedType::NullAllowed);
	};


	/**
	* Spawns given class and returns class T pointer, forcibly sets world position. WILL NOT run Construction Script of Blueprints 
	* to give caller an opportunity to set parameters beforehand.  Caller is responsible for invoking construction
	* manually by calling UGameplayStatics::FinishSpawningActor (see AActor::OnConstruction).
	*/
	template< class T >
	T* SpawnActorDeferred(
		UClass* Class,
		FVector const& Location,
		FRotator const& Rotation,
		AActor* Owner=NULL,
		APawn* Instigator=NULL,
		bool bNoCollisionFail=false
		)
	{
		if( Owner )
		{
			check(this==Owner->GetWorld());
		}
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.bNoCollisionFail = bNoCollisionFail;
		SpawnInfo.Owner = Owner;
		SpawnInfo.Instigator = Instigator;
		SpawnInfo.bDeferConstruction = true;
		return (Class != NULL) ? Cast<T>(SpawnActor(Class, &Location, &Rotation, SpawnInfo )) : NULL;
	};

	/** Returns the current GameMode instance, valid only on server. */
	template< class T >
	T* GetAuthGameMode() const
	{
		return Cast<T>(AuthorityGameMode);
	}
	AGameMode* GetAuthGameMode() const { return AuthorityGameMode; };
	
	/** Returns the current GameState. */
	template< class T >
	T* GetGameState() const
	{
		return Cast<T>(GameState);
	}

	void CopyGameState(AGameMode* FromGameMode, AGameState* FromGameState);


	/** @todo document */
	ABrush*	SpawnBrush();

	/** 
	 * Spawns a PlayerController and binds it to the passed in Player with the specified RemoteRole and options
	 * 
	 * @param Player - the Player to set on the PlayerController
	 * @param RemoteRole - the RemoteRole to set on the PlayerController
	 * @param URL - URL containing player options (name, etc)
	 * @param UniqueId - unique net ID of the player (may be zeroed if no online subsystem or not logged in, e.g. a local game or LAN match)
	 * @param Error (out) - if set, indicates that there was an error - usually is set to a property from which the calling code can look up the actual message
	 * @param InNetPlayerIndex (optional) - the NetPlayerIndex to set on the PlayerController
	 * @return the PlayerController that was spawned (may fail and return NULL)
	 */
	APlayerController* SpawnPlayActor(class UPlayer* Player, ENetRole RemoteRole, const FURL& URL, const TSharedPtr<class FUniqueNetId>& UniqueId, FString& Error, uint8 InNetPlayerIndex = 0);

	/** Try to find an acceptable position to place TestActor as close to possible to PlaceLocation.  Expects PlaceLocation to be a valid location inside the level. */
	bool FindTeleportSpot( AActor* TestActor, FVector& PlaceLocation, FRotator PlaceRotation );

	/** @Return true if Actor would encroach at TestLocation on something that blocks it.  Returns a ProposedAdjustment that might result in an unblocked TestLocation. */
	bool EncroachingBlockingGeometry( AActor* TestActor, FVector TestLocation, FRotator TestRotation, FVector* ProposedAdjustment = NULL );

	/** Begin physics simulation */ 
	void StartPhysicsSim();

	/** Waits for the physics scene to be done processing */
	void FinishPhysicsSim();

	/** Begin cloth simulation */
	void StartClothSim();

	/** Spawns GameMode for the level. */
	bool SetGameMode(const FURL& InURL);

	/** 
	 * Initializes all actors and prepares them to start gameplay
	 * @param InURL commandline URL
	 * @param bResetTime (optional) whether the WorldSettings's TimeSeconds should be reset to zero
	 */
	void InitializeActorsForPlay(const FURL& InURL, bool bResetTime = true);

	/**
	 * Start gameplay. This will cause the game mode to transition to the correct state and call BeginPlay on all actors
	 */
	void BeginPlay();

	/** 
	 * Looks for a PlayerController that was being swapped by the given NetConnection and, if found, destroys it
	 * (because the swap is complete or the connection was closed)
	 * @param Connection - the connection that performed the swap
	 * @return whether a PC waiting for a swap was found
	 */
	bool DestroySwappedPC(UNetConnection* Connection);

	// Begin FNetworkNotify interface
	virtual EAcceptConnection::Type NotifyAcceptingConnection() OVERRIDE;

	/** @todo document */
	virtual void NotifyAcceptedConnection( class UNetConnection* Connection ) OVERRIDE;
	virtual bool NotifyAcceptingChannel( class UChannel* Channel ) OVERRIDE;
	virtual void NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, class FInBunch& Bunch) OVERRIDE;
	// End FNetworkNotify interface

	/** Welcome a new player joining this server. */
	void WelcomePlayer(UNetConnection* Connection);

	/**
	 * Used to get a net driver object by name. Default name is the game net driver
	 * @param NetDriverName the name of the net driver being asked for
	 * @return a pointer to the net driver or NULL if the named driver is not found
	 */
	UNetDriver* GetNetDriver() const
	{
		return NetDriver;
	}

	ENetMode GetNetMode() const;

	/**
	 * Sets the net driver to use for this world
	 * @param NewDriver the new net driver to use
	 */
	void SetNetDriver(UNetDriver* NewDriver)
	{
		NetDriver = NewDriver;
	}

	void AssignActorNewNetGUID(AActor* Actor, bool bIsStatic);

	/**
	 * Sets the number of frames to delay Streaming Volume updating, 
	 * useful if you preload a bunch of levels but the camera hasn't caught up yet 
	 */
	void DelayStreamingVolumeUpdates(int32 InFrameDelay)
	{
		StreamingVolumeUpdateDelay = InFrameDelay;
	}

	/**
	 * Transfers the set of Kismet / Blueprint objects being debugged to the new world that are not already present, and updates blueprints accordingly
	 * @param	NewWorld	The new world to find equivalent objects in
	 */
	void TransferBlueprintDebugReferences(UWorld* NewWorld);

	/**
	 * Notifies the world of a blueprint debugging reference
	 * @param	Blueprint	The blueprint the reference is for
	 * @param	DebugObject The associated debugging object (may be NULL)
	 */
	void NotifyOfBlueprintDebuggingAssociation(class UBlueprint* Blueprint, UObject* DebugObject);

	/** Broadcasts that the number of levels has changed. */
	void BroadcastLevelsChanged() { LevelsChangedEvent.Broadcast(); }

	/** Returns the LevelsChangedEvent member. */
	FOnLevelsChangedEvent& OnLevelsChanged() { return LevelsChangedEvent; }

	/** Returns the actor count. */
	int32 GetProgressDenominator();
	
	/** Returns the actor count. */
	int32 GetActorCount();
	
	/** Returns the net relevant actor count. */
	int32 GetNetRelevantActorCount();

public:

	/*****************************************************************************************************/
	/** Moved from WorldSettings properties - START 														**/
	/*****************************************************************************************************/

	/**
	 * Finds the reverb settings to use for a given view location, taking into account the world's default
	 * settings and the reverb volumes in the world.
	 *
	 * @param	ViewLocation			Current view location.
	 * @param	OutReverbSettings		[out] Upon return, the reverb settings for a camera at ViewLocation.
	 * @param	OutInteriorSettings		[out] Upon return, the interior settings for a camera at ViewLocation.
	 * @return							If the settings came from a reverb volume, the reverb volume's object index is returned.
	 */
	class AReverbVolume* GetAudioSettings( const FVector& ViewLocation, struct FReverbSettings* OutReverbSettings, struct FInteriorSettings* OutInteriorSettings );

	void UpdateMusicTrack(FMusicTrackStruct NewMusicTrack);

	/** Return the URL of this level on the local machine. */
	virtual FString GetLocalURL() const;

	/** Returns whether script is executing within the editor. */
	bool IsPlayInEditor() const;

	/** Returns whether script is executing within a preview window */
	bool IsPlayInPreview() const;

	/** Returns whether script is executing within a mobile preview window */
	bool IsPlayInMobilePreview() const;

	/** Returns true if this world is any kind of game world (including PIE worlds) */
	bool IsGameWorld() const;

	// Return the URL of this level, which may possibly
	// exist on a remote machine.
	virtual FString GetAddressURL() const;

	/**
	 * Called after GWorld has been set. Used to load, but not associate, all
	 * levels in the world in the Editor and at least create linkers in the game.
	 * Should only be called against GWorld::PersistentLevel's WorldSettings.
	 *
	 * @param bForce	If true, load the levels even is a commandlet
	 */
	void LoadSecondaryLevels(bool bForce = false, TSet<FString>* CookedPackages = NULL);

	/** Utility for returning the ULevelStreaming object for a particular sub-level, specified by package name */
	ULevelStreaming* GetLevelStreamingForPackageName(FName PackageName);

#if WITH_EDITOR
	/** 
	 * Called when level property has changed
	 * It refreshes any streaming stuff
	 */
	void RefreshStreamingLevels();

	/**
	 * Called when a specific set of streaming levels need to be refreshed
	 * @param LevelsToRefresh A TArray<ULevelStreaming*> containing pointers to the levels to refresh
	 */
	void RefreshStreamingLevels( const TArray<class ULevelStreaming*>& InLevelsToRefresh );

#endif

	/**
	 * Jumps the server to new level.  If bAbsolute is true and we are using seemless traveling, we
	 * will do an absolute travel (URL will be flushed).
	 *
	 * @param URL the URL that we are traveling to
	 * @param bAbsolute whether we are using relative or absolute travel
	 * @param bShouldSkipGameNotify whether to notify the clients/game or not
	 */
	virtual void ServerTravel(const FString& URL, bool bAbsolute = false, bool bShouldSkipGameNotify = false);

	/** seamlessly travels to the given URL by first loading the entry level in the background,
	 * switching to it, and then loading the specified level. Does not disrupt network communication or disconnect clients.
	 * You may need to implement GameMode::GetSeamlessTravelActorList(), PlayerController::GetSeamlessTravelActorList(),
	 * GameMode::PostSeamlessTravel(), and/or GameMode::HandleSeamlessTravelPlayer() to handle preserving any information
	 * that should be maintained (player teams, etc)
	 * This codepath is designed for worlds that use little or no level streaming and GameModes where the game state
	 * is reset/reloaded when transitioning. (like UT)
	 * @param URL - the URL to travel to; must be on the same server as the current URL
	 * @param bAbsolute (opt) - if true, URL is absolute, otherwise relative
	 * @param MapPackageGuid (opt) - the GUID of the map package to travel to - this is used to find the file when it has been auto-downloaded,
	 * 				so it is only needed for clients
	 */
	void SeamlessTravel(const FString& URL, bool bAbsolute = false, FGuid MapPackageGuid = FGuid());

	/** @return whether we're currently in a seamless transition */
	bool IsInSeamlessTravel();

	/** this function allows pausing the seamless travel in the middle,
	 * right before it starts loading the destination (i.e. while in the transition level)
	 * this gives the opportunity to perform any other loading tasks before the final transition
	 * this function has no effect if we have already started loading the destination (you will get a log warning if this is the case)
	 * @param bNowPaused - whether the transition should now be paused
	 */
	void SetSeamlessTravelMidpointPause(bool bNowPaused);

	/** @return the current detail mode, like EDetailMode but can be outside of the range */
	int32 GetDetailMode();

	/** @todo document */
	void ForceGarbageCollection( bool bFullPurge = false );

	/** asynchronously loads the given levels in preparation for a streaming map transition.
	 * This codepath is designed for worlds that heavily use level streaming and GameModes where the game state should
	 * be preserved through a transition.
	 * @param LevelNames the names of the level packages to load. LevelNames[0] will be the new persistent (primary) level
	 */
	void PrepareMapChange(const TArray<FName>& LevelNames);

	/** @return true if there's a map change currently in progress */
	bool IsPreparingMapChange();

	/** @return true if there is a map change being prepared, returns whether that change is ready to be committed, otherwise false */
	bool IsMapChangeReady();

	/** cancels pending map change (@note: we can't cancel pending async loads, so this won't immediately free the memory) */
	void CancelPendingMapChange();

	/** actually performs the map transition prepared by PrepareMapChange()
	 * it happens in the next tick to avoid GC issues
	 * if a map change is being prepared but isn't ready yet, the transition code will block until it is
	 * wait until IsMapChangeReady() returns true if this is undesired behavior
	 */
	void CommitMapChange();

	/**
	 * Sets NumLightingUnbuiltObjects to the specified value.  Marks the worldsettings package dirty if the value changed.
	 * @param	InNumLightingUnbuiltObjects			The new value.
	 */
	void SetMapNeedsLightingFullyRebuilt(int32 InNumLightingUnbuiltObjects);

	/** Returns TimerManager instance for this world. */
	inline FTimerManager& GetTimerManager() const
	{
		return *TimerManager;
	}

	/** Returns LatentActionManager instance for this world. */
	inline FLatentActionManager& GetLatentActionManager()
	{
		return 	LatentActionManager;
	}

	/** Retrieves information whether all navigation with this world has been rebuilt */
	bool IsNavigationRebuilt() const;

	/** Setup runtime objects for world composition based on folder of currently loaded map */
	void InitializeWorldComposition();

	/** Request to translate world origin to specified position on next tick */
	void RequestNewWorldOrigin(const FIntPoint& InNewOrigin);
	
	/** Translate world origin to specified position  */
	bool SetNewWorldOrigin(const FIntPoint& InNewOrigin);

	/** Sets world origin at specified position and stream-in all relevant levels */
	void NavigateTo(FIntPoint InPosition);

	/** Gets all matinee actors for the current level */
	void GetMatineeActors( TArray<AMatineeActor*>& OutMatineeActors );

	/** Updates all physics constraint actor joint locations.  */
	virtual void UpdateConstraintActors();

public:
	static FString ConvertToPIEPackageName(const FString& PackageName, int32 PIEInstanceID);
	static FString BuildPIEPackagePrefix(int32 PIEInstanceID);
	static UWorld* DuplicateWorldForPIE(const FString& PackageName, UWorld* OwningWorld);
	static FString RemovePIEPrefix(const FString &Source);
	static UWorld* FindWorldInPackage(UPackage* Package);
};

/** Global UWorld pointer */
extern ENGINE_API class UWorldProxy GWorld;

/** World delegates */
class ENGINE_API FWorldDelegates
{
public:
	DECLARE_MULTICAST_DELEGATE_TwoParams(FWorldInitializationEvent, UWorld* /*World*/, const UWorld::InitializationValues /*IVS*/);
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FWorldCleanupEvent, UWorld* /*World*/, bool /*bSessionEnded*/, bool /*bCleanupResources*/);
	DECLARE_MULTICAST_DELEGATE_OneParam(FWorldEvent, UWorld* /*World*/);
	// Delegate type for level change events
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLevelChanged, ULevel*, UWorld*);
	
	// Callback for world initialization (pre)
	static FWorldInitializationEvent OnPreWorldInitialization;
	
	// Callback for world initialization (post)
	static FWorldInitializationEvent OnPostWorldInitialization;

	// Callback for world cleanup
	static FWorldCleanupEvent OnWorldCleanup;

	// Callback for world destruction (only called for initialized worlds)
	static FWorldEvent OnPreWorldFinishDestroy;

	// Sent when a ULevel is added to the world via UWorld::AddToWorld
	static FOnLevelChanged			LevelAddedToWorld;

	// Sent when a ULevel is removed from the world via UWorld::RemoveFromWorld or 
	// LoadMap (a NULL object means the LoadMap case, because all levels will be 
	// removed from the world without a RemoveFromWorld call for each)
	static FOnLevelChanged			LevelRemovedFromWorld;

private:
	FWorldDelegates() {}
};
