// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "PropertyEditorModule.h"
#include "AssetData.h"
#include "PropertyHandle.h"

/////////////////////////////////////////////////////
// FAnimGraphNodeDetails 

class FAnimGraphNodeDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder) OVERRIDE;
	// End of IDetailCustomization interface

protected:
	// Hide any anim graph node properties; used when multiple are selected
	void AbortDisplayOfAllNodes(TArray< TWeakObjectPtr<UObject> >& SelectedObjectsList, class IDetailLayoutBuilder& DetailBuilder);

	// Creates a widget for the supplied property
	TSharedRef<SWidget> CreatePropertyWidget(UProperty* TargetProperty, TSharedRef<IPropertyHandle> TargetPropertyHandle);

	EVisibility GetVisibilityOfProperty(TSharedRef<IPropertyHandle> Handle) const;

	/** Delegate to handle filtering of asset pickers */
	bool OnShouldFilterAnimAsset( const FAssetData& AssetData ) const;

	/** Path to the current blueprints skeleton to allow us to filter asset pickers */
	FString TargetSkeletonName;
};

/////////////////////////////////////////////////////
// FInputScaleBiasCustomization

class FInputScaleBiasCustomization : public IStructCustomization
{
public:
	static TSharedRef<IStructCustomization> MakeInstance();

	// IStructCustomization interface
	virtual void CustomizeStructHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IStructCustomizationUtils& StructCustomizationUtils) OVERRIDE;
	virtual void CustomizeStructChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IStructCustomizationUtils& StructCustomizationUtils) OVERRIDE;
	// End of IStructCustomization interface

	FText GetMinValue(TSharedRef<class IPropertyHandle> StructPropertyHandle) const;
	FText GetMaxValue(TSharedRef<class IPropertyHandle> StructPropertyHandle) const;
	void OnMinValueCommitted(const FText& NewText, ETextCommit::Type CommitInfo, TSharedRef<class IPropertyHandle> StructPropertyHandle);
	void OnMaxValueCommitted(const FText& NewText, ETextCommit::Type CommitInfo, TSharedRef<class IPropertyHandle> StructPropertyHandle);
};

//////////////////////////////////////////////////////////////////////////
// FBoneReferenceCustomization

class FBoneReferenceCustomization : public IStructCustomization
{
public:
	static TSharedRef<IStructCustomization> MakeInstance();

	// IStructCustomization interface
	virtual void CustomizeStructHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IStructCustomizationUtils& StructCustomizationUtils) OVERRIDE;
	virtual void CustomizeStructChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IStructCustomizationUtils& StructCustomizationUtils) OVERRIDE;

private:
	// Storage object for bone hierarchy
	struct FBoneNameInfo
	{
		FBoneNameInfo(FName Name) : BoneName(Name) {}

		FName BoneName;
		TArray<TSharedPtr<FBoneNameInfo>> Children;
	};

	// Creates the combo button menu when clicked
	TSharedRef<SWidget> CreateSkeletonWidgetMenu( TSharedRef<IPropertyHandle> TargetPropertyHandle);
	// Using the current filter, repopulate the tree view
	void RebuildBoneList();
	// Make a single tree row widget
	TSharedRef<ITableRow> MakeTreeRowWidget(TSharedPtr<FBoneNameInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable);
	// Get the children for the provided bone info
	void GetChildrenForInfo(TSharedPtr<FBoneNameInfo> InInfo, TArray< TSharedPtr<FBoneNameInfo> >& OutChildren);

	// Called when the user changes the search filter
	void OnFilterTextChanged( const FText& InFilterText );
	void OnFilterTextCommitted( const FText& SearchText, ETextCommit::Type CommitInfo );
	// Called when the user selects a bone name
	void OnSelectionChanged(TSharedPtr<FBoneNameInfo>, ESelectInfo::Type SelectInfo);
	// Gets the current bone name, used to get the right name for the combo button
	FString GetCurrentBoneName() const;

	// Skeleton to search
	USkeleton* TargetSkeleton;

	// Base combo button 
	TSharedPtr<SComboButton> BonePickerButton;
	// Tree view used in the button menu
	TSharedPtr<STreeView<TSharedPtr<FBoneNameInfo>>> TreeView;

	// Tree info entries for bone picker
	TArray<TSharedPtr<FBoneNameInfo>> SkeletonTreeInfo;
	// Mirror of SkeletonTreeInfo but flattened for searching
	TArray<TSharedPtr<FBoneNameInfo>> SkeletonTreeInfoFlat;
	// Text to filter bone tree with
	FText FilterText;
	// Property to change after bone has been picked
	TSharedPtr<IPropertyHandle> BoneRefProperty;
};

//////////////////////////////////////////////////////////////////////////
// 

// Type used to identify rows in a parent player tree list
namespace EPlayerTreeViewEntryType
{
	enum Type
	{
		Blueprint,
		Graph,
		Node
	};
}

// Describes a single row entry in a player tree view
struct FPlayerTreeViewEntry : public TSharedFromThis<FPlayerTreeViewEntry>
{
	FPlayerTreeViewEntry(FString Name, EPlayerTreeViewEntryType::Type InEntryType, FAnimParentNodeAssetOverride* InOverride = NULL)
	: EntryName(Name)
	, EntryType(InEntryType)
	, Override(InOverride)
	{}

	FORCENOINLINE bool operator==(const FPlayerTreeViewEntry& Other);

	void GenerateNameWidget(TSharedPtr<SHorizontalBox> Box);

	// Name for the row
	FString EntryName;

	// What the row represents 
	EPlayerTreeViewEntryType::Type EntryType;

	// Node asset override for rows that represent nodes
	FAnimParentNodeAssetOverride* Override;

	// Children array for rows that represent blueprints and graphs.
	TArray<TSharedPtr<FPlayerTreeViewEntry>> Children;
};

class FAnimGraphParentPlayerDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance(TWeakPtr<FPersona> InPersona);

	virtual void CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder);

private:

	FAnimGraphParentPlayerDetails(TWeakPtr<FPersona> InPersona) : PersonaPtr(InPersona)
	{}

	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FPlayerTreeViewEntry> EventPtr, const TSharedRef< STableViewBase >& OwnerTable);
	void OnGetChildren(TSharedPtr<FPlayerTreeViewEntry> InParent, TArray< TSharedPtr<FPlayerTreeViewEntry> >& OutChildren);

	// Entries in the tree view
	TArray<TSharedPtr<FPlayerTreeViewEntry>> ListEntries;
	
	// Hosting Persona instance
	TWeakPtr<FPersona> PersonaPtr;

	// Editor meta-object containing override information
	UEditorParentPlayerListObj* EditorObject;
};

class SParentPlayerTreeRow : public SMultiColumnTableRow<TSharedPtr<FAnimGraphParentPlayerDetails>>
{
public:
	SLATE_BEGIN_ARGS(SParentPlayerTreeRow){}
		SLATE_ARGUMENT(TSharedPtr<FPlayerTreeViewEntry>, Item);
		SLATE_ARGUMENT(UEditorParentPlayerListObj*, OverrideObject);
		SLATE_ARGUMENT(TWeakPtr<FPersona>, Persona);
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName);

private:
	
	// Should an asset be filtered, ensures we only approve assets with matching skeletons
	bool OnShouldFilterAsset(const FAssetData& AssetData);

	// Sets the override asset when selected from the asset picker
	void OnAssetSelected(const UObject* Obj);

	void OnCloseMenu(){}

	// Called when the user clicks the focus button, opens a graph panel if necessary in
	// read only mode and focusses on the node.
	FReply OnFocusNodeButtonClicked();

	// Gets the current asset, either an override if one is selected or the original from the node.
	const UAnimationAsset* GetCurrentAssetToUse() const;

	// Whether or not we should show the reset to default button next to the asset picker
	EVisibility GetResetToDefaultVisibility() const;

	// Resets the selected asset override back to the original node's asset
	FReply OnResetButtonClicked();

	// Gets the full path to the current asset
	FString GetCurrentAssetPath() const;

	// Editor object containing all possible overrides
	UEditorParentPlayerListObj* EditorObject;

	// Tree item we are representing
	TSharedPtr<FPlayerTreeViewEntry> Item;

	// Graphnode this row represents, if any
	UAnimGraphNode_Base* GraphNode;

	// Persona editor pointer
	TWeakPtr<FPersona> Persona;
};
