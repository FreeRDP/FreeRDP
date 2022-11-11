/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MULTITRANSPORT PDUs
 *
 * Copyright 2014 Dell Software <Mike.McDonald@software.dell.com>
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

#include <winpr/assert.h>
#include <freerdp/config.h>
#include <freerdp/log.h>

#include "rdp.h"
#include "multitransport.h"

#define TAG FREERDP_TAG("core.multitransport")

struct rdp_multitransport
{
	rdpRdp* rdp;

	UINT32 requestId;         /* requestId (4 bytes) */
	UINT16 requestedProtocol; /* requestedProtocol (2 bytes) */
	UINT16 reserved;          /* reserved (2 bytes) */
	BYTE securityCookie[16];  /* securityCookie (16 bytes) */
};

static BOOL multitransport_compare(const rdpMultitransport* srv, const rdpMultitransport* client)
{
	BOOL abortOnError = !freerdp_settings_get_bool(srv->rdp->settings, FreeRDP_TransportDumpReplay);
	WINPR_ASSERT(srv);
	WINPR_ASSERT(client);

	if (srv->requestId != client->requestId)
	{
		WLog_WARN(TAG,
		          "Multitransport PDU::requestId mismatch expected 0x08%" PRIx32
		          ", got 0x08%" PRIx32,
		          srv->requestId, client->requestId);
		if (abortOnError)
			return FALSE;
	}

	if (srv->requestedProtocol != client->requestedProtocol)
	{
		WLog_WARN(TAG,
		          "Multitransport PDU::requestedProtocol mismatch expected 0x08%" PRIx32
		          ", got 0x08%" PRIx32,
		          srv->requestedProtocol, client->requestedProtocol);
		if (abortOnError)
			return FALSE;
	}

	if (memcmp(srv->securityCookie, client->securityCookie, sizeof(srv->securityCookie)) != 0)
	{
		WLog_WARN(TAG, "Multitransport PDU::securityCookie mismatch");
		if (abortOnError)
			return FALSE;
	}

	return TRUE;
}

state_run_t multitransport_client_recv_request(rdpMultitransport* multitransport, wStream* s)
{
	WINPR_ASSERT(multitransport);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 24))
		return STATE_RUN_FAILED;

	Stream_Read_UINT32(s, multitransport->requestId);         /* requestId (4 bytes) */
	Stream_Read_UINT16(s, multitransport->requestedProtocol); /* requestedProtocol (2 bytes) */
	Stream_Read_UINT16(s, multitransport->reserved);          /* reserved (2 bytes) */
	Stream_Read(s, multitransport->securityCookie,
	            sizeof(multitransport->securityCookie)); /* securityCookie (16 bytes) */

	return STATE_RUN_SUCCESS;
}

BOOL multitransport_server_send_request(rdpMultitransport* multitransport)
{
	WINPR_ASSERT(multitransport);
	wStream* s = rdp_message_channel_pdu_init(multitransport->rdp);
	if (!s)
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, 24))
	{
		Stream_Release(s);
		return FALSE;
	}

	Stream_Write_UINT32(s, multitransport->requestId);         /* requestId (4 bytes) */
	Stream_Write_UINT16(s, multitransport->requestedProtocol); /* requestedProtocol (2 bytes) */
	Stream_Write_UINT16(s, multitransport->reserved);          /* reserved (2 bytes) */
	Stream_Write(s, multitransport->securityCookie,
	             sizeof(multitransport->securityCookie)); /* securityCookie (16 bytes) */

	return rdp_send_message_channel_pdu(multitransport->rdp, s, SEC_TRANSPORT_REQ);
}

BOOL multitransport_client_send_response(rdpMultitransport* multitransport, HRESULT hr)
{
	WINPR_ASSERT(multitransport);

	wStream* s = rdp_message_channel_pdu_init(multitransport->rdp);
	if (!s)
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, 28))
	{
		Stream_Release(s);
		return FALSE;
	}

	Stream_Write_UINT32(s, multitransport->requestId);         /* requestId (4 bytes) */
	Stream_Write_UINT16(s, multitransport->requestedProtocol); /* requestedProtocol (2 bytes) */
	Stream_Write_UINT16(s, multitransport->reserved);          /* reserved (2 bytes) */
	Stream_Write(s, multitransport->securityCookie,
	             sizeof(multitransport->securityCookie)); /* securityCookie (16 bytes) */
	Stream_Write_UINT32(s, hr);
	return rdp_send_message_channel_pdu(multitransport->rdp, s, SEC_TRANSPORT_REQ);
}

BOOL multitransport_server_recv_response(rdpMultitransport* multitransport, wStream* s, HRESULT* hr)
{
	rdpMultitransport multi = { 0 };

	WINPR_ASSERT(multitransport);
	WINPR_ASSERT(hr);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 28))
		return FALSE;

	Stream_Read_UINT32(s, multi.requestId);         /* requestId (4 bytes) */
	Stream_Read_UINT16(s, multi.requestedProtocol); /* requestedProtocol (2 bytes) */
	Stream_Read_UINT16(s, multi.reserved);          /* reserved (2 bytes) */
	Stream_Read(s, multi.securityCookie,
	            sizeof(multi.securityCookie)); /* securityCookie (16 bytes) */
	Stream_Read_UINT32(s, *hr);
	if (!multitransport_compare(multitransport, &multi))
		return FALSE;
	return TRUE;
}

rdpMultitransport* multitransport_new(rdpRdp* rdp, UINT16 protocol)
{
	rdpMultitransport* multi = calloc(1, sizeof(rdpMultitransport));
	if (multi)
	{
		WINPR_ASSERT(rdp);
		multi->rdp = rdp;
		winpr_RAND(&multi->requestId, sizeof(multi->requestId));
		multi->requestedProtocol = protocol;
		winpr_RAND(&multi->securityCookie, sizeof(multi->securityCookie));
	}
	return multi;
}

void multitransport_free(rdpMultitransport* multitransport)
{
	free(multitransport);
}
