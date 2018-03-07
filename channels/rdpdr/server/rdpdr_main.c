/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Device Redirection Virtual Channel Extension
 *
 * Copyright 2014 Dell Software <Mike.McDonald@software.dell.com>
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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
#include <winpr/nt.h>
#include <winpr/print.h>
#include <winpr/stream.h>

#include <freerdp/channels/log.h>
#include "rdpdr_main.h"

#define TAG "rdpdr.server"

static UINT32 g_ClientId = 0;

static RDPDR_IRP* rdpdr_server_irp_new()
{
	RDPDR_IRP* irp;
	irp = (RDPDR_IRP*) calloc(1, sizeof(RDPDR_IRP));
	return irp;
}

static void rdpdr_server_irp_free(RDPDR_IRP* irp)
{
	free(irp);
}

static BOOL rdpdr_server_enqueue_irp(RdpdrServerContext* context,
                                     RDPDR_IRP* irp)
{
	return ListDictionary_Add(context->priv->IrpList,
	                          (void*)(size_t) irp->CompletionId, irp);
}

static RDPDR_IRP* rdpdr_server_dequeue_irp(RdpdrServerContext* context,
        UINT32 completionId)
{
	RDPDR_IRP* irp;
	irp = (RDPDR_IRP*) ListDictionary_Remove(context->priv->IrpList,
	        (void*)(size_t) completionId);
	return irp;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_send_announce_request(RdpdrServerContext* context)
{
	wStream* s;
	BOOL status;
	RDPDR_HEADER header;
	ULONG written;
	WLog_DBG(TAG, "RdpdrServerSendAnnounceRequest");
	header.Component = RDPDR_CTYP_CORE;
	header.PacketId = PAKID_CORE_SERVER_ANNOUNCE;
	s = Stream_New(NULL, RDPDR_HEADER_LENGTH + 8);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, header.Component); /* Component (2 bytes) */
	Stream_Write_UINT16(s, header.PacketId); /* PacketId (2 bytes) */
	Stream_Write_UINT16(s,
	                    context->priv->VersionMajor); /* VersionMajor (2 bytes) */
	Stream_Write_UINT16(s,
	                    context->priv->VersionMinor); /* VersionMinor (2 bytes) */
	Stream_Write_UINT32(s, context->priv->ClientId); /* ClientId (4 bytes) */
	Stream_SealLength(s);
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Buffer(s), Stream_Length(s));
	status = WTSVirtualChannelWrite(context->priv->ChannelHandle,
	                                (PCHAR) Stream_Buffer(s), Stream_Length(s), &written);
	Stream_Free(s, TRUE);
	return status ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_receive_announce_response(RdpdrServerContext* context,
        wStream* s, RDPDR_HEADER* header)
{
	UINT32 ClientId;
	UINT16 VersionMajor;
	UINT16 VersionMinor;

	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, VersionMajor); /* VersionMajor (2 bytes) */
	Stream_Read_UINT16(s, VersionMinor); /* VersionMinor (2 bytes) */
	Stream_Read_UINT32(s, ClientId); /* ClientId (4 bytes) */
	WLog_DBG(TAG,
	         "Client Announce Response: VersionMajor: 0x%08"PRIX16" VersionMinor: 0x%04"PRIX16" ClientId: 0x%08"PRIX32"",
	         VersionMajor, VersionMinor, ClientId);
	context->priv->ClientId = ClientId;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_receive_client_name_request(RdpdrServerContext*
        context, wStream* s, RDPDR_HEADER* header)
{
	UINT32 UnicodeFlag;
	UINT32 ComputerNameLen;

	if (Stream_GetRemainingLength(s) < 12)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, UnicodeFlag); /* UnicodeFlag (4 bytes) */
	Stream_Seek_UINT32(s); /* CodePage (4 bytes), MUST be set to zero */
	Stream_Read_UINT32(s, ComputerNameLen); /* ComputerNameLen (4 bytes) */
	/* UnicodeFlag is either 0 or 1, the other 31 bits must be ignored.
	 */
	UnicodeFlag = UnicodeFlag & 0x00000001;

	/**
	 * Caution: ComputerNameLen is given *bytes*,
	 * not in characters, including the NULL terminator!
	 */

	if (UnicodeFlag)
	{
		if ((ComputerNameLen % 2) || ComputerNameLen > 512 || ComputerNameLen < 2)
		{
			WLog_ERR(TAG, "invalid unicode computer name length: %"PRIu32"", ComputerNameLen);
			return ERROR_INVALID_DATA;
		}
	}
	else
	{
		if (ComputerNameLen > 256 || ComputerNameLen < 1)
		{
			WLog_ERR(TAG, "invalid ascii computer name length: %"PRIu32"", ComputerNameLen);
			return ERROR_INVALID_DATA;
		}
	}

	if (Stream_GetRemainingLength(s) < ComputerNameLen)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	/* ComputerName must be null terminated, check if it really is */

	if (Stream_Pointer(s)[ComputerNameLen - 1] ||
	    (UnicodeFlag && Stream_Pointer(s)[ComputerNameLen - 2]))
	{
		WLog_ERR(TAG, "computer name must be null terminated");
		return ERROR_INVALID_DATA;
	}

	if (context->priv->ClientComputerName)
	{
		free(context->priv->ClientComputerName);
		context->priv->ClientComputerName = NULL;
	}

	if (UnicodeFlag)
	{
		if (ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) Stream_Pointer(s), -1,
		                       &(context->priv->ClientComputerName), 0, NULL, NULL) < 1)
		{
			WLog_ERR(TAG, "failed to convert client computer name");
			return ERROR_INVALID_DATA;
		}
	}
	else
	{
		context->priv->ClientComputerName = _strdup((char*) Stream_Pointer(s));

		if (!context->priv->ClientComputerName)
		{
			WLog_ERR(TAG, "failed to duplicate client computer name");
			return CHANNEL_RC_NO_MEMORY;
		}
	}

	Stream_Seek(s, ComputerNameLen);
	WLog_DBG(TAG, "ClientComputerName: %s", context->priv->ClientComputerName);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_read_capability_set_header(wStream* s,
        RDPDR_CAPABILITY_HEADER* header)
{
	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, header->CapabilityType); /* CapabilityType (2 bytes) */
	Stream_Read_UINT16(s,
	                   header->CapabilityLength); /* CapabilityLength (2 bytes) */
	Stream_Read_UINT32(s, header->Version); /* Version (4 bytes) */
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_write_capability_set_header(wStream* s,
        RDPDR_CAPABILITY_HEADER* header)
{
	if (!Stream_EnsureRemainingCapacity(s, 8))
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	Stream_Write_UINT16(s, header->CapabilityType); /* CapabilityType (2 bytes) */
	Stream_Write_UINT16(s,
	                    header->CapabilityLength); /* CapabilityLength (2 bytes) */
	Stream_Write_UINT32(s, header->Version); /* Version (4 bytes) */
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_read_general_capability_set(RdpdrServerContext*
        context, wStream* s, RDPDR_CAPABILITY_HEADER* header)
{
	UINT32 ioCode1;
	UINT32 extraFlags1;
	UINT32 extendedPdu;
	UINT16 VersionMajor;
	UINT16 VersionMinor;
	UINT32 SpecialTypeDeviceCap;

	if (Stream_GetRemainingLength(s) < 32)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	Stream_Seek_UINT32(s); /* osType (4 bytes), ignored on receipt */
	Stream_Seek_UINT32(s); /* osVersion (4 bytes), unused and must be set to zero */
	Stream_Read_UINT16(s, VersionMajor); /* protocolMajorVersion (2 bytes) */
	Stream_Read_UINT16(s, VersionMinor); /* protocolMinorVersion (2 bytes) */
	Stream_Read_UINT32(s, ioCode1); /* ioCode1 (4 bytes) */
	Stream_Seek_UINT32(
	    s); /* ioCode2 (4 bytes), must be set to zero, reserved for future use */
	Stream_Read_UINT32(s, extendedPdu); /* extendedPdu (4 bytes) */
	Stream_Read_UINT32(s, extraFlags1); /* extraFlags1 (4 bytes) */
	Stream_Seek_UINT32(
	    s); /* extraFlags2 (4 bytes), must be set to zero, reserved for future use */

	if (header->Version == GENERAL_CAPABILITY_VERSION_02)
	{
		if (Stream_GetRemainingLength(s) < 4)
		{
			WLog_ERR(TAG, "not enough data in stream!");
			return ERROR_INVALID_DATA;
		}

		Stream_Read_UINT32(s,
		                   SpecialTypeDeviceCap); /* SpecialTypeDeviceCap (4 bytes) */
	}

	context->priv->UserLoggedOnPdu = (extendedPdu & RDPDR_USER_LOGGEDON_PDU) ?
	                                 TRUE : FALSE;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_write_general_capability_set(RdpdrServerContext*
        context, wStream* s)
{
	UINT32 ioCode1;
	UINT32 extendedPdu;
	UINT32 extraFlags1;
	UINT32 SpecialTypeDeviceCap;
	RDPDR_CAPABILITY_HEADER header;
	header.CapabilityType = CAP_GENERAL_TYPE;
	header.CapabilityLength = RDPDR_CAPABILITY_HEADER_LENGTH + 36;
	header.Version = GENERAL_CAPABILITY_VERSION_02;
	ioCode1 = 0;
	ioCode1 |= RDPDR_IRP_MJ_CREATE; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_CLEANUP; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_CLOSE; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_READ; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_WRITE; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_FLUSH_BUFFERS; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_SHUTDOWN; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_DEVICE_CONTROL; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_QUERY_VOLUME_INFORMATION; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_SET_VOLUME_INFORMATION; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_QUERY_INFORMATION; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_SET_INFORMATION; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_DIRECTORY_CONTROL; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_LOCK_CONTROL; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_QUERY_SECURITY; /* optional */
	ioCode1 |= RDPDR_IRP_MJ_SET_SECURITY; /* optional */
	extendedPdu = 0;
	extendedPdu |= RDPDR_CLIENT_DISPLAY_NAME_PDU; /* always set */
	extendedPdu |= RDPDR_DEVICE_REMOVE_PDUS; /* optional */

	if (context->priv->UserLoggedOnPdu)
		extendedPdu |= RDPDR_USER_LOGGEDON_PDU; /* optional */

	extraFlags1 = 0;
	extraFlags1 |= ENABLE_ASYNCIO; /* optional */
	SpecialTypeDeviceCap = 0;

	if (!Stream_EnsureRemainingCapacity(s, header.CapabilityLength))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rdpdr_server_write_capability_set_header(s, &header);
	Stream_Write_UINT32(s, 0); /* osType (4 bytes), ignored on receipt */
	Stream_Write_UINT32(s,
	                    0); /* osVersion (4 bytes), unused and must be set to zero */
	Stream_Write_UINT16(s,
	                    context->priv->VersionMajor); /* protocolMajorVersion (2 bytes) */
	Stream_Write_UINT16(s,
	                    context->priv->VersionMinor); /* protocolMinorVersion (2 bytes) */
	Stream_Write_UINT32(s, ioCode1); /* ioCode1 (4 bytes) */
	Stream_Write_UINT32(s,
	                    0); /* ioCode2 (4 bytes), must be set to zero, reserved for future use */
	Stream_Write_UINT32(s, extendedPdu); /* extendedPdu (4 bytes) */
	Stream_Write_UINT32(s, extraFlags1); /* extraFlags1 (4 bytes) */
	Stream_Write_UINT32(s,
	                    0); /* extraFlags2 (4 bytes), must be set to zero, reserved for future use */
	Stream_Write_UINT32(s,
	                    SpecialTypeDeviceCap); /* SpecialTypeDeviceCap (4 bytes) */
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_read_printer_capability_set(RdpdrServerContext*
        context, wStream* s, RDPDR_CAPABILITY_HEADER* header)
{
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_write_printer_capability_set(RdpdrServerContext*
        context, wStream* s)
{
	RDPDR_CAPABILITY_HEADER header;
	header.CapabilityType = CAP_PRINTER_TYPE;
	header.CapabilityLength = RDPDR_CAPABILITY_HEADER_LENGTH;
	header.Version = PRINT_CAPABILITY_VERSION_01;

	if (!Stream_EnsureRemainingCapacity(s, header.CapabilityLength))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	return rdpdr_server_write_capability_set_header(s, &header);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_read_port_capability_set(RdpdrServerContext* context,
        wStream* s, RDPDR_CAPABILITY_HEADER* header)
{
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_write_port_capability_set(RdpdrServerContext* context,
        wStream* s)
{
	RDPDR_CAPABILITY_HEADER header;
	header.CapabilityType = CAP_PORT_TYPE;
	header.CapabilityLength = RDPDR_CAPABILITY_HEADER_LENGTH;
	header.Version = PORT_CAPABILITY_VERSION_01;

	if (!Stream_EnsureRemainingCapacity(s, header.CapabilityLength))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	return rdpdr_server_write_capability_set_header(s, &header);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_read_drive_capability_set(RdpdrServerContext* context,
        wStream* s, RDPDR_CAPABILITY_HEADER* header)
{
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_write_drive_capability_set(RdpdrServerContext* context,
        wStream* s)
{
	RDPDR_CAPABILITY_HEADER header;
	header.CapabilityType = CAP_DRIVE_TYPE;
	header.CapabilityLength = RDPDR_CAPABILITY_HEADER_LENGTH;
	header.Version = DRIVE_CAPABILITY_VERSION_02;

	if (!Stream_EnsureRemainingCapacity(s, header.CapabilityLength))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	return rdpdr_server_write_capability_set_header(s, &header);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_read_smartcard_capability_set(RdpdrServerContext*
        context, wStream* s, RDPDR_CAPABILITY_HEADER* header)
{
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_write_smartcard_capability_set(
    RdpdrServerContext* context, wStream* s)
{
	RDPDR_CAPABILITY_HEADER header;
	header.CapabilityType = CAP_SMARTCARD_TYPE;
	header.CapabilityLength = RDPDR_CAPABILITY_HEADER_LENGTH;
	header.Version = SMARTCARD_CAPABILITY_VERSION_01;

	if (!Stream_EnsureRemainingCapacity(s, header.CapabilityLength))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return CHANNEL_RC_OK;
	}

	return rdpdr_server_write_capability_set_header(s, &header);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_send_core_capability_request(RdpdrServerContext*
        context)
{
	wStream* s;
	BOOL status;
	RDPDR_HEADER header;
	UINT16 numCapabilities;
	ULONG written;
	UINT error;
	WLog_DBG(TAG, "RdpdrServerSendCoreCapabilityRequest");
	header.Component = RDPDR_CTYP_CORE;
	header.PacketId = PAKID_CORE_SERVER_CAPABILITY;
	numCapabilities = 1;

	if (context->supportsDrives)
		numCapabilities++;

	if (context->supportsPorts)
		numCapabilities++;

	if (context->supportsPrinters)
		numCapabilities++;

	if (context->supportsSmartcards)
		numCapabilities++;

	s = Stream_New(NULL, RDPDR_HEADER_LENGTH + 512);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, header.Component); /* Component (2 bytes) */
	Stream_Write_UINT16(s, header.PacketId); /* PacketId (2 bytes) */
	Stream_Write_UINT16(s, numCapabilities); /* numCapabilities (2 bytes) */
	Stream_Write_UINT16(s, 0); /* Padding (2 bytes) */

	if ((error = rdpdr_server_write_general_capability_set(context, s)))
	{
		WLog_ERR(TAG,
		         "rdpdr_server_write_general_capability_set failed with error %"PRIu32"!", error);
		return error;
	}

	if (context->supportsDrives)
	{
		if ((error = rdpdr_server_write_drive_capability_set(context, s)))
		{
			WLog_ERR(TAG, "rdpdr_server_write_drive_capability_set failed with error %"PRIu32"!",
			         error);
			return error;
		}
	}

	if (context->supportsPorts)
	{
		if ((error = rdpdr_server_write_port_capability_set(context, s)))
		{
			WLog_ERR(TAG, "rdpdr_server_write_port_capability_set failed with error %"PRIu32"!",
			         error);
			return error;
		}
	}

	if (context->supportsPrinters)
	{
		if ((error = rdpdr_server_write_printer_capability_set(context, s)))
		{
			WLog_ERR(TAG,
			         "rdpdr_server_write_printer_capability_set failed with error %"PRIu32"!", error);
			return error;
		}
	}

	if (context->supportsSmartcards)
	{
		if ((error = rdpdr_server_write_smartcard_capability_set(context, s)))
		{
			WLog_ERR(TAG,
			         "rdpdr_server_write_printer_capability_set failed with error %"PRIu32"!", error);
			return error;
		}
	}

	Stream_SealLength(s);
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Buffer(s), Stream_Length(s));
	status = WTSVirtualChannelWrite(context->priv->ChannelHandle,
	                                (PCHAR) Stream_Buffer(s), Stream_Length(s), &written);
	Stream_Free(s, TRUE);
	return status ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_receive_core_capability_response(
    RdpdrServerContext* context, wStream* s, RDPDR_HEADER* header)
{
	int i;
	UINT status;
	UINT16 numCapabilities;
	RDPDR_CAPABILITY_HEADER capabilityHeader;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, numCapabilities); /* numCapabilities (2 bytes) */
	Stream_Seek_UINT16(s); /* Padding (2 bytes) */

	for (i = 0; i < numCapabilities; i++)
	{
		if ((status = rdpdr_server_read_capability_set_header(s, &capabilityHeader)))
		{
			WLog_ERR(TAG, "rdpdr_server_read_capability_set_header failed with error %"PRIu32"!",
			         status);
			return status;
		}

		switch (capabilityHeader.CapabilityType)
		{
			case CAP_GENERAL_TYPE:
				if ((status = rdpdr_server_read_general_capability_set(context, s,
				              &capabilityHeader)))
				{
					WLog_ERR(TAG, "rdpdr_server_read_general_capability_set failed with error %"PRIu32"!",
					         status);
					return status;
				}

				break;

			case CAP_PRINTER_TYPE:
				if ((status = rdpdr_server_read_printer_capability_set(context, s,
				              &capabilityHeader)))
				{
					WLog_ERR(TAG, "rdpdr_server_read_printer_capability_set failed with error %"PRIu32"!",
					         status);
					return status;
				}

				break;

			case CAP_PORT_TYPE:
				if ((status = rdpdr_server_read_port_capability_set(context, s,
				              &capabilityHeader)))
				{
					WLog_ERR(TAG, "rdpdr_server_read_port_capability_set failed with error %"PRIu32"!",
					         status);
					return status;
				}

				break;

			case CAP_DRIVE_TYPE:
				if ((status = rdpdr_server_read_drive_capability_set(context, s,
				              &capabilityHeader)))
				{
					WLog_ERR(TAG, "rdpdr_server_read_drive_capability_set failed with error %"PRIu32"!",
					         status);
					return status;
				}

				break;

			case CAP_SMARTCARD_TYPE:
				if ((status = rdpdr_server_read_smartcard_capability_set(context, s,
				              &capabilityHeader)))
				{
					WLog_ERR(TAG,
					         "rdpdr_server_read_smartcard_capability_set failed with error %"PRIu32"!", status);
					return status;
				}

				break;

			default:
				WLog_DBG(TAG, "Unknown capabilityType %"PRIu16"", capabilityHeader.CapabilityType);
				Stream_Seek(s, capabilityHeader.CapabilityLength -
				            RDPDR_CAPABILITY_HEADER_LENGTH);
				return ERROR_INVALID_DATA;
				break;
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
	BOOL status;
	RDPDR_HEADER header;
	ULONG written;
	WLog_DBG(TAG, "RdpdrServerSendClientIdConfirm");
	header.Component = RDPDR_CTYP_CORE;
	header.PacketId = PAKID_CORE_CLIENTID_CONFIRM;
	s = Stream_New(NULL, RDPDR_HEADER_LENGTH + 8);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, header.Component); /* Component (2 bytes) */
	Stream_Write_UINT16(s, header.PacketId); /* PacketId (2 bytes) */
	Stream_Write_UINT16(s,
	                    context->priv->VersionMajor); /* VersionMajor (2 bytes) */
	Stream_Write_UINT16(s,
	                    context->priv->VersionMinor); /* VersionMinor (2 bytes) */
	Stream_Write_UINT32(s, context->priv->ClientId); /* ClientId (4 bytes) */
	Stream_SealLength(s);
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Buffer(s), Stream_Length(s));
	status = WTSVirtualChannelWrite(context->priv->ChannelHandle,
	                                (PCHAR) Stream_Buffer(s), Stream_Length(s), &written);
	Stream_Free(s, TRUE);
	return status ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_receive_device_list_announce_request(
    RdpdrServerContext* context, wStream* s, RDPDR_HEADER* header)
{
	int i;
	UINT32 DeviceCount;
	UINT32 DeviceType;
	UINT32 DeviceId;
	char PreferredDosName[9];
	UINT32 DeviceDataLength;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, DeviceCount); /* DeviceCount (4 bytes) */
	WLog_DBG(TAG, "DeviceCount: %"PRIu32"", DeviceCount);

	for (i = 0; i < DeviceCount; i++)
	{
		ZeroMemory(PreferredDosName, sizeof(PreferredDosName));

		if (Stream_GetRemainingLength(s) < 20)
		{
			WLog_ERR(TAG, "not enough data in stream!");
			return ERROR_INVALID_DATA;
		}

		Stream_Read_UINT32(s, DeviceType); /* DeviceType (4 bytes) */
		Stream_Read_UINT32(s, DeviceId); /* DeviceId (4 bytes) */
		Stream_Read(s, PreferredDosName, 8); /* PreferredDosName (8 bytes) */
		Stream_Read_UINT32(s, DeviceDataLength); /* DeviceDataLength (4 bytes) */

		if (Stream_GetRemainingLength(s) < DeviceDataLength)
		{
			WLog_ERR(TAG, "not enough data in stream!");
			return ERROR_INVALID_DATA;
		}

		WLog_DBG(TAG, "Device %d Name: %s Id: 0x%08"PRIX32" DataLength: %"PRIu32"",
		         i, PreferredDosName, DeviceId, DeviceDataLength);

		switch (DeviceType)
		{
			case RDPDR_DTYP_FILESYSTEM:
				if (context->supportsDrives)
				{
					IFCALL(context->OnDriveCreate, context, DeviceId, PreferredDosName);
				}

				break;

			case RDPDR_DTYP_PRINT:
				if (context->supportsPrinters)
				{
					IFCALL(context->OnPrinterCreate, context, DeviceId, PreferredDosName);
				}

				break;

			case RDPDR_DTYP_SERIAL:
			case RDPDR_DTYP_PARALLEL:
				if (context->supportsPorts)
				{
					IFCALL(context->OnPortCreate, context, DeviceId, PreferredDosName);
				}

				break;

			case RDPDR_DTYP_SMARTCARD:
				if (context->supportsSmartcards)
				{
					IFCALL(context->OnSmartcardCreate, context, DeviceId, PreferredDosName);
				}

				break;

			default:
				break;
		}

		Stream_Seek(s, DeviceDataLength);
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_receive_device_list_remove_request(
    RdpdrServerContext* context, wStream* s, RDPDR_HEADER* header)
{
	int i;
	UINT32 DeviceCount;
	UINT32 DeviceType;
	UINT32 DeviceId;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, DeviceCount); /* DeviceCount (4 bytes) */
	WLog_DBG(TAG, "DeviceCount: %"PRIu32"", DeviceCount);

	for (i = 0; i < DeviceCount; i++)
	{
		if (Stream_GetRemainingLength(s) < 4)
		{
			WLog_ERR(TAG, "not enough data in stream!");
			return ERROR_INVALID_DATA;
		}

		Stream_Read_UINT32(s, DeviceId); /* DeviceId (4 bytes) */
		WLog_DBG(TAG, "Device %d Id: 0x%08"PRIX32"", i, DeviceId);
		DeviceType = 0; /* TODO: Save the device type on the announce request. */

		switch (DeviceType)
		{
			case RDPDR_DTYP_FILESYSTEM:
				if (context->supportsDrives)
				{
					IFCALL(context->OnDriveDelete, context, DeviceId);
				}

				break;

			case RDPDR_DTYP_PRINT:
				if (context->supportsPrinters)
				{
					IFCALL(context->OnPrinterDelete, context, DeviceId);
				}

				break;

			case RDPDR_DTYP_SERIAL:
			case RDPDR_DTYP_PARALLEL:
				if (context->supportsPorts)
				{
					IFCALL(context->OnPortDelete, context, DeviceId);
				}

				break;

			case RDPDR_DTYP_SMARTCARD:
				if (context->supportsSmartcards)
				{
					IFCALL(context->OnSmartcardDelete, context, DeviceId);
				}

				break;

			default:
				break;
		}
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_receive_device_io_completion(RdpdrServerContext*
        context, wStream* s, RDPDR_HEADER* header)
{
	UINT32 deviceId;
	UINT32 completionId;
	UINT32 ioStatus;
	RDPDR_IRP* irp;
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < 12)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, deviceId);
	Stream_Read_UINT32(s, completionId);
	Stream_Read_UINT32(s, ioStatus);
	WLog_DBG(TAG, "deviceId=%"PRIu32", completionId=0x%"PRIx32", ioStatus=0x%"PRIx32"", deviceId,
	         completionId, ioStatus);
	irp = rdpdr_server_dequeue_irp(context, completionId);

	if (!irp)
	{
		WLog_ERR(TAG, "IRP not found for completionId=0x%"PRIx32"", completionId);
		return ERROR_INTERNAL_ERROR;
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
	BOOL status;
	RDPDR_HEADER header;
	ULONG written;
	WLog_DBG(TAG, "RdpdrServerSendUserLoggedOn");
	header.Component = RDPDR_CTYP_CORE;
	header.PacketId = PAKID_CORE_USER_LOGGEDON;
	s = Stream_New(NULL, RDPDR_HEADER_LENGTH);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, header.Component); /* Component (2 bytes) */
	Stream_Write_UINT16(s, header.PacketId); /* PacketId (2 bytes) */
	Stream_SealLength(s);
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Buffer(s), Stream_Length(s));
	status = WTSVirtualChannelWrite(context->priv->ChannelHandle,
	                                (PCHAR) Stream_Buffer(s), Stream_Length(s), &written);
	Stream_Free(s, TRUE);
	return status ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_receive_pdu(RdpdrServerContext* context, wStream* s,
                                     RDPDR_HEADER* header)
{
	UINT error = CHANNEL_RC_OK;
	WLog_DBG(TAG, "RdpdrServerReceivePdu: Component: 0x%04"PRIX16" PacketId: 0x%04"PRIX16"",
	         header->Component, header->PacketId);
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Buffer(s), Stream_Length(s));

	if (header->Component == RDPDR_CTYP_CORE)
	{
		switch (header->PacketId)
		{
			case PAKID_CORE_CLIENTID_CONFIRM:
				if ((error = rdpdr_server_receive_announce_response(context, s, header)))
				{
					WLog_ERR(TAG, "rdpdr_server_receive_announce_response failed with error %"PRIu32"!",
					         error);
					return error;
				}

				break;

			case PAKID_CORE_CLIENT_NAME:
				if ((error = rdpdr_server_receive_client_name_request(context, s, header)))
				{
					WLog_ERR(TAG, "rdpdr_server_receive_client_name_request failed with error %"PRIu32"!",
					         error);
					return error;
				}

				if ((error = rdpdr_server_send_core_capability_request(context)))
				{
					WLog_ERR(TAG,
					         "rdpdr_server_send_core_capability_request failed with error %"PRIu32"!", error);
					return error;
				}

				if ((error = rdpdr_server_send_client_id_confirm(context)))
				{
					WLog_ERR(TAG, "rdpdr_server_send_client_id_confirm failed with error %"PRIu32"!",
					         error);
					return error;
				}

				break;

			case PAKID_CORE_CLIENT_CAPABILITY:
				if ((error = rdpdr_server_receive_core_capability_response(context, s, header)))
				{
					WLog_ERR(TAG,
					         "rdpdr_server_receive_core_capability_response failed with error %"PRIu32"!", error);
					return error;
				}

				if (context->priv->UserLoggedOnPdu)
					if ((error = rdpdr_server_send_user_logged_on(context)))
					{
						WLog_ERR(TAG, "rdpdr_server_send_user_logged_on failed with error %"PRIu32"!", error);
						return error;
					}

				break;

			case PAKID_CORE_DEVICELIST_ANNOUNCE:
				if ((error = rdpdr_server_receive_device_list_announce_request(context, s,
				             header)))
				{
					WLog_ERR(TAG,
					         "rdpdr_server_receive_device_list_announce_request failed with error %"PRIu32"!",
					         error);
					return error;
				}

				break;

			case PAKID_CORE_DEVICE_REPLY:
				break;

			case PAKID_CORE_DEVICE_IOREQUEST:
				break;

			case PAKID_CORE_DEVICE_IOCOMPLETION:
				if ((error = rdpdr_server_receive_device_io_completion(context, s, header)))
				{
					WLog_ERR(TAG,
					         "rdpdr_server_receive_device_io_completion failed with error %"PRIu32"!", error);
					return error;
				}

				break;

			case PAKID_CORE_DEVICELIST_REMOVE:
				if ((error = rdpdr_server_receive_device_list_remove_request(context, s,
				             header)))
				{
					WLog_ERR(TAG,
					         "rdpdr_server_receive_device_io_completion failed with error %"PRIu32"!", error);
					return error;
				}

				break;

			default:
				break;
		}
	}
	else if (header->Component == RDPDR_CTYP_PRN)
	{
		switch (header->PacketId)
		{
			case PAKID_PRN_CACHE_DATA:
				break;

			case PAKID_PRN_USING_XPS:
				break;

			default:
				break;
		}
	}
	else
	{
		WLog_WARN(TAG, "Unknown RDPDR_HEADER.Component: 0x%04"PRIX16"", header->Component);
		return ERROR_INVALID_DATA;
	}

	return error;
}

static DWORD WINAPI rdpdr_server_thread(LPVOID arg)
{
	wStream* s;
	DWORD status;
	DWORD nCount;
	void* buffer;
	HANDLE events[8];
	RDPDR_HEADER header;
	HANDLE ChannelEvent;
	DWORD BytesReturned;
	RdpdrServerContext* context;
	UINT error;
	context = (RdpdrServerContext*) arg;
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

	if (WTSVirtualChannelQuery(context->priv->ChannelHandle, WTSVirtualEventHandle,
	                           &buffer, &BytesReturned) == TRUE)
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
		WLog_ERR(TAG, "rdpdr_server_send_announce_request failed with error %"PRIu32"!",
		         error);
		goto out_stream;
	}

	while (1)
	{
		BytesReturned = 0;
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %"PRIu32"!", error);
			goto out_stream;
		}

		status = WaitForSingleObject(context->priv->StopEvent, 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", error);
			goto out_stream;
		}

		if (status == WAIT_OBJECT_0)
			break;

		if (!WTSVirtualChannelRead(context->priv->ChannelHandle, 0,
		                           (PCHAR) Stream_Buffer(s), Stream_Capacity(s), &BytesReturned))
		{
			WLog_ERR(TAG, "WTSVirtualChannelRead failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		if (BytesReturned >= RDPDR_HEADER_LENGTH)
		{
			Stream_SetPosition(s, 0);
			Stream_SetLength(s, BytesReturned);

			while (Stream_GetRemainingLength(s) >= RDPDR_HEADER_LENGTH)
			{
				Stream_Read_UINT16(s, header.Component); /* Component (2 bytes) */
				Stream_Read_UINT16(s, header.PacketId); /* PacketId (2 bytes) */

				if ((error = rdpdr_server_receive_pdu(context, s, &header)))
				{
					WLog_ERR(TAG, "rdpdr_server_receive_pdu failed with error %"PRIu32"!", error);
					goto out_stream;
				}
			}
		}
	}

out_stream:
	Stream_Free(s, TRUE);
out:

	if (error && context->rdpcontext)
		setChannelError(context->rdpcontext, error,
		                "rdpdr_server_thread reported an error");

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
	context->priv->ChannelHandle = WTSVirtualChannelOpen(context->vcm,
	                               WTS_CURRENT_SESSION, "rdpdr");

	if (!context->priv->ChannelHandle)
	{
		WLog_ERR(TAG, "WTSVirtualChannelOpen failed!");
		return CHANNEL_RC_BAD_CHANNEL;
	}

	if (!(context->priv->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		WLog_ERR(TAG, "CreateEvent failed!");
		return ERROR_INTERNAL_ERROR;
	}

	if (!(context->priv->Thread = CreateThread(NULL, 0,
								  rdpdr_server_thread, (void*) context, 0, NULL)))
	{
		WLog_ERR(TAG, "CreateThread failed!");
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

	if (context->priv->StopEvent)
	{
		SetEvent(context->priv->StopEvent);

		if (WaitForSingleObject(context->priv->Thread, INFINITE) == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", error);
			return error;
		}

		CloseHandle(context->priv->Thread);
		context->priv->Thread = NULL;
		CloseHandle(context->priv->StopEvent);
		context->priv->StopEvent = NULL;
	}

	return CHANNEL_RC_OK;
}

static void rdpdr_server_write_device_iorequest(
    wStream* s,
    UINT32 deviceId,
    UINT32 fileId,
    UINT32 completionId,
    UINT32 majorFunction,
    UINT32 minorFunction)
{
	Stream_Write_UINT16(s, RDPDR_CTYP_CORE); /* Component (2 bytes) */
	Stream_Write_UINT16(s, PAKID_CORE_DEVICE_IOREQUEST); /* PacketId (2 bytes) */
	Stream_Write_UINT32(s, deviceId); /* DeviceId (4 bytes) */
	Stream_Write_UINT32(s, fileId); /* FileId (4 bytes) */
	Stream_Write_UINT32(s, completionId); /* CompletionId (4 bytes) */
	Stream_Write_UINT32(s, majorFunction); /* MajorFunction (4 bytes) */
	Stream_Write_UINT32(s, minorFunction); /* MinorFunction (4 bytes) */
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_read_file_directory_information(wStream* s,
        FILE_DIRECTORY_INFORMATION* fdi)
{
	UINT32 fileNameLength;
	ZeroMemory(fdi, sizeof(FILE_DIRECTORY_INFORMATION));

	if (Stream_GetRemainingLength(s) < 64)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, fdi->NextEntryOffset); /* NextEntryOffset (4 bytes) */
	Stream_Read_UINT32(s, fdi->FileIndex); /* FileIndex (4 bytes) */
	Stream_Read_UINT64(s, fdi->CreationTime); /* CreationTime (8 bytes) */
	Stream_Read_UINT64(s, fdi->LastAccessTime); /* LastAccessTime (8 bytes) */
	Stream_Read_UINT64(s, fdi->LastWriteTime); /* LastWriteTime (8 bytes) */
	Stream_Read_UINT64(s, fdi->ChangeTime); /* ChangeTime (8 bytes) */
	Stream_Read_UINT64(s, fdi->EndOfFile); /* EndOfFile (8 bytes) */
	Stream_Read_UINT64(s, fdi->AllocationSize); /* AllocationSize (8 bytes) */
	Stream_Read_UINT32(s, fdi->FileAttributes); /* FileAttributes (4 bytes) */
	Stream_Read_UINT32(s, fileNameLength); /* FileNameLength (4 bytes) */

	if (Stream_GetRemainingLength(s) < fileNameLength)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	WideCharToMultiByte(CP_ACP, 0, (LPCWSTR) Stream_Pointer(s), fileNameLength / 2,
	                    fdi->FileName, sizeof(fdi->FileName), NULL, NULL);
	Stream_Seek(s, fileNameLength);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_send_device_create_request(
    RdpdrServerContext* context,
    UINT32 deviceId,
    UINT32 completionId,
    const char* path,
    UINT32 desiredAccess,
    UINT32 createOptions,
    UINT32 createDisposition)
{
	UINT32 pathLength;
	ULONG written;
	BOOL status;
	wStream* s;
	WLog_DBG(TAG,
	         "RdpdrServerSendDeviceCreateRequest: deviceId=%"PRIu32", path=%s, desiredAccess=0x%"PRIx32" createOptions=0x%"PRIx32" createDisposition=0x%"PRIx32"",
	         deviceId, path, desiredAccess, createOptions, createDisposition);
	/* Compute the required Unicode size. */
	pathLength = (strlen(path) + 1) * sizeof(WCHAR);
	s = Stream_New(NULL, 256 + pathLength);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rdpdr_server_write_device_iorequest(s, deviceId, 0, completionId, IRP_MJ_CREATE,
	                                    0);
	Stream_Write_UINT32(s, desiredAccess); /* DesiredAccess (4 bytes) */
	Stream_Write_UINT32(s, 0); /* AllocationSize (8 bytes) */
	Stream_Write_UINT32(s, 0);
	Stream_Write_UINT32(s, 0); /* FileAttributes (4 bytes) */
	Stream_Write_UINT32(s, 3); /* SharedAccess (4 bytes) */
	Stream_Write_UINT32(s, createDisposition); /* CreateDisposition (4 bytes) */
	Stream_Write_UINT32(s, createOptions); /* CreateOptions (4 bytes) */
	Stream_Write_UINT32(s, pathLength); /* PathLength (4 bytes) */
	/* Convert the path to Unicode. */
	MultiByteToWideChar(CP_ACP, 0, path, -1, (LPWSTR) Stream_Pointer(s),
	                    pathLength);
	Stream_Seek(s, pathLength);
	Stream_SealLength(s);
	status = WTSVirtualChannelWrite(context->priv->ChannelHandle,
	                                (PCHAR) Stream_Buffer(s), Stream_Length(s), &written);
	Stream_Free(s, TRUE);
	return status ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_send_device_close_request(
    RdpdrServerContext* context,
    UINT32 deviceId,
    UINT32 fileId,
    UINT32 completionId)
{
	ULONG written;
	BOOL status;
	wStream* s;
	WLog_DBG(TAG, "RdpdrServerSendDeviceCloseRequest: deviceId=%"PRIu32", fileId=%"PRIu32"",
	         deviceId, fileId);
	s = Stream_New(NULL, 128);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rdpdr_server_write_device_iorequest(s, deviceId, fileId, completionId,
	                                    IRP_MJ_CLOSE, 0);
	Stream_Zero(s, 32); /* Padding (32 bytes) */
	Stream_SealLength(s);
	status = WTSVirtualChannelWrite(context->priv->ChannelHandle,
	                                (PCHAR) Stream_Buffer(s), Stream_Length(s), &written);
	Stream_Free(s, TRUE);
	return status ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_send_device_read_request(
    RdpdrServerContext* context,
    UINT32 deviceId,
    UINT32 fileId,
    UINT32 completionId,
    UINT32 length,
    UINT32 offset)
{
	ULONG written;
	BOOL status;
	wStream* s;
	WLog_DBG(TAG,
	         "RdpdrServerSendDeviceReadRequest: deviceId=%"PRIu32", fileId=%"PRIu32", length=%"PRIu32", offset=%"PRIu32"",
	         deviceId, fileId, length, offset);
	s = Stream_New(NULL, 128);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rdpdr_server_write_device_iorequest(s, deviceId, fileId, completionId,
	                                    IRP_MJ_READ, 0);
	Stream_Write_UINT32(s, length); /* Length (4 bytes) */
	Stream_Write_UINT32(s, offset); /* Offset (8 bytes) */
	Stream_Write_UINT32(s, 0);
	Stream_Zero(s, 20); /* Padding (20 bytes) */
	Stream_SealLength(s);
	status = WTSVirtualChannelWrite(context->priv->ChannelHandle,
	                                (PCHAR) Stream_Buffer(s), Stream_Length(s), &written);
	Stream_Free(s, TRUE);
	return status ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_send_device_write_request(
    RdpdrServerContext* context,
    UINT32 deviceId,
    UINT32 fileId,
    UINT32 completionId,
    const char* data,
    UINT32 length,
    UINT32 offset)
{
	ULONG written;
	BOOL status;
	wStream* s;
	WLog_DBG(TAG,
	         "RdpdrServerSendDeviceWriteRequest: deviceId=%"PRIu32", fileId=%"PRIu32", length=%"PRIu32", offset=%"PRIu32"",
	         deviceId, fileId, length, offset);
	s = Stream_New(NULL, 64 + length);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rdpdr_server_write_device_iorequest(s, deviceId, fileId, completionId,
	                                    IRP_MJ_WRITE, 0);
	Stream_Write_UINT32(s, length); /* Length (4 bytes) */
	Stream_Write_UINT32(s, offset); /* Offset (8 bytes) */
	Stream_Write_UINT32(s, 0);
	Stream_Zero(s, 20); /* Padding (20 bytes) */
	Stream_Write(s, data, length); /* WriteData (variable) */
	Stream_SealLength(s);
	status = WTSVirtualChannelWrite(context->priv->ChannelHandle,
	                                (PCHAR) Stream_Buffer(s), Stream_Length(s), &written);
	Stream_Free(s, TRUE);
	return status ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_send_device_query_directory_request(
    RdpdrServerContext* context,
    UINT32 deviceId,
    UINT32 fileId,
    UINT32 completionId,
    const char* path)
{
	UINT32 pathLength;
	ULONG written;
	BOOL status;
	wStream* s;
	WLog_DBG(TAG,
	         "RdpdrServerSendDeviceQueryDirectoryRequest: deviceId=%"PRIu32", fileId=%"PRIu32", path=%s",
	         deviceId, fileId, path);
	/* Compute the required Unicode size. */
	pathLength = path ? (strlen(path) + 1) * sizeof(WCHAR) : 0;
	s = Stream_New(NULL, 64 + pathLength);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rdpdr_server_write_device_iorequest(s, deviceId, fileId, completionId,
	                                    IRP_MJ_DIRECTORY_CONTROL, IRP_MN_QUERY_DIRECTORY);
	Stream_Write_UINT32(s,
	                    FileDirectoryInformation); /* FsInformationClass (4 bytes) */
	Stream_Write_UINT8(s, path ? 1 : 0); /* InitialQuery (1 byte) */
	Stream_Write_UINT32(s, pathLength); /* PathLength (4 bytes) */
	Stream_Zero(s, 23); /* Padding (23 bytes) */

	/* Convert the path to Unicode. */
	if (pathLength > 0)
	{
		MultiByteToWideChar(CP_ACP, 0, path, -1, (LPWSTR) Stream_Pointer(s),
		                    pathLength);
		Stream_Seek(s, pathLength);
	}

	Stream_SealLength(s);
	status = WTSVirtualChannelWrite(context->priv->ChannelHandle,
	                                (PCHAR) Stream_Buffer(s), Stream_Length(s), &written);
	Stream_Free(s, TRUE);
	return status ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_send_device_file_rename_request(
    RdpdrServerContext* context,
    UINT32 deviceId,
    UINT32 fileId,
    UINT32 completionId,
    const char* path)
{
	UINT32 pathLength;
	ULONG written;
	BOOL status;
	wStream* s;
	WLog_DBG(TAG,
	         "RdpdrServerSendDeviceFileNameRequest: deviceId=%"PRIu32", fileId=%"PRIu32", path=%s",
	         deviceId, fileId, path);
	/* Compute the required Unicode size. */
	pathLength = path ? (strlen(path) + 1) * sizeof(WCHAR) : 0;
	s = Stream_New(NULL, 64 + pathLength);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rdpdr_server_write_device_iorequest(s, deviceId, fileId, completionId,
	                                    IRP_MJ_SET_INFORMATION, 0);
	Stream_Write_UINT32(s,
	                    FileRenameInformation); /* FsInformationClass (4 bytes) */
	Stream_Write_UINT32(s, pathLength + 6); /* Length (4 bytes) */
	Stream_Zero(s, 24); /* Padding (24 bytes) */
	/* RDP_FILE_RENAME_INFORMATION */
	Stream_Write_UINT8(s, 0); /* ReplaceIfExists (1 byte) */
	Stream_Write_UINT8(s, 0); /* RootDirectory (1 byte) */
	Stream_Write_UINT32(s, pathLength); /* FileNameLength (4 bytes) */

	/* Convert the path to Unicode. */
	if (pathLength > 0)
	{
		MultiByteToWideChar(CP_ACP, 0, path, -1, (LPWSTR) Stream_Pointer(s),
		                    pathLength);
		Stream_Seek(s, pathLength);
	}

	Stream_SealLength(s);
	status = WTSVirtualChannelWrite(context->priv->ChannelHandle,
	                                (PCHAR) Stream_Buffer(s), Stream_Length(s), &written);
	Stream_Free(s, TRUE);
	return status ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
}

static void rdpdr_server_convert_slashes(char* path, int size)
{
	int i;

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
static UINT rdpdr_server_drive_create_directory_callback2(
    RdpdrServerContext* context, wStream* s, RDPDR_IRP* irp, UINT32 deviceId,
    UINT32 completionId, UINT32 ioStatus)
{
	WLog_DBG(TAG,
	         "RdpdrServerDriveCreateDirectoryCallback2: deviceId=%"PRIu32", completionId=%"PRIu32", ioStatus=0x%"PRIx32"",
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
static UINT rdpdr_server_drive_create_directory_callback1(
    RdpdrServerContext* context, wStream* s, RDPDR_IRP* irp, UINT32 deviceId,
    UINT32 completionId, UINT32 ioStatus)
{
	UINT32 fileId;
	UINT8 information;
	WLog_DBG(TAG,
	         "RdpdrServerDriveCreateDirectoryCallback1: deviceId=%"PRIu32", completionId=%"PRIu32", ioStatus=0x%"PRIx32"",
	         deviceId, completionId, ioStatus);

	if (ioStatus != STATUS_SUCCESS)
	{
		/* Invoke the create directory completion routine. */
		context->OnDriveCreateDirectoryComplete(context, irp->CallbackData, ioStatus);
		/* Destroy the IRP. */
		rdpdr_server_irp_free(irp);
		return CHANNEL_RC_OK;
	}

	if (Stream_GetRemainingLength(s) < 5)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, fileId); /* FileId (4 bytes) */
	Stream_Read_UINT8(s, information); /* Information (1 byte) */
	/* Setup the IRP. */
	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_create_directory_callback2;
	irp->DeviceId = deviceId;
	irp->FileId = fileId;

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_ERR(TAG, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to close the file */
	return rdpdr_server_send_device_close_request(context, deviceId, fileId,
	        irp->CompletionId);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_create_directory(RdpdrServerContext* context,
        void* callbackData, UINT32 deviceId, const char* path)
{
	RDPDR_IRP* irp;
	irp = rdpdr_server_irp_new();

	if (!irp)
	{
		WLog_ERR(TAG, "rdpdr_server_irp_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_create_directory_callback1;
	irp->CallbackData = callbackData;
	irp->DeviceId = deviceId;
	strncpy(irp->PathName, path, sizeof(irp->PathName));
	rdpdr_server_convert_slashes(irp->PathName, sizeof(irp->PathName));

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_ERR(TAG, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to open the file. */
	return rdpdr_server_send_device_create_request(context, deviceId,
	        irp->CompletionId, irp->PathName,
	        FILE_READ_DATA | SYNCHRONIZE,
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
static UINT rdpdr_server_drive_delete_directory_callback2(
    RdpdrServerContext* context, wStream* s, RDPDR_IRP* irp, UINT32 deviceId,
    UINT32 completionId, UINT32 ioStatus)
{
	WLog_DBG(TAG,
	         "RdpdrServerDriveDeleteDirectoryCallback2: deviceId=%"PRIu32", completionId=%"PRIu32", ioStatus=0x%"PRIx32"",
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
static UINT rdpdr_server_drive_delete_directory_callback1(
    RdpdrServerContext* context, wStream* s, RDPDR_IRP* irp, UINT32 deviceId,
    UINT32 completionId, UINT32 ioStatus)
{
	UINT32 fileId;
	UINT8 information;
	WLog_DBG(TAG,
	         "RdpdrServerDriveDeleteDirectoryCallback1: deviceId=%"PRIu32", completionId=%"PRIu32", ioStatus=0x%"PRIx32"",
	         deviceId, completionId, ioStatus);

	if (ioStatus != STATUS_SUCCESS)
	{
		/* Invoke the delete directory completion routine. */
		context->OnDriveDeleteFileComplete(context, irp->CallbackData, ioStatus);
		/* Destroy the IRP. */
		rdpdr_server_irp_free(irp);
		return CHANNEL_RC_OK;
	}

	if (Stream_GetRemainingLength(s) < 5)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, fileId); /* FileId (4 bytes) */
	Stream_Read_UINT8(s, information); /* Information (1 byte) */
	/* Setup the IRP. */
	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_delete_directory_callback2;
	irp->DeviceId = deviceId;
	irp->FileId = fileId;

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_ERR(TAG, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to close the file */
	return rdpdr_server_send_device_close_request(context, deviceId, fileId,
	        irp->CompletionId);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_delete_directory(RdpdrServerContext* context,
        void* callbackData, UINT32 deviceId, const char* path)
{
	RDPDR_IRP* irp;
	irp = rdpdr_server_irp_new();

	if (!irp)
	{
		WLog_ERR(TAG, "rdpdr_server_irp_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_delete_directory_callback1;
	irp->CallbackData = callbackData;
	irp->DeviceId = deviceId;
	strncpy(irp->PathName, path, sizeof(irp->PathName));
	rdpdr_server_convert_slashes(irp->PathName, sizeof(irp->PathName));

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_ERR(TAG, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to open the file. */
	return rdpdr_server_send_device_create_request(context, deviceId,
	        irp->CompletionId, irp->PathName,
	        DELETE | SYNCHRONIZE, FILE_DIRECTORY_FILE | FILE_DELETE_ON_CLOSE |
	        FILE_SYNCHRONOUS_IO_NONALERT, FILE_OPEN);
}

/*************************************************
 * Drive Query Directory
 ************************************************/

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_query_directory_callback2(
    RdpdrServerContext* context, wStream* s, RDPDR_IRP* irp, UINT32 deviceId,
    UINT32 completionId, UINT32 ioStatus)
{
	UINT error;
	UINT32 length;
	FILE_DIRECTORY_INFORMATION fdi;
	WLog_DBG(TAG,
	         "RdpdrServerDriveQueryDirectoryCallback2: deviceId=%"PRIu32", completionId=%"PRIu32", ioStatus=0x%"PRIx32"",
	         deviceId, completionId, ioStatus);

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, length); /* Length (4 bytes) */

	if (length > 0)
	{
		if ((error = rdpdr_server_read_file_directory_information(s, &fdi)))
		{
			WLog_ERR(TAG,
			         "rdpdr_server_read_file_directory_information failed with error %"PRIu32"!", error);
			return error;
		}
	}
	else
	{
		if (Stream_GetRemainingLength(s) < 1)
		{
			WLog_ERR(TAG, "not enough data in stream!");
			return ERROR_INVALID_DATA;
		}

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
			WLog_ERR(TAG, "rdpdr_server_enqueue_irp failed!");
			rdpdr_server_irp_free(irp);
			return ERROR_INTERNAL_ERROR;
		}

		/* Send a request to query the directory. */
		return rdpdr_server_send_device_query_directory_request(context, irp->DeviceId,
		        irp->FileId, irp->CompletionId, NULL);
	}
	else
	{
		/* Invoke the query directory completion routine. */
		context->OnDriveQueryDirectoryComplete(context, irp->CallbackData, ioStatus,
		                                       NULL);
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
static UINT rdpdr_server_drive_query_directory_callback1(
    RdpdrServerContext* context, wStream* s, RDPDR_IRP* irp, UINT32 deviceId,
    UINT32 completionId, UINT32 ioStatus)
{
	UINT32 fileId;
	WLog_DBG(TAG,
	         "RdpdrServerDriveQueryDirectoryCallback1: deviceId=%"PRIu32", completionId=%"PRIu32", ioStatus=0x%"PRIx32"",
	         deviceId, completionId, ioStatus);

	if (ioStatus != STATUS_SUCCESS)
	{
		/* Invoke the query directory completion routine. */
		context->OnDriveQueryDirectoryComplete(context, irp->CallbackData, ioStatus,
		                                       NULL);
		/* Destroy the IRP. */
		rdpdr_server_irp_free(irp);
		return CHANNEL_RC_OK;
	}

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, fileId);
	/* Setup the IRP. */
	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_query_directory_callback2;
	irp->DeviceId = deviceId;
	irp->FileId = fileId;
	strcat(irp->PathName, "\\*.*");

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_ERR(TAG, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to query the directory. */
	return rdpdr_server_send_device_query_directory_request(context, deviceId,
	        fileId, irp->CompletionId, irp->PathName);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_query_directory(RdpdrServerContext* context,
        void* callbackData, UINT32 deviceId, const char* path)
{
	RDPDR_IRP* irp;
	irp = rdpdr_server_irp_new();

	if (!irp)
	{
		WLog_ERR(TAG, "rdpdr_server_irp_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_query_directory_callback1;
	irp->CallbackData = callbackData;
	irp->DeviceId = deviceId;
	strncpy(irp->PathName, path, sizeof(irp->PathName));
	rdpdr_server_convert_slashes(irp->PathName, sizeof(irp->PathName));

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_ERR(TAG, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to open the directory. */
	return rdpdr_server_send_device_create_request(context, deviceId,
	        irp->CompletionId, irp->PathName,
	        FILE_READ_DATA | SYNCHRONIZE,
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
static UINT rdpdr_server_drive_open_file_callback(RdpdrServerContext* context,
        wStream* s, RDPDR_IRP* irp, UINT32 deviceId, UINT32 completionId,
        UINT32 ioStatus)
{
	UINT32 fileId;
	UINT8 information;
	WLog_DBG(TAG,
	         "RdpdrServerDriveOpenFileCallback: deviceId=%"PRIu32", completionId=%"PRIu32", ioStatus=0x%"PRIx32"",
	         deviceId, completionId, ioStatus);

	if (Stream_GetRemainingLength(s) < 5)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, fileId); /* FileId (4 bytes) */
	Stream_Read_UINT8(s, information); /* Information (1 byte) */
	/* Invoke the open file completion routine. */
	context->OnDriveOpenFileComplete(context, irp->CallbackData, ioStatus, deviceId,
	                                 fileId);
	/* Destroy the IRP. */
	rdpdr_server_irp_free(irp);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_open_file(RdpdrServerContext* context,
        void* callbackData, UINT32 deviceId, const char* path, UINT32 desiredAccess,
        UINT32 createDisposition)
{
	RDPDR_IRP* irp;
	irp = rdpdr_server_irp_new();

	if (!irp)
	{
		WLog_ERR(TAG, "rdpdr_server_irp_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_open_file_callback;
	irp->CallbackData = callbackData;
	irp->DeviceId = deviceId;
	strncpy(irp->PathName, path, sizeof(irp->PathName));
	rdpdr_server_convert_slashes(irp->PathName, sizeof(irp->PathName));

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_ERR(TAG, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to open the file. */
	return rdpdr_server_send_device_create_request(context, deviceId,
	        irp->CompletionId, irp->PathName,
	        desiredAccess | SYNCHRONIZE, FILE_SYNCHRONOUS_IO_NONALERT, createDisposition);
}

/*************************************************
 * Drive Read File
 ************************************************/

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_read_file_callback(RdpdrServerContext* context,
        wStream* s, RDPDR_IRP* irp, UINT32 deviceId, UINT32 completionId,
        UINT32 ioStatus)
{
	UINT32 length;
	char* buffer = NULL;
	WLog_DBG(TAG,
	         "RdpdrServerDriveReadFileCallback: deviceId=%"PRIu32", completionId=%"PRIu32", ioStatus=0x%"PRIx32"",
	         deviceId, completionId, ioStatus);

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, length); /* Length (4 bytes) */

	if (Stream_GetRemainingLength(s) < length)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	if (length > 0)
	{
		buffer = (char*) Stream_Pointer(s);
		Stream_Seek(s, length);
	}

	/* Invoke the read file completion routine. */
	context->OnDriveReadFileComplete(context, irp->CallbackData, ioStatus, buffer,
	                                 length);
	/* Destroy the IRP. */
	rdpdr_server_irp_free(irp);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_read_file(RdpdrServerContext* context,
        void* callbackData, UINT32 deviceId, UINT32 fileId, UINT32 length,
        UINT32 offset)
{
	RDPDR_IRP* irp;
	irp = rdpdr_server_irp_new();

	if (!irp)
	{
		WLog_ERR(TAG, "rdpdr_server_irp_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_read_file_callback;
	irp->CallbackData = callbackData;
	irp->DeviceId = deviceId;
	irp->FileId = fileId;

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_ERR(TAG, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to open the directory. */
	return rdpdr_server_send_device_read_request(context, deviceId, fileId,
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
static UINT rdpdr_server_drive_write_file_callback(RdpdrServerContext* context,
        wStream* s, RDPDR_IRP* irp, UINT32 deviceId, UINT32 completionId,
        UINT32 ioStatus)
{
	UINT32 length;
	WLog_DBG(TAG,
	         "RdpdrServerDriveWriteFileCallback: deviceId=%"PRIu32", completionId=%"PRIu32", ioStatus=0x%"PRIx32"",
	         deviceId, completionId, ioStatus);

	if (Stream_GetRemainingLength(s) < 5)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, length); /* Length (4 bytes) */
	Stream_Seek(s, 1); /* Padding (1 byte) */

	if (Stream_GetRemainingLength(s) < length)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

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
static UINT rdpdr_server_drive_write_file(RdpdrServerContext* context,
        void* callbackData, UINT32 deviceId, UINT32 fileId, const char* buffer,
        UINT32 length, UINT32 offset)
{
	RDPDR_IRP* irp;
	irp = rdpdr_server_irp_new();

	if (!irp)
	{
		WLog_ERR(TAG, "rdpdr_server_irp_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_write_file_callback;
	irp->CallbackData = callbackData;
	irp->DeviceId = deviceId;
	irp->FileId = fileId;

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_ERR(TAG, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to open the directory. */
	return rdpdr_server_send_device_write_request(context, deviceId, fileId,
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
static UINT rdpdr_server_drive_close_file_callback(RdpdrServerContext* context,
        wStream* s, RDPDR_IRP* irp, UINT32 deviceId, UINT32 completionId,
        UINT32 ioStatus)
{
	WLog_DBG(TAG,
	         "RdpdrServerDriveCloseFileCallback: deviceId=%"PRIu32", completionId=%"PRIu32", ioStatus=0x%"PRIx32"",
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
static UINT rdpdr_server_drive_close_file(RdpdrServerContext* context,
        void* callbackData, UINT32 deviceId, UINT32 fileId)
{
	RDPDR_IRP* irp;
	irp = rdpdr_server_irp_new();

	if (!irp)
	{
		WLog_ERR(TAG, "rdpdr_server_irp_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_close_file_callback;
	irp->CallbackData = callbackData;
	irp->DeviceId = deviceId;
	irp->FileId = fileId;

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_ERR(TAG, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to open the directory. */
	return rdpdr_server_send_device_close_request(context, deviceId, fileId,
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
static UINT rdpdr_server_drive_delete_file_callback2(RdpdrServerContext*
        context, wStream* s, RDPDR_IRP* irp, UINT32 deviceId, UINT32 completionId,
        UINT32 ioStatus)
{
	WLog_DBG(TAG,
	         "RdpdrServerDriveDeleteFileCallback2: deviceId=%"PRIu32", completionId=%"PRIu32", ioStatus=0x%"PRIx32"",
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
static UINT rdpdr_server_drive_delete_file_callback1(RdpdrServerContext*
        context, wStream* s, RDPDR_IRP* irp, UINT32 deviceId, UINT32 completionId,
        UINT32 ioStatus)
{
	UINT32 fileId;
	UINT8 information;
	WLog_DBG(TAG,
	         "RdpdrServerDriveDeleteFileCallback1: deviceId=%"PRIu32", completionId=%"PRIu32", ioStatus=0x%"PRIx32"",
	         deviceId, completionId, ioStatus);

	if (ioStatus != STATUS_SUCCESS)
	{
		/* Invoke the close file completion routine. */
		context->OnDriveDeleteFileComplete(context, irp->CallbackData, ioStatus);
		/* Destroy the IRP. */
		rdpdr_server_irp_free(irp);
		return CHANNEL_RC_OK;
	}

	if (Stream_GetRemainingLength(s) < 5)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, fileId); /* FileId (4 bytes) */
	Stream_Read_UINT8(s, information); /* Information (1 byte) */
	/* Setup the IRP. */
	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_delete_file_callback2;
	irp->DeviceId = deviceId;
	irp->FileId = fileId;

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_ERR(TAG, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to close the file */
	return rdpdr_server_send_device_close_request(context, deviceId, fileId,
	        irp->CompletionId);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_server_drive_delete_file(RdpdrServerContext* context,
        void* callbackData, UINT32 deviceId, const char* path)
{
	RDPDR_IRP* irp;
	irp = rdpdr_server_irp_new();

	if (!irp)
	{
		WLog_ERR(TAG, "rdpdr_server_irp_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_delete_file_callback1;
	irp->CallbackData = callbackData;
	irp->DeviceId = deviceId;
	strncpy(irp->PathName, path, sizeof(irp->PathName));
	rdpdr_server_convert_slashes(irp->PathName, sizeof(irp->PathName));

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_ERR(TAG, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to open the file. */
	return rdpdr_server_send_device_create_request(context, deviceId,
	        irp->CompletionId, irp->PathName,
	        FILE_READ_DATA | SYNCHRONIZE,
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
static UINT rdpdr_server_drive_rename_file_callback3(RdpdrServerContext*
        context, wStream* s, RDPDR_IRP* irp, UINT32 deviceId, UINT32 completionId,
        UINT32 ioStatus)
{
	WLog_DBG(TAG,
	         "RdpdrServerDriveRenameFileCallback3: deviceId=%"PRIu32", completionId=%"PRIu32", ioStatus=0x%"PRIx32"",
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
static UINT rdpdr_server_drive_rename_file_callback2(RdpdrServerContext*
        context, wStream* s, RDPDR_IRP* irp, UINT32 deviceId, UINT32 completionId,
        UINT32 ioStatus)
{
	UINT32 length;
	WLog_DBG(TAG,
	         "RdpdrServerDriveRenameFileCallback2: deviceId=%"PRIu32", completionId=%"PRIu32", ioStatus=0x%"PRIx32"",
	         deviceId, completionId, ioStatus);

	if (Stream_GetRemainingLength(s) < 5)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, length); /* Length (4 bytes) */
	Stream_Seek(s, 1); /* Padding (1 byte) */
	/* Invoke the rename file completion routine. */
	context->OnDriveRenameFileComplete(context, irp->CallbackData, ioStatus);
	/* Setup the IRP. */
	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_rename_file_callback3;
	irp->DeviceId = deviceId;

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_ERR(TAG, "rdpdr_server_enqueue_irp failed!");
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
static UINT rdpdr_server_drive_rename_file_callback1(RdpdrServerContext*
        context, wStream* s, RDPDR_IRP* irp, UINT32 deviceId, UINT32 completionId,
        UINT32 ioStatus)
{
	UINT32 fileId;
	UINT8 information;
	WLog_DBG(TAG,
	         "RdpdrServerDriveRenameFileCallback1: deviceId=%"PRIu32", completionId=%"PRIu32", ioStatus=0x%"PRIx32"",
	         deviceId, completionId, ioStatus);

	if (ioStatus != STATUS_SUCCESS)
	{
		/* Invoke the rename file completion routine. */
		context->OnDriveRenameFileComplete(context, irp->CallbackData, ioStatus);
		/* Destroy the IRP. */
		rdpdr_server_irp_free(irp);
		return CHANNEL_RC_OK;
	}

	if (Stream_GetRemainingLength(s) < 5)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, fileId); /* FileId (4 bytes) */
	Stream_Read_UINT8(s, information); /* Information (1 byte) */
	/* Setup the IRP. */
	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_rename_file_callback2;
	irp->DeviceId = deviceId;
	irp->FileId = fileId;

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_ERR(TAG, "rdpdr_server_enqueue_irp failed!");
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
static UINT rdpdr_server_drive_rename_file(RdpdrServerContext* context,
        void* callbackData, UINT32 deviceId, const char* oldPath, const char* newPath)
{
	RDPDR_IRP* irp;
	irp = rdpdr_server_irp_new();

	if (!irp)
	{
		WLog_ERR(TAG, "rdpdr_server_irp_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	irp->CompletionId = context->priv->NextCompletionId++;
	irp->Callback = rdpdr_server_drive_rename_file_callback1;
	irp->CallbackData = callbackData;
	irp->DeviceId = deviceId;
	strncpy(irp->PathName, oldPath, sizeof(irp->PathName));
	strncpy(irp->ExtraBuffer, newPath, sizeof(irp->ExtraBuffer));
	rdpdr_server_convert_slashes(irp->PathName, sizeof(irp->PathName));
	rdpdr_server_convert_slashes(irp->ExtraBuffer, sizeof(irp->ExtraBuffer));

	if (!rdpdr_server_enqueue_irp(context, irp))
	{
		WLog_ERR(TAG, "rdpdr_server_enqueue_irp failed!");
		rdpdr_server_irp_free(irp);
		return ERROR_INTERNAL_ERROR;
	}

	/* Send a request to open the file. */
	return rdpdr_server_send_device_create_request(context, deviceId,
	        irp->CompletionId, irp->PathName,
	        FILE_READ_DATA | SYNCHRONIZE, FILE_SYNCHRONOUS_IO_NONALERT, FILE_OPEN);
}

RdpdrServerContext* rdpdr_server_context_new(HANDLE vcm)
{
	RdpdrServerContext* context;
	context = (RdpdrServerContext*) calloc(1, sizeof(RdpdrServerContext));

	if (context)
	{
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
		context->priv = (RdpdrServerPrivate*) calloc(1, sizeof(RdpdrServerPrivate));

		if (!context->priv)
		{
			WLog_ERR(TAG, "calloc failed!");
			free(context);
			return NULL;
		}

		context->priv->VersionMajor = RDPDR_VERSION_MAJOR;
		context->priv->VersionMinor = RDPDR_VERSION_MINOR_RDP6X;
		context->priv->ClientId = g_ClientId++;
		context->priv->UserLoggedOnPdu = TRUE;
		context->priv->NextCompletionId = 1;
		context->priv->IrpList = ListDictionary_New(TRUE);

		if (!context->priv->IrpList)
		{
			WLog_ERR(TAG, "ListDictionary_New failed!");
			free(context->priv);
			free(context);
			return NULL;
		}
	}
	else
	{
		WLog_ERR(TAG, "calloc failed!");
	}

	return context;
}

void rdpdr_server_context_free(RdpdrServerContext* context)
{
	if (context)
	{
		if (context->priv)
		{
			ListDictionary_Free(context->priv->IrpList);
			free(context->priv);
		}

		free(context);
	}
}

