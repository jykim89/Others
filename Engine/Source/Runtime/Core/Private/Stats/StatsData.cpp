// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "CorePrivate.h"
#if STATS

#include "TaskGraphInterfaces.h"
#include "StatsData.h"
#include "AllocatorFixedSizeFreeList.h"
#include "LockFreeFixedSizeAllocator.h"

DECLARE_CYCLE_STAT(TEXT("Broadcast"),STAT_StatsBroadcast,STATGROUP_StatSystem);
DECLARE_CYCLE_STAT(TEXT("Condense"),STAT_StatsCondense,STATGROUP_StatSystem);
DECLARE_DWORD_COUNTER_STAT(TEXT("Frame Messages"),STAT_StatFrameMessages,STATGROUP_StatSystem);
DECLARE_DWORD_COUNTER_STAT(TEXT("Total Frame Packets"),STAT_StatFramePackets,STATGROUP_StatSystem);
DECLARE_DWORD_COUNTER_STAT(TEXT("Frame Messages Condensed"),STAT_StatFramePacketsCondensed,STATGROUP_StatSystem);

/*-----------------------------------------------------------------------------
	FStatConstants
-----------------------------------------------------------------------------*/

const FName FStatConstants::NAME_ThreadRoot = "ThreadRoot";
const char* FStatConstants::ThreadGroupName = STAT_GROUP_TO_FStatGroup( STATGROUP_Threads )::GetGroupName();
const FName FStatConstants::NAME_ThreadGroup = FStatConstants::ThreadGroupName;
const FName FStatConstants::NAME_NoCategory = FName(TEXT("STATCAT_None"));

const FString FStatConstants::StatsFileExtension = TEXT( ".ue4stats" );
const FString FStatConstants::StatsFileRawExtension = TEXT( ".ue4statsraw" );

const FString FStatConstants::ThreadNameMarker = TEXT( "Thread_" );

/**
* Magic numbers for stats streams
*/
namespace EStatMagic
{
	enum Type
	{
		MAGIC=0x7E1B83C1,
		MAGIC_SWAPPED=0xC1831B7E
	};
}

/*-----------------------------------------------------------------------------
	FRawStatStackNode
-----------------------------------------------------------------------------*/

FRawStatStackNode::FRawStatStackNode(FRawStatStackNode const& Other)
	: Meta(Other.Meta)
{
	Children.Empty(Other.Children.Num());
	for (TMap<FName, FRawStatStackNode*>::TConstIterator It(Other.Children); It; ++It)
	{
		Children.Add(It.Key(), new FRawStatStackNode(*It.Value()));
	}
}

void FRawStatStackNode::MergeMax(FRawStatStackNode const& Other)
{
	check(Meta.NameAndInfo.GetRawName() == Other.Meta.NameAndInfo.GetRawName());
	if (Meta.NameAndInfo.GetField<EStatDataType>() != EStatDataType::ST_None && Meta.NameAndInfo.GetField<EStatDataType>() != EStatDataType::ST_FName)
	{
		FStatsUtils::AccumulateStat(Meta, Other.Meta, EStatOperation::MaxVal);
	}
	for (TMap<FName, FRawStatStackNode*>::TConstIterator It(Other.Children); It; ++It)
	{
		FRawStatStackNode* Child = Children.FindRef(It.Key());
		if (Child)
		{
			Child->MergeMax(*It.Value());
		}
		else
		{
			Children.Add(It.Key(), new FRawStatStackNode(*It.Value()));
		}
	}
}

void FRawStatStackNode::MergeAdd(FRawStatStackNode const& Other)
{
	check(Meta.NameAndInfo.GetRawName() == Other.Meta.NameAndInfo.GetRawName());
	if (Meta.NameAndInfo.GetField<EStatDataType>() != EStatDataType::ST_None && Meta.NameAndInfo.GetField<EStatDataType>() != EStatDataType::ST_FName)
	{
		FStatsUtils::AccumulateStat(Meta, Other.Meta, EStatOperation::Add);
	}
	for (TMap<FName, FRawStatStackNode*>::TConstIterator It(Other.Children); It; ++It)
	{
		FRawStatStackNode* Child = Children.FindRef(It.Key());
		if (Child)
		{
			Child->MergeAdd(*It.Value());
		}
		else
		{
			Children.Add(It.Key(), new FRawStatStackNode(*It.Value()));
		}
	}
}

void FRawStatStackNode::Divide(uint32 Div)
{
	if (Meta.NameAndInfo.GetField<EStatDataType>() != EStatDataType::ST_None && Meta.NameAndInfo.GetField<EStatDataType>() != EStatDataType::ST_FName)
	{
		FStatsUtils::DivideStat(Meta, Div);
	}
	for (TMap<FName, FRawStatStackNode*>::TIterator It(Children); It; ++It)
	{
		It.Value()->Divide(Div);
	}
}


void FRawStatStackNode::Cull(int64 MinCycles, int32 NoCullLevels)
{
	FRawStatStackNode* Culled = NULL;
	for (TMap<FName, FRawStatStackNode*>::TIterator It(Children); It; ++It)
	{
		FRawStatStackNode* Child = It.Value();
		if (NoCullLevels < 1 && FromPackedCallCountDuration_Duration(Child->Meta.GetValue_int64()) < MinCycles)
		{
			if (!Culled)
			{
				Culled = new FRawStatStackNode(FStatMessage(NAME_OtherChildren, EStatDataType::ST_int64, NULL, NULL, NULL, true, true));
				Culled->Meta.NameAndInfo.SetFlag(EStatMetaFlags::IsPackedCCAndDuration, true);
				Culled->Meta.Clear();
			}
			FStatsUtils::AccumulateStat(Culled->Meta, Child->Meta, EStatOperation::Add, true);
			delete Child;
			It.RemoveCurrent();
		}
		else
		{
			Child->Cull(MinCycles, NoCullLevels - 1);
		}
	}
	if (Culled)
	{
		Children.Add(NAME_OtherChildren, Culled);
	}
}

int64 FRawStatStackNode::ChildCycles() const
{
	int64 Total = 0;
	for (TMap<FName, FRawStatStackNode*>::TConstIterator It(Children); It; ++It)
	{
		FRawStatStackNode const* Child = It.Value();
		Total += FromPackedCallCountDuration_Duration(Child->Meta.GetValue_int64());
	}
	return Total;
}

void FRawStatStackNode::AddNameHierarchy(int32 CurrentPrefixDepth)
{
	if (Children.Num())
	{
		if (Children.Num() > 1 && Meta.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64 && Meta.NameAndInfo.GetFlag(EStatMetaFlags::IsPackedCCAndDuration))
		{
			TArray<FRawStatStackNode*> NewChildren;
			TArray<FRawStatStackNode*> ChildArray;
			Children.GenerateValueArray(ChildArray);
			ChildArray.Sort(FStatNameComparer<FRawStatStackNode>());
			TArray<TArray<FName> > ChildNames;
			ChildNames.Empty(ChildArray.Num());
			NewChildren.Empty(ChildArray.Num());

			FString Name;
			for (int32 Index = 0; Index < ChildArray.Num(); Index++)
			{
				FRawStatStackNode& Child = *ChildArray[Index];
				new (ChildNames) TArray<FName>();
				TArray<FName>& ParsedNames = ChildNames[Index];

				TArray<FString> Parts;
				Name = Child.Meta.NameAndInfo.GetRawName().ToString();
				if (Name.StartsWith(TEXT("//")))
				{
					// we won't add hierarchy for grouped stats
					new (ParsedNames) FName(Child.Meta.NameAndInfo.GetRawName());
				}
				else
				{
					Name.ReplaceInline(TEXT("/"), TEXT("."));
					Name.ParseIntoArray(&Parts, TEXT("."), true);
					check(Parts.Num());
					ParsedNames.Empty(Parts.Num());
					for (int32 PartIndex = 0; PartIndex < Parts.Num(); PartIndex++)
					{
						new (ParsedNames) FName(*Parts[PartIndex]);
					}
				}
			}

			int32 StartIndex = 0;

			while (StartIndex < ChildArray.Num())
			{
				int32 MaxParts = ChildNames[StartIndex].Num() - CurrentPrefixDepth;
				int32 NumWithCommonRoot = 1;
				if (MaxParts > 0)
				{
					for (int32 TestIndex = StartIndex + 1; TestIndex < ChildArray.Num(); TestIndex++)
					{
						if (CurrentPrefixDepth >= ChildNames[TestIndex].Num() || ChildNames[TestIndex][CurrentPrefixDepth] != ChildNames[StartIndex][CurrentPrefixDepth])
						{
							break;
						}
						NumWithCommonRoot++;
					}
				}
				if (NumWithCommonRoot < 2 || MaxParts < 1)
				{
					ChildArray[StartIndex]->AddNameHierarchy();
					NewChildren.Add(ChildArray[StartIndex]);
					StartIndex++;
					continue;
				}
				int32 MaxCommonality = CurrentPrefixDepth + 1;
				bool bOk = true;
				for (int32 TestDepth = CurrentPrefixDepth + 1; bOk && TestDepth < ChildNames[StartIndex].Num(); TestDepth++)
				{
					for (int32 TestIndex = StartIndex + 1; bOk && TestIndex < StartIndex + NumWithCommonRoot; TestIndex++)
					{
						if (TestDepth >= ChildNames[TestIndex].Num() || ChildNames[TestIndex][TestDepth] != ChildNames[StartIndex][TestDepth])
						{
							bOk = false;
						}
					}
					if (bOk)
					{
						MaxCommonality = TestDepth + 1;
					}
				}
				FString NewName(TEXT("NameFolder//"));
				for (int32 TestDepth = 0; TestDepth < MaxCommonality; TestDepth++)
				{
					NewName += ChildNames[StartIndex][TestDepth].ToString();
					NewName += TEXT(".");
				}
				NewName += TEXT("..");
				FStatMessage Group(ChildArray[StartIndex]->Meta);
				FName NewFName(*NewName);
				Group.NameAndInfo.SetRawName(NewFName);
				Group.Clear();
				FRawStatStackNode* NewNode = new FRawStatStackNode(Group);
				NewChildren.Add(NewNode);
				for (int32 TestIndex = StartIndex; TestIndex < StartIndex + NumWithCommonRoot; TestIndex++)
				{
					FStatsUtils::AccumulateStat(NewNode->Meta, ChildArray[TestIndex]->Meta, EStatOperation::Add, true);
					NewNode->Children.Add(ChildArray[TestIndex]->Meta.NameAndInfo.GetRawName(), ChildArray[TestIndex]);
				}
				NewNode->AddNameHierarchy(MaxCommonality);
				StartIndex += NumWithCommonRoot;
			}
			Children.Empty(NewChildren.Num());
			for (int32 Index = 0; Index < NewChildren.Num(); Index++)
			{
				Children.Add(NewChildren[Index]->Meta.NameAndInfo.GetRawName(), NewChildren[Index]);
			}
		}
		else
		{
			for (TMap<FName, FRawStatStackNode*>::TIterator It(Children); It; ++It)
			{
				FRawStatStackNode* Child = It.Value();
				Child->AddNameHierarchy();
			}
		}
	}
}

void FRawStatStackNode::AddSelf()
{
	if (Children.Num())
	{
		if (Meta.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64 && Meta.NameAndInfo.GetFlag(EStatMetaFlags::IsPackedCCAndDuration))
		{
			FStatMessage Self(Meta);
			int64 MyTime = Self.GetValue_Duration();
			MyTime -= ChildCycles();
			if (MyTime > 0)
			{
				Self.GetValue_int64() = ToPackedCallCountDuration(
					FromPackedCallCountDuration_CallCount(Self.GetValue_int64()),
					MyTime);
				Self.NameAndInfo.SetRawName(NAME_Self);
				Children.Add(NAME_Self, new FRawStatStackNode(Self));
			}
		}
		for (TMap<FName, FRawStatStackNode*>::TIterator It(Children); It; ++It)
		{
			FRawStatStackNode* Child = It.Value();
			Child->AddSelf();
		}
	}
}

void FRawStatStackNode::DebugPrint(TCHAR const* Filter, int32 InMaxDepth, int32 Depth) const
{
	if (Depth <= InMaxDepth)
	{
		if (!Filter || !*Filter)
		{
			FString TmpDebugStr = FStatsUtils::DebugPrint(Meta);
			UE_LOG(LogStats, Log, TEXT("%s%s"), FCString::Spc(Depth*2), *TmpDebugStr);
		}

		static int64 MinPrint = int64(.004f / FPlatformTime::ToMilliseconds(1.0f) + 0.5f);
		if (Children.Num())
		{
			TArray<FRawStatStackNode*> ChildArray;
			Children.GenerateValueArray(ChildArray);
			ChildArray.Sort( FStatDurationComparer<FRawStatStackNode>() );
			for (int32 Index = 0; Index < ChildArray.Num(); Index++)
			{
				if (ChildArray[Index]->Meta.GetValue_Duration() < MinPrint)
				{
					break;
				}
				if (Filter && *Filter)
				{
					if (ChildArray[Index]->Meta.NameAndInfo.GetRawName().ToString().Contains(Filter))
					{
						ChildArray[Index]->DebugPrint(NULL, InMaxDepth, 0);
					}
					else
					{
						ChildArray[Index]->DebugPrint(Filter, InMaxDepth, 0);
					}
				}
				else
				{
					ChildArray[Index]->DebugPrint(Filter, InMaxDepth, Depth + 1);
				}
			}
		}
	}
}

void FRawStatStackNode::Encode(TArray<FStatMessage>& OutStats) const
{
	FStatMessage* NewStat = new (OutStats) FStatMessage(Meta);
	if (Children.Num())
	{
		NewStat->NameAndInfo.SetField<EStatOperation>(EStatOperation::ChildrenStart);
		for (TMap<FName, FRawStatStackNode*>::TConstIterator It(Children); It; ++It)
		{
			FRawStatStackNode const* Child = It.Value();
			Child->Encode(OutStats);
		}
		FStatMessage* EndStat = new (OutStats) FStatMessage(Meta);
		EndStat->NameAndInfo.SetField<EStatOperation>(EStatOperation::ChildrenEnd);
	}
	else
	{
		NewStat->NameAndInfo.SetField<EStatOperation>(EStatOperation::Leaf);
	}
}

TLockFreeFixedSizeAllocator<sizeof(FRawStatStackNode)>& GetRawStatStackNodeAllocator()
{
	static TLockFreeFixedSizeAllocator<sizeof(FRawStatStackNode)> TheAllocator;
	return TheAllocator;
}

void* FRawStatStackNode::operator new(size_t Size)
{
	checkSlow(Size == sizeof(FRawStatStackNode));
	return GetRawStatStackNodeAllocator().Allocate();
}

void FRawStatStackNode::operator delete(void *RawMemory)
{
	GetRawStatStackNodeAllocator().Free(RawMemory);
}	

/*-----------------------------------------------------------------------------
	FComplexRawStatStackNode
-----------------------------------------------------------------------------*/

TLockFreeFixedSizeAllocator<sizeof(FComplexRawStatStackNode)>& GetRawStatStackNodeAllocatorEx()
{	
	static TLockFreeFixedSizeAllocator<sizeof(FComplexRawStatStackNode)> TheAllocatorComplex;
	return TheAllocatorComplex;
}

void* FComplexRawStatStackNode::operator new(size_t Size)
{
	checkSlow(Size == sizeof(FComplexRawStatStackNode));
	return GetRawStatStackNodeAllocatorEx().Allocate();
}

void FComplexRawStatStackNode::operator delete(void *RawMemory)
{
	GetRawStatStackNodeAllocatorEx().Free(RawMemory);
}

FComplexRawStatStackNode::FComplexRawStatStackNode( const FComplexRawStatStackNode& Other )
	: ComplexStat(Other.ComplexStat)
{
	Children.Empty(Other.Children.Num());
	for (auto It = Other.Children.CreateConstIterator(); It; ++It)
	{
		Children.Add(It.Key(), new FComplexRawStatStackNode(*It.Value()));
	}
}

FComplexRawStatStackNode::FComplexRawStatStackNode( const FRawStatStackNode& Other )
	: ComplexStat(Other.Meta)
{
	Children.Empty(Other.Children.Num());
	for (auto It = Other.Children.CreateConstIterator(); It; ++It)
	{
		Children.Add(It.Key(), new FComplexRawStatStackNode(*It.Value()));
	}
}

void FComplexRawStatStackNode::MergeAddAndMax( const FRawStatStackNode& Other )
{
	FComplexStatUtils::AddAndMax( ComplexStat, Other.Meta, EComplexStatField::IncSum, EComplexStatField::IncMax );

	for (auto It = Other.Children.CreateConstIterator(); It; ++It)
	{
		FComplexRawStatStackNode* Child = Children.FindRef(It.Key());
		if (Child)
		{
			Child->MergeAddAndMax(*It.Value());
		}
		else
		{
			Children.Add(It.Key(), new FComplexRawStatStackNode(*It.Value()));
		}
	}
}

void FComplexRawStatStackNode::Divide(uint32 Div)
{
	if (ComplexStat.NameAndInfo.GetField<EStatDataType>() != EStatDataType::ST_None && ComplexStat.NameAndInfo.GetField<EStatDataType>() != EStatDataType::ST_FName)
	{
		FComplexStatUtils::DivideStat( ComplexStat, Div, EComplexStatField::IncSum, EComplexStatField::IncAve );
	}
	for (auto It = Children.CreateIterator(); It; ++It)
	{
		It.Value()->Divide(Div);
	}
}

void FComplexRawStatStackNode::CopyExclusivesFromSelf()
{
	if( Children.Num() )
	{
		const FComplexRawStatStackNode* SelfStat = Children.FindRef( NAME_Self );
		if( SelfStat )
		{
			ComplexStat.GetValue_int64(EComplexStatField::ExcAve) = SelfStat->ComplexStat.GetValue_int64(EComplexStatField::IncAve);
			ComplexStat.GetValue_int64(EComplexStatField::ExcMax) = SelfStat->ComplexStat.GetValue_int64(EComplexStatField::IncMax);
		}

		for( auto It = Children.CreateIterator(); It; ++It )
		{
			FComplexRawStatStackNode* Child = It.Value();
			Child->CopyExclusivesFromSelf();
		}
	}
}

/*-----------------------------------------------------------------------------
	FStatsThreadState
-----------------------------------------------------------------------------*/

FStatsThreadState::FStatsThreadState(int32 InHistoryFrames)
	: HistoryFrames(InHistoryFrames)
	, MaxFrameSeen(0)
	, MinFrameSeen(-1)
	, LastFullFrameMetaAndNonFrame(-1)
	, LastFullFrameProcessed(-1)
	, bWasLoaded(false)
	, CurrentGameFrame(1)
	, CurrentRenderFrame(1)
{
}

//FStatsThreadState::FStatsThreadState(FString const& Filename)
// void FStatsThreadState::AddMessages(TArray<FStatMessage>& InMessages)
// @see moved to StatsFile.cpp

FStatsThreadState& FStatsThreadState::GetLocalState()
{
	static FStatsThreadState Singleton;
	return Singleton;
}

int64 FStatsThreadState::GetOldestValidFrame() const
{
	if (bWasLoaded)
	{
		if (MaxFrameSeen < 0 || MinFrameSeen < 0)
		{
			return -1;
		}
		return MinFrameSeen;
	}
	int64 Result = -1;
	for (auto It = GoodFrames.CreateConstIterator(); It; ++It)
	{
		if ((Result == -1 || *It < Result) && *It <= LastFullFrameMetaAndNonFrame)
		{
			Result = *It;
		}
	}
	return Result;
}

int64 FStatsThreadState::GetLatestValidFrame() const
{
	if (bWasLoaded)
	{
		if (MaxFrameSeen < 0 || MinFrameSeen < 0)
		{
			return -1;
		}
		if (MaxFrameSeen > MinFrameSeen)
		{
			return MaxFrameSeen - 1;
		}
		return MaxFrameSeen;
	}
	int64 Result = -1;
	for (auto It = GoodFrames.CreateConstIterator(); It; ++It)
	{
		if (*It > Result && *It <= LastFullFrameMetaAndNonFrame)
		{
			Result = *It;
		}
	}
	return Result;
}

void FStatsThreadState::ScanForAdvance(TArray<FStatMessage> const& Data)
{
	for (int32 Index = Data.Num() - 1; Index >= 0 ; Index--)
	{
		FStatMessage const& Item = Data[Index];
		EStatOperation::Type Op = Item.NameAndInfo.GetField<EStatOperation>();
		if (Op == EStatOperation::AdvanceFrameEventGameThread)
		{
			check(Item.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64);
			int64 NewGameFrame = Item.GetValue_int64();

			if (NewGameFrame < 0)
			{
				NewGameFrame = -NewGameFrame;
				BadFrames.Add(NewGameFrame - 1);
			}
			if (CurrentGameFrame > STAT_FRAME_SLOP && CurrentGameFrame + 1 != NewGameFrame)
			{
				// this packet has multiple advances in it. They are all bad.
				check(CurrentGameFrame + 1 < NewGameFrame);
				for (int64 Frame = CurrentGameFrame + 1; Frame <= NewGameFrame; Frame ++)
				{
					BadFrames.Add(Frame - 1);
				}
			}
			CurrentGameFrame = NewGameFrame;
		}
		else if (Op == EStatOperation::AdvanceFrameEventRenderThread)
		{
			check(Item.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64);
			int64 NewRenderFrame = Item.GetValue_int64();

			if (NewRenderFrame < 0)
			{
				NewRenderFrame = -NewRenderFrame;
				BadFrames.Add(NewRenderFrame - 1);
			}

			if (CurrentRenderFrame > STAT_FRAME_SLOP && CurrentRenderFrame + 1 != NewRenderFrame)
			{
				// this packet has multiple advances in it. They are all bad.
				check(CurrentRenderFrame + 1 < NewRenderFrame);
				for (int64 Frame = CurrentRenderFrame + 1; Frame <= NewRenderFrame; Frame ++)
				{
					BadFrames.Add(Frame - 1);
				}
			}
			CurrentRenderFrame = NewRenderFrame;
		}
	}
}

void FStatsThreadState::ScanForAdvance(FStatPacketArray& NewData)
{
	if (!FThreadStats::WillEverCollectData())
	{
		return;
	}

	uint32 Count = 0;
	for (int32 Index = 0; Index < NewData.Packets.Num(); Index++)
	{
		int64 FrameNum = NewData.Packets[Index]->ThreadType == EThreadType::Renderer ? CurrentRenderFrame : CurrentGameFrame;
		NewData.Packets[Index]->Frame = FrameNum;
		TArray<FStatMessage> const& Data = NewData.Packets[Index]->StatMessages;
		ScanForAdvance(Data);
		Count += Data.Num();
	}
	INC_DWORD_STAT_BY(STAT_StatFramePackets, NewData.Packets.Num());
	INC_DWORD_STAT_BY(STAT_StatFrameMessages, Count);
}

void FStatsThreadState::ProcessMetaDataForLoad(TArray<FStatMessage>& Data)
{
	check(bWasLoaded);
	for (int32 Index = 0; Index < Data.Num() ; Index++)
	{
		FStatMessage& Item = Data[Index];
		EStatOperation::Type Op = Item.NameAndInfo.GetField<EStatOperation>();
		if (Op == EStatOperation::SetLongName)
		{
			FindOrAddMetaData(Item);
		}
		else if (Op == EStatOperation::AdvanceFrameEventGameThread)
		{
			check(Item.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64);
			if (Item.GetValue_int64() > 0)
			{
				CurrentGameFrame = Item.GetValue_int64();
				if (CurrentGameFrame > MaxFrameSeen)
				{
					MaxFrameSeen = CurrentGameFrame;
				}
				if (MinFrameSeen < 0)
				{
					MinFrameSeen = CurrentGameFrame;
				}
			}
		}
		else if (Op == EStatOperation::AdvanceFrameEventRenderThread)
		{
			check(Item.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64);
			if (Item.GetValue_int64() > 0)
			{
				CurrentRenderFrame = Item.GetValue_int64();
				if (CurrentGameFrame > MaxFrameSeen)
				{
					MaxFrameSeen = CurrentGameFrame;
				}
				if (MinFrameSeen < 0)
				{
					MinFrameSeen = CurrentGameFrame;
				}
			}
		}
	}
}

void FStatsThreadState::ProcessMetaDataOnly(TArray<FStatMessage>& Data)
{
	for (int32 Index = 0; Index < Data.Num() ; Index++)
	{
		FStatMessage& Item = Data[Index];
		EStatOperation::Type Op = Item.NameAndInfo.GetField<EStatOperation>();
		check(Op == EStatOperation::SetLongName);
		FindOrAddMetaData(Item);
	}
}

void FStatsThreadState::ProcessNonFrameStats(TArray<FStatMessage>& Data, TSet<FName>* NonFrameStatsFound)
{
	check(!bWasLoaded);
	for (int32 Index = 0; Index < Data.Num() ; Index++)
	{
		FStatMessage& Item = Data[Index];
		check(Item.NameAndInfo.GetFlag(EStatMetaFlags::DummyAlwaysOne));  // we should never be sending short names to the stats any more
		EStatOperation::Type Op = Item.NameAndInfo.GetField<EStatOperation>();
		check(Op != EStatOperation::SetLongName);
		if (!Item.NameAndInfo.GetFlag(EStatMetaFlags::ShouldClearEveryFrame))
		{
			if (!(
				Op != EStatOperation::CycleScopeStart && 
				Op != EStatOperation::CycleScopeEnd &&
				Op != EStatOperation::ChildrenStart &&
				Op != EStatOperation::ChildrenEnd &&
				Op != EStatOperation::Leaf &&
				Op != EStatOperation::AdvanceFrameEventGameThread &&
				Op != EStatOperation::AdvanceFrameEventRenderThread
				))
			{
				UE_LOG(LogStats, Fatal, TEXT( "Stat %s was not cleared every frame, but was used with a scope cycle counter." ), *Item.NameAndInfo.GetRawName().ToString() );
			}
			else
			{
				FStatMessage* Result = NotClearedEveryFrame.Find(Item.NameAndInfo.GetRawName());
				if (!Result)
				{
					UE_LOG(LogStats, Error, TEXT( "Stat %s was cleared every frame, but we don't have metadata for it. Data loss." ), *Item.NameAndInfo.GetRawName().ToString() );
				}
				else
				{
					if (NonFrameStatsFound)
					{
						NonFrameStatsFound->Add(Item.NameAndInfo.GetRawName());
					}
					FStatsUtils::AccumulateStat(*Result, Item);
					Item = *Result; // now just write the accumulated value back into the stream
					check(Item.NameAndInfo.GetField<EStatOperation>() == EStatOperation::Set);
				}
			}
		}
	}
}

void FStatsThreadState::AddToHistoryAndEmpty(FStatPacketArray& NewData)
{
	if (!FThreadStats::WillEverCollectData())
	{
		NewData.Empty(); // delete the elements
		CondensedStackHistory.Empty();
		GoodFrames.Empty();
		BadFrames.Empty();
		NotClearedEveryFrame.Empty();
		ShortNameToLongName.Empty();
		Groups.Empty();
		History.Empty();
		return;
	}

	for (int32 Index = 0; Index < NewData.Packets.Num(); Index++)
	{
		int64 FrameNum = NewData.Packets[Index]->Frame;
		FStatPacketArray& Frame = History.FindOrAdd(FrameNum);
		Frame.Packets.Add(NewData.Packets[Index]);
	}

	NewData.Packets.Empty(); // don't delete the elements

	// now deal with metadata and non-frame stats

	TArray<int64> Frames;
	History.GenerateKeyArray(Frames);
	Frames.Sort();

	check(!bWasLoaded);

	int64 LatestFinishedFrame = FMath::Min<int64>(CurrentGameFrame, CurrentRenderFrame) - 1;

	for (int32 Index = 0; Index < Frames.Num(); Index++)
	{
		int64 FrameNum = Frames[Index];
		if (LastFullFrameMetaAndNonFrame < 0)
		{
			LastFullFrameMetaAndNonFrame = FrameNum - 1;
		}
		if (FrameNum <= LatestFinishedFrame && FrameNum == LastFullFrameMetaAndNonFrame + 1)
		{
			FStatPacketArray& Frame = History.FindChecked(FrameNum);

			FStatPacket const* PacketToCopyForNonFrame = NULL;

			TSet<FName> NonFrameStatsFound;
			for (int32 PacketIndex = 0; PacketIndex < Frame.Packets.Num(); PacketIndex++)
			{
				ProcessNonFrameStats(Frame.Packets[PacketIndex]->StatMessages, &NonFrameStatsFound);
				if (!PacketToCopyForNonFrame && Frame.Packets[PacketIndex]->ThreadType == EThreadType::Game)
				{
					PacketToCopyForNonFrame = Frame.Packets[PacketIndex];
				}
			}
			// was this a good frame
			if (!BadFrames.Contains(FrameNum) && PacketToCopyForNonFrame)
			{
				// add the non frame stats as a new last packet

				check(PacketToCopyForNonFrame);

				TArray<FStatMessage>* NonFrameMessages = NULL;

				for (TMap<FName, FStatMessage>::TConstIterator It(NotClearedEveryFrame); It; ++It)
				{
					if (!NonFrameStatsFound.Contains(It.Key())) // don't add stats that are updated during this frame, they would be redundant
					{
						if (!NonFrameMessages)
						{
							int32 NonFrameIndex = Frame.Packets.Add(new FStatPacket(*PacketToCopyForNonFrame));
							NonFrameMessages = &Frame.Packets[NonFrameIndex]->StatMessages;
						}
						new (*NonFrameMessages) FStatMessage(It.Value());
					}
				}
				GoodFrames.Add(FrameNum);
			}
			LastFullFrameMetaAndNonFrame = FrameNum;
		}
	}

	int64 NewLatestFrame = GetLatestValidFrame();

	if (NewLatestFrame > 0)
	{
		check(GoodFrames.Contains(NewLatestFrame));
		if (NewLatestFrame > LastFullFrameProcessed)
		{
			int64 FirstNewFrame = FMath::Max<int64>(GetOldestValidFrame(), LastFullFrameProcessed + 1);

			// let people know
			{
				SCOPE_CYCLE_COUNTER(STAT_StatsBroadcast);
				for (int64 Frame = FirstNewFrame; Frame <= NewLatestFrame; Frame++ )
				{
					if (IsFrameValid(Frame))
					{
						NewFrameDelegate.Broadcast(Frame);
						LastFullFrameProcessed = Frame;
					}
				}
			}
		}
	}

	int64 MinFrameToKeep = LatestFinishedFrame - HistoryFrames;

	for (auto It = BadFrames.CreateIterator(); It; ++It)
	{
		int64 ThisFrame = *It ;
		if (ThisFrame <= LastFullFrameMetaAndNonFrame && ThisFrame < MinFrameToKeep)
		{
			check(ThisFrame <= LastFullFrameMetaAndNonFrame);
			It.RemoveCurrent();
		}
	}
	for (auto It = History.CreateIterator(); It; ++It)
	{
		int64 ThisFrame = It.Key();
		if (ThisFrame <= LastFullFrameMetaAndNonFrame && ThisFrame < MinFrameToKeep)
		{
			check(ThisFrame <= LastFullFrameMetaAndNonFrame);
			It.RemoveCurrent();
		}
	}
	for (auto It = CondensedStackHistory.CreateIterator(); It; ++It)
	{
		int64 ThisFrame = It.Key();
		if (ThisFrame <= LastFullFrameProcessed && ThisFrame < MinFrameToKeep)
		{
			delete It.Value();
			It.RemoveCurrent();
		}
	}
	for (auto It = GoodFrames.CreateIterator(); It; ++It)
	{
		int64 ThisFrame = *It ;
		if (!History.Contains(ThisFrame) && !CondensedStackHistory.Contains(ThisFrame)) // if it isn't in the history anymore, it isn't good anymore
		{
			It.RemoveCurrent();
		}
	}

	check(History.Num() <= HistoryFrames * 2 + 5);
	check(CondensedStackHistory.Num() <= HistoryFrames * 2 + 5);
	check(GoodFrames.Num() <= HistoryFrames * 2 + 5);
	check(BadFrames.Num() <= HistoryFrames * 2 + 5);
}


void FStatsThreadState::GetInclusiveAggregateStackStats(int64 TargetFrame, TArray<FStatMessage>& OutStats, IItemFiler* Filter, bool bAddNonStackStats /*= true*/) const
{
	struct FTimeInfo
	{
		int32 StartCalls;
		int32 StopCalls;
		int32 Recursion;
		FTimeInfo()
			: StartCalls(0)
			, StopCalls(0)
			, Recursion(0)
		{

		}
	};
	TMap<FName, FTimeInfo> Timing;
	TMap<FName, FStatMessage> ThisFrameMetaData;
	TArray<FStatMessage> const& Data = GetCondensedHistory(TargetFrame);
	for (int32 Index = 0; Index < Data.Num(); Index++)
	{
		FStatMessage const& Item = Data[Index];
		if (!Filter || Filter->Keep(Item))
		{
			FName LongName = Item.NameAndInfo.GetRawName();

			EStatOperation::Type Op = Item.NameAndInfo.GetField<EStatOperation>();
			if ((Op == EStatOperation::ChildrenStart || Op == EStatOperation::ChildrenEnd ||  Op == EStatOperation::Leaf) && Item.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle))
			{

				FStatMessage* Result = ThisFrameMetaData.Find(LongName);
				if (!Result)
				{
					Result = &ThisFrameMetaData.Add(LongName, Item);
					Result->NameAndInfo.SetField<EStatOperation>(EStatOperation::Set);
					Result->NameAndInfo.SetFlag(EStatMetaFlags::IsPackedCCAndDuration, true);
					Result->Clear();
				}
				FTimeInfo& ItemTime = Timing.FindOrAdd(LongName);
				if (Op == EStatOperation::ChildrenStart)
				{
					ItemTime.StartCalls++;
					ItemTime.Recursion++;
				}
				else
				{
					if (Op == EStatOperation::ChildrenEnd)
					{
						ItemTime.StopCalls++;
						ItemTime.Recursion--;
					}
					if (!ItemTime.Recursion) // doing aggregates here, so ignore misleading recursion which would be counted twice
					{
						FStatsUtils::AccumulateStat(*Result, Item, EStatOperation::Add);
					}

				}
			}
			else if( bAddNonStackStats )
			{
				FStatsUtils::AddNonStackStats( LongName, Item, Op, ThisFrameMetaData );
			}
		}
	}

	for (TMap<FName, FStatMessage>::TConstIterator It(ThisFrameMetaData); It; ++It)
	{
		OutStats.Add(It.Value());
	}
}

void FStatsThreadState::GetExclusiveAggregateStackStats(int64 TargetFrame, TArray<FStatMessage>& OutStats, IItemFiler* Filter, bool bAddNonStackStats /*= true*/) const
{
	TMap<FName, FStatMessage> ThisFrameMetaData;
	TArray<FStatMessage> const& Data = GetCondensedHistory(TargetFrame);

	TArray<FStatMessage> ChildDurationStack;

	for (int32 Index = 0; Index < Data.Num(); Index++)
	{
		FStatMessage const& Item = Data[Index];
		FName LongName = Item.NameAndInfo.GetRawName();

		EStatOperation::Type Op = Item.NameAndInfo.GetField<EStatOperation>();
		if ((Op == EStatOperation::ChildrenStart || Op == EStatOperation::ChildrenEnd ||  Op == EStatOperation::Leaf) && Item.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle))
		{
			FStatMessage* Result = ThisFrameMetaData.Find(LongName);
			if (!Result)
			{
				Result = &ThisFrameMetaData.Add(LongName, Item);
				Result->NameAndInfo.SetField<EStatOperation>(EStatOperation::Set);
				Result->NameAndInfo.SetFlag(EStatMetaFlags::IsPackedCCAndDuration, true);
				Result->Clear();
			}
			if (Op == EStatOperation::ChildrenStart)
			{
				ChildDurationStack.Add(FStatMessage(Item));
			}
			else 
			{
				if (Op == EStatOperation::ChildrenEnd)
				{
					FStatsUtils::AccumulateStat(*Result, ChildDurationStack.Pop(), EStatOperation::Add);
				}
				else
				{
					FStatsUtils::AccumulateStat(*Result, Item, EStatOperation::Add);
				}
				if (ChildDurationStack.Num())
				{
					FStatsUtils::AccumulateStat(ChildDurationStack.Top(), Item, EStatOperation::Subtract, true);
				}
			}
		}
		else if( bAddNonStackStats )
		{
			FStatsUtils::AddNonStackStats( LongName, Item, Op, ThisFrameMetaData );
		}
	}

	for (TMap<FName, FStatMessage>::TConstIterator It(ThisFrameMetaData); It; ++It)
	{
		if (!Filter || Filter->Keep(It.Value()))
		{
			OutStats.Add(It.Value());
		}
	}
}

TArray<FStatMessage> const& FStatsThreadState::GetCondensedHistory(int64 TargetFrame) const
{
	check(IsFrameValid(TargetFrame));

	TArray<FStatMessage> const* Result = CondensedStackHistory.FindRef(TargetFrame);
	if (Result)
	{
		return *Result;
	}
	SCOPE_CYCLE_COUNTER(STAT_StatsCondense);
	TArray<FStatMessage>& OutStats = *CondensedStackHistory.Add(TargetFrame, new TArray<FStatMessage>);
	Condense(TargetFrame, OutStats);
	INC_DWORD_STAT_BY(STAT_StatFramePacketsCondensed, OutStats.Num());
	return OutStats;
}

void FStatsThreadState::GetRawStackStats(int64 TargetFrame, FRawStatStackNode& Root, TArray<FStatMessage>* OutNonStackStats) const
{
	const FStatPacketArray& Frame = GetStatPacketArray( TargetFrame );
	TMap<FName, FStatMessage> ThisFrameNonStackStats;

	for (int32 PacketIndex = 0; PacketIndex < Frame.Packets.Num(); PacketIndex++)
	{
		FStatPacket const& Packet = *Frame.Packets[PacketIndex];
		const FName ThreadName = GetStatThreadName( Packet );

		FRawStatStackNode* ThreadRoot = Root.Children.FindRef(ThreadName);
		if (!ThreadRoot)
		{
			FString ThreadIdName = FStatsUtils::BuildUniqueThreadName( Packet.ThreadId );
			ThreadRoot = Root.Children.Add( ThreadName, new FRawStatStackNode( FStatMessage( ThreadName, EStatDataType::ST_int64, STAT_GROUP_TO_FStatGroup( STATGROUP_Threads )::GetGroupName(), STAT_GROUP_TO_FStatGroup( STATGROUP_Threads )::GetGroupCategory(), *ThreadIdName, true, true ) ) );
			ThreadRoot->Meta.NameAndInfo.SetFlag(EStatMetaFlags::IsPackedCCAndDuration, true);
			ThreadRoot->Meta.Clear();
		}

		{
			TArray<FStatMessage const*> StartStack;
			TArray<FRawStatStackNode*> Stack;
			Stack.Add(ThreadRoot);
			FRawStatStackNode* Current = Stack.Last();

			TArray<FStatMessage> const& Data = Packet.StatMessages;
			for (int32 Index = 0; Index < Data.Num(); Index++)
			{
				FStatMessage const& Item = Data[Index];
				check(Item.NameAndInfo.GetFlag(EStatMetaFlags::DummyAlwaysOne));  // we should never be sending short names to the stats anymore

				EStatOperation::Type Op = Item.NameAndInfo.GetField<EStatOperation>();

				FName LongName = Item.NameAndInfo.GetRawName();
				if (Op == EStatOperation::CycleScopeStart || Op == EStatOperation::CycleScopeEnd)
				{
					check(Item.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle));
					if (Op == EStatOperation::CycleScopeStart)
					{
						FRawStatStackNode* Result = Current->Children.FindRef(LongName);
						if (!Result)
						{
							Result = Current->Children.Add(LongName, new FRawStatStackNode(Item));
							Result->Meta.NameAndInfo.SetField<EStatOperation>(EStatOperation::Set);
							Result->Meta.NameAndInfo.SetFlag(EStatMetaFlags::IsPackedCCAndDuration, true);
							Result->Meta.Clear();
						}
						Stack.Add(Result);
						StartStack.Add(&Item);
						Current = Result;

					}
					if (Op == EStatOperation::CycleScopeEnd)
					{
						FStatMessage RootCall = FStatsUtils::ComputeCall(*StartStack.Pop(), Item);
						FStatsUtils::AccumulateStat(Current->Meta, RootCall, EStatOperation::Add);
						check(Current->Meta.NameAndInfo.GetFlag(EStatMetaFlags::IsPackedCCAndDuration));
						verify(Current == Stack.Pop());
						Current = Stack.Last();
					}
				}
				else if (OutNonStackStats)
				{
					FStatsUtils::AddNonStackStats( LongName, Item, Op, ThisFrameNonStackStats );
				}
			}
			// not true with partial frames check(Stack.Num() == 1 && Stack.Last() == ThreadRoot && Current == ThreadRoot);
		}
	}
	//add up the thread totals
	for (TMap<FName, FRawStatStackNode*>::TConstIterator ItRoot(Root.Children); ItRoot; ++ItRoot)
	{
		FRawStatStackNode* ThreadRoot = ItRoot.Value();
		for (TMap<FName, FRawStatStackNode*>::TConstIterator It(ThreadRoot->Children); It; ++It)
		{
			ThreadRoot->Meta.GetValue_int64() += It.Value()->Meta.GetValue_int64();
		}
	}
	if (OutNonStackStats)
	{
		for (TMap<FName, FStatMessage>::TConstIterator It(ThisFrameNonStackStats); It; ++It)
		{
			new (*OutNonStackStats) FStatMessage(It.Value());
		}
	}
}

void FStatsThreadState::UncondenseStackStats(int64 TargetFrame, FRawStatStackNode& Root, IItemFiler* Filter, TArray<FStatMessage>* OutNonStackStats) const
{
	TArray<FStatMessage> const& Data = GetCondensedHistory(TargetFrame);
	TMap<FName, FStatMessage> ThisFrameNonStackStats;

	{
		TArray<FRawStatStackNode*> Stack;
		Stack.Add(&Root);
		FRawStatStackNode* Current = Stack.Last();

		for (int32 Index = 0; Index < Data.Num(); Index++)
		{
			FStatMessage const& Item = Data[Index];
			if (!Filter || Filter->Keep(Item))
			{
				EStatOperation::Type Op = Item.NameAndInfo.GetField<EStatOperation>();
				FName LongName = Item.NameAndInfo.GetRawName();
				if (Op == EStatOperation::ChildrenStart || Op == EStatOperation::ChildrenEnd || Op == EStatOperation::Leaf)
				{
					if (LongName != FStatConstants::NAME_ThreadRoot)
					{
						if (Op == EStatOperation::ChildrenStart || Op == EStatOperation::Leaf)
						{
							FRawStatStackNode* Result = Current->Children.FindRef(LongName);
							if (!Result)
							{
								Result = Current->Children.Add(LongName, new FRawStatStackNode(Item));
								Result->Meta.NameAndInfo.SetField<EStatOperation>(EStatOperation::Set);
							}
							else
							{
								FStatsUtils::AccumulateStat(Result->Meta, Item, EStatOperation::Add);
							}
							if (Op == EStatOperation::ChildrenStart)
							{
								Stack.Add(Result);
								Current = Result;
							}
						}
						if (Op == EStatOperation::ChildrenEnd)
						{
							verify(Current == Stack.Pop());
							Current = Stack.Last();
						}
					}
				}
				else if (OutNonStackStats)
				{
					FStatsUtils::AddNonStackStats( LongName, Item, Op, ThisFrameNonStackStats );
				}
			}
		}
	}
	if (OutNonStackStats)
	{
		for (TMap<FName, FStatMessage>::TConstIterator It(ThisFrameNonStackStats); It; ++It)
		{
			OutNonStackStats->Add(It.Value());
		}
	}
}

int64 FStatsThreadState::GetFastThreadFrameTimeInternal( int64 TargetFrame, int32 ThreadID, EThreadType::Type Thread ) const
{
	int64 Result = 0;

	const FStatPacketArray& Frame = GetStatPacketArray( TargetFrame );

	for( int32 PacketIndex = 0; PacketIndex < Frame.Packets.Num(); PacketIndex++ )
	{
		FStatPacket const& Packet = *Frame.Packets[PacketIndex];

		if( Packet.ThreadId == ThreadID || Packet.ThreadType == Thread )
		{
			TArray<FStatMessage> const& Data = Packet.StatMessages;
			for (int32 Index = 0; Index < Data.Num(); Index++)
			{
				FStatMessage const& Item = Data[Index];
				EStatOperation::Type Op = Item.NameAndInfo.GetField<EStatOperation>();
				if (Op == EStatOperation::CycleScopeStart)
				{
					check(Item.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle));
					Result -= Item.GetValue_int64();
					break;
				}
			}
			for (int32 Index = Data.Num() - 1; Index >= 0; Index--)
			{
				FStatMessage const& Item = Data[Index];
				EStatOperation::Type Op = Item.NameAndInfo.GetField<EStatOperation>();
				FName LongName = Item.NameAndInfo.GetRawName();
				if (Op == EStatOperation::CycleScopeEnd)
				{

					check(Item.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle));
					Result += Item.GetValue_int64();
					break;
				}
			}
		}
	}
	return Result;
}

int64 FStatsThreadState::GetFastThreadFrameTime(int64 TargetFrame, EThreadType::Type Thread) const
{
	return GetFastThreadFrameTimeInternal( TargetFrame, 0, Thread );
}

int64 FStatsThreadState::GetFastThreadFrameTime( int64 TargetFrame, uint32 ThreadID ) const
{
	return GetFastThreadFrameTimeInternal( TargetFrame, ThreadID, EThreadType::Invalid );
}

FName FStatsThreadState::GetStatThreadName( const FStatPacket& Packet ) const
{
	FName ThreadName;
	if( Packet.ThreadType == EThreadType::Game )
	{
		ThreadName = NAME_GameThread;
	}
	else if( Packet.ThreadType == EThreadType::Renderer )
	{
		ThreadName = NAME_RenderThread;
	}
	else if( Packet.ThreadType == EThreadType::Other )
	{
		TMap<uint32, FName>& MutableThreads = const_cast<TMap<uint32, FName>&>(Threads);
		FName& NewThreadName = MutableThreads.FindOrAdd( Packet.ThreadId );
		if( NewThreadName == NAME_None )
		{
			UE_LOG( LogStats, Warning, TEXT( "There is no thread with id: %u. Please add thread metadata for this thread." ), Packet.ThreadId );

			static const FName NAME_UnknownThread = TEXT( "UnknownThread" );
			NewThreadName = FName( *FStatsUtils::BuildUniqueThreadName( Packet.ThreadId ) );
			// This is an unknown thread, but still we need the metadata in the system.
			FStartupMessages::Get().AddThreadMetadata( NAME_UnknownThread, Packet.ThreadId );
		}
		ThreadName = NewThreadName;
	}

	check( ThreadName != NAME_None );
	return ThreadName;
}

void FStatsThreadState::Condense(int64 TargetFrame, TArray<FStatMessage>& OutStats) const
{
	static FStatNameAndInfo Adv(NAME_AdvanceFrame, "", "", TEXT(""), EStatDataType::ST_int64, true, false);
	new (OutStats) FStatMessage(Adv.GetEncodedName(), EStatOperation::AdvanceFrameEventGameThread, TargetFrame, false);
	new (OutStats) FStatMessage(Adv.GetEncodedName(), EStatOperation::AdvanceFrameEventRenderThread, TargetFrame, false);
	FRawStatStackNode Root;
	GetRawStackStats(TargetFrame, Root, &OutStats);
	TArray<FStatMessage> StackStats;
	Root.Encode(StackStats);
	OutStats += StackStats;
}

void FStatsThreadState::FindOrAddMetaData(FStatMessage const& Item)
{
	const FName LongName = Item.NameAndInfo.GetRawName();
	const FName ShortName = Item.NameAndInfo.GetShortName();

	FStatMessage* Result = ShortNameToLongName.Find(ShortName);
	if (!Result)
	{
		check(ShortName != LongName);
		FStatMessage AsSet(Item);
		AsSet.Clear();

		const FName GroupName = Item.NameAndInfo.GetGroupName();

		// Whether to add to the threads group.
		const bool bIsThread = FStatConstants::NAME_ThreadGroup == GroupName;
		if( bIsThread )
		{
			// The description of a thread group contains the thread id
			const FString Desc = Item.NameAndInfo.GetDescription();
			Threads.Add( FStatsUtils::ParseThreadID( Desc ), ShortName );
		}

		ShortNameToLongName.Add(ShortName, AsSet); // we want this to be a clear, but it should be a SetLongName
		AsSet.NameAndInfo.SetField<EStatOperation>(EStatOperation::Set);
		check(Item.NameAndInfo.GetField<EStatMetaFlags>());
		Groups.Add(GroupName, ShortName);
		if (GroupName != NAME_Groups && !Item.NameAndInfo.GetFlag(EStatMetaFlags::ShouldClearEveryFrame))
		{
			NotClearedEveryFrame.Add(LongName, AsSet);
		}
		if (Item.NameAndInfo.GetFlag(EStatMetaFlags::IsMemory) && ShortName.ToString().StartsWith(TEXT("MCR_")))
		{
			// this is a pool size
			FPlatformMemory::EMemoryCounterRegion Region = FPlatformMemory::EMemoryCounterRegion(Item.NameAndInfo.GetField<EMemoryRegion>());
			if (MemoryPoolToCapacityLongName.Contains(Region))
			{
				UE_LOG(LogStats, Warning, TEXT("MetaData mismatch. Did you assign a memory pool capacity two different ways? %s vs %s"), *LongName.ToString(), *MemoryPoolToCapacityLongName[Region].ToString());
			}
			else
			{
				MemoryPoolToCapacityLongName.Add(Region, LongName);
			}
		}

		// Add the info to the task graph so we can inform the game thread
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady
		(
			FSimpleDelegateGraphTask::FDelegate::CreateRaw(&FStatGroupGameThreadNotifier::Get(), &FStatGroupGameThreadNotifier::NewData, Item.NameAndInfo)
			, TEXT("StatsGroupToGame")
			, nullptr, ENamedThreads::GameThread
		);
	}
	else
	{
		if (LongName != Result->NameAndInfo.GetRawName())
		{
			UE_LOG(LogStats, Warning, TEXT("MetaData mismatch. Did you assign a stat to two groups? New %s old %s"), *LongName.ToString(), *Result->NameAndInfo.GetRawName().ToString());
		}
	}
}

void FStatsThreadState::AddMissingStats(TArray<FStatMessage>& Dest, TSet<FName> const& EnabledItems) const
{
	TSet<FName> NamesToTry(EnabledItems);
	TMap<FName, int32> NameToIndex;
	for (int32 Index = 0; Index < Dest.Num(); Index++)
	{
		NamesToTry.Remove(Dest[Index].NameAndInfo.GetShortName());
	}

	for (auto It = NamesToTry.CreateConstIterator(); It; ++It)
	{
		FStatMessage const* Zero = ShortNameToLongName.Find(*It);
		if (Zero)
		{
			new (Dest) FStatMessage(*Zero);
		}
	}
}

FString FStatsUtils::DebugPrint(FStatMessage const& Item)
{
	FString Result(TEXT("Invalid"));
	switch (Item.NameAndInfo.GetField<EStatDataType>())
	{
	case EStatDataType::ST_int64:
		if (Item.NameAndInfo.GetFlag(EStatMetaFlags::IsPackedCCAndDuration))
		{
			Result = FString::Printf(TEXT("%.2fms (%4d)"), FPlatformTime::ToMilliseconds(FromPackedCallCountDuration_Duration(Item.GetValue_int64())), FromPackedCallCountDuration_CallCount(Item.GetValue_int64()));
		}
		else if (Item.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle))
		{
			Result = FString::Printf(TEXT("%.2fms"), FPlatformTime::ToMilliseconds(Item.GetValue_int64()));
		}
		else
		{
			Result = FString::Printf(GetStatFormatString<int64>(), Item.GetValue_int64());
		}
		break;
	case EStatDataType::ST_double:
		Result = FString::Printf(GetStatFormatString<double>(), Item.GetValue_double());
		break;
	case EStatDataType::ST_FName:
		Result = Item.GetValue_FName().ToString();
		break;
	}

	Result = FString(FCString::Spc(FMath::Max<int32>(0, 14 - Result.Len()))) + Result;

	FString Desc;
	FName Group;
	FName Category;
	FName ShortName;

	ShortName = Item.NameAndInfo.GetShortName();
	Group = Item.NameAndInfo.GetGroupName();
	Category = Item.NameAndInfo.GetGroupCategory();
	Desc = Item.NameAndInfo.GetDescription();
	Desc.Trim();

	if (Desc.Len())
	{
		Desc += TEXT(" - ");
	}
	Desc += *ShortName.ToString();

	FString GroupStr;
	if (Group != NAME_None)
	{
		GroupStr = TEXT(" - ");
		GroupStr += *Group.ToString();
	}
	if (Category != NAME_None)
	{
		GroupStr = TEXT(" - ");
		GroupStr += *Category.ToString();
	}

	return FString::Printf(TEXT("  %s  -  %s%s"), *Result, *Desc, *GroupStr);
}

void FStatsUtils::AddMergeStatArray(TArray<FStatMessage>& Dest, TArray<FStatMessage> const& Src)
{
	TMap<FName, int32> NameToIndex;
	for (int32 Index = 0; Index < Dest.Num(); Index++)
	{
		NameToIndex.Add(Dest[Index].NameAndInfo.GetRawName(), Index);
	}
	for (int32 Index = 0; Index < Src.Num(); Index++)
	{
		int32* DestIndexPtr = NameToIndex.Find(Src[Index].NameAndInfo.GetRawName());
		int32 DestIndex = -1;
		if (DestIndexPtr)
		{
			DestIndex = *DestIndexPtr;
		}
		else
		{
			DestIndex = Dest.Num();
			NameToIndex.Add(Src[Index].NameAndInfo.GetRawName(), DestIndex);
			FStatMessage NewMessage(Src[Index]);
			NewMessage.Clear();
			Dest.Add(NewMessage);
		}
		AccumulateStat(Dest[DestIndex], Src[Index], EStatOperation::Add);
	}
}

void FStatsUtils::MaxMergeStatArray(TArray<FStatMessage>& Dest, TArray<FStatMessage> const& Src)
{
	TMap<FName, int32> NameToIndex;
	for (int32 Index = 0; Index < Dest.Num(); Index++)
	{
		NameToIndex.Add(Dest[Index].NameAndInfo.GetRawName(), Index);
	}
	for (int32 Index = 0; Index < Src.Num(); Index++)
	{
		int32* DestIndexPtr = NameToIndex.Find(Src[Index].NameAndInfo.GetRawName());
		int32 DestIndex = -1;
		if (DestIndexPtr)
		{
			DestIndex = *DestIndexPtr;
		}
		else
		{
			DestIndex = Dest.Num();
			NameToIndex.Add(Src[Index].NameAndInfo.GetRawName(), DestIndex);
			FStatMessage NewMessage(Src[Index]);
			NewMessage.Clear();
			Dest.Add(NewMessage);
		}
		AccumulateStat(Dest[DestIndex], Src[Index], EStatOperation::MaxVal);
	}
}

void FStatsUtils::DivideStat(FStatMessage& Dest, uint32 Div)
{
	switch (Dest.NameAndInfo.GetField<EStatDataType>())
	{
	case EStatDataType::ST_int64:
		if (Dest.NameAndInfo.GetFlag(EStatMetaFlags::IsPackedCCAndDuration))
		{
			Dest.GetValue_int64() = ToPackedCallCountDuration(
				(FromPackedCallCountDuration_CallCount(Dest.GetValue_int64()) + (Div >> 1)) / Div,
				(FromPackedCallCountDuration_Duration(Dest.GetValue_int64()) + (Div >> 1)) / Div);
		}
		else if (Dest.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle))
		{
			Dest.GetValue_int64() = (Dest.GetValue_int64() + Div - 1) / Div;
		}
		else
		{
			int64 Val = Dest.GetValue_int64();
			Dest.NameAndInfo.SetField<EStatDataType>(EStatDataType::ST_double);
			Dest.GetValue_double() = (double)(Val) / (double)Div;
		}
		break;
	case EStatDataType::ST_double:
		Dest.GetValue_double() /= (double)Div;
		break;
	default:
		check(0);
	}
}

void FStatsUtils::DivideStatArray(TArray<FStatMessage>& DestArray, uint32 Div)
{
	for (int32 Index = 0; Index < DestArray.Num(); Index++)
	{
		FStatMessage &Dest = DestArray[Index];
		DivideStat(Dest, Div);
	}
}

void FStatsUtils::AccumulateStat(FStatMessage& Dest, FStatMessage const& Item, EStatOperation::Type Op /*= EStatOperation::Invalid*/, bool bAllowNameMismatch /*= false*/)
{
	check(bAllowNameMismatch || Dest.NameAndInfo.GetRawName() == Item.NameAndInfo.GetRawName());

	if (Op == EStatOperation::Invalid)
	{
		Op = Item.NameAndInfo.GetField<EStatOperation>();
	}
	check(Dest.NameAndInfo.GetField<EStatDataType>() == Item.NameAndInfo.GetField<EStatDataType>());
	check(Dest.NameAndInfo.GetFlag(EStatMetaFlags::IsPackedCCAndDuration) == Item.NameAndInfo.GetFlag(EStatMetaFlags::IsPackedCCAndDuration));
	switch (Item.NameAndInfo.GetField<EStatDataType>())
	{
	case EStatDataType::ST_int64:
		switch (Op)
		{
		case EStatOperation::Set:
			Dest.GetValue_int64() = Item.GetValue_int64();
			break;
		case EStatOperation::Clear:
			Dest.GetValue_int64() = 0;
			break;
		case EStatOperation::Add:
			Dest.GetValue_int64() += Item.GetValue_int64();
			break;
		case EStatOperation::Subtract:
			if (Dest.NameAndInfo.GetFlag(EStatMetaFlags::IsPackedCCAndDuration))
			{
				// we don't subtract call counts, only times
				Dest.GetValue_int64() = ToPackedCallCountDuration(
					FromPackedCallCountDuration_CallCount(Dest.GetValue_int64()),
					FromPackedCallCountDuration_Duration(Dest.GetValue_int64()) - FromPackedCallCountDuration_Duration(Item.GetValue_int64()));
			}
			else
			{
				Dest.GetValue_int64() -= Item.GetValue_int64();
			}
			break;
		case EStatOperation::MaxVal:
			StatOpMaxVal_Int64( Dest.NameAndInfo, Dest.GetValue_int64(), Item.GetValue_int64() );
			break;
		default:
			check(0);
		}
		break;
	case EStatDataType::ST_double:
		switch (Op)
		{
		case EStatOperation::Set:
			Dest.GetValue_double() = Item.GetValue_double();
			break;
		case EStatOperation::Clear:
			Dest.GetValue_double() = 0;
			break;
		case EStatOperation::Add:
			Dest.GetValue_double() += Item.GetValue_double();
			break;
		case EStatOperation::Subtract:
			Dest.GetValue_double() -= Item.GetValue_double();
			break;
		case EStatOperation::MaxVal:
			Dest.GetValue_double() = FMath::Max<double>(Dest.GetValue_double(), Item.GetValue_double());
			break;
		default:
			check(0);
		}
		break;
	default:
		check(0);
	}
}

FString FStatsUtils::FromEscapedFString(const TCHAR* Escaped)
{
	FString Result;
	FString Input(Escaped);
	while (Input.Len())
	{
		{
			int32 Index = Input.Find(TEXT("$"));
			if (Index == INDEX_NONE)
			{
				Result += Input;
				break;
			}
			Result += Input.Left(Index);
			Input = Input.RightChop(Index + 1);

		}
		{
			int32 IndexEnd = Input.Find(TEXT("$"));
			if (IndexEnd == INDEX_NONE)
			{
				checkStats(0); // malformed escaped fname
				Result += Input;
				break;
			}
			FString Number = Input.Left(IndexEnd);
			Input = Input.RightChop(IndexEnd + 1);
			Result.AppendChar(TCHAR(uint32(FCString::Atoi64(*Number))));
		}
	}
	return Result;
}

FString FStatsUtils::ToEscapedFString(const TCHAR* Source)
{
	FString Invalid(INVALID_NAME_CHARACTERS);
	Invalid += TEXT("$");

	FString Output;
	FString Input(Source);
	int32 StartValid = 0;
	int32 NumValid = 0;

	for (int32 i = 0; i < Input.Len(); i++)
	{
		int32 Index = 0;
		if (!Invalid.FindChar(Input[i], Index))
		{
			NumValid++;
		}
		else
		{
			// Copy the valid range so far
			Output += Input.Mid(StartValid, NumValid);

			// Reset valid ranges
			StartValid = i + 1;
			NumValid = 0;

			// Replace the invalid character with a special string
			Output += FString::Printf(TEXT("$%u$"), uint32(Input[i]));
		}
	}

	// Just return the input if the entire string was valid
	if (StartValid == 0 && NumValid == Input.Len())
	{
		return Input;
	}
	else if (NumValid > 0)
	{
		// Copy the remaining valid part
		Output += Input.Mid(StartValid, NumValid);
	}
	return Output;
}


void FComplexStatUtils::AddAndMax( FComplexStatMessage& Dest, const FStatMessage& Item, EComplexStatField::Type SumIndex, EComplexStatField::Type MaxIndex )
{
	check(Dest.NameAndInfo.GetRawName() == Item.NameAndInfo.GetRawName());

	// Copy the data type from the other stack node.
	if( Dest.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_None )
	{
		Dest.NameAndInfo.SetField<EStatDataType>( Item.NameAndInfo.GetField<EStatDataType>() );
	}

	const EStatDataType::Type StatDataType = Dest.NameAndInfo.GetField<EStatDataType>();

	// Total time.
	if( StatDataType != EStatDataType::ST_None && StatDataType != EStatDataType::ST_FName )
	{
		if( StatDataType == EStatDataType::ST_int64 )
		{
			Dest.GetValue_int64(SumIndex) += Item.GetValue_int64();
		}
		else if( StatDataType == EStatDataType::ST_double )
		{
			Dest.GetValue_double(SumIndex) += Item.GetValue_double();
		}
	}

	// Maximum time.
	if( StatDataType != EStatDataType::ST_None && StatDataType != EStatDataType::ST_FName)
	{
		if( StatDataType == EStatDataType::ST_int64 )
		{
			FStatsUtils::StatOpMaxVal_Int64( Dest.NameAndInfo, Dest.GetValue_int64(MaxIndex), Item.GetValue_int64() );
		}
		else if( StatDataType == EStatDataType::ST_double )
		{
			Dest.GetValue_double(MaxIndex) = FMath::Max<double>(Dest.GetValue_double(MaxIndex), Item.GetValue_double());
		}
	}
}

void FComplexStatUtils::DivideStat( FComplexStatMessage& Dest, uint32 Div, EComplexStatField::Type SumIndex, EComplexStatField::Type DestIndex )
{
	switch (Dest.NameAndInfo.GetField<EStatDataType>())
	{
	case EStatDataType::ST_int64:
		if (Dest.NameAndInfo.GetFlag(EStatMetaFlags::IsPackedCCAndDuration))
		{
			const int64 PackedCCAndDuration = ToPackedCallCountDuration(
				(FromPackedCallCountDuration_CallCount(Dest.GetValue_int64(SumIndex)) + (Div >> 1)) / Div,
				(FromPackedCallCountDuration_Duration(Dest.GetValue_int64(SumIndex)) + (Div >> 1)) / Div);

			Dest.GetValue_int64(DestIndex) = PackedCCAndDuration;
		}
		else if (Dest.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle))
		{
			Dest.GetValue_int64(DestIndex) = (Dest.GetValue_int64(SumIndex) + Div - 1) / Div;
		}
		else
		{
			const int64 Val = Dest.GetValue_int64(SumIndex);

			// Stat data type has change, we need to convert remaining fields to the new data type.
			Dest.FixStatData( EStatDataType::ST_double );

			Dest.GetValue_double(DestIndex) = (double)(Val) / (double)Div;
		}
		break;
	case EStatDataType::ST_double:
		Dest.GetValue_double(DestIndex) = Dest.GetValue_double(SumIndex) / (double)Div;
		break;
	}
}

void FComplexStatUtils::MergeAddAndMaxArray( TArray<FComplexStatMessage>& Dest, const TArray<FStatMessage>& Source, EComplexStatField::Type SumIndex, EComplexStatField::Type MaxIndex )
{
	TMap<FName, int32> NameToIndex;
	for( int32 Index = 0; Index < Dest.Num(); Index++ )
	{
		const FName RawName = Dest[Index].NameAndInfo.GetRawName();
		NameToIndex.Add( RawName, Index );
	}

	for( int32 Index = 0; Index < Source.Num(); Index++ )
	{
		const int32& DestIndex = NameToIndex.FindChecked( Source[Index].NameAndInfo.GetRawName() );
		FComplexStatUtils::AddAndMax( Dest[DestIndex], Source[Index], SumIndex, MaxIndex );
	}
}

void FComplexStatUtils::DiviveStatArray( TArray<FComplexStatMessage>& Dest, uint32 Div, EComplexStatField::Type SumIndex, EComplexStatField::Type DestIndex )
{
	for( int32 Index = 0; Index < Dest.Num(); Index++ )
	{
		FComplexStatMessage &Aggregated = Dest[Index];
		FComplexStatUtils::DivideStat( Aggregated, Div, SumIndex, DestIndex );
	}
}

/** Broadcast the name and info data about any newly registered stat groups */
CORE_API void CheckForRegisteredStatGroups()
{
	FStatGroupGameThreadNotifier::Get().SendData();
}

/** Clear the data that's pending to be sent to prevent it accumulating when not claimed by a delegate */
CORE_API void ClearPendingStatGroups()
{
	FStatGroupGameThreadNotifier::Get().ClearData();
}

#endif