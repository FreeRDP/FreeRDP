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

#include <freerdp/assistance.h>

#include <freerdp/channels/log.h>
#include <freerdp/client/remdesk.h>

#include "remdesk_main.h"

RemdeskClientContext* remdesk_get_client_interface(remdeskPlugin* remdesk)
{
	RemdeskClientContext* pInterface;
	pInterface = (RemdeskClientContext*) remdesk->channelEntryPoints.pInterface;
	return pInterface;
}

int remdesk_virtual_channel_write(remdeskPlugin* remdesk, wStream* s)
{
	UINT32 status = 0;

	if (!remdesk)
		return -1;

	status = remdesk->channelEntryPoints.pVirtualChannelWrite(remdesk->OpenHandle,
			Stream_Buffer(s), (UINT32) Stream_Length(s), s);

	if (status != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG,  "VirtualChannelWrite failed with %s [%08X]",
				 WTSErrorToString(status), status);
		return -1;
	}

	return 1;
}

int remdesk_generate_expert_blob(remdeskPlugin* remdesk)
{
	char* name;
	char* pass;
	char* password;
	rdpSettings* settings = remdesk->settings;

	if (remdesk->ExpertBlob)
		return 1;

	if (settings->RemoteAssistancePassword)
		password = settings->RemoteAssistancePassword;
	else
		password = settings->Password;

	if (!password)
		return -1;

	name = settings->Username;

	if (!name)
		name = "Expert";

	remdesk->EncryptedPassStub = freerdp_assistance_encrypt_pass_stub(password,
			settings->RemoteAssistancePassStub, &(remdesk->EncryptedPassStubSize));

	if (!remdesk->EncryptedPassStub)
		return -1;

	pass = freerdp_assistance_bin_to_hex_string(remdesk->EncryptedPassStub, remdesk->EncryptedPassStubSize);

	if (!pass)
		return -1;

	remdesk->ExpertBlob = freerdp_assistance_construct_expert_blob(name, pass);

	if (!remdesk->ExpertBlob)
		return -1;

	return 1;
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

static int remdesk_recv_ctl_server_announce_pdu(remdeskPlugin* remdesk, wStream* s, REMDESK_CHANNEL_HEADER* header)
{
	return 1;
}

static int remdesk_recv_ctl_version_info_pdu(remdeskPlugin* remdesk, wStream* s, REMDESK_CHANNEL_HEADER* header)
{
	UINT32 versionMajor;
	UINT32 versionMinor;

	if (Stream_GetRemainingLength(s) < 8)
		return -1;

	Stream_Read_UINT32(s, versionMajor); /* versionMajor (4 bytes) */
	Stream_Read_UINT32(s, versionMinor); /* versionMinor (4 bytes) */

	return 1;
}

static int remdesk_send_ctl_version_info_pdu(remdeskPlugin* remdesk)
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

	remdesk_virtual_channel_write(remdesk, s);

	return 1;
}

static int remdesk_recv_ctl_result_pdu(remdeskPlugin* remdesk, wStream* s, REMDESK_CHANNEL_HEADER* header, UINT32 *pResult)
{
	UINT32 result;

	if (Stream_GetRemainingLength(s) < 4)
		return -1;

	Stream_Read_UINT32(s, result); /* result (4 bytes) */

	*pResult = result;
	//WLog_DBG(TAG, "RemdeskRecvResult: 0x%04X", result);
	return 1;
}

static int remdesk_send_ctl_authenticate_pdu(remdeskPlugin* remdesk)
{
	int status;
	wStream* s;
	int cbExpertBlobW = 0;
	WCHAR* expertBlobW = NULL;
	int cbRaConnectionStringW = 0;
	WCHAR* raConnectionStringW = NULL;
	REMDESK_CTL_AUTHENTICATE_PDU pdu;

	status = remdesk_generate_expert_blob(remdesk);

	if (status < 0)
		return -1;

	pdu.expertBlob = remdesk->ExpertBlob;
	pdu.raConnectionString = remdesk->settings->RemoteAssistanceRCTicket;

	status = ConvertToUnicode(CP_UTF8, 0, pdu.raConnectionString, -1, &raConnectionStringW, 0);

	if (status <= 0)
		return -1;

	cbRaConnectionStringW = status * 2;

	status = ConvertToUnicode(CP_UTF8, 0, pdu.expertBlob, -1, &expertBlobW, 0);

	if (status <= 0)
		return -1;

	cbExpertBlobW = status * 2;

	remdesk_prepare_ctl_header(&(pdu.ctlHeader), REMDESK_CTL_AUTHENTICATE,
			cbRaConnectionStringW + cbExpertBlobW);

	s = Stream_New(NULL, REMDESK_CHANNEL_CTL_SIZE + pdu.ctlHeader.DataLength);

	remdesk_write_ctl_header(s, &(pdu.ctlHeader));

	Stream_Write(s, (BYTE*) raConnectionStringW, cbRaConnectionStringW);
	Stream_Write(s, (BYTE*) expertBlobW, cbExpertBlobW);

	Stream_SealLength(s);

	remdesk_virtual_channel_write(remdesk, s);

	free(raConnectionStringW);
	free(expertBlobW);

	return 1;
}

static int remdesk_send_ctl_remote_control_desktop_pdu(remdeskPlugin* remdesk)
{
	int status;
	wStream* s;
	int cbRaConnectionStringW = 0;
	WCHAR* raConnectionStringW = NULL;
	REMDESK_CTL_REMOTE_CONTROL_DESKTOP_PDU pdu;

	pdu.raConnectionString = remdesk->settings->RemoteAssistanceRCTicket;

	status = ConvertToUnicode(CP_UTF8, 0, pdu.raConnectionString, -1, &raConnectionStringW, 0);

	if (status <= 0)
		return -1;

	cbRaConnectionStringW = status * 2;

	remdesk_prepare_ctl_header(&(pdu.ctlHeader), REMDESK_CTL_REMOTE_CONTROL_DESKTOP, cbRaConnectionStringW);

	s = Stream_New(NULL, REMDESK_CHANNEL_CTL_SIZE + pdu.ctlHeader.DataLength);

	remdesk_write_ctl_header(s, &(pdu.ctlHeader));

	Stream_Write(s, (BYTE*) raConnectionStringW, cbRaConnectionStringW);

	Stream_SealLength(s);

	remdesk_virtual_channel_write(remdesk, s);

	free(raConnectionStringW);

	return 1;
}

static int remdesk_send_ctl_verify_password_pdu(remdeskPlugin* remdesk)
{
	int status;
	wStream* s;
	int cbExpertBlobW = 0;
	WCHAR* expertBlobW = NULL;
	REMDESK_CTL_VERIFY_PASSWORD_PDU pdu;

	status = remdesk_generate_expert_blob(remdesk);

	if (status < 0)
		return -1;

	pdu.expertBlob = remdesk->ExpertBlob;

	status = ConvertToUnicode(CP_UTF8, 0, pdu.expertBlob, -1, &expertBlobW, 0);

	if (status <= 0)
		return -1;

	cbExpertBlobW = status * 2;

	remdesk_prepare_ctl_header(&(pdu.ctlHeader), REMDESK_CTL_VERIFY_PASSWORD, cbExpertBlobW);

	s = Stream_New(NULL, REMDESK_CHANNEL_CTL_SIZE + pdu.ctlHeader.DataLength);

	remdesk_write_ctl_header(s, &(pdu.ctlHeader));

	Stream_Write(s, (BYTE*) expertBlobW, cbExpertBlobW);

	Stream_SealLength(s);

	remdesk_virtual_channel_write(remdesk, s);

	free(expertBlobW);

	return 1;
}

static int remdesk_send_ctl_expert_on_vista_pdu(remdeskPlugin* remdesk)
{
	int status;
	wStream* s;
	REMDESK_CTL_EXPERT_ON_VISTA_PDU pdu;

	status = remdesk_generate_expert_blob(remdesk);

	if (status < 0)
		return -1;

	pdu.EncryptedPasswordLength = remdesk->EncryptedPassStubSize;
	pdu.EncryptedPassword = remdesk->EncryptedPassStub;

	remdesk_prepare_ctl_header(&(pdu.ctlHeader), REMDESK_CTL_EXPERT_ON_VISTA,
			pdu.EncryptedPasswordLength);

	s = Stream_New(NULL, REMDESK_CHANNEL_CTL_SIZE + pdu.ctlHeader.DataLength);

	remdesk_write_ctl_header(s, &(pdu.ctlHeader));

	Stream_Write(s, pdu.EncryptedPassword, pdu.EncryptedPasswordLength);

	Stream_SealLength(s);

	remdesk_virtual_channel_write(remdesk, s);

	return 1;
}

static int remdesk_recv_ctl_pdu(remdeskPlugin* remdesk, wStream* s, REMDESK_CHANNEL_HEADER* header)
{
	int status = 1;
	UINT32 msgType = 0;
	UINT32 result = 0;

	if (Stream_GetRemainingLength(s) < 4)
		return -1;

	Stream_Read_UINT32(s, msgType); /* msgType (4 bytes) */

	//WLog_DBG(TAG, "msgType: %d", msgType);

	switch (msgType)
	{
		case REMDESK_CTL_REMOTE_CONTROL_DESKTOP:
			break;

		case REMDESK_CTL_RESULT:
			status = remdesk_recv_ctl_result_pdu(remdesk, s, header, &result);
			break;

		case REMDESK_CTL_AUTHENTICATE:
			break;

		case REMDESK_CTL_SERVER_ANNOUNCE:
			status = remdesk_recv_ctl_server_announce_pdu(remdesk, s, header);
			break;

		case REMDESK_CTL_DISCONNECT:
			break;

		case REMDESK_CTL_VERSIONINFO:
			status = remdesk_recv_ctl_version_info_pdu(remdesk, s, header);

			if (remdesk->Version == 1)
			{
				if (status >= 0)
					status = remdesk_send_ctl_version_info_pdu(remdesk);

				if (status >= 0)
					status = remdesk_send_ctl_authenticate_pdu(remdesk);

				if (status >= 0)
					status = remdesk_send_ctl_remote_control_desktop_pdu(remdesk);
			}
			else if (remdesk->Version == 2)
			{
				if (status >= 0)
					status = remdesk_send_ctl_expert_on_vista_pdu(remdesk);

				if (status >= 0)
					status = remdesk_send_ctl_verify_password_pdu(remdesk);
			}

			break;

		case REMDESK_CTL_ISCONNECTED:
			break;

		case REMDESK_CTL_VERIFY_PASSWORD:
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
			WLog_ERR(TAG,  "unknown msgType: %d", msgType);
			status = -1;
			break;
	}

	return status;
}

static int remdesk_process_receive(remdeskPlugin* remdesk, wStream* s)
{
	int status = 1;
	REMDESK_CHANNEL_HEADER header;

#if 0
	WLog_DBG(TAG, "RemdeskReceive: %d", Stream_GetRemainingLength(s));
	winpr_HexDump(Stream_Pointer(s), Stream_GetRemainingLength(s));
#endif

	remdesk_read_channel_header(s, &header);

	if (strcmp(header.ChannelName, "RC_CTL") == 0)
	{
		status = remdesk_recv_ctl_pdu(remdesk, s, &header);
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

	return status;
}

static void remdesk_process_connect(remdeskPlugin* remdesk)
{
	remdesk->settings = (rdpSettings*) remdesk->channelEntryPoints.pExtendedData;
}

/****************************************************************************************/

static wListDictionary* g_InitHandles = NULL;
static wListDictionary* g_OpenHandles = NULL;

void remdesk_add_init_handle_data(void* pInitHandle, void* pUserData)
{
	if (!g_InitHandles)
		g_InitHandles = ListDictionary_New(TRUE);

	ListDictionary_Add(g_InitHandles, pInitHandle, pUserData);
}

void* remdesk_get_init_handle_data(void* pInitHandle)
{
	void* pUserData = NULL;
	pUserData = ListDictionary_GetItemValue(g_InitHandles, pInitHandle);
	return pUserData;
}

void remdesk_remove_init_handle_data(void* pInitHandle)
{
	ListDictionary_Remove(g_InitHandles, pInitHandle);
	if (ListDictionary_Count(g_InitHandles) < 1)
	{
		ListDictionary_Free(g_InitHandles);
		g_InitHandles = NULL;
	}
}

void remdesk_add_open_handle_data(DWORD openHandle, void* pUserData)
{
	void* pOpenHandle = (void*) (size_t) openHandle;

	if (!g_OpenHandles)
		g_OpenHandles = ListDictionary_New(TRUE);

	ListDictionary_Add(g_OpenHandles, pOpenHandle, pUserData);
}

void* remdesk_get_open_handle_data(DWORD openHandle)
{
	void* pUserData = NULL;
	void* pOpenHandle = (void*) (size_t) openHandle;
	pUserData = ListDictionary_GetItemValue(g_OpenHandles, pOpenHandle);
	return pUserData;
}

void remdesk_remove_open_handle_data(DWORD openHandle)
{
	void* pOpenHandle = (void*) (size_t) openHandle;
	ListDictionary_Remove(g_OpenHandles, pOpenHandle);
	if (ListDictionary_Count(g_OpenHandles) < 1)
	{
		ListDictionary_Free(g_OpenHandles);
		g_OpenHandles = NULL;
	}
}

int remdesk_send(remdeskPlugin* remdesk, wStream* s)
{
	UINT32 status = 0;
	remdeskPlugin* plugin = (remdeskPlugin*) remdesk;

	if (!plugin)
	{
		status = CHANNEL_RC_BAD_INIT_HANDLE;
	}
	else
	{
		status = plugin->channelEntryPoints.pVirtualChannelWrite(plugin->OpenHandle,
			Stream_Buffer(s), (UINT32) Stream_GetPosition(s), s);
	}

	if (status != CHANNEL_RC_OK)
	{
		Stream_Free(s, TRUE);
		WLog_ERR(TAG,  "VirtualChannelWrite failed with %s [%08X]",
				 WTSErrorToString(status), status);
	}

	return status;
}

static void remdesk_virtual_channel_event_data_received(remdeskPlugin* remdesk,
		void* pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	wStream* data_in;

	if ((dataFlags & CHANNEL_FLAG_SUSPEND) || (dataFlags & CHANNEL_FLAG_RESUME))
	{
		return;
	}

	if (dataFlags & CHANNEL_FLAG_FIRST)
	{
		if (remdesk->data_in)
			Stream_Free(remdesk->data_in, TRUE);

		remdesk->data_in = Stream_New(NULL, totalLength);
	}

	data_in = remdesk->data_in;
	Stream_EnsureRemainingCapacity(data_in, (int) dataLength);
	Stream_Write(data_in, pData, dataLength);

	if (dataFlags & CHANNEL_FLAG_LAST)
	{
		if (Stream_Capacity(data_in) != Stream_GetPosition(data_in))
		{
			WLog_ERR(TAG,  "read error");
		}

		remdesk->data_in = NULL;
		Stream_SealLength(data_in);
		Stream_SetPosition(data_in, 0);

		MessageQueue_Post(remdesk->queue, NULL, 0, (void*) data_in, NULL);
	}
}

static VOID VCAPITYPE remdesk_virtual_channel_open_event(DWORD openHandle, UINT event,
		LPVOID pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	remdeskPlugin* remdesk;

	remdesk = (remdeskPlugin*) remdesk_get_open_handle_data(openHandle);

	if (!remdesk)
	{
		WLog_ERR(TAG,  "error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_DATA_RECEIVED:
			remdesk_virtual_channel_event_data_received(remdesk, pData, dataLength, totalLength, dataFlags);
			break;

		case CHANNEL_EVENT_WRITE_COMPLETE:
			Stream_Free((wStream*) pData, TRUE);
			break;

		case CHANNEL_EVENT_USER:
			break;
	}
}

static void* remdesk_virtual_channel_client_thread(void* arg)
{
	wStream* data;
	wMessage message;
	remdeskPlugin* remdesk = (remdeskPlugin*) arg;

	remdesk_process_connect(remdesk);

	while (1)
	{
		if (!MessageQueue_Wait(remdesk->queue))
			break;

		if (MessageQueue_Peek(remdesk->queue, &message, TRUE))
		{
			if (message.id == WMQ_QUIT)
				break;

			if (message.id == 0)
			{
				data = (wStream*) message.wParam;
				remdesk_process_receive(remdesk, data);
			}
		}
	}

	ExitThread(0);
	return NULL;
}

static void remdesk_virtual_channel_event_connected(remdeskPlugin* remdesk, LPVOID pData, UINT32 dataLength)
{
	UINT32 status;

	status = remdesk->channelEntryPoints.pVirtualChannelOpen(remdesk->InitHandle,
		&remdesk->OpenHandle, remdesk->channelDef.name, remdesk_virtual_channel_open_event);

	remdesk_add_open_handle_data(remdesk->OpenHandle, remdesk);

	if (status != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG,  "pVirtualChannelOpen failed with %s [%08X]",
				 WTSErrorToString(status), status);
		return;
	}

	remdesk->queue = MessageQueue_New(NULL);

	remdesk->thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) remdesk_virtual_channel_client_thread, (void*) remdesk, 0, NULL);
}

static void remdesk_virtual_channel_event_disconnected(remdeskPlugin* remdesk)
{
	UINT rc;

	MessageQueue_PostQuit(remdesk->queue, 0);
	WaitForSingleObject(remdesk->thread, INFINITE);

	MessageQueue_Free(remdesk->queue);
	CloseHandle(remdesk->thread);

	remdesk->queue = NULL;
	remdesk->thread = NULL;

	rc = remdesk->channelEntryPoints.pVirtualChannelClose(remdesk->OpenHandle);
	if (CHANNEL_RC_OK != rc)
	{
		WLog_ERR(TAG, "pVirtualChannelClose failed with %s [%08X]",
				 WTSErrorToString(rc), rc);
	}

	if (remdesk->data_in)
	{
		Stream_Free(remdesk->data_in, TRUE);
		remdesk->data_in = NULL;
	}

	remdesk_remove_open_handle_data(remdesk->OpenHandle);
}

static void remdesk_virtual_channel_event_terminated(remdeskPlugin* remdesk)
{
	remdesk_remove_init_handle_data(remdesk->InitHandle);

	free(remdesk);
}

static VOID VCAPITYPE remdesk_virtual_channel_init_event(LPVOID pInitHandle, UINT event, LPVOID pData, UINT dataLength)
{
	remdeskPlugin* remdesk;

	remdesk = (remdeskPlugin*) remdesk_get_init_handle_data(pInitHandle);

	if (!remdesk)
	{
		WLog_ERR(TAG,  "error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_CONNECTED:
			remdesk_virtual_channel_event_connected(remdesk, pData, dataLength);
			break;

		case CHANNEL_EVENT_DISCONNECTED:
			remdesk_virtual_channel_event_disconnected(remdesk);
			break;

		case CHANNEL_EVENT_TERMINATED:
			remdesk_virtual_channel_event_terminated(remdesk);
			break;
	}
}

/* remdesk is always built-in */
#define VirtualChannelEntry	remdesk_VirtualChannelEntry

BOOL VCAPITYPE VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints)
{
	UINT rc;

	remdeskPlugin* remdesk;
	RemdeskClientContext* context;
	CHANNEL_ENTRY_POINTS_FREERDP* pEntryPointsEx;

	remdesk = (remdeskPlugin*) calloc(1, sizeof(remdeskPlugin));

	if (!remdesk)
		return FALSE;

	remdesk->channelDef.options =
			CHANNEL_OPTION_INITIALIZED |
			CHANNEL_OPTION_ENCRYPT_RDP |
			CHANNEL_OPTION_COMPRESS_RDP |
			CHANNEL_OPTION_SHOW_PROTOCOL;

	strcpy(remdesk->channelDef.name, "remdesk");

	remdesk->Version = 2;

	pEntryPointsEx = (CHANNEL_ENTRY_POINTS_FREERDP*) pEntryPoints;

	if ((pEntryPointsEx->cbSize >= sizeof(CHANNEL_ENTRY_POINTS_FREERDP)) &&
			(pEntryPointsEx->MagicNumber == FREERDP_CHANNEL_MAGIC_NUMBER))
	{
		context = (RemdeskClientContext*) calloc(1, sizeof(RemdeskClientContext));

		context->handle = (void*) remdesk;

		*(pEntryPointsEx->ppInterface) = (void*) context;
		remdesk->context = context;
	}

	CopyMemory(&(remdesk->channelEntryPoints), pEntryPoints, sizeof(CHANNEL_ENTRY_POINTS_FREERDP));

	rc = remdesk->channelEntryPoints.pVirtualChannelInit(&remdesk->InitHandle,
		&remdesk->channelDef, 1, VIRTUAL_CHANNEL_VERSION_WIN2000, remdesk_virtual_channel_init_event);
	if (CHANNEL_RC_OK != rc)
	{
		WLog_ERR(TAG, "pVirtualChannelInit failed with %s [%08X]",
				 WTSErrorToString(rc), rc);
		free(remdesk);
		return -1;
	}

	remdesk->channelEntryPoints.pInterface = *(remdesk->channelEntryPoints.ppInterface);
	remdesk->channelEntryPoints.ppInterface = &(remdesk->channelEntryPoints.pInterface);

	remdesk_add_init_handle_data(remdesk->InitHandle, (void*) remdesk);

	return 1;
}
