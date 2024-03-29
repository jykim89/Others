// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSessionSettings.h"
#include "OnlineDelegateMacros.h"

const FName GameSessionName(TEXT("Game"));
const FName PartySessionName(TEXT("Party"));

const FName GamePort(TEXT("GamePort"));
const FName BeaconPort(TEXT("BeaconPort"));

/**
 * Delegate fired when a session create request has completed
 *
 * @param SessionName the name of the session this callback is for
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnCreateSessionComplete, FName, bool);
typedef FOnCreateSessionComplete::FDelegate FOnCreateSessionCompleteDelegate;

/**
 * Delegate fired when the online session has transitioned to the started state
 *
 * @param SessionName the name of the session the that has transitioned to started
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnStartSessionComplete, FName, bool);
typedef FOnStartSessionComplete::FDelegate FOnStartSessionCompleteDelegate;

/**
 * Delegate fired when a update session request has completed
 *
 * @param SessionName the name of the session this callback is for
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnUpdateSessionComplete, FName, bool);
typedef FOnUpdateSessionComplete::FDelegate FOnUpdateSessionCompleteDelegate;

/**
 * Delegate fired when the online session has transitioned to the ending state
 *
 * @param SessionName the name of the session the that was ended
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnEndSessionComplete, FName, bool);
typedef FOnEndSessionComplete::FDelegate FOnEndSessionCompleteDelegate;

/**
 * Delegate fired when a destroying an online session has completed
 *
 * @param SessionName the name of the session this callback is for
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDestroySessionComplete, FName, bool);
typedef FOnDestroySessionComplete::FDelegate FOnDestroySessionCompleteDelegate;

/**
 * Delegate fired when the search for an online session has completed
 *
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnFindSessionsComplete, bool);
typedef FOnFindSessionsComplete::FDelegate FOnFindSessionsCompleteDelegate;

/**
 * Delegate fired when the cancellation of a search for an online session has completed
 *
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCancelFindSessionsComplete, bool);
typedef FOnCancelFindSessionsComplete::FDelegate FOnCancelFindSessionsCompleteDelegate;

/**
 * Delegate fired when the joining process for an online session has completed
 *
 * @param SessionName the name of the session this callback is for
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnJoinSessionComplete, FName, bool);
typedef FOnJoinSessionComplete::FDelegate FOnJoinSessionCompleteDelegate;

/**
 * Delegate fired once the find friend task has completed
 * Session has not been joined at this point, and requires a call to JoinSession() 
 *
 * @param LocalUserNum the controller number of the accepting user
 * @param bWasSuccessful the session was found and is joinable, false otherwise
 * @param FriendSearchResult the search/settings for the session we're attempting to join
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnFindFriendSessionComplete, int32, bool, const FOnlineSessionSearchResult&);
typedef FOnFindFriendSessionComplete::FDelegate FOnFindFriendSessionCompleteDelegate;

/**
 * Delegate fired when an individual server's query has completed
 *
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPingSearchResultsComplete, bool);
typedef FOnPingSearchResultsComplete::FDelegate FOnPingSearchResultsCompleteDelegate;

/**
 * Called when a user accepts a session invitation. Allows the game code a chance
 * to clean up any existing state before accepting the invite. The invite must be
 * accepted by calling JoinSession() after clean up has completed
 *
 * @param LocalUserNum the controller number of the accepting user
 * @param bWasSuccessful the session was found and is joinable, false otherwise
 * @param InviteResult the search/settings for the session we're joining via invite
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnSessionInviteAccepted, int32, bool, const FOnlineSessionSearchResult&);
typedef FOnSessionInviteAccepted::FDelegate FOnSessionInviteAcceptedDelegate;

/**
 * Delegate fired when the session registration process has completed
 *
 * @param SessionName the name of the session the player joined or not
 * @param Players the players that were registered from the online service
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnRegisterPlayersComplete, FName, const TArray< TSharedRef<class FUniqueNetId> >&, bool);
typedef FOnRegisterPlayersComplete::FDelegate FOnRegisterPlayersCompleteDelegate;

/**
 * Delegate fired when the un-registration process has completed
 *
 * @param SessionName the name of the session the player left
 * @param PlayerId the players that were unregistered from the online service
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnUnregisterPlayersComplete, FName, const TArray< TSharedRef<class FUniqueNetId> >&, bool);
typedef FOnUnregisterPlayersComplete::FDelegate FOnUnregisterPlayersCompleteDelegate;

/**
 * Interface definition for the online services session services 
 * Session services are defined as anything related managing a session 
 * and its state within a platform service
 */
class IOnlineSession
{
protected:

	/** Hidden on purpose */
	IOnlineSession() {};

	/**
	 * Adds a new named session to the list (new session)
	 *
	 * @param SessionName the name to search for
	 * @param GameSettings the game settings to add
	 *
	 * @return a pointer to the struct that was added
	 */
	virtual class FNamedOnlineSession* AddNamedSession(FName SessionName, const FOnlineSessionSettings& SessionSettings) = 0;

	/**
	 * Adds a new named session to the list (from existing session data)
	 *
	 * @param SessionName the name to search for
	 * @param GameSettings the game settings to add
	 *
	 * @return a pointer to the struct that was added
	 */
	virtual class FNamedOnlineSession* AddNamedSession(FName SessionName, const FOnlineSession& Session) = 0;

public:
	virtual ~IOnlineSession() {};

	/**
	 * Searches the named session array for the specified session
	 *
	 * @param SessionName the name to search for
	 *
	 * @return pointer to the struct if found, NULL otherwise
	 */
	virtual class FNamedOnlineSession* GetNamedSession(FName SessionName) = 0;

	/**
	 * Searches the named session array for the specified session and removes it
	 *
	 * @param SessionName the name to search for
	 */
	virtual void RemoveNamedSession(FName SessionName) = 0;

	/**
	 * Searches the named session array for any presence enabled session
	 */
	virtual bool HasPresenceSession() = 0;

	/**
	 * Get the current state of a named session
	 *
	 * @param SessionName name of session to query
	 *
	 * @return State of specified session
	 */
	virtual EOnlineSessionState::Type GetSessionState(FName SessionName) const = 0;

	/**
	 * Creates an online session based upon the settings object specified.
	 * NOTE: online session registration is an async process and does not complete
	 * until the OnCreateSessionComplete delegate is called.
	 *
	 * @param HostingPlayerNum the index of the player hosting the session
	 * @param SessionName the name to use for this session so that multiple sessions can exist at the same time
	 * @param NewSessionSettings the settings to use for the new session
	 *
	 * @return true if successful creating the session, false otherwise
	 */
	virtual bool CreateSession(int32 HostingPlayerNum, FName SessionName, const FOnlineSessionSettings& NewSessionSettings) = 0;

	/**
	* Delegate fired when a session create request has completed
	*
	* @param SessionName the name of the session this callback is for
	* @param bWasSuccessful true if the async action completed without error, false if there was an error
	*/
	DEFINE_ONLINE_DELEGATE_TWO_PARAM(OnCreateSessionComplete, FName, bool);

	/**
	 * Marks an online session as in progress (as opposed to being in lobby or pending)
	 *
	 * @param SessionName the name of the session that is being started
	 *
	 * @return true if the call succeeds, false otherwise
	 */
	virtual bool StartSession(FName SessionName) = 0;

	/**
	 * Delegate fired when the online session has transitioned to the started state
	 *
	 * @param SessionName the name of the session the that has transitioned to started
	 * @param bWasSuccessful true if the async action completed without error, false if there was an error
	 */
	DEFINE_ONLINE_DELEGATE_TWO_PARAM(OnStartSessionComplete, FName, bool);

	/**
	 * Updates the localized settings/properties for the session in question
	 *
	 * @param SessionName the name of the session to update
	 * @param UpdatedSessionSettings the object to update the session settings with
	 * @param bShouldRefreshOnlineData whether to submit the data to the backend or not
	 *
	 * @return true if successful creating the session, false otherwise
	 */
	virtual bool UpdateSession(FName SessionName, FOnlineSessionSettings& UpdatedSessionSettings, bool bShouldRefreshOnlineData = true) = 0;

	/**
	 * Delegate fired when a update request has completed
	 *
	 * @param SessionName the name of the session this callback is for
	 * @param bWasSuccessful true if the async action completed without error, false if there was an error
	 */
	DEFINE_ONLINE_DELEGATE_TWO_PARAM(OnUpdateSessionComplete, FName, bool);

	/**
	 * Marks an online session as having been ended
	 *
	 * @param SessionName the name of the session the to end
	 *
	 * @return true if the call succeeds, false otherwise
	 */
	virtual bool EndSession(FName SessionName) = 0;

	/**
	 * Delegate fired when the online session has transitioned to the ending state
	 *
	 * @param SessionName the name of the session the that was ended
	 * @param bWasSuccessful true if the async action completed without error, false if there was an error
	 */
	DEFINE_ONLINE_DELEGATE_TWO_PARAM(OnEndSessionComplete, FName, bool);

	/**
	 * Destroys the specified online session
	 * NOTE: online session de-registration is an async process and does not complete
	 * until the OnDestroySessionComplete delegate is called.
	 *
	 * @param SessionName the name of the session to delete
	 *
	 * @return true if successful destroying the session, false otherwise
	 */
	virtual bool DestroySession(FName SessionName) = 0;

	/**
	 * Delegate fired when a destroying an online session has completed
	 *
	 * @param SessionName the name of the session this callback is for
	 * @param bWasSuccessful true if the async action completed without error, false if there was an error
	 */
	DEFINE_ONLINE_DELEGATE_TWO_PARAM(OnDestroySessionComplete, FName, bool);

	/**
	 * Determine if the player is registered in the specified session
	 *
	 * @param UniqueId the player to check if in session or not
	 * @return true if the player is registered in the session
	 */
	virtual bool IsPlayerInSession(FName SessionName, const FUniqueNetId& UniqueId) = 0;

	/**
	 * Searches for sessions matching the settings specified
	 *
	 * @param SearchingPlayerNum the index of the player searching for a match
	 * @param SearchSettings the desired settings that the returned sessions will have
	 *
	 * @return true if successful searching for sessions, false otherwise
	 */
	virtual bool FindSessions(int32 SearchingPlayerNum, const TSharedRef<FOnlineSessionSearch>& SearchSettings) = 0;

	/**
	 * Delegate fired when the search for an online session has completed
	 *
	 * @param bWasSuccessful true if the async action completed without error, false if there was an error
	 */
	DEFINE_ONLINE_DELEGATE_ONE_PARAM(OnFindSessionsComplete, bool);

	/**
	 * Cancels the current search in progress if possible for that search type
	 *
	 * @return true if successful searching for sessions, false otherwise
	 */
	virtual bool CancelFindSessions() = 0;

	/**
	 * Delegate fired when the cancellation of a search for an online session has completed
	 *
	 * @param bWasSuccessful true if the async action completed without error, false if there was an error
	 */
	DEFINE_ONLINE_DELEGATE_ONE_PARAM(OnCancelFindSessionsComplete, bool);

	/**
	 * Fetches the additional data a session exposes outside of the online service.
	 * NOTE: notifications will come from the OnPingSearchResultsComplete delegate
	 *
	 * @param SearchResult the specific search result to query
	 *
	 * @return true if the query was started, false otherwise
	 */
	virtual bool PingSearchResults(const FOnlineSessionSearchResult& SearchResult) = 0;

	/**
	 * Delegate fired when an individual server's query has completed
	 *
	 * @param bWasSuccessful true if the async action completed without error, false if there was an error
	 */
	DEFINE_ONLINE_DELEGATE_ONE_PARAM(OnPingSearchResultsComplete, bool);

	/**
	 * Joins the session specified
	 *
	 * @param LocalUserNum the index of the player searching for a match
	 * @param SessionName the name of the session to join
	 * @param DesiredSession the desired session to join
	 *
	 * @return true if the call completed successfully, false otherwise
	 */
	virtual bool JoinSession(int32 LocalUserNum, FName SessionName, const FOnlineSessionSearchResult& DesiredSession) = 0;

	/**
	 * Delegate fired when the joining process for an online session has completed
	 *
	 * @param SessionName the name of the session this callback is for
	 * @param bWasSuccessful true if the async action completed without error, false if there was an error
	 */
	DEFINE_ONLINE_DELEGATE_TWO_PARAM(OnJoinSessionComplete, FName, bool);

	/**
	 * Allows the local player to follow a friend into a session
	 *
	 * @param LocalUserNum the local player wanting to join
	 * @param Friend the player that is being followed
	 *
	 * @return true if the async call worked, false otherwise
	 */
	virtual bool FindFriendSession(int32 LocalUserNum, const FUniqueNetId& Friend) = 0;

	/**
	 * Delegate fired once the find friend task has completed
	 * Session has not been joined at this point, and still requires a call to JoinSession()
	 *
	 * @param LocalUserNum the controller number of the accepting user
	 * @param bWasSuccessful the session was found and is joinable, false otherwise
	 * @param FriendSearchResult the search/settings for the session we're attempting to join
	 */
	DEFINE_ONLINE_PLAYER_DELEGATE_TWO_PARAM(MAX_LOCAL_PLAYERS, OnFindFriendSessionComplete, bool, const FOnlineSessionSearchResult&);

	/**
	 * Sends an invitation to play in the player's current session
	 *
	 * @param LocalUserNum the user that is sending the invite
	 * @param SessionName session to invite them to
	 * @param Friend the player to send the invite to
	 *
	 * @return true if successful, false otherwise
	 */
	virtual bool SendSessionInviteToFriend(int32 LocalUserNum, FName SessionName, const FUniqueNetId& Friend) = 0;

	/**
	 * Sends invitations to play in the player's current session
	 *
	 * @param LocalUserNum the user that is sending the invite
	 * @param SessionName session to invite them to
	 * @param Friends the player to send the invite to
	 *
	 * @return true if successful, false otherwise
	 */
	virtual bool SendSessionInviteToFriends(int32 LocalUserNum, FName SessionName, const TArray< TSharedRef<FUniqueNetId> >& Friends) = 0;

	/**
	 * Called when a user accepts a session invitation. Allows the game code a chance
	 * to clean up any existing state before accepting the invite. The invite must be
	 * accepted by calling JoinSession() after clean up has completed
	 *
	 * @param LocalUserNum the controller number of the accepting user
	 * @param bWasSuccessful true if the async action completed without error, false if there was an error
	 * @param InviteResult the search/settings for the session we're joining via invite
	 */
	DEFINE_ONLINE_PLAYER_DELEGATE_TWO_PARAM(MAX_LOCAL_PLAYERS, OnSessionInviteAccepted, bool, const FOnlineSessionSearchResult&);

	/**
	 * Returns the platform specific connection information for joining the match.
	 * Call this function from the delegate of join completion
	 *
	 * @param SessionName the name of the session to resolve
	 * @param ConnectInfo the string containing the platform specific connection information
	 *
	 * @return true if the call was successful, false otherwise
	 */
	virtual bool GetResolvedConnectString(FName SessionName, FString& ConnectInfo) = 0;

	/**
	 * Returns the platform specific connection information for joining a search result.
	 *
	 * @param SearchResult the search result to get connection info from
	 * @param PortType type of port to append to result (Game, Beacon, etc)
	 * @param ConnectInfo the string containing the platform specific connection information
	 *
	 * @return true if the call was successful, false otherwise
	 */
	virtual bool GetResolvedConnectString(const class FOnlineSessionSearchResult& SearchResult, FName PortType, FString& ConnectInfo) = 0;

	/**
	 * Returns the session settings object for the session with a matching name
	 *
	 * @param SessionName the name of the session to return
	 *
	 * @return the settings for this session name
	 */
	virtual FOnlineSessionSettings* GetSessionSettings(FName SessionName) = 0;

	/**
	 * Registers a player with the online service as being part of the online session
	 *
	 * @param SessionName the name of the session the player is joining
	 * @param UniquePlayerId the player to register with the online service
	 * @param bWasInvited whether the player was invited to the session or searched for it
	 *
	 * @return true if the call succeeds, false otherwise
	 */
	virtual bool RegisterPlayer(FName SessionName, const FUniqueNetId& PlayerId, bool bWasInvited) = 0;

	/**
	 * Registers a group of players with the online service as being part of the online session
	 *
	 * @param SessionName the name of the session the player is joining
	 * @param Players the list of players to register with the online service
	 * @param bWasInvited was this list of players invited
	 *
	 * @return true if the call succeeds, false otherwise
	 */
	virtual bool RegisterPlayers(FName SessionName, const TArray< TSharedRef<class FUniqueNetId> >& Players, bool bWasInvited = false) = 0;

	/**
	 * Delegate fired when the session registration process has completed
	 *
	 * @param SessionName the name of the session the player joined or not
	 * @param PlayerId the player that was registered in the online service
	 * @param bWasSuccessful true if the async action completed without error, false if there was an error
	 */
	DEFINE_ONLINE_DELEGATE_THREE_PARAM(OnRegisterPlayersComplete, FName, const TArray< TSharedRef<class FUniqueNetId> >&, bool);

	/**
	 * Unregisters a player with the online service as being part of the online session
	 *
	 * @param SessionName the name of the session the player is leaving
	 * @param PlayerId the player to unregister with the online service
	 *
	 * @return true if the call succeeds, false otherwise
	 */
	virtual bool UnregisterPlayer(FName SessionName, const FUniqueNetId& PlayerId) = 0;

	/**
	 * Unregisters a group of players with the online service as being part of the online session
	 *
	 * @param SessionName the name of the session the player is joining
	 * @param Players the list of players to unregister with the online service
	 *
	 * @return true if the call succeeds, false otherwise
	 */
	virtual bool UnregisterPlayers(FName SessionName, const TArray< TSharedRef<class FUniqueNetId> >& Players) = 0;

	/**
	 * Delegate fired when the un-registration process has completed
	 *
	 * @param SessionName the name of the session the player left
	 * @param PlayerId the player that was unregistered from the online service
	 * @param bWasSuccessful true if the async action completed without error, false if there was an error
	 */
	DEFINE_ONLINE_DELEGATE_THREE_PARAM(OnUnregisterPlayersComplete, FName, const TArray< TSharedRef<class FUniqueNetId> >&, bool);

	/**
	 * Gets the number of known sessions registered with the interface
	 */
	virtual int32 GetNumSessions() = 0;

	/**
	 *	Dumps out the session state for all known sessions
	 */
	virtual void DumpSessionState() = 0;
};

typedef TSharedPtr<IOnlineSession, ESPMode::ThreadSafe> IOnlineSessionPtr;

