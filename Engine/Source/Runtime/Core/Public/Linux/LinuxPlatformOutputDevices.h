// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	LinuxPlatformOutputDevices.h: Linux platform OutputDevices functions
==============================================================================================*/

#pragma once

struct CORE_API FLinuxOutputDevices : public FGenericPlatformOutputDevices
{
	static class FFeedbackContext*		GetWarn();
};

typedef FLinuxOutputDevices FPlatformOutputDevices;
