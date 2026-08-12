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
 * Copyright 2026 Thincast Technologies GmbH
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

#ifndef _WIN32

#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define TEST_SERVER_MAX_ITERATIONS (1 << 20)

static const char test_server_cert_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDCzCCAfOgAwIBAgIUJ4fYw1jTmLQQhnFwryzcBeAok6cwDQYJKoZIhvcNAQEL\n"
    "BQAwFDESMBAGA1UEAwwJZnV6ei10ZXN0MCAXDTI2MDgxMTE2MzE1NloYDzIxMjYw\n"
    "NzE4MTYzMTU2WjAUMRIwEAYDVQQDDAlmdXp6LXRlc3QwggEiMA0GCSqGSIb3DQEB\n"
    "AQUAA4IBDwAwggEKAoIBAQDW3lJNI+ibC7rKUDiC2iqT8fG8L5R03tEsS4fr5uQy\n"
    "iBAFWf1vq8Uyx95QA5sPGpA/LRf7Kg2M1t3B5PWYhG+4VnhNGWF2Zo76wp8W32kX\n"
    "MM65QJ2798AV8+QO+HgH7bcH1UbbNbJFLre4bFIvEo+rUDvOC1P9pYxvnTHOBgzH\n"
    "Y+sEh903YMAPrE9wEAgh2vd1Knl3YWYvbyzMU1mNQ8ZSvCnnR965TS2NN4rXs2Kw\n"
    "dMrjcvrL4e8SiJMEz7A7YqNRHvnWgWv6XCYdFS+EE3LQ5Sgng4DawFv46mLiNP7A\n"
    "QFqm8uTcSmb0OsENRQC2HgdrcnfCZq/lg1jqRmC/q/TdAgMBAAGjUzBRMB0GA1Ud\n"
    "DgQWBBQsvO2OvylJ5rlzvMefx2+WRBwPxDAfBgNVHSMEGDAWgBQsvO2OvylJ5rlz\n"
    "vMefx2+WRBwPxDAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQAW\n"
    "ogjIDaWtMwSwnHxxTmMoKLtNOE+8ZNXTRat903Ro0RGz4F72R+UuI7MEApLn27E0\n"
    "YQYuwR8++5Kc0EKW9inUtMrXFFIEg3FxVcNWPwHZdIm2YsYM7SMEnzgAi58YAwhh\n"
    "PqjV0dxm4N4nQkdTFAEVE9sO7isEsEzC/LQC+qRnKZm2QEsS0bLuGBFnRuipOedN\n"
    "peLAbcWIO5XaStfGEhPGUdCOU5pmqJGx/otYNKHhpS6uKvZAIw26yvaOPq8f72UN\n"
    "QmA9qGipJBjuG7yY626DqUqt5cHUumZ24TENreVgc31nPehb7wx6qPqYIR24uTDy\n"
    "UKRjI3r580PJGTsDj7y1\n"
    "-----END CERTIFICATE-----\n";

static const char test_server_key_pem[] = "-----BEGIN PRIVATE KEY-----\n"
    "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQDW3lJNI+ibC7rK\n"
    "UDiC2iqT8fG8L5R03tEsS4fr5uQyiBAFWf1vq8Uyx95QA5sPGpA/LRf7Kg2M1t3B\n"
    "5PWYhG+4VnhNGWF2Zo76wp8W32kXMM65QJ2798AV8+QO+HgH7bcH1UbbNbJFLre4\n"
    "bFIvEo+rUDvOC1P9pYxvnTHOBgzHY+sEh903YMAPrE9wEAgh2vd1Knl3YWYvbyzM\n"
    "U1mNQ8ZSvCnnR965TS2NN4rXs2KwdMrjcvrL4e8SiJMEz7A7YqNRHvnWgWv6XCYd\n"
    "FS+EE3LQ5Sgng4DawFv46mLiNP7AQFqm8uTcSmb0OsENRQC2HgdrcnfCZq/lg1jq\n"
    "RmC/q/TdAgMBAAECggEAFippqZkMeCAp5RCI/+CzNz9kjWIIKdVJlUz2aNzRCjB0\n"
    "nKS3qxM4fOBW/ACfOJvoKQhFGs0wCCkrR8MPnev9nXHYJ7X4Uq9KTS6SHFkwPWr0\n"
    "zHIQw5EPkQQvsOardT/t24JCNL9xlEb5P253PPFofkcA4GTVRYuUNPhtqKABpfjl\n"
    "lVhwnqwCvRb/R/udWidCTZ5Kp088Fzh2aQ52vK4KDO70PqfR2Jze20qMB9Kj6yCT\n"
    "t6SlKIi7SjQv7CXjlMtiMppwiesls3aeE8MGt8ildDE2Xyw5M8SoUWxPVNzmkJbF\n"
    "YmFauNaJzbE05i7WCm7PpXhZux+SL9bfZrpty8RJmQKBgQD2VHu+2a5hqPqk1f7x\n"
    "BErPqFzHY9ozIY5pXzr0yfIEg0xGsZFm6uGRkfmDd6G67+OytNXexisidu9eaxOL\n"
    "0i+nMbW7iV8v+h7hrmx6PngY7mY/wrkqihWNUyXfEjPxr//hyGH9+Fua09Vg+b1A\n"
    "WBwIyrZTbqtUVMgXH56xvyJfaQKBgQDfTal/uUd/FbgKzNk3YsIhWTvpgfoWZq4p\n"
    "Fc0FO2ZUDGGwzng+yIO7GdAfAiMGxnvDWGTnzIDWgxmrVZzDCsERAY7u5wgQkJvf\n"
    "fx3/ZD7KAGJkfkBcDVQZ5Jdg/3z/VmxNeDwPY8uO3Fd0yv/OLZB4cyns9/GRytWB\n"
    "+FPPcg4vVQKBgQCFEDoQbHKAmtFafabL9y+aYS5NHyldeYD+dszYMsajnXF0trL+\n"
    "z16uThZk6BjbbH6pqHnnb1EZuvmvHVRfsVjAjl/HQHvE5O4NpzU+C8TAYvek9cEk\n"
    "s5bU0tegWqroodQt2RrmIGULi+a2DfIncfEi5q36/8tZMLstko0dI0ykEQKBgBWt\n"
    "Wlj1yYUCvLz/qc6AncvS98fxQC/Qg/OlFCP/4i0ijpE1WeLuYCtXlCaOdIwB1J3g\n"
    "BNujtJYeX+2MAA3HC3r1JcT3VIcXIqqNkoHqX1YIt4R95Q2KlbF1yWQ3KRE4eIcE\n"
    "tv/fdjFGHo9N7Ys8TRwEQfupDiBTCmr1il1G+y2JAoGBAI9wl+3HaLSLZlnOOG+4\n"
    "P5WpaGAXbkWMcLq01Wj6wqhlxRg9jugd9+1DjQ7dcmFWKUEOvyxPvl9wsoBuZMAn\n"
    "EBHFJ1KB5jBtcEkH8HdAhErbIoun294YwsYPgHbBfzuz+B6BSoRFprt0QC8LFQdJ\n"
    "UXcX6dcIPbIz/HHap0MWmHXW\n"
    "-----END PRIVATE KEY-----\n";

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

static BOOL test_server_one(const uint8_t* Data, size_t Size)
{
	int sv[2];

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

	rdpCertificate* cert = freerdp_certificate_new_from_pem(test_server_cert_pem);
	if (!cert)
		goto fail;
	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_RdpServerCertificate, cert, 1))
		goto fail;

	rdpPrivateKey* key = freerdp_key_new_from_pem(test_server_key_pem);
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

		char buffer[256];
		ssize_t rc;
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

#endif /* _WIN32 */

int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	static BOOL init = FALSE;
	if (!init)
	{
		(void)WLog_SetLogLevel(WLog_GetRoot(), WLOG_OFF);
		WTSRegisterWtsApiFunctionTable(FreeRDP_InitWtsApi());
		(void)winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT);
		init = TRUE;
	}
#ifndef _WIN32
	test_server_one(Data, Size);
#else
	WINPR_UNUSED(Data);
	WINPR_UNUSED(Size);
#endif
	return 0;
}
