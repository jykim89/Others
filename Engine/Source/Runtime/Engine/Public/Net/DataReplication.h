// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DataReplication.h:
	Holds classes for data replication (properties and RPCs).
=============================================================================*/
#pragma once

#include "RepLayout.h"

/** struct containing property and offset for replicated actor properties */
struct FReplicatedActorProperty
{
	/** offset into the Actor where this reference is located - includes offsets from any outer structs */
	int32 Offset;
	/** Reference to property object */
	const class UObjectPropertyBase* Property;

	FReplicatedActorProperty(int32 InOffset, const UObjectPropertyBase* InProperty)
		: Offset(InOffset), Property(InProperty)
	{}
};

/** FObjectReplicator
 *   Generic class that replicates properties for an object.
 *   All delta/diffing work is done in this class. 
 *	 Its primary job is to produce and consume chunks of properties/RPCs:
 *
 *		|----------------|
 *		| NetGUID ObjRef |
 * 		|----------------|
 *      |                |		
 *		| Properties...  |
 *		|                |	
 *		| RPCs...        |
 *      |                |
 *      |----------------|
 *		| </End Tag>     |
 *		|----------------|
 *	
 */
class ENGINE_API FObjectReplicator
{
public:
	FObjectReplicator() : 
		ObjectClass( NULL ), 
		bLastUpdateEmpty( false ), 
		bOpenAckCalled( false ),
		Connection( NULL ),
		OwningChannel( NULL ),
		RepState( NULL ),
		RemoteFunctions( NULL )
	{ }

	~FObjectReplicator() 
	{
		CleanUp();
	}

	UClass *										ObjectClass;
	FNetworkGUID									ObjectNetGUID;
	TWeakObjectPtr< UObject >						ObjectPtr;

	TArray<FPropertyRetirement>						Retirement;					// Property retransmission.
	TMap<int32, TSharedPtr<INetDeltaBaseState> >	RecentCustomDeltaState;		// Stores dynamic properties such as TArray which can't fit in the Recent buffer

	TArray< int32 >									LifetimeCustomDeltaProperties;

	uint32											bLastUpdateEmpty	: 1;	// True if last update (ReplicateActor) produced no replicated properties
	uint32											bOpenAckCalled		: 1;

	UNetConnection *								Connection;					// Connection this replicator was created on
	class UActorChannel	*							OwningChannel;

	TArray< UProperty *,TInlineAllocator< 32 > >	RepNotifies;
	TMap< UProperty *, TArray<uint8> >				RepNotifyMetaData;

	TSharedPtr< FRepLayout >						RepLayout;
	FRepState *										RepState;

	struct FRPCCallInfo 
	{
		FName	FuncName;
		int32	Calls;
	};

	TArray< FRPCCallInfo >							RemoteFuncInfo;				// Meta information on pending net RPCs (to be sent)
	FOutBunch *										RemoteFunctions;

	void InitWithObject( UObject * InObject, UNetConnection * InConnection, bool bUseDefaultState = true );
	void CleanUp();

	void StartReplicating( class UActorChannel * InActorChannel );
	void StopReplicating( class UActorChannel * InActorChannel );

	/** Recent/dirty related functions */
	void InitRecentProperties( uint8 * Source );

	/** Takes Data, and compares against shadow state to log differences */
	bool ValidateAgainstState( const UObject * ObjectState );

	static bool SerializeCustomDeltaProperty( UNetConnection * Connection, void * Src, UProperty * Property, int32 ArrayDim, FNetBitWriter & OutBunch, TSharedPtr<INetDeltaBaseState> & NewFullState, TSharedPtr<INetDeltaBaseState> & OldState );

	/** Packet was dropped */
	void	ReceivedNak( int32 NakPacketId );

	void	Serialize(FArchive& Ar);

	/** Writes dirty properties to bunch */
	void	ReplicateCustomDeltaProperties( FOutBunch & Bunch, int32 & LastIndex, bool & bContentBlockWritten );
	bool	ReplicateProperties( FOutBunch & Bunch, FReplicationFlags RepFlags );
	void	PostSendBunch(FPacketIdRange & PacketRange, uint8 bReliable);
	
	bool	ReceivedBunch( FInBunch & Bunch, const FReplicationFlags & RepFlags, bool & bOutHasUnmapped );
	void	PostReceivedBunch();

	void ForceRefreshUnreliableProperties();

	bool bHasReplicatedProperties;

	void QueueRemoteFunctionBunch( UFunction* Func, FOutBunch &Bunch );

	bool ReadyForDormancy(bool debug=false);

	void StartBecomingDormant();

	void UpdateUnmappedObjects( bool & bOutHasMoreUnmapped );

	FORCEINLINE UObject *	GetObject() const { return ObjectPtr.Get(); }
	FORCEINLINE void		SetObject( UObject * NewObj ) { ObjectPtr = TWeakObjectPtr<UObject>( NewObj ); }

	FORCEINLINE void PreNetReceive()		
	{ 
		UObject * Object = GetObject();
		if ( Object != NULL )
		{
			Object->PreNetReceive(); 
		}
	}

	FORCEINLINE void PostNetReceive()	
	{ 
		UObject * Object = GetObject();
		if ( Object != NULL )
		{
			Object->PostNetReceive(); 
		}
	}

	void QueuePropertyRepNotify( UObject * Object, UProperty * Property, const int32 ElementIndex, TArray< uint8 > & MetaData );
};