/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SCard utility functions
 *
 * Copyright 2021 Armin Novak <armin.novak@thincast.com>
 * Copyright 2021 Thincast Technologies GmbH
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

#include <winpr/wlog.h>
#include <winpr/print.h>
#include <winpr/smartcard.h>

#include <freerdp/utils/rdpdr_utils.h>
#include <freerdp/channels/scard.h>
#include <freerdp/channels/rdpdr.h>

#include <freerdp/log.h>
#define TAG FREERDP_TAG("utils.scard")

LONG scard_log_status_error(const char* tag, const char* what, LONG status)
{
	if (status != SCARD_S_SUCCESS)
	{
		DWORD level = WLOG_ERROR;
		switch (status)
		{
			case SCARD_E_TIMEOUT:
				level = WLOG_DEBUG;
				break;
			case SCARD_E_NO_READERS_AVAILABLE:
				level = WLOG_INFO;
				break;
			default:
				break;
		}
		WLog_Print(WLog_Get(tag), level, "%s failed with error %s [%" PRId32 "]", what,
		           SCardGetErrorString(status), status);
	}
	return status;
}

const char* scard_get_ioctl_string(UINT32 ioControlCode, BOOL funcName)
{
	switch (ioControlCode)
	{
		case SCARD_IOCTL_ESTABLISHCONTEXT:
			return funcName ? "SCardEstablishContext" : "SCARD_IOCTL_ESTABLISHCONTEXT";

		case SCARD_IOCTL_RELEASECONTEXT:
			return funcName ? "SCardReleaseContext" : "SCARD_IOCTL_RELEASECONTEXT";

		case SCARD_IOCTL_ISVALIDCONTEXT:
			return funcName ? "SCardIsValidContext" : "SCARD_IOCTL_ISVALIDCONTEXT";

		case SCARD_IOCTL_LISTREADERGROUPSA:
			return funcName ? "SCardListReaderGroupsA" : "SCARD_IOCTL_LISTREADERGROUPSA";

		case SCARD_IOCTL_LISTREADERGROUPSW:
			return funcName ? "SCardListReaderGroupsW" : "SCARD_IOCTL_LISTREADERGROUPSW";

		case SCARD_IOCTL_LISTREADERSA:
			return funcName ? "SCardListReadersA" : "SCARD_IOCTL_LISTREADERSA";

		case SCARD_IOCTL_LISTREADERSW:
			return funcName ? "SCardListReadersW" : "SCARD_IOCTL_LISTREADERSW";

		case SCARD_IOCTL_INTRODUCEREADERGROUPA:
			return funcName ? "SCardIntroduceReaderGroupA" : "SCARD_IOCTL_INTRODUCEREADERGROUPA";

		case SCARD_IOCTL_INTRODUCEREADERGROUPW:
			return funcName ? "SCardIntroduceReaderGroupW" : "SCARD_IOCTL_INTRODUCEREADERGROUPW";

		case SCARD_IOCTL_FORGETREADERGROUPA:
			return funcName ? "SCardForgetReaderGroupA" : "SCARD_IOCTL_FORGETREADERGROUPA";

		case SCARD_IOCTL_FORGETREADERGROUPW:
			return funcName ? "SCardForgetReaderGroupW" : "SCARD_IOCTL_FORGETREADERGROUPW";

		case SCARD_IOCTL_INTRODUCEREADERA:
			return funcName ? "SCardIntroduceReaderA" : "SCARD_IOCTL_INTRODUCEREADERA";

		case SCARD_IOCTL_INTRODUCEREADERW:
			return funcName ? "SCardIntroduceReaderW" : "SCARD_IOCTL_INTRODUCEREADERW";

		case SCARD_IOCTL_FORGETREADERA:
			return funcName ? "SCardForgetReaderA" : "SCARD_IOCTL_FORGETREADERA";

		case SCARD_IOCTL_FORGETREADERW:
			return funcName ? "SCardForgetReaderW" : "SCARD_IOCTL_FORGETREADERW";

		case SCARD_IOCTL_ADDREADERTOGROUPA:
			return funcName ? "SCardAddReaderToGroupA" : "SCARD_IOCTL_ADDREADERTOGROUPA";

		case SCARD_IOCTL_ADDREADERTOGROUPW:
			return funcName ? "SCardAddReaderToGroupW" : "SCARD_IOCTL_ADDREADERTOGROUPW";

		case SCARD_IOCTL_REMOVEREADERFROMGROUPA:
			return funcName ? "SCardRemoveReaderFromGroupA" : "SCARD_IOCTL_REMOVEREADERFROMGROUPA";

		case SCARD_IOCTL_REMOVEREADERFROMGROUPW:
			return funcName ? "SCardRemoveReaderFromGroupW" : "SCARD_IOCTL_REMOVEREADERFROMGROUPW";

		case SCARD_IOCTL_LOCATECARDSA:
			return funcName ? "SCardLocateCardsA" : "SCARD_IOCTL_LOCATECARDSA";

		case SCARD_IOCTL_LOCATECARDSW:
			return funcName ? "SCardLocateCardsW" : "SCARD_IOCTL_LOCATECARDSW";

		case SCARD_IOCTL_GETSTATUSCHANGEA:
			return funcName ? "SCardGetStatusChangeA" : "SCARD_IOCTL_GETSTATUSCHANGEA";

		case SCARD_IOCTL_GETSTATUSCHANGEW:
			return funcName ? "SCardGetStatusChangeW" : "SCARD_IOCTL_GETSTATUSCHANGEW";

		case SCARD_IOCTL_CANCEL:
			return funcName ? "SCardCancel" : "SCARD_IOCTL_CANCEL";

		case SCARD_IOCTL_CONNECTA:
			return funcName ? "SCardConnectA" : "SCARD_IOCTL_CONNECTA";

		case SCARD_IOCTL_CONNECTW:
			return funcName ? "SCardConnectW" : "SCARD_IOCTL_CONNECTW";

		case SCARD_IOCTL_RECONNECT:
			return funcName ? "SCardReconnect" : "SCARD_IOCTL_RECONNECT";

		case SCARD_IOCTL_DISCONNECT:
			return funcName ? "SCardDisconnect" : "SCARD_IOCTL_DISCONNECT";

		case SCARD_IOCTL_BEGINTRANSACTION:
			return funcName ? "SCardBeginTransaction" : "SCARD_IOCTL_BEGINTRANSACTION";

		case SCARD_IOCTL_ENDTRANSACTION:
			return funcName ? "SCardEndTransaction" : "SCARD_IOCTL_ENDTRANSACTION";

		case SCARD_IOCTL_STATE:
			return funcName ? "SCardState" : "SCARD_IOCTL_STATE";

		case SCARD_IOCTL_STATUSA:
			return funcName ? "SCardStatusA" : "SCARD_IOCTL_STATUSA";

		case SCARD_IOCTL_STATUSW:
			return funcName ? "SCardStatusW" : "SCARD_IOCTL_STATUSW";

		case SCARD_IOCTL_TRANSMIT:
			return funcName ? "SCardTransmit" : "SCARD_IOCTL_TRANSMIT";

		case SCARD_IOCTL_CONTROL:
			return funcName ? "SCardControl" : "SCARD_IOCTL_CONTROL";

		case SCARD_IOCTL_GETATTRIB:
			return funcName ? "SCardGetAttrib" : "SCARD_IOCTL_GETATTRIB";

		case SCARD_IOCTL_SETATTRIB:
			return funcName ? "SCardSetAttrib" : "SCARD_IOCTL_SETATTRIB";

		case SCARD_IOCTL_ACCESSSTARTEDEVENT:
			return funcName ? "SCardAccessStartedEvent" : "SCARD_IOCTL_ACCESSSTARTEDEVENT";

		case SCARD_IOCTL_LOCATECARDSBYATRA:
			return funcName ? "SCardLocateCardsByATRA" : "SCARD_IOCTL_LOCATECARDSBYATRA";

		case SCARD_IOCTL_LOCATECARDSBYATRW:
			return funcName ? "SCardLocateCardsByATRB" : "SCARD_IOCTL_LOCATECARDSBYATRW";

		case SCARD_IOCTL_READCACHEA:
			return funcName ? "SCardReadCacheA" : "SCARD_IOCTL_READCACHEA";

		case SCARD_IOCTL_READCACHEW:
			return funcName ? "SCardReadCacheW" : "SCARD_IOCTL_READCACHEW";

		case SCARD_IOCTL_WRITECACHEA:
			return funcName ? "SCardWriteCacheA" : "SCARD_IOCTL_WRITECACHEA";

		case SCARD_IOCTL_WRITECACHEW:
			return funcName ? "SCardWriteCacheW" : "SCARD_IOCTL_WRITECACHEW";

		case SCARD_IOCTL_GETTRANSMITCOUNT:
			return funcName ? "SCardGetTransmitCount" : "SCARD_IOCTL_GETTRANSMITCOUNT";

		case SCARD_IOCTL_RELEASETARTEDEVENT:
			return funcName ? "SCardReleaseStartedEvent" : "SCARD_IOCTL_RELEASETARTEDEVENT";

		case SCARD_IOCTL_GETREADERICON:
			return funcName ? "SCardGetReaderIcon" : "SCARD_IOCTL_GETREADERICON";

		case SCARD_IOCTL_GETDEVICETYPEID:
			return funcName ? "SCardGetDeviceTypeId" : "SCARD_IOCTL_GETDEVICETYPEID";

		default:
			return funcName ? "SCardUnknown" : "SCARD_IOCTL_UNKNOWN";
	}
}

const char* rdpdr_component_string(UINT16 component)
{
	switch (component)
	{
		case RDPDR_CTYP_PRN:
			return "RDPDR_CTYP_PRN";
		case RDPDR_CTYP_CORE:
			return "RDPDR_CTYP_CORE";
		default:
			return "UNKNOWN";
	}
}

const char* rdpdr_packetid_string(UINT16 packetid)
{
	switch (packetid)
	{
		case PAKID_CORE_SERVER_ANNOUNCE:
			return "PAKID_CORE_SERVER_ANNOUNCE";
		case PAKID_CORE_CLIENTID_CONFIRM:
			return "PAKID_CORE_CLIENTID_CONFIRM";
		case PAKID_CORE_CLIENT_NAME:
			return "PAKID_CORE_CLIENT_NAME";
		case PAKID_CORE_DEVICELIST_ANNOUNCE:
			return "PAKID_CORE_DEVICELIST_ANNOUNCE";
		case PAKID_CORE_DEVICE_REPLY:
			return "PAKID_CORE_DEVICE_REPLY";
		case PAKID_CORE_DEVICE_IOREQUEST:
			return "PAKID_CORE_DEVICE_IOREQUEST";
		case PAKID_CORE_DEVICE_IOCOMPLETION:
			return "PAKID_CORE_DEVICE_IOCOMPLETION";
		case PAKID_CORE_SERVER_CAPABILITY:
			return "PAKID_CORE_SERVER_CAPABILITY";
		case PAKID_CORE_CLIENT_CAPABILITY:
			return "PAKID_CORE_CLIENT_CAPABILITY";
		case PAKID_CORE_DEVICELIST_REMOVE:
			return "PAKID_CORE_DEVICELIST_REMOVE";
		case PAKID_CORE_USER_LOGGEDON:
			return "PAKID_CORE_USER_LOGGEDON";
		case PAKID_PRN_CACHE_DATA:
			return "PAKID_PRN_CACHE_DATA";
		case PAKID_PRN_USING_XPS:
			return "PAKID_PRN_USING_XPS";
		default:
			return "UNKNOWN";
	}
}

BOOL rdpdr_write_iocompletion_header(wStream* out, UINT32 DeviceId, UINT32 CompletionId,
                                     UINT32 ioStatus)
{
	WINPR_ASSERT(out);
	Stream_SetPosition(out, 0);
	if (!Stream_EnsureRemainingCapacity(out, 16))
		return FALSE;
	Stream_Write_UINT16(out, RDPDR_CTYP_CORE);                /* Component (2 bytes) */
	Stream_Write_UINT16(out, PAKID_CORE_DEVICE_IOCOMPLETION); /* PacketId (2 bytes) */
	Stream_Write_UINT32(out, DeviceId);                       /* DeviceId (4 bytes) */
	Stream_Write_UINT32(out, CompletionId);                   /* CompletionId (4 bytes) */
	Stream_Write_UINT32(out, ioStatus);                       /* IoStatus (4 bytes) */

	return TRUE;
}

static void rdpdr_dump_packet(wLog* log, DWORD lvl, wStream* s, const char* custom, BOOL send)
{
	if (!WLog_IsLevelActive(log, lvl))
		return;

	const size_t gpos = Stream_GetPosition(s);
	const size_t pos = send ? Stream_GetPosition(s) : Stream_Length(s);

	UINT16 component = 0, packetid = 0;

	Stream_SetPosition(s, 0);

	if (pos >= 2)
		Stream_Read_UINT16(s, component);
	if (pos >= 4)
		Stream_Read_UINT16(s, packetid);

	switch (packetid)
	{
		case PAKID_CORE_SERVER_ANNOUNCE:
		case PAKID_CORE_CLIENTID_CONFIRM:
		{
			UINT16 versionMajor = 0, versionMinor = 0;
			UINT32 clientID = 0;

			if (pos >= 6)
				Stream_Read_UINT16(s, versionMajor);
			if (pos >= 8)
				Stream_Read_UINT16(s, versionMinor);
			if (pos >= 12)
				Stream_Read_UINT32(s, clientID);
			WLog_Print(log, lvl,
			           "%s [%s | %s] [version:%" PRIu16 ".%" PRIu16 "][id:0x%08" PRIx32
			           "] -> %" PRIuz,
			           custom, rdpdr_component_string(component), rdpdr_packetid_string(packetid),
			           versionMajor, versionMinor, clientID, pos);
		}
		break;
		case PAKID_CORE_CLIENT_NAME:
		{
			char name[256] = { 0 };
			UINT32 unicodeFlag = 0;
			UINT32 codePage = 0;
			UINT32 computerNameLen = 0;
			if (pos >= 8)
				Stream_Read_UINT32(s, unicodeFlag);
			if (pos >= 12)
				Stream_Read_UINT32(s, codePage);
			if (pos >= 16)
				Stream_Read_UINT32(s, computerNameLen);
			if (pos >= 16 + computerNameLen)
			{
				if (unicodeFlag == 0)
					Stream_Read(s, name, MIN(sizeof(name), computerNameLen));
				else
					ConvertWCharNToUtf8((const WCHAR*)Stream_Pointer(s),
					                    computerNameLen / sizeof(WCHAR), name, sizeof(name));
			}
			WLog_Print(log, lvl,
			           "%s [%s | %s] [ucs:%" PRIu32 "|cp:%" PRIu32 "][len:0x%08" PRIx32
			           "] '%s' -> %" PRIuz,
			           custom, rdpdr_component_string(component), rdpdr_packetid_string(packetid),
			           unicodeFlag, codePage, computerNameLen, name, pos);
		}
		break;

		case PAKID_CORE_DEVICE_IOREQUEST:
		{
			UINT32 CompletionId = 0;
			UINT32 deviceID = 0, FileId = 0, MajorFunction = 0, MinorFunction = 0;

			if (pos >= 8)
				Stream_Read_UINT32(s, deviceID);
			if (pos >= 12)
				Stream_Read_UINT32(s, FileId);
			if (pos >= 16)
				Stream_Read_UINT32(s, CompletionId);
			if (pos >= 20)
				Stream_Read_UINT32(s, MajorFunction);
			if (pos >= 24)
				Stream_Read_UINT32(s, MinorFunction);
			WLog_Print(log, lvl,
			           "%s [%s | %s] [0x%08" PRIx32 "] FileId=0x%08" PRIx32
			           ", CompletionId=0x%08" PRIx32 ", MajorFunction=0x%08" PRIx32
			           ", MinorFunction=0x%08" PRIx32 " -> %" PRIuz,
			           custom, rdpdr_component_string(component), rdpdr_packetid_string(packetid),
			           deviceID, FileId, CompletionId, MajorFunction, MinorFunction, pos);
		}
		break;
		case PAKID_CORE_DEVICE_IOCOMPLETION:
		{
			UINT32 completionID = 0, ioStatus = 0;
			UINT32 deviceID = 0;
			if (pos >= 8)
				Stream_Read_UINT32(s, deviceID);
			if (pos >= 12)
				Stream_Read_UINT32(s, completionID);
			if (pos >= 16)
				Stream_Read_UINT32(s, ioStatus);

			WLog_Print(log, lvl,
			           "%s [%s | %s] [0x%08" PRIx32 "] completionID=0x%08" PRIx32
			           ", ioStatus=0x%08" PRIx32 " -> %" PRIuz,
			           custom, rdpdr_component_string(component), rdpdr_packetid_string(packetid),
			           deviceID, completionID, ioStatus, pos);
		}
		break;
		case PAKID_CORE_DEVICE_REPLY:
		{
			UINT32 deviceID = 0, status = 0;

			if (pos >= 8)
				Stream_Read_UINT32(s, deviceID);
			if (pos >= 12)
				Stream_Read_UINT32(s, status);
			WLog_Print(log, lvl,
			           "%s [%s | %s] [id:0x%08" PRIx32 ",status=0x%08" PRIx32 "] -> %" PRIuz,
			           custom, rdpdr_component_string(component), rdpdr_packetid_string(packetid),
			           deviceID, status, pos);
		}
		break;
		case PAKID_CORE_CLIENT_CAPABILITY:
		case PAKID_CORE_SERVER_CAPABILITY:
		{
			UINT16 numCapabilities = 0;
			if (pos >= 6)
				Stream_Read_UINT16(s, numCapabilities);
			if (pos >= 8)
				Stream_Seek_UINT16(s); /* padding */
			WLog_Print(log, lvl, "%s [%s | %s] [caps:%" PRIu16 "] -> %" PRIuz, custom,
			           rdpdr_component_string(component), rdpdr_packetid_string(packetid),
			           numCapabilities, pos);
			for (UINT16 x = 0; x < numCapabilities; x++)
			{
				RDPDR_CAPABILITY_HEADER header = { 0 };
				const UINT error = rdpdr_read_capset_header(log, s, &header);
				if (error == CHANNEL_RC_OK)
					Stream_Seek(s, header.CapabilityLength);
			}
		}
		break;
		case PAKID_CORE_DEVICELIST_ANNOUNCE:
		{
			size_t offset = 8;
			UINT32 count = 0;

			if (pos >= offset)
				Stream_Read_UINT32(s, count);

			WLog_Print(log, lvl, "%s [%s | %s] [%" PRIu32 "] -> %" PRIuz, custom,
			           rdpdr_component_string(component), rdpdr_packetid_string(packetid), count,
			           pos);

			for (UINT32 x = 0; x < count; x++)
			{
				RdpdrDevice device = { 0 };

				offset += 20;
				if (pos >= offset)
				{
					Stream_Read_UINT32(s, device.DeviceType);       /* DeviceType (4 bytes) */
					Stream_Read_UINT32(s, device.DeviceId);         /* DeviceId (4 bytes) */
					Stream_Read(s, device.PreferredDosName, 8);     /* PreferredDosName (8 bytes) */
					Stream_Read_UINT32(s, device.DeviceDataLength); /* DeviceDataLength (4 bytes) */
					device.DeviceData = Stream_Pointer(s);
				}
				offset += device.DeviceDataLength;

				WLog_Print(log, lvl,
				           "%s [announce][%" PRIu32 "] %s [0x%08" PRIx32
				           "] '%s' [DeviceDataLength=%" PRIu32 "]",
				           custom, x, freerdp_rdpdr_dtyp_string(device.DeviceType), device.DeviceId,
				           device.PreferredDosName, device.DeviceDataLength);
			}
		}
		break;
		case PAKID_CORE_DEVICELIST_REMOVE:
		{
			size_t offset = 8;
			UINT32 count = 0;

			if (pos >= offset)
				Stream_Read_UINT32(s, count);

			WLog_Print(log, lvl, "%s [%s | %s] [%" PRIu32 "] -> %" PRIuz, custom,
			           rdpdr_component_string(component), rdpdr_packetid_string(packetid), count,
			           pos);

			for (UINT32 x = 0; x < count; x++)
			{
				UINT32 id = 0;

				offset += 4;
				if (pos >= offset)
					Stream_Read_UINT32(s, id);

				WLog_Print(log, lvl, "%s [remove][%" PRIu32 "] id=%" PRIu32, custom, x, id);
			}
		}
		break;
		case PAKID_CORE_USER_LOGGEDON:
			WLog_Print(log, lvl, "%s [%s | %s] -> %" PRIuz, custom,
			           rdpdr_component_string(component), rdpdr_packetid_string(packetid), pos);
			break;
		default:
		{
			WLog_Print(log, lvl, "%s [%s | %s] -> %" PRIuz, custom,
			           rdpdr_component_string(component), rdpdr_packetid_string(packetid), pos);
		}
		break;
	}

	// winpr_HexLogDump(log, lvl, Stream_Buffer(s), pos);
	Stream_SetPosition(s, gpos);
}

void rdpdr_dump_received_packet(wLog* log, DWORD lvl, wStream* s, const char* custom)
{
	rdpdr_dump_packet(log, lvl, s, custom, FALSE);
}

void rdpdr_dump_send_packet(wLog* log, DWORD lvl, wStream* s, const char* custom)
{
	rdpdr_dump_packet(log, lvl, s, custom, TRUE);
}

const char* rdpdr_irp_string(UINT32 major)
{
	switch (major)
	{
		case IRP_MJ_CREATE:
			return "IRP_MJ_CREATE";
		case IRP_MJ_CLOSE:
			return "IRP_MJ_CLOSE";
		case IRP_MJ_READ:
			return "IRP_MJ_READ";
		case IRP_MJ_WRITE:
			return "IRP_MJ_WRITE";
		case IRP_MJ_DEVICE_CONTROL:
			return "IRP_MJ_DEVICE_CONTROL";
		case IRP_MJ_QUERY_VOLUME_INFORMATION:
			return "IRP_MJ_QUERY_VOLUME_INFORMATION";
		case IRP_MJ_SET_VOLUME_INFORMATION:
			return "IRP_MJ_SET_VOLUME_INFORMATION";
		case IRP_MJ_QUERY_INFORMATION:
			return "IRP_MJ_QUERY_INFORMATION";
		case IRP_MJ_SET_INFORMATION:
			return "IRP_MJ_SET_INFORMATION";
		case IRP_MJ_DIRECTORY_CONTROL:
			return "IRP_MJ_DIRECTORY_CONTROL";
		case IRP_MJ_LOCK_CONTROL:
			return "IRP_MJ_LOCK_CONTROL";
		default:
			return "IRP_UNKNOWN";
	}
}

const char* rdpdr_cap_type_string(UINT16 capability)
{
	switch (capability)
	{
		case CAP_GENERAL_TYPE:
			return "CAP_GENERAL_TYPE";
		case CAP_PRINTER_TYPE:
			return "CAP_PRINTER_TYPE";
		case CAP_PORT_TYPE:
			return "CAP_PORT_TYPE";
		case CAP_DRIVE_TYPE:
			return "CAP_DRIVE_TYPE";
		case CAP_SMARTCARD_TYPE:
			return "CAP_SMARTCARD_TYPE";
		default:
			return "CAP_UNKNOWN";
	}
}

UINT rdpdr_read_capset_header(wLog* log, wStream* s, RDPDR_CAPABILITY_HEADER* header)
{
	WINPR_ASSERT(header);
	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 8))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, header->CapabilityType);   /* CapabilityType (2 bytes) */
	Stream_Read_UINT16(s, header->CapabilityLength); /* CapabilityLength (2 bytes) */
	Stream_Read_UINT32(s, header->Version);          /* Version (4 bytes) */

	WLog_Print(log, WLOG_TRACE,
	           "[%s] capability %s [0x%04" PRIx16 "] got version %" PRIu32 ", length %" PRIu16,
	           __FUNCTION__, rdpdr_cap_type_string(header->CapabilityType), header->CapabilityType,
	           header->Version, header->CapabilityLength);
	if (header->CapabilityLength < 8)
	{
		WLog_Print(log, WLOG_ERROR, "[%s] capability %s got short length %" PRIu32, __FUNCTION__,
		           rdpdr_cap_type_string(header->CapabilityType), header->CapabilityLength);
		return ERROR_INVALID_DATA;
	}
	header->CapabilityLength -= 8;
	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, header->CapabilityLength))
		return ERROR_INVALID_DATA;
	return CHANNEL_RC_OK;
}

UINT rdpdr_write_capset_header(wLog* log, wStream* s, const RDPDR_CAPABILITY_HEADER* header)
{
	WINPR_ASSERT(header);
	WINPR_ASSERT(header->CapabilityLength >= 8);

	if (!Stream_EnsureRemainingCapacity(s, header->CapabilityLength))
	{
		WLog_Print(log, WLOG_ERROR, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	WLog_Print(log, WLOG_TRACE, "[%s] writing capability %s version %" PRIu32 ", length %" PRIu16,
	           __FUNCTION__, rdpdr_cap_type_string(header->CapabilityType), header->Version,
	           header->CapabilityLength);
	Stream_Write_UINT16(s, header->CapabilityType);   /* CapabilityType (2 bytes) */
	Stream_Write_UINT16(s, header->CapabilityLength); /* CapabilityLength (2 bytes) */
	Stream_Write_UINT32(s, header->Version);          /* Version (4 bytes) */
	return CHANNEL_RC_OK;
}
