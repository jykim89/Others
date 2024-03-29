// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SlateRemoteModule.cpp: Implements the SlateRemote module.
=============================================================================*/

#include "SlateRemotePrivatePCH.h"


#define LOCTEXT_NAMESPACE "FSlateRemoteModule"


/**
 * Implements the SlateRemoteModule module.
 */
class FSlateRemoteModule
	: public IModuleInterface
{
public:

	// Begin IModuleInterface interface

	virtual void StartupModule( ) OVERRIDE
	{
		if (!SupportsSlateRemote())
		{
			return;
		}

		// register settings
		ISettingsModule* SettingsModule = ISettingsModule::Get();

		if (SettingsModule != nullptr)
		{
			FSettingsSectionDelegates SettingsDelegates;
			SettingsDelegates.ModifiedDelegate = FOnSettingsSectionModified::CreateRaw(this, &FSlateRemoteModule::HandleSettingsSaved);

			SettingsModule->RegisterSettings("Project", "Plugins", "SlateRemote",
				LOCTEXT("SlateRemoteSettingsName", "Slate Remote"),
				LOCTEXT("SlateRemoteSettingsDescription", "Configure the Slate Remote plug-in."),
				GetMutableDefault<USlateRemoteSettings>(),
				SettingsDelegates
			);
		}

		// register application events
		FCoreDelegates::ApplicationHasReactivatedDelegate.AddRaw(this, &FSlateRemoteModule::HandleApplicationHasReactivated);
		FCoreDelegates::ApplicationWillDeactivateDelegate.AddRaw(this, &FSlateRemoteModule::HandleApplicationWillDeactivate);

		RestartServices();
	}

	virtual void ShutdownModule( ) OVERRIDE
	{
		// unregister application events
		FCoreDelegates::ApplicationHasReactivatedDelegate.RemoveAll(this);
		FCoreDelegates::ApplicationWillDeactivateDelegate.RemoveAll(this);

		// unregister settings
		ISettingsModule* SettingsModule = ISettingsModule::Get();

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "SlateRemote");
		}

		// shut down services
		ShutdownRemoteServer();
	}

	virtual bool SupportsDynamicReloading( ) OVERRIDE
	{
		return true;
	}

	// End IModuleInterface interface

protected:

	/**
	 * Initializes the Slate Remote server with the current settings.
	 */
	void InitializeRemoteServer( )
	{
		ShutdownRemoteServer();

		USlateRemoteSettings* Settings = GetMutableDefault<USlateRemoteSettings>();

		// load settings
		bool ResaveSettings = false;

		FIPv4Endpoint ServerEndpoint;

		if (GIsEditor)
		{
			if (!FIPv4Endpoint::Parse(Settings->EditorServerEndpoint, ServerEndpoint))
			{
				if (!Settings->EditorServerEndpoint.IsEmpty())
				{
					GLog->Logf(TEXT("Warning: Invalid Slate Remote EditorServerEndpoint '%s' - binding to all local network adapters instead"), *Settings->EditorServerEndpoint);
				}

				ServerEndpoint = SLATE_REMOTE_SERVER_DEFAULT_EDITOR_ENDPOINT;
				Settings->EditorServerEndpoint = ServerEndpoint.ToText().ToString();
				ResaveSettings = true;
			}
		}
		else
		{
			if (!FIPv4Endpoint::Parse(Settings->GameServerEndpoint, ServerEndpoint))
			{
				if (!Settings->GameServerEndpoint.IsEmpty())
				{
					GLog->Logf(TEXT("Warning: Invalid Slate Remote GameServerEndpoint '%s' - binding to all local network adapters instead"), *Settings->GameServerEndpoint);
				}

				ServerEndpoint = SLATE_REMOTE_SERVER_DEFAULT_GAME_ENDPOINT;
				Settings->GameServerEndpoint = ServerEndpoint.ToText().ToString();
				ResaveSettings = true;
			}
		}

		if (ResaveSettings)
		{
			Settings->SaveConfig();
		}

		// create server
		ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

		if (SocketSubsystem != nullptr)
		{
			RemoteServer = MakeShareable(new FSlateRemoteServer(*SocketSubsystem, ServerEndpoint));
		}
		else
		{
			GLog->Logf(TEXT("Error: SlateRemote: Failed to acquire socket subsystem."));
		}
	}

	/**
	 * Restarts the services that this modules provides.
	 */
	void RestartServices( )
	{
		const USlateRemoteSettings& Settings = *GetDefault<USlateRemoteSettings>();

		if (Settings.EnableRemoteServer)
		{
			if (!RemoteServer.IsValid())
			{
				InitializeRemoteServer();
			}
		}
		else
		{
			ShutdownRemoteServer();
		}
	}

	/**
	 * Shuts down the Slate Remote server.
	 */
	void ShutdownRemoteServer( )
	{
		RemoteServer.Reset();
	}

	/**
	 * Checks whether the Slate Remote server is supported.
	 *
	 * @todo gmp: this should be moved into an Engine module, so it can be shared with other transports
	 *
	 * @return true if networked transport is supported, false otherwise.
	 */
	bool SupportsSlateRemote( ) const
	{
		// disallow in Shipping and Test configurations
		if ((FApp::GetBuildConfiguration() == EBuildConfigurations::Shipping) || (FApp::GetBuildConfiguration() == EBuildConfigurations::Test))
		{
			return false;
		}

		// disallow for commandlets
		if (IsRunningCommandlet())
		{
			return false;
		}

		return true;
	}

private:

	// Callback for when an has been reactivated (i.e. return from sleep on iOS).
	void HandleApplicationHasReactivated( )
	{
		RestartServices();
	}

	// Callback for when the application will be deactivated (i.e. sleep on iOS).
	void HandleApplicationWillDeactivate( )
	{
		ShutdownRemoteServer();
	}

	// Callback for when the settings were saved.
	bool HandleSettingsSaved( )
	{
		RestartServices();

		return true;
	}

private:

	// Holds the Slate Remote server.
	TSharedPtr<FSlateRemoteServer> RemoteServer;
};


IMPLEMENT_MODULE(FSlateRemoteModule, SlateRemote);


#undef LOCTEXT_NAMESPACE
