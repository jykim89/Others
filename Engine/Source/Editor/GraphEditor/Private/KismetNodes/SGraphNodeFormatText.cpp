// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "GraphEditorCommon.h"
#include "SGraphNodeK2Base.h"
#include "SGraphNodeFormatText.h"
#include "KismetPins/SGraphPinExec.h"
#include "NodeFactory.h"

#include "ScopedTransaction.h"
#include "BlueprintEditorUtils.h"

//////////////////////////////////////////////////////////////////////////
// SGraphNodeFormatText

void SGraphNodeFormatText::Construct(const FArguments& InArgs, UK2Node_FormatText* InNode)
{
	this->GraphNode = InNode;

	this->SetCursor( EMouseCursor::CardinalCross );

	this->UpdateGraphNode();
}

void SGraphNodeFormatText::CreatePinWidgets()
{
	// Create Pin widgets for each of the pins, except for the default pin
	for (auto PinIt = GraphNode->Pins.CreateConstIterator(); PinIt; ++PinIt)
	{
		UEdGraphPin* CurrentPin = *PinIt;
		if ((!CurrentPin->bHidden))
		{
			TSharedPtr<SGraphPin> NewPin = FNodeFactory::CreatePinWidget(CurrentPin);
			check(NewPin.IsValid());
			NewPin->SetIsEditable(IsEditable);

			this->AddPin(NewPin.ToSharedRef());
		}
	}
}

void SGraphNodeFormatText::CreateInputSideAddButton(TSharedPtr<SVerticalBox> InputBox)
{
	TSharedRef<SWidget> AddPinButton = AddPinButtonContent(
										NSLOCTEXT("FormatTextNode", "FormatTextNodeAddPinButton", "Add pin"),
										NSLOCTEXT("FormatTextNode", "FormatTextNodeAddPinButton_Tooltip", "Adds an argument to the node"),
										false);

	InputBox->AddSlot()
	.AutoHeight()
	.VAlign(VAlign_Center)
	.Padding(10,10,10,4)
	[
		AddPinButton
	];
}

EVisibility SGraphNodeFormatText::IsAddPinButtonVisible() const
{
	EVisibility VisibilityState = EVisibility::Collapsed;
	if(NULL != Cast<UK2Node_FormatText>(GraphNode))
	{
		VisibilityState = SGraphNode::IsAddPinButtonVisible();
		if(VisibilityState == EVisibility::Visible)
		{
			UK2Node_FormatText* FormatNode = CastChecked<UK2Node_FormatText>(GraphNode);
			VisibilityState = FormatNode->CanEditArguments()? EVisibility::Visible : EVisibility::Collapsed;
		}
	}
	return VisibilityState;
}

FReply SGraphNodeFormatText::OnAddPin()
{
	if(UK2Node_FormatText* FormatText = Cast<UK2Node_FormatText>(GraphNode))
	{
		FormatText->AddArgumentPin();
	}
	return FReply::Handled();
}