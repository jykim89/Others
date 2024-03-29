// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

/*-----------------------------------------------------------------------------
	TimeAccuracy
-----------------------------------------------------------------------------*/

struct FTimeAccuracy
{
	enum Type
	{
		FPS008,
		FPS015,
		FPS030,
		FPS060,
		FPS120,

		InvalidOrMax
	};

	static const float AsFrameTime( Type InTimeAccuracy )
	{
		static const float FrameTimeTable[InvalidOrMax] = {1000.0f/_FPS008,1000.0f/_FPS015,1000.0f/_FPS030,1000.0f/_FPS060,1000.0f/_FPS120};
		return FrameTimeTable[InTimeAccuracy];
	}

	static const float AsInvFrameTime( Type InTimeAccuracy )
	{
		static const float FPSInvTable[InvalidOrMax] = {0.001f*_FPS008,0.001f*_FPS015,0.001f*_FPS030,0.001f*_FPS060,0.001f*_FPS120};
		return FPSInvTable[InTimeAccuracy];
	}

	static const int32 AsFPSCounter( Type InTimeAccuracy )
	{
		static const int32 FPSTable[InvalidOrMax] = {_FPS008,_FPS015,_FPS030,_FPS060,_FPS120};
		return FPSTable[InTimeAccuracy];
	}

private:
	static const int32 _FPS008;
	static const int32 _FPS015;
	static const int32 _FPS030;
	static const int32 _FPS060;
	static const int32 _FPS120;
};

// Private header and implementation

/*-----------------------------------------------------------------------------
	FGraphDataSourceDescription
-----------------------------------------------------------------------------*/

class FGraphDataSourceDescription
{
public:
	FGraphDataSourceDescription( const uint32 InStatID )
		: StatID( InStatID )
		, GroupID( -1 )
		, SampleType( EProfilerSampleTypes::InvalidOrMax )
		, CreationTime( -1 )
	{}

	void Initialize( const FString InStatName, const uint32 InGroupID, const FString InGroupName, const EProfilerSampleTypes::Type InSampleType, const FDateTime InCreationTime )
	{
		StatName     = InStatName;
		GroupID      = InGroupID;
		GroupName    = InGroupName;
		SampleType   = InSampleType;
		CreationTime = InCreationTime;
	}

	/**
	 * @return the ID of the stat owned by this data source.
	 */
	const uint32 GetStatID() const
	{
		return StatID;
	}

	/**
	 * @return the ID of the stat group owned by this data source.
	 */
	const uint32 GetGroupID() const
	{
		return GroupID;
	}

	/**
	 * @return name of the stat owned by this data source.
	 */
	const FString& GetStatName() const
	{
		return StatName;
	}

	/**
	 * @return name of the stat group owned by this data source.
	 */
	const FString& GetGroupName() const 
	{ 
		return GroupName; 
	}

	/**
	 * @return the sample type of the stat owned by this data source.
	 */
	const EProfilerSampleTypes::Type GetSampleType() const
	{
		return SampleType;
	}

	const FDateTime& GetCreationTime() const
	{
		return CreationTime;
	}

	/**
	 * @return number of bytes allocated by class instance.
	 */
	const SIZE_T GetMemoryUsage() const
	{
		SIZE_T MemoryUsage = sizeof(*this) + StatName.GetAllocatedSize() + GroupName.GetAllocatedSize();
		return MemoryUsage;
	}

protected:
	/** The ID of the stat owned by this data source. */
	const uint32 StatID;

	/** The ID of the stat group owned by this data source. */
	uint32 GroupID;

	/** The name of the stat owned by this data source. */
	FString StatName;

	/** The name of the stat group owned by this data source. */
	FString GroupName;

	/** The sample type of the stat owned by this data source. */
	EProfilerSampleTypes::Type SampleType;

	/** The time when this profiler session was created ( time of the connection to the client, time when a profiler capture was created ). */
	FDateTime CreationTime;
};

/*-----------------------------------------------------------------------------
	TCacheDataContainer
-----------------------------------------------------------------------------*/

/**
 * Base class used for caching data.
 *	Type - type of data needed to be cached
 */
template< typename Type >
class TCacheDataContainer : public FNoncopyable
{
protected:
	TCacheDataContainer()
		: CachedValues( 0 )
	{}

	~TCacheDataContainer()
	{}

	/** Clears all cached values and reserves the same amount of memory that was allocated before. */
	void ClearCache()
	{
		CachedChunks.Empty( CachedChunks.Num() );
		CachedValues.Empty( CachedValues.Num() );
	}

	/**
	 * @return number of bytes allocated by class instance.
	 */
	const SIZE_T GetMemoryUsage() const
	{
		SIZE_T MemoryUsage = CachedValues.GetAllocatedSize() + CachedChunks.GetAllocatedSize();
		return MemoryUsage;
	}

	enum 
	{ 
		/**	Number of cached values per chunk. */
		NumElementsPerChunk = 64,

		/**	Number of bytes per chunk. */
		NumBytesPerChunk = NumElementsPerChunk * sizeof(Type)
	};

	/**	Cached values, one chunk contains 256 cached values. */
	mutable TChunkedArray<Type,NumBytesPerChunk> CachedValues;

	/**	Each bit in this bit array indicates whether a chunk is fully cached or not. */
	mutable TBitArray<> CachedChunks;
};

/*-----------------------------------------------------------------------------
	TCachedDataByTime
-----------------------------------------------------------------------------*/

/**
 * Simple class that provides mechanism for caching data by time with predefined time accuracy.
 *	Type			- type of data needed to be cached
 *	ManagerClass	- class that inherits from this class, must implement methods GetUncachedValueFromTimeRange(),GetTotalTimeMS()
 */
template< typename Type, typename ManagerClass >
class TCachedDataByTime : private TCacheDataContainer<Type>
{
public:
	/** Typedef for template< typename Type, typename ManagerClass > class TCachedDataByTime. */
	typedef TCachedDataByTime<Type, ManagerClass> ThisCachedDataByTime;

	typedef TCacheDataContainer<Type> ThisCacheDataContainer;

	TCachedDataByTime( const FTimeAccuracy::Type InTimeAccuracy )
		: TCacheDataContainer<Type>()
		, TimeAccuracyMS( FTimeAccuracy::AsFrameTime(InTimeAccuracy) )
		, InvTimeAccuracyMS( FTimeAccuracy::AsInvFrameTime(InTimeAccuracy) )
	{}

	~TCachedDataByTime()
	{}

	void SetTimeAccuracy( const FTimeAccuracy::Type InTimeAccuracy )
	{
		ClearCache();
		TimeAccuracyMS = FTimeAccuracy::AsFrameTime(InTimeAccuracy);
		InvTimeAccuracyMS = FTimeAccuracy::AsInvFrameTime(InTimeAccuracy);
	}

	/** Clears all cached values and reserves the same amount of memory that was allocated before. */
	void ClearCache()
	{
		TCacheDataContainer<Type>::ClearCache();
	}

	/**
	 * Calculates start index for the specified time range.
	 *
	 * @param StartTimeMS	- the start of the time range
	 * @param EndTimeMS		- the end of the time range
	 *
	 * @return an index of the frame for the specified time range
	 */
	FORCEINLINE const uint32 GetStartIndexFromTimeRange( const float StartTimeMS, const float EndTimeMS ) const
	{
		CheckInvariants( StartTimeMS, EndTimeMS );
		const uint32 Index = FMath::TruncToInt( StartTimeMS * InvTimeAccuracyMS );
		return Index;
	}

	/**
	 * Calculates value for the specified time range.
	 * This method looks for frames that meet the requirements.
	 * Then read the value(s) and calculate the average value.
	 * 
	 * This is only a basic implementation and may change in future. Works only with constant time range.
	 *
	 * @param StartTimeMS	- the start of the time range
	 * @param EndTimeMS		- the end of the time range
	 *
	 * @return a value for the specified time range
	 */
	const Type GetValueFromTimeRange( const float StartTimeMS, const float EndTimeMS ) const
	{
		Type Result = (Type)0;
		const uint32 Index = GetStartIndexFromTimeRange( StartTimeMS, EndTimeMS );
		const uint32 CurrentChunkIndex = Index / ThisCacheDataContainer::NumElementsPerChunk;
		const uint32 TotalNumFrames = FMath::TruncToInt( static_cast<const ManagerClass*>(this)->GetTotalTimeMS() * InvTimeAccuracyMS );

		const uint32 NumNeededChunks = (TotalNumFrames + ThisCacheDataContainer::NumElementsPerChunk - 1) / ThisCacheDataContainer::NumElementsPerChunk;
		const uint32 NumMissingValues = TotalNumFrames - ThisCacheDataContainer::CachedValues.Num();

		// Add missing elements to the cached values.
		ThisCacheDataContainer::CachedValues.Add( NumMissingValues );

		// Add missing chunks and initialize to false.
		for( uint32 NewChunkIndex = ThisCacheDataContainer::CachedChunks.Num(); NewChunkIndex < NumNeededChunks; NewChunkIndex++ )
		{
			ThisCacheDataContainer::CachedChunks.Add( false );
		}

		const bool bIsChunkFullyCached = ThisCacheDataContainer::CachedChunks[CurrentChunkIndex];
		const bool bCanBeCached = CurrentChunkIndex < NumNeededChunks-1;

		// Check if this stat for the specified frame index is included in the cached values.
		if( bIsChunkFullyCached )
		{
			Result = ThisCacheDataContainer::CachedValues(Index);
		}
		// If value is not cached and if value can be cached, initialize the whole chunk.
		else if( !bIsChunkFullyCached && bCanBeCached )
		{
			const uint32 ChunkStartIndex = CurrentChunkIndex * ThisCacheDataContainer::NumElementsPerChunk;
			const uint32 ChunkEndIndex = ChunkStartIndex + ThisCacheDataContainer::NumElementsPerChunk;

			for( uint32 NewValueIndex = ChunkStartIndex; NewValueIndex < ChunkEndIndex; NewValueIndex++ )
			{
				const float SampleStartTimeMS = NewValueIndex * TimeAccuracyMS;
				ThisCacheDataContainer::CachedValues(NewValueIndex) = (Type)static_cast<const ManagerClass*>(this)->GetUncachedValueFromTimeRange( SampleStartTimeMS, SampleStartTimeMS+TimeAccuracyMS );
			}

			ThisCacheDataContainer::CachedChunks[CurrentChunkIndex] = true;
			Result = ThisCacheDataContainer::CachedValues(Index);
		}
		else
		{
			Result = (Type)static_cast<const ManagerClass*>(this)->GetUncachedValueFromTimeRange( StartTimeMS, EndTimeMS );
		}

		return Result;
	}

	/**
	 * @return number of bytes allocated by class instance.
	 */
	const SIZE_T GetMemoryUsage() const
	{
		SIZE_T MemoryUsage = TCacheDataContainer<Type>::GetMemoryUsage();
		return MemoryUsage;
	}

protected:
	FORCEINLINE void CheckInvariants( const float StartTimeMS, const float EndTimeMS ) const
	{
		check( EndTimeMS > StartTimeMS );
		const float TimeRange = EndTimeMS - StartTimeMS;
		const float AbsDiff = FMath::Abs<float>( TimeRange-TimeAccuracyMS );
		bool bIsNearlyEqual = FMath::IsNearlyZero( AbsDiff, 0.1f );
		check( bIsNearlyEqual && TEXT("Time accuracy doesn't match") );
	}

protected:
	/** Time accuracy of the cached data, in milliseconds. */
	float TimeAccuracyMS;

	/** Inverted time accuracy of the cached data, in milliseconds. */
	float InvTimeAccuracyMS;
};

/*-----------------------------------------------------------------------------
	TCachedDataByIndex
-----------------------------------------------------------------------------*/

/**
 *	Simple class that provides mechanism for caching data by index.
 *	Type			- type of data needed to be cached
 *	ManagerClass	- class that inherits from this class, must implement methods GetUncachedValueFromIndex() and GetNumFrames()
 */
template< typename Type, typename ManagerClass >
class TCachedDataByIndex : private TCacheDataContainer<Type>
{
public:
	/** Typedef for template< typename Type, typename ManagerClass > class TCachedDataByIndex. */
	typedef TCachedDataByIndex<Type, ManagerClass> ThisCachedDataByIndex;

	typedef TCacheDataContainer<Type> ThisCacheDataContainer;

	TCachedDataByIndex()
		: TCacheDataContainer<Type>()
	{}

	/** Destructor. */
	~TCachedDataByIndex()
	{}

	/**
	 * @param Index - index of the value that is being retrieved
	 *
	 * @return a value for the specified index, the value is cached on demand and stored in cache for instant access.
	 */
	const Type GetValueFromIndex( const uint32 Index ) const
	{
		Type Result = (Type)0;

		const uint32 CurrentChunkIndex = Index / ThisCacheDataContainer::NumElementsPerChunk;
		const uint32 TotalNumFrames = static_cast<const ManagerClass*>(this)->GetNumFrames();

		const uint32 NumNeededChunks = (TotalNumFrames + ThisCacheDataContainer::NumElementsPerChunk - 1) / ThisCacheDataContainer::NumElementsPerChunk;
		const uint32 NumMissingValues = TotalNumFrames - ThisCacheDataContainer::CachedValues.Num();

		// Add missing elements to the cached values.
		ThisCacheDataContainer::CachedValues.Add( NumMissingValues );

		// Add missing chunks and initialize to false.
		for( uint32 NewChunkIndex = ThisCacheDataContainer::CachedChunks.Num(); NewChunkIndex < NumNeededChunks; NewChunkIndex++ )
		{
			ThisCacheDataContainer::CachedChunks.Add( false );
		}

		const bool bIsChunkFullyCached = ThisCacheDataContainer::CachedChunks[CurrentChunkIndex];
		const bool bCanBeCached = CurrentChunkIndex < NumNeededChunks-1;

		// Check if this stat for the specified frame index is included in the cached values.
		if( bIsChunkFullyCached )
		{
			Result = ThisCacheDataContainer::CachedValues(Index);
		}
		// If value is not cached and if value can be cached, initialize the whole chunk.
		else if( !bIsChunkFullyCached && bCanBeCached )
		{
			const uint32 ChunkStartIndex = CurrentChunkIndex * ThisCacheDataContainer::NumElementsPerChunk;
			const uint32 ChunkEndIndex = ChunkStartIndex + ThisCacheDataContainer::NumElementsPerChunk;

			for( uint32 NewValueIndex = ChunkStartIndex; NewValueIndex < ChunkEndIndex; NewValueIndex++ )
			{
				ThisCacheDataContainer::CachedValues(NewValueIndex) = (Type)static_cast<const ManagerClass*>(this)->GetUncachedValueFromIndex( NewValueIndex );
			}

			ThisCacheDataContainer::CachedChunks[CurrentChunkIndex] = true;

			Result = ThisCacheDataContainer::CachedValues(Index);
		}
		else
		{
			Result = (Type)static_cast<const ManagerClass*>(this)->GetUncachedValueFromIndex( Index );
		}

		return Result;
	}

	/**
	 * @return number of bytes allocated by class instance.
	 */
	const SIZE_T GetMemoryUsage() const
	{
		SIZE_T MemoryUsage = TCacheDataContainer<Type>::GetMemoryUsage();
		return MemoryUsage;
	}
};

/*-----------------------------------------------------------------------------
	FGraphDataSource
-----------------------------------------------------------------------------*/

/**	Type definition for type of the cached values. */
typedef float TGraphDataType;

/* 
 * A specialized view of the a data provider. Provides access only to the specified group of data. 
 * This class allows accessing data in linear way which may be used to draw a line graph.
 */
class FGraphDataSource 
	: public FGraphDataSourceDescription
	, public TCachedDataByIndex< TGraphDataType, FGraphDataSource >
	, public TCachedDataByTime< TGraphDataType, FGraphDataSource >
{
	friend class TCachedDataByIndex< TGraphDataType, FGraphDataSource >;
	friend class TCachedDataByTime< TGraphDataType, FGraphDataSource >;
	friend class FProfilerSession;

protected:
	/**
	 * Initialization constructor, hidden on purpose.
	 *
	 * @param InProfilerSession - a reference to the profiler session that owns this stat
	 * @param InStatID			- the ID of the stat that this graph data source will be created for
	 *
	 */
	FGraphDataSource( const FProfilerSessionRef& InProfilerSession, const uint32 InStatID  );

public:
	/** Virtual destructor. */
	virtual ~FGraphDataSource()
	{
		const SIZE_T MemoryUsage = GetMemoryUsage();
	}

public:
	/**
	 * @return number of bytes allocated by this graph data source.
	 */
	const SIZE_T GetMemoryUsage() const
	{
		SIZE_T MemoryUsage = 0;
		MemoryUsage += ThisCachedDataByIndex::GetMemoryUsage();
		MemoryUsage += ThisCachedDataByTime::GetMemoryUsage();
		MemoryUsage += FGraphDataSourceDescription::GetMemoryUsage();
		return MemoryUsage;

	}

	const uint32 GetNumFrames() const;

	const float GetTotalTimeMS() const;

	const IDataProviderRef GetDataProvider() const;

	/**
	 * @return a session instance ID to the profiler session that owns this graph data source. 
	 */
	const FGuid GetSessionInstanceID() const;

	/**
	 * @return a const pointer to the aggregated stat for the specified stat ID or null if not found.
	 */
	const FProfilerAggregatedStat* GetAggregatedStat() const;


	// @TODO: Returns an offset based on the creation time, used to synchronize several graphs with different creation times.
	const float GetOffsetMS() const
	{
		return 0.0f;
	}

	const bool CanBeDisplayedAsTimeBased() const
	{
		return true;
	}

	const bool CanBeDisplayedAsIndexBased() const
	{
		return true;
	}

protected:
	/* @return a sample value for the specified frame index from the data provider. */
	const TGraphDataType GetUncachedValueFromIndex( const uint32 Index ) const;

	/* @return an approximated sample value for the specified time range from the data provider. */
	const TGraphDataType GetUncachedValueFromTimeRange( const float StartTimeMS, const float EndTimeMS ) const;
	
	/** A reference to the profiler session that owns this graph data source. */
	const FProfilerSessionRef ProfilerSession;

	// @TODO: This needs to be moved to 'filters and presets' filtering options.
	float Scale;
};


class FGraphDataSourceVerticalAggregate;

/* 
 * A specialized view of a few data providers. Provides access only to the specified group of data. 
 * Data is interpolated for 60 frames per second.
 * This class allows accessing data in linear way which may be used to draw a combined line graph with min, max and average values.
 */
class FCombinedGraphDataSource
	: public FGraphDataSourceDescription
	, public TCachedDataByTime< FVector, FCombinedGraphDataSource >
{
	friend class TCachedDataByTime< FVector, FCombinedGraphDataSource >;
	friend class FProfilerManager;

protected:
	/**
	 * Initialization constructor, hidden on purpose.
	 *
	 * @param InStatID			- the ID of the stat that will be drawn in the data graph widget
	 *
	 */
	FCombinedGraphDataSource( const uint32 InStatID, const FTimeAccuracy::Type InTimeAccuracy );

public:
	/** Destructor. */
	~FCombinedGraphDataSource()
	{
		const SIZE_T MemoryUsage = GetMemoryUsage();
	}

	const bool CanBeDisplayedAsMulti() const
	{
		const bool bResult = GetSourcesNum() > 1;
		return bResult;
	}

	const bool CanBeDisplayedAsTimeBased() const
	{
		return GetSourcesNum() > 0;
	}

	const bool CanBeDisplayedAsIndexBased() const
	{
		return GetSourcesNum() == 1;
	}

	const bool IsProfilerSessionRegistered( const FGuid& SessionInstanceID ) const
	{
		const bool bIsRegistered = GraphDataSources.Contains( SessionInstanceID );
		return bIsRegistered;
	}

	void RegisterWithProfilerSession( const FGuid& SessionInstanceID, const FGraphDataSourceRefConst& GraphDataSource )
	{
		GraphDataSources.Add( SessionInstanceID, GraphDataSource );
		ThisCachedDataByTime::ClearCache();
	}

	void UnregisterWithProfilerSession( const FGuid& SessionInstanceID )
	{
		GraphDataSources.Remove( SessionInstanceID );
		ThisCachedDataByTime::ClearCache();
	}

	/**
	 * @return number of bytes allocated by this graph data source.
	 */
	const SIZE_T GetMemoryUsage() const
	{
		SIZE_T MemoryUsage = 0;
		return MemoryUsage;
	}

	const TMap<FGuid,FGraphDataSourceRefConst>::TConstIterator GetSourcesIterator() const
	{
		return GraphDataSources.CreateConstIterator();
	}

	const int GetSourcesNum() const
	{
		return GraphDataSources.Num();
	}

	const FGraphDataSourceRefConst* GetFirstSource() const
	{
		// TODO: Add global accessible Null FGraphDataSourceRef, and other instances used in the profiler to avoid returning empty pointer.
		const FGraphDataSourceRefConst* Result = NULL;
		if( GetSourcesNum() > 0 )
		{
			Result = &GetSourcesIterator().Value();
		}
		return Result;
	}

	const uint32 GetNumFrames() const
	{	
		if( GetSourcesNum() > 0 )
		{
			return FMath::TruncToInt( GetTotalTimeMS() * InvTimeAccuracyMS );
		}
		else
		{
			return 0;
		}
	}

	const float GetTotalTimeMS() const
	{
		if( GetSourcesNum() > 0 )
		{
			float MinTotalTime = 1000.0f * 60 * 60 * 24 * 365;

			for( auto It = GetSourcesIterator(); It; ++It )
			{
				const FGraphDataSourceRefConst& GraphDataSource = It.Value();
				MinTotalTime = FMath::Min( MinTotalTime, GraphDataSource->GetTotalTimeMS() );
			}

			return MinTotalTime;
		}
		else
		{
			return 0.0f;
		}
	}

	/**
	 *
	 * @param StartTimeMS	- the start of the time range
	 * @param EndTimeMS		- the end of the time range
	 * @param out_Indices	- the map that contains calculated start index for each graph data source
	 * 
	 */
	void GetStartIndicesFromTimeRange( const float StartTimeMS, const float EndTimeMS, TMap<FGuid,uint32>& out_StartIndices ) const;

protected:
	/* @return an approximated sample value for the specified time range from the data provider. */
	const FVector GetUncachedValueFromTimeRange( const float StartTimeMS, const float EndTimeMS ) const;

	/** A map of graph data sources for all active profiler session instances for the specified stat ID. */
	TMap<FGuid,FGraphDataSourceRefConst> GraphDataSources;
};

/*-----------------------------------------------------------------------------
	Event graph related type definitions
-----------------------------------------------------------------------------*/

/** Type definition for shared pointers to instances of FEventGraphSample. */
typedef TSharedPtr<class FEventGraphSample> FEventGraphSamplePtr;

/** Type definition for shared references to instances of FEventGraphSample. */
typedef TSharedRef<class FEventGraphSample> FEventGraphSampleRef;

/** Type definition for weak references to instances of FEventGraphSample. */
typedef TWeakPtr<class FEventGraphSample> FEventGraphSampleWeak;

/** Type definition for shared pointers to instances of FEventGraphData. */
typedef TSharedPtr<class FEventGraphData, ESPMode::ThreadSafe> FEventGraphDataPtr;

/** Type definition for shared references to instances of FEventGraphData. */
typedef TSharedRef<class FEventGraphData, ESPMode::ThreadSafe> FEventGraphDataRef;

/** Type definition for shared pointers to instances of FEventGraphDataHandler. */
typedef TSharedPtr<class FEventGraphDataHandler> FEventGraphDataHandlerPtr;

/** Type definition for shared references to instances of FEventGraphDataHandler. */
typedef TSharedRef<class FEventGraphDataHandler> FEventGraphDataHandlerRef;


/*-----------------------------------------------------------------------------
	Minimal event graph sample property management
-----------------------------------------------------------------------------*/

/** Enumerates event graph columns index. */
namespace EEventPropertyIndex
{
	enum Type
	{
		/** Stat name must be the first column, because of the expander arrow. */
		StatName,
		InclusiveTimeMS,
		InclusiveTimePct,
		MinInclusiveTimeMS,
		MaxInclusiveTimeMS,
		AvgInclusiveTimeMS,
		ExclusiveTimeMS,
		ExclusiveTimePct,
		AvgInclusiveTimePerCallMS,
		NumCallsPerFrame,
		AvgNumCallsPerFrame,
		ThreadName,
		ThreadDurationMS,
		FrameDurationMS,
		ThreadPct,
		FramePct,
		ThreadToFramePct,
		StartTimeMS,
		GroupName,
		/** Special name used for unknown property. */
		None,

		// Booleans
		bIsHotPath,
		bIsFiltered,
		bIsCulled,

		// Booleans internal
		bNeedNotCulledChildrenUpdate,

		/** Invalid enum type, may be used as a number of enumerations. */
		InvalidOrMax,
	};
}

/** Enumerates event graph sample value formatting types, usually match with event graph widget's columns. */
namespace EEventPropertyFormatters
{
	enum Type
	{
		/** Name, stored as a string, displayed as a regular string. */
		Name,

		/** Time in milliseconds, stored as a double, displayed as ".3f ms" */
		TimeMS,

		/** Time as percent, stored as a double, displayed as ".1f %" */
		TimePct,

		/** Number of calls, store as a double, displayed as ".1f" */
		Number,

		/** Boolean value, store as a bool, displaying is not supported yet. */
		Bool,

		/** Invalid enum type, may be used as a number of enumerations. */
		InvalidOrMax,
	};
}

/** Enumerates . */
namespace EEventPropertyTypes
{
	enum Type
	{
		/** double. */
		Double,

		/** FName. */
		Name,

		/** bool. */
		Bool,

		/** Invalid enum type, may be used as a number of enumerations. */
		InvalidOrMax,
	};
}

//namespace NEventProp
//{
	class FEventProperty /*: public FNoncopyable*/
	{
		friend class FEventGraphSample;
	protected:
		FEventProperty
		( 
			const EEventPropertyIndex::Type PropertyIndex, 
			const FName PropertyName, 
			const uint32 PropertyOffset, 
			const EEventPropertyFormatters::Type PropertyFormatter
		)
			:
			Index( PropertyIndex ),
			Name( PropertyName ),
			Offset( PropertyOffset ),
			Formatter( PropertyFormatter ),
			Type( GetTypeFromFormatter(PropertyFormatter) )
		{}

		CONSTEXPR EEventPropertyTypes::Type GetTypeFromFormatter( const EEventPropertyFormatters::Type PropertyFormatter ) const
		{
			switch( PropertyFormatter )
			{
			case EEventPropertyFormatters::Name:
				return EEventPropertyTypes::Name;

			case EEventPropertyFormatters::TimeMS:
			case EEventPropertyFormatters::TimePct:
			case EEventPropertyFormatters::Number:
				return EEventPropertyTypes::Double;

			case EEventPropertyFormatters::Bool:
				return EEventPropertyTypes::Bool;

			default:
				check(0);
				return EEventPropertyTypes::InvalidOrMax;
			}
		}

	public:
		const bool IsDouble() const
		{
			return Type == EEventPropertyTypes::Double;
		}

		const bool IsBoolean() const
		{
			return Type == EEventPropertyTypes::Bool;
		}

		const bool IsName() const
		{
			return Type == EEventPropertyTypes::Name;
		}

	public:
		const EEventPropertyIndex::Type Index;
		const FName Name;
		const uint32 Offset;
		const EEventPropertyFormatters::Type Formatter;
		const EEventPropertyTypes::Type Type;
	};

	template< typename Type >
	class TEventPropertyValue
	{
	public:
		FORCEINLINE TEventPropertyValue( const FEventGraphSample& InEvent, const FEventProperty& EventProperty )
			: Event( InEvent )
			, PropertyOffset( EventProperty.Offset )
		{}

		FORCEINLINE_DEBUGGABLE const Type GetPropertyValue() const
		{
			const uint8* const PropertyAddress = GetPropertyAddress();
			const Type Value = *(const Type*)PropertyAddress;
			return Value;
		}

		FORCEINLINE_DEBUGGABLE Type& GetPropertyValueRef()
		{
			const uint8* const PropertyAddress = GetPropertyAddress();
			Type& Value = (Type&)*(const Type*)PropertyAddress;
			return Value;
		}

		FORCEINLINE_DEBUGGABLE const Type GetComparablePropertyValue() const
		{
			return GetPropertyValue();
		}

		FORCEINLINE const uint8* const GetPropertyAddress() const
		{
			const uint8* const SampleAddress = (const uint8* const)&Event;
			const uint8* const PropertyAddress = SampleAddress + PropertyOffset;
			return PropertyAddress;
		}

	protected:
		const FEventGraphSample& Event;
		const uint32 PropertyOffset;
	};

	typedef TEventPropertyValue< double > FEventPropertyValue_Double;
	typedef TEventPropertyValue< bool > FEventPropertyValue_Bool;

	class FEventPropertyValue_Name : public TEventPropertyValue< FName >
	{
	public:
		FORCEINLINE FEventPropertyValue_Name( const FEventGraphSample& InEvent, const FEventProperty& EventProperty )
			: TEventPropertyValue( InEvent, EventProperty )
		{}

		FORCEINLINE_DEBUGGABLE const FString GetComparablePropertyValue() const
		{
			const FString Value = GetPropertyValue().GetPlainNameString();
			return Value;
		}
	};

namespace NEventFormatter
{
	template< EEventPropertyFormatters::Type PropertyType >
	FORCEINLINE FString ToString( const FEventGraphSample& Event, const FEventProperty& EventProperty )
	{
		check(0);
		return FString();
	};

	template <>
	FORCEINLINE FString ToString< EEventPropertyFormatters::Name >( const FEventGraphSample& Event, const FEventProperty& EventProperty )
	{
		return FEventPropertyValue_Name(Event,EventProperty).GetPropertyValue().GetPlainNameString();
	};

	template <>
	FORCEINLINE FString ToString< EEventPropertyFormatters::TimeMS >( const FEventGraphSample& Event, const FEventProperty& EventProperty )
	{
		return FString::Printf( TEXT("%.3f ms"), FEventPropertyValue_Double(Event,EventProperty).GetPropertyValue() );
	};

	template <>
	FORCEINLINE FString ToString< EEventPropertyFormatters::TimePct >( const FEventGraphSample& Event, const FEventProperty& EventProperty )
	{
		return FString::Printf( TEXT("%.1f %%"), FEventPropertyValue_Double(Event,EventProperty).GetPropertyValue() );
	};

	template <>
	FORCEINLINE FString ToString< EEventPropertyFormatters::Number >( const FEventGraphSample& Event, const FEventProperty& EventProperty )
	{
		return FString::Printf( TEXT("%.1f"), FEventPropertyValue_Double(Event,EventProperty).GetPropertyValue() );
	};
}

//};

/** Hides all event graph related functionality under this namespace. */
//namespace NEventGraphSample {

/** Useful constants related to event graph functionality. */
struct FEventGraphConsts
{
	static FName RootEvent;
	static FName Self;
	static FName FakeRoot;
};

// TODO: Rename to FProfilerEvent
/*
 * Contains the same data as the profiler sample with some additions, doesn't depend on the other classes like profiler metadata or profiler aggregates.
 * Modeled to be Slate compatible, thus it inherits from TSharedFromThis.
 */
class FEventGraphSample : public TSharedFromThis<FEventGraphSample>
{
	enum
	{
		/** Maximum number of stack when traversing event graph - should be enough to store root nodes and all its children.*/
		MaxStackSize = 65536
	};

	struct FDuplicateHierarchyTag {};
	struct FDuplicateSimpleTag {};

	friend class FEventArraySorter;
	friend class FEventGraphData;

public:
	/** Initializes the minimal property manager for the event graph sample. */
	static void InitializePropertyManagement();

	static const FEventProperty& GetEventPropertyByIndex( const EEventPropertyIndex::Type PropertyIndex )
	{
		return Properties[PropertyIndex];
	}

	static const FEventProperty& GetEventPropertyByName( const FName PropertyName )
	{
		return *NamedProperties.FindChecked( PropertyName );
	}

protected:
	/** Contains all properties of the event graph sample class. */
	static FEventProperty Properties[ EEventPropertyIndex::InvalidOrMax ];

	/** Contains all properties of the event graph sample class, indexed by the name of the property, stored as FName -> FEventProperty&. */
	static TMap<FName,const FEventProperty*> NamedProperties;

public:
	/** Initialization constructor. */
	FEventGraphSample( const FName InName )
		: _ParentPtr( nullptr )
		, _RootPtr( nullptr )
		, _ThreadPtr( nullptr )
		, _ThreadName( InName )
		, _GroupName( InName )
		, _StatID( 0 )
		, _StatName( InName )
		, _StartTimeMS( 0.0f )
		, _InclusiveTimeMS( 0.0f )
		, _InclusiveTimePct( 0.0f )
		, _MinInclusiveTimeMS( 0.0f )
		, _MaxInclusiveTimeMS( 0.0f )
		, _AvgInclusiveTimeMS( 0.0f )
		, _AvgInclusiveTimePerCallMS( 0.0000f )
		, _NumCallsPerFrame( 0.0f )
		, _AvgNumCallsPerFrame( 0.0f )
		, _ExclusiveTimeMS( 0.0f )
		, _ExclusiveTimePct( 0.0f )
		, _FrameDurationMS( 0.0f )
		, _ThreadDurationMS( 0.0f )
		, _ThreadToFramePct( 0.0f )
		, _ThreadPct( 0.0f )
		, _FramePct( 0.0f )
		, _bIsHotPath( false )
		, _bIsFiltered( false )
		, _bIsCulled( false )
		, bNeedNotCulledChildrenUpdate( true )
	{}
	
	/**
	 * @return creates a named event
	 */
	static FEventGraphSamplePtr CreateNamedEvent( const FName EventName )
	{
		FEventGraphSample* RootEventGraphSample = new FEventGraphSample( EventName );
		return MakeShareable( RootEventGraphSample );
	}

protected:
	/** Initialization constructor. */
	FEventGraphSample
	(
		const FName InThreadName,
 
		const FName InGroupName,
		const uint32 InStatID,
		const FName InStatName,
 
		const double InStartTimeMS,
		const double InInclusiveTimeMS,
 
		const double InMinInclusiveTimeMS,
		const double InMaxInclusiveTimeMS,
		const double InAvgInclusiveTimeMS,
 
		const double InAvgPerCallInclusiveTimeMS,
 
		const double InNumCallsPerFrame,
		const double InAvgNumCallsPerFrame,
 
		const FEventGraphSamplePtr InParentPtr = NULL
	)
		: _ParentPtr( InParentPtr )
		, _ThreadName( InThreadName )
		, _GroupName( InGroupName )
		, _StatID( InStatID )
		, _StatName( InStatName )
		, _StartTimeMS( InStartTimeMS )
		, _InclusiveTimeMS( InInclusiveTimeMS )
		, _InclusiveTimePct( 0.0f )
		, _MinInclusiveTimeMS( InMinInclusiveTimeMS )
		, _MaxInclusiveTimeMS( InMaxInclusiveTimeMS )
		, _AvgInclusiveTimeMS( InAvgInclusiveTimeMS )
		, _AvgInclusiveTimePerCallMS( 0.0000f )
		, _NumCallsPerFrame( InNumCallsPerFrame )
		, _AvgNumCallsPerFrame( InAvgNumCallsPerFrame )
		, _ExclusiveTimeMS( 0.0f )
		, _ExclusiveTimePct( 0.0f )
		, _FrameDurationMS( 0.0f )
		, _ThreadDurationMS( 0.0f )
		, _ThreadToFramePct( 0.0f )
		, _ThreadPct( 0.0f )
		, _FramePct( 0.0f )
		, _bIsHotPath( false )
		, _bIsFiltered( false )
		, _bIsCulled( false )
		, bNeedNotCulledChildrenUpdate( true )
	{}

	/** Copy constructor, copies properties from the specified source event. */
	FEventGraphSample( const FEventGraphSample& SourceEvent, const FDuplicateSimpleTag )
		: _ParentPtr( nullptr )
		, _RootPtr( nullptr )
		, _ThreadPtr( nullptr )
		, _ThreadName( SourceEvent._ThreadName )
		, _GroupName( SourceEvent._GroupName )
		, _StatID( SourceEvent._StatID )
		, _StatName( SourceEvent._StatName )
		, _StartTimeMS( SourceEvent._StartTimeMS )
		, _InclusiveTimeMS( SourceEvent._InclusiveTimeMS )
		, _InclusiveTimePct( SourceEvent._InclusiveTimePct )
		, _MinInclusiveTimeMS( SourceEvent._MinInclusiveTimeMS )
		, _MaxInclusiveTimeMS( SourceEvent._MaxInclusiveTimeMS )
		, _AvgInclusiveTimeMS( SourceEvent._AvgInclusiveTimeMS )
		, _AvgInclusiveTimePerCallMS( SourceEvent._AvgInclusiveTimePerCallMS )
		, _NumCallsPerFrame( SourceEvent._NumCallsPerFrame )
		, _AvgNumCallsPerFrame( SourceEvent._AvgNumCallsPerFrame )
		, _ExclusiveTimeMS( SourceEvent._ExclusiveTimeMS )
		, _ExclusiveTimePct( SourceEvent._ExclusiveTimePct )
		, _FrameDurationMS( SourceEvent._FrameDurationMS )
		, _ThreadDurationMS( SourceEvent._ThreadDurationMS )
		, _ThreadToFramePct( SourceEvent._ThreadToFramePct )
		, _ThreadPct( SourceEvent._ThreadPct )
		, _FramePct( SourceEvent._FramePct )
		, _bIsHotPath( false )
		, _bIsFiltered( false )
		, _bIsCulled( false )
		, bNeedNotCulledChildrenUpdate( true )
	{}

public:
	/*-----------------------------------------------------------------------------
		Operations
	-----------------------------------------------------------------------------*/

	void AddSamplePtr( const FEventGraphSamplePtr& Other )
	{
		(*this) += *Other.Get();
	}

	void DivideSamplePtr( const double Divisor )
	{
		(*this) /= Divisor;
	}

	void MaxSamplePtr( const FEventGraphSamplePtr& Other )
	{
		Max( *Other.Get() );
	}

	FEventGraphSample& operator+=( const FEventGraphSample& Other )
	{
		_InclusiveTimeMS += Other._InclusiveTimeMS;
		_MinInclusiveTimeMS += Other._MinInclusiveTimeMS;
		_MaxInclusiveTimeMS += Other._MaxInclusiveTimeMS;
		_AvgInclusiveTimeMS += Other._AvgInclusiveTimeMS;
		_AvgInclusiveTimePerCallMS += Other._AvgInclusiveTimePerCallMS;
		_NumCallsPerFrame += Other._NumCallsPerFrame;
		_AvgNumCallsPerFrame += Other._AvgNumCallsPerFrame;
		_ExclusiveTimeMS += Other._ExclusiveTimeMS;

		return *this;
	}

	FEventGraphSample& operator/=( const double Divisor )
	{
		_InclusiveTimeMS /= Divisor;
		_MinInclusiveTimeMS /= Divisor;
		_MaxInclusiveTimeMS /= Divisor;
		_AvgInclusiveTimeMS /= Divisor;
		_AvgInclusiveTimePerCallMS /= Divisor;
		_NumCallsPerFrame /= Divisor;
		_AvgNumCallsPerFrame /= Divisor;
		_ExclusiveTimeMS /= Divisor;

		return *this;
	}

	void Max( const FEventGraphSample& Other )
	{
		_InclusiveTimeMS = FMath::Max( _InclusiveTimeMS, Other._InclusiveTimeMS );
		_MinInclusiveTimeMS = FMath::Max( _MinInclusiveTimeMS, Other._MinInclusiveTimeMS );
		_MaxInclusiveTimeMS = FMath::Max( _MaxInclusiveTimeMS, Other._MaxInclusiveTimeMS );
		_AvgInclusiveTimeMS = FMath::Max( _AvgInclusiveTimeMS, Other._AvgInclusiveTimeMS );
		_AvgInclusiveTimePerCallMS = FMath::Max( _AvgInclusiveTimePerCallMS, Other._AvgInclusiveTimePerCallMS );
		_NumCallsPerFrame = FMath::Max( _NumCallsPerFrame, Other._NumCallsPerFrame );
		_AvgNumCallsPerFrame = FMath::Max( _AvgNumCallsPerFrame, Other._AvgNumCallsPerFrame );
		_ExclusiveTimeMS = FMath::Max( _ExclusiveTimeMS, Other._ExclusiveTimeMS );
	}

	const bool AreTheSamePtr( const FEventGraphSamplePtr& Other ) const
	{
		return *this == *Other.Get();
	}

	const bool operator==( const FEventGraphSample& Other ) const
	{
		const bool bAreChildrenTheSame = _ThreadName==Other._ThreadName && _StatID==Other._StatID;// && _GroupName==Other._GroupName;
		return bAreChildrenTheSame;
	}

	/** Destructor. */
	~FEventGraphSample()
	{
#if	_DEBUG
		int k=0;k++;
#endif // _DEBUG
	}

	FEventGraphSamplePtr FindChildPtr( const FEventGraphSamplePtr& ChildToLook );
	// TODO: CombineAndAdd, CombineAndFindMax, Divide FEventGraphDataRef<AggregateFunc,FinalizerFunc,bRequiredCopy>
	void CombineAndAddPtr_Recurrent( const FEventGraphSamplePtr& Other );
	void CombineAndFindMaxPtr_Recurrent( const FEventGraphSamplePtr& Other );
	void Divide_Recurrent( const double Divisor );

	struct FDivide
	{
		FORCEINLINE void operator()( FEventGraphSample* EventPtr, const double Divisor )
		{
			*EventPtr /= Divisor;
		}
	};

	/**
	 * @return true, if this event is a root event.
	 */
	bool IsRoot() const
	{
		return _StatName == FEventGraphConsts::RootEvent;
	}

	/**
	 * @return true, if this event is a fake self event.
	 */
	bool IsSelf() const
	{
		return _StatName == FEventGraphConsts::Self;
	}

	template< typename TFunc > 
	FORCEINLINE void ExecuteOperationForAllChildren( TFunc FuncToCall )
	{
		FEventGraphSample** Stack = new FEventGraphSample*[MaxStackSize];
		Stack[ 0 ] = &AsShared().Get();
		int32 Idx = 1;

		while( Idx > 0 )
		{
			// Get the parent and assign events.
			FEventGraphSample* Current = Stack[ --Idx ];
			FuncToCall( Current );

			// Push children onto the stack.
			const TArray<FEventGraphSamplePtr>& ChildrenPtr = Current->GetChildren();
			const int32 NumChildren = ChildrenPtr.Num();
			for( int32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
			{
				const FEventGraphSamplePtr& ChildPtr = ChildrenPtr[ChildIndex];
				Stack[ Idx++ ] = ChildPtr.Get();
			}
			check(Idx < MaxStackSize);
		}

		delete [] Stack;
	}

	template< typename TFunc, typename TArg0 > 
	FORCEINLINE void ExecuteOperationForAllChildren( TFunc FuncToCall, TArg0& Arg0 )
	{
		FEventGraphSample** Stack = new FEventGraphSample*[MaxStackSize]; 
		Stack[ 0 ] = &AsShared().Get();
		int32 Idx = 1;

		while( Idx > 0 )
		{
			// Get the parent and assign events.
			FEventGraphSample* Current = Stack[ --Idx ];
			FuncToCall( Current, Arg0 );

			// Push children onto the stack.
			const TArray<FEventGraphSamplePtr>& ChildrenPtr = Current->GetChildren();
			const int32 NumChildren = ChildrenPtr.Num();
			for( int32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
			{
				const FEventGraphSamplePtr& ChildPtr = ChildrenPtr[ChildIndex];
				Stack[ Idx++ ] = ChildPtr.Get();
			}
			check(Idx < MaxStackSize);
		}

		delete [] Stack;
	}

protected:
	/** @return a shared pointer to the newly created copy of this event graph sample, creates a full copy of hierarchy and duplicates all samples. */
	FEventGraphSamplePtr DuplicateWithHierarchyPtr()
	{
		FEventGraphSamplePtr ParentPtr = DuplicateSimplePtr();

		// Duplicate children
		const int32 NumChildren = _ChildrenPtr.Num();
		ParentPtr->_ChildrenPtr.Reserve( _ChildrenPtr.Num() );

		for( int32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
		{
			FEventGraphSamplePtr ChildPtr = _ChildrenPtr[ChildIndex]->DuplicateWithHierarchyPtr();
			//ParentPtr->AddChildAndSetParentPtr( ChildPtr );
			ChildPtr->_ParentPtr = ParentPtr;
			ParentPtr->_ChildrenPtr.Add( ChildPtr );
		}
		return ParentPtr;
	}

	FORCEINLINE void AddChildAndSetParentPtr( const FEventGraphSamplePtr& ChildPtr ) 
	{
		ChildPtr->_ParentPtr = AsShared();
		_ChildrenPtr.Add( ChildPtr );
	}


	void SetRootAndThreadForAllChildren()
	{
		FEventGraphSamplePtr RootEvent = AsShared();
		const int32 NumChildren = _ChildrenPtr.Num();
		for( int32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
		{
			const FEventGraphSamplePtr& ThreadEvent = _ChildrenPtr[ChildIndex];
			ThreadEvent->SetRootAndThreadEvents_Iterative( RootEvent.Get(), ThreadEvent.Get() );
		}
	}

	FORCEINLINE void SetRootAndThread( FEventGraphSample* RootEvent, FEventGraphSample* ThreadEvent )
	{
		_RootPtr = RootEvent;
		_ThreadPtr = ThreadEvent;
	}

	void SetRootAndThreadEvents_Iterative( FEventGraphSample* RootEvent, FEventGraphSample* ThreadEvent )
	{
		FEventGraphSample** Stack = new FEventGraphSample*[MaxStackSize];
		Stack[ 0 ] = &AsShared().Get();
		int32 Idx = 1;

		while( Idx > 0 )
		{
			// Get the parent and assign events.
			FEventGraphSample* Current = Stack[ --Idx ];
			Current->SetRootAndThread( RootEvent, ThreadEvent );

			// Push children onto the stack.
			const TArray<FEventGraphSamplePtr>& ChildrenPtr = Current->GetChildren();
			const int32 NumChildren = ChildrenPtr.Num();
			for( int32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
			{
				const FEventGraphSamplePtr& ChildPtr = ChildrenPtr[ChildIndex];
				Stack[ Idx++ ] = ChildPtr.Get();
			}
			check(Idx < MaxStackSize);
		}

		delete [] Stack;
	}

	/** Not used, optimized version @see SetRootAndThreadEvents_Iterative. */
	void SetRootAndThreadEvents_Recurrent( FEventGraphSample* RootEvent, FEventGraphSample* ThreadEvent )
	{
		SetRootAndThread( RootEvent, ThreadEvent );
		const int32 NumChildren = _ChildrenPtr.Num();
		for( int32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
		{
			const FEventGraphSamplePtr& ChildPtr = _ChildrenPtr[ChildIndex];
			ChildPtr->SetRootAndThreadEvents_Recurrent( RootEvent, ThreadEvent );
		}
	}

public:
	/** @return a shared pointer to the newly created copy of this event graph sample, without any children and with no parent. */
	FEventGraphSamplePtr DuplicateSimplePtr()
	{
		return MakeShareable( new FEventGraphSample( *this, FDuplicateSimpleTag() ) );
	}

public:
	/** Adds a child to this sample. */
	FORCEINLINE void AddChildPtr( const FEventGraphSamplePtr& Child ) 
	{
		_ChildrenPtr.Add( Child );
	}

	/**
	 * @return a shared pointer to the parent of this event, may be null.
	 */
	FORCEINLINE FEventGraphSamplePtr GetParent() const 
	{ 
		return _ParentPtr.Pin(); 
	}

	/**
	 * @return a shared pointer to the root event of this event, may be null.
	 */
	const FEventGraphSample* GetRoot() const
	{
		return _RootPtr;
	}

	/**
	 * @return a shared pointer to the thread event of this event, may be null.
	 */
	const FEventGraphSample* GetThread() const
	{
		return _ThreadPtr;
	}
	
	/**
	 * @return a const reference to the child samples of this sample.
	 */
	FORCEINLINE const TArray<FEventGraphSamplePtr>& GetChildren() const 
	{ 
		return _ChildrenPtr; 
	}

	/**
	 * @return a const array that contains all children that have not been culled.
	 */
	FORCEINLINE_DEBUGGABLE const TArray<FEventGraphSamplePtr>& GetNotCulledChildren() const 
	{ 
		const_cast<FEventGraphSample*>(this)->UpdateNotCulledChildrenInternal();
		return _NotCulledChildrenPtr; 
	}

	FORCEINLINE void RequestNotCulledChildrenUpdate()
	{
		bNeedNotCulledChildrenUpdate = true;
	}

protected:
	/**
	 * @return an array that contains all children that have not been culled.
	 */
	FORCEINLINE_DEBUGGABLE TArray<FEventGraphSamplePtr>& GetNotCulledChildrenInternal() 
	{ 
		UpdateNotCulledChildrenInternal();
		return _NotCulledChildrenPtr; 
	}

	/**
	 * Updates an array that will contain all children that have not been culled.
	 */
	FORCEINLINE_DEBUGGABLE void UpdateNotCulledChildrenInternal() 
	{ 
		if( bNeedNotCulledChildrenUpdate )
		{
			_NotCulledChildrenPtr.Reset();

			const int32 NumChildren = _ChildrenPtr.Num();
			for( int32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
			{
				const FEventGraphSamplePtr& ChildPtr = _ChildrenPtr[ChildIndex];
				if( !ChildPtr->_bIsCulled )
				{
					_NotCulledChildrenPtr.Add( ChildPtr );
				}
			}

			bNeedNotCulledChildrenUpdate = false;
		}
	}

public:
	/**
	 * @return true, if this event contains culled children.
	 */
	FORCEINLINE_DEBUGGABLE const bool HasCulledChildren() const 
	{ 
		const bool bHasCulledChildren = GetChildren().Num() != GetNotCulledChildren().Num();
		return bHasCulledChildren; 
	}

	/**
	 * @return a reference to the child samples of this sample.
	 */
	FORCEINLINE TArray<FEventGraphSamplePtr>& GetChildren() 
	{ 
		return _ChildrenPtr; 
	}

	/**
	 * @return a shorter name of this event.
	 */
	FORCEINLINE const FString GetShortEventName() const
	{
		return FProfilerHelper::ShortenName( _StatName.GetPlainNameString() );
	}

	/**
	 * @return a topmost parent of this event, usually it is a thread event. The root event is excluded.
	 */
	FEventGraphSamplePtr GetOutermost()
	{
		FEventGraphSamplePtr Outermost;
		for( FEventGraphSamplePtr Top = AsShared(); Top.IsValid() && Top->GetParent().IsValid(); Top = Top->GetParent() )
		{
			Outermost = Top;
		}
		return Outermost;
	}

	void GetStack( TArray<FEventGraphSamplePtr>& out_Stack )
	{
		for( FEventGraphSamplePtr Top = AsShared(); Top.IsValid() && Top->GetParent().IsValid(); Top = Top->GetParent() )
		{
			out_Stack.Add( Top );
		}
	}

	/**
	 * Generates an array with all event samples, so they can be accessed in the linear way. 
	 * None of the events are duplicated. The root event is excluded.
	 */
	FORCEINLINE_DEBUGGABLE void GetLinearEvents( TArray<FEventGraphSamplePtr>& out_LinearEvents, const bool bUseCulled )
	{
		out_LinearEvents.Reset();

		const TArray<FEventGraphSamplePtr> RootChildren = bUseCulled ? GetNotCulledChildren() : GetChildren();

		const int32 NumChildren = RootChildren.Num();
		for( int32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
		{
			GetLinearEvents_InternalRecurrent( RootChildren[ChildIndex], out_LinearEvents, bUseCulled );
		}
	}

private:
	/** Internal method used to store linearized events. */
	FORCEINLINE_DEBUGGABLE void GetLinearEvents_InternalRecurrent( const FEventGraphSamplePtr& ParentEvent, TArray<FEventGraphSamplePtr>& out_LinearEvents, const bool bUseCulled )
	{
		out_LinearEvents.Add( ParentEvent );
		const TArray<FEventGraphSamplePtr> Children = bUseCulled ? ParentEvent->GetNotCulledChildren() : ParentEvent->GetChildren();
		const int32 NumChildren = Children.Num();
		for( int32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
		{
			GetLinearEvents_InternalRecurrent( Children[ChildIndex], out_LinearEvents, bUseCulled );
		}
	}
	
public:
	/**
	 * @return children time in ms, excluding the self time by default
	 */
	double GetChildrenTimeMS( const bool bExcludeSelf = true ) const
	{
		double ChildrenTotalTimeMS = 0.0f;
		const int32 NumChildren = _ChildrenPtr.Num();
		for( int32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
		{
			const FEventGraphSamplePtr& ChildPtr = _ChildrenPtr[ChildIndex];
			if( ChildPtr->IsSelf() )
			{
				if( !bExcludeSelf )
				{
					ChildrenTotalTimeMS += ChildPtr->_InclusiveTimeMS;
				}
			}
			else
			{
				ChildrenTotalTimeMS += ChildPtr->_InclusiveTimeMS;
			}
		}
		return ChildrenTotalTimeMS;
	}


	/**
	 * @return if exist, a fake self event for this event sample, otherwise null.
	 */
	FEventGraphSamplePtr GetSelfOrNull() const
	{
		FEventGraphSamplePtr Self;
		const int32 NumChildren = _ChildrenPtr.Num();
		for( int32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
		{
			const FEventGraphSamplePtr& ChildPtr = _ChildrenPtr[ChildIndex];
			if( ChildPtr->IsSelf() )
			{
				Self = ChildPtr;
				break;
			}
		}
		return Self;
	}

	/** Not used, optimized version @see ExecuteOperationForAllChildren. */
	void FixAllChildren_Recurrent()
	{
		FixChildrenTimes();

		// Check other children.
		const int32 NumChildren = _ChildrenPtr.Num();
		for( int32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
		{
			FEventGraphSamplePtr ThisChild = _ChildrenPtr[ChildIndex];
			ThisChild->FixAllChildren_Recurrent();
		}
	}

	struct FFixChildrenTimes
	{
		FORCEINLINE void operator()( FEventGraphSample* EventPtr )
		{
			EventPtr->FixChildrenTimes();
		}
	};

	/** Calculates time and percentage values that may depend on a child's parent. */
	void FixChildrenTimes()
	{
		if( _StatName != FEventGraphConsts::RootEvent )
		{
			// Get correct value for frame and thread duration.
			_FrameDurationMS = GetRoot()->_InclusiveTimeMS;
			_ThreadDurationMS = GetThread()->_InclusiveTimeMS;

			FEventGraphSample* Parent = GetParent().Get();
			if( IsSelf() )
			{
				Parent->_ExclusiveTimeMS = _InclusiveTimeMS;
				Parent->_ExclusiveTimePct = 100.0f * Parent->_ExclusiveTimeMS/Parent->_InclusiveTimeMS;
			}

			_InclusiveTimePct = 100.0f * _InclusiveTimeMS / Parent->_InclusiveTimeMS;

			_ThreadToFramePct = 100.0f*_ThreadDurationMS/_FrameDurationMS;
			_ThreadPct = 100.0f*_InclusiveTimeMS/_ThreadDurationMS;
			_FramePct = 100.0f*_InclusiveTimeMS/_FrameDurationMS;
		}
	}

public:
	double& PropertyValueAsDouble( const EEventPropertyIndex::Type PropertyIndex )
	{
		const FEventProperty& EventProperty = GetEventPropertyByIndex(PropertyIndex);
		// In case if you want to get FName/bool as a double.
		checkSlow( EventProperty.IsDouble() );
		FEventPropertyValue_Double Value(*this, EventProperty );
		return Value.GetPropertyValueRef();
	}

	FName& PropertyValueAsFName( const EEventPropertyIndex::Type PropertyIndex )
	{
		const FEventProperty& EventProperty = GetEventPropertyByIndex(PropertyIndex);
		// In case if you want to get a double/bool as FName.
		checkSlow( EventProperty.IsName() );
		FEventPropertyValue_Name Value(*this, EventProperty );
		return Value.GetPropertyValueRef();
	}

	bool& PropertyValueAsBool( const EEventPropertyIndex::Type PropertyIndex )
	{
		const FEventProperty& EventProperty = GetEventPropertyByIndex(PropertyIndex);
		// In case if you want to get a double/FName as a bool.
		checkSlow( EventProperty.IsBoolean() );
		FEventPropertyValue_Bool Value(*this, EventProperty );
		return Value.GetPropertyValueRef();
	}

	const FString GetPropertyValueAsString( const EEventPropertyIndex::Type PropertyIndex ) const
	{
		const FEventProperty& EventProperty = GetEventPropertyByIndex(PropertyIndex);
		// In case if you want to get a double/bool as FName.
		checkSlow( EventProperty.IsName() );
		FEventPropertyValue_Name Value(*this, EventProperty );
		return Value.GetPropertyValueRef().GetPlainNameString();
	}

	const FString GetFormattedValue( EEventPropertyIndex::Type PropertyIndex ) const
	{
		const FEventProperty& EventProperty = GetEventPropertyByIndex(PropertyIndex);

		FString FormattedValue;
		if( EventProperty.Formatter == EEventPropertyFormatters::Name )
		{
			FormattedValue = NEventFormatter::ToString<EEventPropertyFormatters::Name>( *this, EventProperty );
		}
		else if( EventProperty.Formatter == EEventPropertyFormatters::TimeMS )
		{
			FormattedValue = NEventFormatter::ToString<EEventPropertyFormatters::TimeMS>( *this, EventProperty );
		}
		else if( EventProperty.Formatter == EEventPropertyFormatters::TimePct )
		{
			FormattedValue = NEventFormatter::ToString<EEventPropertyFormatters::TimePct>( *this, EventProperty );
		}
		else if( EventProperty.Formatter == EEventPropertyFormatters::Number )
		{
			FormattedValue = NEventFormatter::ToString<EEventPropertyFormatters::Number>( *this, EventProperty );
		}
		else
		{
			check(0);
		}
		return FormattedValue;
	}

	/*-----------------------------------------------------------------------------
		Boolean states
	-----------------------------------------------------------------------------*/

	template< bool FEventGraphSample::*BoolPtr >
	struct FSetBooleanState
	{
		FORCEINLINE void operator()( FEventGraphSample* EventPtr, const bool bValue )
		{
			EventPtr->*BoolPtr = bValue;
		}
	};

 public:
	template< const EEventPropertyIndex::Type BooleanPropertyIndex >
	void SetBooleanStateForAllChildren( const bool bValue )
	{
		//checkAtCompileTime( false, FEventGraphSample_SetBooleanStateForAllChildren_BooleanPropertyIndex_NotSupported );
 
		// Not sure why but this if-statement is not optimized based on BooleanPropertyIndex
		if( BooleanPropertyIndex == EEventPropertyIndex::bIsCulled )
		{
			ExecuteOperationForAllChildren( FSetBooleanState<&FEventGraphSample::_bIsCulled>(), bValue );
		}
		else if( BooleanPropertyIndex == EEventPropertyIndex::bIsFiltered )
		{
			ExecuteOperationForAllChildren( FSetBooleanState<&FEventGraphSample::_bIsFiltered>(), bValue );
		}
		else if( BooleanPropertyIndex == EEventPropertyIndex::bIsHotPath )
		{
			ExecuteOperationForAllChildren( FSetBooleanState<&FEventGraphSample::_bIsHotPath>(), bValue );
		}
		else if( BooleanPropertyIndex == EEventPropertyIndex::bNeedNotCulledChildrenUpdate )
		{
			ExecuteOperationForAllChildren( FSetBooleanState<&FEventGraphSample::bNeedNotCulledChildrenUpdate>(), bValue );
		}
	}
 
	// CLang -> explicit specialization of 'SetBooleanStateForAllChildren' in class scope
// 	template<>
// 	void SetBooleanStateForAllChildren<EEventPropertyIndex::bIsCulled>( const bool bValue )
// 	{
// 		ExecuteOperationForAllChildren( FSetBooleanState<&FEventGraphSample::_bIsCulled>(), bValue );
// 	}
//  
// 	template<>
// 	void SetBooleanStateForAllChildren<EEventPropertyIndex::bIsFiltered>( const bool bValue )
// 	{
// 		ExecuteOperationForAllChildren( FSetBooleanState<&FEventGraphSample::_bIsFiltered>(), bValue );
// 	}
//  
// 	template<>
// 	void SetBooleanStateForAllChildren<EEventPropertyIndex::bIsHotPath>( const bool bValue )
// 	{
// 		ExecuteOperationForAllChildren( FSetBooleanState<&FEventGraphSample::_bIsHotPath>(), bValue );
// 	}
//  
// 	template<>
// 	void SetBooleanStateForAllChildren<EEventPropertyIndex::bNeedNotCulledChildrenUpdate>( const bool bValue )
// 	{
// 		ExecuteOperationForAllChildren( FSetBooleanState<&FEventGraphSample::bNeedNotCulledChildrenUpdate>(), bValue );
// 	}

public:

	/** Not used, optimized version @see SetBooleanStateForAllChildren. */
	void SetBooleanStateForAllChildren_Recurrent( const EEventPropertyIndex::Type BooleanPropertyIndex, const bool bValue )
	{
		PropertyValueAsBool(BooleanPropertyIndex) = bValue;
		const int32 NumChildren = _ChildrenPtr.Num();
		for( int32 ChildNx = 0; ChildNx < NumChildren; ++ChildNx )
		{
			const FEventGraphSamplePtr& Child = _ChildrenPtr[ChildNx];
			Child->SetBooleanStateForAllChildren_Recurrent(BooleanPropertyIndex,bValue);
		}
	}

protected:

	/** A weak pointer to the parent of this event. */
	FEventGraphSampleWeak _ParentPtr;

	/** A weak pointer to the parent of this event. */
	FEventGraphSample* _RootPtr;

	/** A weak pointer to the parent of this event. */
	FEventGraphSample* _ThreadPtr;

	/** Children of this event. */
	TArray<FEventGraphSamplePtr> _ChildrenPtr;

	/** Not culled children of this event. */
	TArray<FEventGraphSamplePtr> _NotCulledChildrenPtr;

public:
	/** Name of the thread that this event was captured on. */
	FName _ThreadName;

	/** Name of the stat group this event belongs to, ex. Engine. */
	FName _GroupName;

	/** Name of this event, ex. Frametime. If empty, it means that this sample is a root sample, use _ThreadName. */
	FName _StatName;

	/** Stat ID of the event. */ // TODO: Not needed, move to _statname if meta data is not available
	uint32 _StatID;

	/** Start time of this event, in milliseconds. */
	double _StartTimeMS;


	/** Duration of this event and its children, in milliseconds. */
	double _InclusiveTimeMS;

	/** Duration of this event and its children as percent of the caller. */
	double _InclusiveTimePct;


	// TODO: Rework to _MaxInclusiveTimeMS, _AvgInclusiveTimeMS

	/** Minimum inclusive time of all instances for this event, in milliseconds. */ // TODO: to be removed, use data graph aggregation 
	double _MinInclusiveTimeMS;

	/** Maximum inclusive time of all instances for this event, in milliseconds. */ // TODO: to be removed, use data graph aggregation 
	double _MaxInclusiveTimeMS;

	/** Average inclusive time of all instances for this event, in milliseconds. */ // TODO: to be removed, use data graph aggregation 
	double _AvgInclusiveTimeMS;

	/** Average inclusive time per call of all instances for this event, in milliseconds. */
	double _AvgInclusiveTimePerCallMS;

	// TODO: Rename into _AvgNumCallsPerFrame and _MaxNumClassPerFrame

	/** Number of times this event was called. */
	double _NumCallsPerFrame;

	/** Average number of times this event was called. */
	double _AvgNumCallsPerFrame;	

	/** Maximum number of times this event was called. */
	//double _MaxNumCallsPerFrame;	

	/** Exclusive time of this event, in milliseconds. */
	double _ExclusiveTimeMS;

	/** Exclusive time of this event as percent of this call's inclusive time. */
	double _ExclusiveTimePct;


	//{
	/** Duration of the frame that this event belongs to, in milliseconds. */
	double _FrameDurationMS;

	/** Duration of the thread that this event was captured on, in milliseconds. */
	double _ThreadDurationMS;
	//}

	/** Percent of time spent in the thread in relation to the entire frame. */
	double _ThreadToFramePct;

	/** Percent of inclusive time spent by this event in the particular thread. */
	double _ThreadPct;

	/** Percent of inclusive time spent by this event in the particular frame. */
	double _FramePct;

	/** True, if this event is marked as being in the hot path. */
	bool _bIsHotPath;

	/** True, if this event is marked as being filtered based, but still should be visible in the event graph, but faded. */
	bool _bIsFiltered;

	/** True, if this event is marked as being culled and shouldn't be visible in the event graph. */
	bool _bIsCulled;

private:
	/** Whether we need to update the array that contains non culled children. */
	bool bNeedNotCulledChildrenUpdate;
};

/*-----------------------------------------------------------------------------
	Sorting by property
-----------------------------------------------------------------------------*/

/** Enumerates compare operations. */
namespace EEventCompareOps
{
	enum Type
	{
		/** A < B. */
		Less,
		/** B < A. */
		Greater,

		/** A == B. */
		Equal,

		/** A contains B. */
		Contains,

		/** Invalid enum type, may be used as a number of enumerations. */
		InvalidOrMax,
	};
}

namespace EventGraphPrivate
{
	/** Helper class used to during sorting the events. */
	template< const EEventPropertyTypes::Type PropType, const EEventCompareOps::Type CompareOp >
	struct TCompareByProperty
	{
	private:
		const EEventPropertyIndex::Type PropertyIndex;

	public:
		FORCEINLINE TCompareByProperty( const EEventPropertyIndex::Type InPropertyIndex )
			: PropertyIndex( InPropertyIndex )
		{}
	};

	template<>
	struct TCompareByProperty<EEventPropertyTypes::Name,EEventCompareOps::Greater>
	{
	private:
		const EEventPropertyIndex::Type PropertyIndex;

	public:
		FORCEINLINE TCompareByProperty( const EEventPropertyIndex::Type InPropertyIndex )
			: PropertyIndex( InPropertyIndex )
		{}

		FORCEINLINE_DEBUGGABLE bool operator()( const FEventGraphSamplePtr& A, const FEventGraphSamplePtr& B ) const 
		{
			return A.Get()->GetPropertyValueAsString(PropertyIndex) > B.Get()->GetPropertyValueAsString(PropertyIndex);
		}
	};

	template<>
	struct TCompareByProperty<EEventPropertyTypes::Double,EEventCompareOps::Greater>
	{
	private:
		const EEventPropertyIndex::Type PropertyIndex;

	public:
		FORCEINLINE TCompareByProperty( const EEventPropertyIndex::Type InPropertyIndex )
			: PropertyIndex( InPropertyIndex )
		{}

		FORCEINLINE_DEBUGGABLE bool operator()( const FEventGraphSamplePtr& A, const FEventGraphSamplePtr& B ) const 
		{
			return A.Get()->PropertyValueAsDouble(PropertyIndex) > B.Get()->PropertyValueAsDouble(PropertyIndex);
		}
	};

	template<>
	struct TCompareByProperty<EEventPropertyTypes::Name,EEventCompareOps::Less>
	{
	private:
		const EEventPropertyIndex::Type PropertyIndex;

	public:
		FORCEINLINE TCompareByProperty( const EEventPropertyIndex::Type InPropertyIndex )
			: PropertyIndex( InPropertyIndex )
		{}

		FORCEINLINE_DEBUGGABLE bool operator()( const FEventGraphSamplePtr& A, const FEventGraphSamplePtr& B ) const 
		{
			return A.Get()->GetPropertyValueAsString(PropertyIndex) < B.Get()->GetPropertyValueAsString(PropertyIndex);
		}
	};

	template<>
	struct TCompareByProperty<EEventPropertyTypes::Double,EEventCompareOps::Less>
	{
	private:
		const EEventPropertyIndex::Type PropertyIndex;

	public:
		FORCEINLINE TCompareByProperty( const EEventPropertyIndex::Type InPropertyIndex )
			: PropertyIndex( InPropertyIndex )
		{}

		FORCEINLINE_DEBUGGABLE bool operator()( const FEventGraphSamplePtr& A, const FEventGraphSamplePtr& B ) const 
		{
			return A.Get()->PropertyValueAsDouble(PropertyIndex) < B.Get()->PropertyValueAsDouble(PropertyIndex);
		}
	};
}


/**
 * Class used to executing specified operation for the specified property on array of events.
 * After executing the specified boolean property will be changed accordingly.
 */
class FEventArrayBooleanOp
{
	// @TODO: Move parameters to constructor and remove static
	// FEventArrayBooleanOp( parameters ).Execute()
	// vs
	// FEventGraphSample::ExecuteOperation( parameters )
public:
	static void ExecuteOperation
	( 
		FEventGraphSamplePtr DestPtr, 
		const EEventPropertyIndex::Type DestPropertyIndex, 
		const FEventGraphSamplePtr SrcPtr, 
		const EEventPropertyIndex::Type SrcPropertyIndex, 
		const EEventCompareOps::Type OpType
	)
	{
		const FEventProperty& SrcEventProperty = FEventGraphSample::GetEventPropertyByIndex(SrcPropertyIndex);
		const bool bCompareAsStrings = SrcEventProperty.IsName();
		const bool bCompareAsFloat = SrcEventProperty.IsDouble();

		const FEventProperty& DestEventProperty = FEventGraphSample::GetEventPropertyByIndex(DestPropertyIndex);
		check( DestEventProperty.IsBoolean() );
	
		// These branches should be gone once template is instantiated.
		if( OpType == EEventCompareOps::Less )
		{
			if( bCompareAsStrings )
			{
				// @TODO: For string use Contains instead of Less/Greater
				const auto Value = SrcPtr->GetPropertyValueAsString(SrcEventProperty.Index);
				ExecuteOperationInternal( DestPtr->GetChildren(), DestEventProperty, SrcPtr, EventGraphPrivate::TCompareByProperty<EEventPropertyTypes::Name,EEventCompareOps::Less>(SrcEventProperty.Index) );
			}
			else if( bCompareAsFloat )
			{
				const auto Value = SrcPtr->PropertyValueAsDouble(SrcEventProperty.Index);
				ExecuteOperationInternal( DestPtr->GetChildren(), DestEventProperty, SrcPtr, EventGraphPrivate::TCompareByProperty<EEventPropertyTypes::Double,EEventCompareOps::Less>(SrcEventProperty.Index) );
			}

		}
		else if( OpType == EEventCompareOps::Greater )
		{
			if( bCompareAsStrings )
			{
				const auto Value = SrcPtr->GetPropertyValueAsString(SrcEventProperty.Index);
				ExecuteOperationInternal( DestPtr->GetChildren(), DestEventProperty, SrcPtr, EventGraphPrivate::TCompareByProperty<EEventPropertyTypes::Name,EEventCompareOps::Greater>(SrcEventProperty.Index) );
			}
			else if( bCompareAsFloat )
			{
				const auto Value = SrcPtr->PropertyValueAsDouble(SrcEventProperty.Index);
				ExecuteOperationInternal( DestPtr->GetChildren(), DestEventProperty, SrcPtr, EventGraphPrivate::TCompareByProperty<EEventPropertyTypes::Double,EEventCompareOps::Greater>(SrcEventProperty.Index) );
			}
		}
	}

private:
	template < const EEventPropertyTypes::Type PropType, const EEventCompareOps::Type CompareOp >
	static void ExecuteOperationInternal
	( 
		TArray< FEventGraphSamplePtr >& DestEvents, 
		const FEventProperty& DestEventProperty, 
		const FEventGraphSamplePtr& SrcPtr,
		const EventGraphPrivate::TCompareByProperty<PropType,CompareOp>& Comparator 
	)
	{
		for( int32 ChildIndex = 0; ChildIndex < DestEvents.Num(); ChildIndex++ )
		{
			FEventGraphSamplePtr& Child = DestEvents[ChildIndex];
			const bool bBooleanState = Comparator( Child, SrcPtr );
			Child->PropertyValueAsBool(DestEventProperty.Index) = bBooleanState;
			ExecuteOperationInternal( Child->GetChildren(), DestEventProperty, SrcPtr, Comparator );
		}
	}

	static void ExecuteAssignOperation( TArray< FEventGraphSamplePtr >& DestEvents, const FEventProperty& DestEventProperty )
	{
		for( int32 ChildIndex = 0; ChildIndex < DestEvents.Num(); ChildIndex++ )
		{
			FEventGraphSamplePtr& Child = DestEvents[ChildIndex];
			Child->PropertyValueAsBool(DestEventProperty.Index) = true;
			ExecuteAssignOperation( Child->GetChildren(), DestEventProperty );
		}
	}
};

/** Class used to sorting array of events, based on specified property. */
class FEventArraySorter
{
public:
	static void Sort( TArray< FEventGraphSamplePtr >& ChildrenToSort, const FName PropertyName, const EEventCompareOps::Type OpType )
	{
		const FEventProperty& EventProperty = FEventGraphSample::GetEventPropertyByName(PropertyName);
		const bool bCompareAsStrings = EventProperty.IsName();
		const bool bCompareAsFloat = EventProperty.IsDouble();

		if( OpType == EEventCompareOps::Greater )
		{
			if( bCompareAsStrings )
			{
				SortInternal( ChildrenToSort, EventGraphPrivate::TCompareByProperty<EEventPropertyTypes::Name,EEventCompareOps::Greater>(EventProperty.Index) );
			}
			else if( bCompareAsFloat )
			{
				SortInternal( ChildrenToSort, EventGraphPrivate::TCompareByProperty<EEventPropertyTypes::Double,EEventCompareOps::Greater>(EventProperty.Index) );
			}
		}
		else if( OpType == EEventCompareOps::Less )
		{
			if( bCompareAsStrings )
			{
				SortInternal( ChildrenToSort, EventGraphPrivate::TCompareByProperty<EEventPropertyTypes::Name,EEventCompareOps::Less>(EventProperty.Index) );
			}
			else if( bCompareAsFloat )
			{
				SortInternal( ChildrenToSort, EventGraphPrivate::TCompareByProperty<EEventPropertyTypes::Double,EEventCompareOps::Less>(EventProperty.Index) );
			}
		}
	}

private:
	template< const EEventPropertyTypes::Type PropType, const EEventCompareOps::Type CompareOp >
	static void SortInternal( TArray< FEventGraphSamplePtr >& ChildrenToSort, const EventGraphPrivate::TCompareByProperty<PropType,CompareOp>& CompareInstance )
	{
		ChildrenToSort.Sort( CompareInstance );

		for( int32 ChildIndex = 0; ChildIndex < ChildrenToSort.Num(); ChildIndex++ )
		{
			TArray<FEventGraphSamplePtr>& Children = ChildrenToSort[ChildIndex]->GetChildren();
			if( Children.Num() )
			{
				SortInternal( Children, CompareInstance );
			}
		}
	}
};

/*-----------------------------------------------------------------------------
	FEventGraphData related classes
-----------------------------------------------------------------------------*/

/** Enumerates event graph types. */
namespace EEventGraphTypes
{
	enum Type
	{
		/** Per-frame average event graph. */
		Average,

		/** Highest "per-frame" event graph. */
		Maximum,

		/** Event graph for one frame, so both average and maximum can be used. */
		OneFrame,

		/** Invalid enum type, may be used as a number of enumerations. */
		InvalidOrMax,
	};

	/**
	 * @param EventGraphType - The value to get the string for.
	 *
	 * @return string representation of the specified EEventGraphTypes value.
	 */
	FString ToName( const EEventGraphTypes::Type EventGraphType );

	/**
	 * @param EventGraphType - The value to get the string for.
	 *
	 * @return string representation with more detailed explanation of the specified EEventGraphTypes value.
	 */
	FString ToDescription( const EEventGraphTypes::Type EventGraphType );
}

/** POD helper class for storing information like type and frame indices about the event graph. */
class FEventGraphDataProperties
{
public:
	FEventGraphDataProperties( uint32 InFrameStartIndex, uint32 InFrameEndIndex, const EEventGraphTypes::Type InEventGraphType )
		: FrameStartIndex( InFrameStartIndex )
		, FrameEndIndex( InFrameEndIndex )
		, EventGraphType( InEventGraphType )
	{}

	uint32 FrameStartIndex;
	uint32 FrameEndIndex;
	EEventGraphTypes::Type EventGraphType;
};

template<> struct TIsPODType<FEventGraphDataProperties> { enum { Value = true }; };

/** Helper class used to initialize event graph samples. */
class FEventGraphDataHandler : public TSharedFromThis<FEventGraphDataHandler>
{
public:
	FEventGraphDataHandler()
	{}

	FEventGraphDataHandler( const FGuid SessionInstanceID, const uint32 FrameStartIndex, const uint32 FrameEndIndex, const EEventGraphTypes::Type EventGraphType )
	{
		EventGraphProperties.Add( SessionInstanceID, FEventGraphDataProperties(FrameStartIndex,FrameEndIndex,EventGraphType) );
	}

	/** Contains information about frames that require opening an event graphs, stored as SessionInstanceID -> StartIndex. */
	TMap<FGuid,FEventGraphDataProperties> EventGraphProperties;
};

/* 
 * Provides access only to the profiler samples specified by a frame index or frame indices.
 * This class allows accessing root and child samples which may be used to create an event graph.
 */
class FEventGraphData : public FNoncopyable
{
	friend class FProfilerSession;

public:
	/** Minimal default constructor. */
	FEventGraphData();

	/** Destructor, for debugging purpose only. */
	FORCEINLINE_DEBUGGABLE ~FEventGraphData()
	{
#if	_DEBUG
		int k=0;k++;
		RootEvent;
#endif // _DEBUG
	}

protected:
	/**
	 * Initialization constructor, hidden on purpose, may be only called from the FProfilerSession class.
	 * 
	 * @param InProfilerSession	- a const reference to the profiler session that will be used to generate this event graph data 
	 * @param InFrameIndex		- the frame number from which to generate this event graph data
	 *
	 */
	FEventGraphData( const FProfilerSessionRef& InProfilerSession, const uint32 InFrameIndex );

	/** Copy constructor, create a full duplication of the source event graph data. */
	FEventGraphData( const FEventGraphData& Source );

	/**
	 * Recursively populates the hierarchy of the event graph samples.
	 *
	 * @param ParentEventGraphSample - <describe ParentEventGraphSample>
	 * @param ParentProfilerSample - <describe ParentProfilerSample>
	 * @param DataProvider - <describe DataProvider>
	 * @param FrameDurationMS - <describe FrameDurationMS>
	 * @param ThreadName - <describe ThreadName>
	 * @param ThreadDurationMS - <describe ThreadDurationMS>
	 *
	 */
	void PopulateHierarchy_Recurrent
	( 
		const FProfilerSessionRef& ProfilerSession,
		const FEventGraphSamplePtr ParentEvent, 
		const FProfilerSample& ParentSample, 
		const IDataProviderRef DataProvider
	);

public:
	
	/**
	 * @return a duplicated instance of this event graph, this is a deep duplication, which means that for all events a new event is created
	 */
	FEventGraphDataRef DuplicateAsRef();

	void CombineAndAdd( const FEventGraphData& Other );

	void CombineAndFindMax( const FEventGraphData& Other );

	void Divide( const int32 NumFrames );


	void Advance( const uint32 InFrameStartIndex, const uint32 InFrameEndIndex );

	/**
	 * @return root event that contains all thread root events and its children
	 */
	FORCEINLINE const FEventGraphSamplePtr& GetRoot() const
	{
		return RootEvent;
	}

public:

	/**
	 * @return the frame start index this event graph data was generated from.
	 */
	FORCEINLINE const uint32 GetFrameStartIndex() const
	{
		return FrameStartIndex;
	}

	/**
	 * @return the frame end index this event graph data was generated from.
	 */
	FORCEINLINE const uint32 GetFrameEndIndex() const
	{
		return FrameEndIndex;
	}

	/**
	 * @return the number of frames used to create this event graph data.
	 */
	const uint32 GetNumFrames() const
	{
		return FrameEndIndex - FrameStartIndex;
	}

	/**
	 * @return the description for this event graph data.
	 */
	FORCEINLINE const FString& GetDescription() const
	{
		return Description;
	}

	/**
	 * @return the profiler session that was used to generate this event graph data, may be null.
	 */
	FProfilerSessionPtr GetProfilerSession() const
	{
		return ProfilerSessionPtr.Pin();
	}

protected:
	// TODO: FEventGraphSamplePtr -> FEventGraphSampleRef

	/** Root sample, contains all thread samples and its children. */
	FEventGraphSamplePtr RootEvent;

	/** Description as "SessionName - FrameIndex/Indices". */
	FString Description;

	/** The frame start index this event graph data was generated from. */
	uint32 FrameStartIndex;

	/** The frame end index this event graph data was generated from. */
	uint32 FrameEndIndex;

	/** A weak pointer to the profiler session that was used to generate this event graph data. */
	FProfilerSessionWeak ProfilerSessionPtr;
};

//}//namespace FEventGraphSample
