// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#if WITH_EDITOR
#include "ObjectEditorUtils.h"
#endif

void FNavTestTickHelper::Tick(float DeltaTime)
{
#if WITH_EDITOR
	if (Owner.IsValid())
	{
		Owner->TickMe();
	}
#endif // WITH_EDITOR
}

TStatId FNavTestTickHelper::GetStatId() const 
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FNavTestTickHelper, STATGROUP_Tickables);
}

ANavigationTestingActor::ANavigationTestingActor(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
#if WITH_EDITORONLY_DATA
	EdRenderComp = PCIP.CreateDefaultSubobject<UNavTestRenderingComponent>(this, TEXT("EdRenderComp"));
	EdRenderComp->PostPhysicsComponentTick.bCanEverTick = false;

#if WITH_RECAST
	TickHelper = NULL;
#endif // WITH_RECAST
#endif // WITH_EDITORONLY_DATA

	NavAgentProps.AgentRadius = 34.f;
	NavAgentProps.AgentHeight = 144.f;
	ShowStepIndex = -1;
	bShowNodePool = true;
	bShowBestPath = true;
	bShowDiffWithPreviousStep = false;
	bShouldBeVisibleInGame = false;
	TextCanvasOffset = FVector2D::ZeroVector;
	bGatherDetailedInfo = true;
	OffsetFromCornersDistance = 0.f;

	QueryingExtent = FVector(DEFAULT_NAV_QUERY_EXTENT_HORIZONTAL, DEFAULT_NAV_QUERY_EXTENT_HORIZONTAL, DEFAULT_NAV_QUERY_EXTENT_VERTICAL);

	// collision profile name set up - found in baseengine.ini
	static FName CollisionProfileName(TEXT("Pawn"));

	CapsuleComponent = PCIP.CreateDefaultSubobject<UCapsuleComponent>(this, TEXT("CollisionCylinder"));
	CapsuleComponent->InitCapsuleSize(NavAgentProps.AgentRadius, NavAgentProps.AgentHeight / 2);
	CapsuleComponent->SetCollisionProfileName(CollisionProfileName);
	CapsuleComponent->CanBeCharacterBase = ECB_No;
	CapsuleComponent->bShouldUpdatePhysicsVolume = true;

	RootComponent = CapsuleComponent;
}

ANavigationTestingActor::~ANavigationTestingActor()
{
#if WITH_RECAST && WITH_EDITORONLY_DATA
	delete TickHelper;
#endif // WITH_RECAST && WITH_EDITORONLY_DATA
}

void ANavigationTestingActor::BeginDestroy()
{
	LastPath.Reset();
	if (OtherActor && OtherActor->OtherActor == this)
	{
		OtherActor->OtherActor = NULL;
		OtherActor->LastPath.Reset();
	}
	Super::BeginDestroy();
}

#if WITH_EDITOR
void ANavigationTestingActor::PreEditChange(UProperty* PropertyThatWillChange)
{
	static const FName NAME_OtherActor = GET_MEMBER_NAME_CHECKED(ANavigationTestingActor, OtherActor);

	if (PropertyThatWillChange && PropertyThatWillChange->GetFName() == NAME_OtherActor && OtherActor && OtherActor->OtherActor == this)
	{
		OtherActor->OtherActor = NULL;
		OtherActor->LastPath.Reset();
		LastPath.Reset();
#if WITH_EDITORONLY_DATA
		OtherActor->EdRenderComp->MarkRenderStateDirty();
		EdRenderComp->MarkRenderStateDirty();
#endif
	}

	Super::PreEditChange(PropertyThatWillChange);
}

void ANavigationTestingActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	static const FName NAME_ShouldBeVisibleInGame = GET_MEMBER_NAME_CHECKED(ANavigationTestingActor, bShouldBeVisibleInGame);
	static const FName NAME_OtherActor = GET_MEMBER_NAME_CHECKED(ANavigationTestingActor, OtherActor);
	static const FName NAME_IsSearchStart = GET_MEMBER_NAME_CHECKED(ANavigationTestingActor, bSearchStart);

	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		const FName ChangedPropName = PropertyChangedEvent.Property->GetFName();
		const FName ChangedCategory = FObjectEditorUtils::GetCategoryFName(PropertyChangedEvent.Property);

		if (ChangedPropName == GET_MEMBER_NAME_CHECKED(FNavAgentProperties,AgentRadius) ||
			ChangedPropName == GET_MEMBER_NAME_CHECKED(FNavAgentProperties,AgentHeight))
		{
			MyNavData = NULL;
			UpdateNavData();

			CapsuleComponent->SetCapsuleSize(NavAgentProps.AgentRadius, NavAgentProps.AgentHeight/2);
		}
		else if (ChangedPropName == GET_MEMBER_NAME_CHECKED(ANavigationTestingActor,QueryingExtent))
		{
			UpdateNavData();

			FNavLocation NavLoc;
			bProjectedLocationValid = GetWorld()->GetNavigationSystem()->ProjectPointToNavigation(GetActorLocation(), NavLoc, QueryingExtent, MyNavData);
			ProjectedLocation = NavLoc.Location;
		}
		else if (ChangedPropName == NAME_ShouldBeVisibleInGame)
		{
			bHidden = !bShouldBeVisibleInGame;
		}
		else if (ChangedCategory == TEXT("Debug"))
		{
#if WITH_EDITORONLY_DATA
			EdRenderComp->MarkRenderStateDirty();
#endif
		}
		else if (ChangedCategory == TEXT("Pathfinding"))
		{
			if (ChangedPropName == NAME_OtherActor)
			{
				if (OtherActor != NULL)
				{
					ANavigationTestingActor* OtherActorsOldOtherActor = OtherActor->OtherActor;

					OtherActor->OtherActor = this;
					bSearchStart = !OtherActor->bSearchStart;

#if WITH_EDITORONLY_DATA
					if (bSearchStart)
					{
						OtherActor->EdRenderComp->MarkRenderStateDirty();
					}
					else
					{
						EdRenderComp->MarkRenderStateDirty();
					}
#endif

					if (OtherActorsOldOtherActor)
					{
						OtherActorsOldOtherActor->OtherActor = NULL;
						OtherActorsOldOtherActor->LastPath.Reset();
#if WITH_EDITORONLY_DATA
						OtherActorsOldOtherActor->EdRenderComp->MarkRenderStateDirty();
#endif
					}
				}
			}
			else if (ChangedPropName == NAME_IsSearchStart)
			{
				if (OtherActor != NULL)
				{
					OtherActor->bSearchStart = !bSearchStart;
				}
			}

			UpdatePathfinding();
		}
	}
}

void ANavigationTestingActor::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	// project location to navmesh
	FNavLocation NavLoc;
	bProjectedLocationValid = GetWorld()->GetNavigationSystem()->ProjectPointToNavigation(GetActorLocation(), NavLoc, QueryingExtent, MyNavData);
	ProjectedLocation = NavLoc.Location;

	if (bSearchStart || (OtherActor != NULL && OtherActor->bSearchStart))
	{
		UpdatePathfinding();
	}
}

void ANavigationTestingActor::PostLoad()
{
	Super::PostLoad();

#if WITH_RECAST && WITH_EDITORONLY_DATA
	if (GIsEditor)
	{
		TickHelper = new FNavTestTickHelper();
		TickHelper->Owner = this;
	}
#endif

	bHidden = !bShouldBeVisibleInGame;
}

void ANavigationTestingActor::TickMe()
{
	UNavigationSystem* NavSys = GetWorld() ? GetWorld()->GetNavigationSystem() : NULL;
	if (NavSys
#if WITH_NAVIGATION_GENERATOR
		&& !NavSys->IsNavigationBuildInProgress()
#endif
		)
	{
#if WITH_RECAST && WITH_EDITORONLY_DATA
		delete TickHelper;
		TickHelper = NULL;
#endif

		UpdatePathfinding();
	}
}

#endif // WITH_EDITOR

void ANavigationTestingActor::OnPathUpdated(INavigationPathGenerator* PathGenerator)
{

}

FVector ANavigationTestingActor::GetNavAgentLocation() const
{ 
	return GetActorLocation(); 
}

void ANavigationTestingActor::UpdateNavData()
{
	if (!MyNavData && GetWorld() && GetWorld()->GetNavigationSystem())
	{
		MyNavData = GetWorld()->GetNavigationSystem()->GetNavDataForProps(NavAgentProps);
	}
}

void ANavigationTestingActor::UpdatePathfinding()
{
	PathfindingTime = 0.0f;
	PathCost = 0.0f;
	bPathSearchOutOfNodes = false;
	bPathIsPartial = false;
	bPathExist = false;
	LastPath.Reset();
	ShowStepIndex = -1;
	PathfindingSteps = 0;
#if WITH_EDITORONLY_DATA
	DebugSteps.Reset();
#endif
	UpdateNavData();

	if (bSearchStart == false && (OtherActor == NULL || OtherActor->bSearchStart == false))
	{
#if WITH_EDITORONLY_DATA
		if (EdRenderComp)
		{
			EdRenderComp->MarkRenderStateDirty();
		}
#endif // WITH_EDITORONLY_DATA
		return;
	}

	if (OtherActor == NULL)
	{
		ANavigationTestingActor* AlternativeOtherActor = NULL;

		for (TActorIterator<ANavigationTestingActor> It(GetWorld()); It; ++It)
		{
			ANavigationTestingActor* TestActor = *It;
			if (TestActor && TestActor != this)
			{
				if( TestActor->OtherActor == this )
				{
					OtherActor = TestActor;
					break;
				}
				// the other one doesn't have anything set - potential end for us
				else if (bSearchStart && TestActor->OtherActor == NULL)
				{
					AlternativeOtherActor = TestActor;
				}
			}
		}

		// if still empty maybe AlternativeOtherActor can fill in
		if (OtherActor == NULL && AlternativeOtherActor != NULL)
		{
			OtherActor = AlternativeOtherActor;
			AlternativeOtherActor->OtherActor = this;
		}
	}

	if (OtherActor)
	{
		if (bSearchStart)
		{
			SearchPathTo(OtherActor);
		}
		else if (OtherActor)
		{
			OtherActor->SearchPathTo(this);
		}
	}
}

void ANavigationTestingActor::SearchPathTo(class ANavigationTestingActor* Goal)
{
#if WITH_EDITORONLY_DATA
	if (EdRenderComp)
	{
		EdRenderComp->MarkRenderStateDirty();
	}
#endif // WITH_EDITORONLY_DATA

	if (Goal == NULL) 
	{
		return;
	}

	UNavigationSystem* NavSys = GetWorld()->GetNavigationSystem();
	check(NavSys);

	const double StartTime = FPlatformTime::Seconds();

	FPathFindingQuery Query = BuildPathFindingQuery(Goal);
	EPathFindingMode::Type Mode = bUseHierarchicalPathfinding ? EPathFindingMode::Hierarchical : EPathFindingMode::Regular;
	FPathFindingResult Result = NavSys->FindPathSync(NavAgentProps, Query, Mode);

	const double EndTime = FPlatformTime::Seconds();
	const float Duration = (EndTime - StartTime);
	PathfindingTime = Duration * 1000000.0f;			// in micro seconds [us]
	bPathIsPartial = Result.IsPartial();
	bPathExist = Result.IsSuccessful() || Result.IsPartial();
	bPathSearchOutOfNodes = bPathExist ? Result.Path->DidSearchReachedLimit() : false;
	LastPath = Result.Path;
	PathCost = bPathExist ? Result.Path->GetCost() : 0.0f;

	if (OffsetFromCornersDistance > 0.0f)
	{
		((FNavMeshPath*)LastPath.Get())->OffsetFromCorners(OffsetFromCornersDistance);
	}

#if WITH_RECAST && WITH_EDITORONLY_DATA
	if (bGatherDetailedInfo && !bUseHierarchicalPathfinding)
	{
		ARecastNavMesh* RecastNavMesh = Cast<ARecastNavMesh>(MyNavData);
		if (RecastNavMesh)
		{
			PathfindingSteps = RecastNavMesh->DebugPathfinding(Query, DebugSteps);
		}
	}
#endif
}

FPathFindingQuery ANavigationTestingActor::BuildPathFindingQuery(const ANavigationTestingActor* Goal) const
{
	check(Goal);
	return FPathFindingQuery(this, MyNavData, GetNavAgentLocation(), Goal->GetNavAgentLocation(), UNavigationQueryFilter::GetQueryFilter(MyNavData, FilterClass));
}

