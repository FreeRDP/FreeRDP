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

#ifndef WINPR_SSPICLI_LSA_PROTO_H
#define WINPR_SSPICLI_LSA_PROTO_H

#include <winpr/wtypes.h>
#include <winpr/stream.h>

/* Each message is a fixed envelope (type, flags, requestId) followed by the message's fields,
 * serialized little-endian: scalars are written in place and byte buffers are a UINT32 length
 * followed by that many bytes. The transport (lsa_transport) prepends the UINT32 frame length;
 * this codec produces and consumes the payload without it. */

#define LSA_PROTO_VERSION_MAJOR 1
#define LSA_PROTO_VERSION_MINOR 0

/* Stream framing cap, also the largest LsaCallAuthenticationPackage buffer. */
#define LSA_PROTO_MAX_FRAME (1024UL * 1024UL)

typedef enum
{
	LSA_MSG_HELLO = 1,
	LSA_MSG_LOOKUP_PACKAGE = 2,
	LSA_MSG_CALL_PACKAGE = 3,
	/* TSSSP SSPI proxy: the daemon runs the CredSSP exchange on the client's behalf. */
	SSPI_MSG_ACQUIRE_CRED = 17,
	SSPI_MSG_FREE_CRED = 18,
	SSPI_MSG_ISC = 19,
	SSPI_MSG_DELETE_CTX = 20
} LSA_MSG_TYPE;

#define LSA_MSG_FLAG_RESPONSE 0x0001
#define LSA_MSG_FLAG_ERROR 0x0002

typedef struct
{
	UINT16 type;
	UINT16 flags;
	UINT32 requestId;
} LSA_MSG_HDR;

/* Writer: owns the output stream between init and finish. */
typedef struct
{
	wStream* s;
} LsaProtoWriter;

/* Reader: borrows the framed stream and reads sequentially from it. */
typedef struct
{
	wStream* s;
} LsaProtoReader;

#ifdef __cplusplus
extern "C"
{
#endif

	/* Start a message: allocates the stream and writes the envelope. On success the caller
	 * writes fields, then calls lsa_proto_writer_finish. */
	WINPR_ATTR_NODISCARD
	BOOL lsa_proto_writer_init(LsaProtoWriter* w, const LSA_MSG_HDR* hdr);

	/* Hand back the framed bytes (payload, without the transport length prefix). The writer is
	 * released either way. */
	WINPR_ATTR_NODISCARD
	BOOL lsa_proto_writer_finish(LsaProtoWriter* w, wStream** framed);

	void lsa_proto_writer_abort(LsaProtoWriter* w);

	WINPR_ATTR_NODISCARD BOOL lsa_proto_write_u16(LsaProtoWriter* w, UINT16 v);
	WINPR_ATTR_NODISCARD BOOL lsa_proto_write_u32(LsaProtoWriter* w, UINT32 v);
	WINPR_ATTR_NODISCARD BOOL lsa_proto_write_u64(LsaProtoWriter* w, UINT64 v);
	WINPR_ATTR_NODISCARD BOOL lsa_proto_write_blob(LsaProtoWriter* w, const BYTE* data, UINT32 len);

	/* Parse a framed message (without the transport length prefix): reads the envelope into hdr.
	 * Fields are then read from the reader. */
	WINPR_ATTR_NODISCARD
	BOOL lsa_proto_reader_init(LsaProtoReader* r, wStream* framed, LSA_MSG_HDR* hdr);

	void lsa_proto_reader_done(LsaProtoReader* r);

	WINPR_ATTR_NODISCARD BOOL lsa_proto_read_u16(LsaProtoReader* r, UINT16* v);
	WINPR_ATTR_NODISCARD BOOL lsa_proto_read_u32(LsaProtoReader* r, UINT32* v);
	WINPR_ATTR_NODISCARD BOOL lsa_proto_read_u64(LsaProtoReader* r, UINT64* v);

	/* Read a length-prefixed byte buffer, allocating *data (caller frees). An empty buffer
	 * yields *data == NULL, *len == 0. */
	WINPR_ATTR_NODISCARD
	BOOL lsa_proto_read_blob(LsaProtoReader* r, BYTE** data, UINT32* len);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_SSPICLI_LSA_PROTO_H */
