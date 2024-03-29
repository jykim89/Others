// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "BTTask_RunBehavior.generated.h"

/**
 * RunBehavior task allows pushing subtrees on execution stack.
 * Subtree asset can't be changed in runtime! 
 *
 * This limitation is caused by support for subtree's root level decorators,
 * which are injected into parent tree, and structure of running tree
 * cannot be modified in runtime (see: BTNode: ExecutionIndex, MemoryOffset)
 *
 * Dynamic subtrees can be implemented, but at the cost of some features:
 * - no root level decorators
 * - no or limited editor preview
 */

UCLASS(MinimalAPI)
class UBTTask_RunBehavior : public UBTTaskNode
{
	GENERATED_UCLASS_BODY()

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory) OVERRIDE;
	virtual uint16 GetInstanceMemorySize() const OVERRIDE;
	virtual FString GetStaticDescription() const OVERRIDE;

	/** called on instance startup, prepares root level nodes to use */
	void InjectNodes(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory, int32& InstancedIndex) const;

	/** @returns number of injected nodes */
	int32 GetInjectedNodesCount() const;

	/** @returns subtree asset */
	class UBehaviorTree* GetSubtreeAsset() const;

protected:

	/** behavior to run */
	UPROPERTY(Category = Node, EditAnywhere)
	class UBehaviorTree* BehaviorAsset;
};

//////////////////////////////////////////////////////////////////////////
// Inlines

FORCEINLINE class UBehaviorTree* UBTTask_RunBehavior::GetSubtreeAsset() const
{
	return BehaviorAsset;
}

FORCEINLINE int32 UBTTask_RunBehavior::GetInjectedNodesCount() const
{
	return BehaviorAsset ? BehaviorAsset->RootDecorators.Num() : 0;
}
