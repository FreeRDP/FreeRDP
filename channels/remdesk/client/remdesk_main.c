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

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_virtual_channel_write(remdeskPlugin* remdesk, wStream* s)
{
	UINT32 status;

	if (!remdesk)
	{
		WLog_ERR(TAG, "remdesk was null!");
		return CHANNEL_RC_INVALID_INSTANCE;
	}

	status = remdesk->channelEntryPoints.pVirtualChannelWrite(remdesk->OpenHandle,
			Stream_Buffer(s), (UINT32) Stream_Length(s), s);

	if (status != CHANNEL_RC_OK)
		WLog_ERR(TAG,  "VirtualChannelWrite failed with %s [%08X]",
				 WTSErrorToString(status), status);

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT remdesk_generate_expert_blob(remdeskPlugin* remdesk)
{
	char* name;
	char* pass;
	char* password;
	rdpSettings* settings = remdesk->settings;

	if (remdesk->ExpertBlob)
		return CHANNEL_RC_OK;

	if (settings->RemoteAssistancePassword)
		password = settings->RemoteAssistancePassword;
	else
		password = settings->Password;

	if (!password)
	{
		WLog_ERR(TAG, "password was not set!");
		return ERROR_INTERNAL_ERROR;
	}

	name = settings->Username;

	if (!name)
		name = "Expert";

	remdesk->EncryptedPassStub = freerdp_assistance_encrypt_pass_stub(password,
			settings->RemoteAssistancePassStub, &(remdesk->EncryptedPassStubSize));

	if (!remdesk->EncryptedPassStub)
	{
		WLog_ERR(TAG, "freerdp_assistance_encrypt_pass_stub failed!");
		return ERROR_INTERNAL_ERROR;
	}

	pass = freerdp_assistance_bin_to_hex_string(remdesk->EncryptedPassStub, remdesk->EncryptedPassStubSize);

	if (!pass)
	{
		WLog_ERR(TAG, "freerdp_assistance_bin_to_hex_string failed!");
		return ERROR_INTERNAL_ERROR;
	}

	remdesk->ExpertBlob = freerdp_assistance_construct_expert_blob(name, pass);

	if (!remdesk->ExpertBlob)
	{
		WLog_ERR(TAG, "freerdp_assistance_construct_expert_blob failed!");
		return ERROR_INTERNAL_ERROR;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_read_channel_header(wStream* s, REMDESK_CHANNEL_HEADER* header)
{
	int status;
	UINT32 ChannelNameLen;
	char* pChannelName = NULL;

	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_ERR(TAG, "Not enought data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, ChannelNameLen); /* ChannelNameLen (4 bytes) */
	Stream_Read_UINT32(s, header->DataLength); /* DataLen (4 bytes) */

	if (ChannelNameLen > 64)
	{
		WLog_ERR(TAG, "ChannelNameLen > 64!");
		return ERROR_INVALID_DATA;
	}

	if ((ChannelNameLen % 2) != 0)
	{
		WLog_ERR(TAG, "ChannelNameLen  % 2) != 0 ");
		return ERROR_INVALID_DATA;
	}

	if (Stream_GetRemainingLength(s) < ChannelNameLen)
	{
		WLog_ERR(TAG, "Not enought data!");
		return ERROR_INVALID_DATA;
	}

	ZeroMemory(header->ChannelName, sizeof(header->ChannelName));

	pChannelName = (char*) header->ChannelName;
	status = ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) Stream_Pointer(s),
			ChannelNameLen / 2, &pChannelName, 32, NULL, NULL);

	Stream_Seek(s, ChannelNameLen);

	if (status <= 0)
	{
		WLog_ERR(TAG, "ConvertFromUnicode failed!");
		return ERROR_INTERNAL_ERROR;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_write_channel_header(wStream* s, REMDESK_CHANNEL_HEADER* header)
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

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_write_ctl_header(wStream* s, REMDESK_CTL_HEADER* ctlHeader)
{
	remdesk_write_channel_header(s, (REMDESK_CHANNEL_HEADER*) ctlHeader);
	Stream_Write_UINT32(s, ctlHeader->msgType); /* msgType (4 bytes) */
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_prepare_ctl_header(REMDESK_CTL_HEADER* ctlHeader, UINT32 msgType, UINT32 msgSize)
{
	ctlHeader->msgType = msgType;
	strcpy(ctlHeader->ChannelName, REMDESK_CHANNEL_CTL_NAME);
	ctlHeader->DataLength = 4 + msgSize;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_recv_ctl_server_announce_pdu(remdeskPlugin* remdesk, wStream* s, REMDESK_CHANNEL_HEADER* header)
{
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_recv_ctl_version_info_pdu(remdeskPlugin* remdesk, wStream* s, REMDESK_CHANNEL_HEADER* header)
{
	UINT32 versionMajor;
	UINT32 versionMinor;

	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_ERR(TAG, "Not enought data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, versionMajor); /* versionMajor (4 bytes) */
	Stream_Read_UINT32(s, versionMinor); /* versionMinor (4 bytes) */

	remdesk->Version = versionMajor;

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_send_ctl_version_info_pdu(remdeskPlugin* remdesk)
{
	wStream* s;
	REMDESK_CTL_VERSION_INFO_PDU pdu;
	UINT  error;

	remdesk_prepare_ctl_header(&(pdu.ctlHeader), REMDESK_CTL_VERSIONINFO, 8);

	pdu.versionMajor = 1;
	pdu.versionMinor = 2;

	s = Stream_New(NULL, REMDESK_CHANNEL_CTL_SIZE + pdu.ctlHeader.DataLength);
	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	remdesk_write_ctl_header(s, &(pdu.ctlHeader));

	Stream_Write_UINT32(s, pdu.versionMajor); /* versionMajor (4 bytes) */
	Stream_Write_UINT32(s, pdu.versionMinor); /* versionMinor (4 bytes) */

	Stream_SealLength(s);

	if ((error = remdesk_virtual_channel_write(remdesk, s)))
		WLog_ERR(TAG, "remdesk_virtual_channel_write failed with error %lu!", error);

	if (error != CHANNEL_RC_OK)
		Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_recv_ctl_result_pdu(remdeskPlugin* remdesk, wStream* s, REMDESK_CHANNEL_HEADER* header, UINT32 *pResult)
{
	UINT32 result;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "Not enought data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, result); /* result (4 bytes) */

	*pResult = result;
	//WLog_DBG(TAG, "RemdeskRecvResult: 0x%04X", result);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_send_ctl_authenticate_pdu(remdeskPlugin* remdesk)
{
	int status;
	UINT error;
	wStream* s = NULL;
	int cbExpertBlobW = 0;
	WCHAR* expertBlobW = NULL;
	int cbRaConnectionStringW = 0;
	WCHAR* raConnectionStringW = NULL;
	REMDESK_CTL_AUTHENTICATE_PDU pdu;

	if ((error = remdesk_generate_expert_blob(remdesk)))
	{
		WLog_ERR(TAG, "remdesk_generate_expert_blob failed with error %lu", error);
		return error;
	}

	pdu.expertBlob = remdesk->ExpertBlob;
	pdu.raConnectionString = remdesk->settings->RemoteAssistanceRCTicket;

	status = ConvertToUnicode(CP_UTF8, 0, pdu.raConnectionString, -1, &raConnectionStringW, 0);

	if (status <= 0)
	{
		WLog_ERR(TAG, "ConvertToUnicode failed!");
		return ERROR_INTERNAL_ERROR;
	}

	cbRaConnectionStringW = status * 2;

	status = ConvertToUnicode(CP_UTF8, 0, pdu.expertBlob, -1, &expertBlobW, 0);

	if (status <= 0)
	{
		WLog_ERR(TAG, "ConvertToUnicode failed!");
		error = ERROR_INTERNAL_ERROR;
		goto out;
	}

	cbExpertBlobW = status * 2;

	remdesk_prepare_ctl_header(&(pdu.ctlHeader), REMDESK_CTL_AUTHENTICATE,
			cbRaConnectionStringW + cbExpertBlobW);

	s = Stream_New(NULL, REMDESK_CHANNEL_CTL_SIZE + pdu.ctlHeader.DataLength);
	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto out;
	}

	remdesk_write_ctl_header(s, &(pdu.ctlHeader));

	Stream_Write(s, (BYTE*) raConnectionStringW, cbRaConnectionStringW);
	Stream_Write(s, (BYTE*) expertBlobW, cbExpertBlobW);

	Stream_SealLength(s);

	if ((error = remdesk_virtual_channel_write(remdesk, s)))
		WLog_ERR(TAG, "remdesk_virtual_channel_write failed with error %lu!", error);

out:
	free(raConnectionStringW);
	free(expertBlobW);
	if (error != CHANNEL_RC_OK)
		Stream_Free(s, TRUE);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_send_ctl_remote_control_desktop_pdu(remdeskPlugin* remdesk)
{
	int status;
	UINT error;
	wStream* s = NULL;
	int cbRaConnectionStringW = 0;
	WCHAR* raConnectionStringW = NULL;
	REMDESK_CTL_REMOTE_CONTROL_DESKTOP_PDU pdu;

	pdu.raConnectionString = remdesk->settings->RemoteAssistanceRCTicket;

	status = ConvertToUnicode(CP_UTF8, 0, pdu.raConnectionString, -1, &raConnectionStringW, 0);

	if (status <= 0)
	{
		WLog_ERR(TAG, "ConvertToUnicode failed!");
		return ERROR_INTERNAL_ERROR;
	}

	cbRaConnectionStringW = status * 2;

	remdesk_prepare_ctl_header(&(pdu.ctlHeader), REMDESK_CTL_REMOTE_CONTROL_DESKTOP, cbRaConnectionStringW);

	s = Stream_New(NULL, REMDESK_CHANNEL_CTL_SIZE + pdu.ctlHeader.DataLength);
	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto out;
	}

	remdesk_write_ctl_header(s, &(pdu.ctlHeader));

	Stream_Write(s, (BYTE*) raConnectionStringW, cbRaConnectionStringW);

	Stream_SealLength(s);

	if ((error = remdesk_virtual_channel_write(remdesk, s)))
		WLog_ERR(TAG, "remdesk_virtual_channel_write failed with error %lu!", error);

out:
	free(raConnectionStringW);
	if (error != CHANNEL_RC_OK)
		Stream_Free(s, TRUE);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_send_ctl_verify_password_pdu(remdeskPlugin* remdesk)
{
	int status;
	UINT error;
	wStream* s;
	int cbExpertBlobW = 0;
	WCHAR* expertBlobW = NULL;
	REMDESK_CTL_VERIFY_PASSWORD_PDU pdu;

	if ((error = remdesk_generate_expert_blob(remdesk)))
	{
		WLog_ERR(TAG, "remdesk_generate_expert_blob failed with error %lu!", error);
		return error;
	}

	pdu.expertBlob = remdesk->ExpertBlob;

	status = ConvertToUnicode(CP_UTF8, 0, pdu.expertBlob, -1, &expertBlobW, 0);

	if (status <= 0)
	{
		WLog_ERR(TAG, "ConvertToUnicode failed!");
		return ERROR_INTERNAL_ERROR;
	}

	cbExpertBlobW = status * 2;

	remdesk_prepare_ctl_header(&(pdu.ctlHeader), REMDESK_CTL_VERIFY_PASSWORD, cbExpertBlobW);

	s = Stream_New(NULL, REMDESK_CHANNEL_CTL_SIZE + pdu.ctlHeader.DataLength);
	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto out;
	}

	remdesk_write_ctl_header(s, &(pdu.ctlHeader));

	Stream_Write(s, (BYTE*) expertBlobW, cbExpertBlobW);

	Stream_SealLength(s);

	if ((error = remdesk_virtual_channel_write(remdesk, s)))
		WLog_ERR(TAG, "remdesk_virtual_channel_write failed with error %lu!", error);

out:
	free(expertBlobW);
	if (error != CHANNEL_RC_OK)
		Stream_Free(s, TRUE);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_send_ctl_expert_on_vista_pdu(remdeskPlugin* remdesk)
{
	UINT error;
	wStream* s;
	REMDESK_CTL_EXPERT_ON_VISTA_PDU pdu;

	if ((error = remdesk_generate_expert_blob(remdesk)))
	{
		WLog_ERR(TAG, "remdesk_generate_expert_blob failed with error %lu!", error);
		return error;
	}

	pdu.EncryptedPasswordLength = remdesk->EncryptedPassStubSize;
	pdu.EncryptedPassword = remdesk->EncryptedPassStub;

	remdesk_prepare_ctl_header(&(pdu.ctlHeader), REMDESK_CTL_EXPERT_ON_VISTA,
			pdu.EncryptedPasswordLength);

	s = Stream_New(NULL, REMDESK_CHANNEL_CTL_SIZE + pdu.ctlHeader.DataLength);
	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	remdesk_write_ctl_header(s, &(pdu.ctlHeader));

	Stream_Write(s, pdu.EncryptedPassword, pdu.EncryptedPasswordLength);

	Stream_SealLength(s);

	return remdesk_virtual_channel_write(remdesk, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_recv_ctl_pdu(remdeskPlugin* remdesk, wStream* s, REMDESK_CHANNEL_HEADER* header)
{
	UINT error = CHANNEL_RC_OK;
	UINT32 msgType = 0;
	UINT32 result = 0;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "Not enought data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, msgType); /* msgType (4 bytes) */

	//WLog_DBG(TAG, "msgType: %d", msgType);

	switch (msgType)
	{
		case REMDESK_CTL_REMOTE_CONTROL_DESKTOP:
			break;

		case REMDESK_CTL_RESULT:
			if ((error = remdesk_recv_ctl_result_pdu(remdesk, s, header, &result)))
				WLog_ERR(TAG, "remdesk_recv_ctl_result_pdu failed with error %lu", error);
			break;

		case REMDESK_CTL_AUTHENTICATE:
			break;

		case REMDESK_CTL_SERVER_ANNOUNCE:
			if ((error = remdesk_recv_ctl_server_announce_pdu(remdesk, s, header)))
				WLog_ERR(TAG, "remdesk_recv_ctl_server_announce_pdu failed with error %lu", error);
			break;

		case REMDESK_CTL_DISCONNECT:
			break;

		case REMDESK_CTL_VERSIONINFO:
			if ((error = remdesk_recv_ctl_version_info_pdu(remdesk, s, header)))
			{
				WLog_ERR(TAG, "remdesk_recv_ctl_version_info_pdu failed with error %lu", error);
				break;
			}

			if (remdesk->Version == 1)
			{
				if ((error = remdesk_send_ctl_version_info_pdu(remdesk)))
				{
					WLog_ERR(TAG, "remdesk_send_ctl_version_info_pdu failed with error %lu", error);
					break;
				}

				if ((error = remdesk_send_ctl_authenticate_pdu(remdesk)))
				{
					WLog_ERR(TAG, "remdesk_send_ctl_authenticate_pdu failed with error %lu", error);
					break;
				}

				if ((error = remdesk_send_ctl_remote_control_desktop_pdu(remdesk)))
				{
					WLog_ERR(TAG, "remdesk_send_ctl_remote_control_desktop_pdu failed with error %lu", error);
					break;
				}
			}
			else if (remdesk->Version == 2)
			{
				if ((error = remdesk_send_ctl_expert_on_vista_pdu(remdesk)))
				{
					WLog_ERR(TAG, "remdesk_send_ctl_expert_on_vista_pdu failed with error %lu", error);
					break;
				}

				if ((error = remdesk_send_ctl_verify_password_pdu(remdesk)))
				{
					WLog_ERR(TAG, "remdesk_send_ctl_verify_password_pdu failed with error %lu", error);
					break;
				}
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
static UINT remdesk_process_receive(remdeskPlugin* remdesk, wStream* s)
{
	UINT status;
	REMDESK_CHANNEL_HEADER header;

#if 0
	WLog_DBG(TAG, "RemdeskReceive: %d", Stream_GetRemainingLength(s));
	winpr_HexDump(Stream_Pointer(s), Stream_GetRemainingLength(s));
#endif

	if ((status = remdesk_read_channel_header(s, &header)))
	{
		WLog_ERR(TAG, "remdesk_read_channel_header failed with error %lu", status);
		return status;
	}

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

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT remdesk_add_init_handle_data(void* pInitHandle, void* pUserData)
{
	if (!g_InitHandles)
	{
		g_InitHandles = ListDictionary_New(TRUE);
		if (!g_InitHandles)
			return CHANNEL_RC_NO_MEMORY;
	}

	return ListDictionary_Add(g_InitHandles, pInitHandle, pUserData) ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
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

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT remdesk_add_open_handle_data(DWORD openHandle, void* pUserData)
{
	void* pOpenHandle = (void*) (size_t) openHandle;

	if (!g_OpenHandles)
	{
		g_OpenHandles = ListDictionary_New(TRUE);
		if (!g_OpenHandles)
			return CHANNEL_RC_NO_MEMORY;
	}

	return ListDictionary_Add(g_OpenHandles, pOpenHandle, pUserData) ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
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

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT remdesk_send(remdeskPlugin* remdesk, wStream* s)
{
	UINT status = 0;
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

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_virtual_channel_event_data_received(remdeskPlugin* remdesk,
		void* pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	wStream* data_in;

	if ((dataFlags & CHANNEL_FLAG_SUSPEND) || (dataFlags & CHANNEL_FLAG_RESUME))
	{
		return CHANNEL_RC_OK;
	}

	if (dataFlags & CHANNEL_FLAG_FIRST)
	{
		if (remdesk->data_in)
			Stream_Free(remdesk->data_in, TRUE);

		remdesk->data_in = Stream_New(NULL, totalLength);
		if (!remdesk->data_in)
		{
			WLog_ERR(TAG, "Stream_New failed!");
			return CHANNEL_RC_NO_MEMORY;
		}
	}

	data_in = remdesk->data_in;
	if (!Stream_EnsureRemainingCapacity(data_in, (int) dataLength))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return CHANNEL_RC_NO_MEMORY;
	}
	Stream_Write(data_in, pData, dataLength);

	if (dataFlags & CHANNEL_FLAG_LAST)
	{
		if (Stream_Capacity(data_in) != Stream_GetPosition(data_in))
		{
			WLog_ERR(TAG,  "read error");
			return ERROR_INTERNAL_ERROR;
		}

		remdesk->data_in = NULL;
		Stream_SealLength(data_in);
		Stream_SetPosition(data_in, 0);

		if (!MessageQueue_Post(remdesk->queue, NULL, 0, (void*) data_in, NULL))
		{
			WLog_ERR(TAG, "MessageQueue_Post failed!");
			return ERROR_INTERNAL_ERROR;
		}
	}
	return CHANNEL_RC_OK;
}

static VOID VCAPITYPE remdesk_virtual_channel_open_event(DWORD openHandle, UINT event,
		LPVOID pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	remdeskPlugin* remdesk;
	UINT error = CHANNEL_RC_OK;

	remdesk = (remdeskPlugin*) remdesk_get_open_handle_data(openHandle);

	if (!remdesk)
	{
		WLog_ERR(TAG,  "error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_DATA_RECEIVED:
			if ((error = remdesk_virtual_channel_event_data_received(remdesk, pData, dataLength, totalLength, dataFlags)))
				WLog_ERR(TAG, "remdesk_virtual_channel_event_data_received failed with error %lu!", error);
			break;

		case CHANNEL_EVENT_WRITE_COMPLETE:
			Stream_Free((wStream*) pData, TRUE);
			break;

		case CHANNEL_EVENT_USER:
			break;

		default:
			WLog_ERR(TAG, "unhandled event %lu!", event);
			error = ERROR_INTERNAL_ERROR;

	}
	if (error && remdesk->rdpcontext)
		setChannelError(remdesk->rdpcontext, error, "remdesk_virtual_channel_open_event reported an error");

}

static void* remdesk_virtual_channel_client_thread(void* arg)
{
	wStream* data;
	wMessage message;
	remdeskPlugin* remdesk = (remdeskPlugin*) arg;
	UINT error = CHANNEL_RC_OK;

	remdesk_process_connect(remdesk);

	while (1)
	{
		if (!MessageQueue_Wait(remdesk->queue))
		{
			WLog_ERR(TAG, "MessageQueue_Wait failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		if (!MessageQueue_Peek(remdesk->queue, &message, TRUE)) {
			WLog_ERR(TAG, "MessageQueue_Peek failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}
		if (message.id == WMQ_QUIT)
			break;

		if (message.id == 0)
		{
			data = (wStream*) message.wParam;
			if ((error = remdesk_process_receive(remdesk, data)))
			{
				WLog_ERR(TAG, "remdesk_process_receive failed with error %lu!", error);
				break;
			}
		}
	}

	if (error && remdesk->rdpcontext)
		setChannelError(remdesk->rdpcontext, error, "remdesk_virtual_channel_client_thread reported an error");

	ExitThread((DWORD)error);
	return NULL;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_virtual_channel_event_connected(remdeskPlugin* remdesk, LPVOID pData, UINT32 dataLength)
{
	UINT32 status;
	UINT error;

	status = remdesk->channelEntryPoints.pVirtualChannelOpen(remdesk->InitHandle,
		&remdesk->OpenHandle, remdesk->channelDef.name, remdesk_virtual_channel_open_event);

	if (status != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG,  "pVirtualChannelOpen failed with %s [%08X]",
				 WTSErrorToString(status), status);
		return status;
	}

	if ((error = remdesk_add_open_handle_data(remdesk->OpenHandle, remdesk)))
	{
		WLog_ERR(TAG, "remdesk_add_open_handle_data failed with error %lu", error);
		return error;
	}

	remdesk->queue = MessageQueue_New(NULL);
	if (!remdesk->queue)
	{
		WLog_ERR(TAG, "MessageQueue_New failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto error_out;
	}

	remdesk->thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) remdesk_virtual_channel_client_thread, (void*) remdesk, 0, NULL);
	if (!remdesk->thread)
	{
		WLog_ERR(TAG, "CreateThread failed");
		error = ERROR_INTERNAL_ERROR;
		goto error_out;
	}
	return CHANNEL_RC_OK;
error_out:
	remdesk_remove_open_handle_data(remdesk->OpenHandle);
	MessageQueue_Free(remdesk->queue);
	remdesk->queue = NULL;
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_virtual_channel_event_disconnected(remdeskPlugin* remdesk)
{
	UINT rc;

	if (MessageQueue_PostQuit(remdesk->queue, 0) && (WaitForSingleObject(remdesk->thread, INFINITE) == WAIT_FAILED))
	{
		rc = GetLastError();
		WLog_ERR(TAG, "WaitForSingleObject failed with error %lu", rc);
		return rc;
	}

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
	return rc;
}

static void remdesk_virtual_channel_event_terminated(remdeskPlugin* remdesk)
{
	remdesk_remove_init_handle_data(remdesk->InitHandle);

	free(remdesk);
}

static VOID VCAPITYPE remdesk_virtual_channel_init_event(LPVOID pInitHandle,
							 UINT event, LPVOID pData,
							 UINT dataLength)
{
	remdeskPlugin* remdesk;
	UINT error = CHANNEL_RC_OK;

	remdesk = (remdeskPlugin*) remdesk_get_init_handle_data(pInitHandle);

	if (!remdesk)
	{
		WLog_ERR(TAG,  "error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_CONNECTED:
			if ((error = remdesk_virtual_channel_event_connected(remdesk, pData, dataLength)))
				WLog_ERR(TAG,  "remdesk_virtual_channel_event_connected failed with error %lu", error);
			break;

		case CHANNEL_EVENT_DISCONNECTED:
			if ((error = remdesk_virtual_channel_event_disconnected(remdesk)))
				WLog_ERR(TAG,  "remdesk_virtual_channel_event_disconnected failed with error %lu", error);
			break;

		case CHANNEL_EVENT_TERMINATED:
			remdesk_virtual_channel_event_terminated(remdesk);
			break;
	}
	if (error && remdesk->rdpcontext)
		setChannelError(remdesk->rdpcontext, error, "remdesk_virtual_channel_init_event reported an error");
}

/* remdesk is always built-in */
#define VirtualChannelEntry	remdesk_VirtualChannelEntry

BOOL VCAPITYPE VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints)
{
	UINT rc;
	UINT error;

	remdeskPlugin* remdesk;
	RemdeskClientContext* context = NULL;
	CHANNEL_ENTRY_POINTS_FREERDP* pEntryPointsEx;

	remdesk = (remdeskPlugin*) calloc(1, sizeof(remdeskPlugin));

	if (!remdesk)
	{
		WLog_ERR(TAG, "calloc failed!");
		return FALSE;
	}

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
		if (!context)
		{
			WLog_ERR(TAG, "calloc failed!");
			goto error_out;
		}

		context->handle = (void*) remdesk;

		*(pEntryPointsEx->ppInterface) = (void*) context;
		remdesk->context = context;
		remdesk->rdpcontext = pEntryPointsEx->context;
	}

	CopyMemory(&(remdesk->channelEntryPoints), pEntryPoints, sizeof(CHANNEL_ENTRY_POINTS_FREERDP));

	rc = remdesk->channelEntryPoints.pVirtualChannelInit(&remdesk->InitHandle,
		&remdesk->channelDef, 1, VIRTUAL_CHANNEL_VERSION_WIN2000, remdesk_virtual_channel_init_event);
	if (CHANNEL_RC_OK != rc)
	{
		WLog_ERR(TAG, "pVirtualChannelInit failed with %s [%08X]",
				 WTSErrorToString(rc), rc);
		goto error_out;
	}

	remdesk->channelEntryPoints.pInterface = *(remdesk->channelEntryPoints.ppInterface);
	remdesk->channelEntryPoints.ppInterface = &(remdesk->channelEntryPoints.pInterface);

	if ((error = remdesk_add_init_handle_data(remdesk->InitHandle, (void*) remdesk)))
	{
		WLog_ERR(TAG, "remdesk_add_init_handle_data failed with error %lu!", error);
		goto error_out;
	}
	return TRUE;
error_out:
	free(remdesk);
	free(context);
	return FALSE;
}
