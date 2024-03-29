// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "Paper2DPrivatePCH.h"
#include "PaperBatchManager.h"
#include "PaperBatchComponent.h"
#include "PaperBatchSceneProxy.h"

namespace FPaperBatchManagerNS
{
	static TMap< UWorld*, UPaperBatchComponent* > WorldToBatcherMap;

	static FWorldDelegates::FWorldInitializationEvent::FDelegate OnWorldCreatedDelegate;
	static FWorldDelegates::FWorldEvent::FDelegate OnWorldDestroyedDelegate;
}

//////////////////////////////////////////////////////////////////////////
// FPaperBatchManager

void FPaperBatchManager::Initialize()
{
	FPaperBatchManagerNS::OnWorldCreatedDelegate = FWorldDelegates::FWorldInitializationEvent::FDelegate::CreateStatic(&FPaperBatchManager::OnWorldCreated);
	FWorldDelegates::OnPostWorldInitialization.Add(FPaperBatchManagerNS::OnWorldCreatedDelegate);

	FPaperBatchManagerNS::OnWorldDestroyedDelegate = FWorldDelegates::FWorldEvent::FDelegate::CreateStatic(&FPaperBatchManager::OnWorldDestroyed);
	FWorldDelegates::OnPreWorldFinishDestroy.Add(FPaperBatchManagerNS::OnWorldDestroyedDelegate);
}

void FPaperBatchManager::Shutdown()
{
	FWorldDelegates::OnPostWorldInitialization.Remove(FPaperBatchManagerNS::OnWorldCreatedDelegate);
	FWorldDelegates::OnPreWorldFinishDestroy.Remove(FPaperBatchManagerNS::OnWorldDestroyedDelegate);
}

void FPaperBatchManager::OnWorldCreated(UWorld* World, const UWorld::InitializationValues IVS)
{
	UPaperBatchComponent* Batcher = NewObject<UPaperBatchComponent>();
	Batcher->UpdateBounds();
	Batcher->AddToRoot();
	FPaperBatchManagerNS::WorldToBatcherMap.Add(World, Batcher);

	if (IVS.bInitializeScenes)
	{
		Batcher->RegisterComponentWithWorld(World);
	}
}

void FPaperBatchManager::OnWorldDestroyed(UWorld* World)
{
	UPaperBatchComponent* Batcher = FPaperBatchManagerNS::WorldToBatcherMap.FindChecked(World);

	if (Batcher->IsRegistered())
	{
		Batcher->UnregisterComponent();
	}

	FPaperBatchManagerNS::WorldToBatcherMap.Remove(World);
	Batcher->RemoveFromRoot();
}

UPaperBatchComponent* FPaperBatchManager::GetBatchComponent(UWorld* World)
{
	UPaperBatchComponent* Batcher = FPaperBatchManagerNS::WorldToBatcherMap.FindChecked(World);
	check(Batcher);
	return Batcher;
}

FPaperBatchSceneProxy* FPaperBatchManager::GetBatcher(FSceneInterface* Scene)
{
	UPaperBatchComponent* BatchComponent = GetBatchComponent(Scene->GetWorld());
	FPaperBatchSceneProxy* BatchProxy = static_cast<FPaperBatchSceneProxy*>(BatchComponent->SceneProxy);
	check(BatchProxy);
	return BatchProxy;
}
