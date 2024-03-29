// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraphNode.generated.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class SGraphNode;

/** Enum to indicate what sort of title we want */
UENUM()
namespace ENodeTitleType
{
	enum Type
	{
		// The full title, may be multiple lines
		FullTitle,
		// More concise, single line title
		ListView,
		// Returns the editable title (which might not be a title at all)
		EditableTitle
	};
}

/** Enum to indicate if a node has advanced-display-pins, and if they are shown */
UENUM()
namespace ENodeAdvancedPins
{
	enum Type
	{
		// No advanced pins
		NoPins,
		// There are some advanced pins, and they are shown
		Shown,
		// There are some advanced pins, and they are hidden
		Hidden
	};
}

// This is the context for a GetContextMenuActions call into a specific node
struct FGraphNodeContextMenuBuilder
{
	// The blueprint associated with this context; may be NULL for non-Kismet related graphs.
	const UBlueprint* Blueprint;
	// The graph associated with this context.
	const UEdGraph* Graph;
	// The node associated with this context.
	const UEdGraphNode* Node;
	// The pin associated with this context; may be NULL when over a node.
	const UEdGraphPin* Pin;
	// The menu builder to append actions to.
	class FMenuBuilder* MenuBuilder;
	// Whether the graph editor is currently part of a debugging session (any non-debugging commands should be disabled).
	bool bIsDebugging;

	FGraphNodeContextMenuBuilder(const UEdGraph* InGraph, const UEdGraphNode* InNode, const UEdGraphPin* InPin, class FMenuBuilder* InMenuBuilder, bool bInDebuggingMode);
};

UCLASS()
class ENGINE_API UEdGraphNode : public UObject
{
	GENERATED_UCLASS_BODY()

	/** List of connector pins */
	UPROPERTY()
	TArray<class UEdGraphPin*> Pins;

	/** X position of node in the editor */
	UPROPERTY()
	int32 NodePosX;

	/** Y position of node in the editor */
	UPROPERTY()
	int32 NodePosY;

	/** Width of node in the editor; only used when the node can be resized */
	UPROPERTY()
	int32 NodeWidth;

	/** Height of node in the editor; only used when the node can be resized */
	UPROPERTY()
	int32 NodeHeight;

#if WITH_EDITORONLY_DATA
	/** If true, this node can be resized and should be drawn with a resize handle */
	UPROPERTY()
	uint32 bCanResizeNode:1;
#endif // WITH_EDITORONLY_DATA

	/** Flag to check for compile error/warning */
	UPROPERTY()
	uint32 bHasCompilerMessage:1;

#if WITH_EDITORONLY_DATA
	/** If true, this node can be renamed in the editor */
	UPROPERTY()
	uint32 bCanRenameNode:1;
#endif // WITH_EDITORONLY_DATA

	/** Comment string that is drawn on the node */
	UPROPERTY()
	FString NodeComment;

	/** Flag to store node specific compile error/warning*/
	UPROPERTY()
	int32 ErrorType;
	
	/** Error/Warning description */
	UPROPERTY()
	FString ErrorMsg;
	
	/** GUID to uniquely identify this node, to facilitate diff'ing versions of this graph */
	UPROPERTY()
	FGuid NodeGuid;

	/** Enum to indicate if a node has advanced-display-pins, and if they are shown */
	UPROPERTY()
	TEnumAsByte<ENodeAdvancedPins::Type> AdvancedPinDisplay;

#if WITH_EDITOR

private:
	static TArray<UEdGraphPin*> PooledPins;

public:
	// UObject interface
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	virtual void PostLoad() OVERRIDE;
	// End of UObject interface

	/** widget representing this node if exists */
	TWeakPtr<SGraphNode> NodeWidget;

	/** Create a new pin on this node using the supplied info, and return the new pin */
	UEdGraphPin* CreatePin(EEdGraphPinDirection Dir, const FString& PinCategory, const FString& PinSubCategory, UObject* PinSubCategoryObject, bool bIsArray, bool bIsReference, const FString& PinName, bool bIsConst = false, int32 Index = INDEX_NONE);

	// Allocates a pin from the pool
	static UEdGraphPin* AllocatePinFromPool(UEdGraphNode* OuterNode);

	// Returns the specified pin to the pool
	static void ReturnPinToPool(UEdGraphPin* OldPin);

	/** Find a pin on this node with the supplied name */
	UEdGraphPin* FindPin(const FString& PinName) const;

	/** Find a pin on this node with the supplied name and assert if it is not present */
	UEdGraphPin* FindPinChecked(const FString& PinName) const;
	
	/** Find a pin on this node with the supplied name and remove it */
	void DiscardPin(UEdGraphPin* Pin);

	/** Whether or not this node should be given the chance to override pin names.  If this returns true, then GetPinNameOverride() will be called for each pin, each frame */
	virtual bool ShouldOverridePinNames() const { return false; }

	/** Gets the overridden name for the specified pin, if any */
	virtual FString GetPinNameOverride(const UEdGraphPin& Pin) const { return FString(TEXT("")); }

	/** Gets the display name for a pin */
	virtual FString GetPinDisplayName(const UEdGraphPin* Pin) const;

	/**
	 * Fetch the hover text for a pin when the graph is being edited.
	 * 
	 * @param   Pin				The pin to fetch hover text for (should belong to this node)
	 * @param   HoverTextOut	This will get filled out with the requested text
	 */
	virtual void GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const;

	/** Gets the index for a pin */
	int32 GetPinIndex(UEdGraphPin* Pin) const;

	/** Break all links on this node */
	void BreakAllNodeLinks();

	/** Snap this node to a specified grid size */
	void SnapToGrid(float GridSnapSize);

	/** Clear error flag */
	void ClearCompilerMessage()
	{
		bHasCompilerMessage = false;
	}

	/** Generate a unique pin name, trying to stick close to a passed in name */
	FString CreateUniquePinName(FString SourcePinName) const
	{
		FString PinName(SourcePinName);
		
		int32 Index = 1;
		while (FindPin(PinName) != NULL)
		{
			++Index;
			PinName = SourcePinName + FString::FromInt(Index);
		}

		return PinName;
	}

	/** Returns the graph that contains this node */
	class UEdGraph* GetGraph() const
	{
		UEdGraph* Graph = Cast<UEdGraph>(GetOuter());
		if(Graph == NULL)
		{
			ensureMsgf(false, TEXT("EdGraphNode::GetGraph : '%s' does not have a UEdGraph as an Outer."), *GetPathName());
		}
		return Graph;
	}

	/**
	 * Allocate default pins for a given node, based only the NodeType, which should already be filled in.
	 *
	 * @return	true if the pin creation succeeds, false if there was a problem (such a failure to find a function when the node is a function call).
	 */
	virtual void AllocateDefaultPins() {}

	/** Destroy the specified node */
	virtual void DestroyNode();

	/**
	 * Refresh the connectors on a node, preserving as many connections as it can.
	 */
	virtual void ReconstructNode() {}

	/**
	 * Perform any steps necessary prior to copying a node into the paste buffer
	 */
	virtual void PrepareForCopying() {}

	/**
	 * Determine if this node can live in the specified graph
	 */
	virtual bool CanPasteHere(const UEdGraph* TargetGraph, const UEdGraphSchema* Schema) const { return CanCreateUnderSpecifiedSchema(Schema); }

	/**
	 * Determine if this node can be created under the specified schema
     */
	virtual bool CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const { return true; }

	/**
	 * Perform any fixups (deep copies of associated data, etc...) necessary after a node has been pasted in the editor
	 */
	virtual void PostPasteNode() {}

	/** Gets the name of this node, shown in title bar */
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const;
	
	/** Gets the name of this node in the native language. This should always be overridden when a node's title is built from concatenation
		By default this will return the source string from GetNodeTitle, which is not guaranteed to be correct */
	virtual FString GetNodeNativeTitle(ENodeTitleType::Type TitleType) const;

	/** Gets the searchable metadata of this node */
	virtual FText GetNodeSearchTitle() const;

	/** 
	 * Gets the draw color of a node's title bar
	 */
	virtual FLinearColor GetNodeTitleColor() const;

	/**
	 * Get the draw color for a node's comment popup
	 */
	virtual FLinearColor GetNodeCommentColor() const;

	/**
	 * Gets the tooltip to display when over the node
	 */
	virtual FString GetTooltip() const;

	/**
	 * Returns the keywords that should be used when searching for this node
	 */
	virtual FString GetKeywords() const { return TEXT(""); }

	/**
	 * Returns the link used for external documentation for the graph node
	 */
	virtual FString GetDocumentationLink() const { return TEXT(""); }

	/**
	 * Returns the name of the excerpt to display from the specified external documentation link for the graph node
	 * Default behavior is to return the class name (including prefix)
	 */
	virtual FString GetDocumentationExcerptName() const;

	/** @return name or brush to use in menu or on node */
	virtual FName GetPaletteIcon(FLinearColor& OutColor) const { return TEXT("GraphEditor.Default_16x"); }

	/** Should we show the Palette Icon for this node on the node title */
	virtual bool ShowPaletteIconOnNode() const { return false; }

	/**
	 * Autowire a newly created node.
	 *
	 * @param	FromPin	The source pin that caused the new node to be created (typically a drag-release context menu creation).
	 */
	virtual void AutowireNewNode(UEdGraphPin* FromPin) {}

	// A chance to initialize a new node; called just once when a new node is created, before AutowireNewNode or AllocateDefaultPins is called.
	// This method is not called when a node is reconstructed, etc...
	virtual void PostPlacedNewNode() {}

	/** Called when the DefaultValue of one of the pins of this node is changed in the editor */
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) {}

	/** Called when the connection list of one of the pins of this node is changed in the editor */
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) {}

	/** Called when one of the pins of this node has had its' pin type changed from an external source (like the SPinTypeSelector in the case of kismet) */
	virtual void PinTypeChanged(UEdGraphPin* Pin) {}

	/**
	 * Called when something external to this node has changed the connection list of any of the pins in the node
	 *   - Different from PinConnectionListChanged as this is called outside of any loops iterating over our pins allowing
	 *     us to do things like reconstruct the node safely without trashing pins we are already iterating on
	 *   - Typically called after a user induced action like making a pin connection or a pin break
	 */
	virtual void NodeConnectionListChanged() {}

	/** Shorthand way to access the schema of the graph that owns this node */
	const class UEdGraphSchema* GetSchema() const;

	/** Whether or not this node can be safely duplicated (via copy/paste, etc...) in the graph */
	virtual bool CanDuplicateNode() const;

	/** Whether or not this node can be deleted by user action */
	virtual bool CanUserDeleteNode() const;

	/** Tries to come up with a descriptive name for the compiled output */
	virtual FString GetDescriptiveCompiledName() const;

	/** Update node size to new value */
	virtual void ResizeNode(const FVector2D& NewSize) {}

	// Returns true if this node is deprecated
	virtual bool IsDeprecated() const;

	// Returns true if this node should produce a compiler warning on deprecation
	virtual bool ShouldWarnOnDeprecation() const { return true; }

	// Returns the string to use when reporting the deprecation
	virtual FString GetDeprecationMessage() const;

	// Returns the object that should be focused when double-clicking on this node
	// (the object can be an actor, which selects it in the world, or a node/graph/pin)
	virtual UObject* GetJumpTargetForDoubleClick() const;

	/** Create a new unique Guid for this node */
	void CreateNewGuid();

	/** Gets a list of actions that can be done to this particular node */
	virtual void GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const {}

	// Gives each visual node a chance to do final validation before it's node is harvested for use at runtime
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const {}

	/** Gives the node the option to customize how diffs are discovered within it.  */
	virtual void FindDiffs(class UEdGraphNode* OtherNode, struct FDiffResults& Results ) ;

	// This function gets menu items that can be created using this node given the specified context
	virtual void GetMenuEntries(struct FGraphContextMenuBuilder& ContextMenuBuilder) const {}

	// create a name validator for this node
	virtual TSharedPtr<class INameValidatorInterface> MakeNameValidator() const { return NULL; }

	// called when this node is being renamed after a successful name validation
	virtual void OnRenameNode(const FString& NewName) {}

	/** Return whether to draw this node as a comment node */
	virtual bool ShouldDrawNodeAsComment() const { return false; }

#endif // WITH_EDITOR

};



