// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * The pending deleted object.
 */
struct FPendingDelete : TSharedFromThis<FPendingDelete>
{
public:
	FPendingDelete( UObject* InObject );

	/** Checks for references on disk and in memory for this object filling out all information. */
	void CheckForReferences();

	/** Gets the object being deleted. */
	UObject* GetObject() { return Object; }

	/** Checks if the object is contained in the pending delete object hierarchy */
	bool IsObjectContained(const UObject* InObject) const;

	/** Checks if the package is contained in the asset package being deleted */
	bool IsAssetContained(const FName& PackageName) const;

	/** Is the pending deleted object referenced in memory by something other than the undo stack; INCLUDES PENDING DELETES */
	bool IsReferencedInMemory() const { return bIsReferencedInMemory; }

	/** Is the pending deleted object referenced in memory by the undo stack; INCLUDES PENDING DELETES */
	bool IsReferencedInMemoryByUndo() const { return bIsReferencedInMemoryByUndo; }

	/** Returns if the pending delete is internal, and need not be shown to the user. */
	bool IsInternal() const { return bIsInternal; }

	/** Sets if the pending delete is internal, and need not be shown to the user. */
	void IsInternal(bool Value) { bIsInternal = Value; }

	/** Support comparing for unique insertions */
	bool operator == ( const FPendingDelete& Other ) const;

	/** The on disk references to this object */
	TArray<FName> DiskReferences;

	/** In memory references to this object */
	FReferencerInformationList MemoryReferences;

	/** The remaining disk references; EXCLUDES PENDING DELETES */
	int32 RemainingDiskReferences;
	/** The remaining memory references; EXCLUDES PENDING DELETES */
	int32 RemainingMemoryReferences;

private:
	/** The object to delete */
	UObject* Object;

	/** Internal objects being deleted that we need to make sure aren't counted as memory references. */
	TArray<UObject*> InternalObjects;

	/** A flag indicating that references have been checked, so don't check again. */
	bool bReferencesChecked;

	/** flag indicating if this object is referenced in memory by the engine. */
	bool bIsReferencedInMemory;

	/** flag indicating if this object is referenced in memory by the undo stack. */
	bool bIsReferencedInMemoryByUndo;

	/**
	 * flag to control the visibility of this pending deleted object.  Some internal objects,
	 * like blueprint generated classes and skeleton classes need to be added to the list of pending
	 * deletes but users don't need to see them.
	 */
	bool bIsInternal;
};

/**
 * The model behind a delete operation, which is an asynchronous process because of all the checks
 * that must be performed against the GC for UObjects, and looking up references for assets through
 * the asset registry.
 */
class FAssetDeleteModel
{
public:
	/** States used to manage the async deletion process. */
	enum EState
	{
		// Waiting to start scanning
		Waiting = 0,
		// Begin scanning for references
		StartScanning,
		// Scan for references to the pending deleted assets
		Scanning,
		// check compatibility for replacing references
		UpdateActions,
		// Finished
		Finished,
	};

public:

	/** Constructor */
	FAssetDeleteModel( const TArray<UObject*>& InObjectsToDelete );

	/** Destructor */
	~FAssetDeleteModel();

	/** Add an object to the list of pending deleted assets, this will invalidate the scanning state. */
	void AddObjectToDelete(UObject* InObject);

	/** Returns the pending deleted assets. */
	const TArray< TSharedPtr< FPendingDelete > >* GetPendingDeletedAssets() const { return &PendingDeletes; };

	/** Returns the current state of the deletion process */
	EState GetState() const { return State; }

	/** Gets the packages of the assets on disk that reference the pending deleted objects;  won't be accurate until the scanning process completes. */
	const TSet< FName >& GetAssetReferences() const { return OnDiskReferences; };

	/** Ticks the delete model which does a little work before returning so that we don't completely block when deleting a lot of things. */
	void Tick( const float InDeltaTime );

	/** Returns true if the object is one of the pending deleted assets. */
	bool IsObjectInPendingDeletes( const UObject* InObject ) const;

	/** Returns true if the package is one of the pending deleted assets. */
	bool IsAssetInPendingDeletes( const FName& PackageName ) const;

	/** Returns true if it is valid to delete the current objects with no problems. */
	bool CanDelete() const;

	/** Performs the delete if it's possible. */
	bool DoDelete();

	/** Returns true if it is valid to force the delete of the current assets. */
	bool CanForceDelete() const;

	/** Performs a force delete on the pending deleted assets if possible. */
	bool DoForceDelete();

	/** Returns true if it's valid to replace the references of the pending deleted objects. */
	bool CanReplaceReferences() const;

	/** Returns true if it is valid to force the delete of the current assets with the provided asset. */
	bool CanReplaceReferencesWith( const FAssetData& InAssetData ) const;

	/** Performs the replace references action if possible with the provided asset. */
	bool DoReplaceReferences( const FAssetData& ReplaceReferencesWith );

	/** Gets the 0..1 progress of the scanning. */
	float GetProgress() const;

	/** Gets the current text to display for the current progress of the scanning. */
	FText GetProgressText() const;

	/** Is any of the pending deleted assets being referenced in memory. */
	bool IsAnythingReferencedInMemory() const { return bIsAnythingReferencedInMemory; }

	/** Is any of the pending deleted assets being referenced in the undo stack. */
	bool IsAnythingReferencedInMemoryByUndo() const { return bIsAnythingReferencedInMemoryByUndo; }
	
	/** Goes to the next actor in the loaded level if it is available */
	bool GoToNextReferenceInLevel() const;

	/** Gets the number of objects successfully deleted. */
	int32 GetDeletedObjectCount() const;
	
	/** Fires whenever the state changes. */
	DECLARE_EVENT_OneParam( FAssetDeleteModel, FOnStateChanged, EState /*NewState*/ );
	FOnStateChanged& OnStateChanged()
	{
		return StateChanged;
	}

private:
	void PrepareToDelete(UObject* InObject);

	/** Sets the current state of the model. */
	void SetState( EState NewState );

	/** Computes the value that should be used for CanReplaceReferences */
	bool ComputeCanReplaceReferences();

private:

	/** Holds an event delegate that is executed when the state changes */
	FOnStateChanged StateChanged;

	/** The assets being deleted */
	TArray< TSharedPtr< FPendingDelete > > PendingDeletes;

	/** On disk references to the currently to be deleted objects */
	TSet< FName > OnDiskReferences;

	/** The internal progress/state of the delete model which can take several frames to recalculate deletion validity */
	EState State;

	/** Pending objects can replaced so the 'Replace References' option should be available */
	bool bPendingObjectsCanBeReplaced;

	/** Is any of the pending deleted assets being referenced in memory. */
	bool bIsAnythingReferencedInMemory;

	/** Is any of the pending deleted assets being referenced in the undo stack. */
	bool bIsAnythingReferencedInMemoryByUndo;

	/** A tick-to-tick state tracking variable so we know what pending deleted object we checked last. */
	int32 PendingDeleteIndex;

	/** The number of objects successfully deleted. */
	int32 ObjectsDeleted;
};
