/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Device Redirection Virtual Channel Extension
 *
 * Copyright 2014 Dell Software <Mike.McDonald@software.dell.com>
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015-2022 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2022 Armin Novak <anovak@thincast.com>
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
#include <freerdp/utils/rdpdr_utils.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/nt.h>
#include <winpr/print.h>
#include <winpr/stream.h>

#include <freerdp/channels/log.h>
#include "rdpdr_main.h"

#define RDPDR_ADD_PRINTER_EVENT 0x00000001
#define RDPDR_UPDATE_PRINTER_EVENT 0x00000002
#define RDPDR_DELETE_PRINTER_EVENT 0x00000003
#define RDPDR_RENAME_PRINTER_EVENT 0x00000004

#define RDPDR_HEADER_LENGTH 4
#define RDPDR_CAPABILITY_HEADER_LENGTH 8

struct s_rdpdr_server_private
{
	HANDLE Thread;
	HANDLE StopEvent;
	void* ChannelHandle;

	UINT32 ClientId;
	UINT16 VersionMajor;
	UINT16 VersionMinor;
	char* ClientComputerName;

	BOOL UserLoggedOnPdu;

	wListDictionary* IrpList;
	UINT32 NextCompletionId;

	wHashTable* devicelist;
	wLog* log;
};

static void rdpdr_device_free(RdpdrDevice* device)
{
	if (!device)
		return;
	free(device->DeviceData);
	free(device);
}

static void rdpdr_device_free_h(void* obj)
{
	RdpdrDevice* other = obj;
	rdpdr_device_free(other);
}

static UINT32 rdpdr_deviceid_hash(const void* id)
{
	WINPR_ASSERT(id);
	return *((const UINT32*)id);
}

static RdpdrDevice* rdpdr_device_new(void)
{
	return calloc(1, sizeof(RdpdrDevice));
}

static void* rdpdr_device_clone(const void* val)
{
	const RdpdrDevice* other = val;
	RdpdrDevice* tmp;

	if (!other)
		return NULL;

	tmp = rdpdr_device_new();
	if (!tmp)
		goto fail;

	*tmp = *other;
	if (other->DeviceData)
	{
		tmp->DeviceData = malloc(other->DeviceDataLength);
		if (!tmp->DeviceData)
			goto fail;
		memcpy(tmp->DeviceData, other->DeviceData, other->DeviceDataLength);
	}
	return tmp;

fail:
	rdpdr_device_free(tmp);
	return NULL;
}

static RdpdrDevice* rdpdr_get_device_by_id(RdpdrServerPrivate* priv, UINT32 DeviceId)
{
	WINPR_ASSERT(priv);

	return HashTable_GetItemValue(priv->devicelist, &DeviceId);
}

static BOOL rdpdr_remove_device_by_id(RdpdrServerPrivate* priv, UINT32 DeviceId)
{
	const RdpdrDevice* device = rdpdr_get_device_by_id(priv, DeviceId);
	WINPR_ASSERT(priv);

	if (!device)
	{
		WLog_Print(priv->log, WLOG_WARN, "[del] Device Id: 0x%08" PRIX32 ": no such device",
		           DeviceId);
		return FALSE;
	}
	WLog_Print(priv->log, WLOG_DEBUG,
	           "[del] Device Name: %s Id: 0x%08" PRIX32 " DataLength: %" PRIu32 "",
	           device->PreferredDosName, device->DeviceId, device->DeviceDataLength);
	return HashTable_Remove(priv->devicelist, &DeviceId);
}

static BOOL rdpdr_add_device(RdpdrServerPrivate* priv, const RdpdrDevice* device)
{
	WINPR_ASSERT(priv);
	WINPR_ASSERT(device);

	WLog_Print(priv->log, WLOG_DEBUG,
	           "[add] Device Name: %s Id: 0x%08" PRIX32 " DataLength: %" PRIu32 "",
	           device->PreferredDosName, device->DeviceId, device->DeviceDataLength);

	return HashTable_Insert(priv->devicelist, &device->DeviceId, device);
}

static UINT32 g_ClientId = 0;

static const WCHAR* rdpdr_read_ustring(wLog* log, wStream* s, size_t bytelen)
{
	const size_t charlen = (bytelen + 1) / sizeof(WCHAR);
	const WCHAR* str = (const WCHAR*)Stream_Pointer(s);
	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, bytelen))
		return NULL;
	if (_wcsnlen(str, charlen) == charlen)
	{
		WLog_Print(log, WLOG_WARN, "[rdpdr] unicode string not '\0' terminated");
		return NULL;
	}
	Stream_Seek(s, bytelen);
	return str;
}

static RDPDR_IRP* rdpdr_server_irp_new(void)
{
	RDPDR_IRP* irp = (RDPDR_IRP*)calloc(1, sizeof(RDPDR_IRP));
	return irp;
}

static void rdpdr_server_irp_free(RDPDR_IRP* irp)
{
	free(irp);
}

static BOOL rdpdr_server_enqueue_irp(RdpdrServerContext* context, RDPDR_IRP* irp)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	return ListDictionary_Add(context->priv->IrpList, (void*)(size_t)irp->CompletionId, irp);
}

static RDPDR_IRP* rdpdr_server_dequeue_irp(RdpdrServerContext* context, UINT32 completionId)
{
	RDPDR_IRP* irp;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	irp = (RDPDR_IRP*)ListDictionary_Remove(context->priv->IrpList, (void*)(size_t)completionId);
	return irp;
}

static UINT rdpdr_seal_send_free_request(RdpdrServerContext* context, wStream* s)
{
	BOOL status;
	size_t length;
	ULONG written;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(s);

	Stream_SealLength(s);
	length = Stream_Length(s);
	WINPR_ASSERT(length <= ULONG_MAX);
	Stream_SetPosition(s, 0);

	if (length >= RDPDR_HEADER_LENGTH)
	{
		RDPDR_HEADER header = { 0 };
		Stream_Read_UINT16(s, header.Component);
		Stream_Read_UINT16(s, header.PacketId);

		WLog_Print(context->priv->log, WLOG_DEBUG,
		           "sending message {Component %s[%04" PRIx16 "], PacketId %s[%04" PRIx16 "]",
		           rdpdr_component_string(header.Component), header.Component,
		           rdpdr_packetid_string(header.PacketId), header.PacketId);
	}
	winpr_HexLogDump(context->priv->log, WLOG_DEBUG, Stream_Buffer(s), Stream_Length(s));
	status = WTSVirtualChannelWrite(context->priv->ChannelHandle, (PCHAR)Stream_Buffer(s),
	                                (ULONG)length, &written);
	Stream_Free(s, TRUE);
	return status ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_send_announce_request(RdpdrServerContext* context)
{
	UINT error;
	wStream* s;
	RDPDR_HEADER header = { 0 };

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	header.Component = RDPDR_CTYP_CORE;
	header.PacketId = PAKID_CORE_SERVER_ANNOUNCE;

	error = IFCALLRESULT(CHANNEL_RC_OK, context->SendServerAnnounce, context);
	if (error != CHANNEL_RC_OK)
		return error;

	s = Stream_New(NULL, RDPDR_HEADER_LENGTH + 8);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, header.Component);            /* Component (2 bytes) */
	Stream_Write_UINT16(s, header.PacketId);             /* PacketId (2 bytes) */
	Stream_Write_UINT16(s, context->priv->VersionMajor); /* VersionMajor (2 bytes) */
	Stream_Write_UINT16(s, context->priv->VersionMinor); /* VersionMinor (2 bytes) */
	Stream_Write_UINT32(s, context->priv->ClientId);     /* ClientId (4 bytes) */
	return rdpdr_seal_send_free_request(context, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_receive_announce_response(RdpdrServerContext* context, wStream* s,
                                                   const RDPDR_HEADER* header)
{
	UINT32 ClientId;
	UINT16 VersionMajor;
	UINT16 VersionMinor;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(header);

	WINPR_UNUSED(header);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 8))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, VersionMajor); /* VersionMajor (2 bytes) */
	Stream_Read_UINT16(s, VersionMinor); /* VersionMinor (2 bytes) */
	Stream_Read_UINT32(s, ClientId);     /* ClientId (4 bytes) */
	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "Client Announce Response: VersionMajor: 0x%08" PRIX16 " VersionMinor: 0x%04" PRIX16
	           " ClientId: 0x%08" PRIX32 "",
	           VersionMajor, VersionMinor, ClientId);
	context->priv->ClientId = ClientId;

	return IFCALLRESULT(CHANNEL_RC_OK, context->ReceiveAnnounceResponse, context, VersionMajor,
	                    VersionMinor, ClientId);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_receive_client_name_request(RdpdrServerContext* context, wStream* s,
                                                     const RDPDR_HEADER* header)
{
	UINT32 UnicodeFlag;
	UINT32 CodePage;
	UINT32 ComputerNameLen;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(s);
	WINPR_ASSERT(header);
	WINPR_UNUSED(header);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 12))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, UnicodeFlag);     /* UnicodeFlag (4 bytes) */
	Stream_Read_UINT32(s, CodePage);        /* CodePage (4 bytes), MUST be set to zero */
	Stream_Read_UINT32(s, ComputerNameLen); /* ComputerNameLen (4 bytes) */
	/* UnicodeFlag is either 0 or 1, the other 31 bits must be ignored.
	 */
	UnicodeFlag = UnicodeFlag & 0x00000001;

	if (CodePage != 0)
		WLog_Print(context->priv->log, WLOG_WARN,
		           "[MS-RDPEFS] 2.2.2.4 Client Name Request (DR_CORE_CLIENT_NAME_REQ)::CodePage "
		           "must be 0, but is 0x%08" PRIx32,
		           CodePage);

	/**
	 * Caution: ComputerNameLen is given *bytes*,
	 * not in characters, including the NULL terminator!
	 */

	if (UnicodeFlag)
	{
		if ((ComputerNameLen % 2) || ComputerNameLen > 512 || ComputerNameLen < 2)
		{
			WLog_Print(context->priv->log, WLOG_ERROR,
			           "invalid unicode computer name length: %" PRIu32 "", ComputerNameLen);
			return ERROR_INVALID_DATA;
		}
	}
	else
	{
		if (ComputerNameLen > 256 || ComputerNameLen < 1)
		{
			WLog_Print(context->priv->log, WLOG_ERROR,
			           "invalid ascii computer name length: %" PRIu32 "", ComputerNameLen);
			return ERROR_INVALID_DATA;
		}
	}

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, ComputerNameLen))
		return ERROR_INVALID_DATA;

	/* ComputerName must be null terminated, check if it really is */

	if (Stream_Pointer(s)[ComputerNameLen - 1] ||
	    (UnicodeFlag && Stream_Pointer(s)[ComputerNameLen - 2]))
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "computer name must be null terminated");
		return ERROR_INVALID_DATA;
	}

	if (context->priv->ClientComputerName)
	{
		free(context->priv->ClientComputerName);
		context->priv->ClientComputerName = NULL;
	}

	if (UnicodeFlag)
	{
		context->priv->ClientComputerName =
		    Stream_Read_UTF16_String_As_UTF8(s, ComputerNameLen / sizeof(WCHAR), NULL);
		if (!context->priv->ClientComputerName)
		{
			WLog_Print(context->priv->log, WLOG_ERROR, "failed to convert client computer name");
			return ERROR_INVALID_DATA;
		}
	}
	else
	{
		const char* name = (const char*)Stream_Pointer(s);
		context->priv->ClientComputerName = _strdup(name);
		Stream_Seek(s, ComputerNameLen);

		if (!context->priv->ClientComputerName)
		{
			WLog_Print(context->priv->log, WLOG_ERROR, "failed to duplicate client computer name");
			return CHANNEL_RC_NO_MEMORY;
		}
	}

	WLog_Print(context->priv->log, WLOG_DEBUG, "ClientComputerName: %s",
	           context->priv->ClientComputerName);
	return IFCALLRESULT(CHANNEL_RC_OK, context->ReceiveClientNameRequest, context, ComputerNameLen,
	                    context->priv->ClientComputerName);
}

static UINT rdpdr_server_write_capability_set_header_cb(RdpdrServerContext* context, wStream* s,
                                                        const RDPDR_CAPABILITY_HEADER* header)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	UINT error = rdpdr_write_capset_header(context->priv->log, s, header);
	if (error != CHANNEL_RC_OK)
		return error;

	return IFCALLRESULT(CHANNEL_RC_OK, context->SendCaps, context, header, 0, NULL);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_read_general_capability_set(RdpdrServerContext* context, wStream* s,
                                                     const RDPDR_CAPABILITY_HEADER* header)
{
	UINT32 ioCode1;
	UINT32 extraFlags1;
	UINT32 extendedPdu;
	UINT16 VersionMajor;
	UINT16 VersionMinor;
	UINT32 SpecialTypeDeviceCap;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(header);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 32))
		return ERROR_INVALID_DATA;

	Stream_Seek_UINT32(s);               /* osType (4 bytes), ignored on receipt */
	Stream_Seek_UINT32(s);               /* osVersion (4 bytes), unused and must be set to zero */
	Stream_Read_UINT16(s, VersionMajor); /* protocolMajorVersion (2 bytes) */
	Stream_Read_UINT16(s, VersionMinor); /* protocolMinorVersion (2 bytes) */
	Stream_Read_UINT32(s, ioCode1);      /* ioCode1 (4 bytes) */
	Stream_Seek_UINT32(s); /* ioCode2 (4 bytes), must be set to zero, reserved for future use */
	Stream_Read_UINT32(s, extendedPdu); /* extendedPdu (4 bytes) */
	Stream_Read_UINT32(s, extraFlags1); /* extraFlags1 (4 bytes) */
	Stream_Seek_UINT32(s); /* extraFlags2 (4 bytes), must be set to zero, reserved for future use */

	if (VersionMajor != RDPDR_MAJOR_RDP_VERSION)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "unsupported RDPDR version %" PRIu16 ".%" PRIu16,
		           VersionMajor, VersionMinor);
		return ERROR_INVALID_DATA;
	}

	switch (VersionMinor)
	{
		case RDPDR_MINOR_RDP_VERSION_13:
			break;
		case RDPDR_MINOR_RDP_VERSION_6_X:
			break;
		case RDPDR_MINOR_RDP_VERSION_5_2:
			break;
		case RDPDR_MINOR_RDP_VERSION_5_1:
			break;
		case RDPDR_MINOR_RDP_VERSION_5_0:
			break;
		default:
			WLog_Print(context->priv->log, WLOG_WARN,
			           "unsupported RDPDR minor version %" PRIu16 ".%" PRIu16, VersionMajor,
			           VersionMinor);
			break;
	}

	if (header->Version == GENERAL_CAPABILITY_VERSION_02)
	{
		if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 4))
			return ERROR_INVALID_DATA;

		Stream_Read_UINT32(s, SpecialTypeDeviceCap); /* SpecialTypeDeviceCap (4 bytes) */
	}

	context->priv->UserLoggedOnPdu = (extendedPdu & RDPDR_USER_LOGGEDON_PDU) ? TRUE : FALSE;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_write_general_capability_set(RdpdrServerContext* context, wStream* s)
{
	UINT32 ioCode1;
	UINT32 extendedPdu;
	UINT32 extraFlags1;
	UINT32 SpecialTypeDeviceCap;
	const RDPDR_CAPABILITY_HEADER header = { CAP_GENERAL_TYPE, RDPDR_CAPABILITY_HEADER_LENGTH + 36,
		                                     GENERAL_CAPABILITY_VERSION_02 };

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	ioCode1 = 0;
	ioCode1 |= RDPDR_IRP_MJ_CREATE;                   /* always set */
	ioCode1 |= RDPDR_IRP_MJ_CLEANUP;                  /* always set */
	ioCode1 |= RDPDR_IRP_MJ_CLOSE;                    /* always set */
	ioCode1 |= RDPDR_IRP_MJ_READ;                     /* always set */
	ioCode1 |= RDPDR_IRP_MJ_WRITE;                    /* always set */
	ioCode1 |= RDPDR_IRP_MJ_FLUSH_BUFFERS;            /* always set */
	ioCode1 |= RDPDR_IRP_MJ_SHUTDOWN;                 /* always set */
	ioCode1 |= RDPDR_IRP_MJ_DEVICE_CONTROL;           /* always set */
	ioCode1 |= RDPDR_IRP_MJ_QUERY_VOLUME_INFORMATION; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_SET_VOLUME_INFORMATION;   /* always set */
	ioCode1 |= RDPDR_IRP_MJ_QUERY_INFORMATION;        /* always set */
	ioCode1 |= RDPDR_IRP_MJ_SET_INFORMATION;          /* always set */
	ioCode1 |= RDPDR_IRP_MJ_DIRECTORY_CONTROL;        /* always set */
	ioCode1 |= RDPDR_IRP_MJ_LOCK_CONTROL;             /* always set */
	ioCode1 |= RDPDR_IRP_MJ_QUERY_SECURITY;           /* optional */
	ioCode1 |= RDPDR_IRP_MJ_SET_SECURITY;             /* optional */
	extendedPdu = 0;
	extendedPdu |= RDPDR_CLIENT_DISPLAY_NAME_PDU; /* always set */
	extendedPdu |= RDPDR_DEVICE_REMOVE_PDUS;      /* optional */

	if (context->priv->UserLoggedOnPdu)
		extendedPdu |= RDPDR_USER_LOGGEDON_PDU; /* optional */

	extraFlags1 = 0;
	extraFlags1 |= ENABLE_ASYNCIO; /* optional */
	SpecialTypeDeviceCap = 0;

	UINT error = rdpdr_write_capset_header(context->priv->log, s, &header);
	if (error != CHANNEL_RC_OK)
		return error;

	const BYTE* data = Stream_Pointer(s);
	const size_t start = Stream_GetPosition(s);
	Stream_Write_UINT32(s, 0); /* osType (4 bytes), ignored on receipt */
	Stream_Write_UINT32(s, 0); /* osVersion (4 bytes), unused and must be set to zero */
	Stream_Write_UINT16(s, context->priv->VersionMajor); /* protocolMajorVersion (2 bytes) */
	Stream_Write_UINT16(s, context->priv->VersionMinor); /* protocolMinorVersion (2 bytes) */
	Stream_Write_UINT32(s, ioCode1);                     /* ioCode1 (4 bytes) */
	Stream_Write_UINT32(s, 0); /* ioCode2 (4 bytes), must be set to zero, reserved for future use */
	Stream_Write_UINT32(s, extendedPdu); /* extendedPdu (4 bytes) */
	Stream_Write_UINT32(s, extraFlags1); /* extraFlags1 (4 bytes) */
	Stream_Write_UINT32(
	    s, 0); /* extraFlags2 (4 bytes), must be set to zero, reserved for future use */
	Stream_Write_UINT32(s, SpecialTypeDeviceCap); /* SpecialTypeDeviceCap (4 bytes) */
	const size_t end = Stream_GetPosition(s);
	return IFCALLRESULT(CHANNEL_RC_OK, context->SendCaps, context, &header, end - start, data);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_read_printer_capability_set(RdpdrServerContext* context, wStream* s,
                                                     const RDPDR_CAPABILITY_HEADER* header)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(s);
	WINPR_ASSERT(header);
	WINPR_UNUSED(context);
	WINPR_UNUSED(header);
	WINPR_UNUSED(s);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_write_printer_capability_set(RdpdrServerContext* context, wStream* s)
{
	const RDPDR_CAPABILITY_HEADER header = { CAP_PRINTER_TYPE, RDPDR_CAPABILITY_HEADER_LENGTH,
		                                     PRINT_CAPABILITY_VERSION_01 };
	WINPR_UNUSED(context);
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	return rdpdr_server_write_capability_set_header_cb(context, s, &header);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_read_port_capability_set(RdpdrServerContext* context, wStream* s,
                                                  const RDPDR_CAPABILITY_HEADER* header)
{
	WINPR_UNUSED(context);
	WINPR_UNUSED(s);
	WINPR_UNUSED(header);
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_write_port_capability_set(RdpdrServerContext* context, wStream* s)
{
	const RDPDR_CAPABILITY_HEADER header = { CAP_PORT_TYPE, RDPDR_CAPABILITY_HEADER_LENGTH,
		                                     PORT_CAPABILITY_VERSION_01 };
	WINPR_UNUSED(context);
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	return rdpdr_server_write_capability_set_header_cb(context, s, &header);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_read_drive_capability_set(RdpdrServerContext* context, wStream* s,
                                                   const RDPDR_CAPABILITY_HEADER* header)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_UNUSED(context);
	WINPR_UNUSED(header);
	WINPR_UNUSED(s);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_write_drive_capability_set(RdpdrServerContext* context, wStream* s)
{
	const RDPDR_CAPABILITY_HEADER header = { CAP_DRIVE_TYPE, RDPDR_CAPABILITY_HEADER_LENGTH,
		                                     DRIVE_CAPABILITY_VERSION_02 };

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_UNUSED(context);

	return rdpdr_server_write_capability_set_header_cb(context, s, &header);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_read_smartcard_capability_set(RdpdrServerContext* context, wStream* s,
                                                       const RDPDR_CAPABILITY_HEADER* header)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_UNUSED(context);
	WINPR_UNUSED(header);
	WINPR_UNUSED(s);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_write_smartcard_capability_set(RdpdrServerContext* context, wStream* s)
{
	const RDPDR_CAPABILITY_HEADER header = { CAP_SMARTCARD_TYPE, RDPDR_CAPABILITY_HEADER_LENGTH,
		                                     SMARTCARD_CAPABILITY_VERSION_01 };
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	WINPR_UNUSED(context);

	return rdpdr_server_write_capability_set_header_cb(context, s, &header);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_send_core_capability_request(RdpdrServerContext* context)
{
	wStream* s;
	RDPDR_HEADER header = { 0 };
	UINT16 numCapabilities;
	UINT error;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	header.Component = RDPDR_CTYP_CORE;
	header.PacketId = PAKID_CORE_SERVER_CAPABILITY;
	numCapabilities = 1;

	if ((context->supported & RDPDR_DTYP_FILESYSTEM) != 0)
		numCapabilities++;

	if (((context->supported & RDPDR_DTYP_PARALLEL) != 0) ||
	    ((context->supported & RDPDR_DTYP_SERIAL) != 0))
		numCapabilities++;

	if ((context->supported & RDPDR_DTYP_PRINT) != 0)
		numCapabilities++;

	if ((context->supported & RDPDR_DTYP_SMARTCARD) != 0)
		numCapabilities++;

	s = Stream_New(NULL, RDPDR_HEADER_LENGTH + 512);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, header.Component); /* Component (2 bytes) */
	Stream_Write_UINT16(s, header.PacketId);  /* PacketId (2 bytes) */
	Stream_Write_UINT16(s, numCapabilities);  /* numCapabilities (2 bytes) */
	Stream_Write_UINT16(s, 0);                /* Padding (2 bytes) */

	if ((error = rdpdr_server_write_general_capability_set(context, s)))
	{
		WLog_Print(context->priv->log, WLOG_ERROR,
		           "rdpdr_server_write_general_capability_set failed with error %" PRIu32 "!",
		           error);
		goto out;
	}

	if ((context->supported & RDPDR_DTYP_FILESYSTEM) != 0)
	{
		if ((error = rdpdr_server_write_drive_capability_set(context, s)))
		{
			WLog_Print(context->priv->log, WLOG_ERROR,
			           "rdpdr_server_write_drive_capability_set failed with error %" PRIu32 "!",
			           error);
			goto out;
		}
	}

	if (((context->supported & RDPDR_DTYP_PARALLEL) != 0) ||
	    ((context->supported & RDPDR_DTYP_SERIAL) != 0))
	{
		if ((error = rdpdr_server_write_port_capability_set(context, s)))
		{
			WLog_Print(context->priv->log, WLOG_ERROR,
			           "rdpdr_server_write_port_capability_set failed with error %" PRIu32 "!",
			           error);
			goto out;
		}
	}

	if ((context->supported & RDPDR_DTYP_PRINT) != 0)
	{
		if ((error = rdpdr_server_write_printer_capability_set(context, s)))
		{
			WLog_Print(context->priv->log, WLOG_ERROR,
			           "rdpdr_server_write_printer_capability_set failed with error %" PRIu32 "!",
			           error);
			goto out;
		}
	}

	if ((context->supported & RDPDR_DTYP_SMARTCARD) != 0)
	{
		if ((error = rdpdr_server_write_smartcard_capability_set(context, s)))
		{
			WLog_Print(context->priv->log, WLOG_ERROR,
			           "rdpdr_server_write_printer_capability_set failed with error %" PRIu32 "!",
			           error);
			goto out;
		}
	}

	return rdpdr_seal_send_free_request(context, s);
out:
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_receive_core_capability_response(RdpdrServerContext* context, wStream* s,
                                                          const RDPDR_HEADER* header)
{
	UINT16 i = 0;
	UINT status = 0;
	UINT16 numCapabilities = 0;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	WINPR_UNUSED(header);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, numCapabilities); /* numCapabilities (2 bytes) */
	Stream_Seek_UINT16(s);                  /* Padding (2 bytes) */

	UINT16 caps = 0;
	for (i = 0; i < numCapabilities; i++)
	{
		RDPDR_CAPABILITY_HEADER capabilityHeader = { 0 };
		const size_t start = Stream_GetPosition(s);

		if ((status = rdpdr_read_capset_header(context->priv->log, s, &capabilityHeader)))
		{
			WLog_Print(context->priv->log, WLOG_ERROR, "%s failed with error %" PRIu32 "!",
			           __FUNCTION__, status);
			return status;
		}

		status = IFCALLRESULT(CHANNEL_RC_OK, context->ReceiveCaps, context, &capabilityHeader,
		                      Stream_GetRemainingLength(s), Stream_Pointer(s));
		if (status != CHANNEL_RC_OK)
			return status;

		caps |= capabilityHeader.CapabilityType;
		switch (capabilityHeader.CapabilityType)
		{
			case CAP_GENERAL_TYPE:
				if ((status =
				         rdpdr_server_read_general_capability_set(context, s, &capabilityHeader)))
				{
					WLog_Print(context->priv->log, WLOG_ERROR, "%s failed with error %" PRIu32 "!",
					           __FUNCTION__, status);
					return status;
				}

				break;

			case CAP_PRINTER_TYPE:
				if ((status =
				         rdpdr_server_read_printer_capability_set(context, s, &capabilityHeader)))
				{
					WLog_Print(context->priv->log, WLOG_ERROR, "%s failed with error %" PRIu32 "!",
					           __FUNCTION__, status);
					return status;
				}

				break;

			case CAP_PORT_TYPE:
				if ((status = rdpdr_server_read_port_capability_set(context, s, &capabilityHeader)))
				{
					WLog_Print(context->priv->log, WLOG_ERROR, "%s failed with error %" PRIu32 "!",
					           __FUNCTION__, status);
					return status;
				}

				break;

			case CAP_DRIVE_TYPE:
				if ((status =
				         rdpdr_server_read_drive_capability_set(context, s, &capabilityHeader)))
				{
					WLog_Print(context->priv->log, WLOG_ERROR, "%s failed with error %" PRIu32 "!",
					           __FUNCTION__, status);
					return status;
				}

				break;

			case CAP_SMARTCARD_TYPE:
				if ((status =
				         rdpdr_server_read_smartcard_capability_set(context, s, &capabilityHeader)))
				{
					WLog_Print(context->priv->log, WLOG_ERROR, "%s failed with error %" PRIu32 "!",
					           __FUNCTION__, status);
					return status;
				}

				break;

			default:
				WLog_Print(context->priv->log, WLOG_WARN, "Unknown capabilityType %" PRIu16 "",
				           capabilityHeader.CapabilityType);
				Stream_Seek(s, capabilityHeader.CapabilityLength);
				return ERROR_INVALID_DATA;
		}

		for (UINT16 x = 0; x < 16; x++)
		{
			const UINT16 mask = (UINT16)(1 << x);
			if (((caps & mask) != 0) && ((context->supported & mask) == 0))
			{
				WLog_Print(context->priv->log, WLOG_WARN,
				           "[%s] client sent capability %s we did not announce!", __FUNCTION__,
				           freerdp_rdpdr_dtyp_string(mask));
			}

			/* we assume the server supports the capability. so only deactivate what the client did
			 * not respond with */
			if ((caps & mask) == 0)
				context->supported &= ~mask;
		}

		const size_t end = Stream_GetPosition(s);
		const size_t diff = end - start;
		if (diff != capabilityHeader.CapabilityLength + RDPDR_CAPABILITY_HEADER_LENGTH)
		{
			WLog_Print(context->priv->log, WLOG_WARN,
			           "{capability %s[0x%04" PRIx16 "]} processed %" PRIuz
			           " bytes, but expected to be %" PRIu16,
			           rdpdr_cap_type_string(capabilityHeader.CapabilityType),
			           capabilityHeader.CapabilityType, diff, capabilityHeader.CapabilityLength);
		}
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_send_client_id_confirm(RdpdrServerContext* context)
{
	wStream* s;
	RDPDR_HEADER header = { 0 };

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	header.Component = RDPDR_CTYP_CORE;
	header.PacketId = PAKID_CORE_CLIENTID_CONFIRM;
	s = Stream_New(NULL, RDPDR_HEADER_LENGTH + 8);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, header.Component);            /* Component (2 bytes) */
	Stream_Write_UINT16(s, header.PacketId);             /* PacketId (2 bytes) */
	Stream_Write_UINT16(s, context->priv->VersionMajor); /* VersionMajor (2 bytes) */
	Stream_Write_UINT16(s, context->priv->VersionMinor); /* VersionMinor (2 bytes) */
	Stream_Write_UINT32(s, context->priv->ClientId);     /* ClientId (4 bytes) */
	return rdpdr_seal_send_free_request(context, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_receive_device_list_announce_request(RdpdrServerContext* context,
                                                              wStream* s,
                                                              const RDPDR_HEADER* header)
{
	UINT32 i = 0;
	UINT32 DeviceCount = 0;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	WINPR_UNUSED(header);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, DeviceCount); /* DeviceCount (4 bytes) */
	WLog_Print(context->priv->log, WLOG_DEBUG, "DeviceCount: %" PRIu32 "", DeviceCount);

	for (i = 0; i < DeviceCount; i++)
	{
		UINT error;
		RdpdrDevice device = { 0 };

		if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 20))
			return ERROR_INVALID_DATA;

		Stream_Read_UINT32(s, device.DeviceType);       /* DeviceType (4 bytes) */
		Stream_Read_UINT32(s, device.DeviceId);         /* DeviceId (4 bytes) */
		Stream_Read(s, device.PreferredDosName, 8);     /* PreferredDosName (8 bytes) */
		Stream_Read_UINT32(s, device.DeviceDataLength); /* DeviceDataLength (4 bytes) */
		device.DeviceData = Stream_Pointer(s);

		if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, device.DeviceDataLength))
			return ERROR_INVALID_DATA;

		if (!rdpdr_add_device(context->priv, &device))
			return ERROR_INTERNAL_ERROR;

		error = IFCALLRESULT(CHANNEL_RC_OK, context->ReceiveDeviceAnnounce, context, &device);
		if (error != CHANNEL_RC_OK)
			return error;

		switch (device.DeviceType)
		{
			case RDPDR_DTYP_FILESYSTEM:
				if ((context->supported & RDPDR_DTYP_FILESYSTEM) != 0)
					error = IFCALLRESULT(CHANNEL_RC_OK, context->OnDriveCreate, context, &device);
				break;

			case RDPDR_DTYP_PRINT:
				if ((context->supported & RDPDR_DTYP_PRINT) != 0)
					error = IFCALLRESULT(CHANNEL_RC_OK, context->OnPrinterCreate, context, &device);
				break;

			case RDPDR_DTYP_SERIAL:
				if (device.DeviceDataLength != 0)
				{
					WLog_Print(context->priv->log, WLOG_WARN,
					           "[rdpdr] RDPDR_DTYP_SERIAL::DeviceDataLength != 0 [%" PRIu32 "]",
					           device.DeviceDataLength);
					error = ERROR_INVALID_DATA;
				}
				else if ((context->supported & RDPDR_DTYP_SERIAL) != 0)
					error =
					    IFCALLRESULT(CHANNEL_RC_OK, context->OnSerialPortCreate, context, &device);
				break;

			case RDPDR_DTYP_PARALLEL:
				if (device.DeviceDataLength != 0)
				{
					WLog_Print(context->priv->log, WLOG_WARN,
					           "[rdpdr] RDPDR_DTYP_PARALLEL::DeviceDataLength != 0 [%" PRIu32 "]",
					           device.DeviceDataLength);
					error = ERROR_INVALID_DATA;
				}
				else if ((context->supported & RDPDR_DTYP_PARALLEL) != 0)
					error = IFCALLRESULT(CHANNEL_RC_OK, context->OnParallelPortCreate, context,
					                     &device);
				break;

			case RDPDR_DTYP_SMARTCARD:
				if (device.DeviceDataLength != 0)
				{
					WLog_Print(context->priv->log, WLOG_WARN,
					           "[rdpdr] RDPDR_DTYP_SMARTCARD::DeviceDataLength != 0 [%" PRIu32 "]",
					           device.DeviceDataLength);
					error = ERROR_INVALID_DATA;
				}
				else if ((context->supported & RDPDR_DTYP_SMARTCARD) != 0)
					error =
					    IFCALLRESULT(CHANNEL_RC_OK, context->OnSmartcardCreate, context, &device);
				break;

			default:
				WLog_Print(context->priv->log, WLOG_WARN,
				           "[MS-RDPEFS] 2.2.2.9 Client Device List Announce Request "
				           "(DR_CORE_DEVICELIST_ANNOUNCE_REQ) unknown device type %04" PRIx16
				           " at position %" PRIu32,
				           device.DeviceType, i);
				error = ERROR_INVALID_DATA;
				break;
		}

		if (error != CHANNEL_RC_OK)
			return error;

		Stream_Seek(s, device.DeviceDataLength);
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_receive_device_list_remove_request(RdpdrServerContext* context, wStream* s,
                                                            const RDPDR_HEADER* header)
{
	UINT32 i;
	UINT32 DeviceCount;
	UINT32 DeviceType;
	UINT32 DeviceId;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	WINPR_UNUSED(header);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, DeviceCount); /* DeviceCount (4 bytes) */
	WLog_Print(context->priv->log, WLOG_DEBUG, "DeviceCount: %" PRIu32 "", DeviceCount);

	for (i = 0; i < DeviceCount; i++)
	{
		UINT error;
		const RdpdrDevice* device;

		if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 4))
			return ERROR_INVALID_DATA;

		Stream_Read_UINT32(s, DeviceId); /* DeviceId (4 bytes) */
		device = rdpdr_get_device_by_id(context->priv, DeviceId);
		WLog_Print(context->priv->log, WLOG_DEBUG, "Device %" PRIu32 " Id: 0x%08" PRIX32 "", i,
		           DeviceId);
		DeviceType = 0;
		if (device)
			DeviceType = device->DeviceType;

		error =
		    IFCALLRESULT(CHANNEL_RC_OK, context->ReceiveDeviceRemove, context, DeviceId, device);
		if (error != CHANNEL_RC_OK)
			return error;

		switch (DeviceType)
		{
			case RDPDR_DTYP_FILESYSTEM:
				if ((context->supported & RDPDR_DTYP_FILESYSTEM) != 0)
					error = IFCALLRESULT(CHANNEL_RC_OK, context->OnDriveDelete, context, DeviceId);
				break;

			case RDPDR_DTYP_PRINT:
				if ((context->supported & RDPDR_DTYP_PRINT) != 0)
					error =
					    IFCALLRESULT(CHANNEL_RC_OK, context->OnPrinterDelete, context, DeviceId);
				break;

			case RDPDR_DTYP_SERIAL:
				if ((context->supported & RDPDR_DTYP_SERIAL) != 0)
					error =
					    IFCALLRESULT(CHANNEL_RC_OK, context->OnSerialPortDelete, context, DeviceId);
				break;

			case RDPDR_DTYP_PARALLEL:
				if ((context->supported & RDPDR_DTYP_PARALLEL) != 0)
					error = IFCALLRESULT(CHANNEL_RC_OK, context->OnParallelPortDelete, context,
					                     DeviceId);
				break;

			case RDPDR_DTYP_SMARTCARD:
				if ((context->supported & RDPDR_DTYP_SMARTCARD) != 0)
					error =
					    IFCALLRESULT(CHANNEL_RC_OK, context->OnSmartcardDelete, context, DeviceId);
				break;

			default:
				break;
		}

		if (error != CHANNEL_RC_OK)
			return error;

		if (!rdpdr_remove_device_by_id(context->priv, DeviceId))
			return ERROR_INVALID_DATA;
	}

	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_io_create_request(RdpdrServerContext* context, wStream* s,
                                                   UINT32 DeviceId, UINT32 FileId,
                                                   UINT32 CompletionId)
{
	const WCHAR* path;
	UINT32 DesiredAccess, AllocationSize, FileAttributes, SharedAccess, CreateDisposition,
	    CreateOptions, PathLength;

	WINPR_ASSERT(context);
	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 32))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, DesiredAccess);
	Stream_Read_UINT32(s, AllocationSize);
	Stream_Read_UINT32(s, FileAttributes);
	Stream_Read_UINT32(s, SharedAccess);
	Stream_Read_UINT32(s, CreateDisposition);
	Stream_Read_UINT32(s, CreateOptions);
	Stream_Read_UINT32(s, PathLength);

	path = rdpdr_read_ustring(context->priv->log, s, PathLength);
	if (!path && (PathLength > 0))
		return ERROR_INVALID_DATA;

	WLog_Print(context->priv->log, WLOG_WARN,
	           "[MS-RDPEFS] 2.2.1.4.1 Device Create Request (DR_CREATE_REQ) not implemented");
	WLog_Print(context->priv->log, WLOG_WARN, "TODO");

	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_io_close_request(RdpdrServerContext* context, wStream* s,
                                                  UINT32 DeviceId, UINT32 FileId,
                                                  UINT32 CompletionId)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 32))
		return ERROR_INVALID_DATA;

	Stream_Seek(s, 32); /* Padding */

	WLog_Print(context->priv->log, WLOG_WARN,
	           "[MS-RDPEFS] 2.2.1.4.2 Device Close Request (DR_CLOSE_REQ) not implemented");
	WLog_Print(context->priv->log, WLOG_WARN, "TODO");

	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_io_read_request(RdpdrServerContext* context, wStream* s,
                                                 UINT32 DeviceId, UINT32 FileId,
                                                 UINT32 CompletionId)
{
	UINT32 Length;
	UINT64 Offset;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 32))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, Length);
	Stream_Read_UINT64(s, Offset);
	Stream_Seek(s, 20); /* Padding */

	WLog_Print(context->priv->log, WLOG_WARN,
	           "[MS-RDPEFS] 2.2.1.4.3 Device Read Request (DR_READ_REQ) not implemented");
	WLog_Print(context->priv->log, WLOG_WARN, "TODO");

	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_io_write_request(RdpdrServerContext* context, wStream* s,
                                                  UINT32 DeviceId, UINT32 FileId,
                                                  UINT32 CompletionId)
{
	UINT32 Length;
	UINT64 Offset;
	const BYTE* data;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 32))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, Length);
	Stream_Read_UINT64(s, Offset);
	Stream_Seek(s, 20); /* Padding */

	data = Stream_Pointer(s);
	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, Length))
		return ERROR_INVALID_DATA;
	Stream_Seek(s, Length);

	WLog_Print(context->priv->log, WLOG_WARN,
	           "[MS-RDPEFS] 2.2.1.4.4 Device Write Request (DR_WRITE_REQ) not implemented");
	WLog_Print(context->priv->log, WLOG_WARN, "TODO");

	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_io_device_control_request(RdpdrServerContext* context, wStream* s,
                                                           UINT32 DeviceId, UINT32 FileId,
                                                           UINT32 CompletionId)
{
	UINT32 OutputBufferLength;
	UINT32 InputBufferLength;
	UINT32 IoControlCode;
	const BYTE* InputBuffer;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 32))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, OutputBufferLength);
	Stream_Read_UINT32(s, InputBufferLength);
	Stream_Read_UINT32(s, IoControlCode);
	Stream_Seek(s, 20); /* Padding */

	InputBuffer = Stream_Pointer(s);
	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, InputBufferLength))
		return ERROR_INVALID_DATA;
	Stream_Seek(s, InputBufferLength);

	WLog_Print(context->priv->log, WLOG_WARN,
	           "[MS-RDPEFS] 2.2.1.4.5 Device Control Request (DR_CONTROL_REQ) not implemented");
	WLog_Print(context->priv->log, WLOG_WARN, "TODO");

	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_io_query_volume_information_request(RdpdrServerContext* context,
                                                                     wStream* s, UINT32 DeviceId,
                                                                     UINT32 FileId,
                                                                     UINT32 CompletionId)
{
	UINT32 FsInformationClass;
	UINT32 Length;
	const BYTE* QueryVolumeBuffer;

	WINPR_ASSERT(context);
	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 32))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, FsInformationClass);
	Stream_Read_UINT32(s, Length);
	Stream_Seek(s, 24); /* Padding */

	QueryVolumeBuffer = Stream_Pointer(s);
	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, Length))
		return ERROR_INVALID_DATA;
	Stream_Seek(s, Length);

	WLog_Print(context->priv->log, WLOG_WARN,
	           "[MS-RDPEFS] 2.2.3.3.6 Server Drive Query Volume Information Request "
	           "(DR_DRIVE_QUERY_VOLUME_INFORMATION_REQ) not implemented");
	WLog_Print(context->priv->log, WLOG_WARN, "TODO");

	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_io_set_volume_information_request(RdpdrServerContext* context,
                                                                   wStream* s, UINT32 DeviceId,
                                                                   UINT32 FileId,
                                                                   UINT32 CompletionId)
{
	UINT32 FsInformationClass;
	UINT32 Length;
	const BYTE* SetVolumeBuffer;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 32))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, FsInformationClass);
	Stream_Read_UINT32(s, Length);
	Stream_Seek(s, 24); /* Padding */

	SetVolumeBuffer = Stream_Pointer(s);
	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, Length))
		return ERROR_INVALID_DATA;
	Stream_Seek(s, Length);

	WLog_Print(context->priv->log, WLOG_WARN,
	           "[MS-RDPEFS] 2.2.3.3.7 Server Drive Set Volume Information Request "
	           "(DR_DRIVE_SET_VOLUME_INFORMATION_REQ) not implemented");
	WLog_Print(context->priv->log, WLOG_WARN, "TODO");

	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_io_query_information_request(RdpdrServerContext* context,
                                                              wStream* s, UINT32 DeviceId,
                                                              UINT32 FileId, UINT32 CompletionId)
{
	UINT32 FsInformationClass;
	UINT32 Length;
	const BYTE* QueryBuffer;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 32))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, FsInformationClass);
	Stream_Read_UINT32(s, Length);
	Stream_Seek(s, 24); /* Padding */

	QueryBuffer = Stream_Pointer(s);
	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, Length))
		return ERROR_INVALID_DATA;
	Stream_Seek(s, Length);

	WLog_Print(context->priv->log, WLOG_WARN,
	           "[MS-RDPEFS] 2.2.3.3.8 Server Drive Query Information Request "
	           "(DR_DRIVE_QUERY_INFORMATION_REQ) not implemented");
	WLog_Print(context->priv->log, WLOG_WARN, "TODO");

	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_io_set_information_request(RdpdrServerContext* context, wStream* s,
                                                            UINT32 DeviceId, UINT32 FileId,
                                                            UINT32 CompletionId)
{
	UINT32 FsInformationClass;
	UINT32 Length;
	const BYTE* SetBuffer;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 32))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, FsInformationClass);
	Stream_Read_UINT32(s, Length);
	Stream_Seek(s, 24); /* Padding */

	SetBuffer = Stream_Pointer(s);
	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, Length))
		return ERROR_INVALID_DATA;
	Stream_Seek(s, Length);

	WLog_Print(context->priv->log, WLOG_WARN,
	           "[MS-RDPEFS] 2.2.3.3.9 Server Drive Set Information Request "
	           "(DR_DRIVE_SET_INFORMATION_REQ) not implemented");
	WLog_Print(context->priv->log, WLOG_WARN, "TODO");

	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_io_query_directory_request(RdpdrServerContext* context, wStream* s,
                                                            UINT32 DeviceId, UINT32 FileId,
                                                            UINT32 CompletionId)
{
	BYTE InitialQuery;
	UINT32 FsInformationClass;
	UINT32 PathLength;
	const WCHAR* Path;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 32))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, FsInformationClass);
	Stream_Read_UINT8(s, InitialQuery);
	Stream_Read_UINT32(s, PathLength);
	Stream_Seek(s, 23); /* Padding */

	Path = rdpdr_read_ustring(context->priv->log, s, PathLength);
	if (!Path && (PathLength > 0))
		return ERROR_INVALID_DATA;

	WLog_Print(context->priv->log, WLOG_WARN,
	           "[MS-RDPEFS] 2.2.3.3.10 Server Drive Query Directory Request "
	           "(DR_DRIVE_QUERY_DIRECTORY_REQ) not implemented");
	WLog_Print(context->priv->log, WLOG_WARN, "TODO");

	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_io_change_directory_request(RdpdrServerContext* context,
                                                             wStream* s, UINT32 DeviceId,
                                                             UINT32 FileId, UINT32 CompletionId)
{
	BYTE WatchTree;
	UINT32 CompletionFilter;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 32))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT8(s, WatchTree);
	Stream_Read_UINT32(s, CompletionFilter);
	Stream_Seek(s, 27); /* Padding */

	WLog_Print(context->priv->log, WLOG_WARN,
	           "[MS-RDPEFS] 2.2.3.3.11 Server Drive NotifyChange Directory Request "
	           "(DR_DRIVE_NOTIFY_CHANGE_DIRECTORY_REQ) not implemented");
	WLog_Print(context->priv->log, WLOG_WARN, "TODO");

	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_io_directory_control_request(RdpdrServerContext* context,
                                                              wStream* s, UINT32 DeviceId,
                                                              UINT32 FileId, UINT32 CompletionId,
                                                              UINT32 MinorFunction)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	switch (MinorFunction)
	{
		case IRP_MN_QUERY_DIRECTORY:
			return rdpdr_server_receive_io_query_directory_request(context, s, DeviceId, FileId,
			                                                       CompletionId);
		case IRP_MN_NOTIFY_CHANGE_DIRECTORY:
			return rdpdr_server_receive_io_change_directory_request(context, s, DeviceId, FileId,
			                                                        CompletionId);
		default:
			WLog_Print(context->priv->log, WLOG_WARN,
			           "[MS-RDPEFS] 2.2.1.4 Device I/O Request (DR_DEVICE_IOREQUEST) "
			           "MajorFunction=%s, MinorFunction=%08" PRIx32 " is not supported",
			           rdpdr_irp_string(IRP_MJ_DIRECTORY_CONTROL), MinorFunction);
			return ERROR_INVALID_DATA;
	}
}

static UINT rdpdr_server_receive_io_lock_control_request(RdpdrServerContext* context, wStream* s,
                                                         UINT32 DeviceId, UINT32 FileId,
                                                         UINT32 CompletionId)
{
	UINT32 Operation;
	UINT32 Lock;
	UINT32 NumLocks;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 32))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, Operation);
	Stream_Read_UINT32(s, Lock);
	Stream_Read_UINT32(s, NumLocks);
	Stream_Seek(s, 20); /* Padding */

	Lock &= 0x01; /* Only byte 0 is of importance */

	for (UINT32 x = 0; x < NumLocks; x++)
	{
		UINT64 Length;
		UINT64 Offset;

		if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 16))
			return ERROR_INVALID_DATA;

		Stream_Read_UINT64(s, Length);
		Stream_Read_UINT64(s, Offset);
	}

	WLog_Print(context->priv->log, WLOG_WARN,
	           "[MS-RDPEFS] 2.2.3.3.12 Server Drive Lock Control Request (DR_DRIVE_LOCK_REQ) "
	           "not implemented");
	WLog_Print(context->priv->log, WLOG_WARN, "TODO");

	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_device_io_request(RdpdrServerContext* context, wStream* s,
                                                   const RDPDR_HEADER* header)
{
	UINT32 DeviceId;
	UINT32 FileId;
	UINT32 CompletionId;
	UINT32 MajorFunction;
	UINT32 MinorFunction;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(header);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 20))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, DeviceId);
	Stream_Read_UINT32(s, FileId);
	Stream_Read_UINT32(s, CompletionId);
	Stream_Read_UINT32(s, MajorFunction);
	Stream_Read_UINT32(s, MinorFunction);
	if ((MinorFunction != 0) && (MajorFunction != IRP_MJ_DIRECTORY_CONTROL))
		WLog_Print(context->priv->log, WLOG_WARN,
		           "[MS-RDPEFS] 2.2.1.4 Device I/O Request (DR_DEVICE_IOREQUEST) MajorFunction=%s, "
		           "MinorFunction=0x%08" PRIx32 " != 0",
		           rdpdr_irp_string(MajorFunction), MinorFunction);

	switch (MajorFunction)
	{
		case IRP_MJ_CREATE:
			return rdpdr_server_receive_io_create_request(context, s, DeviceId, FileId,
			                                              CompletionId);
		case IRP_MJ_CLOSE:
			return rdpdr_server_receive_io_close_request(context, s, DeviceId, FileId,
			                                             CompletionId);
		case IRP_MJ_READ:
			return rdpdr_server_receive_io_read_request(context, s, DeviceId, FileId, CompletionId);
		case IRP_MJ_WRITE:
			return rdpdr_server_receive_io_write_request(context, s, DeviceId, FileId,
			                                             CompletionId);
		case IRP_MJ_DEVICE_CONTROL:
			return rdpdr_server_receive_io_device_control_request(context, s, DeviceId, FileId,
			                                                      CompletionId);
		case IRP_MJ_QUERY_VOLUME_INFORMATION:
			return rdpdr_server_receive_io_query_volume_information_request(context, s, DeviceId,
			                                                                FileId, CompletionId);
		case IRP_MJ_QUERY_INFORMATION:
			return rdpdr_server_receive_io_query_information_request(context, s, DeviceId, FileId,
			                                                         CompletionId);
		case IRP_MJ_SET_INFORMATION:
			return rdpdr_server_receive_io_set_information_request(context, s, DeviceId, FileId,
			                                                       CompletionId);
		case IRP_MJ_DIRECTORY_CONTROL:
			return rdpdr_server_receive_io_directory_control_request(context, s, DeviceId, FileId,
			                                                         CompletionId, MinorFunction);
		case IRP_MJ_LOCK_CONTROL:
			return rdpdr_server_receive_io_lock_control_request(context, s, DeviceId, FileId,
			                                                    CompletionId);
		default:
			WLog_Print(
			    context->priv->log, WLOG_WARN,
			    "[MS-RDPEFS] 2.2.1.4 Device I/O Request (DR_DEVICE_IOREQUEST) not implemented");
			WLog_Print(context->priv->log, WLOG_WARN,
			           "got DeviceId=0x%08" PRIx32 ", FileId=0x%08" PRIx32
			           ", CompletionId=0x%08" PRIx32 ", MajorFunction=0x%08" PRIx32
			           ", MinorFunction=0x%08" PRIx32,
			           DeviceId, FileId, CompletionId, MajorFunction, MinorFunction);
			return ERROR_INVALID_DATA;
	}
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_receive_device_io_completion(RdpdrServerContext* context, wStream* s,
                                                      const RDPDR_HEADER* header)
{
	UINT32 deviceId;
	UINT32 completionId;
	UINT32 ioStatus;
	RDPDR_IRP* irp;
	UINT error = CHANNEL_RC_OK;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(s);
	WINPR_ASSERT(header);

	WINPR_UNUSED(header);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 12))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, deviceId);
	Stream_Read_UINT32(s, completionId);
	Stream_Read_UINT32(s, ioStatus);
	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "deviceId=%" PRIu32 ", completionId=0x%" PRIx32 ", ioStatus=0x%" PRIx32 "", deviceId,
	           completionId, ioStatus);
	irp = rdpdr_server_dequeue_irp(context, completionId);

	if (!irp)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "IRP not found for completionId=0x%" PRIx32 "",
		           completionId);
		return CHANNEL_RC_OK;
	}

	/* Invoke the callback. */
	if (irp->Callback)
	{
		error = (*irp->Callback)(context, s, irp, deviceId, completionId, ioStatus);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_send_user_logged_on(RdpdrServerContext* context)
{
	wStream* s;
	RDPDR_HEADER header = { 0 };

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	header.Component = RDPDR_CTYP_CORE;
	header.PacketId = PAKID_CORE_USER_LOGGEDON;
	s = Stream_New(NULL, RDPDR_HEADER_LENGTH);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, header.Component); /* Component (2 bytes) */
	Stream_Write_UINT16(s, header.PacketId);  /* PacketId (2 bytes) */
	return rdpdr_seal_send_free_request(context, s);
}

static UINT rdpdr_server_receive_prn_cache_add_printer(RdpdrServerContext* context, wStream* s)
{
	char PortDosName[9] = { 0 };
	UINT32 PnPNameLen;
	UINT32 DriverNameLen;
	UINT32 PrinterNameLen;
	UINT32 CachedFieldsLen;
	const WCHAR* PnPName;
	const WCHAR* DriverName;
	const WCHAR* PrinterName;
	const BYTE* config;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 24))
		return ERROR_INVALID_DATA;

	Stream_Read(s, PortDosName, 8);
	Stream_Read_UINT32(s, PnPNameLen);
	Stream_Read_UINT32(s, DriverNameLen);
	Stream_Read_UINT32(s, PrinterNameLen);
	Stream_Read_UINT32(s, CachedFieldsLen);

	PnPName = rdpdr_read_ustring(context->priv->log, s, PnPNameLen);
	if (!PnPName && (PnPNameLen > 0))
		return ERROR_INVALID_DATA;
	DriverName = rdpdr_read_ustring(context->priv->log, s, DriverNameLen);
	if (!DriverName && (DriverNameLen > 0))
		return ERROR_INVALID_DATA;
	PrinterName = rdpdr_read_ustring(context->priv->log, s, PrinterNameLen);
	if (!PrinterName && (PrinterNameLen > 0))
		return ERROR_INVALID_DATA;

	config = Stream_Pointer(s);
	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, CachedFieldsLen))
		return ERROR_INVALID_DATA;
	Stream_Seek(s, CachedFieldsLen);

	WLog_Print(context->priv->log, WLOG_WARN,
	           "[MS-RDPEPC] 2.2.2.3 Add Printer Cachedata (DR_PRN_ADD_CACHEDATA) not implemented");
	WLog_Print(context->priv->log, WLOG_WARN, "TODO");
	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_prn_cache_update_printer(RdpdrServerContext* context, wStream* s)
{
	UINT32 PrinterNameLen;
	UINT32 CachedFieldsLen;
	const WCHAR* PrinterName;
	const BYTE* config;

	WINPR_ASSERT(context);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 8))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, PrinterNameLen);
	Stream_Read_UINT32(s, CachedFieldsLen);

	PrinterName = rdpdr_read_ustring(context->priv->log, s, PrinterNameLen);
	if (!PrinterName && (PrinterNameLen > 0))
		return ERROR_INVALID_DATA;

	config = Stream_Pointer(s);
	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, CachedFieldsLen))
		return ERROR_INVALID_DATA;
	Stream_Seek(s, CachedFieldsLen);

	WLog_Print(
	    context->priv->log, WLOG_WARN,
	    "[MS-RDPEPC] 2.2.2.4 Update Printer Cachedata (DR_PRN_UPDATE_CACHEDATA) not implemented");
	WLog_Print(context->priv->log, WLOG_WARN, "TODO");
	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_prn_cache_delete_printer(RdpdrServerContext* context, wStream* s)
{
	UINT32 PrinterNameLen;
	const WCHAR* PrinterName;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, PrinterNameLen);

	PrinterName = rdpdr_read_ustring(context->priv->log, s, PrinterNameLen);
	if (!PrinterName && (PrinterNameLen > 0))
		return ERROR_INVALID_DATA;

	WLog_Print(
	    context->priv->log, WLOG_WARN,
	    "[MS-RDPEPC] 2.2.2.5 Delete Printer Cachedata (DR_PRN_DELETE_CACHEDATA) not implemented");
	WLog_Print(context->priv->log, WLOG_WARN, "TODO");
	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_prn_cache_rename_cachedata(RdpdrServerContext* context, wStream* s)
{
	UINT32 OldPrinterNameLen;
	UINT32 NewPrinterNameLen;
	const WCHAR* OldPrinterName;
	const WCHAR* NewPrinterName;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 8))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, OldPrinterNameLen);
	Stream_Read_UINT32(s, NewPrinterNameLen);

	OldPrinterName = rdpdr_read_ustring(context->priv->log, s, OldPrinterNameLen);
	if (!OldPrinterName && (OldPrinterNameLen > 0))
		return ERROR_INVALID_DATA;
	NewPrinterName = rdpdr_read_ustring(context->priv->log, s, NewPrinterNameLen);
	if (!NewPrinterName && (NewPrinterNameLen > 0))
		return ERROR_INVALID_DATA;

	WLog_Print(
	    context->priv->log, WLOG_WARN,
	    "[MS-RDPEPC] 2.2.2.6 Rename Printer Cachedata (DR_PRN_RENAME_CACHEDATA) not implemented");
	WLog_Print(context->priv->log, WLOG_WARN, "TODO");
	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_prn_cache_data_request(RdpdrServerContext* context, wStream* s,
                                                        const RDPDR_HEADER* header)
{
	UINT32 EventId;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(header);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, EventId);
	switch (EventId)
	{
		case RDPDR_ADD_PRINTER_EVENT:
			return rdpdr_server_receive_prn_cache_add_printer(context, s);
		case RDPDR_UPDATE_PRINTER_EVENT:
			return rdpdr_server_receive_prn_cache_update_printer(context, s);
		case RDPDR_DELETE_PRINTER_EVENT:
			return rdpdr_server_receive_prn_cache_delete_printer(context, s);
		case RDPDR_RENAME_PRINTER_EVENT:
			return rdpdr_server_receive_prn_cache_rename_cachedata(context, s);
		default:
			WLog_Print(context->priv->log, WLOG_WARN,
			           "[MS-RDPEPC] PAKID_PRN_CACHE_DATA unknown EventId=0x%08" PRIx32, EventId);
			return ERROR_INVALID_DATA;
	}
}

static UINT rdpdr_server_receive_prn_using_xps_request(RdpdrServerContext* context, wStream* s,
                                                       const RDPDR_HEADER* header)
{
	UINT32 PrinterId;
	UINT32 Flags;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(header);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 8))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, PrinterId);
	Stream_Read_UINT32(s, Flags);

	WLog_Print(
	    context->priv->log, WLOG_WARN,
	    "[MS-RDPEPC] 2.2.2.2 Server Printer Set XPS Mode (DR_PRN_USING_XPS) not implemented");
	WLog_Print(context->priv->log, WLOG_WARN, "PrinterId=0x%08" PRIx32 ", Flags=0x%08" PRIx32,
	           PrinterId, Flags);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_receive_pdu(RdpdrServerContext* context, wStream* s,
                                     const RDPDR_HEADER* header)
{
	UINT error = ERROR_INVALID_DATA;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(s);
	WINPR_ASSERT(header);

	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "receiving message {Component %s[%04" PRIx16 "], PacketId %s[%04" PRIx16 "]",
	           rdpdr_component_string(header->Component), header->Component,
	           rdpdr_packetid_string(header->PacketId), header->PacketId);

	if (header->Component == RDPDR_CTYP_CORE)
	{
		switch (header->PacketId)
		{
			case PAKID_CORE_SERVER_ANNOUNCE:
				WLog_Print(context->priv->log, WLOG_ERROR,
				           "[MS-RDPEFS] 2.2.2.2 Server Announce Request "
				           "(DR_CORE_SERVER_ANNOUNCE_REQ) must not be sent by a client!");
				break;

			case PAKID_CORE_CLIENTID_CONFIRM:
				error = rdpdr_server_receive_announce_response(context, s, header);
				break;

			case PAKID_CORE_CLIENT_NAME:
				error = rdpdr_server_receive_client_name_request(context, s, header);
				if (error == CHANNEL_RC_OK)
					error = rdpdr_server_send_core_capability_request(context);
				if (error == CHANNEL_RC_OK)
					error = rdpdr_server_send_client_id_confirm(context);
				break;

			case PAKID_CORE_USER_LOGGEDON:
				WLog_Print(context->priv->log, WLOG_ERROR,
				           "[MS-RDPEFS] 2.2.2.5 Server User Logged On (DR_CORE_USER_LOGGEDON) "
				           "must not be sent by a client!");
				break;

			case PAKID_CORE_SERVER_CAPABILITY:
				WLog_Print(context->priv->log, WLOG_ERROR,
				           "[MS-RDPEFS] 2.2.2.7 Server Core Capability Request "
				           "(DR_CORE_CAPABILITY_REQ) must not be sent by a client!");
				break;

			case PAKID_CORE_CLIENT_CAPABILITY:
				error = rdpdr_server_receive_core_capability_response(context, s, header);
				if (error == CHANNEL_RC_OK)
				{
					if (context->priv->UserLoggedOnPdu)
						error = rdpdr_server_send_user_logged_on(context);
				}

				break;

			case PAKID_CORE_DEVICELIST_ANNOUNCE:
				error = rdpdr_server_receive_device_list_announce_request(context, s, header);
				break;

			case PAKID_CORE_DEVICELIST_REMOVE:
				error = rdpdr_server_receive_device_list_remove_request(context, s, header);
				break;

			case PAKID_CORE_DEVICE_REPLY:
				WLog_Print(context->priv->log, WLOG_ERROR,
				           "[MS-RDPEFS] 2.2.2.1 Server Device Announce Response "
				           "(DR_CORE_DEVICE_ANNOUNCE_RSP) must not be sent by a client!");
				break;

			case PAKID_CORE_DEVICE_IOREQUEST:
				error = rdpdr_server_receive_device_io_request(context, s, header);
				break;

			case PAKID_CORE_DEVICE_IOCOMPLETION:
				error = rdpdr_server_receive_device_io_completion(context, s, header);
				break;

			default:
				WLog_Print(context->priv->log, WLOG_WARN,
				           "Unknown RDPDR_HEADER.Component: %s [0x%04" PRIx16 "], PacketId: %s",
				           rdpdr_component_string(header->Component), header->Component,
				           rdpdr_packetid_string(header->PacketId));
				break;
		}
	}
	else if (header->Component == RDPDR_CTYP_PRN)
	{
		switch (header->PacketId)
		{
			case PAKID_PRN_CACHE_DATA:
				error = rdpdr_server_receive_prn_cache_data_request(context, s, header);
				break;

			case PAKID_PRN_USING_XPS:
				error = rdpdr_server_receive_prn_using_xps_request(context, s, header);
				break;

			default:
				WLog_Print(context->priv->log, WLOG_WARN,
				           "Unknown RDPDR_HEADER.Component: %s [0x%04" PRIx16 "], PacketId: %s",
				           rdpdr_component_string(header->Component), header->Component,
				           rdpdr_packetid_string(header->PacketId));
				break;
		}
	}
	else
	{
		WLog_Print(context->priv->log, WLOG_WARN,
		           "Unknown RDPDR_HEADER.Component: %s [0x%04" PRIx16 "], PacketId: %s",
		           rdpdr_component_string(header->Component), header->Component,
		           rdpdr_packetid_string(header->PacketId));
	}

	return IFCALLRESULT(error, context->ReceivePDU, context, header, error);
}

static DWORD WINAPI rdpdr_server_thread(LPVOID arg)
{
	DWORD status = 0;
	DWORD nCount = 0;
	void* buffer = NULL;
	HANDLE events[8] = { 0 };
	HANDLE ChannelEvent = NULL;
	DWORD BytesReturned = 0;
	UINT error;
	RdpdrServerContext* context = (RdpdrServerContext*)arg;
	wStream* s = Stream_New(NULL, 4096);

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "Stream_New failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto out;
	}

	if (WTSVirtualChannelQuery(context->priv->ChannelHandle, WTSVirtualEventHandle, &buffer,
	                           &BytesReturned) == TRUE)
	{
		if (BytesReturned == sizeof(HANDLE))
			CopyMemory(&ChannelEvent, buffer, sizeof(HANDLE));

		WTSFreeMemory(buffer);
	}

	nCount = 0;
	events[nCount++] = ChannelEvent;
	events[nCount++] = context->priv->StopEvent;

	if ((error = rdpdr_server_send_announce_request(context)))
	{
		WLog_Print(context->priv->log, WLOG_ERROR,
		           "rdpdr_server_send_announce_request failed with error %" PRIu32 "!", error);
		goto out_stream;
	}

	while (1)
	{
		size_t capacity;
		BytesReturned = 0;
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_Print(context->priv->log, WLOG_ERROR,
			           "WaitForMultipleObjects failed with error %" PRIu32 "!", error);
			goto out_stream;
		}

		status = WaitForSingleObject(context->priv->StopEvent, 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_Print(context->priv->log, WLOG_ERROR,
			           "WaitForSingleObject failed with error %" PRIu32 "!", error);
			goto out_stream;
		}

		if (status == WAIT_OBJECT_0)
			break;

		if (!WTSVirtualChannelRead(context->priv->ChannelHandle, 0, NULL, 0, &BytesReturned))
		{
			WLog_Print(context->priv->log, WLOG_ERROR, "WTSVirtualChannelRead failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}
		if (!Stream_EnsureRemainingCapacity(s, BytesReturned))
		{
			WLog_Print(context->priv->log, WLOG_ERROR, "Stream_EnsureRemainingCapacity failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		capacity = MIN(Stream_Capacity(s), ULONG_MAX);
		if (!WTSVirtualChannelRead(context->priv->ChannelHandle, 0, (PCHAR)Stream_Buffer(s),
		                           (ULONG)capacity, &BytesReturned))
		{
			WLog_Print(context->priv->log, WLOG_ERROR, "WTSVirtualChannelRead failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		if (BytesReturned >= RDPDR_HEADER_LENGTH)
		{
			Stream_SetPosition(s, 0);
			Stream_SetLength(s, BytesReturned);

			while (Stream_GetRemainingLength(s) >= RDPDR_HEADER_LENGTH)
			{
				RDPDR_HEADER header = { 0 };

				Stream_Read_UINT16(s, header.Component); /* Component (2 bytes) */
				Stream_Read_UINT16(s, header.PacketId);  /* PacketId (2 bytes) */

				if ((error = rdpdr_server_receive_pdu(context, s, &header)))
				{
					WLog_Print(context->priv->log, WLOG_ERROR,
					           "rdpdr_server_receive_pdu failed with error %" PRIu32 "!", error);
					goto out_stream;
				}
			}
		}
	}

out_stream:
	Stream_Free(s, TRUE);
out:

	if (error && context->rdpcontext)
		setChannelError(context->rdpcontext, error, "rdpdr_server_thread reported an error");

	ExitThread(error);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_start(RdpdrServerContext* context)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	context->priv->ChannelHandle =
	    WTSVirtualChannelOpen(context->vcm, WTS_CURRENT_SESSION, RDPDR_SVC_CHANNEL_NAME);

	if (!context->priv->ChannelHandle)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "WTSVirtualChannelOpen failed!");
		return CHANNEL_RC_BAD_CHANNEL;
	}

	if (!(context->priv->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "CreateEvent failed!");
		return ERROR_INTERNAL_ERROR;
	}

	if (!(context->priv->Thread =
	          CreateThread(NULL, 0, rdpdr_server_thread, (void*)context, 0, NULL)))
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "CreateThread failed!");
		CloseHandle(context->priv->StopEvent);
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
static UINT rdpdr_server_stop(RdpdrServerContext* context)
{
	UINT error;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	if (context->priv->StopEvent)
	{
		SetEvent(context->priv->StopEvent);

		if (WaitForSingleObject(context->priv->Thread, INFINITE) == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_Print(context->priv->log, WLOG_ERROR,
			           "WaitForSingleObject failed with error %" PRIu32 "!", error);
			return error;
		}

		CloseHandle(context->priv->Thread);
		context->priv->Thread = NULL;
		CloseHandle(context->priv->StopEvent);
		context->priv->StopEvent = NULL;
	}

	if (context->priv->ChannelHandle)
	{
		WTSVirtualChannelClose(context->priv->ChannelHandle);
		context->priv->ChannelHandle = NULL;
	}
	return CHANNEL_RC_OK;
}

static void rdpdr_server_write_device_iorequest(wStream* s, UINT32 deviceId, UINT32 fileId,
                                                UINT32 completionId, UINT32 majorFunction,
                                                UINT32 minorFunction)
{
	Stream_Write_UINT16(s, RDPDR_CTYP_CORE);             /* Component (2 bytes) */
	Stream_Write_UINT16(s, PAKID_CORE_DEVICE_IOREQUEST); /* PacketId (2 bytes) */
	Stream_Write_UINT32(s, deviceId);                    /* DeviceId (4 bytes) */
	Stream_Write_UINT32(s, fileId);                      /* FileId (4 bytes) */
	Stream_Write_UINT32(s, completionId);                /* CompletionId (4 bytes) */
	Stream_Write_UINT32(s, majorFunction);               /* MajorFunction (4 bytes) */
	Stream_Write_UINT32(s, minorFunction);               /* MinorFunction (4 bytes) */
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_read_file_directory_information(wLog* log, wStream* s,
                                                         FILE_DIRECTORY_INFORMATION* fdi)
{
	UINT32 fileNameLength;
	WINPR_ASSERT(fdi);
	ZeroMemory(fdi, sizeof(FILE_DIRECTORY_INFORMATION));

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 64))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, fdi->NextEntryOffset);        /* NextEntryOffset (4 bytes) */
	Stream_Read_UINT32(s, fdi->FileIndex);              /* FileIndex (4 bytes) */
	Stream_Read_INT64(s, fdi->CreationTime.QuadPart);   /* CreationTime (8 bytes) */
	Stream_Read_INT64(s, fdi->LastAccessTime.QuadPart); /* LastAccessTime (8 bytes) */
	Stream_Read_INT64(s, fdi->LastWriteTime.QuadPart);  /* LastWriteTime (8 bytes) */
	Stream_Read_INT64(s, fdi->ChangeTime.QuadPart);     /* ChangeTime (8 bytes) */
	Stream_Read_INT64(s, fdi->EndOfFile.QuadPart);      /* EndOfFile (8 bytes) */
	Stream_Read_INT64(s, fdi->AllocationSize.QuadPart); /* AllocationSize (8 bytes) */
	Stream_Read_UINT32(s, fdi->FileAttributes);         /* FileAttributes (4 bytes) */
	Stream_Read_UINT32(s, fileNameLength);              /* FileNameLength (4 bytes) */

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, fileNameLength))
		return ERROR_INVALID_DATA;

	if (Stream_Read_UTF16_String_As_UTF8_Buffer(s, fileNameLength / sizeof(WCHAR), fdi->FileName,
	                                            ARRAYSIZE(fdi->FileName)) < 0)
		return ERROR_INVALID_DATA;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_send_device_create_request(RdpdrServerContext* context, UINT32 deviceId,
                                                    UINT32 completionId, const char* path,
                                                    UINT32 desiredAccess, UINT32 createOptions,
                                                    UINT32 createDisposition)
{
	size_t pathLength;
	wStream* s;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "RdpdrServerSendDeviceCreateRequest: deviceId=%" PRIu32
	           ", path=%s, desiredAccess=0x%" PRIx32 " createOptions=0x%" PRIx32
	           " createDisposition=0x%" PRIx32 "",
	           deviceId, path, desiredAccess, createOptions, createDisposition);
	/* Compute the required Unicode size. */
	pathLength = (strlen(path) + 1U) * sizeof(WCHAR);
	s = Stream_New(NULL, 256U + pathLength);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rdpdr_server_write_device_iorequest(s, deviceId, 0, completionId, IRP_MJ_CREATE, 0);
	Stream_Write_UINT32(s, desiredAccess); /* DesiredAccess (4 bytes) */
	Stream_Write_UINT32(s, 0);             /* AllocationSize (8 bytes) */
	Stream_Write_UINT32(s, 0);
	Stream_Write_UINT32(s, 0);                 /* FileAttributes (4 bytes) */
	Stream_Write_UINT32(s, 3);                 /* SharedAccess (4 bytes) */
	Stream_Write_UINT32(s, createDisposition); /* CreateDisposition (4 bytes) */
	Stream_Write_UINT32(s, createOptions);     /* CreateOptions (4 bytes) */
	WINPR_ASSERT(pathLength <= UINT32_MAX);
	Stream_Write_UINT32(s, (UINT32)pathLength); /* PathLength (4 bytes) */
	                                            /* Convert the path to Unicode. */
	if (Stream_Write_UTF16_String_From_UTF8(s, pathLength / sizeof(WCHAR), path,
	                                        pathLength / sizeof(WCHAR), TRUE) < 0)
		return ERROR_INTERNAL_ERROR;
	return rdpdr_seal_send_free_request(context, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_send_device_close_request(RdpdrServerContext* context, UINT32 deviceId,
                                                   UINT32 fileId, UINT32 completionId)
{
	wStream* s;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "RdpdrServerSendDeviceCloseRequest: deviceId=%" PRIu32 ", fileId=%" PRIu32 "",
	           deviceId, fileId);
	s = Stream_New(NULL, 128);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rdpdr_server_write_device_iorequest(s, deviceId, fileId, completionId, IRP_MJ_CLOSE, 0);
	Stream_Zero(s, 32); /* Padding (32 bytes) */
	return rdpdr_seal_send_free_request(context, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_send_device_read_request(RdpdrServerContext* context, UINT32 deviceId,
                                                  UINT32 fileId, UINT32 completionId, UINT32 length,
                                                  UINT32 offset)
{
	wStream* s;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "RdpdrServerSendDeviceReadRequest: deviceId=%" PRIu32 ", fileId=%" PRIu32
	           ", length=%" PRIu32 ", offset=%" PRIu32 "",
	           deviceId, fileId, length, offset);
	s = Stream_New(NULL, 128);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rdpdr_server_write_device_iorequest(s, deviceId, fileId, completionId, IRP_MJ_READ, 0);
	Stream_Write_UINT32(s, length); /* Length (4 bytes) */
	Stream_Write_UINT32(s, offset); /* Offset (8 bytes) */
	Stream_Write_UINT32(s, 0);
	Stream_Zero(s, 20); /* Padding (20 bytes) */
	return rdpdr_seal_send_free_request(context, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_send_device_write_request(RdpdrServerContext* context, UINT32 deviceId,
                                                   UINT32 fileId, UINT32 completionId,
                                                   const char* data, UINT32 length, UINT32 offset)
{
	wStream* s;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "RdpdrServerSendDeviceWriteRequest: deviceId=%" PRIu32 ", fileId=%" PRIu32
	           ", length=%" PRIu32 ", offset=%" PRIu32 "",
	           deviceId, fileId, length, offset);
	s = Stream_New(NULL, 64 + length);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rdpdr_server_write_device_iorequest(s, deviceId, fileId, completionId, IRP_MJ_WRITE, 0);
	Stream_Write_UINT32(s, length); /* Length (4 bytes) */
	Stream_Write_UINT32(s, offset); /* Offset (8 bytes) */
	Stream_Write_UINT32(s, 0);
	Stream_Zero(s, 20);            /* Padding (20 bytes) */
	Stream_Write(s, data, length); /* WriteData (variable) */
	return rdpdr_seal_send_free_request(context, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_send_device_query_directory_request(RdpdrServerContext* context,
                                                             UINT32 deviceId, UINT32 fileId,
                                                             UINT32 completionId, const char* path)
{
	size_t pathLength;
	wStream* s;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "RdpdrServerSendDeviceQueryDirectoryRequest: deviceId=%" PRIu32 ", fileId=%" PRIu32
	           ", path=%s",
	           deviceId, fileId, path);
	/* Compute the required Unicode size. */
	pathLength = path ? (strlen(path) + 1) * sizeof(WCHAR) : 0;
	s = Stream_New(NULL, 64 + pathLength);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rdpdr_server_write_device_iorequest(s, deviceId, fileId, completionId, IRP_MJ_DIRECTORY_CONTROL,
	                                    IRP_MN_QUERY_DIRECTORY);
	Stream_Write_UINT32(s, FileDirectoryInformation); /* FsInformationClass (4 bytes) */
	Stream_Write_UINT8(s, path ? 1 : 0);              /* InitialQuery (1 byte) */
	WINPR_ASSERT(pathLength <= UINT32_MAX);
	Stream_Write_UINT32(s, (UINT32)pathLength); /* PathLength (4 bytes) */
	Stream_Zero(s, 23);                         /* Padding (23 bytes) */

	/* Convert the path to Unicode. */
	if (pathLength > 0)
	{
		if (Stream_Write_UTF16_String_From_UTF8(s, pathLength / sizeof(WCHAR), path,
		                                        pathLength / sizeof(WCHAR), TRUE) < 0)
			return ERROR_INTERNAL_ERROR;
	}

	return rdpdr_seal_send_free_request(context, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_send_device_file_rename_request(RdpdrServerContext* context,
                                                         UINT32 deviceId, UINT32 fileId,
                                                         UINT32 completionId, const char* path)
{
	size_t pathLength;
	wStream* s;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "RdpdrServerSendDeviceFileNameRequest: deviceId=%" PRIu32 ", fileId=%" PRIu32
	           ", path=%s",
	           deviceId, fileId, path);
	/* Compute the required Unicode size. */
	pathLength = path ? (strlen(path) + 1) * sizeof(WCHAR) : 0;
	s = Stream_New(NULL, 64 + pathLength);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rdpdr_server_write_device_iorequest(s, deviceId, fileId, completionId, IRP_MJ_SET_INFORMATION,
	                                    0);
	Stream_Write_UINT32(s, FileRenameInformation); /* FsInformationClass (4 bytes) */
	WINPR_ASSERT(pathLength <= UINT32_MAX - 6U);
	Stream_Write_UINT32(s, (UINT32)pathLength + 6U); /* Length (4 bytes) */
	Stream_Zero(s, 24);                              /* Padding (24 bytes) */
	/* RDP_FILE_RENAME_INFORMATION */
	Stream_Write_UINT8(s, 0);                   /* ReplaceIfExists (1 byte) */
	Stream_Write_UINT8(s, 0);                   /* RootDirectory (1 byte) */
	Stream_Write_UINT32(s, (UINT32)pathLength); /* FileNameLength (4 bytes) */

	/* Convert the path to Unicode. */
	if (pathLength > 0)
	{
		if (Stream_Write_UTF16_String_From_UTF8(s, pathLength / sizeof(WCHAR), path,
		                                        pathLength / sizeof(WCHAR), TRUE) < 0)
			return ERROR_INTERNAL_ERROR;
	}

	return rdpdr_seal_send_free_request(context, s);
}

static void rdpdr_server_convert_slashes(char* path, int size)
{
	int i;
	WINPR_ASSERT(path || (size <= 0));

	for (i = 0; (i < size) && (path[i] != '\0'); i++)
	{
		if (path[i] == '/')
			path[i] = '\\';
	}
}

/*************************************************
 * Drive Create Directory
 ************************************************/

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_create_directory_callback2(RdpdrServerContext* context, wStream* s,
                                                          RDPDR_IRP* irp, UINT32 deviceId,
                                                          UINT32 completionId, UINT32 ioStatus)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(s);
	WINPR_ASSERT(irp);
	WINPR_UNUSED(s);

	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "RdpdrServerDriveCreateDirectoryCallback2: deviceId=%" PRIu32
	           ", completionId=%" PRIu32 ", ioStatus=0x%" PRIx32 "",
	           deviceId, completionId, ioStatus);
	/* Invoke the create directory completion routine. */
	context->OnDriveCreateDirectoryComplete(context, irp->CallbackData, ioStatus);
	/* Destroy the IRP. */
	rdpdr_server_irp_free(irp);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_create_directory_callback1(RdpdrServerContext* context, wStream* s,
                                                          RDPDR_IRP* irp, UINT32 deviceId,
                                                          UINT32 completionId, UINT32 ioStatus)
{
	UINT32 fileId;
	UINT8 information;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(s);
	WINPR_ASSERT(irp);
	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "RdpdrServerDriveCreateDirectoryCallback1: deviceId=%" PRIu32
	           ", completionId=%" PRIu32 ", ioStatus=0x%" PRIx32 "",
	           deviceId, completionId, ioStatus);

	if (ioStatus != STATUS_SUCCESS)
	{
		/* Invoke the create directory completion routine. */
		context->OnDriveCreateDirectoryComplete(context, irp->CallbackData, ioStatus);
		/* Destroy the IRP. */
		rdpdr_server_irp_free(irp);
		return CHANNEL_RC_OK;
	}

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 5))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, fileId);     /* FileId (4 bytes) */
	Stream_Read_UINT8(s, information); /* Information (1 byte) */
	/* Setup the IRP. */
	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_create_directory_callback2;
	irp->DeviceId = deviceId;
	irp->FileId = fileId;

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to close the file */
	return rdpdr_server_send_device_close_request(context, deviceId, fileId, irp->CompletionId);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_create_directory(RdpdrServerContext* context, void* callbackData,
                                                UINT32 deviceId, const char* path)
{
	RDPDR_IRP* irp;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(callbackData);
	WINPR_ASSERT(path);
	irp = rdpdr_server_irp_new();

	if (!irp)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpdr_server_irp_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_create_directory_callback1;
	irp->CallbackData = callbackData;
	irp->DeviceId = deviceId;
	strncpy(irp->PathName, path, sizeof(irp->PathName) - 1);
	rdpdr_server_convert_slashes(irp->PathName, sizeof(irp->PathName));

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to open the file. */
	return rdpdr_server_send_device_create_request(
	    context, deviceId, irp->CompletionId, irp->PathName, FILE_READ_DATA | SYNCHRONIZE,
	    FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT, FILE_CREATE);
}

/*************************************************
 * Drive Delete Directory
 ************************************************/

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_delete_directory_callback2(RdpdrServerContext* context, wStream* s,
                                                          RDPDR_IRP* irp, UINT32 deviceId,
                                                          UINT32 completionId, UINT32 ioStatus)
{
	WINPR_UNUSED(s);
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);

	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "RdpdrServerDriveDeleteDirectoryCallback2: deviceId=%" PRIu32
	           ", completionId=%" PRIu32 ", ioStatus=0x%" PRIx32 "",
	           deviceId, completionId, ioStatus);
	/* Invoke the delete directory completion routine. */
	context->OnDriveDeleteDirectoryComplete(context, irp->CallbackData, ioStatus);
	/* Destroy the IRP. */
	rdpdr_server_irp_free(irp);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_delete_directory_callback1(RdpdrServerContext* context, wStream* s,
                                                          RDPDR_IRP* irp, UINT32 deviceId,
                                                          UINT32 completionId, UINT32 ioStatus)
{
	UINT32 fileId;
	UINT8 information;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);
	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "RdpdrServerDriveDeleteDirectoryCallback1: deviceId=%" PRIu32
	           ", completionId=%" PRIu32 ", ioStatus=0x%" PRIx32 "",
	           deviceId, completionId, ioStatus);

	if (ioStatus != STATUS_SUCCESS)
	{
		/* Invoke the delete directory completion routine. */
		context->OnDriveDeleteFileComplete(context, irp->CallbackData, ioStatus);
		/* Destroy the IRP. */
		rdpdr_server_irp_free(irp);
		return CHANNEL_RC_OK;
	}

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 5))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, fileId);     /* FileId (4 bytes) */
	Stream_Read_UINT8(s, information); /* Information (1 byte) */
	/* Setup the IRP. */
	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_delete_directory_callback2;
	irp->DeviceId = deviceId;
	irp->FileId = fileId;

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to close the file */
	return rdpdr_server_send_device_close_request(context, deviceId, fileId, irp->CompletionId);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_delete_directory(RdpdrServerContext* context, void* callbackData,
                                                UINT32 deviceId, const char* path)
{
	RDPDR_IRP* irp = rdpdr_server_irp_new();
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);

	if (!irp)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpdr_server_irp_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_delete_directory_callback1;
	irp->CallbackData = callbackData;
	irp->DeviceId = deviceId;
	strncpy(irp->PathName, path, sizeof(irp->PathName) - 1);
	rdpdr_server_convert_slashes(irp->PathName, sizeof(irp->PathName));

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to open the file. */
	return rdpdr_server_send_device_create_request(
	    context, deviceId, irp->CompletionId, irp->PathName, DELETE | SYNCHRONIZE,
	    FILE_DIRECTORY_FILE | FILE_DELETE_ON_CLOSE | FILE_SYNCHRONOUS_IO_NONALERT, FILE_OPEN);
}

/*************************************************
 * Drive Query Directory
 ************************************************/

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_query_directory_callback2(RdpdrServerContext* context, wStream* s,
                                                         RDPDR_IRP* irp, UINT32 deviceId,
                                                         UINT32 completionId, UINT32 ioStatus)
{
	UINT error;
	UINT32 length;
	FILE_DIRECTORY_INFORMATION fdi = { 0 };

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);
	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "RdpdrServerDriveQueryDirectoryCallback2: deviceId=%" PRIu32
	           ", completionId=%" PRIu32 ", ioStatus=0x%" PRIx32 "",
	           deviceId, completionId, ioStatus);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, length); /* Length (4 bytes) */

	if (length > 0)
	{
		if ((error = rdpdr_server_read_file_directory_information(context->priv->log, s, &fdi)))
		{
			WLog_Print(context->priv->log, WLOG_ERROR,
			           "rdpdr_server_read_file_directory_information failed with error %" PRIu32
			           "!",
			           error);
			return error;
		}
	}
	else
	{
		if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 1))
			return ERROR_INVALID_DATA;

		Stream_Seek(s, 1); /* Padding (1 byte) */
	}

	if (ioStatus == STATUS_SUCCESS)
	{
		/* Invoke the query directory completion routine. */
		context->OnDriveQueryDirectoryComplete(context, irp->CallbackData, ioStatus,
		                                       length > 0 ? &fdi : NULL);
		/* Setup the IRP. */
		irp->CompletionId = context->priv->NextCompletionId++;
		irp->Callback = rdpdr_server_drive_query_directory_callback2;

		if (!rdpdr_server_enqueue_irp(context, irp))
		{
			WLog_Print(context->priv->log, WLOG_ERROR, "rdpdr_server_enqueue_irp failed!");
			rdpdr_server_irp_free(irp);
			return ERROR_INTERNAL_ERROR;
		}

		/* Send a request to query the directory. */
		return rdpdr_server_send_device_query_directory_request(context, irp->DeviceId, irp->FileId,
		                                                        irp->CompletionId, NULL);
	}
	else
	{
		/* Invoke the query directory completion routine. */
		context->OnDriveQueryDirectoryComplete(context, irp->CallbackData, ioStatus, NULL);
		/* Destroy the IRP. */
		rdpdr_server_irp_free(irp);
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_query_directory_callback1(RdpdrServerContext* context, wStream* s,
                                                         RDPDR_IRP* irp, UINT32 deviceId,
                                                         UINT32 completionId, UINT32 ioStatus)
{
	UINT32 fileId;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);
	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "RdpdrServerDriveQueryDirectoryCallback1: deviceId=%" PRIu32
	           ", completionId=%" PRIu32 ", ioStatus=0x%" PRIx32 "",
	           deviceId, completionId, ioStatus);

	if (ioStatus != STATUS_SUCCESS)
	{
		/* Invoke the query directory completion routine. */
		context->OnDriveQueryDirectoryComplete(context, irp->CallbackData, ioStatus, NULL);
		/* Destroy the IRP. */
		rdpdr_server_irp_free(irp);
		return CHANNEL_RC_OK;
	}

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, fileId);
	/* Setup the IRP. */
	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_query_directory_callback2;
	irp->DeviceId = deviceId;
	irp->FileId = fileId;
	winpr_str_append("\\*.*", irp->PathName, ARRAYSIZE(irp->PathName), NULL);

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to query the directory. */
	return rdpdr_server_send_device_query_directory_request(context, deviceId, fileId,
	                                                        irp->CompletionId, irp->PathName);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_query_directory(RdpdrServerContext* context, void* callbackData,
                                               UINT32 deviceId, const char* path)
{
	RDPDR_IRP* irp = rdpdr_server_irp_new();
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);

	if (!irp)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpdr_server_irp_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_query_directory_callback1;
	irp->CallbackData = callbackData;
	irp->DeviceId = deviceId;
	strncpy(irp->PathName, path, sizeof(irp->PathName) - 1);
	rdpdr_server_convert_slashes(irp->PathName, sizeof(irp->PathName));

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to open the directory. */
	return rdpdr_server_send_device_create_request(
	    context, deviceId, irp->CompletionId, irp->PathName, FILE_READ_DATA | SYNCHRONIZE,
	    FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT, FILE_OPEN);
}

/*************************************************
 * Drive Open File
 ************************************************/

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_open_file_callback(RdpdrServerContext* context, wStream* s,
                                                  RDPDR_IRP* irp, UINT32 deviceId,
                                                  UINT32 completionId, UINT32 ioStatus)
{
	UINT32 fileId;
	UINT8 information;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);
	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "RdpdrServerDriveOpenFileCallback: deviceId=%" PRIu32 ", completionId=%" PRIu32
	           ", ioStatus=0x%" PRIx32 "",
	           deviceId, completionId, ioStatus);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 5))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, fileId);     /* FileId (4 bytes) */
	Stream_Read_UINT8(s, information); /* Information (1 byte) */
	/* Invoke the open file completion routine. */
	context->OnDriveOpenFileComplete(context, irp->CallbackData, ioStatus, deviceId, fileId);
	/* Destroy the IRP. */
	rdpdr_server_irp_free(irp);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_open_file(RdpdrServerContext* context, void* callbackData,
                                         UINT32 deviceId, const char* path, UINT32 desiredAccess,
                                         UINT32 createDisposition)
{
	RDPDR_IRP* irp = rdpdr_server_irp_new();
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);

	if (!irp)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpdr_server_irp_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_open_file_callback;
	irp->CallbackData = callbackData;
	irp->DeviceId = deviceId;
	strncpy(irp->PathName, path, sizeof(irp->PathName) - 1);
	rdpdr_server_convert_slashes(irp->PathName, sizeof(irp->PathName));

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to open the file. */
	return rdpdr_server_send_device_create_request(context, deviceId, irp->CompletionId,
	                                               irp->PathName, desiredAccess | SYNCHRONIZE,
	                                               FILE_SYNCHRONOUS_IO_NONALERT, createDisposition);
}

/*************************************************
 * Drive Read File
 ************************************************/

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_read_file_callback(RdpdrServerContext* context, wStream* s,
                                                  RDPDR_IRP* irp, UINT32 deviceId,
                                                  UINT32 completionId, UINT32 ioStatus)
{
	UINT32 length;
	char* buffer = NULL;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);
	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "RdpdrServerDriveReadFileCallback: deviceId=%" PRIu32 ", completionId=%" PRIu32
	           ", ioStatus=0x%" PRIx32 "",
	           deviceId, completionId, ioStatus);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, length); /* Length (4 bytes) */

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, length))
		return ERROR_INVALID_DATA;

	if (length > 0)
	{
		buffer = (char*)Stream_Pointer(s);
		Stream_Seek(s, length);
	}

	/* Invoke the read file completion routine. */
	context->OnDriveReadFileComplete(context, irp->CallbackData, ioStatus, buffer, length);
	/* Destroy the IRP. */
	rdpdr_server_irp_free(irp);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_read_file(RdpdrServerContext* context, void* callbackData,
                                         UINT32 deviceId, UINT32 fileId, UINT32 length,
                                         UINT32 offset)
{
	RDPDR_IRP* irp = rdpdr_server_irp_new();
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);

	if (!irp)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpdr_server_irp_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_read_file_callback;
	irp->CallbackData = callbackData;
	irp->DeviceId = deviceId;
	irp->FileId = fileId;

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to open the directory. */
	return rdpdr_server_send_device_read_request(context, deviceId, fileId, irp->CompletionId,
	                                             length, offset);
}

/*************************************************
 * Drive Write File
 ************************************************/

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_write_file_callback(RdpdrServerContext* context, wStream* s,
                                                   RDPDR_IRP* irp, UINT32 deviceId,
                                                   UINT32 completionId, UINT32 ioStatus)
{
	UINT32 length;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);
	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "RdpdrServerDriveWriteFileCallback: deviceId=%" PRIu32 ", completionId=%" PRIu32
	           ", ioStatus=0x%" PRIx32 "",
	           deviceId, completionId, ioStatus);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 5))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, length); /* Length (4 bytes) */
	Stream_Seek(s, 1);             /* Padding (1 byte) */

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, length))
		return ERROR_INVALID_DATA;

	/* Invoke the write file completion routine. */
	context->OnDriveWriteFileComplete(context, irp->CallbackData, ioStatus, length);
	/* Destroy the IRP. */
	rdpdr_server_irp_free(irp);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_write_file(RdpdrServerContext* context, void* callbackData,
                                          UINT32 deviceId, UINT32 fileId, const char* buffer,
                                          UINT32 length, UINT32 offset)
{
	RDPDR_IRP* irp = rdpdr_server_irp_new();
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);

	if (!irp)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpdr_server_irp_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_write_file_callback;
	irp->CallbackData = callbackData;
	irp->DeviceId = deviceId;
	irp->FileId = fileId;

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to open the directory. */
	return rdpdr_server_send_device_write_request(context, deviceId, fileId, irp->CompletionId,
	                                              buffer, length, offset);
}

/*************************************************
 * Drive Close File
 ************************************************/

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_close_file_callback(RdpdrServerContext* context, wStream* s,
                                                   RDPDR_IRP* irp, UINT32 deviceId,
                                                   UINT32 completionId, UINT32 ioStatus)
{
	WINPR_UNUSED(s);
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);

	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "RdpdrServerDriveCloseFileCallback: deviceId=%" PRIu32 ", completionId=%" PRIu32
	           ", ioStatus=0x%" PRIx32 "",
	           deviceId, completionId, ioStatus);
	/* Invoke the close file completion routine. */
	context->OnDriveCloseFileComplete(context, irp->CallbackData, ioStatus);
	/* Destroy the IRP. */
	rdpdr_server_irp_free(irp);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_close_file(RdpdrServerContext* context, void* callbackData,
                                          UINT32 deviceId, UINT32 fileId)
{
	RDPDR_IRP* irp = rdpdr_server_irp_new();
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);

	if (!irp)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpdr_server_irp_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_close_file_callback;
	irp->CallbackData = callbackData;
	irp->DeviceId = deviceId;
	irp->FileId = fileId;

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to open the directory. */
	return rdpdr_server_send_device_close_request(context, deviceId, fileId, irp->CompletionId);
}

/*************************************************
 * Drive Delete File
 ************************************************/

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_delete_file_callback2(RdpdrServerContext* context, wStream* s,
                                                     RDPDR_IRP* irp, UINT32 deviceId,
                                                     UINT32 completionId, UINT32 ioStatus)
{
	WINPR_UNUSED(s);
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);

	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "RdpdrServerDriveDeleteFileCallback2: deviceId=%" PRIu32 ", completionId=%" PRIu32
	           ", ioStatus=0x%" PRIx32 "",
	           deviceId, completionId, ioStatus);
	/* Invoke the delete file completion routine. */
	context->OnDriveDeleteFileComplete(context, irp->CallbackData, ioStatus);
	/* Destroy the IRP. */
	rdpdr_server_irp_free(irp);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_delete_file_callback1(RdpdrServerContext* context, wStream* s,
                                                     RDPDR_IRP* irp, UINT32 deviceId,
                                                     UINT32 completionId, UINT32 ioStatus)
{
	UINT32 fileId;
	UINT8 information;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);
	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "RdpdrServerDriveDeleteFileCallback1: deviceId=%" PRIu32 ", completionId=%" PRIu32
	           ", ioStatus=0x%" PRIx32 "",
	           deviceId, completionId, ioStatus);

	if (ioStatus != STATUS_SUCCESS)
	{
		/* Invoke the close file completion routine. */
		context->OnDriveDeleteFileComplete(context, irp->CallbackData, ioStatus);
		/* Destroy the IRP. */
		rdpdr_server_irp_free(irp);
		return CHANNEL_RC_OK;
	}

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 5))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, fileId);     /* FileId (4 bytes) */
	Stream_Read_UINT8(s, information); /* Information (1 byte) */
	/* Setup the IRP. */
	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_delete_file_callback2;
	irp->DeviceId = deviceId;
	irp->FileId = fileId;

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to close the file */
	return rdpdr_server_send_device_close_request(context, deviceId, fileId, irp->CompletionId);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_delete_file(RdpdrServerContext* context, void* callbackData,
                                           UINT32 deviceId, const char* path)
{
	RDPDR_IRP* irp = rdpdr_server_irp_new();
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);

	if (!irp)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpdr_server_irp_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_delete_file_callback1;
	irp->CallbackData = callbackData;
	irp->DeviceId = deviceId;
	strncpy(irp->PathName, path, sizeof(irp->PathName) - 1);
	rdpdr_server_convert_slashes(irp->PathName, sizeof(irp->PathName));

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to open the file. */
	return rdpdr_server_send_device_create_request(
	    context, deviceId, irp->CompletionId, irp->PathName, FILE_READ_DATA | SYNCHRONIZE,
	    FILE_DELETE_ON_CLOSE | FILE_SYNCHRONOUS_IO_NONALERT, FILE_OPEN);
}

/*************************************************
 * Drive Rename File
 ************************************************/

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_rename_file_callback3(RdpdrServerContext* context, wStream* s,
                                                     RDPDR_IRP* irp, UINT32 deviceId,
                                                     UINT32 completionId, UINT32 ioStatus)
{
	WINPR_UNUSED(context);
	WINPR_UNUSED(s);
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);

	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "RdpdrServerDriveRenameFileCallback3: deviceId=%" PRIu32 ", completionId=%" PRIu32
	           ", ioStatus=0x%" PRIx32 "",
	           deviceId, completionId, ioStatus);
	/* Destroy the IRP. */
	rdpdr_server_irp_free(irp);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_rename_file_callback2(RdpdrServerContext* context, wStream* s,
                                                     RDPDR_IRP* irp, UINT32 deviceId,
                                                     UINT32 completionId, UINT32 ioStatus)
{
	UINT32 length;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);
	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "RdpdrServerDriveRenameFileCallback2: deviceId=%" PRIu32 ", completionId=%" PRIu32
	           ", ioStatus=0x%" PRIx32 "",
	           deviceId, completionId, ioStatus);

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 5))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, length); /* Length (4 bytes) */
	Stream_Seek(s, 1);             /* Padding (1 byte) */
	/* Invoke the rename file completion routine. */
	context->OnDriveRenameFileComplete(context, irp->CallbackData, ioStatus);
	/* Setup the IRP. */
	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_rename_file_callback3;
	irp->DeviceId = deviceId;

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to close the file */
	return rdpdr_server_send_device_close_request(context, deviceId, irp->FileId,
	                                              irp->CompletionId);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_rename_file_callback1(RdpdrServerContext* context, wStream* s,
                                                     RDPDR_IRP* irp, UINT32 deviceId,
                                                     UINT32 completionId, UINT32 ioStatus)
{
	UINT32 fileId;
	UINT8 information;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);
	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "RdpdrServerDriveRenameFileCallback1: deviceId=%" PRIu32 ", completionId=%" PRIu32
	           ", ioStatus=0x%" PRIx32 "",
	           deviceId, completionId, ioStatus);

	if (ioStatus != STATUS_SUCCESS)
	{
		/* Invoke the rename file completion routine. */
		context->OnDriveRenameFileComplete(context, irp->CallbackData, ioStatus);
		/* Destroy the IRP. */
		rdpdr_server_irp_free(irp);
		return CHANNEL_RC_OK;
	}

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 5))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, fileId);     /* FileId (4 bytes) */
	Stream_Read_UINT8(s, information); /* Information (1 byte) */
	/* Setup the IRP. */
	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_rename_file_callback2;
	irp->DeviceId = deviceId;
	irp->FileId = fileId;

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to rename the file */
	return rdpdr_server_send_device_file_rename_request(context, deviceId, fileId,
	                                                    irp->CompletionId, irp->ExtraBuffer);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_rename_file(RdpdrServerContext* context, void* callbackData,
                                           UINT32 deviceId, const char* oldPath,
                                           const char* newPath)
{
	RDPDR_IRP* irp;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	irp = rdpdr_server_irp_new();

	if (!irp)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpdr_server_irp_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_rename_file_callback1;
	irp->CallbackData = callbackData;
	irp->DeviceId = deviceId;
	strncpy(irp->PathName, oldPath, sizeof(irp->PathName) - 1);
	strncpy(irp->ExtraBuffer, newPath, sizeof(irp->ExtraBuffer) - 1);
	rdpdr_server_convert_slashes(irp->PathName, sizeof(irp->PathName));
	rdpdr_server_convert_slashes(irp->ExtraBuffer, sizeof(irp->ExtraBuffer));

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to open the file. */
	return rdpdr_server_send_device_create_request(context, deviceId, irp->CompletionId,
	                                               irp->PathName, FILE_READ_DATA | SYNCHRONIZE,
	                                               FILE_SYNCHRONOUS_IO_NONALERT, FILE_OPEN);
}

static void rdpdr_server_private_free(RdpdrServerPrivate* ctx)
{
	if (!ctx)
		return;
	ListDictionary_Free(ctx->IrpList);
	HashTable_Free(ctx->devicelist);
	free(ctx->ClientComputerName);
	free(ctx);
}

#define TAG CHANNELS_TAG("rdpdr.server")
static RdpdrServerPrivate* rdpdr_server_private_new(void)
{
	RdpdrServerPrivate* priv = (RdpdrServerPrivate*)calloc(1, sizeof(RdpdrServerPrivate));

	if (!priv)
		goto fail;

	priv->log = WLog_Get(TAG);
	priv->VersionMajor = RDPDR_VERSION_MAJOR;
	priv->VersionMinor = RDPDR_VERSION_MINOR_RDP6X;
	priv->ClientId = g_ClientId++;
	priv->UserLoggedOnPdu = TRUE;
	priv->NextCompletionId = 1;
	priv->IrpList = ListDictionary_New(TRUE);

	if (!priv->IrpList)
		goto fail;

	priv->devicelist = HashTable_New(FALSE);
	if (!priv->devicelist)
		goto fail;

	HashTable_SetHashFunction(priv->devicelist, rdpdr_deviceid_hash);
	wObject* obj = HashTable_ValueObject(priv->devicelist);
	WINPR_ASSERT(obj);
	obj->fnObjectFree = rdpdr_device_free_h;
	obj->fnObjectNew = rdpdr_device_clone;

	return priv;
fail:
	rdpdr_server_private_free(priv);
	return NULL;
}

RdpdrServerContext* rdpdr_server_context_new(HANDLE vcm)
{
	RdpdrServerContext* context = (RdpdrServerContext*)calloc(1, sizeof(RdpdrServerContext));

	if (!context)
		goto fail;

	context->vcm = vcm;
	context->Start = rdpdr_server_start;
	context->Stop = rdpdr_server_stop;
	context->DriveCreateDirectory = rdpdr_server_drive_create_directory;
	context->DriveDeleteDirectory = rdpdr_server_drive_delete_directory;
	context->DriveQueryDirectory = rdpdr_server_drive_query_directory;
	context->DriveOpenFile = rdpdr_server_drive_open_file;
	context->DriveReadFile = rdpdr_server_drive_read_file;
	context->DriveWriteFile = rdpdr_server_drive_write_file;
	context->DriveCloseFile = rdpdr_server_drive_close_file;
	context->DriveDeleteFile = rdpdr_server_drive_delete_file;
	context->DriveRenameFile = rdpdr_server_drive_rename_file;
	context->priv = rdpdr_server_private_new();
	if (!context->priv)
		goto fail;

	/* By default announce everything, the server application can deactivate that later on */
	context->supported = UINT16_MAX;

	return context;
fail:
	rdpdr_server_context_free(context);
	return NULL;
}

void rdpdr_server_context_free(RdpdrServerContext* context)
{
	if (!context)
		return;

	rdpdr_server_private_free(context->priv);
	free(context);
}
