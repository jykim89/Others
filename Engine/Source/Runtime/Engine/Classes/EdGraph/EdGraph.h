// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EdGraph.generated.h"

USTRUCT()
struct ENGINE_API FGraphReference
{
	GENERATED_USTRUCT_BODY()
protected:
	// Reference to the actual graph
	UPROPERTY()
	mutable class UEdGraph* MacroGraph;

	// The blueprint the graph is contained within
	UPROPERTY()
	class UBlueprint* GraphBlueprint;

	// The graph GUID so we can refind it if it has been renamed
	UPROPERTY()
	FGuid GraphGuid;

public:
	FGraphReference()
		: MacroGraph(NULL)
		, GraphBlueprint(NULL)
	{
	}

	void PostSerialize(const FArchive& Ar);

	class UBlueprint* GetBlueprint() const
	{
		return GraphBlueprint;
	}

#if WITH_EDITORONLY_DATA
	void SetGraph(UEdGraph* InGraph);
	UEdGraph* GetGraph() const;
#endif
};

template<>
struct TStructOpsTypeTraits<FGraphReference> : public TStructOpsTypeTraitsBase
{
	enum 
	{
		WithPostSerialize = true,
	};
};

UCLASS(dependson=(UEdGraphNode))
class ENGINE_API UEdGraph : public UObject
{
	GENERATED_UCLASS_BODY()

	/** The schema that this graph obeys */
	UPROPERTY()
	TSubclassOf<class UEdGraphSchema>  Schema;

	/** Set of all nodes in this graph */
	UPROPERTY()
	TArray<class UEdGraphNode*> Nodes;

	/** If true, graph can be edited by the user */
	UPROPERTY()
	uint32 bEditable:1;

	/** If true, graph can be deleted from the whatever container it is in */
	UPROPERTY()
	uint32 bAllowDeletion:1;

	/** If true, graph can be renamed; Note: Graph can also be renamed if bAllowDeletion is true currently */
	UPROPERTY()
	uint32 bAllowRenaming:1;

#if WITH_EDITORONLY_DATA
	/** Child graphs that are a part of this graph; the separation is purely visual */
	UPROPERTY()
	TArray<class UEdGraph*> SubGraphs;

	/** Guid for this graph */
	UPROPERTY()
	FGuid GraphGuid;

	/** Guid of interface graph this graph comes from (used for conforming) */
	UPROPERTY()
	FGuid InterfaceGuid;
#endif // WITH_EDITORONLY_DATA

public:
	template <typename NodeType> friend struct FGraphNodeCreator;

	/** Get the schema associated with this graph */
	const UEdGraphSchema* GetSchema() const;

	/** Add a listener for OnGraphChanged events */
	void AddOnGraphChangedHandler( const FOnGraphChanged::FDelegate& InHandler );

	/** Remove a listener for OnGraphChanged events */
	void RemoveOnGraphChangedHandler( const FOnGraphChanged::FDelegate& InHandler );

#if WITH_EDITORONLY_DATA
	// Begin UObject interface
	virtual void PostInitProperties() OVERRIDE;
	// End UObject interface
#endif

public:
	/** 
	 * Creates an empty node of the given type. 
	 * User is fully responsible for building the node. Most nodes should be created through
	 * a FGraphNodeCreator builder.
	 */
	template <typename NodeClass>
	NodeClass* CreateBlankNode()
	{
		return (NodeClass*)CreateNode(NodeClass::StaticClass());
	}

	/** 
	 * Add a node to the graph 
	 * @param NodeToAdd	A graph node to add to the graph
	 * @param bUserAction	true if the node was added as the result of a direct user action
	 * @param bSelectNewNode	Whether or not to select the new node being created
	 */
	void AddNode( UEdGraphNode* NodeToAdd, bool bUserAction = false, bool bSelectNewNode = true );

	/** Remove a node from this graph */
	bool RemoveNode( UEdGraphNode* NodeToRemove );

	/** Signal to listeners that the graph has changed */
	void NotifyGraphChanged();

	/** 
	 * Move all nodes from this graph to another graph
	 * @param DestinationGraph	The graph to move the nodes too 
	 * @param bIsLoading		If true, the node move is occurring during a blueprint load
	 */
	void MoveNodesToAnotherGraph(UEdGraph* DestinationGraph, bool bIsLoading);

	/** Finds all the nodes of a given minimum type in the graph */
	template<class MinRequiredType, class ArrayElementType>
	inline void GetNodesOfClassEx(TArray<ArrayElementType*>& OutNodes) const
	{
		for (int32 i = 0; i < Nodes.Num(); i++)
		{
			UEdGraphNode* Node = Nodes[i];
			if (MinRequiredType* TypedNode = Cast<MinRequiredType>(Node))
			{
				OutNodes.Add(TypedNode);
			}
		}
	}

	/** Gets all the nodes in the graph of a given type */
	template<class MinRequiredType>
	inline void GetNodesOfClass(TArray<MinRequiredType*>& OutNodes) const
	{
		GetNodesOfClassEx<MinRequiredType, MinRequiredType>(OutNodes);
	}

	/** Get all children graphs in the specified graph */
	void GetAllChildrenGraphs(TArray<UEdGraph*>& Graphs) const;

	/** Util to find a good place for a new node */
	FVector2D GetGoodPlaceForNewNode();

#if WITH_EDITOR
	/** Notify the graph and its associated listeners that a property is about to change  */
	void NotifyPreChange( const FString & PropertyName );

	/** Notify the graph and associated listeners that a property has changed */
	void NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, const FString & PropertyName );

	/** Add a delegate listening for property change notifications */
	void AddPropertyChangedNotifier(const FOnPropertyChanged::FDelegate& InDelegate );

	/** Remove a delegate listening for property changed notifications */
	void RemovePropertyChangedNotifier(const FOnPropertyChanged::FDelegate& InDelegate );
#endif

protected:
	void NotifyGraphChanged( const FEdGraphEditAction& Action );

	/** 
	 * Creates an empty node in this graph. Use FGraphNodeCreator above
	 *
	 * @param NewNodeClass		The node class to create
	 * @param bSelectNewNode	Whether or not to select the new node being created
	 *
	 * @return A new graph node of the given type
	 */
	UEdGraphNode* CreateNode ( TSubclassOf<UEdGraphNode> NewNodeClass, bool bSelectNewNode = true );

private:
	/** A delegate that broadcasts a notification whenever the graph has changed. */
	FOnGraphChanged OnGraphChanged;

#if WITH_EDITORONLY_DATA
	/** Delegate to call when a graph's property has changed */
	FOnPropertyChanged		PropertyChangedNotifiers;
#endif
};



/** 
 * Helper object to ensure a graph node is correctly constructed
 *
 * Typical use pattern is:
 * FNodeGraphNodeCreate<NodeType> NodeCreator(Graph);
 * NodeType* Node = NodeCreator.CreateNode();
 * // calls to build out node 
 * Node->MemberVar = ...
 * NodeCreator.Finalize
 */
template <typename NodeType>
struct FGraphNodeCreator
{
public:
	FGraphNodeCreator(UEdGraph& InGraph)
		: bPlaced(false), Graph(InGraph), Node(NULL)
	{
	}

	/** Create an empty placeable graph node */
	NodeType* CreateNode(bool bSelectNewNode = true)
	{
		Node = (NodeType*)Graph.CreateNode(NodeType::StaticClass(), bSelectNewNode);
		return Node;
	} 

	/** Call to finalize the node's construction */
	void Finalize()
	{
		check(!bPlaced);
		Node->CreateNewGuid();
		Node->PostPlacedNewNode();
		bPlaced = true;
		if (Node->Pins.Num() == 0)
		{
			Node->AllocateDefaultPins();
		}
	}

	/** Destructor. Ensures that finalized was called */
	~FGraphNodeCreator()
	{
		checkf(bPlaced, TEXT("Created node was not finalized in a FGraphNodeCreator<%s>"), *NodeType::StaticClass()->GetName());
	}

private:
	// Hide copy and assignment operator
	FGraphNodeCreator(const FGraphNodeCreator& rhs);
	FGraphNodeCreator* operator= (const FGraphNodeCreator& rhs);

	/** The created node */
	NodeType* Node;
	/** Graph reference we're creating the node in */
	UEdGraph& Graph;
	/** If the node has placed and finalized */
	bool bPlaced;
};

