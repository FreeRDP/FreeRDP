/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RpcShadow2 stub encoding and decoding
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

#include <string.h>

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/string.h>

#include <freerdp/log.h>

#include "../remote_shadow.h"

#if defined(WITH_LIBSMB2) && !defined(_WIN32)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <winpr/synch.h>
#include <winpr/sysinfo.h>
#include <winpr/thread.h>

#include <freerdp/freerdp.h>
#define TEST_RPC_SILENT_HOST
#endif

#define TAG FREERDP_TAG("test.core.shadow")

static const char invitation[] =
    "<E><A KH=\"a\" ID=\"b\"/><C><T ID=\"1\" SID=\"0\"><L P=\"49152\" N=\"192.0.2.1\"/>"
    "</T></C></E>";

/* RpcShadow2(session 1, take control, silent, cchInvitation 8192) as Windows expects it */
static BOOL test_write_request(void)
{
	const BYTE expected[] = {
		0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00
	};
	BOOL rc = FALSE;
	wStream* s = Stream_New(nullptr, 4);
	if (!s)
		return FALSE;

	if (!remote_shadow_write_shadow2_stub(s, 1, TRUE, TRUE))
		goto out;
	if ((Stream_GetPosition(s) != sizeof(expected)) ||
	    (memcmp(Stream_Buffer(s), expected, sizeof(expected)) != 0))
		goto out;

	/* view only, the session user is asked */
	Stream_ResetPosition(s);
	if (!remote_shadow_write_shadow2_stub(s, 7, FALSE, FALSE))
		goto out;
	{
		const BYTE* b = Stream_Buffer(s);
		rc = (b[0] == 7) && (b[4] == 0) && (b[6] == 1);
	}
out:
	Stream_Free(s, TRUE);
	return rc;
}

/* SHADOW_REQUEST_RESPONSE, conformant varying string, HRESULT */
static wStream* make_response(UINT16 permission, const char* text, UINT32 maxCount, UINT32 offset,
                              UINT32 actualDelta, BOOL truncate)
{
	size_t wlen = 0;
	WCHAR* wtext = ConvertUtf8ToWCharAlloc(text, &wlen);
	wStream* s = Stream_New(nullptr, 64 + 2 * (wlen + 1));
	if (!wtext || !s)
	{
		free(wtext);
		Stream_Free(s, TRUE);
		return nullptr;
	}

	const UINT32 count = (UINT32)wlen + 1; /* including the terminating null */
	Stream_Write_UINT16(s, permission);
	Stream_Write_UINT16(s, 0);
	Stream_Write_UINT32(s, maxCount ? maxCount : count);
	Stream_Write_UINT32(s, offset);
	Stream_Write_UINT32(s, count + actualDelta);
	Stream_Write(s, wtext, sizeof(WCHAR) * count);
	while (Stream_GetPosition(s) % 4)
		Stream_Write_UINT8(s, 0);
	if (!truncate)
		Stream_Write_UINT32(s, 0);
	free(wtext);

	Stream_SealLength(s);
	Stream_ResetPosition(s);
	return s;
}

static BOOL test_read_response(wLog* log)
{
	UINT16 permission = 0xFFFF;
	UINT32 hresult = 0xFFFFFFFF;
	char* text = nullptr;
	BOOL rc = FALSE;

	wStream* s = make_response(0, invitation, 0, 0, 0, FALSE);
	if (!s)
		return FALSE;
	if (!remote_shadow_read_shadow2_stub(log, s, &permission, &hresult, &text))
		goto out;
	rc = (permission == 0) && (hresult == 0) && text && (strcmp(text, invitation) == 0);
out:
	free(text);
	Stream_Free(s, TRUE);
	return rc;
}

static BOOL test_read_declined(wLog* log)
{
	UINT16 permission = 0;
	UINT32 hresult = 0xFFFFFFFF;
	char* text = nullptr;

	/* a declined request returns an empty string */
	wStream* s = make_response(1, "", 0, 0, 0, FALSE);
	if (!s)
		return FALSE;
	const BOOL ok = remote_shadow_read_shadow2_stub(log, s, &permission, &hresult, &text);
	Stream_Free(s, TRUE);
	const BOOL rc = ok && (permission == 1) && (hresult == 0);
	free(text);
	return rc;
}

static BOOL test_read_malformed(wLog* log)
{
	struct
	{
		UINT32 maxCount;
		UINT32 offset;
		UINT32 actualDelta;
		BOOL truncate;
	} cases[] = {
		{ 2, 0, 0, FALSE },      /* actual count larger than max count */
		{ 200, 0, 10, FALSE },   /* actual count within max count, past the end of the data */
		{ 0, 0, 0, TRUE },       /* HRESULT missing */
		{ 100000, 0, 0, FALSE }, /* max count above cchInvitation */
		{ 0, 1, 0, FALSE }       /* non-zero offset */
	};

	for (size_t x = 0; x < ARRAYSIZE(cases); x++)
	{
		UINT16 permission = 0;
		UINT32 hresult = 0;
		char* text = nullptr;
		wStream* s = make_response(0, invitation, cases[x].maxCount, cases[x].offset,
		                           cases[x].actualDelta, cases[x].truncate);
		if (!s)
			return FALSE;
		const BOOL ok = remote_shadow_read_shadow2_stub(log, s, &permission, &hresult, &text);
		Stream_Free(s, TRUE);
		if (ok || text)
		{
			free(text);
			(void)fprintf(stderr, "malformed case %" PRIuz " was accepted\n", x);
			return FALSE;
		}
	}
	return TRUE;
}

#if defined(WITH_LIBSMB2)
/* a throwaway self-signed P-256 certificate (CN=shadow-test), base64 wrapped as Windows sends it */
#define TEST_CERT_BASE64                                                    \
	"MIIBYjCCAQigAwIBAgIUaNDsJKxFdEh7DfMPTAiuYLxSUeEwCgYIKoZIzj0EAwIw&#xA;" \
	"FjEUMBIGA1UEAwwLc2hhZG93LXRlc3QwIBcNMjYxMTIwMjMyMDUyWhgPMjEyNjEw&#xA;" \
	"MjcyMzIwNTJaMBYxFDASBgNVBAMMC3NoYWRvdy10ZXN0MFkwEwYHKoZIzj0CAQYI&#xA;" \
	"KoZIzj0DAQcDQgAEXUgffLP2AGS3Tkp2d7LueQDUt9YvxuyBeYMA3qDqySAvM9a2&#xA;" \
	"7zCZr27nzcM4E/mb7HorQPPrAWJ1ewxIInoAHqMyMDAwHQYDVR0OBBYEFNX7/hzF&#xA;" \
	"IY5ue+1n5Ga8k3HCTu96MA8GA1UdEwEB/wQFMAMBAf8wCgYIKoZIzj0EAwIDSAAw&#xA;" \
	"RQIgQLcWyAaYKMGdpZ7VGVsBX+lxzhe+z0OaOCETfU5xI+4CIQCU/0ix8+YOL6ME&#xA;" \
	"ZAVni1yGUSoi6POa4rxZOpZvHibacw==&#xA;"
#define TEST_CERT_FINGERPRINT \
	"sha256:9afece0414b1ec13ce1ad603d65bd4dab8d1a93793d93c7e9c11d7e69421bd03"

static const char invitation_full[] =
    "<E><A KH=\"x\" ID=\"y\" CE=\"" TEST_CERT_BASE64 "\"/><C><T ID=\"1\" SID=\"1\">"
    "<L P=\"3391\" N=\"fe80::1%13\"/><L P=\"3391\" N=\"FEB0::2%6\"/>"
    "<L P=\"3392\" N=\"192.0.2.10\"/></T></C></E>";
static const char invitation_no_certificate[] =
    "<E><A KH=\"x\" ID=\"y\"/><C><T ID=\"1\" SID=\"1\"><L P=\"3392\" N=\"192.0.2.10\" "
    "CE=\"" TEST_CERT_BASE64 "\"/></T></C></E>";
static const char invitation_link_local_only[] =
    "<E><A KH=\"x\" ID=\"y\"/><C><T ID=\"1\" SID=\"1\"><L P=\"3391\" N=\"fe80::1%13\"/>"
    "</T></C></E>";

static BOOL check_string(const rdpSettings* settings, FreeRDP_Settings_Keys_String key,
                         const char* expected)
{
	const char* value = freerdp_settings_get_string(settings, key);
	if (!expected)
		return !value || !*value;
	return value && (strcmp(value, expected) == 0);
}

/* the invitation points the connection at its listener; the host name stays for the next call */
static BOOL test_apply_invitation(void)
{
	BOOL rc = FALSE;
	rdpSettings* settings = freerdp_settings_new(0);
	if (!settings || !freerdp_settings_set_string(settings, FreeRDP_ServerHostname, "shadow-host"))
		goto out;

	/* link-local listeners (fe80::/10) are dropped, the certificate is pinned once */
	for (size_t round = 0; round < 2; round++)
	{
		if (!remote_shadow_apply_invitation(settings, invitation_full))
			goto out;
		const char* address =
		    freerdp_settings_get_pointer_array(settings, FreeRDP_TargetNetAddresses, 0);
		if ((freerdp_settings_get_uint32(settings, FreeRDP_TargetNetAddressCount) != 1) ||
		    !address || (strcmp(address, "192.0.2.10") != 0) ||
		    (freerdp_settings_get_uint32(settings, FreeRDP_ServerPort) != 3392) ||
		    !check_string(settings, FreeRDP_ServerHostname, "shadow-host") ||
		    !check_string(settings, FreeRDP_CertificateAcceptedFingerprints,
		                  TEST_CERT_FINGERPRINT) ||
		    !freerdp_settings_get_bool(settings, FreeRDP_NlaSecurity))
		{
			(void)fprintf(stderr, "invitation applied wrongly in round %" PRIuz "\n", round);
			goto out;
		}
	}

	/* a CE attribute outside the A element is not the listener certificate */
	freerdp_settings_free(settings);
	settings = freerdp_settings_new(0);
	if (!settings ||
	    !freerdp_settings_set_string(settings, FreeRDP_ServerHostname, "shadow-host") ||
	    !remote_shadow_apply_invitation(settings, invitation_no_certificate) ||
	    !check_string(settings, FreeRDP_CertificateAcceptedFingerprints, nullptr))
	{
		(void)fprintf(stderr, "a certificate outside the A element was pinned\n");
		goto out;
	}

	/* nothing reachable is left */
	if (remote_shadow_apply_invitation(settings, invitation_link_local_only))
	{
		(void)fprintf(stderr, "an invitation with only link-local listeners was accepted\n");
		goto out;
	}
	rc = TRUE;
out:
	freerdp_settings_free(settings);
	return rc;
}
#endif

#ifdef TEST_RPC_SILENT_HOST
/* a TCP server that accepts connections and never answers, like a host stuck in RpcShadow2 */
static int silent_listener(UINT16* port)
{
	struct sockaddr_in addr = { .sin_family = AF_INET, .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
	socklen_t len = sizeof(addr);
	const int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;
	if ((bind(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) || (listen(fd, 4) != 0) ||
	    (getsockname(fd, (struct sockaddr*)&addr, &len) != 0))
	{
		close(fd);
		return -1;
	}
	*port = ntohs(addr.sin_port);
	return fd;
}

static DWORD WINAPI abort_later(LPVOID arg)
{
	Sleep(200);
	(void)freerdp_abort_connect_context(arg);
	return 0;
}

#define SLOW_HOST_DELAY_MS 500
/* as in TestConnect: an aborted or failed connection attempt ends within 5 s */
#define RPC_MAX_DURATION_MS 5000

static BOOL read_full(int fd, BYTE* data, size_t length)
{
	for (size_t done = 0; done < length;)
	{
		const ssize_t rc = recv(fd, &data[done], length - done, 0);
		if (rc <= 0)
			return FALSE;
		done += (size_t)rc;
	}
	return TRUE;
}

/*
 * A host on a slow link: it answers the SMB2 NEGOTIATE request only after SLOW_HOST_DELAY_MS,
 * several times the poll interval of the client, with an error response
 * (STATUS_ACCESS_DENIED) that ends the call. Then it waits until the client goes away.
 */
static DWORD WINAPI slow_host(LPVOID arg)
{
	const int listener = *(const int*)arg;
	BYTE request[4096] = WINPR_C_ARRAY_INIT;
	BYTE answer[4 + 64 + 9] = WINPR_C_ARRAY_INIT;

	/* bounded: a client that never connects must not leave this thread in accept() */
	struct pollfd pfd = { .fd = listener, .events = POLLIN };
	if (poll(&pfd, 1, RPC_MAX_DURATION_MS) <= 0)
		return 1;
	const int fd = accept(listener, nullptr, nullptr);
	if (fd < 0)
		return 1;

	/* NetBIOS session header: zero, 24 bit big endian length */
	DWORD rc = 1;
	if (!read_full(fd, request, 4))
		goto out;
	const size_t length = ((size_t)request[1] << 16u) | ((size_t)request[2] << 8u) | request[3];
	if ((length < 64) || (length > sizeof(request) - 4) || !read_full(fd, &request[4], length))
		goto out;

	Sleep(SLOW_HOST_DELAY_MS);

	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticInit(&sbuffer, answer, sizeof(answer));
	Stream_Write_UINT8(s, 0);
	Stream_Write_UINT8(s, 0);
	Stream_Write_UINT16_BE(s, sizeof(answer) - 4);
	Stream_Write(s, "\xFESMB", 4);        /* ProtocolId */
	Stream_Write_UINT16(s, 64);           /* StructureSize */
	Stream_Write_UINT16(s, 0);            /* CreditCharge */
	Stream_Write_UINT32(s, 0xC0000022);   /* Status: STATUS_ACCESS_DENIED */
	Stream_Write_UINT16(s, 0);            /* Command: NEGOTIATE */
	Stream_Write_UINT16(s, 1);            /* CreditResponse */
	Stream_Write_UINT32(s, 1);            /* Flags: SERVER_TO_REDIR */
	Stream_Write_UINT32(s, 0);            /* NextCommand */
	Stream_Write(s, &request[4 + 24], 8); /* MessageId of the request */
	Stream_Zero(s, 4 + 4 + 8 + 16);       /* Reserved, TreeId, SessionId, Signature */
	Stream_Write_UINT16(s, 9);            /* ERROR StructureSize */
	Stream_Zero(s, 1 + 1 + 4 + 1);        /* ErrorContextCount, Reserved, ByteCount, ErrorData */
	if (send(fd, answer, sizeof(answer), 0) != (ssize_t)sizeof(answer))
		goto out;

	while (recv(fd, request, sizeof(request), 0) > 0)
		;
	rc = 0;
out:
	close(fd);
	return rc;
}

typedef enum
{
	HOST_SILENT,
	HOST_SILENT_ABORT,
	HOST_SLOW_ANSWER
} TestHost;

/* calls RpcShadow2 against a test host; TRUE when the call failed, as it must */
static BOOL rpc_against_test_host(TestHost mode, UINT32 timeout, UINT64* elapsed)
{
	BOOL rc = FALSE;
	HANDLE thread = nullptr;
	HANDLE server = nullptr;
	UINT16 port = 0;
	char host[32] = WINPR_C_ARRAY_INIT;
	const int fd = silent_listener(&port);
	freerdp* instance = freerdp_new();
	if ((fd < 0) || !instance || !freerdp_context_new(instance))
		goto out;

	rdpSettings* settings = instance->context->settings;
	(void)_snprintf(host, sizeof(host), "127.0.0.1:%" PRIu16, port);
	if (!freerdp_settings_set_string(settings, FreeRDP_ServerHostname, host) ||
	    !freerdp_settings_set_string(settings, FreeRDP_Username, "user") ||
	    !freerdp_settings_set_string(settings, FreeRDP_Password, "password"))
		goto out;
	if ((mode == HOST_SILENT_ABORT) &&
	    !(thread = CreateThread(nullptr, 0, abort_later, instance->context, 0, nullptr)))
		goto out;
	if ((mode == HOST_SLOW_ANSWER) &&
	    !(server = CreateThread(nullptr, 0, slow_host, (LPVOID)&fd, 0, nullptr)))
		goto out;

	UINT16 permission = 0;
	UINT32 hresult = 0;
	char* invitation = nullptr;
	const UINT64 start = GetTickCount64();
	const BOOL ok =
	    remote_shadow_rpc_shadow2(instance->context, timeout, &permission, &hresult, &invitation);
	*elapsed = GetTickCount64() - start;
	free(invitation);
	rc = !ok;
out:
	if (thread)
	{
		(void)WaitForSingleObject(thread, INFINITE);
		(void)CloseHandle(thread);
	}
	if (instance)
	{
		freerdp_context_free(instance);
		freerdp_free(instance);
	}
	/* the client is gone, so the slow host returns; if it never connected, its poll times out */
	if (server)
	{
		DWORD code = 1;
		(void)WaitForSingleObject(server, INFINITE);
		if (!GetExitCodeThread(server, &code) || (code != 0))
			rc = FALSE;
		(void)CloseHandle(server);
	}
	if (fd >= 0)
		close(fd);
	return rc;
}

/*
 * a client closing while it reconnects must not wait for a host that does not answer, and a
 * host that answers late must still be heard
 */
static BOOL test_rpc_gives_up(void)
{
	UINT64 elapsed = 0;
	/* without the abort check this waits the whole 30 s */
	if (!rpc_against_test_host(HOST_SILENT_ABORT, 30000, &elapsed) ||
	    (elapsed > RPC_MAX_DURATION_MS))
	{
		(void)fprintf(stderr, "aborted call took %" PRIu64 " ms\n", elapsed);
		return FALSE;
	}
	if (!rpc_against_test_host(HOST_SILENT, 500, &elapsed) || (elapsed > RPC_MAX_DURATION_MS))
	{
		(void)fprintf(stderr, "call with a 500 ms timeout took %" PRIu64 " ms\n", elapsed);
		return FALSE;
	}
	/*
	 * a slow link is not a reason to give up: the answer that comes after several poll
	 * intervals still ends the call, long before the deadline
	 */
	if (!rpc_against_test_host(HOST_SLOW_ANSWER, 30000, &elapsed) ||
	    (elapsed + 10 < SLOW_HOST_DELAY_MS) || (elapsed > RPC_MAX_DURATION_MS))
	{
		(void)fprintf(stderr, "call to a host answering after %d ms took %" PRIu64 " ms\n",
		              SLOW_HOST_DELAY_MS, elapsed);
		return FALSE;
	}
	return TRUE;
}
#endif

int TestRemoteShadow(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	wLog* log = WLog_Get(TAG);

	if (!test_write_request())
	{
		(void)fprintf(stderr, "test_write_request failed\n");
		return -1;
	}
	if (!test_read_response(log))
	{
		(void)fprintf(stderr, "test_read_response failed\n");
		return -1;
	}
	if (!test_read_declined(log))
	{
		(void)fprintf(stderr, "test_read_declined failed\n");
		return -1;
	}
	if (!test_read_malformed(log))
	{
		(void)fprintf(stderr, "test_read_malformed failed\n");
		return -1;
	}
#if defined(WITH_LIBSMB2)
	if (!test_apply_invitation())
	{
		(void)fprintf(stderr, "test_apply_invitation failed\n");
		return -1;
	}
#endif
#ifdef TEST_RPC_SILENT_HOST
	if (!test_rpc_gives_up())
	{
		(void)fprintf(stderr, "test_rpc_gives_up failed\n");
		return -1;
	}
#endif
	return 0;
}
