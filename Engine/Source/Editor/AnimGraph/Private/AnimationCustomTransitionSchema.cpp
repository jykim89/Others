// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphPrivatePCH.h"
#include "AnimationCustomTransitionSchema.h"
#include "AnimGraphNode_CustomTransitionResult.h"
#include "AnimGraphNode_TransitionPoseEvaluator.h"
#include "AnimStateTransitionNode.h"
#include "AnimationCustomTransitionGraph.h"

/////////////////////////////////////////////////////
// UAnimationCustomTransitionSchema

UAnimationCustomTransitionSchema::UAnimationCustomTransitionSchema(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

void UAnimationCustomTransitionSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	// Create the result node
	FGraphNodeCreator<UAnimGraphNode_CustomTransitionResult> ResultNodeCreator(Graph);
	UAnimGraphNode_CustomTransitionResult* ResultSinkNode = ResultNodeCreator.CreateNode();
	ResultSinkNode->NodePosX = 0;
	ResultSinkNode->NodePosY = 0;
	ResultNodeCreator.Finalize();

	UAnimationCustomTransitionGraph* TypedGraph = CastChecked<UAnimationCustomTransitionGraph>(&Graph);
	TypedGraph->MyResultNode = ResultSinkNode;

	// Create the source and destination input states
	{
		FGraphNodeCreator<UAnimGraphNode_TransitionPoseEvaluator> SourceNodeCreator(Graph);
		UAnimGraphNode_TransitionPoseEvaluator* SourcePoseNode = SourceNodeCreator.CreateNode();
		SourcePoseNode->Node.DataSource = EEvaluatorDataSource::EDS_SourcePose;
		SourcePoseNode->NodePosX = -300;
		SourcePoseNode->NodePosY = -150;
		SourceNodeCreator.Finalize();

	}
	{
		FGraphNodeCreator<UAnimGraphNode_TransitionPoseEvaluator> DestinationNodeCreator(Graph);
		UAnimGraphNode_TransitionPoseEvaluator* DestinationPoseNode = DestinationNodeCreator.CreateNode();
		DestinationPoseNode->Node.DataSource = EEvaluatorDataSource::EDS_DestinationPose;
		DestinationPoseNode->NodePosX = -300;
		DestinationPoseNode->NodePosY = 150;
		DestinationNodeCreator.Finalize();
	}
}

void UAnimationCustomTransitionSchema::GetGraphDisplayInformation(const UEdGraph& Graph, /*out*/ FGraphDisplayInfo& DisplayInfo) const
{
	DisplayInfo.PlainName = FText::FromString( Graph.GetName() );

	if (const UAnimStateTransitionNode* TransNode = Cast<const UAnimStateTransitionNode>(Graph.GetOuter()))
	{
		DisplayInfo.PlainName = FText::Format( NSLOCTEXT("Animation", "CustomBlendGraphTitle", "{0} (custom blend)"), TransNode->GetNodeTitle(ENodeTitleType::FullTitle) );
	}

	DisplayInfo.DisplayName = DisplayInfo.PlainName;
}
