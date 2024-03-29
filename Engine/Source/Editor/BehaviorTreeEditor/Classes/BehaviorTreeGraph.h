// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "BehaviorTreeGraph.generated.h"

UCLASS()
class UBehaviorTreeGraph : public UEdGraph
{
	GENERATED_UCLASS_BODY()

	enum EDebuggerFlags
	{
		ClearDebuggerFlags,
		SkipDebuggerFlags,
	};

	/** counter incremented on every graph update, used for refreshing injected nodes in parent graph */
	UPROPERTY()
	int32 GraphVersion;

	void UpdateBlackboardChange();
	void UpdateAsset(EDebuggerFlags DebuggerFlags);
	void UpdateAbortHighlight(struct FAbortDrawHelper& Mode0, struct FAbortDrawHelper& Mode1);
	void CreateBTFromGraph(class UBehaviorTreeGraphNode* RootEdNode);
	void RemoveOrphanedNodes();
	void UpdatePinConnectionTypes();
	void UpdateDeprecatedNodes();
	bool UpdateInjectedNodes();
	class UEdGraphNode* FindInjectedNode(int32 Index);
	void ReplaceNodeConnections(UEdGraphNode* OldNode, UEdGraphNode* NewNode);
	void RebuildExecutionOrder();

	void LockUpdates();
	void UnlockUpdates();

protected:

	/** if set, graph modifications won't cause updates in internal tree structure (skipping UpdateAsset)
	* flag allows freezing update during heavy changes like pasting new nodes */
	uint32 bLockUpdates : 1;
};



