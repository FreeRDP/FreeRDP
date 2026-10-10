#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/secapi.h>
#include <winpr/nt.h>
#include <winpr/environment.h>

#include "../lsa_proto.h"

#define FAKE_PACKAGE_ID 0x2A

/* ---- tiny framing helpers mirroring the client (UINT32 big-endian length prefix) ---- */

static BOOL rw_all(int fd, BYTE* data, size_t len, BOOL writing)
{
	size_t off = 0;
	while (off < len)
	{
		const ssize_t n =
		    writing ? write(fd, &data[off], len - off) : read(fd, &data[off], len - off);
		if (n > 0)
			off += (size_t)n;
		else
			return FALSE;
	}
	return TRUE;
}

static wStream* frame_recv(int fd)
{
	BYTE pfx[4] = { 0 };
	if (!rw_all(fd, pfx, sizeof(pfx), FALSE))
		return NULL;
	const UINT32 len =
	    ((UINT32)pfx[0] << 24) | ((UINT32)pfx[1] << 16) | ((UINT32)pfx[2] << 8) | (UINT32)pfx[3];
	if ((len == 0) || (len > LSA_PROTO_MAX_FRAME))
		return NULL;
	wStream* s = Stream_New(NULL, len);
	if (!s)
		return NULL;
	if (!rw_all(fd, Stream_Buffer(s), len, FALSE) || !Stream_SetLength(s, len) ||
	    !Stream_SetPosition(s, 0))
	{
		Stream_Free(s, TRUE);
		return NULL;
	}
	return s;
}

static BOOL frame_send(int fd, wStream* payload)
{
	const size_t len = Stream_Length(payload);
	BYTE pfx[4] = { (BYTE)(len >> 24), (BYTE)(len >> 16), (BYTE)(len >> 8), (BYTE)len };
	return rw_all(fd, pfx, sizeof(pfx), TRUE) && rw_all(fd, Stream_Buffer(payload), len, TRUE);
}

/* ---- fake lsa4posix daemon: one connection, serves HELLO/LOOKUP/CALL ---- */

static void* fake_daemon(void* arg)
{
	const int listenFd = *(const int*)arg;
	const int conn = accept(listenFd, NULL, NULL);
	if (conn < 0)
		return NULL;

	for (;;)
	{
		wStream* in = frame_recv(conn);
		if (!in)
			break;

		LsaProtoReader r = { 0 };
		LSA_MSG_HDR hdr = { 0 };
		if (!lsa_proto_reader_init(&r, in, &hdr))
		{
			Stream_Free(in, TRUE);
			break;
		}

		LSA_MSG_HDR rh = { hdr.type, LSA_MSG_FLAG_RESPONSE, hdr.requestId };
		LsaProtoWriter w = { 0 };
		BOOL ok = lsa_proto_writer_init(&w, &rh);

		if (ok && (hdr.type == LSA_MSG_HELLO))
		{
			UINT16 maj = 0;
			UINT16 min = 0;
			UINT32 cf = 0;
			ok = lsa_proto_read_u16(&r, &maj) && lsa_proto_read_u16(&r, &min) &&
			     lsa_proto_read_u32(&r, &cf);
			ok = ok && lsa_proto_write_u32(&w, STATUS_SUCCESS) &&
			     lsa_proto_write_u16(&w, LSA_PROTO_VERSION_MAJOR) &&
			     lsa_proto_write_u16(&w, LSA_PROTO_VERSION_MINOR) && lsa_proto_write_u32(&w, 0) &&
			     lsa_proto_write_u64(&w, 0x1122334455667788ULL);
		}
		else if (ok && (hdr.type == LSA_MSG_LOOKUP_PACKAGE))
		{
			BYTE* name = NULL;
			UINT32 nameLen = 0;
			ok = lsa_proto_read_blob(&r, &name, &nameLen);
			free(name);
			ok = ok && lsa_proto_write_u32(&w, STATUS_SUCCESS) &&
			     lsa_proto_write_u32(&w, FAKE_PACKAGE_ID);
		}
		else if (ok && (hdr.type == LSA_MSG_CALL_PACKAGE))
		{
			UINT32 pkg = 0;
			BYTE* submit = NULL;
			UINT32 submitLen = 0;
			ok = lsa_proto_read_u32(&r, &pkg) && lsa_proto_read_blob(&r, &submit, &submitLen);
			/* echo the submit buffer straight back as the return buffer */
			ok = ok && lsa_proto_write_u32(&w, STATUS_SUCCESS) && lsa_proto_write_u32(&w, 0) &&
			     lsa_proto_write_blob(&w, submit, submitLen);
			free(submit);
		}
		else
			ok = FALSE;

		lsa_proto_reader_done(&r);
		Stream_Free(in, TRUE);

		wStream* out = NULL;
		if (ok && lsa_proto_writer_finish(&w, &out))
		{
			const BOOL sent = frame_send(conn, out);
			Stream_Free(out, TRUE);
			if (!sent)
				break;
		}
		else
		{
			lsa_proto_writer_abort(&w);
			break;
		}
	}

	close(conn);
	return NULL;
}

static int test_codec_roundtrip(void)
{
	const BYTE submit[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03 };
	LSA_MSG_HDR hdr = { LSA_MSG_CALL_PACKAGE, 0, 0x12345678 };
	LsaProtoWriter w = { 0 };
	wStream* framed = NULL;
	if (!lsa_proto_writer_init(&w, &hdr) || !lsa_proto_write_u32(&w, 0x99) ||
	    !lsa_proto_write_blob(&w, submit, sizeof(submit)) || !lsa_proto_writer_finish(&w, &framed))
	{
		lsa_proto_writer_abort(&w);
		(void)fprintf(stderr, "codec: encode failed\n");
		return -1;
	}

	int rc = -1;
	LsaProtoReader r = { 0 };
	LSA_MSG_HDR got = { 0 };
	UINT32 pkg = 0;
	BYTE* blob = NULL;
	UINT32 blobLen = 0;
	if (lsa_proto_reader_init(&r, framed, &got) && lsa_proto_read_u32(&r, &pkg) &&
	    lsa_proto_read_blob(&r, &blob, &blobLen))
	{
		if ((got.type == LSA_MSG_CALL_PACKAGE) && (got.requestId == 0x12345678) && (pkg == 0x99) &&
		    (blobLen == sizeof(submit)) && (memcmp(blob, submit, sizeof(submit)) == 0))
			rc = 0;
		else
			(void)fprintf(stderr, "codec: field mismatch\n");
	}
	free(blob);
	lsa_proto_reader_done(&r);
	Stream_Free(framed, TRUE);
	return rc;
}

static int test_fake_daemon(void)
{
	char dir[] = "/tmp/lsatestXXXXXX";
	if (!mkdtemp(dir))
	{
		(void)fprintf(stderr, "mkdtemp failed\n");
		return -1;
	}

	char sockPath[128] = { 0 };
	(void)snprintf(sockPath, sizeof(sockPath), "%s/lsa.sock", dir);

	int rc = -1;
	int listenFd = socket(AF_UNIX, SOCK_STREAM, 0);
	pthread_t thread = 0;
	BOOL threadStarted = FALSE;
	HANDLE lsa = NULL;

	if (listenFd < 0)
		goto cleanup;

	struct sockaddr_un addr = { 0 };
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sockPath, sizeof(addr.sun_path) - 1);
	if ((bind(listenFd, (struct sockaddr*)&addr, sizeof(addr)) != 0) || (listen(listenFd, 1) != 0))
		goto cleanup;

	(void)SetEnvironmentVariableA("LSA4POSIX_SOCKET", sockPath);

	if (pthread_create(&thread, NULL, fake_daemon, &listenFd) != 0)
		goto cleanup;
	threadStarted = TRUE;

	if (LsaConnectUntrusted(&lsa) != STATUS_SUCCESS)
	{
		(void)fprintf(stderr, "LsaConnectUntrusted failed\n");
		goto cleanup;
	}

	char pkgName[] = "TSSSP";
	LSA_STRING name = { (USHORT)strlen(pkgName), (USHORT)sizeof(pkgName), pkgName };
	ULONG pkgId = 0;
	if ((LsaLookupAuthenticationPackage(lsa, &name, &pkgId) != STATUS_SUCCESS) ||
	    (pkgId != FAKE_PACKAGE_ID))
	{
		(void)fprintf(stderr, "LsaLookupAuthenticationPackage failed (id=%lu)\n",
		              (unsigned long)pkgId);
		goto cleanup;
	}

	BYTE submit[] = { 'r', 'd', 'p', 'e', 'a', 'r', 0x00, 0xFF };
	PVOID ret = NULL;
	ULONG retLen = 0;
	NTSTATUS protoStatus = 0;
	if ((LsaCallAuthenticationPackage(lsa, pkgId, submit, sizeof(submit), &ret, &retLen,
	                                  &protoStatus) != STATUS_SUCCESS) ||
	    (retLen != sizeof(submit)) || !ret || (memcmp(ret, submit, sizeof(submit)) != 0))
	{
		(void)fprintf(stderr, "LsaCallAuthenticationPackage echo mismatch (len=%lu)\n",
		              (unsigned long)retLen);
		(void)LsaFreeReturnBuffer(ret);
		goto cleanup;
	}
	(void)LsaFreeReturnBuffer(ret);
	rc = 0;

cleanup:
	if (lsa)
		(void)LsaDeregisterLogonProcess(lsa);
	if (listenFd >= 0)
		close(listenFd);
	if (threadStarted)
		pthread_join(thread, NULL);
	(void)unlink(sockPath);
	(void)rmdir(dir);
	return rc;
}

int TestLsaClient(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (test_codec_roundtrip() != 0)
		return -1;
	if (test_fake_daemon() != 0)
		return -1;
	return 0;
}
