// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystem.h"
#include "OnlineSubsystemImpl.h"
#include "OnlineSubsystemSteamPackage.h"

/** Forward declarations of all interface classes */
typedef TSharedPtr<class FOnlineSessionSteam, ESPMode::ThreadSafe> FOnlineSessionSteamPtr;
typedef TSharedPtr<class FOnlineIdentitySteam, ESPMode::ThreadSafe> FOnlineIdentitySteamPtr;
typedef TSharedPtr<class FOnlineFriendsSteam, ESPMode::ThreadSafe> FOnlineFriendsSteamPtr;
typedef TSharedPtr<class FOnlineSharedCloudSteam, ESPMode::ThreadSafe> FOnlineSharedCloudSteamPtr;
typedef TSharedPtr<class FOnlineUserCloudSteam, ESPMode::ThreadSafe> FOnlineUserCloudSteamPtr;
typedef TSharedPtr<class FOnlineLeaderboardsSteam, ESPMode::ThreadSafe> FOnlineLeaderboardsSteamPtr;
typedef TSharedPtr<class FOnlineVoiceSteam, ESPMode::ThreadSafe> FOnlineVoiceSteamPtr;
typedef TSharedPtr<class FOnlineExternalUISteam, ESPMode::ThreadSafe> FOnlineExternalUISteamPtr;
typedef TSharedPtr<class FOnlineAchievementsSteam, ESPMode::ThreadSafe> FOnlineAchievementsSteamPtr;

/**
 *	OnlineSubsystemSteam - Implementation of the online subsystem for STEAM services
 */
class ONLINESUBSYSTEMSTEAM_API FOnlineSubsystemSteam : 
	public FOnlineSubsystemImpl, 
	public FTickerObjectBase
{
protected:

	/** Has the STEAM client APIs been initialized */
	bool bSteamworksClientInitialized;

	/** Whether or not the Steam game server API is initialized */
	bool bSteamworksGameServerInitialized;

	/** Steam App ID for the running game */
	uint32 SteamAppID;

	/** Steam port - the local port used to communicate with the steam servers */
	int32 GameServerSteamPort;

	/** Game port - the port that clients will connect to for gameplay */
	int32 GameServerGamePort;

	/** Query port - the port that will manage server browser related duties and info */
	int32 GameServerQueryPort;

	/** Array of the files in the cloud for a given user */
	TArray<struct FSteamUserCloudData*> UserCloudData;

	/** Interface to the session services */
	FOnlineSessionSteamPtr SessionInterface;

	/** Interface to the profile services */
	FOnlineIdentitySteamPtr IdentityInterface;

	/** Interface to the friend services */
	FOnlineFriendsSteamPtr FriendInterface;

	/** Interface to the shared cloud services */
	FOnlineSharedCloudSteamPtr SharedCloudInterface;
	
	/** Interface to the user cloud services */
	FOnlineUserCloudSteamPtr UserCloudInterface;

	/** Interface to the leaderboard services */
	FOnlineLeaderboardsSteamPtr LeaderboardsInterface;

	/** Interface to the voice engine */
	FOnlineVoiceSteamPtr VoiceInterface;

	/** Interface to the external UI services */
	FOnlineExternalUISteamPtr ExternalUIInterface;

	/** Interface for achievements */
	FOnlineAchievementsSteamPtr AchievementsInterface;

	/** Online async task runnable */
	class FOnlineAsyncTaskManagerSteam* OnlineAsyncTaskThreadRunnable;

	/** Online async task thread */
	class FRunnableThread* OnlineAsyncTaskThread;

PACKAGE_SCOPE:

	/** Only the factory makes instances */
	FOnlineSubsystemSteam(FName InInstanceName) :
		FOnlineSubsystemImpl(InInstanceName),
		bSteamworksClientInitialized(false),
		bSteamworksGameServerInitialized(false),
		SteamAppID(0),
		GameServerSteamPort(0),
		GameServerGamePort(0),
		GameServerQueryPort(0),
		SessionInterface(NULL),
		IdentityInterface(NULL),
		FriendInterface(NULL),
		SharedCloudInterface(NULL),
		UserCloudInterface(NULL),
		LeaderboardsInterface(NULL),
		VoiceInterface(NULL),
		ExternalUIInterface(NULL),
		OnlineAsyncTaskThreadRunnable(NULL),
		OnlineAsyncTaskThread(NULL)
	{}

	FOnlineSubsystemSteam() : 
		bSteamworksClientInitialized(false),
		bSteamworksGameServerInitialized(false),
		SteamAppID(0),
		GameServerSteamPort(0),
		GameServerGamePort(0),
		GameServerQueryPort(0),
		SessionInterface(NULL),
		IdentityInterface(NULL),
		FriendInterface(NULL),
		SharedCloudInterface(NULL),
		UserCloudInterface(NULL),
		LeaderboardsInterface(NULL),
		VoiceInterface(NULL),
		ExternalUIInterface(NULL),
		OnlineAsyncTaskThreadRunnable(NULL),
		OnlineAsyncTaskThread(NULL)
	{}

	/** Critical sections for thread safe operation of the cloud files */
	FCriticalSection UserCloudDataLock;

	/**
	 * Is Steam available for use
	 * @return true if Steam is available, false otherwise
	 */
	bool IsEnabled();

	/**
	 *	Initialize the client side APIs for Steam 
	 * @return true if the API was initialized successfully, false otherwise
	 */
	bool InitSteamworksClient(bool bRelaunchInSteam, int32 SteamAppId);

	/**
	 *	Initialize the server side APIs for Steam 
	 * @return true if the API was initialized successfully, false otherwise
	 */
	bool InitSteamworksServer();

	/**
	 *	Shutdown the Steam APIs
	 */
	void ShutdownSteamworks();
	
	/**
	 *	Add an async task onto the task queue for processing
	 * @param AsyncTask - new heap allocated task to process on the async task thread
	 */
	void QueueAsyncTask(class FOnlineAsyncTask* AsyncTask);

	/**
	 *	Add an async task onto the outgoing task queue for processing
	 * @param AsyncItem - new heap allocated task to process on the async task thread
	 */
	void QueueAsyncOutgoingItem(class FOnlineAsyncItem* AsyncItem);

	/**
	 *	Add an async msg onto the msg queue for processing
	 * @param AsyncMsg - new heap allocated msg to process on the async task thread
	 */
	void QueueAsyncMsg(class FOnlineAsyncMsgSteam* AsyncMsg);

    /** 
     * **INTERNAL**
     * Get the metadata related to a given user
     * This information is only available after calling EnumerateUserFiles
     *
     * @param UserId the UserId to search for
     * @return the struct with the metadata about the requested user, will always return a valid struct, creating one if necessary
     *
     */
	struct FSteamUserCloudData* GetUserCloudEntry(const FUniqueNetId& UserId);

	/** 
     * **INTERNAL**
     * Clear the metadata related to a given user's file on Steam
     * This information is only available after calling EnumerateUserFiles
     * It doesn't actually delete any of the actual data on disk
     *
     * @param UserId the UserId for the file to search for
     * @param Filename the file to get metadata about
     * @return the true if the delete was successful, false otherwise
     *
     */
	bool ClearUserCloudMetadata(const FUniqueNetId& UserId, const FString& Filename);

	/**
	 *	Clear out all the data related to user cloud storage
	 */
	void ClearUserCloudFiles();

	/** 
	 * **INTERNAL** 
	 * Get the interface for accessing leaderboards/stats
	 *
	 * @return pointer for the appropriate class
	 */
	FOnlineLeaderboardsSteam * GetInternalLeaderboardsInterface();

public:

	virtual ~FOnlineSubsystemSteam()
	{
	}

	// IOnlineSubsystem

	virtual IOnlineSessionPtr GetSessionInterface() const OVERRIDE;
	virtual IOnlineFriendsPtr GetFriendsInterface() const OVERRIDE;
	virtual IOnlineSharedCloudPtr GetSharedCloudInterface() const OVERRIDE;
	virtual IOnlineUserCloudPtr GetUserCloudInterface() const OVERRIDE;
	virtual IOnlineLeaderboardsPtr GetLeaderboardsInterface() const OVERRIDE;
	virtual IOnlineVoicePtr GetVoiceInterface() const OVERRIDE;
	virtual IOnlineExternalUIPtr GetExternalUIInterface() const OVERRIDE;
	virtual IOnlineTimePtr GetTimeInterface() const OVERRIDE;
	virtual IOnlineIdentityPtr GetIdentityInterface() const OVERRIDE;
	virtual IOnlineTitleFilePtr GetTitleFileInterface() const OVERRIDE;
	virtual IOnlineEntitlementsPtr GetEntitlementsInterface() const OVERRIDE;
	virtual IOnlineStorePtr GetStoreInterface() const OVERRIDE;
	virtual IOnlineEventsPtr GetEventsInterface() const OVERRIDE;
	virtual IOnlineAchievementsPtr GetAchievementsInterface() const OVERRIDE;
	virtual IOnlineSharingPtr GetSharingInterface() const OVERRIDE;
	virtual IOnlineUserPtr GetUserInterface() const OVERRIDE;
	virtual IOnlineMessagePtr GetMessageInterface() const OVERRIDE;
	virtual IOnlinePresencePtr GetPresenceInterface() const OVERRIDE;
	virtual bool IsLocalPlayer(const FUniqueNetId& UniqueId) const OVERRIDE;
	virtual bool Init() OVERRIDE;
	virtual bool Shutdown() OVERRIDE;
	virtual bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) OVERRIDE;
	FString GetAppId() const OVERRIDE;

	// FTickerObjectBase

	virtual bool Tick(float DeltaTime) OVERRIDE;

	// FOnlineSubsystemSteam

	/**
	 * Whether or not the Steam Client interfaces are available; these interfaces are only available, if the Steam Client program is running
	 * NOTE: These interfaces are made unavailable, when running a dedicated server
	 *
	 * @return	Whether or not the Steam Client interfaces are available
	 */
	inline bool IsSteamClientAvailable()
	{
		return bSteamworksClientInitialized;
	}

	/**
	 * Whether or not the Steam game server interfaces are available; these interfaces are always available, so long as they were initialized correctly
	 * NOTE: The Steam Client does not need to be running for the game server interfaces to initialize
	 * NOTE: These interfaces are made unavailable, when not running a server
	 *
	 * @return	Whether or not the Steam game server interfaces are available
	 */
	inline bool IsSteamServerAvailable()
	{
		// @todo Steam - add some logic to detect somehow we intended to be a "Steam client" but failed that part
		// yet managed to still initialize the game server aspects of Steam
		return bSteamworksGameServerInitialized;
	}

	/**
	 * @return the steam app id for this app
	 */
	inline uint32 GetSteamAppId() const
	{
		return SteamAppID;
	}

	/**
	 *	@return the port the game has registered for play 
	 */
	inline int32 GetGameServerGamePort() const
	{
		return GameServerGamePort;
	}

	/**
	 *	@return the port the game has registered for talking to Steam
	 */
	inline int32 GetGameServerSteamPort() const
	{
		return GameServerSteamPort;
	}

	/**
	 *	@return the port the game has registered for incoming server queries
	 */
	inline int32 GetGameServerQueryPort() const
	{
		return GameServerQueryPort;
	}
};

typedef TSharedPtr<FOnlineSubsystemSteam, ESPMode::ThreadSafe> FOnlineSubsystemSteamPtr;

