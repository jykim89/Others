// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ListStaticMeshesImportedFromSpeedTreesCommandlet.generated.h"

UCLASS()
class UListStaticMeshesImportedFromSpeedTreesCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()

	// Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) OVERRIDE;
	// End UCommandlet Interface
};