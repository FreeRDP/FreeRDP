/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * End to end fuzzer for the FreeRDP server core.
 *
 * A garbage byte stream is fed into a real server peer over a
 * UNIX domain socketpair. The peer runs through the full server
 * protocol stack (transport, negotiation, MCS, security and PDU
 * parsing) exactly like a server would for a real client
 * connection.
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

#include <freerdp/peer.h>
#include <freerdp/freerdp.h>

#include <freerdp/settings.h>
#include <freerdp/constants.h>
#include <freerdp/crypto/certificate.h>
#include <freerdp/crypto/privatekey.h>

#include <freerdp/channels/wtsvc.h>
#include <freerdp/channels/channels.h>

#include "../multitransport.h"

#include <winpr/assert.h>
#include <winpr/ssl.h>
#include <winpr/wlog.h>

#ifdef _WIN32
#error "WIN32 not supported, remove this file from compile!"
#endif

#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define TEST_SERVER_MAX_ITERATIONS (1 << 20)

#include "TestFuzzServerCerts.h"

static BOOL test_peer_post_connect(freerdp_peer* peer)
{
	WINPR_ASSERT(peer);
	return TRUE;
}

static BOOL test_peer_activate(freerdp_peer* peer)
{
	WINPR_ASSERT(peer);
	return TRUE;
}

static int test_peer_virtual_channel_read(freerdp_peer* peer, HANDLE hChannel, BYTE* buffer,
                                          UINT32 length)
{
	WINPR_UNUSED(peer);
	WINPR_UNUSED(hChannel);
	WINPR_UNUSED(buffer);
	WINPR_UNUSED(length);
	return 0;
}

struct test_peer_context
{
	rdpContext _p;

	HANDLE vcm;
};
typedef struct test_peer_context testPeerContext;

static void test_peer_context_free(freerdp_peer* client, rdpContext* ctx)
{
	testPeerContext* context = (testPeerContext*)ctx;

	WINPR_UNUSED(client);

	if (context && context->vcm)
	{
		WTSCloseServer(context->vcm);
		context->vcm = NULL;
	}
}

static BOOL test_peer_context_new(freerdp_peer* client, rdpContext* ctx)
{
	testPeerContext* context = (testPeerContext*)ctx;

	WINPR_ASSERT(client);
	WINPR_ASSERT(context);

	context->vcm = WTSOpenServerA((LPSTR)client->context);
	if (!context->vcm || context->vcm == INVALID_HANDLE_VALUE)
		goto fail;

	return TRUE;
fail:
	test_peer_context_free(client, ctx);
	return FALSE;
}

static BOOL test_server_one(const char* fkey, const char* fcert, const uint8_t* Data, size_t Size)
{
	int sv[2] = WINPR_C_ARRAY_INIT;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
		return FALSE;

	freerdp_peer* client = freerdp_peer_new(sv[0]);
	if (!client)
	{
		close(sv[0]);
		close(sv[1]);
		return FALSE;
	}

	client->ContextSize = sizeof(testPeerContext);
	client->ContextNew = test_peer_context_new;
	client->ContextFree = test_peer_context_free;
	if (!freerdp_peer_context_new(client))
		goto fail;

	WINPR_ASSERT(client->context);
	rdpSettings* settings = client->context->settings;
	WINPR_ASSERT(settings);

	rdpCertificate* cert = freerdp_certificate_new_from_file(fcert);
	if (!cert)
		goto fail;
	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_RdpServerCertificate, cert, 1))
		goto fail;

	rdpPrivateKey* key = freerdp_key_new_from_file(fkey);
	if (!key)
		goto fail;
	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_RdpServerRsaKey, key, 1))
		goto fail;

	/*
	 * Plain RDP security. Together with the local peer flag below this
	 * results in the plaintext local peer path (no RDP encryption), which
	 * allows the byte stream fuzzer to reach all the way into the post
	 * activation parsing code.
	 */
	if (!freerdp_settings_set_bool(settings, FreeRDP_RdpSecurity, TRUE))
		goto fail;
	if (!freerdp_settings_set_bool(settings, FreeRDP_UseRdpSecurityLayer, TRUE))
		goto fail;
	if (!freerdp_settings_set_bool(settings, FreeRDP_TlsSecurity, FALSE))
		goto fail;
	if (!freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, FALSE))
		goto fail;
	if (!freerdp_settings_set_bool(settings, FreeRDP_ExtSecurity, FALSE))
		goto fail;
	if (!freerdp_settings_set_uint32(settings, FreeRDP_EncryptionLevel, ENCRYPTION_LEVEL_NONE))
		goto fail;

	/*
	 * Widen the negotiated surface: codecs, autodetect, multitransport and
	 * channel handling (no channel join shortcut) so more of the server
	 * parsing code is reachable once a (valid) client connects.
	 */
	if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, TRUE))
		goto fail;
	if (!freerdp_settings_set_bool(settings, FreeRDP_NSCodec, TRUE))
		goto fail;
	if (!freerdp_settings_set_bool(settings, FreeRDP_SupportMultitransport, TRUE))
		goto fail;
	if (!freerdp_settings_set_uint32(settings, FreeRDP_MultitransportFlags,
	                                 INITIATE_REQUEST_PROTOCOL_UDPFECR))
		goto fail;
	if (!freerdp_settings_set_bool(settings, FreeRDP_SupportSkipChannelJoin, FALSE))
		goto fail;

	client->PostConnect = test_peer_post_connect;
	client->Activate = test_peer_activate;
	client->VirtualChannelRead = test_peer_virtual_channel_read;

	/*
	 * The socketpair is a local connection. Mark it as such so the server
	 * uses the plaintext local peer path (no RDP encryption, see nego.c),
	 * which allows the byte stream fuzzer to reach all the way into the post
	 * activation parsing code.
	 */
	client->local = TRUE;
	if (!client->Initialize(client))
		goto fail;

	testPeerContext* context = (testPeerContext*)client->context;
	WINPR_ASSERT(context);

	/* make sure we never block on the client side of the socketpair */
	if (fcntl(sv[1], F_SETFL, O_NONBLOCK) != 0)
		goto fail;

	{
		size_t written = 0;
		while (written < Size)
		{
			const ssize_t rc = write(sv[1], Data + written, Size - written);
			if (rc < 0)
			{
				if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
					break;
				goto fail;
			}
			written += (size_t)rc;
		}
	}
	if (shutdown(sv[1], SHUT_WR) != 0)
		goto fail;

	/*
	 * Drive the server state machine synchronously until it fails or
	 * reaches EOF (the client side of the socketpair is closed after
	 * shutdown). Drain server output so its socket buffer can not fill up.
	 */
	for (size_t i = 0; i < TEST_SERVER_MAX_ITERATIONS; i++)
	{
		if (!client->CheckFileDescriptor(client))
			break;

		if (WTSVirtualChannelManagerCheckFileDescriptor(context->vcm) != TRUE)
			break;

		char buffer[256] = WINPR_C_ARRAY_INIT;
		ssize_t rc = 0;
		do
		{
			rc = read(sv[1], buffer, sizeof(buffer));
		} while (rc > 0);
	}

fail:
	freerdp_peer_context_free(client);
	freerdp_peer_free(client);
	close(sv[1]);
	return TRUE;
}

int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	const char* key = getTestCredentialsFileNameFor("crt");
	const char* cert = getTestCredentialsFileNameFor("key");

	if (key && cert)
	{
		if (WLog_SetLogLevel(WLog_GetRoot(), WLOG_OFF) &&
		    WTSRegisterWtsApiFunctionTable(FreeRDP_InitWtsApi()) &&
		    winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT))
		{
			test_server_one(key, cert, Data, Size);
		}
	}
	free(key);
	free(cert);

	return 0;
}
