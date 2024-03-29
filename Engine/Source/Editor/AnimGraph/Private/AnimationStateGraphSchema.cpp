// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphPrivatePCH.h"
#include "BlueprintUtilities.h"
#include "GraphEditorActions.h"
#include "ScopedTransaction.h"
#include "AssetData.h"
#include "AnimationStateGraphSchema.h"
#include "AnimationStateGraph.h"
#include "AnimStateNode.h"
#include "AnimGraphNode_StateResult.h"
#define LOCTEXT_NAMESPACE "AnimationStateGraphSchema"


/////////////////////////////////////////////////////
// UAnimationStateGraphSchema

UAnimationStateGraphSchema::UAnimationStateGraphSchema(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

void UAnimationStateGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	// Create the result Create
	FGraphNodeCreator<UAnimGraphNode_StateResult> NodeCreator(Graph);
	UAnimGraphNode_StateResult* ResultSinkNode = NodeCreator.CreateNode();
	NodeCreator.Finalize();

	UAnimationStateGraph* TypedGraph = CastChecked<UAnimationStateGraph>(&Graph);
	TypedGraph->MyResultNode = ResultSinkNode;
}

void UAnimationStateGraphSchema::GetGraphDisplayInformation(const UEdGraph& Graph, /*out*/ FGraphDisplayInfo& DisplayInfo) const
{
	DisplayInfo.PlainName = FText::FromString( Graph.GetName() );

	if (const UAnimStateNode* StateNode = Cast<const UAnimStateNode>(Graph.GetOuter()))
	{
		DisplayInfo.PlainName = FText::Format( LOCTEXT("StateNameGraphTitle", "{0} (state)"), FText::FromString( StateNode->GetStateName() ) );
	}

	DisplayInfo.DisplayName = DisplayInfo.PlainName;
}

#undef LOCTEXT_NAMESPACE
