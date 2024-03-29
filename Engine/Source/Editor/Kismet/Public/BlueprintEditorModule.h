// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkit.h"
#include "WorkflowOrientedApp/WorkflowCentricApplication.h"

extern const FName BlueprintEditorAppName;

class FBlueprintEditor;

/**
 * Enum editor public interface
 */
class KISMET_API IUserDefinedEnumEditor : public FAssetEditorToolkit
{
};

/**
 * Enum editor public interface
 */
class KISMET_API IUserDefinedStructureEditor : public FAssetEditorToolkit
{
};

/**
 * Blueprint editor public interface
 */
class KISMET_API IBlueprintEditor : public FWorkflowCentricApplication
{
public:
	virtual void JumpToHyperlink(const UObject* ObjectReference, bool bRequestRename) = 0;

	/** Invokes the search UI and sets the mode and search terms optionally */
	virtual void SummonSearchUI(bool bSetFindWithinBlueprint, FString NewSearchTerms = FString(), bool bSelectFirstResult = false) = 0;

	virtual void RefreshEditors() = 0;

	virtual bool CanPasteNodes() const= 0;

	virtual void PasteNodesHere(class UEdGraph* Graph, const FVector2D& Location) = 0;

	virtual bool GetBoundsForSelectedNodes(class FSlateRect& Rect, float Padding ) = 0;

	/** Util to get the currently selected SCS editor tree Nodes */
	virtual TArray<TSharedPtr<class FSCSEditorTreeNode> >  GetSelectedSCSEditorTreeNodes() const = 0;

	/** Get number of currently selected nodes in the SCS editor tree */
	virtual int32 GetNumberOfSelectedNodes() const = 0;

	/** Find and select a specific SCS editor tree node associated with the given component */
	virtual TSharedPtr<class FSCSEditorTreeNode> FindAndSelectSCSEditorTreeNode(const class UActorComponent* InComponent, bool IsCntrlDown) = 0;

	/** Used to track node class creation ( event, call function, macro ) and the type */
	virtual void AnalyticsTrackNewNode( FName NodeClass, FName NodeType ) = 0;

};

DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<class ISCSEditorCustomization>, FSCSEditorCustomizationBuilder, TSharedRef< IBlueprintEditor > /* InBlueprintEditor */);

/**
 * The blueprint editor module provides the blueprint editor application.
 */
class FBlueprintEditorModule : public IModuleInterface,
	public IHasMenuExtensibility
{
public:
	// IModuleInterface interface
	virtual void StartupModule() OVERRIDE;
	virtual void ShutdownModule() OVERRIDE;
	// End of IModuleInterface interface

	/**
	 * Creates an instance of a Kismet editor object.  Only virtual so that it can be called across the DLL boundary.
	 *
	 * Note: This function should not be called directly, use one of the following instead:
	 *	- FKismetEditorUtilities::BringKismetToFocusAttentionOnObject
	 *  - FAssetEditorManager::Get().OpenEditorForAsset
	 *
	 * @param	Mode					Mode that this editor should operate in
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	Blueprint				The blueprint object to start editing
	 * @param	bShouldOpenInDefaultsMode	If true, the editor will open in defaults editing mode
	 *
	 * @return	Interface to the new Blueprint editor
	 */
	virtual TSharedRef<IBlueprintEditor> CreateBlueprintEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UBlueprint* Blueprint, bool bShouldOpenInDefaultsMode = false);
	virtual TSharedRef<IBlueprintEditor> CreateBlueprintEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, const TArray< UBlueprint* >& BlueprintsToEdit );

	/**
	 * Creates an instance of a Enum editor object.
	 *
	 * @param	Mode					Mode that this editor should operate in
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	UDEnum					The user-defined Enum to start editing
	 *
	 * @return	Interface to the new Enum editor
	 */
	virtual TSharedRef<IUserDefinedEnumEditor> CreateUserDefinedEnumEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UUserDefinedEnum* UDEnum);

	/**
	 * Creates an instance of a Structure editor object.
	 *
	 * @param	Mode					Mode that this editor should operate in
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	UDEnum					The user-defined structure to start editing
	 *
	 * @return	Interface to the new Struct editor
	 */
	virtual TSharedRef<IUserDefinedStructureEditor> CreateUserDefinedStructEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UUserDefinedStruct* UDStruct);

	/** Gets the extensibility managers for outside entities to extend blueprint editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() {return MenuExtensibilityManager;}

	/** 
	 * Register a customization for interacting with the SCS editor 
	 * @param	InComponentName			The name of the component to customize behavior for
	 * @param	InCustomizationBuilder	The delegate used to create customization instances
	 */
	virtual void RegisterSCSEditorCustomization(const FName& InComponentName, FSCSEditorCustomizationBuilder InCustomizationBuilder);

	/** 
	 * Unregister a previously registered customization for interacting with the SCS editor 
	 * @param	InComponentName			The name of the component to customize behavior for
	 */
	virtual void UnregisterSCSEditorCustomization(const FName& InComponentName);

	/** Delegate for binding functions to be called when the blueprint editor finishes getting created */
	DECLARE_EVENT_OneParam( FBlueprintEditorModule, FBlueprintEditorOpenedEvent, EBlueprintType );
	FBlueprintEditorOpenedEvent& OnBlueprintEditorOpened() { return BlueprintEditorOpened; }

private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;

	// Event to be called when the blueprint editor is opened
	FBlueprintEditorOpenedEvent BlueprintEditorOpened;

	/** Customizations for the SCS editor */
	TMap<FName, FSCSEditorCustomizationBuilder> SCSEditorCustomizations;
};
