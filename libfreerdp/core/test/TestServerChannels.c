/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server side virtual channel unit test
 *
 * Copyright 2026 Sayed Kaif <metsw24@gmail.com>
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
#include <winpr/wtsapi.h>

#include <freerdp/peer.h>
#include <freerdp/constants.h>
#include <freerdp/channels/channels.h>
#include <freerdp/channels/wtsvc.h>

#include "../mcs.h"
#include "../rdp.h"

#define TEST_SVC_CHANNEL_ID 1004

static BOOL recv_pdu(freerdp_peer* client, const BYTE* data, size_t size)
{
	return client->ReceiveChannelData(client, TEST_SVC_CHANNEL_ID, data, size,
	                                  CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST, size);
}

/* A DYNVC PDU that ends before its ChannelId field must be rejected, see
 * [MS-RDPEDYC] 2.2.2. */
static BOOL test_truncated_dynvc_pdu(void)
{
	BOOL rc = FALSE;
	HANDLE vcm = INVALID_HANDLE_VALUE;

	freerdp_peer* client = calloc(1, sizeof(freerdp_peer));
	if (!client)
		return FALSE;

	client->ContextSize = sizeof(rdpContext);
	if (!freerdp_peer_context_new(client))
		goto fail;

	rdpMcs* mcs = client->context->rdp->mcs;
	mcs->channelCount = 1;
	(void)strncpy(mcs->channels[0].Name, DRDYNVC_SVC_CHANNEL_NAME, CHANNEL_NAME_LEN);
	mcs->channels[0].ChannelId = TEST_SVC_CHANNEL_ID;
	mcs->channels[0].joined = TRUE;

	vcm = WTSOpenServerA((LPSTR)client->context);
	if (!vcm || (vcm == INVALID_HANDLE_VALUE))
		goto fail;

	if (!WTSVirtualChannelManagerOpen(vcm))
		goto fail;

	/* DYNVC_CAPS_RSP, moves the drdynvc channel to DRDYNVC_STATE_READY */
	const BYTE capsRsp[] = { 0x50, 0x00, 0x01, 0x00 };
	if (!recv_pdu(client, capsRsp, sizeof(capsRsp)))
		goto fail;

	HANDLE dvc = WTSVirtualChannelOpenEx(1, "testdvc", WTS_CHANNEL_OPTION_DYNAMIC);
	if (!dvc)
		goto fail;

	/* DYNVC_CREATE_RSP for ChannelId 1, HRESULT 0 */
	const BYTE createRsp[] = { 0x10, 0x01, 0x00, 0x00, 0x00, 0x00 };
	if (!recv_pdu(client, createRsp, sizeof(createRsp)))
		goto fail;

	/* Same PDU truncated to its header byte: the ChannelId is no longer part of
	 * the received data. */
	const BYTE truncated[] = { 0x10 };
	if (recv_pdu(client, truncated, sizeof(truncated)))
	{
		(void)fprintf(stderr, "truncated DYNVC PDU accepted\n");
		goto fail;
	}

	rc = TRUE;
fail:
	if (vcm != INVALID_HANDLE_VALUE)
		WTSCloseServer(vcm);
	freerdp_peer_context_free(client);
	free(client);
	return rc;
}

int TestServerChannels(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char* argv[])
{
	WTSRegisterWtsApiFunctionTable(FreeRDP_InitWtsApi());

	if (!test_truncated_dynvc_pdu())
		return -1;

	return 0;
}
