/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Remote Assistance Virtual Channel
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

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/stream.h>

#include <freerdp/freerdp.h>

#include "remdesk_main.h"
#include "remdesk_common.h"

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_virtual_channel_write(RemdeskServerContext* context, wStream* s)
{
	const size_t len = Stream_Length(s);
	WINPR_ASSERT(len <= UINT32_MAX);
	ULONG BytesWritten = 0;
	BOOL status = WTSVirtualChannelWrite(context->priv->ChannelHandle, Stream_BufferAs(s, char),
	                                     (UINT32)len, &BytesWritten);
	return (status) ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_send_ctl_result_pdu(RemdeskServerContext* context, UINT32 result)
{
	wStream* s = NULL;
	REMDESK_CTL_RESULT_PDU pdu;
	UINT error = 0;
	pdu.result = result;

	if ((error = remdesk_prepare_ctl_header(&(pdu.ctlHeader), REMDESK_CTL_RESULT, 4)))
	{
		WLog_ERR(TAG, "remdesk_prepare_ctl_header failed with error %" PRIu32 "!", error);
		return error;
	}

	s = Stream_New(NULL, REMDESK_CHANNEL_CTL_SIZE + pdu.ctlHeader.ch.DataLength);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if ((error = remdesk_write_ctl_header(s, &(pdu.ctlHeader))))
	{
		WLog_ERR(TAG, "remdesk_write_ctl_header failed with error %" PRIu32 "!", error);
		goto out;
	}

	Stream_Write_UINT32(s, pdu.result); /* result (4 bytes) */
	Stream_SealLength(s);

	if ((error = remdesk_virtual_channel_write(context, s)))
		WLog_ERR(TAG, "remdesk_virtual_channel_write failed with error %" PRIu32 "!", error);

out:
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_send_ctl_version_info_pdu(RemdeskServerContext* context)
{
	wStream* s = NULL;
	REMDESK_CTL_VERSION_INFO_PDU pdu;
	UINT error = 0;

	if ((error = remdesk_prepare_ctl_header(&(pdu.ctlHeader), REMDESK_CTL_VERSIONINFO, 8)))
	{
		WLog_ERR(TAG, "remdesk_prepare_ctl_header failed with error %" PRIu32 "!", error);
		return error;
	}

	pdu.versionMajor = 1;
	pdu.versionMinor = 2;
	s = Stream_New(NULL, REMDESK_CHANNEL_CTL_SIZE + pdu.ctlHeader.ch.DataLength);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if ((error = remdesk_write_ctl_header(s, &(pdu.ctlHeader))))
	{
		WLog_ERR(TAG, "remdesk_write_ctl_header failed with error %" PRIu32 "!", error);
		goto out;
	}

	Stream_Write_UINT32(s, pdu.versionMajor); /* versionMajor (4 bytes) */
	Stream_Write_UINT32(s, pdu.versionMinor); /* versionMinor (4 bytes) */
	Stream_SealLength(s);

	if ((error = remdesk_virtual_channel_write(context, s)))
		WLog_ERR(TAG, "remdesk_virtual_channel_write failed with error %" PRIu32 "!", error);

out:
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_recv_ctl_version_info_pdu(RemdeskServerContext* context, wStream* s,
                                              REMDESK_CHANNEL_HEADER* header)
{
	UINT32 versionMajor = 0;
	UINT32 versionMinor = 0;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, versionMajor); /* versionMajor (4 bytes) */
	Stream_Read_UINT32(s, versionMinor); /* versionMinor (4 bytes) */
	if ((versionMajor != 1) || (versionMinor != 2))
	{
		WLog_ERR(TAG, "REMOTEDESKTOP_CTL_VERSIONINFO_PACKET invalid version %" PRIu32 ".%" PRIu32,
		         versionMajor, versionMinor);
		return ERROR_INVALID_DATA;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_recv_ctl_remote_control_desktop_pdu(RemdeskServerContext* context, wStream* s,
                                                        REMDESK_CHANNEL_HEADER* header)
{
	SSIZE_T cchStringW = 0;
	SSIZE_T cbRaConnectionStringW = 0;
	REMDESK_CTL_REMOTE_CONTROL_DESKTOP_PDU pdu = { 0 };
	UINT error = 0;
	UINT32 msgLength = header->DataLength - 4;
	const WCHAR* pStringW = Stream_ConstPointer(s);
	const WCHAR* raConnectionStringW = pStringW;

	while ((msgLength > 0) && pStringW[cchStringW])
	{
		msgLength -= 2;
		cchStringW++;
	}

	if (pStringW[cchStringW] || !cchStringW)
		return ERROR_INVALID_DATA;

	cchStringW++;
	cbRaConnectionStringW = cchStringW * 2;
	pdu.raConnectionString =
	    ConvertWCharNToUtf8Alloc(raConnectionStringW, cbRaConnectionStringW / sizeof(WCHAR), NULL);
	if (!pdu.raConnectionString)
		return ERROR_INTERNAL_ERROR;

	WLog_INFO(TAG, "RaConnectionString: %s", pdu.raConnectionString);
	free(pdu.raConnectionString);

	if ((error = remdesk_send_ctl_result_pdu(context, 0)))
		WLog_ERR(TAG, "remdesk_send_ctl_result_pdu failed with error %" PRIu32 "!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_recv_ctl_authenticate_pdu(RemdeskServerContext* context, wStream* s,
                                              REMDESK_CHANNEL_HEADER* header)
{
	int cchStringW = 0;
	UINT32 msgLength = 0;
	int cbExpertBlobW = 0;
	const WCHAR* expertBlobW = NULL;
	int cbRaConnectionStringW = 0;
	REMDESK_CTL_AUTHENTICATE_PDU pdu = { 0 };
	msgLength = header->DataLength - 4;
	const WCHAR* pStringW = Stream_ConstPointer(s);
	const WCHAR* raConnectionStringW = pStringW;

	while ((msgLength > 0) && pStringW[cchStringW])
	{
		msgLength -= 2;
		cchStringW++;
	}

	if (pStringW[cchStringW] || !cchStringW)
		return ERROR_INVALID_DATA;

	cchStringW++;
	cbRaConnectionStringW = cchStringW * 2;
	pStringW += cchStringW;
	expertBlobW = pStringW;
	cchStringW = 0;

	while ((msgLength > 0) && pStringW[cchStringW])
	{
		msgLength -= 2;
		cchStringW++;
	}

	if (pStringW[cchStringW] || !cchStringW)
		return ERROR_INVALID_DATA;

	cchStringW++;
	cbExpertBlobW = cchStringW * 2;
	pdu.raConnectionString =
	    ConvertWCharNToUtf8Alloc(raConnectionStringW, cbRaConnectionStringW / sizeof(WCHAR), NULL);
	if (!pdu.raConnectionString)
		return ERROR_INTERNAL_ERROR;

	pdu.expertBlob = ConvertWCharNToUtf8Alloc(expertBlobW, cbExpertBlobW / sizeof(WCHAR), NULL);
	if (!pdu.expertBlob)
	{
		free(pdu.raConnectionString);
		return ERROR_INTERNAL_ERROR;
	}

	WLog_INFO(TAG, "RaConnectionString: %s ExpertBlob: %s", pdu.raConnectionString, pdu.expertBlob);
	free(pdu.raConnectionString);
	free(pdu.expertBlob);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_recv_ctl_verify_password_pdu(RemdeskServerContext* context, wStream* s,
                                                 REMDESK_CHANNEL_HEADER* header)
{
	SSIZE_T cbExpertBlobW = 0;
	REMDESK_CTL_VERIFY_PASSWORD_PDU pdu;
	UINT error = 0;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return ERROR_INVALID_DATA;

	const WCHAR* expertBlobW = Stream_ConstPointer(s);
	cbExpertBlobW = header->DataLength - 4;

	pdu.expertBlob = ConvertWCharNToUtf8Alloc(expertBlobW, cbExpertBlobW / sizeof(WCHAR), NULL);
	if (pdu.expertBlob)
		return ERROR_INTERNAL_ERROR;

	WLog_INFO(TAG, "ExpertBlob: %s", pdu.expertBlob);

	if ((error = remdesk_send_ctl_result_pdu(context, 0)))
		WLog_ERR(TAG, "remdesk_send_ctl_result_pdu failed with error %" PRIu32 "!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_recv_ctl_pdu(RemdeskServerContext* context, wStream* s,
                                 REMDESK_CHANNEL_HEADER* header)
{
	UINT error = CHANNEL_RC_OK;
	UINT32 msgType = 0;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, msgType); /* msgType (4 bytes) */
	WLog_INFO(TAG, "msgType: %" PRIu32 "", msgType);

	switch (msgType)
	{
		case REMDESK_CTL_REMOTE_CONTROL_DESKTOP:
			if ((error = remdesk_recv_ctl_remote_control_desktop_pdu(context, s, header)))
			{
				WLog_ERR(TAG,
				         "remdesk_recv_ctl_remote_control_desktop_pdu failed with error %" PRIu32
				         "!",
				         error);
				return error;
			}

			break;

		case REMDESK_CTL_AUTHENTICATE:
			if ((error = remdesk_recv_ctl_authenticate_pdu(context, s, header)))
			{
				WLog_ERR(TAG, "remdesk_recv_ctl_authenticate_pdu failed with error %" PRIu32 "!",
				         error);
				return error;
			}

			break;

		case REMDESK_CTL_DISCONNECT:
			break;

		case REMDESK_CTL_VERSIONINFO:
			if ((error = remdesk_recv_ctl_version_info_pdu(context, s, header)))
			{
				WLog_ERR(TAG, "remdesk_recv_ctl_version_info_pdu failed with error %" PRIu32 "!",
				         error);
				return error;
			}

			break;

		case REMDESK_CTL_ISCONNECTED:
			break;

		case REMDESK_CTL_VERIFY_PASSWORD:
			if ((error = remdesk_recv_ctl_verify_password_pdu(context, s, header)))
			{
				WLog_ERR(TAG, "remdesk_recv_ctl_verify_password_pdu failed with error %" PRIu32 "!",
				         error);
				return error;
			}

			break;

		case REMDESK_CTL_EXPERT_ON_VISTA:
			break;

		case REMDESK_CTL_RANOVICE_NAME:
			break;

		case REMDESK_CTL_RAEXPERT_NAME:
			break;

		case REMDESK_CTL_TOKEN:
			break;

		default:
			WLog_ERR(TAG, "remdesk_recv_control_pdu: unknown msgType: %" PRIu32 "", msgType);
			error = ERROR_INVALID_DATA;
			break;
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_server_receive_pdu(RemdeskServerContext* context, wStream* s)
{
	UINT error = CHANNEL_RC_OK;
	REMDESK_CHANNEL_HEADER header;
#if 0
	WLog_INFO(TAG, "RemdeskReceive: %"PRIuz"", Stream_GetRemainingLength(s));
	winpr_HexDump(WCHAR* expertBlobW = NULL;(s), Stream_GetRemainingLength(s));
#endif

	if ((error = remdesk_read_channel_header(s, &header)))
	{
		WLog_ERR(TAG, "remdesk_read_channel_header failed with error %" PRIu32 "!", error);
		return error;
	}

	if (strcmp(header.ChannelName, "RC_CTL") == 0)
	{
		if ((error = remdesk_recv_ctl_pdu(context, s, &header)))
		{
			WLog_ERR(TAG, "remdesk_recv_ctl_pdu failed with error %" PRIu32 "!", error);
			return error;
		}
	}
	else if (strcmp(header.ChannelName, "70") == 0)
	{
	}
	else if (strcmp(header.ChannelName, "71") == 0)
	{
	}
	else if (strcmp(header.ChannelName, ".") == 0)
	{
	}
	else if (strcmp(header.ChannelName, "1000.") == 0)
	{
	}
	else if (strcmp(header.ChannelName, "RA_FX") == 0)
	{
	}
	else
	{
	}

	return error;
}

static DWORD WINAPI remdesk_server_thread(LPVOID arg)
{
	wStream* s = NULL;
	DWORD status = 0;
	DWORD nCount = 0;
	void* buffer = NULL;
	UINT32* pHeader = NULL;
	UINT32 PduLength = 0;
	HANDLE events[8];
	HANDLE ChannelEvent = NULL;
	DWORD BytesReturned = 0;
	RemdeskServerContext* context = NULL;
	UINT error = 0;
	context = (RemdeskServerContext*)arg;
	buffer = NULL;
	BytesReturned = 0;
	ChannelEvent = NULL;
	s = Stream_New(NULL, 4096);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto out;
	}

	if (WTSVirtualChannelQuery(context->priv->ChannelHandle, WTSVirtualEventHandle, &buffer,
	                           &BytesReturned) == TRUE)
	{
		if (BytesReturned == sizeof(HANDLE))
			ChannelEvent = *(HANDLE*)buffer;

		WTSFreeMemory(buffer);
	}
	else
	{
		WLog_ERR(TAG, "WTSVirtualChannelQuery failed!");
		error = ERROR_INTERNAL_ERROR;
		goto out;
	}

	nCount = 0;
	events[nCount++] = ChannelEvent;
	events[nCount++] = context->priv->StopEvent;

	if ((error = remdesk_send_ctl_version_info_pdu(context)))
	{
		WLog_ERR(TAG, "remdesk_send_ctl_version_info_pdu failed with error %" PRIu32 "!", error);
		goto out;
	}

	while (1)
	{
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %" PRIu32 "", error);
			break;
		}

		status = WaitForSingleObject(context->priv->StopEvent, 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", error);
			break;
		}

		if (status == WAIT_OBJECT_0)
		{
			break;
		}

		const size_t len = Stream_Capacity(s);
		if (len > UINT32_MAX)
		{
			error = ERROR_INTERNAL_ERROR;
			break;
		}
		if (WTSVirtualChannelRead(context->priv->ChannelHandle, 0, Stream_BufferAs(s, char),
		                          (UINT32)len, &BytesReturned))
		{
			if (BytesReturned)
				Stream_Seek(s, BytesReturned);
		}
		else
		{
			if (!Stream_EnsureRemainingCapacity(s, BytesReturned))
			{
				WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
				error = CHANNEL_RC_NO_MEMORY;
				break;
			}
		}

		if (Stream_GetPosition(s) >= 8)
		{
			pHeader = Stream_BufferAs(s, UINT32);
			PduLength = pHeader[0] + pHeader[1] + 8;

			if (PduLength >= Stream_GetPosition(s))
			{
				Stream_SealLength(s);
				Stream_SetPosition(s, 0);

				if ((error = remdesk_server_receive_pdu(context, s)))
				{
					WLog_ERR(TAG, "remdesk_server_receive_pdu failed with error %" PRIu32 "!",
					         error);
					break;
				}

				Stream_SetPosition(s, 0);
			}
		}
	}

	Stream_Free(s, TRUE);
out:

	if (error && context->rdpcontext)
		setChannelError(context->rdpcontext, error, "remdesk_server_thread reported an error");

	ExitThread(error);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_server_start(RemdeskServerContext* context)
{
	context->priv->ChannelHandle =
	    WTSVirtualChannelOpen(context->vcm, WTS_CURRENT_SESSION, REMDESK_SVC_CHANNEL_NAME);

	if (!context->priv->ChannelHandle)
	{
		WLog_ERR(TAG, "WTSVirtualChannelOpen failed!");
		return ERROR_INTERNAL_ERROR;
	}

	if (!(context->priv->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		WLog_ERR(TAG, "CreateEvent failed!");
		return ERROR_INTERNAL_ERROR;
	}

	if (!(context->priv->Thread =
	          CreateThread(NULL, 0, remdesk_server_thread, (void*)context, 0, NULL)))
	{
		WLog_ERR(TAG, "CreateThread failed!");
		(void)CloseHandle(context->priv->StopEvent);
		context->priv->StopEvent = NULL;
		return ERROR_INTERNAL_ERROR;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_server_stop(RemdeskServerContext* context)
{
	UINT error = 0;
	(void)SetEvent(context->priv->StopEvent);

	if (WaitForSingleObject(context->priv->Thread, INFINITE) == WAIT_FAILED)
	{
		error = GetLastError();
		WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "!", error);
		return error;
	}

	(void)CloseHandle(context->priv->Thread);
	(void)CloseHandle(context->priv->StopEvent);
	return CHANNEL_RC_OK;
}

RemdeskServerContext* remdesk_server_context_new(HANDLE vcm)
{
	RemdeskServerContext* context = NULL;
	context = (RemdeskServerContext*)calloc(1, sizeof(RemdeskServerContext));

	if (context)
	{
		context->vcm = vcm;
		context->Start = remdesk_server_start;
		context->Stop = remdesk_server_stop;
		context->priv = (RemdeskServerPrivate*)calloc(1, sizeof(RemdeskServerPrivate));

		if (!context->priv)
		{
			free(context);
			return NULL;
		}

		context->priv->Version = 1;
	}

	return context;
}

void remdesk_server_context_free(RemdeskServerContext* context)
{
	if (context)
	{
		if (context->priv->ChannelHandle != INVALID_HANDLE_VALUE)
			(void)WTSVirtualChannelClose(context->priv->ChannelHandle);

		free(context->priv);
		free(context);
	}
}
