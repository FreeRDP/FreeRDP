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

	status = remdesk->channelEntryPoints.pVirtualChannelWriteEx(remdesk->InitHandle,
	         remdesk->OpenHandle,
	         Stream_Buffer(s), (UINT32) Stream_Length(s), s);

	if (status != CHANNEL_RC_OK)
		WLog_ERR(TAG,  "pVirtualChannelWriteEx failed with %s [%08"PRIX32"]",
		         WTSErrorToString(status), status);

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_generate_expert_blob(remdeskPlugin* remdesk)
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

	pass = freerdp_assistance_bin_to_hex_string(remdesk->EncryptedPassStub,
	        remdesk->EncryptedPassStubSize);

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
static UINT remdesk_read_channel_header(wStream* s,
                                        REMDESK_CHANNEL_HEADER* header)
{
	int status;
	UINT32 ChannelNameLen;
	char* pChannelName = NULL;

	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_ERR(TAG, "Not enough data!");
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
		WLog_ERR(TAG, "ChannelNameLen %% 2) != 0 ");
		return ERROR_INVALID_DATA;
	}

	if (Stream_GetRemainingLength(s) < ChannelNameLen)
	{
		WLog_ERR(TAG, "Not enough data!");
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
static UINT remdesk_write_channel_header(wStream* s,
        REMDESK_CHANNEL_HEADER* header)
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
static UINT remdesk_prepare_ctl_header(REMDESK_CTL_HEADER* ctlHeader,
                                       UINT32 msgType, UINT32 msgSize)
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
static UINT remdesk_recv_ctl_server_announce_pdu(remdeskPlugin* remdesk,
        wStream* s, REMDESK_CHANNEL_HEADER* header)
{
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_recv_ctl_version_info_pdu(remdeskPlugin* remdesk,
        wStream* s, REMDESK_CHANNEL_HEADER* header)
{
	UINT32 versionMajor;
	UINT32 versionMinor;

	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_ERR(TAG, "Not enough data!");
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
		WLog_ERR(TAG, "remdesk_virtual_channel_write failed with error %"PRIu32"!", error);

	if (error != CHANNEL_RC_OK)
		Stream_Free(s, TRUE);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_recv_ctl_result_pdu(remdeskPlugin* remdesk, wStream* s,
                                        REMDESK_CHANNEL_HEADER* header, UINT32* pResult)
{
	UINT32 result;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "Not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, result); /* result (4 bytes) */
	*pResult = result;
	//WLog_DBG(TAG, "RemdeskRecvResult: 0x%08"PRIX32"", result);
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
		WLog_ERR(TAG, "remdesk_generate_expert_blob failed with error %"PRIu32"", error);
		return error;
	}

	pdu.expertBlob = remdesk->ExpertBlob;
	pdu.raConnectionString = remdesk->settings->RemoteAssistanceRCTicket;
	status = ConvertToUnicode(CP_UTF8, 0, pdu.raConnectionString, -1,
	                          &raConnectionStringW, 0);

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
		WLog_ERR(TAG, "remdesk_virtual_channel_write failed with error %"PRIu32"!", error);

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
	status = ConvertToUnicode(CP_UTF8, 0, pdu.raConnectionString, -1,
	                          &raConnectionStringW, 0);

	if (status <= 0)
	{
		WLog_ERR(TAG, "ConvertToUnicode failed!");
		return ERROR_INTERNAL_ERROR;
	}

	cbRaConnectionStringW = status * 2;
	remdesk_prepare_ctl_header(&(pdu.ctlHeader), REMDESK_CTL_REMOTE_CONTROL_DESKTOP,
	                           cbRaConnectionStringW);
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
		WLog_ERR(TAG, "remdesk_virtual_channel_write failed with error %"PRIu32"!", error);

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
		WLog_ERR(TAG, "remdesk_generate_expert_blob failed with error %"PRIu32"!", error);
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
	remdesk_prepare_ctl_header(&(pdu.ctlHeader), REMDESK_CTL_VERIFY_PASSWORD,
	                           cbExpertBlobW);
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
		WLog_ERR(TAG, "remdesk_virtual_channel_write failed with error %"PRIu32"!", error);

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
		WLog_ERR(TAG, "remdesk_generate_expert_blob failed with error %"PRIu32"!", error);
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
static UINT remdesk_recv_ctl_pdu(remdeskPlugin* remdesk, wStream* s,
                                 REMDESK_CHANNEL_HEADER* header)
{
	UINT error = CHANNEL_RC_OK;
	UINT32 msgType = 0;
	UINT32 result = 0;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "Not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, msgType); /* msgType (4 bytes) */

	//WLog_DBG(TAG, "msgType: %"PRIu32"", msgType);

	switch (msgType)
	{
		case REMDESK_CTL_REMOTE_CONTROL_DESKTOP:
			break;

		case REMDESK_CTL_RESULT:
			if ((error = remdesk_recv_ctl_result_pdu(remdesk, s, header, &result)))
				WLog_ERR(TAG, "remdesk_recv_ctl_result_pdu failed with error %"PRIu32"", error);

			break;

		case REMDESK_CTL_AUTHENTICATE:
			break;

		case REMDESK_CTL_SERVER_ANNOUNCE:
			if ((error = remdesk_recv_ctl_server_announce_pdu(remdesk, s, header)))
				WLog_ERR(TAG, "remdesk_recv_ctl_server_announce_pdu failed with error %"PRIu32"",
				         error);

			break;

		case REMDESK_CTL_DISCONNECT:
			break;

		case REMDESK_CTL_VERSIONINFO:
			if ((error = remdesk_recv_ctl_version_info_pdu(remdesk, s, header)))
			{
				WLog_ERR(TAG, "remdesk_recv_ctl_version_info_pdu failed with error %"PRIu32"", error);
				break;
			}

			if (remdesk->Version == 1)
			{
				if ((error = remdesk_send_ctl_version_info_pdu(remdesk)))
				{
					WLog_ERR(TAG, "remdesk_send_ctl_version_info_pdu failed with error %"PRIu32"", error);
					break;
				}

				if ((error = remdesk_send_ctl_authenticate_pdu(remdesk)))
				{
					WLog_ERR(TAG, "remdesk_send_ctl_authenticate_pdu failed with error %"PRIu32"", error);
					break;
				}

				if ((error = remdesk_send_ctl_remote_control_desktop_pdu(remdesk)))
				{
					WLog_ERR(TAG,
					         "remdesk_send_ctl_remote_control_desktop_pdu failed with error %"PRIu32"", error);
					break;
				}
			}
			else if (remdesk->Version == 2)
			{
				if ((error = remdesk_send_ctl_expert_on_vista_pdu(remdesk)))
				{
					WLog_ERR(TAG, "remdesk_send_ctl_expert_on_vista_pdu failed with error %"PRIu32"",
					         error);
					break;
				}

				if ((error = remdesk_send_ctl_verify_password_pdu(remdesk)))
				{
					WLog_ERR(TAG, "remdesk_send_ctl_verify_password_pdu failed with error %"PRIu32"",
					         error);
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
			WLog_ERR(TAG,  "unknown msgType: %"PRIu32"", msgType);
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
	WLog_DBG(TAG, "RemdeskReceive: %"PRIuz"", Stream_GetRemainingLength(s));
	winpr_HexDump(Stream_Pointer(s), Stream_GetRemainingLength(s));
#endif

	if ((status = remdesk_read_channel_header(s, &header)))
	{
		WLog_ERR(TAG, "remdesk_read_channel_header failed with error %"PRIu32"", status);
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

static VOID VCAPITYPE remdesk_virtual_channel_open_event_ex(LPVOID lpUserParam, DWORD openHandle,
        UINT event,
        LPVOID pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	UINT error = CHANNEL_RC_OK;
	remdeskPlugin* remdesk = (remdeskPlugin*) lpUserParam;

	if (!remdesk || (remdesk->OpenHandle != openHandle))
	{
		WLog_ERR(TAG,  "error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_DATA_RECEIVED:
			if ((error = remdesk_virtual_channel_event_data_received(remdesk, pData,
			             dataLength, totalLength, dataFlags)))
				WLog_ERR(TAG,
				         "remdesk_virtual_channel_event_data_received failed with error %"PRIu32"!", error);

			break;

		case CHANNEL_EVENT_WRITE_COMPLETE:
			Stream_Free((wStream*) pData, TRUE);
			break;

		case CHANNEL_EVENT_USER:
			break;

		default:
			WLog_ERR(TAG, "unhandled event %"PRIu32"!", event);
			error = ERROR_INTERNAL_ERROR;
	}

	if (error && remdesk->rdpcontext)
		setChannelError(remdesk->rdpcontext, error,
		                "remdesk_virtual_channel_open_event_ex reported an error");
}

static DWORD WINAPI remdesk_virtual_channel_client_thread(LPVOID arg)
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

		if (!MessageQueue_Peek(remdesk->queue, &message, TRUE))
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

			if ((error = remdesk_process_receive(remdesk, data)))
			{
				WLog_ERR(TAG, "remdesk_process_receive failed with error %"PRIu32"!", error);
				break;
			}
		}
	}

	if (error && remdesk->rdpcontext)
		setChannelError(remdesk->rdpcontext, error,
		                "remdesk_virtual_channel_client_thread reported an error");

	ExitThread(error);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT remdesk_virtual_channel_event_connected(remdeskPlugin* remdesk,
        LPVOID pData, UINT32 dataLength)
{
	UINT32 status;
	UINT error;
	status = remdesk->channelEntryPoints.pVirtualChannelOpenEx(remdesk->InitHandle,
	         &remdesk->OpenHandle, remdesk->channelDef.name,
	         remdesk_virtual_channel_open_event_ex);

	if (status != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG,  "pVirtualChannelOpenEx failed with %s [%08"PRIX32"]",
		         WTSErrorToString(status), status);
		return status;
	}

	remdesk->queue = MessageQueue_New(NULL);

	if (!remdesk->queue)
	{
		WLog_ERR(TAG, "MessageQueue_New failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto error_out;
	}

	remdesk->thread = CreateThread(NULL, 0,
								   remdesk_virtual_channel_client_thread, (void*) remdesk,
	                               0, NULL);

	if (!remdesk->thread)
	{
		WLog_ERR(TAG, "CreateThread failed");
		error = ERROR_INTERNAL_ERROR;
		goto error_out;
	}

	return CHANNEL_RC_OK;
error_out:
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

	if (MessageQueue_PostQuit(remdesk->queue, 0)
	    && (WaitForSingleObject(remdesk->thread, INFINITE) == WAIT_FAILED))
	{
		rc = GetLastError();
		WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"", rc);
		return rc;
	}

	MessageQueue_Free(remdesk->queue);
	CloseHandle(remdesk->thread);
	remdesk->queue = NULL;
	remdesk->thread = NULL;
	rc = remdesk->channelEntryPoints.pVirtualChannelCloseEx(remdesk->InitHandle, remdesk->OpenHandle);

	if (CHANNEL_RC_OK != rc)
	{
		WLog_ERR(TAG, "pVirtualChannelCloseEx failed with %s [%08"PRIX32"]",
		         WTSErrorToString(rc), rc);
	}

	remdesk->OpenHandle = 0;

	if (remdesk->data_in)
	{
		Stream_Free(remdesk->data_in, TRUE);
		remdesk->data_in = NULL;
	}

	return rc;
}

static void remdesk_virtual_channel_event_terminated(remdeskPlugin* remdesk)
{
	remdesk->InitHandle = 0;
	free(remdesk->context);
	free(remdesk);
}

static VOID VCAPITYPE remdesk_virtual_channel_init_event_ex(LPVOID lpUserParam, LPVOID pInitHandle,
        UINT event, LPVOID pData,
        UINT dataLength)
{
	UINT error = CHANNEL_RC_OK;
	remdeskPlugin* remdesk = (remdeskPlugin*) lpUserParam;

	if (!remdesk || (remdesk->InitHandle != pInitHandle))
	{
		WLog_ERR(TAG,  "error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_CONNECTED:
			if ((error = remdesk_virtual_channel_event_connected(remdesk, pData,
			             dataLength)))
				WLog_ERR(TAG,  "remdesk_virtual_channel_event_connected failed with error %"PRIu32"",
				         error);

			break;

		case CHANNEL_EVENT_DISCONNECTED:
			if ((error = remdesk_virtual_channel_event_disconnected(remdesk)))
				WLog_ERR(TAG,
				         "remdesk_virtual_channel_event_disconnected failed with error %"PRIu32"", error);

			break;

		case CHANNEL_EVENT_TERMINATED:
			remdesk_virtual_channel_event_terminated(remdesk);
			break;

		case CHANNEL_EVENT_ATTACHED:
		case CHANNEL_EVENT_DETACHED:
		default:
			break;
	}

	if (error && remdesk->rdpcontext)
		setChannelError(remdesk->rdpcontext, error,
		                "remdesk_virtual_channel_init_event reported an error");
}

/* remdesk is always built-in */
#define VirtualChannelEntryEx	remdesk_VirtualChannelEntryEx

BOOL VCAPITYPE VirtualChannelEntryEx(PCHANNEL_ENTRY_POINTS pEntryPoints, PVOID pInitHandle)
{
	UINT rc;
	remdeskPlugin* remdesk;
	RemdeskClientContext* context = NULL;
	CHANNEL_ENTRY_POINTS_FREERDP_EX* pEntryPointsEx;

	if (!pEntryPoints)
	{
		return FALSE;
	}

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
	pEntryPointsEx = (CHANNEL_ENTRY_POINTS_FREERDP_EX*) pEntryPoints;

	if ((pEntryPointsEx->cbSize >= sizeof(CHANNEL_ENTRY_POINTS_FREERDP_EX)) &&
	    (pEntryPointsEx->MagicNumber == FREERDP_CHANNEL_MAGIC_NUMBER))
	{
		context = (RemdeskClientContext*) calloc(1, sizeof(RemdeskClientContext));

		if (!context)
		{
			WLog_ERR(TAG, "calloc failed!");
			goto error_out;
		}

		context->handle = (void*) remdesk;
		remdesk->context = context;
		remdesk->rdpcontext = pEntryPointsEx->context;
	}

	CopyMemory(&(remdesk->channelEntryPoints), pEntryPoints,
	           sizeof(CHANNEL_ENTRY_POINTS_FREERDP_EX));
	remdesk->InitHandle = pInitHandle;
	rc = remdesk->channelEntryPoints.pVirtualChannelInitEx(remdesk, context, pInitHandle,
	        &remdesk->channelDef, 1, VIRTUAL_CHANNEL_VERSION_WIN2000,
	        remdesk_virtual_channel_init_event_ex);

	if (CHANNEL_RC_OK != rc)
	{
		WLog_ERR(TAG, "pVirtualChannelInitEx failed with %s [%08"PRIX32"]",
		         WTSErrorToString(rc), rc);
		goto error_out;
	}

	remdesk->channelEntryPoints.pInterface = context;
	return TRUE;
error_out:
	free(remdesk);
	free(context);
	return FALSE;
}
