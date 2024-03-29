// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SkillSystemGlobals.generated.h"

/** Holds global data for the skill system. Can be configured per project via config file */
UCLASS(config=Game)
class SKILLSYSTEM_API USkillSystemGlobals : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Holds all of the valid gameplay-related tags that can be applied to assets */
	UPROPERTY(config)
	FString GlobalCurveTableName;

	/** Holds information about the valid attributes' min and max values and stacking rules */
	UPROPERTY(config)
	FString GlobalAttributeDataTableName;

	class UCurveTable *	GetGlobalCurveTable();

	class UDataTable * GetGlobalAttributeDataTable();

	void AutomationTestOnly_SetGlobalCurveTable(class UCurveTable *InTable)
	{
		GlobalCurveTable = InTable;
	}

	void AutomationTestOnly_SetGlobalAttributeDataTable(class UDataTable *InTable)
	{
		GlobalAttributeDataTable = InTable;
	}

private:

	class UCurveTable* GlobalCurveTable;

	class UDataTable* GlobalAttributeDataTable;

#if WITH_EDITOR
	void OnCurveTableReimported(UObject* InObject);
	void OnDataTableReimported(UObject* InObject);
#endif

};
