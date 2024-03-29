// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UserDefinedStruct.generated.h"

UENUM()
enum EUserDefinedStructureStatus
{
	// Struct is in an unknown state
	UDSS_UpToDate,
	// Struct has been modified but not recompiled
	UDSS_Dirty,
	// Struct tried but failed to be compiled
	UDSS_Error,
	// Struct is a duplicate, the original one was changed
	UDSS_Duplicate,

	UDSS_MAX,
};

UCLASS()
class ENGINE_API UUserDefinedStruct : public UScriptStruct
{
	GENERATED_UCLASS_BODY()

public:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TEnumAsByte<enum EUserDefinedStructureStatus> Status;

	// the original struct, when current struct isn't a temporary duplicate, the field should be null
	UPROPERTY(Transient)
	TWeakObjectPtr<UUserDefinedStruct> PrimaryStruct;

	UPROPERTY()
	FString ErrorMessage;

	UPROPERTY()
	UObject* EditorData;
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	// UObject interface.
	virtual void Serialize(FArchive& Ar) OVERRIDE;
	virtual void PostDuplicate(bool bDuplicateForPIE) OVERRIDE;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const OVERRIDE;
	// End of UObject interface.
#endif	// WITH_EDITOR

	virtual void SerializeTaggedProperties( FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, uint8* Defaults ) const OVERRIDE;

	// UScriptStruct interface.
	virtual void RecursivelyPreload() OVERRIDE;
	// End of  UScriptStruct interface.
};
