// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "GraphEditorCommon.h"
#include "SoundDefinitions.h"
#include "SGraphNodeSoundBase.h"
//#include "IDocumentation.h"
#include "ScopedTransaction.h"

/////////////////////////////////////////////////////
// SGraphNodeSoundBase

void SGraphNodeSoundBase::Construct(const FArguments& InArgs, class USoundCueGraphNode* InNode)
{
	this->GraphNode = InNode;
	this->SoundNode = InNode;

	this->SetCursor(EMouseCursor::CardinalCross);

	this->UpdateGraphNode();
}

void SGraphNodeSoundBase::CreateOutputSideAddButton(TSharedPtr<SVerticalBox> OutputBox)
{
	TSharedRef<SWidget> AddPinButton = AddPinButtonContent(
		NSLOCTEXT("SoundNode", "SoundNodeAddPinButton", "Add input"),
		NSLOCTEXT("SoundNode", "SoundNodeAddPinButton_Tooltip", "Adds an input to the sound node")
	);

	OutputBox->AddSlot()
	.AutoHeight()
	.VAlign(VAlign_Center)
	.Padding(10,10,10,4)
	[
		AddPinButton
	];
}

EVisibility SGraphNodeSoundBase::IsAddPinButtonVisible() const
{
	EVisibility Visibility = SGraphNode::IsAddPinButtonVisible();
	if (Visibility == EVisibility::Visible)
	{
		if (!SoundNode->CanAddInputPin())
		{
			Visibility = EVisibility::Collapsed;
		}
	}
	return Visibility;
}

FReply SGraphNodeSoundBase::OnAddPin()
{
	SoundNode->AddInputPin();

	return FReply::Handled();
}
