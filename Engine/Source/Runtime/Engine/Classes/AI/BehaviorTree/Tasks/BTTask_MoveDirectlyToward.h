// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "BTTask_MoveDirectlyToward.generated.h"

struct FBTMoveDirectlyTowardMemory
{
	/** Move request ID */
	FAIRequestID MoveRequestID;
};

UCLASS(config=Game, Meta=(
	Tooltip="Moves the AI pawn toward the specified Actor or Location (Vector) blackboard entry in a straight line, without regard to any navigation system.  If you need the AI to navigate, use the \"Move To\" node instead."))
class ENGINE_API UBTTask_MoveDirectlyToward : public UBTTask_BlackboardBase
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(config, Category=Node, EditAnywhere, meta=(ClampMin = "0.0"))
	float AcceptableRadius;

	/** set to true will result in not updating move destination in 
	 *	case where goal is an Actor that can change 
	 *	his location while task is being performed */
	UPROPERTY(Category=Node, EditAnywhere)
	uint32 bForceMoveToLocation:1;

	UPROPERTY(Category=Node, EditAnywhere)
	uint32 bProjectVectorGoalToNavigation:1;

	UPROPERTY(Category=Node, EditAnywhere)
	uint32 bAllowStrafe : 1;

	virtual EBTNodeResult::Type ExecuteTask(class UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory) OVERRIDE;
	virtual EBTNodeResult::Type AbortTask(class UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory) OVERRIDE;
	virtual uint16 GetInstanceMemorySize() const OVERRIDE;
	virtual void DescribeRuntimeValues(const class UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const OVERRIDE;
	virtual FString GetStaticDescription() const OVERRIDE;
};
