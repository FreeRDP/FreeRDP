/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Multiparty Virtual Channel
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#include <winpr/crt.h>
#include <winpr/print.h>

#include <freerdp/channels/log.h>
#include <freerdp/client/encomsp.h>

#include "encomsp_main.h"

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT encomsp_read_header(wStream* s, ENCOMSP_ORDER_HEADER* header)
{
	if (Stream_GetRemainingLength(s) < ENCOMSP_ORDER_HEADER_SIZE)
	{
		WLog_ERR(TAG, "Not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, header->Type); /* Type (2 bytes) */
	Stream_Read_UINT16(s, header->Length); /* Length (2 bytes) */
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT encomsp_write_header(wStream* s, ENCOMSP_ORDER_HEADER* header)
{
	Stream_Write_UINT16(s, header->Type); /* Type (2 bytes) */
	Stream_Write_UINT16(s, header->Length); /* Length (2 bytes) */
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT encomsp_read_unicode_string(wStream* s, ENCOMSP_UNICODE_STRING* str)
{
	ZeroMemory(str, sizeof(ENCOMSP_UNICODE_STRING));

	if (Stream_GetRemainingLength(s) < 2)
	{
		WLog_ERR(TAG, "Not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, str->cchString); /* cchString (2 bytes) */

	if (str->cchString > 1024)
	{
		WLog_ERR(TAG, "cchString was %"PRIu16" but has to be < 1025!", str->cchString);
		return ERROR_INVALID_DATA;
	}

	if (Stream_GetRemainingLength(s) < (size_t)(str->cchString * 2))
	{
		WLog_ERR(TAG, "Not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read(s, &(str->wString), (str->cchString * 2)); /* String (variable) */
	return CHANNEL_RC_OK;
}

static EncomspClientContext* encomsp_get_client_interface(
    encomspPlugin* encomsp)
{
	EncomspClientContext* pInterface;
	pInterface = (EncomspClientContext*) encomsp->channelEntryPoints.pInterface;
	return pInterface;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT encomsp_virtual_channel_write(encomspPlugin* encomsp, wStream* s)
{
	UINT status;

	if (!encomsp)
		return ERROR_INVALID_HANDLE;

#if 0
	WLog_INFO(TAG, "EncomspWrite (%"PRIuz")", Stream_Length(s));
	winpr_HexDump(Stream_Buffer(s), Stream_Length(s));
#endif
	status = encomsp->channelEntryPoints.pVirtualChannelWriteEx(encomsp->InitHandle,
	         encomsp->OpenHandle,
	         Stream_Buffer(s), (UINT32) Stream_Length(s), s);

	if (status != CHANNEL_RC_OK)
		WLog_ERR(TAG,  "VirtualChannelWriteEx failed with %s [%08"PRIX32"]",
		         WTSErrorToString(status), status);

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT encomsp_recv_filter_updated_pdu(encomspPlugin* encomsp, wStream* s,
        ENCOMSP_ORDER_HEADER* header)
{
	int beg, end;
	EncomspClientContext* context;
	ENCOMSP_FILTER_UPDATED_PDU pdu;
	UINT error = CHANNEL_RC_OK;
	context = encomsp_get_client_interface(encomsp);

	if (!context)
		return ERROR_INVALID_HANDLE;

	beg = ((int) Stream_GetPosition(s)) - ENCOMSP_ORDER_HEADER_SIZE;
	CopyMemory(&pdu, header, sizeof(ENCOMSP_ORDER_HEADER));

	if (Stream_GetRemainingLength(s) < 1)
	{
		WLog_ERR(TAG, "Not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT8(s, pdu.Flags); /* Flags (1 byte) */
	end = (int) Stream_GetPosition(s);

	if ((beg + header->Length) < end)
	{
		WLog_ERR(TAG, "Not enough data!");
		return ERROR_INVALID_DATA;
	}

	if ((beg + header->Length) > end)
	{
		if (Stream_GetRemainingLength(s) < (size_t)((beg + header->Length) - end))
		{
			WLog_ERR(TAG, "Not enough data!");
			return ERROR_INVALID_DATA;
		}

		Stream_SetPosition(s, (beg + header->Length));
	}

	IFCALLRET(context->FilterUpdated, error, context, &pdu);

	if (error)
		WLog_ERR(TAG, "context->FilterUpdated failed with error %"PRIu32"", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT encomsp_recv_application_created_pdu(encomspPlugin* encomsp,
        wStream* s, ENCOMSP_ORDER_HEADER* header)
{
	int beg, end;
	EncomspClientContext* context;
	ENCOMSP_APPLICATION_CREATED_PDU pdu;
	UINT error;
	context = encomsp_get_client_interface(encomsp);

	if (!context)
		return ERROR_INVALID_HANDLE;

	beg = ((int) Stream_GetPosition(s)) - ENCOMSP_ORDER_HEADER_SIZE;
	CopyMemory(&pdu, header, sizeof(ENCOMSP_ORDER_HEADER));

	if (Stream_GetRemainingLength(s) < 6)
	{
		WLog_ERR(TAG, "Not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.Flags); /* Flags (2 bytes) */
	Stream_Read_UINT32(s, pdu.AppId); /* AppId (4 bytes) */

	if ((error = encomsp_read_unicode_string(s, &(pdu.Name))))
	{
		WLog_ERR(TAG, "encomsp_read_unicode_string failed with error %"PRIu32"", error);
		return error;
	}

	end = (int) Stream_GetPosition(s);

	if ((beg + header->Length) < end)
	{
		WLog_ERR(TAG, "Not enough data!");
		return ERROR_INVALID_DATA;
	}

	if ((beg + header->Length) > end)
	{
		if (Stream_GetRemainingLength(s) < (size_t)((beg + header->Length) - end))
		{
			WLog_ERR(TAG, "Not enough data!");
			return ERROR_INVALID_DATA;
		}

		Stream_SetPosition(s, (beg + header->Length));
	}

	IFCALLRET(context->ApplicationCreated, error, context, &pdu);

	if (error)
		WLog_ERR(TAG, "context->ApplicationCreated failed with error %"PRIu32"", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT encomsp_recv_application_removed_pdu(encomspPlugin* encomsp,
        wStream* s, ENCOMSP_ORDER_HEADER* header)
{
	int beg, end;
	EncomspClientContext* context;
	ENCOMSP_APPLICATION_REMOVED_PDU pdu;
	UINT error = CHANNEL_RC_OK;
	context = encomsp_get_client_interface(encomsp);

	if (!context)
		return ERROR_INVALID_HANDLE;

	beg = ((int) Stream_GetPosition(s)) - ENCOMSP_ORDER_HEADER_SIZE;
	CopyMemory(&pdu, header, sizeof(ENCOMSP_ORDER_HEADER));

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "Not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, pdu.AppId); /* AppId (4 bytes) */
	end = (int) Stream_GetPosition(s);

	if ((beg + header->Length) < end)
	{
		WLog_ERR(TAG, "Not enough data!");
		return ERROR_INVALID_DATA;
	}

	if ((beg + header->Length) > end)
	{
		if (Stream_GetRemainingLength(s) < (size_t)((beg + header->Length) - end))
		{
			WLog_ERR(TAG, "Not enough data!");
			return ERROR_INVALID_DATA;
		}

		Stream_SetPosition(s, (beg + header->Length));
	}

	IFCALLRET(context->ApplicationRemoved, error, context, &pdu);

	if (error)
		WLog_ERR(TAG, "context->ApplicationRemoved failed with error %"PRIu32"", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT encomsp_recv_window_created_pdu(encomspPlugin* encomsp, wStream* s,
        ENCOMSP_ORDER_HEADER* header)
{
	int beg, end;
	EncomspClientContext* context;
	ENCOMSP_WINDOW_CREATED_PDU pdu;
	UINT error;
	context = encomsp_get_client_interface(encomsp);

	if (!context)
		return ERROR_INVALID_HANDLE;

	beg = ((int) Stream_GetPosition(s)) - ENCOMSP_ORDER_HEADER_SIZE;
	CopyMemory(&pdu, header, sizeof(ENCOMSP_ORDER_HEADER));

	if (Stream_GetRemainingLength(s) < 10)
	{
		WLog_ERR(TAG, "Not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.Flags); /* Flags (2 bytes) */
	Stream_Read_UINT32(s, pdu.AppId); /* AppId (4 bytes) */
	Stream_Read_UINT32(s, pdu.WndId); /* WndId (4 bytes) */

	if ((error = encomsp_read_unicode_string(s, &(pdu.Name))))
	{
		WLog_ERR(TAG, "encomsp_read_unicode_string failed with error %"PRIu32"", error);
		return error;
	}

	end = (int) Stream_GetPosition(s);

	if ((beg + header->Length) < end)
	{
		WLog_ERR(TAG, "Not enough data!");
		return ERROR_INVALID_DATA;
	}

	if ((beg + header->Length) > end)
	{
		if (Stream_GetRemainingLength(s) < (size_t)((beg + header->Length) - end))
		{
			WLog_ERR(TAG, "Not enough data!");
			return ERROR_INVALID_DATA;
		}

		Stream_SetPosition(s, (beg + header->Length));
	}

	IFCALLRET(context->WindowCreated, error, context, &pdu);

	if (error)
		WLog_ERR(TAG, "context->WindowCreated failed with error %"PRIu32"", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT encomsp_recv_window_removed_pdu(encomspPlugin* encomsp, wStream* s,
        ENCOMSP_ORDER_HEADER* header)
{
	int beg, end;
	EncomspClientContext* context;
	ENCOMSP_WINDOW_REMOVED_PDU pdu;
	UINT error = CHANNEL_RC_OK;
	context = encomsp_get_client_interface(encomsp);

	if (!context)
		return ERROR_INVALID_HANDLE;

	beg = ((int) Stream_GetPosition(s)) - ENCOMSP_ORDER_HEADER_SIZE;
	CopyMemory(&pdu, header, sizeof(ENCOMSP_ORDER_HEADER));

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "Not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, pdu.WndId); /* WndId (4 bytes) */
	end = (int) Stream_GetPosition(s);

	if ((beg + header->Length) < end)
	{
		WLog_ERR(TAG, "Not enough data!");
		return ERROR_INVALID_DATA;
	}

	if ((beg + header->Length) > end)
	{
		if (Stream_GetRemainingLength(s) < (size_t)((beg + header->Length) - end))
		{
			WLog_ERR(TAG, "Not enough data!");
			return ERROR_INVALID_DATA;
		}

		Stream_SetPosition(s, (beg + header->Length));
	}

	IFCALLRET(context->WindowRemoved, error, context, &pdu);

	if (error)
		WLog_ERR(TAG, "context->WindowRemoved failed with error %"PRIu32"", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT encomsp_recv_show_window_pdu(encomspPlugin* encomsp, wStream* s,
        ENCOMSP_ORDER_HEADER* header)
{
	int beg, end;
	EncomspClientContext* context;
	ENCOMSP_SHOW_WINDOW_PDU pdu;
	UINT error = CHANNEL_RC_OK;
	context = encomsp_get_client_interface(encomsp);

	if (!context)
		return ERROR_INVALID_HANDLE;

	beg = ((int) Stream_GetPosition(s)) - ENCOMSP_ORDER_HEADER_SIZE;
	CopyMemory(&pdu, header, sizeof(ENCOMSP_ORDER_HEADER));

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "Not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, pdu.WndId); /* WndId (4 bytes) */
	end = (int) Stream_GetPosition(s);

	if ((beg + header->Length) < end)
	{
		WLog_ERR(TAG, "Not enough data!");
		return ERROR_INVALID_DATA;
	}

	if ((beg + header->Length) > end)
	{
		if (Stream_GetRemainingLength(s) < (size_t)((beg + header->Length) - end))
		{
			WLog_ERR(TAG, "Not enough data!");
			return ERROR_INVALID_DATA;
		}

		Stream_SetPosition(s, (beg + header->Length));
	}

	IFCALLRET(context->ShowWindow, error, context, &pdu);

	if (error)
		WLog_ERR(TAG, "context->ShowWindow failed with error %"PRIu32"", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT encomsp_recv_participant_created_pdu(encomspPlugin* encomsp,
        wStream* s, ENCOMSP_ORDER_HEADER* header)
{
	int beg, end;
	EncomspClientContext* context;
	ENCOMSP_PARTICIPANT_CREATED_PDU pdu;
	UINT error;
	context = encomsp_get_client_interface(encomsp);

	if (!context)
		return ERROR_INVALID_HANDLE;

	beg = ((int) Stream_GetPosition(s)) - ENCOMSP_ORDER_HEADER_SIZE;
	CopyMemory(&pdu, header, sizeof(ENCOMSP_ORDER_HEADER));

	if (Stream_GetRemainingLength(s) < 10)
	{
		WLog_ERR(TAG, "Not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, pdu.ParticipantId); /* ParticipantId (4 bytes) */
	Stream_Read_UINT32(s, pdu.GroupId); /* GroupId (4 bytes) */
	Stream_Read_UINT16(s, pdu.Flags); /* Flags (2 bytes) */

	if ((error = encomsp_read_unicode_string(s, &(pdu.FriendlyName))))
	{
		WLog_ERR(TAG, "encomsp_read_unicode_string failed with error %"PRIu32"", error);
		return error;
	}

	end = (int) Stream_GetPosition(s);

	if ((beg + header->Length) < end)
	{
		WLog_ERR(TAG, "Not enough data!");
		return ERROR_INVALID_DATA;
	}

	if ((beg + header->Length) > end)
	{
		if (Stream_GetRemainingLength(s) < (size_t)((beg + header->Length) - end))
		{
			WLog_ERR(TAG, "Not enough data!");
			return ERROR_INVALID_DATA;
		}

		Stream_SetPosition(s, (beg + header->Length));
	}

	IFCALLRET(context->ParticipantCreated, error, context, &pdu);

	if (error)
		WLog_ERR(TAG, "context->ParticipantCreated failed with error %"PRIu32"", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT encomsp_recv_participant_removed_pdu(encomspPlugin* encomsp,
        wStream* s, ENCOMSP_ORDER_HEADER* header)
{
	int beg, end;
	EncomspClientContext* context;
	ENCOMSP_PARTICIPANT_REMOVED_PDU pdu;
	UINT error = CHANNEL_RC_OK;
	context = encomsp_get_client_interface(encomsp);

	if (!context)
		return ERROR_INVALID_HANDLE;

	beg = ((int) Stream_GetPosition(s)) - ENCOMSP_ORDER_HEADER_SIZE;
	CopyMemory(&pdu, header, sizeof(ENCOMSP_ORDER_HEADER));

	if (Stream_GetRemainingLength(s) < 12)
	{
		WLog_ERR(TAG, "Not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, pdu.ParticipantId); /* ParticipantId (4 bytes) */
	Stream_Read_UINT32(s, pdu.DiscType); /* DiscType (4 bytes) */
	Stream_Read_UINT32(s, pdu.DiscCode); /* DiscCode (4 bytes) */
	end = (int) Stream_GetPosition(s);

	if ((beg + header->Length) < end)
	{
		WLog_ERR(TAG, "Not enough data!");
		return ERROR_INVALID_DATA;
	}

	if ((beg + header->Length) > end)
	{
		if (Stream_GetRemainingLength(s) < (size_t)((beg + header->Length) - end))
		{
			WLog_ERR(TAG, "Not enough data!");
			return ERROR_INVALID_DATA;
		}

		Stream_SetPosition(s, (beg + header->Length));
	}

	IFCALLRET(context->ParticipantRemoved, error, context, &pdu);

	if (error)
		WLog_ERR(TAG, "context->ParticipantRemoved failed with error %"PRIu32"", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT encomsp_recv_change_participant_control_level_pdu(
    encomspPlugin* encomsp, wStream* s, ENCOMSP_ORDER_HEADER* header)
{
	int beg, end;
	EncomspClientContext* context;
	ENCOMSP_CHANGE_PARTICIPANT_CONTROL_LEVEL_PDU pdu;
	UINT error = CHANNEL_RC_OK;
	context = encomsp_get_client_interface(encomsp);

	if (!context)
		return ERROR_INVALID_HANDLE;

	beg = ((int) Stream_GetPosition(s)) - ENCOMSP_ORDER_HEADER_SIZE;
	CopyMemory(&pdu, header, sizeof(ENCOMSP_ORDER_HEADER));

	if (Stream_GetRemainingLength(s) < 6)
	{
		WLog_ERR(TAG, "Not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.Flags); /* Flags (2 bytes) */
	Stream_Read_UINT32(s, pdu.ParticipantId); /* ParticipantId (4 bytes) */
	end = (int) Stream_GetPosition(s);

	if ((beg + header->Length) < end)
	{
		WLog_ERR(TAG, "Not enough data!");
		return ERROR_INVALID_DATA;
	}

	if ((beg + header->Length) > end)
	{
		if (Stream_GetRemainingLength(s) < (size_t)((beg + header->Length) - end))
		{
			WLog_ERR(TAG, "Not enough data!");
			return ERROR_INVALID_DATA;
		}

		Stream_SetPosition(s, (beg + header->Length));
	}

	IFCALLRET(context->ChangeParticipantControlLevel, error, context, &pdu);

	if (error)
		WLog_ERR(TAG, "context->ChangeParticipantControlLevel failed with error %"PRIu32"",
		         error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT encomsp_send_change_participant_control_level_pdu(
    EncomspClientContext* context,
    ENCOMSP_CHANGE_PARTICIPANT_CONTROL_LEVEL_PDU* pdu)
{
	wStream* s;
	encomspPlugin* encomsp;
	UINT error;
	encomsp = (encomspPlugin*) context->handle;
	pdu->Type = ODTYPE_PARTICIPANT_CTRL_CHANGED;
	pdu->Length = ENCOMSP_ORDER_HEADER_SIZE + 6;
	s = Stream_New(NULL, pdu->Length);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if ((error = encomsp_write_header(s, (ENCOMSP_ORDER_HEADER*) pdu)))
	{
		WLog_ERR(TAG, "encomsp_write_header failed with error %"PRIu32"!", error);
		return error;
	}

	Stream_Write_UINT16(s, pdu->Flags); /* Flags (2 bytes) */
	Stream_Write_UINT32(s, pdu->ParticipantId); /* ParticipantId (4 bytes) */
	Stream_SealLength(s);
	return encomsp_virtual_channel_write(encomsp, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT encomsp_recv_graphics_stream_paused_pdu(encomspPlugin* encomsp,
        wStream* s, ENCOMSP_ORDER_HEADER* header)
{
	int beg, end;
	EncomspClientContext* context;
	ENCOMSP_GRAPHICS_STREAM_PAUSED_PDU pdu;
	UINT error = CHANNEL_RC_OK;
	context = encomsp_get_client_interface(encomsp);

	if (!context)
		return ERROR_INVALID_HANDLE;

	beg = ((int) Stream_GetPosition(s)) - ENCOMSP_ORDER_HEADER_SIZE;
	CopyMemory(&pdu, header, sizeof(ENCOMSP_ORDER_HEADER));
	end = (int) Stream_GetPosition(s);

	if ((beg + header->Length) < end)
	{
		WLog_ERR(TAG, "Not enough data!");
		return ERROR_INVALID_DATA;
	}

	if ((beg + header->Length) > end)
	{
		if (Stream_GetRemainingLength(s) < (size_t)((beg + header->Length) - end))
		{
			WLog_ERR(TAG, "Not enough data!");
			return ERROR_INVALID_DATA;
		}

		Stream_SetPosition(s, (beg + header->Length));
	}

	IFCALLRET(context->GraphicsStreamPaused, error, context, &pdu);

	if (error)
		WLog_ERR(TAG, "context->GraphicsStreamPaused failed with error %"PRIu32"", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT encomsp_recv_graphics_stream_resumed_pdu(encomspPlugin* encomsp,
        wStream* s, ENCOMSP_ORDER_HEADER* header)
{
	int beg, end;
	EncomspClientContext* context;
	ENCOMSP_GRAPHICS_STREAM_RESUMED_PDU pdu;
	UINT error = CHANNEL_RC_OK;
	context = encomsp_get_client_interface(encomsp);

	if (!context)
		return ERROR_INVALID_HANDLE;

	beg = ((int) Stream_GetPosition(s)) - ENCOMSP_ORDER_HEADER_SIZE;
	CopyMemory(&pdu, header, sizeof(ENCOMSP_ORDER_HEADER));
	end = (int) Stream_GetPosition(s);

	if ((beg + header->Length) < end)
	{
		WLog_ERR(TAG, "Not enough data!");
		return ERROR_INVALID_DATA;
	}

	if ((beg + header->Length) > end)
	{
		if (Stream_GetRemainingLength(s) < (size_t)((beg + header->Length) - end))
		{
			WLog_ERR(TAG, "Not enough data!");
			return ERROR_INVALID_DATA;
		}

		Stream_SetPosition(s, (beg + header->Length));
	}

	IFCALLRET(context->GraphicsStreamResumed, error, context, &pdu);

	if (error)
		WLog_ERR(TAG, "context->GraphicsStreamResumed failed with error %"PRIu32"", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT encomsp_process_receive(encomspPlugin* encomsp, wStream* s)
{
	UINT error = CHANNEL_RC_OK;
	ENCOMSP_ORDER_HEADER header;

	while (Stream_GetRemainingLength(s) > 0)
	{
		if ((error = encomsp_read_header(s, &header)))
		{
			WLog_ERR(TAG, "encomsp_read_header failed with error %"PRIu32"!", error);
			return error;
		}

		//WLog_DBG(TAG, "EncomspReceive: Type: %"PRIu16" Length: %"PRIu16"", header.Type, header.Length);

		switch (header.Type)
		{
			case ODTYPE_FILTER_STATE_UPDATED:
				if ((error = encomsp_recv_filter_updated_pdu(encomsp, s, &header)))
				{
					WLog_ERR(TAG, "encomsp_recv_filter_updated_pdu failed with error %"PRIu32"!", error);
					return error;
				}

				break;

			case ODTYPE_APP_REMOVED:
				if ((error = encomsp_recv_application_removed_pdu(encomsp, s, &header)))
				{
					WLog_ERR(TAG, "encomsp_recv_application_removed_pdu failed with error %"PRIu32"!",
					         error);
					return error;
				}

				break;

			case ODTYPE_APP_CREATED:
				if ((error = encomsp_recv_application_created_pdu(encomsp, s, &header)))
				{
					WLog_ERR(TAG, "encomsp_recv_application_removed_pdu failed with error %"PRIu32"!",
					         error);
					return error;
				}

				break;

			case ODTYPE_WND_REMOVED:
				if ((error = encomsp_recv_window_removed_pdu(encomsp, s, &header)))
				{
					WLog_ERR(TAG, "encomsp_recv_window_removed_pdu failed with error %"PRIu32"!", error);
					return error;
				}

				break;

			case ODTYPE_WND_CREATED:
				if ((error = encomsp_recv_window_created_pdu(encomsp, s, &header)))
				{
					WLog_ERR(TAG, "encomsp_recv_window_created_pdu failed with error %"PRIu32"!", error);
					return error;
				}

				break;

			case ODTYPE_WND_SHOW:
				if ((error = encomsp_recv_show_window_pdu(encomsp, s, &header)))
				{
					WLog_ERR(TAG, "encomsp_recv_show_window_pdu failed with error %"PRIu32"!", error);
					return error;
				}

				break;

			case ODTYPE_PARTICIPANT_REMOVED:
				if ((error = encomsp_recv_participant_removed_pdu(encomsp, s, &header)))
				{
					WLog_ERR(TAG, "encomsp_recv_participant_removed_pdu failed with error %"PRIu32"!",
					         error);
					return error;
				}

				break;

			case ODTYPE_PARTICIPANT_CREATED:
				if ((error = encomsp_recv_participant_created_pdu(encomsp, s, &header)))
				{
					WLog_ERR(TAG, "encomsp_recv_participant_created_pdu failed with error %"PRIu32"!",
					         error);
					return error;
				}

				break;

			case ODTYPE_PARTICIPANT_CTRL_CHANGED:
				if ((error = encomsp_recv_change_participant_control_level_pdu(encomsp, s,
				             &header)))
				{
					WLog_ERR(TAG,
					         "encomsp_recv_change_participant_control_level_pdu failed with error %"PRIu32"!",
					         error);
					return error;
				}

				break;

			case ODTYPE_GRAPHICS_STREAM_PAUSED:
				if ((error = encomsp_recv_graphics_stream_paused_pdu(encomsp, s, &header)))
				{
					WLog_ERR(TAG, "encomsp_recv_graphics_stream_paused_pdu failed with error %"PRIu32"!",
					         error);
					return error;
				}

				break;

			case ODTYPE_GRAPHICS_STREAM_RESUMED:
				if ((error = encomsp_recv_graphics_stream_resumed_pdu(encomsp, s, &header)))
				{
					WLog_ERR(TAG, "encomsp_recv_graphics_stream_resumed_pdu failed with error %"PRIu32"!",
					         error);
					return error;
				}

				break;

			default:
				WLog_ERR(TAG, "header.Type %"PRIu16" not found", header.Type);
				return ERROR_INVALID_DATA;
				break;
		}
	}

	return error;
}

static void encomsp_process_connect(encomspPlugin* encomsp)
{
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT encomsp_virtual_channel_event_data_received(encomspPlugin* encomsp,
        void* pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	wStream* data_in;

	if ((dataFlags & CHANNEL_FLAG_SUSPEND) || (dataFlags & CHANNEL_FLAG_RESUME))
		return CHANNEL_RC_OK;

	if (dataFlags & CHANNEL_FLAG_FIRST)
	{
		if (encomsp->data_in)
			Stream_Free(encomsp->data_in, TRUE);

		encomsp->data_in = Stream_New(NULL, totalLength);

		if (!encomsp->data_in)
		{
			WLog_ERR(TAG, "Stream_New failed!");
			return CHANNEL_RC_NO_MEMORY;
		}
	}

	data_in = encomsp->data_in;

	if (!Stream_EnsureRemainingCapacity(data_in, (int) dataLength))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write(data_in, pData, dataLength);

	if (dataFlags & CHANNEL_FLAG_LAST)
	{
		if (Stream_Capacity(data_in) != Stream_GetPosition(data_in))
		{
			WLog_ERR(TAG,  "encomsp_plugin_process_received: read error");
			return ERROR_INVALID_DATA;
		}

		encomsp->data_in = NULL;
		Stream_SealLength(data_in);
		Stream_SetPosition(data_in, 0);

		if (!MessageQueue_Post(encomsp->queue, NULL, 0, (void*) data_in, NULL))
		{
			WLog_ERR(TAG, "MessageQueue_Post failed!");
			return ERROR_INTERNAL_ERROR;
		}
	}

	return CHANNEL_RC_OK;
}

static VOID VCAPITYPE encomsp_virtual_channel_open_event_ex(LPVOID lpUserParam, DWORD openHandle,
        UINT event,
        LPVOID pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	UINT error = CHANNEL_RC_OK;
	encomspPlugin* encomsp = (encomspPlugin*) lpUserParam;

	if (!encomsp || (encomsp->OpenHandle != openHandle))
	{
		WLog_ERR(TAG,  "error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_DATA_RECEIVED:
			if ((error = encomsp_virtual_channel_event_data_received(encomsp, pData,
			             dataLength, totalLength, dataFlags)))
				WLog_ERR(TAG, "encomsp_virtual_channel_event_data_received failed with error %"PRIu32"", error);

			break;

		case CHANNEL_EVENT_WRITE_COMPLETE:
			Stream_Free((wStream*) pData, TRUE);
			break;

		case CHANNEL_EVENT_USER:
			break;
	}

	if (error && encomsp->rdpcontext)
		setChannelError(encomsp->rdpcontext, error, "encomsp_virtual_channel_open_event reported an error");

	return;
}

static void* encomsp_virtual_channel_client_thread(void* arg)
{
	wStream* data;
	wMessage message;
	encomspPlugin* encomsp = (encomspPlugin*) arg;
	UINT error = CHANNEL_RC_OK;
	encomsp_process_connect(encomsp);

	while (1)
	{
		if (!MessageQueue_Wait(encomsp->queue))
		{
			WLog_ERR(TAG, "MessageQueue_Wait failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		if (!MessageQueue_Peek(encomsp->queue, &message, TRUE))
		{
			WLog_ERR(TAG, "MessageQueue_Peek failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		if (message.id == WMQ_QUIT)
			break;

		if (message.id == 0)
		{
			data = (wStream*) message.wParam;

			if ((error = encomsp_process_receive(encomsp, data)))
			{
				WLog_ERR(TAG, "encomsp_process_receive failed with error %"PRIu32"!", error);
				break;
			}
		}
	}

	if (error && encomsp->rdpcontext)
		setChannelError(encomsp->rdpcontext, error,
		                "encomsp_virtual_channel_client_thread reported an error");

	ExitThread((DWORD)error);
	return NULL;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT encomsp_virtual_channel_event_connected(encomspPlugin* encomsp,
        LPVOID pData, UINT32 dataLength)
{
	UINT32 status;
	status = encomsp->channelEntryPoints.pVirtualChannelOpenEx(encomsp->InitHandle,
	         &encomsp->OpenHandle, encomsp->channelDef.name,
	         encomsp_virtual_channel_open_event_ex);

	if (status != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "pVirtualChannelOpen failed with %s [%08"PRIX32"]",
		         WTSErrorToString(status), status);
		return status;
	}

	encomsp->queue = MessageQueue_New(NULL);

	if (!encomsp->queue)
	{
		WLog_ERR(TAG, "MessageQueue_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if (!(encomsp->thread = CreateThread(NULL, 0,
	                                     (LPTHREAD_START_ROUTINE) encomsp_virtual_channel_client_thread, (void*) encomsp,
	                                     0, NULL)))
	{
		WLog_ERR(TAG, "CreateThread failed!");
		MessageQueue_Free(encomsp->queue);
		return ERROR_INTERNAL_ERROR;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT encomsp_virtual_channel_event_disconnected(encomspPlugin* encomsp)
{
	UINT rc;

	if (MessageQueue_PostQuit(encomsp->queue, 0)
	    && (WaitForSingleObject(encomsp->thread, INFINITE) == WAIT_FAILED))
	{
		rc = GetLastError();
		WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"", rc);
		return rc;
	}

	MessageQueue_Free(encomsp->queue);
	CloseHandle(encomsp->thread);
	encomsp->queue = NULL;
	encomsp->thread = NULL;
	rc = encomsp->channelEntryPoints.pVirtualChannelCloseEx(encomsp->InitHandle, encomsp->OpenHandle);

	if (CHANNEL_RC_OK != rc)
	{
		WLog_ERR(TAG, "pVirtualChannelClose failed with %s [%08"PRIX32"]",
		         WTSErrorToString(rc), rc);
		return rc;
	}

	encomsp->OpenHandle = 0;

	if (encomsp->data_in)
	{
		Stream_Free(encomsp->data_in, TRUE);
		encomsp->data_in = NULL;
	}

	return CHANNEL_RC_OK;
}


/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT encomsp_virtual_channel_event_terminated(encomspPlugin* encomsp)
{
	encomsp->InitHandle = 0;
	free(encomsp->context);
	free(encomsp);
	return CHANNEL_RC_OK;
}

static VOID VCAPITYPE encomsp_virtual_channel_init_event_ex(LPVOID lpUserParam, LPVOID pInitHandle,
        UINT event, LPVOID pData, UINT dataLength)
{
	UINT error = CHANNEL_RC_OK;
	encomspPlugin* encomsp = (encomspPlugin*) lpUserParam;

	if (!encomsp || (encomsp->InitHandle != pInitHandle))
	{
		WLog_ERR(TAG,  "error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_CONNECTED:
			if ((error = encomsp_virtual_channel_event_connected(encomsp, pData,
			             dataLength)))
				WLog_ERR(TAG, "encomsp_virtual_channel_event_connected failed with error %"PRIu32"",
				         error);

			break;

		case CHANNEL_EVENT_DISCONNECTED:
			if ((error = encomsp_virtual_channel_event_disconnected(encomsp)))
				WLog_ERR(TAG,
				         "encomsp_virtual_channel_event_disconnected failed with error %"PRIu32"", error);

			break;

		case CHANNEL_EVENT_TERMINATED:
			encomsp_virtual_channel_event_terminated(encomsp);
			break;

		default:
			WLog_ERR(TAG, "Unhandled event type %"PRIu32"", event);
	}

	if (error && encomsp->rdpcontext)
		setChannelError(encomsp->rdpcontext, error, "encomsp_virtual_channel_init_event reported an error");
}

/* encomsp is always built-in */
#define VirtualChannelEntryEx	encomsp_VirtualChannelEntryEx

BOOL VCAPITYPE VirtualChannelEntryEx(PCHANNEL_ENTRY_POINTS_EX pEntryPoints, PVOID pInitHandle)
{
	UINT rc;
	encomspPlugin* encomsp;
	EncomspClientContext* context = NULL;
	CHANNEL_ENTRY_POINTS_FREERDP_EX* pEntryPointsEx;
	BOOL isFreerdp = FALSE;
	encomsp = (encomspPlugin*) calloc(1, sizeof(encomspPlugin));

	if (!encomsp)
	{
		WLog_ERR(TAG, "calloc failed!");
		return FALSE;
	}

	encomsp->channelDef.options =
	    CHANNEL_OPTION_INITIALIZED |
	    CHANNEL_OPTION_ENCRYPT_RDP |
	    CHANNEL_OPTION_COMPRESS_RDP |
	    CHANNEL_OPTION_SHOW_PROTOCOL;
	strcpy(encomsp->channelDef.name, "encomsp");
	pEntryPointsEx = (CHANNEL_ENTRY_POINTS_FREERDP_EX*) pEntryPoints;

	if ((pEntryPointsEx->cbSize >= sizeof(CHANNEL_ENTRY_POINTS_FREERDP_EX)) &&
	    (pEntryPointsEx->MagicNumber == FREERDP_CHANNEL_MAGIC_NUMBER))
	{
		context = (EncomspClientContext*) calloc(1, sizeof(EncomspClientContext));

		if (!context)
		{
			WLog_ERR(TAG, "calloc failed!");
			goto error_out;
		}

		context->handle = (void*) encomsp;
		context->FilterUpdated = NULL;
		context->ApplicationCreated = NULL;
		context->ApplicationRemoved = NULL;
		context->WindowCreated = NULL;
		context->WindowRemoved = NULL;
		context->ShowWindow = NULL;
		context->ParticipantCreated = NULL;
		context->ParticipantRemoved = NULL;
		context->ChangeParticipantControlLevel =
		    encomsp_send_change_participant_control_level_pdu;
		context->GraphicsStreamPaused = NULL;
		context->GraphicsStreamResumed = NULL;
		encomsp->context = context;
		encomsp->rdpcontext = pEntryPointsEx->context;
		isFreerdp = TRUE;
	}

	CopyMemory(&(encomsp->channelEntryPoints), pEntryPoints,
	           sizeof(CHANNEL_ENTRY_POINTS_FREERDP_EX));
	encomsp->InitHandle = pInitHandle;
	rc = encomsp->channelEntryPoints.pVirtualChannelInitEx(encomsp, context, pInitHandle,
	        &encomsp->channelDef, 1, VIRTUAL_CHANNEL_VERSION_WIN2000,
	        encomsp_virtual_channel_init_event_ex);

	if (CHANNEL_RC_OK != rc)
	{
		WLog_ERR(TAG, "failed with %s [%08"PRIX32"]",
		         WTSErrorToString(rc), rc);
		goto error_out;
	}

	encomsp->channelEntryPoints.pInterface = context;
	return TRUE;
error_out:

	if (isFreerdp)
		free(encomsp->context);

	free(encomsp);
	return FALSE;
}
