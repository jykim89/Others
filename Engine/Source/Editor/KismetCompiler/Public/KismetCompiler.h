// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraphCompilerUtilities.h"
#include "BPTerminal.h"
#include "KismetCompiledFunctionContext.h"
#include "KismetCompilerMisc.h"


KISMETCOMPILER_API DECLARE_LOG_CATEGORY_EXTERN(LogK2Compiler, Log, All);

//////////////////////////////////////////////////////////////////////////
// FKismetCompilerContext

class KISMETCOMPILER_API FKismetCompilerContext : public FGraphCompilerContext
{
protected:
	typedef FGraphCompilerContext Super;

	// Schema for the graph being compiled 
	UEdGraphSchema_K2* Schema;

	// Map from node class to a handler functor
	TMap< TSubclassOf<class UEdGraphNode>, FNodeHandlingFunctor*> NodeHandlers;

	// Map of properties created for timelines; to aid in debug data generation
	TMap<class UTimelineTemplate*, class UProperty*> TimelineToMemberVariableMap;

	// Map from UProperties to default object values, to be fixed up after compilation is complete
	TMap<FName, FString> DefaultPropertyValueMap;

	// Names of functions created
	TSet<FString> CreatedFunctionNames;

	// List of functions currently allocated
	TIndirectArray<FKismetFunctionContext> FunctionList;
protected:
	// This struct holds the various compilation options, such as which passes to perform, whether to save intermediate results, etc
	FKismetCompilerOptions CompileOptions;

	// Maximum height encountered in this row; used to position the next row appropriately
	int32 MacroRowMaxHeight;

	// Maximum bounds of the spawning area
	int32 MinimumSpawnX;
	int32 MaximumSpawnX;

	// Average node size for nodes with no size
	int32 AverageNodeWidth;
	int32 AverageNodeHeight;

	// Padding
	int32 HorizontalSectionPadding;
	int32 VerticalSectionPadding;
	int32 HorizontalNodePadding;
	
	// Used to space expanded macro nodes when saving intermediate results
	int32 MacroSpawnX;
	int32 MacroSpawnY;

	UScriptStruct* VectorStruct;
	UScriptStruct* RotatorStruct;
	UScriptStruct* TransformStruct;
	UScriptStruct* LinearColorStruct;

	// If set, this is a list of all the objects that are currently loading
	TArray<UObject*>* ObjLoaded;

public:
	UBlueprint* Blueprint;
	UBlueprintGeneratedClass* NewClass;

	// The ubergraph; valid from roughly the start of CreateAndProcessEventGraph
	UEdGraph* ConsolidatedEventGraph;

	// The ubergraph context; valid from the end of CreateAndProcessEventGraph
	FKismetFunctionContext* UbergraphContext;

	TMap<UEdGraphNode*, UEdGraphNode*> CallsIntoUbergraph;
	int32 bIsFullCompile:1;

	// Map from a name to the number of times it's been 'created' (identical nodes create the same variable names, so they need something appended)
	FNetNameMapping ClassScopeNetNameMap;

	// Special maps used for autocreated macros to preserve information about their source
	FBacktrackMap FinalNodeBackToMacroSourceMap;
	TMultiMap<TWeakObjectPtr<UEdGraphNode>, TWeakObjectPtr<UEdGraphNode>> MacroSourceToMacroInstanceNodeMap;
public:
	FKismetCompilerContext(UBlueprint* SourceSketch, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompilerOptions, TArray<UObject*>* InObjLoaded);
	virtual ~FKismetCompilerContext();

	/** Compile a blueprint into a class and a set of functions */
	virtual void Compile();

	const UEdGraphSchema_K2* GetSchema() const { return Schema; }

	// Spawns an intermediate node associated with the source node (for error purposes)
	template <typename NodeType>
	NodeType* SpawnIntermediateNode(UEdGraphNode* SourceNode, UEdGraph* ParentGraph = NULL)
	{
		if (ParentGraph == NULL)
		{
			ParentGraph = SourceNode->GetGraph();
		}

		NodeType* Result = ParentGraph->CreateBlankNode<NodeType>();
		MessageLog.NotifyIntermediateObjectCreation(Result, SourceNode);
		Result->CreateNewGuid();

		AutoAssignNodePosition(Result);

		return Result;
	}

	/**
	 * Moves pin links over from the source-pin to the specified intermediate, 
	 * and validates the result (additionally logs a redirect from the 
	 * intermediate-pin back to the source so we can back trace for debugging, etc.)
	 * 
	 * @param  SourcePin		The pin you want disconnected.
	 * @param  IntermediatePin	The pin you want the SourcePin's links moved to.
	 * @return The result from calling the schema's MovePinLinks().
	 */
	FPinConnectionResponse MovePinLinksToIntermediate(UEdGraphPin& SourcePin, UEdGraphPin& IntermediatePin);

	/**
	 * Copies pin links over from the source-pin to the specified intermediate, 
	 * and validates the result (additionally logs a redirect from the 
	 * intermediate-pin back to the source so we can back trace for debugging, etc.)
	 * 
	 * @param  SourcePin		The pin whose links you want copied.
	 * @param  IntermediatePin	The pin you want the SourcePin's links copied to.
	 * @return The result from calling the schema's CopyPinLinks().
	 */
	FPinConnectionResponse CopyPinLinksToIntermediate(UEdGraphPin& SourcePin, UEdGraphPin& IntermediatePin);

	UK2Node_TemporaryVariable* SpawnInternalVariable(UEdGraphNode* SourceNode, FString Category, FString SubCategory = TEXT(""), UObject* SubcategoryObject = NULL, bool bIsArray = false);

protected:
	virtual UEdGraphSchema_K2* CreateSchema();
	virtual void PostCreateSchema();
	virtual void SpawnNewClass(const FString& NewClassName);

	/**
	 * Backwards Compatability:  Ensures that the passed in TargetClass is of the proper type (e.g. BlueprintGeneratedClass, AnimBlueprintGeneratedClass), and NULLs the reference if it is not 
	 */
	virtual void EnsureProperGeneratedClass(UClass*& TargetClass);

	/**
	 * Removes the properties and functions from a class, so that new ones can be created in its place
	 * 
	 * @param ClassToClean		The UClass to scrub
	 * @param OldCDO			Reference to the old CDO of the class, so we can copy the properties from it to the new class's CDO
	 */
	virtual void CleanAndSanitizeClass(UBlueprintGeneratedClass* ClassToClean, UObject*& OldCDO);

	/** 
	 * Checks a connection response, and errors if it didn't succeed (not public, 
	 * users should be using MovePinLinksToIntermediate/CopyPinLinksToIntermediate 
	 * instead of wrapping their own with this).
	 */
	void CheckConnectionResponse(const FPinConnectionResponse &Response, const UEdGraphNode *Node);

protected:
	// FGraphCompilerContext interface
	virtual void ValidateLink(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const OVERRIDE;
	virtual void ValidatePin(const UEdGraphPin* Pin) const OVERRIDE;
	virtual void ValidateNode(const UEdGraphNode* Node) const OVERRIDE;
	virtual bool CanIgnoreNode(const UEdGraphNode* Node) const OVERRIDE;
	virtual bool ShouldForceKeepNode(const UEdGraphNode* Node) const OVERRIDE;
	virtual void PruneIsolatedNodes(const TArray<UEdGraphNode*>& RootSet, TArray<UEdGraphNode*>& GraphNodes) OVERRIDE;
	virtual bool PinIsImportantForDependancies(const UEdGraphPin* Pin) const OVERRIDE
	{
		// The execution wires do not form data dependencies, they are only important for final scheduling and that is handled thru gotos
		return Pin->PinType.PinCategory != Schema->PC_Exec;
	}

protected:
	// Expands out nodes that need it
	void ExpansionStep(UEdGraph* Graph, bool bAllowUbergraphExpansions);

	// Advances the macro position tracking
	void AdvanceMacroPlacement(int32 Width, int32 Height);
	void AutoAssignNodePosition(UEdGraphNode* Node);
	void CreateCommentBlockAroundNodes(const TArray<UEdGraphNode*>& Nodes, UObject* SourceObject, UEdGraph* TargetGraph, FString CommentText, FLinearColor CommentColor, int32& Out_OffsetX, int32& Out_OffsetY);

	/** Creates a class variable */
	virtual UProperty* CreateVariable(const FName Name, const FEdGraphPinType& Type);

	// Gives derived classes a chance to emit debug data
	virtual void PostCompileDiagnostics() {}

	/** Determines if a node is pure */
	virtual bool IsNodePure(const UEdGraphNode* Node) const;

	/** Creates a class variable for each entry in the Blueprint NewVars array */
	virtual void CreateClassVariablesFromBlueprint();

	/** Creates a property with flags including PropertyFlags in the Scope structure for each entry in the Terms array */
	void CreatePropertiesFromList(UStruct* Scope, UField**& PropertyStorageLocation, TIndirectArray<FBPTerminal>& Terms, uint64 PropertyFlags, bool bPropertiesAreLocal, bool bPropertiesAreParameters = false);

	/** Creates the properties on a function that store the function parameters, results, and local variables */
	void CreateLocalVariablesForFunction(FKismetFunctionContext& Context);

	/** Creates user defined local variables for function */
	void CreateUserDefinedLocalVariablesForFunction(FKismetFunctionContext& Context, UField**& PropertyStorageLocation);

	/** Adds a default value entry into the DefaultPropertyValueMap for the property specified */
	void SetPropertyDefaultValue(const UProperty* PropertyToSet, FString& Value);

	/** Copies default values cached for the terms in the DefaultPropertyValueMap to the final CDO */
	virtual void CopyTermDefaultsToDefaultObject(UObject* DefaultObject);

	/** 
	 * Function works only is first native superclass is AActor.
	 * If ReceiveTick event is defined, force CanEverTick.
	 * If ReceiveAnyDamage or ReceiveRadialDamage or ReceivePointDamage event is defined, force CanBeDamaged.
	 */
	void SetCanEverTickForActor();

	/** Scan FunctionList and return Entry point, for matching one  */
	const UK2Node_FunctionEntry* FindLocalEntryPoint(const UFunction* Function) const;

	//@TODO: Debug printing
	void PrintVerboseInfoStruct(UStruct* Struct) const;
	void PrintVerboseInformation(UClass* Class) const;
	//@ENDTODO

	/**
	 * Performs transformations on specific nodes that require it according to the schema
	 */
	virtual void TransformNodes(FKismetFunctionContext& Context);

	/**
	 * Merges in any all ubergraph pages into the gathering ubergraph
	 */
	virtual void MergeUbergraphPagesIn(UEdGraph* Ubergraph);

	/**
	 * Creates a list of functions to compile
	 */
	virtual void CreateFunctionList();

	/** Creates a new function context and adds it to the function list to be processed. */
	FKismetFunctionContext* CreateFunctionContext();

	/**
	 * Merges macros/subgraphs into the graph and validates it, creating a function list entry if it's reasonable.
	 */
	virtual void ProcessOneFunctionGraph(UEdGraph* SourceGraph);

	/**
	 * Picks the name to use for an autogenerated event stub
	 */ 
	virtual FName GetEventStubFunctionName(UK2Node_Event* SrcEventNode);

	/**
	 * Gets the unique name for this context's ExecuteUbergraph function
	 */
	FName GetUbergraphCallName() const
	{
		check(Schema);
		const FString UbergraphCallString = Schema->FN_ExecuteUbergraphBase.ToString() + TEXT("_") + Blueprint->GetName();
		return FName(*UbergraphCallString);
	}

	/**
	 * Expands any macro instances and collapses any tunnels in the nodes of SourceGraph
	 */
	void ExpandTunnelsAndMacros(UEdGraph* SourceGraph);

	/**
	 * Merges pages and creates function stubs, etc...
	 */
	void CreateAndProcessUbergraph();

	/** Create a stub function graph for the event node, and have it invoke the correct point in the ubergraph */
	void CreateFunctionStubForEvent(UK2Node_Event* Event, UObject* OwnerOfTemporaries);

	/** Expand timeline nodes into necessary nodes */
	void ExpandTimelineNodes(UEdGraph* SourceGraph);

	/** Expand any PlayMovieScene nodes */
	void ExpandPlayMovieSceneNodes(UEdGraph* SourceGraph);

	/** Used internally by ExpandPlayMovieSceneNodes() to generate a node network to allocate a URuntimeMovieScenePlayer object instance on demand */
	UEdGraphPin* ExpandNodesToAllocateRuntimeMovieScenePlayer( UEdGraph* SourceGraph, class UK2Node_PlayMovieScene* PlayMovieSceneNode, class ULevel* Level, class UK2Node_TemporaryVariable*& OutPlayerVariableNode );

	/**
	 * First phase of compiling a function graph
	 *   - Performs initial validation that the graph is at least well formed enough to be processed further
	 *   - Creates a copy of the graph to allow further transformations to occur
	 *   - Prunes the 'graph' to only included the connected portion that contains the function entry point 
	 *   - Schedules execution of each node based on data/execution dependencies
	 *   - Creates a UFunction object containing parameters and local variables (but no script code yet)
	 */
	virtual void PrecompileFunction(FKismetFunctionContext& Context);

	/**
	 * Second phase of compiling a function graph
	 *   - Generates an executable statement list
	 */
	virtual void CompileFunction(FKismetFunctionContext& Context);

	/**
	 * Final phase of compiling a function graph; called after all functions have had CompileFunction called
	 *   - Patches up cross-references, etc..., and performs final validation
	 */
	virtual void PostcompileFunction(FKismetFunctionContext& Context);

	/**
	 * Handles final post-compilation setup, flags, creates cached values that would normally be set during deserialization, etc...
	 */
	void FinishCompilingFunction(FKismetFunctionContext& Context);

	/**
	 * Handles adding the implemented interface information to the class
	 */
	virtual void AddInterfacesFromBlueprint(UClass* Class);

	/**
	 * Handles final post-compilation setup, flags, creates cached values that would normally be set during deserialization, etc...
	 */
	virtual void FinishCompilingClass(UClass* Class);

	/** Build the dynamic bindings objects used to tie events to delegates at runtime */
	void BuildDynamicBindingObjects(UBlueprintGeneratedClass* Class);

	/**
	 *  If a function in the graph cannot be placed as event make sure that it is not.
	 */
	void VerifyValidOverrideEvent(const UEdGraph* Graph);

	/**
	 *  If a function in the graph cannot be overridden make sure that it is not.
	 */
	void VerifyValidOverrideFunction(const UEdGraph* Graph);

	/**
	 * Checks if self pins are connected.
	 */
	void ValidateSelfPinsInGraph(const UEdGraph* SourceGraph);

	/** Ensures that all variables have valid names for compilation/replication */
	void ValidateVariableNames();

	/** Ensures that all timelines have valid names for compilation/replication */
	void ValidateTimelineNames();

	/** Ensures that all function graphs have valid names for compilation/replication */
	void ValidateFunctionGraphNames();

	/** Validates the generated class */
	virtual bool ValidateGeneratedClass(UBlueprintGeneratedClass* Class);

private:
	/**
	 * Handles creating a new event node for a given output on a timeline node utilizing the named function
	 */
	void CreatePinEventNodeForTimelineFunction(UK2Node_Timeline* TimelineNode, UEdGraph* SourceGraph, FName FunctionName, const FString& PinName, FName ExecFuncName);

	/** Util for creating a node to call a function on a timeline and move connections to it */
	class UK2Node_CallFunction* CreateCallTimelineFunction(UK2Node_Timeline* TimelineNode, UEdGraph* SourceGraph, FName FunctionName, UEdGraphPin* TimelineVarPin, UEdGraphPin* TimelineFunctionPin);

	/**
	 * Function to reset graph node's error flag before compiling
	 *
	 * @param: Reference to graph instance
	 */
	void ResetErrorFlags(UEdGraph* Graph) const;
	
};

//////////////////////////////////////////////////////////////////////////
