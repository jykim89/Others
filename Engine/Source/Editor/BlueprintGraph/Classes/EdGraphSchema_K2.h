// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.generated.h"

/** Reference to an structure (only used in 'docked' palette) */
USTRUCT()
struct BLUEPRINTGRAPH_API FEdGraphSchemaAction_K2Struct : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY()

	// Simple type info
	static FString StaticGetTypeId() {static FString Type = TEXT("FEdGraphSchemaAction_K2Struct"); return Type;}
	virtual FString GetTypeId() const OVERRIDE { return StaticGetTypeId(); } 

	UStruct* Struct;

	void AddReferencedObjects( FReferenceCollector& Collector ) OVERRIDE
	{
		if( Struct )
		{
			Collector.AddReferencedObject(Struct);
		}
	}

	FName GetPathName() const
	{
		return Struct ? FName(*Struct->GetPathName()) : NAME_None;
	}

	FEdGraphSchemaAction_K2Struct() 
		: FEdGraphSchemaAction()
	{}

	FEdGraphSchemaAction_K2Struct (const FString& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InGrouping)
	{}
};

// Constants used for metadata, etc... in blueprints
struct BLUEPRINTGRAPH_API FBlueprintMetadata
{
public:
	// Struct/Enum/Class:
	// If true, this class, struct, or enum is a valid type for use as a variable in a blueprint
	static const FName MD_AllowableBlueprintVariableType;
	
	// If true, this class, struct, or enum is not valid for use as a variable in a blueprint
	static const FName MD_NotAllowableBlueprintVariableType;

	// Class:
	// [ClassMetadata] If present, the component class can be spawned by a blueprint
	static const FName MD_BlueprintSpawnableComponent;

	/** If true, the class will be usable as a base for blueprints */
	static const FName MD_IsBlueprintBase;

	//    function metadata
	/** Specifies a UFUNCTION as Kismet protected, which can only be called from itself */
	static const FName MD_Protected;

	/** Marks a UFUNCTION as latent execution */
	static const FName MD_Latent;

	/** Marks a UFUNCTION as unsafe for use in the UCS, which prevents it from being called from the UCS.  Useful for things that spawn actors, etc that should never happen in the UCS */
	static const FName MD_UnsafeForConstructionScripts;

	// The category that a function appears under in the palette
	static const FName MD_FunctionCategory;

	// [FunctionMetadata] Indicates that the function is deprecated
	static const FName MD_DeprecatedFunction;

	// [FunctionMetadata] Supplies the custom message to use for deprecation
	static const FName MD_DeprecationMessage;

	// [FunctionMetadata] Indicates that the function should be drawn as a compact node with the specified body title
	static const FName MD_CompactNodeTitle;
	
	// [FunctionMetadata] Indicates that the function should be drawn with this title over the function name
	static const FName MD_FriendlyName;

	//    property metadata

	/** UPROPERTY will be exposed on "Spawn Blueprint" nodes as an input  */
	static const FName MD_ExposeOnSpawn;

	/** UPROPERTY cannot be modified by other blueprints */
	static const FName MD_Private;

	/** If true, the specified UObject parameter will default to "self" if nothing is connected */
	static const FName MD_DefaultToSelf;

	/** The specified parameter should be used as the context object when retrieving a UWorld pointer (implies hidden and default-to-self) */
	static const FName MD_WorldContext;

	/** If true, an unconnected pin will generate a UPROPERTY under the hood to connect as the input, which will be set to the literal value for the pin.  Only valid for reference parameters. */
	static const FName MD_AutoCreateRefTerm;

	/** If true, the hidden default to self pins will be visible when the function is placed in a child blueprint of the class. */
	static const FName MD_ShowHiddenSelfPins;

	static const FName MD_BlueprintInternalUseOnly;
	static const FName MD_NeedsLatentFixup;

	static const FName MD_LatentCallbackTarget;

	/** If true, properties defined in the C++ private scope will be accessible to blueprints */
	static const FName MD_AllowPrivateAccess;

	/** Categories of functions to expose on this property */
	static const FName MD_ExposeFunctionCategories;

	// [InterfaceMetadata]
	static const FName MD_CannotImplementInterfaceInBlueprint;
	static const FName MD_ProhibitedInterfaces;

	/** Keywords used when searching for functions */
	static const FName MD_FunctionKeywords;

	/** Indicates that during compile we want to create multiple exec pins from an enum param */
	static const FName MD_ExpandEnumAsExecs;

	static const FName MD_CommutativeAssociativeBinaryOperator;

	/** Metadata string that indicates to use the MaterialParameterCollectionFunction node. */
	static const FName MD_MaterialParameterCollectionFunction;

	/** Metadata string that sets the tooltip */
	static const FName MD_Tooltip;
private:
	// This class should never be instantiated
	FBlueprintMetadata() {}
};

/** Information about what we want to call this function on */
struct FFunctionTargetInfo
{
	enum EFunctionTarget
	{
		EFT_Default, // Just call function on target object
		EFT_Actor, // Create an Actor node and wire to target
		EFT_Component // Create a component variable ref node and wire to target
	};

	/** What kind of call function action are we creating */
	EFunctionTarget FunctionTarget;

	/** If CFO_Actor, call on these Actors. */
	TArray< TWeakObjectPtr<AActor> > Actors;

	/** If CFO_Component, call on this component variable of blueprint */
	FName ComponentPropertyName;

	// Constructor
	FFunctionTargetInfo()
		: FunctionTarget(EFT_Default)
		, ComponentPropertyName(NAME_None)
	{}

	FFunctionTargetInfo(TArray<AActor*>& InActors)
		: FunctionTarget(EFT_Actor)
		, ComponentPropertyName(NAME_None)
	{
		for ( int32 ActorIndex = 0; ActorIndex < InActors.Num(); ActorIndex++ )
		{
			if ( InActors[ActorIndex] != NULL )
			{
				Actors.Add( TWeakObjectPtr<AActor>( InActors[ActorIndex] ) );
			}
		}
	}

	FFunctionTargetInfo( FName InComponentPropertyName )
		: FunctionTarget(EFT_Component)
		, ComponentPropertyName(InComponentPropertyName)
	{}
};

UCLASS(config=Editor)
class BLUEPRINTGRAPH_API UEdGraphSchema_K2 : public UEdGraphSchema
{
	GENERATED_UCLASS_BODY()

	// Allowable PinType.PinCategory values
	UPROPERTY()
	FString PC_Exec;

	UPROPERTY()
	FString PC_Meta;

	// PC_Array - not implemented yet
	UPROPERTY()
	FString PC_Boolean;

	UPROPERTY()
	FString PC_Byte;

	UPROPERTY()
	FString PC_Class;    // SubCategoryObject is the MetaClass of the Class passed thru this pin, or SubCategory can be 'self'. The DefaultValue string should always be empty, use DefaultObject.

	UPROPERTY()
	FString PC_Int;

	UPROPERTY()
	FString PC_Float;

	UPROPERTY()
	FString PC_Name;

	UPROPERTY()
	FString PC_Delegate;	// SubCategoryObject is the UFunction of the delegate signature

	UPROPERTY()
	FString PC_MCDelegate;	// SubCategoryObject is the UFunction of the delegate signature

	UPROPERTY()
	FString PC_Object;		// SubCategoryObject is the Class of the object passed thru this pin, or SubCategory can be 'self'. The DefaultValue string should always be empty, use DefaultObject.

	UPROPERTY()
	FString PC_Interface;	// SubCategoryObject is the Class of the object passed thru this pin.

	UPROPERTY()
	FString PC_String;

	UPROPERTY()
	FString PC_Text;

	UPROPERTY()
	FString PC_Struct;    // SubCategoryObject is the ScriptStruct of the struct passed thru this pin, 'self' is not a valid SubCategory. DefaultObject should always be empty, the DefaultValue string may be used for supported structs.

	UPROPERTY()
	FString PC_Wildcard;  // Special matching rules are imposed by the node itself

	// Common PinType.PinSubCategory values
	UPROPERTY()
	FString PSC_Self;    // Category=PC_Object or PC_Class, indicates the class being compiled

	UPROPERTY()
	FString PSC_Index;	// Category=PC_Wildcard, indicates the wildcard will only accept Int, Bool, Byte and Enum pins (used when a pin represents indexing a list)

	// Pin names that have special meaning and required types in some contexts (depending on the node type)
	UPROPERTY()
	FString PN_Execute;    // Category=PC_Exec, singleton, input

	UPROPERTY()
	FString PN_Then;    // Category=PC_Exec, singleton, output

	UPROPERTY()
	FString PN_Completed;    // Category=PC_Exec, singleton, output

	UPROPERTY()
	FString PN_DelegateEntry;    // Category=PC_Exec, singleton, output; entry point for a dynamically bound delegate

	UPROPERTY()
	FString PN_EntryPoint;	// entry point to the ubergraph

	UPROPERTY()
	FString PN_Self;    // Category=PC_Object, singleton, input

	UPROPERTY()
	FString PN_Else;    // Category=PC_Exec, singleton, output

	UPROPERTY()
	FString PN_Loop;    // Category=PC_Exec, singleton, output

	UPROPERTY()
	FString PN_After;    // Category=PC_Exec, singleton, output

	UPROPERTY()
	FString PN_ReturnValue;		// Category=PC_Object, singleton, output

	UPROPERTY()
	FString PN_ObjectToCast;    // Category=PC_Object, singleton, input

	UPROPERTY()
	FString PN_Condition;    // Category=PC_Boolean, singleton, input

	UPROPERTY()
	FString PN_Start;    // Category=PC_Int, singleton, input

	UPROPERTY()
	FString PN_Stop;    // Category=PC_Int, singleton, input

	UPROPERTY()
	FString PN_Index;    // Category=PC_Int, singleton, output

	UPROPERTY()
	FString PN_CastSucceeded;    // Category=PC_Exec, singleton, output

	UPROPERTY()
	FString PN_CastFailed;    // Category=PC_Exec, singleton, output

	UPROPERTY()
	FString PN_CastedValuePrefix;    // Category=PC_Object, singleton, output; actual pin name varies depending on the type to be casted to, this is just a prefix

	UPROPERTY()
	FString PN_MatineeFinished;    // Category=PC_Exec, singleton, output

	// construction script function names
	UPROPERTY()
	FName FN_UserConstructionScript;

	UPROPERTY()
	FName FN_ExecuteUbergraphBase;

	// metadata keys

	//   class metadata


	// graph names
	UPROPERTY()
	FName GN_EventGraph;

	UPROPERTY()
	FName GN_AnimGraph;

	// variable names
	UPROPERTY()
	FName VR_DefaultCategory;

	// action grouping values
	UPROPERTY()
	int32 AG_LevelReference;

	/** Whether or not the schema should allow the user to use blueprint communications */
	UPROPERTY(globalconfig)
	bool bAllowBlueprintComms;
	
public:
	//////////////////////////////////////////////////////////////////////////
	// FPinTypeInfo
	/** Class used for creating type tree selection info, which aggregates the various PC_* and PinSubtypes in the schema into a heirarchy */
	class BLUEPRINTGRAPH_API FPinTypeTreeInfo
	{
	private:
		FPinTypeTreeInfo()
			: bReadOnly(false)
		{
		}

		void Init(const FString& FriendlyCategoryName, const FString& CategoryName, const UEdGraphSchema_K2* Schema, const FString& InTooltip, bool bInReadOnly);
	
	private:
		/** The pin type corresponding to the schema type */
		FEdGraphPinType PinType;

		/** Asset Reference, used when PinType.PinSubCategoryObject is not loaded yet */
		FStringAssetReference SubCategoryObjectAssetReference;

	public:
		/** The children of this pin type */
		TArray< TSharedPtr<FPinTypeTreeInfo> > Children;

		/** Whether or not this pin type is selectable as an actual type, or is just a category, with some subtypes */
		bool bReadOnly;

		/** Friendly display name of pin type; also used to see if it has subtypes */
		FString FriendlyName;

		/** Text for regular tooltip */
		FString Tooltip;

	public:
		const FEdGraphPinType& GetPinType(bool bForceLoadedSubCategoryObject);
		void SetPinSubTypeCategory(const FString& SubCategory)
		{
			PinType.PinSubCategory = SubCategory;
		}

		FPinTypeTreeInfo(const FString& FriendlyName, const FString& CategoryName, const UEdGraphSchema_K2* Schema, const FString& InTooltip, bool bInReadOnly = false);
		FPinTypeTreeInfo(const FString& CategoryName, const UEdGraphSchema_K2* Schema, const FString& InTooltip, bool bInReadOnly = false);
		FPinTypeTreeInfo(const FString& CategoryName, UObject* SubCategoryObject, const FString& InTooltip, bool bInReadOnly = false);
		FPinTypeTreeInfo(const FString& CategoryName, const FStringAssetReference& SubCategoryObject, const FString& InTooltip, bool bInReadOnly = false);

		FPinTypeTreeInfo(TSharedPtr<FPinTypeTreeInfo> InInfo)
		{
			PinType = InInfo->PinType;
			bReadOnly = InInfo->bReadOnly;
			FriendlyName = InInfo->FriendlyName;
			Tooltip = InInfo->Tooltip;
			SubCategoryObjectAssetReference = InInfo->SubCategoryObjectAssetReference;
		}
		
		/** Returns a succinct menu description of this type */
		FString GetDescription() const
		{
			if ((PinType.PinCategory != FriendlyName) && !FriendlyName.IsEmpty())
			{
				return FriendlyName;
			}
			else if( PinType.PinSubCategoryObject.IsValid() )
			{
				FString DisplayName = PinType.PinSubCategoryObject->GetName();
				//@todo:  Fix this once the XX_YYYY names in the schema are static!  This is mirrored to PC_Class
				if( (PinType.PinCategory == TEXT("class")) && PinType.PinSubCategoryObject->IsA(UClass::StaticClass()) )
				{
					DisplayName = FString::Printf(TEXT("class'%s'"), *DisplayName);
				}

				return DisplayName;
			}
			else if( !PinType.PinCategory.IsEmpty() )
			{
				return PinType.PinCategory;
			}
			else
			{
				return TEXT("Error!");
			}
		}

		FString GetToolTip() const
		{
			if (PinType.PinSubCategoryObject.IsValid())
			{
				if (Tooltip.IsEmpty() || (PinType.PinSubCategoryObject->GetName() == Tooltip))
				{
					if ( (PinType.PinCategory == TEXT("struct")) && PinType.PinSubCategoryObject->IsA(UScriptStruct::StaticClass()) )
					{
						return PinType.PinSubCategoryObject->GetPathName();
					}
				}
			}
			return Tooltip;
		}
	};

public:
	// Begin EdGraphSchema Interface
	virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const OVERRIDE;
	virtual void GetContextMenuActions(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, class FMenuBuilder* MenuBuilder, bool bIsDebugging) const OVERRIDE;
	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const OVERRIDE;
	virtual bool TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const OVERRIDE;
	virtual bool CreateAutomaticConversionNodeAndConnections(UEdGraphPin* A, UEdGraphPin* B) const OVERRIDE;
	virtual FString IsPinDefaultValid(const UEdGraphPin* Pin, const FString& NewDefaultValue, UObject* NewDefaultObject, const FText& InNewDefaultText) const OVERRIDE;
	virtual bool DoesSupportPinWatching() const	OVERRIDE;
	virtual bool IsPinBeingWatched(UEdGraphPin const* Pin) const OVERRIDE;
	virtual void ClearPinWatch(UEdGraphPin const* Pin) const OVERRIDE;
	virtual void TrySetDefaultValue(UEdGraphPin& Pin, const FString& NewDefaultValue) const OVERRIDE;
	virtual void TrySetDefaultObject(UEdGraphPin& Pin, UObject* NewDefaultObject) const OVERRIDE;
	virtual void TrySetDefaultText(UEdGraphPin& InPin, const FText& InNewDefaultText) const OVERRIDE;
	virtual bool ShouldHidePinDefaultValue(UEdGraphPin* Pin) const OVERRIDE;
	virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const OVERRIDE;
	virtual FString GetPinDisplayName(const UEdGraphPin* Pin) const OVERRIDE;
	virtual void ConstructBasicPinTooltip(const UEdGraphPin& Pin, const FString& PinDescription, FString& TooltipOut) const OVERRIDE;
	virtual EGraphType GetGraphType(const UEdGraph* TestEdGraph) const OVERRIDE;
	virtual bool IsTitleBarPin(const UEdGraphPin& Pin) const OVERRIDE;
	virtual void BreakNodeLinks(UEdGraphNode& TargetNode) const OVERRIDE;
	virtual void BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const OVERRIDE;
	virtual void BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) OVERRIDE;
	virtual void ReconstructNode(UEdGraphNode& TargetNode, bool bIsBatchRequest=false) const OVERRIDE;
	virtual bool CanEncapuslateNode(UEdGraphNode const& TestNode) const OVERRIDE;
	virtual void HandleGraphBeingDeleted(UEdGraph& GraphBeingRemoved) const OVERRIDE;
	virtual void GetGraphDisplayInformation(const UEdGraph& Graph, /*out*/ FGraphDisplayInfo& DisplayInfo) const OVERRIDE;
	virtual void DroppedAssetsOnGraph(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraph* Graph) const OVERRIDE;
	virtual void DroppedAssetsOnNode(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraphNode* Node) const OVERRIDE;
	virtual void DroppedAssetsOnPin(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraphPin* Pin) const OVERRIDE;
	virtual void GetAssetsNodeHoverMessage(const TArray<FAssetData>& Assets, const UEdGraphNode* HoverNode, FString& OutTooltipText, bool& OutOkIcon) const OVERRIDE;
	virtual void GetAssetsPinHoverMessage(const TArray<FAssetData>& Assets, const UEdGraphPin* HoverPin, FString& OutTooltipText, bool& OutOkIcon) const OVERRIDE;
	virtual bool CanDuplicateGraph(UEdGraph* InSourceGraph) const OVERRIDE;
	virtual UEdGraph* DuplicateGraph(UEdGraph* GraphToDuplicate) const OVERRIDE;
	virtual UEdGraphNode* CreateSubstituteNode(UEdGraphNode* Node, const UEdGraph* Graph, FObjectInstancingGraph* InstanceGraph) const OVERRIDE;
	virtual int32 GetNodeSelectionCount(const UEdGraph* Graph) const OVERRIDE;
	virtual TSharedPtr<FEdGraphSchemaAction> GetCreateCommentAction() const OVERRIDE;
	virtual bool FadeNodeWhenDraggingOffPin(const UEdGraphNode* Node, const UEdGraphPin* Pin) const OVERRIDE;
	virtual void BackwardCompatibilityNodeConversion(UEdGraph* Graph, bool bOnlySafeChanges) const OVERRIDE;
	// End EdGraphSchema Interface

	// Do validation, that doesn't require a knowledge about actual pin. 
	virtual bool DefaultValueSimpleValidation(const FEdGraphPinType& PinType, const FString& PinName, const FString& NewDefaultValue, UObject* NewDefaultObject, const FText& InText, FString* OutMsg = NULL) const;

	/** Returns true if the owning node is a function with AutoCreateRefTerm meta data */
	bool IsAutoCreateRefTerm(const UEdGraphPin* Pin) const;

	/** See if a class has any members that are accessible by a blueprint */
	bool ClassHasBlueprintAccessibleMembers(const UClass* InClass) const;

	/**
	 * Checks to see if the specified graph is a construction script
	 *
	 * @param	TestEdGraph		Graph to test
	 * @return	true if this is a construction script
	 */
	bool IsConstructionScript(const UEdGraph* TestEdGraph) const;

	/**
	 * Checks to see if the specified graph is a composite graph
	 *
	 * @param	TestEdGraph		Graph to test
	 * @return	true if this is a composite graph
	 */
	bool IsCompositeGraph(const UEdGraph* TestEdGraph) const;

	/**
	 * Checks to see if a pin is an execution pin.
	 *
	 * @param	Pin	The pin to check.
	 * @return	true if it is an execution pin.
	 */
	inline bool IsExecPin(const UEdGraphPin& Pin) const
	{
		return Pin.PinType.PinCategory == PC_Exec;
	}

	/**
	 * Checks to see if a pin is a Self pin (indicating the calling context for the node)
	 *
	 * @param	Pin	The pin to check.
	 * @return	true if it is a Self pin.
	 */
	virtual bool IsSelfPin(const UEdGraphPin& Pin) const OVERRIDE;

	/**
	 * Checks to see if a pin is a meta-pin (either a Self or Exec pin)
	 *
	 * @param	Pin	The pin to check.
	 * @return	true if it is a Self or Exec pin.
	 */
	inline bool IsMetaPin(const UEdGraphPin& Pin) const
	{
		return IsSelfPin(Pin) || IsExecPin(Pin);
	}

	/** Is given string a delegate category name ? */
	virtual bool IsDelegateCategory(const FString& Category) const OVERRIDE;

	/** Returns whether a pin category is compatible with an Index Wildcard (PC_Wildcard and PSC_Index) */
	inline bool IsIndexWildcardCompatible(const FEdGraphPinType& PinType) const
	{
		return (!PinType.bIsArray) && 
			(
				PinType.PinCategory == PC_Boolean || 
				PinType.PinCategory == PC_Int || 
				PinType.PinCategory == PC_Byte ||
				(PinType.PinCategory == PC_Wildcard && PinType.PinSubCategory == PSC_Index)
			);
	}

	/**
	 * Searches for the first execution pin with the specified direction on the node
	 *
	 * @param	Node			The node to search.
	 * @param	PinDirection	The required pin direction.
	 *
	 * @return	the first found execution pin with the correct direction or null if there were no matching pins.
	 */
	UEdGraphPin* FindExecutionPin(const UEdGraphNode& Node, EEdGraphPinDirection PinDirection) const
	{
		for (int32 PinIndex = 0; PinIndex < Node.Pins.Num(); ++PinIndex)
		{
			UEdGraphPin* Pin = Node.Pins[PinIndex];

			if ((Pin->Direction == PinDirection) && IsExecPin(*Pin))
			{
				return Pin;
			}
		}

		return NULL;
	}

	/**
	 * Searches for the first Self pin with the specified direction on the node
	 *
	 * @param	Node			The node to search.
	 * @param	PinDirection	The required pin direction.
	 *
	 * @return	the first found self pin with the correct direction or null if there were no matching pins.
	 */
	UEdGraphPin* FindSelfPin(const UEdGraphNode& Node, EEdGraphPinDirection PinDirection) const
	{
		for (int32 PinIndex = 0; PinIndex < Node.Pins.Num(); ++PinIndex)
		{
			UEdGraphPin* Pin = Node.Pins[PinIndex];

			if ((Pin->Direction == PinDirection) && IsSelfPin(*Pin))
			{
				return Pin;
			}
		}

		return NULL;
	}

	/** Can Pin be promoted to a variable? */
	bool CanPromotePinToVariable (const UEdGraphPin& Pin) const;

	/**
	 * Convert the type of a UProperty to the corresponding pin type.
	 *
	 * @param		Property	The property to convert.
	 * @param [out]	TypeOut		The resulting pin type.
	 *
	 * @return	true on success, false if the property is unsupported or invalid.
	 */
	bool ConvertPropertyToPinType(const UProperty* Property, /*out*/ FEdGraphPinType& TypeOut) const;

	/** Flags to indicate different types of blueprint callable functions */
	enum EFunctionType
	{
		FT_Imperative	= 0x01,
		FT_Pure			= 0x02,
		FT_Const		= 0x04,
		FT_Protected	= 0x08,
	};
	
	/**
	 * Finds the parent function for the specified function, if any
	 *
	 * @param	Function			The function to find a parent function for
	 * @return	The UFunction parentfunction, if any.
	 */
	UFunction* GetCallableParentFunction(UFunction* Function) const;

	/** Whether or not the specified actor is a valid target for bound events and literal references (in the right level, not a builder brush, etc */
	bool IsActorValidForLevelScriptRefs(const AActor* TestActor, const ULevelScriptBlueprint* Blueprint) const;

	/** 
	 *	Generate a list of replaceable nodes for context menu based on the editor's current selection 
	 *
	 *	@param	Reference to graph node
	 *	@param	Reference to context menu builder
	 */
	void AddSelectedReplaceableNodes( UBlueprint* Blueprint, const UEdGraphNode* InGraphNode, FMenuBuilder* MenuBuilder ) const;

	/**
	 *	Function to replace current graph node reference object with a new object
	 *
	 *	@param Reference to graph node
	 *	@param Reference to new reference object
	 */
	void ReplaceSelectedNode(UEdGraphNode* SourceNode, AActor* TargetActor);

	/**
	 * Looks at all member functions of a specified class and creates 'as delegate' getters for ones matching a given signature.
	 *
	 * @param	Class  				The calling context to scan.
	 * @param	SignatureToMatch	The signature function/delegate to match.
	 * @param [in,out]	OutTypes	Array to append 'as delegate' getters to.
	 */
	void ListFunctionsMatchingSignatureAsDelegates(FGraphContextMenuBuilder& ContextMenuBuilder, const UClass* Class, const UFunction* SignatureToMatch) const;

	/** Returns whether a function is marked 'override' and doesn't have any out parameters */
	static bool FunctionCanBePlacedAsEvent(const UFunction* InFunction);

	/** Can this function be called by kismet delegate */
	static bool FunctionCanBeUsedInDelegate(const UFunction* InFunction);

	/** Can this function be called by kismet code */
	static bool CanUserKismetCallFunction(const UFunction* Function);

	enum EDelegateFilterMode
	{
		CannotBeDelegate,
		MustBeDelegate,
		VariablesAndDelegates
	};

	/** Can this variable be accessed by kismet code */
	static bool CanUserKismetAccessVariable(const UProperty* Property, const UClass* InClass, EDelegateFilterMode FilterMode);

	/** Can this function be overridden by kismet (either placed as event or new function graph created) */
	static bool CanKismetOverrideFunction(const UFunction* Function);

	/** returns friendly signiture name if possible or Removes any mangling to get the unmangled signature name of the function */
	static FString GetFriendlySignitureName(const UFunction* Function);

	static bool IsAllowableBlueprintVariableType(const class UEnum* InEnum);
	static bool IsAllowableBlueprintVariableType(const class UClass* InClass);
	static bool IsAllowableBlueprintVariableType(const class UScriptStruct *InStruct);

	static bool IsPropertyExposedOnSpawn(const UProperty* Property);

	/**
	 * Returns a list of parameters for the function that are specified as automatically emitting terms for unconnected ref parameters in the compiler (MD_AutoCreateRefTerm)
	 *
	 * @param	Function				The function to check for auto-emitted ref terms on
	 * @param	AutoEmitParameterNames	(out) Returns an array of param names that should be auto-emitted if nothing is connected
	 */
	void GetAutoEmitTermParameters(const UFunction* Function, TArray<FString>& AutoEmitParameterNames) const;

	/**
	 * Determine if a function has a parameter of a specific type.
	 *
	 * @param	InFunction	  	The function to search.
	 * @param	CallingContext  The blueprint that you're looking to call the function from (some functions hide different pins depending on the blueprint they're in)
	 * @param	DesiredPinType	The type that at least one function parameter needs to be.
	 * @param	bWantOutput   	The direction that the parameter needs to be.
	 *
	 * @return	true if at least one parameter is of the correct type and direction.
	 */
	bool FunctionHasParamOfType(const UFunction* InFunction, UBlueprint const* CallingContext, const FEdGraphPinType& DesiredPinType, bool bWantOutput) const;

	/**
	 * Add the specified flags to the function entry node of the graph, to make sure they get compiled in to the generated function
	 *
	 * @param	CurrentGraph	The graph of the function to modify the flags for
	 * @param	ExtraFlags		The flags to add to the function entry node
	 */
	void AddExtraFunctionFlags(const UEdGraph* CurrentGraph, int32 ExtraFlags) const;

	/**
	 * Marks the function entry of a graph as editable via function editor or not-editable
	 *
	 * @param	CurrentGraph	The graph of the function to modify the entry node for
	 * @param	bNewEditable	Whether or not the function entry for the graph should be editable via the function editor
	 */
	void MarkFunctionEntryAsEditable(const UEdGraph* CurrentGraph, bool bNewEditable) const;

	/** 
	 * Populate new macro graph with entry and possibly return node
	 * 
	 * @param	Graph			Graph to add the function terminators to
	 * @param	ContextClass	If specified, the graph terminators will use this class to search for context for signatures (i.e. interface functions)
	 */
	virtual void CreateMacroGraphTerminators(UEdGraph& Graph, UClass* Class) const;

	/** 
	 * Populate new function graph with entry and possibly return node
	 * 
	 * @param	Graph			Graph to add the function terminators to
	 * @param	ContextClass	If specified, the graph terminators will use this class to search for context for signatures (i.e. interface functions)
	 */
	virtual void CreateFunctionGraphTerminators(UEdGraph& Graph, UClass* Class) const;

	/** 
	 * Populate new function graph with entry and possibly return node
	 * 
	 * @param	Graph			Graph to add the function terminators to
	 * @param	FunctionSignature	The function signature to mimic when creating the inputs and outputs for the function.
	 */
	virtual void CreateFunctionGraphTerminators(UEdGraph& Graph, UFunction* FunctionSignature) const;

	/**
	 * Converts a pin type into a fully qualified string (e.g., object'ObjectName').
	 *
	 * @param	Type	The type to convert into a string.
	 *
	 * @return	The converted type string.
	 */
	static FString TypeToString(const FEdGraphPinType& Type);

	/**
	 * Converts the type of a property into a fully qualified string (e.g., object'ObjectName').
	 *
	 * @param	Property	The property to convert into a string.
	 *
	 * @return	The converted type string.
	 */
	static FString TypeToString(UProperty* const Property);

	/**
	 * Converts a pin type into a fully qualified FText (e.g., object'ObjectName').
	 *
	 * @param	Type	The type to convert into a FText.
	 *
	 * @return	The converted type text.
	 */
	static FText TypeToText(const FEdGraphPinType& Type);

	/**
	 * Get the type tree for all of the property types valid for this schema
	 *
	 * @param	TypeTree		The array that will contain the type tree hierarchy for this schema upon returning
	 * @param	bAllowExec		Whether or not to add the exec type to the type tree
	 * @param	bAllowWildcard	Whether or not to add the wildcard type to the type tree
	 */
	void GetVariableTypeTree( TArray< TSharedPtr<FPinTypeTreeInfo> >& TypeTree, bool bAllowExec, bool bAllowWildcard ) const;

	/**
	 * Get the type tree for the index property types valid for this schema
	 *
	 * @param	TypeTree	The array that will contain the type tree hierarchy for this schema upon returning
	 * @param	bAllowExec	Whether or not to add the exec type to the type tree
	 * @param	bAllowExec	Whether or not to add the exec type to the type tree
	 */
	void GetVariableIndexTypeTree( TArray< TSharedPtr<FPinTypeTreeInfo> >& TypeTree, bool bAllowExec, bool bAllowWildcard ) const;

	/**
	 * Returns whether or not the specified type has valid subtypes available
	 *
	 * @param	Type	The type to check for subtypes
	 */
	bool DoesTypeHaveSubtypes( const FString& FriendlyTypeName ) const;

	/**
	 * Gets a list of variable subtypes that are valid for the specified type
	 *
	 * @param	Type			The type to grab subtypes for
	 * @param	SubtypesList	(out) Upon return, this will be a list of valid subtype objects for the specified type
	 */
	void GetVariableSubtypes(const FString& Type, TArray<UObject*>& SubtypesList) const;

	/**
	 * Returns true if the types and directions of two pins are schema compatible. Handles
	 * outputting a more derived type to an input pin expecting a less derived type.
	 *
	 * @param	PinA		  	The pin a.
	 * @param	PinB		  	The pin b.
	 * @param	CallingContext	(optional) The calling context (required to properly evaluate pins of type Self)
	 * @param	bIgnoreArray	(optional) Whether or not to ignore differences between array and non-array types
	 *
	 * @return	true if the pin types and directions are compatible.
	 */
	virtual bool ArePinsCompatible(const UEdGraphPin* PinA, const UEdGraphPin* PinB, const UClass* CallingContext = NULL, bool bIgnoreArray = false) const;

	/**
	 * Returns the connection response for connecting PinA to PinB, which have already been determined to be compatible
	 * types with a compatible direction.  InputPin and OutputPin are PinA and PinB or vis versa, indicating their direction.
	 *
	 * @param	PinA		  	The pin a.
	 * @param	PinB		  	The pin b.
	 * @param	InputPin	  	Either PinA or PinB, depending on which one is the input.
	 * @param	OutputPin	  	Either PinA or PinB, depending on which one is the output.
	 *
	 * @return	The message and action to take on trying to make this connection.
	 */
	virtual const FPinConnectionResponse DetermineConnectionResponseOfCompatibleTypedPins(const UEdGraphPin* PinA, const UEdGraphPin* PinB, const UEdGraphPin* InputPin, const UEdGraphPin* OutputPin) const;

	/**
	 * Returns true if the two pin types are schema compatible.  Handles outputting a more derived
	 * type to an input pin expecting a less derived type.
	 *
	 * @param	Output		  	The output type.
	 * @param	Input		  	The input type.
	 * @param	CallingContext	(optional) The calling context (required to properly evaluate pins of type Self)
	 * @param	bIgnoreArray	(optional) Whether or not to ignore differences between array and non-array types
	 *
	 * @return	true if the pin types are compatible.
	 */
	virtual bool ArePinTypesCompatible(const FEdGraphPinType& Output, const FEdGraphPinType& Input, const UClass* CallingContext = NULL, bool bIgnoreArray = false) const;

	/**
	 * Sets the default value of a pin based on the type of the pin (0 for int, false for boolean, etc...)
	 */
	virtual void SetPinDefaultValueBasedOnType(UEdGraphPin* Pin) const;

	/** Utility that makes sure existing connections are valid, breaking any that are now illegal. */
	static void ValidateExistingConnections(UEdGraphPin* Pin);

	/** Find a 'set value by name' function for the specified pin, if it exists */
	static UFunction* FindSetVariableByNameFunction(const FEdGraphPinType& PinType);

	/** Find an appropriate function to call to perform an automatic cast operation */
	virtual bool SearchForAutocastFunction(const UEdGraphPin* OutputPin, const UEdGraphPin* InputPin, /*out*/ FName& TargetFunction) const;

	/** Find an appropriate node that can convert from one pin type to another (not a cast; e.g. "MakeLiteralArray" node) */
	virtual bool FindSpecializedConversionNode(const UEdGraphPin* OutputPin, const UEdGraphPin* InputPin, bool bCreateNode, /*out*/ class UK2Node*& TargetNode) const;

	/** Get menu for breaking links to specific nodes*/
	void GetBreakLinkToSubMenuActions(class FMenuBuilder& MenuBuilder, class UEdGraphPin* InGraphPin);

	/** Get menu for jumping to specific pin links */
	void GetJumpToConnectionSubMenuActions(class FMenuBuilder& MenuBuilder, class UEdGraphPin* InGraphPin);

	/** Create menu for variable get/set nodes which refer to a variable which does not exist. */
	void GetNonExistentVariableMenu(const UEdGraphNode* InGraphNode, UBlueprint* OwnerBlueprint, FMenuBuilder* MenuBuilder) const;

	// Calculates an average position between the nodes owning the two specified pins
	static FVector2D CalculateAveragePositionBetweenNodes(UEdGraphPin* InputPin, UEdGraphPin* OutputPin);

	// Tries to connect any pins with matching types and directions from the conversion node to the specified input and output pins
	void AutowireConversionNode(UEdGraphPin* InputPin, UEdGraphPin* OutputPin, UEdGraphNode* ConversionNode) const;

	/** Calculates an estimated height for the specified node */
	static float EstimateNodeHeight( UEdGraphNode* Node );

	/**
	 * Checks if the graph supports impure functions
	 *
	 * @param InGraph		Graph to check
	 *
	 * @return				True if the graph supports impure functions
	 */
	bool DoesGraphSupportImpureFunctions(const UEdGraph* InGraph) const;

	/**
	 * Checks to see if the passed in function is valid in the class
	 *
	 * @param	InClass  			Class being checked to see if the function is valid for
	 * @param	InFunction			Function being checked
	 * @param	InDestGraph			Graph we will be using action for (may be NULL)
	 * @param	InFunctionTypes		Combination of EFunctionType to indicate types of functions accepted
	 * @param	bInShowInherited	Allows for inherited functions
	 * @param	bInCalledForEach	Call for each element in an array (a node accepts array)
	 * @param	InTargetInfo		Allows spawning nodes which also create a target variable as well
	 */
	bool CanFunctionBeUsedInClass(const UClass* InClass, UFunction* InFunction, const UEdGraph* InDestGraph, uint32 InFunctionTypes, bool bInShowInherited, bool bInCalledForEach, const FFunctionTargetInfo& InTargetInfo) const;

	/**
	 * Makes connections into/or out of the gateway node, connect directly to the associated networks on the opposite side of the tunnel
	 * When done, none of the pins on the gateway node will be connected to anything.
	 * Requires both this gateway node and it's associated node to be in the same graph already (post-merging)
	 *
	 * @param InGatewayNode			The function or tunnel node
	 * @param InEntryNode			The entry node in the inner graph
	 * @param InResultNode			The result node in the inner graph
	 *
	 * @return						Returns TRUE if successful
	 */
	bool CollapseGatewayNode(UK2Node* InNode, UEdGraphNode* InEntryNode, UEdGraphNode* InResultNode) const;

	/**
	 * Connects all of the linked pins from PinA to all of the linked pins from PinB, removing
	 * both PinA and PinB from being linked to anything else
	 * Requires the nodes that own the pins to be in the same graph already (post-merging)
	 */
	void CombineTwoPinNetsAndRemoveOldPins(UEdGraphPin* InPinA, UEdGraphPin* InPinB) const;

	/** Function that returns _all_ nodes we could place */
	static void GetAllActions(struct FBlueprintPaletteListBuilder& PaletteBuilder);

	/** Helper method to add items valid to the palette list */
	static void GetPaletteActions(struct FBlueprintPaletteListBuilder& ActionMenuBuilder, TWeakObjectPtr<UClass> FilterClass = NULL);

	/** some inherited schemas don't want anim-notify actions listed, so this is an easy way to check that */
	virtual bool DoesSupportAnimNotifyActions() const { return true; }

	///////////////////////////////////////////////////////////////////////////////////
	// NonExistent Variables: Broken Get/Set Nodes where the variable is does not exist 

	/** Create the variable that the broken node refers to */
	static void OnCreateNonExistentVariable(class UK2Node_Variable* Variable, UBlueprint* OwnerBlueprint);

	/** Replace the variable that a variable node refers to when the variable it refers to does not exist */
	static void OnReplaceVariableForVariableNode(class UK2Node_Variable* Variable, UBlueprint* OwnerBlueprint, FString VariableName);

	/** Create sub menu that shows all possible variables that can be used to replace the existing variable reference */
	static void GetReplaceNonExistentVariableMenu(class FMenuBuilder& MenuBuilder, class UK2Node_Variable* Variable, UBlueprint* OwnerBlueprint);

private:

	/**
	 * Returns true if the specified function has any out parameters
	 * @param [in] Function	The function to check for out parameters
	 * @return true if there are out parameters, else false
	 */
	bool DoesFunctionHaveOutParameters( const UFunction* Function ) const;
};

