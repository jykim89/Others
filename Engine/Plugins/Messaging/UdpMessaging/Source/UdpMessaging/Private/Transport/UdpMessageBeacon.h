// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UdpMessageHelloSender.h: Declares the FUdpMessageHelloSender class.
=============================================================================*/

#pragma once


/**
 * Implements a beacon sender thread.
 *
 * @todo gmp: add documentation for FUdpMessageBeacon
 */
class FUdpMessageBeacon
	: public FRunnable
{
public:

	/** 
	 * Creates and initializes a new Hello sender.
	 *
	 * @param InSocket The network socket used to send Hello segments.
	 * @param InSocketId The network socket identifier (used to detect unicast endpoint).
	 * @param InMulticastEndpoint The multicast group endpoint to transport messages to.
	 */
	FUdpMessageBeacon(FSocket* InSocket, const FGuid& InSocketId, const FIPv4Endpoint& InMulticastEndpoint);

	/**
	 * Destructor.
	 */
	virtual ~FUdpMessageBeacon();

public:

	/**
	 * Gets the current time interval between Hello segments.
	 *
	 * @return Beacon interval.
	 */
	FTimespan GetBeaconInterval()
	{
		return BeaconInterval;
	}

	/**
	 * Sets the number of known IP endpoints.
	 *
	 * @param EndpointCount The current number of known endpoints.
	 */
	void SetEndpointCount(int32 EndpointCount);

public:

	// Begin FRunnable interface

	virtual bool Init() OVERRIDE;

	virtual uint32 Run() OVERRIDE;

	virtual void Stop() OVERRIDE;

	virtual void Exit() OVERRIDE { }
	
	// End FRunnable interface

protected:

	/**
	 * Sends the specified segment.
	 *
	 * @param SegmentType The type of segment to send (Hello or Bye).
	 */
	void SendSegment(EUdpMessageSegments::Type SegmentType);

private:

	// Holds the calculated interval between Hello segments.
	FTimespan BeaconInterval;

	// Holds an event signaling that an endpoint left.
	FEvent* EndpointLeftEvent;

	// Holds the number of known endpoints when NextHelloTime was last calculated.
	int32 LastEndpointCount;

	// Holds the time at which the last Hello segment was sent.
	FDateTime LastHelloSent;

	// Holds the multicast address and port number to send to.
	TSharedPtr<FInternetAddr> MulticastAddress;

	// Holds the time at which the next Hello segment must be sent.
	FDateTime NextHelloTime;

	// Holds local node identifier.
	FGuid NodeId;

	// Holds the socket used to send Hello segments.
	FSocket* Socket;

	// Holds a flag indicating that the thread is stopping.
	bool Stopping;

	// Holds the thread object.
	FRunnableThread* Thread;

private:
	
	// Defines the time interval per endpoint.
	static const FTimespan IntervalPerEndpoint;

	// Defines the minimum interval for Hello segments.
	static const FTimespan MinimumInterval;
};
