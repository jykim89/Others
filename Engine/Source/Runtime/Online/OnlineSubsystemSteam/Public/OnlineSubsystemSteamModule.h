// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "ModuleInterface.h"

/**
 * Online subsystem module class  (STEAM Implementation)
 * Code related to the loading of the STEAM module
 */
class FOnlineSubsystemSteamModule : public IModuleInterface
{
private:

	/** Class responsible for creating instance(s) of the subsystem */
	class FOnlineFactorySteam* SteamFactory;

#if PLATFORM_WINDOWS || PLATFORM_MAC
	/** Handle to the STEAM API dll */
	void* SteamDLLHandle;
	/** Handle to the STEAM dedicated server support dlls */
	void* SteamServerDLLHandle;
#endif	//PLATFORM_WINDOWS

	/**
	 *	Load the required modules for Steam
	 */
	void LoadSteamModules();

	/** 
	 *	Unload the required modules for Steam
	 */
	void UnloadSteamModules();

public:

	FOnlineSubsystemSteamModule()
        : SteamFactory(NULL)
#if PLATFORM_WINDOWS || PLATFORM_MAC
		, SteamDLLHandle(NULL)
		, SteamServerDLLHandle(NULL)
#endif	//PLATFORM_WINDOWS
	{}

	virtual ~FOnlineSubsystemSteamModule() {}

	// IModuleInterface

	virtual void StartupModule() OVERRIDE;
	virtual void ShutdownModule() OVERRIDE;

	virtual bool SupportsDynamicReloading() OVERRIDE
	{
		return false;
	}

	virtual bool SupportsAutomaticShutdown() OVERRIDE
	{
		return false;
	}

	/**
	 * Are the Steam support Dlls loaded
	 */
	bool AreSteamDllsLoaded() const;
};