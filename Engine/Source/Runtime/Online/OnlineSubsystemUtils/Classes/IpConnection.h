// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

//
// Ip based implementation of a network connection used by the net driver class
//

#pragma once
#include "IpConnection.generated.h"

UCLASS(transient, config=Engine)
class ONLINESUBSYSTEMUTILS_API UIpConnection : public UNetConnection
{
    GENERATED_UCLASS_BODY()
	// Variables.
	TSharedPtr<FInternetAddr>	RemoteAddr;
	class FSocket*				Socket;
	class FResolveInfo*			ResolveInfo;

	// Begin NetConnection Interface
	virtual void InitBase(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) OVERRIDE;
	virtual void InitRemoteConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, const class FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) OVERRIDE;
	virtual void InitLocalConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) OVERRIDE;
	virtual void LowLevelSend(void* Data,int32 Count) OVERRIDE;
	FString LowLevelGetRemoteAddress(bool bAppendPort=false) OVERRIDE;
	FString LowLevelDescribe() OVERRIDE;
	virtual int32 GetAddrAsInt(void) OVERRIDE;
	virtual int32 GetAddrPort(void) OVERRIDE;
	// End NetConnection Interface
};
