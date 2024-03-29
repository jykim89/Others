// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSessionInterface.h"
#include "OnlineSubsystemNullTypes.h"
#include "OnlineSubsystemNullPackage.h"

/**
 * Interface definition for the online services session services 
 * Session services are defined as anything related managing a session 
 * and its state within a platform service
 */
class FOnlineSessionNull : public IOnlineSession
{
private:

	/** Reference to the main Null subsystem */
	class FOnlineSubsystemNull* NullSubsystem;

	/** Handles advertising sessions over LAN and client searches */
	class FLANSession* LANSessionManager;

	/** Hidden on purpose */
	FOnlineSessionNull() :
		NullSubsystem(NULL),
		LANSessionManager(NULL),
		CurrentSessionSearch(NULL)
	{}

	/**
	 * Ticks any lan beacon background tasks
	 *
	 * @param DeltaTime the time since the last tick
	 */
	void TickLanTasks(float DeltaTime);

	/**
	 * Checks whether there are any sessions that need to be advertised (over LAN)
	 *
	 * @return true if there is at least one
	 */
	bool NeedsToAdvertise();

	/**
	 * Determines whether this particular session should be advertised (over LAN)
	 *
	 * @return true if yes
	 */
	bool NeedsToAdvertise( FNamedOnlineSession& Session );

	/**
	 * Updates the status of LAN session (creates it if needed, shuts down if not)
	 * 
	 * @return ERROR_SUCCESS if everything went successful, an error code otherwise
	 */
	uint32 UpdateLANStatus();

	/**
	 *	Join a LAN session
	 * 
	 * @param PlayerNum local index of the user initiating the request
	 * @param Session newly allocated session with join information
	 * @param SearchSession the desired session to join
	 * 
	 * @return ERROR_SUCCESS if successful, an error code otherwise
	 */
	uint32 JoinLANSession(int32 PlayerNum, class FNamedOnlineSession* Session, const class FOnlineSession* SearchSession);

	/**
	 * Builds a LAN search query and broadcasts it
	 *
	 * @return ERROR_SUCCESS if successful, an error code otherwise
	 */
	uint32 FindLANSession();

	/**
	 * Finishes searching over LAN and returns to hosting (if needed)
	 *
	 * @return ERROR_SUCCESS if successful, an error code otherwise
	 */
	uint32 FinalizeLANSearch();

	/**
	 * Adds the game session data to the packet that is sent by the host
	 * in response to a server query
	 *
	 * @param Packet the writer object that will encode the data
	 * @param Session the session to add to the packet
	 */
	void AppendSessionToPacket(class FNboSerializeToBufferNull& Packet, class FOnlineSession* Session);

	/**
	 * Adds the game settings data to the packet that is sent by the host
	 * in response to a server query
	 *
	 * @param Packet the writer object that will encode the data
	 * @param SessionSettings the session settings to add to the packet
	 */
	void AppendSessionSettingsToPacket(class FNboSerializeToBufferNull& Packet, FOnlineSessionSettings* SessionSettings);

	/**
	 * Reads the settings data from the packet and applies it to the
	 * specified object
	 *
	 * @param Packet the reader object that will read the data
	 * @param SessionSettings the session settings to copy the data to
	 */
	void ReadSessionFromPacket(class FNboSerializeFromBufferNull& Packet, class FOnlineSession* Session);

	/**
	 * Reads the settings data from the packet and applies it to the
	 * specified object
	 *
	 * @param Packet the reader object that will read the data
	 * @param SessionSettings the session settings to copy the data to
	 */
	void ReadSettingsFromPacket(class FNboSerializeFromBufferNull& Packet, FOnlineSessionSettings& SessionSettings);

	/*
	 * Delegate triggered when the LAN beacon has detected a valid client request has been received
	 *
	 * @param PacketData packet data sent by the requesting client with header information removed
	 * @param PacketLength length of the packet not including header size
	 * @param ClientNonce the nonce returned by the client to return with the server packet
	 */
	void OnValidQueryPacketReceived(uint8* PacketData, int32 PacketLength, uint64 ClientNonce);

	/*
	 * Delegate triggered when the LAN beacon has detected a valid host response to a client request has been received
	 *
	 * @param PacketData packet data sent by the requesting client with header information removed
	 * @param PacketLength length of the packet not including header size
	 */
	void OnValidResponsePacketReceived(uint8* PacketData, int32 PacketLength);

	/*
	 * Delegate triggered when the LAN beacon has finished searching (some time after last received host packet)
	 */
	void OnLANSearchTimeout();

PACKAGE_SCOPE:

	/** Critical sections for thread safe operation of session lists */
	mutable FCriticalSection SessionLock;

	/** Current session settings */
	TArray<FNamedOnlineSession> Sessions;

	/** Current search object */
	TSharedPtr<FOnlineSessionSearch> CurrentSessionSearch;

	/** Current search start time. */
	double SessionSearchStartInSeconds;

	FOnlineSessionNull(class FOnlineSubsystemNull* InSubsystem) :
		NullSubsystem(InSubsystem),
		LANSessionManager(NULL),
		CurrentSessionSearch(NULL),
		SessionSearchStartInSeconds(0)
	{}

	/**
	 * Session tick for various background tasks
	 */
	void Tick(float DeltaTime);

	// IOnlineSession
	class FNamedOnlineSession* AddNamedSession(FName SessionName, const FOnlineSessionSettings& SessionSettings) OVERRIDE
	{
		FScopeLock ScopeLock(&SessionLock);
		return new (Sessions) FNamedOnlineSession(SessionName, SessionSettings);
	}

	class FNamedOnlineSession* AddNamedSession(FName SessionName, const FOnlineSession& Session) OVERRIDE
	{
		FScopeLock ScopeLock(&SessionLock);
		return new (Sessions) FNamedOnlineSession(SessionName, Session);
	}

	/**
	 * Parse the command line for invite/join information at launch
	 */
	void CheckPendingSessionInvite();

	/**
	* Registers and updates voice data for the given player id
	*
	* @param PlayerId player to register with the voice subsystem
	*/
	void RegisterVoice(const FUniqueNetId& PlayerId);

	/**
	* Unregisters a given player id from the voice subsystem
	*
	* @param PlayerId player to unregister with the voice subsystem
	*/
	void UnregisterVoice(const FUniqueNetId& PlayerId);

	/**
	 * Registers all local players with the current session
	 *
	 * @param Session the session that they are registering in
	 */
	void RegisterLocalPlayers(class FNamedOnlineSession* Session);

public:

	virtual ~FOnlineSessionNull() {}

	FNamedOnlineSession* GetNamedSession(FName SessionName) OVERRIDE
	{
		FScopeLock ScopeLock(&SessionLock);
		for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
		{
			if (Sessions[SearchIndex].SessionName == SessionName)
			{
				return &Sessions[SearchIndex];
			}
		}
		return NULL;
	}

	virtual void RemoveNamedSession(FName SessionName) OVERRIDE
	{
		FScopeLock ScopeLock(&SessionLock);
		for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
		{
			if (Sessions[SearchIndex].SessionName == SessionName)
			{
				Sessions.RemoveAtSwap(SearchIndex);
				return;
			}
		}
	}

	virtual EOnlineSessionState::Type GetSessionState(FName SessionName) const OVERRIDE
	{
		FScopeLock ScopeLock(&SessionLock);
		for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
		{
			if (Sessions[SearchIndex].SessionName == SessionName)
			{
				return Sessions[SearchIndex].SessionState;
			}
		}

		return EOnlineSessionState::NoSession;
	}

	virtual bool HasPresenceSession() OVERRIDE
	{
		FScopeLock ScopeLock(&SessionLock);
		for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
		{
			if (Sessions[SearchIndex].SessionSettings.bUsesPresence)
			{
				return true;
			}
		}
		
		return false;
	}

	// IOnlineSession
	virtual bool CreateSession(int32 HostingPlayerNum, FName SessionName, const FOnlineSessionSettings& NewSessionSettings) OVERRIDE;
	virtual bool StartSession(FName SessionName) OVERRIDE;
	virtual bool UpdateSession(FName SessionName, FOnlineSessionSettings& UpdatedSessionSettings, bool bShouldRefreshOnlineData = true) OVERRIDE;
	virtual bool EndSession(FName SessionName) OVERRIDE;
	virtual bool DestroySession(FName SessionName) OVERRIDE;
	virtual bool IsPlayerInSession(FName SessionName, const FUniqueNetId& UniqueId) OVERRIDE;
	virtual bool FindSessions(int32 SearchingPlayerNum, const TSharedRef<FOnlineSessionSearch>& SearchSettings) OVERRIDE;
	virtual bool CancelFindSessions() OVERRIDE;
	virtual bool PingSearchResults(const FOnlineSessionSearchResult& SearchResult) OVERRIDE;
	virtual bool JoinSession(int32 PlayerNum, FName SessionName, const FOnlineSessionSearchResult& DesiredSession) OVERRIDE;
	virtual bool FindFriendSession(int32 LocalUserNum, const FUniqueNetId& Friend) OVERRIDE;
	virtual bool SendSessionInviteToFriend(int32 LocalUserNum, FName SessionName, const FUniqueNetId& Friend) OVERRIDE;
	virtual bool SendSessionInviteToFriends(int32 LocalUserNum, FName SessionName, const TArray< TSharedRef<FUniqueNetId> >& Friends) OVERRIDE;
	virtual bool GetResolvedConnectString(FName SessionName, FString& ConnectInfo) OVERRIDE;
	virtual bool GetResolvedConnectString(const class FOnlineSessionSearchResult& SearchResult, FName PortType, FString& ConnectInfo) OVERRIDE;
	virtual FOnlineSessionSettings* GetSessionSettings(FName SessionName) OVERRIDE;
	virtual bool RegisterPlayer(FName SessionName, const FUniqueNetId& PlayerId, bool bWasInvited) OVERRIDE;
	virtual bool RegisterPlayers(FName SessionName, const TArray< TSharedRef<FUniqueNetId> >& Players, bool bWasInvited = false) OVERRIDE;
	virtual bool UnregisterPlayer(FName SessionName, const FUniqueNetId& PlayerId) OVERRIDE;
	virtual bool UnregisterPlayers(FName SessionName, const TArray< TSharedRef<FUniqueNetId> >& Players) OVERRIDE;
	virtual int32 GetNumSessions() OVERRIDE;
	virtual void DumpSessionState() OVERRIDE;
};

typedef TSharedPtr<FOnlineSessionNull, ESPMode::ThreadSafe> FOnlineSessionNullPtr;
