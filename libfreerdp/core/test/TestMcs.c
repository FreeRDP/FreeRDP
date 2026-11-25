/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MCS unit test
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

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/peer.h>
#include <freerdp/transport_io.h>

#include "../mcs.h"
#include "../rdp.h"

#define TPKT_X224_HEADER_LENGTH 7
#define DISCONNECT_PROVIDER_ULTIMATUM_LENGTH 9

static BYTE captured[DISCONNECT_PROVIDER_ULTIMATUM_LENGTH] = WINPR_C_ARRAY_INIT;
static size_t capturedLength = 0;

static int capture_write_pdu(WINPR_ATTR_UNUSED rdpTransport* transport, wStream* s)
{
	const size_t length = Stream_GetPosition(s);
	if (length > sizeof(captured))
		return -1;

	memcpy(captured, Stream_Buffer(s), length);
	capturedLength = length;
	return (int)length;
}

/* [MS-RDPBCGR] 2.2.2.3 gives 21 80 for rn-user-requested (3), the other values
 * follow from the aligned PER encoding of the 3 bit reason. */
static BOOL test_disconnect_provider_ultimatum(rdpMcs* mcs, enum Disconnect_Ultimatum reason,
                                               BYTE b1, BYTE b2)
{
	capturedLength = 0;
	if (!mcs_send_disconnect_provider_ultimatum(mcs, reason))
	{
		(void)fprintf(stderr, "failed to send reason %d\n", reason);
		return FALSE;
	}

	if (capturedLength != DISCONNECT_PROVIDER_ULTIMATUM_LENGTH)
	{
		(void)fprintf(stderr, "reason %d: unexpected length %" PRIuz "\n", reason, capturedLength);
		return FALSE;
	}

	if ((captured[TPKT_X224_HEADER_LENGTH] != b1) || (captured[TPKT_X224_HEADER_LENGTH + 1] != b2))
	{
		(void)fprintf(stderr,
		              "reason %d: got %02" PRIx8 " %02" PRIx8 ", expected %02" PRIx8 " %02" PRIx8
		              "\n",
		              reason, captured[TPKT_X224_HEADER_LENGTH],
		              captured[TPKT_X224_HEADER_LENGTH + 1], b1, b2);
		return FALSE;
	}

	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticConstInit(&sbuffer, captured, capturedLength);
	if (!Stream_SetPosition(s, TPKT_X224_HEADER_LENGTH + 1))
		return FALSE;

	int decoded = -1;
	if (!mcs_recv_disconnect_provider_ultimatum(mcs, s, &decoded))
	{
		(void)fprintf(stderr, "failed to receive reason %d\n", reason);
		return FALSE;
	}

	if (decoded != (int)reason)
	{
		(void)fprintf(stderr, "reason %d decoded as %d\n", reason, decoded);
		return FALSE;
	}

	return TRUE;
}

int TestMcs(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char* argv[])
{
	int rc = -1;

	freerdp_peer* client = calloc(1, sizeof(freerdp_peer));
	if (!client)
		return -1;

	client->ContextSize = sizeof(rdpContext);
	if (!freerdp_peer_context_new(client))
		goto fail;

	{
		const rdpTransportIo* io = freerdp_get_io_callbacks(client->context);
		if (!io)
			goto fail;

		rdpTransportIo capture = *io;
		capture.WritePdu = capture_write_pdu;
		if (!freerdp_set_io_callbacks(client->context, &capture))
			goto fail;
	}

	{
		rdpMcs* mcs = client->context->rdp->mcs;
		if (!test_disconnect_provider_ultimatum(mcs, Disconnect_Ultimatum_domain_disconnected, 0x20,
		                                        0x00))
			goto fail;
		if (!test_disconnect_provider_ultimatum(mcs, Disconnect_Ultimatum_provider_initiated, 0x20,
		                                        0x80))
			goto fail;
		if (!test_disconnect_provider_ultimatum(mcs, Disconnect_Ultimatum_token_purged, 0x21, 0x00))
			goto fail;
		if (!test_disconnect_provider_ultimatum(mcs, Disconnect_Ultimatum_user_requested, 0x21,
		                                        0x80))
			goto fail;
		if (!test_disconnect_provider_ultimatum(mcs, Disconnect_Ultimatum_channel_purged, 0x22,
		                                        0x00))
			goto fail;
	}

	rc = 0;
fail:
	freerdp_peer_context_free(client);
	free(client);
	return rc;
}
