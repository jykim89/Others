// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UniquePtr.h"
#include "IPackageAutoSaver.h"
#include "UnrealEdEngine.generated.h"

UENUM()
enum EPackageNotifyState
{
	// The user has been prompted with the balloon taskbar message
	NS_BalloonPrompted,
	// The user responded to the balloon task bar message and got the modal prompt to checkout dialog and responded to it
	NS_DialogPrompted,
	// The package has been marked dirty and is pending a balloon prompt
	NS_PendingPrompt,
	NS_MAX,
};

UENUM()
enum EWriteDisallowedWarningState
{
	// The user needs to be warned about the package
	WDWS_PendingWarn,
	// The user has been warned about the package
	WDWS_Warned,
	// Warning for the package unnecessary
	WDWS_WarningUnnecessary,
	WDWS_MAX,
};

/** Used during asset renaming/duplication to specify class-specific package/group targets. */
USTRUCT()
struct FClassMoveInfo
{
	GENERATED_USTRUCT_BODY()

	/** The type of asset this MoveInfo applies to. */
	UPROPERTY(config)
	FString ClassName;

	/** The target package info which assets of this type are moved/duplicated. */
	UPROPERTY(config)
	FString PackageName;

	/** The target group info which assets of this type are moved/duplicated. */
	UPROPERTY(config)
	FString GroupName;

	/** If true, this info is applied when moving/duplicating assets. */
	UPROPERTY(config)
	uint32 bActive:1;


	FClassMoveInfo()
		: bActive(false)
	{
	}

};

/** Used during asset renaming/duplication to specify class-specific package/group targets. */
USTRUCT()
struct FTemplateMapInfo
{
	GENERATED_USTRUCT_BODY()

	/** The Texture2D associated with this map template */
	UPROPERTY()
	UTexture2D* ThumbnailTexture;

	/** The object path to the template map */
	UPROPERTY(config)
	FString Map;

	FTemplateMapInfo()
		: ThumbnailTexture(NULL)
	{
	}
};

UCLASS(config=Engine, transient)
class UNREALED_API UUnrealEdEngine : public UEditorEngine, public FNotifyHook
{
	GENERATED_UCLASS_BODY()

	/** Global instance of the editor options class. */
	UPROPERTY()
	class UUnrealEdOptions* EditorOptionsInst;

	/**
	 * Manager responsible for configuring auto reimport
	 */
	UPROPERTY()
	class UAutoReimportManager* AutoReimportManager;

	/** A buffer for implementing material expression copy/paste. */
	UPROPERTY()
	class UMaterial* MaterialCopyPasteBuffer;

	/** A buffer for implementing matinee track/group copy/paste. */
	UPROPERTY()
	TArray<class UObject*> MatineeCopyPasteBuffer;

	/** A buffer for implementing sound cue nodes copy/paste. */
	UPROPERTY()
	class USoundCue* SoundCueCopyPasteBuffer;

	/** Global list of instanced animation compression algorithms. */
	UPROPERTY()
	TArray<class UAnimCompress*> AnimationCompressionAlgorithms;

	/** Array of packages to be fully loaded at Editor startup. */
	UPROPERTY(config)
	TArray<FString> PackagesToBeFullyLoadedAtStartup;

	/** Current target for LOD parenting operations (actors will use this as the replacement) */
	UPROPERTY()
	class AActor* CurrentLODParentActor;

	/** If we have packages that are pending and we should notify the user that they need to be checked out */
	UPROPERTY()
	uint32 bNeedToPromptForCheckout:1;

	/** Whether the user needs to be prompted about a package being saved with an engine version newer than the current one or not */
	UPROPERTY()
	uint32 bNeedWarningForPkgEngineVer:1;

	/** Whether the user needs to be prompted about a package being saved when the user does not have permission to write the file */
	UPROPERTY()
	uint32 bNeedWarningForWritePermission:1;

	/** Array of sorted, localized editor sprite categories */
	UPROPERTY()
	TArray<FString> SortedSpriteCategories_DEPRECATED;

	/** List of info for all known template maps */
	UPROPERTY(config)
	TArray<FTemplateMapInfo> TemplateMapInfos;

	/** Cooker server incase we want to cook ont he side while editing... */
	UPROPERTY()
	class UCookOnTheFlyServer* CookServer;

	/** A mapping of packages to their checkout notify state.  This map only contains dirty packages.  Once packages become clean again, they are removed from the map.*/
	TMap<TWeakObjectPtr<UPackage>, uint8>	PackageToNotifyState;

	/** Map to track which packages have been checked for engine version when modified */
	TMap<FString, uint8>		PackagesCheckedForEngineVersion;

	/** Map to track which packages have been checked for write permission when modified */
	TMap<FString, uint8>		PackagesCheckedForWritePermission;

	/** Mapping of sprite category ids to their matching indices in the sorted sprite categories array */
	TMap<FName, int32>			SpriteIDToIndexMap;

	/** Map from component class to visualizer object to use */
	TMap< FName, TSharedPtr<class FComponentVisualizer> > ComponentVisualizerMap;

	// Begin UObject interface.
	~UUnrealEdEngine();
	virtual void FinishDestroy() OVERRIDE;
	virtual void Serialize( FArchive& Ar ) OVERRIDE;
	// End UObject interface.

	// Begin FNotify interface.
	virtual void NotifyPreChange( UProperty* PropertyAboutToChange ) OVERRIDE;
	virtual void NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged ) OVERRIDE;
	// End FNotify interface.

	// Begin UEditorEngine Interface
	virtual void SelectActor(AActor* Actor, bool InSelected, bool bNotify, bool bSelectEvenIfHidden=false) OVERRIDE;
	virtual bool CanSelectActor(AActor* Actor, bool InSelected, bool bSelectEvenIfHidden=false, bool bWarnIfLevelLocked=false) const OVERRIDE;
	virtual void SelectGroup(AGroupActor* InGroupActor, bool bForceSelection=false, bool bInSelected=true, bool bNotify=true) OVERRIDE;
	virtual void SelectBSPSurf(UModel* InModel, int32 iSurf, bool bSelected, bool bNoteSelectionChange) OVERRIDE;
	virtual void SelectNone(bool bNoteSelectionChange, bool bDeselectBSPSurfs, bool WarnAboutManyActors=true) OVERRIDE;
	virtual void NoteSelectionChange() OVERRIDE;
	virtual void NoteActorMovement() OVERRIDE;
	virtual void FinishAllSnaps() OVERRIDE;
	virtual void Cleanse( bool ClearSelection, bool Redraw, const FText& Reason ) OVERRIDE;
	virtual bool GetMapBuildCancelled() const OVERRIDE;
	virtual void SetMapBuildCancelled( bool InCancelled ) OVERRIDE;
	virtual FVector GetPivotLocation() OVERRIDE;
	virtual void SetPivot(FVector NewPivot, bool bSnapPivotToGrid, bool bIgnoreAxis, bool bAssignPivot=false) OVERRIDE;
	virtual void ResetPivot() OVERRIDE;
	virtual void RedrawLevelEditingViewports(bool bInvalidateHitProxies=true) OVERRIDE;
	virtual void TakeHighResScreenShots() OVERRIDE;
	virtual void GetPackageList( TArray<UPackage*>* InPackages, UClass* InClass ) OVERRIDE;
	virtual bool ShouldAbortActorDeletion() const OVERRIDE;
	virtual void CloseEditor() OVERRIDE;
	virtual void OnOpenMatinee() OVERRIDE;
	// End UEditorEngine Interface 
	
	// Begin FExec Interface
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar=*GLog ) OVERRIDE;
	// End FExec Interface


	// Begin UEngine Interface.
	virtual void Init(IEngineLoop* InEngineLoop) OVERRIDE;
	virtual void PreExit() OVERRIDE;
	virtual void Tick(float DeltaSeconds, bool bIdleMode) OVERRIDE;
	// End UEngine Interface.

	/** Builds a list of sprite categories for use in menus */
	void MakeSortedSpriteInfo(TArray<struct FSpriteCategoryInfo>& OutSortedSpriteInfo) const;

	/** called when a package has has its dirty state updated */
	void OnPackageDirtyStateUpdated( UPackage* Pkg);
	/** caled by FCoreDelegate::PostGarbageCollect */
	void OnPostGarbageCollect();
	/** called by color picker change event */
	void OnColorPickerChanged();
	/** called by the viewport client before a windows message is processed */
	void OnPreWindowsMessage(FViewport* Viewport, uint32 Message);
	/** called by the viewport client after a windows message is processed */
	void OnPostWindowsMessage(FViewport* Viewport, uint32 Message);

	/** Register a function to draw extra information when a particular component is selected */
	void RegisterComponentVisualizer(FName ComponentClassName, TSharedPtr<class FComponentVisualizer> Visualizer);

	/** Unregister component visualizer function */
	void UnregisterComponentVisualizer(FName ComponentClassName);

	/** Draw component visualizers for components for selected actors */
	void DrawComponentVisualizers(const FSceneView* View, FPrimitiveDrawInterface* PDI);

	/** Updates the property windows of selected actors */
	virtual void UpdateFloatingPropertyWindows();

	/**
	*	Updates the property windows of the actors in the supplied ActorList
	*
	*	@param	ActorList	The list of actors whose property windows should be updated
	*
	*/
	virtual void UpdateFloatingPropertyWindowsFromActorList( const TArray< UObject *>& ActorList );

	/**
	 * Fast track function to set render thread flags marking selection rather than reconnecting all components
	 * @param InActor - the actor to toggle view flags for
	 */
	virtual void SetActorSelectionFlags (AActor* InActor);

	/**
	 * Called to reset the editor's pivot (widget) location using the currently selected objects.  Usually
	 * called when the selection changes.
	 * @param bOnChange Set to true when we know for a fact the selected object has changed
	 */
	void UpdatePivotLocationForSelection( bool bOnChange = false );


	/**
	 * Replaces the specified actor with a new actor of the specified class.  The new actor
	 * will be selected if the current actor was selected.
	 *
	 * @param	CurrentActor			The actor to replace.
	 * @param	NewActorClass			The class for the new actor.
	 * @param	Archetype				The template to use for the new actor.
	 * @param	bNoteSelectionChange	If true, call NoteSelectionChange if the new actor was created successfully.
	 * @return							The new actor.
	 */
	virtual AActor* ReplaceActor( AActor* CurrentActor, UClass* NewActorClass, UObject* Archetype, bool bNoteSelectionChange );


	/**
	 * @return Returns the global instance of the editor options class.
	 */
	UUnrealEdOptions* GetUnrealEdOptions();

	/**
	 * Iterate over all levels of the world and create a list of world infos, then
	 * Iterate over selected actors and assemble a list of actors which can be deleted.
	 *
	 * @param	InWorld					The world we want to examine
	 * @param	bStopAtFirst			Whether or not we should stop at the first deletable actor we encounter
	 * @param	bLogUndeletable			Should we log all the undeletable actors
	 * @param	OutDeletableActors		Can be NULL, provides a list of all the actors, from the selection, that are deletable
	 * @return							true if any of the selection can be deleted
	 */
	bool CanDeleteSelectedActors( const UWorld* InWorld, const bool bStopAtFirst, const bool bLogUndeletable, TArray<AActor*>* OutDeletableActors = NULL ) const;

	// UnrealEdSrv stuff.
	bool Exec_Edit( UWorld* InWorld, const TCHAR* Str, FOutputDevice& Ar );
	bool Exec_Pivot( const TCHAR* Str, FOutputDevice& Ar );
	bool Exec_Actor( UWorld* InWorld, const TCHAR* Str, FOutputDevice& Ar );
	bool Exec_Mode( const TCHAR* Str, FOutputDevice& Ar );
	bool Exec_SkeletalMesh( const TCHAR* Str, FOutputDevice& Ar );
	bool Exec_Group( const TCHAR* Str, FOutputDevice& Ar );


	// Editor actor virtuals from EditorActor.cpp.
	/**
	 * Select all actors and BSP models, except those which are hidden.
	 */
	virtual void edactSelectAll( UWorld* InWorld );

	/**
	 * Invert the selection of all actors and BSP models.
	 */
	virtual void edactSelectInvert( UWorld* InWorld );

	/**
	 * Select all actors in a particular class.
	 *
	 * @param	InWorld		World context
	 * @param	InClass		Class of actor to select
	 */
	virtual void edactSelectOfClass( UWorld* InWorld, UClass* Class );

	/**
	 * Select all actors of a particular class and archetype.
	 *
	 * @param	InWorld		World context
	 * @param	InClass		Class of actor to select
	 * @param	InArchetype	Archetype of actor to select
	 */
	virtual void edactSelectOfClassAndArchetype( UWorld* InWorld, const UClass* InClass, const UObject* InArchetype );

	/**
	 * Select all actors in a particular class and its subclasses.
	 *
	 * @param	InWorld		World context
	 */
	virtual void edactSelectSubclassOf( UWorld* InWorld, UClass* Class );

	/**
	 * Select all actors in a level that are marked for deletion.
	 *
	 * @param	InWorld		World context
	 */
	virtual void edactSelectDeleted( UWorld* InWorld );

	/**
	 * Select all actors that have the same static mesh assigned to them as the selected ones.
	 *
	 * @param bAllClasses		If true, also select non-AStaticMeshActor actors whose meshes match.
	 */
	virtual void edactSelectMatchingStaticMesh(bool bAllClasses);

	/**
	 * Select all actors that have the same skeletal mesh assigned to them as the selected ones.
	 *
	 * @param bAllClasses		If true, also select non-ASkeletalMeshActor actors whose meshes match.
	 */
	virtual void edactSelectMatchingSkeletalMesh(bool bAllClasses);

	/**
	 * Select all material actors that have the same material assigned to them as the selected ones.
	 */
	virtual void edactSelectMatchingMaterial();

	/**
	 * Select all emitter actors that have the same particle system template assigned to them as the selected ones.
	 */
	virtual void edactSelectMatchingEmitter();

	/**
	 * Select the relevant lights for all selected actors
	 *
	 * @param	InWorld					World context
	 */
	virtual void edactSelectRelevantLights( UWorld* InWorld );

	/**
	 * Deletes all selected actors.  bIgnoreKismetReferenced is ignored when bVerifyDeletionCanHappen is true.
	 *
	 * @param	InWorld						World context
	 * @param	bVerifyDeletionCanHappen	[opt] If true (default), verify that deletion can be performed.
	 * @return								true unless the delete operation was aborted.
	 */
	virtual bool edactDeleteSelected( UWorld* InWorld, bool bVerifyDeletionCanHappen=true ) OVERRIDE;

	/**
	 * Creates a new group from the current selection removing any existing groups.
	 */
	virtual void edactRegroupFromSelected();

	/**
	 * Disbands any groups in the current selection, does not attempt to maintain any hierarchy
	 */
	virtual void edactUngroupFromSelected();

	/**
	 * Locks any groups in the current selection
	 */
	virtual void edactLockSelectedGroups();

	/**
	 * Unlocks any groups in the current selection
	 */
	virtual void edactUnlockSelectedGroups();

	/**
	 * Activates "Add to Group" mode which allows the user to select a group to append current selection
	 */
	virtual void edactAddToGroup();

	/**
	 * Removes any groups or actors in the current selection from their immediate parent.
	 * If all actors/subgroups are removed, the parent group will be destroyed.
	 */
	virtual void edactRemoveFromGroup();
	
	/**
	 * Opens the dialog window for merging selected actors into single static mesh
	 */
	virtual void edactMergeActors();

	/**
	 * Merges selected actors geometry grouping them by materials 
	 */
	virtual void edactMergeActorsByMaterials();

	/**
	 * Copy selected actors to the clipboard.  Does not copy PrefabInstance actors or parts of Prefabs.
	 *
	 * @param	InWorld					World context
	 * @param	DestinationData			If != NULL, additionally copy data to string
	 */
	virtual void edactCopySelected(UWorld* InWorld, FString* DestinationData = NULL) OVERRIDE;

	/**
	 * Paste selected actors from the clipboard.
	 *
	 * @param	InWorld					World context
	 * @param	bDuplicate			Is this a duplicate operation (as opposed to a real paste)?
	 * @param	bOffsetLocations	Should the actor locations be offset after they are created?
	 * @param	bWarnIfHidden		If true displays a warning if the destination level is hidden
	 * @param	SourceData			If != NULL, use instead of clipboard data
	 */
	virtual void edactPasteSelected(UWorld* InWorld, bool bDuplicate, bool bOffsetLocations, bool bWarnIfHidden, FString* SourceData = NULL) OVERRIDE;

	/**
	 * Duplicates selected actors.  Handles the case where you are trying to duplicate PrefabInstance actors.
	 *
	 * @param	InLevel				Level to place duplicate
	 * @param	bUseOffset			Should the actor locations be offset after they are created?
	 */
	virtual void edactDuplicateSelected(ULevel* InLevel, bool bUseOffset) OVERRIDE;

	/**
	 * Replace all selected brushes with the default brush.
	 *
	 * @param	InWorld					World context
	 */
	virtual void edactReplaceSelectedBrush( UWorld* InWorld );

	/**
	 * Replace all selected non-brush actors with the specified class.
	 */
	virtual void edactReplaceSelectedNonBrushWithClass(UClass* Class);

	/**
	 * Replace all actors of the specified source class with actors of the destination class.
	 *
	 * @param	InWorld		World context	 
	 * @param	SrcClass	The class of actors to replace.
	 * @param	DstClass	The class to replace with.
	 */
	virtual void edactReplaceClassWithClass(UWorld* InWorld, UClass* SrcClass, UClass* DstClass);

	/**
	* Align the origin with the current grid.
	*/
	virtual void edactAlignOrigin();

	/**
	 * Align all vertices with the current grid.
	 */
	virtual void edactAlignVertices();

	/**
	 * Hide selected actors and BSP models by marking their bHiddenEdTemporary flags true. Will not
	 * modify/dirty actors/BSP.
	 */
	virtual void edactHideSelected( UWorld* InWorld );

	/**
	 * Hide unselected actors and BSP models by marking their bHiddenEdTemporary flags true. Will not
	 * modify/dirty actors/BSP.
	 */
	virtual void edactHideUnselected( UWorld* InWorld );

	/**
	 * Attempt to unhide all actors and BSP models by setting their bHiddenEdTemporary flags to false if they
	 * are true. Note: Will not unhide actors/BSP hidden by higher priority visibility settings, such as bHiddenEdGroup,
	 * but also will not modify/dirty actors/BSP.
	 */
	virtual void edactUnHideAll( UWorld* InWorld );

	/**
	 * Mark all selected actors and BSP models to be hidden upon editor startup, by setting their bHiddenEd flag to
	 * true, if it is not already. This directly modifies/dirties the relevant actors/BSP.
	 */
	virtual void edactHideSelectedStartup( UWorld* InWorld );

	/**
	 * Mark all actors and BSP models to be shown upon editor startup, by setting their bHiddenEd flag to false, if it is
	 * not already. This directly modifies/dirties the relevant actors/BSP.
	 */
	virtual void edactUnHideAllStartup( UWorld* InWorld );

	/**
	 * Mark all selected actors and BSP models to be shown upon editor startup, by setting their bHiddenEd flag to false, if it
	 * not already. This directly modifies/dirties the relevant actors/BSP.
	 */
	virtual void edactUnHideSelectedStartup( UWorld* InWorld );

	/**
	 * Show selected actors and BSP models by marking their bHiddenEdTemporary flags false. Will not
	 * modify/dirty actors/BSP.
	 */
	virtual void edactUnhideSelected( UWorld* InWorld );

	/** Will create a map of currently visible BSP surfaces. */
	virtual void CreateBSPVisibilityMap( UWorld* InWorld, TMap<AActor*, TArray<int32>>& OutBSPMap, bool& bOutAllVisible );

	/** Go through a map of BSP and make only the requested objects visible. */
	virtual void MakeBSPMapVisible(const TMap<AActor*, TArray<int32>>& InBSPMap, UWorld* InWorld );

	/** Returns the configuration of attachment that would result from calling AttachSelectedActors at this point in time */
	AActor* GetDesiredAttachmentState(TArray<AActor*>& OutNewChildren);

	/** Uses the current selection state to attach actors together. Last selected Actor becomes the base. */
	void AttachSelectedActors();


	// Hook replacements.
	void ShowActorProperties();

	/**
	 * Checks to see if any worlds are dirty (that is, they need to be saved.)
	 *
	 * @param	InWorld	World to search for dirty worlds
	 * 
	 * @return true if any worlds are dirty
	 */
	bool AnyWorldsAreDirty( UWorld* InWorld ) const;

	/**
	 * Checks to see if any content packages are dirty (that is, they need to be saved.)
	 *
	 * @return true if any content packages are dirty
	 */
	bool AnyContentPackagesAreDirty() const;

	// Misc
	/**
	 * Attempts to prompt the user with a balloon notification to checkout modified packages from source control.
	 * Will defer prompting the user if they are interacting with something
	 */
	void AttemptModifiedPackageNotification();

	/**
	 * Alerts the user to any packages that have been modified which have been previously saved with an engine version newer than
	 * the current version. These packages cannot be saved, so the user should be alerted ASAP.
	 */
	void AttemptWarnAboutPackageEngineVersions();

	/**
	 * Alerts the user to any packages that they do not have permission to write to. This may happen in the program files directory if the user is not an admin.
	 * These packages cannot be saved, so the user should be alerted ASAP.
	 */
	void AttemptWarnAboutWritePermission();

	/**
	 * Prompts the user with a modal checkout dialog to checkout packages from source control.
	 * This should only be called by the auto prompt to checkout package notification system.
	 * For a general checkout packages routine use FEditorFileUtils::PromptToCheckoutPackages
	 *
	 * @param bPromptAll	If true we prompt for all packages in the PackageToNotifyState map.  If false only prompt about ones we have never prompted about before.
	 */
	void PromptToCheckoutModifiedPackages( bool bPromptAll = false );

	/**
	 * Checks to see if there are any packages in the PackageToNotifyState map that are not checked out by the user
	 *
	 * @return True if packages need to be checked out.
	 */
	bool DoDirtyPackagesNeedCheckout() const;

	/**
	 * Checks whether the specified map is a template map.
	 *
	 * @return true if the map is a template map, false otherwise.
	 */
	bool IsTemplateMap( const FString& MapName ) const;

	/**
	 * Returns true if the user is currently interacting with a viewport.
	 */
	bool IsUserInteracting();

	void SetCurrentClass( UClass* InClass );

	/**
	 * @return true if selection of translucent objects in perspective viewports is allowed
	 */
	virtual bool AllowSelectTranslucent() const;

	/**
	 * @return true if only editor-visible levels should be loaded in Play-In-Editor sessions
	 */
	virtual bool OnlyLoadEditorVisibleLevelsInPIE() const;
	
	/**
	 * If all selected actors belong to the same level, that level is made the current level.
	 */
	void MakeSelectedActorsLevelCurrent();

	/** Returns the thumbnail manager and creates it if missing */
	class UThumbnailManager* GetThumbnailManager();

	/**
	 * Returns whether saving the specified package is allowed
	 */
	virtual bool CanSavePackage( UPackage* PackageToSave );

	/** Converts kismet based matinees in the current level to matinees controlled via matinee actors */
	void ConvertMatinees();

	/**
	 * Updates the volume actor visibility for all viewports based on the passed in volume class
	 *
	 * @param InVolumeActorClass	The type of volume actors to update.  If NULL is passed in all volume actor types are updated.
	 * @param InViewport			The viewport where actor visibility should apply.  Pass NULL for all editor viewports.
	 */
	void UpdateVolumeActorVisibility( const UClass* InVolumeActorClass = NULL , FLevelEditorViewportClient* InViewport = NULL);

	/**
	 * Get the index of the provided sprite category
	 *
	 * @param	InSpriteCategory	Sprite category to get the index of
	 *
	 * @return	Index of the provided sprite category, if possible; INDEX_NONE otherwise
	 */
	virtual int32 GetSpriteCategoryIndex( const FName& InSpriteCategory );
	
	/**
	 * Shows the LightingStaticMeshInfoWindow, creating it first if it hasn't been initialized.
	 */
	void ShowLightingStaticMeshInfoWindow();
	
	/**
	 * Shows the SceneStatsWindow, creating it first if it hasn't been initialized.
	 */
	void OpenSceneStatsWindow();

	/**
	 * Shows the TextureStatsWindow, creating it first if it hasn't been initialized.
	 */
	void OpenTextureStatsWindow();

	/**
	* Puts all of the AVolume classes into the passed in array and sorts them by class name.
	*
	* @param	VolumeClasses		Array to populate with AVolume classes.
	*/
	void GetSortedVolumeClasses( TArray< UClass* >* VolumeClasses );

	/**
	 * Checks the destination level visibility and warns the user if he is trying to paste to a hidden level, offering the option to cancel the operation or unhide the level that is hidden
	 * 
	 * @param InWorld			World context
	 */
	bool WarnIfDestinationLevelIsHidden( UWorld* InWorld );

	/**
	 * Generate the package thumbails if they are needed. 
	 */
	UPackage* GeneratePackageThumbnailsIfRequired( const TCHAR* Str, FOutputDevice& Ar, TArray<FString>& ThumbNamesToUnload );

	/** @return The package auto-saver instance used by the editor */
	IPackageAutoSaver& GetPackageAutoSaver() const
	{
		return *PackageAutoSaver;
	}

	/**
	 * Exec command handlers
	 */
	bool HandleDumpModelGUIDCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool HandleModalTestCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool HandleDumpBPClassesCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool HandleFindOutdateInstancesCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool HandleDumpSelectionCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool HandleBuildLightingCommand( const TCHAR* Str, FOutputDevice& Ar, UWorld* InWorld );
	bool HandleBuildPathsCommand( const TCHAR* Str, FOutputDevice& Ar, UWorld* InWorld );
	bool HandleUpdateLandscapeEditorDataCommand( const TCHAR* Str, FOutputDevice& Ar, UWorld* InWorld );
	bool HandleUpdateLandscapeMICCommand( const TCHAR* Str, FOutputDevice& Ar, UWorld* InWorld );
	bool HandleConvertMatineesCommand( const TCHAR* Str, FOutputDevice& Ar, UWorld* InWorld );
	bool HandleDisasmScriptCommand( const TCHAR* Str, FOutputDevice& Ar );	

	/** OnEditorModeChanged delegate which looks for Matinee editor closing */
	void OnMatineeEditorClosed( class FEdMode* Mode, bool IsEntering );
	
protected:
	EWriteDisallowedWarningState GetWarningStateForWritePermission(const FString& PackageName) const;

	/** The package auto-saver instance used by the editor */
	TUniquePtr<IPackageAutoSaver> PackageAutoSaver;
};
