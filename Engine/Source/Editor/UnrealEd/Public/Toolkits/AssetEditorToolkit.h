// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseToolkit.h"
#include "Toolkits/AssetEditorManager.h"		// For IAssetEditorInstance derive



DECLARE_DELEGATE_RetVal( bool, FRequestAssetEditorClose );

/**
 * Base class for toolkits that are used for asset editing (abstract)
 */
class UNREALED_API FAssetEditorToolkit : public IAssetEditorInstance, public FBaseToolkit, public TSharedFromThis< FAssetEditorToolkit >
{

public:

	/** Default constructor */
	FAssetEditorToolkit();

	/**
	 * Initializes this asset editor.  Called immediately after construction.  If you override this, remember to
	 * call the base class implementation
	 *
	 * @param	Mode					Asset editing mode for this editor (standalone or world-centric)
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	AppIdentifier			When Mode is Standalone, this is the app identifier of the app that should host this toolkit
	 * @param	StandaloneDefaultLayout	The default layout for a standalone asset editor
	 * @param	bCreateDefaultToolbar	The default toolbar, which can be extended
	 * @param	bCreateDefaultStandaloneMenu	True if in standalone mode, the asset editor should automatically generate a default "asset" menu, or false if you're going to do this yourself in your derived asset editor's implementation
	 * @param	ObjectToEdit			The object to edit
	 * @param	bInIsToolbarFocusable	Whether the buttons on the default toolbar can receive keyboard focus
	 */
	virtual void InitAssetEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, const FName AppIdentifier, const TSharedRef<FTabManager::FLayout>& StandaloneDefaultLayout, const bool bCreateDefaultStandaloneMenu, const bool bCreateDefaultToolbar, const TArray<UObject*>& ObjectsToEdit, const bool bInIsToolbarFocusable = false);
	virtual void InitAssetEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, const FName AppIdentifier, const TSharedRef<FTabManager::FLayout>& StandaloneDefaultLayout, const bool bCreateDefaultStandaloneMenu, const bool bCreateDefaultToolbar, UObject* ObjectToEdit, const bool bInIsToolbarFocusable = false);

	/** Virtual destructor, so that we can clean up our app when destroyed */
	virtual ~FAssetEditorToolkit();

	/** IToolkit interface */
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) OVERRIDE;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) OVERRIDE;
	virtual bool IsAssetEditor() const OVERRIDE;
	virtual const TArray< UObject* >* GetObjectsCurrentlyBeingEdited() const OVERRIDE;
	virtual FName GetToolkitFName() const = 0;					// Must implement in derived class!
	virtual FText GetBaseToolkitName() const = 0;				// Must implement in derived class!
	virtual FText GetToolkitName() const OVERRIDE;		
	virtual FString GetWorldCentricTabPrefix() const = 0;		// Must implement in derived class!
	virtual class FEdMode* GetEditorMode() const OVERRIDE;

	/** IAssetEditorInstance interface */
	virtual FName GetEditorName() const OVERRIDE;
	virtual void FocusWindow(UObject* ObjectToFocusOn = NULL) OVERRIDE;
	virtual bool CloseWindow() OVERRIDE;

	/**
	 * Fills in the supplied menu with commands for working with this asset file
	 *
	 * @param	MenuBuilder		The menu to add commands to
	 */
	void FillDefaultFileMenuCommands( FMenuBuilder& MenuBuilder );

	/**
	 * Fills in the supplied menu with commands for modifying this asset that are generally common to most asset editors
	 *
	 * @param	MenuBuilder		The menu to add commands to
	 */
	void FillDefaultAssetMenuCommands( FMenuBuilder& MenuBuilder );

	/**
	 * Fills in the supplied menu with commands for the help menu
	 *
	 * @param	MenuBuilder		The menu to add commands to
	 */
	void FillDefaultHelpMenuCommands( FMenuBuilder& MenuBuilder );

	/** @return	For standalone asset editing tool-kits, returns the toolkit host that was last hosting this asset editor before it was switched to standalone mode (if it's still valid.)  Returns null if these conditions aren't met. */
	TSharedPtr< IToolkitHost > GetPreviousWorldCentricToolkitHost();

	/**
	 * Static: Used internally to set the world-centric toolkit host for a newly-created standalone asset editing toolkit
	 *
	 * @param ToolkitHost	The tool-kit to use if/when this toolkit is switched back to world-centric mode
	 */
	static void SetPreviousWorldCentricToolkitHostForNewAssetEditor( TSharedRef< IToolkitHost > ToolkitHost );

	/** Applies the passed in layout (or the saved user-modified version if available).  Must be called after InitAssetEditor. */
	void RestoreFromLayout( const TSharedRef<FTabManager::FLayout>& NewLayout );

	/** @return Returns this asset editor's tab manager object.  May be NULL for non-standalone toolkits */
	TSharedPtr<FTabManager> GetTabManager()
	{
		return TabManager;
	}
	
	/** Makes a default asset editing toolbar */
	void GenerateToolbar();
	
	/** Regenerates the menubar and toolbar widgets */
	void RegenerateMenusAndToolbars();

	/** Called at the end of RegenerateMenusAndToolbars() */
	virtual void PostRegenerateMenusAndToolbars() { }
	
	/** Adds or removes extenders to the default menu or the toolbar menu this asset editor */
	void AddMenuExtender(TSharedPtr<FExtender> Extender);
	void RemoveMenuExtender(TSharedPtr<FExtender> Extender);
	void AddToolbarExtender(TSharedPtr<FExtender> Extender);
	void RemoveToolbarExtender(TSharedPtr<FExtender> Extender);

	/**
	 * Allows the caller to set a menu overlay, displayed to the far right of the editor's menu bar
	 *
	 * @param Widget The widget to use as the overlay
	 */
	void SetMenuOverlay( TSharedRef<SWidget> Widget );

	/** Adds or removes widgets from the default toolbar in this asset editor */
	void AddToolbarWidget(TSharedRef<SWidget> Widget);
	void RemoveAllToolbarWidgets();

	/** Gets the toolbar tab id */
	FName GetToolbarTabId() const {return ToolbarTabId;}

	/** True if this actually is editing an asset */
	bool IsActuallyAnAsset() const;

protected:

	/**	Returns the single object currently being edited. Asserts if currently editing no object or multiple objects */
	UObject* GetEditingObject() const;

	/**	Returns an array of all the objects currently being edited. Asserts if editing no objects */
	const TArray< UObject* >& GetEditingObjects() const;

	/** Adds an item to the Editing Objects list */
	virtual void AddEditingObject(UObject* Object);

	/** Removes an item from the Editing Objects list */
	virtual void RemoveEditingObject(UObject* Object);

	/** Called to test if "Save" should be enabled for this asset */
	virtual bool CanSaveAsset() const {return true;}

	/** Called when "Save" is clicked for this asset */
	virtual void SaveAsset_Execute();

	/** Called when "Find in Content Browser" is clicked for this asset */
	virtual void FindInContentBrowser_Execute();

	/** Called when "Browse Documentation" is clicked for this asset */
	virtual void BrowseDocumentation_Execute() const;

	/** @return the documentation location for this editor */
	virtual FString GetDocumentationLink() const;

	/** Called to check to see if there's an asset capable of being reimported */
	virtual bool CanReimport() const;
	virtual bool CanReimport( UObject* EditingObject ) const;

	/** Called when "Reimport" is clicked for this asset */
	virtual void Reimport_Execute();
	virtual void Reimport_Execute( UObject* EditingObject );

	/** Called to determine if the user should be prompted for a new file if one is missing during an asset reload */
	virtual bool ShouldPromptForNewFilesOnReload(const UObject& object) const;

	/** Called when this toolkit would close */
	virtual bool OnRequestClose() {return true;}
		
	/** 
	  * Static: Called when "Switch to Standalone Editor" is clicked for the asset editor
	  *
	  * @param ThisToolkitWeakRef	The toolkit that we want to restart in standalone mode
	  */
	static void SwitchToStandaloneEditor_Execute( TWeakPtr< FAssetEditorToolkit > ThisToolkitWeakRef );

	/** 
	  * Static: Called when "Switch to World-Centric Editor" is clicked for the asset editor
	  *
	  * @param	ThisToolkitWeakRef			The toolkit that we want to restart in world-centric mode
	  */
	static void SwitchToWorldCentricEditor_Execute( TWeakPtr< FAssetEditorToolkit > ThisToolkitWeakRef );

	/** @return a pointer to the brush to use for the tab icon */
	virtual const FSlateBrush* GetDefaultTabIcon() const;
	
private:
	/** Spawns the toolbar tab */
	TSharedRef<SDockTab> SpawnTab_Toolbar(const FSpawnTabArgs& Args);

protected:

	/** For standalone asset editing tool-kits that were switched from world-centric mode on the fly, this stores
	    the toolkit host (level editor) that hosted this toolkit last.  This is used to allow the user to switch the
		toolkit back to world-centric mode. */
	TWeakPtr< IToolkitHost > PreviousWorldCentricToolkitHost;

	/** Controls our internal layout */
	TSharedPtr<FTabManager> TabManager;

private:
	/** The toolkit standalone host; may be NULL */
	TWeakPtr< class SStandaloneAssetEditorToolkitHost > StandaloneHost;

	/** Static: World centric toolkit host to use for the next created asset editing toolkit */
	static TWeakPtr< IToolkitHost > PreviousWorldCentricToolkitHostForNewAssetEditor;

	/** The object we're currently editing */
	// @todo toolkit minor: Currently we don't need to serialize this object reference because the FAssetEditorManager is kept in sync (and will always serialize it.)
	TArray<UObject*> EditingObjects;
	
	/** Asset Editor Default Toolbar */
	TSharedPtr<class SWidget> Toolbar;

	/** The widget that will house the default Toolbar widget */
	TSharedPtr<SBorder> ToolbarWidgetContent;

	/** The menu extenders to populate the main toolbar with */
	TArray< TSharedPtr<FExtender> > ToolbarExtenders;

	/** Additional widgets to be added to the toolbar */
	TArray< TSharedRef<SWidget> > ToolbarWidgets;

	/** Whether the buttons on the default toolbar can receive keyboard focus */
	bool bIsToolbarFocusable;

	/**	The tab ids for all the tabs used */
	static const FName ToolbarTabId;
};



DECLARE_DELEGATE_RetVal_TwoParams( TSharedRef<FExtender>, FAssetEditorExtender, const TSharedRef<FUICommandList>, const TArray<UObject*>);

/**
 * Extensibility managers simply keep a series of FExtenders for a single menu/toolbar/anything
 * It is here to keep a standardized approach to editor extensibility among modules
 */
class UNREALED_API FExtensibilityManager
{
public:
	/** Functions for outsiders to add or remove their extenders */
	virtual void AddExtender(TSharedPtr<FExtender> Extender) {Extenders.AddUnique(Extender);}
	virtual void RemoveExtender(TSharedPtr<FExtender> Extender) {Extenders.Remove(Extender);}
	
	/** Gets all extender delegates for this manager */
	virtual TArray<FAssetEditorExtender>& GetExtenderDelegates() {return ExtenderDelegates;}

	/** Gets all extenders, consolidated, for use by the editor to be extended */
	virtual TSharedPtr<FExtender> GetAllExtenders();
	/** Gets all extenders and asset editor extenders from delegates consolidated */
	virtual TSharedPtr<FExtender> GetAllExtenders(const TSharedRef<FUICommandList>& CommandList, const TArray<UObject*>& ContextSensitiveObjects);
	
private:
	/** A list of extenders the editor will use */
	TArray<TSharedPtr<FExtender>> Extenders;
	/** A list of extender delegates the editor will use */
	TArray<FAssetEditorExtender> ExtenderDelegates;
};



/** Indicates that a class has a default menu that is extensible */
class IHasMenuExtensibility
{
public:
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() = 0;
};

/** Indicates that a class has a default toolbar that is extensible */
class IHasToolBarExtensibility
{
public:
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() = 0;
};
