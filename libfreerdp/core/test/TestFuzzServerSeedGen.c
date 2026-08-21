/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Seed generator for the end to end server fuzzer (TestFuzzServer).
 *
 * Runs a real FreeRDP client against the fuzz server
 * configuration over a localhost TCP connection with a capturing
 * relay in between. The captured client->server byte stream is
 * written as seed corpus files that TestFuzzServer can mutate.
 *
 * Usage: TestFuzzServerSeedGen <output-dir> [<post-connect-input>]
 *
 *   <output-dir>          directory to write the seeds to
 *   <post-connect-input>  0 or 1, whether to send keyboard/mouse
 *                         input PDUs after activation (default 1)
 *
 * The fuzz server configuration must stay in sync with
 * TestFuzzServer.c. A build with WITH_CHANNELS=ON is required so
 * the client channel addins (e.g. rdpdr) are available.
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
#include <freerdp/client.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/input.h>
#include <winpr/tools/makecert.h>

#include "../multitransport.h"

#include "TestFuzzServerCerts.h"

#include <winpr/ssl.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/crt.h>
#include <winpr/path.h>

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

struct test_peer_context
{
	rdpContext _p;

	HANDLE vcm;
};
typedef struct test_peer_context testPeerContext;

typedef struct
{
	BYTE* data;
	size_t len;
	size_t cap;
} capture_t;

typedef struct
{
	freerdp_peer* client;
} peer_arg_t;

typedef struct
{
	int client_sock;
	int peer_sock;
	capture_t* capture;
} relay_arg_t;

static void capture_add(capture_t* c, const void* data, size_t size)
{
	WINPR_ASSERT(c);
	WINPR_ASSERT(data || (size == 0));

	if (c->len + size > c->cap)
	{
		size_t ncap = c->cap ? c->cap * 2 : 65536;
		while (ncap < c->len + size)
			ncap *= 2;
		BYTE* nd = realloc(c->data, ncap);
		if (!nd)
		{
			(void)fprintf(stderr, "capture realloc failed\n");
			return;
		}
		c->data = nd;
		c->cap = ncap;
	}
	memcpy(c->data + c->len, data, size);
	c->len += size;
}

static void test_peer_context_free(freerdp_peer* client, rdpContext* ctx)
{
	testPeerContext* context = (testPeerContext*)ctx;
	WINPR_UNUSED(client);
	if (context && context->vcm)
	{
		WTSCloseServer(context->vcm);
		context->vcm = nullptr;
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

/*
 * Configure the server peer exactly like TestFuzzServer.c does so the
 * captured client stream replays against the fuzzer.
 */
static BOOL configure_server(freerdp_peer* client)
{
	WINPR_ASSERT(client);
	BOOL rc = FALSE;

	char* path = getTestCredentialsFilePath();
	char* fcert = getTestCredentialsFileNameFor("crt");
	char* fkey = getTestCredentialsFileNameFor("key");
	if (!path || !fcert || !fkey)
		goto fail;

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

	/* local connection => the server uses the plaintext local peer path */
	client->local = TRUE;
	if (!client->Initialize(client))
		goto fail;

	rc = TRUE;
fail:

	free(path);
	return rc;
}

static DWORD WINAPI peer_thread(LPVOID arg)
{
	freerdp_peer* client = ((peer_arg_t*)arg)->client;
	DWORD error = CHANNEL_RC_OK;

	if (!configure_server(client))
	{
		freerdp_peer_context_free(client);
		freerdp_peer_free(client);
		return 1;
	}

	testPeerContext* context = (testPeerContext*)client->context;
	WINPR_ASSERT(context);

	while (error == CHANNEL_RC_OK)
	{
		HANDLE handles[32] = WINPR_C_ARRAY_INIT;
		DWORD count = 0;
		DWORD tmp = client->GetEventHandles(client, &handles[count], ARRAYSIZE(handles) - count);
		if (tmp == 0)
			break;
		count += tmp;
		handles[count++] = WTSVirtualChannelManagerGetEventHandle(context->vcm);
		DWORD status = WaitForMultipleObjects(count, handles, FALSE, 5000);
		if (status == WAIT_FAILED || status == WAIT_TIMEOUT)
			break;
		if (client->CheckFileDescriptor(client) != TRUE)
			break;
		if (WTSVirtualChannelManagerCheckFileDescriptor(context->vcm) != TRUE)
			break;
	}

	client->Disconnect(client);
	freerdp_peer_context_free(client);
	freerdp_peer_free(client);
	return 0;
}

static void* relay_c2s(void* arg)
{
	relay_arg_t* a = (relay_arg_t*)arg;
	BYTE buf[65536] = WINPR_C_ARRAY_INIT;
	for (;;)
	{
		ssize_t rc = read(a->client_sock, buf, sizeof(buf));
		if (rc <= 0)
			break;
		capture_add(a->capture, buf, (size_t)rc);
		ssize_t w = write(a->peer_sock, buf, (size_t)rc);
		if (w != rc)
			break;
	}
	shutdown(a->peer_sock, SHUT_WR);
	return nullptr;
}

static void* relay_s2c(void* arg)
{
	relay_arg_t* a = (relay_arg_t*)arg;
	BYTE buf[65536] = WINPR_C_ARRAY_INIT;
	for (;;)
	{
		ssize_t rc = read(a->peer_sock, buf, sizeof(buf));
		if (rc <= 0)
			break;
		ssize_t w = write(a->client_sock, buf, (size_t)rc);
		if (w != rc)
			break;
	}
	shutdown(a->client_sock, SHUT_WR);
	return nullptr;
}

typedef struct
{
	int port;
	int send_input;
} client_arg_t;

static DWORD WINAPI client_thread(LPVOID arg)
{
	client_arg_t* a = (client_arg_t*)arg;
	char arg1[64] = WINPR_C_ARRAY_INIT;
	(void)snprintf(arg1, sizeof(arg1), "/v:127.0.0.1:%d", a->port);
	char* argv[] = { "seedgen", arg1, "/sec:rdp", "/cert:ignore", "/rfx", nullptr };
	int argc = 5;

	RDP_CLIENT_ENTRY_POINTS entry = WINPR_C_ARRAY_INIT;
	entry.Size = sizeof(RDP_CLIENT_ENTRY_POINTS);
	entry.Version = RDP_CLIENT_INTERFACE_VERSION;
	entry.ContextSize = sizeof(rdpContext);

	rdpContext* context = freerdp_client_context_new(&entry);
	if (!context)
		return 1;

	if (!freerdp_settings_set_bool(context->settings, FreeRDP_DeactivateClientDecoding, TRUE))
		goto fail;
	if (!freerdp_settings_set_string(context->settings, FreeRDP_Username, "test"))
		goto fail;
	if (!freerdp_settings_set_string(context->settings, FreeRDP_Password, "test"))
		goto fail;
	/* Enable network autodetect so the client announces the MCS message
	 * channel; the server needs it to send the licensing and demand active
	 * PDUs on the correct channel. */
	if (!freerdp_settings_set_bool(context->settings, FreeRDP_NetworkAutoDetect, TRUE))
		goto fail;
	if (!freerdp_settings_set_bool(context->settings, FreeRDP_SupportMultitransport, TRUE))
		goto fail;

	if (freerdp_client_settings_parse_command_line(context->settings, argc, argv, FALSE) < 0)
		goto fail;

	if (!freerdp_connect(context->instance))
	{
		(void)fprintf(stderr, "client connect failed\n");
		return 1;
	}

	if (a->send_input)
	{
		rdpInput* input = context->input;
		WINPR_ASSERT(input);
		for (int i = 0; i < 3; i++)
		{
			freerdp_input_send_synchronize_event(input, 0);
			freerdp_input_send_keyboard_event(input, KBD_FLAGS_DOWN, 0x1e); /* 'a' */
			freerdp_input_send_keyboard_event(input, KBD_FLAGS_RELEASE, 0x1e);
			freerdp_input_send_mouse_event(input, PTR_FLAGS_MOVE, 100 + i * 10, 100);
			freerdp_input_send_mouse_event(input, PTR_FLAGS_BUTTON1, 100 + i * 10, 100);
			Sleep(50);
		}
		Sleep(500);
	}

	freerdp_disconnect(context->instance);
	freerdp_client_context_free(context);
	return 0;
fail:
	return 1;
}

static int write_seed(const char* dir, const char* name, const capture_t* c)
{
	char path[512] = WINPR_C_ARRAY_INIT;
	(void)snprintf(path, sizeof(path), "%s/%s", dir, name);
	FILE* f = fopen(path, "wb");
	if (!f)
		return -1;
	size_t w = fwrite(c->data, 1, c->len, f);
	(void)fclose(f);
	return (w == c->len) ? 0 : -1;
}

int main(int argc, char** argv)
{
	const char* outdir = argc > 1 ? argv[1] : ".";
	int do_input = 1;
	if (argc > 2)
	{
		errno = 0;
		const unsigned long val = strtoul(argv[2], nullptr, 0);
		if ((errno != 0) || (val > INT_MAX))
			return -1;
		do_input = (int)val;
	}

	WTSRegisterWtsApiFunctionTable(FreeRDP_InitWtsApi());
	(void)winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT);

	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	if (lfd < 0)
		return 1;
	int one = 1;
	setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	struct sockaddr_in sa = { 0 };
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sa.sin_port = 0;
	if (bind(lfd, (struct sockaddr*)&sa, sizeof(sa)) != 0)
		return 1;
	if (listen(lfd, 4) != 0)
		return 1;
	socklen_t slen = sizeof(sa);
	getsockname(lfd, (struct sockaddr*)&sa, &slen);
	int port = ntohs(sa.sin_port);
	printf("listening on 127.0.0.1:%d\n", port);

	client_arg_t carg = { port, do_input };
	HANDLE hclient = CreateThread(nullptr, 0, client_thread, &carg, 0, nullptr);

	int cfd = accept(lfd, nullptr, nullptr);
	if (cfd < 0)
		return 1;
	printf("client connected\n");

	int sv[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
		return 1;

	freerdp_peer* client = freerdp_peer_new(sv[0]);
	if (!client)
		return 1;

	capture_t capture = WINPR_C_ARRAY_INIT;
	relay_arg_t ra = { cfd, sv[1], &capture };

	pthread_t t1 = WINPR_C_ARRAY_INIT;
	pthread_create(&t1, nullptr, relay_c2s, &ra);
	pthread_t t2 = WINPR_C_ARRAY_INIT;
	pthread_create(&t2, nullptr, relay_s2c, &ra);

	peer_arg_t pa = { client };
	HANDLE hpeer = CreateThread(nullptr, 0, peer_thread, &pa, 0, nullptr);

	WaitForSingleObject(hclient, 30000);
	/* give the peer a moment to drain */
	Sleep(200);
	close(cfd);
	close(sv[1]);
	WaitForSingleObject(hpeer, 10000);

	CloseHandle(hclient);
	CloseHandle(hpeer);
	close(sv[0]);
	close(lfd);

	printf("captured %zu bytes\n", capture.len);

	if (capture.len == 0)
		return 1;

	/* write full capture as the primary seed */
	write_seed(outdir, "handshake", &capture);

	/* write truncated seeds at phase boundaries to give the fuzzer
	 * incremental starting points */
	static const size_t cuts[] = { 64, 128, 256, 512, 1024 };
	for (size_t i = 0; i < sizeof(cuts) / sizeof(cuts[0]); i++)
	{
		if (capture.len > cuts[i])
		{
			char name[64] = WINPR_C_ARRAY_INIT;
			capture_t t = WINPR_C_ARRAY_INIT;
			t.data = capture.data;
			t.len = cuts[i];
			(void)snprintf(name, sizeof(name), "handshake_cut%zu", cuts[i]);
			write_seed(outdir, name, &t);
		}
	}

	free(capture.data);
	return 0;
}
