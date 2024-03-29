// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"

#if WITH_EDITOR
#include "MessageLog.h"
#include "UObjectToken.h"
#endif

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// USlateNonLeafWidgetComponent

USlateNonLeafWidgetComponent::USlateNonLeafWidgetComponent(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	PrimaryComponentTick.bCanEverTick = true;

	bAutoActivate = true;
	bTickInEditor = true;
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
