// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SlateRemotePrivatePCH.h: Pre-compiled header file for the SlateRemote module.
=============================================================================*/

#pragma once


/* Private Dependencies
 *****************************************************************************/

#include "Core.h"
#include "ModuleManager.h"
#include "Networking.h"
#include "Settings.h"
#include "Slate.h"
#include "SlateCore.h"
#include "Sockets.h"
#include "SocketSubsystem.h"


/* Private Constants
 *****************************************************************************/

/**
 * Defines the default IP endpoint for the Slate Remote server running in the Editor.
 */
#define SLATE_REMOTE_SERVER_DEFAULT_EDITOR_ENDPOINT FIPv4Endpoint(FIPv4Address(0, 0, 0, 0), 41766)

/**
 * Defines the default IP endpoint for the Slate Remote server running in a game.
 */
#define SLATE_REMOTE_SERVER_DEFAULT_GAME_ENDPOINT FIPv4Endpoint(FIPv4Address(0, 0, 0, 0), 41765)

/**
 * Defines the protocol version of the UDP message transport.
 */
#define SLATE_REMOTE_SERVER_PROTOCOL_VERSION 1


/* Private includes
 *****************************************************************************/

#include "SlateRemoteSettings.h"
#include "SlateRemoteServerMessage.h"
#include "SlateRemoteServer.h"
