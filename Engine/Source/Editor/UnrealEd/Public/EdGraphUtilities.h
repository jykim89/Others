// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet2/CompilerResultsLog.h"

//////////////////////////////////////////////////////////////////////////

struct UNREALED_API FGraphPanelNodeFactory : public TSharedFromThis<FGraphPanelNodeFactory>
{
public:
	virtual TSharedPtr<class SGraphNode> CreateNode(class UEdGraphNode* Node) const { return NULL; }
};

//////////////////////////////////////////////////////////////////////////

struct UNREALED_API FGraphPanelPinFactory : public TSharedFromThis<FGraphPanelPinFactory>
{
public:
	virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* Pin) const { return NULL; }
};

//////////////////////////////////////////////////////////////////////////

class UNREALED_API FEdGraphUtilities
{
public:
	static void RegisterVisualNodeFactory(TSharedPtr<FGraphPanelNodeFactory> NewFactory);
	static void UnregisterVisualNodeFactory(TSharedPtr<FGraphPanelNodeFactory> OldFactory);

	static void RegisterVisualPinFactory(TSharedPtr<FGraphPanelPinFactory> NewFactory);
	static void UnregisterVisualPinFactory(TSharedPtr<FGraphPanelPinFactory> OldFactory);

	/** After pasting nodes, need to perform some fixup for pins etc. */
	static void PostProcessPastedNodes(TSet<class UEdGraphNode*>& SpawnedNodes);

	/** Finds the net associated with a particular pin */
	static UEdGraphPin* GetNetFromPin(UEdGraphPin* Pin);

	/** 
	 *	Clones (deep copies) a UEdGraph, including all of it's nodes and pins and their links, maintaining a mapping from the clone to the source nodes (even across multiple clonings) 
	 *	A NewOuter of NULL indicates the transient package
	 */
	static UEdGraph* CloneGraph(UEdGraph* Source, UObject* NewOuter, FCompilerResultsLog* MessageLog = NULL, bool bCloningForCompile = false);

	/**
	  * Clones the content from SourceGraph and merges it into MergeTarget;
	  * including merging/flattening all of the children from the SourceGraph into MergeTarget
	  *
	  * Also optionally populates *OutClonedNodes with the list of cloned objects.
	  */
	static void CloneAndMergeGraphIn(UEdGraph* MergeTarget, UEdGraph* SourceGraph, FCompilerResultsLog& MessageLog, bool bRequireSchemaMatch, TArray<UEdGraphNode*>* OutClonedNodes = NULL);

	/**
	 * Moves the contents of all of the children graphs of ParentGraph (recursively) into the MergeTarget graph.
	 * This does not clone, it's destructive to the ParentGraph
	 */
	static void MergeChildrenGraphsIn(UEdGraph* MergeTarget, UEdGraph* ParentGraph, bool bRequireSchemaMatch);

	/** Tries to rename the graph to have a name similar to BaseName */
	static void RenameGraphCloseToName(UEdGraph* Graph, const FString& BaseName, int32 StartIndex = 1);
	static void RenameGraphToNameOrCloseToName(UEdGraph* Graph, const FString& DesiredName);

	/**
	 * Exports a set of nodes to text
	 *
	 * @param	NodesToExport		Set of nodes to export, all nodes must be in the same graph
	 * @param	[out] ExportedText	The text representation will be stored here
	 */
	static void ExportNodesToText(TSet<UObject*> NodesToExport, /*out*/ FString& ExportedText);

	/**
	 * Imports a set of previously exported nodes into a graph
	 *
	 * @param	DestinationGraph		The target graph to place the imported nodes in
	 * @param	TextToImport			The text representation of the nodes to import
	 * @param	[out] ImportedNodeSet	The set of nodes that were imported
	 */
	static void ImportNodesFromText(UEdGraph* DestinationGraph, const FString& TextToImport, /*out*/ TSet<UEdGraphNode*>& ImportedNodeSet);

	/**
	 * Can we import nodes from this text ?
	 *
	 * @param	DestinationGraph		The target graph to place the imported nodes in
	 * @param	TextToImport			The text representation of the nodes to import
	 */
	static bool CanImportNodesFromText(const UEdGraph* DestinationGraph, const FString& TextToImport);

	// Calculates an approximate bounding box for the specified nodes
	static FIntRect CalculateApproximateNodeBoundaries(const TArray<UEdGraphNode*>& Nodes);

	/** Copy Common State of data from OldNode to NewNode **/
	static void CopyCommonState(UEdGraphNode* OldNode, UEdGraphNode* NewNode);

	struct FNodeVisitor
	{
		TSet<UEdGraphNode*> VisitedNodes;

		void TouchNode(UEdGraphNode* Node)
		{
		}

		void TraverseNodes(UEdGraphNode* Node)
		{
			VisitedNodes.Add(Node);
			TouchNode(Node);

			// Follow every pin
			for (int32 i = 0; i < Node->Pins.Num(); ++i)
			{
				UEdGraphPin* MyPin = Node->Pins[i];

				// And every connection to the pin
				for (int32 j = 0; j < MyPin->LinkedTo.Num(); ++j)
				{
					UEdGraphPin* OtherPin = MyPin->LinkedTo[j];
					if( OtherPin )
					{
						UEdGraphNode* OtherNode = OtherPin->GetOwningNodeUnchecked();
						if (OtherNode && !VisitedNodes.Contains(OtherNode))
						{
							TraverseNodes(OtherNode);
						}
					}
				}
			}
		}
	};
private:
	static TArray< TSharedPtr<FGraphPanelNodeFactory> > VisualNodeFactories;
	static TArray< TSharedPtr<FGraphPanelPinFactory> > VisualPinFactories;
	friend class FNodeFactory;

	// Should never create an instance of this class
	FEdGraphUtilities() {}
};

//////////////////////////////////////////////////////////////////////////

/** A weak reference to a UEdGraphPin object that can remain valid through pin connection state changes that might trigger owner node reconstruction */
struct FWeakGraphPinPtr
{
	/** Default constructor */
	FORCEINLINE FWeakGraphPinPtr()
	{
		Reset();
	}

	/** Construct from another weak pointer */
	FORCEINLINE FWeakGraphPinPtr(const FWeakGraphPinPtr &OtherPinPtr)
	{
		(*this) = OtherPinPtr;
	}

	/** Construct from a UEdGraphPin object pointer */
	FORCEINLINE FWeakGraphPinPtr(const class UEdGraphPin *Pin)
	{
		(*this) = Pin;
	}

	/** Reset the weak pointer back to NULL state */
	FORCEINLINE void Reset()
	{
		PinName = TEXT("");
		PinObjectPtr.Reset();
		NodeObjectPtr.Reset();
	}

	/** Assign from another weak pointer */
	FORCEINLINE void operator=(const FWeakGraphPinPtr &OtherPinPtr)
	{
		PinName = OtherPinPtr.PinName;
		PinObjectPtr = OtherPinPtr.PinObjectPtr;
		NodeObjectPtr = OtherPinPtr.NodeObjectPtr;
	}

	/** Assign from a UEdGraphPin object pointer */
	UNREALED_API void operator=(const class UEdGraphPin *Pin);

	/** Compare weak pointers for equality */ 
	FORCEINLINE bool operator==(const FWeakGraphPinPtr &OtherPinPtr) const
	{
		return PinName.Equals(OtherPinPtr.PinName)
			&& PinObjectPtr == OtherPinPtr.PinObjectPtr
			&& NodeObjectPtr == OtherPinPtr.NodeObjectPtr;
	}

	/** Compare weak pointers for inequality */ 
	FORCEINLINE bool operator!=(const FWeakGraphPinPtr &OtherPinPtr) const
	{
		return !PinName.Equals(OtherPinPtr.PinName)
			|| PinObjectPtr != OtherPinPtr.PinObjectPtr
			|| NodeObjectPtr != OtherPinPtr.NodeObjectPtr;
	}

	/** Method to obtain the actual object reference from the weak pointer. Will attempt to fix up the pin object reference if it's no longer valid, otherwise returns NULL */
	UNREALED_API class UEdGraphPin *Get();

	/** Cast to object pointer type */
	FORCEINLINE operator class UEdGraphPin*()
	{
		return Get();
	};

	/** Arrow operator */
	FORCEINLINE class UEdGraphPin* operator->()
	{
		return Get();
	}

	/** Test for validity */
	FORCEINLINE bool IsValid()
	{
		return Get() != NULL;
	}

private:
	/** Pin name string */
	FString PinName;

	/** Weak reference to the UEdGraphPin object */
	TWeakObjectPtr<class UEdGraphPin> PinObjectPtr;

	/** Weak reference to the UEdGraphNode object that owns the pin object */
	TWeakObjectPtr<class UEdGraphNode> NodeObjectPtr;
};
