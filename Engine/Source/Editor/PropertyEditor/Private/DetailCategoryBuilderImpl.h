// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class FDetailCategoryWidgetDecl;

/**
 * Defines a customization for a specific detail
 */
struct FDetailLayoutCustomization
{
	FDetailLayoutCustomization();
	/** The property node for the property detail */
	TSharedPtr<class FDetailPropertyRow> PropertyRow;
	/** A group of customizations */
	TSharedPtr<class FDetailGroup> DetailGroup;
	/** Custom Widget for displaying the detail */
	TSharedPtr<class FDetailWidgetRow> WidgetDecl;
	/** Custom builder for more complicated widgets */
	TSharedPtr<class FDetailCustomBuilderRow> CustomBuilderRow;
	/** @return true if this customization has a property node */
	bool HasPropertyNode() const { return GetPropertyNode().IsValid(); }
	/** @return true if this customization has a custom widget */
	bool HasCustomWidget() const { return WidgetDecl.IsValid(); }
	/** @return true if this customization has a custom builder (custom builders will set the custom widget */
	bool HasCustomBuilder() const { return CustomBuilderRow.IsValid(); }
	/** @return true if this customization has a group */
	bool HasGroup() const { return DetailGroup.IsValid(); }
	/** @return true if this customization is valid */
	bool IsValidCustomization() const { return HasPropertyNode() || HasCustomWidget() || HasCustomBuilder() || HasGroup(); }
	/** @return the property node for this customization (if any ) */
	TSharedPtr<FPropertyNode> GetPropertyNode() const;
	/** @return The row to display from this customization */
	FDetailWidgetRow GetWidgetRow() const;
};

typedef TArray<FDetailLayoutCustomization> FCustomizationList;

class FDetailLayout
{
public:
	FDetailLayout( FName InInstanceName )
		: InstanceName( InInstanceName )
	{}

	void AddCustomLayout( const FDetailLayoutCustomization& Layout, bool bAdvanced );
	void AddDefaultLayout( const FDetailLayoutCustomization& Layout, bool bAdvanced );

	const FCustomizationList& GetCustomSimpleLayouts() const { return CustomSimpleLayouts; }
	const FCustomizationList& GetCustomAdvancedLayouts() const { return CustomAdvancedLayouts; }
	const FCustomizationList& GetDefaultSimpleLayouts() const { return DefaultSimpleLayouts; }
	const FCustomizationList& GetDefaultAdvancedLayouts() const { return DefaultAdvancedLayouts; }

	bool HasAdvancedLayouts() const { return CustomAdvancedLayouts.Num() > 0 || DefaultAdvancedLayouts.Num() > 0; }

	FName GetInstanceName() const { return InstanceName; }

private:
	void AddLayoutInternal( const FDetailLayoutCustomization& Layout, FCustomizationList& ListToUse );
private:
	/** Customized layouts that appear in the simple (visible by default) area of a category */
	FCustomizationList CustomSimpleLayouts;
	/** Customized layouts that appear in the advanced (hidden by default) details area of a category */
	FCustomizationList CustomAdvancedLayouts;
	/** Default layouts that appear in the simple (visible by default) details area of a category */
	FCustomizationList DefaultSimpleLayouts;
	/** Default layouts that appear in the advanced (visible by default) details area of a category */
	FCustomizationList DefaultAdvancedLayouts;
	/** The sort order in which this layout is displayed (lower numbers are displayed first) */
	FName InstanceName;
};


class FDetailLayoutMap
{
public:
	FDetailLayoutMap()
		: bContainsBaseInstance( false )
	{}

	FDetailLayout& FindOrAdd( FName InstanceName )
	{
		for( int32 LayoutIndex = 0; LayoutIndex < Layouts.Num(); ++LayoutIndex )
		{
			FDetailLayout& Layout = Layouts[LayoutIndex];
			if( Layout.GetInstanceName() == InstanceName )
			{
				return Layout;
			}
		}

		bContainsBaseInstance |= (InstanceName == NAME_None);

		int32 Index = Layouts.Add( FDetailLayout( InstanceName ) );

		return Layouts[Index];
	}

	/**
	 * @return The number of layouts
	 */
	int32 Num() const { return Layouts.Num(); }

	/**
	 * @return Gets a layout at a specific instance
	 */
	const FDetailLayout& operator[]( int32 Index ) const { return Layouts[Index]; }

	/**
	 * @return Whether or not we need to display a group border around a list of details.
	 */
	bool ShouldShowGroup( FName RequiredGroupName ) const
	{
		// Should show the group if the group name is not empty and there are more than two entries in the list where one of them is not the default "none" entry (represents the base object)
		return RequiredGroupName != NAME_None && Layouts.Num() > 1 && ( Layouts.Num() > 2 || !bContainsBaseInstance );
	}
private:
	TArray<FDetailLayout> Layouts;
	bool bContainsBaseInstance;
};


/**
 * Detail category implementation
 */
class FDetailCategoryImpl : public IDetailCategoryBuilder, public IDetailTreeNode, public TSharedFromThis<FDetailCategoryImpl>
{
public: 
	FDetailCategoryImpl( FName InCategoryName, TSharedRef<FDetailLayoutBuilderImpl> InDetalLayout );
	~FDetailCategoryImpl();

	/** IDetailCategoryBuilder interface */
	virtual IDetailCategoryBuilder& InitiallyCollapsed( bool bShouldBeInitiallyCollapsed ) OVERRIDE;
	virtual IDetailCategoryBuilder& OnExpansionChanged( FOnBooleanValueChanged InOnExpansionChanged ) OVERRIDE;
	virtual IDetailCategoryBuilder& RestoreExpansionState( bool bRestore ) OVERRIDE;
	virtual IDetailCategoryBuilder& HeaderContent( TSharedRef<SWidget> InHeaderContent ) OVERRIDE;
	virtual IDetailPropertyRow& AddProperty( FName PropertyPath, UClass* ClassOuter = NULL, FName InstanceName = NAME_None,  EPropertyLocation::Type Location = EPropertyLocation::Default ) OVERRIDE;
	virtual IDetailPropertyRow& AddProperty( TSharedPtr<IPropertyHandle> PropertyHandle,  EPropertyLocation::Type Location = EPropertyLocation::Default ) OVERRIDE;
	virtual IDetailPropertyRow* AddExternalProperty( const TArray<UObject*>& Objects, FName PropertyName, EPropertyLocation::Type Location = EPropertyLocation::Default ) OVERRIDE;
	virtual IDetailLayoutBuilder& GetParentLayout() const OVERRIDE { return *DetailLayoutBuilder.Pin(); }
	virtual FDetailWidgetRow& AddCustomRow( const FString& FilterString, bool bForAdvanced = false ) OVERRIDE;
	virtual void AddCustomBuilder( TSharedRef<IDetailCustomNodeBuilder> InCustomBuilder, bool bForAdvanced = false ) OVERRIDE;
	virtual IDetailGroup& AddGroup( FName GroupName, const FString& LocalizedDisplayName, bool bForAdvanced = false ) OVERRIDE;
	virtual void GetDefaultProperties( TArray<TSharedRef<IPropertyHandle> >& OutAllProperties, bool bSimpleProperties = true, bool bAdvancedProperties = true ) OVERRIDE;
	virtual const FString& GetDisplayName() const OVERRIDE { return DisplayName; }

	/** IDetailTreeNode interface */
	virtual SDetailsView& GetDetailsView() const OVERRIDE { return DetailLayoutBuilder.Pin()->GetDetailsView(); }
	virtual TSharedRef< ITableRow > GenerateNodeWidget( const TSharedRef<STableViewBase>& OwnerTable, const FDetailColumnSizeData& ColumnSizeData, const TSharedRef<IPropertyUtilities>& PropertyUtilities ) OVERRIDE;
	virtual void GetChildren( TArray< TSharedRef<IDetailTreeNode> >& OutChildren ) OVERRIDE;
	virtual bool ShouldBeExpanded() const OVERRIDE;
	virtual ENodeVisibility::Type GetVisibility() const OVERRIDE;
	virtual void FilterNode( const FDetailFilter& DetailFilter ) OVERRIDE;
	virtual void Tick( float DeltaTime ) OVERRIDE {}
	virtual bool ShouldShowOnlyChildren() const OVERRIDE { return false; }
	virtual FName GetNodeName() const OVERRIDE { return GetCategoryName(); }

	/**
	 * @return The name of the category
	 */
	FName GetCategoryName() const { return CategoryName; }

	/**
	 * @return The parent detail layout builder for this category
	 */
	FDetailLayoutBuilderImpl& GetParentLayoutImpl() const { return *DetailLayoutBuilder.Pin(); }

	/**
	 * Generates the children for this category                                                              
	 */
	void GenerateLayout();

	/**
	 * Adds a property node to the default category layout
	 *
	 * @param PropertyNode			The property node to add
	 * @param InstanceName			The name of the property instance (for duplicate properties of the same type)
	 */
	void AddPropertyNode( TSharedRef<FPropertyNode> PropertyNode, FName InstanceName );

	/** 
	 * Sets the sort order for this category
	 */
	void SetSortOrder( int32 InOrder ) { SortOrder = InOrder; }

	/**
	 * Gets the sort order for this category
	 */
	int32 GetSortOrder() const { return SortOrder; }

	/**
	 * Sets the display name of the category string 
	 *
	 * @param CategoryName			Base category name to use if no localized override is set
	 * @param LocalizedNameOverride	The localized name override to use (can be empty)
	 */
	void SetDisplayName( FName CategoryName, const FString& LocalizedNameOverride );

	/**
	 * Request that a child node of this category be expanded or collapsed
	 *
	 * @param TreeNode				The node to expand or collapse
	 * @param bShouldBeExpanded		True if the node should be expanded, false to collapse it	
	 */
	void RequestItemExpanded( TSharedRef<IDetailTreeNode> TreeNode, bool bShouldBeExpanded );

	/**
	 * Notifies the tree view that it needs to be refreshed 
	 *
	 * @param bRefilterCategory True if the category should be refiltered
	 */
	void RefreshTree( bool bRefilterCategory );

	/**
	 * Adds a node that needs to be ticked
	 *
	 * @param TickableNode	The node that needs to be ticked
	 */
	void AddTickableNode( IDetailTreeNode& TickableNode );

	/**
	 * Removes a node that no longer needs to be ticked
	 *
	 * @param TickableNode	The node that no longer needs to be ticked
	 */
	void RemoveTickableNode( IDetailTreeNode& TickableNode );

	/** @return The category path for this category */
	const FString& GetCategoryPathName() const { return CategoryPathName; }
	
	/**
	 * Saves the expansion state of a tree node in this category
	 *
	 * @param InTreeNode	The node to save expansion state from
	 */
	void SaveExpansionState( IDetailTreeNode& InTreeNode );

	/**
	 * Gets the saved expansion state of a tree node in this category	
	 *
	 * @param InTreeNode	The node to get expansion state for
	 * @return true if the node should be expanded, false otherwise
	 */	
	bool GetSavedExpansionState( IDetailTreeNode& InTreeNode ) const;

	/** @return true if this category only contains advanced properties */
	bool ContainsOnlyAdvanced() const;

	/**
	 * Called when the advanced dropdown button is clicked 
	 */
	void OnAdvancedDropdownClicked();

private:
	virtual void OnItemExpansionChanged( bool bIsExpanded ) OVERRIDE;

	/**
	 * Adds a new filter widget to this category (for checking if anything is visible in the category when filtered)
	 */
	void AddFilterWidget( TSharedRef<SWidget> InWidget );

	/**
	 * Generates children for each layout                                                              
	 */
	void GenerateChildrenForLayouts();

	/**
	 * Generates nodes from a list of customization in a single layout
	 *
	 * @param InCustomizationList	The list of customizations to generate nodes from
	 * @param OutNodeList			The generated nodes
	 * @param bDefaultLayouts		True if we are generating a default layout
	 */
	void GenerateNodesFromCustomizations( const FCustomizationList& InCustomizationList, bool bDefaultLayouts, FDetailNodeList& OutNodeList, bool &bOutHasMultipleColumns );

	/**
	 * Generates nodes from a list of customization in a single layout
	 *
	 * @param RequiredGroupName 	If valid the children will be surrounded by a group
	 * @param bDefaultLayout	True if we are generating a default layout
	 * @param bNeedsGroup		True if the children need to be grouped
	 * @param LayoutList		The list of customizations to generate nodes from 
         * @param OutChildren		The generated nodes
	 */
	bool GenerateChildrenForSingleLayout( const FName RequiredGroupName, bool bDefaultLayout, bool bNeedsGroup, const FCustomizationList& LayoutList, FDetailNodeList& OutChildren, bool& bOutHasMultipleColumns );

	/**
	 * @return Whether or not a customization should appear in the advanced section of the category by default
	 */
	bool IsAdvancedLayout( const FDetailLayoutCustomization& LayoutInfo );
	
	/**
	 * Adds a custom layout to this category
	 * 
	 * @param LayoutInfo	The custom layout information
	 * @param bForAdvanced	Whether or not the custom layout should appear in the advanced section of the category
	 */
	void AddCustomLayout( const FDetailLayoutCustomization& LayoutInfo, bool bForAdvanced );

	/**
	 * Adds a default layout to this category
	 * 
	 * @param DefaultLayoutInfo		The layout information
	 * @param bForAdvanced			Whether or not the layout should appear in the advanced section of the category
	 */
	void AddDefaultLayout( const FDetailLayoutCustomization& DefaultLayoutInfo, bool bForAdvanced, FName InstanceName );
	
	/**
	 * Returns the layout for a given object instance name
	 *
	 * @param InstanceName the name of the instance to get the layout for
	 */
	FDetailLayout& GetLayoutForInstance( FName InstanceName );

	/**
	 * @return True of we should show the advanced button
	 */
	bool ShouldShowAdvanced() const;

	/**
	 * @return true if the advaned dropdown button is enabled
	 */
	bool IsAdvancedDropdownEnabled() const;

	/**
	 * @return the visibility of the advanced help text drop down (it is visible in a category if there are no simple properties)
	 */
	EVisibility GetAdvancedHelpTextVisibility() const;

	/**
	 * @return true if the parent that hosts us is enabled
	 */
	bool IsParentEnabled() const;

private:
	/** Layouts that appear in this category category */
	FDetailLayoutMap LayoutMap;
	/** All Simple child nodes */
	TArray< TSharedRef<IDetailTreeNode> > SimpleChildNodes;
	/** All Advanced child nodes */
	TArray< TSharedRef<IDetailTreeNode> > AdvancedChildNodes;
	/** Advanced dropdown node (always shown) */
	TSharedPtr<IDetailTreeNode> AdvancedDropdownNodeBottom;
	/** Advanced dropdown node that is shown if the advanced dropdown is expanded */
	TSharedPtr<IDetailTreeNode> AdvancedDropdownNodeTop;
	/** Delegate called when expansion of the category changes */
	FOnBooleanValueChanged OnExpansionChangedDelegate;
	/** The display name of the category */
	FString DisplayName;
	/** The path name of the category */
	FString CategoryPathName;
	/** Custom header content displayed to the right of the category name */
	TSharedPtr<SWidget> HeaderContentWidget;
	/** The parent detail builder */
	TWeakPtr<FDetailLayoutBuilderImpl> DetailLayoutBuilder;
	/** The category identifier */
	FName CategoryName;
	/** The sort order of this category (amongst all categories) */
	int32 SortOrder;
	/** Whether or not to restore the expansion state between sessions */
	bool bRestoreExpansionState;
	/** Whether or not the category should be initially collapsed */
	bool bShouldBeInitiallyCollapsed;
	/** Whether or not advanced properties should be shown (as specified by the user) */
	bool bUserShowAdvanced;
	/** Whether or not advanced properties are forced to be shown (this is an independent toggle from bShowAdvanced which is user driven)*/
	bool bForceAdvanced;
	/** Whether or not the content in the category is being filtered */
	bool bHasFilterStrings;
	/** true if anything is visible in the category */
	bool bHasVisibleDetails;
};
