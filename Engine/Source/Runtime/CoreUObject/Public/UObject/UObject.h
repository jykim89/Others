// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnObjUObject.h: Unreal object base class
=============================================================================*/

#ifndef __UNOBJUOBJECT_H__
#define __UNOBJUOBJECT_H__

DECLARE_LOG_CATEGORY_EXTERN(LogObj, Log, All);

namespace ECastCheckedType
{
	enum Type
	{
		NullAllowed,
		NullChecked
	};
};

/** Passed to GetResourceSize() to indicate which resource size should be returned.*/
namespace EResourceSizeMode
{
	enum Type
	{
		/** Only exclusive resource size */
		Exclusive,
		/** Resource size of the object and all of its references */
		Inclusive,
	};
};

//
// The base class of all objects.
//
class COREUOBJECT_API UObject : public UObjectBaseUtility
{
	// Declarations.
	DECLARE_CLASS(UObject,UObject,CLASS_Abstract|CLASS_NoExport,CASTCLASS_None,CoreUObject,NO_API)

	typedef UObject WithinClass;
	static const TCHAR* StaticConfigName() {return TEXT("Engine");}

	// Constructors and destructors.
	UObject(const class FPostConstructInitializeProperties& PCIP);
	UObject( EStaticConstructor, EObjectFlags InFlags );

	static void StaticRegisterNativesUObject() 
	{
	}

	//==========================================
	// UObject interface.
	//==========================================	
protected:
    /** 
     * This function actually does the work for the GetDetailInfo and is virtual.  
     * It should only be called from GetDetailedInfo as GetDetailedInfo is safe to call on NULL object pointers
     **/
	virtual FString GetDetailedInfoInternal() const { return TEXT("No_Detailed_Info_Specified"); }
public:
	/**
	 * Called after the C++ constructor and after the properties have been initialized, but before the config has been loaded, etc.
	 * mainly this is to emulate some behavior of when the constructor was called after the properties were intialized.
	 */
	virtual void PostInitProperties();

	/**
	 * Called from within SavePackage on the passed in base/ root. The return value of this function will be passed to
	 * PostSaveRoot. This is used to allow objects used as base to perform required actions before saving and cleanup
	 * afterwards.
	 * @param Filename: Name of the file being saved to (includes path)
	 * @param AdditionalPackagesToCook [out] Array of other packages the Root wants to make sure are cooked when this is cooked
	 *
	 * @return	Whether PostSaveRoot needs to perform internal cleanup
	 */
	virtual bool PreSaveRoot(const TCHAR* Filename, TArray<FString>& AdditionalPackagesToCook)
	{
		return false;
	}
	/**
	 * Called from within SavePackage on the passed in base/ root. This function is being called after the package
	 * has been saved and can perform cleanup.
	 *
	 * @param	bCleanupIsRequired	Whether PreSaveRoot dirtied state that needs to be cleaned up
	 */
	virtual void PostSaveRoot( bool bCleanupIsRequired ) {}

	/**
	 * Presave function. Gets called once before an object gets serialized for saving. This function is necessary
	 * for save time computation as Serialize gets called three times per object from within SavePackage.
	 *
	 * @warning: Objects created from within PreSave will NOT have PreSave called on them!!!
	 */
	virtual void PreSave() {}

	/**
	 * Note that the object will be modified.  If we are currently recording into the 
	 * transaction buffer (undo/redo), save a copy of this object into the buffer and 
	 * marks the package as needing to be saved.
	 *
	 * @param	bAlwaysMarkDirty	if true, marks the package dirty even if we aren't
	 *								currently recording an active undo/redo transaction
	 * @return true if the object was saved to the transaction buffer
	 */
	virtual bool Modify( bool bAlwaysMarkDirty=true );

#if WITH_EDITOR
	/**
	 * The cooker (or DDC commandlet) have dealt with this object and will never deal with it again.
	 * The object should try to free memory
	 */
	virtual void CookerWillNeverCookAgain() {}
	/** 
	 * Called when the object was loaded from another class via active class redirects.
	 */
	virtual void LoadedFromAnotherClass(const FName& OldClassName) {}
#endif

	/** 
	 * Do any object-specific cleanup required immediately after loading an object, 
	 * and immediately after any undo/redo.
	 */
	virtual void PostLoad();

	/**
	 * Instances components for objects being loaded from disk, if necessary.  Ensures that component references
	 * between nested components are fixed up correctly.
	 *
	 * @param	OuterInstanceGraph	when calling this method on subobjects, specifies the instancing graph which contains all instanced
	 *								subobjects and components for a subobject root.
	 */
	virtual void PostLoadSubobjects( FObjectInstancingGraph* OuterInstanceGraph );
	
	/**
	 * Called before destroying the object.  This is called immediately upon deciding to destroy the object, to allow the object to begin an
	 * asynchronous cleanup process.
	 */
	virtual void BeginDestroy();

	/**
	 * Called to check if the object is ready for FinishDestroy.  This is called after BeginDestroy to check the completion of the
	 * potentially asynchronous object cleanup.
	 * @return True if the object's asynchronous cleanup has completed and it is ready for FinishDestroy to be called.
	 */
	virtual bool IsReadyForFinishDestroy() { return true; }

#if WITH_EDITOR
	/**
	 * Called in response to the linker changing, this can only happen in the editor
	 */
	virtual void PostLinkerChange() {}
#endif

	/**
	 * Called to finish destroying the object.  After UObject::FinishDestroy is called, the object's memory should no longer be accessed.
	 *
	 * note: because properties are destroyed here, Super::FinishDestroy() should always be called at the end of your child class's
	 * FinishDestroy() method, rather than at the beginning.
	 */
	virtual void FinishDestroy();

	/** UObject serializer. */
	virtual void Serialize( FArchive& Ar );

	virtual void ShutdownAfterError() {}

	/** 
	 * This is called when property is modified by InterpPropertyTracks
	 *
	 * @param PropertyThatChanged	Property that changed
	 */
	virtual void PostInterpChange(UProperty* PropertyThatChanged) {}

#if WITH_EDITOR
	/** 
	 * This is called when property is about to be modified by InterpPropertyTracks
	 *
	 * @param PropertyThatWillChange	Property that will be changed
	 */
	virtual void PreEditChange(UProperty* PropertyAboutToChange);

	/**
	 * This alternate version of PreEditChange is called when properties inside structs are modified.  The property that was actually modified
	 * is located at the tail of the list.  The head of the list of the UStructProperty member variable that contains the property that was modified.
	 *
	 * @param PropertyAboutToChange the property that is about to be modified
	 */
	virtual void PreEditChange( class FEditPropertyChain& PropertyAboutToChange );

	/**
	 * Called by the editor to query whether a property of this object is allowed to be modified.
	 * The property editor uses this to disable controls for properties that should not be changed.
	 * When overriding this function you should always call the parent implementation first.
	 *
	 * @param	InProperty	The property to query
	 *
	 * @return	true if the property can be modified in the editor, otherwise false
	 */
	virtual bool CanEditChange( const UProperty* InProperty ) const;

	/** 
	 * Intentionally non-virtual as it calls the FPropertyChangedEvent version
	 */
	void PostEditChange();

	/**
	 * Called when a property on this object has been modified externally
	 *
	 * @param PropertyThatChanged the property that was modified
	 */
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent);

	/**
	 * This alternate version of PostEditChange is called when properties inside structs are modified.  The property that was actually modified
	 * is located at the tail of the list.  The head of the list of the UStructProperty member variable that contains the property that was modified.
	 */
	virtual void PostEditChangeChainProperty( struct FPropertyChangedChainEvent& PropertyChangedEvent );

	/** Called before applying a transaction to the object.  Default implementation simply calls PreEditChange. */
	virtual void PreEditUndo();

	/** Called after applying a transaction to the object.  Default implementation simply calls PostEditChange. */
	virtual void PostEditUndo();
#endif // WITH_EDITOR

	// @todo document
	virtual void PostRename(UObject* OldOuter, const FName OldName) {}
	
	/**
	 * Called after duplication & serialization and before PostLoad. Used to e.g. make sure UStaticMesh's UModel gets copied as well.
	 * Note: NOT called on components on actor duplication (alt-drag or copy-paste).  Use PostEditImport as well to cover that case.
	 */
	virtual void PostDuplicate(bool bDuplicateForPIE) {}

	/**
	 * Called during saving to determine the load flags to save with the object.
	 * Upon reload, this object will be discarded on clients
	 *
	 * @return	true if this object should not be loaded on clients
	 */
	virtual bool NeedsLoadForClient() const 
	{ 
		return true; 
	}

	/**
	 * Called during saving to determine the load flags to save with the object.
	 * Upon reload, this object will be discarded on servers
	 *
	 * @return	true if this object should not be loaded on servers
	 */
	virtual bool NeedsLoadForServer() const 
	{ 
		return true; 
	}

	/** 
	 *	Determines if you can create an object from the supplied template in the current context (editor, client only, dedicated server, game/listen) 
	 *	This calls NeedsLoadForClient & NeedsLoadForServer
	 */
	static bool CanCreateInCurrentContext(UObject* Template);

	/**
	* Exports the property values for the specified object as text to the output device. Required for Copy&Paste
	* Most objects don't need this as unreal script can handle most cases.
	*
	* @param	Out				the output device to send the exported text to
	* @param	Indent			number of spaces to prepend to each line of output
	*
	* see also: ImportCustomProperties()
	*/
	virtual void ExportCustomProperties(FOutputDevice& Out, uint32 Indent)	{}

	/**
	* Exports the property values for the specified object as text to the output device. Required for Copy&Paste
	* Most objects don't need this as unreal script can handle most cases.
	*
	* @param	SourceText		the input data (zero terminated), must not be 0
	* @param	Warn			for error reporting, must not be 0
	*
	* see also: ExportCustomProperties()
	*/
	virtual void ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn) {}

	/**
	 * Called after importing property values for this object (paste, duplicate or .t3d import)
	 * Allow the object to perform any cleanup for properties which shouldn't be duplicated or
	 * are unsupported by the script serialization
	 */
	virtual void PostEditImport() {}

	/**
	 * Called from ReloadConfig after the object has reloaded its configuration data.
	 */
	virtual void PostReloadConfig( class UProperty* PropertyThatWasLoaded ) {}

	/** Rename this object to a unique name.*/
	virtual bool Rename( const TCHAR* NewName=NULL, UObject* NewOuter=NULL, ERenameFlags Flags=REN_None );


	/** @return a one line description of an object for viewing in the thumbnail view of the generic browser */
	virtual FString GetDesc() { return TEXT( "" ); }

#if WITH_ENGINE
	virtual class UWorld* GetWorld() const;
	class UWorld* GetWorldChecked(bool& bSupported) const;
	bool ImplementsGetWorld() const;
#endif

	/**
	 * Callback for retrieving a textual representation of natively serialized properties.  Child classes should implement this method if they wish
	 * to have natively serialized property values included in things like diffcommandlet output.
	 *
	 * @param	out_PropertyValues	receives the property names and values which should be reported for this object.  The map's key should be the name of
	 *								the property and the map's value should be the textual representation of the property's value.  The property value should
	 *								be formatted the same way that UProperty::ExportText formats property values (i.e. for arrays, wrap in quotes and use a comma
	 *								as the delimiter between elements, etc.)
	 * @param	ExportFlags			bitmask of EPropertyPortFlags used for modifying the format of the property values
	 *
	 * @return	true if property values were added to the map.
	 */
	virtual bool GetNativePropertyValues( TMap<FString,FString>& out_PropertyValues, uint32 ExportFlags=0 ) const
	{
		return false;
	}

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor. The
	 * default behavior is to return 0 which indicates that the resource shouldn't display its
	 * size which is used to not confuse people by displaying small sizes for e.g. objects like
	 * materials
	 *
	 * @param	Type	Indicates which resource size should be returned
	 * @return	Size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode)
	{
		return 0;
	}

	/** 
	 * Returns the name of the exporter factory used to export this object
	 * Used when multiple factories have the same extension
	 */
	virtual FName GetExporterName( void )
	{
		return( FName( TEXT( "" ) ) );
	}

	/**
	 * Returns whether this wave file is a localized resource.
	 *
	 * @return true if it is a localized resource, false otherwise.
	 */
	virtual bool IsLocalizedResource()
	{
		return false;
	}

	/**
	 * Callback used to allow object register its direct object references that are not already covered by
	 * the token stream.
	 *
	 * @param InThis Object to collect references from.
	 * @param Collector	FReferenceCollector objects to be used to collect references.
	 */
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	/**
	 * Helper function to call AddReferencedObjects for this object's class.
	 *
	 * @param Collector	FReferenceCollector objects to be used to collect references.
	 */
	void CallAddReferencedObjects(FReferenceCollector& Collector);

	/**
	 * Save information for StaticAllocateObject in the case of overwriting an existing object.
	 * StaticAllocateObject will call delete on the result after calling Restore()
	 *
	 * @return An FRestoreForUObjectOverwrite that can restore the object or NULL if this is not necessary.
	 */
	virtual FRestoreForUObjectOverwrite* GetRestoreForUObjectOverwrite()
	{
		return NULL;
	}

	/**
	 * Returns whether native properties are identical to the one of the passed in component.
	 *
	 * @param	Other	Other component to compare against
	 *
	 * @return true if native properties are identical, false otherwise
	 */
	virtual bool AreNativePropertiesIdenticalTo( UObject* Other ) const
	{
		return true;
	}

	/**
	 * Gathers a list of asset registry searchable tags which are name/value pairs with some type information
	 * This only needs to be implemented for asset objects
	 *
	 * @param	OutTags		A list of key-value pairs associated with this object and their types
	 */
	struct FAssetRegistryTag
	{
		enum ETagType
		{
			TT_Hidden,
			TT_Alphabetical,
			TT_Numerical,
			TT_Dimensional
		};

		FName Name;
		FString Value;
		ETagType Type;

		FAssetRegistryTag(FName InName, const FString& InValue, ETagType InType)
			: Name(InName), Value(InValue), Type(InType) {}

		/** Gathers a list of asset registry searchable tags from given objects properties */
		COREUOBJECT_API static void GetAssetRegistryTagsFromSearchableProperties(const UObject* Object, TArray<FAssetRegistryTag>& OutTags);
	};
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const;

	/** Returns true if this object is considered an asset. */
	virtual bool IsAsset() const;

	/** Returns true if this object is safe to add to the root set. */
	virtual bool IsSafeForRootSet() const;

	/** 
	 * Tags objects that are part of the same asset with the specified object flag, used for GC checking
	 *
	 * @param	ObjectFlags	Object Flags to enable on the related objects
	 */
	virtual void TagSubobjects(EObjectFlags NewFlags);

	/** Returns properties that are replicated for the lifetime of the actor channel */
	virtual void GetLifetimeReplicatedProps( TArray< class FLifetimeProperty > & OutLifetimeProps ) const;

	/** Called right before receiving a bunch */
	virtual void PreNetReceive();

	/** Called right after receiving a bunch */
	virtual void PostNetReceive();

	/**
 	 *******************************************************
	 * Non virtual functions, not intended to be overridden
	 *******************************************************
	 */

	/**
	 * Test the selection state of a UObject
	 *
	 * @return		true if the object is selected, false otherwise.
	 * @todo UE4 this doesn't belong here, but it doesn't belong anywhere else any better
	 */
	bool IsSelected() const;

#if WITH_EDITOR
	/**
	 * Serializes all objects which have this object as their archetype into GMemoryArchive, then recursively calls this function
	 * on each of those objects until the full list has been processed.
	 * Called when a property value is about to be modified in an archetype object. 
	 *
	 * @param	AffectedObjects		the array of objects which have this object in their ObjectArchetype chain and will be affected by the change.
	 *								Objects which have this object as their direct ObjectArchetype are removed from the list once they're processed.
	 */
	void PropagatePreEditChange( TArray<UObject*>& AffectedObjects, FEditPropertyChain& PropertyAboutToChange );

	/**
	 * De-serializes all objects which have this object as their archetype from the GMemoryArchive, then recursively calls this function
	 * on each of those objects until the full list has been processed.
	 *
	 * @param	AffectedObjects		the array of objects which have this object in their ObjectArchetype chain and will be affected by the change.
	 *								Objects which have this object as their direct ObjectArchetype are removed from the list once they're processed.
	 */
	void PropagatePostEditChange( TArray<UObject*>& AffectedObjects, FPropertyChangedChainEvent& PropertyChangedEvent );
#endif // WITH_EDITOR

	/**
	 * Serializes the script property data located at Data.  When saving, only saves those properties which differ from the corresponding
	 * value in the specified 'DiffObject' (usually the object's archetype).
	 *
	 * @param	Ar				the archive to use for serialization
	 */
	void SerializeScriptProperties( FArchive& Ar ) const;

	/**
	 * Wrapper function for InitProperties() which handles safely tearing down this object before re-initializing it
	 * from the specified source object.
	 *
	 * @param	SourceObject	the object to use for initializing property values in this object.  If not specified, uses this object's archetype.
	 * @param	InstanceGraph	contains the mappings of instanced objects and components to their templates
	 */
	void ReinitializeProperties( UObject* SourceObject=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL );

	/** Handle culture change. */
	virtual void CultureChange();

	/**
	 * This will return detail info about this specific object. (e.g. AudioComponent will return the name of the cue,
	 * ParticleSystemComponent will return the name of the ParticleSystem)  The idea here is that in many places
	 * you have a component of interest but what you really want is some characteristic that you can use to track
	 * down where it came from.  
	 *
	 * @note	safe to call on NULL object pointers!
	 */
	FString GetDetailedInfo() const;

	/**
	 * Called before destroying the object.  This is called immediately upon deciding to destroy the object, to allow the object to begin an
	 * asynchronous cleanup process.
	 */
	bool ConditionalBeginDestroy();
	bool ConditionalFinishDestroy();
	
	/** PostLoad if needed. */
	void ConditionalPostLoad();

	/**
	 * Instances subobjects and components for objects being loaded from disk, if necessary.  Ensures that references
	 * between nested components are fixed up correctly.
	 *
	 * @param	OuterInstanceGraph	when calling this method on subobjects, specifies the instancing graph which contains all instanced
	 *								subobjects and components for a subobject root.
	 */
	void ConditionalPostLoadSubobjects( struct FObjectInstancingGraph* OuterInstanceGraph=NULL );

	/** Make sure this object has been shut down. */
	void ConditionalShutdownAfterError();


	/**
	 * Starts caching of platform specific data for the target platform
	 * Called when cooking before serialization so that object can prepare platform specific data
	 * Not called during normal loading of objects
	 * 
	 * @param	TargetPlatform	target platform to cache platform specific data for
	 */
	virtual void BeginCacheForCookedPlatformData( const ITargetPlatform *TargetPlatform ) {  }

	/**
	 * Clears cached cooked platform data for specific platform
	 * 
	 * @param	TargetPlatform	target platform to cache platform specific data for
	 */
	virtual void ClearCachedCookedPlatformData( const ITargetPlatform *TargetPlatform ) {  }

	/**
	 * Clear all cached cooked platform data
	 * 
	 * @param	TargetPlatform	target platform to cache platform specific data for
	 */
	virtual void ClearAllCachedCookedPlatformData() { }


	/**
	 * Determine if this object has SomeObject in its archetype chain.
	 */
	inline bool IsBasedOnArchetype( const UObject* const SomeObject ) const;

	UFunction* FindFunction( FName InName ) const;
	UFunction* FindFunctionChecked( FName InName ) const;

	/**
	 * Uses the TArchiveObjectReferenceCollector to build a list of all components referenced by this object which have this object as the outer
	 *
	 * @param	OutDefaultSubobjects	the array that should be populated with the default subobjects "owned" by this object
	 * @param	bIncludeNestedSubobjects	controls whether subobjects which are contained by this object, but do not have this object
	 *										as its direct Outer should be included
	 */
	void CollectDefaultSubobjects( TArray<UObject*>& OutDefaultSubobjects, bool bIncludeNestedSubobjects=false );

	/**
	 * Checks default sub-object assumptions.
	 *
	 * @param bForceCheck Force checks even if not enabled globally.
	 * @return true if the assumptions are met, false otherwise.
	 */
	virtual bool CheckDefaultSubobjects(bool bForceCheck = false);

	/**
	 * Save configuration.
	 * @warning: Must be safe on class-default metaobjects.
	 * !!may benefit from hierarchical propagation, deleting keys that match superclass...not sure what's best yet.
	 */
	void SaveConfig( uint64 Flags=CPF_Config, const TCHAR* Filename=NULL, FConfigCacheIni* Config=GConfig );

	/**
	 * Saves just the section(s) for this class into the default ini file for the class (with just the changes from base)
	 */
	void UpdateDefaultConfigFile();

	/**
	 * Imports property values from an .ini file.
	 *
	 * @param	Class				the class to use for determining which section of the ini to retrieve text values from
	 * @param	Filename			indicates the filename to load values from; if not specified, uses ConfigClass's ClassConfigName
	 * @param	PropagationFlags	indicates how this call to LoadConfig should be propagated; expects a bitmask of UE4::ELoadConfigPropagationFlags values.
	 * @param	PropertyToLoad		if specified, only the ini value for the specified property will be imported.
	 */
	void LoadConfig( UClass* ConfigClass=NULL, const TCHAR* Filename=NULL, uint32 PropagationFlags=UE4::LCPF_None, class UProperty* PropertyToLoad=NULL );

	/**
	 * Wrapper method for LoadConfig that is used when reloading the config data for objects at runtime which have already loaded their config data at least once.
	 * Allows the objects the receive a callback that it's configuration data has been reloaded.
	 *
	 * @param	Class				the class to use for determining which section of the ini to retrieve text values from
	 * @param	Filename			indicates the filename to load values from; if not specified, uses ConfigClass's ClassConfigName
	 * @param	PropagationFlags	indicates how this call to LoadConfig should be propagated; expects a bitmask of UE4::ELoadConfigPropagationFlags values.
	 * @param	PropertyToLoad		if specified, only the ini value for the specified property will be imported
	 */
	void ReloadConfig( UClass* ConfigClass=NULL, const TCHAR* Filename=NULL, uint32 PropagationFlags=UE4::LCPF_None, class UProperty* PropertyToLoad=NULL );

	/** Import an object from a file. */
	void ParseParms( const TCHAR* Parms );

	/**
	 * Outputs a string to an arbitrary output device, describing the list of objects which are holding references to this one.
	 *
	 * @param	Ar						the output device to send output to
	 * @param	Referencers				optionally allows the caller to specify the list of references to output.
	 */
	void OutputReferencers( FOutputDevice& Ar, FReferencerInformationList* Referencers=NULL );
	
	// @todo document
	void RetrieveReferencers( TArray<FReferencerInformation>* OutInternalReferencers, TArray<FReferencerInformation>* OutExternalReferencers);

	/**
	 * Changes the linker and linker index to the passed in one. A linker of NULL and linker index of INDEX_NONE
	 * indicates that the object is without a linker.
	 *
	 * @param LinkerLoad				New LinkerLoad object to set
	 * @param LinkerIndex				New LinkerIndex to set
	 * @param bShouldDetachExisting		If true, detach existing linker and call PostLinkerChange
	 */
	void SetLinker( ULinkerLoad* LinkerLoad, int32 LinkerIndex, bool bShouldDetachExisting=true );

	/**
	 * Creates a new archetype based on this UObject.  The archetype's property values will match
	 * the current values of this UObject.
	 *
	 * @param	ArchetypeName			the name for the new class
	 * @param	ArchetypeOuter			the outer to create the new class in (package?)
	 * @param	AlternateArchetype		if specified, is set as the ObjectArchetype for the newly created archetype, after the new archetype
	 *									is initialized against "this".  Should only be specified in cases where you need the new archetype to
	 *									inherit the property values of this object, but don't want this object to be the new archetype's ObjectArchetype.
	 * @param	InstanceGraph			contains the mappings of instanced objects and components to their templates
	 *
	 * @return	a pointer to a UObject which has values identical to this object
	 */
	UObject* CreateArchetype( const TCHAR* ArchetypeName, UObject* ArchetypeOuter, UObject* AlternateArchetype=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL );

	/**
	 *	Update the ObjectArchetype of this UObject based on this UObject's properties.
	 */
	void UpdateArchetype();

	/**
	 * Return the template that an object with this class, outer and name would be
	 * 
	 * @return the archetype for this object
	 */
	static UObject* GetArchetypeFromRequiredInfo(UClass* Class, UObject* Outer, FName Name, bool bIsCDO);

	/**
	 * Return the template this object is based on. 
	 * 
	 * @return the archetype for this object
	 */
	UObject* GetArchetype() const
	{
		return GetArchetypeFromRequiredInfo(GetClass(), GetOuter(), GetFName(), HasAnyFlags(RF_ClassDefaultObject));
	}

	/**
	 * Builds a list of objects which have this object in their archetype chain.
	 *
	 * @param	Instances	receives the list of objects which have this one in their archetype chain
	 */
	void GetArchetypeInstances( TArray<UObject*>& Instances );

	/**
	 * Wrapper for calling UClass::InstanceSubobjectTemplates() for this object.
	 */
	void InstanceSubobjectTemplates( struct FObjectInstancingGraph* InstanceGraph = NULL );

	/**
	 * Returns true if this object implements the interface T, false otherwise.
	 */
	template<class T>
	bool Implements() const;

	//==========================================
	// Kismet Virtual Machine
	//==========================================
	//@todo UE4 - do these really need to be virtual to support actors?

	//
	// Script processing functions.
	//
	virtual void ProcessEvent( UFunction* Function, void* Parms );

	/**
	 * Return the space this function should be called.   Checks to see if this function should
	 * be called locally, remotely, or simply absorbed under the given conditions
	 *
	 * @param Function function to call
	 * @param Parameters arguments to the function call
	 * @param Stack stack frame for the function call
	 * @return bitmask representing all callspaces that apply to this UFunction in the given context
	 */
	virtual int32 GetFunctionCallspace( UFunction* Function, void* Parameters, FFrame* Stack )
	{
		return FunctionCallspace::Local;
	}

	/**
	 * Call the actor's function remotely
	 *
	 * @param Function function to call
	 * @param Parameters arguments to the function call
	 * @param Stack stack frame for the function call
	 */
	virtual bool CallRemoteFunction( UFunction* Function, void* Parms, struct FOutParmRec* OutParms, FFrame* Stack )
	{
		return false;
	}

	/** Command line. */
	bool CallFunctionByNameWithArguments( const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor );

	// Call a function
	void CallFunction( FFrame& Stack, RESULT_DECL, UFunction* Function );

	//
	// Internal function call processing.
	// @warning: might not write anything to Result if proper type isn't returned.
	//
	void ProcessInternal( FFrame& Stack, RESULT_DECL );

	/**
	 * This function handles a console exec sent to the object; it is virtual so 'nexus' objects like
	 * a player controller can reroute the command to several different objects.
	 */
	virtual bool ProcessConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor)
	{
		return CallFunctionByNameWithArguments(Cmd, Ar, Executor);
	}

	/** advances Stack's code past the parameters to the given Function and if the function has a return value, copies the zero value for that property to the memory for the return value
	 * @param Stack the script stack frame
	 * @param Result pointer to where the return value should be written
	 * @param Function the function being called
	 */
	void SkipFunction(FFrame& Stack, RESULT_DECL, UFunction* Function);

	/**
	 * Called on the target when a class is loaded with ClassGeneratedBy is loaded.  Should regenerate the class if needed, and return the updated class
	 * @return	Updated instance of the class, if needed, or NULL if no regeneration was necessary
	 */
	virtual UClass* RegenerateClass(UClass* ClassToRegenerate, UObject* PreviousCDO, TArray<UObject*>& ObjLoaded) { return NULL; };

	/** 
	 * Returns whether this object is contained in or part of a blueprint object
	 */
	bool IsInBlueprint() const;

	/** 
	 *  Destroy properties that won't be destroyed by the native destructor
	 */
	void DestroyNonNativeProperties();

	// UnrealScript intrinsics.

	// Undefined native handler
	DECLARE_FUNCTION(execUndefined);

	
	// @todo document below declared functions
	// Variables
	DECLARE_FUNCTION(execLocalVariable);
	DECLARE_FUNCTION(execInstanceVariable);
	DECLARE_FUNCTION(execDefaultVariable);
	DECLARE_FUNCTION(execLocalOutVariable);
	DECLARE_FUNCTION(execInterfaceVariable);
	DECLARE_FUNCTION(execInterfaceContext);
	DECLARE_FUNCTION(execArrayElement);
	DECLARE_FUNCTION(execBoolVariable);
	DECLARE_FUNCTION(execClassDefaultVariable);
	DECLARE_FUNCTION(execEndFunctionParms);


	// Do Nothing 
	DECLARE_FUNCTION(execNothing);
	DECLARE_FUNCTION(execNothingOp4a);

	/** Breakpoint; only observed in the editor; executing it at any other time is a NOP */
	DECLARE_FUNCTION(execBreakpoint);

	/** Tracepoint; only observed in the editor; executing it at any other time is a NOP */
	DECLARE_FUNCTION(execTracepoint);
	DECLARE_FUNCTION(execWireTracepoint);

	DECLARE_FUNCTION(execEndOfScript);

	/** failsafe for functions that return a value - returns the zero value for a property and logs that control reached the end of a non-void function */
	DECLARE_FUNCTION(execReturnNothing);
	DECLARE_FUNCTION(execEmptyParmValue);

	// Commands
	DECLARE_FUNCTION(execJump);
	DECLARE_FUNCTION(execJumpIfNot);
	DECLARE_FUNCTION(execAssert);

	/** 
	 * Push a code offset onto the execution flow stack for future execution.  
	 * Current execution continues to the next instruction after the push one.
	 */
	DECLARE_FUNCTION(execPushExecutionFlow);

	/**
	 * Pops a code offset from the execution flow stack and starts execution there.
	 * If there are no stack entries left, it is treated as an execution error.
	 */
	DECLARE_FUNCTION(execPopExecutionFlow);
	DECLARE_FUNCTION(execComputedJump);

	/** 
	 * Pops a code offset from the execution flow stack and starts execution there, if a condition is not true.
	 * If there are no stack entries left, it is treated as an execution error.
	 */
	DECLARE_FUNCTION(execPopExecutionFlowIfNot);


	// Assignment
	DECLARE_FUNCTION(execLet);
	DECLARE_FUNCTION(execLetObj);
	DECLARE_FUNCTION(execLetWeakObjPtr);
	DECLARE_FUNCTION(execLetBool);
	DECLARE_FUNCTION(execLetDelegate);
	DECLARE_FUNCTION(execLetMulticastDelegate);

	// Delegates 
	DECLARE_FUNCTION(execAddMulticastDelegate);
	DECLARE_FUNCTION(execClearMulticastDelegate);
	DECLARE_FUNCTION(execEatReturnValue);
	DECLARE_FUNCTION(execRemoveMulticastDelegate);

	// Context expressions
	DECLARE_FUNCTION(execSelf);
	DECLARE_FUNCTION(execContext);
	DECLARE_FUNCTION(execContext_FailSilent);
	DECLARE_FUNCTION(execStructMemberContext);

	// Function calls
	DECLARE_FUNCTION(execVirtualFunction);
	DECLARE_FUNCTION(execFinalFunction);

	// Struct comparison
	DECLARE_FUNCTION(execStructCmpEq);
	DECLARE_FUNCTION(execStructCmpNe);
	DECLARE_FUNCTION(execStructMember);

	// @todo delegate: Delegate comparison is not supported for multi-cast delegates
	DECLARE_FUNCTION(execEqualEqual_DelegateDelegate);
	DECLARE_FUNCTION(execNotEqual_DelegateDelegate);
	DECLARE_FUNCTION(execEqualEqual_DelegateFunction);
	DECLARE_FUNCTION(execNotEqual_DelegateFunction);

	// Constants
	DECLARE_FUNCTION(execIntConst);
	DECLARE_FUNCTION(execSkipOffsetConst);
	DECLARE_FUNCTION(execFloatConst);
	DECLARE_FUNCTION(execStringConst);
	DECLARE_FUNCTION(execUnicodeStringConst);
	DECLARE_FUNCTION(execTextConst);
	DECLARE_FUNCTION(execObjectConst);

	// @todo delegate: Multi-cast versions needed for script execution!		(Need Add, Remove, Clear/Empty)
	DECLARE_FUNCTION(execInstanceDelegate);
	DECLARE_FUNCTION(execNameConst);
	DECLARE_FUNCTION(execByteConst);
	DECLARE_FUNCTION(execIntZero);
	DECLARE_FUNCTION(execIntOne);
	DECLARE_FUNCTION(execTrue);
	DECLARE_FUNCTION(execFalse);
	DECLARE_FUNCTION(execNoObject);
	DECLARE_FUNCTION(execIntConstByte);
	DECLARE_FUNCTION(execRotationConst);
	DECLARE_FUNCTION(execVectorConst);
	DECLARE_FUNCTION(execTransformConst);
	DECLARE_FUNCTION(execStructConst);
	DECLARE_FUNCTION(execSetArray);

	// Object construction
	DECLARE_FUNCTION(execNew);
	DECLARE_FUNCTION(execClassContext);
	DECLARE_FUNCTION(execNativeParm);

	// Conversions 
	DECLARE_FUNCTION(execDynamicCast);
	DECLARE_FUNCTION(execMetaCast);
	DECLARE_FUNCTION(execPrimitiveCast);
	DECLARE_FUNCTION(execInterfaceCast);

	// Cast functions
	DECLARE_FUNCTION(execObjectToBool);
	DECLARE_FUNCTION(execInterfaceToBool);
	DECLARE_FUNCTION(execObjectToInterface);
	DECLARE_FUNCTION(execInterfaceToInterface);

	// Dynamic array functions
	// Array support
	DECLARE_FUNCTION(execGetDynArrayElement);
	DECLARE_FUNCTION(execSetDynArrayElement);
	DECLARE_FUNCTION(execGetDynArrayLength);
	DECLARE_FUNCTION(execSetDynArrayLength);
	DECLARE_FUNCTION(execDynArrayInsert);
	DECLARE_FUNCTION(execDynArrayRemove);
	DECLARE_FUNCTION(execDynArrayFind);
	DECLARE_FUNCTION(execDynArrayFindStruct);
	DECLARE_FUNCTION(execDynArrayAdd);
	DECLARE_FUNCTION(execDynArrayAddItem);
	DECLARE_FUNCTION(execDynArrayInsertItem);
	DECLARE_FUNCTION(execDynArrayRemoveItem);
	DECLARE_FUNCTION(execDynArraySort);

	DECLARE_FUNCTION(execBindDelegate);
	DECLARE_FUNCTION(execCallMulticastDelegate);

	// -- K2 support functions
	struct Object_eventExecuteUbergraph_Parms
	{
		int32 EntryPoint;
	};
	virtual void ExecuteUbergraph(int32 EntryPoint)
	{
		Object_eventExecuteUbergraph_Parms Parms;
		Parms.EntryPoint=EntryPoint;
		ProcessEvent(FindFunctionChecked(NAME_ExecuteUbergraph),&Parms);
	}

protected: 

	/** Checks it's ok to perform subobjects check at this time. */
	bool CanCheckDefaultSubObjects(bool bForceCheck, bool& bResult);

private:
	void ProcessContextOpcode(FFrame& Stack, RESULT_DECL, bool bCanFailSilent);
};

#endif	// __UNOBJUOBJECT_H__

