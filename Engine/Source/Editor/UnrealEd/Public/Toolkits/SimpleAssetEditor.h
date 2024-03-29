// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseToolkit.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"

class UNREALED_API FSimpleAssetEditor : public FAssetEditorToolkit
{
public:
	/** Delegate that, given an array of assets, returns an array of objects to use in the details view of an FSimpleAssetEditor */
	DECLARE_DELEGATE_RetVal_OneParam(TArray<UObject*>, FGetDetailsViewObjects, const TArray<UObject*>&);

	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) OVERRIDE;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) OVERRIDE;


	/**
	 * Edits the specified asset object
	 *
	 * @param	Mode					Asset editing mode for this editor (standalone or world-centric)
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	ObjectsToEdit			The object to edit
	 * @param	GetDetailsViewObjects	If bound, a delegate to get the array of objects to use in the details view; uses ObjectsToEdit if not bound
	 */
	void InitEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, const TArray<UObject*>& ObjectsToEdit, FGetDetailsViewObjects GetDetailsViewObjects );

	/** Destructor */
	virtual ~FSimpleAssetEditor();

	/** IToolkit interface */
	virtual FName GetToolkitFName() const OVERRIDE;
	virtual FText GetBaseToolkitName() const OVERRIDE;
	virtual FText GetToolkitName() const OVERRIDE;
	virtual FString GetWorldCentricTabPrefix() const OVERRIDE;
	virtual FLinearColor GetWorldCentricTabColorScale() const OVERRIDE;
	
	/** Used to show or hide certain properties */
	void SetPropertyVisibilityDelegate(FIsPropertyVisible InVisibilityDelegate);

private:
	/** Create the properties tab and its content */
	TSharedRef<SDockTab> SpawnPropertiesTab( const FSpawnTabArgs& Args );

	/** Dockable tab for properties */
	TSharedPtr< SDockableTab > PropertiesTab;

	/** Details view */
	TSharedPtr< class IDetailsView > DetailsView;

	/** App Identifier. Technically, all simple editors are the same app, despite editing a variety of assets. */
	static const FName SimpleEditorAppIdentifier;

	/**	The tab ids for all the tabs used */
	static const FName PropertiesTabId;

public:
	static TSharedRef<FSimpleAssetEditor> CreateEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UObject* ObjectToEdit, FGetDetailsViewObjects GetDetailsViewObjects = FGetDetailsViewObjects() );

	static TSharedRef<FSimpleAssetEditor> CreateEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, const TArray<UObject*>& ObjectsToEdit, FGetDetailsViewObjects GetDetailsViewObjects = FGetDetailsViewObjects() );
};
