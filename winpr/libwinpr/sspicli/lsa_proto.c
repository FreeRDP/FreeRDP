/**
 * WinPR: Windows Portable Runtime
 * lsa4posix wire protocol (length-prefixed messages between the WinPR LSA client and the daemon)
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

#include <winpr/config.h>

#include <winpr/crt.h>
#include <winpr/assert.h>

#include "lsa_proto.h"

#define TAG "com.winpr.sspicli.lsa"

BOOL lsa_proto_writer_init(LsaProtoWriter* w, const LSA_MSG_HDR* hdr)
{
	WINPR_ASSERT(w);
	WINPR_ASSERT(hdr);

	w->s = Stream_New(nullptr, 256);
	if (!w->s)
		return FALSE;

	if (!lsa_proto_write_u16(w, hdr->type) || !lsa_proto_write_u16(w, hdr->flags) ||
	    !lsa_proto_write_u32(w, hdr->requestId))
	{
		lsa_proto_writer_abort(w);
		return FALSE;
	}

	return TRUE;
}

void lsa_proto_writer_abort(LsaProtoWriter* w)
{
	if (!w)
		return;
	Stream_Free(w->s, TRUE);
	w->s = nullptr;
}

BOOL lsa_proto_writer_finish(LsaProtoWriter* w, wStream** framed)
{
	WINPR_ASSERT(w);
	WINPR_ASSERT(framed);

	*framed = nullptr;

	if (!w->s)
		return FALSE;

	Stream_SealLength(w->s);
	if (!Stream_SetPosition(w->s, 0))
	{
		lsa_proto_writer_abort(w);
		return FALSE;
	}

	*framed = w->s;
	w->s = nullptr; /* ownership moves to the caller */
	return TRUE;
}

BOOL lsa_proto_write_u16(LsaProtoWriter* w, UINT16 v)
{
	WINPR_ASSERT(w);
	if (!w->s || !Stream_EnsureRemainingCapacity(w->s, 2))
		return FALSE;
	Stream_Write_UINT16(w->s, v);
	return TRUE;
}

BOOL lsa_proto_write_u32(LsaProtoWriter* w, UINT32 v)
{
	WINPR_ASSERT(w);
	if (!w->s || !Stream_EnsureRemainingCapacity(w->s, 4))
		return FALSE;
	Stream_Write_UINT32(w->s, v);
	return TRUE;
}

BOOL lsa_proto_write_u64(LsaProtoWriter* w, UINT64 v)
{
	WINPR_ASSERT(w);
	if (!w->s || !Stream_EnsureRemainingCapacity(w->s, 8))
		return FALSE;
	Stream_Write_UINT64(w->s, v);
	return TRUE;
}

/* A byte buffer is a UINT32 length followed by that many bytes. */
BOOL lsa_proto_write_blob(LsaProtoWriter* w, const BYTE* data, UINT32 len)
{
	WINPR_ASSERT(w);

	if ((len > 0) && !data)
		return FALSE;
	if (len > LSA_PROTO_MAX_FRAME)
		return FALSE;

	if (!lsa_proto_write_u32(w, len))
		return FALSE;
	if (len == 0)
		return TRUE;

	if (!Stream_EnsureRemainingCapacity(w->s, len))
		return FALSE;
	Stream_Write(w->s, data, len);
	return TRUE;
}

BOOL lsa_proto_reader_init(LsaProtoReader* r, wStream* framed, LSA_MSG_HDR* hdr)
{
	WINPR_ASSERT(r);
	WINPR_ASSERT(framed);
	WINPR_ASSERT(hdr);

	r->s = framed;

	if (!lsa_proto_read_u16(r, &hdr->type) || !lsa_proto_read_u16(r, &hdr->flags) ||
	    !lsa_proto_read_u32(r, &hdr->requestId))
	{
		r->s = nullptr;
		return FALSE;
	}

	return TRUE;
}

void lsa_proto_reader_done(LsaProtoReader* r)
{
	if (!r)
		return;
	r->s = nullptr; /* the framed stream is owned by the caller */
}

BOOL lsa_proto_read_u16(LsaProtoReader* r, UINT16* v)
{
	WINPR_ASSERT(r);
	WINPR_ASSERT(v);
	if (!r->s || !Stream_CheckAndLogRequiredLength(TAG, r->s, 2))
		return FALSE;
	Stream_Read_UINT16(r->s, *v);
	return TRUE;
}

BOOL lsa_proto_read_u32(LsaProtoReader* r, UINT32* v)
{
	WINPR_ASSERT(r);
	WINPR_ASSERT(v);
	if (!r->s || !Stream_CheckAndLogRequiredLength(TAG, r->s, 4))
		return FALSE;
	Stream_Read_UINT32(r->s, *v);
	return TRUE;
}

BOOL lsa_proto_read_u64(LsaProtoReader* r, UINT64* v)
{
	WINPR_ASSERT(r);
	WINPR_ASSERT(v);
	if (!r->s || !Stream_CheckAndLogRequiredLength(TAG, r->s, 8))
		return FALSE;
	Stream_Read_UINT64(r->s, *v);
	return TRUE;
}

BOOL lsa_proto_read_blob(LsaProtoReader* r, BYTE** data, UINT32* len)
{
	WINPR_ASSERT(r);
	WINPR_ASSERT(data);
	WINPR_ASSERT(len);

	*data = nullptr;
	*len = 0;

	UINT32 count = 0;
	if (!lsa_proto_read_u32(r, &count))
		return FALSE;
	if (count > LSA_PROTO_MAX_FRAME)
		return FALSE;
	if (count == 0)
		return TRUE;

	if (!Stream_CheckAndLogRequiredLength(TAG, r->s, count))
		return FALSE;

	BYTE* buf = (BYTE*)malloc(count);
	if (!buf)
		return FALSE;

	Stream_Read(r->s, buf, count);

	*data = buf;
	*len = count;
	return TRUE;
}
