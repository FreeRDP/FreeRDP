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
	WINPR_ASSERT(rgndata);

	if (len < 32)
	{
		WLog_Print(logger, WLOG_ERROR, "invalid RGNDATA");
		return ERROR_INVALID_DATA;
	}

	const UINT32 dwSize = Stream_Get_UINT32(s);

	if (dwSize != 32)
	{
		WLog_Print(logger, WLOG_ERROR, "invalid RGNDATA dwSize");
		return ERROR_INVALID_DATA;
	}

	const UINT32 iType = Stream_Get_UINT32(s);

	if (iType != RDH_RECTANGLE)
	{
		WLog_Print(logger, WLOG_ERROR, "iType %" PRIu32 " for RGNDATA is not supported", iType);
		return ERROR_UNSUPPORTED_TYPE;
	}

	rgndata->nRectCount = Stream_Get_UINT32(s);
	Stream_Seek_UINT32(s); /* nRgnSize IGNORED */
	{
		const INT32 x = Stream_Get_INT32(s);
		const INT32 y = Stream_Get_INT32(s);
		const INT32 right = Stream_Get_INT32(s);
		const INT32 bottom = Stream_Get_INT32(s);
		if ((abs(x) > INT16_MAX) || (abs(y) > INT16_MAX))
			return ERROR_INVALID_DATA;
		const INT32 w = right - x;
		const INT32 h = bottom - y;
		if ((abs(w) > INT16_MAX) || (abs(h) > INT16_MAX))
			return ERROR_INVALID_DATA;
		rgndata->boundingRect.x = (INT16)x;
		rgndata->boundingRect.y = (INT16)y;
		rgndata->boundingRect.width = (INT16)w;
		rgndata->boundingRect.height = (INT16)h;
	}
	len -= 32;

	if (len / (4 * 4) < rgndata->nRectCount)
	{
		WLog_Print(logger, WLOG_ERROR, "not enough data for region rectangles");
		return ERROR_INVALID_DATA;
	}

	if (rgndata->nRectCount)
	{
		RDP_RECT* tmp = realloc(rgndata->rects, rgndata->nRectCount * sizeof(RDP_RECT));

		if (!tmp)
		{
			WLog_Print(logger, WLOG_ERROR, "unable to allocate memory for %" PRIu32 " RECTs",
			           rgndata->nRectCount);
			return CHANNEL_RC_NO_MEMORY;
		}
		rgndata->rects = tmp;

		for (UINT32 i = 0; i < rgndata->nRectCount; i++)
		{
			RDP_RECT* rect = &rgndata->rects[i];

			if (!Stream_CheckAndLogRequiredLengthWLog(logger, s, 16))
				return CHANNEL_RC_NULL_DATA;

			const INT32 x = Stream_Get_INT32(s);
			const INT32 y = Stream_Get_INT32(s);
			const INT32 right = Stream_Get_INT32(s);
			const INT32 bottom = Stream_Get_INT32(s);
			if ((abs(x) > INT16_MAX) || (abs(y) > INT16_MAX))
				return ERROR_INVALID_DATA;

			const INT32 w = right - x;
			const INT32 h = bottom - y;
			if ((abs(w) > INT16_MAX) || (abs(h) > INT16_MAX))
				return ERROR_INVALID_DATA;

			rect->x = (INT16)x;
			rect->y = (INT16)y;
			rect->width = (INT16)w;
			rect->height = (INT16)h;
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
	UINT ret = CHANNEL_RC_OK;

	WINPR_ASSERT(callback);
	GEOMETRY_PLUGIN* geometry = (GEOMETRY_PLUGIN*)callback->plugin;
	WINPR_ASSERT(geometry);

	wLog* logger = geometry->base.log;
	GeometryClientContext* context = (GeometryClientContext*)geometry->base.iface.pInterface;
	WINPR_ASSERT(context);

	if (!Stream_CheckAndLogRequiredLengthWLog(logger, s, 4))
		return ERROR_INVALID_DATA;

	const UINT32 length = Stream_Get_UINT32(s); /* Length (4 bytes) */

	if (!Stream_CheckAndLogRequiredLengthWLog(logger, s, (length - 4)))
	{
		WLog_Print(logger, WLOG_ERROR, "invalid packet length");
		return ERROR_INVALID_DATA;
	}

	if (!Stream_CheckAndLogRequiredLengthWLog(logger, s, 20))
		return ERROR_INVALID_DATA;

	context->remoteVersion = Stream_Get_UINT32(s);
	const UINT64 id = Stream_Get_UINT64(s);
	const UINT32 updateType = Stream_Get_UINT32(s);
	Stream_Seek_UINT32(s); /* flags */

	MAPPED_GEOMETRY* mappedGeometry = HashTable_GetItemValue(context->geometries, &id);

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

		if (!Stream_CheckAndLogRequiredLengthWLog(logger, s, 48))
		{
			// NOLINTNEXTLINE(clang-analyzer-unix.Malloc): HashTable_Insert ownership mappedGeometry
			return ERROR_INVALID_DATA;
		}

		mappedGeometry->topLevelId = Stream_Get_UINT64(s);

		mappedGeometry->left = Stream_Get_INT32(s);
		mappedGeometry->top = Stream_Get_INT32(s);
		mappedGeometry->right = Stream_Get_INT32(s);
		mappedGeometry->bottom = Stream_Get_INT32(s);

		mappedGeometry->topLevelLeft = Stream_Get_INT32(s);
		mappedGeometry->topLevelTop = Stream_Get_INT32(s);
		mappedGeometry->topLevelRight = Stream_Get_INT32(s);
		mappedGeometry->topLevelBottom = Stream_Get_INT32(s);

		const UINT32 geometryType = Stream_Get_UINT32(s);
		if (geometryType != 0x02)
			WLog_Print(logger, WLOG_DEBUG, "geometryType should be set to 0x02 and is 0x%" PRIx32,
			           geometryType);

		const UINT32 cbGeometryBuffer = Stream_Get_UINT32(s);
		if (!Stream_CheckAndLogRequiredLengthWLog(logger, s, cbGeometryBuffer))
		{
			// NOLINTNEXTLINE(clang-analyzer-unix.Malloc): HashTable_Insert ownership mappedGeometry
			return ERROR_INVALID_DATA;
		}

		if (cbGeometryBuffer > 0)
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
	                                                           geometry_on_close, NULL };

static UINT init_plugin_cb(GENERIC_DYNVC_PLUGIN* base, WINPR_ATTR_UNUSED rdpContext* rcontext,
                           rdpSettings* settings)
{
	GeometryClientContext* context = NULL;
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
FREERDP_ENTRY_POINT(UINT VCAPITYPE geometry_DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints))
{
	return freerdp_generic_DVCPluginEntry(pEntryPoints, TAG, GEOMETRY_DVC_CHANNEL_NAME,
	                                      sizeof(GEOMETRY_PLUGIN), sizeof(GENERIC_CHANNEL_CALLBACK),
	                                      &geometry_callbacks, init_plugin_cb, terminate_plugin_cb);
}
