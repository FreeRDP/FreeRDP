/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
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

#include <freerdp/config.h>

#include <winpr/assert.h>
#include <winpr/string.h>
#include <winpr/print.h>

#include "pf_channel_rdpdr.h"
#include "pf_channel_smartcard.h"

#include <freerdp/server/proxy/proxy_log.h>
#include <freerdp/channels/rdpdr.h>
#include <freerdp/channels/channels.h>
#include <freerdp/utils/rdpdr_utils.h>

#define TAG PROXY_TAG("channel.rdpdr")

#define SCARD_DEVICE_ID UINT32_MAX

typedef struct
{
	InterceptContextMapEntry base;
	wStream* s;
	wStream* buffer;
	UINT16 versionMajor;
	UINT16 versionMinor;
	UINT32 clientID;
	UINT32 computerNameLen;
	BOOL computerNameUnicode;
	union
	{
		WCHAR* wc;
		char* c;
		void* v;
	} computerName;
	UINT32 SpecialDeviceCount;
	UINT32 capabilityVersions[6];
} pf_channel_common_context;

typedef enum
{
	STATE_CLIENT_EXPECT_SERVER_ANNOUNCE_REQUEST = 0x01,
	STATE_CLIENT_EXPECT_SERVER_CORE_CAPABILITY_REQUEST = 0x02,
	STATE_CLIENT_EXPECT_SERVER_CLIENT_ID_CONFIRM = 0x04,
	STATE_CLIENT_CHANNEL_RUNNING = 0x10
} pf_channel_client_state;

typedef struct
{
	pf_channel_common_context common;
	pf_channel_client_state state;
	UINT32 flags;
	UINT16 maxMajorVersion;
	UINT16 maxMinorVersion;
	wQueue* queue;
	wLog* log;
} pf_channel_client_context;

typedef enum
{
	STATE_SERVER_INITIAL,
	STATE_SERVER_EXPECT_CLIENT_ANNOUNCE_REPLY,
	STATE_SERVER_EXPECT_CLIENT_NAME_REQUEST,
	STATE_SERVER_EXPECT_EXPECT_CLIENT_CAPABILITY_RESPONE,
	STATE_SERVER_CHANNEL_RUNNING
} pf_channel_server_state;

typedef struct
{
	pf_channel_common_context common;
	pf_channel_server_state state;
	DWORD SessionId;
	HANDLE handle;
	wArrayList* blockedDevices;
	wLog* log;
} pf_channel_server_context;

#define proxy_client "[proxy<-->client]"
#define proxy_server "[proxy<-->server]"

#define proxy_client_rx proxy_client " receive"
#define proxy_client_tx proxy_client " send"
#define proxy_server_rx proxy_server " receive"
#define proxy_server_tx proxy_server " send"

#define SERVER_RX_LOG(log, lvl, fmt, ...) WLog_Print(log, lvl, proxy_client_rx fmt, ##__VA_ARGS__)
#define CLIENT_RX_LOG(log, lvl, fmt, ...) WLog_Print(log, lvl, proxy_server_rx fmt, ##__VA_ARGS__)
#define SERVER_TX_LOG(log, lvl, fmt, ...) WLog_Print(log, lvl, proxy_client_tx fmt, ##__VA_ARGS__)
#define CLIENT_TX_LOG(log, lvl, fmt, ...) WLog_Print(log, lvl, proxy_server_tx fmt, ##__VA_ARGS__)
#define RX_LOG(srv, lvl, fmt, ...)                  \
	do                                              \
	{                                               \
		if (srv)                                    \
		{                                           \
			SERVER_RX_LOG(lvl, fmt, ##__VA_ARGS__); \
		}                                           \
		else                                        \
		{                                           \
			CLIENT_RX_LOG(lvl, fmt, ##__VA_ARGS__); \
		}                                           \
	} while (0)

#define SERVER_RXTX_LOG(send, log, lvl, fmt, ...)        \
	do                                                   \
	{                                                    \
		if (send)                                        \
		{                                                \
			SERVER_TX_LOG(log, lvl, fmt, ##__VA_ARGS__); \
		}                                                \
		else                                             \
		{                                                \
			SERVER_RX_LOG(log, lvl, fmt, ##__VA_ARGS__); \
		}                                                \
	} while (0)

#define Stream_CheckAndLogRequiredLengthSrv(log, s, len)                                       \
	Stream_CheckAndLogRequiredLengthWLogEx(log, WLOG_WARN, s, len,                             \
	                                       proxy_client_rx " %s(%s:%" PRIuz ")", __FUNCTION__, \
	                                       __FILE__, __LINE__)
#define Stream_CheckAndLogRequiredLengthClient(log, s, len)                                    \
	Stream_CheckAndLogRequiredLengthWLogEx(log, WLOG_WARN, s, len,                             \
	                                       proxy_server_rx " %s(%s:%" PRIuz ")", __FUNCTION__, \
	                                       __FILE__, __LINE__)
#define Stream_CheckAndLogRequiredLengthRx(srv, log, s, len) \
	Stream_CheckAndLogRequiredLengthRx_(srv, log, s, len, __FUNCTION__, __FILE__, __LINE__)
static BOOL Stream_CheckAndLogRequiredLengthRx_(BOOL srv, wLog* log, wStream* s, size_t len,
                                                const char* fkt, const char* file, size_t line)
{
	const char* fmt =
	    srv ? proxy_server_rx " %s(%s:%" PRIuz ")" : proxy_client_rx " %s(%s:%" PRIuz ")";

	return Stream_CheckAndLogRequiredLengthWLogEx(log, WLOG_WARN, s, len, fmt, fkt, file, line);
}

static const char* rdpdr_server_state_to_string(pf_channel_server_state state)
{
	switch (state)
	{
		case STATE_SERVER_INITIAL:
			return "STATE_SERVER_INITIAL";
		case STATE_SERVER_EXPECT_CLIENT_ANNOUNCE_REPLY:
			return "STATE_SERVER_EXPECT_CLIENT_ANNOUNCE_REPLY";
		case STATE_SERVER_EXPECT_CLIENT_NAME_REQUEST:
			return "STATE_SERVER_EXPECT_CLIENT_NAME_REQUEST";
		case STATE_SERVER_EXPECT_EXPECT_CLIENT_CAPABILITY_RESPONE:
			return "STATE_SERVER_EXPECT_EXPECT_CLIENT_CAPABILITY_RESPONE";
		case STATE_SERVER_CHANNEL_RUNNING:
			return "STATE_SERVER_CHANNEL_RUNNING";
		default:
			return "STATE_SERVER_UNKNOWN";
	}
}

static const char* rdpdr_client_state_to_string(pf_channel_client_state state)
{
	switch (state)
	{
		case STATE_CLIENT_EXPECT_SERVER_ANNOUNCE_REQUEST:
			return "STATE_CLIENT_EXPECT_SERVER_ANNOUNCE_REQUEST";
		case STATE_CLIENT_EXPECT_SERVER_CORE_CAPABILITY_REQUEST:
			return "STATE_CLIENT_EXPECT_SERVER_CORE_CAPABILITY_REQUEST";
		case STATE_CLIENT_EXPECT_SERVER_CLIENT_ID_CONFIRM:
			return "STATE_CLIENT_EXPECT_SERVER_CLIENT_ID_CONFIRM";
		case STATE_CLIENT_CHANNEL_RUNNING:
			return "STATE_CLIENT_CHANNEL_RUNNING";
		default:
			return "STATE_CLIENT_UNKNOWN";
	}
}

static wStream* rdpdr_get_send_buffer(pf_channel_common_context* rdpdr, UINT16 component,
                                      UINT16 PacketID, size_t capacity)
{
	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(rdpdr->s);
	if (!Stream_SetPosition(rdpdr->s, 0))
		return NULL;
	if (!Stream_EnsureCapacity(rdpdr->s, capacity + 4))
		return NULL;
	Stream_Write_UINT16(rdpdr->s, component);
	Stream_Write_UINT16(rdpdr->s, PacketID);
	return rdpdr->s;
}

static wStream* rdpdr_client_get_send_buffer(pf_channel_client_context* rdpdr, UINT16 component,
                                             UINT16 PacketID, size_t capacity)
{
	WINPR_ASSERT(rdpdr);
	return rdpdr_get_send_buffer(&rdpdr->common, component, PacketID, capacity);
}

static wStream* rdpdr_server_get_send_buffer(pf_channel_server_context* rdpdr, UINT16 component,
                                             UINT16 PacketID, size_t capacity)
{
	WINPR_ASSERT(rdpdr);
	return rdpdr_get_send_buffer(&rdpdr->common, component, PacketID, capacity);
}

static UINT rdpdr_client_send(wLog* log, pClientContext* pc, wStream* s)
{
	UINT16 channelId;

	WINPR_ASSERT(log);
	WINPR_ASSERT(pc);
	WINPR_ASSERT(s);
	WINPR_ASSERT(pc->context.instance);

	if (!pc->connected)
	{
		CLIENT_TX_LOG(log, WLOG_WARN, "Ignoring channel %s message, not connected!",
		              RDPDR_SVC_CHANNEL_NAME);
		return CHANNEL_RC_OK;
	}

	channelId = freerdp_channels_get_id_by_name(pc->context.instance, RDPDR_SVC_CHANNEL_NAME);
	/* Ignore unmappable channels. Might happen when the channel was already down and
	 * some delayed message is tried to be sent. */
	if ((channelId == 0) || (channelId == UINT16_MAX))
		return ERROR_INTERNAL_ERROR;

	Stream_SealLength(s);
	rdpdr_dump_send_packet(log, WLOG_TRACE, s, proxy_server_tx);
	WINPR_ASSERT(pc->context.instance->SendChannelData);
	if (!pc->context.instance->SendChannelData(pc->context.instance, channelId, Stream_Buffer(s),
	                                           Stream_Length(s)))
		return ERROR_EVT_CHANNEL_NOT_FOUND;
	return CHANNEL_RC_OK;
}

static UINT rdpdr_seal_send_free_request(pf_channel_server_context* context, wStream* s)
{
	BOOL status;
	size_t len;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->handle);
	WINPR_ASSERT(s);

	Stream_SealLength(s);
	len = Stream_Length(s);
	WINPR_ASSERT(len <= ULONG_MAX);

	rdpdr_dump_send_packet(context->log, WLOG_TRACE, s, proxy_client_tx);
	status = WTSVirtualChannelWrite(context->handle, (char*)Stream_Buffer(s), (ULONG)len, NULL);
	return (status) ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
}

static BOOL rdpdr_process_server_header(BOOL server, wLog* log, wStream* s, UINT16 component,
                                        UINT16 PacketId, size_t expect)
{
	UINT16 rpacketid, rcomponent;

	WINPR_ASSERT(s);
	if (!Stream_CheckAndLogRequiredLengthRx(server, log, s, 4))
	{
		RX_LOG(server, log, WLOG_WARN, "RDPDR_HEADER[%s | %s]: expected length 4, got %" PRIuz,
		       rdpdr_component_string(component), rdpdr_packetid_string(PacketId),
		       Stream_GetRemainingLength(s));
		return FALSE;
	}

	Stream_Read_UINT16(s, rcomponent);
	Stream_Read_UINT16(s, rpacketid);

	if (rcomponent != component)
	{
		RX_LOG(server, log, WLOG_WARN, "RDPDR_HEADER[%s | %s]: got component %s",
		       rdpdr_component_string(component), rdpdr_packetid_string(PacketId),
		       rdpdr_component_string(rcomponent));
		return FALSE;
	}

	if (rpacketid != PacketId)
	{
		RX_LOG(server, log, WLOG_WARN, "RDPDR_HEADER[%s | %s]: got PacketID %s",
		       rdpdr_component_string(component), rdpdr_packetid_string(PacketId),
		       rdpdr_packetid_string(rpacketid));
		return FALSE;
	}

	if (!Stream_CheckAndLogRequiredLengthRx(server, log, s, expect))
	{
		RX_LOG(server, log, WLOG_WARN,
		       "RDPDR_HEADER[%s | %s] not enought data, expected %" PRIuz ", "
		       "got %" PRIuz,
		       rdpdr_component_string(component), rdpdr_packetid_string(PacketId), expect,
		       Stream_GetRemainingLength(s));
		return ERROR_INVALID_DATA;
	}

	return TRUE;
}

static BOOL rdpdr_check_version(BOOL server, wLog* log, UINT16 versionMajor, UINT16 versionMinor,
                                UINT16 component, UINT16 PacketId)
{
	if (versionMajor != RDPDR_VERSION_MAJOR)
	{
		RX_LOG(server, log, WLOG_WARN, "[%s | %s] expected MajorVersion %" PRIu16 ", got %" PRIu16,
		       rdpdr_component_string(component), rdpdr_packetid_string(PacketId),
		       RDPDR_VERSION_MAJOR, versionMajor);
		return FALSE;
	}
	switch (versionMinor)
	{
		case RDPDR_VERSION_MINOR_RDP50:
		case RDPDR_VERSION_MINOR_RDP51:
		case RDPDR_VERSION_MINOR_RDP52:
		case RDPDR_VERSION_MINOR_RDP6X:
		case RDPDR_VERSION_MINOR_RDP10X:
			break;
		default:
		{
			RX_LOG(server, log, WLOG_WARN, "[%s | %s] unsupported MinorVersion %" PRIu16,
			       rdpdr_component_string(component), rdpdr_packetid_string(PacketId),
			       versionMinor);
			return FALSE;
		}
	}
	return TRUE;
}

static UINT rdpdr_process_server_announce_request(pf_channel_client_context* rdpdr, wStream* s)
{
	const UINT16 component = RDPDR_CTYP_CORE;
	const UINT16 packetid = PAKID_CORE_SERVER_ANNOUNCE;
	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(s);

	if (!rdpdr_process_server_header(FALSE, rdpdr->log, s, component, packetid, 8))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, rdpdr->common.versionMajor);
	Stream_Read_UINT16(s, rdpdr->common.versionMinor);

	if (!rdpdr_check_version(FALSE, rdpdr->log, rdpdr->common.versionMajor,
	                         rdpdr->common.versionMinor, component, packetid))
		return ERROR_INVALID_DATA;

	/* Limit maximum channel protocol version to the one set by proxy server */
	if (rdpdr->common.versionMajor > rdpdr->maxMajorVersion)
	{
		rdpdr->common.versionMajor = rdpdr->maxMajorVersion;
		rdpdr->common.versionMinor = rdpdr->maxMinorVersion;
	}
	else if (rdpdr->common.versionMinor > rdpdr->maxMinorVersion)
		rdpdr->common.versionMinor = rdpdr->maxMinorVersion;

	Stream_Read_UINT32(s, rdpdr->common.clientID);
	return CHANNEL_RC_OK;
}

static UINT rdpdr_server_send_announce_request(pf_channel_server_context* context)
{
	wStream* s =
	    rdpdr_server_get_send_buffer(context, RDPDR_CTYP_CORE, PAKID_CORE_SERVER_ANNOUNCE, 8);
	if (!s)
		return CHANNEL_RC_NO_MEMORY;

	Stream_Write_UINT16(s, context->common.versionMajor); /* VersionMajor (2 bytes) */
	Stream_Write_UINT16(s, context->common.versionMinor); /* VersionMinor (2 bytes) */
	Stream_Write_UINT32(s, context->common.clientID);     /* ClientId (4 bytes) */
	return rdpdr_seal_send_free_request(context, s);
}

static UINT rdpdr_process_client_announce_reply(pf_channel_server_context* rdpdr, wStream* s)
{
	const UINT16 component = RDPDR_CTYP_CORE;
	const UINT16 packetid = PAKID_CORE_CLIENTID_CONFIRM;
	UINT16 versionMajor, versionMinor;
	UINT32 clientID;

	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(s);

	if (!rdpdr_process_server_header(TRUE, rdpdr->log, s, component, packetid, 8))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, versionMajor);
	Stream_Read_UINT16(s, versionMinor);

	if (!rdpdr_check_version(TRUE, rdpdr->log, versionMajor, versionMinor, component, packetid))
		return ERROR_INVALID_DATA;

	if ((rdpdr->common.versionMajor != versionMajor) ||
	    (rdpdr->common.versionMinor != versionMinor))
	{
		SERVER_RX_LOG(
		    rdpdr->log, WLOG_WARN,
		    "[%s | %s] downgrading version from %" PRIu16 ".%" PRIu16 " to %" PRIu16 ".%" PRIu16,
		    rdpdr_component_string(component), rdpdr_packetid_string(packetid),
		    rdpdr->common.versionMajor, rdpdr->common.versionMinor, versionMajor, versionMinor);
		rdpdr->common.versionMajor = versionMajor;
		rdpdr->common.versionMinor = versionMinor;
	}
	Stream_Read_UINT32(s, clientID);
	if (rdpdr->common.clientID != clientID)
	{
		SERVER_RX_LOG(rdpdr->log, WLOG_WARN,
		              "[%s | %s] changing clientID 0x%08" PRIu32 " to 0x%08" PRIu32,
		              rdpdr_component_string(component), rdpdr_packetid_string(packetid),
		              rdpdr->common.clientID, clientID);
		rdpdr->common.clientID = clientID;
	}

	return CHANNEL_RC_OK;
}

static UINT rdpdr_send_client_announce_reply(pClientContext* pc, pf_channel_client_context* rdpdr)
{
	wStream* s =
	    rdpdr_client_get_send_buffer(rdpdr, RDPDR_CTYP_CORE, PAKID_CORE_CLIENTID_CONFIRM, 8);
	if (!s)
		return CHANNEL_RC_NO_MEMORY;

	Stream_Write_UINT16(s, rdpdr->common.versionMajor);
	Stream_Write_UINT16(s, rdpdr->common.versionMinor);
	Stream_Write_UINT32(s, rdpdr->common.clientID);
	return rdpdr_client_send(rdpdr->log, pc, s);
}

static UINT rdpdr_process_client_name_request(pf_channel_server_context* rdpdr, wStream* s,
                                              pClientContext* pc)
{
	UINT32 unicodeFlag;
	UINT32 codePage;
	void* tmp;

	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(s);
	WINPR_ASSERT(pc);

	if (!rdpdr_process_server_header(TRUE, rdpdr->log, s, RDPDR_CTYP_CORE, PAKID_CORE_CLIENT_NAME,
	                                 12))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, unicodeFlag);
	rdpdr->common.computerNameUnicode = (unicodeFlag & 1);

	Stream_Read_UINT32(s, codePage);
	WINPR_UNUSED(codePage); /* Field is ignored */
	Stream_Read_UINT32(s, rdpdr->common.computerNameLen);
	if (!Stream_CheckAndLogRequiredLengthSrv(rdpdr->log, s, rdpdr->common.computerNameLen))
	{
		SERVER_RX_LOG(
		    rdpdr->log, WLOG_WARN, "[%s | %s]: missing data, got " PRIu32 ", expected %" PRIu32,
		    rdpdr_component_string(RDPDR_CTYP_CORE), rdpdr_packetid_string(PAKID_CORE_CLIENT_NAME),
		    Stream_GetRemainingLength(s), rdpdr->common.computerNameLen);
		return ERROR_INVALID_DATA;
	}
	tmp = realloc(rdpdr->common.computerName.v, rdpdr->common.computerNameLen);
	if (!tmp)
		return CHANNEL_RC_NO_MEMORY;
	rdpdr->common.computerName.v = tmp;

	Stream_Read(s, rdpdr->common.computerName.v, rdpdr->common.computerNameLen);

	pc->computerNameLen = rdpdr->common.computerNameLen;
	pc->computerNameUnicode = rdpdr->common.computerNameUnicode;
	tmp = realloc(pc->computerName.v, pc->computerNameLen);
	if (!tmp)
		return CHANNEL_RC_NO_MEMORY;
	pc->computerName.v = tmp;
	memcpy(pc->computerName.v, rdpdr->common.computerName.v, pc->computerNameLen);

	return CHANNEL_RC_OK;
}

static UINT rdpdr_send_client_name_request(pClientContext* pc, pf_channel_client_context* rdpdr)
{
	wStream* s;

	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(pc);

	{
		void* tmp = realloc(rdpdr->common.computerName.v, pc->computerNameLen);
		if (!tmp)
			return CHANNEL_RC_NO_MEMORY;
		rdpdr->common.computerName.v = tmp;
		rdpdr->common.computerNameLen = pc->computerNameLen;
		rdpdr->common.computerNameUnicode = pc->computerNameUnicode;
		memcpy(rdpdr->common.computerName.v, pc->computerName.v, pc->computerNameLen);
	}
	s = rdpdr_client_get_send_buffer(rdpdr, RDPDR_CTYP_CORE, PAKID_CORE_CLIENT_NAME,
	                                 12U + rdpdr->common.computerNameLen);
	if (!s)
		return CHANNEL_RC_NO_MEMORY;

	Stream_Write_UINT32(s, rdpdr->common.computerNameUnicode
	                           ? 1
	                           : 0); /* unicodeFlag, 0 for ASCII and 1 for Unicode */
	Stream_Write_UINT32(s, 0);       /* codePage, must be set to zero */
	Stream_Write_UINT32(s, rdpdr->common.computerNameLen);
	Stream_Write(s, rdpdr->common.computerName.v, rdpdr->common.computerNameLen);
	return rdpdr_client_send(rdpdr->log, pc, s);
}

#define rdpdr_ignore_capset(srv, log, s, header) \
	rdpdr_ignore_capset_((srv), (log), (s), header, __FUNCTION__)
static UINT rdpdr_ignore_capset_(BOOL srv, wLog* log, wStream* s,
                                 const RDPDR_CAPABILITY_HEADER* header, const char* fkt)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(header);

	Stream_Seek(s, header->CapabilityLength);
	return CHANNEL_RC_OK;
}

static UINT rdpdr_client_process_general_capset(pf_channel_client_context* rdpdr, wStream* s,
                                                const RDPDR_CAPABILITY_HEADER* header)
{
	WINPR_UNUSED(rdpdr);
	return rdpdr_ignore_capset(FALSE, rdpdr->log, s, header);
}

static UINT rdpdr_process_printer_capset(pf_channel_client_context* rdpdr, wStream* s,
                                         const RDPDR_CAPABILITY_HEADER* header)
{
	WINPR_UNUSED(rdpdr);
	return rdpdr_ignore_capset(FALSE, rdpdr->log, s, header);
}

static UINT rdpdr_process_port_capset(pf_channel_client_context* rdpdr, wStream* s,
                                      const RDPDR_CAPABILITY_HEADER* header)
{
	WINPR_UNUSED(rdpdr);
	return rdpdr_ignore_capset(FALSE, rdpdr->log, s, header);
}

static UINT rdpdr_process_drive_capset(pf_channel_client_context* rdpdr, wStream* s,
                                       const RDPDR_CAPABILITY_HEADER* header)
{
	WINPR_UNUSED(rdpdr);
	return rdpdr_ignore_capset(FALSE, rdpdr->log, s, header);
}

static UINT rdpdr_process_smartcard_capset(pf_channel_client_context* rdpdr, wStream* s,
                                           const RDPDR_CAPABILITY_HEADER* header)
{
	WINPR_UNUSED(rdpdr);
	return rdpdr_ignore_capset(FALSE, rdpdr->log, s, header);
}

static UINT rdpdr_process_server_core_capability_request(pf_channel_client_context* rdpdr,
                                                         wStream* s)
{
	UINT status = CHANNEL_RC_OK;
	UINT16 i;
	UINT16 numCapabilities;

	WINPR_ASSERT(rdpdr);

	if (!rdpdr_process_server_header(FALSE, rdpdr->log, s, RDPDR_CTYP_CORE,
	                                 PAKID_CORE_SERVER_CAPABILITY, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, numCapabilities);
	Stream_Seek(s, 2); /* pad (2 bytes) */

	for (i = 0; i < numCapabilities; i++)
	{
		RDPDR_CAPABILITY_HEADER header = { 0 };
		UINT error = rdpdr_read_capset_header(rdpdr->log, s, &header);
		if (error != CHANNEL_RC_OK)
			return error;

		if (header.CapabilityType < ARRAYSIZE(rdpdr->common.capabilityVersions))
		{
			if (rdpdr->common.capabilityVersions[header.CapabilityType] > header.Version)
				rdpdr->common.capabilityVersions[header.CapabilityType] = header.Version;

			WLog_Print(rdpdr->log, WLOG_TRACE,
			           "[%s] capability %s got version %" PRIu32 ", will use version %" PRIu32,
			           __FUNCTION__, rdpdr_cap_type_string(header.CapabilityType), header.Version,
			           rdpdr->common.capabilityVersions[header.CapabilityType]);
		}

		switch (header.CapabilityType)
		{
			case CAP_GENERAL_TYPE:
				status = rdpdr_client_process_general_capset(rdpdr, s, &header);
				break;

			case CAP_PRINTER_TYPE:
				status = rdpdr_process_printer_capset(rdpdr, s, &header);
				break;

			case CAP_PORT_TYPE:
				status = rdpdr_process_port_capset(rdpdr, s, &header);
				break;

			case CAP_DRIVE_TYPE:
				status = rdpdr_process_drive_capset(rdpdr, s, &header);
				break;

			case CAP_SMARTCARD_TYPE:
				status = rdpdr_process_smartcard_capset(rdpdr, s, &header);
				break;

			default:
				WLog_Print(
				    rdpdr->log, WLOG_WARN,
				    "[%s] unknown capability 0x%04" PRIx16 ", length %" PRIu16 ", version %" PRIu32,
				    __FUNCTION__, header.CapabilityType, header.CapabilityLength, header.Version);
				Stream_Seek(s, header.CapabilityLength);
				break;
		}

		if (status != CHANNEL_RC_OK)
			return status;
	}

	return CHANNEL_RC_OK;
}

static BOOL rdpdr_write_general_capset(wLog* log, pf_channel_common_context* rdpdr, wStream* s)
{
	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(s);

	const RDPDR_CAPABILITY_HEADER header = { CAP_GENERAL_TYPE, 44,
		                                     rdpdr->capabilityVersions[CAP_GENERAL_TYPE] };
	if (rdpdr_write_capset_header(log, s, &header) != CHANNEL_RC_OK)
		return FALSE;
	Stream_Write_UINT32(s, 0);                   /* osType, ignored on receipt */
	Stream_Write_UINT32(s, 0);                   /* osVersion, should be ignored */
	Stream_Write_UINT16(s, rdpdr->versionMajor); /* protocolMajorVersion, must be set to 1 */
	Stream_Write_UINT16(s, rdpdr->versionMinor); /* protocolMinorVersion */
	Stream_Write_UINT32(s, 0x0000FFFF);          /* ioCode1 */
	Stream_Write_UINT32(s, 0); /* ioCode2, must be set to zero, reserved for future use */
	Stream_Write_UINT32(s, RDPDR_DEVICE_REMOVE_PDUS | RDPDR_CLIENT_DISPLAY_NAME_PDU |
	                           RDPDR_USER_LOGGEDON_PDU); /* extendedPDU */
	Stream_Write_UINT32(s, ENABLE_ASYNCIO);              /* extraFlags1 */
	Stream_Write_UINT32(s, 0); /* extraFlags2, must be set to zero, reserved for future use */
	Stream_Write_UINT32(s, rdpdr->SpecialDeviceCount); /* SpecialTypeDeviceCap, number of special
	                                                      devices to be redirected before logon */
	return TRUE;
}

static BOOL rdpdr_write_printer_capset(wLog* log, pf_channel_common_context* rdpdr, wStream* s)
{
	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(s);

	const RDPDR_CAPABILITY_HEADER header = { CAP_PRINTER_TYPE, 8,
		                                     rdpdr->capabilityVersions[CAP_PRINTER_TYPE] };
	if (rdpdr_write_capset_header(log, s, &header) != CHANNEL_RC_OK)
		return FALSE;
	return TRUE;
}

static BOOL rdpdr_write_port_capset(wLog* log, pf_channel_common_context* rdpdr, wStream* s)
{
	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(s);

	const RDPDR_CAPABILITY_HEADER header = { CAP_PORT_TYPE, 8,
		                                     rdpdr->capabilityVersions[CAP_PORT_TYPE] };
	if (rdpdr_write_capset_header(log, s, &header) != CHANNEL_RC_OK)
		return FALSE;
	return TRUE;
}

static BOOL rdpdr_write_drive_capset(wLog* log, pf_channel_common_context* rdpdr, wStream* s)
{
	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(s);

	const RDPDR_CAPABILITY_HEADER header = { CAP_DRIVE_TYPE, 8,
		                                     rdpdr->capabilityVersions[CAP_DRIVE_TYPE] };
	if (rdpdr_write_capset_header(log, s, &header) != CHANNEL_RC_OK)
		return FALSE;
	return TRUE;
}

static BOOL rdpdr_write_smartcard_capset(wLog* log, pf_channel_common_context* rdpdr, wStream* s)
{
	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(s);

	const RDPDR_CAPABILITY_HEADER header = { CAP_SMARTCARD_TYPE, 8,
		                                     rdpdr->capabilityVersions[CAP_SMARTCARD_TYPE] };
	if (rdpdr_write_capset_header(log, s, &header) != CHANNEL_RC_OK)
		return FALSE;
	return TRUE;
}

static UINT rdpdr_send_server_capability_request(pf_channel_server_context* rdpdr)
{
	wStream* s =
	    rdpdr_server_get_send_buffer(rdpdr, RDPDR_CTYP_CORE, PAKID_CORE_SERVER_CAPABILITY, 8);
	if (!s)
		return CHANNEL_RC_NO_MEMORY;
	Stream_Write_UINT16(s, 5); /* numCapabilities */
	Stream_Write_UINT16(s, 0); /* pad */
	if (!rdpdr_write_general_capset(rdpdr->log, &rdpdr->common, s))
		return CHANNEL_RC_NO_MEMORY;
	if (!rdpdr_write_printer_capset(rdpdr->log, &rdpdr->common, s))
		return CHANNEL_RC_NO_MEMORY;
	if (!rdpdr_write_port_capset(rdpdr->log, &rdpdr->common, s))
		return CHANNEL_RC_NO_MEMORY;
	if (!rdpdr_write_drive_capset(rdpdr->log, &rdpdr->common, s))
		return CHANNEL_RC_NO_MEMORY;
	if (!rdpdr_write_smartcard_capset(rdpdr->log, &rdpdr->common, s))
		return CHANNEL_RC_NO_MEMORY;
	return rdpdr_seal_send_free_request(rdpdr, s);
}

static UINT rdpdr_process_client_capability_response(pf_channel_server_context* rdpdr, wStream* s)
{
	const UINT16 component = RDPDR_CTYP_CORE;
	const UINT16 packetid = PAKID_CORE_CLIENT_CAPABILITY;
	UINT status = CHANNEL_RC_OK;
	UINT16 numCapabilities, x;
	WINPR_ASSERT(rdpdr);

	if (!rdpdr_process_server_header(TRUE, rdpdr->log, s, component, packetid, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, numCapabilities);
	Stream_Seek_UINT16(s); /* padding */

	for (x = 0; x < numCapabilities; x++)
	{
		RDPDR_CAPABILITY_HEADER header = { 0 };
		UINT error = rdpdr_read_capset_header(rdpdr->log, s, &header);
		if (error != CHANNEL_RC_OK)
			return error;
		if (header.CapabilityType < ARRAYSIZE(rdpdr->common.capabilityVersions))
		{
			if (rdpdr->common.capabilityVersions[header.CapabilityType] > header.Version)
				rdpdr->common.capabilityVersions[header.CapabilityType] = header.Version;

			WLog_Print(rdpdr->log, WLOG_TRACE,
			           "[%s] capability %s got version %" PRIu32 ", will use version %" PRIu32,
			           __FUNCTION__, rdpdr_cap_type_string(header.CapabilityType), header.Version,
			           rdpdr->common.capabilityVersions[header.CapabilityType]);
		}

		switch (header.CapabilityType)
		{
			case CAP_GENERAL_TYPE:
				status = rdpdr_ignore_capset(TRUE, rdpdr->log, s, &header);
				break;

			case CAP_PRINTER_TYPE:
				status = rdpdr_ignore_capset(TRUE, rdpdr->log, s, &header);
				break;

			case CAP_PORT_TYPE:
				status = rdpdr_ignore_capset(TRUE, rdpdr->log, s, &header);
				break;

			case CAP_DRIVE_TYPE:
				status = rdpdr_ignore_capset(TRUE, rdpdr->log, s, &header);
				break;

			case CAP_SMARTCARD_TYPE:
				status = rdpdr_ignore_capset(TRUE, rdpdr->log, s, &header);
				break;

			default:
				SERVER_RX_LOG(rdpdr->log, WLOG_WARN,
				              "[%s | %s] invalid capability type 0x%04" PRIx16,
				              rdpdr_component_string(component), rdpdr_packetid_string(packetid),
				              header.CapabilityType);
				status = ERROR_INVALID_DATA;
				break;
		}

		if (status != CHANNEL_RC_OK)
			break;
	}

	return status;
}

static UINT rdpdr_send_client_capability_response(pClientContext* pc,
                                                  pf_channel_client_context* rdpdr)
{
	wStream* s;

	WINPR_ASSERT(rdpdr);
	s = rdpdr_client_get_send_buffer(rdpdr, RDPDR_CTYP_CORE, PAKID_CORE_CLIENT_CAPABILITY, 4);
	if (!s)
		return CHANNEL_RC_NO_MEMORY;

	Stream_Write_UINT16(s, 5); /* numCapabilities */
	Stream_Write_UINT16(s, 0); /* pad */
	if (!rdpdr_write_general_capset(rdpdr->log, &rdpdr->common, s))
		return CHANNEL_RC_NO_MEMORY;
	if (!rdpdr_write_printer_capset(rdpdr->log, &rdpdr->common, s))
		return CHANNEL_RC_NO_MEMORY;
	if (!rdpdr_write_port_capset(rdpdr->log, &rdpdr->common, s))
		return CHANNEL_RC_NO_MEMORY;
	if (!rdpdr_write_drive_capset(rdpdr->log, &rdpdr->common, s))
		return CHANNEL_RC_NO_MEMORY;
	if (!rdpdr_write_smartcard_capset(rdpdr->log, &rdpdr->common, s))
		return CHANNEL_RC_NO_MEMORY;
	return rdpdr_client_send(rdpdr->log, pc, s);
}

static UINT rdpdr_send_server_clientid_confirm(pf_channel_server_context* rdpdr)
{
	wStream* s;

	s = rdpdr_server_get_send_buffer(rdpdr, RDPDR_CTYP_CORE, PAKID_CORE_CLIENTID_CONFIRM, 8);
	if (!s)
		return CHANNEL_RC_NO_MEMORY;
	Stream_Write_UINT16(s, rdpdr->common.versionMajor);
	Stream_Write_UINT16(s, rdpdr->common.versionMinor);
	Stream_Write_UINT32(s, rdpdr->common.clientID);
	return rdpdr_seal_send_free_request(rdpdr, s);
}

static UINT rdpdr_process_server_clientid_confirm(pf_channel_client_context* rdpdr, wStream* s)
{
	UINT16 versionMajor;
	UINT16 versionMinor;
	UINT32 clientID;

	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(s);

	if (!rdpdr_process_server_header(FALSE, rdpdr->log, s, RDPDR_CTYP_CORE,
	                                 PAKID_CORE_CLIENTID_CONFIRM, 8))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, versionMajor);
	Stream_Read_UINT16(s, versionMinor);
	if (!rdpdr_check_version(FALSE, rdpdr->log, versionMajor, versionMinor, RDPDR_CTYP_CORE,
	                         PAKID_CORE_CLIENTID_CONFIRM))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, clientID);

	if ((versionMajor != rdpdr->common.versionMajor) ||
	    (versionMinor != rdpdr->common.versionMinor))
	{
		CLIENT_RX_LOG(rdpdr->log, WLOG_WARN,
		              "[%s | %s] Version mismatch, sent %" PRIu16 ".%" PRIu16
		              ", downgraded to %" PRIu16 ".%" PRIu16,
		              rdpdr_component_string(RDPDR_CTYP_CORE),
		              rdpdr_packetid_string(PAKID_CORE_CLIENTID_CONFIRM),
		              rdpdr->common.versionMajor, rdpdr->common.versionMinor, versionMajor,
		              versionMinor);
		rdpdr->common.versionMajor = versionMajor;
		rdpdr->common.versionMinor = versionMinor;
	}

	if (clientID != rdpdr->common.clientID)
	{
		CLIENT_RX_LOG(rdpdr->log, WLOG_WARN,
		              "[%s | %s] clientID mismatch, sent 0x%08" PRIx32 ", changed to 0x%08" PRIx32,
		              rdpdr_component_string(RDPDR_CTYP_CORE),
		              rdpdr_packetid_string(PAKID_CORE_CLIENTID_CONFIRM), rdpdr->common.clientID,
		              clientID);
		rdpdr->common.clientID = clientID;
	}

	return CHANNEL_RC_OK;
}

static BOOL
rdpdr_process_server_capability_request_or_clientid_confirm(pf_channel_client_context* rdpdr,
                                                            wStream* s)
{
	const UINT32 mask = STATE_CLIENT_EXPECT_SERVER_CLIENT_ID_CONFIRM |
	                    STATE_CLIENT_EXPECT_SERVER_CORE_CAPABILITY_REQUEST;
	const UINT16 rcomponent = RDPDR_CTYP_CORE;
	UINT16 component, packetid;

	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(s);

	if ((rdpdr->flags & mask) == mask)
	{
		CLIENT_RX_LOG(rdpdr->log, WLOG_WARN, "[%s]: already past this state, abort!", __FUNCTION__);
		return FALSE;
	}

	if (!Stream_CheckAndLogRequiredLengthClient(rdpdr->log, s, 4))
		return FALSE;

	Stream_Read_UINT16(s, component);
	if (rcomponent != component)
	{
		CLIENT_RX_LOG(rdpdr->log, WLOG_WARN, "[%s]: got component %s, expected %s", __FUNCTION__,
		              rdpdr_component_string(component), rdpdr_component_string(rcomponent));
		return FALSE;
	}
	Stream_Read_UINT16(s, packetid);
	Stream_Rewind(s, 4);

	switch (packetid)
	{
		case PAKID_CORE_SERVER_CAPABILITY:
			if (rdpdr->flags & STATE_CLIENT_EXPECT_SERVER_CORE_CAPABILITY_REQUEST)
			{
				CLIENT_RX_LOG(rdpdr->log, WLOG_WARN, "[%s]: got duplicate packetid %s",
				              __FUNCTION__, rdpdr_packetid_string(packetid));
				return FALSE;
			}
			rdpdr->flags |= STATE_CLIENT_EXPECT_SERVER_CORE_CAPABILITY_REQUEST;
			return rdpdr_process_server_core_capability_request(rdpdr, s) == CHANNEL_RC_OK;
		case PAKID_CORE_CLIENTID_CONFIRM:
		default:
			if (rdpdr->flags & STATE_CLIENT_EXPECT_SERVER_CLIENT_ID_CONFIRM)
			{
				CLIENT_RX_LOG(rdpdr->log, WLOG_WARN, "[%s]: got duplicate packetid %s",
				              __FUNCTION__, rdpdr_packetid_string(packetid));
				return FALSE;
			}
			rdpdr->flags |= STATE_CLIENT_EXPECT_SERVER_CLIENT_ID_CONFIRM;
			return rdpdr_process_server_clientid_confirm(rdpdr, s) == CHANNEL_RC_OK;
	}
}

#if defined(WITH_PROXY_EMULATE_SMARTCARD)
static UINT rdpdr_send_emulated_scard_device_list_announce_request(pClientContext* pc,
                                                                   pf_channel_client_context* rdpdr)
{
	wStream* s;

	s = rdpdr_client_get_send_buffer(rdpdr, RDPDR_CTYP_CORE, PAKID_CORE_DEVICELIST_ANNOUNCE, 24);
	if (!s)
		return CHANNEL_RC_NO_MEMORY;

	Stream_Write_UINT32(s, 1);                    /* deviceCount -> our emulated smartcard only */
	Stream_Write_UINT32(s, RDPDR_DTYP_SMARTCARD); /* deviceType */
	Stream_Write_UINT32(
	    s, SCARD_DEVICE_ID); /* deviceID -> reserve highest value for the emulated smartcard */
	Stream_Write(s, "SCARD\0\0\0", 8);
	Stream_Write_UINT32(s, 6);
	Stream_Write(s, "SCARD\0", 6);

	return rdpdr_client_send(rdpdr->log, pc, s);
}

static UINT rdpdr_send_emulated_scard_device_remove(pClientContext* pc,
                                                    pf_channel_client_context* rdpdr)
{
	wStream* s;

	s = rdpdr_client_get_send_buffer(rdpdr, RDPDR_CTYP_CORE, PAKID_CORE_DEVICELIST_REMOVE, 24);
	if (!s)
		return CHANNEL_RC_NO_MEMORY;

	Stream_Write_UINT32(s, 1); /* deviceCount -> our emulated smartcard only */
	Stream_Write_UINT32(
	    s, SCARD_DEVICE_ID); /* deviceID -> reserve highest value for the emulated smartcard */

	return rdpdr_client_send(rdpdr->log, pc, s);
}

static UINT rdpdr_process_server_device_announce_response(pf_channel_client_context* rdpdr,
                                                          wStream* s)
{
	const UINT16 component = RDPDR_CTYP_CORE;
	const UINT16 packetid = PAKID_CORE_DEVICE_REPLY;
	UINT32 deviceID;
	UINT32 resultCode;

	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(s);

	if (!rdpdr_process_server_header(TRUE, rdpdr->log, s, component, packetid, 8))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, deviceID);
	Stream_Read_UINT32(s, resultCode);

	if (deviceID != SCARD_DEVICE_ID)
	{
		CLIENT_RX_LOG(rdpdr->log, WLOG_WARN,
		              "[%s | %s] deviceID mismatch, sent 0x%08" PRIx32 ", changed to 0x%08" PRIx32,
		              rdpdr_component_string(component), rdpdr_packetid_string(packetid),
		              SCARD_DEVICE_ID, deviceID);
	}
	else if (resultCode != 0)
	{
		CLIENT_RX_LOG(rdpdr->log, WLOG_WARN,
		              "[%s | %s] deviceID 0x%08" PRIx32 " resultCode=0x%08" PRIx32,
		              rdpdr_component_string(component), rdpdr_packetid_string(packetid), deviceID,
		              resultCode);
	}
	else
		CLIENT_RX_LOG(rdpdr->log, WLOG_DEBUG,
		              "[%s | %s] deviceID 0x%08" PRIx32 " resultCode=0x%08" PRIx32
		              " -> emulated smartcard redirected!",
		              rdpdr_component_string(component), rdpdr_packetid_string(packetid), deviceID,
		              resultCode);

	return CHANNEL_RC_OK;
}
#endif

static BOOL pf_channel_rdpdr_rewrite_device_list_to(wStream* s, UINT32 fromVersion,
                                                    UINT32 toVersion)
{
	BOOL rc = FALSE;
	if (fromVersion == toVersion)
	{
		Stream_SealLength(s);
		return TRUE;
	}

	const size_t cap = Stream_GetRemainingLength(s);
	wStream* clone = Stream_New(NULL, cap);
	if (!clone)
		goto fail;
	const size_t pos = Stream_GetPosition(s);
	Stream_Copy(s, clone, cap);
	Stream_SealLength(clone);

	Stream_SetPosition(clone, 0);
	Stream_SetPosition(s, pos);

	/* Skip device count */
	if (!Stream_SafeSeek(s, 4))
		goto fail;

	UINT32 count = 0;
	if (Stream_GetRemainingLength(clone) < 4)
		goto fail;
	Stream_Read_UINT32(clone, count);

	for (UINT32 x = 0; x < count; x++)
	{
		RdpdrDevice device = { 0 };
		const size_t charCount = ARRAYSIZE(device.PreferredDosName);
		if (Stream_GetRemainingLength(clone) < 20)
			goto fail;

		Stream_Read_UINT32(clone, device.DeviceType);           /* DeviceType (4 bytes) */
		Stream_Read_UINT32(clone, device.DeviceId);             /* DeviceId (4 bytes) */
		Stream_Read(clone, device.PreferredDosName, charCount); /* PreferredDosName (8 bytes) */
		Stream_Read_UINT32(clone, device.DeviceDataLength);     /* DeviceDataLength (4 bytes) */
		device.DeviceData = Stream_Pointer(clone);
		if (!Stream_SafeSeek(clone, device.DeviceDataLength))
			goto fail;

		if (!Stream_EnsureRemainingCapacity(s, 20))
			goto fail;
		Stream_Write_UINT32(s, device.DeviceType);
		Stream_Write_UINT32(s, device.DeviceId);
		Stream_Write(s, device.PreferredDosName, charCount);

		if (device.DeviceType == RDPDR_DTYP_FILESYSTEM)
		{
			if (toVersion == DRIVE_CAPABILITY_VERSION_01)
				Stream_Write_UINT32(s, 0); /* No unicode name */
			else
			{
				const size_t datalen = charCount * sizeof(WCHAR);
				if (!Stream_EnsureRemainingCapacity(s, datalen + sizeof(UINT32)))
					goto fail;
				Stream_Write_UINT32(s, datalen);

				const SSIZE_T rc = Stream_Write_UTF16_String_From_UTF8(
				    s, charCount, device.PreferredDosName, charCount - 1, TRUE);
				if (rc < 0)
					goto fail;
			}
		}
		else
		{
			Stream_Write_UINT32(s, device.DeviceDataLength);
			if (!Stream_EnsureRemainingCapacity(s, device.DeviceDataLength))
				goto fail;
			Stream_Write(s, device.DeviceData, device.DeviceDataLength);
		}
	}

	Stream_SealLength(s);
	rc = TRUE;

fail:
	Stream_Free(clone, TRUE);
	return rc;
}

static BOOL pf_channel_rdpdr_rewrite_device_list(pf_channel_client_context* rdpdr,
                                                 pServerContext* ps, wStream* s, BOOL toServer)
{
	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(ps);

	if (Stream_Length(s) < 4)
		return FALSE;

	UINT16 component, packetid;
	Stream_SetPosition(s, 0);
	Stream_Read_UINT16(s, component);
	Stream_Read_UINT16(s, packetid);
	if ((component != RDPDR_CTYP_CORE) || (packetid != PAKID_CORE_DEVICELIST_ANNOUNCE))
		return TRUE;

	const pf_channel_server_context* srv =
	    HashTable_GetItemValue(ps->interceptContextMap, RDPDR_SVC_CHANNEL_NAME);
	if (!srv)
		return FALSE;
	UINT32 from = srv->common.capabilityVersions[CAP_DRIVE_TYPE];
	UINT32 to = rdpdr->common.capabilityVersions[CAP_DRIVE_TYPE];
	if (toServer)
	{
		from = rdpdr->common.capabilityVersions[CAP_DRIVE_TYPE];
		to = srv->common.capabilityVersions[CAP_DRIVE_TYPE];
	}
	return pf_channel_rdpdr_rewrite_device_list_to(s, from, to);
}

static BOOL pf_channel_rdpdr_client_send_to_server(pf_channel_client_context* rdpdr,
                                                   pServerContext* ps, wStream* s)
{
	WINPR_ASSERT(rdpdr);
	if (ps)
	{
		UINT16 server_channel_id = WTSChannelGetId(ps->context.peer, RDPDR_SVC_CHANNEL_NAME);

		/* Ignore messages for channels that can not be mapped.
		 * The client might not have enabled support for this specific channel,
		 * so just drop the message. */
		if (server_channel_id == 0)
			return TRUE;

		if (!pf_channel_rdpdr_rewrite_device_list(rdpdr, ps, s, TRUE))
			return FALSE;
		size_t len = Stream_Length(s);
		Stream_SetPosition(s, len);
		rdpdr_dump_send_packet(rdpdr->log, WLOG_TRACE, s, proxy_client_tx);
		WINPR_ASSERT(ps->context.peer);
		WINPR_ASSERT(ps->context.peer->SendChannelData);
		return ps->context.peer->SendChannelData(ps->context.peer, server_channel_id,
		                                         Stream_Buffer(s), len);
	}
	return TRUE;
}

static BOOL pf_channel_send_client_queue(pClientContext* pc, pf_channel_client_context* rdpdr);

#if defined(WITH_PROXY_EMULATE_SMARTCARD)
static BOOL rdpdr_process_server_loggedon_request(pServerContext* ps, pClientContext* pc,
                                                  pf_channel_client_context* rdpdr, wStream* s,
                                                  UINT16 component, UINT16 packetid)
{
	WLog_DBG(TAG, "[%s | %s]", rdpdr_component_string(component), rdpdr_packetid_string(packetid));
	if (rdpdr_send_emulated_scard_device_remove(pc, rdpdr) != CHANNEL_RC_OK)
		return FALSE;
	if (rdpdr_send_emulated_scard_device_list_announce_request(pc, rdpdr) != CHANNEL_RC_OK)
		return FALSE;
	return pf_channel_rdpdr_client_send_to_server(rdpdr, ps, s);
}

static BOOL filter_smartcard_io_requests(pf_channel_client_context* rdpdr, wStream* s,
                                         UINT16* pPacketid)
{
	BOOL rc = FALSE;
	UINT16 component, packetid;
	UINT32 deviceID = 0;
	size_t pos;

	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(pPacketid);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	pos = Stream_GetPosition(s);
	Stream_Read_UINT16(s, component);
	Stream_Read_UINT16(s, packetid);

	if (Stream_GetRemainingLength(s) >= 4)
		Stream_Read_UINT32(s, deviceID);

	WLog_DBG(TAG, "got: [%s | %s]: [0x%08" PRIx32 "]", rdpdr_component_string(component),
	         rdpdr_packetid_string(packetid), deviceID);

	if (component != RDPDR_CTYP_CORE)
		goto fail;

	switch (packetid)
	{
		case PAKID_CORE_SERVER_ANNOUNCE:
		case PAKID_CORE_CLIENTID_CONFIRM:
		case PAKID_CORE_CLIENT_NAME:
		case PAKID_CORE_DEVICELIST_ANNOUNCE:
		case PAKID_CORE_DEVICELIST_REMOVE:
		case PAKID_CORE_SERVER_CAPABILITY:
		case PAKID_CORE_CLIENT_CAPABILITY:
			WLog_WARN(TAG, "Filtering client -> server message [%s | %s]",
			          rdpdr_component_string(component), rdpdr_packetid_string(packetid));
			*pPacketid = packetid;
			break;
		case PAKID_CORE_USER_LOGGEDON:
			*pPacketid = packetid;
			break;
		case PAKID_CORE_DEVICE_REPLY:
		case PAKID_CORE_DEVICE_IOREQUEST:
			if (deviceID != SCARD_DEVICE_ID)
				goto fail;
			*pPacketid = packetid;
			break;
		default:
			if (deviceID != SCARD_DEVICE_ID)
				goto fail;
			WLog_WARN(TAG, "Got [%s | %s] for deviceID 0x%08" PRIx32 ", TODO: Not handled!",
			          rdpdr_component_string(component), rdpdr_packetid_string(packetid), deviceID);
			goto fail;
	}

	rc = TRUE;

fail:
	Stream_SetPosition(s, pos);
	return rc;
}
#endif

BOOL pf_channel_send_client_queue(pClientContext* pc, pf_channel_client_context* rdpdr)
{
	UINT16 channelId;

	WINPR_ASSERT(pc);
	WINPR_ASSERT(rdpdr);

	if (rdpdr->state != STATE_CLIENT_CHANNEL_RUNNING)
		return FALSE;
	channelId = freerdp_channels_get_id_by_name(pc->context.instance, RDPDR_SVC_CHANNEL_NAME);
	if ((channelId == 0) || (channelId == UINT16_MAX))
		return TRUE;

	Queue_Lock(rdpdr->queue);
	while (Queue_Count(rdpdr->queue) > 0)
	{
		wStream* s = Queue_Dequeue(rdpdr->queue);
		if (!s)
			continue;

		size_t len = Stream_Length(s);
		Stream_SetPosition(s, len);

		rdpdr_dump_send_packet(rdpdr->log, WLOG_TRACE, s, proxy_server_tx " (queue) ");
		WINPR_ASSERT(pc->context.instance->SendChannelData);
		if (!pc->context.instance->SendChannelData(pc->context.instance, channelId,
		                                           Stream_Buffer(s), len))
		{
			CLIENT_TX_LOG(rdpdr->log, WLOG_ERROR, "xxxxxx TODO: Failed to send data!");
		}
	skip:
		Stream_Free(s, TRUE);
	}
	Queue_Unlock(rdpdr->queue);
	return TRUE;
}

static BOOL rdpdr_handle_server_announce_request(pClientContext* pc,
                                                 pf_channel_client_context* rdpdr, wStream* s)
{
	WINPR_ASSERT(pc);
	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(s);

	if (rdpdr_process_server_announce_request(rdpdr, s) != CHANNEL_RC_OK)
		return FALSE;
	if (rdpdr_send_client_announce_reply(pc, rdpdr) != CHANNEL_RC_OK)
		return FALSE;
	if (rdpdr_send_client_name_request(pc, rdpdr) != CHANNEL_RC_OK)
		return FALSE;
	rdpdr->state = STATE_CLIENT_EXPECT_SERVER_CORE_CAPABILITY_REQUEST;
	return TRUE;
}

BOOL pf_channel_rdpdr_client_handle(pClientContext* pc, UINT16 channelId, const char* channel_name,
                                    const BYTE* xdata, size_t xsize, UINT32 flags, size_t totalSize)
{
	pf_channel_client_context* rdpdr;
	pServerContext* ps;
	wStream* s;
#if defined(WITH_PROXY_EMULATE_SMARTCARD)
	UINT16 packetid;
#endif

	WINPR_ASSERT(pc);
	WINPR_ASSERT(pc->pdata);
	WINPR_ASSERT(pc->interceptContextMap);
	WINPR_ASSERT(channel_name);
	WINPR_ASSERT(xdata);

	ps = pc->pdata->ps;

	rdpdr = HashTable_GetItemValue(pc->interceptContextMap, channel_name);
	if (!rdpdr)
	{
		WLog_ERR(TAG, "[%s]: Channel %s [0x%04" PRIx16 "] missing context in interceptContextMap",
		         __FUNCTION__, channel_name, channelId);
		return FALSE;
	}
	s = rdpdr->common.buffer;
	if (flags & CHANNEL_FLAG_FIRST)
		Stream_SetPosition(s, 0);
	if (!Stream_EnsureRemainingCapacity(s, xsize))
	{
		CLIENT_RX_LOG(rdpdr->log, WLOG_ERROR,
		              "[%s]: Channel %s [0x%04" PRIx16 "] not enough memory [need %" PRIuz "]",
		              __FUNCTION__, channel_name, channelId, xsize);
		return FALSE;
	}
	Stream_Write(s, xdata, xsize);
	if ((flags & CHANNEL_FLAG_LAST) == 0)
		return TRUE;

	Stream_SealLength(s);
	Stream_SetPosition(s, 0);
	if (Stream_Length(s) != totalSize)
	{
		CLIENT_RX_LOG(rdpdr->log, WLOG_WARN,
		              "Received invalid %s channel data (server -> proxy), expected %" PRIuz
		              "bytes, got %" PRIuz,
		              channel_name, totalSize, Stream_Length(s));
		return FALSE;
	}

	rdpdr_dump_received_packet(rdpdr->log, WLOG_TRACE, s, proxy_server_rx);
	switch (rdpdr->state)
	{
		case STATE_CLIENT_EXPECT_SERVER_ANNOUNCE_REQUEST:
			if (!rdpdr_handle_server_announce_request(pc, rdpdr, s))
				return FALSE;
			break;
		case STATE_CLIENT_EXPECT_SERVER_CORE_CAPABILITY_REQUEST:
			if (!rdpdr_process_server_capability_request_or_clientid_confirm(rdpdr, s))
				return FALSE;
			rdpdr->state = STATE_CLIENT_EXPECT_SERVER_CLIENT_ID_CONFIRM;
			break;
		case STATE_CLIENT_EXPECT_SERVER_CLIENT_ID_CONFIRM:
			if (!rdpdr_process_server_capability_request_or_clientid_confirm(rdpdr, s))
				return FALSE;
			if (rdpdr_send_client_capability_response(pc, rdpdr) != CHANNEL_RC_OK)
				return FALSE;
#if defined(WITH_PROXY_EMULATE_SMARTCARD)
			if (pf_channel_smartcard_client_emulate(pc))
			{
				if (rdpdr_send_emulated_scard_device_list_announce_request(pc, rdpdr) !=
				    CHANNEL_RC_OK)
					return FALSE;
				rdpdr->state = STATE_CLIENT_CHANNEL_RUNNING;
			}
			else
#endif
			{
				rdpdr->state = STATE_CLIENT_CHANNEL_RUNNING;
				pf_channel_send_client_queue(pc, rdpdr);
			}

			break;
		case STATE_CLIENT_CHANNEL_RUNNING:
#if defined(WITH_PROXY_EMULATE_SMARTCARD)
			if (!pf_channel_smartcard_client_emulate(pc) ||
			    !filter_smartcard_io_requests(rdpdr, s, &packetid))
				return pf_channel_rdpdr_client_send_to_server(rdpdr, ps, s);
			else
			{
				switch (packetid)
				{
					case PAKID_CORE_USER_LOGGEDON:
						return rdpdr_process_server_loggedon_request(ps, pc, rdpdr, s,
						                                             RDPDR_CTYP_CORE, packetid);
					case PAKID_CORE_DEVICE_IOREQUEST:
					{
						wStream* out = rdpdr_client_get_send_buffer(
						    rdpdr, RDPDR_CTYP_CORE, PAKID_CORE_DEVICE_IOCOMPLETION, 0);
						WINPR_ASSERT(out);

						if (!rdpdr_process_server_header(FALSE, rdpdr->log, s, RDPDR_CTYP_CORE,
						                                 PAKID_CORE_DEVICE_IOREQUEST, 20))
							return FALSE;

						if (!pf_channel_smartcard_client_handle(rdpdr->log, pc, s, out,
						                                        rdpdr_client_send))
							return FALSE;
					}
					break;
					case PAKID_CORE_SERVER_ANNOUNCE:
						pf_channel_rdpdr_client_reset(pc);
						if (!rdpdr_handle_server_announce_request(pc, rdpdr, s))
							return FALSE;
						break;
					case PAKID_CORE_SERVER_CAPABILITY:
						rdpdr->state = STATE_CLIENT_EXPECT_SERVER_CORE_CAPABILITY_REQUEST;
						rdpdr->flags = 0;
						return pf_channel_rdpdr_client_handle(pc, channelId, channel_name, xdata,
						                                      xsize, flags, totalSize);
					case PAKID_CORE_DEVICE_REPLY:
						break;
					default:
						CLIENT_RX_LOG(
						    rdpdr->log, WLOG_ERROR,
						    "[%s]: Channel %s [0x%04" PRIx16
						    "] we´ve reached an impossible state %s! [%s] aliens invaded!",
						    __FUNCTION__, channel_name, channelId,
						    rdpdr_client_state_to_string(rdpdr->state),
						    rdpdr_packetid_string(packetid));
						return FALSE;
				}
			}
			break;
#else
			return pf_channel_rdpdr_client_send_to_server(rdpdr, ps, s);
#endif
		default:
			CLIENT_RX_LOG(rdpdr->log, WLOG_ERROR,
			              "[%s]: Channel %s [0x%04" PRIx16
			              "] we´ve reached an impossible state %s! aliens invaded!",
			              __FUNCTION__, channel_name, channelId,
			              rdpdr_client_state_to_string(rdpdr->state));
			return FALSE;
	}

	return TRUE;
}

static void pf_channel_rdpdr_common_context_free(pf_channel_common_context* common)
{
	if (!common)
		return;
	free(common->computerName.v);
	Stream_Free(common->s, TRUE);
	Stream_Free(common->buffer, TRUE);
}

static void pf_channel_rdpdr_client_context_free(InterceptContextMapEntry* base)
{
	pf_channel_client_context* entry = (pf_channel_client_context*)base;
	if (!entry)
		return;

	pf_channel_rdpdr_common_context_free(&entry->common);
	Queue_Free(entry->queue);
	free(entry);
}

static BOOL pf_channel_rdpdr_common_context_new(pf_channel_common_context* common,
                                                void (*fkt)(InterceptContextMapEntry*))
{
	if (!common)
		return FALSE;
	common->base.free = fkt;
	common->s = Stream_New(NULL, 1024);
	if (!common->s)
		return FALSE;
	common->buffer = Stream_New(NULL, 1024);
	if (!common->buffer)
		return FALSE;
	common->computerNameUnicode = 1;
	common->computerName.v = NULL;
	common->versionMajor = RDPDR_VERSION_MAJOR;
	common->versionMinor = RDPDR_VERSION_MINOR_RDP10X;
	common->clientID = SCARD_DEVICE_ID;

	const UINT32 versions[] = { 0,
		                        GENERAL_CAPABILITY_VERSION_02,
		                        PRINT_CAPABILITY_VERSION_01,
		                        PORT_CAPABILITY_VERSION_01,
		                        DRIVE_CAPABILITY_VERSION_02,
		                        SMARTCARD_CAPABILITY_VERSION_01 };

	memcpy(common->capabilityVersions, versions, sizeof(common->capabilityVersions));
	return TRUE;
}

static BOOL pf_channel_rdpdr_client_pass_message(pServerContext* ps, pClientContext* pc,
                                                 UINT16 channelId, const char* channel_name,
                                                 wStream* s)
{
	pf_channel_client_context* rdpdr;

	WINPR_ASSERT(ps);
	WINPR_ASSERT(pc);

	rdpdr = HashTable_GetItemValue(pc->interceptContextMap, channel_name);
	if (!rdpdr)
		return TRUE; /* Ignore data for channels not available on proxy -> server connection */
	WINPR_ASSERT(rdpdr->queue);

	if (!pf_channel_rdpdr_rewrite_device_list(rdpdr, ps, s, FALSE))
		return FALSE;
	if (!Queue_Enqueue(rdpdr->queue, s))
		return FALSE;
	pf_channel_send_client_queue(pc, rdpdr);
	return TRUE;
}

#if defined(WITH_PROXY_EMULATE_SMARTCARD)
static BOOL filter_smartcard_device_list_remove(pf_channel_server_context* rdpdr, wStream* s)
{
	size_t pos;
	UINT32 x, count;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, sizeof(UINT32)))
		return TRUE;
	pos = Stream_GetPosition(s);
	Stream_Read_UINT32(s, count);

	if (count == 0)
		return TRUE;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, count * sizeof(UINT32)))
		return TRUE;

	for (x = 0; x < count; x++)
	{
		UINT32 deviceID;
		BYTE* dst = Stream_Pointer(s);
		Stream_Read_UINT32(s, deviceID);
		if (deviceID == SCARD_DEVICE_ID)
		{
			ArrayList_Remove(rdpdr->blockedDevices, (void*)(size_t)deviceID);

			/* This is the only device, filter it! */
			if (count == 1)
				return TRUE;

			/* Remove this device from the list */
			memmove(dst, Stream_Pointer(s), (count - x - 1) * sizeof(UINT32));

			count--;
			Stream_SetPosition(s, pos);
			Stream_Write_UINT32(s, count);
			return FALSE;
		}
	}

	return FALSE;
}

static BOOL filter_smartcard_device_io_request(pf_channel_server_context* rdpdr, wStream* s)
{
	UINT32 DeviceID;
	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(s);
	Stream_Read_UINT32(s, DeviceID);
	return ArrayList_Contains(rdpdr->blockedDevices, (void*)(size_t)DeviceID);
}

static BOOL filter_smartcard_device_list_announce(pf_channel_server_context* rdpdr, wStream* s)
{
	size_t pos;
	UINT32 x, count;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, sizeof(UINT32)))
		return TRUE;
	pos = Stream_GetPosition(s);
	Stream_Read_UINT32(s, count);

	if (count == 0)
		return TRUE;

	for (x = 0; x < count; x++)
	{
		UINT32 DeviceType;
		UINT32 DeviceId;
		char PreferredDosName[8];
		UINT32 DeviceDataLength;
		BYTE* dst = Stream_Pointer(s);
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 20))
			return TRUE;
		Stream_Read_UINT32(s, DeviceType);
		Stream_Read_UINT32(s, DeviceId);
		Stream_Read(s, PreferredDosName, ARRAYSIZE(PreferredDosName));
		Stream_Read_UINT32(s, DeviceDataLength);
		if (!Stream_SafeSeek(s, DeviceDataLength))
			return TRUE;
		if (DeviceType == RDPDR_DTYP_SMARTCARD)
		{
			ArrayList_Append(rdpdr->blockedDevices, (void*)(size_t)DeviceId);
			if (count == 1)
				return TRUE;

			WLog_INFO(TAG, "Filtering smartcard device 0x%08" PRIx32 "", DeviceId);

			memmove(dst, Stream_Pointer(s), Stream_GetRemainingLength(s));
			Stream_SetPosition(s, pos);
			Stream_Write_UINT32(s, count - 1);
			return FALSE;
		}
	}

	return FALSE;
}

static BOOL filter_smartcard_device_list_announce_request(pf_channel_server_context* rdpdr,
                                                          wStream* s)
{
	BOOL rc = TRUE;
	size_t pos;
	UINT16 component, packetid;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return FALSE;

	pos = Stream_GetPosition(s);

	Stream_Read_UINT16(s, component);
	Stream_Read_UINT16(s, packetid);

	if (component != RDPDR_CTYP_CORE)
		goto fail;

	switch (packetid)
	{
		case PAKID_CORE_DEVICELIST_ANNOUNCE:
			if (filter_smartcard_device_list_announce(rdpdr, s))
				goto fail;
			break;
		case PAKID_CORE_DEVICELIST_REMOVE:
			if (filter_smartcard_device_list_remove(rdpdr, s))
				goto fail;
			break;
		case PAKID_CORE_DEVICE_IOREQUEST:
			if (filter_smartcard_device_io_request(rdpdr, s))
				goto fail;
			break;

		case PAKID_CORE_SERVER_ANNOUNCE:
		case PAKID_CORE_CLIENTID_CONFIRM:
		case PAKID_CORE_CLIENT_NAME:
		case PAKID_CORE_DEVICE_REPLY:
		case PAKID_CORE_SERVER_CAPABILITY:
		case PAKID_CORE_CLIENT_CAPABILITY:
		case PAKID_CORE_USER_LOGGEDON:
			WLog_WARN(TAG, "Filtering client -> server message [%s | %s]",
			          rdpdr_component_string(component), rdpdr_packetid_string(packetid));
			goto fail;
		default:
			break;
	}

	rc = FALSE;
fail:
	Stream_SetPosition(s, pos);
	return rc;
}
#endif

static void* stream_copy(const void* obj)
{
	const wStream* src = obj;
	wStream* dst = Stream_New(NULL, Stream_Capacity(src));
	if (!dst)
		return NULL;
	memcpy(Stream_Buffer(dst), Stream_ConstBuffer(src), Stream_Capacity(dst));
	Stream_SetLength(dst, Stream_Length(src));
	Stream_SetPosition(dst, Stream_GetPosition(src));
	return dst;
}

static void stream_free(void* obj)
{
	wStream* s = obj;
	Stream_Free(s, TRUE);
}

BOOL pf_channel_rdpdr_client_new(pClientContext* pc)
{
	wObject* obj;
	pf_channel_client_context* rdpdr;

	WINPR_ASSERT(pc);
	WINPR_ASSERT(pc->interceptContextMap);

	rdpdr = calloc(1, sizeof(pf_channel_client_context));
	if (!rdpdr)
		return FALSE;
	rdpdr->log = WLog_Get(TAG);
	if (!pf_channel_rdpdr_common_context_new(&rdpdr->common, pf_channel_rdpdr_client_context_free))
		goto fail;

	rdpdr->maxMajorVersion = RDPDR_VERSION_MAJOR;
	rdpdr->maxMinorVersion = RDPDR_VERSION_MINOR_RDP10X;
	rdpdr->state = STATE_CLIENT_EXPECT_SERVER_ANNOUNCE_REQUEST;

	rdpdr->queue = Queue_New(TRUE, 0, 0);
	if (!rdpdr->queue)
		goto fail;
	obj = Queue_Object(rdpdr->queue);
	WINPR_ASSERT(obj);
	obj->fnObjectNew = stream_copy;
	obj->fnObjectFree = stream_free;
	return HashTable_Insert(pc->interceptContextMap, RDPDR_SVC_CHANNEL_NAME, rdpdr);
fail:
	pf_channel_rdpdr_client_context_free(&rdpdr->common.base);
	return FALSE;
}

void pf_channel_rdpdr_client_free(pClientContext* pc)
{
	WINPR_ASSERT(pc);
	WINPR_ASSERT(pc->interceptContextMap);
	HashTable_Remove(pc->interceptContextMap, RDPDR_SVC_CHANNEL_NAME);
}

static void pf_channel_rdpdr_server_context_free(InterceptContextMapEntry* base)
{
	pf_channel_server_context* entry = (pf_channel_server_context*)base;
	if (!entry)
		return;

	WTSVirtualChannelClose(entry->handle);
	pf_channel_rdpdr_common_context_free(&entry->common);
	ArrayList_Free(entry->blockedDevices);
	free(entry);
}

BOOL pf_channel_rdpdr_server_new(pServerContext* ps)
{
	pf_channel_server_context* rdpdr;
	PULONG pSessionId = NULL;
	DWORD BytesReturned = 0;

	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->interceptContextMap);

	rdpdr = calloc(1, sizeof(pf_channel_server_context));
	if (!rdpdr)
		return FALSE;
	rdpdr->log = WLog_Get(TAG);
	if (!pf_channel_rdpdr_common_context_new(&rdpdr->common, pf_channel_rdpdr_server_context_free))
		goto fail;
	rdpdr->state = STATE_SERVER_INITIAL;

	rdpdr->blockedDevices = ArrayList_New(FALSE);
	if (!rdpdr->blockedDevices)
		goto fail;

	rdpdr->SessionId = WTS_CURRENT_SESSION;
	if (WTSQuerySessionInformationA(ps->vcm, WTS_CURRENT_SESSION, WTSSessionId, (LPSTR*)&pSessionId,
	                                &BytesReturned))
	{
		rdpdr->SessionId = (DWORD)*pSessionId;
		WTSFreeMemory(pSessionId);
	}

	rdpdr->handle = WTSVirtualChannelOpenEx(rdpdr->SessionId, RDPDR_SVC_CHANNEL_NAME, 0);
	if (rdpdr->handle == 0)
		goto fail;
	return HashTable_Insert(ps->interceptContextMap, RDPDR_SVC_CHANNEL_NAME, rdpdr);
fail:
	pf_channel_rdpdr_server_context_free(&rdpdr->common.base);
	return FALSE;
}

void pf_channel_rdpdr_server_free(pServerContext* ps)
{
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->interceptContextMap);
	HashTable_Remove(ps->interceptContextMap, RDPDR_SVC_CHANNEL_NAME);
}

static pf_channel_server_context* get_channel(pServerContext* ps, BOOL send)
{
	pf_channel_server_context* rdpdr;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->interceptContextMap);

	rdpdr = HashTable_GetItemValue(ps->interceptContextMap, RDPDR_SVC_CHANNEL_NAME);
	if (!rdpdr)
	{
		SERVER_RXTX_LOG(send, rdpdr->log, WLOG_ERROR,
		                "[%s]: Channel %s missing context in interceptContextMap", __FUNCTION__,
		                RDPDR_SVC_CHANNEL_NAME);
		return NULL;
	}

	return rdpdr;
}

BOOL pf_channel_rdpdr_server_handle(pServerContext* ps, UINT16 channelId, const char* channel_name,
                                    const BYTE* xdata, size_t xsize, UINT32 flags, size_t totalSize)
{
	wStream* s;
	pClientContext* pc;
	pf_channel_server_context* rdpdr = get_channel(ps, FALSE);
	if (!rdpdr)
		return FALSE;

	WINPR_ASSERT(ps->pdata);
	pc = ps->pdata->pc;

	s = rdpdr->common.buffer;

	if (flags & CHANNEL_FLAG_FIRST)
		Stream_SetPosition(s, 0);

	if (!Stream_EnsureRemainingCapacity(s, xsize))
		return FALSE;
	Stream_Write(s, xdata, xsize);

	if ((flags & CHANNEL_FLAG_LAST) == 0)
		return TRUE;

	Stream_SealLength(s);
	Stream_SetPosition(s, 0);

	if (Stream_Length(s) != totalSize)
	{
		SERVER_RX_LOG(rdpdr->log, WLOG_WARN,
		              "Received invalid %s channel data (client -> proxy), expected %" PRIuz
		              "bytes, got %" PRIuz,
		              channel_name, totalSize, Stream_Length(s));
		return FALSE;
	}

	rdpdr_dump_received_packet(rdpdr->log, WLOG_TRACE, s, proxy_client_rx);
	switch (rdpdr->state)
	{
		case STATE_SERVER_EXPECT_CLIENT_ANNOUNCE_REPLY:
			if (rdpdr_process_client_announce_reply(rdpdr, s) != CHANNEL_RC_OK)
				return FALSE;
			rdpdr->state = STATE_SERVER_EXPECT_CLIENT_NAME_REQUEST;
			break;
		case STATE_SERVER_EXPECT_CLIENT_NAME_REQUEST:
			if (rdpdr_process_client_name_request(rdpdr, s, pc) != CHANNEL_RC_OK)
				return FALSE;
			if (rdpdr_send_server_capability_request(rdpdr) != CHANNEL_RC_OK)
				return FALSE;
			if (rdpdr_send_server_clientid_confirm(rdpdr) != CHANNEL_RC_OK)
				return FALSE;
			rdpdr->state = STATE_SERVER_EXPECT_EXPECT_CLIENT_CAPABILITY_RESPONE;
			break;
		case STATE_SERVER_EXPECT_EXPECT_CLIENT_CAPABILITY_RESPONE:
			if (rdpdr_process_client_capability_response(rdpdr, s) != CHANNEL_RC_OK)
				return FALSE;
			rdpdr->state = STATE_SERVER_CHANNEL_RUNNING;
			break;
		case STATE_SERVER_CHANNEL_RUNNING:
#if defined(WITH_PROXY_EMULATE_SMARTCARD)
			if (!pf_channel_smartcard_client_emulate(pc) ||
			    !filter_smartcard_device_list_announce_request(rdpdr, s))
			{
				if (!pf_channel_rdpdr_client_pass_message(ps, pc, channelId, channel_name, s))
					return FALSE;
			}
			else
				return pf_channel_smartcard_server_handle(ps, s);
#else
			if (!pf_channel_rdpdr_client_pass_message(ps, pc, channelId, channel_name, s))
				return FALSE;
#endif
			break;
		default:
		case STATE_SERVER_INITIAL:
			SERVER_RX_LOG(rdpdr->log, WLOG_WARN, "Invalid state %s",
			              rdpdr_server_state_to_string(rdpdr->state));
			return FALSE;
	}

	return TRUE;
}

BOOL pf_channel_rdpdr_server_announce(pServerContext* ps)
{
	pf_channel_server_context* rdpdr = get_channel(ps, TRUE);
	if (!rdpdr)
		return FALSE;

	WINPR_ASSERT(rdpdr->state == STATE_SERVER_INITIAL);
	if (rdpdr_server_send_announce_request(rdpdr) != CHANNEL_RC_OK)
		return FALSE;
	rdpdr->state = STATE_SERVER_EXPECT_CLIENT_ANNOUNCE_REPLY;
	return TRUE;
}

BOOL pf_channel_rdpdr_client_reset(pClientContext* pc)
{
	pf_channel_client_context* rdpdr;

	WINPR_ASSERT(pc);
	WINPR_ASSERT(pc->pdata);
	WINPR_ASSERT(pc->interceptContextMap);

	rdpdr = HashTable_GetItemValue(pc->interceptContextMap, RDPDR_SVC_CHANNEL_NAME);
	if (!rdpdr)
		return TRUE;

	Queue_Clear(rdpdr->queue);
	rdpdr->flags = 0;
	rdpdr->state = STATE_CLIENT_EXPECT_SERVER_ANNOUNCE_REQUEST;

	return TRUE;
}

static PfChannelResult pf_rdpdr_back_data(proxyData* pdata,
                                          const pServerStaticChannelContext* channel,
                                          const BYTE* xdata, size_t xsize, UINT32 flags,
                                          size_t totalSize)
{
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(channel);

	if (!pf_channel_rdpdr_client_handle(pdata->pc, channel->back_channel_id, channel->channel_name,
	                                    xdata, xsize, flags, totalSize))
		return PF_CHANNEL_RESULT_ERROR;

#if defined(WITH_PROXY_EMULATE_SMARTCARD)
	if (pf_channel_smartcard_client_emulate(pdata->pc))
		return PF_CHANNEL_RESULT_DROP;
#endif
	return PF_CHANNEL_RESULT_DROP;
}

static PfChannelResult pf_rdpdr_front_data(proxyData* pdata,
                                           const pServerStaticChannelContext* channel,
                                           const BYTE* xdata, size_t xsize, UINT32 flags,
                                           size_t totalSize)
{
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(channel);

	if (!pf_channel_rdpdr_server_handle(pdata->ps, channel->front_channel_id, channel->channel_name,
	                                    xdata, xsize, flags, totalSize))
		return PF_CHANNEL_RESULT_ERROR;

#if defined(WITH_PROXY_EMULATE_SMARTCARD)
	if (pf_channel_smartcard_client_emulate(pdata->pc))
		return PF_CHANNEL_RESULT_DROP;
#endif
	return PF_CHANNEL_RESULT_DROP;
}

BOOL pf_channel_setup_rdpdr(pServerContext* ps, pServerStaticChannelContext* channel)
{
	channel->onBackData = pf_rdpdr_back_data;
	channel->onFrontData = pf_rdpdr_front_data;

	if (!pf_channel_rdpdr_server_new(ps))
		return FALSE;
	if (!pf_channel_rdpdr_server_announce(ps))
		return FALSE;

	return TRUE;
}
