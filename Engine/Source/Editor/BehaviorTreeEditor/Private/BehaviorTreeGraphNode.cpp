// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTreeEditorPrivatePCH.h"
#include "ScopedTransaction.h"
#include "SGraphEditorActionMenu_BehaviorTree.h"
#include "BlueprintNodeHelpers.h"

#define LOCTEXT_NAMESPACE "BehaviorTreeGraphNode"

UBehaviorTreeGraphNode::UBehaviorTreeGraphNode(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
	NodeInstance = NULL;
	bHighlightInAbortRange0 = false;
	bHighlightInAbortRange1 = false;
	bHighlightInSearchRange0 = false;
	bHighlightInSearchRange1 = false;
	bHighlightInSearchTree = false;
	bRootLevel = false;
	bInjectedNode = false;
	bHasObserverError = false;
	bHasBreakpoint = false;
	bIsBreakpointEnabled = false;
	bDebuggerMarkCurrentlyActive = false;
	bDebuggerMarkPreviouslyActive = false;
	bDebuggerMarkFlashActive = false;
	bDebuggerMarkSearchSucceeded = false;
	bDebuggerMarkSearchFailed = false;
	bDebuggerMarkSearchOptional = false;
	bDebuggerMarkSearchTrigger = false;
	bDebuggerMarkSearchFailedTrigger = false;
	DebuggerSearchPathIndex = -1;
	DebuggerSearchPathSize = 0;
	DebuggerUpdateCounter = -1;
}

void UBehaviorTreeGraphNode::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UBehaviorTreeEditorTypes::PinCategory_MultipleNodes, TEXT(""), NULL, false, false, TEXT("In"));
	CreatePin(EGPD_Output, UBehaviorTreeEditorTypes::PinCategory_MultipleNodes, TEXT(""), NULL, false, false, TEXT("Out"));
}

void UBehaviorTreeGraphNode::PostPlacedNewNode()
{
	UClass* NodeClass = ClassData.GetClass();
	if (NodeClass)
	{
		UBehaviorTree* BT = Cast<UBehaviorTree>(GetBehaviorTreeGraph()->GetOuter());
		NodeInstance = ConstructObject<UBTNode>(NodeClass, BT);

		UBTNode* BTNode = (UBTNode*)NodeInstance;
		BTNode->SetFlags(RF_Transactional);
		BTNode->InitializeFromAsset(BT);
		BTNode->InitializeNode(NULL, MAX_uint16, 0, 0);
	}
}

bool UBehaviorTreeGraphNode::CanDuplicateNode() const
{
	return bInjectedNode ? false : Super::CanDuplicateNode();
}

bool UBehaviorTreeGraphNode::CanUserDeleteNode() const
{
	return bInjectedNode ? false : Super::CanUserDeleteNode();
}

void UBehaviorTreeGraphNode::PrepareForCopying()
{
	if (NodeInstance)
	{
		// Temporarily take ownership of the node instance, so that it is not deleted when cutting
		NodeInstance->Rename(NULL, this, REN_DontCreateRedirectors | REN_DoNotDirty );
	}
}

void UBehaviorTreeGraphNode::PostEditImport()
{
	ResetNodeOwner();

	if (NodeInstance)
	{
		UBehaviorTree* BT = Cast<UBehaviorTree>(GetBehaviorTreeGraph()->GetOuter());
		UBTNode* BTNode = (UBTNode*)NodeInstance;
		BTNode->InitializeFromAsset(BT);
		BTNode->InitializeNode(NULL, MAX_uint16, 0, 0);
	}
}

void UBehaviorTreeGraphNode::PostCopyNode()
{
	ResetNodeOwner();
}

void UBehaviorTreeGraphNode::ResetNodeOwner()
{
	if (NodeInstance)
	{
		UBehaviorTree* BT = Cast<UBehaviorTree>(GetBehaviorTreeGraph()->GetOuter());
		NodeInstance->Rename(NULL, BT, REN_DontCreateRedirectors | REN_DoNotDirty);
	}
}

FString	UBehaviorTreeGraphNode::GetDescription() const
{
	if (const UBTNode* Node = Cast<UBTNode>(NodeInstance))
	{
		return Node->GetStaticDescription();
	}

	return FString();
}

FString UBehaviorTreeGraphNode::GetTooltip() const
{
	FString TooltipDesc = bHasObserverError ? LOCTEXT("ObserverError", "Observer has invalid abort setting!").ToString() :		
		(DebuggerRuntimeDescription.Len() > 0) ? DebuggerRuntimeDescription : ErrorMessage;

	if (bInjectedNode)
	{
		TooltipDesc = LOCTEXT("Injected", "Injected: ").ToString() + ((TooltipDesc.Len() > 0) ? TooltipDesc : GetDescription());
	}

	return TooltipDesc;
}

UEdGraphPin* UBehaviorTreeGraphNode::GetInputPin(int32 InputIndex) const
{
	check(InputIndex >= 0);

	for (int32 PinIndex = 0, FoundInputs = 0; PinIndex < Pins.Num(); PinIndex++)
	{
		if (Pins[PinIndex]->Direction == EGPD_Input)
		{
			if (InputIndex == FoundInputs)
			{
				return Pins[PinIndex];
			}
			else
			{
				FoundInputs++;
			}
		}
	}

	return NULL;
}

UEdGraphPin* UBehaviorTreeGraphNode::GetOutputPin(int32 InputIndex) const
{
	check(InputIndex >= 0);

	for (int32 PinIndex = 0, FoundInputs = 0; PinIndex < Pins.Num(); PinIndex++)
	{
		if (Pins[PinIndex]->Direction == EGPD_Output)
		{
			if (InputIndex == FoundInputs)
			{
				return Pins[PinIndex];
			}
			else
			{
				FoundInputs++;
			}
		}
	}

	return NULL;
}

void UBehaviorTreeGraphNode::AutowireNewNode(UEdGraphPin* FromPin)
{
	Super::AutowireNewNode(FromPin);

	if (FromPin != NULL)
	{
		if (GetSchema()->TryCreateConnection(FromPin, GetInputPin()))
		{
			FromPin->GetOwningNode()->NodeConnectionListChanged();
		}
	}
}


UBehaviorTreeGraph* UBehaviorTreeGraphNode::GetBehaviorTreeGraph()
{
	return CastChecked<UBehaviorTreeGraph>(GetGraph());
}

void UBehaviorTreeGraphNode::NodeConnectionListChanged()
{
	GetBehaviorTreeGraph()->UpdateAsset(UBehaviorTreeGraph::SkipDebuggerFlags);
}


bool UBehaviorTreeGraphNode::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* DesiredSchema) const
{
	return DesiredSchema->GetClass()->IsChildOf(UEdGraphSchema_BehaviorTree::StaticClass());
}

void UBehaviorTreeGraphNode::DiffProperties(UStruct* Struct, void* DataA, void* DataB, FDiffResults& Results, FDiffSingleResult& Diff)
{
	for (TFieldIterator<UProperty> PropertyIt(Struct, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		UProperty* Prop = *PropertyIt;
 		// skip properties we cant see
		if (!Prop->HasAnyPropertyFlags(CPF_Edit|CPF_BlueprintVisible) ||
			Prop->HasAnyPropertyFlags(CPF_Transient) ||
			Prop->HasAnyPropertyFlags(CPF_DisableEditOnInstance) ||
			Prop->IsA(UFunction::StaticClass()) ||
			Prop->IsA(UDelegateProperty::StaticClass()) ||
			Prop->IsA(UMulticastDelegateProperty::StaticClass()))
		{
			continue;
		}
		
		FString ValueStringA = BlueprintNodeHelpers::DescribeProperty(Prop, Prop->ContainerPtrToValuePtr<uint8>(DataA));
		FString ValueStringB  = BlueprintNodeHelpers::DescribeProperty(Prop, Prop->ContainerPtrToValuePtr<uint8>(DataB));

		if ( ValueStringA != ValueStringB )
		{
			if(Results)
			{
				Diff.DisplayString = FString::Printf(*LOCTEXT("DIF_NodeProperty", "Property Changed: %s ").ToString(), *Prop->GetName());
				Results.Add(Diff);
			}
		}
	}
}

void UBehaviorTreeGraphNode::FindDiffs(UEdGraphNode* OtherNode, FDiffResults& Results)
{
	FDiffSingleResult Diff;
	Diff.Diff = EDiffType::NODE_PROPERTY;
	Diff.Node1 = this;
	Diff.Node2 = OtherNode;
	Diff.ToolTip =  FString::Printf(*LOCTEXT("DIF_NodePropertyToolTip", "A Property of the node has changed").ToString());
	Diff.DisplayColor = FLinearColor(0.25f,0.71f,0.85f);
	
	UBehaviorTreeGraphNode* ThisBehaviorTreeNode = Cast<UBehaviorTreeGraphNode>(this);
	UBehaviorTreeGraphNode* OtherBehaviorTreeNode = Cast<UBehaviorTreeGraphNode>(OtherNode);
	if(ThisBehaviorTreeNode && OtherBehaviorTreeNode)
	{
		DiffProperties( ThisBehaviorTreeNode->GetClass(), ThisBehaviorTreeNode, OtherBehaviorTreeNode, Results, Diff );

		UBTNode* ThisNodeInstance = Cast<UBTNode>(ThisBehaviorTreeNode->NodeInstance);
	    UBTNode* OtherNodeInstance = Cast<UBTNode>(OtherBehaviorTreeNode->NodeInstance);
	    if(ThisNodeInstance && OtherNodeInstance)
	    {
		    DiffProperties( ThisNodeInstance->GetClass(), ThisNodeInstance, OtherNodeInstance, Results, Diff );
		}
	}
}

void UBehaviorTreeGraphNode::AddSubNode(UBehaviorTreeGraphNode* NodeTemplate, class UEdGraph* ParentGraph)
{
	const FScopedTransaction Transaction(LOCTEXT("AddNode", "Add Node"));
	ParentGraph->Modify();
	Modify();

	NodeTemplate->SetFlags(RF_Transactional);

	// set outer to be the graph so it doesn't go away
	NodeTemplate->Rename(NULL, ParentGraph, REN_NonTransactional);
	NodeTemplate->ParentNode = this;

	NodeTemplate->CreateNewGuid();
	NodeTemplate->PostPlacedNewNode();
	NodeTemplate->AllocateDefaultPins();
	NodeTemplate->AutowireNewNode(NULL);

	NodeTemplate->NodePosX = 0;
	NodeTemplate->NodePosY = 0;

	if (Cast<UBehaviorTreeGraphNode_CompositeDecorator>(NodeTemplate) || Cast<UBehaviorTreeGraphNode_Decorator>(NodeTemplate))
	{
		bool bAppend = true;
		for (int32 Idx = 0; Idx < Decorators.Num(); Idx++)
		{
			if (Decorators[Idx]->bInjectedNode)
			{
				Decorators.Insert(NodeTemplate, Idx);
				bAppend = false;
				break;
			}
		}

		if (bAppend)
		{
			Decorators.Add(NodeTemplate);
		}
	} 
	else
	{
		Services.Add(NodeTemplate);
	}

	ParentGraph->NotifyGraphChanged();
	GetBehaviorTreeGraph()->UpdateAsset(UBehaviorTreeGraph::SkipDebuggerFlags);
}


FString GetShortTypeNameHelper(const UObject* Ob)
{
	if (Ob->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
	{
		return Ob->GetClass()->GetName().LeftChop(2);
	}

	FString TypeDesc = Ob->GetClass()->GetName();
	const int32 ShortNameIdx = TypeDesc.Find(TEXT("_"));
	if (ShortNameIdx != INDEX_NONE)
	{
		TypeDesc = TypeDesc.Mid(ShortNameIdx + 1);
	}

	return TypeDesc;
}

void UBehaviorTreeGraphNode::CreateAddDecoratorSubMenu(class FMenuBuilder& MenuBuilder, UEdGraph* Graph) const
{
	TSharedRef<SGraphEditorActionMenu_BehaviorTree> Menu =	
		SNew(SGraphEditorActionMenu_BehaviorTree)
		.GraphObj( Graph )
		.GraphNode((UBehaviorTreeGraphNode*)this)
		.SubNodeType(ESubNode::Decorator)
		.AutoExpandActionMenu(true);

	MenuBuilder.AddWidget(Menu,FText(),true);
}

void UBehaviorTreeGraphNode::CreateAddServiceSubMenu(class FMenuBuilder& MenuBuilder, UEdGraph* Graph) const
{
	TSharedRef<SGraphEditorActionMenu_BehaviorTree> Menu =	
		SNew(SGraphEditorActionMenu_BehaviorTree)
		.GraphObj( Graph )
		.GraphNode((UBehaviorTreeGraphNode*)this)
		.SubNodeType(ESubNode::Service)
		.AutoExpandActionMenu(true);

	MenuBuilder.AddWidget(Menu,FText(),true);
}

void UBehaviorTreeGraphNode::AddContextMenuActionsDecorators(const FGraphNodeContextMenuBuilder& Context) const
{
	Context.MenuBuilder->AddSubMenu(
		LOCTEXT("AddDecorator", "Add Decorator..." ),
		LOCTEXT("AddDecoratorTooltip", "Adds new decorator as a subnode" ),
		FNewMenuDelegate::CreateUObject( this, &UBehaviorTreeGraphNode::CreateAddDecoratorSubMenu,(UEdGraph*)Context.Graph));
}

void UBehaviorTreeGraphNode::AddContextMenuActionsServices(const FGraphNodeContextMenuBuilder& Context) const
{
	Context.MenuBuilder->AddSubMenu(
		LOCTEXT("AddService", "Add Service..." ),
		LOCTEXT("AddServiceTooltip", "Adds new service as a subnode" ),
		FNewMenuDelegate::CreateUObject( this, &UBehaviorTreeGraphNode::CreateAddServiceSubMenu,(UEdGraph*)Context.Graph));
}

void UBehaviorTreeGraphNode::DestroyNode()
{
	if (ParentNode)
	{
		ParentNode->Modify();
		ParentNode->Decorators.Remove(this);
		ParentNode->Services.Remove(this);
	}
	UEdGraphNode::DestroyNode();
}

void UBehaviorTreeGraphNode::ClearDebuggerState()
{
	bHasBreakpoint = false;
	bIsBreakpointEnabled = false;
	bDebuggerMarkCurrentlyActive = false;
	bDebuggerMarkPreviouslyActive = false;
	bDebuggerMarkFlashActive = false;
	bDebuggerMarkSearchSucceeded = false;
	bDebuggerMarkSearchFailed = false;
	bDebuggerMarkSearchOptional = false;
	bDebuggerMarkSearchTrigger = false;
	bDebuggerMarkSearchFailedTrigger = false;
	DebuggerSearchPathIndex = -1;
	DebuggerSearchPathSize = 0;
	DebuggerUpdateCounter = -1;
	DebuggerRuntimeDescription.Empty();
}

FName UBehaviorTreeGraphNode::GetNameIcon() const
{
	return FName("BTEditor.Graph.BTNode.Icon");
}

#undef LOCTEXT_NAMESPACE
