/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * The peer's announced settings: the server's RDP version
 *
 * Copyright 2026 Cong Zhang <congzhangzh@gmail.com>
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

#include <freerdp/freerdp.h>
#include <freerdp/peer.h>

#include "../gcc.h"
#include "../mcs.h"
#include "../rdp.h"

/* The server writes its GCC conference create response with serverVersion, the client reads it
 * with clientVersion: the client keeps the common version, the remote settings the server's. */
static BOOL test_versions(UINT32 serverVersion, UINT32 clientVersion, UINT32 agreed)
{
	BOOL rc = FALSE;
	wStream* userData = nullptr;
	wStream* s = nullptr;
	freerdp* instance = nullptr;

	freerdp_peer* server = calloc(1, sizeof(freerdp_peer));
	if (!server)
		return FALSE;
	server->ContextSize = sizeof(rdpContext);
	if (!freerdp_peer_context_new(server))
		goto fail;
	if (!freerdp_settings_set_uint32(server->context->settings, FreeRDP_RdpVersion, serverVersion))
		goto fail;

	userData = Stream_New(nullptr, 4096);
	s = Stream_New(nullptr, 4096);
	if (!userData || !s)
		goto fail;
	if (!gcc_write_server_data_blocks(userData, server->context->rdp->mcs))
	{
		(void)fprintf(stderr, "gcc_write_server_data_blocks failed\n");
		goto fail;
	}
	if (!gcc_write_conference_create_response(s, userData))
	{
		(void)fprintf(stderr, "gcc_write_conference_create_response failed\n");
		goto fail;
	}
	Stream_SealLength(s);
	Stream_ResetPosition(s);

	instance = freerdp_new();
	if (!instance)
		goto fail;
	instance->ContextSize = sizeof(rdpContext);
	if (!freerdp_context_new(instance))
		goto fail;

	{
		rdpContext* context = instance->context;
		const rdpSettings* remoteSettings = freerdp_get_remote_settings(context);
		if (!remoteSettings)
			goto fail;
		const FreeRDP_Settings_Keys_UInt32 unsent[] = { FreeRDP_RdpVersion, FreeRDP_OsMajorType,
			                                            FreeRDP_OsMinorType };
		for (size_t x = 0; x < ARRAYSIZE(unsent); x++)
		{
			if (freerdp_settings_get_uint32(remoteSettings, unsent[x]) != 0)
			{
				(void)fprintf(stderr, "%s is set before the server sent it\n",
				              freerdp_settings_get_name_for_key(unsent[x]));
				goto fail;
			}
		}
		if (freerdp_settings_get_pointer(remoteSettings, FreeRDP_RdpServerCertificate))
		{
			(void)fprintf(stderr, "RdpServerCertificate is set before the server sent one\n");
			goto fail;
		}
		if (!freerdp_settings_set_uint32(context->settings, FreeRDP_RdpVersion, clientVersion))
			goto fail;
		if (!gcc_read_conference_create_response(s, context->rdp->mcs))
		{
			(void)fprintf(stderr, "gcc_read_conference_create_response failed\n");
			goto fail;
		}

		const UINT32 local = freerdp_settings_get_uint32(context->settings, FreeRDP_RdpVersion);
		const UINT32 remote =
		    freerdp_settings_get_uint32(freerdp_get_remote_settings(context), FreeRDP_RdpVersion);
		if ((local != agreed) || (remote != serverVersion))
		{
			(void)fprintf(stderr,
			              "server 0x%08" PRIx32 ", client 0x%08" PRIx32 ": agreed 0x%08" PRIx32
			              " (expected 0x%08" PRIx32 "), remote 0x%08" PRIx32
			              " (expected the server's)\n",
			              serverVersion, clientVersion, local, agreed, remote);
			goto fail;
		}
	}
	rc = TRUE;

fail:
	if (instance)
		freerdp_context_free(instance);
	freerdp_free(instance);
	Stream_Free(s, TRUE);
	Stream_Free(userData, TRUE);
	freerdp_peer_context_free(server);
	free(server);
	return rc;
}

int TestRemoteSettings(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char* argv[])
{
	if (freerdp_get_remote_settings(nullptr))
		return -1;
	/* a newer server: the client keeps its own version */
	if (!test_versions(RDP_VERSION_10_12, RDP_VERSION_10_7, RDP_VERSION_10_7))
		return -1;
	/* an older server: both sides at the server's version */
	if (!test_versions(RDP_VERSION_5_PLUS, RDP_VERSION_10_12, RDP_VERSION_5_PLUS))
		return -1;
	return 0;
}
