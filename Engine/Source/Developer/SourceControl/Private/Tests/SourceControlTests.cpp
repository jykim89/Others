// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SourceControlPrivatePCH.h"
#include "AutomationTest.h"
#include "SourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlOperations.h"
#include "ISourceControlLabel.h"
#include "ISourceControlRevision.h"
#include "Developer/AssetTools/Public/AssetToolsModule.h"

#if WITH_EDITOR

/**
 * Helper class for receiving the results of async source control operations
 */
class FAsyncCommandHelper
{
public:
	FAsyncCommandHelper( const FString& InParameter = FString() )
		: Parameter(InParameter)
		, bDispatched(false)
		, bDone(false)
		, bSuccessful(false)
	{
	}

	void SourceControlOperationComplete( const FSourceControlOperationRef& Operation, ECommandResult::Type InResult )
	{
		bDone = true;
		bSuccessful = InResult == ECommandResult::Succeeded;
	}

	const FString& GetParameter() const
	{
		return Parameter;
	}

	bool IsDispatched() const
	{
		return bDispatched;
	}

	void SetDispatched()
	{
		bDispatched = true;
	}

	bool IsDone() const
	{
		return bDone;
	}

	bool IsSuccessful() const
	{
		return bSuccessful;
	}

private:
	/** Parameter we perform this operation with, if any */
	FString Parameter;

	/** Whether the async operation been issued */
	bool bDispatched;

	/** Whether the async operation has completed */
	bool bDone;

	/** Whether the operation was successful */
	bool bSuccessful;
};

static void GetProviders(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands)
{
	// we want to use all the providers we can find except 'None'
	FSourceControlModule& SourceControlModule = FModuleManager::LoadModuleChecked<FSourceControlModule>( "SourceControl" );
	int32 ProviderCount = SourceControlModule.GetNumSourceControlProviders();
	for(int32 ProviderIndex = 0; ProviderIndex < ProviderCount; ProviderIndex++)
	{
		FName ProviderName = SourceControlModule.GetSourceControlProviderName(ProviderIndex);
		if(ProviderName != TEXT("None"))
		{
			OutBeautifiedNames.Add(ProviderName.ToString());
		}
	}

	// commands are the same as names in this case
	OutTestCommands = OutBeautifiedNames;
}

static void AppendFilename(const FString& InFilename, TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands)
{
	// append the filename to the commands we have passed in
	for(auto Iter(OutBeautifiedNames.CreateIterator()); Iter; Iter++)
	{
		*Iter = FString::Printf(TEXT("%s (%s)"), *InFilename, **Iter);
	}

	for(auto Iter(OutTestCommands.CreateIterator()); Iter; Iter++)
	{
		*Iter += TEXT(" ");
		*Iter += InFilename;
	}
}

/**
 * Helper struct used to restore read-only state
 */
struct FReadOnlyState
{
	FReadOnlyState(const FString& InPackageName, bool bInReadOnly)
		: PackageName(InPackageName)
		, bReadOnly(bInReadOnly)
	{
	}

	FString PackageName;

	bool bReadOnly;
};

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FSetReadOnlyFlag, FReadOnlyState, ReadOnlyState);

bool FSetReadOnlyFlag::Update()
{
	FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*SourceControlHelpers::PackageFilename(ReadOnlyState.PackageName), ReadOnlyState.bReadOnly);
	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FSetProviderLatentCommand, FName, ProviderName);

bool FSetProviderLatentCommand::Update()
{
	// set to 'None' first so the provider is reinitialized
	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	SourceControlModule.SetProvider("None");
	SourceControlModule.SetProvider(ProviderName);
	if(SourceControlModule.GetProvider().GetName() != ProviderName || !SourceControlModule.IsEnabled())
	{
		UE_LOG(LogSourceControl, Error, TEXT("Could not set provider to '%s'"), *ProviderName.ToString());
	}
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FSetProviderTest, "Editor.Source Control.Set Provider", EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser)

void FSetProviderTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	GetProviders(OutBeautifiedNames, OutTestCommands);
}

bool FSetProviderTest::RunTest(const FString& Parameters)
{
	// parameter is the provider we want to use
	ADD_LATENT_AUTOMATION_COMMAND(FSetProviderLatentCommand(FName(*Parameters)));
	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FConnectLatentCommand, FAsyncCommandHelper, AsyncHelper);

bool FConnectLatentCommand::Update()
{
	// attempt a login and wait for the result
	if(!AsyncHelper.IsDispatched())
	{
		if(ISourceControlModule::Get().GetProvider().Login( FString(), EConcurrency::Asynchronous, FSourceControlOperationComplete::CreateRaw( &AsyncHelper, &FAsyncCommandHelper::SourceControlOperationComplete ) ) != ECommandResult::Succeeded)
		{
			return false;
		}
		AsyncHelper.SetDispatched();
	}

	return AsyncHelper.IsDone();
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FConnectTest, "Editor.Source Control.Connect", EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser)

void FConnectTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	GetProviders(OutBeautifiedNames, OutTestCommands);
}

bool FConnectTest::RunTest(const FString& Parameters)
{
	// parameter is the provider we want to use
	ADD_LATENT_AUTOMATION_COMMAND(FSetProviderLatentCommand(FName(*Parameters)));
	ADD_LATENT_AUTOMATION_COMMAND(FConnectLatentCommand(FAsyncCommandHelper()));

	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FRevertLatentCommand, FAsyncCommandHelper, AsyncHelper);

bool FRevertLatentCommand::Update()
{
	if(!AsyncHelper.IsDispatched())
	{
		if( ISourceControlModule::Get().GetProvider().Execute( 
			ISourceControlOperation::Create<FRevert>(), 
			SourceControlHelpers::PackageFilename(AsyncHelper.GetParameter()),
			EConcurrency::Asynchronous, 
			FSourceControlOperationComplete::CreateRaw( &AsyncHelper, &FAsyncCommandHelper::SourceControlOperationComplete ) 
			) != ECommandResult::Succeeded)
		{
			return true;
		}

		AsyncHelper.SetDispatched();
	}

	if(AsyncHelper.IsDone())
	{		
		// check state now we are done
		TSharedPtr<ISourceControlState, ESPMode::ThreadSafe> SourceControlState = ISourceControlModule::Get().GetProvider().GetState(SourceControlHelpers::PackageFilename(AsyncHelper.GetParameter()), EStateCacheUsage::Use);
		if(!SourceControlState.IsValid())
		{
			UE_LOG(LogSourceControl, Error, TEXT("Could not retrieve state for file '%s'"), *AsyncHelper.GetParameter());
		}
		else
		{
			if(SourceControlState->IsSourceControlled())
			{
				if(!SourceControlState->CanCheckout())
				{
					UE_LOG(LogSourceControl, Error, TEXT("Unexpected state following Revert operation for file '%s'"), *AsyncHelper.GetParameter());
				}
			}
		}
	}

	return AsyncHelper.IsDone();
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FCheckOutLatentCommand, FAsyncCommandHelper, AsyncHelper);

bool FCheckOutLatentCommand::Update()
{
	if(!AsyncHelper.IsDispatched())
	{
		if( ISourceControlModule::Get().GetProvider().Execute( 
			ISourceControlOperation::Create<FCheckOut>(), 
			SourceControlHelpers::PackageFilename(AsyncHelper.GetParameter()),
			EConcurrency::Asynchronous, 
			FSourceControlOperationComplete::CreateRaw( &AsyncHelper, &FAsyncCommandHelper::SourceControlOperationComplete ) 
			) != ECommandResult::Succeeded)
		{
			return true;
		}

		AsyncHelper.SetDispatched();
	}

	if(AsyncHelper.IsDone())
	{
		// check state now we are done
		TSharedPtr<ISourceControlState, ESPMode::ThreadSafe> SourceControlState = ISourceControlModule::Get().GetProvider().GetState(SourceControlHelpers::PackageFilename(AsyncHelper.GetParameter()), EStateCacheUsage::Use);
		if(!SourceControlState.IsValid())
		{
			UE_LOG(LogSourceControl, Error, TEXT("Could not retrieve state for file '%s'"), *AsyncHelper.GetParameter());
		}
		else
		{
			if(!SourceControlState->IsCheckedOut())
			{
				UE_LOG(LogSourceControl, Error, TEXT("Unexpected state following Check Out operation for file '%s'"), *AsyncHelper.GetParameter());
			}
		}
	}

	return AsyncHelper.IsDone();
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FCheckOutTest, "Editor.Source Control.Check Out", EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser)

void FCheckOutTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	GetProviders(OutBeautifiedNames, OutTestCommands);
	AppendFilename(TEXT("/Engine/EditorAutomation/SourceControlTest"), OutBeautifiedNames, OutTestCommands);
}

bool FCheckOutTest::RunTest(const FString& Parameters)
{
	// parameter is the provider we want to use followed by the filename
	const FString Delimiter(TEXT(" "));
	TArray<FString> ParamArray;
	Parameters.ParseIntoArray(&ParamArray, *Delimiter, true);
	ensure(ParamArray.Num() == 2);

	// check to see if we should restore the read only status after this test
	bool bWasReadOnly = IFileManager::Get().IsReadOnly(*SourceControlHelpers::PackageFilename(ParamArray[1]));

	ADD_LATENT_AUTOMATION_COMMAND(FSetProviderLatentCommand(FName(*ParamArray[0])));
	ADD_LATENT_AUTOMATION_COMMAND(FConnectLatentCommand(FAsyncCommandHelper()));
	ADD_LATENT_AUTOMATION_COMMAND(FCheckOutLatentCommand(FAsyncCommandHelper(ParamArray[1])));
	ADD_LATENT_AUTOMATION_COMMAND(FRevertLatentCommand(FAsyncCommandHelper(ParamArray[1])));

	ADD_LATENT_AUTOMATION_COMMAND(FSetReadOnlyFlag(FReadOnlyState(ParamArray[1], bWasReadOnly)));

	return true;
}

DECLARE_DELEGATE_OneParam(FAddLatentCommands, const FString&)

/**
 * Helper function used to generate parameters from one latent command to pass to another
 */
struct FLatentCommandChain
{
public:
	FLatentCommandChain(const FString& InParameter, const FAddLatentCommands& InLatentCommandsDelegate)
		: Parameter(InParameter)
		, LatentCommandDelegate(InLatentCommandsDelegate)
	{
	}

	/** Parameter to the first latent command */
	FString Parameter;

	/** Delegate to call once the first command is done (usually with output from the first latent command) */
	FAddLatentCommands LatentCommandDelegate;
};

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FCreatePackageLatentCommand, FLatentCommandChain, CommandChain);

bool FCreatePackageLatentCommand::Update()
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	FString PackageName;
	FString AssetName;
	AssetToolsModule.Get().CreateUniqueAssetName(CommandChain.Parameter, TEXT("New"), PackageName, AssetName);
	
	FString OriginalPackageName = SourceControlHelpers::PackageFilename(CommandChain.Parameter);
	FString NewPackageName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

	if(FPlatformFileManager::Get().GetPlatformFile().CopyFile(*NewPackageName, *OriginalPackageName))
	{
		UPackage* Package = LoadPackage(NULL, *NewPackageName, LOAD_None);
		if(Package != NULL)
		{
			CommandChain.LatentCommandDelegate.ExecuteIfBound(PackageName);
		}
		else
		{
			UE_LOG(LogSourceControl, Error, TEXT("Could not load temporary package '%s'"), *PackageName);
		}
	}
	else
	{
		UE_LOG(LogSourceControl, Error, TEXT("Could not create temporary package to add '%s'"), *PackageName);
	}

	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FDeletePackageLatentCommand, FString, Parameter);

bool FDeletePackageLatentCommand::Update()
{
	UPackage* Package = FindPackage(NULL, *Parameter);
	if(Package != NULL)
	{
		TArray<UPackage*> Packages;
		Packages.Add(Package);
		if(PackageTools::UnloadPackages(Packages))
		{
			FString PackageFileName = SourceControlHelpers::PackageFilename(Parameter);
			if(!FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*PackageFileName))
			{
				UE_LOG(LogSourceControl, Error, TEXT("Could not delete temporary package '%s'"), *PackageFileName);
			}
		}
		else
		{
			UE_LOG(LogSourceControl, Error, TEXT("Could not unload temporary package '%s'"), *Parameter);
		}
	}
	else
	{
		UE_LOG(LogSourceControl, Error, TEXT("Could not find temporary package '%s'"), *Parameter);
	}

	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FMarkForAddLatentCommand, FAsyncCommandHelper, AsyncHelper);

bool FMarkForAddLatentCommand::Update()
{
	if(!AsyncHelper.IsDispatched())
	{
		if( ISourceControlModule::Get().GetProvider().Execute( 
			ISourceControlOperation::Create<FMarkForAdd>(), 
			SourceControlHelpers::PackageFilename(AsyncHelper.GetParameter()),
			EConcurrency::Asynchronous, 
			FSourceControlOperationComplete::CreateRaw( &AsyncHelper, &FAsyncCommandHelper::SourceControlOperationComplete ) 
			) != ECommandResult::Succeeded)
		{
			return true;
		}

		AsyncHelper.SetDispatched();
	}

	if(AsyncHelper.IsDone())
	{
		// check state now we are done
		TSharedPtr<ISourceControlState, ESPMode::ThreadSafe> SourceControlState = ISourceControlModule::Get().GetProvider().GetState(SourceControlHelpers::PackageFilename(AsyncHelper.GetParameter()), EStateCacheUsage::Use);
		if(!SourceControlState.IsValid())
		{
			UE_LOG(LogSourceControl, Error, TEXT("Could not retrieve state for file '%s'"), *AsyncHelper.GetParameter());
		}
		else
		{
			if(!SourceControlState->IsAdded())
			{
				UE_LOG(LogSourceControl, Error, TEXT("Unexpected state following Mark For Add operation for file '%s'"), *AsyncHelper.GetParameter());
			}
		}
	}

	return AsyncHelper.IsDone();
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FMarkForAddTest, "Editor.Source Control.Mark For Add", EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser)

void FMarkForAddTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	GetProviders(OutBeautifiedNames, OutTestCommands);
	AppendFilename(TEXT("/Engine/EditorAutomation/SourceControlTest"), OutBeautifiedNames, OutTestCommands);
}

bool FMarkForAddTest::RunTest(const FString& Parameters)
{
	// parameter is the provider we want to use followed by the filename
	const FString Delimiter(TEXT(" "));
	TArray<FString> ParamArray;
	Parameters.ParseIntoArray(&ParamArray, *Delimiter, true);
	ensure(ParamArray.Num() == 2);

	ADD_LATENT_AUTOMATION_COMMAND(FSetProviderLatentCommand(FName(*ParamArray[0])));
	ADD_LATENT_AUTOMATION_COMMAND(FConnectLatentCommand(FAsyncCommandHelper()));

	struct Local
	{
		static void AddDependentCommands(const FString& InParameter)
		{
			ADD_LATENT_AUTOMATION_COMMAND(FMarkForAddLatentCommand(FAsyncCommandHelper(InParameter)));
			ADD_LATENT_AUTOMATION_COMMAND(FRevertLatentCommand(FAsyncCommandHelper(InParameter)));
			ADD_LATENT_AUTOMATION_COMMAND(FDeletePackageLatentCommand(InParameter));
		}
	};

	ADD_LATENT_AUTOMATION_COMMAND(FCreatePackageLatentCommand(FLatentCommandChain(ParamArray[1], FAddLatentCommands::CreateStatic(&Local::AddDependentCommands))));

	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FDeleteLatentCommand, FAsyncCommandHelper, AsyncHelper);

bool FDeleteLatentCommand::Update()
{
	if(!AsyncHelper.IsDispatched())
	{
		if( ISourceControlModule::Get().GetProvider().Execute( 
			ISourceControlOperation::Create<FDelete>(), 
			SourceControlHelpers::PackageFilename(AsyncHelper.GetParameter()),
			EConcurrency::Asynchronous, 
			FSourceControlOperationComplete::CreateRaw( &AsyncHelper, &FAsyncCommandHelper::SourceControlOperationComplete ) 
			) != ECommandResult::Succeeded)
		{
			return true;
		}

		AsyncHelper.SetDispatched();
	}

	if(AsyncHelper.IsDone())
	{
		// check state now we are done
		TSharedPtr<ISourceControlState, ESPMode::ThreadSafe> SourceControlState = ISourceControlModule::Get().GetProvider().GetState(SourceControlHelpers::PackageFilename(AsyncHelper.GetParameter()), EStateCacheUsage::Use);
		if(!SourceControlState.IsValid())
		{
			UE_LOG(LogSourceControl, Error, TEXT("Could not retrieve state for file '%s'"), *AsyncHelper.GetParameter());
		}
		else
		{
			if(!SourceControlState->IsDeleted())
			{
				UE_LOG(LogSourceControl, Error, TEXT("Unexpected state following Delete operation for file '%s'"), *AsyncHelper.GetParameter());
			}
		}
	}

	return AsyncHelper.IsDone();
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FDeleteTest, "Editor.Source Control.Delete", EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser)

void FDeleteTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	GetProviders(OutBeautifiedNames, OutTestCommands);
	AppendFilename(TEXT("/Engine/EditorAutomation/SourceControlTest"), OutBeautifiedNames, OutTestCommands);
}

bool FDeleteTest::RunTest(const FString& Parameters)
{
	// parameter is the provider we want to use followed by the filename
	const FString Delimiter(TEXT(" "));
	TArray<FString> ParamArray;
	Parameters.ParseIntoArray(&ParamArray, *Delimiter, true);
	ensure(ParamArray.Num() == 2);

	// check to see if we should restore the read only status after this test
	bool bWasReadOnly = IFileManager::Get().IsReadOnly(*SourceControlHelpers::PackageFilename(ParamArray[1]));

	FString AbsoluteFilename = SourceControlHelpers::PackageFilename(ParamArray[1]);

	ADD_LATENT_AUTOMATION_COMMAND(FSetProviderLatentCommand(FName(*ParamArray[0])));
	ADD_LATENT_AUTOMATION_COMMAND(FConnectLatentCommand(FAsyncCommandHelper()));
	ADD_LATENT_AUTOMATION_COMMAND(FDeleteLatentCommand(FAsyncCommandHelper(AbsoluteFilename)));
	ADD_LATENT_AUTOMATION_COMMAND(FRevertLatentCommand(FAsyncCommandHelper(AbsoluteFilename)));

	ADD_LATENT_AUTOMATION_COMMAND(FSetReadOnlyFlag(FReadOnlyState(ParamArray[1], bWasReadOnly)));

	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FCheckInLatentCommand, FAsyncCommandHelper, AsyncHelper);

bool FCheckInLatentCommand::Update()
{
	if(!AsyncHelper.IsDispatched())
	{
		TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOperation = ISourceControlOperation::Create<FCheckIn>();
		CheckInOperation->SetDescription(NSLOCTEXT("SourceControlTests", "TestChangelistDescription", "[AUTOMATED TEST] Automatic checkin, testing functionality."));

		if( ISourceControlModule::Get().GetProvider().Execute( 
			CheckInOperation, 
			SourceControlHelpers::PackageFilename(AsyncHelper.GetParameter()),
			EConcurrency::Asynchronous, 
			FSourceControlOperationComplete::CreateRaw( &AsyncHelper, &FAsyncCommandHelper::SourceControlOperationComplete ) 
			) != ECommandResult::Succeeded)
		{
			return true;
		}

		AsyncHelper.SetDispatched();
	}

	if(AsyncHelper.IsDone())
	{
		// check state now we are done
		TSharedPtr<ISourceControlState, ESPMode::ThreadSafe> SourceControlState = ISourceControlModule::Get().GetProvider().GetState(SourceControlHelpers::PackageFilename(AsyncHelper.GetParameter()), EStateCacheUsage::Use);
		if(!SourceControlState.IsValid())
		{
			UE_LOG(LogSourceControl, Error, TEXT("Could not retrieve state for file '%s'"), *AsyncHelper.GetParameter());
		}
		else
		{
			if(!SourceControlState->IsSourceControlled() || !SourceControlState->CanCheckout())
			{
				UE_LOG(LogSourceControl, Error, TEXT("Unexpected state following Check In operation for file '%s'"), *AsyncHelper.GetParameter());
			}
		}
	}

	return AsyncHelper.IsDone();
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FEditTextureLatentCommand, FString, PackageName);

bool FEditTextureLatentCommand::Update()
{
	// make a minor edit to the texture in the package we are passed
	UPackage* Package = LoadPackage(NULL, *PackageName, LOAD_None);
	if(Package != NULL)
	{
		UTexture2D* Texture = FindObject<UTexture2D>(Package, TEXT("SourceControlTest"));
		check(Texture);
		Texture->AdjustBrightness = FMath::FRand();
		Package->SetDirtyFlag(true);
		if(!UPackage::SavePackage(Package, NULL, RF_Standalone, *FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension()), GError, nullptr, false, true, SAVE_NoError))
		{
			UE_LOG(LogSourceControl, Error, TEXT("Could not save package: '%s'"), *PackageName);
		}
		TArray<UPackage*> Packages;
		Packages.Add(Package);
		PackageTools::UnloadPackages(Packages);
	}
	else
	{
		UE_LOG(LogSourceControl, Error, TEXT("Could not find package for edit: '%s'"), *PackageName);
	}

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FCheckInTest, "Editor.Source Control.Check In", EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser)

void FCheckInTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	GetProviders(OutBeautifiedNames, OutTestCommands);
	AppendFilename(TEXT("/Engine/EditorAutomation/SourceControlTest"), OutBeautifiedNames, OutTestCommands);
}

bool FCheckInTest::RunTest(const FString& Parameters)
{
	// parameter is the provider we want to use followed by the filename
	const FString Delimiter(TEXT(" "));
	TArray<FString> ParamArray;
	Parameters.ParseIntoArray(&ParamArray, *Delimiter, true);
	ensure(ParamArray.Num() == 2);

	// check to see if we should restore the read only status after this test
	bool bWasReadOnly = IFileManager::Get().IsReadOnly(*SourceControlHelpers::PackageFilename(ParamArray[1]));

	// parameter is the provider we want to use
	ADD_LATENT_AUTOMATION_COMMAND(FSetProviderLatentCommand(FName(*ParamArray[0])));
	ADD_LATENT_AUTOMATION_COMMAND(FConnectLatentCommand(FAsyncCommandHelper()));
	ADD_LATENT_AUTOMATION_COMMAND(FCheckOutLatentCommand(FAsyncCommandHelper(ParamArray[1])));
	ADD_LATENT_AUTOMATION_COMMAND(FEditTextureLatentCommand(ParamArray[1]));
	ADD_LATENT_AUTOMATION_COMMAND(FCheckInLatentCommand(FAsyncCommandHelper(ParamArray[1])));

	ADD_LATENT_AUTOMATION_COMMAND(FSetReadOnlyFlag(FReadOnlyState(ParamArray[1], bWasReadOnly)));

	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FSyncLatentCommand, FAsyncCommandHelper, AsyncHelper);

bool FSyncLatentCommand::Update()
{
	if(!AsyncHelper.IsDispatched())
	{
		if( ISourceControlModule::Get().GetProvider().Execute( 
			ISourceControlOperation::Create<FSync>(), 
			SourceControlHelpers::PackageFilename(AsyncHelper.GetParameter()),
			EConcurrency::Asynchronous, 
			FSourceControlOperationComplete::CreateRaw( &AsyncHelper, &FAsyncCommandHelper::SourceControlOperationComplete ) 
			) != ECommandResult::Succeeded)
		{
			return true;
		}

		AsyncHelper.SetDispatched();
	}

	if(AsyncHelper.IsDone())
	{
		// check state now we are done
		TSharedPtr<ISourceControlState, ESPMode::ThreadSafe> SourceControlState = ISourceControlModule::Get().GetProvider().GetState(SourceControlHelpers::PackageFilename(AsyncHelper.GetParameter()), EStateCacheUsage::Use);
		if(!SourceControlState.IsValid())
		{
			UE_LOG(LogSourceControl, Error, TEXT("Could not retrieve state for file '%s'"), *AsyncHelper.GetParameter());
		}
		else
		{
			if(!SourceControlState->IsCurrent())
			{
				UE_LOG(LogSourceControl, Error, TEXT("Unexpected state following Sync operation for file '%s'"), *AsyncHelper.GetParameter());
			}
		}
	}

	return AsyncHelper.IsDone();
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FSyncTest, "Editor.Source Control.Sync", EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser)

void FSyncTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	GetProviders(OutBeautifiedNames, OutTestCommands);
	AppendFilename(TEXT("/Engine/EditorAutomation/SourceControlTest"), OutBeautifiedNames, OutTestCommands);
}

bool FSyncTest::RunTest(const FString& Parameters)
{
	// parameter is the provider we want to use followed by the filename
	const FString Delimiter(TEXT(" "));
	TArray<FString> ParamArray;
	Parameters.ParseIntoArray(&ParamArray, *Delimiter, true);
	ensure(ParamArray.Num() == 2);

	ADD_LATENT_AUTOMATION_COMMAND(FSetProviderLatentCommand(FName(*ParamArray[0])));
	ADD_LATENT_AUTOMATION_COMMAND(FConnectLatentCommand(FAsyncCommandHelper()));
	ADD_LATENT_AUTOMATION_COMMAND(FSyncLatentCommand(FAsyncCommandHelper(ParamArray[1])));

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FRevertTest, "Editor.Source Control.Revert", EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser)

void FRevertTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	GetProviders(OutBeautifiedNames, OutTestCommands);
	AppendFilename(TEXT("/Engine/EditorAutomation/SourceControlTest"), OutBeautifiedNames, OutTestCommands);
}

bool FRevertTest::RunTest(const FString& Parameters)
{
	// parameter is the provider we want to use followed by the filename
	const FString Delimiter(TEXT(" "));
	TArray<FString> ParamArray;
	Parameters.ParseIntoArray(&ParamArray, *Delimiter, true);
	ensure(ParamArray.Num() == 2);

	ADD_LATENT_AUTOMATION_COMMAND(FSetProviderLatentCommand(FName(*ParamArray[0])));
	ADD_LATENT_AUTOMATION_COMMAND(FConnectLatentCommand(FAsyncCommandHelper()));

	struct Local
	{
		static void AddDepedentCommands(const FString& InParameter)
		{
			ADD_LATENT_AUTOMATION_COMMAND(FMarkForAddLatentCommand(FAsyncCommandHelper(InParameter)));
			ADD_LATENT_AUTOMATION_COMMAND(FRevertLatentCommand(FAsyncCommandHelper(InParameter)));
			ADD_LATENT_AUTOMATION_COMMAND(FDeletePackageLatentCommand(InParameter));
		}
	};

	ADD_LATENT_AUTOMATION_COMMAND(FCreatePackageLatentCommand(FLatentCommandChain(ParamArray[1], FAddLatentCommands::CreateStatic(&Local::AddDepedentCommands))))

	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FUpdateStatusLatentCommand, FAsyncCommandHelper, AsyncHelper);

bool FUpdateStatusLatentCommand::Update()
{
	if(!AsyncHelper.IsDispatched())
	{
		TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> UpdateStatusOperation = ISourceControlOperation::Create<FUpdateStatus>();
		UpdateStatusOperation->SetUpdateHistory(true);
		UpdateStatusOperation->SetGetOpenedOnly(true);

		if( ISourceControlModule::Get().GetProvider().Execute( 
			UpdateStatusOperation, 
			SourceControlHelpers::PackageFilename(AsyncHelper.GetParameter()),
			EConcurrency::Asynchronous, 
			FSourceControlOperationComplete::CreateRaw( &AsyncHelper, &FAsyncCommandHelper::SourceControlOperationComplete ) 
			) != ECommandResult::Succeeded)
		{
			return true;
		}

		AsyncHelper.SetDispatched();
	}

	return AsyncHelper.IsDone();
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FGetStateLatentCommand, FString, Filename);

bool FGetStateLatentCommand::Update()
{
	FSourceControlStatePtr SourceControlState = ISourceControlModule::Get().GetProvider().GetState(SourceControlHelpers::PackageFilename(Filename), EStateCacheUsage::Use);
	if(!SourceControlState.IsValid())
	{
		UE_LOG(LogSourceControl, Error, TEXT("Failed to get a valid state for file: %s"), *Filename);
	}
	else
	{
		if(!SourceControlState->IsCheckedOut())
		{
			UE_LOG(LogSourceControl, Error, TEXT("File '%s' should be checked out, but isnt."), *Filename);
		}
		else
		{
			if(SourceControlState->GetHistorySize() == 0)
			{
				UE_LOG(LogSourceControl, Error, TEXT("Failed to get a valid history for file: %s"), *Filename);
			}
			else
			{
				TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> HistoryItem = SourceControlState->GetHistoryItem(0);
				if(!HistoryItem.IsValid())
				{
					UE_LOG(LogSourceControl, Error, TEXT("Failed to get a valid history item 0 for file: %s"), *Filename);
				}
			}
		}
	}

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FUpdateStatusTest, "Editor.Source Control.Update Status", EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser)

void FUpdateStatusTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	GetProviders(OutBeautifiedNames, OutTestCommands);
	AppendFilename(TEXT("/Engine/EditorAutomation/SourceControlTest"), OutBeautifiedNames, OutTestCommands);
}

bool FUpdateStatusTest::RunTest(const FString& Parameters)
{
	// parameter is the provider we want to use followed by the filename
	const FString Delimiter(TEXT(" "));
	TArray<FString> ParamArray;
	Parameters.ParseIntoArray(&ParamArray, *Delimiter, true);
	ensure(ParamArray.Num() == 2);

	ADD_LATENT_AUTOMATION_COMMAND(FSetProviderLatentCommand(FName(*ParamArray[0])));
	ADD_LATENT_AUTOMATION_COMMAND(FConnectLatentCommand(FAsyncCommandHelper()));
	ADD_LATENT_AUTOMATION_COMMAND(FCheckOutLatentCommand(FAsyncCommandHelper(ParamArray[1])));
	ADD_LATENT_AUTOMATION_COMMAND(FUpdateStatusLatentCommand(FAsyncCommandHelper(ParamArray[1])));
	ADD_LATENT_AUTOMATION_COMMAND(FGetStateLatentCommand(ParamArray[1]));
	ADD_LATENT_AUTOMATION_COMMAND(FRevertLatentCommand(ParamArray[1]));

	return true;
}

/**
 * Helper struct for FGetLabelLatentCommand
 */
struct FLabelAndFilename
{
	FLabelAndFilename( const FString& InLabel, const FString& InFilename )
		: Label(InLabel)
		, Filename(InFilename)
	{
	}

	/** Label to use */
	FString Label;

	/** Filename to use */
	FString Filename;
};

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FGetLabelLatentCommand, FLabelAndFilename, LabelAndFilename);

bool FGetLabelLatentCommand::Update()
{
	// @todo: for the moment, getting labels etc. is synchronous.

	TArray< TSharedRef<ISourceControlLabel> > Labels = ISourceControlModule::Get().GetProvider().GetLabels(LabelAndFilename.Label);
	if(Labels.Num() == 0)
	{
		UE_LOG(LogSourceControl, Error, TEXT("No labels available that use the spec '%s'"), *LabelAndFilename.Label);
	}
	else
	{
		TArray< TSharedRef<ISourceControlRevision, ESPMode::ThreadSafe> > Revisions;
		Labels[0]->GetFileRevisions(FPaths::ConvertRelativePathToFull(LabelAndFilename.Filename), Revisions);
		if(Revisions.Num() == 0)
		{
			UE_LOG(LogSourceControl, Error, TEXT("No revisions of file '%s' found at label '%s'"), *LabelAndFilename.Filename, *LabelAndFilename.Label);
		}
		else
		{
			FString TempGetFilename;
			Revisions[0]->Get(TempGetFilename);
			if(TempGetFilename.Len() == 0 || !FPaths::FileExists(TempGetFilename))
			{
				UE_LOG(LogSourceControl, Error, TEXT("Could not get revision of file '%s' using label '%s'"), *LabelAndFilename.Filename, *LabelAndFilename.Label);
			}

			FString TempGetAnnotatedFilename;
			Revisions[0]->GetAnnotated(TempGetAnnotatedFilename);
			if(TempGetAnnotatedFilename.Len() == 0 || !FPaths::FileExists(TempGetAnnotatedFilename))
			{
				UE_LOG(LogSourceControl, Error, TEXT("Could not get annotated revision of file '%s' using label '%s'"), *LabelAndFilename.Filename, *LabelAndFilename.Label);
			}
		}
	}

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FGetLabelTest, "Editor.Source Control.Get Label", EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser)

void FGetLabelTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	GetProviders(OutBeautifiedNames, OutTestCommands);
	AppendFilename(TEXT("SourceControlAutomationLabel"), OutBeautifiedNames, OutTestCommands);
}

bool FGetLabelTest::RunTest(const FString& Parameters)
{
	// parameter is the provider we want to use followed by the label
	const FString Delimiter(TEXT(" "));
	TArray<FString> ParamArray;
	Parameters.ParseIntoArray(&ParamArray, *Delimiter, true);
	ensure(ParamArray.Num() == 2);

	ADD_LATENT_AUTOMATION_COMMAND(FSetProviderLatentCommand(FName(*ParamArray[0])));
	ADD_LATENT_AUTOMATION_COMMAND(FConnectLatentCommand(FAsyncCommandHelper()));
	ADD_LATENT_AUTOMATION_COMMAND(FGetLabelLatentCommand(FLabelAndFilename(ParamArray[1], TEXT("../../../Engine/Source/Developer/SourceControl/SourceControl.Build.cs"))));

	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FGetRevisionLatentCommand, FString, Filename);

bool FGetRevisionLatentCommand::Update()
{
	// @todo: for the moment, getting revisions etc. is synchronous.

	FSourceControlStatePtr SourceControlState = ISourceControlModule::Get().GetProvider().GetState(SourceControlHelpers::PackageFilename(Filename), EStateCacheUsage::Use);
	if(!SourceControlState.IsValid())
	{
		UE_LOG(LogSourceControl, Error, TEXT("Failed to get a valid state for file: %s"), *Filename);
	}
	else
	{
		if(SourceControlState->GetHistorySize() == 0)
		{
			UE_LOG(LogSourceControl, Error, TEXT("Failed to get a valid history for file: %s"), *Filename);
		}
		else
		{
			TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> HistoryItem = SourceControlState->GetHistoryItem(0);
			if(!HistoryItem.IsValid())
			{
				UE_LOG(LogSourceControl, Error, TEXT("Failed to get a valid history item 0 for file: %s"), *Filename);
			}
			else
			{
				FString TempGetFilename;
				HistoryItem->Get(TempGetFilename);
				if(TempGetFilename.Len() == 0 || !FPaths::FileExists(TempGetFilename))
				{
					UE_LOG(LogSourceControl, Error, TEXT("Could not get revision of file '%s'"), *Filename);
				}
			}
		}
	}

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FGetRevisionTest, "Editor.Source Control.Get Revision", EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser)

void FGetRevisionTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	GetProviders(OutBeautifiedNames, OutTestCommands);
	AppendFilename(TEXT("/Engine/EditorAutomation/SourceControlTest"), OutBeautifiedNames, OutTestCommands);
}

bool FGetRevisionTest::RunTest(const FString& Parameters)
{
	// parameter is the provider we want to use followed by the filename
	const FString Delimiter(TEXT(" "));
	TArray<FString> ParamArray;
	Parameters.ParseIntoArray(&ParamArray, *Delimiter, true);
	ensure(ParamArray.Num() == 2);

	ADD_LATENT_AUTOMATION_COMMAND(FSetProviderLatentCommand(FName(*ParamArray[0])));
	ADD_LATENT_AUTOMATION_COMMAND(FConnectLatentCommand(FAsyncCommandHelper()));
	ADD_LATENT_AUTOMATION_COMMAND(FUpdateStatusLatentCommand(FAsyncCommandHelper(ParamArray[1])));
	ADD_LATENT_AUTOMATION_COMMAND(FGetRevisionLatentCommand(ParamArray[1]));

	return true;
}

#endif	// #if WITH_EDITOR