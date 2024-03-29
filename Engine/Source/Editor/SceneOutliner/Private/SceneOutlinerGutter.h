// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISceneOutliner.h"
#include "ISceneOutlinerColumn.h"
#include "SceneOutlinerInitializationOptions.h"

DECLARE_DELEGATE_TwoParams(FOnSetItemVisibility, TSharedRef<SceneOutliner::TOutlinerTreeItem>, bool)

/**
 * A gutter for the SceneOutliner which is capable of displaying a variety of Actor details
 */
class FSceneOutlinerGutter : public ISceneOutlinerColumn
{

public:

	/**	Constructor */
	FSceneOutlinerGutter(FOnSetItemVisibility InOnSetItemVisibility);

	// -----------------------------------------
	// ISceneOutlinerColumn Implementation
	virtual FName GetColumnID() OVERRIDE;

	virtual SHeaderRow::FColumn::FArguments ConstructHeaderRowColumn() OVERRIDE;

	virtual const TSharedRef< SWidget > ConstructRowWidget( const TSharedRef<SceneOutliner::TOutlinerTreeItem> TreeItem ) override;

	virtual bool ProvidesSearchStrings() OVERRIDE { return false; }

	virtual void PopulateActorSearchStrings(const AActor* const Actor, OUT TArray< FString >& OutSearchStrings) const OVERRIDE {}

	virtual bool SupportsSorting() const OVERRIDE { return true; }

	virtual void SortItems(TArray<TSharedPtr<SceneOutliner::TOutlinerTreeItem>>& RootItems, const EColumnSortMode::Type SortMode) const OVERRIDE;
	// -----------------------------------------

private:

	/** A delegate to execute when we need to set the visibility of an item */
	FOnSetItemVisibility OnSetItemVisibility;
};