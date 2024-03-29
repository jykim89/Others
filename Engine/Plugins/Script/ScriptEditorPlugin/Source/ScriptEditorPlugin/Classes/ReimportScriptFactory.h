// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// ReimportTextureFactory
//=============================================================================

#pragma once
#include "ScriptFactory.h"
#include "ReimportScriptFactory.generated.h"

UCLASS(MinimalApi, collapsecategories)
class UReimportScriptFactory : public UScriptFactory, public FReimportHandler
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	class UScriptAsset* OriginalScript;

	// Begin FReimportHandler interface
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) OVERRIDE;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) OVERRIDE;
	virtual EReimportResult::Type Reimport(UObject* Obj) OVERRIDE;
	// End FReimportHandler interface
};



