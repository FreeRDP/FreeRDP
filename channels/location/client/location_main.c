/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Location Virtual Channel Extension
 *
 * Copyright 2024 Armin Novak <anovak@thincast.com>
 * Copyright 2024 Thincast Technologies GmbH
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
#include <float.h>
#include <math.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/stream.h>

#include <freerdp/client/channels.h>
#include <freerdp/channels/log.h>
#include <freerdp/channels/location.h>
#include <freerdp/client/location.h>
#include <freerdp/utils/encoded_types.h>

#define TAG CHANNELS_TAG("location.client")

/* implement [MS-RDPEL]
 * https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-rdpel/4397a0af-c821-4b75-9068-476fb579c327
 */
typedef struct
{
	GENERIC_DYNVC_PLUGIN baseDynPlugin;
	LocationClientContext context;
} LOCATION_PLUGIN;

typedef struct
{
	GENERIC_CHANNEL_CALLBACK baseCb;
	UINT32 serverVersion;
	UINT32 clientVersion;
	UINT32 serverFlags;
	UINT32 clientFlags;
} LOCATION_CALLBACK;

static BOOL location_read_header(wLog* log, wStream* s, UINT16* ppduType, UINT32* ppduLength)
{
	WINPR_ASSERT(log);
	WINPR_ASSERT(s);
	WINPR_ASSERT(ppduType);
	WINPR_ASSERT(ppduLength);

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 6))
		return FALSE;
	Stream_Read_UINT16(s, *ppduType);
	Stream_Read_UINT32(s, *ppduLength);
	if (*ppduLength < 6)
	{
		WLog_Print(log, WLOG_ERROR,
		           "RDPLOCATION_HEADER::pduLengh=%" PRIu16 " < sizeof(RDPLOCATION_HEADER)[6]",
		           *ppduLength);
		return FALSE;
	}
	return Stream_CheckAndLogRequiredLengthWLog(log, s, *ppduLength - 6ull);
}

static BOOL location_write_header(wStream* s, UINT16 pduType, UINT32 pduLength)
{
	if (!Stream_EnsureRemainingCapacity(s, 6))
		return FALSE;
	Stream_Write_UINT16(s, pduType);
	Stream_Write_UINT32(s, pduLength + 6);
	return Stream_EnsureRemainingCapacity(s, pduLength);
}

static BOOL location_read_server_ready_pdu(LOCATION_CALLBACK* callback, wStream* s, UINT16 pduSize)
{
	if (pduSize < 6 + 4)
		return FALSE; // Short message

	Stream_Read_UINT32(s, callback->serverVersion);
	if (pduSize >= 6 + 4 + 4)
		Stream_Read_UINT32(s, callback->serverFlags);
	return TRUE;
}

static UINT location_channel_send(IWTSVirtualChannel* channel, wStream* s)
{
	const size_t len = Stream_GetPosition(s);
	if (len > UINT32_MAX)
		return ERROR_INTERNAL_ERROR;

	Stream_SetPosition(s, 2);
	Stream_Write_UINT32(s, (UINT32)len);

	WINPR_ASSERT(channel);
	WINPR_ASSERT(channel->Write);
	return channel->Write(channel, (UINT32)len, Stream_Buffer(s), NULL);
}

static UINT location_send_client_ready_pdu(const LOCATION_CALLBACK* callback)
{
	wStream sbuffer = { 0 };
	BYTE buffer[32] = { 0 };
	wStream* s = Stream_StaticInit(&sbuffer, buffer, sizeof(buffer));
	WINPR_ASSERT(s);

	if (!location_write_header(s, PDUTYPE_CLIENT_READY, 8))
		return ERROR_OUTOFMEMORY;

	Stream_Write_UINT32(s, callback->clientVersion);
	Stream_Write_UINT32(s, callback->clientFlags);
	return location_channel_send(callback->baseCb.channel, s);
}

static const char* location_version_str(UINT32 version, char* buffer, size_t size)
{
	const char* str = NULL;
	switch (version)
	{
		case RDPLOCATION_PROTOCOL_VERSION_100:
			str = "RDPLOCATION_PROTOCOL_VERSION_100";
			break;
		case RDPLOCATION_PROTOCOL_VERSION_200:
			str = "RDPLOCATION_PROTOCOL_VERSION_200";
			break;
		default:
			str = "RDPLOCATION_PROTOCOL_VERSION_UNKNOWN";
			break;
	}

	(void)_snprintf(buffer, size, "%s [0x%08" PRIx32 "]", str, version);
	return buffer;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT location_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream* data)
{
	LOCATION_CALLBACK* callback = (LOCATION_CALLBACK*)pChannelCallback;

	WINPR_ASSERT(callback);

	LOCATION_PLUGIN* plugin = (LOCATION_PLUGIN*)callback->baseCb.plugin;
	WINPR_ASSERT(plugin);

	UINT16 pduType = 0;
	UINT32 pduLength = 0;
	if (!location_read_header(plugin->baseDynPlugin.log, data, &pduType, &pduLength))
		return ERROR_INVALID_DATA;

	switch (pduType)
	{
		case PDUTYPE_SERVER_READY:
			if (!location_read_server_ready_pdu(callback, data, pduLength))
				return ERROR_INVALID_DATA;

			switch (callback->serverVersion)
			{
				case RDPLOCATION_PROTOCOL_VERSION_200:
					callback->clientVersion = RDPLOCATION_PROTOCOL_VERSION_200;
					break;
				case RDPLOCATION_PROTOCOL_VERSION_100:
					callback->clientVersion = RDPLOCATION_PROTOCOL_VERSION_100;
					break;
				default:
					callback->clientVersion = RDPLOCATION_PROTOCOL_VERSION_100;
					if (callback->serverVersion > RDPLOCATION_PROTOCOL_VERSION_200)
						callback->clientVersion = RDPLOCATION_PROTOCOL_VERSION_200;
					break;
			}

			char cbuffer[64] = { 0 };
			char sbuffer[64] = { 0 };
			WLog_Print(plugin->baseDynPlugin.log, WLOG_DEBUG,
			           "Server version %s, client version %s",
			           location_version_str(callback->serverVersion, sbuffer, sizeof(sbuffer)),
			           location_version_str(callback->clientVersion, cbuffer, sizeof(cbuffer)));

			if (!plugin->context.LocationStart)
			{
				WLog_Print(plugin->baseDynPlugin.log, WLOG_WARN,
				           "LocationStart=NULL, no location data will be sent");
				return CHANNEL_RC_OK;
			}
			const UINT res =
			    plugin->context.LocationStart(&plugin->context, callback->clientVersion, 0);
			if (res != CHANNEL_RC_OK)
				return res;
			return location_send_client_ready_pdu(callback);
		default:
			WLog_WARN(TAG, "invalid pduType=%s");
			return ERROR_INVALID_DATA;
	}
}

static UINT location_send_base_location3d(IWTSVirtualChannel* channel,
                                          const RDPLOCATION_BASE_LOCATION3D_PDU* pdu)
{
	wStream sbuffer = { 0 };
	BYTE buffer[32] = { 0 };
	wStream* s = Stream_StaticInit(&sbuffer, buffer, sizeof(buffer));
	WINPR_ASSERT(s);
	WINPR_ASSERT(channel);
	WINPR_ASSERT(pdu);

	if (pdu->source)
		WLog_DBG(TAG,
		         "latitude=%lf, logitude=%lf, altitude=%" PRId32
		         ", speed=%lf, heading=%lf, haccuracy=%lf, source=%" PRIu8,
		         pdu->latitude, pdu->longitude, pdu->altitude, pdu->speed, pdu->heading,
		         pdu->horizontalAccuracy, *pdu->source);
	else
		WLog_DBG(TAG, "latitude=%lf, logitude=%lf, altitude=%" PRId32, pdu->latitude,
		         pdu->longitude, pdu->altitude);

	if (!location_write_header(s, PDUTYPE_BASE_LOCATION3D, pdu->source ? 25 : 12))
		return ERROR_OUTOFMEMORY;

	if (!freerdp_write_four_byte_float(s, pdu->latitude) ||
	    !freerdp_write_four_byte_float(s, pdu->longitude) ||
	    !freerdp_write_four_byte_signed_integer(s, pdu->altitude))
		return ERROR_INTERNAL_ERROR;

	if (pdu->source)
	{
		if (!freerdp_write_four_byte_float(s, *pdu->speed) ||
		    !freerdp_write_four_byte_float(s, *pdu->heading) ||
		    !freerdp_write_four_byte_float(s, *pdu->horizontalAccuracy))
			return ERROR_INTERNAL_ERROR;

		Stream_Write_UINT8(s, *pdu->source);
	}

	return location_channel_send(channel, s);
}

static UINT location_send_location2d_delta(IWTSVirtualChannel* channel,
                                           const RDPLOCATION_LOCATION2D_DELTA_PDU* pdu)
{
	wStream sbuffer = { 0 };
	BYTE buffer[32] = { 0 };
	wStream* s = Stream_StaticInit(&sbuffer, buffer, sizeof(buffer));
	WINPR_ASSERT(s);

	WINPR_ASSERT(channel);
	WINPR_ASSERT(pdu);

	const BOOL ext = pdu->speedDelta && pdu->headingDelta;

	if (ext)
		WLog_DBG(TAG, "latitude=%lf, logitude=%lf, speed=%lf, heading=%lf", pdu->latitudeDelta,
		         pdu->longitudeDelta, pdu->speedDelta, pdu->headingDelta);
	else
		WLog_DBG(TAG, "latitude=%lf, logitude=%lf", pdu->latitudeDelta, pdu->longitudeDelta);

	if (!location_write_header(s, PDUTYPE_LOCATION2D_DELTA, ext ? 16 : 8))
		return ERROR_OUTOFMEMORY;

	if (!freerdp_write_four_byte_float(s, pdu->latitudeDelta) ||
	    !freerdp_write_four_byte_float(s, pdu->longitudeDelta))
		return ERROR_INTERNAL_ERROR;

	if (ext)
	{
		if (!freerdp_write_four_byte_float(s, *pdu->speedDelta) ||
		    !freerdp_write_four_byte_float(s, *pdu->headingDelta))
			return ERROR_INTERNAL_ERROR;
	}

	return location_channel_send(channel, s);
}

static UINT location_send_location3d_delta(IWTSVirtualChannel* channel,
                                           const RDPLOCATION_LOCATION3D_DELTA_PDU* pdu)
{
	wStream sbuffer = { 0 };
	BYTE buffer[32] = { 0 };
	wStream* s = Stream_StaticInit(&sbuffer, buffer, sizeof(buffer));
	WINPR_ASSERT(s);

	WINPR_ASSERT(channel);
	WINPR_ASSERT(pdu);

	const BOOL ext = pdu->speedDelta && pdu->headingDelta;

	if (ext)
		WLog_DBG(TAG, "latitude=%lf, logitude=%lf, altitude=%" PRId32 ", speed=%lf, heading=%lf",
		         pdu->latitudeDelta, pdu->longitudeDelta, pdu->altitudeDelta, pdu->speedDelta,
		         pdu->headingDelta);
	else
		WLog_DBG(TAG, "latitude=%lf, logitude=%lf, altitude=%" PRId32, pdu->latitudeDelta,
		         pdu->longitudeDelta, pdu->altitudeDelta);

	if (!location_write_header(s, PDUTYPE_LOCATION3D_DELTA, ext ? 20 : 12))
		return ERROR_OUTOFMEMORY;

	if (!freerdp_write_four_byte_float(s, pdu->latitudeDelta) ||
	    !freerdp_write_four_byte_float(s, pdu->longitudeDelta) ||
	    !freerdp_write_four_byte_signed_integer(s, pdu->altitudeDelta))
		return ERROR_INTERNAL_ERROR;

	if (ext)
	{
		if (!freerdp_write_four_byte_float(s, *pdu->speedDelta) ||
		    !freerdp_write_four_byte_float(s, *pdu->headingDelta))
			return ERROR_INTERNAL_ERROR;
	}

	return location_channel_send(channel, s);
}

static UINT location_send(LocationClientContext* context, LOCATION_PDUTYPE type, size_t count, ...)
{
	WINPR_ASSERT(context);

	LOCATION_PLUGIN* loc = context->handle;
	WINPR_ASSERT(loc);

	GENERIC_LISTENER_CALLBACK* cb = loc->baseDynPlugin.listener_callback;
	WINPR_ASSERT(cb);

	IWTSVirtualChannel* channel = cb->channel;
	WINPR_ASSERT(channel);

	const LOCATION_CALLBACK* callback =
	    (const LOCATION_CALLBACK*)loc->baseDynPlugin.channel_callbacks;
	WINPR_ASSERT(callback);

	UINT32 res = ERROR_INTERNAL_ERROR;
	va_list ap = { 0 };
	va_start(ap, count);
	switch (type)
	{
		case PDUTYPE_BASE_LOCATION3D:
			if ((count != 3) && (count != 7))
				res = ERROR_INVALID_PARAMETER;
			else
			{
				RDPLOCATION_BASE_LOCATION3D_PDU pdu = { 0 };
				LOCATIONSOURCE source = LOCATIONSOURCE_IP;
				double speed = FP_NAN;
				double heading = FP_NAN;
				double horizontalAccuracy = FP_NAN;
				pdu.latitude = va_arg(ap, double);
				pdu.longitude = va_arg(ap, double);
				pdu.altitude = va_arg(ap, INT32);
				if ((count > 3) && (callback->clientVersion >= RDPLOCATION_PROTOCOL_VERSION_200))
				{
					speed = va_arg(ap, double);
					heading = va_arg(ap, double);
					horizontalAccuracy = va_arg(ap, double);
					source = va_arg(ap, int);
					pdu.speed = &speed;
					pdu.heading = &heading;
					pdu.horizontalAccuracy = &horizontalAccuracy;
					pdu.source = &source;
				}
				res = location_send_base_location3d(channel, &pdu);
			}
			break;
		case PDUTYPE_LOCATION2D_DELTA:
			if ((count != 2) && (count != 4))
				res = ERROR_INVALID_PARAMETER;
			else
			{
				RDPLOCATION_LOCATION2D_DELTA_PDU pdu = { 0 };

				pdu.latitudeDelta = va_arg(ap, double);
				pdu.longitudeDelta = va_arg(ap, double);

				double speedDelta = FP_NAN;
				double headingDelta = FP_NAN;
				if ((count > 2) && (callback->clientVersion >= RDPLOCATION_PROTOCOL_VERSION_200))
				{
					speedDelta = va_arg(ap, double);
					headingDelta = va_arg(ap, double);
					pdu.speedDelta = &speedDelta;
					pdu.headingDelta = &headingDelta;
				}
				res = location_send_location2d_delta(channel, &pdu);
			}
			break;
		case PDUTYPE_LOCATION3D_DELTA:
			if ((count != 3) && (count != 5))
				res = ERROR_INVALID_PARAMETER;
			else
			{
				RDPLOCATION_LOCATION3D_DELTA_PDU pdu = { 0 };
				double speedDelta = FP_NAN;
				double headingDelta = FP_NAN;

				pdu.latitudeDelta = va_arg(ap, double);
				pdu.longitudeDelta = va_arg(ap, double);
				pdu.altitudeDelta = va_arg(ap, INT32);
				if ((count > 3) && (callback->clientVersion >= RDPLOCATION_PROTOCOL_VERSION_200))
				{
					speedDelta = va_arg(ap, double);
					headingDelta = va_arg(ap, double);
					pdu.speedDelta = &speedDelta;
					pdu.headingDelta = &headingDelta;
				}
				res = location_send_location3d_delta(channel, &pdu);
			}
			break;
		default:
			res = ERROR_INVALID_PARAMETER;
			break;
	}
	va_end(ap);
	return res;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT location_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	UINT res = CHANNEL_RC_OK;
	GENERIC_CHANNEL_CALLBACK* callback = (GENERIC_CHANNEL_CALLBACK*)pChannelCallback;

	if (callback)
	{
		LOCATION_PLUGIN* plugin = (LOCATION_PLUGIN*)callback->plugin;
		WINPR_ASSERT(plugin);

		res = IFCALLRESULT(CHANNEL_RC_OK, plugin->context.LocationStop, &plugin->context);
	}
	free(callback);

	return res;
}

static UINT location_init(GENERIC_DYNVC_PLUGIN* plugin, rdpContext* context, rdpSettings* settings)
{
	LOCATION_PLUGIN* loc = (LOCATION_PLUGIN*)plugin;

	WINPR_ASSERT(loc);

	loc->context.LocationSend = location_send;
	loc->context.handle = loc;
	plugin->iface.pInterface = &loc->context;
	return CHANNEL_RC_OK;
}

static const IWTSVirtualChannelCallback location_callbacks = { location_on_data_received,
	                                                           NULL, /* Open */
	                                                           location_on_close, NULL };

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
FREERDP_ENTRY_POINT(UINT VCAPITYPE location_DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints))
{
	return freerdp_generic_DVCPluginEntry(pEntryPoints, TAG, LOCATION_DVC_CHANNEL_NAME,
	                                      sizeof(LOCATION_PLUGIN), sizeof(LOCATION_CALLBACK),
	                                      &location_callbacks, location_init, NULL);
}
