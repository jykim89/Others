// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SourceCodeAccessSettings.generated.h"

UCLASS(config=EditorUserSettings)
class USourceCodeAccessSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	/** The source code editor we prefer to use. */
	UPROPERTY(Config, EditAnywhere, Category="Source Code Editor", meta=(DisplayName="Source Code Editor"))
	FString PreferredAccessor;
};