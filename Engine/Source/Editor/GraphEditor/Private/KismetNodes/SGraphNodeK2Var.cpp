// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.



#include "GraphEditorCommon.h"
#include "SGraphNodeK2Base.h"
#include "SGraphNodeK2Var.h"
#include "ClassIconFinder.h"
#include "IDocumentation.h"

void SGraphNodeK2Var::Construct( const FArguments& InArgs, UK2Node* InNode )
{
	this->GraphNode = InNode;

	this->SetCursor( EMouseCursor::CardinalCross );

	this->UpdateGraphNode();
}

FSlateColor SGraphNodeK2Var::GetVariableColor() const
{
	return GraphNode->GetNodeTitleColor();
}

void SGraphNodeK2Var::UpdateGraphNode()
{
	InputPins.Empty();
	OutputPins.Empty();

	// Reset variables that are going to be exposed, in case we are refreshing an already setup node.
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	FText TitleText;
	bool bPadTitle = false;
	float HorizontalTitleMargin = 0.0f;
	float VerticalTitleMargin = 8.0f;
	FMargin TitleMargin = FMargin(0.0f, 8.0f);
	EHorizontalAlignment TitleHAlign = HAlign_Center;
	TSharedPtr<SWidget> TitleWidget;

	if (GraphNode->IsA(UK2Node_VariableSet::StaticClass()))
	{
		UK2Node_VariableSet *SetNode = Cast<UK2Node_VariableSet>(GraphNode);
		if(SetNode->HasLocalRepNotify())
		{
			TitleText = NSLOCTEXT("GraphEditor", "VariableSetWithNotify", "SET w/ Notify");
			//+ FString::Printf(TEXT("\n(%s)"), *SetNode->GetRepNotifyName().ToString());
		}
		else
		{
			TitleText = NSLOCTEXT("GraphEditor", "VariableSet", "SET");
		}
	}
	else if (UK2Node_StructOperation* StructOp = Cast<UK2Node_StructOperation>(GraphNode))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("VariableName"), StructOp->GetVarNameText());
		if (GraphNode->IsA(UK2Node_StructMemberGet::StaticClass()))
		{
			TitleText = FText::Format(NSLOCTEXT("GraphEditor", "StructMemberGet", "Get in {VariableName}"), Args);
		}
		else if (GraphNode->IsA(UK2Node_StructMemberSet::StaticClass()))
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("VariableName"), StructOp->GetVarNameText());
			TitleText = FText::Format(NSLOCTEXT("GraphEditor", "StructMemberSet", "Set in {VariableName}"), Args);
		}
		else if (GraphNode->IsA(UK2Node_MakeStruct::StaticClass()))
		{
			TitleText = FText::Format(NSLOCTEXT("GraphEditor", "MakeStruct", "Make {VariableName}"), Args);
		}
		else
		{
			check(false);
		}
		bPadTitle = true;
		HorizontalTitleMargin = 12.0f;
		TitleMargin = FMargin(12.0f, VerticalTitleMargin);
	}
	else if (UK2Node_Literal* LiteralRef = Cast<UK2Node_Literal>(GraphNode))
	{
		FText SubTitleText;

		// Get the name of the level the object is in.
		if(AActor* Actor = Cast<AActor>(LiteralRef->GetObjectRef()))
		{
			FText LevelName;

			if(ULevel* Level = Actor->GetLevel())
			{
				if ( Level->IsPersistentLevel() )
				{
					LevelName = NSLOCTEXT("GraphEditor", "PersistentTag", "Persistent Level");
				}
				else
				{
					LevelName = FText::FromString(FPaths::GetCleanFilename(Actor->GetOutermost()->GetName()));
				}
			}
			SubTitleText = FText::Format(NSLOCTEXT("GraphEditor", "ActorRef", "from {0}"), LevelName);
		}

		TitleText = GraphNode->GetNodeTitle(ENodeTitleType::ListView);

		TitleHAlign = HAlign_Left;
		TitleMargin = FMargin(12.0f, VerticalTitleMargin, 32.0f, 2.0f);

		TitleWidget = 
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Top)
				.AutoWidth()
			[
				SNew(SImage)
					.Image( FClassIconFinder::FindIconForClass( LiteralRef->GetObjectRef() ? LiteralRef->GetObjectRef()->GetClass() : NULL ))
			]

			+SHorizontalBox::Slot()
				.Padding(2.0f, 0.0f, 0.0f, 0.0f)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Top)
				.AutoWidth()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
					.VAlign(VAlign_Top)
					.AutoHeight()
				[
					SNew(STextBlock)
						.WrapTextAt(128.0f)
						.TextStyle( FEditorStyle::Get(), "Graph.Node.NodeTitle" )
						.Text(TitleText)
				]

				+SVerticalBox::Slot()
					.VAlign(VAlign_Top)
					.AutoHeight()
				[
					SNew(STextBlock)
						.Visibility(TitleText.IsEmpty()? EVisibility::Collapsed : EVisibility::Visible)
						.WrapTextAt(128.0f)
						.TextStyle( FEditorStyle::Get(), "Graph.Node.NodeTitleExtraLines" )
						.Text(SubTitleText)
				]
			];
	}
	else
	{
		TitleWidget = SNullWidget::NullWidget;
	}

	if(!TitleWidget.IsValid())
	{
		TitleWidget = SNew(STextBlock)
						.TextStyle( FEditorStyle::Get(), "Graph.Node.NodeTitle" )
						.Text(TitleText);
	}

	TSharedPtr<SWidget> ErrorText = SetupErrorReporting();

	//             ________________
	//            | (>) L |  R (>) |
	//            | (>) E |  I (>) |
	//            | (>) F |  G (>) |
	//            | (>) T |  H (>) |
	//            |       |  T (>) |
	//            |_______|________|
	//
	this->ContentScale.Bind( this, &SGraphNode::GetContentScale );
	this->ChildSlot
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.VAlign(VAlign_Top)
		.AutoHeight() 
		.Padding( FMargin(5.0f, 1.0f) )
		[
			ErrorText->AsShared()
		]
		+SVerticalBox::Slot()
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Image( FEditorStyle::GetBrush("Graph.VarNode.Body") )
			]
			+ SOverlay::Slot()
			.VAlign(VAlign_Top)
			[
				SNew(SImage)
				.Image( FEditorStyle::GetBrush("Graph.VarNode.ColorSpill") )
				.ColorAndOpacity( this, &SGraphNodeK2Var::GetVariableColor )
			]
			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Image( FEditorStyle::GetBrush("Graph.VarNode.Gloss") )
			]
			+SOverlay::Slot()
			.VAlign(VAlign_Top)
			.HAlign(TitleHAlign)
			.Padding( TitleMargin )
			[
				TitleWidget.ToSharedRef()
			]
			+ SOverlay::Slot()
			.Padding( FMargin(0,4) )
			[
				// NODE CONTENT AREA
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.FillWidth(1.0f)
				.Padding( FMargin(2,0) )
				[
					// LEFT
					SAssignNew(LeftNodeBox, SVerticalBox)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.Padding( FMargin(2,0) )
				[
					// RIGHT
					SAssignNew(RightNodeBox, SVerticalBox)
				]
			]
		]
	];

	// Add padding widgets at the top of the pin boxes if it's a struct operation with a long title
	if (bPadTitle)
	{
		LeftNodeBox->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(SSpacer).Size(FVector2D(0.0f, 16.0f))
			];
		RightNodeBox->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(SSpacer).Size(FVector2D(0.0f, 16.0f))
			];
	}

	// Create widgets for each of the real pins
	CreatePinWidgets();
}

const FSlateBrush* SGraphNodeK2Var::GetShadowBrush(bool bSelected) const
{
	return bSelected ? FEditorStyle::GetBrush(TEXT("Graph.VarNode.ShadowSelected")) : FEditorStyle::GetBrush(TEXT("Graph.VarNode.Shadow"));
}