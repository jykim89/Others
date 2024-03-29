// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraphPin.h"
#include "BlueprintCore.h"
#include "Blueprint.generated.h"

/** States a blueprint can be in */
UENUM()
enum EBlueprintStatus
{
	// Blueprint is in an unknown state
	BS_Unknown,
	// Blueprint has been modified but not recompiled
	BS_Dirty,
	// Blueprint tried but failed to be compiled
	BS_Error,
	// Blueprint has been compiled since it was last modified
	BS_UpToDate,
	// Blueprint is in the process of being created for the first time
	BS_BeingCreated,
	// Blueprint has been compiled since it was last modified. There are warnings.
	BS_UpToDateWithWarnings,
	BS_MAX,
};

/** Types of blueprints */
UENUM()
enum EBlueprintType
{
	// Normal blueprint
	BPTYPE_Normal				UMETA(DisplayName="Blueprint"),
	// Blueprint that is const during execution (no state graph and methods cannot modify member variables)
	BPTYPE_Const				UMETA(DisplayName="Const Blueprint"),
	// Blueprint that serves as a container for macros to be used in other blueprints
	BPTYPE_MacroLibrary			UMETA(DisplayName="Blueprint Macro Library"),
	// Blueprint that serves as an interface to be implemented by other blueprints
	BPTYPE_Interface			UMETA(DisplayName="Blueprint Interface"),
	// Blueprint that handles level scripting
	BPTYPE_LevelScript			UMETA(DisplayName="Level Blueprint"),
	// Blueprint that servers as a container for functions to be used in other blueprints
	BPTYPE_FunctionLibrary		UMETA(DisplayName="Blueprint Function Library"),

	BPTYPE_MAX,
};


// Type of compilation
namespace EKismetCompileType
{
	enum Type
	{
		SkeletonOnly,
		Full,
		StubAfterFailure, 
		BytecodeOnly,
	};
};

struct FKismetCompilerOptions
{
public:
	/** The compile type to perform (full compile, skeleton pass only, etc) */
	EKismetCompileType::Type	CompileType;

	/** Whether or not to save intermediate build products (temporary graphs and expanded macros) for debugging */
	bool bSaveIntermediateProducts;

	/** Whether or not this compile is for a duplicated blueprint */
	bool bIsDuplicationInstigated;

	bool DoesRequireBytecodeGeneration() const
	{
		return (CompileType == EKismetCompileType::Full) || (CompileType == EKismetCompileType::BytecodeOnly);
	}

	/** Whether or not this compile type should operate on the generated class of the blueprint, as opposed to just the skeleton */
	bool IsGeneratedClassCompileType() const
	{
		return (CompileType != EKismetCompileType::SkeletonOnly);
	}

	FKismetCompilerOptions()
		: CompileType(EKismetCompileType::Full)
		, bSaveIntermediateProducts(false)
		, bIsDuplicationInstigated(false)
	{
	};
};


/** One metadata entry for a variable */
USTRUCT()
struct FBPVariableMetaDataEntry
{
	GENERATED_USTRUCT_BODY()

	/** Name of metadata key */
	UPROPERTY(EditAnywhere, Category=BPVariableMetaDataEntry)
	FName DataKey;

	/** Name of metadata value */
	UPROPERTY(EditAnywhere, Category=BPVariableMetaDataEntry)
	FString DataValue;



		FBPVariableMetaDataEntry() {}
		FBPVariableMetaDataEntry(const FName& InKey, const FString& InValue)
		: DataKey(InKey)
		, DataValue(InValue)
		{}
	
};

/** Struct indicating a variable in the generated class */
USTRUCT()
struct FBPVariableDescription
{
	GENERATED_USTRUCT_BODY()

	/** Name of the variable */
	UPROPERTY(EditAnywhere, Category=BPVariableDescription)
	FName VarName;

	/** A Guid that will remain constant even if the VarName changes */
	UPROPERTY()
	FGuid VarGuid;

	/** Type of the variable */
	UPROPERTY(EditAnywhere, Category=BPVariableDescription)
	struct FEdGraphPinType VarType;

	/** Friendly name of the variable */
	UPROPERTY(EditAnywhere, Category=BPVariableDescription)
	FString FriendlyName;

	/** Category this variable should be in */
	UPROPERTY(EditAnywhere, Category=BPVariableDescription)
	FName Category;

	/** Property flags for this variable - Changed from int32 to uint64*/
	UPROPERTY(EditAnywhere, Category=BPVariableDescription)
	uint64 PropertyFlags;

	UPROPERTY(EditAnywhere, Category=BPVariableRepNotify)
	FName RepNotifyFunc;

	/** Metadata information for this variable */
	UPROPERTY(EditAnywhere, Category=BPVariableDescription)
	TArray<struct FBPVariableMetaDataEntry> MetaDataArray;

	/** Optional new default value stored as string*/
	UPROPERTY(EditAnywhere, Category=BPVariableDescription)
	FString DefaultValue;

	FBPVariableDescription()
		: PropertyFlags(CPF_Edit)
	{
	}

	/** Set a metadata value on the variable */
	ENGINE_API void SetMetaData(const FName& Key, const FString& Value);
	/** Gets a metadata value on the variable; asserts if the value isn't present.  Check for validiy using FindMetaDataEntryIndexForKey. */
	ENGINE_API FString GetMetaData(const FName& Key);
	/** Clear metadata value on the variable */
	ENGINE_API void RemoveMetaData(const FName& Key);
	/** Find the index in the array of a metadata entry */
	ENGINE_API int32 FindMetaDataEntryIndexForKey(const FName& Key) const;
	/** Checks if there is metadata for a key */
	ENGINE_API bool HasMetaData(const FName& Key) const;
	
};

/** Struct containing information about what interfaces are implemented in this blueprint */
USTRUCT()
struct FBPInterfaceDescription
{
	GENERATED_USTRUCT_BODY()

	/** Reference to the interface class we're adding to this blueprint */
	UPROPERTY()
	TSubclassOf<class UInterface>  Interface;

	/** References to the graphs associated with the required functions for this interface */
	UPROPERTY()
	TArray<class UEdGraph*> Graphs;


	FBPInterfaceDescription()
		: Interface(NULL)
	{
	}

};

USTRUCT()
struct FEditedDocumentInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	UObject* EditedObject;

	/** Saved view position */
	UPROPERTY()
	FVector2D SavedViewOffset;

	/** Saved zoom amount */
	UPROPERTY()
	float SavedZoomAmount;

	FEditedDocumentInfo()
		: EditedObject(NULL)
		, SavedViewOffset(0.0f, 0.0f)
		, SavedZoomAmount(-1.0f)
	{

	}

	FEditedDocumentInfo(UObject* InEditedObject)
		: EditedObject(InEditedObject)
		, SavedViewOffset(0.0f, 0.0f)
		, SavedZoomAmount(-1.0f)
	{

	}

	FEditedDocumentInfo(UObject* InEditedObject, FVector2D& InSavedViewOffset, float InSavedZoomAmount)
		: EditedObject(InEditedObject)
		, SavedViewOffset(InSavedViewOffset)
		, SavedZoomAmount(InSavedZoomAmount)
	{

	}

	friend bool operator==( const FEditedDocumentInfo& LHS, const FEditedDocumentInfo& RHS )
	{
		return LHS.EditedObject == RHS.EditedObject && LHS.SavedViewOffset == RHS.SavedViewOffset && LHS.SavedZoomAmount == RHS.SavedZoomAmount;
	}
};

///////////////

/**
 * Blueprints are special assets that provide an intuitive, node-based interface that can be used to create new types of Actors
 * and script level events; giving designers and gameplay programmers the tools to quickly create and iterate gameplay from
 * within Unreal Editor without ever needing to write a line of code.
 */
UCLASS(config=Engine, dependson=(UEdGraphPin,UEdGraph,UEdGraphNode), BlueprintType)
class ENGINE_API UBlueprint : public UBlueprintCore
{
	GENERATED_UCLASS_BODY()

	/** Whether or not this blueprint should recompile itself on load */
	UPROPERTY(config)
	uint32 bRecompileOnLoad:1;

	/** Pointer to the parent class that the generated class should derive from */
	UPROPERTY(AssetRegistrySearchable)
	TSubclassOf<class UObject>  ParentClass;

	UPROPERTY(transient)
	UObject* PRIVATE_InnermostPreviousCDO;

	/** When the class generated by this blueprint is loaded, it will be recompiled the first time.  After that initial recompile, subsequent loads will skip the regeneration step */
	UPROPERTY(transient)
	uint32 bHasBeenRegenerated:1;

	/** State flag to indicate whether or not the Blueprint is currently being regenerated on load */
	UPROPERTY(transient)
	uint32 bIsRegeneratingOnLoad:1;

#if WITH_EDITORONLY_DATA
	/** Whether or not this blueprint is newly created, and hasn't been opened in an editor yet */
	UPROPERTY(transient)
	uint32 bIsNewlyCreated:1;

	/** Whether to force opening the full (non data-only) editor for this blueprint. */
	UPROPERTY(transient)
	uint32 bForceFullEditor:1;
	/**whether or not you want to continuously rerun the construction script for an actor as you drag it in the editor, or only when the drag operation is complete*/
	UPROPERTY(EditAnywhere, Category=BlueprintOption)
	uint32 bRunConstructionScriptOnDrag:1;

	/** Whether or not this blueprint's class is a const class or not.  Should set CLASS_Const in the KismetCompiler. */
	UPROPERTY(EditAnywhere, Category=BlueprintOption)
	uint32 bGenerateConstClass:1;

	/**shows up in the content browser when the blueprint is hovered */
	UPROPERTY(EditAnywhere, Category=BlueprintOption)
	FString BlueprintDescription;

	/** TRUE to show a warning when attempting to start in PIE and there is a compiler error on this Blueprint */
	UPROPERTY(transient)
	bool bDisplayCompilePIEWarning;

#endif //WITH_EDITORONLY_DATA

	/** 'Simple' construction script - graph of components to instance */
	UPROPERTY()
	class USimpleConstructionScript* SimpleConstructionScript;

#if WITH_EDITORONLY_DATA
	/** Set of pages that combine into a single uber-graph */
	UPROPERTY()
	TArray<class UEdGraph*> UbergraphPages;

	/** Set of functions implemented for this class graphically */
	UPROPERTY()
	TArray<class UEdGraph*> FunctionGraphs;

	/** Graphs of signatures for delegates */
	UPROPERTY()
	TArray<class UEdGraph*> DelegateSignatureGraphs;

	/** Set of macros implemented for this class */
	UPROPERTY()
	TArray<class UEdGraph*> MacroGraphs;

	/** Set of functions actually compiled for this class */
	UPROPERTY(transient, duplicatetransient)
	TArray<class UEdGraph*> IntermediateGeneratedGraphs;

	/** Set of functions actually compiled for this class */
	UPROPERTY(transient, duplicatetransient)
	TArray<class UEdGraph*> EventGraphs;
#endif // WITH_EDITORONLY_DATA

	/** Array of component template objects, used by AddComponent function */
	UPROPERTY()
	TArray<class UActorComponent*> ComponentTemplates;

	/** Array of templates for timelines that should be created */
	UPROPERTY()
	TArray<class UTimelineTemplate*> Timelines;

	/** The type of this blueprint */
	UPROPERTY(AssetRegistrySearchable)
	TEnumAsByte<enum EBlueprintType> BlueprintType;

#if WITH_EDITORONLY_DATA
	/** The current status of this blueprint */
	UPROPERTY(transient)
	TEnumAsByte<enum EBlueprintStatus> Status;

	/** Array of new variables to be added to generated class */
	UPROPERTY()
	TArray<struct FBPVariableDescription> NewVariables;

	/** Array of info about the interfaces we implement in this blueprint */
	UPROPERTY(AssetRegistrySearchable)
	TArray<struct FBPInterfaceDescription> ImplementedInterfaces;
	
#endif // WITH_EDITORONLY_DATA

	/** The version of the blueprint system that was used to  create this blueprint */
	UPROPERTY()
	int32 BlueprintSystemVersion;

#if WITH_EDITORONLY_DATA
	/** Set of documents that were being edited in this blueprint, so we can open them right away */
	UPROPERTY()
	TArray<struct FEditedDocumentInfo> LastEditedDocuments;

	/** Persistent debugging options */
	UPROPERTY()
	TArray<class UBreakpoint*> Breakpoints;

	UPROPERTY()
	TArray<class UEdGraphPin*> PinWatches;
#endif // WITH_EDITORONLY_DATA

public:
	/** Broadcasts a notification whenever the blueprint has changed. */
	DECLARE_EVENT_OneParam( UBlueprint, FChangedEvent, class UBlueprint* );
	FChangedEvent& OnChanged() { return ChangedEvent; }

	/**	This should NOT be public */
	void BroadcastChanged() { ChangedEvent.Broadcast( this ); }

#if WITH_EDITORONLY_DATA
protected:
	/** Current object being debugged for this blueprint */
	TWeakObjectPtr< class UObject > CurrentObjectBeingDebugged;

	/** Current world being debugged for this blueprint */
	TWeakObjectPtr< class UWorld > CurrentWorldBeingDebugged;
public:
	/** Information for thumbnail rendering */
	UPROPERTY()
	class UThumbnailInfo* ThumbnailInfo;

	/** The blueprint is currently compiled */
	UPROPERTY(transient)
	uint32 bBeingCompiled:1;
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITORONLY_DATA
	bool IsUpToDate() const
	{
		return BS_UpToDate == Status || BS_UpToDateWithWarnings == Status;
	}

	bool IsPossiblyDirty() const
	{
		return (BS_Dirty == Status) || (BS_Unknown == Status);
	}
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	static bool ValidateGeneratedClass(const UClass* InClass);

	/** Find the object in the TemplateObjects array with the supplied name */
	UActorComponent* FindTemplateByName(const FName& TemplateName);

	/** Find a timeline by name */
	class UTimelineTemplate* FindTimelineTemplateByVariableName(const FName& TimelineName);	

	/** Find a timeline by name */
	const class UTimelineTemplate* FindTimelineTemplateByVariableName(const FName& TimelineName) const;	

	void GetBlueprintClassNames(FName& GeneratedClassName, FName& SkeletonClassName, FName NameOverride = NAME_None) const
	{
		FName NameToUse = (NameOverride != NAME_None) ? NameOverride : GetFName();

		const FString GeneratedClassNameString = FString::Printf(TEXT("%s_C"), *NameToUse.ToString());
		GeneratedClassName = FName(*GeneratedClassNameString);

		const FString SkeletonClassNameString = FString::Printf(TEXT("SKEL_%s_C"), *NameToUse.ToString());
		SkeletonClassName = FName(*SkeletonClassNameString);
	}
	
	void GetBlueprintCDONames(FName& GeneratedClassName, FName& SkeletonClassName, FName NameOverride = NAME_None) const
	{
		FName NameToUse = (NameOverride != NAME_None) ? NameOverride : GetFName();

		const FString GeneratedClassNameString = FString::Printf(TEXT("Default__%s_C"), *NameToUse.ToString());
		GeneratedClassName = FName(*GeneratedClassNameString);

		const FString SkeletonClassNameString = FString::Printf(TEXT("Default__SKEL_%s_C"), *NameToUse.ToString());
		SkeletonClassName = FName(*SkeletonClassNameString);
	}

	// Should the generic blueprint factory work for this blueprint?
	virtual bool SupportedByDefaultBlueprintFactory() const
	{
		return true;
	}

	/** Sets the current object being debugged */
	virtual void SetObjectBeingDebugged(UObject* NewObject);

	virtual void SetWorldBeingDebugged(UWorld* NewWorld);

protected:
	/** Gets asset registry tags */
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const OVERRIDE;

private:
	/** Sets the current object being debugged */
	void DebuggingWorldRegistrationHelper(UObject* ObjectProvidingWorld, UObject* ValueToRegister);

	
public:
	/** @return the current object being debugged, which can be NULL */
	virtual UObject* GetObjectBeingDebugged();

	virtual class UWorld* GetWorldBeingDebugged();


	// Begin UObject interface (WITH_EDITOR)
	virtual void PostDuplicate(bool bDuplicateForPIE) OVERRIDE;
	virtual bool Rename(const TCHAR* NewName = NULL, UObject* NewOuter = NULL, ERenameFlags Flags = REN_None) OVERRIDE;
	virtual UClass* RegenerateClass(UClass* ClassToRegenerate, UObject* PreviousCDO, TArray<UObject*>& ObjLoaded) OVERRIDE;
	virtual void PostLoad() OVERRIDE;
	virtual void PostLoadSubobjects( FObjectInstancingGraph* OuterInstanceGraph ) OVERRIDE;
	// End of UObject interface

	/** Consigns the GeneratedClass and the SkeletonGeneratedClass to oblivion, and nulls their references */
	void RemoveGeneratedClasses();

	/** @return the user-friendly name of the blueprint */
	virtual FString GetFriendlyName() const;

	bool ChangeOwnerOfTemplates();

#endif	//#if WITH_EDITOR

	// Begin UObject interface
	virtual void Serialize(FArchive& Ar) OVERRIDE;
	virtual FString GetDesc(void) OVERRIDE;
	virtual void TagSubobjects(EObjectFlags NewFlags) OVERRIDE;
	virtual bool NeedsLoadForClient() const OVERRIDE;
	virtual bool NeedsLoadForServer() const OVERRIDE;
	// End of UObject interface

	/** Get the Blueprint object that generated the supplied class */
	static UBlueprint* GetBlueprintFromClass(const UClass* InClass);

	/** 
	 * Gets an array of all blueprints used to generate this class and its parents.  0th elements is the BP used to generate InClass
	 *
	 * @param InClass				The class to get the blueprint lineage for
	 * @param OutBlueprintParents	Array with the blueprints used to generate this class and its parents.  0th = this, Nth = least derived BP-based parent
	 * @return						true if there were no status errors in any of the parent blueprints, otherwise false
	 */
	static bool GetBlueprintHierarchyFromClass(const UClass* InClass, TArray<UBlueprint*>& OutBlueprintParents);

#if WITH_EDITORONLY_DATA
	template<class TFieldType>
	static FName GetFieldNameFromClassByGuid(const UClass* InClass, const FGuid VarGuid)
	{
		UProperty* AssertPropertyType = (TFieldType*)0;

		TArray<UBlueprint*> Blueprints;
		UBlueprint::GetBlueprintHierarchyFromClass(InClass, Blueprints);

		for (int32 BPIndex = 0; BPIndex < Blueprints.Num(); ++BPIndex)
		{
			UBlueprint* Blueprint = Blueprints[BPIndex];
			for (int32 VarIndex = 0; VarIndex < Blueprint->NewVariables.Num(); ++VarIndex)
			{
				const FBPVariableDescription& BPVarDesc = Blueprint->NewVariables[VarIndex];
				if (BPVarDesc.VarGuid == VarGuid)
				{
					return BPVarDesc.VarName;
				}
			}
		}

		return NAME_None;
	}

	template<class TFieldType>
	static bool GetGuidFromClassByFieldName(const UClass* InClass, const FName VarName, FGuid& VarGuid)
	{
		UProperty* AssertPropertyType = (TFieldType*)0;

		TArray<UBlueprint*> Blueprints;
		UBlueprint::GetBlueprintHierarchyFromClass(InClass, Blueprints);

		for (int32 BPIndex = 0; BPIndex < Blueprints.Num(); ++BPIndex)
		{
			UBlueprint* Blueprint = Blueprints[BPIndex];
			for (int32 VarIndex = 0; VarIndex < Blueprint->NewVariables.Num(); ++VarIndex)
			{
				const FBPVariableDescription& BPVarDesc = Blueprint->NewVariables[VarIndex];
				if (BPVarDesc.VarName == VarName)
				{
					VarGuid = BPVarDesc.VarGuid;
					return true;
				}
			}
		}

		return false;
	}

	static FName GetFunctionNameFromClassByGuid(const UClass* InClass, const FGuid FunctionGuid);
	static bool GetFunctionGuidFromClassByFieldName(const UClass* InClass, const FName FunctionName, FGuid& FunctionGuid);
#endif

	/** Find a function given its name and optionally an object property name within this Blueprint */
	ETimelineSigType GetTimelineSignatureForFunctionByName(const FName& FunctionName, const FName& ObjectPropertyName);

	/** Gets the current blueprint system version. Note- incrementing this version will invalidate ALL existing blueprints! */
	static int32 GetCurrentBlueprintSystemVersion()
	{
		return 2;
	}

	/** Get all graphs in this blueprint */
	void GetAllGraphs(TArray<UEdGraph*>& Graphs) const;

private:

	/** Broadcasts a notification whenever the blueprint has changed. */
	FChangedEvent ChangedEvent;

#if WITH_EDITOR
public:
	/** If this blueprint is currently being compiled, the CurrentMessageLog will be the log currently being used to send logs to. */
	class FCompilerResultsLog* CurrentMessageLog;

	/** 
	 * Sends a message to the CurrentMessageLog, if there is one available.  Otherwise, defaults to logging to the normal channels.
	 * Should use this for node and blueprint actions that happen during compilation!
	 */
	void Message_Note(const FString& MessageToLog);
	void Message_Warn(const FString& MessageToLog);
	void Message_Error(const FString& MessageToLog);
#endif

};

#if WITH_EDITORONLY_DATA
template<>
inline FName UBlueprint::GetFieldNameFromClassByGuid<UFunction>(const UClass* InClass, const FGuid FunctionGuid)
{
	return GetFunctionNameFromClassByGuid(InClass, FunctionGuid);
}

template<>
inline bool UBlueprint::GetGuidFromClassByFieldName<UFunction>(const UClass* InClass, const FName FunctionName, FGuid& FunctionGuid)
{
	return GetFunctionGuidFromClassByFieldName(InClass, FunctionName, FunctionGuid);
}
#endif // #if WITH_EDITORONLY_DATA
