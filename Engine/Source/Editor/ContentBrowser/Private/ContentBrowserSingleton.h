// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once

class SContentBrowser;

#define MAX_CONTENT_BROWSERS 4

/**
 * Content browser module singleton implementation class
 */
class FContentBrowserSingleton : public IContentBrowserSingleton
{
public:
	/** Constructor, Destructor */
	FContentBrowserSingleton();
	virtual ~FContentBrowserSingleton();

	// IContentBrowserSingleton interface
	virtual TSharedRef<class SWidget> CreateAssetPicker(const FAssetPickerConfig& AssetPickerConfig) OVERRIDE;
	virtual TSharedRef<class SWidget> CreatePathPicker(const FPathPickerConfig& PathPickerConfig) OVERRIDE;
	virtual TSharedRef<class SWidget> CreateCollectionPicker(const FCollectionPickerConfig& CollectionPickerConfig) OVERRIDE;
	virtual bool HasPrimaryContentBrowser() const OVERRIDE;
	virtual void FocusPrimaryContentBrowser(bool bFocusSearch) OVERRIDE;
	virtual void CreateNewAsset(const FString& DefaultAssetName, const FString& PackagePath, UClass* AssetClass, UFactory* Factory) OVERRIDE;
	virtual void SyncBrowserToAssets(const TArray<class FAssetData>& AssetDataList, bool bAllowLockedBrowsers = false) OVERRIDE;
	virtual void SyncBrowserToAssets(const TArray<UObject*>& AssetList, bool bAllowLockedBrowsers = false) OVERRIDE;
	virtual void GetSelectedAssets(TArray<FAssetData>& SelectedAssets) OVERRIDE;

		/** Gets the content browser singleton as a FContentBrowserSingleton */
	static FContentBrowserSingleton& Get();
	
	/** Sets the current primary content browser. */
	void SetPrimaryContentBrowser(const TSharedRef<SContentBrowser>& NewPrimaryBrowser);

	/** Notifies the singleton that a browser was closed */
	void ContentBrowserClosed(const TSharedRef<SContentBrowser>& ClosedBrowser);

private:

	/** 
	 * Delegate handlers
	 **/
	void OnEditorLoadSelectedAssetsIfNeeded();

	/** Sets the primary content browser to the next valid browser in the list of all browsers */
	void ChooseNewPrimaryBrowser();

	/** Gives focus to the specified content browser */
	void FocusContentBrowser(const TSharedPtr<SContentBrowser>& BrowserToFocus);

	/** Summons a new content browser */
	void SummonNewBrowser(bool bAllowLockedBrowsers = false);

	/** Handler for when a property changes on any object */
	void OnObjectPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent);

	/** Handler for a request to spawn a new content browser tab */
	TSharedRef<SDockTab> SpawnContentBrowserTab( const FSpawnTabArgs& SpawnTabArgs, int32 BrowserIdx );

	/** Handler for a request to spawn a new content browser tab */
	FText GetContentBrowserTabLabel(int32 BrowserIdx);

	/** Returns true if this content browser is locked (can be used even when closed) */
	bool IsLocked(const FName& InstanceName) const;

	/** Returns a localized name for the tab/menu entry with index */
	static FText GetContentBrowserLabelWithIndex( int32 BrowserIdx );

public:
	/** The tab identifier/instance name for content browser tabs */
	FName ContentBrowserTabIDs[MAX_CONTENT_BROWSERS];

private:
	TArray<TWeakPtr<SContentBrowser>> AllContentBrowsers;

	TMap<FName, TWeakPtr<FTabManager>> BrowserToLastKnownTabManagerMap;

	TWeakPtr<SContentBrowser> PrimaryContentBrowser;

	/** An incrementing int32 which is used when making unique settings strings */
	int32 SettingsStringID;
};