// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphPrivatePCH.h"
#include "AnimGraphNode_RotationOffsetBlendSpace.h"
#include "GraphEditorActions.h"
#include "CompilerResultsLog.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_RotationOffsetBlendSpace

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_RotationOffsetBlendSpace::UAnimGraphNode_RotationOffsetBlendSpace(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

FString UAnimGraphNode_RotationOffsetBlendSpace::GetTooltip() const
{
	return FString::Printf(TEXT("AimOffset %s"), *(Node.BlendSpace->GetPathName()));
}

FText UAnimGraphNode_RotationOffsetBlendSpace::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	const FText BlendSpaceName((Node.BlendSpace != NULL) ? FText::FromString(Node.BlendSpace->GetName()) : LOCTEXT("None", "(None)"));

	FFormatNamedArguments Args;
	Args.Add(TEXT("BlendSpaceName"), BlendSpaceName);

	if (TitleType == ENodeTitleType::ListView)
	{
		return FText::Format(LOCTEXT("AimOffsetListTitle", "AimOffset '{BlendSpaceName}'"), Args);
	}
	else
	{
		return FText::Format(LOCTEXT("AimOffsetFullTitle", "{BlendSpaceName}\nAimOffset"), Args);
	}
}

FString UAnimGraphNode_RotationOffsetBlendSpace::GetNodeNativeTitle(ENodeTitleType::Type TitleType) const
{
	// Do not setup this function for localization, intentionally left unlocalized!
	
	const FString BlendSpaceName((Node.BlendSpace != NULL) ? *(Node.BlendSpace->GetName()) : TEXT("(None)"));

	if (TitleType == ENodeTitleType::ListView)
	{
		return FString::Printf(TEXT("AimOffset '%s'"), *BlendSpaceName);
	}
	else
	{
		return FString::Printf(TEXT("%s\nAimOffset"), *BlendSpaceName);
	}
}

void UAnimGraphNode_RotationOffsetBlendSpace::GetMenuEntries(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	const bool bWantAimOffsets = true;
	GetBlendSpaceEntries(bWantAimOffsets, ContextMenuBuilder);
}

void UAnimGraphNode_RotationOffsetBlendSpace::ValidateAnimNodeDuringCompilation(class USkeleton* ForSkeleton, class FCompilerResultsLog& MessageLog)
{
	if (Node.BlendSpace == NULL)
	{
		MessageLog.Error(TEXT("@@ references an unknown blend space"), this);
	}
	else if (Cast<UAimOffsetBlendSpace>(Node.BlendSpace) == NULL &&
			 Cast<UAimOffsetBlendSpace1D>(Node.BlendSpace) == NULL)
	{
		MessageLog.Error(TEXT("@@ references an invalid blend space (one that is not an aim offset)"), this);
	}
	else
	{
		USkeleton * BlendSpaceSkeleton = Node.BlendSpace->GetSkeleton();
		if (BlendSpaceSkeleton && // if blend space doesn't have skeleton, it might be due to blend space not loaded yet, @todo: wait with anim blueprint compilation until all assets are loaded?
			!BlendSpaceSkeleton->IsCompatible(ForSkeleton))
		{
			MessageLog.Error(TEXT("@@ references blendspace that uses different skeleton @@"), this, BlendSpaceSkeleton);
		}
	}
}

void UAnimGraphNode_RotationOffsetBlendSpace::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	if (!Context.bIsDebugging)
	{
		// add an option to convert to single frame
		Context.MenuBuilder->BeginSection("AnimGraphNodeBlendSpacePlayer", NSLOCTEXT("A3Nodes", "BlendSpaceHeading", "Blend Space"));
		{
			Context.MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().OpenRelatedAsset);
		}
		Context.MenuBuilder->EndSection();
	}
}

#undef LOCTEXT_NAMESPACE