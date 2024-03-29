// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	NewsFeedPrivatePCH.h: Pre-compiled header file for the NewsFeed module.
=============================================================================*/

#pragma once

#include "../Public/NewsFeed.h"


/* Dependencies
 *****************************************************************************/

#include "AnalyticsEventAttribute.h"
#include "Base64.h"
#include "CoreUObject.h"
#include "DesktopPlatformModule.h"
#include "EngineAnalytics.h"
#include "HttpModule.h"
#include "IAnalyticsProvider.h"
#include "IHttpBase.h"
#include "IHttpRequest.h"
#include "IHttpResponse.h"
#include "ImageWrapper.h"
#include "Json.h"
#include "ModuleManager.h"
#include "Online.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineTitleFileInterface.h"
#include "SecureHash.h"
#include "Settings.h"
#include "Ticker.h"


/* Private includes
 *****************************************************************************/

#include "CdnNewsFeedTitleFile.h"
#include "LocalNewsFeedTitleFile.h"
#include "NewsFeedItem.h"
#include "NewsFeedSettings.h"
#include "NewsFeedCache.h"

#include "SNewsFeedListRow.h"
#include "SNewsFeed.h"
