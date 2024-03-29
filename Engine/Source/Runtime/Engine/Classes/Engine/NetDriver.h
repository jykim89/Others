// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

//
// Base class of a network driver attached to an active or pending level.
#pragma once
//
#include "NetDriver.generated.h"


//
// Whether to support net lag and packet loss testing.
//
#define DO_ENABLE_NET_TEST !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#if DO_ENABLE_NET_TEST
/** Holds the packet simulation settings in one place */
struct FPacketSimulationSettings
{
	int32	PktLoss;
	int32	PktOrder;
	int32	PktDup;
	int32	PktLag;
	int32	PktLagVariance;

	/** Ctor. Zeroes the settings */
	FPacketSimulationSettings() : 
		PktLoss(0),
		PktOrder(0),
		PktDup(0),
		PktLag(0),
		PktLagVariance(0) 
	{
	}

	/** reads in settings from the .ini file 
	 * @note: overwrites all previous settings
	 */
	void LoadConfig();

	/**
	 * Registers commands for auto-completion, etc.
	 */
	void RegisterCommands();

	/**
	 * Unregisters commands for auto-completion, etc.
	 */
	void UnregisterCommands();

	/**
	 * Reads the settings from a string: command line or an exec
	 *
	 * @param Stream the string to read the settings from
	 */
	bool ParseSettings(const TCHAR* Stream);
};
#endif

//
// Priority sortable list.
//
struct FActorPriority
{
	int32					Priority;	// Update priority, higher = more important.
	
	class AActor*			Actor;		// Actor.	
	class UActorChannel*	Channel;	// Actor channel.

	struct FActorDestructionInfo *	DestructionInfo;	// Destroy an actor

	FActorPriority() : 
		Priority(0), Actor(NULL), Channel(NULL), DestructionInfo(NULL)
	{}

	FActorPriority(class UNetConnection* InConnection, class UActorChannel* InChannel, class AActor* InActor, const TArray<struct FNetViewer>& Viewers, bool bLowBandwidth);
	FActorPriority(class UNetConnection* InConnection, struct FActorDestructionInfo * DestructInfo, const TArray<struct FNetViewer>& Viewers );
};

struct FActorDestructionInfo
{
	TWeakObjectPtr<UObject>		ObjOuter;
	FVector			DestroyedPosition;
	FNetworkGUID	NetGUID;
	FString			PathName;

	FName			StreamingLevelName;
};


UCLASS(dependson=UEngineTypes, Abstract, customConstructor, transient, MinimalAPI, config=Engine)
class UNetDriver : public UObject, public FExec
{
	GENERATED_UCLASS_BODY()

protected:

	ENGINE_API void InternalProcessRemoteFunction(class AActor* Actor, class UObject * SubObject, class UNetConnection* Connection, class UFunction* Function, void* Parms, FOutParmRec* OutParms, FFrame* Stack, bool IsServer);

public:

	/** Used to specify the class to use for connections */
	UPROPERTY(Config)
	FString NetConnectionClassName;

	/** @todo document */
	UPROPERTY(Config)
	int32 MaxDownloadSize;

	/** @todo document */
	UPROPERTY(Config)
	uint32 bClampListenServerTickRate:1;

	/** @todo document */
	UPROPERTY(Config)
	int32 NetServerMaxTickRate;

	/** @todo document */
	UPROPERTY(Config)
	int32 MaxInternetClientRate;

	/** @todo document */
	UPROPERTY(Config)
	int32 MaxClientRate;

	/** Amount of time a server will wait before traveling to next map, gives clients time to receive final RPCs on existing level @see NextSwitchCountdown */
	UPROPERTY(Config)
	float ServerTravelPause;

	/** @todo document */
	UPROPERTY(Config)
	float SpawnPrioritySeconds;

	/** @todo document */
	UPROPERTY(Config)
	float RelevantTimeout;

	/** @todo document */
	UPROPERTY(Config)
	float KeepAliveTime;

	/** @todo document */
	UPROPERTY(Config)
	float InitialConnectTimeout;

	/** @todo document */
	UPROPERTY(Config)
	float ConnectionTimeout;

	/** Requires engine version to match exactly in order to connect. Else fall back to GEngineMinNetVersion check */
	UPROPERTY(Config)
	bool RequireEngineVersionMatch;

	/** Connection to the server (this net driver is a client) */
	UPROPERTY()
	class UNetConnection* ServerConnection;

	/** Array of connections to clients (this net driver is a host) */
	UPROPERTY()
	TArray<class UNetConnection*> ClientConnections;

	/** World this net driver is associated with */
	UPROPERTY()
	class UWorld* World;

	/** @todo document */
	UPROPERTY()
	UPackageMap* MasterMap;

	/** The loaded UClass of the net connection type to use */
	UPROPERTY()
	UClass* NetConnectionClass;

	/** @todo document */
	UPROPERTY()
	UProperty* RoleProperty;
	
	/** @todo document */
	UPROPERTY()
	UProperty* RemoteRoleProperty;

	/** Used to specify the net driver to filter actors with (NAME_None || NAME_GameNetDriver is the default net driver) */
	UPROPERTY(Config)
	FName NetDriverName;

	/** Interface for communication network state to others (ie World usually, but anything that implements FNetworkNotify) */
	class FNetworkNotify*		Notify;
	
	/** Accumulated time for the net driver, updated by Tick */
	UPROPERTY()
	float						Time;
	/** If true then client connections are to other client peers */ 
	bool						bIsPeer;
	/** @todo document */
	bool						ProfileStats;
	/** Timings for Socket::SendTo() and Socket::RecvFrom() */
	int32						SendCycles, RecvCycles;
	/** Stats for network perf */
	uint32						InBytesPerSecond;
	/** todo document */
	uint32						OutBytesPerSecond;
	/** todo document */
	uint32						InBytes;
	/** todo document */
	uint32						OutBytes;
	/** Outgoing rate of NetGUID Bunches */
	uint32						NetGUIDOutBytes;
	/** Incoming rate of NetGUID Bunches */
	uint32						NetGUIDInBytes;
	/** todo document */
	uint32						InPackets;
	/** todo document */
	uint32						OutPackets;
	/** todo document */
	uint32						InBunches;
	/** todo document */
	uint32						OutBunches;
	/** todo document */
	uint32						InPacketsLost;
	/** todo document */
	uint32						OutPacketsLost;
	/** todo document */
	uint32						InOutOfOrderPackets;
	/** todo document */
	uint32						OutOutOfOrderPackets;
	/** Tracks the total number of voice packets sent */
	uint32						VoicePacketsSent;
	/** Tracks the total number of voice bytes sent */
	uint32						VoiceBytesSent;
	/** Tracks the total number of voice packets received */
	uint32						VoicePacketsRecv;
	/** Tracks the total number of voice bytes received */
	uint32						VoiceBytesRecv;
	/** Tracks the voice data percentage of in bound bytes */
	uint32						VoiceInPercent;
	/** Tracks the voice data percentage of out bound bytes */
	uint32						VoiceOutPercent;
	/** Time of last stat update */
	double						StatUpdateTime;
	/** Interval between gathering stats */
	float						StatPeriod;

	/** Used to determine if checking for standby cheats should occur */
	bool						bIsStandbyCheckingEnabled;
	/** Used to determine whether we've already caught a cheat or not */
	bool						bHasStandbyCheatTriggered;
	/** The amount of time without packets before triggering the cheat code */
	float						StandbyRxCheatTime;
	/** todo document */
	float						StandbyTxCheatTime;
	/** The point we think the host is cheating or shouldn't be hosting due to crappy network */
	int32						BadPingThreshold;
	/** The number of clients missing data before triggering the standby code */
	float						PercentMissingForRxStandby;
	float						PercentMissingForTxStandby;
	/** The number of clients with bad ping before triggering the standby code */
	float						PercentForBadPing;
	/** The amount of time to wait before checking a connection for standby issues */
	float						JoinInProgressStandbyWaitTime;
	/** Used to track whether a given actor was replicated by the net driver recently */
	int32						NetTag;
	/** Dumps next net update's relevant actors when true*/
	bool						DebugRelevantActors;

	TArray< TWeakObjectPtr<AActor> >	LastPrioritizedActors;
	TArray< TWeakObjectPtr<AActor> >	LastRelevantActors;
	TArray< TWeakObjectPtr<AActor> >	LastSentActors;
	TArray< TWeakObjectPtr<AActor> >	LastNonRelevantActors;

	void						PrintDebugRelevantActors();
	
	/** The server adds an entry into this map for every actor that is destroyed that join-in-progress
	 *  clients need to know about, that is, startup actors. Also, individual UNetConnections
	 *  need to keep track of FActorDestructionInfo for dormant and recently-dormant actors in addition
	 *  to startup actors (because they won't have an associated channel), and this map stores those
	 *  FActorDestructionInfos also.
	 */
	TMap<FNetworkGUID, FActorDestructionInfo>	DestroyedStartupOrDormantActors;

	/** Maps FRepChangedPropertyTracker to active objects that are replicating properties */
	TMap< TWeakObjectPtr< UObject >, TSharedPtr< FRepChangedPropertyTracker > >	RepChangedPropertyTrackerMap;
	/** Used to invalidate properties marked "unchanged" in FRepChangedPropertyTracker's */
	uint32																		ReplicationFrame;

	/** Maps FRepLayout to the respective UClass */
	TMap< TWeakObjectPtr< UObject >, TSharedPtr< FRepLayout > >					RepLayoutMap;

	/** Creates if necessary, and returns a FRepLayout that maps to the passed in UClass */
	TSharedPtr< FRepLayout >	GetObjectClassRepLayout( UClass * Class );

	/** Creates if necessary, and returns a FRepLayout that maps to the passed in UFunction */
	TSharedPtr<FRepLayout>		GetFunctionRepLayout( UFunction * Function );

	/** Creates if necessary, and returns a FRepLayout that maps to the passed in UStruct */
	TSharedPtr<FRepLayout>		GetStructRepLayout( UStruct * Struct );

	TSet< TWeakPtr< FObjectReplicator > > UnmappedReplicators;

	/**
	* Updates the standby cheat information and
	 * causes the dialog to be shown/hidden as needed
	 */
	void UpdateStandbyCheatStatus(void);

#if DO_ENABLE_NET_TEST
	FPacketSimulationSettings	PacketSimulationSettings;
#endif

	// Constructors.
	ENGINE_API UNetDriver(const class FPostConstructInitializeProperties& PCIP);


	// Begin UObject interface.
	ENGINE_API virtual void PostInitProperties() OVERRIDE;
	ENGINE_API virtual void FinishDestroy() OVERRIDE;
	ENGINE_API virtual void Serialize( FArchive& Ar ) OVERRIDE;
	ENGINE_API static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	// End UObject interface.

	// Begin FExec interface

	/**
	 * Handle exec commands
	 *
	 * @param InWorld	the world context
	 * @param Cmd		the exec command being executed
	 * @param Ar		the archive to log results to
	 *
	 * @return true if the handler consumed the input, false to continue searching handlers
	 */
	ENGINE_API virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar=*GLog) OVERRIDE;

	ENGINE_API ENetMode	GetNetMode() const;

	// End FExec interface.

	/** 
	 * Returns true if this net driver is valid for the current configuration.
	 * Safe to call on a CDO if necessary
	 *
	 * @return true if available, false otherwise
	 */
	ENGINE_API virtual bool IsAvailable() const PURE_VIRTUAL( UNetDriver::IsAvailable, return false;)

	/**
	 * Common initialization between server and client connection setup
	 * 
	 * @param bInitAsClient are we a client or server
	 * @param InNotify notification object to associate with the net driver
	 * @param URL destination
	 * @param bReuseAddressAndPort whether to allow multiple sockets to be bound to the same address/port
	 * @param Error output containing an error string on failure
	 *
	 * @return true if successful, false otherwise (check Error parameter)
	 */
	ENGINE_API virtual bool InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error);

	/**
	 * Initialize the net driver in client mode
	 *
	 * @param InNotify notification object to associate with the net driver
	 * @param ConnectURL remote ip:port of host to connect to
	 * @param Error resulting error string from connection attempt
	 * 
	 * @return true if successful, false otherwise (check Error parameter)
	 */
	ENGINE_API virtual bool InitConnect(class FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error ) PURE_VIRTUAL( UNetDriver::InitConnect, return true;);

	/**
	 * Initialize the network driver in server mode (listener)
	 *
	 * @param InNotify notification object to associate with the net driver
	 * @param ListenURL the connection URL for this listener
	 * @param bReuseAddressAndPort whether to allow multiple sockets to be bound to the same address/port
	 * @param Error out param with any error messages generated 
	 *
	 * @return true if successful, false otherwise (check Error parameter)
	 */
	ENGINE_API virtual bool InitListen(class FNetworkNotify* InNotify, FURL& ListenURL, bool bReuseAddressAndPort, FString& Error) PURE_VIRTUAL( UNetDriver::InitListen, return true;);


	/** Initializes the net connection class to use for new connections */
	ENGINE_API virtual bool InitConnectionClass(void);

	/** Shutdown all connections managed by this net driver */
	ENGINE_API virtual void Shutdown();

	/* Close socket and Free the memory the OS allocated for this socket */
	ENGINE_API virtual void LowLevelDestroy();

	/* @return network number */
	virtual FString LowLevelGetNetworkNumber() PURE_VIRTUAL(UNetDriver::LowLevelGetNetworkNumber,return TEXT(""););

	/** Make sure this connection is in a reasonable state. */
	ENGINE_API virtual void AssertValid();

	/**
	 * Called to replicate any relevant actors to the connections contained within this net driver
	 *
	 * Process as many clients as allowed given Engine.NetClientTicksPerSecond, first building a list of actors to consider for relevancy checking,
	 * and then attempting to replicate each actor for each connection that it is relevant to until the connection becomes saturated.
	 *
	 * NetClientTicksPerSecond is used to throttle how many clients are updated each frame, hoping to avoid saturating the server's upstream bandwidth, although
	 * the current solution is far from optimal.  Ideally the throttling could be based upon the server connection becoming saturated, at which point each
	 * connection is reduced to priority only updates, and spread out amongst several ticks.  Also might want to investigate eliminating the redundant consider/relevancy
	 * checks for Actors that were successfully replicated for some channels but not all, since that would make a decent CPU optimization.
	 *
	 * @param DeltaSeconds elapsed time since last call
	 *
	 * @return the number of actors that were replicated
	 */
	ENGINE_API virtual int32 ServerReplicateActors(float DeltaSeconds);

	/**
	 * Process a remote function call on some actor destined for a remote location
	 *
	 * @param Actor actor making the function call
	 * @param Function function definition called
	 * @param Params parameters in a UObject memory layout
	 * @param Stack stack frame the UFunction is called in
	 * @param SubObject optional: sub object to actually call function on
	 */
	ENGINE_API virtual void ProcessRemoteFunction(class AActor* Actor, class UFunction* Function, void* Parameters, struct FOutParmRec* OutParms, struct FFrame* Stack, class UObject * SubObject = NULL ) PURE_VIRTUAL(UNetDriver::ProcessRemoteFunction,);

	/** handle time update */
	ENGINE_API virtual void TickDispatch( float DeltaTime );

	/** ReplicateActors and Flush */
	ENGINE_API virtual void TickFlush(float DeltaSeconds);

	/** PostTick actions */
	ENGINE_API virtual void PostTickFlush();

	/**
	 * Process any local talker packets that need to be sent to clients
	 */
	ENGINE_API virtual void ProcessLocalServerPackets();

	/**
	 * Process any local talker packets that need to be sent to the server
	 */
	ENGINE_API virtual void ProcessLocalClientPackets();

	/**
	 * Determines which other connections should receive the voice packet and
	 * queues the packet for those connections. Used for sending both local/remote voice packets.
	 *
	 * @param VoicePacket the packet to be queued
	 * @param CameFromConn the connection this packet came from (NULL if local)
	 */
	ENGINE_API virtual void ReplicateVoicePacket(TSharedPtr<class FVoicePacket> VoicePacket, class UNetConnection* CameFromConn);

#if !UE_BUILD_SHIPPING
	/**
	 * Exec command handlers
	 */
	bool HandleSocketsCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandlePackageMapCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleNetFloodCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleNetDebugTextCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleNetDisconnectCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleNetDumpServerRPCCommand( const TCHAR* Cmd, FOutputDevice& Ar );
#endif

	/** Flushes actor from NetDriver's dormancy list, but does not change any state on the Actor itself */
	ENGINE_API void FlushActorDormancy(class AActor *Actor);

	/** Called when a spawned actor is destroyed. */
	ENGINE_API virtual void NotifyActorDestroyed( AActor* Actor, bool IsSeamlessTravel=false );

	ENGINE_API virtual void NotifyStreamingLevelUnload( ULevel* );

	ENGINE_API virtual void NotifyActorLevelUnloaded( AActor* Actor );
	
	/** creates a child connection and adds it to the given parent connection */
	ENGINE_API virtual class UChildConnection* CreateChild(UNetConnection* Parent);

	/** @return String that uniquely describes the net driver instance */
	FString GetDescription() 
	{ 
		return FString::Printf(TEXT("%s %s%s"), *NetDriverName.ToString(), *GetName(), bIsPeer ? TEXT("(PEER)") : TEXT(""));
	}

	/** @return true if this netdriver is handling accepting connections */
	ENGINE_API virtual bool IsServer() const;

	/** verifies that the client has loaded or can load the package with the specified information
	 * if found, sets the Info's Parent to the package and notifies the server of our generation of the package
	 * if not, handles downloading the package, skipping it, or disconnecting, depending on the requirements of the package
	 * @param Info the info on the package the client should have
	 * @return true if we're done verifying this package, false if we're not done yet (because i.e. async loading is in progress)
	 */
	bool VerifyPackageInfo(FPackageInfo& Info);

	/** Flushes and clears all packagemaps on this driver and driver's connections */
	ENGINE_API virtual void ResetPackageMaps();

	ENGINE_API virtual void LockPackageMaps();

	ENGINE_API virtual void CleanPackageMaps();

	ENGINE_API void PreSeamlessTravelGarbageCollect();

	ENGINE_API void PostSeamlessTravelGarbageCollect();

	/**
	 * Get the socket subsytem appropriate for this net driver
	 */
	virtual class ISocketSubsystem* GetSocketSubsystem() PURE_VIRTUAL(UNetDriver::GetSocketSubsystem, return NULL;);

	/**
	 * Associate a world with this net driver. 
	 * Disassociates any previous world first.
	 * 
	 * @param InWorld the world to associate with this netdriver
	 */
	ENGINE_API void SetWorld(class UWorld* InWorld);

	/**
	 * Get the world associated with this net driver
	 */
	class UWorld* GetWorld() const { return World; }

	/** Called during seamless travel to clear all state that was tied to the previous game world (actor lists, etc) */
	ENGINE_API virtual void ResetGameWorldState();

	/** @return true if the net resource is valid or false if it should not be used */
	virtual bool IsNetResourceValid(void) PURE_VIRTUAL(UNetDriver::IsNetResourceValid, return false;);

	bool NetObjectIsDynamic(const UObject *Object) const;

	/** Draws debug markers in the world based on network state */
	void DrawNetDriverDebug();

	/** 
	 * Finds a FRepChangedPropertyTracker associated with an object.
	 * If not found, creates one.
	*/
	TSharedPtr<FRepChangedPropertyTracker> FindOrCreateRepChangedPropertyTracker(UObject *Obj);

protected:

	/** Adds (fully initialized, ready to go) client connection to the ClientConnections list + any other game related setup */
	ENGINE_API void	AddClientConnection(UNetConnection * NewConnection);

	/** Register all TickDispatch, TickFlush, PostTickFlush to tick in World */
	ENGINE_API void RegisterTickEvents(class UWorld* InWorld) const;
	/** Unregister all TickDispatch, TickFlush, PostTickFlush to tick in World */
	ENGINE_API void UnregisterTickEvents(class UWorld* InWorld) const;
	/** Returns true if this actor is considered to be in a loaded level */
	bool IsLevelInitializedForActor(class AActor* InActor, class UNetConnection* InConnection);
};
