// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationTestingActor.generated.h"

struct FNavTestTickHelper : FTickableGameObject
{
	TWeakObjectPtr<class ANavigationTestingActor> Owner;

	FNavTestTickHelper() : Owner(NULL) {}
	virtual void Tick(float DeltaTime);
	virtual bool IsTickable() const { return Owner.IsValid(); }
	virtual bool IsTickableInEditor() const { return true; }
	virtual TStatId GetStatId() const ;
};

UENUM()
namespace ENavCostDisplay
{
	enum Type
	{
		TotalCost,
		HeuristicOnly,
		RealCostOnly,
	};
}

UCLASS(hidecategories=(Object, Actor, Input, Rendering), showcategories=("Input|MouseInput", "Input|TouchInput"), Blueprintable)
class ENGINE_API ANavigationTestingActor : public AActor, public INavAgentInterface, public INavPathObserverInterface
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TSubobjectPtr<class UCapsuleComponent> CapsuleComponent;

#if WITH_EDITORONLY_DATA
	/** Editor Preview */
	UPROPERTY()
	TSubobjectPtr<class UNavTestRenderingComponent> EdRenderComp;
#endif // WITH_EDITORONLY_DATA

	/** @todo document */
	UPROPERTY(EditAnywhere, Category=Agent)
	FNavAgentProperties NavAgentProps;

	UPROPERTY(EditAnywhere, Category=Agent)
	FVector QueryingExtent;

	UPROPERTY(transient)
	ANavigationData* MyNavData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=AgentStatus)
	FVector ProjectedLocation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=AgentStatus)
	uint32 bProjectedLocationValid : 1;

	UPROPERTY(EditAnywhere, Category=Pathfinding)
	uint32 bSearchStart : 1;

	UPROPERTY(EditAnywhere, Category=Pathfinding)
	uint32 bUseHierarchicalPathfinding : 1;

	/** if set, all steps of A* algorithm will be accessible for debugging */
	UPROPERTY(EditAnywhere, Category=Pathfinding)
	uint32 bGatherDetailedInfo : 1;

	/** show polys from open (orange) and closed (yellow) sets */
	UPROPERTY(EditAnywhere, Category=Debug)
	uint32 bShowNodePool : 1;

	/** show current best path */
	UPROPERTY(EditAnywhere, Category=Debug)
	uint32 bShowBestPath : 1;

	/** show which nodes were modified in current A* step */
	UPROPERTY(EditAnywhere, Category=Debug)
	uint32 bShowDiffWithPreviousStep : 1;

	UPROPERTY(EditAnywhere, Category=Debug)
	uint32 bShouldBeVisibleInGame : 1;

	/** determines which cost will be shown*/
	UPROPERTY(EditAnywhere, Category=Debug)
	TEnumAsByte<ENavCostDisplay::Type> CostDisplayMode;

	/** text canvas offset to apply */
	UPROPERTY(EditAnywhere, Category=Debug)
	FVector2D TextCanvasOffset;

	UPROPERTY(transient, VisibleAnywhere, BlueprintReadOnly, Category=PathfindingStatus)
	uint32 bPathExist : 1;

	UPROPERTY(transient, VisibleAnywhere, BlueprintReadOnly, Category=PathfindingStatus)
	uint32 bPathIsPartial : 1;
	
	UPROPERTY(transient, VisibleAnywhere, BlueprintReadOnly, Category=PathfindingStatus)
	uint32 bPathSearchOutOfNodes : 1;

	/** Time in micro seconds */
	UPROPERTY(transient, VisibleAnywhere, BlueprintReadOnly, Category=PathfindingStatus)
	float PathfindingTime;

	UPROPERTY(transient, VisibleAnywhere, BlueprintReadOnly, Category=PathfindingStatus)
	float PathCost;

	UPROPERTY(transient, VisibleAnywhere, BlueprintReadOnly, Category=PathfindingStatus)
	int32 PathfindingSteps;

	UPROPERTY(transient, EditAnywhere, Category=Pathfinding)
	ANavigationTestingActor* OtherActor;

	/** "None" will result in default filter being used */
	UPROPERTY(EditAnywhere, Category=Pathfinding)
	TSubclassOf<class UNavigationQueryFilter> FilterClass;

	UPROPERTY(transient, EditInstanceOnly, Category=Debug, meta=(ClampMin="-1", UIMin="-1"))
	int32 ShowStepIndex;

	UPROPERTY(EditAnywhere, Category=Pathfinding)
	float OffsetFromCornersDistance;

#if WITH_RECAST && WITH_EDITORONLY_DATA
	/** detail data gathered from each step of regular A* algorithm */
	TArray<struct FRecastDebugPathfindingStep> DebugSteps;

	FNavTestTickHelper* TickHelper;
#endif

	FNavPathSharedPtr LastPath;

	/** Dtor */
	virtual ~ANavigationTestingActor();

	virtual void BeginDestroy() OVERRIDE;

#if WITH_EDITOR
	virtual void PreEditChange(UProperty* PropertyThatWillChange) OVERRIDE;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
	virtual void PostEditMove(bool bFinished) OVERRIDE;
	
	virtual void PostLoad() OVERRIDE;
	void TickMe();
#endif // WITH_EDITOR

	virtual bool UpdateNavigationRelevancy() OVERRIDE { SetNavigationRelevancy(false); return false; }
	// Begin INavAgentInterface Interface
	virtual const struct FNavAgentProperties* GetNavAgentProperties() const OVERRIDE { return &NavAgentProps; }
	virtual FVector GetNavAgentLocation() const OVERRIDE;
	virtual void GetMoveGoalReachTest(class AActor* MovingActor, const FVector& MoveOffset, FVector& GoalOffset, float& GoalRadius, float& GoalHalfHeight) const OVERRIDE {}
	// End INavAgentInterface Interface

	// Begin INavPathObserverInterface Interface
	virtual void OnPathUpdated(class INavigationPathGenerator* PathGenerator) OVERRIDE;
	// End INavPathObserverInterface Interface	

	void UpdateNavData();
	void UpdatePathfinding();
	void GatherDetailedData(class ANavigationTestingActor* Goal);
	void SearchPathTo(class ANavigationTestingActor* Goal);

	// Virtual method to override if you want to customize the query being 
	// constructed for the path find (e.g. change the filter or add 
	// constraints/goal evaluators).
	virtual FPathFindingQuery BuildPathFindingQuery(const ANavigationTestingActor* Goal) const;
};
