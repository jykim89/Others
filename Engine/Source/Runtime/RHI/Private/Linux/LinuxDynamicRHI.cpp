// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#include "RHI.h"
#include "ModuleManager.h"

#if USE_DYNAMIC_RHI

FDynamicRHI* PlatformCreateDynamicRHI()
{
	FDynamicRHI* DynamicRHI = NULL;
	// Load the dynamic RHI module.
	IDynamicRHIModule* DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("OpenGLDrv"));

	if (!DynamicRHIModule->IsSupported())
	{
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "RequiredOpenGL", "OpenGL 3.2 is required to run the engine."));
		FPlatformMisc::RequestExit(1);
		DynamicRHIModule = NULL;
	}

	// Create the dynamic RHI.
	if (DynamicRHIModule)
	{
		DynamicRHI = DynamicRHIModule->CreateRHI();
	}

	return DynamicRHI;
}

#endif // USE_DYNAMIC_RHI
