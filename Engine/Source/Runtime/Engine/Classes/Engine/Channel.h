// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

//
// Base class of communication channels.
//

#pragma once
#include "Channel.generated.h"

// Constant for all buffers that are reading from the network
const int MAX_STRING_SERIALIZE_SIZE	= NAME_SIZE;

// Types of channels.
enum EChannelType
{
	CHTYPE_None			= 0,  // Invalid type.
	CHTYPE_Control		= 1,  // Connection control.
	CHTYPE_Actor  		= 2,  // Actor-update channel.
	CHTYPE_File         = 3,  // Binary file transfer.
	CHTYPE_Voice		= 4,  // VoIP data channel
	CHTYPE_MAX          = 8,  // Maximum.
};

// The channel index to use for voice
#define VOICE_CHANNEL_INDEX 1

UCLASS(transient)
class UChannel : public UObject
{
	GENERATED_UCLASS_BODY()
	
	UPROPERTY()
	class UNetConnection*	Connection;		// Owner connection.

	// Variables.
	uint32			OpenAcked:1;		// Whether open has been acknowledged.
	uint32			Closing:1;			// State of the channel.
	uint32			Dormant:1;			// Channel is going dormant (it will close but the client will not destroy 
	uint32			OpenTemporary:1;	// Opened temporarily.
	uint32			Broken:1;			// Has encountered errors and is ignoring subsequent packets.
	uint32			bTornOff:1;			// Actor associated with this channel was torn off
	uint32			bPendingDormancy:1;	// Channel wants to go dormant (it will check during tick if it can go dormant)
	int32					ChIndex;			// Index of this channel.
	int32					OpenedLocally;		// Whether channel was opened locally or by remote.
	FPacketIdRange		OpenPacketId;		// Packet the spawn message was sent in.
	EChannelType		ChType;				// Type of this channel.
	int32					NumInRec;			// Number of packets in InRec.
	int32					NumOutRec;			// Number of packets in OutRec.
	int32					NegotiatedVer;		// Negotiated version of engine = Min(client version, server version).
	class FInBunch*		InRec;				// Incoming data with queued dependencies.
	class FOutBunch*	OutRec;				// Outgoing reliable unacked data.
	class FInBunch*		InPartialBunch;		// Partial bunch we are receiving (incoming partial bunches are appended to this)

	/** UChannel statics. */
	static UClass* ChannelClasses[CHTYPE_MAX];
	
	/** @return true if the specified channel type exists. */
	static bool IsKnownChannelType( int32 Type );

	
	// Begin UObject Interface


	virtual void BeginDestroy() OVERRIDE;


	// End UObject Interface
	

	/** UChannel interface. */
	virtual void Init( UNetConnection* InConnection, int32 InChIndex, bool InOpenedLocally );

	/** Set the closing flag. */
	virtual void SetClosingFlag();

	/** Close the base channel. */
	virtual void Close();

	/** Describe the channel. */
	virtual FString Describe();

	/** Handle an incoming bunch. */
	virtual void ReceivedBunch( FInBunch& Bunch ) PURE_VIRTUAL(UChannel::ReceivedBunch,);
	
	/** Negative acknowledgment processing. */
	virtual void ReceivedNak( int32 NakPacketId );
	
	/** Handle time passing on this channel. */
	virtual void Tick();

	// General channel functions.
	/** Handle an acknowledgment on this channel. */
	void ReceivedAcks();
	
	/** Process a properly-sequenced bunch. */
	bool ReceivedSequencedBunch( FInBunch& Bunch );
	
	/** 
	 * Process a raw, possibly out-of-sequence bunch: either queue it or dispatch it.
	 * The bunch is sure not to be discarded.
	 */
	void ReceivedRawBunch( FInBunch & Bunch, bool & bOutSkipAck );
	
	/** Send a bunch if it's not overflowed, and queue it if it's reliable. */
	virtual FPacketIdRange SendBunch(FOutBunch* Bunch, bool Merge);
	
	/** Return whether this channel is ready for sending. */
	int32 IsNetReady( bool Saturate );

	/** Make sure the incoming buffer is in sequence and there are no duplicates. */
	void AssertInSequenced();

	/** cleans up channel if it hasn't already been */
	void ConditionalCleanUp()
	{
		if (!IsPendingKill())
		{
			MarkPendingKill();
			CleanUp();
		}
	}

	/** Returns true if channel is ready to go dormant (e.g., all outstanding property updates have been ACK'd) */
	virtual bool ReadyForDormancy(bool suppressLogs=false) { return false; }

	/** Puts the channel in a state to start becoming dormant. It will not become dormant until ReadyForDormancy returns true in Tick */
	virtual void StartBecomingDormant() { }

	void PrintReliableBunchBuffer();

protected:

	/** Closes the actor channel but with a 'dormant' flag set so it can be reopened */
	virtual void BecomeDormant() { }

	/** cleans up channel structures and NULLs references to the channel */
	virtual void CleanUp();

private:

	/** Just sends the bunch out on the connection */
	int32 SendRawBunch(FOutBunch* Bunch, bool Merge);

	/** Final step to prepare bunch to be sent. If reliable, adds to ack list. */
	FOutBunch* PrepBunch(FOutBunch* Bunch, FOutBunch* OutBunch, bool Merge);

	/** Received next bunch to process. This handles partial bunches */
	bool ReceivedNextBunch( FInBunch & Bunch, bool & bOutSkipAck );
};
