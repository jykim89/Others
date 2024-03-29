// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

UKismetAIAsyncTaskProxy::UKismetAIAsyncTaskProxy(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

void UKismetAIAsyncTaskProxy::OnMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type MovementResult)
{
	if (RequestID.IsEquivalent(MoveRequestId) && AIController.IsValid())
	{
		if (!AIController->IsPendingKill() && AIController->ReceiveMoveCompleted.IsBound())
		{
			AIController->ReceiveMoveCompleted.RemoveDynamic(this, &UKismetAIAsyncTaskProxy::OnMoveCompleted);
		}

		if (MovementResult == EPathFollowingResult::Success)
		{
			OnSuccess.Broadcast(MovementResult);
		}
		else
		{
			OnFail.Broadcast(MovementResult);
		}
	}
}

void UKismetAIAsyncTaskProxy::OnNoPath()
{
	OnFail.Broadcast(EPathFollowingResult::Aborted);
}

void UKismetAIAsyncTaskProxy::BeginDestroy()
{
	if (AIController.IsValid() && !AIController->IsPendingKill() && AIController->ReceiveMoveCompleted.IsBound())
	{
		AIController->ReceiveMoveCompleted.RemoveDynamic(this, &UKismetAIAsyncTaskProxy::OnMoveCompleted);
	}

	Super::BeginDestroy();
}

UKismetAIHelperLibrary::UKismetAIHelperLibrary(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

class UKismetAIAsyncTaskProxy* UKismetAIHelperLibrary::CreateMoveToProxyObject(class UObject* WorldContextObject, APawn* Pawn, FVector Destination, AActor* TargetActor, float AcceptanceRadius, bool bStopOnOverlap)
{
	check(WorldContextObject);
	if (!Pawn)
	{
		return NULL;
	}
	UKismetAIAsyncTaskProxy* MyObj = NULL;
	AAIController* AIController = Cast<AAIController>(Pawn->GetController());
	if (AIController)
	{
		MyObj = NewObject<UKismetAIAsyncTaskProxy>();
		FNavPathSharedPtr Path = TargetActor ? AIController->FindPath(TargetActor, true) : AIController->FindPath(Destination, true);
		if (Path.IsValid())
		{
			MyObj->AIController = AIController;
			MyObj->AIController->ReceiveMoveCompleted.AddDynamic(MyObj, &UKismetAIAsyncTaskProxy::OnMoveCompleted);
			MyObj->MoveRequestId = MyObj->AIController->RequestMove(Path, TargetActor, AcceptanceRadius, bStopOnOverlap);
		}
		else
		{
			UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
			World->GetTimerManager().SetTimer(MyObj, &UKismetAIAsyncTaskProxy::OnNoPath, 0.1, false);
		}
	}
	return MyObj;
}

void UKismetAIHelperLibrary::SendAIMessage(APawn* Target, FName Message, UObject* MessageSource, bool bSuccess)
{
	FAIMessage::Send(Target, FAIMessage(Message, MessageSource, bSuccess));
}

APawn* UKismetAIHelperLibrary::SpawnAIFromClass(class UObject* WorldContextObject, TSubclassOf<APawn> PawnClass, class UBehaviorTree* BehaviorTree, FVector Location, FRotator Rotation, bool bNoCollisionFail)
{
	APawn* NewPawn = NULL;

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (World && *PawnClass)
	{
		FActorSpawnParameters ActorSpawnParams;
		ActorSpawnParams.bNoCollisionFail = bNoCollisionFail;
		NewPawn = World->SpawnActor<APawn>(*PawnClass, Location, Rotation, ActorSpawnParams);

		if (NewPawn != NULL)
		{
			if (NewPawn->Controller == NULL)
			{	// NOTE: SpawnDefaultController ALSO calls Possess() to possess the pawn (if a controller is successfully spawned).
				NewPawn->SpawnDefaultController();
			}

			if (BehaviorTree != NULL)
			{
				AAIController* AIController = Cast<AAIController>(NewPawn->Controller);

				if (AIController != NULL)
				{
					AIController->RunBehaviorTree(BehaviorTree);
				}
			}
		}
	}

	return NewPawn;
}

APawn* UKismetAIHelperLibrary::SpawnAI(class UObject* WorldContextObject, UBlueprint* Pawn, class UBehaviorTree* BehaviorTree, FVector Location, FRotator Rotation, bool bNoCollisionFail)
{
	APawn* NewPawn = NULL;

	const bool bGoodBPGeneratedClass = Pawn && Pawn->GeneratedClass && Pawn->GeneratedClass->IsChildOf(APawn::StaticClass());
	if (bGoodBPGeneratedClass)
	{
		NewPawn = SpawnAIFromClass(WorldContextObject, *(Pawn->GeneratedClass), BehaviorTree, Location, Rotation, bNoCollisionFail);
	}

	return NewPawn;
}

UBlackboardComponent* UKismetAIHelperLibrary::GetBlackboard(AActor* Target)
{
	UBlackboardComponent* BlackboardComp = NULL;

	APawn* TargetPawn = Cast<APawn>(Target);
	if (TargetPawn && TargetPawn->GetController())
	{
		BlackboardComp = TargetPawn->GetController()->FindComponentByClass<UBlackboardComponent>();
	}

	if (BlackboardComp == NULL && Target)
	{
		BlackboardComp = Target->FindComponentByClass<UBlackboardComponent>();
	}

	return BlackboardComp;
}
