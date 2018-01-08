/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Geometry tracking Virtual Channel Extension
 *
 * Copyright 2017 David Fort <contact@hardening-consulting.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/print.h>
#include <winpr/stream.h>
#include <winpr/cmdline.h>
#include <winpr/collections.h>

#include <freerdp/addin.h>
#include <freerdp/client/geometry.h>
#include <freerdp/channels/log.h>

#define TAG CHANNELS_TAG("geometry.client")

#include "geometry_main.h"

struct _GEOMETRY_CHANNEL_CALLBACK
{
	IWTSVirtualChannelCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
	IWTSVirtualChannel* channel;
};
typedef struct _GEOMETRY_CHANNEL_CALLBACK GEOMETRY_CHANNEL_CALLBACK;

struct _GEOMETRY_LISTENER_CALLBACK
{
	IWTSListenerCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
	GEOMETRY_CHANNEL_CALLBACK* channel_callback;
};
typedef struct _GEOMETRY_LISTENER_CALLBACK GEOMETRY_LISTENER_CALLBACK;

struct _GEOMETRY_PLUGIN
{
	IWTSPlugin iface;

	IWTSListener* listener;
	GEOMETRY_LISTENER_CALLBACK* listener_callback;

	GeometryClientContext* context;
};
typedef struct _GEOMETRY_PLUGIN GEOMETRY_PLUGIN;


static UINT32 geometry_read_RGNDATA(wStream* s, UINT32 len, FREERDP_RGNDATA* rgndata)
{
	UINT32 dwSize, iType;
	INT32 right, bottom;

	if (len < 32)
	{
		WLog_ERR(TAG, "invalid RGNDATA");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, dwSize);

	if (dwSize != 32)
	{
		WLog_ERR(TAG, "invalid RGNDATA dwSize");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, iType);

	if (iType != RDH_RECTANGLE)
	{
		WLog_ERR(TAG, "iType %"PRIu32" for RGNDATA is not supported", iType);
		return ERROR_UNSUPPORTED_TYPE;
	}

	Stream_Read_UINT32(s, rgndata->nRectCount);
	Stream_Seek_UINT32(s); /* nRgnSize IGNORED */
	Stream_Read_INT32(s, rgndata->boundingRect.x);
	Stream_Read_INT32(s, rgndata->boundingRect.y);
	Stream_Read_INT32(s, right);
	Stream_Read_INT32(s, bottom);
	rgndata->boundingRect.width = right - rgndata->boundingRect.x;
	rgndata->boundingRect.height = bottom - rgndata->boundingRect.y;
	len -= 32;

	if (len / (4 * 4) < rgndata->nRectCount)
	{
		WLog_ERR(TAG, "not enough data for region rectangles");
	}

	if (rgndata->nRectCount)
	{
		int i;

		if (!(rgndata->rects = calloc(rgndata->nRectCount, sizeof(RDP_RECT))))
		{
			WLog_ERR(TAG, "unable to allocate memory for %"PRIu32" RECTs", rgndata->nRectCount);
			return CHANNEL_RC_NO_MEMORY;
		}

		for (i = 0; i < rgndata->nRectCount; i++)
		{
			Stream_Read_INT32(s, rgndata->rects[i].x);
			Stream_Read_INT32(s, rgndata->rects[i].y);
			Stream_Read_INT32(s, right);
			Stream_Read_INT32(s, bottom);
			rgndata->rects[i].width = right - rgndata->rects[i].x;
			rgndata->rects[i].height = bottom - rgndata->rects[i].y;
		}
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT geometry_recv_pdu(GEOMETRY_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT32 length, cbGeometryBuffer;
	MAPPED_GEOMETRY_PACKET packet;
	GEOMETRY_PLUGIN* geometry;
	GeometryClientContext* context;
	UINT ret;
	geometry = (GEOMETRY_PLUGIN*) callback->plugin;
	context = (GeometryClientContext*)geometry->iface.pInterface;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "not enough remaining data");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, length); /* Length (4 bytes) */

	if (length < 73 || Stream_GetRemainingLength(s) < (length - 4))
	{
		WLog_ERR(TAG, "invalid packet length");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, packet.version);
	Stream_Read_UINT64(s, packet.mappingId);
	Stream_Read_UINT32(s, packet.updateType);
	Stream_Seek_UINT32(s); /* flags */
	Stream_Read_UINT64(s, packet.topLevelId);
	Stream_Read_INT32(s, packet.left);
	Stream_Read_INT32(s, packet.top);
	Stream_Read_INT32(s, packet.right);
	Stream_Read_INT32(s, packet.bottom);
	Stream_Read_INT32(s, packet.topLevelLeft);
	Stream_Read_INT32(s, packet.topLevelTop);
	Stream_Read_INT32(s, packet.topLevelRight);
	Stream_Read_INT32(s, packet.topLevelBottom);
	Stream_Read_UINT32(s, packet.geometryType);
	Stream_Read_UINT32(s, cbGeometryBuffer);

	if (Stream_GetRemainingLength(s) < cbGeometryBuffer)
	{
		WLog_ERR(TAG, "invalid packet length");
		return ERROR_INVALID_DATA;
	}

	ZeroMemory(&packet.geometry, sizeof(packet.geometry));

	if (cbGeometryBuffer)
	{
		ret = geometry_read_RGNDATA(s, cbGeometryBuffer, &packet.geometry);

		if (ret != CHANNEL_RC_OK)
			return ret;
	}
	else
		ret = CHANNEL_RC_OK;

	if (context->MappedGeometryPacket)
		ret = context->MappedGeometryPacket(context, &packet);

	free(packet.geometry.rects);
	return ret;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT geometry_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream* data)
{
	GEOMETRY_CHANNEL_CALLBACK* callback = (GEOMETRY_CHANNEL_CALLBACK*) pChannelCallback;
	return geometry_recv_pdu(callback, data);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT geometry_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	free(pChannelCallback);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT geometry_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
        IWTSVirtualChannel* pChannel, BYTE* Data, BOOL* pbAccept,
        IWTSVirtualChannelCallback** ppCallback)
{
	GEOMETRY_CHANNEL_CALLBACK* callback;
	GEOMETRY_LISTENER_CALLBACK* listener_callback = (GEOMETRY_LISTENER_CALLBACK*) pListenerCallback;
	callback = (GEOMETRY_CHANNEL_CALLBACK*) calloc(1, sizeof(GEOMETRY_CHANNEL_CALLBACK));

	if (!callback)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	callback->iface.OnDataReceived = geometry_on_data_received;
	callback->iface.OnClose = geometry_on_close;
	callback->plugin = listener_callback->plugin;
	callback->channel_mgr = listener_callback->channel_mgr;
	callback->channel = pChannel;
	listener_callback->channel_callback = callback;
	*ppCallback = (IWTSVirtualChannelCallback*) callback;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT geometry_plugin_initialize(IWTSPlugin* pPlugin, IWTSVirtualChannelManager* pChannelMgr)
{
	UINT status;
	GEOMETRY_PLUGIN* geometry = (GEOMETRY_PLUGIN*) pPlugin;
	geometry->listener_callback = (GEOMETRY_LISTENER_CALLBACK*) calloc(1,
	                              sizeof(GEOMETRY_LISTENER_CALLBACK));

	if (!geometry->listener_callback)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	geometry->listener_callback->iface.OnNewChannelConnection = geometry_on_new_channel_connection;
	geometry->listener_callback->plugin = pPlugin;
	geometry->listener_callback->channel_mgr = pChannelMgr;
	status = pChannelMgr->CreateListener(pChannelMgr, GEOMETRY_DVC_CHANNEL_NAME, 0,
	                                     (IWTSListenerCallback*) geometry->listener_callback, &(geometry->listener));
	geometry->listener->pInterface = geometry->iface.pInterface;
	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT geometry_plugin_terminated(IWTSPlugin* pPlugin)
{
	GEOMETRY_PLUGIN* geometry = (GEOMETRY_PLUGIN*) pPlugin;
	free(geometry->listener_callback);
	free(geometry->iface.pInterface);
	free(pPlugin);
	return CHANNEL_RC_OK;
}

/**
 * Channel Client Interface
 */


#ifdef BUILTIN_CHANNELS
#define DVCPluginEntry		geometry_DVCPluginEntry
#else
#define DVCPluginEntry		FREERDP_API DVCPluginEntry
#endif

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	UINT error = CHANNEL_RC_OK;
	GEOMETRY_PLUGIN* geometry;
	GeometryClientContext* context;
	geometry = (GEOMETRY_PLUGIN*) pEntryPoints->GetPlugin(pEntryPoints, "geometry");

	if (!geometry)
	{
		geometry = (GEOMETRY_PLUGIN*) calloc(1, sizeof(GEOMETRY_PLUGIN));

		if (!geometry)
		{
			WLog_ERR(TAG, "calloc failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		geometry->iface.Initialize = geometry_plugin_initialize;
		geometry->iface.Connected = NULL;
		geometry->iface.Disconnected = NULL;
		geometry->iface.Terminated = geometry_plugin_terminated;
		context = (GeometryClientContext*) calloc(1, sizeof(GeometryClientContext));

		if (!context)
		{
			WLog_ERR(TAG, "calloc failed!");
			free(geometry);
			return CHANNEL_RC_NO_MEMORY;
		}

		context->handle = (void*) geometry;
		geometry->iface.pInterface = (void*) context;
		geometry->context = context;
		error = pEntryPoints->RegisterPlugin(pEntryPoints, "geometry", (IWTSPlugin*) geometry);
	}
	else
	{
		WLog_ERR(TAG, "could not get geometry Plugin.");
		return CHANNEL_RC_BAD_CHANNEL;
	}

	return error;
}
