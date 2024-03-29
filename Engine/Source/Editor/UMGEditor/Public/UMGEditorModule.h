// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnrealEd.h"
#include "ModuleInterface.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkit.h"

extern const FName UMGEditorAppIdentifier;

class FUMGEditor;

class IUMGEditorModule : public IModuleInterface, public IHasMenuExtensibility, public IHasToolBarExtensibility
{
public:

};
