// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "PerforceSourceControlPrivatePCH.h"
#include "PerforceSourceControlProvider.h"
#include "PerforceSourceControlCommand.h"
#include "PerforceSourceControlLabel.h"
#include "PerforceConnection.h"
#include "IPerforceSourceControlWorker.h"
#include "PerforceSourceControlModule.h"
#include "PerforceSourceControlSettings.h"
#include "SPerforceSourceControlSettings.h"
#include "MessageLog.h"
#include "ScopedSourceControlProgress.h"

static FName ProviderName("Perforce");

#define LOCTEXT_NAMESPACE "PerforceSourceControl"

/** Init of connection with source control server */
void FPerforceSourceControlProvider::Init(bool bForceConnection)
{
	LoadSSLLibraries();
	ParseCommandLineSettings(bForceConnection);
}

/** API Specific close the connection with source control server*/
void FPerforceSourceControlProvider::Close()
{
	if ( PersistentConnection )
	{
		PersistentConnection->Disconnect();
		delete PersistentConnection;
		PersistentConnection = NULL;
	}

	// clear the cache
	StateCache.Empty();

	bServerAvailable = false;

	UnloadSSLLibraries();
}

TSharedRef<FPerforceSourceControlState, ESPMode::ThreadSafe> FPerforceSourceControlProvider::GetStateInternal(const FString& Filename)
{
	TSharedRef<FPerforceSourceControlState, ESPMode::ThreadSafe>* State = StateCache.Find(Filename);
	if(State != NULL)
	{
		// found cached item
		return (*State);
	}
	else
	{
		// cache an unknown state for this item
		TSharedRef<FPerforceSourceControlState, ESPMode::ThreadSafe> NewState = MakeShareable( new FPerforceSourceControlState(Filename) );
		StateCache.Add(Filename, NewState);
		return NewState;
	}
}

FText FPerforceSourceControlProvider::GetStatusText() const
{
	FPerforceSourceControlModule& PerforceSourceControl = FModuleManager::LoadModuleChecked<FPerforceSourceControlModule>( "PerforceSourceControl" );
	const FPerforceSourceControlSettings& Settings = PerforceSourceControl.AccessSettings();

	FFormatNamedArguments Args;
	Args.Add( TEXT("IsEnabled"), IsEnabled() ? LOCTEXT("Yes", "Yes") : LOCTEXT("No", "No") );
	Args.Add( TEXT("IsConnected"), (IsEnabled() && IsAvailable()) ? LOCTEXT("Yes", "Yes") : LOCTEXT("No", "No") );
	Args.Add( TEXT("PortNumber"), FText::FromString( Settings.GetPort() ) );
	Args.Add( TEXT("UserName"), FText::FromString( Settings.GetUserName() ) );
	Args.Add( TEXT("ClientSpecName"), FText::FromString( Settings.GetWorkspace() ) );

	return FText::Format( LOCTEXT("PerforceStatusText", "Enabled: {IsEnabled}\nConnected: {IsConnected}\n\nPort: {PortNumber}\nUser name: {UserName}\nClient name: {ClientSpecName}"), Args );
}

bool FPerforceSourceControlProvider::IsEnabled() const
{
	return true;
}

bool FPerforceSourceControlProvider::IsAvailable() const
{
	return bServerAvailable;
}

bool FPerforceSourceControlProvider::EstablishPersistentConnection()
{
	FPerforceSourceControlModule& PerforceSourceControl = FModuleManager::LoadModuleChecked<FPerforceSourceControlModule>( "PerforceSourceControl" );
	FPerforceConnectionInfo ConnectionInfo = PerforceSourceControl.AccessSettings().GetConnectionInfo();

	bool bIsValidConnection = false;
	if ( !PersistentConnection )
	{
		PersistentConnection = new FPerforceConnection(ConnectionInfo);
	}

	bIsValidConnection = PersistentConnection->IsValidConnection();
	if ( !bIsValidConnection )
	{
		delete PersistentConnection;
		PersistentConnection = new FPerforceConnection(ConnectionInfo);
		bIsValidConnection = PersistentConnection->IsValidConnection();
	}

	bServerAvailable = bIsValidConnection;
	return bIsValidConnection;
}

/**
 * Loads user/SCC information from the command line or INI file.
 */
void FPerforceSourceControlProvider::ParseCommandLineSettings(bool bForceConnection)
{
	ISourceControlModule& SourceControlModule = FModuleManager::LoadModuleChecked<ISourceControlModule>( "SourceControl" );
	FPerforceSourceControlModule& PerforceSourceControl = FModuleManager::GetModuleChecked<FPerforceSourceControlModule>( "PerforceSourceControl" );

	bool bFoundCmdLineSettings = false;

	// Check command line for any overridden settings
	FString PortName = PerforceSourceControl.AccessSettings().GetPort();
	FString UserName = PerforceSourceControl.AccessSettings().GetUserName();
	FString ClientSpecName = PerforceSourceControl.AccessSettings().GetWorkspace();
	FString HostOverrideName = PerforceSourceControl.AccessSettings().GetHostOverride();
	bFoundCmdLineSettings = FParse::Value(FCommandLine::Get(), TEXT("P4Port="), PortName);
	bFoundCmdLineSettings |= FParse::Value(FCommandLine::Get(), TEXT("P4User="), UserName);
	bFoundCmdLineSettings |= FParse::Value(FCommandLine::Get(), TEXT("P4Client="), ClientSpecName);
	bFoundCmdLineSettings |= FParse::Value(FCommandLine::Get(), TEXT("P4Host="), HostOverrideName);
	bFoundCmdLineSettings |= FParse::Value(FCommandLine::Get(), TEXT("P4Passwd="), Ticket);

	if(bFoundCmdLineSettings)
	{
		PerforceSourceControl.AccessSettings().SetPort(PortName);
		PerforceSourceControl.AccessSettings().SetUserName(UserName);
		PerforceSourceControl.AccessSettings().SetWorkspace(ClientSpecName);
		PerforceSourceControl.AccessSettings().SetHostOverride(HostOverrideName);
	}
	
	if (bForceConnection)
	{
		FPerforceConnectionInfo ConnectionInfo = PerforceSourceControl.AccessSettings().GetConnectionInfo();
		if(FPerforceConnection::EnsureValidConnection(PortName, UserName, ClientSpecName, ConnectionInfo))
		{
			PerforceSourceControl.AccessSettings().SetPort(PortName);
			PerforceSourceControl.AccessSettings().SetUserName(UserName);
			PerforceSourceControl.AccessSettings().SetWorkspace(ClientSpecName);
			PerforceSourceControl.AccessSettings().SetHostOverride(HostOverrideName);		
		}

		bServerAvailable = true;
	}

	//Save off settings so this doesn't happen every time
	PerforceSourceControl.SaveSettings();
}


void FPerforceSourceControlProvider::GetWorkspaceList(const FPerforceConnectionInfo& InConnectionInfo, TArray<FString>& OutWorkspaceList, TArray<FText>& OutErrorMessages)
{
	//attempt to ask perforce for a list of client specs that belong to this user
	FPerforceConnection Connection(InConnectionInfo);
	Connection.GetWorkspaceList(InConnectionInfo, FOnIsCancelled(), OutWorkspaceList, OutErrorMessages);
}

const FString& FPerforceSourceControlProvider::GetTicket() const 
{ 
	return Ticket; 
}

const FName& FPerforceSourceControlProvider::GetName() const
{
	return ProviderName;
}

ECommandResult::Type FPerforceSourceControlProvider::GetState( const TArray<FString>& InFiles, TArray< TSharedRef<ISourceControlState, ESPMode::ThreadSafe> >& OutState, EStateCacheUsage::Type InStateCacheUsage )
{
	if(!IsEnabled())
	{
		return ECommandResult::Failed;
	}

	TArray<FString> AbsoluteFiles;
	for( TArray<FString>::TConstIterator It(InFiles); It; It++)
	{
		AbsoluteFiles.Add(FPaths::ConvertRelativePathToFull(*It));
	}

	if(InStateCacheUsage == EStateCacheUsage::ForceUpdate)
	{
		Execute(ISourceControlOperation::Create<FUpdateStatus>(), AbsoluteFiles);
	}

	for( TArray<FString>::TConstIterator It(AbsoluteFiles); It; It++)
	{
		TSharedRef<FPerforceSourceControlState, ESPMode::ThreadSafe>* State = StateCache.Find(*It);
		if(State != NULL)
		{
			// found cached item for this file, return that
			OutState.Add(*State);
		}
		else
		{
			// cache an unknown state for this item & return that
			TSharedRef<FPerforceSourceControlState, ESPMode::ThreadSafe> NewState = MakeShareable( new FPerforceSourceControlState(*It) );
			StateCache.Add(*It, NewState);
			OutState.Add(NewState);
		}
	}

	return ECommandResult::Succeeded;
}

void FPerforceSourceControlProvider::RegisterSourceControlStateChanged( const FSourceControlStateChanged::FDelegate& SourceControlStateChanged )
{
	OnSourceControlStateChanged.Add( SourceControlStateChanged );
}

void FPerforceSourceControlProvider::UnregisterSourceControlStateChanged( const FSourceControlStateChanged::FDelegate& SourceControlStateChanged )
{
	OnSourceControlStateChanged.Remove( SourceControlStateChanged );
}

ECommandResult::Type FPerforceSourceControlProvider::Execute( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation, const TArray<FString>& InFiles, EConcurrency::Type InConcurrency, const FSourceControlOperationComplete& InOperationCompleteDelegate )
{
	if(!IsEnabled())
	{
		return ECommandResult::Failed;
	}

	TArray<FString> AbsoluteFiles;
	for( TArray<FString>::TConstIterator It(InFiles); It; It++)
	{
		AbsoluteFiles.Add(FPaths::ConvertRelativePathToFull(*It));
	}

	// Query to see if the we allow this operation
	TSharedPtr<IPerforceSourceControlWorker, ESPMode::ThreadSafe> Worker = CreateWorker(InOperation->GetName());
	if(!Worker.IsValid())
	{
		// this operation is unsupported by this source control provider
		FFormatNamedArguments Arguments;
		Arguments.Add( TEXT("OperationName"), FText::FromName(InOperation->GetName()) );
		Arguments.Add( TEXT("ProviderName"), FText::FromName(GetName()) );
		FMessageLog("SourceControl").Error(FText::Format(LOCTEXT("UnsupportedOperation", "Operation '{OperationName}' not supported by source control provider '{ProviderName}'"), Arguments));
		return ECommandResult::Failed;
	}

	// fire off operation
	if(InConcurrency == EConcurrency::Synchronous)
	{
		FPerforceSourceControlCommand* Command = new FPerforceSourceControlCommand(InOperation, Worker.ToSharedRef());
		Command->bAutoDelete = false;
		Command->Files = AbsoluteFiles;
		Command->OperationCompleteDelegate = InOperationCompleteDelegate;
		return ExecuteSynchronousCommand(*Command, InOperation->GetInProgressString(), true);
	}
	else
	{
		FPerforceSourceControlCommand* Command = new FPerforceSourceControlCommand(InOperation, Worker.ToSharedRef());
		Command->bAutoDelete = true;
		Command->Files = AbsoluteFiles;
		Command->OperationCompleteDelegate = InOperationCompleteDelegate;
		return IssueCommand(*Command, false);
	}
}

bool FPerforceSourceControlProvider::CanCancelOperation( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation ) const
{
	for (int32 CommandIndex = 0; CommandIndex < CommandQueue.Num(); ++CommandIndex)
	{
		FPerforceSourceControlCommand& Command = *CommandQueue[CommandIndex];
		if (Command.Operation == InOperation)
		{
			check(Command.bAutoDelete);
			return true;
		}
	}

	// operation was not in progress!
	return false;
}

void FPerforceSourceControlProvider::CancelOperation( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation )
{
	for (int32 CommandIndex = 0; CommandIndex < CommandQueue.Num(); ++CommandIndex)
	{
		FPerforceSourceControlCommand& Command = *CommandQueue[CommandIndex];
		if (Command.Operation == InOperation)
		{
			check(Command.bAutoDelete);
			Command.Cancel();
			return;
		}
	}
}

bool FPerforceSourceControlProvider::UsesLocalReadOnlyState() const
{
	return true;
}

void FPerforceSourceControlProvider::OutputCommandMessages(const FPerforceSourceControlCommand& InCommand) const
{
	FMessageLog SourceControlLog("SourceControl");

	for (int32 ErrorIndex = 0; ErrorIndex < InCommand.ErrorMessages.Num(); ++ErrorIndex)
	{
		SourceControlLog.Error(InCommand.ErrorMessages[ErrorIndex]);
	}

	for (int32 InfoIndex = 0; InfoIndex < InCommand.InfoMessages.Num(); ++InfoIndex)
	{
		SourceControlLog.Info(InCommand.InfoMessages[InfoIndex]);
	}
}

void FPerforceSourceControlProvider::Tick()
{	
	bool bStatesUpdated = false;
	for (int32 CommandIndex = 0; CommandIndex < CommandQueue.Num(); ++CommandIndex)
	{
		FPerforceSourceControlCommand& Command = *CommandQueue[CommandIndex];
		if (Command.bExecuteProcessed)
		{
			// Remove command from the queue
			CommandQueue.RemoveAt(CommandIndex);

			// update connection state
			bServerAvailable = !Command.bConnectionDropped || Command.bCancelled;

			// let command update the states of any files
			bStatesUpdated |= Command.Worker->UpdateStates();

			// dump any messages to output log
			OutputCommandMessages(Command);

			// run the completion delegate if we have one bound
			ECommandResult::Type Result = ECommandResult::Failed;
			if(Command.bCommandSuccessful)
			{
				Result = ECommandResult::Succeeded;
			}
			else if(Command.bCancelled)
			{
				Result = ECommandResult::Cancelled;
			}
			Command.OperationCompleteDelegate.ExecuteIfBound(Command.Operation, Result);

			//commands that are left in the array during a tick need to be deleted
			if(Command.bAutoDelete)
			{
				// Only delete commands that are not running 'synchronously'
				delete &Command;
			}

			// only do one command per tick loop, as we dont want concurrent modification 
			// of the command queue (which can happen in the completion delegate)
			break;
		}
	}

	if(bStatesUpdated)
	{
		OnSourceControlStateChanged.Broadcast();
	}
}

static void ParseGetLabelsResults(const FP4RecordSet& InRecords, TArray< TSharedRef<ISourceControlLabel> >& OutLabels)
{
	// Iterate over each record found as a result of the command, parsing it for relevant information
	for (int32 Index = 0; Index < InRecords.Num(); ++Index)
	{
		const FP4Record& ClientRecord = InRecords[Index];
		FString LabelName = ClientRecord(TEXT("label"));
		if(LabelName.Len() > 0)
		{
			OutLabels.Add(MakeShareable( new FPerforceSourceControlLabel(LabelName) ) );
		}
	}
}

TArray< TSharedRef<ISourceControlLabel> > FPerforceSourceControlProvider::GetLabels( const FString& InMatchingSpec ) const
{
	TArray< TSharedRef<ISourceControlLabel> > Labels;

	FPerforceSourceControlModule& PerforceSourceControl = FModuleManager::LoadModuleChecked<FPerforceSourceControlModule>("PerforceSourceControl");
	FScopedPerforceConnection ScopedConnection(EConcurrency::Synchronous, PerforceSourceControl.AccessSettings().GetConnectionInfo());
	if(ScopedConnection.IsValid())
	{
		FPerforceConnection& Connection = ScopedConnection.GetConnection();
		FP4RecordSet Records;
		TArray<FString> Parameters;
		TArray<FText> ErrorMessages;
		Parameters.Add(TEXT("-E"));
		Parameters.Add(InMatchingSpec);
		bool bConnectionDropped = false;
		if(Connection.RunCommand(TEXT("labels"), Parameters, Records, ErrorMessages, FOnIsCancelled(), bConnectionDropped))
		{
			ParseGetLabelsResults(Records, Labels);
		}
		else
		{
			// output errors if any
			for (int32 ErrorIndex = 0; ErrorIndex < ErrorMessages.Num(); ++ErrorIndex)
			{
				FMessageLog("SourceControl").Warning(ErrorMessages[ErrorIndex]);
			}
		}
	}

	return Labels;
}

TSharedRef<class SWidget> FPerforceSourceControlProvider::MakeSettingsWidget() const
{
	return SNew(SPerforceSourceControlSettings);
}

TSharedPtr<IPerforceSourceControlWorker, ESPMode::ThreadSafe> FPerforceSourceControlProvider::CreateWorker(const FName& InOperationName) const
{
	const FGetPerforceSourceControlWorker* Operation = WorkersMap.Find(InOperationName);
	if(Operation != NULL)
	{
		return Operation->Execute();
	}
		
	return NULL;
}

void FPerforceSourceControlProvider::RegisterWorker( const FName& InName, const FGetPerforceSourceControlWorker& InDelegate )
{
	WorkersMap.Add( InName, InDelegate );
}

ECommandResult::Type FPerforceSourceControlProvider::ExecuteSynchronousCommand(FPerforceSourceControlCommand& InCommand, const FText& Task, bool bSuppressResponseMsg)
{
	ECommandResult::Type Result = ECommandResult::Failed;

	struct Local
	{
		static void CancelCommand(FPerforceSourceControlCommand* InControlCommand)
		{
			InControlCommand->Cancel();
		}
	};

	// Display the progress dialog
	FScopedSourceControlProgress Progress(Task, FSimpleDelegate::CreateStatic(&Local::CancelCommand, &InCommand));

	// Perform the command asynchronously
	IssueCommand( InCommand, false );

	// Wait until the queue is empty. Only at this point is our command guaranteed to be removed from the queue
	while(CommandQueue.Num() > 0)
	{
		// Tick the command queue and update progress.
		Tick();

		Progress.Tick();

		// Sleep for a bit so we don't busy-wait so much.
		FPlatformProcess::Sleep(0.01f);
	}

	if(InCommand.bCommandSuccessful)
	{
		Result = ECommandResult::Succeeded;
	}
	else if(InCommand.bCancelled)
	{
		Result = ECommandResult::Cancelled;
	}

	// If the command failed, inform the user that they need to try again
	if ( !InCommand.bCancelled && Result != ECommandResult::Succeeded && !bSuppressResponseMsg )
	{
		FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("Perforce_ServerUnresponsive", "Perforce server is unresponsive. Please check your connection and try again.") );
	}

	// Delete the command now
	check(!InCommand.bAutoDelete);
	delete &InCommand;

	return Result;
}

ECommandResult::Type FPerforceSourceControlProvider::IssueCommand(FPerforceSourceControlCommand& InCommand, const bool bSynchronous)
{
	if ( !bSynchronous && GThreadPool != NULL )
	{
		// Queue this to our worker thread(s) for resolving
		GThreadPool->AddQueuedWork(&InCommand);
		CommandQueue.Add(&InCommand);
		return ECommandResult::Succeeded;
	}
	else
	{
		InCommand.bCommandSuccessful = InCommand.DoWork();

		InCommand.Worker->UpdateStates();

		OutputCommandMessages(InCommand);

		// Callback now if present. When asynchronous, this callback gets called from Tick().
		ECommandResult::Type Result = InCommand.bCommandSuccessful ? ECommandResult::Succeeded : ECommandResult::Failed;
		InCommand.OperationCompleteDelegate.ExecuteIfBound(InCommand.Operation, Result);

		return Result;
	}
}

void FPerforceSourceControlProvider::LoadSSLLibraries()
{
#if PLATFORM_WINDOWS
#if PLATFORM_64BITS

#if _MSC_VER >= 1800
	const FString VSVersion = TEXT("VS2013/");
#else
	const FString VSVersion = TEXT("VS2012/");
#endif

	const FString PlatformString = TEXT("Win64");
	const FString RootOpenSSLPath = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/OpenSSL/") / PlatformString / VSVersion;

	FString DLLToLoad = RootOpenSSLPath + TEXT("libeay32.dll");
	Module_libeay32 = LoadLibraryW(*DLLToLoad);
	verifyf(Module_libeay32, TEXT("Failed to load DLL %s"), *DLLToLoad);
	DLLToLoad = RootOpenSSLPath + TEXT("ssleay32.dll");
	Module_ssleay32 = LoadLibraryW(*DLLToLoad);
	verifyf(Module_ssleay32, TEXT("Failed to load DLL %s"), *DLLToLoad);
#endif
#endif
}

void FPerforceSourceControlProvider::UnloadSSLLibraries()
{
#if PLATFORM_WINDOWS
	if(Module_libeay32)
	{
		FreeLibrary(Module_libeay32);
		Module_libeay32 = NULL;
	}

	if(Module_ssleay32)
	{
		FreeLibrary(Module_ssleay32);
		Module_ssleay32 = NULL;
	}
#endif
}

#undef LOCTEXT_NAMESPACE