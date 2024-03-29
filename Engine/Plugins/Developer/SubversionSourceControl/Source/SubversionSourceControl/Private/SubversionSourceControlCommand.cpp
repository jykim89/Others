// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SubversionSourceControlPrivatePCH.h"
#include "SubversionSourceControlCommand.h"
#include "SubversionSourceControlModule.h"
#include "SubversionSourceControlProvider.h"
#include "ISubversionSourceControlWorker.h"
#include "SSubversionSourceControlSettings.h"

FSubversionSourceControlCommand::FSubversionSourceControlCommand(const TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe>& InOperation, const TSharedRef<class ISubversionSourceControlWorker, ESPMode::ThreadSafe>& InWorker, const FSourceControlOperationComplete& InOperationCompleteDelegate )
	: Operation(InOperation)
	, Worker(InWorker)
	, OperationCompleteDelegate(InOperationCompleteDelegate)
	, bExecuteProcessed(0)
	, bCommandSuccessful(false)
	, bAutoDelete(true)
	, Concurrency(EConcurrency::Synchronous)
{
	// grab the providers settings here, so we don't access them once the worker thread is launched
	check(IsInGameThread());
	FSubversionSourceControlModule& SubversionSourceControl = FModuleManager::LoadModuleChecked<FSubversionSourceControlModule>( "SubversionSourceControl" );
	FSubversionSourceControlProvider& Provider = SubversionSourceControl.GetProvider();
	RepositoryName = Provider.GetRepositoryName();
	UserName = Provider.GetUserName();
	WorkingCopyRoot = Provider.GetWorkingCopyRoot();

	// password needs to be gotten straight from the input UI, its not stored anywhere else
	if(SSubversionSourceControlSettings::GetPassword().Len() > 0)
	{
		Password = SSubversionSourceControlSettings::GetPassword();
	}
}

bool FSubversionSourceControlCommand::DoWork()
{
	bCommandSuccessful = Worker->Execute(*this);
	FPlatformAtomics::InterlockedExchange(&bExecuteProcessed, 1);

	return bCommandSuccessful;
}

void FSubversionSourceControlCommand::Abandon()
{
	FPlatformAtomics::InterlockedExchange(&bExecuteProcessed, 1);
}

void FSubversionSourceControlCommand::DoThreadedWork()
{
	Concurrency = EConcurrency::Asynchronous;
	DoWork();
}
