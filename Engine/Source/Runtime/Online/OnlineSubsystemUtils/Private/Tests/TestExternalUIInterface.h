// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "OnlineExternalUIInterface.h"

/** Enumeration of external UI tests */
namespace ETestExternalUIInterfaceState
{
	enum Type
	{
		Begin,
		ShowLoginUI,
		ShowFriendsUI,
		ShowInviteUI,
		ShowAchievementsUI,
		ShowWebURL,
		ShowProfileUI,
		End,
	};
}

/**
 * Class used to test the external UI interface
 */
 class FTestExternalUIInterface
 {

	/** The subsystem that was requested to be tested or the default if empty */
	const FString SubsystemName;

	/** Booleans that control which external UIs to test */
	bool bTestLoginUI;
	bool bTestFriendsUI;
	bool bTestInviteUI;
	bool bTestAchievementsUI;
	bool bTestWebURL;
	bool bTestProfileUI;

	/** The online interface to use for testing */
	IOnlineSubsystem* OnlineSub;

	/** Convenient access to the external UI interfaces */
	IOnlineExternalUIPtr ExternalUI;

	/** Delegate for external UI opening and closing */
	FOnExternalUIChangeDelegate ExternalUIChangeDelegate;

	/** Current external UI test */
	ETestExternalUIInterfaceState::Type State;

	/** Completes testing and cleans up after itself */
	void FinishTest();

	/** Go to the next test */
	void StartNextTest();

	/** Specific test functions */
	bool TestLoginUI();
	bool TestFriendsUI();
	bool TestInviteUI();
	bool TestAchievementsUI();
	bool TestWebURL();
	bool TestProfileUI();

	/** Delegate called when external UI is opening and closing */
	void OnExternalUIChange(bool bIsOpening);

	/** Delegate executed when the user login UI has been closed. */
	void OnLoginUIClosed(TSharedPtr<FUniqueNetId> LoggedInUserId, const int LocalUserId);

	/** Delegate executed when the user profile UI has been closed. */
	void OnProfileUIClosed();

 public:

	/**
	 * Constructor
	 *
	 */
	FTestExternalUIInterface(const FString& InSubsystemName, bool bInTestLoginUI, bool bInTestFriendsUI, bool bInTestInviteUI, bool bInTestAchievementsUI, bool bInTestWebURL, bool bInTestProfileUI)
		:	SubsystemName(InSubsystemName)
		,	bTestLoginUI(bInTestLoginUI)
		,	bTestFriendsUI(bInTestFriendsUI)
		,	bTestInviteUI(bInTestInviteUI)
		,	bTestAchievementsUI(bInTestAchievementsUI)
		,	bTestWebURL(bInTestWebURL)
		,	bTestProfileUI(bInTestProfileUI)
		,	OnlineSub(NULL)
		,	ExternalUI(NULL)
		,	State(ETestExternalUIInterfaceState::Begin)
	{
	}

	/**
	 * Kicks off all of the testing process
	 *
	 */
	void Test();

 };

