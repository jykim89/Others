// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "OnlineFriendsInterface.h"

/**
 * Class used to test the friends interface
 */
 class FTestFriendsInterface
 {
	/** The subsystem that was requested to be tested or the default if empty */
	const FString SubsystemName;
	/** The online interface to use for testing */
	IOnlineSubsystem* OnlineSub;
	/** Delegate to use for reading the friends list */
	FOnReadFriendsListCompleteDelegate OnReadFriendsCompleteDelegate;
	/** Delegate when invites accepted */
	FOnAcceptInviteCompleteDelegate OnAcceptInviteCompleteDelegate;
	/** Delegate when invites have been sent */
	FOnSendInviteCompleteDelegate OnSendInviteCompleteDelegate;
	/** Delegate to use for deleting a friend entry */
	FOnDeleteFriendCompleteDelegate OnDeleteFriendCompleteDelegate;
	/** Delegate to use for deleting the friends list */
	FOnDeleteFriendsListCompleteDelegate OnDeleteFriendsListCompleteDelegate;

	/** Default name of friends list for running tests */
	FString FriendsListName;
	/** Filled in after reading friends list. Invites that are pending will be auto-accepted */
	TArray<TSharedPtr<FUniqueNetId> > InvitesToAccept;
	/** List of friends to send invites to. From test options */
	TArray<TSharedPtr<FUniqueNetId> > InvitesToSend;
	/** List of friends to delete */
	TArray<TSharedPtr<FUniqueNetId> > FriendsToDelete;
	
	/** true to enable friends list read */
	bool bReadFriendsList;
	/** true to enable auto accept of pending invites */
	bool bAcceptInvites;
	/** true to send invites */
	bool bSendInvites;
	/** true to delete individual friends from list */
	bool bDeleteFriends;
	/** true to delete the test friends list */
	bool bDeleteFriendsList;

	/** Hidden on purpose */
	FTestFriendsInterface()
		: SubsystemName()
	{
	}

	/**
	 * Step through the various tests that should be run and initiate the next one
	 */
	void StartNextTest();

	/**
	 * Finish/cleanup the tests
	 */
	void FinishTest();

	/**
	 * Delegate used when the friends read request has completed
	 *
	 * @param LocalPlayer the controller number of the associated user that made the request
	 * @param bWasSuccessful true if the async action completed without error, false if there was an error
	 * @param ListName name of the friends list that was operated on
	 * @param ErrorStr string representing the error condition
	 */
	void OnReadFriendsComplete(int32 LocalPlayer, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr);

	/**
	 * Delegate used when an invite accept request has completed
	 *
	 * @param LocalPlayer the controller number of the associated user that made the request
	 * @param bWasSuccessful true if the async action completed without error, false if there was an error
	 * @param FriendId player that invited us
	 * @param ListName name of the friends list that was operated on
	 * @param ErrorStr string representing the error condition
	 */
	void OnAcceptInviteComplete(int32 LocalPlayer, bool bWasSuccessful, const FUniqueNetId& FriendId, const FString& ListName, const FString& ErrorStr);

	/**
	 * Delegate used when an invite send request has completed
	 *
	 * @param LocalPlayer the controller number of the associated user that made the request
	 * @param bWasSuccessful true if the async action completed without error, false if there was an error
	 * @param FriendId player that was invited
	 * @param ListName name of the friends list that was operated on
	 * @param ErrorStr string representing the error condition
	 */
	void OnSendInviteComplete(int32 LocalPlayer, bool bWasSuccessful, const FUniqueNetId& FriendId, const FString& ListName, const FString& ErrorStr);

	/**
	 * Delegate used when an friend delete request has completed
	 *
	 * @param LocalUserNum the controller number of the associated user that made the request
	 * @param bWasSuccessful true if the async action completed without error, false if there was an error
	 * @param FriendId player that was deleted
	 * @param ListName name of the friends list that was operated on
	 * @param ErrorStr string representing the error condition
	 */
	void OnDeleteFriendComplete(int32 LocalPlayer, bool bWasSuccessful, const FUniqueNetId& FriendId, const FString& ListName, const FString& ErrorStr);

	/**
     * Delegate used when the friends list delete request has completed
     *
	 * @param LocalPlayer the controller number of the associated user that made the request
     * @param bWasSuccessful true if the async action completed without error, false if there was an error
	 * @param ListName name of the friends list that was operated on
	 * @param ErrorStr string representing the error condition
     */
	void OnDeleteFriendsListComplete(int32 LocalPlayer, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr);

 public:
	/**
	 * Sets the subsystem name to test
	 *
	 * @param InSubsystemName the subsystem to test
	 */
	FTestFriendsInterface(const FString& InSubsystemName) 
		: SubsystemName(InSubsystemName)
		, OnlineSub(NULL)
		, FriendsListName(EFriendsLists::ToString(EFriendsLists::Default))
		, bReadFriendsList(true)
		, bAcceptInvites(true)
		, bSendInvites(true)
		, bDeleteFriends(true)
		, bDeleteFriendsList(false)
	{
	}

	/**
	 * Kicks off all of the testing process
	 *
	 * @param Invites list of friend ids to send invites for
	 */
	void Test(class UWorld* InWorld, const TArray<FString>& Invites);
 };
