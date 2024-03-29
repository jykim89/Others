// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AutomationTestStatus.cpp: Implements the AutomationTestStatus class.
=============================================================================*/

#include "AutomationControllerPrivatePCH.h"


FAutomationReport::FAutomationReport(FAutomationTestInfo& InTestInfo, bool InIsParent)
	: bEnabled( false )
	, bNodeExpandInUI( false )
	, bSelfPassesFilter( false )
	, bIsParent( InIsParent )
	, SupportFlags( 0 )
	, TestInfo( InTestInfo )
{
	// Enable smoke tests
	if ( TestInfo.GetTestType() == EAutomationTestType::ATT_SmokeTest )
	{
		bEnabled = true;
	}
}


void FAutomationReport::Empty()
{
	//release references to all child tests
	ChildReports.Empty();
	FilteredChildReports.Empty();
}


FString FAutomationReport::GetAssetName() const
{
	return TestInfo.GetTestParameter();
}


FString FAutomationReport::GetCommand() const
{
	return TestInfo.GetTestName();
}


const FString& FAutomationReport::GetDisplayName() const
{
	return TestInfo.GetDisplayName();
}


FString FAutomationReport::GetDisplayNameWithDecoration() const
{
	FString FinalDisplayName = TestInfo.GetDisplayName();
	//if this is an internal leaf node and the "decoration" name is being requested
	if (ChildReports.Num())
	{
		int32 NumChildren = GetTotalNumChildren();
		//append on the number of child tests
		return TestInfo.GetDisplayName() + FString::Printf(TEXT(" (%d)"), NumChildren);
	}
	return FinalDisplayName;
}


int32 FAutomationReport::GetTotalNumChildren() const
{
	int32 Total = 0;
	for (int32 ChildIndex = 0; ChildIndex < ChildReports.Num(); ++ChildIndex)
	{
		int ChildCount = ChildReports[ChildIndex]->GetTotalNumChildren();
		//Only count leaf nodes
		if (ChildCount == 0)
		{
			Total ++;
		}
		Total += ChildCount;
	}
	return Total;
}

void FAutomationReport::GetEnabledTestNames(TArray<FString>& OutEnabledTestNames, FString CurrentPath) const
{
	//if this is a leaf and this test is enabled
	if ((ChildReports.Num() == 0) && IsEnabled())
	{
		const FString FullTestName = CurrentPath.Len() > 0 ? CurrentPath.AppendChar(TCHAR('.')) + TestInfo.GetDisplayName() : TestInfo.GetDisplayName();
		OutEnabledTestNames.Add(FullTestName);
	}
	else
	{
		if( !CurrentPath.IsEmpty() )
		{
			CurrentPath += TEXT(".");
		}
		CurrentPath += TestInfo.GetDisplayName();
		//recurse through the hierarchy
		for (int32 ChildIndex = 0; ChildIndex < ChildReports.Num(); ++ChildIndex)
		{
			ChildReports[ChildIndex]->GetEnabledTestNames(OutEnabledTestNames,CurrentPath);
		}
	}
	return;
}

void FAutomationReport::SetEnabledTests(const TArray<FString>& EnabledTests, FString CurrentPath)
{
	if (ChildReports.Num() == 0)
	{
		//Find of the full name of this test and see if it is in our list
		const FString FullTestName = CurrentPath.Len() > 0 ? CurrentPath.AppendChar(TCHAR('.')) + TestInfo.GetDisplayName() : TestInfo.GetDisplayName();
		const bool bNewEnabled = EnabledTests.Contains(FullTestName);
		SetEnabled(bNewEnabled);
	}
	else
	{
		if( !CurrentPath.IsEmpty() )
		{
			CurrentPath += TEXT(".");
		}
		CurrentPath += TestInfo.GetDisplayName();

		//recurse through the hierarchy
		for (int32 ChildIndex = 0; ChildIndex < ChildReports.Num(); ++ChildIndex)
		{
			ChildReports[ChildIndex]->SetEnabledTests(EnabledTests,CurrentPath);
		}

		//Parent nodes should be checked if all of its children are
		const int32 TotalNumChildern = GetTotalNumChildren();
		const int32 EnabledChildren = GetEnabledTestsNum();
		bEnabled = (TotalNumChildern == EnabledChildren);
	}
}

/** Recursively gets the number of enabled tests */
int32 FAutomationReport::GetEnabledTestsNum() const
{
	int32 Total = 0;
	//if this is a leaf and this test is enabled
	if ((ChildReports.Num() == 0) && IsEnabled())
	{
		Total++;
	}
	else
	{
		//recurse through the hierarchy
		for (int32 ChildIndex = 0; ChildIndex < ChildReports.Num(); ++ChildIndex)
		{
			Total += ChildReports[ChildIndex]->GetEnabledTestsNum();
		}
	}
	return Total;
}

bool FAutomationReport::IsEnabled() const
{
	return bEnabled;
}


void FAutomationReport::SetEnabled(bool bShouldBeEnabled)
{
	bEnabled = bShouldBeEnabled;
	//set children to the same value
	for (int32 ChildIndex = 0; ChildIndex < FilteredChildReports.Num(); ++ChildIndex)
	{
		FilteredChildReports[ChildIndex]->SetEnabled(bShouldBeEnabled);
	}
}


void FAutomationReport::SetSupport(const int32 ClusterIndex)
{
	SupportFlags |= (1<<ClusterIndex);

	//ensure there is enough room in the array for status per platform
	for (int32 i = 0; i <= ClusterIndex; ++i)
	{
		//Make sure we have enough results for a single pass
		TArray<FAutomationTestResults> AutomationTestResult;
		AutomationTestResult.Add( FAutomationTestResults() );
		Results.Add( AutomationTestResult );
	}
}


bool FAutomationReport::IsSupported(const int32 ClusterIndex) const
{
	return (SupportFlags & (1<<ClusterIndex)) ? true : false;
}


uint8 FAutomationReport::GetTestType( ) const
{
	return TestInfo.GetTestType();
}


void FAutomationReport::SetTestType( const uint8 InTestType )
{
	TestInfo.AddTestType( InTestType );

	if ( InTestType == EAutomationTestType::ATT_SmokeTest )
	{
		bEnabled = true;
	}
}

const bool FAutomationReport::IsParent()
{
	return bIsParent;
}

const bool FAutomationReport::IsSmokeTest( )
{
	return GetTestType( ) & EAutomationTestType::ATT_SmokeTest ? true : false;
}

bool FAutomationReport::SetFilter( TSharedPtr< AutomationFilterCollection > InFilter, const bool ParentPassedFilter )
{
	//assume that this node and all its children fail to pass the filter test
	bool bSelfOrChildPassedFilter = false;

	//assume this node should not be expanded in the UI
	bNodeExpandInUI = false;

	//test for empty search string or matching search string
	bSelfPassesFilter = InFilter->PassesAllFilters( SharedThis( this ) );

	//clear the currently filtered tests array
	FilteredChildReports.Empty();

	//see if any children pass the filter
	for( int32 ChildIndex = 0; ChildIndex < ChildReports.Num(); ++ChildIndex )
	{
		bool ThisChildPassedFilter = ChildReports[ChildIndex]->SetFilter( InFilter, bSelfPassesFilter );

		if( ThisChildPassedFilter || bSelfPassesFilter || ParentPassedFilter )
		{
			FilteredChildReports.Add( ChildReports[ ChildIndex ] );
		}

		if ( bNodeExpandInUI == false && ThisChildPassedFilter == true )
		{
			// A child node has passed the filter, so we want to expand this node in the UI
			bNodeExpandInUI = true;
		}
	}

	//if we passed name, speed, and status tests
	if( bSelfPassesFilter || bNodeExpandInUI )
	{
		//Passed the filter!
		bSelfOrChildPassedFilter = true;
	}

	return bSelfOrChildPassedFilter;
}

TArray<TSharedPtr<IAutomationReport> >& FAutomationReport::GetFilteredChildren()
{
	return FilteredChildReports;
}

TArray<TSharedPtr<IAutomationReport> >& FAutomationReport::GetChildReports()
{
	return ChildReports;
}

void FAutomationReport::ResetForExecution(const int32 NumTestPasses)
{
	TestInfo.ResetNumDevicesRunningTest();

	//if this test is enabled
	if (IsEnabled())
	{
		for (int32 ClusterIndex = 0; ClusterIndex < Results.Num(); ++ClusterIndex)
		{
			//Make sure we have enough results
			if( NumTestPasses > Results[ClusterIndex].Num() )
			{
				for(int32 PassCount = Results[ClusterIndex].Num(); PassCount < NumTestPasses; ++PassCount)
				{
					Results[ClusterIndex].Add( FAutomationTestResults() );
				}
			}
			else if( NumTestPasses < Results[ClusterIndex].Num() )
			{
				Results[ClusterIndex].RemoveAt(NumTestPasses, Results[ClusterIndex].Num() - NumTestPasses);
			}

			for( int32 PassIndex = 0; PassIndex < Results[ClusterIndex].Num(); ++PassIndex)
			{
				//reset all stats
				Results[ClusterIndex][PassIndex].State = EAutomationState::NotRun;
				Results[ClusterIndex][PassIndex].Warnings.Empty();
				Results[ClusterIndex][PassIndex].Errors.Empty();
			}
		}
	}
	//recurse to children
	for (int32 ChildIndex = 0; ChildIndex < ChildReports.Num(); ++ChildIndex)
	{
		ChildReports[ChildIndex]->ResetForExecution(NumTestPasses);
	}
}


void FAutomationReport::SetResults( const int32 ClusterIndex, const int32 PassIndex, const FAutomationTestResults& InResults )
{
	//verify this is a platform this test is aware of
	check((ClusterIndex >= 0) && (ClusterIndex < Results.Num()));
	check((PassIndex >= 0) && (PassIndex < Results[ClusterIndex].Num()));

	if( InResults.State == EAutomationState::InProcess )
	{
		TestInfo.InformOfNewDeviceRunningTest();
	}

	Results[ ClusterIndex ][ PassIndex ] = InResults;
	// Add an error report if none was received
	if ( InResults.State == EAutomationState::Fail && InResults.Errors.Num() == 0 && InResults.Warnings.Num() == 0 )
	{
		Results[ClusterIndex][PassIndex].Errors.Add( "No Report Generated" );
	}
}


void FAutomationReport::GetCompletionStatus(const int32 ClusterIndex, const int32 PassIndex, FAutomationCompleteState& OutCompletionState)
{
	//if this test is enabled and a leaf test
	if (IsSupported(ClusterIndex) && (ChildReports.Num()==0))
	{
		EAutomationState::Type CurrentState = Results[ClusterIndex][PassIndex].State;
		//Enabled and In-Process
		if (IsEnabled())
		{
			OutCompletionState.TotalEnabled++;
			if (CurrentState == EAutomationState::InProcess)
			{
				OutCompletionState.NumEnabledInProcess++;
			}
		}

		//Warnings
		if (Results[ClusterIndex][PassIndex].Warnings.Num() > 0)
		{
			IsEnabled() ? OutCompletionState.NumEnabledTestsWarnings++ : OutCompletionState.NumDisabledTestsWarnings++;
		}

		//Test results
		if (CurrentState == EAutomationState::Success)
		{
			IsEnabled() ? OutCompletionState.NumEnabledTestsPassed++ : OutCompletionState.NumDisabledTestsPassed++;
		}
		else if (CurrentState == EAutomationState::Fail)
		{
			IsEnabled() ? OutCompletionState.NumEnabledTestsFailed++ : OutCompletionState.NumDisabledTestsFailed++;
		}
		else if( CurrentState == EAutomationState::NotEnoughParticipants )
		{
			IsEnabled() ? OutCompletionState.NumEnabledTestsCouldntBeRun++ : OutCompletionState.NumDisabledTestsCouldntBeRun++;
		}
	}
	//recurse to children
	for (int32 ChildIndex = 0; ChildIndex < ChildReports.Num(); ++ChildIndex)
	{
		ChildReports[ChildIndex]->GetCompletionStatus(ClusterIndex,PassIndex, OutCompletionState);
	}
}


EAutomationState::Type FAutomationReport::GetState(const int32 ClusterIndex, const int32 PassIndex) const
{
	if ((ClusterIndex >= 0) && (ClusterIndex < Results.Num()) &&
		(PassIndex >= 0) && (PassIndex < Results[ClusterIndex].Num()))
	{
		return Results[ClusterIndex][PassIndex].State;
	}
	return EAutomationState::NotRun;
}


const FAutomationTestResults& FAutomationReport::GetResults( const int32 ClusterIndex, const int32 PassIndex ) 
{
	return Results[ClusterIndex][PassIndex];
}

const int32 FAutomationReport::GetNumResults( const int32 ClusterIndex )
{
	return Results[ClusterIndex].Num();
}

const int32 FAutomationReport::GetCurrentPassIndex( const int32 ClusterIndex )
{
	int32 PassIndex = 1;
	for(; PassIndex < Results[ClusterIndex].Num(); ++PassIndex )
	{
		if( Results[ClusterIndex][PassIndex].State == EAutomationState::NotRun )
			break;
	}
	return PassIndex - 1;
}

FString FAutomationReport::GetGameInstanceName( const int32 ClusterIndex )
{
	return Results[ClusterIndex][0].GameInstance;
}

TSharedPtr<IAutomationReport> FAutomationReport::EnsureReportExists(FAutomationTestInfo& InTestInfo, const int32 ClusterIndex, const int32 NumPasses)
{
	//Split New Test Name by the first "." found
	FString NameToMatch = InTestInfo.GetDisplayName();
	FString NameRemainder;
	//if this is a leaf test (no ".")
	if (!InTestInfo.GetDisplayName().Split(TEXT("."), &NameToMatch, &NameRemainder))
	{
		NameToMatch = InTestInfo.GetDisplayName();
	}

	if ( NameRemainder.Len() != 0 )
	{
		// Set the test info name to be the remaining string
		InTestInfo.SetDisplayName( NameRemainder );
	}

	TSharedPtr<IAutomationReport> MatchTest;
	//go backwards.  Most recent event most likely matches
	int32 TestIndex = ChildReports.Num()-1;
	for (; TestIndex >=0; --TestIndex)
	{
		//if the name matches
		if (ChildReports[TestIndex]->GetDisplayName() == NameToMatch)
		{
			MatchTest = ChildReports[TestIndex];
			break;
		}
	}
	//if there isn't already a test like this
	if (!MatchTest.IsValid())
	{
		if ( NameRemainder.Len() == 0 )
		{
			// Create a new leaf node
			MatchTest = MakeShareable(new FAutomationReport(InTestInfo));			
		}
		else
		{
			// Create a parent node
			FAutomationTestInfo ParentTestInfo( NameToMatch, "", InTestInfo.GetTestType(), InTestInfo.GetNumParticipantsRequired() ) ;
			MatchTest = MakeShareable(new FAutomationReport(ParentTestInfo, true));
		}
		//make new test
		ChildReports.Add(MatchTest);
	}
	//mark this test as supported on a particular platform
	MatchTest->SetSupport(ClusterIndex);

	MatchTest->SetTestType( InTestInfo.GetTestType() );
	MatchTest->SetNumParticipantsRequired( MatchTest->GetNumParticipantsRequired() > InTestInfo.GetNumParticipantsRequired() ? MatchTest->GetNumParticipantsRequired() : InTestInfo.GetNumParticipantsRequired() );

	TSharedPtr<IAutomationReport> FoundTest;
	//if this is a leaf node
	if (NameRemainder.Len() == 0)
	{
		FoundTest = MatchTest;
	}
	else
	{
		//recurse to add to the proper layer
		FoundTest = MatchTest->EnsureReportExists(InTestInfo, ClusterIndex, NumPasses);
	}

	return FoundTest;
}


TSharedPtr<IAutomationReport> FAutomationReport::GetNextReportToExecute(bool& bOutAllTestsComplete, const int32 ClusterIndex, const int32 PassIndex, const int32 NumDevicesInCluster)
{
	TSharedPtr<IAutomationReport> NextReport;
	//if this is not a leaf node
	if (ChildReports.Num())
	{
		for (int32 ReportIndex = 0; ReportIndex < ChildReports.Num(); ++ReportIndex)
		{
			NextReport = ChildReports[ReportIndex]->GetNextReportToExecute(bOutAllTestsComplete, ClusterIndex, PassIndex, NumDevicesInCluster);
			//if we found one, return it
			if (NextReport.IsValid())
			{
				break;
			}
		}
	}
	else
	{
		//consider self
		if (IsEnabled() && IsSupported(ClusterIndex))
		{
			EAutomationState::Type TestState = GetState(ClusterIndex,PassIndex);
			//if any enabled test hasn't been run yet or is in process
			if ((TestState != EAutomationState::Success) && (TestState != EAutomationState::Fail) && (TestState != EAutomationState::NotEnoughParticipants))
			{
				//make sure we announce we are NOT done with all tests
				bOutAllTestsComplete = false;
			}
			if (TestState == EAutomationState::NotRun)
			{
				//Found one to run next
				NextReport = AsShared();
			}
		}
	}
	return NextReport;
}
const bool FAutomationReport::HasErrors()
{
	bool bHasErrors = false;
	for (int32 ClusterIndex = 0; ClusterIndex < Results.Num(); ++ClusterIndex )
	{
		for( int32 PassIndex = 0; PassIndex < Results[ClusterIndex].Num(); ++PassIndex)
		{
			//if we want tests with errors and this test had them OR we want tests warnings and this test had them
			if( Results[ ClusterIndex ][ PassIndex ].Errors.Num() ) 
			{
				//mark this test as having passed the results filter
				bHasErrors = true;
				break;
			}
		}
	}
	return bHasErrors;
}

const bool FAutomationReport::HasWarnings()
{
	bool bHasWarnings = false;
	for (int32 ClusterIndex = 0; ClusterIndex < Results.Num(); ++ClusterIndex )
	{
		for( int32 PassIndex = 0; PassIndex < Results[ClusterIndex].Num(); ++PassIndex)
		{
			//if we want tests with errors and this test had them OR we want tests warnings and this test had them
			if( Results[ ClusterIndex ][ PassIndex ].Warnings.Num() ) 
			{
				//mark this test as having passed the results filter
				bHasWarnings = true;
				break;
			}
		}
	}
	return bHasWarnings;
}


const bool FAutomationReport::GetDurationRange(float& OutMinTime, float& OutMaxTime)
{
	//assume we haven't found any tests that have completed successfully
	OutMinTime = MAX_FLT;
	OutMaxTime = 0.0f;
	bool bAnyResultsFound = false;

	//keep sum of all child tests
	float ChildTotalMinTime = 0.0f;
	float ChildTotalMaxTime = 0.0f;
	for (int32 ReportIndex = 0; ReportIndex < ChildReports.Num(); ++ReportIndex)
	{
		float ChildMinTime = MAX_FLT;
		float ChildMaxTime = 0.0f;
		if (ChildReports[ReportIndex]->GetDurationRange(ChildMinTime, ChildMaxTime))
		{
			ChildTotalMinTime += ChildMinTime;
			ChildTotalMaxTime += ChildMaxTime;
			bAnyResultsFound = true;
		}
	}

	//if any child test had valid timings
	if (bAnyResultsFound)
	{
		OutMinTime = ChildTotalMinTime;
		OutMaxTime = ChildTotalMaxTime;
	}

	for (int32 ClusterIndex = 0; ClusterIndex < Results.Num(); ++ClusterIndex )
	{
		for( int32 PassIndex = 0; PassIndex < Results[ClusterIndex].Num(); ++PassIndex)
		{
			//if we want tests with errors and this test had them OR we want tests warnings and this test had them
			if( Results[ClusterIndex][PassIndex].State == EAutomationState::Success)
			{
				OutMinTime = FMath::Min(OutMinTime, Results[ClusterIndex][PassIndex].Duration );
				OutMaxTime = FMath::Max(OutMaxTime, Results[ClusterIndex][PassIndex].Duration );
				bAnyResultsFound = true;
			}
		}
	}
	return bAnyResultsFound;
}


const int32 FAutomationReport::GetNumDevicesRunningTest() const
{
	return TestInfo.GetNumDevicesRunningTest();
}


const int32 FAutomationReport::GetNumParticipantsRequired() const
{
	return TestInfo.GetNumParticipantsRequired();
}


void FAutomationReport::SetNumParticipantsRequired( const int32 NewCount )
{
	TestInfo.SetNumParticipantsRequired( NewCount );
}


bool FAutomationReport::IncrementNetworkCommandResponses()
{
	NumberNetworkResponsesReceived++;
	return (NumberNetworkResponsesReceived == TestInfo.GetNumParticipantsRequired());
}


void FAutomationReport::ResetNetworkCommandResponses()
{
	NumberNetworkResponsesReceived = 0;
}


const bool FAutomationReport::ExpandInUI() const
{
	return bNodeExpandInUI;
}


void FAutomationReport::StopRunningTest()
{
	if( IsEnabled() )
	{
		for( int32 ResultsIndex = 0; ResultsIndex < Results.Num(); ++ResultsIndex )
		{
			for( int32 PassIndex = 0; PassIndex < Results[ResultsIndex].Num(); ++PassIndex)
			{
				if( Results[ResultsIndex][PassIndex].State == EAutomationState::InProcess )
				{
					Results[ResultsIndex][PassIndex].State = EAutomationState::NotRun;
				}
			}
		}
	}

	// Recurse to children
	for( int32 ChildIndex = 0; ChildIndex < ChildReports.Num(); ++ChildIndex )
	{
		ChildReports[ChildIndex]->StopRunningTest();
	}
}