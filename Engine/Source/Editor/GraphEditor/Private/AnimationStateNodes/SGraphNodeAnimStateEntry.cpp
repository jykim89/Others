// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "GraphEditorCommon.h"
#include "SGraphNodeAnimStateEntry.h"
#include "SGraphPreviewer.h"
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"
#include "NodeFactory.h"
#include "AnimStateEntryNode.h"

/////////////////////////////////////////////////////
// SGraphNodeAnimStateEntry

void SGraphNodeAnimStateEntry::Construct(const FArguments& InArgs, UAnimStateEntryNode* InNode)
{
	this->GraphNode = InNode;

	this->SetCursor(EMouseCursor::CardinalCross);

	this->UpdateGraphNode();
}

void SGraphNodeAnimStateEntry::GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const
{

}

FSlateColor SGraphNodeAnimStateEntry::GetBorderBackgroundColor() const
{
	FLinearColor InactiveStateColor(0.08f, 0.08f, 0.08f);
	FLinearColor ActiveStateColorDim(0.4f, 0.3f, 0.15f);
	FLinearColor ActiveStateColorBright(1.f, 0.6f, 0.35f);

	return InactiveStateColor;
}

void SGraphNodeAnimStateEntry::UpdateGraphNode()
{
	InputPins.Empty();
	OutputPins.Empty();
	
	// Reset variables that are going to be exposed, in case we are refreshing an already setup node.
	RightNodeBox.Reset();
	LeftNodeBox.Reset();


	FLinearColor TitleShadowColor(0.6f, 0.6f, 0.6f);

	this->ContentScale.Bind( this, &SGraphNode::GetContentScale );
	this->ChildSlot
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush( "Graph.StateNode.Body" ) )
			.Padding(0)
			.BorderBackgroundColor( this, &SGraphNodeAnimStateEntry::GetBorderBackgroundColor )
			[
				SNew(SOverlay)

				// PIN AREA
				+SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.Padding(10.0f)
				[
					SAssignNew(RightNodeBox, SVerticalBox)
				]
			]			
		];

	CreatePinWidgets();
}

void SGraphNodeAnimStateEntry::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
{
	PinToAdd->SetOwner( SharedThis(this) );
	RightNodeBox->AddSlot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.FillHeight(1.0f)
		[
			PinToAdd
		];
	OutputPins.Add(PinToAdd);
}

FString SGraphNodeAnimStateEntry::GetPreviewCornerText() const
{
	return FString(TEXT("Entry point for state machine"));
}
