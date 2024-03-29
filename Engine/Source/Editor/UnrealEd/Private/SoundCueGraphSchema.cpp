// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SoundCueGraphSchema.cpp
=============================================================================*/

#include "UnrealEd.h"
#include "Slate.h"
#include "AssetData.h"
#include "GraphEditorActions.h"
#include "SoundDefinitions.h"
#include "SoundCueEditorUtilities.h"
#include "ScopedTransaction.h"
#include "GraphEditor.h"

#define LOCTEXT_NAMESPACE "SoundCueSchema"

TArray<UClass*> USoundCueGraphSchema::SoundNodeClasses;
bool USoundCueGraphSchema::bSoundNodeClassesInitialized = false;

/////////////////////////////////////////////////////
// FSoundCueGraphSchemaAction_NewNode

UEdGraphNode* FSoundCueGraphSchemaAction_NewNode::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	check(SoundNodeClass);

	USoundCue* SoundCue = CastChecked<USoundCueGraph>(ParentGraph)->GetSoundCue();
	const FScopedTransaction Transaction( LOCTEXT("SoundCueEditorNewSoundNode", "Sound Cue Editor: New Sound Node") );
	ParentGraph->Modify();
	SoundCue->Modify();

	USoundNode* NewNode = SoundCue->ConstructSoundNode<USoundNode>(SoundNodeClass, bSelectNewNode);

	// If this node allows >0 children but by default has zero - create a connector for starters
	if (NewNode->GetMaxChildNodes() > 0 && NewNode->ChildNodes.Num() == 0)
	{
		NewNode->CreateStartingConnectors();
	}

	// Attempt to connect inputs to selected nodes, unless we're already dragging from a single output
	if (FromPin == NULL || FromPin->Direction == EGPD_Input)
	{
		ConnectToSelectedNodes(NewNode, ParentGraph);
	}

	NewNode->GraphNode->NodePosX = Location.X;
	NewNode->GraphNode->NodePosY = Location.Y;

	NewNode->GraphNode->AutowireNewNode(FromPin);

	SoundCue->PostEditChange();
	SoundCue->MarkPackageDirty();

	return NewNode->GraphNode;
}

void FSoundCueGraphSchemaAction_NewNode::ConnectToSelectedNodes(USoundNode* NewNode, class UEdGraph* ParentGraph) const
{
	// only connect if node can have many children
	if (NewNode->GetMaxChildNodes() > 1)
	{
		const FGraphPanelSelectionSet SelectedNodes = FSoundCueEditorUtilities::GetSelectedNodes(ParentGraph);

		TArray<USoundNode*> SortedNodes;
		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			USoundCueGraphNode* SelectedNode = Cast<USoundCueGraphNode>(*NodeIt);

			if (SelectedNode)
			{
				// Sort the nodes by y position
				bool bInserted = false;
				for (int32 Index = 0; Index < SortedNodes.Num(); ++Index)
				{
					if (SortedNodes[Index]->GraphNode->NodePosY > SelectedNode->NodePosY)
					{
						SortedNodes.Insert(SelectedNode->SoundNode, Index);
						bInserted = true;
						break;
					}
				}
				if (!bInserted)
				{
					SortedNodes.Add(SelectedNode->SoundNode);
				}
			}
		}
		if (SortedNodes.Num() > 1)
		{
			CastChecked<USoundCueGraphSchema>(NewNode->GraphNode->GetSchema())->TryConnectNodes(SortedNodes, NewNode);
		}
	}
}

/////////////////////////////////////////////////////
// FSoundCueGraphSchemaAction_NewFromSelected

UEdGraphNode* FSoundCueGraphSchemaAction_NewFromSelected::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	USoundCue* SoundCue = CastChecked<USoundCueGraph>(ParentGraph)->GetSoundCue();
	const FScopedTransaction Transaction( LOCTEXT("SoundCueEditorNewFromSelection", "Sound Cue Editor: New From Selection") );
	ParentGraph->Modify();
	SoundCue->Modify();

	UEdGraphNode* CreatedNode = NULL;

	FVector2D WaveStartLocation = Location;

	if (SoundNodeClass)
	{
		// If we will create another node, move wave nodes out of the way.
		WaveStartLocation.X -= 200;
	}

	TArray<USoundWave*> SelectedWaves;
	TArray<USoundNode*> CreatedPlayers;
	GEditor->GetSelectedObjects()->GetSelectedObjects<USoundWave>(SelectedWaves);
	FSoundCueEditorUtilities::CreateWaveContainers(SelectedWaves, SoundCue, CreatedPlayers, WaveStartLocation);

	if (SoundNodeClass)
	{
		USoundNode* NewNode = SoundCue->ConstructSoundNode<USoundNode>(SoundNodeClass, bSelectNewNode);
		USoundCueGraphNode* NewGraphNode = NewNode->GraphNode;
		const USoundCueGraphSchema* NewSchema = CastChecked<USoundCueGraphSchema>(NewGraphNode->GetSchema());

		// If this node allows >0 children but by default has zero - create a connector for starters
		if (NewNode->GetMaxChildNodes() > 0 && NewNode->ChildNodes.Num() == 0)
		{
			NewNode->CreateStartingConnectors();
		}

		NewSchema->TryConnectNodes(CreatedPlayers, NewNode);

		NewGraphNode->NodePosX = Location.X;
		NewGraphNode->NodePosY = Location.Y;

		CreatedNode = NewNode->GraphNode;
	}
	else
	{
		if (CreatedPlayers.Num() > 0)
		{
			CreatedNode = CreatedPlayers[0]->GraphNode;
		}
	}

	if (CreatedNode)
	{
		CreatedNode->AutowireNewNode(FromPin);
	}

	SoundCue->PostEditChange();
	SoundCue->MarkPackageDirty();

	return CreatedNode;
}

/////////////////////////////////////////////////////
// FSoundCueGraphSchemaAction_NewComment

UEdGraphNode* FSoundCueGraphSchemaAction_NewComment::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	// Add menu item for creating comment boxes
	UEdGraphNode_Comment* CommentTemplate = NewObject<UEdGraphNode_Comment>();

	FVector2D SpawnLocation = Location;

	FSlateRect Bounds;
	if (FSoundCueEditorUtilities::GetBoundsForSelectedNodes(ParentGraph, Bounds, 50.0f))
	{
		CommentTemplate->SetBounds(Bounds);
		SpawnLocation.X = CommentTemplate->NodePosX;
		SpawnLocation.Y = CommentTemplate->NodePosY;
	}

	return FEdGraphSchemaAction_NewNode::SpawnNodeFromTemplate<UEdGraphNode_Comment>(ParentGraph, CommentTemplate, SpawnLocation);
}

/////////////////////////////////////////////////////
// FSoundCueGraphSchemaAction_Paste

UEdGraphNode* FSoundCueGraphSchemaAction_Paste::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	FSoundCueEditorUtilities::PasteNodesHere(ParentGraph, Location);
	return NULL;
}

/////////////////////////////////////////////////////
// USoundCueGraphSchema

USoundCueGraphSchema::USoundCueGraphSchema(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

bool USoundCueGraphSchema::ConnectionCausesLoop(const UEdGraphPin* InputPin, const UEdGraphPin* OutputPin) const
{
	USoundCueGraphNode* InputNode = Cast<USoundCueGraphNode>(InputPin->GetOwningNode());

	if (InputNode)
	{
		// Only nodes representing SoundNodes have outputs
		USoundCueGraphNode* OutputNode = CastChecked<USoundCueGraphNode>(OutputPin->GetOwningNode());

		// Grab all child nodes. We can't just test the output because 
		// the loop could happen from any additional child nodes. 
		TArray<USoundNode*> Nodes;
		OutputNode->SoundNode->GetAllNodes(Nodes);

		// If our test input is in that set, return true.
		return Nodes.Contains(InputNode->SoundNode);
	}

	// Simple connection to root node
	return false;
}

void USoundCueGraphSchema::GetPaletteActions(FGraphActionMenuBuilder& ActionMenuBuilder) const
{
	GetAllSoundNodeActions(ActionMenuBuilder, false);
	GetCommentAction(ActionMenuBuilder);
}

void USoundCueGraphSchema::TryConnectNodes(const TArray<USoundNode*>& OutputNodes, USoundNode* InputNode) const
{
	for (int32 Index = 0; Index < OutputNodes.Num(); Index++)
	{
		if ( Index < InputNode->GetMaxChildNodes() )
		{
			if (Index >= InputNode->GraphNode->GetInputCount())
			{
				InputNode->GraphNode->CreateInputPin();
			}
			TryCreateConnection( InputNode->GraphNode->GetInputPin(Index), OutputNodes[Index]->GraphNode->GetOutputPin() );
		}
	}
}

void USoundCueGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	GetAllSoundNodeActions(ContextMenuBuilder, true);

	GetCommentAction(ContextMenuBuilder, ContextMenuBuilder.CurrentGraph);

	if (!ContextMenuBuilder.FromPin && FSoundCueEditorUtilities::CanPasteNodes(ContextMenuBuilder.CurrentGraph))
	{
		TSharedPtr<FSoundCueGraphSchemaAction_Paste> NewAction( new FSoundCueGraphSchemaAction_Paste(TEXT(""), LOCTEXT("PasteHereAction", "Paste here"), TEXT(""), 0) );
		ContextMenuBuilder.AddAction( NewAction );
	}
}

void USoundCueGraphSchema::GetContextMenuActions(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, class FMenuBuilder* MenuBuilder, bool bIsDebugging) const
{
	if (InGraphPin)
	{
		MenuBuilder->BeginSection("SoundCueGraphSchemaPinActions", LOCTEXT("PinActionsMenuHeader", "Pin Actions"));
		{
			// Only display the 'Break Link' option if there is a link to break!
			if (InGraphPin->LinkedTo.Num() > 0)
			{
				MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().BreakPinLinks );
			}
		}
		MenuBuilder->EndSection();
	}
	else if (InGraphNode)
	{
		const USoundCueGraphNode* SoundGraphNode = Cast<const USoundCueGraphNode>(InGraphNode);

		MenuBuilder->BeginSection("SoundCueGraphSchemaNodeActions", LOCTEXT("NodeActionsMenuHeader", "Node Actions"));
		{
			MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().BreakNodeLinks);
		}
		MenuBuilder->EndSection();
	}

	Super::GetContextMenuActions(CurrentGraph, InGraphNode, InGraphPin, MenuBuilder, bIsDebugging);
}

void USoundCueGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	const int32 RootNodeHeightOffset = -58;

	// Create the result node
	FGraphNodeCreator<USoundCueGraphNode_Root> NodeCreator(Graph);
	USoundCueGraphNode_Root* ResultRootNode = NodeCreator.CreateNode();
	ResultRootNode->NodePosY = RootNodeHeightOffset;
	NodeCreator.Finalize();
}

const FPinConnectionResponse USoundCueGraphSchema::CanCreateConnection(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const
{
	// Make sure the pins are not on the same node
	if (PinA->GetOwningNode() == PinB->GetOwningNode())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionSameNode", "Both are on the same node"));
	}

	// Compare the directions
	const UEdGraphPin* InputPin = NULL;
	const UEdGraphPin* OutputPin = NULL;

	if (!CategorizePinsByDirection(PinA, PinB, /*out*/ InputPin, /*out*/ OutputPin))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionIncompatible", "Directions are not compatible"));
	}

	if (ConnectionCausesLoop(InputPin, OutputPin))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionLoop", "Connection would cause loop"));
	}

	// Break existing connections on inputs only - multiple output connections are acceptable
	if (InputPin->LinkedTo.Num() > 0)
	{
		ECanCreateConnectionResponse ReplyBreakOutputs;
		if (InputPin == PinA)
		{
			ReplyBreakOutputs = CONNECT_RESPONSE_BREAK_OTHERS_A;
		}
		else
		{
			ReplyBreakOutputs = CONNECT_RESPONSE_BREAK_OTHERS_B;
		}
		return FPinConnectionResponse(ReplyBreakOutputs, LOCTEXT("ConnectionReplace", "Replace existing connections"));
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, TEXT(""));
}

bool USoundCueGraphSchema::TryCreateConnection(UEdGraphPin* PinA, UEdGraphPin* PinB) const
{
	bool bModified = UEdGraphSchema::TryCreateConnection(PinA, PinB);

	if (bModified)
	{
		CastChecked<USoundCueGraph>(PinA->GetOwningNode()->GetGraph())->GetSoundCue()->CompileSoundNodesFromGraphNodes();
	}

	return bModified;
}

bool USoundCueGraphSchema::ShouldHidePinDefaultValue(UEdGraphPin* Pin) const
{
	return true;
}

FLinearColor USoundCueGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return FLinearColor::White;
}

void USoundCueGraphSchema::BreakNodeLinks(UEdGraphNode& TargetNode) const
{
	Super::BreakNodeLinks(TargetNode);

	CastChecked<USoundCueGraph>(TargetNode.GetGraph())->GetSoundCue()->CompileSoundNodesFromGraphNodes();
}

void USoundCueGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const
{
	const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "GraphEd_BreakPinLinks", "Break Pin Links") );

	Super::BreakPinLinks(TargetPin, bSendsNodeNotifcation);

	// if this would notify the node then we need to compile the SoundCue
	if (bSendsNodeNotifcation)
	{
		CastChecked<USoundCueGraph>(TargetPin.GetOwningNode()->GetGraph())->GetSoundCue()->CompileSoundNodesFromGraphNodes();
	}
}

void USoundCueGraphSchema::DroppedAssetsOnGraph(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraph* Graph) const
{
	TArray<USoundWave*> Waves;
	for (int32 AssetIdx = 0; AssetIdx < Assets.Num(); ++AssetIdx)
	{
		USoundWave* SoundWav = Cast<USoundWave>(Assets[AssetIdx].GetAsset());
		if (SoundWav)
		{
			Waves.Add(SoundWav);
		}
	}

	if (Waves.Num() > 0)
	{
		const FScopedTransaction Transaction( LOCTEXT("SoundCueEditorDropWave", "Sound Cue Editor: Drag and Drop Sound Wave") );

		USoundCueGraph* SoundCueGraph = CastChecked<USoundCueGraph>(Graph);
		USoundCue* SoundCue = SoundCueGraph->GetSoundCue();

		SoundCueGraph->Modify();

		TArray<USoundNode*> CreatedPlayers;
		FSoundCueEditorUtilities::CreateWaveContainers(Waves, SoundCue, CreatedPlayers, GraphPosition);
	}
}

void USoundCueGraphSchema::GetAllSoundNodeActions(FGraphActionMenuBuilder& ActionMenuBuilder, bool bShowSelectedActions) const
{
	InitSoundNodeClasses();

	FText SelectedItemText;

	if (bShowSelectedActions)
	{
		FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();

		// Get display text for any items that may be selected
		if (ActionMenuBuilder.FromPin == NULL)
		{
			TArray<USoundWave*> SelectedWavs;
			GEditor->GetSelectedObjects()->GetSelectedObjects<USoundWave>(SelectedWavs);
			if (SelectedWavs.Num())
			{
				SelectedItemText = FText::FromString(FString::Printf(TEXT("%s"), (SelectedWavs.Num() > 1) ? *(LOCTEXT("MultipleWAVsSelected", "Multiple WAVs").ToString()) : *SelectedWavs[0]->GetName()));
			}
		}
		else
		{
			USoundWave* SelectedWave = GEditor->GetSelectedObjects()->GetTop<USoundWave>();
			if (SelectedWave && ActionMenuBuilder.FromPin->Direction == EGPD_Input)
			{
				SelectedItemText = FText::FromString(SelectedWave->GetName());
			}
		}

		bShowSelectedActions = !SelectedItemText.IsEmpty();
	}

	for (UClass* SoundNodeClass : SoundNodeClasses)
	{
		USoundNode* SoundNode = SoundNodeClass->GetDefaultObject<USoundNode>();

		// when dragging from an output pin you can create anything but a wave player
		if (!ActionMenuBuilder.FromPin || ActionMenuBuilder.FromPin->Direction == EGPD_Input || SoundNode->GetMaxChildNodes() > 0)
		{
			const FText Name = FText::FromString(SoundNodeClass->GetDescription());

			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("Name"), Name);
				const FText AddToolTip = FText::Format(LOCTEXT("NewSoundCueNodeTooltip", "Adds {Name} node here"), Arguments);
				TSharedPtr<FSoundCueGraphSchemaAction_NewNode> NewNodeAction(new FSoundCueGraphSchemaAction_NewNode(LOCTEXT("SoundNodeAction", "Sound Node").ToString(), Name, AddToolTip.ToString(), 0));
				ActionMenuBuilder.AddAction(NewNodeAction);
				NewNodeAction->SoundNodeClass = SoundNodeClass;
			}

			if (bShowSelectedActions && (SoundNode->GetMaxChildNodes() == USoundNode::MAX_ALLOWED_CHILD_NODES || SoundNodeClass == USoundNodeWavePlayer::StaticClass()))
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("Name"), Name);
				Arguments.Add(TEXT("SelectedItems"), SelectedItemText);
				const FText MenuDesc = FText::Format(LOCTEXT("NewSoundNodeRandom", "{Name}: {SelectedItems}"), Arguments);
				const FText ToolTip = FText::Format(LOCTEXT("NewSoundNodeRandomTooltip", "Adds a {Name} node for {SelectedItems} here"), Arguments);
				TSharedPtr<FSoundCueGraphSchemaAction_NewFromSelected> NewNodeAction(new FSoundCueGraphSchemaAction_NewFromSelected(TEXT("From Selected"),
					MenuDesc,
					ToolTip.ToString(), 0));
				ActionMenuBuilder.AddAction(NewNodeAction);
				NewNodeAction->SoundNodeClass = (SoundNodeClass == USoundNodeWavePlayer::StaticClass() ? NULL : SoundNodeClass);
			}
		}
	}
}

void USoundCueGraphSchema::GetCommentAction(FGraphActionMenuBuilder& ActionMenuBuilder, const UEdGraph* CurrentGraph) const
{
	if (!ActionMenuBuilder.FromPin)
	{
		const bool bIsManyNodesSelected = CurrentGraph ? (FSoundCueEditorUtilities::GetNumberOfSelectedNodes(CurrentGraph) > 0) : false;
		const FText MenuDescription = bIsManyNodesSelected ? LOCTEXT("CreateCommentAction", "Create Comment from Selection") : LOCTEXT("AddCommentAction", "Add Comment...");
		const FString ToolTip = LOCTEXT("CreateCommentToolTip", "Creates a comment.").ToString();

		TSharedPtr<FSoundCueGraphSchemaAction_NewComment> NewAction(new FSoundCueGraphSchemaAction_NewComment(TEXT(""), MenuDescription, ToolTip, 0));
		ActionMenuBuilder.AddAction( NewAction );
	}
}

void USoundCueGraphSchema::InitSoundNodeClasses()
{
	if(bSoundNodeClassesInitialized)
	{
		return;
	}

	// Construct list of non-abstract sound node classes.
	for(TObjectIterator<UClass> It; It; ++It)
	{
		if(It->IsChildOf(USoundNode::StaticClass()) 
			&& !It->HasAnyClassFlags(CLASS_Abstract)
			&& !It->IsChildOf(UDEPRECATED_SoundNodeDeprecated::StaticClass()))
		{
			SoundNodeClasses.Add(*It);
		}
	}

	SoundNodeClasses.Sort();

	bSoundNodeClassesInitialized = true;
}

int32 USoundCueGraphSchema::GetNodeSelectionCount(const UEdGraph* Graph) const
{
	return FSoundCueEditorUtilities::GetNumberOfSelectedNodes(Graph);
}

TSharedPtr<FEdGraphSchemaAction> USoundCueGraphSchema::GetCreateCommentAction() const
{
	return TSharedPtr<FEdGraphSchemaAction>(static_cast<FEdGraphSchemaAction*>(new FSoundCueGraphSchemaAction_NewComment));
}

#undef LOCTEXT_NAMESPACE
