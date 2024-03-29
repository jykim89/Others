// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ILauncherProfile.h: Declares the ILauncherProfile interface.
=============================================================================*/

#pragma once


namespace ELauncherProfileCookModes
{
	/**
	 * Enumerates modes in which the launcher cooks builds.
	 */
	enum Type
	{
		/**
		 * Do not cook the build (default).
		 */
		DoNotCook,

		/**
		 * Pre-cook using user specified settings.
		 */
		ByTheBook,

		/**
		 * Cook the build on the fly while the game is running.
		 */
		OnTheFly
	};
}


namespace ELauncherProfileCookedMaps
{
	/**
	 * Enumerates selections for maps to cook.
	 */
	enum Type
	{
		/**
		 * Cook all maps.
		 */
		AllMaps,

		/**
		 * Don't cook any maps. Only startup packages will be cooked.
		 */
		NoMaps,

		/**
		 * Cook user selected maps.
		 */
		SelectedMaps

	};
}


namespace ELauncherProfileDeploymentModes
{
	/**
	 * Enumerates deployment modes.
	 */
	enum Type
	{
		/**
		 * Do not deploy the build to any device.
		 */
		DoNotDeploy,

		/**
		 * Copy all required file to the device.
		 */
		CopyToDevice,

		/**
		 * Let the device get required files from a file server.
		 */
		FileServer,

		/**
		 * Copy a build from a repository to the device.
		 */
		 CopyRepository,
	};
}


namespace ELauncherProfileLaunchModes
{
	/**
	 * Enumerates launch modes.
	 */
	enum Type
	{
		/**
		 * Do not launch.
		 */
		DoNotLaunch,

		/**
		 * Launch with customized roles per device.
		 */
		CustomRoles,

		/**
		 * Launch with the default role on all deployed devices.
		 */
		DefaultRole	
	};
}


namespace ELauncherProfilePackagingModes
{
	/**
	 * Enumerates packaging modes.
	 */
	enum Type
	{
		/**
		 * Do not package.
		 */
		DoNotPackage,

		/**
		 * Package and store the build locally.
		 */
		Locally,

		/**
		 * Package and store the build in a shared repository.
		 */
		SharedRepository
	};
}


namespace ELauncherProfileValidationErrors
{
	/**
	 * Enumerates profile validation messages.
	 */
	enum Type
	{
		/**
		 * Deployment by copying required files to a device requires
		 * cooking by the book and is incompatible with cook on the fly.
		 */
		CopyToDeviceRequiresCookByTheBook,

		/**
		 * Custom launch roles are not yet supported.
		 */
		CustomRolesNotSupportedYet,

		/**
		 * A device group must be selected when deploying builds.
		 */
		DeployedDeviceGroupRequired,
		
		/**
		 * The initial culture configured for launch is not part of the selected build.
		 */
		InitialCultureNotAvailable,

		/**
		 * The initial map configured for launch is not part of the selected build.
		 */
		InitialMapNotAvailable,

		/**
		 * The specified launch command line is not formatted correctly.
		 */
		MalformedLaunchCommandLine,

		/**
		 * A build configuration is required when creating new builds.
		 */
		NoBuildConfigurationSelected,

		/**
		 * When cooking a build, at least one culture must be included.
		 */
		NoCookedCulturesSelected,

		/**
		 * One or more launch roles do not have a device assigned.
		 */
		NoLaunchRoleDeviceAssigned,

		/**
		 * At least one platform is required when creating new builds.
		 */
		NoPlatformSelected,

		/**
		 * A game is required when creating new builds.
		 */
		NoProjectSelected,

		/**
		 * The deployment requires a package directory to be specified
		 */
		NoPackageDirectorySpecified,
		
		/**
		 * The platform SDK is not installed but is required.
		 */
		NoPlatformSDKInstalled,
	};
}


/**
 * Type definition for shared pointers to instances of ILauncherProfile.
 */
typedef TSharedPtr<class ILauncherProfile> ILauncherProfilePtr;

/**
 * Type definition for shared references to instances of ILauncherProfile.
 */
typedef TSharedRef<class ILauncherProfile> ILauncherProfileRef;


/**
 * Delegate type for changing the device group to deploy to.
 *
 * The first parameter is the selected device group (or NULL if the selection was cleared).
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLauncherProfileDeployedDeviceGroupChanged, const ILauncherDeviceGroupPtr&)

/**
 * Delegate type for a change in project
 */
DECLARE_MULTICAST_DELEGATE(FOnProfileProjectChanged);

/**
 * Interface for launcher profile.
 */
class ILauncherProfile
{
public:

	/**
	 * Gets the unique identifier of the profile.
	 *
	 * @return The profile identifier.
	 */
	virtual FGuid GetId( ) const = 0;

	/**
	 * Gets the human readable name of the profile.
	 *
	 * @return The profile name.
	 */
	virtual FString GetName( ) const = 0;

	/**
	 * Checks whether the last validation yielded the specified error.
	 *
	 * @param Error - The validation error to check for.
	 *
	 * @return true if the error is present, false otherwise.
	 */
	virtual bool HasValidationError( ELauncherProfileValidationErrors::Type Error ) const = 0;

	/**
	 * Checks whether devices of the specified platform can be deployed to.
	 *
	 * Whether a platform is deployable depends on the current profile settings.
	 * The right combination of build, cook and package settings must be present.
	 *
	 * @param PlatformName - The name of the platform to deploy.
	 *
	 * @return true if the platform is deployable, false otherwise.
	 */
	virtual bool IsDeployablePlatform( const FString& PlatformName ) = 0;

	/**
	 * Checks whether this profile is valid to use when running a game instance.
	 *
	 * @return Whether the profile is valid or not.
	 */
	virtual bool IsValidForLaunch( ) = 0;

	/**
	 * Serializes the profile from or into the specified archive.
	 *
	 * @param Archive - The archive to serialize from or into.
	 *
	 * @return true if the profile was serialized, false otherwise.
	 */
	virtual bool Serialize( FArchive& Archive ) = 0;

	/**
	 * Sets all profile settings to their defaults.
	 */
	virtual void SetDefaults( ) = 0;

	/**
	 * Updates the name of the profile.
	 *
	 * @param NewName - The new name of the profile.
	 */
	virtual void SetName( const FString& NewName ) = 0;


public:

	/**
	 * Gets the name of the build configuration.
	 *
	 * @return Build configuration name.
	 *
	 * @see SetBuildConfigurationName
	 */
	virtual EBuildConfigurations::Type GetBuildConfiguration( ) const = 0;

	/**
	 * Gets the build configuration name of the cooker.
	 *
	 * @return Cook configuration name.
	 *
	 * @see SetCookConfigurationName
	 */
	virtual EBuildConfigurations::Type GetCookConfiguration( ) const = 0;

	/**
	 * Gets the selected cook mode.
	 *
	 * @return Cook mode.
	 */
	virtual ELauncherProfileCookModes::Type GetCookMode( ) const = 0;

	/**
	 * Gets the cooker command line options.
	 *
	 * @return Cook options string.
	 */
	virtual const FString& GetCookOptions( ) const = 0;

	/**
	 * Gets the list of cooked culture.
	 *
	 * @return Collection of culture names.
	 *
	 * @see AddCookedCulture
	 * @see ClearCookedCultures
	 * @see RemoveCookedCulture
	 */
	virtual const TArray<FString>& GetCookedCultures( ) const = 0;

	/**
	 * Gets the list of cooked maps.
	 *
	 * @return Collection of map names.
	 *
	 * @see AddCookedMap
	 * @see ClearCookedMaps
	 * @see RemoveCookedMap
	 */
	virtual const TArray<FString>& GetCookedMaps( ) const = 0;

	/**
	 * Gets the names of the platforms to build for.
	 *
	 * @return Read-only collection of platform names.
	 *
	 * @see AddCookedPlatform
	 * @see ClearCookedPlatforms
	 * @see RemoveCookedPlatform
	 */
	virtual const TArray<FString>& GetCookedPlatforms( ) const = 0;

	/**
	 * Gets the default launch role.
	 *
	 * @return A reference to the default launch role.
	 */
	virtual const ILauncherProfileLaunchRoleRef& GetDefaultLaunchRole( ) const = 0;

	/**
	 * Gets the device group to deploy to.
	 *
	 * @return The device group, or NULL if none was configured.
	 *
	 * @see SetDeployedDeviceGroup
	 */
	virtual ILauncherDeviceGroupPtr GetDeployedDeviceGroup( ) const = 0;

	/**
	 * Gets the deployment mode.
	 *
	 * @return The deployment mode.
	 *
	 * @see SetDeploymentMode
	 */
	virtual ELauncherProfileDeploymentModes::Type GetDeploymentMode( ) const = 0;

    /**
     * Gets the close mode for the cook on the fly server
     *
     * @return the close mode
     *
     * @see SetForceClose
     */
    virtual bool GetForceClose() const = 0;
    
	/**
	 * Gets the launch mode.
	 *
	 * @return The launch mode.
	 *
	 * @see SetLaunchMode
	 */
	virtual ELauncherProfileLaunchModes::Type GetLaunchMode( ) const = 0;

	/**
	 * Gets the profile's collection of launch roles.
	 *
	 * @return A read-only collection of launch roles.
	 *
	 * @see CreateLaunchRole
	 * @see RemoveLaunchRole
	 */
	virtual const TArray<ILauncherProfileLaunchRolePtr>& GetLaunchRoles( ) const = 0;

	/**
	 * Gets the launch roles assigned to the specified device.
	 *
	 * @param DeviceId - The identifier of the device.
	 * @param OutRoles - Will hold the assigned roles, if any.
	 */
	virtual const int32 GetLaunchRolesFor( const FString& DeviceId, TArray<ILauncherProfileLaunchRolePtr>& OutRoles ) = 0;

	/**
	 * Gets the packaging mode.
	 *
	 * @return The packaging mode.
	 *
	 * @see SetPackagingMode
	 */
	virtual ELauncherProfilePackagingModes::Type GetPackagingMode( ) const = 0;

	/**
	 * Gets the packaging directory.
	 *
	 * @return The packaging directory.
	 *
	 * @see SetPackageDirectory
	 */
	virtual FString GetPackageDirectory( ) const = 0;

	/**
	 * Gets the name of the Unreal project to use.
	 */
	virtual FString GetProjectName( ) const = 0;

	/**
	 * Gets the base project path for the project (e.g. Samples/Showcases/MyShowcase)
	 */
	virtual FString GetProjectBasePath() const = 0;

	/**
	 * Gets the full path to the Unreal project to use.
	 *
	 * @return The path.
	 *
	 * @see SetRocketProjactPath
	 */
	virtual const FString& GetProjectPath( ) const = 0;

    /**
     * Gets the timeout time for the cook on the fly server
     *
     * @return timeout time
     *
     * @see SetTimeout
     */
    virtual uint32 GetTimeout() const = 0;
    
	/**
	 * Checks whether the game should be built.
	 *
	 * @return true if building the game, false otherwise.
	 *
	 * @see SetBuildGame
	 */
	virtual bool IsBuilding() const = 0;

	/**
	 * Checks whether incremental cooking is enabled.
	 *
	 * @return true if cooking incrementally, false otherwise.
	 *
	 * @see SetIncrementalCooking
	 */
	virtual bool IsCookingIncrementally( ) const = 0;

	/**
	 * Checks whether unversioned cooking is enabled.
	 *
	 * @return true if cooking unversioned, false otherwise.
	 *
	 * @see SetUnversionedCooking
	 */
	virtual bool IsCookingUnversioned( ) const = 0;

	/**
	 * Checks whether the file server's console window should be hidden.
	 *
	 * @return true if the file server should be hidden, false otherwise.
	 *
	 * @see SetHideFileServer
	 */
	virtual bool IsFileServerHidden( ) const = 0;

	/**
	 * Checks whether the file server is a streaming file server.
	 *
	 * @return true if the file server is streaming, false otherwise.
	 *
	 * @see SetStreamingFileServer
	 */
	virtual bool IsFileServerStreaming( ) const = 0;

	/**
	 * Checks whether packaging with UnrealPak is enabled.
	 *
	 * @return true if UnrealPak is used, false otherwise.
	 *
	 * @see SetPackageWithUnrealPak
	 */
	virtual bool IsPackingWithUnrealPak( ) const = 0;

	/**
	 * Checks whether the profile's selected project supports Engine maps.
	 *
	 * @return true if Engine maps are supported, false otherwise.
	 */
	virtual bool SupportsEngineMaps( ) const = 0;


public:

	/**
	 * Adds a culture to cook (only used if cooking by the book).
	 *
	 * @param CultureName - The name of the culture to cook.
	 *
	 * @see ClearCookedCultures
	 * @see GetCookedCultures
	 * @see RemoveCookedCulture
	 */
	virtual void AddCookedCulture( const FString& CultureName ) = 0;

	/**
	 * Adds a map to cook (only used if cooking by the book).
	 *
	 * @param MapName - The name of the map to cook.
	 *
	 * @see ClearCookedMaps
	 * @see GetCookedMaps
	 * @see RemoveCookedMap
	 */
	virtual void AddCookedMap( const FString& MapName ) = 0;

	/**
	 * Adds a platform to cook (only used if cooking by the book).
	 *
	 * @param PlatformName - The name of the platform to add.
	 *
	 * @see ClearCookedPlatforms
	 * @see GetCookedPlatforms
	 * @see RemoveCookedPlatform
	 */
	virtual void AddCookedPlatform( const FString& PlatformName ) = 0;

	/**
	 * Removes all cooked cultures.
	 *
	 * @see AddCookedCulture
	 * @see GetCookedCulture
	 * @see RemoveCookedCulture
	 */
	virtual void ClearCookedCultures( ) = 0;

	/**
	 * Removes all cooked maps.
	 *
	 * @see AddCookedMap
	 * @see GetCookedMap
	 * @see RemoveCookedMap
	 */
	virtual void ClearCookedMaps( ) = 0;

	/**
	 * Removes all cooked platforms.
	 *
	 * @see AddCookedPlatform
	 * @see GetCookedPlatforms
	 * @see RemoveCookedPlatform
	 */
	virtual void ClearCookedPlatforms( ) = 0;

	/**
	 * Creates a new launch role and adds it to the profile.
	 *
	 * @return The created role.
	 *
	 * @see GetLaunchRoles
	 * @see RemoveLaunchRole
	 */
	virtual ILauncherProfileLaunchRolePtr CreateLaunchRole( ) = 0;

	/**
	 * Removes a cooked culture.
	 *
	 * @param CultureName - The name of the culture to remove.
	 *
	 * @see AddCookedCulture
	 * @see ClearCookedCultures
	 * @see GetCookedCultures
	 */
	virtual void RemoveCookedCulture( const FString& CultureName ) = 0;

	/**
	 * Removes a cooked map.
	 *
	 * @param MapName - The name of the map to remove.
	 *
	 * @see AddCookedMap
	 * @see ClearCookedMaps
	 * @see GetCookedMaps
	 */
	virtual void RemoveCookedMap( const FString& MapName ) = 0;

	/**
	 * Removes a platform from the cook list.
	 *
	 * @param PlatformName - The name of the platform to remove.
	 *
	 * @see AddBuildPlatform
	 * @see ClearCookedPlatforms
	 * @see GetBuildPlatforms
	 */
	virtual void RemoveCookedPlatform( const FString& PlatformName ) = 0;

	/**
	 * Removes the given launch role from the profile.
	 *
	 * @param Role - The role to remove.
	 *
	 * @see CreateLaunchRole
	 * @see GetLaunchRoles
	 */
	virtual void RemoveLaunchRole( const ILauncherProfileLaunchRoleRef& Role ) = 0;

	/**
	 * Sets whether to build the game.
	 *
	 * @param Build - Whether the game should be built.
	 *
	 * @see IsBuilding
	 */
	virtual void SetBuildGame( bool Build ) = 0;

	/**
	 * Sets the build configuration.
	 *
	 * @param ConfigurationName - The build configuration name to set.
	 *
	 * @see GetBuildConfigurationName
	 */
	virtual void SetBuildConfiguration( EBuildConfigurations::Type Configuration ) = 0;

	/**
	 * Sets the build configuration of the cooker.
	 *
	 * @param Configuration - The cooker's build configuration to set.
	 *
	 * @see GetBuildConfigurationName
	 */
	virtual void SetCookConfiguration( EBuildConfigurations::Type Configuration ) = 0;

	/**
	 * Sets the cook mode.
	 *
	 * @param Mode - The cook mode.
	 *
	 * @see GetCookMode
	 */
	virtual void SetCookMode( ELauncherProfileCookModes::Type Mode ) = 0;

	/**
	 * Sets whether to pack with UnrealPak.
	 *
	 * @param UseUnrealPak - Whether UnrealPak should be used.
	 *
	 * @see IsPackingWithUnrealPak
	 */
	virtual void SetDeployWithUnrealPak( bool UseUnrealPak ) = 0;

	/**
	 * Sets the device group to deploy to.
	 *
	 * @param DeviceGroup - The device group, or NULL to reset this setting.
	 *
	 * @see GetDeployedDeviceGroup
	 */
	virtual void SetDeployedDeviceGroup( const ILauncherDeviceGroupPtr& DeviceGroup ) = 0;

	/**
	 * Sets the deployment mode.
	 *
	 * @param Mode - The deployment mode to set.
	 *
	 * @see GetDeploymentMode
	 */
	virtual void SetDeploymentMode( ELauncherProfileDeploymentModes::Type Mode ) = 0;

    /**
     * Sets the cook on the fly close mode
     *
     * @param Close - the close mode to set
     *
     * @see GetForceClose
     */
    virtual void SetForceClose( bool Close ) = 0;
    
	/**
	 * Sets whether to hide the file server's console window.
	 *
	 * @param Hide - Whether to hide the window.
	 *
	 * @see GetHideFileServerWindow
	 */
	virtual void SetHideFileServerWindow( bool Hide ) = 0;

	/**
	 * Sets incremental cooking.
	 *
	 * @param Incremental - Whether cooking should be incremental.
	 *
	 * @see IsCookingIncrementally
	 */
	virtual void SetIncrementalCooking( bool Incremental ) = 0;

	/**
	 * Sets the launch mode.
	 *
	 * @param Mode - The launch mode to set.
	 *
	 * @see GetLaunchMode
	 */
	virtual void SetLaunchMode( ELauncherProfileLaunchModes::Type Mode ) = 0;

	/**
	 * Sets the packaging mode.
	 *
	 * @param Mode - The packaging mode to set.
	 *
	 * @see GetPackagingMode
	 */
	virtual void SetPackagingMode( ELauncherProfilePackagingModes::Type Mode ) = 0;

	/**
	 * Sets the packaging directory.
	 *
	 * @param Dir - The packaging directory to set.
	 *
	 * @see GetPackageDirectory
	 */
	virtual void SetPackageDirectory( const FString& Dir ) = 0;

	/**
	 * Sets the path to the Rocket project to use.
	 *
	 * @param Path - The full path to the project.
	 *
	 * @see GetRocketProjectPath
	 */
	virtual void SetProjectPath( const FString& Path ) = 0;

	/**
	 * Sets whether to use a streaming file server.
	 *
	 * @param Streaming - Whether a streaming server should be used.
	 *
	 * @see GetStreamingFileServer
	 */
	virtual void SetStreamingFileServer( bool Streaming ) = 0;

    /**
     * Sets the cook on the fly server timeout
     *
     * @param InTime - amount of time to wait before timing out
     *
     * @see GetTimeout
     */
    virtual void SetTimeout(uint32 InTime) = 0;
    
	/**
	 * Sets unversioned cooking.
	 *
	 * @param Unversioned - Whether cooking is unversioned.
	 *
	 * @see IsCookingUnversioned
	 */
	virtual void SetUnversionedCooking( bool Unversioned ) = 0;


	/**
	 * Accesses delegate used when the project changes
	 */
	virtual FOnProfileProjectChanged& OnProjectChanged() = 0;

public:

	/**
	 * Virtual destructor.
	 */
	virtual ~ILauncherProfile( ) { }
};
