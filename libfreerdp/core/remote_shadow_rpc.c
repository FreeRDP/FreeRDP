/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Remote session shadowing: RpcShadow2 over SMB (libsmb2)
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

/*
 * A minimal connection oriented DCE/RPC client ([C706], [MS-RPCE]) over a named pipe, for the
 * one call shadowing needs. The server rejects RpcShadow2 below
 * RPC_C_AUTHN_LEVEL_PKT_PRIVACY, so the bind carries NTLM ([MS-RPCE] 3.3.1.5.2) and every
 * request and response is sealed: the header and security trailer are signed, the stub is
 * signed and encrypted.
 */

#include <freerdp/config.h>

#include <fcntl.h>
#ifdef _WIN32
#include <winsock2.h>
#define shadow_poll WSAPoll
#else
#include <errno.h>
#include <poll.h>
#define shadow_poll poll
#endif

#include <winpr/assert.h>
#include <winpr/sysinfo.h>
#include <winpr/stream.h>
#include <winpr/sspi.h>

#include <freerdp/log.h>
#include <freerdp/settings.h>

#include <smb2/smb2.h>
#include <smb2/libsmb2.h>

#include "credssp_auth.h"
#include "settings.h"
#include "utils.h"
#include "remote_shadow.h"

#define SHADOW_PIPE "SessEnvPublicRpc"
/* SMB2_SEC_NTLMSSP: enum smb2_sec is public only since libsmb2 7.0, the value is 1 since 4.0 */
#define SHADOW_SMB2_SEC_NTLMSSP 1
#define SHADOW_READ_SIZE 8192
/* how often a pending SMB call checks whether the connection was aborted */
#define SHADOW_POLL_MS 100
/* closing the pipe and the share after the call, which may have used up its own deadline */
#define SHADOW_CLOSE_TIMEOUT_MS 2000

#define RPC_VERSION 5
#define RPC_PTYPE_REQUEST 0
#define RPC_PTYPE_RESPONSE 2
#define RPC_PTYPE_FAULT 3
#define RPC_PTYPE_BIND 11
#define RPC_PTYPE_BIND_ACK 12
#define RPC_PTYPE_BIND_NAK 13
#define RPC_PTYPE_AUTH3 16
#define RPC_PFC_FIRST_FRAG 0x01u
#define RPC_PFC_LAST_FRAG 0x02u
#define RPC_HEADER_LENGTH 16
#define RPC_REQUEST_HEADER_LENGTH 24
#define RPC_SEC_TRAILER_LENGTH 8
#define RPC_MAX_FRAG 4280
#define RPC_C_AUTHN_WINNT 10
#define RPC_C_AUTHN_LEVEL_PKT_PRIVACY 6
#define RPC_AUTH_CONTEXT_ID 1
#define RPC_SHADOW2_OPNUM 0
/* the PDU header and the security trailer are signed, not encrypted */
#define RPC_SIGNED_ONLY ((ULONG)SECBUFFER_DATA | (ULONG)SECBUFFER_READONLY_WITH_CHECKSUM)

/* SessEnvPublicRpc {1257B580-CE2F-4109-82D6-A9459D0BF6BC} v1.0, wire order */
static const BYTE SessEnvPublicRpcUuid[16] = { 0x80, 0xB5, 0x57, 0x12, 0x2F, 0xCE, 0x09, 0x41,
	                                           0x82, 0xD6, 0xA9, 0x45, 0x9D, 0x0B, 0xF6, 0xBC };
/* NDR transfer syntax {8a885d04-1ceb-11c9-9fe8-08002b104860} v2, wire order */
static const BYTE NdrTransferSyntaxUuid[16] = { 0x04, 0x5D, 0x88, 0x8A, 0xEB, 0x1C, 0xC9, 0x11,
	                                            0x9F, 0xE8, 0x08, 0x00, 0x2B, 0x10, 0x48, 0x60 };

/* completion of the one libsmb2 call in flight */
typedef struct
{
	BOOL done;
	int status;
	void* data;
} ShadowSmbCall;

typedef struct
{
	wLog* log;
	rdpContext* context;
	UINT64 deadline;
	/* aborted, timed out or failed: send nothing more, only free the context */
	BOOL broken;
	ShadowSmbCall call;
	struct smb2_context* smb2;
	struct smb2fh* fh;
	rdpCredsspAuth* auth;
	UINT32 callId;
	ULONG sendSeq;
	ULONG recvSeq;
	wStream* s;
} ShadowRpc;

typedef struct
{
	BYTE ptype;
	BYTE flags;
	UINT16 fragLength;
	UINT16 authLength;
} ShadowRpcHeader;

static void smb_call_done(WINPR_ATTR_UNUSED struct smb2_context* smb2, int status,
                          void* command_data, void* private_data)
{
	ShadowSmbCall* call = private_data;
	WINPR_ASSERT(call);
	call->done = TRUE;
	call->status = status;
	call->data = command_data;
}

static ShadowSmbCall* smb_call_start(ShadowRpc* rpc)
{
	rpc->call = (ShadowSmbCall){ 0 };
	return &rpc->call;
}

/** Logs a libsmb2 call that could not be queued: its callback never runs. */
static BOOL smb_call_queued(ShadowRpc* rpc, int rc, const char* what)
{
	if (rc >= 0)
		return TRUE;
	WLog_Print(rpc->log, WLOG_ERROR, "%s failed: %s", what, smb2_get_error(rpc->smb2));
	return FALSE;
}

/**
 * Runs the libsmb2 event loop until the call in flight completes. Gives up when the connection
 * is aborted (freerdp_abort_connect_context(), a client closing while it reconnects) or the
 * deadline passes, so a consent prompt nobody answers does not block the caller.
 */
static BOOL smb_call_wait(ShadowRpc* rpc, const char* what)
{
	while (!rpc->call.done)
	{
		if (utils_abort_event_is_set(rpc->context->rdp))
		{
			WLog_Print(rpc->log, WLOG_DEBUG, "%s aborted", what);
			rpc->broken = TRUE;
			return FALSE;
		}
		if (GetTickCount64() >= rpc->deadline)
		{
			WLog_Print(rpc->log, WLOG_ERROR, "%s timed out", what);
			rpc->broken = TRUE;
			return FALSE;
		}

		struct pollfd pfd = { .fd = smb2_get_fd(rpc->smb2),
			                  .events = (short)smb2_which_events(rpc->smb2) };
		const int rc = shadow_poll(&pfd, 1, SHADOW_POLL_MS);
		if (rc < 0)
		{
#ifndef _WIN32
			if (errno == EINTR)
				continue;
#endif
			WLog_Print(rpc->log, WLOG_ERROR, "%s: poll failed", what);
			rpc->broken = TRUE;
			return FALSE;
		}
		if ((rc == 0) || (pfd.revents == 0))
			continue;
		if (smb2_service(rpc->smb2, pfd.revents) < 0)
		{
			rpc->broken = TRUE;
			/* the reply was processed before the connection failed */
			if (rpc->call.done)
				break;
			WLog_Print(rpc->log, WLOG_ERROR, "%s failed: %s", what, smb2_get_error(rpc->smb2));
			return FALSE;
		}
	}
	return TRUE;
}

static BOOL pipe_write(ShadowRpc* rpc, const BYTE* data, size_t length)
{
	WINPR_ASSERT(length <= UINT32_MAX);
	ShadowSmbCall* call = smb_call_start(rpc);
	if (!smb_call_queued(
	        rpc, smb2_write_async(rpc->smb2, rpc->fh, data, (uint32_t)length, smb_call_done, call),
	        "writing to \\pipe\\" SHADOW_PIPE) ||
	    !smb_call_wait(rpc, "writing to \\pipe\\" SHADOW_PIPE))
		return FALSE;
	if ((call->status < 0) || ((size_t)call->status != length))
	{
		WLog_Print(rpc->log, WLOG_ERROR, "writing to \\pipe\\" SHADOW_PIPE " failed: %s",
		           smb2_get_error(rpc->smb2));
		return FALSE;
	}
	return TRUE;
}

/** Reads one message (one DCE/RPC PDU) from the pipe into rpc->s, positioned at its start. */
static BOOL pipe_read(ShadowRpc* rpc)
{
	Stream_ResetPosition(rpc->s);
	if (!Stream_EnsureCapacity(rpc->s, SHADOW_READ_SIZE))
		return FALSE;
	ShadowSmbCall* call = smb_call_start(rpc);
	/* this read waits for the answer to RpcShadow2, while the session user decides */
	if (!smb_call_queued(rpc,
	                     smb2_read_async(rpc->smb2, rpc->fh, Stream_Buffer(rpc->s),
	                                     SHADOW_READ_SIZE, smb_call_done, call),
	                     "reading from \\pipe\\" SHADOW_PIPE) ||
	    !smb_call_wait(rpc, "reading from \\pipe\\" SHADOW_PIPE))
		return FALSE;
	const int rc = call->status;
	if (rc <= 0)
	{
		WLog_Print(rpc->log, WLOG_ERROR, "reading from \\pipe\\" SHADOW_PIPE " failed: %s",
		           rc < 0 ? smb2_get_error(rpc->smb2) : "no data");
		return FALSE;
	}
	if (!Stream_SetLength(rpc->s, (size_t)rc))
		return FALSE;
	return TRUE;
}

static void rpc_write_header(wStream* s, BYTE ptype, UINT16 fragLength, UINT16 authLength,
                             UINT32 callId)
{
	Stream_Write_UINT8(s, RPC_VERSION);
	Stream_Write_UINT8(s, 0); /* rpc_vers_minor */
	Stream_Write_UINT8(s, ptype);
	Stream_Write_UINT8(s, RPC_PFC_FIRST_FRAG | RPC_PFC_LAST_FRAG);
	Stream_Write_UINT8(s, 0x10); /* drep: little endian, ASCII, IEEE */
	Stream_Write_UINT8(s, 0);
	Stream_Write_UINT8(s, 0);
	Stream_Write_UINT8(s, 0);
	Stream_Write_UINT16(s, fragLength);
	Stream_Write_UINT16(s, authLength);
	Stream_Write_UINT32(s, callId);
}

static void rpc_write_sec_trailer(wStream* s, BYTE padLength)
{
	Stream_Write_UINT8(s, RPC_C_AUTHN_WINNT);
	Stream_Write_UINT8(s, RPC_C_AUTHN_LEVEL_PKT_PRIVACY);
	Stream_Write_UINT8(s, padLength);
	Stream_Write_UINT8(s, 0); /* auth_reserved */
	Stream_Write_UINT32(s, RPC_AUTH_CONTEXT_ID);
}

static BOOL rpc_read_header(ShadowRpc* rpc, ShadowRpcHeader* header)
{
	wStream* s = rpc->s;

	if (!Stream_CheckAndLogRequiredLengthWLog(rpc->log, s, RPC_HEADER_LENGTH))
		return FALSE;

	const BYTE version = Stream_Get_UINT8(s);
	Stream_Seek_UINT8(s); /* rpc_vers_minor */
	header->ptype = Stream_Get_UINT8(s);
	header->flags = Stream_Get_UINT8(s);
	const BYTE drep = Stream_Get_UINT8(s);
	Stream_Seek(s, 3);
	header->fragLength = Stream_Get_UINT16(s);
	header->authLength = Stream_Get_UINT16(s);
	Stream_Seek_UINT32(s); /* call_id */

	if ((version != RPC_VERSION) || ((drep & 0xF0u) != 0x10u))
	{
		WLog_Print(rpc->log, WLOG_ERROR,
		           "unexpected DCE/RPC header: version %" PRIu8 ", drep 0x%02" PRIx8, version,
		           drep);
		return FALSE;
	}

	if ((header->fragLength != Stream_Length(s)) ||
	    (header->fragLength < RPC_HEADER_LENGTH + header->authLength))
	{
		WLog_Print(rpc->log, WLOG_ERROR,
		           "invalid DCE/RPC PDU: frag_length %" PRIu16 ", auth_length %" PRIu16 ", %" PRIuz
		           " bytes read",
		           header->fragLength, header->authLength, Stream_Length(s));
		return FALSE;
	}
	return TRUE;
}

static BOOL rpc_auth_setup(ShadowRpc* rpc, rdpContext* context)
{
	SEC_WINNT_AUTH_IDENTITY identity = WINPR_C_ARRAY_INIT;
	rdpSettings* settings = context->settings;

	rpc->auth = credssp_auth_new(context);
	if (!rpc->auth)
		return FALSE;

	if (!credssp_auth_init(rpc->auth, NTLM_SSP_NAME, nullptr))
		return FALSE;

	if (!identity_set_from_settings(&identity, settings, FreeRDP_Username, FreeRDP_Domain,
	                                FreeRDP_Password))
		return FALSE;

	const BOOL rc = credssp_auth_setup_client(
	    rpc->auth, "host", freerdp_settings_get_string(settings, FreeRDP_ServerHostname), &identity,
	    nullptr);
	sspi_FreeAuthIdentity(&identity);
	if (!rc)
		return FALSE;

	const ULONG flags = (ULONG)ISC_REQ_USE_DCE_STYLE | (ULONG)ISC_REQ_CONFIDENTIALITY |
	                    (ULONG)ISC_REQ_INTEGRITY | (ULONG)ISC_REQ_REPLAY_DETECT |
	                    (ULONG)ISC_REQ_SEQUENCE_DETECT;
	credssp_auth_set_flags(rpc->auth, flags);

	return credssp_auth_authenticate(rpc->auth) >= 0;
}

/** bind with the NTLM NEGOTIATE token */
static BOOL rpc_bind(ShadowRpc* rpc)
{
	const SecBuffer* token = credssp_auth_get_output_buffer(rpc->auth);
	if (!token || (token->cbBuffer == 0) || (token->cbBuffer > UINT16_MAX - 128))
		return FALSE;

	const UINT16 bodyLength = RPC_HEADER_LENGTH + 8 + 4 + 4 + 20 + 20;
	const UINT16 authLength = (UINT16)token->cbBuffer;
	const UINT16 length = bodyLength + RPC_SEC_TRAILER_LENGTH + authLength;
	wStream* s = rpc->s;

	Stream_ResetPosition(s);
	if (!Stream_EnsureCapacity(s, length))
		return FALSE;

	rpc_write_header(s, RPC_PTYPE_BIND, length, authLength, rpc->callId++);
	Stream_Write_UINT16(s, RPC_MAX_FRAG); /* max_xmit_frag */
	Stream_Write_UINT16(s, RPC_MAX_FRAG); /* max_recv_frag */
	Stream_Write_UINT32(s, 0);            /* assoc_group_id */
	Stream_Write_UINT8(s, 1);             /* n_context_elem */
	Stream_Write_UINT8(s, 0);
	Stream_Write_UINT16(s, 0);
	Stream_Write_UINT16(s, 0); /* p_cont_id */
	Stream_Write_UINT8(s, 1);  /* n_transfer_syn */
	Stream_Write_UINT8(s, 0);
	Stream_Write(s, SessEnvPublicRpcUuid, sizeof(SessEnvPublicRpcUuid));
	Stream_Write_UINT16(s, 1); /* major version */
	Stream_Write_UINT16(s, 0); /* minor version */
	Stream_Write(s, NdrTransferSyntaxUuid, sizeof(NdrTransferSyntaxUuid));
	Stream_Write_UINT32(s, 2);
	rpc_write_sec_trailer(s, 0);
	Stream_Write(s, token->pvBuffer, token->cbBuffer);

	return pipe_write(rpc, Stream_Buffer(s), Stream_GetPosition(s));
}

/** bind_ack: check the context was accepted, feed the NTLM CHALLENGE to the auth context */
static BOOL rpc_read_bind_ack(ShadowRpc* rpc)
{
	ShadowRpcHeader header = WINPR_C_ARRAY_INIT;
	wStream* s = rpc->s;

	if (!pipe_read(rpc) || !rpc_read_header(rpc, &header))
		return FALSE;

	if (header.ptype == RPC_PTYPE_BIND_NAK)
	{
		WLog_Print(rpc->log, WLOG_ERROR, "the server rejected the SessEnvPublicRpc bind");
		return FALSE;
	}
	if (header.ptype != RPC_PTYPE_BIND_ACK)
	{
		WLog_Print(rpc->log, WLOG_ERROR, "expected a DCE/RPC bind_ack, got PDU type %" PRIu8,
		           header.ptype);
		return FALSE;
	}

	if (!Stream_CheckAndLogRequiredLengthWLog(rpc->log, s, 10))
		return FALSE;
	Stream_Seek(s, 8); /* max_xmit_frag, max_recv_frag, assoc_group_id */
	const UINT16 secAddrLength = Stream_Get_UINT16(s);
	if (!Stream_SafeSeek(s, secAddrLength))
		return FALSE;

	/* the result list is 4 byte aligned */
	const size_t pad = (4 - (Stream_GetPosition(s) % 4)) % 4;
	if (!Stream_SafeSeek(s, pad) || !Stream_CheckAndLogRequiredLengthWLog(rpc->log, s, 6))
		return FALSE;
	const BYTE results = Stream_Get_UINT8(s);
	Stream_Seek(s, 3);
	const UINT16 result = Stream_Get_UINT16(s);
	if ((results < 1) || (result != 0))
	{
		WLog_Print(rpc->log, WLOG_ERROR,
		           "the server did not accept the NDR transfer syntax (%" PRIu16 ")", result);
		return FALSE;
	}

	if (header.authLength == 0)
	{
		WLog_Print(rpc->log, WLOG_ERROR, "the bind_ack carries no NTLM challenge");
		return FALSE;
	}

	SecBuffer challenge = { .cbBuffer = header.authLength,
		                    .BufferType = SECBUFFER_TOKEN,
		                    .pvBuffer = malloc(header.authLength) };
	if (!challenge.pvBuffer)
		return FALSE;
	memcpy(challenge.pvBuffer, Stream_Buffer(s) + header.fragLength - header.authLength,
	       header.authLength);
	credssp_auth_take_input_buffer(rpc->auth, &challenge);

	return credssp_auth_authenticate(rpc->auth) >= 0;
}

/** rpc_auth_3 with the NTLM AUTHENTICATE token; the server does not answer it */
static BOOL rpc_auth3(ShadowRpc* rpc)
{
	const SecBuffer* token = credssp_auth_get_output_buffer(rpc->auth);
	if (!token || (token->cbBuffer == 0) || (token->cbBuffer > UINT16_MAX - 128))
		return FALSE;

	const UINT16 authLength = (UINT16)token->cbBuffer;
	const UINT16 length = RPC_HEADER_LENGTH + 4 + RPC_SEC_TRAILER_LENGTH + authLength;
	wStream* s = rpc->s;

	Stream_ResetPosition(s);
	if (!Stream_EnsureCapacity(s, length))
		return FALSE;

	rpc_write_header(s, RPC_PTYPE_AUTH3, length, authLength, rpc->callId++);
	Stream_Write_UINT16(s, RPC_MAX_FRAG); /* max_xmit_frag (unused) */
	Stream_Write_UINT16(s, RPC_MAX_FRAG); /* max_recv_frag (unused) */
	rpc_write_sec_trailer(s, 0);
	Stream_Write(s, token->pvBuffer, token->cbBuffer);

	return pipe_write(rpc, Stream_Buffer(s), Stream_GetPosition(s));
}

static BOOL rpc_seal(ShadowRpc* rpc, BYTE* pdu, size_t stubLength, size_t trailerOffset,
                     size_t signatureLength, BOOL encrypt)
{
	SecurityFunctionTable* table = nullptr;
	CtxtHandle context = WINPR_C_ARRAY_INIT;
	SecBuffer buffers[4] = {
		{ RPC_REQUEST_HEADER_LENGTH, RPC_SIGNED_ONLY, pdu },
		{ (ULONG)stubLength, SECBUFFER_DATA, pdu + RPC_REQUEST_HEADER_LENGTH },
		{ RPC_SEC_TRAILER_LENGTH, RPC_SIGNED_ONLY, pdu + trailerOffset },
		{ (ULONG)signatureLength, SECBUFFER_TOKEN, pdu + trailerOffset + RPC_SEC_TRAILER_LENGTH },
	};
	SecBufferDesc desc = { SECBUFFER_VERSION, ARRAYSIZE(buffers), buffers };

	credssp_auth_tableAndContext(rpc->auth, &table, &context);
	if (!table)
		return FALSE;

	SECURITY_STATUS status = SEC_E_INTERNAL_ERROR;
	if (encrypt)
	{
		WINPR_ASSERT(table->EncryptMessage);
		status = table->EncryptMessage(&context, 0, &desc, rpc->sendSeq++);
	}
	else
	{
		WINPR_ASSERT(table->DecryptMessage);
		status = table->DecryptMessage(&context, &desc, rpc->recvSeq++, nullptr);
	}

	if (status != SEC_E_OK)
	{
		WLog_Print(rpc->log, WLOG_ERROR, "%s failed with %s [0x%08" PRIx32 "]",
		           encrypt ? "EncryptMessage" : "DecryptMessage", GetSecurityStatusString(status),
		           (UINT32)status);
		return FALSE;
	}
	return TRUE;
}

static BOOL rpc_request(ShadowRpc* rpc, UINT16 opnum, const BYTE* stub, size_t stubLength)
{
	const size_t signatureLength = credssp_auth_trailer_size(rpc->auth);
	const size_t pad = (4 - ((RPC_REQUEST_HEADER_LENGTH + stubLength) % 4)) % 4;
	const size_t trailerOffset = RPC_REQUEST_HEADER_LENGTH + stubLength + pad;
	const size_t length = trailerOffset + RPC_SEC_TRAILER_LENGTH + signatureLength;
	wStream* s = rpc->s;

	if ((length > RPC_MAX_FRAG) || (signatureLength == 0))
		return FALSE;

	Stream_ResetPosition(s);
	if (!Stream_EnsureCapacity(s, length))
		return FALSE;
	Stream_Zero(s, length);
	Stream_ResetPosition(s);

	rpc_write_header(s, RPC_PTYPE_REQUEST, (UINT16)length, (UINT16)signatureLength, rpc->callId++);
	Stream_Write_UINT32(s, (UINT32)stubLength); /* alloc_hint */
	Stream_Write_UINT16(s, 0);                  /* p_cont_id */
	Stream_Write_UINT16(s, opnum);
	Stream_Write(s, stub, stubLength);
	Stream_Seek(s, pad);
	rpc_write_sec_trailer(s, (BYTE)pad);

	if (!rpc_seal(rpc, Stream_Buffer(s), stubLength + pad, trailerOffset, signatureLength, TRUE))
		return FALSE;

	return pipe_write(rpc, Stream_Buffer(s), length);
}

/** Reads response PDUs until the last fragment, appending the unsealed stub data to stub. */
static BOOL rpc_read_response(ShadowRpc* rpc, wStream* stub)
{
	for (size_t fragments = 0; fragments < 16; fragments++)
	{
		ShadowRpcHeader header = WINPR_C_ARRAY_INIT;
		wStream* s = rpc->s;

		if (!pipe_read(rpc) || !rpc_read_header(rpc, &header))
			return FALSE;

		if (!Stream_CheckAndLogRequiredLengthWLog(rpc->log, s, 8))
			return FALSE;
		Stream_Seek_UINT32(s); /* alloc_hint */
		Stream_Seek_UINT16(s); /* p_cont_id */
		Stream_Seek_UINT16(s); /* cancel_count, reserved */

		if (header.ptype == RPC_PTYPE_FAULT)
		{
			UINT32 status = 0;
			if (Stream_CheckAndLogRequiredLengthWLog(rpc->log, s, 4))
				status = Stream_Get_UINT32(s);
			WLog_Print(rpc->log, WLOG_ERROR,
			           "RpcShadow2 failed with DCE/RPC fault 0x%08" PRIx32 "%s", status,
			           (status == 5) ? " (access denied)" : "");
			return FALSE;
		}
		if (header.ptype != RPC_PTYPE_RESPONSE)
		{
			WLog_Print(rpc->log, WLOG_ERROR, "expected a DCE/RPC response, got PDU type %" PRIu8,
			           header.ptype);
			return FALSE;
		}

		const size_t signatureLength = header.authLength;
		if ((signatureLength == 0) ||
		    (header.fragLength <
		     RPC_REQUEST_HEADER_LENGTH + RPC_SEC_TRAILER_LENGTH + signatureLength))
		{
			WLog_Print(rpc->log, WLOG_ERROR, "the RpcShadow2 response is not sealed");
			return FALSE;
		}

		const size_t trailerOffset = header.fragLength - signatureLength - RPC_SEC_TRAILER_LENGTH;
		const size_t sealedLength = trailerOffset - RPC_REQUEST_HEADER_LENGTH;
		BYTE* pdu = Stream_Buffer(s);
		const BYTE padLength = pdu[trailerOffset + 2];

		if ((pdu[trailerOffset] != RPC_C_AUTHN_WINNT) ||
		    (pdu[trailerOffset + 1] != RPC_C_AUTHN_LEVEL_PKT_PRIVACY) || (padLength > sealedLength))
		{
			WLog_Print(rpc->log, WLOG_ERROR, "unexpected RpcShadow2 security trailer");
			return FALSE;
		}

		if (!rpc_seal(rpc, pdu, sealedLength, trailerOffset, signatureLength, FALSE))
			return FALSE;

		const size_t stubLength = sealedLength - padLength;
		if (!Stream_EnsureRemainingCapacity(stub, stubLength))
			return FALSE;
		Stream_Write(stub, pdu + RPC_REQUEST_HEADER_LENGTH, stubLength);

		if (header.flags & RPC_PFC_LAST_FRAG)
			return TRUE;
	}

	WLog_Print(rpc->log, WLOG_ERROR, "the RpcShadow2 response has too many fragments");
	return FALSE;
}

BOOL remote_shadow_rpc_shadow2(rdpContext* context, UINT32 timeout, UINT16* permission,
                               UINT32* hresult, char** invitation)
{
	BOOL rc = FALSE;
	BOOL connected = FALSE;
	ShadowSmbCall* call = nullptr;
	ShadowRpc rpc = { .log = WLog_Get(FREERDP_TAG("core.shadow")),
		              .context = context,
		              .deadline = GetTickCount64() + timeout,
		              .callId = 1 };
	wStream* stub = Stream_New(nullptr, SHADOW_READ_SIZE);
	wStream* request = Stream_New(nullptr, 64);
	rdpSettings* settings = context->settings;

	WINPR_ASSERT(invitation);
	*invitation = nullptr;

	const char* host = freerdp_settings_get_string(settings, FreeRDP_ServerHostname);
	const char* user = freerdp_settings_get_string(settings, FreeRDP_Username);
	const char* domain = freerdp_settings_get_string(settings, FreeRDP_Domain);
	const char* password = freerdp_settings_get_string(settings, FreeRDP_Password);

	rpc.s = Stream_New(nullptr, SHADOW_READ_SIZE);
	rpc.smb2 = smb2_init_context();
	if (!host || !rpc.s || !stub || !request || !rpc.smb2)
		goto out;

	smb2_set_security_mode(rpc.smb2, SMB2_NEGOTIATE_SIGNING_ENABLED);
	smb2_set_authentication(rpc.smb2, SHADOW_SMB2_SEC_NTLMSSP);
	if (user)
		smb2_set_user(rpc.smb2, user);
	if (domain && *domain)
		smb2_set_domain(rpc.smb2, domain);
	if (password)
		smb2_set_password(rpc.smb2, password);

	call = smb_call_start(&rpc);
	if ((smb2_connect_share_async(rpc.smb2, host, "IPC$", user, smb_call_done, call) < 0) ||
	    !smb_call_wait(&rpc, "connecting to IPC$") || (call->status < 0))
	{
		if (!rpc.broken || call->done)
			WLog_Print(rpc.log, WLOG_ERROR, "SMB connection to \\\\%s\\IPC$ failed: %s", host,
			           smb2_get_error(rpc.smb2));
		goto out;
	}
	connected = TRUE;
	WLog_Print(rpc.log, WLOG_DEBUG, "SMB session to %s established", host);

	call = smb_call_start(&rpc);
	if (!smb_call_queued(&rpc, smb2_open_async(rpc.smb2, SHADOW_PIPE, O_RDWR, smb_call_done, call),
	                     "opening \\pipe\\" SHADOW_PIPE) ||
	    !smb_call_wait(&rpc, "opening \\pipe\\" SHADOW_PIPE))
		goto out;
	if ((call->status < 0) || !call->data)
	{
		WLog_Print(rpc.log, WLOG_ERROR, "opening \\pipe\\" SHADOW_PIPE " on %s failed: %s", host,
		           smb2_get_error(rpc.smb2));
		goto out;
	}
	rpc.fh = call->data;

	WLog_Print(rpc.log, WLOG_DEBUG, "\\pipe\\" SHADOW_PIPE " opened");
	if (!rpc_auth_setup(&rpc, context) || !rpc_bind(&rpc) || !rpc_read_bind_ack(&rpc) ||
	    !rpc_auth3(&rpc))
	{
		WLog_Print(rpc.log, WLOG_ERROR, "the authenticated SessEnvPublicRpc bind failed");
		goto out;
	}

	WLog_Print(rpc.log, WLOG_DEBUG, "authenticated bind done, calling RpcShadow2");
	if (!remote_shadow_write_shadow2_stub(
	        request, freerdp_settings_get_uint32(settings, FreeRDP_RemoteShadowSessionId),
	        freerdp_settings_get_bool(settings, FreeRDP_RemoteShadowControl),
	        freerdp_settings_get_bool(settings, FreeRDP_RemoteShadowNoConsentPrompt)))
		goto out;

	if (!rpc_request(&rpc, RPC_SHADOW2_OPNUM, Stream_Buffer(request),
	                 Stream_GetPosition(request)) ||
	    !rpc_read_response(&rpc, stub))
		goto out;

	WLog_Print(rpc.log, WLOG_DEBUG, "RpcShadow2 answered");
	Stream_SealLength(stub);
	Stream_ResetPosition(stub);
	rc = remote_shadow_read_shadow2_stub(rpc.log, stub, permission, hresult, invitation);

out:
	/* a broken connection is not closed politely: it would wait for the deadline again */
	rpc.deadline = GetTickCount64() + SHADOW_CLOSE_TIMEOUT_MS;
	if (rpc.fh && !rpc.broken)
	{
		call = smb_call_start(&rpc);
		if (smb2_close_async(rpc.smb2, rpc.fh, smb_call_done, call) == 0)
			(void)smb_call_wait(&rpc, "closing \\pipe\\" SHADOW_PIPE);
	}
	if (connected && !rpc.broken)
	{
		call = smb_call_start(&rpc);
		if (smb2_disconnect_share_async(rpc.smb2, smb_call_done, call) == 0)
			(void)smb_call_wait(&rpc, "disconnecting from IPC$");
	}
	/* cancels what is still in flight: the callbacks point into rpc, which is still alive */
	if (rpc.smb2)
		smb2_destroy_context(rpc.smb2);
	credssp_auth_free(rpc.auth);
	Stream_Free(rpc.s, TRUE);
	Stream_Free(request, TRUE);
	Stream_Free(stub, TRUE);
	return rc;
}
