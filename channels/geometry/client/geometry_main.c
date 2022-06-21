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

#include <freerdp/config.h>

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
#include <freerdp/client/channels.h>
#include <freerdp/client/geometry.h>
#include <freerdp/channels/log.h>

#define TAG CHANNELS_TAG("geometry.client")

#include "geometry_main.h"

typedef struct
{
	GENERIC_DYNVC_PLUGIN base;
	GeometryClientContext* context;
} GEOMETRY_PLUGIN;

static UINT32 mappedGeometryHash(const void* v)
{
	const UINT64* g = (const UINT64*)v;
	return (UINT32)((*g >> 32) + (*g & 0xffffffff));
}

static BOOL mappedGeometryKeyCompare(const void* v1, const void* v2)
{
	const UINT64* g1 = (const UINT64*)v1;
	const UINT64* g2 = (const UINT64*)v2;
	return *g1 == *g2;
}

static void freerdp_rgndata_reset(FREERDP_RGNDATA* data)
{
	data->nRectCount = 0;
}

static UINT32 geometry_read_RGNDATA(wLog* logger, wStream* s, UINT32 len, FREERDP_RGNDATA* rgndata)
{
	UINT32 dwSize, iType;
	INT32 right, bottom;
	INT32 x, y, w, h;

	if (len < 32)
	{
		WLog_Print(logger, WLOG_ERROR, "invalid RGNDATA");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, dwSize);

	if (dwSize != 32)
	{
		WLog_Print(logger, WLOG_ERROR, "invalid RGNDATA dwSize");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, iType);

	if (iType != RDH_RECTANGLE)
	{
		WLog_Print(logger, WLOG_ERROR, "iType %" PRIu32 " for RGNDATA is not supported", iType);
		return ERROR_UNSUPPORTED_TYPE;
	}

	Stream_Read_UINT32(s, rgndata->nRectCount);
	Stream_Seek_UINT32(s); /* nRgnSize IGNORED */
	Stream_Read_INT32(s, x);
	Stream_Read_INT32(s, y);
	Stream_Read_INT32(s, right);
	Stream_Read_INT32(s, bottom);
	if ((abs(x) > INT16_MAX) || (abs(y) > INT16_MAX))
		return ERROR_INVALID_DATA;
	w = right - x;
	h = bottom - y;
	if ((abs(w) > INT16_MAX) || (abs(h) > INT16_MAX))
		return ERROR_INVALID_DATA;
	rgndata->boundingRect.x = (INT16)x;
	rgndata->boundingRect.y = (INT16)y;
	rgndata->boundingRect.width = (INT16)w;
	rgndata->boundingRect.height = (INT16)h;
	len -= 32;

	if (len / (4 * 4) < rgndata->nRectCount)
	{
		WLog_Print(logger, WLOG_ERROR, "not enough data for region rectangles");
	}

	if (rgndata->nRectCount)
	{
		UINT32 i;
		RDP_RECT* tmp = realloc(rgndata->rects, rgndata->nRectCount * sizeof(RDP_RECT));

		if (!tmp)
		{
			WLog_Print(logger, WLOG_ERROR, "unable to allocate memory for %" PRIu32 " RECTs",
			           rgndata->nRectCount);
			return CHANNEL_RC_NO_MEMORY;
		}
		rgndata->rects = tmp;

		for (i = 0; i < rgndata->nRectCount; i++)
		{
			Stream_Read_INT32(s, x);
			Stream_Read_INT32(s, y);
			Stream_Read_INT32(s, right);
			Stream_Read_INT32(s, bottom);
			if ((abs(x) > INT16_MAX) || (abs(y) > INT16_MAX))
				return ERROR_INVALID_DATA;
			w = right - x;
			h = bottom - y;
			if ((abs(w) > INT16_MAX) || (abs(h) > INT16_MAX))
				return ERROR_INVALID_DATA;
			rgndata->rects[i].x = (INT16)x;
			rgndata->rects[i].y = (INT16)y;
			rgndata->rects[i].width = (INT16)w;
			rgndata->rects[i].height = (INT16)h;
		}
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT geometry_recv_pdu(GENERIC_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT32 length, cbGeometryBuffer;
	MAPPED_GEOMETRY* mappedGeometry;
	GEOMETRY_PLUGIN* geometry;
	GeometryClientContext* context;
	UINT ret = CHANNEL_RC_OK;
	UINT32 updateType, geometryType;
	UINT64 id;
	wLog* logger;

	geometry = (GEOMETRY_PLUGIN*)callback->plugin;
	logger = geometry->base.log;
	context = (GeometryClientContext*)geometry->base.iface.pInterface;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, length); /* Length (4 bytes) */

	if (length < 73 || !Stream_CheckAndLogRequiredLength(TAG, s, (length - 4)))
	{
		WLog_Print(logger, WLOG_ERROR, "invalid packet length");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, context->remoteVersion);
	Stream_Read_UINT64(s, id);
	Stream_Read_UINT32(s, updateType);
	Stream_Seek_UINT32(s); /* flags */

	mappedGeometry = HashTable_GetItemValue(context->geometries, &id);

	if (updateType == GEOMETRY_CLEAR)
	{
		if (!mappedGeometry)
		{
			WLog_Print(logger, WLOG_ERROR,
			           "geometry 0x%" PRIx64 " not found here, ignoring clear command", id);
			return CHANNEL_RC_OK;
		}

		WLog_Print(logger, WLOG_DEBUG, "clearing geometry 0x%" PRIx64 "", id);

		if (mappedGeometry->MappedGeometryClear &&
		    !mappedGeometry->MappedGeometryClear(mappedGeometry))
			return ERROR_INTERNAL_ERROR;

		if (!HashTable_Remove(context->geometries, &id))
			WLog_Print(logger, WLOG_ERROR, "geometry not removed from geometries");
	}
	else if (updateType == GEOMETRY_UPDATE)
	{
		BOOL newOne = FALSE;

		if (!mappedGeometry)
		{
			newOne = TRUE;
			WLog_Print(logger, WLOG_DEBUG, "creating geometry 0x%" PRIx64 "", id);
			mappedGeometry = calloc(1, sizeof(MAPPED_GEOMETRY));
			if (!mappedGeometry)
				return CHANNEL_RC_NO_MEMORY;

			mappedGeometry->refCounter = 1;
			mappedGeometry->mappingId = id;

			if (!HashTable_Insert(context->geometries, &(mappedGeometry->mappingId),
			                      mappedGeometry))
			{
				WLog_Print(logger, WLOG_ERROR,
				           "unable to register geometry 0x%" PRIx64 " in the table", id);
				free(mappedGeometry);
				return CHANNEL_RC_NO_MEMORY;
			}
		}
		else
		{
			WLog_Print(logger, WLOG_DEBUG, "updating geometry 0x%" PRIx64 "", id);
		}

		Stream_Read_UINT64(s, mappedGeometry->topLevelId);

		Stream_Read_INT32(s, mappedGeometry->left);
		Stream_Read_INT32(s, mappedGeometry->top);
		Stream_Read_INT32(s, mappedGeometry->right);
		Stream_Read_INT32(s, mappedGeometry->bottom);

		Stream_Read_INT32(s, mappedGeometry->topLevelLeft);
		Stream_Read_INT32(s, mappedGeometry->topLevelTop);
		Stream_Read_INT32(s, mappedGeometry->topLevelRight);
		Stream_Read_INT32(s, mappedGeometry->topLevelBottom);

		Stream_Read_UINT32(s, geometryType);
		if (geometryType != 0x02)
			WLog_Print(logger, WLOG_DEBUG, "geometryType should be set to 0x02 and is 0x%" PRIx32,
			           geometryType);

		Stream_Read_UINT32(s, cbGeometryBuffer);
		if (!Stream_CheckAndLogRequiredLength(TAG, s, cbGeometryBuffer))
			return ERROR_INVALID_DATA;

		if (cbGeometryBuffer)
		{
			ret = geometry_read_RGNDATA(logger, s, cbGeometryBuffer, &mappedGeometry->geometry);
			if (ret != CHANNEL_RC_OK)
				return ret;
		}
		else
		{
			freerdp_rgndata_reset(&mappedGeometry->geometry);
		}

		if (newOne)
		{
			if (context->MappedGeometryAdded &&
			    !context->MappedGeometryAdded(context, mappedGeometry))
			{
				WLog_Print(logger, WLOG_ERROR, "geometry added callback failed");
				ret = ERROR_INTERNAL_ERROR;
			}
		}
		else
		{
			if (mappedGeometry->MappedGeometryUpdate &&
			    !mappedGeometry->MappedGeometryUpdate(mappedGeometry))
			{
				WLog_Print(logger, WLOG_ERROR, "geometry update callback failed");
				ret = ERROR_INTERNAL_ERROR;
			}
		}
	}
	else
	{
		WLog_Print(logger, WLOG_ERROR, "unknown updateType=%" PRIu32 "", updateType);
		ret = CHANNEL_RC_OK;
	}

	return ret;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT geometry_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream* data)
{
	GENERIC_CHANNEL_CALLBACK* callback = (GENERIC_CHANNEL_CALLBACK*)pChannelCallback;
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

static void mappedGeometryUnref_void(void* arg)
{
	MAPPED_GEOMETRY* g = (MAPPED_GEOMETRY*)arg;
	mappedGeometryUnref(g);
}

/**
 * Channel Client Interface
 */

static const IWTSVirtualChannelCallback geometry_callbacks = { geometry_on_data_received,
	                                                           NULL, /* Open */
	                                                           geometry_on_close };

static UINT init_plugin_cb(GENERIC_DYNVC_PLUGIN* base, rdpContext* rcontext, rdpSettings* settings)
{
	GeometryClientContext* context;
	GEOMETRY_PLUGIN* geometry = (GEOMETRY_PLUGIN*)base;

	WINPR_ASSERT(base);
	WINPR_UNUSED(settings);

	context = (GeometryClientContext*)calloc(1, sizeof(GeometryClientContext));
	if (!context)
	{
		WLog_Print(base->log, WLOG_ERROR, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	context->geometries = HashTable_New(FALSE);
	if (!context->geometries)
	{
		WLog_Print(base->log, WLOG_ERROR, "unable to allocate geometries");
		free(context);
		return CHANNEL_RC_NO_MEMORY;
	}

	HashTable_SetHashFunction(context->geometries, mappedGeometryHash);
	{
		wObject* obj = HashTable_KeyObject(context->geometries);
		obj->fnObjectEquals = mappedGeometryKeyCompare;
	}
	{
		wObject* obj = HashTable_ValueObject(context->geometries);
		obj->fnObjectFree = mappedGeometryUnref_void;
	}
	context->handle = (void*)geometry;

	geometry->context = context;
	geometry->base.iface.pInterface = (void*)context;

	return CHANNEL_RC_OK;
}

static void terminate_plugin_cb(GENERIC_DYNVC_PLUGIN* base)
{
	GEOMETRY_PLUGIN* geometry = (GEOMETRY_PLUGIN*)base;

	if (geometry->context)
		HashTable_Free(geometry->context->geometries);
	free(geometry->context);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT geometry_DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	return freerdp_generic_DVCPluginEntry(pEntryPoints, TAG, GEOMETRY_DVC_CHANNEL_NAME,
	                                      sizeof(GEOMETRY_PLUGIN), sizeof(GENERIC_CHANNEL_CALLBACK),
	                                      &geometry_callbacks, init_plugin_cb, terminate_plugin_cb);
}
