// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

UBTDecorator_DoesPathExist::UBTDecorator_DoesPathExist(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
	NodeName = "Does path exist";

	// accept only actors and vectors
	BlackboardKeyA.AddObjectFilter(this, AActor::StaticClass());
	BlackboardKeyA.AddVectorFilter(this);
	BlackboardKeyB.AddObjectFilter(this, AActor::StaticClass());
	BlackboardKeyB.AddVectorFilter(this);

	bAllowAbortLowerPri = false;
	bAllowAbortNone = true;
	bAllowAbortChildNodes = false;
	FlowAbortMode = EBTFlowAbortMode::None;

	BlackboardKeyA.SelectedKeyName = FBlackboard::KeySelf;
	PathQueryType = EPathExistanceQueryType::HierarchicalQuery;
}

void UBTDecorator_DoesPathExist::InitializeFromAsset(class UBehaviorTree* Asset)
{
	Super::InitializeFromAsset(Asset);

	if (bUseSelf)
	{
		BlackboardKeyA.SelectedKeyName = FBlackboard::KeySelf;
		bUseSelf = false;
	}

	UBlackboardData* BBAsset = GetBlackboardAsset();
	BlackboardKeyA.CacheSelectedKey(BBAsset);
	BlackboardKeyB.CacheSelectedKey(BBAsset);
}

bool UBTDecorator_DoesPathExist::CalculateRawConditionValue(class UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory) const 
{
	const UBlackboardComponent* BlackboardComp = OwnerComp->GetBlackboardComponent();
	if (BlackboardComp == NULL)
	{
		return false;
	}

	FVector PointA = FVector::ZeroVector;
	FVector PointB = FVector::ZeroVector;	
	const bool bHasPointA = BlackboardComp->GetLocationFromEntry(BlackboardKeyA.GetSelectedKeyID(), PointA);
	const bool bHasPointB = BlackboardComp->GetLocationFromEntry(BlackboardKeyB.GetSelectedKeyID(), PointB);
	
	bool bHasPath = false;

	const UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(OwnerComp->GetWorld());
	if (NavSys && bHasPointA && bHasPointB)
	{
		const AAIController* AIOwner = Cast<AAIController>(OwnerComp->GetOwner());
		const ANavigationData* NavData = AIOwner && AIOwner->NavComponent ? AIOwner->NavComponent->GetNavData() : NULL;
		TSharedPtr<const FNavigationQueryFilter> QueryFilter = UNavigationQueryFilter::GetQueryFilter(NavData, FilterClass);

		if (PathQueryType == EPathExistanceQueryType::NavmeshRaycast2D)
		{
#if WITH_RECAST
			const ARecastNavMesh* RecastNavMesh = Cast<const ARecastNavMesh>(NavData);
			bHasPath = RecastNavMesh && RecastNavMesh->IsSegmentOnNavmesh(PointA, PointB, QueryFilter);
#endif
		}
		else
		{		
			EPathFindingMode::Type TestMode = (PathQueryType == EPathExistanceQueryType::HierarchicalQuery) ? EPathFindingMode::Hierarchical : EPathFindingMode::Regular;
			bHasPath = NavSys->TestPathSync(FPathFindingQuery(AIOwner, NavData, PointA, PointB, QueryFilter), TestMode);
		}
	}

	return bHasPath;
}

FString UBTDecorator_DoesPathExist::GetStaticDescription() const 
{
	const UEnum* PathTypeEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EPathExistanceQueryType"));
	check(PathTypeEnum);

	return FString::Printf(TEXT("%s: Find path from %s to %s (mode:%s)"),
		*Super::GetStaticDescription(),
		*BlackboardKeyA.SelectedKeyName.ToString(),
		*BlackboardKeyB.SelectedKeyName.ToString(),
		*PathTypeEnum->GetEnumName(PathQueryType));
}
