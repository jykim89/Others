// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ConfigCacheIni.h: Unreal config file reading/writing.
=============================================================================*/

/*-----------------------------------------------------------------------------
	Config cache.
-----------------------------------------------------------------------------*/

#pragma once

CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogConfig, Warning, All);


typedef TMultiMap<FName,FString> FConfigSectionMap;

// One section in a config file.
class FConfigSection : public FConfigSectionMap
{
public:
	bool HasQuotes( const FString& Test ) const;
	bool operator==( const FConfigSection& Other ) const;
	bool operator!=( const FConfigSection& Other ) const;
};

/**
 * FIniFilename struct.
 * 
 * Helper struct for generating ini files.
 */
struct FIniFilename
{
	/** Ini filename */
	FString Filename;
	/** If true this ini file is required to generate the output ini. */
	bool bRequired;

	FIniFilename(const FString& InFilename, bool InIsRequired)
		: Filename(InFilename)
		, bRequired(InIsRequired) 
	{}
};

// One config file.
class FConfigFile : public TMap<FString,FConfigSection>
{
public:
	bool Dirty, NoSave;

	/** The name of this config file */	
	FName Name;

	// The collection of source files which were used to generate this file.
	TArray< FIniFilename > SourceIniHierarchy;

	/** The untainted config file which contains the coalesced base/default options. I.e. No Saved/ options*/
	FConfigFile* SourceConfigFile;
	
	CORE_API FConfigFile();
	FConfigFile( int32 ) {}	// @todo UE4 DLL: Workaround for instantiated TMap template during DLLExport (TMap::FindRef)
	CORE_API ~FConfigFile();
	
	bool operator==( const FConfigFile& Other ) const;
	bool operator!=( const FConfigFile& Other ) const;

	CORE_API bool Combine( const FString& Filename);
	CORE_API void CombineFromBuffer(const FString& Filename,const FString& Buffer);
	void Read( const FString& Filename );
	bool Write( const FString& Filename, bool bDoRemoteWrite=true, const FString& InitialText=FString() );
	CORE_API void Dump(FOutputDevice& Ar);

	CORE_API bool GetString( const TCHAR* Section, const TCHAR* Key, FString& Value ) const;
	CORE_API bool GetText( const TCHAR* Section, const TCHAR* Key, FText& Value ) const;
	bool GetInt64( const TCHAR* Section, const TCHAR* Key, int64& Value ) const;

	void SetString( const TCHAR* Section, const TCHAR* Key, const TCHAR* Value );
	void SetText( const TCHAR* Section, const TCHAR* Key, const FText& Value );

	void SetInt64( const TCHAR* Section, const TCHAR* Key, const int64 Value );
	
	/**
	 * Process the contents of an .ini file that has been read into an FString
	 * 
	 * @param Filename Name of the .ini file the contents came from
	 * @param Contents Contents of the .ini file
	 */
	void ProcessInputFileContents(const FString& Filename, FString& Contents);


	/** Adds any properties that exist in InSourceFile that this config file is missing */
	void AddMissingProperties( const FConfigFile& InSourceFile );

	/**
	 * Saves only the sections in this FConfigFile its source files. All other sections in the file are left alone. The sections in this
	 * file are completely replaced. If IniRootName is specified, the saved settings are the diffed against the file in the hierarchy up
	 * to right before this file (so, if you are saving DefaultEngine.ini, and IniRootName is "Engine", then Base.ini and BaseEngine.ini
	 * will be loaded, and only differences against that will be saved into DefaultEngine.ini)
	 *
	 * ======================================
	 * @todo: This currently doesn't work with array properties!! It will output the entire array, and without + notation!!
	 * ======================================
	 *
	 * @param IniRootName the name (like "Engine") to use to load a .ini hierarchy to diff against
	 */
	CORE_API void UpdateSections(const TCHAR* DiskFilename, const TCHAR* IniRootName=NULL);

	
	/** 
	 * Check the source hierarchy which was loaded without any user changes from the Config/Saved dir.
	 * If anything in the default/base options have changed, we need to ensure that these propagate through
	 * to the final config so they are not potentially ignored
	 */
	void ProcessSourceAndCheckAgainstBackup();

private:

	/** Checks if the PropertyValue should be exported in quotes when writing the ini to disk. */
	bool ShouldExportQuotedString(const FString& PropertyValue) const;

	/** 
	 * Save the source hierarchy which was loaded out to a backup file so we can check future changes in the base/default configs
	 */
	void SaveSourceToBackupFile();

	/** 
	 * Process the property for Writing to a default file. We will need to check for array operations, as default ini's rely on this
	 * being correct to function properly
	 *
	 * @param InCompletePropertyToProcess - The complete property which we need to process for saving.
	 * @param OutText - The stream we are processing the array to
	 * @param SectionName - The section name the array property is being written to
	 * @param PropertyName - The property name of the array
	 */
	void ProcessPropertyAndWriteForDefaults(const TArray< FString >& InCompletePropertyToProcess, FString& OutText, const FString& SectionName, const FString& PropertyName);

};

/**
 * Declares a delegate type that's used by the config system to allow iteration of key value pairs.
 */
DECLARE_DELEGATE_TwoParams(FKeyValueSink, const TCHAR*, const TCHAR*);

// Set of all cached config files.
class CORE_API FConfigCacheIni : public TMap<FString,FConfigFile>
{
public:
	// Basic functions.
	FConfigCacheIni();
	virtual ~FConfigCacheIni();

	/**
	* Disables any file IO by the config cache system
	*/
	virtual void DisableFileOperations();

	/**
	* Re-enables file IO by the config cache system
	*/
	virtual void EnableFileOperations();

	/**
	 * Returns whether or not file operations are disabled
	 */
	virtual bool AreFileOperationsDisabled();

	/**
	 * @return true after after the basic .ini files have been loaded
	 */
	bool IsReadyForUse()
	{
		return bIsReadyForUse;
	}

	/**
	* Prases apart an ini section that contains a list of 1-to-N mappings of strings in the following format
	*	 [PerMapPackages]
	*	 MapName=Map1
	*	 Package=PackageA
	*	 Package=PackageB
	*	 MapName=Map2
	*	 Package=PackageC
	*	 Package=PackageD
	* 
	* @param Section Name of section to look in
	* @param KeyOne Key to use for the 1 in the 1-to-N (MapName in the above example)
	* @param KeyN Key to use for the N in the 1-to-N (Package in the above example)
	* @param OutMap Map containing parsed results
	* @param Filename Filename to use to find the section
	*
	* NOTE: The function naming is weird because you can't apparently have an overridden function differnt only by template type params
	*/
	virtual void Parse1ToNSectionOfStrings(const TCHAR* Section, const TCHAR* KeyOne, const TCHAR* KeyN, TMap<FString, TArray<FString> >& OutMap, const FString& Filename);

	/**
	* Parses apart an ini section that contains a list of 1-to-N mappings of names in the following format
	*	 [PerMapPackages]
	*	 MapName=Map1
	*	 Package=PackageA
	*	 Package=PackageB
	*	 MapName=Map2
	*	 Package=PackageC
	*	 Package=PackageD
	* 
	* @param Section Name of section to look in
	* @param KeyOne Key to use for the 1 in the 1-to-N (MapName in the above example)
	* @param KeyN Key to use for the N in the 1-to-N (Package in the above example)
	* @param OutMap Map containing parsed results
	* @param Filename Filename to use to find the section
	*
	* NOTE: The function naming is weird because you can't apparently have an overridden function differnt only by template type params
	*/
	virtual void Parse1ToNSectionOfNames(const TCHAR* Section, const TCHAR* KeyOne, const TCHAR* KeyN, TMap<FName, TArray<FName> >& OutMap, const FString& Filename);

	FConfigFile* FindConfigFile( const FString& Filename );
	FConfigFile* Find( const FString& InFilename, bool CreateIfNotFound );
	void Flush( bool Read, const FString& Filename=TEXT("") );

	void LoadFile( const FString& InFilename, const FConfigFile* Fallback = NULL, const TCHAR* PlatformString = NULL );
	void SetFile( const FString& InFilename, const FConfigFile* NewConfigFile );
	void UnloadFile( const FString& Filename );
	void Detach( const FString& Filename );

	bool GetString( const TCHAR* Section, const TCHAR* Key, FString& Value, const FString& Filename );
	bool GetText( const TCHAR* Section, const TCHAR* Key, FText& Value, const FString& Filename );
	bool GetSection( const TCHAR* Section, TArray<FString>& Result, const FString& Filename );
	FConfigSection* GetSectionPrivate( const TCHAR* Section, bool Force, bool Const, const FString& Filename );
	void SetString( const TCHAR* Section, const TCHAR* Key, const TCHAR* Value, const FString& Filename );
	void SetText( const TCHAR* Section, const TCHAR* Key, const FText& Value, const FString& Filename );
	void RemoveKey( const TCHAR* Section, const TCHAR* Key, const FString& Filename );
	void EmptySection( const TCHAR* Section, const FString& Filename );
	void EmptySectionsMatchingString( const TCHAR* SectionString, const FString& Filename );

	/**
	 * Retrieve a list of all of the config files stored in the cache
	 *
	 * @param ConfigFilenames Out array to receive the list of filenames
	 */
	void GetConfigFilenames(TArray<FString>& ConfigFilenames);

	/**
	 * Retrieve the names for all sections contained in the file specified by Filename
	 *
	 * @param	Filename			the file to retrieve section names from
	 * @param	out_SectionNames	will receive the list of section names
	 *
	 * @return	true if the file specified was successfully found;
	 */
	bool GetSectionNames( const FString& Filename, TArray<FString>& out_SectionNames );

	/**
	 * Retrieve the names of sections which contain data for the specified PerObjectConfig class.
	 *
	 * @param	Filename			the file to retrieve section names from
	 * @param	SearchClass			the name of the PerObjectConfig class to retrieve sections for.
	 * @param	out_SectionNames	will receive the list of section names that correspond to PerObjectConfig sections of the specified class
	 * @param	MaxResults			the maximum number of section names to retrieve
	 *
	 * @return	true if the file specified was found and it contained at least 1 section for the specified class
	 */
	bool GetPerObjectConfigSections( const FString& Filename, const FString& SearchClass, TArray<FString>& out_SectionNames, int32 MaxResults=1024 );

	void Exit();

	/**
	 * Prints out the entire config set, or just a single file if an ini is specified
	 * 
	 * @param Ar the device to write to
	 * @param IniName An optional ini name to restrict the writing to (Engine or WrangleContent) - meant to be used with "final" .ini files (not Default*)
	 */
	void Dump(FOutputDevice& Ar, const TCHAR* IniName=NULL);

	/**
	 * Dumps memory stats for each file in the config cache to the specified archive.
	 *
	 * @param	Ar	the output device to dump the results to
	 */
	virtual void ShowMemoryUsage( FOutputDevice& Ar );

	/**
	 * USed to get the max memory usage for the FConfigCacheIni
	 *
	 * @return the amount of memory in byes
	 */
	virtual SIZE_T GetMaxMemoryUsage();

	/**
	 * allows to iterate through all key value pairs
	 * @return false:error e.g. Section or Filename not found
	 */
	virtual bool ForEachEntry(const FKeyValueSink& Visitor, const TCHAR* Section, const FString& Filename);

	// Derived functions.
	FString GetStr
	(
		const TCHAR*		Section, 
		const TCHAR*		Key, 
		const FString&	Filename 
	);
	bool GetInt
	(
		const TCHAR*		Section,
		const TCHAR*		Key,
		int32&				Value,
		const FString&	Filename
	);
	bool GetFloat
	(
		const TCHAR*		Section,
		const TCHAR*		Key,
		float&				Value,
		const FString&	Filename
	);
	bool GetDouble
	(
		const TCHAR*		Section,
		const TCHAR*		Key,
		double&				Value,
		const FString&	Filename
	);
	bool GetBool
	(
		const TCHAR*		Section,
		const TCHAR*		Key,
		bool&				Value,
		const FString&	Filename
	);
	int32 GetArray
	(
		const TCHAR*		Section,
		const TCHAR*		Key,
		TArray<FString>&	out_Arr,
		const FString&	Filename
	);
	/** Loads a "delimited" list of strings
	 * @param Section - Section of the ini file to load from
	 * @param Key - The key in the section of the ini file to load
	 * @param out_Arr - Array to load into
	 * @param Filename - Ini file to load from
	 */
	int32 GetSingleLineArray
	(
		const TCHAR*		Section,
		const TCHAR*		Key,
		TArray<FString>&	out_Arr,
		const FString&	Filename
	);
	bool GetColor
	(
		const TCHAR*		Section,
		const TCHAR*		Key,
		FColor&				Value,
		const FString&	Filename
	);
	bool GetVector
	(
		const TCHAR*		Section,
		const TCHAR*		Key,
		FVector&			Value,
		const FString&	Filename
	);
	bool GetVector4
	(
		const TCHAR*		Section,
		const TCHAR*		Key,
		FVector4&			Value,
		const FString&	Filename
	);
	bool GetRotator
	(
		const TCHAR*		Section,
		const TCHAR*		Key,
		FRotator&			Value,
		const FString&	Filename
	);

	void SetInt
	(
		const TCHAR*		Section,
		const TCHAR*		Key,
		int32					Value,
		const FString&	Filename
	);
	void SetFloat
	(
		const TCHAR*		Section,
		const TCHAR*		Key,
		float				Value,
		const FString&	Filename
	);
	void SetDouble
	(
		const TCHAR*		Section,
		const TCHAR*		Key,
		double				Value,
		const FString&	Filename
	);
	void SetBool
	(
		const TCHAR*		Section,
		const TCHAR*		Key,
		bool				Value,
		const FString&	Filename
	);
	void SetArray
	(
		const TCHAR*			Section,
		const TCHAR*			Key,
		const TArray<FString>&	Value,
		const FString&		Filename
	);
	/** Saves a "delimited" list of strings
	 * @param Section - Section of the ini file to save to
	 * @param Key - The key in the section of the ini file to save
	 * @param out_Arr - Array to save from
	 * @param Filename - Ini file to save to
	 */
	void SetSingleLineArray
	(
		const TCHAR*			Section,
		const TCHAR*			Key,
		const TArray<FString>&	In_Arr,
		const FString&		Filename
	);
	void SetColor
	(
		const TCHAR*		Section,
		const TCHAR*		Key,
		FColor				Value,
		const FString&	Filename
	);
	void SetVector
	(
		const TCHAR*		Section,
		const TCHAR*		Key,
		FVector				Value,
		const FString&	Filename
	);
	void SetVector4
	(
		const TCHAR*		Section,
		const TCHAR*		Key,
		const FVector4&		Value,
		const FString&	Filename
	);
	void SetRotator
	(
		const TCHAR*		Section,
		const TCHAR*		Key,
		FRotator			Value,
		const FString&	Filename
	);

	// Static allocator.
	static FConfigCacheIni* Factory();

	// Static helper functions

	/**
	 * Creates GConfig, loads the standard global ini files (Engine, Editor, etc),
	 * fills out GEngineIni, etc. and marks GConfig as ready for use
	 */
	static void InitializeConfigSystem();

	/**
	 * Loads and generates a destination ini file and adds it to GConfig:
	 *   - Looking on commandline for override source/dest .ini filenames
	 *   - Generating the name for the engine to refer to the ini
	 *   - Loading a source .ini file
	 *   - Processing all BasedOn's
	 *   - Filling out an FConfigFile
	 *   - Save the generated ini
	 *   - Adds the FConfigFile to GConfig
	 *
	 * @param FinalIniFilename The output name of the generated .ini file (in Game\Saved\Config)
	 * @param BaseIniName The "base" ini name, with no extension (ie, Engine, Game, etc)
	 * @param Platform The platform to load the .ini for (if NULL, uses current)
	 * @param GameName The name of the game to load the .ini for (if NULL, uses current)
	 * @param bForceReload If true, the destination .in will be regenerated from the source, otherwise this will only process if the dest isn't in GConfig
	 * @param bRequireDefaultIni If true, the Default*.ini file is required to exist when generating the final ini file.
	 * @param bAllowGeneratedIniWhenCooked If true, the engine will attempt to load the generated/user INI file when loading.
	 * @param GeneratedConfigDir The location where generated config files are made.
	 * @return true if the final ini was created successfully.
	 */
	static bool LoadGlobalIniFile(FString& FinalIniFilename, const TCHAR* BaseIniName, const TCHAR* Platform=NULL, const TCHAR* GameName=NULL, bool bForceReload=false, bool bRequireDefaultIni=false, bool bAllowGeneratedIniWhenCooked=true, const TCHAR* GeneratedConfigDir = *FPaths::GeneratedConfigDir());

	/**
	 * Load an ini file directly into an FConfigFile, and nothing is written to GConfig or disk. 
	 * The passed in .ini name can be a "base" (Engine, Game) which will be modified by platform and/or commandline override,
	 * or it can be a full ini filenname (ie WrangleContent) loaded from the Source config directory
	 *
	 * @param ConfigFile The output object to fill
	 * @param IniName Either a Base ini name (Engine) or a full ini name (WrangleContent). NO PATH OR EXTENSION SHOULD BE USED!
	 * @param bIsBaseIniName true if IniName is a Base name, which can be overridden on commandline, etc.
	 * @param Platform The platform to use for Base ini names
	 * @param GameName The game to use for Base ini names
	 */
	static void LoadLocalIniFile(FConfigFile& ConfigFile, const TCHAR* IniName, bool bIsBaseIniName, const TCHAR* Platform=NULL, const TCHAR* GameName=NULL);

	/**
	 * Load an ini file directly into an FConfigFile from the specified config folders, and nothing is written to GConfig or disk. 
	 * The passed in .ini name can be a "base" (Engine, Game) which will be modified by platform and/or commandline override,
	 * or it can be a full ini filenname (ie WrangleContent) loaded from the Source config directory
	 *
	 * @param ConfigFile The output object to fill
	 * @param IniName Either a Base ini name (Engine) or a full ini name (WrangleContent). NO PATH OR EXTENSION SHOULD BE USED!
	 * @param EngineConfigDir Engine config directory.
	 * @param SourceConfigDir Game config directory.
	 * @param bGenerateDestIni true if IniName is a Base name, which can be overridden on commandline, etc.
	 * @param Platform The platform to use for Base ini names
	 * @param GameName The game to use for Base ini names
	 */
	static void LoadExternalIniFile(FConfigFile& ConfigFile, const TCHAR* IniName, const TCHAR* EngineConfigDir, const TCHAR* SourceConfigDir, bool bGenerateDestIni, const TCHAR* Platform=NULL, const TCHAR* GameName=NULL);

	/**
	 * Needs to be called after GConfig is set and LoadCoalescedFile was called.
	 * Loads the state of console variables.
	 * Works even if the variable is registered after the ini file was loaded.
	 */
	static void LoadConsoleVariablesFromINI();

private:
	/** true if file operations should not be performed */
	bool bAreFileOperationsDisabled;

	/** true after the base .ini files have been loaded, and GConfig is generally "ready for use" */
	bool bIsReadyForUse;
};

/**
 * Helper function to read the contents of an ini file and a specified group of cvar parameters, where sections in the ini file are marked [InName@InGroupNumber]
 * @param InSectionBaseName - The base name of the section to apply cvars from (i.e. the bit before the @)
 * @param InGroupNumber - The group number required
 * @param InIniFilename - The ini filename
 */
CORE_API void ApplyCVarSettingsGroupFromIni(const TCHAR* InSectionBaseName, int32 InGroupNumber, const TCHAR* InIniFilename);

/**
 * Helper function to read the contents of an ini file and a specified group of cvar parameters, where sections in the ini file are marked [InName]
 * @param InSectionBaseName - The base name of the section to apply cvars from
 * @param InIniFilename - The ini filename
 */
CORE_API void ApplyCVarSettingsFromIni(const TCHAR* InSectionBaseName, const TCHAR* InIniFilename);
