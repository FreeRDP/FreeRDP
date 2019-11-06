/*
 Basic type defines for TSX RDC

 Copyright 2013 Thincast Technologies GmbH, Authors: Martin Fleisz, Dorian Johnson

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#ifndef TSXRemoteDesktop_TSXTypes_h
#define TSXRemoteDesktop_TSXTypes_h

#pragma mark Internal state

// Represents the underlying state of a TWSession RDP connection.
typedef enum _TSXConnectionState
{
	TSXConnectionClosed =
	    0, // Session either hasn't begun connecting, or its connection has finished disconnecting.
	TSXConnectionConnecting =
	    1, // Session is in the process of establishing an RDP connection. A TCP or SSL connection
	       // might be established, but the RDP initialization sequence isn't finished.
	TSXConnectionConnected =
	    2, // Session has a full RDP connection established; though if the windows computer doesn't
	       // support NLA, a login screen might be shown in the session.
	TSXConnectionDisconnected = 3 // Session is disconnected at the RDP layer. TSX RDC might still
	                              // be disposing of resources, however.
} TSXConnectionState;

#pragma mark Session settings

// Represents the type of screen resolution the user has selected. Most are dynamic sizes, meaning
// that the actual session dimensions are calculated when connecting.
typedef enum _TSXScreenOptions
{
	TSXScreenOptionFixed = 0,     // A static resolution, like 1024x768
	TSXScreenOptionFitScreen = 1, // Upon connection, fit the session to the entire screen size
	TSXScreenOptionCustom = 2,    // Like fixed just specified by the user
} TSXScreenOptions;

typedef enum _TSXAudioPlaybackOptions
{
	TSXAudioPlaybackLocal = 0,
	TSXAudioPlaybackServer = 1,
	TSXAudioPlaybackSilent = 2
} TSXAudioPlaybackOptions;

typedef enum _TSXProtocolSecurityOptions
{
	TSXProtocolSecurityAutomatic = 0,
	TSXProtocolSecurityRDP = 1,
	TSXProtocolSecurityTLS = 2,
	TSXProtocolSecurityNLA = 3
} TSXProtocolSecurityOptions;

#endif
