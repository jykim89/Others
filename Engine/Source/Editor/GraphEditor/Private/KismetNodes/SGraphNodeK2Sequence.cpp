// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "GraphEditorCommon.h"
#include "SGraphNodeK2Base.h"
#include "SGraphNodeK2Sequence.h"


void SGraphNodeK2Sequence::Construct( const FArguments& InArgs, UK2Node* InNode )
{
	GraphNode = InNode;

	SetCursor( EMouseCursor::CardinalCross );

	UpdateGraphNode();
}

void SGraphNodeK2Sequence::CreateOutputSideAddButton(TSharedPtr<SVerticalBox> OutputBox)
{
	TSharedRef<SWidget> AddPinButton = AddPinButtonContent(
		NSLOCTEXT("SequencerNode", "SequencerNodeAddPinButton", "Add pin"),
		NSLOCTEXT("SequencerNode", "SequencerNodeAddPinButton_ToolTip", "Add new pin"));

	OutputBox->AddSlot()
	.AutoHeight()
	.VAlign(VAlign_Center)
	.Padding(10,10,10,4)
	[
		AddPinButton
	];
}

FReply SGraphNodeK2Sequence::OnAddPin()
{
	if (UK2Node_ExecutionSequence* SequenceNode = Cast<UK2Node_ExecutionSequence>(GraphNode))
	{
		SequenceNode->AddPinToExecutionNode();
		UpdateGraphNode();
		GraphNode->GetGraph()->NotifyGraphChanged();
	}
	else if (UK2Node_MakeArray* MakeArrayNode = Cast<UK2Node_MakeArray>(GraphNode))
	{
		MakeArrayNode->AddInputPin();
		UpdateGraphNode();
		GraphNode->GetGraph()->NotifyGraphChanged();
	}
	else if (UK2Node_CommutativeAssociativeBinaryOperator* OperatorNode = Cast<UK2Node_CommutativeAssociativeBinaryOperator>(GraphNode))
	{
		OperatorNode->AddInputPin();
		UpdateGraphNode();
		GraphNode->GetGraph()->NotifyGraphChanged();
	}

	return FReply::Handled();
}