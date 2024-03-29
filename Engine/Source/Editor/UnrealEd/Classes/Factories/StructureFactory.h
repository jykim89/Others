// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StructureFactory.generated.h"

UCLASS(hidecategories=Object, MinimalAPI)
class UStructureFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	// Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) OVERRIDE;
	// Begin UFactory Interface
};



