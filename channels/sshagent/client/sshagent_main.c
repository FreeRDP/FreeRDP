/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SSH Agent Virtual Channel Extension
 *
 * Copyright 2013 Christian Hofstaedtler
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2017 Ben Cohen
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

/*
 * sshagent_main.c: DVC plugin to forward queries from RDP to the ssh-agent
 *
 * This relays data to and from an ssh-agent program equivalent running on the
 * RDP server to an ssh-agent running locally.  Unlike the normal ssh-agent,
 * which sends data over an SSH channel, the data is send over an RDP dynamic
 * virtual channel.
 *
 * protocol specification:
 *     Forward data verbatim over RDP dynamic virtual channel named "sshagent"
 *     between a ssh client on the xrdp server and the real ssh-agent where
 *     the RDP client is running.  Each connection by a separate client to
 *     xrdp-ssh-agent gets a separate DVC invocation.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pwd.h>
#include <unistd.h>
#include <errno.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>

#include "sshagent_main.h"
#include <freerdp/channels/log.h>

#define TAG CHANNELS_TAG("sshagent.client")

typedef struct _SSHAGENT_LISTENER_CALLBACK SSHAGENT_LISTENER_CALLBACK;
struct _SSHAGENT_LISTENER_CALLBACK
{
	IWTSListenerCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;

	rdpContext* rdpcontext;
	const char* agent_uds_path;
};

typedef struct _SSHAGENT_CHANNEL_CALLBACK SSHAGENT_CHANNEL_CALLBACK;
struct _SSHAGENT_CHANNEL_CALLBACK
{
	IWTSVirtualChannelCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
	IWTSVirtualChannel* channel;

	rdpContext* rdpcontext;
	int agent_fd;
	HANDLE thread;
	CRITICAL_SECTION lock;
};

typedef struct _SSHAGENT_PLUGIN SSHAGENT_PLUGIN;
struct _SSHAGENT_PLUGIN
{
	IWTSPlugin iface;

	SSHAGENT_LISTENER_CALLBACK* listener_callback;

	rdpContext* rdpcontext;
};


/**
 * Function to open the connection to the sshagent
 *
 * @return The fd on success, otherwise -1
 */
static int connect_to_sshagent(const char* udspath)
{
	int agent_fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (agent_fd == -1)
	{
		WLog_ERR(TAG, "Can't open Unix domain socket!");
		return -1;
	}

	struct sockaddr_un addr;

	memset(&addr, 0, sizeof(addr));

	addr.sun_family = AF_UNIX;

	strncpy(addr.sun_path, udspath, sizeof(addr.sun_path) - 1);

	int rc = connect(agent_fd, (struct sockaddr*)&addr, sizeof(addr));

	if (rc != 0)
	{
		WLog_ERR(TAG, "Can't connect to Unix domain socket \"%s\"!",
		         udspath);
		close(agent_fd);
		return -1;
	}

	return agent_fd;
}


/**
 * Entry point for thread to read from the ssh-agent socket and forward
 * the data to RDP
 *
 * @return NULL
 */
static void* sshagent_read_thread(void* data)
{
	SSHAGENT_CHANNEL_CALLBACK* callback = (SSHAGENT_CHANNEL_CALLBACK*)data;
	BYTE buffer[4096];
	int going = 1;
	UINT status = CHANNEL_RC_OK;

	while (going)
	{
		int bytes_read = read(callback->agent_fd,
		                      buffer,
		                      sizeof(buffer));

		if (bytes_read == 0)
		{
			/* Socket closed cleanly at other end */
			going = 0;
		}
		else if (bytes_read < 0)
		{
			if (errno != EINTR)
			{
				WLog_ERR(TAG,
				         "Error reading from sshagent, errno=%d",
				         errno);
				status = ERROR_READ_FAULT;
				going = 0;
			}
		}
		else
		{
			/* Something read: forward to virtual channel */
			status = callback->channel->Write(callback->channel,
			                                  bytes_read,
			                                  buffer,
			                                  NULL);

			if (status != CHANNEL_RC_OK)
			{
				going = 0;
			}
		}
	}

	close(callback->agent_fd);

	if (status != CHANNEL_RC_OK)
		setChannelError(callback->rdpcontext, status,
		                "sshagent_read_thread reported an error");

	ExitThread(0);
	return NULL;
}

/**
 * Callback for data received from the RDP server; forward this to ssh-agent
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT sshagent_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream* data)
{
	SSHAGENT_CHANNEL_CALLBACK* callback = (SSHAGENT_CHANNEL_CALLBACK*) pChannelCallback;
	BYTE* pBuffer = Stream_Pointer(data);
	UINT32 cbSize = Stream_GetRemainingLength(data);
	BYTE* pos = pBuffer;
	/* Forward what we have received to the ssh agent */
	UINT32 bytes_to_write = cbSize;
	errno = 0;

	while (bytes_to_write > 0)
	{
		int bytes_written = write(callback->agent_fd, pos,
		                          bytes_to_write);

		if (bytes_written < 0)
		{
			if (errno != EINTR)
			{
				WLog_ERR(TAG,
				         "Error writing to sshagent, errno=%d",
				         errno);
				return ERROR_WRITE_FAULT;
			}
		}
		else
		{
			bytes_to_write -= bytes_written;
			pos += bytes_written;
		}
	}

	/* Consume stream */
	Stream_Seek(data, cbSize);
	return CHANNEL_RC_OK;
}

/**
 * Callback for when the virtual channel is closed
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT sshagent_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	SSHAGENT_CHANNEL_CALLBACK* callback = (SSHAGENT_CHANNEL_CALLBACK*) pChannelCallback;
	/* Call shutdown() to wake up the read() in sshagent_read_thread(). */
	shutdown(callback->agent_fd, SHUT_RDWR);
	EnterCriticalSection(&callback->lock);

	if (WaitForSingleObject(callback->thread, INFINITE) == WAIT_FAILED)
	{
		UINT error = GetLastError();
		WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", error);
		return error;
	}

	CloseHandle(callback->thread);
	LeaveCriticalSection(&callback->lock);
	DeleteCriticalSection(&callback->lock);
	free(callback);
	return CHANNEL_RC_OK;
}


/**
 * Callback for when a new virtual channel is opened
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT sshagent_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
        IWTSVirtualChannel* pChannel, BYTE* Data, BOOL* pbAccept,
        IWTSVirtualChannelCallback** ppCallback)
{
	SSHAGENT_CHANNEL_CALLBACK* callback;
	SSHAGENT_LISTENER_CALLBACK* listener_callback = (SSHAGENT_LISTENER_CALLBACK*) pListenerCallback;
	callback = (SSHAGENT_CHANNEL_CALLBACK*) calloc(1, sizeof(SSHAGENT_CHANNEL_CALLBACK));

	if (!callback)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	/* Now open a connection to the local ssh-agent.  Do this for each
	 * connection to the plugin in case we mess up the agent session. */
	callback->agent_fd
	    = connect_to_sshagent(listener_callback->agent_uds_path);

	if (callback->agent_fd == -1)
	{
		free(callback);
		return CHANNEL_RC_INITIALIZATION_ERROR;
	}

	InitializeCriticalSection(&callback->lock);
	callback->iface.OnDataReceived = sshagent_on_data_received;
	callback->iface.OnClose = sshagent_on_close;
	callback->plugin = listener_callback->plugin;
	callback->channel_mgr = listener_callback->channel_mgr;
	callback->channel = pChannel;
	callback->rdpcontext = listener_callback->rdpcontext;
	callback->thread
	    = CreateThread(NULL,
	                   0,
	                   (LPTHREAD_START_ROUTINE) sshagent_read_thread,
	                   (void*) callback,
	                   0,
	                   NULL);

	if (!callback->thread)
	{
		WLog_ERR(TAG, "CreateThread failed!");
		DeleteCriticalSection(&callback->lock);
		free(callback);
		return CHANNEL_RC_INITIALIZATION_ERROR;
	}

	*ppCallback = (IWTSVirtualChannelCallback*) callback;
	return CHANNEL_RC_OK;
}

/**
 * Callback for when the plugin is initialised
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT sshagent_plugin_initialize(IWTSPlugin* pPlugin, IWTSVirtualChannelManager* pChannelMgr)
{
	SSHAGENT_PLUGIN* sshagent = (SSHAGENT_PLUGIN*) pPlugin;
	sshagent->listener_callback = (SSHAGENT_LISTENER_CALLBACK*) calloc(1,
	                              sizeof(SSHAGENT_LISTENER_CALLBACK));

	if (!sshagent->listener_callback)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	sshagent->listener_callback->rdpcontext = sshagent->rdpcontext;
	sshagent->listener_callback->iface.OnNewChannelConnection = sshagent_on_new_channel_connection;
	sshagent->listener_callback->plugin = pPlugin;
	sshagent->listener_callback->channel_mgr = pChannelMgr;
	sshagent->listener_callback->agent_uds_path = getenv("SSH_AUTH_SOCK");

	if (sshagent->listener_callback->agent_uds_path == NULL)
	{
		WLog_ERR(TAG, "Environment variable $SSH_AUTH_SOCK undefined!");
		free(sshagent->listener_callback);
		sshagent->listener_callback = NULL;
		return CHANNEL_RC_INITIALIZATION_ERROR;
	}

	return pChannelMgr->CreateListener(pChannelMgr, "SSHAGENT", 0,
	                                   (IWTSListenerCallback*) sshagent->listener_callback, NULL);
}

/**
 * Callback for when the plugin is terminated
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT sshagent_plugin_terminated(IWTSPlugin* pPlugin)
{
	SSHAGENT_PLUGIN* sshagent = (SSHAGENT_PLUGIN*) pPlugin;
	free(sshagent);
	return CHANNEL_RC_OK;
}

#ifdef BUILTIN_CHANNELS
#define DVCPluginEntry		sshagent_DVCPluginEntry
#else
#define DVCPluginEntry		FREERDP_API DVCPluginEntry
#endif

/**
 * Main entry point for sshagent DVC plugin
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	UINT status = CHANNEL_RC_OK;
	SSHAGENT_PLUGIN* sshagent;
	sshagent = (SSHAGENT_PLUGIN*) pEntryPoints->GetPlugin(pEntryPoints, "sshagent");

	if (!sshagent)
	{
		sshagent = (SSHAGENT_PLUGIN*) calloc(1, sizeof(SSHAGENT_PLUGIN));

		if (!sshagent)
		{
			WLog_ERR(TAG, "calloc failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		sshagent->iface.Initialize = sshagent_plugin_initialize;
		sshagent->iface.Connected = NULL;
		sshagent->iface.Disconnected = NULL;
		sshagent->iface.Terminated = sshagent_plugin_terminated;
		sshagent->rdpcontext = ((freerdp*)((rdpSettings*) pEntryPoints->GetRdpSettings(
		                                       pEntryPoints))->instance)->context;
		status = pEntryPoints->RegisterPlugin(pEntryPoints, "sshagent", (IWTSPlugin*) sshagent);
	}

	return status;
}

/* vim: set sw=8:ts=8:noet: */
