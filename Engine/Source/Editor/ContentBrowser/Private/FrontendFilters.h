// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FrontendFilterBase.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

/** A filter that displays only checked out assets */
class FFrontendFilter_CheckedOut : public FFrontendFilter, public TSharedFromThis<FFrontendFilter_CheckedOut>
{
public:
	FFrontendFilter_CheckedOut(TSharedPtr<FFrontendFilterCategory> InCategory);

	// FFrontendFilter implementation
	virtual FString GetName() const OVERRIDE { return TEXT("CheckedOut"); }
	virtual FText GetDisplayName() const OVERRIDE { return LOCTEXT("FrontendFilter_CheckedOut", "Checked Out"); }
	virtual FText GetToolTipText() const OVERRIDE { return LOCTEXT("FrontendFilter_CheckedOutTooltip", "Show only assets that you have checked out or pending for add."); }
	virtual void ActiveStateChanged(bool bActive) OVERRIDE;

	// IFilter implementation
	virtual bool PassesFilter( AssetFilterType InItem ) const OVERRIDE;

private:
	
	/** Request the source control status for this filter */
	void RequestStatus();

	/** Callback when source control operation has completed */
	void SourceControlOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);
};

/** A filter that displays only modified assets */
class FFrontendFilter_Modified : public FFrontendFilter
{
public:
	FFrontendFilter_Modified(TSharedPtr<FFrontendFilterCategory> InCategory) : FFrontendFilter(InCategory) {}

	// FFrontendFilter implementation
	virtual FString GetName() const OVERRIDE { return TEXT("Modified"); }
	virtual FText GetDisplayName() const OVERRIDE { return LOCTEXT("FrontendFilter_Modified", "Modified"); }
	virtual FText GetToolTipText() const OVERRIDE { return LOCTEXT("FrontendFilter_ModifiedTooltip", "Show only assets that have been modified and not yet saved."); }

	// IFilter implementation
	virtual bool PassesFilter( AssetFilterType InItem ) const OVERRIDE;
};

/** A filter that displays blueprints that have replicated properties */
class FFrontendFilter_ReplicatedBlueprint : public FFrontendFilter
{
public:
	FFrontendFilter_ReplicatedBlueprint(TSharedPtr<FFrontendFilterCategory> InCategory) : FFrontendFilter(InCategory) {}

	// FFrontendFilter implementation
	virtual FString GetName() const OVERRIDE { return TEXT("ReplicatedBlueprint"); }
	virtual FText GetDisplayName() const OVERRIDE { return LOCTEXT("FFrontendFilter_ReplicatedBlueprint", "Replicated Blueprints"); }
	virtual FText GetToolTipText() const OVERRIDE { return LOCTEXT("FFrontendFilter_ReplicatedBlueprintToolTip", "Show only blueprints with replicated properties."); }

	// IFilter implementation
	virtual bool PassesFilter( AssetFilterType InItem ) const OVERRIDE;
};

/** An inverse filter that allows display of content in developer folders that are not the current user's */
class FFrontendFilter_ShowOtherDevelopers : public FFrontendFilter
{
public:
	/** Constructor */
	FFrontendFilter_ShowOtherDevelopers(TSharedPtr<FFrontendFilterCategory> InCategory);

	// FFrontendFilter implementation
	virtual FString GetName() const OVERRIDE { return TEXT("ShowOtherDevelopers"); }
	virtual FText GetDisplayName() const OVERRIDE { return LOCTEXT("FrontendFilter_ShowOtherDevelopers", "Other Developers"); }
	virtual FText GetToolTipText() const OVERRIDE { return LOCTEXT("FrontendFilter_ShowOtherDevelopersTooltip", "Allow display of assets in developer folders that aren't yours."); }
	virtual bool IsInverseFilter() const OVERRIDE { return true; }
	virtual void SetCurrentFilter(const FARFilter& InFilter) OVERRIDE;

	// IFilter implementation
	virtual bool PassesFilter( AssetFilterType InItem ) const OVERRIDE;

private:
	FString BaseDeveloperPath;
	FString UserDeveloperPath;
	bool bIsOnlyOneDeveloperPathSelected;
};

/** An inverse filter that allows display of object redirectors */
class FFrontendFilter_ShowRedirectors : public FFrontendFilter
{
public:
	/** Constructor */
	FFrontendFilter_ShowRedirectors(TSharedPtr<FFrontendFilterCategory> InCategory);

	// FFrontendFilter implementation
	virtual FString GetName() const OVERRIDE { return TEXT("ShowRedirectors"); }
	virtual FText GetDisplayName() const OVERRIDE { return LOCTEXT("FrontendFilter_ShowRedirectors", "Show Redirectors"); }
	virtual FText GetToolTipText() const OVERRIDE { return LOCTEXT("FrontendFilter_ShowRedirectorsToolTip", "Allow display of Redirectors."); }
	virtual bool IsInverseFilter() const OVERRIDE { return true; }
	virtual void SetCurrentFilter(const FARFilter& InFilter) OVERRIDE;

	// IFilter implementation
	virtual bool PassesFilter( AssetFilterType InItem ) const OVERRIDE;

private:
	bool bAreRedirectorsInBaseFilter;
	FName RedirectorClassName;
};

/** A filter that only displays assets used by loaded levels */
class FFrontendFilter_InUseByLoadedLevels : public FFrontendFilter
{
public:
	/** Constructor/Destructor */
	FFrontendFilter_InUseByLoadedLevels(TSharedPtr<FFrontendFilterCategory> InCategory);
	~FFrontendFilter_InUseByLoadedLevels();

	// FFrontendFilter implementation
	virtual FString GetName() const OVERRIDE { return TEXT("InUseByLoadedLevels"); }
	virtual FText GetDisplayName() const OVERRIDE { return LOCTEXT("FrontendFilter_InUseByLoadedLevels", "In Use By Level"); }
	virtual FText GetToolTipText() const OVERRIDE { return LOCTEXT("FrontendFilter_InUseByLoadedLevelsToolTip", "Show only assets that are currently in use by any loaded level."); }
	virtual void ActiveStateChanged(bool bActive) OVERRIDE;

	// IFilter implementation
	virtual bool PassesFilter( AssetFilterType InItem ) const OVERRIDE;

	/** Handler for when maps change in the editor */
	void OnEditorMapChange( uint32 MapChangeFlags );

private:
	bool bIsCurrentlyActive;
};
#undef LOCTEXT_NAMESPACE
