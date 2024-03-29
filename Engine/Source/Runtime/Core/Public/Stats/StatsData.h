// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	StatsData.h: Performance stats framework.
=============================================================================*/
#pragma once

#if STATS

/**
* Used for a few different things, but roughly one more than the maximum render thread game thread lag, in frames.
*/
enum
{
	STAT_FRAME_SLOP = 3,
	MAX_STAT_LAG = 4,
};

/** Holds stats related constants. */
struct CORE_API FStatConstants
{
	/** Special name for thread root. */
	static const FName NAME_ThreadRoot;

	/** This is a special group name used to store threads metadata. */
	static const char* ThreadGroupName;
	static const FName NAME_ThreadGroup;

	/** Special case category, when we want to Stat to appear at the root of the menu (leaving the category blank omits it from the menu entirely) */
	static const FName NAME_NoCategory;

	/** Extension used to save a stats file. */
	static const FString StatsFileExtension;

	/** Extension used to save a raw stats file, may be changed to the same as a regular stats file. */
	static const FString StatsFileRawExtension;

	/** Indicates that the item is a thread. */
	static const FString ThreadNameMarker;
};

/** Enumerates stat compare types. */
struct EStatCompareBy
{
	enum Type
	{
		/** By stat name. */
		Name,
		/** By call count, only for scoped cycle counters. */
		CallCount,
		/** By total accumulated value. */
		Sum,
	};
};

/** Stat display mode. */
struct EStatDisplayMode
{
	enum Type
	{
		Invalid			= 0x0,
		Hierarchical	= 0x1,
		Flat			= 0x2,
	};
};

/** Sort predicate for alphabetic ordering. */
template< typename T >
struct FStatNameComparer
{
	FORCEINLINE_STATS bool operator()( const T& A, const T& B ) const
	{
		check(0);
		return 0;
	}
};

/** Sort predicate with the slowest inclusive time first. */
template< typename T >
struct FStatDurationComparer
{
	FORCEINLINE_STATS bool operator()( const T& A, const T& B ) const
	{
		check(0);
		return 0;
	}
};

/** Sort predicate with the lowest call count first.  */
template< typename T >
struct FStatCallCountComparer
{
	FORCEINLINE_STATS bool operator()( const T& A, const T& B ) const
	{
		check(0);
		return 0;
	}
};

/** Sort predicate for alphabetic ordering, specialized for FStatMessage. */
template<>
struct FStatNameComparer<FStatMessage>
{
	FORCEINLINE_STATS bool operator()( const FStatMessage& A, const FStatMessage& B ) const
	{
		return A.NameAndInfo.GetRawName().Compare( B.NameAndInfo.GetRawName() ) < 0;
	}
};

/** Sort predicate with the slowest inclusive time first, specialized for FStatMessage. */
template<>
struct FStatDurationComparer<FStatMessage>
{
	FORCEINLINE_STATS bool operator()( const FStatMessage& A, const FStatMessage& B ) const
	{
		const uint32 DurationA = FromPackedCallCountDuration_Duration( A.GetValue_int64() );
		const uint32 DurationB = FromPackedCallCountDuration_Duration( B.GetValue_int64() );
		return DurationA == DurationB ? FStatNameComparer<FStatMessage>()(A,B) : DurationA > DurationB;
	}
};

/** Sort predicate with the lowest call count first, specialized for FStatMessage. */
template<>
struct FStatCallCountComparer<FStatMessage>
{
	FORCEINLINE_STATS bool operator()( const FStatMessage& A, const FStatMessage& B ) const
	{
		const uint32 CallCountA = FromPackedCallCountDuration_CallCount( A.GetValue_int64() );
		const uint32 CallCountB = FromPackedCallCountDuration_CallCount( B.GetValue_int64() );
		return CallCountA == CallCountB ? FStatNameComparer<FStatMessage>()(A,B) : CallCountA > CallCountB;
	}
};

/**
* An indirect array of stat packets.
*/
//@todo can we just use TIndirectArray here?
struct FStatPacketArray
{
	TArray<FStatPacket*> Packets;

	~FStatPacketArray()
	{
		Empty();
	}

	void Empty()
	{
		for (int32 Index = 0; Index < Packets.Num(); Index++)
		{
			delete Packets[Index];
		}
		Packets.Empty();
	}
};

/**
* A call stack of stat messages. Normally we don't store or transmit these; they are used for stats processing and visualization.
*/
struct CORE_API FRawStatStackNode
{
	/** The stat message corresponding to this node **/
	FStatMessage Meta; // includes aggregated inclusive time and call counts packed into the int64

	/** Map from long name to children of this node **/
	TMap<FName, FRawStatStackNode*> Children;

	/** Constructor, this always and only builds the thread root node. The thread root is not a numeric stat! **/
	FRawStatStackNode()
		: Meta(FStatConstants::NAME_ThreadRoot, EStatDataType::ST_None, nullptr, nullptr, nullptr, false, false)
	{
	}

	/** Copy Constructor **/
	explicit FRawStatStackNode(FRawStatStackNode const& Other);

	/** Constructor used to build a child from a stat messasge **/
	explicit FRawStatStackNode(FStatMessage const& InMeta)
		: Meta(InMeta)
	{
	}

	/** Assignment operator. */
	FRawStatStackNode& operator=(const FRawStatStackNode& Other)
	{
		if( this != &Other )
		{
			DeleteAllChildrenNodes();
			Meta = Other.Meta;

			Children.Empty(Other.Children.Num());
			for (TMap<FName, FRawStatStackNode*>::TConstIterator It(Other.Children); It; ++It)
			{
				Children.Add(It.Key(), new FRawStatStackNode(*It.Value()));
			}
		}
		return *this;
	}

	/** Destructor, clean up the memory and children. **/
	~FRawStatStackNode()
	{
		DeleteAllChildrenNodes();
	}

	void MergeMax(FRawStatStackNode const& Other);
	void MergeAdd(FRawStatStackNode const& Other);
	void Divide(uint32 Div);

	/** Cull this tree, merging children below MinCycles long **/
	void Cull(int64 MinCycles, int32 NoCullLevels = 0);

	/** Adds name hiearchy. **/
	void AddNameHierarchy(int32 CurrentPrefixDepth = 0);

	/** Adds self nodes. **/
	void AddSelf();

	/** Print this tree to the log **/
	void DebugPrint(TCHAR const* Filter = NULL, int32 MaxDepth = MAX_int32, int32 Depth = 0) const;

	/** Condense this tree into a flat list using EStatOperation::ChildrenStart, EStatOperation::ChildrenEnd, EStatOperation::Leaf **/
	void Encode(TArray<FStatMessage>& OutStats) const;

	/** Sum the inclusive cycles of the children **/
	int64 ChildCycles() const;

	/** Sorts children using the specified comparer class. */
	template< class TComparer >
	void Sort( const TComparer& Comparer )
	{
		Children.ValueSort( Comparer );
		for( auto It = Children.CreateIterator(); It; ++It )
		{
			FRawStatStackNode* Child = It.Value();
			Child->Sort( Comparer );
		}
	}

	/** Custom new/delete; these nodes are always pooled, except the root which one normally puts on the stack. */
	void* operator new(size_t Size);
	void operator delete(void *RawMemory);

protected:
	void DeleteAllChildrenNodes()
	{
		for (TMap<FName, FRawStatStackNode*>::TIterator It(Children); It; ++It)
		{
			delete It.Value();
		}
	}
};

/** Sort predicate for alphabetic ordering, specialized for FRawStatStackNode. */
template<>
struct FStatNameComparer<FRawStatStackNode>
{
	FORCEINLINE_STATS bool operator()( const FRawStatStackNode& A, const FRawStatStackNode& B ) const
	{
		return A.Meta.NameAndInfo.GetRawName().Compare( B.Meta.NameAndInfo.GetRawName() ) < 0;
	}
};

/** Sort predicate with the slowest inclusive time first, specialized for FRawStatStackNode. */
template<>
struct FStatDurationComparer<FRawStatStackNode>
{
	FORCEINLINE_STATS bool operator()( const FRawStatStackNode& A, const FRawStatStackNode& B ) const
	{
		const uint32 DurationA = FromPackedCallCountDuration_Duration( A.Meta.GetValue_int64() );
		const uint32 DurationB = FromPackedCallCountDuration_Duration( B.Meta.GetValue_int64() );
		return DurationA == DurationB ? FStatNameComparer<FRawStatStackNode>()(A,B) : DurationA > DurationB;
	}
};

/** Sort predicate with the lowest call count first, specialized for FRawStatStackNode. */
template<>
struct FStatCallCountComparer<FRawStatStackNode>
{
	FORCEINLINE_STATS bool operator()( const FRawStatStackNode& A, const FRawStatStackNode& B ) const
	{
		const uint32 CallCountA = FromPackedCallCountDuration_CallCount( A.Meta.GetValue_int64() );
		const uint32 CallCountB = FromPackedCallCountDuration_CallCount( B.Meta.GetValue_int64() );
		return CallCountA == CallCountB ? FStatNameComparer<FRawStatStackNode>()(A,B) : CallCountA > CallCountB;
	}
};

/** Same as the FRawStatStackNode, but for the FComplexStatMessage. */
struct FComplexRawStatStackNode
{
	/** Complex stat. */
	FComplexStatMessage ComplexStat;

	/** Map from long name to children of this node **/
	TMap<FName, FComplexRawStatStackNode*> Children;

	/** Default constructor. */
	FComplexRawStatStackNode()
	{}

	/** Copy constructor. */
	explicit FComplexRawStatStackNode(FComplexRawStatStackNode const& Other);

	/** Copy constructor. */
	explicit FComplexRawStatStackNode(FRawStatStackNode const& Other);

	void CopyNameHierarchy( const FRawStatStackNode& Other )	
	{
		DeleteAllChildrenNodes();

		ComplexStat = Other.Meta;

		Children.Empty(Other.Children.Num());
		for (auto It = Other.Children.CreateConstIterator(); It; ++It)
		{
			Children.Add(It.Key(), new FComplexRawStatStackNode(*It.Value()));
		}
	}

public:
	/** Destructor, clean up the memory and children. **/
	~FComplexRawStatStackNode()
	{
		DeleteAllChildrenNodes();
	}

	/** Merges this stack with the other. */
	void MergeAddAndMax( const FRawStatStackNode& Other );

	/** Divides this stack by the specified value. */
	void Divide(uint32 Div);

	/** Copies exclusive times from the self node. **/
	void CopyExclusivesFromSelf();

	/** Custom new/delete; these nodes are always pooled, except the root which one normally puts on the stack. */
	void* operator new(size_t Size);
	void operator delete(void *RawMemory);

protected:
	void DeleteAllChildrenNodes()
	{
		for (auto It = Children.CreateIterator(); It; ++It)
		{
			delete It.Value();
		}
	}
};

//@todo split header

/** Delegate that FStatsThreadState calls on the stats thread whenever we have a new frame. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnNewFrameHistory, int64);

/**
* Some of the FStatsThreadState data methods allow filtering. Derive your filter from this class
*/
struct IItemFiler
{
	/** return true if you want to keep the item for the purposes of collecting stats **/
	virtual bool Keep(FStatMessage const& Item) = 0;
};

// @TODO yrx 2014-03-21 Move metadata functionality into a separate class
/**
* Tracks stat state and history
*  GetLocalState() is a singleton to the state for stats being collected in this executable.
*  Other instances can be used to load stats for visualization.
*/
class CORE_API FStatsThreadState
{
	friend class FStatsThread;

	/** Internal method to scan the messages to update the current frame **/
	void ScanForAdvance(TArray<FStatMessage> const& Data);

	/** Internal method to scan the messages to update the current frame and accumulate any non-frame stats. **/
	void ScanForAdvance(FStatPacketArray& NewData);

	/** Internal method to scan the messages to accumulate any non-frame stats. **/
	void ProcessMetaDataForLoad(TArray<FStatMessage>& Data);

public:
	/** Internal method to add meta data packets to the data structures. **/
	void ProcessMetaDataOnly(TArray<FStatMessage>& Data);

	/** Marks this stats state as loaded. */
	void MarkAsLoaded()
	{
		bWasLoaded =  true;
	}

private:
	/** Internal method to scan the messages to accumulate any non-frame stats. **/
	void ProcessNonFrameStats(TArray<FStatMessage>& Data, TSet<FName>* NonFrameStatsFound);

	/** Internal method to place the data into the history, discard and broadcast any new frames to anyone who cares. **/
	void AddToHistoryAndEmpty(FStatPacketArray& NewData);

	/** Called in response to SetLongName messages to update ShortNameToLongName and NotClearedEveryFrame **/
	void FindOrAddMetaData(FStatMessage const& Item);

	/** Parse the raw frame history into a tree representation **/
	void GetRawStackStats(int64 FrameNumber, FRawStatStackNode& Out, TArray<FStatMessage>* OutNonStackStats = NULL) const;

	/** Parse the raw frame history into a flat-array-based tree representation **/
	void Condense(int64 TargetFrame, TArray<FStatMessage>& OutStats) const;

	/** Returns cycles spent in the specified thread. */
	int64 GetFastThreadFrameTimeInternal(int64 TargetFrame, int32 ThreadID, EThreadType::Type Thread) const;

	/** Number of frames to keep in the history. **/
	int32 HistoryFrames;

	/** Largest frame seen. Loaded stats only. **/
	int64 MaxFrameSeen;

	/** First frame seen. Loaded stats only. **/
	int64 MinFrameSeen;

	/** Used to track which packets have been sent to listeners **/
	int64 LastFullFrameMetaAndNonFrame;

	/** Used to track which packets have been sent to listeners **/
	int64 LastFullFrameProcessed;

	/** Valid frame computation is different if we just loaded these stats **/
	bool bWasLoaded;

	/** Cached condensed frames. This saves a lot of time since multiple listeners can use the same condensed data **/
	mutable TMap<int64, TArray<FStatMessage>* > CondensedStackHistory;

	/** After we add to the history, these frames are known good. **/
	TSet<int64>	GoodFrames;

	/** These frames are known bad. **/
	TSet<int64>	BadFrames;

public:

	/** Constructor used by GetLocalState(), also used by the profiler to hold a previewing stats thread state. We don't keep many frames by default **/
	FStatsThreadState(int32 InHistoryFrames = STAT_FRAME_SLOP + 2);

	/** Constructor to load stats from a file **/
	FStatsThreadState(FString const& Filename);

	/** Delegate we fire every time we have a new complete frame of data. **/
	// @TODO yrx 2014-03-21 Not thread-safe ?
	mutable FOnNewFrameHistory NewFrameDelegate;

	/** New packets not on the render thread as assign this frame **/
	int64 CurrentGameFrame;

	/** New packets on the render thread as assign this frame **/
	int64 CurrentRenderFrame;

	/** List of the stats, with data of all of the things that are NOT cleared every frame. We need to accumulate these over the entire life of the executable. **/
	TMap<FName, FStatMessage> NotClearedEveryFrame;

	/** Map from a short name to the SetLongName message that defines the metadata and long name for this stat **/
	TMap<FName, FStatMessage> ShortNameToLongName;

	/** Map from memory pool to long name**/
	TMap<FPlatformMemory::EMemoryCounterRegion, FName> MemoryPoolToCapacityLongName;

	/** Defines the groups. the group called "Groups" can be used to enumerate the groups. **/
	TMultiMap<FName, FName> Groups;

	/** Map from a thread id to the thread name. */
	TMap<uint32, FName> Threads;

	/** Raw data for a frame. **/
	TMap<int64, FStatPacketArray> History;

	/** Return the oldest frame of data we have. **/
	int64 GetOldestValidFrame() const;

	/** Return the newest frame of data we have. **/
	int64 GetLatestValidFrame() const;

	/** Return true if this is a valid frame. **/
	bool IsFrameValid(int64 Frame) const
	{
		return GoodFrames.Contains(Frame);
	}

	/** Used by the hitch detector. Does a very fast look a thread to decide if this is a hitch frame. **/
	int64 GetFastThreadFrameTime(int64 TargetFrame, EThreadType::Type Thread) const;
	int64 GetFastThreadFrameTime(int64 TargetFrame, uint32 ThreadID) const;

	/** For the specified stat packet looks for the thread name. */
	FName GetStatThreadName( const FStatPacket& Packet ) const;

	/** Looks in the history for a condensed frame, builds it if it isn't there. **/
	TArray<FStatMessage> const& GetCondensedHistory(int64 TargetFrame) const;

	/** Looks in the history for a stat packet array, the stat packet array must be in the history. */
	const FStatPacketArray& GetStatPacketArray( int64 TargetFrame ) const
	{
		check( IsFrameValid( TargetFrame ) );
		const FStatPacketArray& Frame = History.FindChecked( TargetFrame );
		return Frame;
	}

	/** Gets the old-skool flat grouped inclusive stats. These ignore recursion, merge threads, etc and so generally the condensed callstack is less confusing. **/
	void GetInclusiveAggregateStackStats(int64 TargetFrame, TArray<FStatMessage>& OutStats, IItemFiler* Filter = NULL, bool bAddNonStackStats = true) const;

	/** Gets the old-skool flat grouped exclusive stats. These merge threads, etc and so generally the condensed callstack is less confusing. **/
	void GetExclusiveAggregateStackStats(int64 TargetFrame, TArray<FStatMessage>& OutStats, IItemFiler* Filter = NULL, bool bAddNonStackStats = true) const;

	/** Used to turn the condensed version of stack stats back into a tree for easier handling. **/
	void UncondenseStackStats(int64 TargetFrame, FRawStatStackNode& Out, IItemFiler* Filter = NULL, TArray<FStatMessage>* OutNonStackStats = NULL) const;

	/** Adds missing stats to the group so it doesn't jitter. **/
	void AddMissingStats(TArray<FStatMessage>& Dest, TSet<FName> const& EnabledItems) const;

	/** Adds a frame worth of messages */
	void AddMessages(TArray<FStatMessage>& InMessages);

	/** Singleton to get the stats being collected by this executable **/
	static FStatsThreadState& GetLocalState();
};

//@todo split header

/**
* Set of utility functions for dealing with stats
*/
struct CORE_API FStatsUtils
{
	/** Divides a stat by an integer. Used to finish an average. **/
	static void DivideStat(FStatMessage& Dest, uint32 Div);

	/** Merges two stat message arrays, adding corresponding number values **/
	static void AddMergeStatArray(TArray<FStatMessage>& Dest, TArray<FStatMessage> const& Item);

	/** Merges two stat message arrays, corresponding number values are set to the max **/
	static void MaxMergeStatArray(TArray<FStatMessage>& Dest, TArray<FStatMessage> const& Item);

	/** Divides a stat array by an integer. Used to finish an average. **/
	static void DivideStatArray(TArray<FStatMessage>& Dest, uint32 Div);

	/** Apply Op or Item.Op to Item and Dest, soring the result in dest. **/
	static void AccumulateStat(FStatMessage& Dest, FStatMessage const& Item, EStatOperation::Type Op = EStatOperation::Invalid, bool bAllowNameMismatch = false);

	/** Adds a non-stack stats */
	static void AddNonStackStats( const FName LongName, const FStatMessage& Item, const EStatOperation::Type Op, TMap<FName, FStatMessage>& out_NonStackStats )
	{
		if (
			Item.NameAndInfo.GetField<EStatDataType>() != EStatDataType::ST_None && 
			Item.NameAndInfo.GetField<EStatDataType>() != EStatDataType::ST_FName &&
			(Op == EStatOperation::Set ||
			Op == EStatOperation::Clear ||
			Op == EStatOperation::Add ||
			Op == EStatOperation::Subtract ||
			Op == EStatOperation::MaxVal))
		{
			FStatMessage* Result = out_NonStackStats.Find(LongName);
			if (!Result)
			{
				Result = &out_NonStackStats.Add(LongName, Item);
				Result->NameAndInfo.SetField<EStatOperation>(EStatOperation::Set);
				Result->Clear();
			}
			FStatsUtils::AccumulateStat(*Result, Item);
		}
	}

	/** Spews a stat message, not all messages are supported, this is used for reports, so it focuses on numeric output, not say the operation. **/
	static FString DebugPrint(FStatMessage const& Item);

	/** Subtract a CycleScopeStart from a CycleScopeEnd to create a IsPackedCCAndDuration with the call count and inclusive cycles. **/
	static FStatMessage ComputeCall(FStatMessage const& ScopeStart, FStatMessage const& ScopeEnd)
	{
		checkStats(ScopeStart.NameAndInfo.GetField<EStatOperation>() == EStatOperation::CycleScopeStart);
		checkStats(ScopeEnd.NameAndInfo.GetField<EStatOperation>() == EStatOperation::CycleScopeEnd);
		FStatMessage Result(ScopeStart);
		Result.NameAndInfo.SetField<EStatOperation>(EStatOperation::Set);
		Result.NameAndInfo.SetFlag(EStatMetaFlags::IsPackedCCAndDuration, true);
		checkStats(ScopeEnd.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle));

		// cycles can wrap and are actually uint32's so we do something special here
		int64 Delta = int32(uint32(ScopeEnd.GetValue_int64()) - uint32(ScopeStart.GetValue_int64()));
		checkStats(Delta >= 0);
		Result.GetValue_int64() = ToPackedCallCountDuration(1, uint32(Delta));
		return Result;
	}

	/** Finds a maximum for int64 based stat data. */
	static void StatOpMaxVal_Int64( const FStatNameAndInfo& DestNameAndInfo, int64& Dest, const int64& Other )
	{
		if (DestNameAndInfo.GetFlag(EStatMetaFlags::IsPackedCCAndDuration))
		{
			Dest = ToPackedCallCountDuration(
				FMath::Max<uint32>(FromPackedCallCountDuration_CallCount(Dest), FromPackedCallCountDuration_CallCount(Other)),
				FMath::Max<uint32>(FromPackedCallCountDuration_Duration(Dest), FromPackedCallCountDuration_Duration(Other)));
		}
		else
		{
			Dest = FMath::Max<int64>(Dest,Other);
		}
	}

	/** Internal use, converts arbitrary string to and from an escaped notation for storage in an FName. **/
	static FString ToEscapedFString(const TCHAR* Source);
	static FString FromEscapedFString(const TCHAR* Escaped);
	
	static FString BuildUniqueThreadName( uint32 InThreadID )
	{
		// Make unique name.
		return FString::Printf( TEXT( "%s%x_0" ), *FStatConstants::ThreadNameMarker, InThreadID );
	}

	static int32 ParseThreadID( const FString& InThreadName, FString* out_ThreadName = nullptr )
	{
		// Extract the thread id.
		const FString ThreadName = InThreadName.Replace( TEXT( "_0" ), TEXT( "" ) );
		int32 Index = 0;
		ThreadName.FindLastChar(TEXT('_'),Index);

		// Parse the thread name if requested.
		if( out_ThreadName )
		{
			*out_ThreadName = ThreadName.Left( Index );
		}

		const FString ThreadIDStr = ThreadName.RightChop(Index+1);
		const uint32 ThreadID = FParse::HexNumber( *ThreadIDStr );

		return ThreadID;
	}
};

/**
* Contains helpers functions to manage complex stat messages.
*/
struct FComplexStatUtils
{
	/** Accumulates a stat message into a complex stat message. */
	static void AddAndMax( FComplexStatMessage& Dest, const FStatMessage& Item, EComplexStatField::Type SumIndex, EComplexStatField::Type MaxIndex );

	/** Divides a complex stat message by the specified value. */
	static void DivideStat( FComplexStatMessage& Dest, uint32 Div, EComplexStatField::Type SumIndex, EComplexStatField::Type DestIndex );

	/** Merges two complex stat message arrays. */
	static void MergeAddAndMaxArray( TArray<FComplexStatMessage>& Dest, const TArray<FStatMessage>& Source, EComplexStatField::Type SumIndex, EComplexStatField::Type MaxIndex );

	/** Divides a complex stat array by the specified value. */
	static void DiviveStatArray( TArray<FComplexStatMessage>& Dest, uint32 Div, EComplexStatField::Type SumIndex, EComplexStatField::Type DestIndex );
};

//@todo split header

/**
* Predicate to sort stats into reverse order of definition, which historically is how people specified a preferred order.
*/
struct FGroupSort
{
	FORCEINLINE bool operator()( FStatMessage const& A, FStatMessage const& B ) const 
	{ 
		FName GroupA = A.NameAndInfo.GetGroupName();
		FName GroupB = B.NameAndInfo.GetGroupName();
		// first sort by group
		if (GroupA == GroupB)
		{
			// cycle stats come first
			if (A.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle) && !B.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle))
			{
				return true;
			}
			if (!A.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle) && B.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle))
			{
				return false;
			}
			// then memory
			if (A.NameAndInfo.GetFlag(EStatMetaFlags::IsMemory) && !B.NameAndInfo.GetFlag(EStatMetaFlags::IsMemory))
			{
				return true;
			}
			if (!A.NameAndInfo.GetFlag(EStatMetaFlags::IsMemory) && B.NameAndInfo.GetFlag(EStatMetaFlags::IsMemory))
			{
				return false;
			}
			// otherwise, reverse order of definition
			return A.NameAndInfo.GetRawName().GetIndex() > B.NameAndInfo.GetRawName().GetIndex();
		}
		if (GroupA == NAME_None)
		{
			return false;
		}
		if (GroupB == NAME_None)
		{
			return true;
		}
		return GroupA.GetIndex() > GroupB.GetIndex();
	}
};


/** Holds stats data used for displayed on the hud. */
struct FHudGroup
{
	/** Array of all flat aggregates for the last n frames. */
	TArray<FComplexStatMessage> FlatAggregate;
	
	/** Array of all aggregates for the last n frames. */
	TArray<FComplexStatMessage> HierAggregate;

	/** Array of indentations for the history stack, must match the size. */
	TArray<int32> Indentation;

	/** Memory aggregates. */
	TArray<FComplexStatMessage> MemoryAggregate;

	/** Counters aggregates. */
	TArray<FComplexStatMessage> CountersAggregate;
};

/**
* Information sent from the stats thread to the game thread to render on the HUD
*/
struct FGameThreadHudData
{
	TIndirectArray<FHudGroup> HudGroups;
	TArray<FName> GroupNames;
	TArray<FString> GroupDescriptions;
	TMap<FPlatformMemory::EMemoryCounterRegion, int64> PoolCapacity;
	TMap<FPlatformMemory::EMemoryCounterRegion, FString> PoolAbbreviation;
};

/**
* Singleton that holds the last data sent from the stats thread to the game thread for HUD stats
*/
struct CORE_API FHUDGroupGameThreadRenderer 
{
	FGameThreadHudData* Latest;

	FHUDGroupGameThreadRenderer()
		: Latest(nullptr)
	{
	}

	~FHUDGroupGameThreadRenderer()
	{
		delete Latest;
		Latest = nullptr;
	}

	void NewData(FGameThreadHudData* Data)
	{
		delete Latest;
		Latest = Data;
	}

	static FHUDGroupGameThreadRenderer& Get();
};

/**
 * Singleton that holds a list of newly registered group stats to inform the game thread of
 */
struct CORE_API FStatGroupGameThreadNotifier
{
public:
	static FStatGroupGameThreadNotifier& Get();

	void NewData(FStatNameAndInfo NameAndInfo)
	{
		NameAndInfos.Add(NameAndInfo);
	}

	void SendData()
	{
		if (NameAndInfos.Num() > 0)
		{
			// Should only do this if the delegate has been registered!
			check(NewStatGroupDelegate.IsBound());
			NewStatGroupDelegate.Execute(NameAndInfos);
			ClearData();
		}
	}

	void ClearData()
	{
		NameAndInfos.Empty();
	}

	/** Delegate we fire every time new stat groups have been registered */
	DECLARE_DELEGATE_OneParam(FOnNewStatGroupRegistered, const TArray<FStatNameAndInfo>&);
	FOnNewStatGroupRegistered NewStatGroupDelegate;

private:
	FStatGroupGameThreadNotifier(){}
	~FStatGroupGameThreadNotifier(){}

	/** A list of all the stat groups which need broadcasting */
	TArray<FStatNameAndInfo> NameAndInfos;
};

#endif
