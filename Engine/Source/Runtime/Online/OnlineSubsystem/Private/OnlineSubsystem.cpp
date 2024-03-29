// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemPrivatePCH.h"
#include "OnlineSessionInterface.h"
#include "OnlineIdentityInterface.h"
#include "NboSerializer.h"

DEFINE_LOG_CATEGORY(LogOnline);
DEFINE_LOG_CATEGORY(LogOnlineGame);

#if STATS
ONLINESUBSYSTEM_API DEFINE_STAT(STAT_Online_Async);
ONLINESUBSYSTEM_API DEFINE_STAT(STAT_Online_AsyncTasks);
ONLINESUBSYSTEM_API DEFINE_STAT(STAT_Session_Interface);
ONLINESUBSYSTEM_API DEFINE_STAT(STAT_Voice_Interface);
#endif

uint32 GetBuildUniqueId()
{
	static bool bStaticCheck = false;
	static bool bUseBuildIdOverride = false;
	static int32 BuildIdOverride = 0;
	if (!bStaticCheck)
	{
#if !UE_BUILD_SHIPPING
		if (FParse::Value(FCommandLine::Get(), TEXT("BuildIdOverride="), BuildIdOverride) && BuildIdOverride != 0)
		{
			bUseBuildIdOverride = true;
		}
		else
#endif // !UE_BUILD_SHIPPING
		{
			if (!GConfig->GetBool(TEXT("OnlineSubsystem"), TEXT("bUseBuildIdOverride"), bUseBuildIdOverride, GEngineIni))
			{
				UE_LOG_ONLINE(Warning, TEXT("Missing bUseBuildIdOverride= in [OnlineSubsystem] of DefaultEngine.ini"));
			}

			if (!GConfig->GetInt(TEXT("OnlineSubsystem"), TEXT("BuildIdOverride"), BuildIdOverride, GEngineIni))
			{
				UE_LOG_ONLINE(Warning, TEXT("Missing BuildIdOverride= in [OnlineSubsystem] of DefaultEngine.ini"));
			}
		}

		bStaticCheck = true;
	}

	uint32 Crc = 0;
	if (bUseBuildIdOverride == false)
	{
		/** Engine package CRC doesn't change, can't be used as the version - BZ */
		FNboSerializeToBuffer Buffer(64);
		// Serialize to a NBO buffer for consistent CRCs across platforms
		Buffer << GEngineNetVersion;
		// Now calculate the CRC
		Crc = FCrc::MemCrc_DEPRECATED((uint8*)Buffer, Buffer.GetByteCount());
	}
	else
	{
		Crc = (uint32)BuildIdOverride;
	}

	UE_LOG_ONLINE(Verbose, TEXT("GetBuildUniqueId: GEngineNetVersion %d bUseBuildIdOverride %d BuildIdOverride %d Crc %d"),
		GEngineNetVersion,
		bUseBuildIdOverride,
		BuildIdOverride,
		Crc);

	return Crc;
}

bool IsPlayerInSessionImpl(IOnlineSession* SessionInt, FName SessionName, const FUniqueNetId& UniqueId)
{
	bool bFound = false;
	FNamedOnlineSession* Session = SessionInt->GetNamedSession(SessionName);
	if (Session != NULL)
	{
		const bool bIsSessionOwner = *Session->OwningUserId == UniqueId;

		FUniqueNetIdMatcher PlayerMatch(UniqueId);
		if (bIsSessionOwner || 
			Session->RegisteredPlayers.FindMatch(PlayerMatch) != INDEX_NONE)
		{
			bFound = true;
		}
	}
	return bFound;
}

