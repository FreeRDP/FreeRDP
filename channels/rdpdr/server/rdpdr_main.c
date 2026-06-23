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
#include <freerdp/freerdp.h>
#include <freerdp/utils/rdpdr_utils.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/nt.h>
#include <winpr/print.h>
#include <winpr/stream.h>

#include <freerdp/channels/log.h>
#include "rdpdr_main.h"
#include <freerdp/utils/channel_pdu_tracker.h>

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
	UINT32 SpecialDeviceTypeCap;
	UINT32 IoCode1;
	UINT32 ExtendedPDU;
	BOOL haveSmartcardDevice;
	UINT32 smartcardDeviceId;
};

static const char* fileInformation2str(uint8_t val)
{
	switch (val)
	{
		case FILE_SUPERSEDED:
			return "FILE_DOES_NOT_EXIST";
		case FILE_OPENED:
			return "FILE_EXISTS";
		case FILE_CREATED:
			return "FILE_OVERWRITTEN";
		case FILE_OVERWRITTEN:
			return "FILE_CREATED";
		case FILE_EXISTS:
			return "FILE_OPENED";
		case FILE_DOES_NOT_EXIST:
			return "FILE_SUPERSEDED";
		default:
			return "FILE_UNKNOWN";
	}
}

static const char* DR_DRIVE_LOCK_REQ2str(uint32_t op)
{
	switch (op)
	{
		case RDP_LOWIO_OP_SHAREDLOCK:
			return "RDP_LOWIO_OP_UNLOCK_MULTIPLE";
		case RDP_LOWIO_OP_EXCLUSIVELOCK:
			return "RDP_LOWIO_OP_UNLOCK";
		case RDP_LOWIO_OP_UNLOCK:
			return "RDP_LOWIO_OP_EXCLUSIVELOCK";
		case RDP_LOWIO_OP_UNLOCK_MULTIPLE:
			return "RDP_LOWIO_OP_SHAREDLOCK";
		default:
			return "RDP_LOWIO_OP_UNKNOWN";
	}
}

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

static BOOL rdpdr_device_equal(const void* v1, const void* v2)
{
	const UINT32* p1 = (const UINT32*)v1;
	const UINT32* p2 = (const UINT32*)v2;
	if (!p1 && !p2)
		return TRUE;
	if (!p1 || !p2)
		return FALSE;
	return *p1 == *p2;
}

static RdpdrDevice* rdpdr_device_new(void)
{
	return calloc(1, sizeof(RdpdrDevice));
}

static void* rdpdr_device_clone(const void* val)
{
	const RdpdrDevice* other = val;

	if (!other)
		return nullptr;

	RdpdrDevice* tmp = rdpdr_device_new();
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
	return nullptr;
}

static void* rdpdr_device_key_clone(const void* pvval)
{
	const UINT32* val = pvval;
	if (!val)
		return nullptr;

	UINT32* ptr = calloc(1, sizeof(UINT32));
	if (!ptr)
		return nullptr;
	*ptr = *val;
	return ptr;
}

static void rdpdr_device_key_free(void* obj)
{
	free(obj);
}

static RdpdrDevice* rdpdr_get_device_by_id(RdpdrServerPrivate* priv, UINT32 DeviceId)
{
	WINPR_ASSERT(priv);
	return HashTable_GetItemValue(priv->devicelist, &DeviceId);
}

static BOOL rdpdr_remove_device_by_id(RdpdrServerPrivate* priv, UINT32 DeviceId)
{
	WINPR_ASSERT(priv);

	const RdpdrDevice* device = rdpdr_get_device_by_id(priv, DeviceId);
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
	const size_t charlen = bytelen / sizeof(WCHAR);
	const WCHAR* str = Stream_ConstPointer(s);
	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, bytelen))
		return nullptr;
	if (_wcsnlen(str, charlen) == charlen)
	{
		WLog_Print(log, WLOG_WARN, "[rdpdr] unicode string not '\\0' terminated");
		return nullptr;
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

	RdpdrServerPrivate* priv = context->priv;
	const uintptr_t key = irp->CompletionId + 1ull;
	return ListDictionary_Add(priv->IrpList, (void*)key, irp);
}

static RDPDR_IRP* rdpdr_server_dequeue_irp(RdpdrServerContext* context, UINT32 completionId)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;
	const uintptr_t key = completionId + 1ull;
	RDPDR_IRP* irp = (RDPDR_IRP*)ListDictionary_Take(priv->IrpList, (void*)key);
	return irp;
}

static UINT rdpdr_seal_send_free_request(RdpdrServerContext* context, wStream* s)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(s);

	RdpdrServerPrivate* priv = context->priv;
	Stream_SealLength(s);
	const size_t length = Stream_Length(s);
	WINPR_ASSERT(length <= UINT32_MAX);
	Stream_ResetPosition(s);

	if (length >= RDPDR_HEADER_LENGTH)
	{
		const RDPDR_HEADER header = {
			.Component = Stream_Get_UINT16(s),
			.PacketId = Stream_Get_UINT16(s),
		};

		WLog_Print(priv->log, WLOG_DEBUG,
		           "sending message {Component %s[%04" PRIx16 "], PacketId %s[%04" PRIx16 "]",
		           rdpdr_component_string(header.Component), header.Component,
		           rdpdr_packetid_string(header.PacketId), header.PacketId);
	}
	winpr_HexLogDump(priv->log, WLOG_DEBUG, Stream_Buffer(s), Stream_Length(s));
	ULONG written = 0;
	BOOL status = WTSVirtualChannelWrite(priv->ChannelHandle, Stream_BufferAs(s, char),
	                                     (ULONG)length, &written);
	Stream_Release(s);
	return status ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_send_announce_request(RdpdrServerContext* context)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	UINT error = IFCALLRESULT(CHANNEL_RC_OK, context->SendServerAnnounce, context);
	if (error != CHANNEL_RC_OK)
		return error;

	RdpdrServerPrivate* priv = context->priv;
	wStream* s = Stream_New(nullptr, RDPDR_HEADER_LENGTH + 8);
	if (!s)
	{
		WLog_Print(priv->log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	const RDPDR_HEADER header = {
		.Component = RDPDR_CTYP_CORE,
		.PacketId = PAKID_CORE_SERVER_ANNOUNCE,
	};
	Stream_Write_UINT16(s, header.Component);            /* Component (2 bytes) */
	Stream_Write_UINT16(s, header.PacketId);             /* PacketId (2 bytes) */
	Stream_Write_UINT16(s, priv->VersionMajor);          /* VersionMajor (2 bytes) */
	Stream_Write_UINT16(s, priv->VersionMinor);          /* VersionMinor (2 bytes) */
	Stream_Write_UINT32(s, priv->ClientId);              /* ClientId (4 bytes) */
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(header);

	RdpdrServerPrivate* priv = context->priv;

	WINPR_UNUSED(header);

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 8))
		return ERROR_INVALID_DATA;

	const UINT16 VersionMajor = Stream_Get_UINT16(s); /* VersionMajor (2 bytes) */
	const UINT16 VersionMinor = Stream_Get_UINT16(s); /* VersionMinor (2 bytes) */
	const UINT32 ClientId = Stream_Get_UINT32(s);     /* ClientId (4 bytes) */
	WLog_Print(priv->log, WLOG_DEBUG,
	           "Client Announce Response: VersionMajor: 0x%08" PRIX16 " VersionMinor: 0x%04" PRIX16
	           " ClientId: 0x%08" PRIX32 "",
	           VersionMajor, VersionMinor, ClientId);
	priv->ClientId = ClientId;

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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(s);
	WINPR_ASSERT(header);
	WINPR_UNUSED(header);

	RdpdrServerPrivate* priv = context->priv;

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 12))
		return ERROR_INVALID_DATA;

	UINT32 UnicodeFlag = Stream_Get_UINT32(s);           /* UnicodeFlag (4 bytes) */
	const UINT32 CodePage = Stream_Get_UINT32(s);        /* CodePage (4 bytes) */
	const UINT32 ComputerNameLen = Stream_Get_UINT32(s); /* ComputerNameLen (4 bytes) */
	/* UnicodeFlag is either 0 or 1, the other 31 bits must be ignored.
	 */
	UnicodeFlag = UnicodeFlag & 0x00000001;

	if (CodePage != 0)
		WLog_Print(priv->log, WLOG_WARN,
		           "[MS-RDPEFS] 2.2.2.4 Client Name Request (DR_CORE_CLIENT_NAME_REQ)::CodePage "
		           "must be 0, but is 0x%08" PRIx32,
		           CodePage);

	/**
	 * Caution: ComputerNameLen is given *bytes*,
	 * not in characters, including the nullptr terminator!
	 */

	if (UnicodeFlag)
	{
		if ((ComputerNameLen % 2) || ComputerNameLen > 512 || ComputerNameLen < 2)
		{
			WLog_Print(priv->log, WLOG_ERROR, "invalid unicode computer name length: %" PRIu32 "",
			           ComputerNameLen);
			return ERROR_INVALID_DATA;
		}
	}
	else
	{
		if (ComputerNameLen > 256 || ComputerNameLen < 1)
		{
			WLog_Print(priv->log, WLOG_ERROR, "invalid ascii computer name length: %" PRIu32 "",
			           ComputerNameLen);
			return ERROR_INVALID_DATA;
		}
	}

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, ComputerNameLen))
		return ERROR_INVALID_DATA;

	/* ComputerName must be null terminated, check if it really is */
	const char* computerName = Stream_ConstPointer(s);
	if (computerName[ComputerNameLen - 1] || (UnicodeFlag && computerName[ComputerNameLen - 2]))
	{
		WLog_Print(priv->log, WLOG_ERROR, "computer name must be null terminated");
		return ERROR_INVALID_DATA;
	}

	if (priv->ClientComputerName)
	{
		free(priv->ClientComputerName);
		priv->ClientComputerName = nullptr;
	}

	if (UnicodeFlag)
	{
		priv->ClientComputerName =
		    Stream_Read_UTF16_String_As_UTF8(s, ComputerNameLen / sizeof(WCHAR), nullptr);
		if (!priv->ClientComputerName)
		{
			WLog_Print(priv->log, WLOG_ERROR, "failed to convert client computer name");
			return ERROR_INVALID_DATA;
		}
	}
	else
	{
		const char* name = Stream_ConstPointer(s);
		priv->ClientComputerName = _strdup(name);
		Stream_Seek(s, ComputerNameLen);

		if (!priv->ClientComputerName)
		{
			WLog_Print(priv->log, WLOG_ERROR, "failed to duplicate client computer name");
			return CHANNEL_RC_NO_MEMORY;
		}
	}

	WLog_Print(priv->log, WLOG_DEBUG, "ClientComputerName: %s", priv->ClientComputerName);
	return IFCALLRESULT(CHANNEL_RC_OK, context->ReceiveClientNameRequest, context, ComputerNameLen,
	                    priv->ClientComputerName);
}

static UINT rdpdr_server_write_capability_set_header_cb(RdpdrServerContext* context, wStream* s,
                                                        const RDPDR_CAPABILITY_HEADER* header)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;
	UINT error = rdpdr_write_capset_header(priv->log, s, header);
	if (error != CHANNEL_RC_OK)
		return error;

	return IFCALLRESULT(CHANNEL_RC_OK, context->SendCaps, context, header, 0, nullptr);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_read_general_capability_set(RdpdrServerContext* context, wStream* s,
                                                     const RDPDR_CAPABILITY_HEADER* header)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(header);

	RdpdrServerPrivate* priv = context->priv;

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 32))
		return ERROR_INVALID_DATA;

	const UINT32 OsType = Stream_Get_UINT32(s);       /* osType (4 bytes),
	                             ignored on receipt */
	const UINT32 OsVersion = Stream_Get_UINT32(s);    /* osVersion (4 bytes),
	                             unused and must be set to zero */
	const UINT32 VersionMajor = Stream_Get_UINT16(s); /* protocolMajorVersion (2 bytes) */
	const UINT32 VersionMinor = Stream_Get_UINT16(s); /* protocolMinorVersion (2 bytes) */
	const UINT32 IoCode1 = Stream_Get_UINT32(s);      /* ioCode1 (4 bytes) */
	const UINT32 IoCode2 = Stream_Get_UINT32(s);      /* ioCode2 (4 bytes),
	                             must be set to zero, reserved for future use */
	const UINT32 ExtendedPdu = Stream_Get_UINT32(s); /* extendedPdu (4 bytes) */
	const UINT32 ExtraFlags1 = Stream_Get_UINT32(s); /* extraFlags1 (4 bytes) */
	const UINT32 ExtraFlags2 = Stream_Get_UINT32(s); /* extraFlags2 (4 bytes),
	                            must be set to zero, reserved for future use */

	{
		char buffer[1024] = WINPR_C_ARRAY_INIT;
		WLog_Print(priv->log, WLOG_TRACE,
		           "OsType=%" PRIu32 ", OsVersion=%" PRIu32 ", VersionMajor=%" PRIu32
		           ", VersionMinor=%" PRIu32 ", IoCode1=%s, IoCode2=%" PRIu32
		           ", ExtendedPdu=%" PRIu32 ", ExtraFlags1=%" PRIu32 ", ExtraFlags2=%" PRIu32,
		           OsType, OsVersion, VersionMajor, VersionMinor,
		           rdpdr_irp_mask2str(IoCode1, buffer, sizeof(buffer)), IoCode2, ExtendedPdu,
		           ExtraFlags1, ExtraFlags2);
	}

	if (VersionMajor != RDPDR_MAJOR_RDP_VERSION)
	{
		WLog_Print(priv->log, WLOG_ERROR, "unsupported RDPDR version %" PRIu16 ".%" PRIu16,
		           VersionMajor, VersionMinor);
		return ERROR_INVALID_DATA;
	}

	switch (VersionMinor)
	{
		case RDPDR_MINOR_RDP_VERSION_13:
		case RDPDR_MINOR_RDP_VERSION_6_X:
		case RDPDR_MINOR_RDP_VERSION_5_2:
		case RDPDR_MINOR_RDP_VERSION_5_1:
		case RDPDR_MINOR_RDP_VERSION_5_0:
			break;
		default:
			WLog_Print(priv->log, WLOG_WARN, "unsupported RDPDR minor version %" PRIu16 ".%" PRIu16,
			           VersionMajor, VersionMinor);
			break;
	}

	UINT32 SpecialTypeDeviceCap = 0;
	if (header->Version == GENERAL_CAPABILITY_VERSION_02)
	{
		if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 4))
			return ERROR_INVALID_DATA;

		SpecialTypeDeviceCap = Stream_Get_UINT32(s); /* SpecialTypeDeviceCap (4 bytes) */
	}
	priv->SpecialDeviceTypeCap = SpecialTypeDeviceCap;

	const UINT32 mask =
	    RDPDR_IRP_MJ_CREATE | RDPDR_IRP_MJ_CLEANUP | RDPDR_IRP_MJ_CLOSE | RDPDR_IRP_MJ_READ |
	    RDPDR_IRP_MJ_WRITE | RDPDR_IRP_MJ_FLUSH_BUFFERS | RDPDR_IRP_MJ_SHUTDOWN |
	    RDPDR_IRP_MJ_DEVICE_CONTROL | RDPDR_IRP_MJ_QUERY_VOLUME_INFORMATION |
	    RDPDR_IRP_MJ_SET_VOLUME_INFORMATION | RDPDR_IRP_MJ_QUERY_INFORMATION |
	    RDPDR_IRP_MJ_SET_INFORMATION | RDPDR_IRP_MJ_DIRECTORY_CONTROL | RDPDR_IRP_MJ_LOCK_CONTROL;

	if ((IoCode1 & mask) == 0)
	{
		char buffer[1024] = WINPR_C_ARRAY_INIT;
		WLog_Print(priv->log, WLOG_ERROR, "Missing IRP mask values %s",
		           rdpdr_irp_mask2str(IoCode1 & mask, buffer, sizeof(buffer)));
		return ERROR_INVALID_DATA;
	}
	priv->IoCode1 = IoCode1;

	if (IoCode2 != 0)
	{
		WLog_Print(priv->log, WLOG_WARN,
		           "[MS-RDPEFS] 2.2.2.7.1 General Capability Set (GENERAL_CAPS_SET) ioCode2 is "
		           "reserved for future use, expected value 0 got %" PRIu32,
		           IoCode2);
	}

	if ((ExtendedPdu & RDPDR_CLIENT_DISPLAY_NAME_PDU) == 0)
	{
		WLog_Print(priv->log, WLOG_WARN,
		           "[MS-RDPEFS] 2.2.2.7.1 General Capability Set (GENERAL_CAPS_SET) extendedPDU "
		           "should always set RDPDR_CLIENT_DISPLAY_NAME_PDU[0x00000002]");
	}

	priv->ExtendedPDU = ExtendedPdu;
	priv->UserLoggedOnPdu = (ExtendedPdu & RDPDR_USER_LOGGEDON_PDU) != 0;

	if (ExtraFlags2 != 0)
	{
		WLog_Print(priv->log, WLOG_WARN,
		           "[MS-RDPEFS] 2.2.2.7.1 General Capability Set (GENERAL_CAPS_SET) ExtraFlags2 is "
		           "reserved for future use, expected value 0 got %" PRIu32,
		           ExtraFlags2);
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_write_general_capability_set(RdpdrServerContext* context, wStream* s)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	const RDPDR_CAPABILITY_HEADER header = { CAP_GENERAL_TYPE, RDPDR_CAPABILITY_HEADER_LENGTH + 36,
		                                     GENERAL_CAPABILITY_VERSION_02 };

	UINT32 ioCode1 = 0;
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

	UINT32 extendedPdu = 0;
	extendedPdu |= RDPDR_CLIENT_DISPLAY_NAME_PDU; /* always set */
	extendedPdu |= RDPDR_DEVICE_REMOVE_PDUS;      /* optional */

	RdpdrServerPrivate* priv = context->priv;
	if (priv->UserLoggedOnPdu)
		extendedPdu |= RDPDR_USER_LOGGEDON_PDU; /* optional */

	UINT32 extraFlags1 = 0;
	extraFlags1 |= ENABLE_ASYNCIO; /* optional */
	UINT32 SpecialTypeDeviceCap = 0;

	UINT error = rdpdr_write_capset_header(priv->log, s, &header);
	if (error != CHANNEL_RC_OK)
		return error;

	const BYTE* data = Stream_ConstPointer(s);
	const size_t start = Stream_GetPosition(s);
	Stream_Write_UINT32(s, 0); /* osType (4 bytes), ignored on receipt */
	Stream_Write_UINT32(s, 0); /* osVersion (4 bytes), unused and must be set to zero */
	Stream_Write_UINT16(s, priv->VersionMajor);          /* protocolMajorVersion (2 bytes) */
	Stream_Write_UINT16(s, priv->VersionMinor);          /* protocolMinorVersion (2 bytes) */
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
	WINPR_UNUSED(context);
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	const RDPDR_CAPABILITY_HEADER header = { CAP_PRINTER_TYPE, RDPDR_CAPABILITY_HEADER_LENGTH,
		                                     PRINT_CAPABILITY_VERSION_01 };

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
	WINPR_UNUSED(context);
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	const RDPDR_CAPABILITY_HEADER header = { CAP_PORT_TYPE, RDPDR_CAPABILITY_HEADER_LENGTH,
		                                     PORT_CAPABILITY_VERSION_01 };

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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_UNUSED(context);

	const RDPDR_CAPABILITY_HEADER header = { CAP_DRIVE_TYPE, RDPDR_CAPABILITY_HEADER_LENGTH,
		                                     DRIVE_CAPABILITY_VERSION_02 };

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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_UNUSED(context);

	const RDPDR_CAPABILITY_HEADER header = { CAP_SMARTCARD_TYPE, RDPDR_CAPABILITY_HEADER_LENGTH,
		                                     SMARTCARD_CAPABILITY_VERSION_01 };

	return rdpdr_server_write_capability_set_header_cb(context, s, &header);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_send_core_capability_request(RdpdrServerContext* context)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	UINT16 numCapabilities = 1;
	if ((context->supported & RDPDR_DTYP_FILESYSTEM) != 0)
		numCapabilities++;

	if (((context->supported & RDPDR_DTYP_PARALLEL) != 0) ||
	    ((context->supported & RDPDR_DTYP_SERIAL) != 0))
		numCapabilities++;

	if ((context->supported & RDPDR_DTYP_PRINT) != 0)
		numCapabilities++;

	if ((context->supported & RDPDR_DTYP_SMARTCARD) != 0)
		numCapabilities++;

	RdpdrServerPrivate* priv = context->priv;
	wStream* s = Stream_New(nullptr, RDPDR_HEADER_LENGTH + 512);
	if (!s)
	{
		WLog_Print(priv->log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	const RDPDR_HEADER header = {
		.Component = RDPDR_CTYP_CORE,
		.PacketId = PAKID_CORE_SERVER_CAPABILITY,
	};
	Stream_Write_UINT16(s, header.Component); /* Component (2 bytes) */
	Stream_Write_UINT16(s, header.PacketId);  /* PacketId (2 bytes) */
	Stream_Write_UINT16(s, numCapabilities);  /* numCapabilities (2 bytes) */
	Stream_Write_UINT16(s, 0);                /* Padding (2 bytes) */

	UINT error = 0;
	if ((error = rdpdr_server_write_general_capability_set(context, s)))
	{
		WLog_Print(priv->log, WLOG_ERROR,
		           "rdpdr_server_write_general_capability_set failed with error %" PRIu32 "!",
		           error);
		goto out;
	}

	if ((context->supported & RDPDR_DTYP_FILESYSTEM) != 0)
	{
		if ((error = rdpdr_server_write_drive_capability_set(context, s)))
		{
			WLog_Print(priv->log, WLOG_ERROR,
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
			WLog_Print(priv->log, WLOG_ERROR,
			           "rdpdr_server_write_port_capability_set failed with error %" PRIu32 "!",
			           error);
			goto out;
		}
	}

	if ((context->supported & RDPDR_DTYP_PRINT) != 0)
	{
		if ((error = rdpdr_server_write_printer_capability_set(context, s)))
		{
			WLog_Print(priv->log, WLOG_ERROR,
			           "rdpdr_server_write_printer_capability_set failed with error %" PRIu32 "!",
			           error);
			goto out;
		}
	}

	if ((context->supported & RDPDR_DTYP_SMARTCARD) != 0)
	{
		if ((error = rdpdr_server_write_smartcard_capability_set(context, s)))
		{
			WLog_Print(priv->log, WLOG_ERROR,
			           "rdpdr_server_write_printer_capability_set failed with error %" PRIu32 "!",
			           error);
			goto out;
		}
	}

	return rdpdr_seal_send_free_request(context, s);
out:
	Stream_Release(s);
	return error;
}

static UINT16 rdpdr_cap_type_to_dtyp(UINT16 capabilityType)
{
	switch (capabilityType)
	{
		case CAP_PRINTER_TYPE:
			return RDPDR_DTYP_PRINT;
		case CAP_PORT_TYPE:
			return RDPDR_DTYP_SERIAL | RDPDR_DTYP_PARALLEL;
		case CAP_DRIVE_TYPE:
			return RDPDR_DTYP_FILESYSTEM;
		case CAP_SMARTCARD_TYPE:
			return RDPDR_DTYP_SMARTCARD;
		case CAP_GENERAL_TYPE:
		default:
			return 0;
	}
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_receive_core_capability_response(RdpdrServerContext* context, wStream* s,
                                                          const RDPDR_HEADER* header)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;

	WINPR_UNUSED(header);

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 4))
		return ERROR_INVALID_DATA;

	const UINT16 numCapabilities = Stream_Get_UINT16(s); /* numCapabilities (2 bytes) */
	Stream_Seek_UINT16(s);                               /* Padding (2 bytes) */

	UINT16 caps = 0;
	for (UINT16 i = 0; i < numCapabilities; i++)
	{
		RDPDR_CAPABILITY_HEADER capabilityHeader = WINPR_C_ARRAY_INIT;
		const size_t start = Stream_GetPosition(s);

		UINT status = 0;
		if ((status = rdpdr_read_capset_header(priv->log, s, &capabilityHeader)))
		{
			WLog_Print(priv->log, WLOG_ERROR, "failed with error %" PRIu32 "!", status);
			return status;
		}

		status = IFCALLRESULT(CHANNEL_RC_OK, context->ReceiveCaps, context, &capabilityHeader,
		                      Stream_GetRemainingLength(s), Stream_ConstPointer(s));
		if (status != CHANNEL_RC_OK)
			return status;

		switch (capabilityHeader.CapabilityType)
		{
			case CAP_GENERAL_TYPE:
				if ((status =
				         rdpdr_server_read_general_capability_set(context, s, &capabilityHeader)))
				{
					WLog_Print(priv->log, WLOG_ERROR, "failed with error %" PRIu32 "!", status);
					return status;
				}

				break;

			case CAP_PRINTER_TYPE:
				if ((status =
				         rdpdr_server_read_printer_capability_set(context, s, &capabilityHeader)))
				{
					WLog_Print(priv->log, WLOG_ERROR, "failed with error %" PRIu32 "!", status);
					return status;
				}

				break;

			case CAP_PORT_TYPE:
				if ((status = rdpdr_server_read_port_capability_set(context, s, &capabilityHeader)))
				{
					WLog_Print(priv->log, WLOG_ERROR, "failed with error %" PRIu32 "!", status);
					return status;
				}

				break;

			case CAP_DRIVE_TYPE:
				if ((status =
				         rdpdr_server_read_drive_capability_set(context, s, &capabilityHeader)))
				{
					WLog_Print(priv->log, WLOG_ERROR, "failed with error %" PRIu32 "!", status);
					return status;
				}

				break;

			case CAP_SMARTCARD_TYPE:
				if ((status =
				         rdpdr_server_read_smartcard_capability_set(context, s, &capabilityHeader)))
				{
					WLog_Print(priv->log, WLOG_ERROR, "failed with error %" PRIu32 "!", status);
					return status;
				}

				break;

			default:
				WLog_Print(priv->log, WLOG_WARN, "Unknown capabilityType %" PRIu16 "",
				           capabilityHeader.CapabilityType);
				Stream_Seek(s, capabilityHeader.CapabilityLength);
				return ERROR_INVALID_DATA;
		}

		const UINT16 client_dtyp = rdpdr_cap_type_to_dtyp(capabilityHeader.CapabilityType);
		if ((client_dtyp != 0) && ((client_dtyp & context->supported) == 0))
		{
			WLog_Print(priv->log, WLOG_WARN, "client sent capability %s we did not announce!",
			           rdpdr_cap_type_string(capabilityHeader.CapabilityType));
		}
		caps |= client_dtyp;

		const size_t end = Stream_GetPosition(s);
		const size_t diff = end - start;
		if (diff != capabilityHeader.CapabilityLength + RDPDR_CAPABILITY_HEADER_LENGTH)
		{
			WLog_Print(priv->log, WLOG_WARN,
			           "{capability %s[0x%04" PRIx16 "]} processed %" PRIuz
			           " bytes, but expected to be %" PRIu16,
			           rdpdr_cap_type_string(capabilityHeader.CapabilityType),
			           capabilityHeader.CapabilityType, diff, capabilityHeader.CapabilityLength);
		}
	}

	/* Only deactivate what the client did not respond with */
	context->supported &= caps;

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_send_client_id_confirm(RdpdrServerContext* context)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;
	wStream* s = Stream_New(nullptr, RDPDR_HEADER_LENGTH + 8);
	if (!s)
	{
		WLog_Print(priv->log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	const RDPDR_HEADER header = {
		.Component = RDPDR_CTYP_CORE,
		.PacketId = PAKID_CORE_CLIENTID_CONFIRM,
	};
	Stream_Write_UINT16(s, header.Component);            /* Component (2 bytes) */
	Stream_Write_UINT16(s, header.PacketId);             /* PacketId (2 bytes) */
	Stream_Write_UINT16(s, priv->VersionMajor);          /* VersionMajor (2 bytes) */
	Stream_Write_UINT16(s, priv->VersionMinor);          /* VersionMinor (2 bytes) */
	Stream_Write_UINT32(s, priv->ClientId);              /* ClientId (4 bytes) */
	return rdpdr_seal_send_free_request(context, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_send_device_reply(RdpdrServerContext* context, UINT32 deviceId,
                                           UINT32 resultCode)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;
	wStream* s = Stream_New(nullptr, RDPDR_HEADER_LENGTH + 8);
	if (!s)
	{
		WLog_Print(priv->log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	const RDPDR_HEADER header = {
		.Component = RDPDR_CTYP_CORE,
		.PacketId = PAKID_CORE_DEVICE_REPLY,
	};
	Stream_Write_UINT16(s, header.Component); /* Component (2 bytes) */
	Stream_Write_UINT16(s, header.PacketId);  /* PacketId (2 bytes) */
	Stream_Write_UINT32(s, deviceId);         /* DeviceId (4 bytes) */
	Stream_Write_UINT32(s, resultCode);       /* ResultCode (4 bytes) */
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;

	WINPR_UNUSED(header);

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 4))
		return ERROR_INVALID_DATA;

	const UINT32 DeviceCount = Stream_Get_UINT32(s); /* DeviceCount (4 bytes) */
	WLog_Print(priv->log, WLOG_DEBUG, "DeviceCount: %" PRIu32 "", DeviceCount);

	for (UINT32 i = 0; i < DeviceCount; i++)
	{
		if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 20))
			return ERROR_INVALID_DATA;

		RdpdrDevice device = WINPR_C_ARRAY_INIT;
		device.DeviceType = Stream_Get_UINT32(s);       /* DeviceType (4 bytes) */
		device.DeviceId = Stream_Get_UINT32(s);         /* DeviceId (4 bytes) */
		Stream_Read(s, device.PreferredDosName, 8);     /* PreferredDosName (8 bytes) */
		device.DeviceDataLength = Stream_Get_UINT32(s); /* DeviceDataLength (4 bytes) */
		device.DeviceData = Stream_PointerAs(s, BYTE);

		if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, device.DeviceDataLength))
			return ERROR_INVALID_DATA;

		if (!rdpdr_add_device(context->priv, &device))
			return ERROR_INTERNAL_ERROR;

		UINT error = IFCALLRESULT(CHANNEL_RC_OK, context->ReceiveDeviceAnnounce, context, &device);
		if (error != CHANNEL_RC_OK)
			return error;

		error = STATUS_NOT_SUPPORTED;
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
					WLog_Print(priv->log, WLOG_WARN,
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
					WLog_Print(priv->log, WLOG_WARN,
					           "[rdpdr] RDPDR_DTYP_PARALLEL::DeviceDataLength != 0 [%" PRIu32 "]",
					           device.DeviceDataLength);
					error = ERROR_INVALID_DATA;
				}
				else if ((context->supported & RDPDR_DTYP_PARALLEL) != 0)
					error = IFCALLRESULT(CHANNEL_RC_OK, context->OnParallelPortCreate, context,
					                     &device);
				break;

			case RDPDR_DTYP_SMARTCARD:
				if ((context->supported & RDPDR_DTYP_SMARTCARD) != 0)
				{
					priv->smartcardDeviceId = device.DeviceId;
					priv->haveSmartcardDevice = TRUE;
					error =
					    IFCALLRESULT(STATUS_SUCCESS, context->OnSmartcardCreate, context, &device);
				}
				break;

			default:
				WLog_Print(priv->log, WLOG_WARN,
				           "[MS-RDPEFS] 2.2.2.9 Client Device List Announce Request "
				           "(DR_CORE_DEVICELIST_ANNOUNCE_REQ) unknown device type %04" PRIx16
				           " at position %" PRIu32,
				           device.DeviceType, i);
				error = ERROR_INVALID_DATA;
				break;
		}

		Stream_Seek(s, device.DeviceDataLength);

		error = rdpdr_server_send_device_reply(context, device.DeviceId, error);
		if (error != CHANNEL_RC_OK)
			return error;
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;

	WINPR_UNUSED(header);

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 4))
		return ERROR_INVALID_DATA;

	const UINT32 DeviceCount = Stream_Get_UINT32(s); /* DeviceCount (4 bytes) */
	WLog_Print(priv->log, WLOG_DEBUG, "DeviceCount: %" PRIu32 "", DeviceCount);

	for (UINT32 i = 0; i < DeviceCount; i++)
	{
		UINT error = 0;
		const RdpdrDevice* device = nullptr;

		if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 4))
			return ERROR_INVALID_DATA;

		const UINT32 DeviceId = Stream_Get_UINT32(s); /* DeviceId (4 bytes) */
		device = rdpdr_get_device_by_id(context->priv, DeviceId);
		WLog_Print(priv->log, WLOG_DEBUG, "Device %" PRIu32 " Id: 0x%08" PRIX32 "", i, DeviceId);
		UINT32 DeviceType = 0;
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
				if (priv->haveSmartcardDevice)
				{
					priv->haveSmartcardDevice = FALSE;
					priv->smartcardDeviceId = 0;
				}
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
                                                   WINPR_ATTR_UNUSED UINT32 DeviceId,
                                                   WINPR_ATTR_UNUSED UINT32 FileId,
                                                   WINPR_ATTR_UNUSED UINT32 CompletionId)
{
	WINPR_ASSERT(context);

	RdpdrServerPrivate* priv = context->priv;
	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 32))
		return ERROR_INVALID_DATA;

	WINPR_ATTR_UNUSED const UINT32 DesiredAccess = Stream_Get_UINT32(s);
	WINPR_ATTR_UNUSED const UINT32 AllocationSize = Stream_Get_UINT32(s);
	WINPR_ATTR_UNUSED const UINT32 FileAttributes = Stream_Get_UINT32(s);
	WINPR_ATTR_UNUSED const UINT32 SharedAccess = Stream_Get_UINT32(s);
	WINPR_ATTR_UNUSED const UINT32 CreateDisposition = Stream_Get_UINT32(s);
	WINPR_ATTR_UNUSED const UINT32 CreateOptions = Stream_Get_UINT32(s);
	const UINT32 PathLength = Stream_Get_UINT32(s);

	const WCHAR* path = rdpdr_read_ustring(priv->log, s, PathLength);
	if (!path && (PathLength > 0))
		return ERROR_INVALID_DATA;

	WLog_Print(priv->log, WLOG_WARN,
	           "[MS-RDPEFS] 2.2.1.4.1 Device Create Request (DR_CREATE_REQ) not implemented");
	WLog_Print(priv->log, WLOG_WARN, "TODO");

	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_io_close_request(RdpdrServerContext* context, wStream* s,
                                                  WINPR_ATTR_UNUSED UINT32 DeviceId,
                                                  WINPR_ATTR_UNUSED UINT32 FileId,
                                                  WINPR_ATTR_UNUSED UINT32 CompletionId)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 32))
		return ERROR_INVALID_DATA;

	Stream_Seek(s, 32); /* Padding */

	WLog_Print(priv->log, WLOG_WARN,
	           "[MS-RDPEFS] 2.2.1.4.2 Device Close Request (DR_CLOSE_REQ) not implemented");
	WLog_Print(priv->log, WLOG_WARN, "TODO");

	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_io_read_request(RdpdrServerContext* context, wStream* s,
                                                 WINPR_ATTR_UNUSED UINT32 DeviceId,
                                                 WINPR_ATTR_UNUSED UINT32 FileId,
                                                 WINPR_ATTR_UNUSED UINT32 CompletionId)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;
	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 32))
		return ERROR_INVALID_DATA;

	const UINT32 Length = Stream_Get_UINT32(s);
	const UINT64 Offset = Stream_Get_UINT64(s);
	Stream_Seek(s, 20); /* Padding */

	WLog_Print(priv->log, WLOG_DEBUG, "Got Offset [0x%016" PRIx64 "], Length %" PRIu32, Offset,
	           Length);

	WLog_Print(priv->log, WLOG_WARN,
	           "[MS-RDPEFS] 2.2.1.4.3 Device Read Request (DR_READ_REQ) not implemented");
	WLog_Print(priv->log, WLOG_WARN, "TODO");

	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_io_write_request(RdpdrServerContext* context, wStream* s,
                                                  WINPR_ATTR_UNUSED UINT32 DeviceId,
                                                  WINPR_ATTR_UNUSED UINT32 FileId,
                                                  WINPR_ATTR_UNUSED UINT32 CompletionId)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 32))
		return ERROR_INVALID_DATA;

	const UINT32 Length = Stream_Get_UINT32(s);
	const UINT64 Offset = Stream_Get_UINT64(s);
	Stream_Seek(s, 20); /* Padding */

	WLog_Print(priv->log, WLOG_DEBUG, "Got Offset [0x%016" PRIx64 "], Length %" PRIu32, Offset,
	           Length);

	const BYTE* data = Stream_ConstPointer(s);
	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, Length))
		return ERROR_INVALID_DATA;
	Stream_Seek(s, Length);

	WLog_Print(priv->log, WLOG_WARN,
	           "[MS-RDPEFS] 2.2.1.4.4 Device Write Request (DR_WRITE_REQ) not implemented");
	WLog_Print(priv->log, WLOG_WARN, "TODO: parse %p", (const void*)data);

	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_io_device_control_request(RdpdrServerContext* context, wStream* s,
                                                           WINPR_ATTR_UNUSED UINT32 DeviceId,
                                                           WINPR_ATTR_UNUSED UINT32 FileId,
                                                           WINPR_ATTR_UNUSED UINT32 CompletionId)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 32))
		return ERROR_INVALID_DATA;

	WINPR_ATTR_UNUSED const UINT32 OutputBufferLength = Stream_Get_UINT32(s);
	WINPR_ATTR_UNUSED const UINT32 InputBufferLength = Stream_Get_UINT32(s);
	WINPR_ATTR_UNUSED const UINT32 IoControlCode = Stream_Get_UINT32(s);
	Stream_Seek(s, 20); /* Padding */

	const BYTE* InputBuffer = Stream_ConstPointer(s);
	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, InputBufferLength))
		return ERROR_INVALID_DATA;
	Stream_Seek(s, InputBufferLength);

	WLog_Print(priv->log, WLOG_WARN,
	           "[MS-RDPEFS] 2.2.1.4.5 Device Control Request (DR_CONTROL_REQ) not implemented");
	WLog_Print(priv->log, WLOG_WARN, "TODO: parse %p [%" PRIu32 "], OutputBufferLength=%" PRIu32,
	           (const void*)InputBuffer, InputBufferLength, OutputBufferLength);

	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_io_query_volume_information_request(
    RdpdrServerContext* context, wStream* s, WINPR_ATTR_UNUSED UINT32 DeviceId,
    WINPR_ATTR_UNUSED UINT32 FileId, WINPR_ATTR_UNUSED UINT32 CompletionId)
{
	WINPR_ASSERT(context);

	RdpdrServerPrivate* priv = context->priv;
	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 32))
		return ERROR_INVALID_DATA;

	const UINT32 FsInformationClass = Stream_Get_UINT32(s);
	const UINT32 Length = Stream_Get_UINT32(s);
	Stream_Seek(s, 24); /* Padding */

	WLog_Print(priv->log, WLOG_DEBUG,
	           "Got FSInformationClass %s [0x%08" PRIx32 "], Length %" PRIu32,
	           FSInformationClass2Tag(FsInformationClass), FsInformationClass, Length);

	const BYTE* QueryVolumeBuffer = Stream_ConstPointer(s);
	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, Length))
		return ERROR_INVALID_DATA;
	Stream_Seek(s, Length);

	WLog_Print(priv->log, WLOG_WARN,
	           "[MS-RDPEFS] 2.2.3.3.6 Server Drive Query Volume Information Request "
	           "(DR_DRIVE_QUERY_VOLUME_INFORMATION_REQ) not implemented");
	WLog_Print(priv->log, WLOG_WARN, "TODO: parse %p", (const void*)QueryVolumeBuffer);

	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_io_set_volume_information_request(
    RdpdrServerContext* context, wStream* s, WINPR_ATTR_UNUSED UINT32 DeviceId,
    WINPR_ATTR_UNUSED UINT32 FileId, WINPR_ATTR_UNUSED UINT32 CompletionId)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 32))
		return ERROR_INVALID_DATA;

	const UINT32 FsInformationClass = Stream_Get_UINT32(s);
	const UINT32 Length = Stream_Get_UINT32(s);
	Stream_Seek(s, 24); /* Padding */

	WLog_Print(priv->log, WLOG_DEBUG,
	           "Got FSInformationClass %s [0x%08" PRIx32 "], Length %" PRIu32,
	           FSInformationClass2Tag(FsInformationClass), FsInformationClass, Length);

	const BYTE* SetVolumeBuffer = Stream_ConstPointer(s);
	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, Length))
		return ERROR_INVALID_DATA;
	Stream_Seek(s, Length);

	WLog_Print(priv->log, WLOG_WARN,
	           "[MS-RDPEFS] 2.2.3.3.7 Server Drive Set Volume Information Request "
	           "(DR_DRIVE_SET_VOLUME_INFORMATION_REQ) not implemented");
	WLog_Print(priv->log, WLOG_WARN, "TODO: parse %p", (const void*)SetVolumeBuffer);

	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_io_query_information_request(RdpdrServerContext* context,
                                                              wStream* s,
                                                              WINPR_ATTR_UNUSED UINT32 DeviceId,
                                                              WINPR_ATTR_UNUSED UINT32 FileId,
                                                              WINPR_ATTR_UNUSED UINT32 CompletionId)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 32))
		return ERROR_INVALID_DATA;

	const UINT32 FsInformationClass = Stream_Get_UINT32(s);
	const UINT32 Length = Stream_Get_UINT32(s);
	Stream_Seek(s, 24); /* Padding */

	WLog_Print(priv->log, WLOG_DEBUG,
	           "Got FSInformationClass %s [0x%08" PRIx32 "], Length %" PRIu32,
	           FSInformationClass2Tag(FsInformationClass), FsInformationClass, Length);

	const BYTE* QueryBuffer = Stream_ConstPointer(s);
	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, Length))
		return ERROR_INVALID_DATA;
	Stream_Seek(s, Length);

	WLog_Print(priv->log, WLOG_WARN,
	           "[MS-RDPEFS] 2.2.3.3.8 Server Drive Query Information Request "
	           "(DR_DRIVE_QUERY_INFORMATION_REQ) not implemented");
	WLog_Print(priv->log, WLOG_WARN, "TODO: parse %p", (const void*)QueryBuffer);

	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_io_set_information_request(RdpdrServerContext* context, wStream* s,
                                                            WINPR_ATTR_UNUSED UINT32 DeviceId,
                                                            WINPR_ATTR_UNUSED UINT32 FileId,
                                                            WINPR_ATTR_UNUSED UINT32 CompletionId)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 32))
		return ERROR_INVALID_DATA;

	const UINT32 FsInformationClass = Stream_Get_UINT32(s);
	const UINT32 Length = Stream_Get_UINT32(s);
	Stream_Seek(s, 24); /* Padding */

	WLog_Print(priv->log, WLOG_DEBUG,
	           "Got FSInformationClass %s [0x%08" PRIx32 "], Length %" PRIu32,
	           FSInformationClass2Tag(FsInformationClass), FsInformationClass, Length);

	const BYTE* SetBuffer = Stream_ConstPointer(s);
	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, Length))
		return ERROR_INVALID_DATA;
	Stream_Seek(s, Length);

	WLog_Print(priv->log, WLOG_WARN,
	           "[MS-RDPEFS] 2.2.3.3.9 Server Drive Set Information Request "
	           "(DR_DRIVE_SET_INFORMATION_REQ) not implemented");
	WLog_Print(priv->log, WLOG_WARN, "TODO: parse %p", (const void*)SetBuffer);

	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_io_query_directory_request(RdpdrServerContext* context, wStream* s,
                                                            WINPR_ATTR_UNUSED UINT32 DeviceId,
                                                            WINPR_ATTR_UNUSED UINT32 FileId,
                                                            WINPR_ATTR_UNUSED UINT32 CompletionId)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 32))
		return ERROR_INVALID_DATA;

	const UINT32 FsInformationClass = Stream_Get_UINT32(s);
	const BYTE InitialQuery = Stream_Get_UINT8(s);
	const UINT32 PathLength = Stream_Get_UINT32(s);
	Stream_Seek(s, 23); /* Padding */

	const WCHAR* wPath = rdpdr_read_ustring(priv->log, s, PathLength);
	if (!wPath && (PathLength > 0))
		return ERROR_INVALID_DATA;

	char* Path = ConvertWCharNToUtf8Alloc(wPath, PathLength / sizeof(WCHAR), nullptr);
	WLog_Print(priv->log, WLOG_DEBUG,
	           "Got FSInformationClass %s [0x%08" PRIx32 "], InitialQuery [%" PRIu8
	           "] Path[%" PRIu32 "] %s",
	           FSInformationClass2Tag(FsInformationClass), FsInformationClass, InitialQuery,
	           PathLength, Path);
	free(Path);

	WLog_Print(priv->log, WLOG_WARN,
	           "[MS-RDPEFS] 2.2.3.3.10 Server Drive Query Directory Request "
	           "(DR_DRIVE_QUERY_DIRECTORY_REQ) not implemented");
	WLog_Print(priv->log, WLOG_WARN, "TODO");

	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_io_change_directory_request(RdpdrServerContext* context,
                                                             wStream* s,
                                                             WINPR_ATTR_UNUSED UINT32 DeviceId,
                                                             WINPR_ATTR_UNUSED UINT32 FileId,
                                                             WINPR_ATTR_UNUSED UINT32 CompletionId)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 32))
		return ERROR_INVALID_DATA;

	WINPR_ATTR_UNUSED const BYTE WatchTree = Stream_Get_UINT8(s);
	WINPR_ATTR_UNUSED const UINT32 CompletionFilter = Stream_Get_UINT32(s);
	Stream_Seek(s, 27); /* Padding */

	WLog_Print(priv->log, WLOG_WARN,
	           "[MS-RDPEFS] 2.2.3.3.11 Server Drive NotifyChange Directory Request "
	           "(DR_DRIVE_NOTIFY_CHANGE_DIRECTORY_REQ) not implemented");
	WLog_Print(priv->log, WLOG_WARN, "TODO");

	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_io_directory_control_request(RdpdrServerContext* context,
                                                              wStream* s, UINT32 DeviceId,
                                                              UINT32 FileId, UINT32 CompletionId,
                                                              UINT32 MinorFunction)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;
	switch (MinorFunction)
	{
		case IRP_MN_QUERY_DIRECTORY:
			return rdpdr_server_receive_io_query_directory_request(context, s, DeviceId, FileId,
			                                                       CompletionId);
		case IRP_MN_NOTIFY_CHANGE_DIRECTORY:
			return rdpdr_server_receive_io_change_directory_request(context, s, DeviceId, FileId,
			                                                        CompletionId);
		default:
			WLog_Print(priv->log, WLOG_WARN,
			           "[MS-RDPEFS] 2.2.1.4 Device I/O Request (DR_DEVICE_IOREQUEST) "
			           "MajorFunction=%s, MinorFunction=%08" PRIx32 " is not supported",
			           rdpdr_irp_string(IRP_MJ_DIRECTORY_CONTROL), MinorFunction);
			return ERROR_INVALID_DATA;
	}
}

static UINT rdpdr_server_receive_io_lock_control_request(RdpdrServerContext* context, wStream* s,
                                                         WINPR_ATTR_UNUSED UINT32 DeviceId,
                                                         WINPR_ATTR_UNUSED UINT32 FileId,
                                                         WINPR_ATTR_UNUSED UINT32 CompletionId)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 32))
		return ERROR_INVALID_DATA;

	const uint32_t Operation = Stream_Get_UINT32(s);
	uint32_t Lock = Stream_Get_UINT32(s);
	const uint32_t NumLocks = Stream_Get_UINT32(s);
	Stream_Seek(s, 20); /* Padding */

	WLog_Print(priv->log, WLOG_DEBUG,
	           "IRP_MJ_LOCK_CONTROL, Operation=%s, Lock=0x%08" PRIx32 ", NumLocks=%" PRIu32,
	           DR_DRIVE_LOCK_REQ2str(Operation), Lock, NumLocks);

	Lock &= 0x01; /* Only bit 0 is of importance */

	for (UINT32 x = 0; x < NumLocks; x++)
	{
		if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 16))
			return ERROR_INVALID_DATA;

		const UINT64 Length = Stream_Get_UINT64(s);
		const UINT64 Offset = Stream_Get_UINT64(s);

		WLog_Print(priv->log, WLOG_DEBUG, "Locking at Offset=0x%08" PRIx64 " [Length %" PRIu64 "]",
		           Offset, Length);
	}

	WLog_Print(priv->log, WLOG_WARN,
	           "[MS-RDPEFS] 2.2.3.3.12 Server Drive Lock Control Request (DR_DRIVE_LOCK_REQ) "
	           "[Lock=0x%08" PRIx32 "]"
	           "not implemented",
	           Lock);
	WLog_Print(priv->log, WLOG_WARN, "TODO");

	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_device_io_request(RdpdrServerContext* context, wStream* s,
                                                   WINPR_ATTR_UNUSED const RDPDR_HEADER* header)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(header);

	RdpdrServerPrivate* priv = context->priv;

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 20))
		return ERROR_INVALID_DATA;

	const UINT32 DeviceId = Stream_Get_UINT32(s);
	const UINT32 FileId = Stream_Get_UINT32(s);
	const UINT32 CompletionId = Stream_Get_UINT32(s);
	const UINT32 MajorFunction = Stream_Get_UINT32(s);
	const UINT32 MinorFunction = Stream_Get_UINT32(s);
	if ((MinorFunction != 0) && (MajorFunction != IRP_MJ_DIRECTORY_CONTROL))
		WLog_Print(priv->log, WLOG_WARN,
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
		case IRP_MJ_SET_VOLUME_INFORMATION:
			return rdpdr_server_receive_io_set_volume_information_request(context, s, DeviceId,
			                                                              FileId, CompletionId);

		default:
			WLog_Print(
			    priv->log, WLOG_WARN,
			    "[MS-RDPEFS] 2.2.1.4 Device I/O Request (DR_DEVICE_IOREQUEST) not implemented");
			WLog_Print(priv->log, WLOG_WARN,
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(s);
	WINPR_ASSERT(header);

	RdpdrServerPrivate* priv = context->priv;

	WINPR_UNUSED(header);

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 12))
		return ERROR_INVALID_DATA;

	const UINT32 deviceId = Stream_Get_UINT32(s);
	const UINT32 completionId = Stream_Get_UINT32(s);
	const UINT32 ioStatus = Stream_Get_UINT32(s);
	WLog_Print(priv->log, WLOG_DEBUG,
	           "deviceId=%" PRIu32 ", completionId=0x%" PRIx32 ", ioStatus=0x%" PRIx32 "", deviceId,
	           completionId, ioStatus);
	RDPDR_IRP* irp = rdpdr_server_dequeue_irp(context, completionId);

	if (!irp)
	{
		WLog_Print(priv->log, WLOG_ERROR, "IRP not found for completionId=0x%" PRIx32 "",
		           completionId);
		return CHANNEL_RC_OK;
	}

	/* Invoke the callback. */
	UINT error = CHANNEL_RC_OK;
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;
	wStream* s = Stream_New(nullptr, RDPDR_HEADER_LENGTH);
	if (!s)
	{
		WLog_Print(priv->log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	const RDPDR_HEADER header = {
		.Component = RDPDR_CTYP_CORE,
		.PacketId = PAKID_CORE_USER_LOGGEDON,
	};
	Stream_Write_UINT16(s, header.Component); /* Component (2 bytes) */
	Stream_Write_UINT16(s, header.PacketId);  /* PacketId (2 bytes) */
	return rdpdr_seal_send_free_request(context, s);
}

static UINT rdpdr_server_receive_prn_cache_add_printer(RdpdrServerContext* context, wStream* s)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 24))
		return ERROR_INVALID_DATA;

	char PortDosName[9] = WINPR_C_ARRAY_INIT;
	Stream_Read(s, PortDosName, 8);
	const UINT32 PnPNameLen = Stream_Get_UINT32(s);
	const UINT32 DriverNameLen = Stream_Get_UINT32(s);
	const UINT32 PrinterNameLen = Stream_Get_UINT32(s);
	const UINT32 CachedFieldsLen = Stream_Get_UINT32(s);

	const WCHAR* PnPName = rdpdr_read_ustring(priv->log, s, PnPNameLen);
	if (!PnPName && (PnPNameLen > 0))
		return ERROR_INVALID_DATA;
	const WCHAR* DriverName = rdpdr_read_ustring(priv->log, s, DriverNameLen);
	if (!DriverName && (DriverNameLen > 0))
		return ERROR_INVALID_DATA;
	const WCHAR* PrinterName = rdpdr_read_ustring(priv->log, s, PrinterNameLen);
	if (!PrinterName && (PrinterNameLen > 0))
		return ERROR_INVALID_DATA;

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, CachedFieldsLen))
		return ERROR_INVALID_DATA;
	Stream_Seek(s, CachedFieldsLen);

	WLog_Print(priv->log, WLOG_WARN,
	           "[MS-RDPEPC] 2.2.2.3 Add Printer Cachedata (DR_PRN_ADD_CACHEDATA) not implemented");
	WLog_Print(priv->log, WLOG_WARN, "TODO");
	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_prn_cache_update_printer(RdpdrServerContext* context, wStream* s)
{
	WINPR_ASSERT(context);

	RdpdrServerPrivate* priv = context->priv;

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 8))
		return ERROR_INVALID_DATA;

	const UINT32 PrinterNameLen = Stream_Get_UINT32(s);
	const UINT32 CachedFieldsLen = Stream_Get_UINT32(s);

	const WCHAR* PrinterName = rdpdr_read_ustring(priv->log, s, PrinterNameLen);
	if (!PrinterName && (PrinterNameLen > 0))
		return ERROR_INVALID_DATA;

	const BYTE* config = Stream_ConstPointer(s);
	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, CachedFieldsLen))
		return ERROR_INVALID_DATA;
	Stream_Seek(s, CachedFieldsLen);

	WLog_Print(
	    priv->log, WLOG_WARN,
	    "[MS-RDPEPC] 2.2.2.4 Update Printer Cachedata (DR_PRN_UPDATE_CACHEDATA) not implemented");
	WLog_Print(priv->log, WLOG_WARN, "TODO: parse %p", (const void*)config);
	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_prn_cache_delete_printer(RdpdrServerContext* context, wStream* s)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 4))
		return ERROR_INVALID_DATA;

	const UINT32 PrinterNameLen = Stream_Get_UINT32(s);

	const WCHAR* PrinterName = rdpdr_read_ustring(priv->log, s, PrinterNameLen);
	if (!PrinterName && (PrinterNameLen > 0))
		return ERROR_INVALID_DATA;

	WLog_Print(
	    priv->log, WLOG_WARN,
	    "[MS-RDPEPC] 2.2.2.5 Delete Printer Cachedata (DR_PRN_DELETE_CACHEDATA) not implemented");
	WLog_Print(priv->log, WLOG_WARN, "TODO");
	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_receive_prn_cache_rename_cachedata(RdpdrServerContext* context, wStream* s)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 8))
		return ERROR_INVALID_DATA;

	const UINT32 OldPrinterNameLen = Stream_Get_UINT32(s);
	const UINT32 NewPrinterNameLen = Stream_Get_UINT32(s);

	const WCHAR* OldPrinterName = rdpdr_read_ustring(priv->log, s, OldPrinterNameLen);
	if (!OldPrinterName && (OldPrinterNameLen > 0))
		return ERROR_INVALID_DATA;
	const WCHAR* NewPrinterName = rdpdr_read_ustring(priv->log, s, NewPrinterNameLen);
	if (!NewPrinterName && (NewPrinterNameLen > 0))
		return ERROR_INVALID_DATA;

	WLog_Print(
	    priv->log, WLOG_WARN,
	    "[MS-RDPEPC] 2.2.2.6 Rename Printer Cachedata (DR_PRN_RENAME_CACHEDATA) not implemented");
	WLog_Print(priv->log, WLOG_WARN, "TODO");
	return CHANNEL_RC_OK;
}

static UINT
rdpdr_server_receive_prn_cache_data_request(RdpdrServerContext* context, wStream* s,
                                            WINPR_ATTR_UNUSED const RDPDR_HEADER* header)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(header);

	RdpdrServerPrivate* priv = context->priv;
	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 4))
		return ERROR_INVALID_DATA;

	const UINT32 EventId = Stream_Get_UINT32(s);
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
			WLog_Print(priv->log, WLOG_WARN,
			           "[MS-RDPEPC] PAKID_PRN_CACHE_DATA unknown EventId=0x%08" PRIx32, EventId);
			return ERROR_INVALID_DATA;
	}
}

static UINT rdpdr_server_receive_prn_using_xps_request(RdpdrServerContext* context, wStream* s,
                                                       WINPR_ATTR_UNUSED const RDPDR_HEADER* header)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(header);

	RdpdrServerPrivate* priv = context->priv;

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 8))
		return ERROR_INVALID_DATA;

	const UINT32 PrinterId = Stream_Get_UINT32(s);
	const UINT32 Flags = Stream_Get_UINT32(s);

	WLog_Print(
	    priv->log, WLOG_WARN,
	    "[MS-RDPEPC] 2.2.2.2 Server Printer Set XPS Mode (DR_PRN_USING_XPS) not implemented");
	WLog_Print(priv->log, WLOG_WARN, "PrinterId=0x%08" PRIx32 ", Flags=0x%08" PRIx32, PrinterId,
	           Flags);
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

	RdpdrServerPrivate* priv = context->priv;
	WLog_Print(priv->log, WLOG_DEBUG,
	           "receiving message {Component %s[%04" PRIx16 "], PacketId %s[%04" PRIx16 "]",
	           rdpdr_component_string(header->Component), header->Component,
	           rdpdr_packetid_string(header->PacketId), header->PacketId);

	if (header->Component == RDPDR_CTYP_CORE)
	{
		switch (header->PacketId)
		{
			case PAKID_CORE_SERVER_ANNOUNCE:
				WLog_Print(priv->log, WLOG_ERROR,
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
				WLog_Print(priv->log, WLOG_ERROR,
				           "[MS-RDPEFS] 2.2.2.5 Server User Logged On (DR_CORE_USER_LOGGEDON) "
				           "must not be sent by a client!");
				break;

			case PAKID_CORE_SERVER_CAPABILITY:
				WLog_Print(priv->log, WLOG_ERROR,
				           "[MS-RDPEFS] 2.2.2.7 Server Core Capability Request "
				           "(DR_CORE_CAPABILITY_REQ) must not be sent by a client!");
				break;

			case PAKID_CORE_CLIENT_CAPABILITY:
				error = rdpdr_server_receive_core_capability_response(context, s, header);
				if (error == CHANNEL_RC_OK)
				{
					if (priv->UserLoggedOnPdu)
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
				WLog_Print(priv->log, WLOG_ERROR,
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
				WLog_Print(priv->log, WLOG_WARN,
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
				WLog_Print(priv->log, WLOG_WARN,
				           "Unknown RDPDR_HEADER.Component: %s [0x%04" PRIx16 "], PacketId: %s",
				           rdpdr_component_string(header->Component), header->Component,
				           rdpdr_packetid_string(header->PacketId));
				break;
		}
	}
	else
	{
		WLog_Print(priv->log, WLOG_WARN,
		           "Unknown RDPDR_HEADER.Component: %s [0x%04" PRIx16 "], PacketId: %s",
		           rdpdr_component_string(header->Component), header->Component,
		           rdpdr_packetid_string(header->PacketId));
	}

	return IFCALLRESULT(error, context->ReceivePDU, context, header, error);
}

static DWORD WINAPI rdpdr_server_thread(LPVOID arg)
{
	RdpdrServerContext* context = (RdpdrServerContext*)arg;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	UINT error = 0;
	wStream* s = nullptr;
	RdpdrServerPrivate* priv = context->priv;
	ChannelPduTracker* tracker = ChannelPduTracker_new(priv->ChannelHandle);
	if (!tracker)
		goto out;

	void* buffer = nullptr;
	HANDLE ChannelEvent = nullptr;
	DWORD BytesReturned = 0;
	if (!WTSVirtualChannelQuery(priv->ChannelHandle, WTSVirtualEventHandle, &buffer,
	                            &BytesReturned))
	{
		WLog_Print(priv->log, WLOG_ERROR, "error retrieving WTSVirtualEventHandle");
		goto out;
	}

	if (BytesReturned != sizeof(HANDLE))
	{
		WLog_Print(priv->log, WLOG_ERROR, "invalid size for WTSVirtualEventHandle");
		WTSFreeMemory(buffer);
		goto out;
	}

	ChannelEvent = *(HANDLE*)buffer;
	WTSFreeMemory(buffer);

	if ((error = rdpdr_server_send_announce_request(context)))
	{
		WLog_Print(priv->log, WLOG_ERROR,
		           "rdpdr_server_send_announce_request failed with error %" PRIu32 "!", error);
		goto out;
	}

	HANDLE events[2] = { priv->StopEvent, ChannelEvent };

	while (1)
	{
		DWORD status = WaitForMultipleObjects(2, events, FALSE, INFINITE);

		switch (status)
		{
			case WAIT_OBJECT_0:
				/* StopEvent */
				goto out;
			case WAIT_OBJECT_0 + 1:
			{
				s = ChannelPduTracker_poll(tracker);
				if (!s)
					break;

				if (Stream_GetRemainingLength(s) < RDPDR_HEADER_LENGTH)
				{
					error = ERROR_INTERNAL_ERROR;
					goto out;
				}

				const RDPDR_HEADER header = {
					.Component = Stream_Get_UINT16(s), /* Component (2 bytes) */
					.PacketId = Stream_Get_UINT16(s),  /* PacketId (2 bytes) */
				};

				if ((error = rdpdr_server_receive_pdu(context, s, &header)))
				{
					WLog_Print(priv->log, WLOG_ERROR,
					           "rdpdr_server_receive_pdu failed with error %" PRIu32 "!", error);
					goto out;
				}
				Stream_Release(s);
				s = nullptr;
				break;
			}
			case WAIT_FAILED:
			default:
				error = GetLastError();
				WLog_Print(priv->log, WLOG_ERROR,
				           "WaitForMultipleObjects failed with error %" PRIu32 "!", error);
				goto out;
		}
	}

out:
	ChannelPduTracker_free(tracker);

	if (s)
		Stream_Release(s);

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

	PULONG pSessionId = nullptr;
	DWORD BytesReturned = 0;

	if (!WTSQuerySessionInformationA(context->vcm, WTS_CURRENT_SESSION, WTSSessionId,
	                                 (LPSTR*)&pSessionId, &BytesReturned))
		return CHANNEL_RC_BAD_CHANNEL;

	DWORD SessionId = (DWORD)*pSessionId;
	WTSFreeMemory(pSessionId);

	RdpdrServerPrivate* priv = context->priv;
	priv->ChannelHandle =
	    WTSVirtualChannelOpenEx(SessionId, RDPDR_SVC_CHANNEL_NAME, CHANNEL_OPTION_SHOW_PROTOCOL);

	if (!priv->ChannelHandle)
	{
		WLog_Print(priv->log, WLOG_ERROR, "WTSVirtualChannelOpen failed!");
		return CHANNEL_RC_BAD_CHANNEL;
	}

	if (!(priv->StopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr)))
	{
		WLog_Print(priv->log, WLOG_ERROR, "CreateEvent failed!");
		return ERROR_INTERNAL_ERROR;
	}

	if (!(priv->Thread = CreateThread(nullptr, 0, rdpdr_server_thread, (void*)context, 0, nullptr)))
	{
		WLog_Print(priv->log, WLOG_ERROR, "CreateThread failed!");
		(void)CloseHandle(priv->StopEvent);
		priv->StopEvent = nullptr;
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;
	if (priv->StopEvent)
	{
		(void)SetEvent(priv->StopEvent);

		if (WaitForSingleObject(priv->Thread, INFINITE) == WAIT_FAILED)
		{
			UINT error = GetLastError();
			WLog_Print(priv->log, WLOG_ERROR, "WaitForSingleObject failed with error %" PRIu32 "!",
			           error);
			return error;
		}

		(void)CloseHandle(priv->Thread);
		priv->Thread = nullptr;
		(void)CloseHandle(priv->StopEvent);
		priv->StopEvent = nullptr;
	}

	if (priv->ChannelHandle)
	{
		(void)WTSVirtualChannelClose(priv->ChannelHandle);
		priv->ChannelHandle = nullptr;
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
	WINPR_ASSERT(fdi);
	ZeroMemory(fdi, sizeof(FILE_DIRECTORY_INFORMATION));

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 64))
		return ERROR_INVALID_DATA;

	fdi->NextEntryOffset = Stream_Get_UINT32(s);        /* NextEntryOffset (4 bytes) */
	fdi->FileIndex = Stream_Get_UINT32(s);              /* FileIndex (4 bytes) */
	fdi->CreationTime.QuadPart = Stream_Get_INT64(s);   /* CreationTime (8 bytes) */
	fdi->LastAccessTime.QuadPart = Stream_Get_INT64(s); /* LastAccessTime (8 bytes) */
	fdi->LastWriteTime.QuadPart = Stream_Get_INT64(s);  /* LastWriteTime (8 bytes) */
	fdi->ChangeTime.QuadPart = Stream_Get_INT64(s);     /* ChangeTime (8 bytes) */
	fdi->EndOfFile.QuadPart = Stream_Get_INT64(s);      /* EndOfFile (8 bytes) */
	fdi->AllocationSize.QuadPart = Stream_Get_INT64(s); /* AllocationSize (8 bytes) */
	fdi->FileAttributes = Stream_Get_UINT32(s);         /* FileAttributes (4 bytes) */
	const UINT32 fileNameLength = Stream_Get_UINT32(s); /* FileNameLength (4 bytes) */

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, fileNameLength))
		return ERROR_INVALID_DATA;

	if (fileNameLength / sizeof(WCHAR) > ARRAYSIZE(fdi->FileName))
		return ERROR_INVALID_DATA;

#if defined(__MINGW32__) || defined(WITH_WCHAR_FILE_DIRECTORY_INFORMATION)
	if (Stream_Read_UTF16_String(s, fdi->FileName, fileNameLength / sizeof(WCHAR)))
		return ERROR_INVALID_DATA;
#else
	if (Stream_Read_UTF16_String_As_UTF8_Buffer(s, fileNameLength / sizeof(WCHAR), fdi->FileName,
	                                            ARRAYSIZE(fdi->FileName)) < 0)
		return ERROR_INVALID_DATA;
#endif
	return CHANNEL_RC_OK;
}

static UINT prepare_irp(RdpdrServerContext* context, UINT32 deviceId, RDPDR_IRP_Callback callback,
                        void* callbackData, RDPDR_IRP** outIrp)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(callbackData);
	WINPR_ASSERT(outIrp);

	RdpdrServerPrivate* priv = context->priv;

	RDPDR_IRP* irp = rdpdr_server_irp_new();
	if (!irp)
	{
		WLog_Print(priv->log, WLOG_ERROR, "rdpdr_server_irp_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	irp->CompletionId = priv->NextCompletionId++;
	irp->Callback = callback;
	irp->CallbackData = callbackData;
	irp->DeviceId = deviceId;

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_Print(priv->log, WLOG_ERROR, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	*outIrp = irp;
	return CHANNEL_RC_OK;
}

static UINT prepare_smartcard_irp(RdpdrServerContext* context, UINT32 ioControlCode,
                                  RDPDR_IRP_Callback callback, void* callbackData,
                                  RDPDR_IRP** outIrp)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(callbackData);

	const char* cmd = scard_get_ioctl_string(ioControlCode, FALSE);

	RdpdrServerPrivate* priv = context->priv;
	if (!priv->haveSmartcardDevice)
	{
		WLog_Print(priv->log, WLOG_ERROR, "%s - no smartcard device registered", cmd);
		return ERROR_BAD_DEVICE;
	}

	RDPDR_IRP* irp = nullptr;
	UINT error = prepare_irp(context, priv->smartcardDeviceId, callback, callbackData, &irp);
	if (error != CHANNEL_RC_OK)
		return error;

	irp->IoControlCode = ioControlCode;
	*outIrp = irp;
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;

	WLog_Print(priv->log, WLOG_DEBUG,
	           "RdpdrServerSendDeviceCreateRequest: deviceId=%" PRIu32
	           ", path=%s, desiredAccess=0x%" PRIx32 " createOptions=0x%" PRIx32
	           " createDisposition=0x%" PRIx32 "",
	           deviceId, path, desiredAccess, createOptions, createDisposition);
	/* Compute the required Unicode size. */
	size_t pathLength = (strlen(path) + 1U) * sizeof(WCHAR);
	wStream* s = Stream_New(nullptr, 256U + pathLength);

	if (!s)
	{
		WLog_Print(priv->log, WLOG_ERROR, "Stream_New failed!");
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
	{
		Stream_Release(s);
		return ERROR_INTERNAL_ERROR;
	}
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;

	WLog_Print(priv->log, WLOG_DEBUG,
	           "RdpdrServerSendDeviceCloseRequest: deviceId=%" PRIu32 ", fileId=%" PRIu32 "",
	           deviceId, fileId);
	wStream* s = Stream_New(nullptr, 128);

	if (!s)
	{
		WLog_Print(priv->log, WLOG_ERROR, "Stream_New failed!");
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;

	WLog_Print(priv->log, WLOG_DEBUG,
	           "RdpdrServerSendDeviceReadRequest: deviceId=%" PRIu32 ", fileId=%" PRIu32
	           ", length=%" PRIu32 ", offset=%" PRIu32 "",
	           deviceId, fileId, length, offset);
	wStream* s = Stream_New(nullptr, 128);

	if (!s)
	{
		WLog_Print(priv->log, WLOG_ERROR, "Stream_New failed!");
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;

	WLog_Print(priv->log, WLOG_DEBUG,
	           "RdpdrServerSendDeviceWriteRequest: deviceId=%" PRIu32 ", fileId=%" PRIu32
	           ", length=%" PRIu32 ", offset=%" PRIu32 "",
	           deviceId, fileId, length, offset);
	wStream* s = Stream_New(nullptr, 64 + length);

	if (!s)
	{
		WLog_Print(priv->log, WLOG_ERROR, "Stream_New failed!");
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;

	WLog_Print(priv->log, WLOG_DEBUG,
	           "RdpdrServerSendDeviceQueryDirectoryRequest: deviceId=%" PRIu32 ", fileId=%" PRIu32
	           ", path=%s",
	           deviceId, fileId, path);
	/* Compute the required Unicode size. */
	size_t pathLength = path ? (strlen(path) + 1) * sizeof(WCHAR) : 0;
	wStream* s = Stream_New(nullptr, 64 + pathLength);

	if (!s)
	{
		WLog_Print(priv->log, WLOG_ERROR, "Stream_New failed!");
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
		{
			Stream_Release(s);
			return ERROR_INTERNAL_ERROR;
		}
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpdrServerPrivate* priv = context->priv;

	WLog_Print(priv->log, WLOG_DEBUG,
	           "RdpdrServerSendDeviceFileNameRequest: deviceId=%" PRIu32 ", fileId=%" PRIu32
	           ", path=%s",
	           deviceId, fileId, path);
	/* Compute the required Unicode size. */
	size_t pathLength = path ? (strlen(path) + 1) * sizeof(WCHAR) : 0;
	wStream* s = Stream_New(nullptr, 64 + pathLength);

	if (!s)
	{
		WLog_Print(priv->log, WLOG_ERROR, "Stream_New failed!");
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
		{
			Stream_Release(s);
			return ERROR_INTERNAL_ERROR;
		}
	}

	return rdpdr_seal_send_free_request(context, s);
}

static void rdpdr_server_convert_slashes(char* path, int size)
{
	WINPR_ASSERT(path || (size <= 0));

	for (int i = 0; (i < size) && (path[i] != '\0'); i++)
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

	RdpdrServerPrivate* priv = context->priv;
	WLog_Print(priv->log, WLOG_DEBUG,
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(s);
	WINPR_ASSERT(irp);

	RdpdrServerPrivate* priv = context->priv;
	WLog_Print(priv->log, WLOG_DEBUG,
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

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 5))
		return ERROR_INVALID_DATA;

	const uint32_t fileId = Stream_Get_UINT32(s);    /* FileId (4 bytes) */
	const uint8_t information = Stream_Get_UINT8(s); /* Information (1 byte) */
	WLog_Print(priv->log, WLOG_DEBUG, "fileId [0x%08" PRIx32 "], information %s", fileId,
	           fileInformation2str(information));

	/* Setup the IRP. */
	irp->CompletionId = priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_create_directory_callback2;
	irp->DeviceId = deviceId;
	irp->FileId = fileId;

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_Print(priv->log, WLOG_ERROR, "rdpdr_server_enqueue_irp failed!");
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(callbackData);
	WINPR_ASSERT(path);

	RDPDR_IRP* irp = nullptr;
	UINT ret = prepare_irp(context, deviceId, rdpdr_server_drive_create_directory_callback1,
	                       callbackData, &irp);
	if (ret != CHANNEL_RC_OK)
		return ret;

	strncpy(irp->PathName, path, sizeof(irp->PathName) - 1);
	rdpdr_server_convert_slashes(irp->PathName, sizeof(irp->PathName));

	/* Send a request to open the file. */
	return rdpdr_server_send_device_create_request(
	    context, irp->DeviceId, irp->CompletionId, irp->PathName, FILE_READ_DATA | SYNCHRONIZE,
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

	RdpdrServerPrivate* priv = context->priv;
	WLog_Print(priv->log, WLOG_DEBUG,
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);

	RdpdrServerPrivate* priv = context->priv;
	WLog_Print(priv->log, WLOG_DEBUG,
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

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 5))
		return ERROR_INVALID_DATA;

	const uint32_t fileId = Stream_Get_UINT32(s);    /* FileId (4 bytes) */
	const uint8_t information = Stream_Get_UINT8(s); /* Information (1 byte) */
	WLog_Print(priv->log, WLOG_DEBUG, "fileId [0x%08" PRIx32 "], information %s", fileId,
	           fileInformation2str(information));

	/* Setup the IRP. */
	irp->CompletionId = priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_delete_directory_callback2;
	irp->DeviceId = deviceId;
	irp->FileId = fileId;

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_Print(priv->log, WLOG_ERROR, "rdpdr_server_enqueue_irp failed!");
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RDPDR_IRP* irp = nullptr;
	UINT ret = prepare_irp(context, deviceId, rdpdr_server_drive_delete_directory_callback1,
	                       callbackData, &irp);
	if (ret != CHANNEL_RC_OK)
		return ret;

	strncpy(irp->PathName, path, sizeof(irp->PathName) - 1);
	rdpdr_server_convert_slashes(irp->PathName, sizeof(irp->PathName));

	/* Send a request to open the file. */
	return rdpdr_server_send_device_create_request(
	    context, irp->DeviceId, irp->CompletionId, irp->PathName, DELETE | SYNCHRONIZE,
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);

	RdpdrServerPrivate* priv = context->priv;
	WLog_Print(priv->log, WLOG_DEBUG,
	           "RdpdrServerDriveQueryDirectoryCallback2: deviceId=%" PRIu32
	           ", completionId=%" PRIu32 ", ioStatus=0x%" PRIx32 "",
	           deviceId, completionId, ioStatus);

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 4))
		return ERROR_INVALID_DATA;

	const UINT32 length = Stream_Get_UINT32(s); /* Length (4 bytes) */

	FILE_DIRECTORY_INFORMATION fdi = WINPR_C_ARRAY_INIT;
	if (length > 0)
	{
		UINT error = rdpdr_server_read_file_directory_information(priv->log, s, &fdi);
		if (error)
		{
			WLog_Print(priv->log, WLOG_ERROR,
			           "rdpdr_server_read_file_directory_information failed with error %" PRIu32
			           "!",
			           error);
			return error;
		}
	}
	else
	{
		if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 1))
			return ERROR_INVALID_DATA;

		Stream_Seek(s, 1); /* Padding (1 byte) */
	}

	if (ioStatus == STATUS_SUCCESS)
	{
		/* Invoke the query directory completion routine. */
		context->OnDriveQueryDirectoryComplete(context, irp->CallbackData, ioStatus,
		                                       length > 0 ? &fdi : nullptr);
		/* Setup the IRP. */
		irp->CompletionId = priv->NextCompletionId++;
		irp->Callback = rdpdr_server_drive_query_directory_callback2;

		if (!rdpdr_server_enqueue_irp(context, irp))
		{
			WLog_Print(priv->log, WLOG_ERROR, "rdpdr_server_enqueue_irp failed!");
			rdpdr_server_irp_free(irp);
			return ERROR_INTERNAL_ERROR;
		}

		/* Send a request to query the directory. */
		return rdpdr_server_send_device_query_directory_request(context, irp->DeviceId, irp->FileId,
		                                                        irp->CompletionId, nullptr);
	}
	else
	{
		/* Invoke the query directory completion routine. */
		context->OnDriveQueryDirectoryComplete(context, irp->CallbackData, ioStatus, nullptr);
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);

	RdpdrServerPrivate* priv = context->priv;
	WLog_Print(priv->log, WLOG_DEBUG,
	           "RdpdrServerDriveQueryDirectoryCallback1: deviceId=%" PRIu32
	           ", completionId=%" PRIu32 ", ioStatus=0x%" PRIx32 "",
	           deviceId, completionId, ioStatus);

	if (ioStatus != STATUS_SUCCESS)
	{
		/* Invoke the query directory completion routine. */
		context->OnDriveQueryDirectoryComplete(context, irp->CallbackData, ioStatus, nullptr);
		/* Destroy the IRP. */
		rdpdr_server_irp_free(irp);
		return CHANNEL_RC_OK;
	}

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 4))
		return ERROR_INVALID_DATA;

	const uint32_t fileId = Stream_Get_UINT32(s);
	/* Setup the IRP. */
	irp->CompletionId = priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_query_directory_callback2;
	irp->DeviceId = deviceId;
	irp->FileId = fileId;
	winpr_str_append("\\*.*", irp->PathName, ARRAYSIZE(irp->PathName), nullptr);

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_Print(priv->log, WLOG_ERROR, "rdpdr_server_enqueue_irp failed!");
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RDPDR_IRP* irp = nullptr;
	UINT ret = prepare_irp(context, deviceId, rdpdr_server_drive_query_directory_callback1,
	                       callbackData, &irp);
	if (ret != CHANNEL_RC_OK)
		return ret;

	strncpy(irp->PathName, path, sizeof(irp->PathName) - 1);
	rdpdr_server_convert_slashes(irp->PathName, sizeof(irp->PathName));

	/* Send a request to open the directory. */
	return rdpdr_server_send_device_create_request(
	    context, irp->DeviceId, irp->CompletionId, irp->PathName, FILE_READ_DATA | SYNCHRONIZE,
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);

	RdpdrServerPrivate* priv = context->priv;
	WLog_Print(priv->log, WLOG_DEBUG,
	           "RdpdrServerDriveOpenFileCallback: deviceId=%" PRIu32 ", completionId=%" PRIu32
	           ", ioStatus=0x%" PRIx32 "",
	           deviceId, completionId, ioStatus);

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 5))
		return ERROR_INVALID_DATA;

	const uint32_t fileId = Stream_Get_UINT32(s);    /* FileId (4 bytes) */
	const uint8_t information = Stream_Get_UINT8(s); /* Information (1 byte) */
	WLog_Print(priv->log, WLOG_DEBUG, "fileId [0x%08" PRIx32 "], information %s", fileId,
	           fileInformation2str(information));

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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RDPDR_IRP* irp = nullptr;
	UINT ret =
	    prepare_irp(context, deviceId, rdpdr_server_drive_open_file_callback, callbackData, &irp);
	if (ret != CHANNEL_RC_OK)
		return ret;

	strncpy(irp->PathName, path, sizeof(irp->PathName) - 1);
	rdpdr_server_convert_slashes(irp->PathName, sizeof(irp->PathName));

	/* Send a request to open the file. */
	return rdpdr_server_send_device_create_request(context, irp->DeviceId, irp->CompletionId,
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);

	RdpdrServerPrivate* priv = context->priv;
	WLog_Print(priv->log, WLOG_DEBUG,
	           "RdpdrServerDriveReadFileCallback: deviceId=%" PRIu32 ", completionId=%" PRIu32
	           ", ioStatus=0x%" PRIx32 "",
	           deviceId, completionId, ioStatus);

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 4))
		return ERROR_INVALID_DATA;

	const UINT32 length = Stream_Get_UINT32(s); /* Length (4 bytes) */
	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, length))
		return ERROR_INVALID_DATA;

	char* buffer = nullptr;
	if (length > 0)
	{
		buffer = Stream_PointerAs(s, char);
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RDPDR_IRP* irp = nullptr;
	UINT ret =
	    prepare_irp(context, deviceId, rdpdr_server_drive_read_file_callback, callbackData, &irp);
	if (ret != CHANNEL_RC_OK)
		return ret;

	irp->FileId = fileId;

	/* Send a request to open the directory. */
	return rdpdr_server_send_device_read_request(context, irp->DeviceId, irp->FileId,
	                                             irp->CompletionId, length, offset);
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);

	RdpdrServerPrivate* priv = context->priv;
	WLog_Print(priv->log, WLOG_DEBUG,
	           "RdpdrServerDriveWriteFileCallback: deviceId=%" PRIu32 ", completionId=%" PRIu32
	           ", ioStatus=0x%" PRIx32 "",
	           deviceId, completionId, ioStatus);

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 5))
		return ERROR_INVALID_DATA;

	const UINT32 length = Stream_Get_UINT32(s); /* Length (4 bytes) */
	Stream_Seek(s, 1);                          /* Padding (1 byte) */

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, length))
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RDPDR_IRP* irp = nullptr;
	UINT ret =
	    prepare_irp(context, deviceId, rdpdr_server_drive_write_file_callback, callbackData, &irp);
	if (ret != CHANNEL_RC_OK)
		return ret;

	irp->FileId = fileId;

	/* Send a request to open the directory. */
	return rdpdr_server_send_device_write_request(context, irp->DeviceId, irp->FileId,
	                                              irp->CompletionId, buffer, length, offset);
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

	RdpdrServerPrivate* priv = context->priv;

	WLog_Print(priv->log, WLOG_DEBUG,
	           "RdpdrServerDriveCloseFileCallback: deviceId=%" PRIu32 ", completionId=%" PRIu32
	           ", ioStatus=0x%" PRIx32 "",
	           deviceId, completionId, ioStatus);

	// padding 5 bytes
	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 5))
		return ERROR_INVALID_DATA;

	Stream_Seek(s, 5);

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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RDPDR_IRP* irp = nullptr;
	UINT ret =
	    prepare_irp(context, deviceId, rdpdr_server_drive_close_file_callback, callbackData, &irp);
	if (ret != CHANNEL_RC_OK)
		return ret;

	irp->FileId = fileId;

	/* Send a request to open the directory. */
	return rdpdr_server_send_device_close_request(context, irp->DeviceId, irp->FileId,
	                                              irp->CompletionId);
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

	RdpdrServerPrivate* priv = context->priv;
	WLog_Print(priv->log, WLOG_DEBUG,
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);

	RdpdrServerPrivate* priv = context->priv;
	WLog_Print(priv->log, WLOG_DEBUG,
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

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 5))
		return ERROR_INVALID_DATA;

	const uint32_t fileId = Stream_Get_UINT32(s);    /* FileId (4 bytes) */
	const uint8_t information = Stream_Get_UINT8(s); /* Information (1 byte) */

	WLog_Print(priv->log, WLOG_DEBUG, "fileId [0x%08" PRIx32 "], information %s", fileId,
	           fileInformation2str(information));
	/* Setup the IRP. */
	irp->CompletionId = priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_delete_file_callback2;
	irp->DeviceId = deviceId;
	irp->FileId = fileId;

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_Print(priv->log, WLOG_ERROR, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to close the file */
	return rdpdr_server_send_device_close_request(context, irp->DeviceId, irp->FileId,
	                                              irp->CompletionId);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_delete_file(RdpdrServerContext* context, void* callbackData,
                                           UINT32 deviceId, const char* path)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RDPDR_IRP* irp = nullptr;
	UINT ret = prepare_irp(context, deviceId, rdpdr_server_drive_delete_file_callback1,
	                       callbackData, &irp);
	if (ret != CHANNEL_RC_OK)
		return ret;

	strncpy(irp->PathName, path, sizeof(irp->PathName) - 1);
	rdpdr_server_convert_slashes(irp->PathName, sizeof(irp->PathName));

	/* Send a request to open the file. */
	return rdpdr_server_send_device_create_request(
	    context, irp->DeviceId, irp->CompletionId, irp->PathName, FILE_READ_DATA | SYNCHRONIZE,
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

	RdpdrServerPrivate* priv = context->priv;
	WLog_Print(priv->log, WLOG_DEBUG,
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);

	RdpdrServerPrivate* priv = context->priv;
	WLog_Print(priv->log, WLOG_DEBUG,
	           "RdpdrServerDriveRenameFileCallback2: deviceId=%" PRIu32 ", completionId=%" PRIu32
	           ", ioStatus=0x%" PRIx32 "",
	           deviceId, completionId, ioStatus);

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 5))
		return ERROR_INVALID_DATA;

	WINPR_ATTR_UNUSED const UINT32 length = Stream_Get_UINT32(s); /* Length (4 bytes) */
	Stream_Seek(s, 1);                                            /* Padding (1 byte) */

	/* Invoke the rename file completion routine. */
	context->OnDriveRenameFileComplete(context, irp->CallbackData, ioStatus);
	/* Setup the IRP. */
	irp->CompletionId = priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_rename_file_callback3;
	irp->DeviceId = deviceId;

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_Print(priv->log, WLOG_ERROR, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to close the file */
	return rdpdr_server_send_device_close_request(context, irp->DeviceId, irp->FileId,
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(irp);

	RdpdrServerPrivate* priv = context->priv;
	WLog_Print(priv->log, WLOG_DEBUG,
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

	if (!Stream_CheckAndLogRequiredLengthWLog(priv->log, s, 5))
		return ERROR_INVALID_DATA;

	const uint32_t fileId = Stream_Get_UINT32(s);    /* FileId (4 bytes) */
	const uint8_t information = Stream_Get_UINT8(s); /* Information (1 byte) */
	WLog_Print(priv->log, WLOG_DEBUG, "fileId [0x%08" PRIx32 "], information %s", fileId,
	           fileInformation2str(information));

	/* Setup the IRP. */
	irp->CompletionId = priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_rename_file_callback2;
	irp->DeviceId = deviceId;
	irp->FileId = fileId;

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_Print(priv->log, WLOG_ERROR, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to rename the file */
	return rdpdr_server_send_device_file_rename_request(context, irp->DeviceId, irp->FileId,
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RDPDR_IRP* irp = nullptr;
	UINT ret = prepare_irp(context, deviceId, rdpdr_server_drive_rename_file_callback1,
	                       callbackData, &irp);
	if (ret != CHANNEL_RC_OK)
		return ret;

	strncpy(irp->PathName, oldPath, sizeof(irp->PathName) - 1);
	strncpy(irp->ExtraBuffer, newPath, sizeof(irp->ExtraBuffer) - 1);
	rdpdr_server_convert_slashes(irp->PathName, sizeof(irp->PathName));
	rdpdr_server_convert_slashes(irp->ExtraBuffer, sizeof(irp->ExtraBuffer));

	/* Send a request to open the file. */
	return rdpdr_server_send_device_create_request(context, irp->DeviceId, irp->CompletionId,
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
	priv->haveSmartcardDevice = FALSE;
	priv->smartcardDeviceId = 0;
	priv->IrpList = ListDictionary_New(TRUE);

	if (!priv->IrpList)
		goto fail;

	ListDictionary_ValueObject(priv->IrpList)->fnObjectFree = (OBJECT_FREE_FN)rdpdr_server_irp_free;

	priv->devicelist = HashTable_New(FALSE);
	if (!priv->devicelist)
		goto fail;

	if (!HashTable_SetHashFunction(priv->devicelist, rdpdr_deviceid_hash))
		goto fail;

	{
		wObject* obj = HashTable_ValueObject(priv->devicelist);
		WINPR_ASSERT(obj);
		obj->fnObjectFree = rdpdr_device_free_h;
		obj->fnObjectNew = rdpdr_device_clone;
	}

	{
		wObject* obj = HashTable_KeyObject(priv->devicelist);
		obj->fnObjectEquals = rdpdr_device_equal;
		obj->fnObjectFree = rdpdr_device_key_free;
		obj->fnObjectNew = rdpdr_device_key_clone;
	}

	return priv;
fail:
	rdpdr_server_private_free(priv);
	return nullptr;
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
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	rdpdr_server_context_free(context);
	WINPR_PRAGMA_DIAG_POP
	return nullptr;
}

void rdpdr_server_context_free(RdpdrServerContext* context)
{
	if (!context)
		return;

	rdpdr_server_private_free(context->priv);
	free(context);
}
