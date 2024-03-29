// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "LatentActions.h"

/////////////////////////////////////////////////////
// FPendingLatentAction

#if WITH_EDITOR
FString FPendingLatentAction::GetDescription() const
{
	return TEXT("Not implemented");
}
#endif

/////////////////////////////////////////////////////
// FLatentActionManager

void FLatentActionManager::RemoveActionsForObject(TWeakObjectPtr<UObject> InObject)
{
	ObjectsToRemove.Add(InObject);
}

void FLatentActionManager::ProcessLatentActions(UObject* InObject, float DeltaTime)
{
	if (ObjectsToRemove.Num())
	{
		for (const auto Key : ObjectsToRemove)
		{
			FActionList* const ObjectActionList = ObjectToActionListMap.Find(Key);
			if (ObjectActionList)
			{
				for (TMultiMap<int32, FPendingLatentAction*>::TConstIterator It(*ObjectActionList); It; ++It)
				{
					FPendingLatentAction* Action = It.Value();
					Action->NotifyActionAborted();
					delete Action;
				}
				ObjectActionList->Empty();

				ObjectToActionListMap.Remove(Key); //TODO: we should be able to obtain an Iterator from Map::Find function. "Iterator.RemoveCurrent();" should be called here.
			}
		}
		ObjectsToRemove.Empty();
	}

	//@TODO: K2: Very inefficient code right now
	if (InObject != NULL)
	{
		if (!ProcessedThisFrame.Contains(InObject))
		{
			if (FActionList* ObjectActionList = ObjectToActionListMap.Find(InObject))
			{
				TickLatentActionForObject(DeltaTime, *ObjectActionList, InObject);
				ProcessedThisFrame.Add(InObject);
			}
		}
	}
	else 
	{
		for (FObjectToActionListMap::TIterator ObjIt(ObjectToActionListMap); ObjIt; ++ObjIt)
		{	
			UObject* Object = ObjIt.Key().Get();
			FActionList& ObjectActionList = ObjIt.Value();

			if (Object != NULL)
			{
				// Tick all outstanding actions for this object
				if (ObjectActionList.Num() > 0)
				{
					if (!ProcessedThisFrame.Contains(Object))
					{
						TickLatentActionForObject(DeltaTime, ObjectActionList, Object);
						ProcessedThisFrame.Add(Object);
					}
				}
			}
			else
			{
				// Terminate all outstanding actions for this object, which has been GCed
				for (TMultiMap<int32, FPendingLatentAction*>::TConstIterator It(ObjectActionList); It; ++It)
				{
					FPendingLatentAction* Action = It.Value();
					Action->NotifyObjectDestroyed();
					delete Action;
				}
				ObjectActionList.Empty();
			}

			// Remove the entry if there are no pending actions remaining for this object (or if the object was NULLed and cleaned up)
			if (ObjectActionList.Num() == 0)
			{
				ObjIt.RemoveCurrent();
			}
		}
	}
}

void FLatentActionManager::TickLatentActionForObject(float DeltaTime, FActionList& ObjectActionList, UObject* InObject)
{
	typedef TPair<int32, FPendingLatentAction*> FActionListPair;
	TArray<FActionListPair, TInlineAllocator<4>> ItemsToRemove;
	
	FLatentResponse Response(DeltaTime);
	for (TMultiMap<int32, FPendingLatentAction*>::TConstIterator It(ObjectActionList); It; ++It)
	{
		FPendingLatentAction* Action = It.Value();

		Response.bRemoveAction = false;

		Action->UpdateOperation(Response);

		if (Response.bRemoveAction)
		{
			new (ItemsToRemove) FActionListPair(TPairInitializer<int32, FPendingLatentAction*>(It.Key(), Action));
		}
	}

	// Remove any items that were deleted
	for (int32 i = 0; i < ItemsToRemove.Num(); ++i)
	{
		const FActionListPair& ItemPair = ItemsToRemove[i];
		const int32 ItemIndex = ItemPair.Key;
		FPendingLatentAction* DyingAction = ItemPair.Value;
		ObjectActionList.Remove(ItemIndex, DyingAction);
		delete DyingAction;
	}

	// Trigger any pending execution links
	for (int32 i = 0; i < Response.LinksToExecute.Num(); ++i)
	{
		FLatentResponse::FExecutionInfo& LinkInfo = Response.LinksToExecute[i];
		if (LinkInfo.LinkID != INDEX_NONE)
		{
			if (UObject* CallbackTarget = LinkInfo.CallbackTarget.Get())
			{
				check(CallbackTarget == InObject);

				if (UFunction* ExecutionFunction = CallbackTarget->FindFunction(LinkInfo.ExecutionFunction))
				{
					CallbackTarget->ProcessEvent(ExecutionFunction, &(LinkInfo.LinkID));
				}
				else
				{
					UE_LOG(LogScript, Warning, TEXT("FLatentActionManager::ProcessLatentActions: Could not find latent action resume point named '%s' on '%s' called by '%s'"),
						*LinkInfo.ExecutionFunction.ToString(), *(CallbackTarget->GetPathName()), *(InObject->GetPathName()));
				}
			}
			else
			{
				UE_LOG(LogScript, Warning, TEXT("FLatentActionManager::ProcessLatentActions: CallbackTarget is None."));
			}
		}
	}
}

#if WITH_EDITOR


FString FLatentActionManager::GetDescription(UObject* InObject, int32 UUID) const
{
	check(InObject);

	FString Description = *NSLOCTEXT("LatentActionManager", "NoPendingActions", "No Pending Actions").ToString();

	const FLatentActionManager::FActionList* ObjectActionList = GetActionListForObject(InObject);
	if ((ObjectActionList != NULL) && (ObjectActionList->Num() > 0))
	{	
		TArray<FPendingLatentAction*> Actions;
		ObjectActionList->MultiFind(UUID, Actions);

		const int32 PendingActions = Actions.Num();

		// See if there are pending actions
		if (PendingActions > 0)
		{
			FPendingLatentAction* PrimaryAction = Actions[0];
			FString ActionDesc = PrimaryAction->GetDescription();

			Description = (PendingActions > 1)
				? FString::Printf( *NSLOCTEXT("LatentActionManager", "NumPendingActions", "%d Pending Actions: %s").ToString(), PendingActions, *ActionDesc)
				: ActionDesc;
		}
	}
	return Description;
}
void FLatentActionManager::GetActiveUUIDs(UObject* InObject, TSet<int32>& UUIDList) const
{
	check(InObject);

	const FLatentActionManager::FActionList* ObjectActionList = GetActionListForObject(InObject);
	if ((ObjectActionList != NULL) && (ObjectActionList->Num() > 0))
	{
		for (TMultiMap<int32, FPendingLatentAction*>::TConstIterator It(*ObjectActionList); It; ++It)
		{
			UUIDList.Add(It.Key());
		}
	}
}

#endif