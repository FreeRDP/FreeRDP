/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Dynamic Virtual Channel Interface
 *
 * Copyright 2010-2011 Vic Lee
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * DVC Plugin API: See the original MS DVC Client API:
 * http://msdn.microsoft.com/en-us/library/bb540880%28v=VS.85%29.aspx
 *
 * The FreeRDP DVC Plugin API is a simulation of the MS DVC Client API in C.
 * The main difference is that every interface method must take an instance
 * pointer as the first parameter.
 */

/**
 * Implemented by DRDYNVC:
 * o IWTSVirtualChannelManager
 * o IWTSListener
 * o IWTSVirtualChannel
 *
 * Implemented by DVC plugin:
 * o IWTSPlugin
 * o IWTSListenerCallback
 * o IWTSVirtualChannelCallback
 *
 * A basic DVC plugin implementation:
 * 1. DVCPluginEntry:
 *    The plugin entry point, which creates and initializes a new IWTSPlugin
 *    instance
 * 2. IWTSPlugin.Initialize:
 *    Call IWTSVirtualChannelManager.CreateListener with a newly created
 *    IWTSListenerCallback instance
 * 3. IWTSListenerCallback.OnNewChannelConnection:
 *    Create IWTSVirtualChannelCallback instance if the new channel is accepted
 */

#ifndef __FREERDP_DVC_H
#define __FREERDP_DVC_H

#include <freerdp/types.h>

typedef struct _IWTSVirtualChannelManager IWTSVirtualChannelManager;
typedef struct _IWTSListener IWTSListener;
typedef struct _IWTSVirtualChannel IWTSVirtualChannel;

typedef struct _IWTSPlugin IWTSPlugin;
typedef struct _IWTSListenerCallback IWTSListenerCallback;
typedef struct _IWTSVirtualChannelCallback IWTSVirtualChannelCallback;

struct _IWTSListener
{
	/* Retrieves the listener-specific configuration. */
	int (*GetConfiguration) (IWTSListener* pListener,
		void** ppPropertyBag);
};

struct _IWTSVirtualChannel
{
	/* Starts a write request on the channel. */
	int (*Write) (IWTSVirtualChannel* pChannel,
		uint32 cbSize,
		uint8* pBuffer,
		void* pReserved);
	/* Closes the channel. */
	int (*Close) (IWTSVirtualChannel* pChannel);
};

struct _IWTSVirtualChannelManager
{
	/* Returns an instance of a listener object that listens on a specific
	   endpoint, or creates a static channel. */
	int (*CreateListener) (IWTSVirtualChannelManager* pChannelMgr,
		const char* pszChannelName,
		uint32 ulFlags,
		IWTSListenerCallback* pListenerCallback,
		IWTSListener** ppListener);
	/* Push a virtual channel event.
	   This is a FreeRDP extension to standard MS API. */
	int (*PushEvent) (IWTSVirtualChannelManager* pChannelMgr,
		RDP_EVENT* pEvent);
};

struct _IWTSPlugin
{
	/* Used for the first call that is made from the client to the plug-in. */
	int (*Initialize) (IWTSPlugin* pPlugin,
		IWTSVirtualChannelManager* pChannelMgr);
	/* Notifies the plug-in that the Remote Desktop Connection (RDC) client
	   has successfully connected to the Remote Desktop Session Host (RD
	   Session Host) server. */
	int (*Connected) (IWTSPlugin* pPlugin);
	/* Notifies the plug-in that the Remote Desktop Connection (RDC) client
	   has disconnected from the RD Session Host server. */
	int (*Disconnected) (IWTSPlugin* pPlugin,
		uint32 dwDisconnectCode);
	/* Notifies the plug-in that the Remote Desktop Connection (RDC) client
	   has terminated. */
	int (*Terminated) (IWTSPlugin* pPlugin);
};

struct _IWTSListenerCallback
{
	/* Accepts or denies a connection request for an incoming connection to
	   the associated listener. */
	int (*OnNewChannelConnection) (IWTSListenerCallback* pListenerCallback,
		IWTSVirtualChannel* pChannel,
		uint8* Data,
		int* pbAccept,
		IWTSVirtualChannelCallback** ppCallback);
};

struct _IWTSVirtualChannelCallback
{
	/* Notifies the user about data that is being received. */
	int (*OnDataReceived) (IWTSVirtualChannelCallback* pChannelCallback,
		uint32 cbSize,
		uint8* pBuffer);
	/* Notifies the user that the channel has been closed. */
	int (*OnClose) (IWTSVirtualChannelCallback* pChannelCallback);
};

/* The DVC Plugin entry points */
typedef struct _IDRDYNVC_ENTRY_POINTS IDRDYNVC_ENTRY_POINTS;
struct _IDRDYNVC_ENTRY_POINTS
{
	int (*RegisterPlugin) (IDRDYNVC_ENTRY_POINTS* pEntryPoints,
		const char* name, IWTSPlugin* pPlugin);
	IWTSPlugin* (*GetPlugin) (IDRDYNVC_ENTRY_POINTS* pEntryPoints,
		const char* name);
	RDP_PLUGIN_DATA* (*GetPluginData) (IDRDYNVC_ENTRY_POINTS* pEntryPoints);
};

typedef int (*PDVC_PLUGIN_ENTRY) (IDRDYNVC_ENTRY_POINTS*);

#endif
