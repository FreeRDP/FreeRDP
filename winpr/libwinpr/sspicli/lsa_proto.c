/**
 * WinPR: Windows Portable Runtime
 * lsa4posix wire protocol (NDR-serialized messages between the WinPR LSA client and the daemon)
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

/* little-endian, type serialization version 1, as the [MS-RDPEAR] payloads */
#define LSA_PROTO_NDR_VERSION 1

BOOL lsa_proto_writer_init(LsaProtoWriter* w, const LSA_MSG_HDR* hdr)
{
	WINPR_ASSERT(w);
	WINPR_ASSERT(hdr);

	w->s = Stream_New(nullptr, 256);
	w->enc = winpr_ndr_encoder_new(FALSE, LSA_PROTO_NDR_VERSION);
	if (!w->s || !w->enc)
		goto fail;

	if (!winpr_ndr_encoder_write_header(w->enc, w->s) ||
	    !winpr_ndr_encoder_start_object(w->enc, w->s) ||
	    !winpr_ndr_encoder_write_toplevel_refid(w->enc, w->s))
		goto fail;

	if (!lsa_proto_write_u16(w, hdr->type) || !lsa_proto_write_u16(w, hdr->flags) ||
	    !lsa_proto_write_u32(w, hdr->requestId))
		goto fail;

	return TRUE;

fail:
	lsa_proto_writer_abort(w);
	return FALSE;
}

void lsa_proto_writer_abort(LsaProtoWriter* w)
{
	if (!w)
		return;
	winpr_ndr_encoder_destroy(&w->enc);
	Stream_Free(w->s, TRUE);
	w->s = nullptr;
}

BOOL lsa_proto_writer_finish(LsaProtoWriter* w, wStream** framed)
{
	WINPR_ASSERT(w);
	WINPR_ASSERT(framed);

	*framed = nullptr;

	if (!w->s || !w->enc || !winpr_ndr_encoder_end_object(w->enc, w->s))
	{
		lsa_proto_writer_abort(w);
		return FALSE;
	}

	Stream_SealLength(w->s);
	if (!Stream_SetPosition(w->s, 0))
	{
		lsa_proto_writer_abort(w);
		return FALSE;
	}

	*framed = w->s;
	w->s = nullptr; /* ownership moves to the caller */
	winpr_ndr_encoder_destroy(&w->enc);
	return TRUE;
}

BOOL lsa_proto_write_u16(LsaProtoWriter* w, UINT16 v)
{
	WINPR_ASSERT(w);
	if (!w->s || !w->enc)
		return FALSE;
	return winpr_ndr_encoder_write_uint16(w->enc, w->s, v);
}

BOOL lsa_proto_write_u32(LsaProtoWriter* w, UINT32 v)
{
	WINPR_ASSERT(w);
	if (!w->s || !w->enc)
		return FALSE;
	return winpr_ndr_encoder_write_uint32(w->enc, w->s, v);
}

BOOL lsa_proto_write_u64(LsaProtoWriter* w, UINT64 v)
{
	WINPR_ASSERT(w);
	if (!w->s || !w->enc)
		return FALSE;
	return winpr_ndr_encoder_write_uint64(w->enc, w->s, v);
}

/* A byte buffer is a conformant array of bytes: its UINT32 count, then the bytes. */
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

	return winpr_ndr_encoder_write_data(w->enc, w->s, data, len);
}

BOOL lsa_proto_reader_init(LsaProtoReader* r, wStream* framed, LSA_MSG_HDR* hdr)
{
	WINPR_ASSERT(r);
	WINPR_ASSERT(framed);
	WINPR_ASSERT(hdr);

	const wStream empty = WINPR_C_ARRAY_INIT;
	r->object = empty;
	r->dec = winpr_ndr_decoder_new_fromStream(framed);
	if (!r->dec)
		return FALSE;

	winpr_ndr_refid refId = WINPR_NDR_PTR_NULL;
	if (!winpr_ndr_decoder_read_object(r->dec, framed, &r->object) ||
	    !winpr_ndr_decoder_read_refpointer(r->dec, &r->object, &refId) || !refId)
		goto fail;

	if (!lsa_proto_read_u16(r, &hdr->type) || !lsa_proto_read_u16(r, &hdr->flags) ||
	    !lsa_proto_read_u32(r, &hdr->requestId))
		goto fail;

	return TRUE;

fail:
	lsa_proto_reader_done(r);
	return FALSE;
}

void lsa_proto_reader_done(LsaProtoReader* r)
{
	if (!r)
		return;
	/* the object is a view into the framed stream, which is owned by the caller */
	winpr_ndr_decoder_destroy(&r->dec);
}

BOOL lsa_proto_read_u16(LsaProtoReader* r, UINT16* v)
{
	WINPR_ASSERT(r);
	WINPR_ASSERT(v);
	if (!r->dec)
		return FALSE;
	return winpr_ndr_decoder_read_uint16(r->dec, &r->object, v);
}

BOOL lsa_proto_read_u32(LsaProtoReader* r, UINT32* v)
{
	WINPR_ASSERT(r);
	WINPR_ASSERT(v);
	if (!r->dec)
		return FALSE;
	return winpr_ndr_decoder_read_uint32(r->dec, &r->object, v);
}

BOOL lsa_proto_read_u64(LsaProtoReader* r, UINT64* v)
{
	WINPR_ASSERT(r);
	WINPR_ASSERT(v);
	if (!r->dec)
		return FALSE;
	return winpr_ndr_decoder_read_uint64(r->dec, &r->object, v);
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

	if (!Stream_CheckAndLogRequiredLength(TAG, &r->object, count))
		return FALSE;

	BYTE* buf = (BYTE*)malloc(count);
	if (!buf)
		return FALSE;

	Stream_Read(&r->object, buf, count);
	winpr_ndr_decoder_bytes_read(r->dec, count);

	*data = buf;
	*len = count;
	return TRUE;
}
