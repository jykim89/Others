// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphPrivatePCH.h"
#include "AnimGraphNode_StateMachine.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_StateMachine

UAnimGraphNode_StateMachine::UAnimGraphNode_StateMachine(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	bCanRenameNode = true;
}
