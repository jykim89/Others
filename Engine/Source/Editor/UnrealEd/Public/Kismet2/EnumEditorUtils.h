// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#ifndef __EnumEditorUtils_h__
#define __EnumEditorUtils_h__

#pragma once

#include "Engine.h"
#include "ListenerManager.h"

class UNREALED_API FEnumEditorUtils
{
	static void BroadcastChanges(const UUserDefinedEnum* Enum, const TArray<FName>& OldNames, bool bResolveData = true);

	/** copy full enumeratos names from given enum to OutEnumNames, the last '_MAX' enumerator is skipped */
	static void CopyEnumeratorsWithoutMax(const UEnum* Enum, TArray<FName>& OutEnumNames);
public:

	class FEnumEditorManager : public FListenerManager<UUserDefinedEnum>
	{
		FEnumEditorManager();
	public:
		UNREALED_API static FEnumEditorManager& Get();
	};

	typedef FEnumEditorManager::ListenerType INotifyOnEnumChanged;

	//////////////////////////////////////////////////////////////////////////
	// User defined enumerations

	/** Creates new user defined enum in given blueprint. */
	static UEnum* CreateUserDefinedEnum(UObject* InParent, FName EnumName, EObjectFlags Flags);

	/** return if an enum can be named/renamed with given name*/
	static bool IsNameAvailebleForUserDefinedEnum(FName Name);

	/** Updates enumerators names after name or path of the Enum was changed */
	static void UpdateAfterPathChanged(UEnum* Enum);

	/** adds new enumerator (with default unique name) for user defined enum */
	static void AddNewEnumeratorForUserDefinedEnum(class UUserDefinedEnum* Enum);

	/** Removes enumerator from enum*/
	static void RemoveEnumeratorFromUserDefinedEnum(class UUserDefinedEnum* Enum, int32 EnumeratorIndex);

	/** Reorder enumerators in enum. Swap an enumerator with given name, with previous or next (based on bDirectionUp parameter) */
	static void MoveEnumeratorInUserDefinedEnum(class UUserDefinedEnum* Enum, int32 EnumeratorIndex, bool bDirectionUp);

	/** check if NewName is a short name and is acceptable as name in given enum */
	static bool IsProperNameForUserDefinedEnumerator(const UEnum* Enum, FString NewName);

	/*
	 *	Try to update an out-of-date enum index after an enum's change
	 *
	 *	@param Enum - new version of enum
	 *	@param Ar - special archive
	 *	@param EnumeratorIndex - old enumerator index
	 *
	 *	@return new enum 
	 */
	static int32 ResolveEnumerator(const UEnum* Enum, FArchive& Ar, int32 EnumeratorIndex);

	//DISPLAY NAME
	static FString GetEnumeratorDisplayName(const UUserDefinedEnum* Enum, int32 EnumeratorIndex);
	static bool SetEnumeratorDisplayName(UUserDefinedEnum* Enum, int32 EnumeratorIndex, FString NewDisplayName);
	static bool IsEnumeratorDisplayNameValid(const UUserDefinedEnum* Enum, FString NewDisplayName);
	static void EnsureAllDisplayNamesExist(class UUserDefinedEnum* Enum);
};

#endif // __EnumEditorUtils_h__
