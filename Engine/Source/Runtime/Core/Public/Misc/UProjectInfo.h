// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

/** Dictionary of all the non-foreign projects for an engine installation, found by parsing .uprojectdirs files for source directories. */
class CORE_API FUProjectDictionary
{
public:
	/** Scans the engine root directory for all the known projects */
	FUProjectDictionary(const FString& InRootDir);
	
	/** Determines whether a project is a foreign project or not */
	bool IsForeignProject(const FString& ProjectFileName) const;

	/** Gets the project filename for the given game. Empty if not found. */
	FString GetRelativeProjectPathForGame(const TCHAR* GameName, const FString& BaseDir) const;

	/** Gets a list of all the known projects */
	TArray<FString> GetProjectPaths() const;

	/** Gets the project dictionary for the active engine installation */
	static FUProjectDictionary& GetDefault();

private:
	/** Map of short game names to full project paths */
	TMap<FString, FString> ShortProjectNameDictionary;
};
