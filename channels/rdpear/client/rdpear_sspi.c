/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Authentication redirection virtual channel, native Windows implementation
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
#include <winpr/wtypes.h>
#include <winpr/crt.h>
#include <winpr/wlog.h>
#include <winpr/stream.h>
#include <winpr/endian.h>
#include <winpr/secapi.h>

#include <freerdp/freerdp.h>
#include <freerdp/channels/log.h>
#include <freerdp/channels/rdpear.h>
#include <freerdp/client/channels.h>
#include <freerdp/transport_io.h>

#define TAG CHANNELS_TAG("rdpear.client")

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif

/* [MS-RDPEAR] 2.2.1.1 TSRemoteGuardPacket header: ProtocolMagic, Length,
 * Version, Reserved, TsPkgContext */
#define RDPEAR_PACKET_HEADER_LENGTH 24
#define RDPEAR_PACKET_CONTEXT_OFFSET 16

/* The Windows client does not decode the redirected calls. It passes each
 * packet to the TSSSP LSA package, which processes it for the TSSSP context of
 * the connection (see libfreerdp/core/tsssp.c). */
typedef struct
{
	GENERIC_DYNVC_PLUGIN base;
	rdpContext* rdp_context;
	HANDLE hLsa;
	ULONG authPackage;
	BOOL lsaConnected;
} RDPEAR_PLUGIN;

/**
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpear_on_open(IWTSVirtualChannelCallback* pChannelCallback)
{
	GENERIC_CHANNEL_CALLBACK* callback = (GENERIC_CHANNEL_CALLBACK*)pChannelCallback;
	WINPR_ASSERT(callback);

	RDPEAR_PLUGIN* rdpear = (RDPEAR_PLUGIN*)callback->plugin;
	WINPR_ASSERT(rdpear);

	NTSTATUS res = LsaConnectUntrusted(&rdpear->hLsa);
	if (res != STATUS_SUCCESS)
	{
		WLog_ERR(TAG, "LsaConnectUntrusted failed: 0x%08" PRIx32, (UINT32)res);
		return LsaNtStatusToWinError(res);
	}
	rdpear->lsaConnected = TRUE;

	CHAR name[] = "TSSSP";
	LSA_STRING packageName = WINPR_C_ARRAY_INIT;
	packageName.Buffer = name;
	packageName.Length = (USHORT)strlen(name);
	packageName.MaximumLength = (USHORT)sizeof(name);

	res = LsaLookupAuthenticationPackage(rdpear->hLsa, &packageName, &rdpear->authPackage);
	if (res != STATUS_SUCCESS)
	{
		WLog_ERR(TAG, "LsaLookupAuthenticationPackage(TSSSP) failed: 0x%08" PRIx32, (UINT32)res);
		(void)LsaDeregisterLogonProcess(rdpear->hLsa);
		rdpear->lsaConnected = FALSE;
		return LsaNtStatusToWinError(res);
	}

	return CHANNEL_RC_OK;
}

/**
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpear_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	GENERIC_CHANNEL_CALLBACK* callback = (GENERIC_CHANNEL_CALLBACK*)pChannelCallback;
	WINPR_ASSERT(callback);

	RDPEAR_PLUGIN* rdpear = (RDPEAR_PLUGIN*)callback->plugin;
	WINPR_ASSERT(rdpear);

	if (rdpear->lsaConnected)
	{
		(void)LsaDeregisterLogonProcess(rdpear->hLsa);
		rdpear->lsaConnected = FALSE;
	}

	return CHANNEL_RC_OK;
}

/**
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpear_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream* s)
{
	GENERIC_CHANNEL_CALLBACK* callback = (GENERIC_CHANNEL_CALLBACK*)pChannelCallback;
	WINPR_ASSERT(callback);

	RDPEAR_PLUGIN* rdpear = (RDPEAR_PLUGIN*)callback->plugin;
	WINPR_ASSERT(rdpear);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, RDPEAR_PACKET_HEADER_LENGTH))
		return ERROR_INVALID_DATA;

	UINT32 magic = 0;
	Stream_Peek_UINT32(s, magic);
	if (magic != RDPEAR_PROTOCOL_MAGIC)
	{
		WLog_ERR(TAG, "invalid ProtocolMagic 0x%08" PRIx32, magic);
		return ERROR_INVALID_DATA;
	}

	const size_t length = Stream_GetRemainingLength(s);
	if (length > UINT32_MAX)
		return ERROR_INVALID_DATA;

	/* TsPkgContext is zero on the wire; the LSA needs our TSSSP context in it. */
	UINT64 context = 0;
	if (!freerdp_tsssp_get_context(rdpear->rdp_context, &context))
	{
		WLog_ERR(TAG, "no TSSSP context, Remote Credential Guard is not active");
		return ERROR_INVALID_STATE;
	}

	BYTE* packet = malloc(length);
	if (!packet)
		return CHANNEL_RC_NO_MEMORY;
	memcpy(packet, Stream_ConstPointer(s), length);
	winpr_Data_Write_UINT64(&packet[RDPEAR_PACKET_CONTEXT_OFFSET], context);

	PVOID response = nullptr;
	ULONG responseLength = 0;
	NTSTATUS protocolStatus = 0;
	const NTSTATUS res =
	    LsaCallAuthenticationPackage(rdpear->hLsa, rdpear->authPackage, packet, (ULONG)length,
	                                 &response, &responseLength, &protocolStatus);
	free(packet);
	if (res != STATUS_SUCCESS)
	{
		WLog_ERR(TAG, "LsaCallAuthenticationPackage failed: 0x%08" PRIx32, (UINT32)res);
		if (response)
			(void)LsaFreeReturnBuffer(response);
		return LsaNtStatusToWinError(res);
	}

	UINT status = CHANNEL_RC_OK;
	if (response && (responseLength > 0))
	{
		/* TsPkgContext must be zero on the wire in this direction too. */
		if (responseLength >= RDPEAR_PACKET_HEADER_LENGTH)
			winpr_Data_Write_UINT64(&((BYTE*)response)[RDPEAR_PACKET_CONTEXT_OFFSET], 0);
		else
			WLog_WARN(TAG, "short response from the TSSSP package (%" PRIu32 " bytes)",
			          (UINT32)responseLength);

		status = callback->channel->Write(callback->channel, responseLength, response, nullptr);
	}

	if (response)
		(void)LsaFreeReturnBuffer(response);

	return status;
}

static UINT init_plugin_cb(GENERIC_DYNVC_PLUGIN* base, rdpContext* rcontext, rdpSettings* settings)
{
	WINPR_ASSERT(base);
	WINPR_UNUSED(settings);

	RDPEAR_PLUGIN* rdpear = (RDPEAR_PLUGIN*)base;
	rdpear->rdp_context = rcontext;
	return CHANNEL_RC_OK;
}

static const IWTSVirtualChannelCallback rdpear_callbacks = { rdpear_on_data_received,
	                                                         rdpear_on_open, rdpear_on_close,
	                                                         nullptr };

/**
 * @return 0 on success, otherwise a Win32 error code
 */
FREERDP_ENTRY_POINT(UINT rdpear_DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints))
{
	return freerdp_generic_DVCPluginEntry(pEntryPoints, TAG, RDPEAR_DVC_CHANNEL_NAME,
	                                      sizeof(RDPEAR_PLUGIN), sizeof(GENERIC_CHANNEL_CALLBACK),
	                                      &rdpear_callbacks, init_plugin_cb, nullptr);
}
