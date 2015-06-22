/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Remote Assistance Virtual Channel
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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
#include <winpr/stream.h>

#include "remdesk_main.h"

static int remdesk_virtual_channel_write(RemdeskServerContext* context, wStream* s)
{
	BOOL status;
	ULONG BytesWritten = 0;

	status = WTSVirtualChannelWrite(context->priv->ChannelHandle,
			(PCHAR) Stream_Buffer(s), Stream_Length(s), &BytesWritten);

	return (status) ? 1 : -1;
}

static int remdesk_read_channel_header(wStream* s, REMDESK_CHANNEL_HEADER* header)
{
	int status;
	UINT32 ChannelNameLen;
	char* pChannelName = NULL;

	if (Stream_GetRemainingLength(s) < 8)
		return -1;

	Stream_Read_UINT32(s, ChannelNameLen); /* ChannelNameLen (4 bytes) */
	Stream_Read_UINT32(s, header->DataLength); /* DataLen (4 bytes) */

	if (ChannelNameLen > 64)
		return -1;

	if ((ChannelNameLen % 2) != 0)
		return -1;

	if (Stream_GetRemainingLength(s) < ChannelNameLen)
		return -1;

	ZeroMemory(header->ChannelName, sizeof(header->ChannelName));

	pChannelName = (char*) header->ChannelName;
	status = ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) Stream_Pointer(s),
			ChannelNameLen / 2, &pChannelName, 32, NULL, NULL);

	Stream_Seek(s, ChannelNameLen);

	if (status <= 0)
		return -1;

	return 1;
}

static int remdesk_write_channel_header(wStream* s, REMDESK_CHANNEL_HEADER* header)
{
	int index;
	UINT32 ChannelNameLen;
	WCHAR ChannelNameW[32];

	ZeroMemory(ChannelNameW, sizeof(ChannelNameW));

	for (index = 0; index < 32; index++)
	{
		ChannelNameW[index] = (WCHAR) header->ChannelName[index];
	}

	ChannelNameLen = (strlen(header->ChannelName) + 1) * 2;

	Stream_Write_UINT32(s, ChannelNameLen); /* ChannelNameLen (4 bytes) */
	Stream_Write_UINT32(s, header->DataLength); /* DataLen (4 bytes) */

	Stream_Write(s, ChannelNameW, ChannelNameLen); /* ChannelName (variable) */

	return 1;
}

static int remdesk_write_ctl_header(wStream* s, REMDESK_CTL_HEADER* ctlHeader)
{
	remdesk_write_channel_header(s, (REMDESK_CHANNEL_HEADER*) ctlHeader);
	Stream_Write_UINT32(s, ctlHeader->msgType); /* msgType (4 bytes) */
	return 1;
}

static int remdesk_prepare_ctl_header(REMDESK_CTL_HEADER* ctlHeader, UINT32 msgType, UINT32 msgSize)
{
	ctlHeader->msgType = msgType;
	strcpy(ctlHeader->ChannelName, REMDESK_CHANNEL_CTL_NAME);
	ctlHeader->DataLength = 4 + msgSize;
	return 1;
}

static int remdesk_send_ctl_result_pdu(RemdeskServerContext* context, UINT32 result)
{
	wStream* s;
	REMDESK_CTL_RESULT_PDU pdu;

	pdu.result = result;

	remdesk_prepare_ctl_header(&(pdu.ctlHeader), REMDESK_CTL_RESULT, 4);

	s = Stream_New(NULL, REMDESK_CHANNEL_CTL_SIZE + pdu.ctlHeader.DataLength);

	remdesk_write_ctl_header(s, &(pdu.ctlHeader));

	Stream_Write_UINT32(s, pdu.result); /* result (4 bytes) */

	Stream_SealLength(s);

	remdesk_virtual_channel_write(context, s);

	Stream_Free(s, TRUE);

	return 1;
}

static int remdesk_send_ctl_version_info_pdu(RemdeskServerContext* context)
{
	wStream* s;
	REMDESK_CTL_VERSION_INFO_PDU pdu;

	remdesk_prepare_ctl_header(&(pdu.ctlHeader), REMDESK_CTL_VERSIONINFO, 8);

	pdu.versionMajor = 1;
	pdu.versionMinor = 2;

	s = Stream_New(NULL, REMDESK_CHANNEL_CTL_SIZE + pdu.ctlHeader.DataLength);

	remdesk_write_ctl_header(s, &(pdu.ctlHeader));

	Stream_Write_UINT32(s, pdu.versionMajor); /* versionMajor (4 bytes) */
	Stream_Write_UINT32(s, pdu.versionMinor); /* versionMinor (4 bytes) */

	Stream_SealLength(s);

	remdesk_virtual_channel_write(context, s);

	Stream_Free(s, TRUE);

	return 1;
}

static int remdesk_recv_ctl_version_info_pdu(RemdeskServerContext* context, wStream* s, REMDESK_CHANNEL_HEADER* header)
{
	UINT32 versionMajor;
	UINT32 versionMinor;

	if (Stream_GetRemainingLength(s) < 8)
		return -1;

	Stream_Read_UINT32(s, versionMajor); /* versionMajor (4 bytes) */
	Stream_Read_UINT32(s, versionMinor); /* versionMinor (4 bytes) */

	return 1;
}

static int remdesk_recv_ctl_remote_control_desktop_pdu(RemdeskServerContext* context, wStream* s, REMDESK_CHANNEL_HEADER* header)
{
	int status;
	int cchStringW;
	WCHAR* pStringW;
	UINT32 msgLength;
	int cbRaConnectionStringW = 0;
	WCHAR* raConnectionStringW = NULL;
	REMDESK_CTL_REMOTE_CONTROL_DESKTOP_PDU pdu;

	msgLength = header->DataLength - 4;

	pStringW = (WCHAR*) Stream_Pointer(s);
	raConnectionStringW = pStringW;
	cchStringW = 0;

	while ((msgLength > 0) && pStringW[cchStringW])
	{
		msgLength -= 2;
		cchStringW++;
	}

	if (pStringW[cchStringW] || !cchStringW)
		return -1;

	cchStringW++;
	cbRaConnectionStringW = cchStringW * 2;

	pdu.raConnectionString = NULL;

	status = ConvertFromUnicode(CP_UTF8, 0, raConnectionStringW,
			cbRaConnectionStringW / 2, &pdu.raConnectionString, 0, NULL, NULL);

	if (status <= 0)
		return -1;

	WLog_INFO(TAG, "RaConnectionString: %s",
			  pdu.raConnectionString);
	free(pdu.raConnectionString);

	remdesk_send_ctl_result_pdu(context, 0);

	return 1;
}

static int remdesk_recv_ctl_authenticate_pdu(RemdeskServerContext* context, wStream* s, REMDESK_CHANNEL_HEADER* header)
{
	int status;
	int cchStringW;
	WCHAR* pStringW;
	UINT32 msgLength;
	int cbExpertBlobW = 0;
	WCHAR* expertBlobW = NULL;
	int cbRaConnectionStringW = 0;
	WCHAR* raConnectionStringW = NULL;
	REMDESK_CTL_AUTHENTICATE_PDU pdu;

	msgLength = header->DataLength - 4;

	pStringW = (WCHAR*) Stream_Pointer(s);
	raConnectionStringW = pStringW;
	cchStringW = 0;

	while ((msgLength > 0) && pStringW[cchStringW])
	{
		msgLength -= 2;
		cchStringW++;
	}

	if (pStringW[cchStringW] || !cchStringW)
		return -1;

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
		return -1;

	cchStringW++;
	cbExpertBlobW = cchStringW * 2;

	pdu.raConnectionString = NULL;

	status = ConvertFromUnicode(CP_UTF8, 0, raConnectionStringW,
			cbRaConnectionStringW / 2, &pdu.raConnectionString, 0, NULL, NULL);

	if (status <= 0)
		return -1;

	pdu.expertBlob = NULL;

	status = ConvertFromUnicode(CP_UTF8, 0, expertBlobW,
			cbExpertBlobW / 2, &pdu.expertBlob, 0, NULL, NULL);

	if (status <= 0)
		return -1;

	WLog_INFO(TAG, "RaConnectionString: %s ExpertBlob: %s",
			  pdu.raConnectionString, pdu.expertBlob);
	free(pdu.raConnectionString);
	free(pdu.expertBlob);

	return 1;
}

static int remdesk_recv_ctl_verify_password_pdu(RemdeskServerContext* context, wStream* s, REMDESK_CHANNEL_HEADER* header)
{
	int status;
	int cbExpertBlobW = 0;
	WCHAR* expertBlobW = NULL;
	REMDESK_CTL_VERIFY_PASSWORD_PDU pdu;

	if (Stream_GetRemainingLength(s) < 8)
		return -1;

	pdu.expertBlob = NULL;
	expertBlobW = (WCHAR*) Stream_Pointer(s);
	cbExpertBlobW = header->DataLength - 4;

	status = ConvertFromUnicode(CP_UTF8, 0, expertBlobW, cbExpertBlobW / 2, &pdu.expertBlob, 0, NULL, NULL);
	WLog_INFO(TAG, "ExpertBlob: %s", pdu.expertBlob);
	remdesk_send_ctl_result_pdu(context, 0);

	return 1;
}

static int remdesk_recv_ctl_pdu(RemdeskServerContext* context, wStream* s, REMDESK_CHANNEL_HEADER* header)
{
	int status = 1;
	UINT32 msgType = 0;

	if (Stream_GetRemainingLength(s) < 4)
		return -1;

	Stream_Read_UINT32(s, msgType); /* msgType (4 bytes) */
	WLog_INFO(TAG, "msgType: %d", msgType);

	switch (msgType)
	{
		case REMDESK_CTL_REMOTE_CONTROL_DESKTOP:
			status = remdesk_recv_ctl_remote_control_desktop_pdu(context, s, header);
			break;

		case REMDESK_CTL_AUTHENTICATE:
			status = remdesk_recv_ctl_authenticate_pdu(context, s, header);
			break;

		case REMDESK_CTL_DISCONNECT:
			break;

		case REMDESK_CTL_VERSIONINFO:
			status = remdesk_recv_ctl_version_info_pdu(context, s, header);
			break;

		case REMDESK_CTL_ISCONNECTED:
			break;

		case REMDESK_CTL_VERIFY_PASSWORD:
			status = remdesk_recv_ctl_verify_password_pdu(context, s, header);
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
			WLog_ERR(TAG, "remdesk_recv_control_pdu: unknown msgType: %d", msgType);
			status = -1;
			break;
	}

	return status;
}

static int remdesk_server_receive_pdu(RemdeskServerContext* context, wStream* s)
{
	int status = 1;
	REMDESK_CHANNEL_HEADER header;

#if 0
	WLog_INFO(TAG, "RemdeskReceive: %d", Stream_GetRemainingLength(s));
	winpr_HexDump(Stream_Pointer(s), Stream_GetRemainingLength(s));
#endif

	if (remdesk_read_channel_header(s, &header) < 0)
		return -1;

	if (strcmp(header.ChannelName, "RC_CTL") == 0)
	{
		status = remdesk_recv_ctl_pdu(context, s, &header);
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

	return 1;
}

static void* remdesk_server_thread(void* arg)
{
	wStream* s;
	DWORD status;
	DWORD nCount;
	void* buffer;
	UINT32* pHeader;
	UINT32 PduLength;
	HANDLE events[8];
	HANDLE ChannelEvent;
	DWORD BytesReturned;
	RemdeskServerContext* context;

	context = (RemdeskServerContext*) arg;

	buffer = NULL;
	BytesReturned = 0;
	ChannelEvent = NULL;

	s = Stream_New(NULL, 4096);

	if (WTSVirtualChannelQuery(context->priv->ChannelHandle, WTSVirtualEventHandle, &buffer, &BytesReturned) == TRUE)
	{
		if (BytesReturned == sizeof(HANDLE))
			CopyMemory(&ChannelEvent, buffer, sizeof(HANDLE));

		WTSFreeMemory(buffer);
	}

	nCount = 0;
	events[nCount++] = ChannelEvent;
	events[nCount++] = context->priv->StopEvent;

	remdesk_send_ctl_version_info_pdu(context);

	while (1)
	{
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (WaitForSingleObject(context->priv->StopEvent, 0) == WAIT_OBJECT_0)
		{
			break;
		}

		if (WTSVirtualChannelRead(context->priv->ChannelHandle, 0,
				(PCHAR) Stream_Buffer(s), Stream_Capacity(s), &BytesReturned))
		{
			if (BytesReturned)
				Stream_Seek(s, BytesReturned);
		}
		else
		{
			Stream_EnsureRemainingCapacity(s, BytesReturned);
		}

		if (Stream_GetPosition(s) >= 8)
		{
			pHeader = (UINT32*) Stream_Buffer(s);
			PduLength = pHeader[0] + pHeader[1] + 8;

			if (PduLength >= Stream_GetPosition(s))
			{
				Stream_SealLength(s);
				Stream_SetPosition(s, 0);
				remdesk_server_receive_pdu(context, s);
				Stream_SetPosition(s, 0);
			}
		}
	}

	Stream_Free(s, TRUE);

	return NULL;
}

static int remdesk_server_start(RemdeskServerContext* context)
{
	context->priv->ChannelHandle = WTSVirtualChannelOpen(context->vcm, WTS_CURRENT_SESSION, "remdesk");

	if (!context->priv->ChannelHandle)
		return -1;

	context->priv->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	context->priv->Thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) remdesk_server_thread, (void*) context, 0, NULL);

	return 1;
}

static int remdesk_server_stop(RemdeskServerContext* context)
{
	SetEvent(context->priv->StopEvent);

	WaitForSingleObject(context->priv->Thread, INFINITE);
	CloseHandle(context->priv->Thread);

	return 1;
}

RemdeskServerContext* remdesk_server_context_new(HANDLE vcm)
{
	RemdeskServerContext* context;

	context = (RemdeskServerContext*) calloc(1, sizeof(RemdeskServerContext));

	if (context)
	{
		context->vcm = vcm;

		context->Start = remdesk_server_start;
		context->Stop = remdesk_server_stop;

		context->priv = (RemdeskServerPrivate*) calloc(1, sizeof(RemdeskServerPrivate));

		if (context->priv)
		{
			context->priv->Version = 1;
		}
	}

	return context;
}

void remdesk_server_context_free(RemdeskServerContext* context)
{
	if (context)
	{
		free(context->priv);
		free(context);
	}
}
