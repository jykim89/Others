// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	App.cpp: Implements the FApp class.
=============================================================================*/

#include "CorePrivate.h"
#include "Runtime/Launch/Resources/Version.h"


/* FApp static initialization
 *****************************************************************************/

FGuid FApp::InstanceId = FGuid::NewGuid();
FGuid FApp::SessionId = FGuid::NewGuid();
FString FApp::SessionName = FString();
FString FApp::SessionOwner = FString();
bool FApp::Standalone = true;
bool FApp::bIsBenchmarking = false;
double FApp::FixedDeltaTime = 1 / 30.0;
bool FApp::bUseFixedTimeStep = false;
double FApp::CurrentTime = 0.0;
double FApp::LastTime = 0.0;
double FApp::DeltaTime = 1 / 30.0;

/* FApp static interface
 *****************************************************************************/

FString FApp::GetBranchName( )
{
	return FString(TEXT(BRANCH_NAME));
}

EBuildConfigurations::Type FApp::GetBuildConfiguration()
{
#if UE_BUILD_DEBUG
	return EBuildConfigurations::Debug;

#elif UE_BUILD_DEVELOPMENT
	static bool bUsingDebugGame = FParse::Param(FCommandLine::Get(), TEXT("debug"));
	return bUsingDebugGame ? EBuildConfigurations::DebugGame : EBuildConfigurations::Development;

#elif UE_BUILD_SHIPPING || UI_BUILD_SHIPPING_EDITOR
	return EBuildConfigurations::Shipping;

#elif UE_BUILD_TEST
	return EBuildConfigurations::Test;

#else
	return EBuildConfigurations::Unknown;
#endif
}

FString FApp::GetBuildDate( )
{
	return FString(ANSI_TO_TCHAR(__DATE__));
}


void FApp::InitializeSession( )
{
	// parse session details on command line
	FString InstanceIdString;

	if (FParse::Value(FCommandLine::Get(), TEXT("-InstanceId="), InstanceIdString))
	{
		if (!FGuid::Parse(InstanceIdString, InstanceId))
		{
			UE_LOG(LogInit, Warning, TEXT("Invalid InstanceId on command line: %s"), *InstanceIdString);
		}
	}

	if (!InstanceId.IsValid())
	{
		InstanceId = FGuid::NewGuid();
	}

	FString SessionIdString;

	if (FParse::Value(FCommandLine::Get(), TEXT("-SessionId="), SessionIdString))
	{
		if (FGuid::Parse(SessionIdString, SessionId))
		{
			Standalone = false;
		}
		else
		{
			UE_LOG(LogInit, Warning, TEXT("Invalid SessionId on command line: %s"), *SessionIdString);
		}
	}

	FParse::Value(FCommandLine::Get(), TEXT("-SessionName="), SessionName);

	if (!FParse::Value(FCommandLine::Get(), TEXT("-SessionOwner="), SessionOwner))
	{
		SessionOwner = FPlatformProcess::UserName(false);
	}
}

bool FApp::IsInstalled()
{
#if UE_BUILD_SHIPPING && PLATFORM_DESKTOP && !UE_SERVER
	static bool bIsInstalled = !FParse::Param(FCommandLine::Get(), TEXT("NotInstalled"));
#else
	static bool bIsInstalled = FParse::Param(FCommandLine::Get(), TEXT("Installed"));
#endif
	return bIsInstalled;
}

bool FApp::IsEngineInstalled()
{
	static bool bIsInstalledEngine = IsInstalled() || (FRocketSupport::IsRocket() ? !FParse::Param(FCommandLine::Get(), TEXT("EngineNotInstalled")) : FParse::Param(FCommandLine::Get(), TEXT("EngineInstalled")));
	return bIsInstalledEngine;
}

bool FApp::ShouldUseThreadingForPerformance( )
{
	static bool OnlyOneThread = FParse::Param(FCommandLine::Get(), TEXT("ONETHREAD")) || IsRunningDedicatedServer() || !FPlatformProcess::SupportsMultithreading() || FPlatformMisc::NumberOfCores() < 2;
	return !OnlyOneThread;
}
