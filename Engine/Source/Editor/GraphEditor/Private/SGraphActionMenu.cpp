// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "GraphEditorCommon.h"
#include "SGraphEditorActionMenu.h"
#include "GraphActionNode.h"
#include "SScrollBorder.h"
#include "IDocumentation.h"

#define LOCTEXT_NAMESPACE "GraphActionMenu"

namespace GraphActionMenuHelpers
{
	bool ActionMatchesName(const FEdGraphSchemaAction* InGraphAction, const FName& ItemName)
	{
		bool bCheck = false;

		bCheck |= (InGraphAction->GetTypeId() == FEdGraphSchemaAction_K2Var::StaticGetTypeId() &&
			((FEdGraphSchemaAction_K2Var*)InGraphAction)->GetVariableName() == ItemName);
		bCheck |= (InGraphAction->GetTypeId() == FEdGraphSchemaAction_K2LocalVar::StaticGetTypeId() &&
			((FEdGraphSchemaAction_K2LocalVar*)InGraphAction)->GetVariableName() == ItemName);
		bCheck |= (InGraphAction->GetTypeId() == FEdGraphSchemaAction_K2Graph::StaticGetTypeId() &&
			((FEdGraphSchemaAction_K2Graph*)InGraphAction)->EdGraph &&
			((FEdGraphSchemaAction_K2Graph*)InGraphAction)->EdGraph->GetFName() == ItemName);
		bCheck |= (InGraphAction->GetTypeId() == FEdGraphSchemaAction_K2Enum::StaticGetTypeId() &&
			((FEdGraphSchemaAction_K2Enum*)InGraphAction)->GetPathName() == ItemName);
		bCheck |= (InGraphAction->GetTypeId() == FEdGraphSchemaAction_K2Struct::StaticGetTypeId() &&
			((FEdGraphSchemaAction_K2Struct*)InGraphAction)->GetPathName() == ItemName);
		bCheck |= (InGraphAction->GetTypeId() == FEdGraphSchemaAction_K2Delegate::StaticGetTypeId() &&
			((FEdGraphSchemaAction_K2Delegate*)InGraphAction)->GetDelegateName() == ItemName);
		bCheck |= (InGraphAction->GetTypeId() == FEdGraphSchemaAction_K2TargetNode::StaticGetTypeId() &&
			((FEdGraphSchemaAction_K2TargetNode*)InGraphAction)->NodeTemplate->GetNodeTitle(ENodeTitleType::EditableTitle).ToString() == ItemName.ToString());

		return bCheck;
	}
}

void SDefaultGraphActionWidget::Construct(const FArguments& InArgs, const FCreateWidgetForActionData* InCreateData)
{
	ActionPtr = InCreateData->Action;
	MouseButtonDownDelegate = InCreateData->MouseButtonDownDelegate;

	this->ChildSlot
	[
		SNew(SHorizontalBox)
		.ToolTipText( InCreateData->Action->TooltipDescription )
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 9 ))
			.Text(InCreateData->Action->MenuDescription)
			.HighlightText(InArgs._HighlightText)
		]
	];
}

FReply SDefaultGraphActionWidget::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if( MouseButtonDownDelegate.Execute( ActionPtr ) )
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

//////////////////////////////////////////////////////////////////////////

class SGraphActionCategoryWidget : public SCompoundWidget
{
	SLATE_BEGIN_ARGS( SGraphActionCategoryWidget ) 
	{}
		SLATE_ATTRIBUTE( FText, HighlightText )
		SLATE_EVENT( FOnTextCommitted, OnTextCommitted )
		SLATE_EVENT( FIsSelected, IsSelected )
		SLATE_ATTRIBUTE( bool, IsReadOnly )
	SLATE_END_ARGS()

	TWeakPtr<FGraphActionNode> ActionNode;

public:
	TWeakPtr<SInlineEditableTextBlock> InlineWidget;

	void Construct( const FArguments& InArgs, TSharedPtr<FGraphActionNode> InActionNode )
	{
		ActionNode = InActionNode;

		FText CategoryText = FText::FromString(InActionNode->Category);
		TSharedRef<SToolTip> ToolTipWidget = IDocumentation::Get()->CreateToolTip(CategoryText, NULL, TEXT("Shared/GraphNodes/Blueprint/NodeCategories"), InActionNode->Category);

		this->ChildSlot
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SAssignNew(InlineWidget, SInlineEditableTextBlock)
				.Font( FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 9 )  )
				.Text( CategoryText )
				.ToolTip( ToolTipWidget )
				.HighlightText( InArgs._HighlightText )
				.OnVerifyTextChanged( this, &SGraphActionCategoryWidget::OnVerifyTextChanged )
				.OnTextCommitted( InArgs._OnTextCommitted )
				.IsSelected( InArgs._IsSelected )
				.IsReadOnly( InArgs._IsReadOnly )
			]
		];
	}

	// SWidget interface
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) OVERRIDE
	{
		TSharedPtr<FGraphEditorDragDropAction> GraphDropOp = DragDropEvent.GetOperationAs<FGraphEditorDragDropAction>();
		if (GraphDropOp.IsValid())
		{
			GraphDropOp->DroppedOnCategory( ActionNode.Pin()->Category );
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	virtual void OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) OVERRIDE
	{
		TSharedPtr<FGraphEditorDragDropAction> GraphDropOp = DragDropEvent.GetOperationAs<FGraphEditorDragDropAction>();
		if (GraphDropOp.IsValid())
		{
			GraphDropOp->SetHoveredCategoryName( ActionNode.Pin()->Category );
		}
	}

	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) OVERRIDE
	{
		TSharedPtr<FGraphEditorDragDropAction> GraphDropOp = DragDropEvent.GetOperationAs<FGraphEditorDragDropAction>();
		if (GraphDropOp.IsValid())
		{
			GraphDropOp->SetHoveredCategoryName( FString(TEXT("")) );
		}
	}

	// End of SWidget interface

	/** Callback for the SInlineEditableTextBlock to verify the text before commit */
	bool OnVerifyTextChanged(const FText& InText, FText& OutErrorMessage)
	{
		if(InText.ToString().Len() > NAME_SIZE)
		{
			OutErrorMessage = LOCTEXT("CategoryNameTooLong_Error", "Name too long!");
			return false;
		}

		return true;
	}
};

//////////////////////////////////////////////////////////////////////////

void SGraphActionMenu::Construct( const FArguments& InArgs, bool bIsReadOnly/* = true*/ )
{
	this->SelectedSuggestion = INDEX_NONE;
	this->bIgnoreUIUpdate = false;

	this->bAutoExpandActionMenu = InArgs._AutoExpandActionMenu;
	this->bShowFilterTextBox = InArgs._ShowFilterTextBox;
	this->bAlphaSortItems = InArgs._AlphaSortItems;
	this->OnActionSelected = InArgs._OnActionSelected;
	this->OnActionDoubleClicked = InArgs._OnActionDoubleClicked;
	this->OnActionDragged = InArgs._OnActionDragged;
	this->OnCategoryDragged = InArgs._OnCategoryDragged;
	this->OnCreateWidgetForAction = InArgs._OnCreateWidgetForAction;
	this->OnCreateCustomRowExpander = InArgs._OnCreateCustomRowExpander;
	this->OnCollectAllActions = InArgs._OnCollectAllActions;
	this->OnCategoryTextCommitted = InArgs._OnCategoryTextCommitted;
	this->OnCanRenameSelectedAction = InArgs._OnCanRenameSelectedAction;
	this->OnGetSectionTitle = InArgs._OnGetSectionTitle;
	this->FilteredRootAction = FGraphActionNode::NewCategory(TEXT("FILTEREDROOT"));
	
	// If a delegate for filtering text is passed in, assign it so that it will be used instead of the built-in filter box
	if(InArgs._OnGetFilterText.IsBound())
	{
		this->OnGetFilterText = InArgs._OnGetFilterText;
	}

	TreeView = SNew(STreeView< TSharedPtr<FGraphActionNode> >)
		.ItemHeight(24)
		.TreeItemsSource(&(this->FilteredRootAction->Children))
		.OnGenerateRow(this, &SGraphActionMenu::MakeWidget, bIsReadOnly)
		.OnSelectionChanged(this, &SGraphActionMenu::OnItemSelected)
		.OnMouseButtonDoubleClick(this, &SGraphActionMenu::OnItemDoubleClicked)
		.OnContextMenuOpening(InArgs._OnContextMenuOpening)
		.OnGetChildren(this, &SGraphActionMenu::OnGetChildrenForCategory)
		.SelectionMode(ESelectionMode::Single)
		.OnItemScrolledIntoView(this, &SGraphActionMenu::OnItemScrolledIntoView);


	this->ChildSlot
	[
		SNew(SVerticalBox)

		// FILTER BOX
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(FilterTextBox, SSearchBox)
			// If there is an external filter delegate, do not display this filter box
			.Visibility(InArgs._OnGetFilterText.IsBound()? EVisibility::Collapsed : EVisibility::Visible)
			.OnTextChanged( this, &SGraphActionMenu::OnFilterTextChanged )
			.OnTextCommitted( this, &SGraphActionMenu::OnFilterTextCommitted )
		]

		// ACTION LIST
		+SVerticalBox::Slot()
		.Padding(FMargin(0.0f, 2.0f, 0.0f, 0.0f))
		.FillHeight(1.f)
		[
			SNew(SScrollBorder, TreeView.ToSharedRef())
			[
				TreeView.ToSharedRef()
			]
		]
	];

	if (!InArgs._ShowFilterTextBox)
	{
		FilterTextBox->SetVisibility(EVisibility::Collapsed);
	}

	// Get all actions.
	RefreshAllActions(false);
}

void SGraphActionMenu::RefreshAllActions(bool bPreserveExpansion, bool bHandleOnSelectionEvent/*=true*/)
{
	// Save Selection (of only the first selected thing)
	TArray< TSharedPtr<FGraphActionNode> > SelectedNodes = TreeView->GetSelectedItems();
	TSharedPtr<FEdGraphSchemaAction> SelectedAction = SelectedNodes.Num() > 0 ? SelectedNodes[0]->Actions[0] : NULL;

	AllActions.Empty();
	OnCollectAllActions.ExecuteIfBound(AllActions);
	GenerateFilteredItems(bPreserveExpansion);

	// Re-apply selection #0 if possible
	if (SelectedAction.IsValid())
	{
		TArray<TSharedPtr<FGraphActionNode>> GraphNodes;
		FilteredRootAction->GetAllNodes(GraphNodes, false);
		for (int32 i = 0; i < GraphNodes.Num(); ++i)
		{
			TSharedPtr<FEdGraphSchemaAction> GraphAction = GraphNodes[i]->Actions[0];
			if (GraphAction.IsValid() && GraphAction->MenuDescription.ToString() == SelectedAction->MenuDescription.ToString())
			{
				// Clear the selection (if this node is already selected then setting it will have no effect)
				TreeView->ClearSelection();
				// Now set the selection
				if(bHandleOnSelectionEvent)
				{
					TreeView->SetSelection(GraphNodes[i], ESelectInfo::OnMouseClick);
				}
				else
				{
					// If we do not want to handle the selection, set it directly so it will reselect the item but not handle the event.
					TreeView->SetSelection(GraphNodes[i], ESelectInfo::Direct);
				}
				
				break;
			}
		}
	}
}

TSharedRef<SEditableTextBox> SGraphActionMenu::GetFilterTextBox()
{
	return FilterTextBox.ToSharedRef();
}

void SGraphActionMenu::GetSelectedActions(TArray< TSharedPtr<FEdGraphSchemaAction> >& OutSelectedActions) const
{
	OutSelectedActions.Empty();

	TArray< TSharedPtr<FGraphActionNode> > SelectedNodes = TreeView->GetSelectedItems();
	if(SelectedNodes.Num() > 0)
	{
		for ( int32 NodeIndex = 0; NodeIndex < SelectedNodes.Num(); NodeIndex++ )
		{
			OutSelectedActions.Append( SelectedNodes[NodeIndex]->Actions );
		}
	}
}

void SGraphActionMenu::OnRequestRenameOnActionNode()
{
	TArray< TSharedPtr<FGraphActionNode> > SelectedNodes = TreeView->GetSelectedItems();
	if(SelectedNodes.Num() > 0)
	{
		if(SelectedNodes[0]->RenameRequestEvent.IsBound())
		{
			SelectedNodes[0]->BroadcastRenameRequest();
		}
		else
		{
			TreeView->RequestScrollIntoView(SelectedNodes[0]);
			SelectedNodes[0]->bIsRenameRequestBeforeReady = true;
		}
	}
}

bool SGraphActionMenu::CanRequestRenameOnActionNode() const
{
	TArray< TSharedPtr<FGraphActionNode> > SelectedNodes = TreeView->GetSelectedItems();
	if(SelectedNodes.Num() == 1 && OnCanRenameSelectedAction.IsBound())
	{
		return OnCanRenameSelectedAction.Execute(SelectedNodes[0]);
	}

	return false;
}

FString SGraphActionMenu::GetSelectedCategoryName() const
{
	TArray< TSharedPtr<FGraphActionNode> > SelectedNodes = TreeView->GetSelectedItems();
	return (SelectedNodes.Num() > 0) ? SelectedNodes[0]->Category : FString();
}

void SGraphActionMenu::GetSelectedCategorySubActions(TArray<TSharedPtr<FEdGraphSchemaAction>>& OutActions) const
{
	TArray< TSharedPtr<FGraphActionNode> > SelectedNodes = TreeView->GetSelectedItems();
	for ( int32 SelectionIndex = 0; SelectionIndex < SelectedNodes.Num(); SelectionIndex++ )
	{
		if ( SelectedNodes[SelectionIndex].IsValid() )
		{
			GetCategorySubActions(SelectedNodes[SelectionIndex], OutActions);
		}
	}
}

void SGraphActionMenu::GetCategorySubActions(TWeakPtr<FGraphActionNode> InAction, TArray<TSharedPtr<FEdGraphSchemaAction>>& OutActions) const
{
	if(InAction.IsValid())
	{
		TSharedPtr<FGraphActionNode> CategoryNode = InAction.Pin();
		TArray<TSharedPtr<FGraphActionNode>> Children;
		CategoryNode->GetLeafNodes(Children);

		for (int32 i = 0; i < Children.Num(); ++i)
		{
			TSharedPtr<FGraphActionNode> CurrentChild = Children[i];

			if (CurrentChild.IsValid() && CurrentChild->IsActionNode())
			{
				for ( int32 ActionIndex = 0; ActionIndex != CurrentChild->Actions.Num(); ActionIndex++ )
				{
					OutActions.Add(CurrentChild->Actions[ActionIndex]);
				}
			}
		}
	}
}

bool SGraphActionMenu::SelectItemByName(const FName& ItemName, ESelectInfo::Type SelectInfo )
{
	if (ItemName != NAME_None)
	{
		TSharedPtr<FGraphActionNode> SelectionNode;

		TArray<TSharedPtr<FGraphActionNode>> GraphNodes;
		FilteredRootAction->GetAllNodes(GraphNodes, false);
		for (int32 i = 0; i < GraphNodes.Num() && !SelectionNode.IsValid(); ++i)
		{
			TSharedPtr<FGraphActionNode> CurrentGraphNode = GraphNodes[i];

			FEdGraphSchemaAction* GraphAction = CurrentGraphNode->Actions[0].Get();
			if (GraphAction)
			{

				if (GraphActionMenuHelpers::ActionMatchesName(GraphAction, ItemName))
				{
					SelectionNode = GraphNodes[i];

					break;
				}
			}

			// One of the children may match
			for(int32 ChildIdx = 0; ChildIdx < CurrentGraphNode->Children.Num() && !SelectionNode.IsValid(); ++ChildIdx)
			{
				TSharedPtr<FGraphActionNode> CurrentChildNode = CurrentGraphNode->Children[ChildIdx];

				for ( int32 ActionIndex = 0; ActionIndex < CurrentChildNode->Actions.Num(); ActionIndex++ )
				{
					FEdGraphSchemaAction* ChildGraphAction = CurrentChildNode->Actions[ActionIndex].Get();

					if(ChildGraphAction)
					{
						if (GraphActionMenuHelpers::ActionMatchesName(ChildGraphAction, ItemName))
						{
							SelectionNode = GraphNodes[i]->Children[ChildIdx];

							break;
						}
					}
				}
			}
		}

		if(SelectionNode.IsValid())
		{
			TreeView->SetSelection(SelectionNode,SelectInfo);
			TreeView->RequestScrollIntoView(SelectionNode);
			return true;
		}
	}
	else
	{
		TreeView->ClearSelection();
		return true;
	}
	return false;
}

void SGraphActionMenu::ExpandCategory(const FString& CategoryName)
{
	if (CategoryName.Len())
	{
		TArray<TSharedPtr<FGraphActionNode>> GraphNodes;
		FilteredRootAction->GetAllNodes(GraphNodes, false);
		for (int32 i = 0; i < GraphNodes.Num(); ++i)
		{
			if (GraphNodes[i]->Category == CategoryName)
			{
				GraphNodes[i]->ExpandAllChildren(TreeView);
			}
		}
	}
}

static bool CompareGraphActionNode(TSharedPtr<FGraphActionNode> A, TSharedPtr<FGraphActionNode> B)
{
	check(A.IsValid());
	check(B.IsValid());

	// First check grouping is the same
	if(A->Category != B->Category)
	{
		return false;
	}

	if(A->Actions[0].IsValid() && B->Actions[0].IsValid())
	{
		return A->Actions[0]->MenuDescription.CompareTo(B->Actions[0]->MenuDescription) == 0;
	}
	else if(!A->Actions[0].IsValid() && !B->Actions[0].IsValid())
	{
		return true;
	}
	else
	{
		return false;
	}
}

template<typename ItemType, typename ComparisonType> 
void RestoreExpansionState(TSharedPtr< STreeView<ItemType> > InTree, const TArray<ItemType>& ItemSource, const TSet<ItemType>& OldExpansionState, ComparisonType ComparisonFunction)
{
	check(InTree.IsValid());

	// Iterate over new tree items
	for(int32 ItemIdx=0; ItemIdx<ItemSource.Num(); ItemIdx++)
	{
		ItemType NewItem = ItemSource[ItemIdx];

		// Look through old expansion state
		for (typename TSet<ItemType>::TConstIterator OldExpansionIter(OldExpansionState); OldExpansionIter; ++OldExpansionIter)
		{
			const ItemType OldItem = *OldExpansionIter;
			// See if this matches this new item
			if(ComparisonFunction(OldItem, NewItem))
			{
				// It does, so expand it
				InTree->SetItemExpansion(NewItem, true);
			}
		}
	}
}


void SGraphActionMenu::GenerateFilteredItems(bool bPreserveExpansion)
{
	// First, save off current expansion state
	TSet< TSharedPtr<FGraphActionNode> > OldExpansionState;
	if(bPreserveExpansion)
	{
		TreeView->GetExpandedItems(OldExpansionState);
	}

	// Clear the filtered root action
	FilteredRootAction->ClearChildren();

	// Trim and sanitized the filter text (so that it more likely matches the action descriptions)
	FString TrimmedFilterString = FText::TrimPrecedingAndTrailing(GetFilterText()).ToString();

	// Tokenize the search box text into a set of terms; all of them must be present to pass the filter
	TArray<FString> FilterTerms;
	TrimmedFilterString.ParseIntoArray(/*out*/ &FilterTerms, TEXT(" "), true);
	
	// Generate a list of sanitized versions of the strings
	TArray<FString> SanitizedFilterTerms;
	for (int32 iFilters = 0; iFilters < FilterTerms.Num() ; iFilters++)
	{
		FString EachString = FName::NameToDisplayString( FilterTerms[iFilters], false );
		EachString = EachString.Replace( TEXT( " " ), TEXT( "" ) );
		SanitizedFilterTerms.Add( EachString );
	}
	ensure( SanitizedFilterTerms.Num() == FilterTerms.Num() );// Both of these should match !

	const bool bRequiresFiltering = FilterTerms.Num() > 0;
	int32 BestMatchCount = 0;
	int32 BestMatchIndex = INDEX_NONE;		
	for (int32 CurTypeIndex=0; CurTypeIndex < AllActions.GetNumActions(); ++CurTypeIndex)
	{
		FGraphActionListBuilderBase::ActionGroup& CurrentAction = AllActions.GetAction( CurTypeIndex );

		// If we're filtering, search check to see if we need to show this action
		bool bShowAction = true;
		int32 EachWeight = 0;
		if (bRequiresFiltering)
		{
			// Combine the actions string, separate with \n so terms don't run into each other, and remove the spaces (incase the user is searching for a variable)
			// In the case of groups containing multiple actions, they will have been created and added at the same place in the code, using the same description
			// and keywords, so we only need to use the first one for filtering.
			FString SearchText = CurrentAction.Actions[0]->MenuDescription.ToString() + LINE_TERMINATOR + CurrentAction.Actions[0]->GetSearchTitle().ToString() + LINE_TERMINATOR + CurrentAction.Actions[0]->Keywords + LINE_TERMINATOR +CurrentAction.Actions[0]->Category;
			SearchText = SearchText.Replace( TEXT( " " ), TEXT( "" ) );
			// Get the 'weight' of this in relation to the filter
			EachWeight = GetActionFilteredWeight( CurrentAction, FilterTerms, SanitizedFilterTerms );
			FString EachTermSanitized;
			for (int32 FilterIndex = 0; (FilterIndex < FilterTerms.Num()) && bShowAction; ++FilterIndex)
			{
				const bool bMatchesTerm = ( SearchText.Contains( FilterTerms[FilterIndex] ) || ( SearchText.Contains( SanitizedFilterTerms[FilterIndex] ) == true ) );
				bShowAction = bShowAction && bMatchesTerm;
			}
		}

		if (bShowAction)
		{
			// If this action has a greater relevance than others, cache its index.
			if( EachWeight > BestMatchCount )
			{
				BestMatchCount = EachWeight;
				BestMatchIndex = CurTypeIndex;
			}								
			// Add the action to the filtered list.  This will automatically place it in the right subcategory
			TArray<FString> CategoryChain;
			CurrentAction.GetCategoryChain(CategoryChain);
			
			TSharedPtr<FGraphActionNode> NewNode = FGraphActionNode::NewAction(CurrentAction.Actions);
			FilteredRootAction->AddChild(NewNode, CategoryChain, bAlphaSortItems);
		}
	}

	TreeView->RequestTreeRefresh();

	// Update the filtered list (needs to be done in a separate pass because the list is sorted as items are inserted)
	FilteredActionNodes.Empty();
	FilteredRootAction->GetAllNodes(FilteredActionNodes, true);

	// Get _all_ new nodes (flattened tree basically)
	TArray< TSharedPtr<FGraphActionNode> > AllNodes;
	FilteredRootAction->GetAllNodes(AllNodes, false);

	// If theres a BestMatchIndex find it in the actions nodes and select it (maybe this should check the current selected suggestion first ?)
	if( BestMatchIndex != INDEX_NONE ) 
	{
		FGraphActionListBuilderBase::ActionGroup& FilterSelectAction = AllActions.GetAction( BestMatchIndex );
		if( FilterSelectAction.Actions[0].IsValid() == true )
		{
			for (int32 iNode = 0; iNode < FilteredActionNodes.Num() ; iNode++)
			{
				if( FilteredActionNodes[ iNode ].Get()->Actions[ 0 ] == FilterSelectAction.Actions[ 0 ] )
				{
					SelectedSuggestion = iNode;
				}
			}	
		}	
	}

	// Make sure the selected suggestion stays within the filtered list
	if ((SelectedSuggestion >= 0) && (FilteredActionNodes.Num() > 0))
	{
		//@TODO: Should try to actually maintain the highlight on the same item if it survived the filtering
		SelectedSuggestion = FMath::Clamp<int32>(SelectedSuggestion, 0, FilteredActionNodes.Num() - 1);
		MarkActiveSuggestion();
	}
	else
	{
		SelectedSuggestion = INDEX_NONE;
	}


	if (ShouldExpandNodes())
	{
		// Expand all
		FilteredRootAction->ExpandAllChildren(TreeView);
	}
	else
	{
		// Expand to match the old state
		RestoreExpansionState< TSharedPtr<FGraphActionNode> >(TreeView, AllNodes, OldExpansionState, CompareGraphActionNode);
	}
}

int32 SGraphActionMenu::GetActionFilteredWeight( const FGraphActionListBuilderBase::ActionGroup& InCurrentAction, const TArray<FString>& InFilterTerms, const TArray<FString>& InSanitizedFilterTerms )
{
	// The overall 'weight'
	int32 TotalWeight = 0;

	// Some simple weight figures to help find the most appropriate match	
	int32 WholeMatchWeightMultiplier = 2;
	int32 DescriptionWeight = 5;
	int32 CategoryWeight = 3;
	int32 NodeTitleWeight = 3;

	// Helper array
	struct FArrayWithWeight
	{
		TArray< FString > Array;
		int32			  Weight;
	};

	// Setup an array of arrays so we can do a weighted search			
	TArray< FArrayWithWeight > WeightedArrayList;
	FArrayWithWeight EachEntry;

	int32 Action = 0;
	if( InCurrentAction.Actions[Action].IsValid() == true )
	{
		// Combine the actions string, separate with \n so terms don't run into each other, and remove the spaces (incase the user is searching for a variable)
		// In the case of groups containing multiple actions, they will have been created and added at the same place in the code, using the same description
		// and keywords, so we only need to use the first one for filtering.
		FString SearchText = InCurrentAction.Actions[0]->MenuDescription.ToString() + LINE_TERMINATOR + InCurrentAction.Actions[Action]->GetSearchTitle().ToString() + LINE_TERMINATOR + InCurrentAction.Actions[Action]->Keywords + LINE_TERMINATOR +InCurrentAction.Actions[Action]->Category;
		SearchText = SearchText.Replace( TEXT( " " ), TEXT( "" ) );

		// First the keywords
		InCurrentAction.Actions[Action]->Keywords.ParseIntoArray( &EachEntry.Array, TEXT(" "), true );
		EachEntry.Weight = 1;
		WeightedArrayList.Add( EachEntry );

		// The description
		InCurrentAction.Actions[Action]->MenuDescription.ToString().ParseIntoArray( &EachEntry.Array, TEXT(" "), true );
		EachEntry.Weight = DescriptionWeight;
		WeightedArrayList.Add( EachEntry );

		// The node search title weight
		InCurrentAction.Actions[Action]->GetSearchTitle().ToString().ParseIntoArray( &EachEntry.Array, TEXT(" "), true );
		EachEntry.Weight = NodeTitleWeight;
		WeightedArrayList.Add( EachEntry );

		// The category
		InCurrentAction.Actions[Action]->Category.ParseIntoArray( &EachEntry.Array, TEXT(" "), true );
		EachEntry.Weight = CategoryWeight;
		WeightedArrayList.Add( EachEntry );

		// Now iterate through all the filter terms and calculate a 'weight' using the values and multipliers
		FString EachTerm;
		FString EachTermSanitized;
		for (int32 FilterIndex = 0; FilterIndex < InFilterTerms.Num(); ++FilterIndex)
		{
			EachTerm = InFilterTerms[FilterIndex];
			EachTermSanitized = InSanitizedFilterTerms[FilterIndex];
			if( SearchText.Contains( EachTerm ) )
			{
				TotalWeight += 2;
			}
			else if( SearchText.Contains( EachTermSanitized ) )
			{
				TotalWeight++;
			}		
			// Now check the weighted lists	(We could further improve the hit weight by checking consecutive word matches)
			int32 WeightPerList = 0;
			for (int32 iFindCount = 0; iFindCount < WeightedArrayList.Num() ; iFindCount++)
			{
				TArray<FString>& KeywordArray = WeightedArrayList[iFindCount].Array;
				int32 EachWeight = WeightedArrayList[iFindCount].Weight;
				int32 WholeMatchCount = 0;
				for (int32 iEachWord = 0; iEachWord < KeywordArray.Num() ; iEachWord++)
				{
					// If we get an exact match weight the find count to get exact matches higher priority
					if( KeywordArray[ iEachWord ] == EachTerm )
					{
						WeightPerList += EachWeight * WholeMatchWeightMultiplier;
						WholeMatchCount++;
					}
					else if( KeywordArray[ iEachWord ].Contains( EachTerm ) )
					{
						WeightPerList += EachWeight;
					}
					else if( KeywordArray[ iEachWord ] == EachTermSanitized )
					{
						WeightPerList += ( EachWeight * WholeMatchWeightMultiplier ) / 2;
						WholeMatchCount++;
					}
					else if( KeywordArray[ iEachWord ].Contains( EachTermSanitized ) )
					{
						WeightPerList += EachWeight / 2;
					}
				}
				// Increase the weight if theres a larger % of matches in the keyword list
				if( WholeMatchCount != 0 )
				{
					int32 PercentAdjust = ( 100 / KeywordArray.Num() ) * WholeMatchCount;
					WeightPerList *= PercentAdjust;
				}
				TotalWeight += WeightPerList;
			}
		}
	}
	return TotalWeight;
}

// Returns true if the tree should be autoexpanded
bool SGraphActionMenu::ShouldExpandNodes() const
{
	// Expand all the categories that have filter results, or when there are only a few to show
	const bool bFilterActive = !GetFilterText().IsEmpty();
	const bool bOnlyAFewTotal = AllActions.GetNumActions() < 10;

	return bFilterActive || bOnlyAFewTotal || bAutoExpandActionMenu;
}

bool SGraphActionMenu::CanRenameNode(TWeakPtr<FGraphActionNode> InNode) const
{
	return !OnCanRenameSelectedAction.Execute(InNode);
}

void SGraphActionMenu::OnFilterTextChanged( const FText& InFilterText )
{
	// Reset the selection if the string is empty
	if( InFilterText.IsEmpty() == true )
	{
		SelectedSuggestion = INDEX_NONE;
	}
	GenerateFilteredItems(false);
}

void SGraphActionMenu::OnFilterTextCommitted(const FText& InText, ETextCommit::Type CommitInfo)
{
	if (CommitInfo == ETextCommit::OnEnter)
	{
		TryToSpawnActiveSuggestion();
	}
}

bool SGraphActionMenu::TryToSpawnActiveSuggestion()
{
	TArray< TSharedPtr<FGraphActionNode> > SelectionList = TreeView->GetSelectedItems();

	if (SelectionList.Num() == 1)
	{
		// This isnt really a keypress - its Direct, but its always called from a keypress function. (Maybe pass the selectinfo in ?)
		OnItemSelected( SelectionList[0], ESelectInfo::OnKeyPress );
		return true;
	}
	else if (FilteredActionNodes.Num() == 1)
	{
		OnItemSelected( FilteredActionNodes[0], ESelectInfo::OnKeyPress );
		return true;
	}

	return false;
}

void SGraphActionMenu::OnGetChildrenForCategory( TSharedPtr<FGraphActionNode> InItem, TArray< TSharedPtr<FGraphActionNode> >& OutChildren )
{
	if (InItem->Children.Num())
	{
		OutChildren = InItem->Children;
	}
}

void SGraphActionMenu::OnNameTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit, TWeakPtr< FGraphActionNode > InAction )
{
	if(OnCategoryTextCommitted.IsBound())
	{
		OnCategoryTextCommitted.Execute(NewText, InTextCommit, InAction);
	}
}

void SGraphActionMenu::OnItemScrolledIntoView( TSharedPtr<FGraphActionNode> InActionNode, const TSharedPtr<ITableRow>& InWidget )
{
	if(InActionNode->bIsRenameRequestBeforeReady)
	{
		InActionNode->bIsRenameRequestBeforeReady = false;
		InActionNode->BroadcastRenameRequest();
	}
}

TSharedRef<ITableRow> SGraphActionMenu::MakeWidget( TSharedPtr<FGraphActionNode> InItem, const TSharedRef<STableViewBase>& OwnerTable, bool bIsReadOnly )
{
	// In the case of FGraphActionNodes that have multiple actions, all of the actions will
	// have the same text as they will have been created at the same point - only the actual
	// action itself will differ, which is why parts of this function only refer to InItem->Actions[0]
	// rather than iterating over the array

	// Create the widget but do not add any content, the widget is needed to pass the IsSelectedExclusively function down to the potential SInlineEditableTextBlock widget
	TSharedPtr< STableRow< TSharedPtr<FGraphActionNode> > > TableRow = SNew(STableRow< TSharedPtr<FGraphActionNode> >, OwnerTable)
		.OnDragDetected( this, &SGraphActionMenu::OnItemDragDetected )
		.ShowSelection( !InItem->IsSeparator() );


	TSharedPtr<SWidget> RowContent;

	if( InItem->IsActionNode() )
	{
		check(InItem->Actions.Num() > 0 && InItem->Actions[0].IsValid() );

		FCreateWidgetForActionData CreateData(&InItem->OnRenameRequest());
		CreateData.Action = InItem->Actions[0];
		CreateData.HighlightText = TAttribute<FText>(this, &SGraphActionMenu::GetFilterText);
		CreateData.MouseButtonDownDelegate = FCreateWidgetMouseButtonDown::CreateSP( this, &SGraphActionMenu::OnMouseButtonDownEvent );

		if(OnCreateWidgetForAction.IsBound())
		{
			CreateData.IsRowSelectedDelegate = FIsSelected::CreateSP( TableRow.Get(), &STableRow< TSharedPtr<FGraphActionNode> >::IsSelectedExclusively );
			CreateData.bIsReadOnly = bIsReadOnly;
			CreateData.bHandleMouseButtonDown = false;		//Default to NOT using the delegate. OnCreateWidgetForAction can set to true if we need it
			RowContent = OnCreateWidgetForAction.Execute( &CreateData );
		}
		else
		{
			RowContent = SNew(SDefaultGraphActionWidget, &CreateData);
		}
	}
	else if( InItem->IsCategoryNode() )
	{
		TWeakPtr< FGraphActionNode > WeakItem = InItem;

		// Hook up the delegate for verifying the category action is read only or not
		SGraphActionCategoryWidget::FArguments ReadOnlyArgument;
		if(bIsReadOnly)
		{
			ReadOnlyArgument.IsReadOnly(bIsReadOnly);
		}
		else
		{
			ReadOnlyArgument.IsReadOnly(this, &SGraphActionMenu::CanRenameNode, WeakItem);
		}

		TSharedRef<SGraphActionCategoryWidget> CategoryWidget = 
			SNew(SGraphActionCategoryWidget, InItem)
			.HighlightText(this, &SGraphActionMenu::GetFilterText)
			.OnTextCommitted(this, &SGraphActionMenu::OnNameTextCommitted, TWeakPtr< FGraphActionNode >(InItem))
			.IsSelected(TableRow.Get(), &STableRow< TSharedPtr<FGraphActionNode> >::IsSelectedExclusively)
			.IsReadOnly(ReadOnlyArgument._IsReadOnly);

		if(!bIsReadOnly)
		{
			InItem->OnRenameRequest().BindSP( CategoryWidget->InlineWidget.Pin().Get(), &SInlineEditableTextBlock::EnterEditingMode );
		}

		RowContent = CategoryWidget;
	}
	else if( InItem->IsSeparator() )
	{
		FText SectionTitle;
		if( OnGetSectionTitle.IsBound() )
		{
			SectionTitle = OnGetSectionTitle.Execute(InItem->SectionID);
		}

		if( SectionTitle.IsEmpty() == true )
		{
			RowContent = SNew( SVerticalBox )
				.Visibility(EVisibility::HitTestInvisible)
				+SVerticalBox::Slot()
				.AutoHeight()
				// Add some empty space before the line, and a tiny bit after it
				.Padding( 0.0f, 5.f, 0.0f, 5.f )
				[
					SNew( SBorder )

					// We'll use the border's padding to actually create the horizontal line
					.Padding( 1.f )

					// Separator graphic
					.BorderImage( FEditorStyle::GetBrush( TEXT( "Menu.Separator" ) ) )
				];
		}
		else
		{
			RowContent = SNew( SVerticalBox )
			.Visibility(EVisibility::HitTestInvisible)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding( 0.0f, 2.f, 0.0f, 0.f )
			[
				SNew(STextBlock)
				.Text( SectionTitle )
				.TextStyle( FEditorStyle::Get(), TEXT("Menu.Heading") )
			]
			+SVerticalBox::Slot()
			.AutoHeight()			
			// Add some empty space before the line, and a tiny bit after it
			.Padding( 0.0f, 2.f, 0.0f, 5.f )
			[
				SNew( SBorder )

				// We'll use the border's padding to actually create the horizontal line
				.Padding( 1.f )

				// Separator graphic
				.BorderImage( FEditorStyle::GetBrush( TEXT( "Menu.Separator" ) ) )
			];	
		}
	}

	TSharedPtr<SHorizontalBox> RowContainer;
	TableRow->SetContent
	( 
		SAssignNew(RowContainer, SHorizontalBox)
	);

	TSharedPtr<SExpanderArrow> ExpanderWidget;
	if (OnCreateCustomRowExpander.IsBound())
	{
		FCustomExpanderData CreateData;
		CreateData.TableRow        = TableRow;
		CreateData.WidgetContainer = RowContainer;

		if (InItem->IsActionNode())
		{
			check(InItem->Actions.Num() > 0);
			CreateData.RowAction = InItem->Actions[0];
		}

		ExpanderWidget = OnCreateCustomRowExpander.Execute(CreateData);
	}
	else 
	{
		ExpanderWidget = SNew(SExpanderArrow, TableRow);
	}

	RowContainer->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Right)
	[
		ExpanderWidget.ToSharedRef()
	];

	RowContainer->AddSlot()
		.FillWidth(1.0)
	[
		RowContent.ToSharedRef()
	];

	return TableRow.ToSharedRef();
}

FText SGraphActionMenu::GetFilterText() const
{
	// If there is an external source for the filter, use that text instead
	if(OnGetFilterText.IsBound())
	{
		return OnGetFilterText.Execute();
	}
	
	return FilterTextBox->GetText();;
}

void SGraphActionMenu::OnItemSelected( TSharedPtr< FGraphActionNode > InSelectedItem, ESelectInfo::Type SelectInfo )
{
	if (!bIgnoreUIUpdate)
	{
		// Filter out selection changes that should not trigger execution
		if( ( SelectInfo == ESelectInfo::OnMouseClick )  || ( SelectInfo == ESelectInfo::OnKeyPress ) || !InSelectedItem.IsValid())		
		{
			HandleSelection(InSelectedItem);
		}
	}
}

void SGraphActionMenu::OnItemDoubleClicked( TSharedPtr< FGraphActionNode > InClickedItem )
{
	if ( InClickedItem.IsValid() && !bIgnoreUIUpdate )
	{
		if ( InClickedItem->IsActionNode() )
		{
			OnActionDoubleClicked.ExecuteIfBound(InClickedItem->Actions);
		}
		else if (InClickedItem->Children.Num())
		{
			TreeView->SetItemExpansion(InClickedItem, !TreeView->IsItemExpanded(InClickedItem));
		}
	}
}

FReply SGraphActionMenu::OnItemDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// Start a function-call drag event for any entry that can be called by kismet
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		TArray< TSharedPtr<FGraphActionNode> > SelectedNodes = TreeView->GetSelectedItems();
		if(SelectedNodes.Num() > 0)
		{
			TSharedPtr<FGraphActionNode> Node = SelectedNodes[0];
			// Dragging a ctaegory
			if(Node.IsValid() && Node->IsCategoryNode())
			{
				if(OnCategoryDragged.IsBound())
				{
					return OnCategoryDragged.Execute(Node->Category, MouseEvent);
				}
			}
			// Dragging an action
			else
			{
				if(OnActionDragged.IsBound())
				{
					TArray< TSharedPtr<FEdGraphSchemaAction> > Actions;
					GetSelectedActions(Actions);
					return OnActionDragged.Execute(Actions, MouseEvent);
				}
			}
		}
	}

	return FReply::Unhandled();
}

bool SGraphActionMenu::OnMouseButtonDownEvent( TWeakPtr<FEdGraphSchemaAction> InAction )
{
	bool bResult = false;
	if( (!bIgnoreUIUpdate) && InAction.IsValid() )
	{
		TArray< TSharedPtr<FGraphActionNode> > SelectionList = TreeView->GetSelectedItems();
		TSharedPtr<FGraphActionNode> SelectedNode;
		if (SelectionList.Num() == 1)
		{	
			SelectedNode = SelectionList[0];			
		}
		else if (FilteredActionNodes.Num() == 1)
		{
			SelectedNode = FilteredActionNodes[0];			
		}
		if( ( SelectedNode.IsValid() ) && ( SelectedNode->Actions.Num() > 0 ) && ( SelectedNode->Actions[0].IsValid() ) )
		{
			if( SelectedNode->Actions[0].Get() == InAction.Pin().Get() )
			{				
				bResult = HandleSelection( SelectedNode );
			}
		}
	}
	return bResult;
}

FReply SGraphActionMenu::OnKeyDown( const FGeometry& MyGeometry, const FKeyboardEvent& KeyboardEvent )
{
	int32 SelectionDelta = 0;

	// Escape dismisses the menu without placing a node
	if (KeyboardEvent.GetKey() == EKeys::Escape)
	{
		FSlateApplication::Get().DismissAllMenus();
		return FReply::Handled();
	}
	else if ((KeyboardEvent.GetKey() == EKeys::Enter) && !bIgnoreUIUpdate)
	{
		return TryToSpawnActiveSuggestion() ? FReply::Handled() : FReply::Unhandled();
	}
	else if (FilteredActionNodes.Num() > 0)
	{
		// Up and down move thru the filtered node list
		if (KeyboardEvent.GetKey() == EKeys::Up)
		{
			SelectionDelta = -1;
		}
		else if (KeyboardEvent.GetKey() == EKeys::Down)
		{
			SelectionDelta = +1;
		}

		if (SelectionDelta != 0)
		{
			// If we have no selected suggestion then we need to use the items in the root to set the selection and set the focus
			if( SelectedSuggestion == INDEX_NONE )
			{
				SelectedSuggestion = (SelectedSuggestion + SelectionDelta + FilteredRootAction->Children.Num()) % FilteredRootAction->Children.Num();
				TGuardValue<bool> PreventSelectionFromTriggeringCommit(bIgnoreUIUpdate, true);
				TreeView->SetSelection(FilteredRootAction->Children[SelectedSuggestion], ESelectInfo::OnKeyPress);
				TreeView->RequestScrollIntoView(FilteredRootAction->Children[SelectedSuggestion]);
				return FReply::Handled().SetKeyboardFocus( SharedThis( TreeView.Get() ), EKeyboardFocusCause::WindowActivate );
			}

			//Move up or down one, wrapping around
			SelectedSuggestion = (SelectedSuggestion + SelectionDelta + FilteredActionNodes.Num()) % FilteredActionNodes.Num();

			MarkActiveSuggestion();

			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

void SGraphActionMenu::MarkActiveSuggestion()
{
	TGuardValue<bool> PreventSelectionFromTriggeringCommit(bIgnoreUIUpdate, true);

	if (SelectedSuggestion >= 0)
	{
		TSharedPtr<FGraphActionNode>& ActionToSelect = FilteredActionNodes[SelectedSuggestion];

		TreeView->SetSelection(ActionToSelect);
		TreeView->RequestScrollIntoView(ActionToSelect);
	}
	else
	{
		TreeView->ClearSelection();
	}
}

void SGraphActionMenu::AddReferencedObjects( FReferenceCollector& Collector )
{
	for (int32 CurTypeIndex=0; CurTypeIndex < AllActions.GetNumActions(); ++CurTypeIndex)
	{
		FGraphActionListBuilderBase::ActionGroup& Action = AllActions.GetAction( CurTypeIndex );

		for ( int32 ActionIndex = 0; ActionIndex < Action.Actions.Num(); ActionIndex++ )
		{
			Action.Actions[ActionIndex]->AddReferencedObjects(Collector);
		}
	}
}

bool SGraphActionMenu::HandleSelection( TSharedPtr< FGraphActionNode > &InSelectedItem )
{
	bool bResult = false;
	if( OnActionSelected.IsBound() )
	{
		if ( InSelectedItem.IsValid() && InSelectedItem->IsActionNode() )
		{
			OnActionSelected.Execute(InSelectedItem->Actions);
			bResult = true;
		}
		else
		{
			OnActionSelected.Execute(TArray< TSharedPtr<FEdGraphSchemaAction> >());
			bResult = true;
		}
	}
	return bResult;
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
