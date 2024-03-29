// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OnlineDelegateMacros.h"

#include "OnlineExternalUIInterface.h"
#include "OnlineSubsystemGooglePlayPackage.h"

/** 
 * Interface definition for the online services external UIs
 * Any online service that provides extra UI overlays will implement the relevant functions
 */
class FOnlineExternalUIGooglePlay :
	public IOnlineExternalUI
{

public:
	FOnlineExternalUIGooglePlay();

	virtual bool ShowLoginUI(const int ControllerIndex, bool bShowOnlineOnly, const FOnLoginUIClosedDelegate& Delegate) OVERRIDE;
	virtual bool ShowFriendsUI(int32 LocalUserNum) OVERRIDE;
	virtual bool ShowInviteUI(int32 LocalUserNum) OVERRIDE;
	virtual bool ShowAchievementsUI(int32 LocalUserNum) OVERRIDE;
	virtual bool ShowLeaderboardUI(const FString& LeaderboardName) OVERRIDE;
	virtual bool ShowWebURL(const FString& WebURL) OVERRIDE;
	virtual bool ShowProfileUI(const FUniqueNetId& Requestor, const FUniqueNetId& Requestee, const FOnProfileUIClosedDelegate& Delegate) OVERRIDE;
};

typedef TSharedPtr<FOnlineExternalUIGooglePlay, ESPMode::ThreadSafe> FOnlineExternalUIGooglePlayPtr;

