// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved. 
#include "ScriptPluginPrivatePCH.h"

//////////////////////////////////////////////////////////////////////////

AScriptTestActor::AScriptTestActor(const FPostConstructInitializeProperties& PCIP)
	: Super( PCIP )
{
}

float AScriptTestActor::TestFunction(float InValue, float InFactor, bool bMultiply)
{
	if (bMultiply)
	{
		return InValue * InFactor;
	}
	else
	{
		return InValue / InFactor;
	}
}