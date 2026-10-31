/**
 * WinPR: Windows Portable Runtime
 * lsa4posix: connection handling and message dispatch
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
#include <winpr/wlog.h>
#include <winpr/nt.h>
#include <winpr/stream.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "lsa4posix.h"
#include "lsa_proto.h"
#include "ap_tsssp.h"

#define TAG LSA4POSIX_TAG

/* The TSSSP authentication package id handed out by LSA_LOOKUP_PACKAGE. */
#define LSA4POSIX_TSSSP_PACKAGE_ID 1

static BOOL rw_all(int fd, BYTE* data, size_t len, BOOL writing)
{
	size_t off = 0;
	while (off < len)
	{
		const ssize_t n =
		    writing ? write(fd, &data[off], len - off) : read(fd, &data[off], len - off);
		if (n > 0)
			off += (size_t)n;
		else if ((n < 0) && (errno == EINTR))
			continue;
		else
			return FALSE;
	}
	return TRUE;
}

static wStream* frame_recv(int fd)
{
	BYTE pfx[4] = { 0 };
	if (!rw_all(fd, pfx, sizeof(pfx), FALSE))
		return nullptr;
	const UINT32 len =
	    ((UINT32)pfx[0] << 24) | ((UINT32)pfx[1] << 16) | ((UINT32)pfx[2] << 8) | (UINT32)pfx[3];
	if ((len == 0) || (len > LSA_PROTO_MAX_FRAME))
		return nullptr;
	wStream* s = Stream_New(nullptr, len);
	if (!s)
		return nullptr;
	if (!rw_all(fd, Stream_Buffer(s), len, FALSE) || !Stream_SetLength(s, len) ||
	    !Stream_SetPosition(s, 0))
	{
		Stream_Free(s, TRUE);
		return nullptr;
	}
	return s;
}

static BOOL frame_send(int fd, wStream* payload)
{
	const size_t len = Stream_Length(payload);
	if ((len == 0) || (len > LSA_PROTO_MAX_FRAME))
		return FALSE;
	BYTE pfx[4] = { (BYTE)(len >> 24), (BYTE)(len >> 16), (BYTE)(len >> 8), (BYTE)len };
	return rw_all(fd, pfx, sizeof(pfx), TRUE) && rw_all(fd, Stream_Buffer(payload), len, TRUE);
}

static BOOL send_response(int fd, LsaProtoWriter* w)
{
	wStream* out = nullptr;
	if (!lsa_proto_writer_finish(w, &out))
	{
		lsa_proto_writer_abort(w);
		return FALSE;
	}
	const BOOL rc = frame_send(fd, out);
	Stream_Free(out, TRUE);
	return rc;
}

static BOOL handle_hello(int fd, const LSA_MSG_HDR* req, LsaProtoReader* r)
{
	UINT16 major = 0;
	UINT16 minor = 0;
	UINT32 clientFlags = 0;
	if (!lsa_proto_read_u16(r, &major) || !lsa_proto_read_u16(r, &minor) ||
	    !lsa_proto_read_u32(r, &clientFlags))
		return FALSE;

	LSA_MSG_HDR rh = { req->type, LSA_MSG_FLAG_RESPONSE, req->requestId };
	LsaProtoWriter w = { 0 };
	if (!lsa_proto_writer_init(&w, &rh) || !lsa_proto_write_u32(&w, STATUS_SUCCESS) ||
	    !lsa_proto_write_u16(&w, LSA_PROTO_VERSION_MAJOR) ||
	    !lsa_proto_write_u16(&w, LSA_PROTO_VERSION_MINOR) || !lsa_proto_write_u32(&w, 0) ||
	    !lsa_proto_write_u64(&w, (UINT64)getpid()))
	{
		lsa_proto_writer_abort(&w);
		return FALSE;
	}
	return send_response(fd, &w);
}

static BOOL handle_lookup(int fd, const LSA_MSG_HDR* req, LsaProtoReader* r)
{
	BYTE* name = nullptr;
	UINT32 nameLen = 0;
	if (!lsa_proto_read_blob(r, &name, &nameLen))
		return FALSE;

	NTSTATUS status = STATUS_NO_SUCH_PACKAGE;
	UINT32 packageId = 0;
	if ((nameLen == 5) && (memcmp(name, "TSSSP", 5) == 0))
	{
		status = STATUS_SUCCESS;
		packageId = LSA4POSIX_TSSSP_PACKAGE_ID;
	}
	free(name);

	LSA_MSG_HDR rh = { req->type, LSA_MSG_FLAG_RESPONSE, req->requestId };
	LsaProtoWriter w = { 0 };
	if (!lsa_proto_writer_init(&w, &rh) || !lsa_proto_write_u32(&w, (UINT32)status) ||
	    !lsa_proto_write_u32(&w, packageId))
	{
		lsa_proto_writer_abort(&w);
		return FALSE;
	}
	return send_response(fd, &w);
}

static BOOL handle_call(int fd, const LSA_MSG_HDR* req, LsaProtoReader* r)
{
	UINT32 packageId = 0;
	BYTE* submit = nullptr;
	UINT32 submitLen = 0;
	if (!lsa_proto_read_u32(r, &packageId) || !lsa_proto_read_blob(r, &submit, &submitLen))
		return FALSE;

	NTSTATUS protocolStatus = STATUS_NOT_IMPLEMENTED;
	BYTE* ret = nullptr;
	UINT32 retLen = 0;
	if (packageId == LSA4POSIX_TSSSP_PACKAGE_ID)
	{
		if (!ap_tsssp_call_package(submit, submitLen, &protocolStatus, &ret, &retLen))
			WLog_WARN(TAG, "TSSSP call package failed: 0x%08" PRIx32, (UINT32)protocolStatus);
	}
	free(submit);

	LSA_MSG_HDR rh = { req->type, LSA_MSG_FLAG_RESPONSE, req->requestId };
	LsaProtoWriter w = { 0 };
	if (!lsa_proto_writer_init(&w, &rh) || !lsa_proto_write_u32(&w, STATUS_SUCCESS) ||
	    !lsa_proto_write_u32(&w, (UINT32)protocolStatus) || !lsa_proto_write_blob(&w, ret, retLen))
	{
		lsa_proto_writer_abort(&w);
		free(ret);
		return FALSE;
	}
	free(ret);
	return send_response(fd, &w);
}

static BOOL handle_sspi(ApTssspState* ap, int fd, const LSA_MSG_HDR* req, LsaProtoReader* r)
{
	LSA_MSG_HDR rh = { req->type, LSA_MSG_FLAG_RESPONSE, req->requestId };
	LsaProtoWriter w = { 0 };
	if (!lsa_proto_writer_init(&w, &rh))
		return FALSE;

	BOOL ok = FALSE;
	switch (req->type)
	{
		case SSPI_MSG_ACQUIRE_CRED:
			ok = ap_tsssp_acquire(ap, r, &w);
			break;
		case SSPI_MSG_ISC:
			ok = ap_tsssp_isc(ap, r, &w);
			break;
		case SSPI_MSG_DELETE_CTX:
			ok = ap_tsssp_delete_ctx(ap, r, &w);
			break;
		case SSPI_MSG_FREE_CRED:
			ok = ap_tsssp_free_cred(ap, r, &w);
			break;
		default:
			break;
	}

	if (!ok)
	{
		lsa_proto_writer_abort(&w);
		return FALSE;
	}
	return send_response(fd, &w);
}

void lsa4posix_serve_connection(int fd)
{
	ApTssspState* ap = ap_tsssp_new();
	if (!ap)
	{
		close(fd);
		return;
	}

	for (;;)
	{
		wStream* in = frame_recv(fd);
		if (!in)
			break;

		LsaProtoReader r = { 0 };
		LSA_MSG_HDR hdr = { 0 };
		if (!lsa_proto_reader_init(&r, in, &hdr))
		{
			Stream_Free(in, TRUE);
			break;
		}

		BOOL ok = FALSE;
		switch (hdr.type)
		{
			case LSA_MSG_HELLO:
				ok = handle_hello(fd, &hdr, &r);
				break;
			case LSA_MSG_LOOKUP_PACKAGE:
				ok = handle_lookup(fd, &hdr, &r);
				break;
			case LSA_MSG_CALL_PACKAGE:
				ok = handle_call(fd, &hdr, &r);
				break;
			case SSPI_MSG_ACQUIRE_CRED:
			case SSPI_MSG_ISC:
			case SSPI_MSG_DELETE_CTX:
			case SSPI_MSG_FREE_CRED:
				ok = handle_sspi(ap, fd, &hdr, &r);
				break;
			default:
				WLog_WARN(TAG, "unknown message type %" PRIu16, hdr.type);
				break;
		}

		lsa_proto_reader_done(&r);
		Stream_Free(in, TRUE);
		if (!ok)
			break;
	}

	ap_tsssp_free(ap);
	close(fd);
}
