/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RPC over HTTP
 *
 * Copyright 2012 Fujitsu Technology Solutions GmbH
 * Copyright 2012 Dmitrij Jasnov <dmitrij.jasnov@ts.fujitsu.com>
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/dsparse.h>

#include <openssl/rand.h>

#include "http.h"
#include "ntlm.h"

#include "rpc.h"

/* Security Verification Trailer Signature */

rpc_sec_verification_trailer RPC_SEC_VERIFICATION_TRAILER =
	{ { 0x8a, 0xe3, 0x13, 0x71, 0x02, 0xf4, 0x36, 0x71 } };

/* Syntax UUIDs */

const p_uuid_t TSGU_UUID =
{
	0x44E265DD, /* time_low */
	0x7DAF, /* time_mid */
	0x42CD, /* time_hi_and_version */
	0x85, /* clock_seq_hi_and_reserved */
	0x60, /* clock_seq_low */
	{ 0x3C, 0xDB, 0x6E, 0x7A, 0x27, 0x29 } /* node[6] */
};

#define TSGU_SYNTAX_IF_VERSION	0x00030001

const p_uuid_t NDR_UUID =
{
	0x8A885D04, /* time_low */
	0x1CEB, /* time_mid */
	0x11C9, /* time_hi_and_version */
	0x9F, /* clock_seq_hi_and_reserved */
	0xE8, /* clock_seq_low */
	{ 0x08, 0x00, 0x2B, 0x10, 0x48, 0x60 } /* node[6] */
};

#define NDR_SYNTAX_IF_VERSION	0x00000002

const p_uuid_t BTFN_UUID =
{
	0x6CB71C2C, /* time_low */
	0x9812, /* time_mid */
	0x4540, /* time_hi_and_version */
	0x03, /* clock_seq_hi_and_reserved */
	0x00, /* clock_seq_low */
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } /* node[6] */
};

#define BTFN_SYNTAX_IF_VERSION	0x00000001

extern const RPC_FAULT_CODE RPC_TSG_FAULT_CODES[];

static char* PTYPE_STRINGS[] =
{
	"PTYPE_REQUEST",
	"PTYPE_PING",
	"PTYPE_RESPONSE",
	"PTYPE_FAULT",
	"PTYPE_WORKING",
	"PTYPE_NOCALL",
	"PTYPE_REJECT",
	"PTYPE_ACK",
	"PTYPE_CL_CANCEL",
	"PTYPE_FACK",
	"PTYPE_CANCEL_ACK",
	"PTYPE_BIND",
	"PTYPE_BIND_ACK",
	"PTYPE_BIND_NAK",
	"PTYPE_ALTER_CONTEXT",
	"PTYPE_ALTER_CONTEXT_RESP",
	"PTYPE_RPC_AUTH_3",
	"PTYPE_SHUTDOWN",
	"PTYPE_CO_CANCEL",
	"PTYPE_ORPHANED",
	"PTYPE_RTS",
	""
};

const RPC_FAULT_CODE RPC_FAULT_CODES[] =
{
	DEFINE_RPC_FAULT_CODE(nca_s_fault_object_not_found)
	DEFINE_RPC_FAULT_CODE(nca_s_fault_cancel)
	DEFINE_RPC_FAULT_CODE(nca_s_fault_addr_error)
	DEFINE_RPC_FAULT_CODE(nca_s_fault_context_mismatch)
	DEFINE_RPC_FAULT_CODE(nca_s_fault_fp_div_zero)
	DEFINE_RPC_FAULT_CODE(nca_s_fault_fp_error)
	DEFINE_RPC_FAULT_CODE(nca_s_fault_fp_overflow)
	DEFINE_RPC_FAULT_CODE(nca_s_fault_fp_underflow)
	DEFINE_RPC_FAULT_CODE(nca_s_fault_ill_inst)
	DEFINE_RPC_FAULT_CODE(nca_s_fault_int_div_by_zero)
	DEFINE_RPC_FAULT_CODE(nca_s_fault_int_overflow)
	DEFINE_RPC_FAULT_CODE(nca_s_fault_invalid_bound)
	DEFINE_RPC_FAULT_CODE(nca_s_fault_invalid_tag)
	DEFINE_RPC_FAULT_CODE(nca_s_fault_pipe_closed)
	DEFINE_RPC_FAULT_CODE(nca_s_fault_pipe_comm_error)
	DEFINE_RPC_FAULT_CODE(nca_s_fault_pipe_discipline)
	DEFINE_RPC_FAULT_CODE(nca_s_fault_pipe_empty)
	DEFINE_RPC_FAULT_CODE(nca_s_fault_pipe_memory)
	DEFINE_RPC_FAULT_CODE(nca_s_fault_pipe_order)
	DEFINE_RPC_FAULT_CODE(nca_s_fault_remote_no_memory)
	DEFINE_RPC_FAULT_CODE(nca_s_fault_user_defined)
	DEFINE_RPC_FAULT_CODE(nca_s_fault_tx_open_failed)
	DEFINE_RPC_FAULT_CODE(nca_s_fault_codeset_conv_error)
	DEFINE_RPC_FAULT_CODE(nca_s_fault_no_client_stub)
	DEFINE_RPC_FAULT_CODE(RPC_S_INVALID_STRING_BINDING)
	DEFINE_RPC_FAULT_CODE(RPC_S_WRONG_KIND_OF_BINDING)
	DEFINE_RPC_FAULT_CODE(RPC_S_INVALID_BINDING)
	DEFINE_RPC_FAULT_CODE(RPC_S_PROTSEQ_NOT_SUPPORTED)
	DEFINE_RPC_FAULT_CODE(RPC_S_INVALID_RPC_PROTSEQ)
	DEFINE_RPC_FAULT_CODE(RPC_S_INVALID_STRING_UUID)
	DEFINE_RPC_FAULT_CODE(RPC_S_INVALID_ENDPOINT_FORMAT)
	DEFINE_RPC_FAULT_CODE(RPC_S_INVALID_NET_ADDR)
	DEFINE_RPC_FAULT_CODE(RPC_S_NO_ENDPOINT_FOUND)
	DEFINE_RPC_FAULT_CODE(RPC_S_INVALID_TIMEOUT)
	DEFINE_RPC_FAULT_CODE(RPC_S_OBJECT_NOT_FOUND)
	DEFINE_RPC_FAULT_CODE(RPC_S_ALREADY_REGISTERED)
	DEFINE_RPC_FAULT_CODE(RPC_S_TYPE_ALREADY_REGISTERED)
	DEFINE_RPC_FAULT_CODE(RPC_S_ALREADY_LISTENING)
	DEFINE_RPC_FAULT_CODE(RPC_S_NO_PROTSEQS_REGISTERED)
	DEFINE_RPC_FAULT_CODE(RPC_S_NOT_LISTENING)
	DEFINE_RPC_FAULT_CODE(RPC_S_UNKNOWN_MGR_TYPE)
	DEFINE_RPC_FAULT_CODE(RPC_S_UNKNOWN_IF)
	DEFINE_RPC_FAULT_CODE(RPC_S_NO_BINDINGS)
	DEFINE_RPC_FAULT_CODE(RPC_S_NO_PROTSEQS)
	DEFINE_RPC_FAULT_CODE(RPC_S_CANT_CREATE_ENDPOINT)
	DEFINE_RPC_FAULT_CODE(RPC_S_OUT_OF_RESOURCES)
	DEFINE_RPC_FAULT_CODE(RPC_S_SERVER_UNAVAILABLE)
	DEFINE_RPC_FAULT_CODE(RPC_S_SERVER_TOO_BUSY)
	DEFINE_RPC_FAULT_CODE(RPC_S_INVALID_NETWORK_OPTIONS)
	DEFINE_RPC_FAULT_CODE(RPC_S_NO_CALL_ACTIVE)
	DEFINE_RPC_FAULT_CODE(RPC_S_CALL_FAILED)
	DEFINE_RPC_FAULT_CODE(RPC_S_CALL_FAILED_DNE)
	DEFINE_RPC_FAULT_CODE(RPC_S_PROTOCOL_ERROR)
	DEFINE_RPC_FAULT_CODE(RPC_S_PROXY_ACCESS_DENIED)
	DEFINE_RPC_FAULT_CODE(RPC_S_UNSUPPORTED_TRANS_SYN)
	DEFINE_RPC_FAULT_CODE(RPC_S_UNSUPPORTED_TYPE)
	DEFINE_RPC_FAULT_CODE(RPC_S_INVALID_TAG)
	DEFINE_RPC_FAULT_CODE(RPC_S_INVALID_BOUND)
	DEFINE_RPC_FAULT_CODE(RPC_S_NO_ENTRY_NAME)
	DEFINE_RPC_FAULT_CODE(RPC_S_INVALID_NAME_SYNTAX)
	DEFINE_RPC_FAULT_CODE(RPC_S_UNSUPPORTED_NAME_SYNTAX)
	DEFINE_RPC_FAULT_CODE(RPC_S_UUID_NO_ADDRESS)
	DEFINE_RPC_FAULT_CODE(RPC_S_DUPLICATE_ENDPOINT)
	DEFINE_RPC_FAULT_CODE(RPC_S_UNKNOWN_AUTHN_TYPE)
	DEFINE_RPC_FAULT_CODE(RPC_S_MAX_CALLS_TOO_SMALL)
	DEFINE_RPC_FAULT_CODE(RPC_S_STRING_TOO_LONG)
	DEFINE_RPC_FAULT_CODE(RPC_S_PROTSEQ_NOT_FOUND)
	DEFINE_RPC_FAULT_CODE(RPC_S_PROCNUM_OUT_OF_RANGE)
	DEFINE_RPC_FAULT_CODE(RPC_S_BINDING_HAS_NO_AUTH)
	DEFINE_RPC_FAULT_CODE(RPC_S_UNKNOWN_AUTHN_SERVICE)
	DEFINE_RPC_FAULT_CODE(RPC_S_UNKNOWN_AUTHN_LEVEL)
	DEFINE_RPC_FAULT_CODE(RPC_S_INVALID_AUTH_IDENTITY)
	DEFINE_RPC_FAULT_CODE(RPC_S_UNKNOWN_AUTHZ_SERVICE)
	DEFINE_RPC_FAULT_CODE(EPT_S_INVALID_ENTRY)
	DEFINE_RPC_FAULT_CODE(EPT_S_CANT_PERFORM_OP)
	DEFINE_RPC_FAULT_CODE(EPT_S_NOT_REGISTERED)
	DEFINE_RPC_FAULT_CODE(RPC_S_NOTHING_TO_EXPORT)
	DEFINE_RPC_FAULT_CODE(RPC_S_INCOMPLETE_NAME)
	DEFINE_RPC_FAULT_CODE(RPC_S_INVALID_VERS_OPTION)
	DEFINE_RPC_FAULT_CODE(RPC_S_NO_MORE_MEMBERS)
	DEFINE_RPC_FAULT_CODE(RPC_S_NOT_ALL_OBJS_UNEXPORTED)
	DEFINE_RPC_FAULT_CODE(RPC_S_INTERFACE_NOT_FOUND)
	DEFINE_RPC_FAULT_CODE(RPC_S_ENTRY_ALREADY_EXISTS)
	DEFINE_RPC_FAULT_CODE(RPC_S_ENTRY_NOT_FOUND)
	DEFINE_RPC_FAULT_CODE(RPC_S_NAME_SERVICE_UNAVAILABLE)
	DEFINE_RPC_FAULT_CODE(RPC_S_INVALID_NAF_ID)
	DEFINE_RPC_FAULT_CODE(RPC_S_CANNOT_SUPPORT)
	DEFINE_RPC_FAULT_CODE(RPC_S_NO_CONTEXT_AVAILABLE)
	DEFINE_RPC_FAULT_CODE(RPC_S_INTERNAL_ERROR)
	DEFINE_RPC_FAULT_CODE(RPC_S_ZERO_DIVIDE)
	DEFINE_RPC_FAULT_CODE(RPC_S_ADDRESS_ERROR)
	DEFINE_RPC_FAULT_CODE(RPC_S_FP_DIV_ZERO)
	DEFINE_RPC_FAULT_CODE(RPC_S_FP_UNDERFLOW)
	DEFINE_RPC_FAULT_CODE(RPC_S_FP_OVERFLOW)
	DEFINE_RPC_FAULT_CODE(RPC_X_NO_MORE_ENTRIES)
	DEFINE_RPC_FAULT_CODE(RPC_X_SS_CHAR_TRANS_OPEN_FAIL)
	DEFINE_RPC_FAULT_CODE(RPC_X_SS_CHAR_TRANS_SHORT_FILE)
	DEFINE_RPC_FAULT_CODE(RPC_X_SS_IN_NULL_CONTEXT)
	DEFINE_RPC_FAULT_CODE(RPC_X_SS_CONTEXT_DAMAGED)
	DEFINE_RPC_FAULT_CODE(RPC_X_SS_HANDLES_MISMATCH)
	DEFINE_RPC_FAULT_CODE(RPC_X_SS_CANNOT_GET_CALL_HANDLE)
	DEFINE_RPC_FAULT_CODE(RPC_X_NULL_REF_POINTER)
	DEFINE_RPC_FAULT_CODE(RPC_X_ENUM_VALUE_OUT_OF_RANGE)
	DEFINE_RPC_FAULT_CODE(RPC_X_BYTE_COUNT_TOO_SMALL)
	DEFINE_RPC_FAULT_CODE(RPC_X_BAD_STUB_DATA)
	{ 0, NULL }
};

void rpc_pdu_header_print(rpcconn_hdr_t* header)
{
	printf("rpc_vers: %d\n", header->common.rpc_vers);
	printf("rpc_vers_minor: %d\n", header->common.rpc_vers_minor);

	if (header->common.ptype > PTYPE_RTS)
		printf("ptype: %s (%d)\n", "PTYPE_UNKNOWN", header->common.ptype);
	else
		printf("ptype: %s (%d)\n", PTYPE_STRINGS[header->common.ptype], header->common.ptype);

	printf("pfc_flags (0x%02X) = {", header->common.pfc_flags);
	if (header->common.pfc_flags & PFC_FIRST_FRAG)
		printf(" PFC_FIRST_FRAG");
	if (header->common.pfc_flags & PFC_LAST_FRAG)
		printf(" PFC_LAST_FRAG");
	if (header->common.pfc_flags & PFC_PENDING_CANCEL)
		printf(" PFC_PENDING_CANCEL");
	if (header->common.pfc_flags & PFC_RESERVED_1)
		printf(" PFC_RESERVED_1");
	if (header->common.pfc_flags & PFC_CONC_MPX)
		printf(" PFC_CONC_MPX");
	if (header->common.pfc_flags & PFC_DID_NOT_EXECUTE)
		printf(" PFC_DID_NOT_EXECUTE");
	if (header->common.pfc_flags & PFC_OBJECT_UUID)
		printf(" PFC_OBJECT_UUID");
	printf(" }\n");

	printf("packed_drep[4]: %02X %02X %02X %02X\n",
			header->common.packed_drep[0], header->common.packed_drep[1],
			header->common.packed_drep[2], header->common.packed_drep[3]);

	printf("frag_length: %d\n", header->common.frag_length);
	printf("auth_length: %d\n", header->common.auth_length);
	printf("call_id: %d\n", header->common.call_id);

	if (header->common.ptype == PTYPE_RESPONSE)
	{
		printf("alloc_hint: %d\n", header->response.alloc_hint);
		printf("p_cont_id: %d\n", header->response.p_cont_id);
		printf("cancel_count: %d\n", header->response.cancel_count);
		printf("reserved: %d\n", header->response.reserved);
	}
}

STREAM* rpc_ntlm_http_request(rdpRpc* rpc, SecBuffer* ntlm_token, int content_length, TSG_CHANNEL channel)
{
	STREAM* s;
	char* base64_ntlm_token;
	HttpContext* http_context;
	HttpRequest* http_request;

	http_request = http_request_new();
	base64_ntlm_token = crypto_base64_encode(ntlm_token->pvBuffer, ntlm_token->cbBuffer);

	if (channel == TSG_CHANNEL_IN)
	{
		http_context = rpc->NtlmHttpIn->context;
		http_request_set_method(http_request, "RPC_IN_DATA");
	}
	else if (channel == TSG_CHANNEL_OUT)
	{
		http_context = rpc->NtlmHttpOut->context;
		http_request_set_method(http_request, "RPC_OUT_DATA");
	}
	else
	{
		return NULL;
	}

	http_request->ContentLength = content_length;
	http_request_set_uri(http_request, http_context->URI);

	http_request_set_auth_scheme(http_request, "NTLM");
	http_request_set_auth_param(http_request, base64_ntlm_token);

	s = http_request_write(http_context, http_request);
	http_request_free(http_request);

	free(base64_ntlm_token);

	return s;
}

BOOL rpc_ntlm_http_out_connect(rdpRpc* rpc)
{
	STREAM* s;
	rdpSettings* settings;
	int ntlm_token_length;
	BYTE* ntlm_token_data;
	HttpResponse* http_response;
	rdpNtlm* ntlm = rpc->NtlmHttpOut->ntlm;

	settings = rpc->settings;

	if (settings->GatewayUseSameCredentials)
	{
		ntlm_client_init(ntlm, TRUE, settings->Username,
			settings->Domain, settings->Password);
		ntlm_client_make_spn(ntlm, _T("HTTP"), settings->GatewayHostname);
	}
	else
	{
		ntlm_client_init(ntlm, TRUE, settings->GatewayUsername,
			settings->GatewayDomain, settings->GatewayPassword);
		ntlm_client_make_spn(ntlm, _T("HTTP"), settings->GatewayHostname);
	}

	ntlm_authenticate(ntlm);

	s = rpc_ntlm_http_request(rpc, &ntlm->outputBuffer, 0, TSG_CHANNEL_OUT);

	/* Send OUT Channel Request */

	DEBUG_RPC("\n%s", s->data);
	rpc_out_write(rpc, s->data, s->size);
	stream_free(s);

	/* Receive OUT Channel Response */

	http_response = http_response_recv(rpc->TlsOut);

	ntlm_token_data = NULL;
	crypto_base64_decode((BYTE*) http_response->AuthParam, strlen(http_response->AuthParam),
			&ntlm_token_data, &ntlm_token_length);

	ntlm->inputBuffer.pvBuffer = ntlm_token_data;
	ntlm->inputBuffer.cbBuffer = ntlm_token_length;

	ntlm_authenticate(ntlm);

	http_response_free(http_response);

	s = rpc_ntlm_http_request(rpc, &ntlm->outputBuffer, 76, TSG_CHANNEL_OUT);

	/* Send OUT Channel Request */

	DEBUG_RPC("\n%s", s->data);
	rpc_out_write(rpc, s->data, s->size);
	stream_free(s);

	ntlm_client_uninit(ntlm);
	ntlm_free(ntlm);

	return TRUE;
}

BOOL rpc_ntlm_http_in_connect(rdpRpc* rpc)
{
	STREAM* s;
	rdpSettings* settings;
	int ntlm_token_length;
	BYTE* ntlm_token_data;
	HttpResponse* http_response;
	rdpNtlm* ntlm = rpc->NtlmHttpIn->ntlm;

	settings = rpc->settings;

	if (settings->GatewayUseSameCredentials)
	{
		ntlm_client_init(ntlm, TRUE, settings->Username,
			settings->Domain, settings->Password);
		ntlm_client_make_spn(ntlm, _T("HTTP"), settings->GatewayHostname);
	}
	else
	{
		ntlm_client_init(ntlm, TRUE, settings->GatewayUsername,
			settings->GatewayDomain, settings->GatewayPassword);
		ntlm_client_make_spn(ntlm, _T("HTTP"), settings->GatewayHostname);
	}

	ntlm_authenticate(ntlm);

	s = rpc_ntlm_http_request(rpc, &ntlm->outputBuffer, 0, TSG_CHANNEL_IN);

	/* Send IN Channel Request */

	DEBUG_RPC("\n%s", s->data);
	rpc_in_write(rpc, s->data, s->size);
	stream_free(s);

	/* Receive IN Channel Response */

	http_response = http_response_recv(rpc->TlsIn);

	ntlm_token_data = NULL;
	crypto_base64_decode((BYTE*) http_response->AuthParam, strlen(http_response->AuthParam),
			&ntlm_token_data, &ntlm_token_length);

	ntlm->inputBuffer.pvBuffer = ntlm_token_data;
	ntlm->inputBuffer.cbBuffer = ntlm_token_length;

	ntlm_authenticate(ntlm);

	http_response_free(http_response);

	s = rpc_ntlm_http_request(rpc, &ntlm->outputBuffer, 0x40000000, TSG_CHANNEL_IN);

	/* Send IN Channel Request */

	DEBUG_RPC("\n%s", s->data);
	rpc_in_write(rpc, s->data, s->size);
	stream_free(s);

	ntlm_client_uninit(ntlm);
	ntlm_free(ntlm);

	return TRUE;
}

void rpc_pdu_header_init(rdpRpc* rpc, rpcconn_hdr_t* header)
{
	header->common.rpc_vers = rpc->rpc_vers;
	header->common.rpc_vers_minor = rpc->rpc_vers_minor;
	header->common.packed_drep[0] = rpc->packed_drep[0];
	header->common.packed_drep[1] = rpc->packed_drep[1];
	header->common.packed_drep[2] = rpc->packed_drep[2];
	header->common.packed_drep[3] = rpc->packed_drep[3];
}

UINT32 rpc_offset_align(UINT32* offset, UINT32 alignment)
{
	UINT32 pad;

	pad = *offset;
	*offset = (*offset + alignment - 1) & ~(alignment - 1);
	pad = *offset - pad;

	return pad;
}

UINT32 rpc_offset_pad(UINT32* offset, UINT32 pad)
{
	*offset += pad;
	return pad;
}

BOOL rpc_send_bind_pdu(rdpRpc* rpc)
{
	BYTE* buffer;
	UINT32 offset;
	p_cont_elem_t* p_cont_elem;
	rpcconn_bind_hdr_t* bind_pdu;
	rdpSettings* settings = rpc->settings;

	rpc->ntlm = ntlm_new();

	DEBUG_RPC("Sending bind PDU");

	ntlm_client_init(rpc->ntlm, FALSE, settings->Username, settings->Domain, settings->Password);

	ntlm_authenticate(rpc->ntlm);

	bind_pdu = (rpcconn_bind_hdr_t*) malloc(sizeof(rpcconn_bind_hdr_t));
	ZeroMemory(bind_pdu, sizeof(rpcconn_bind_hdr_t));

	rpc_pdu_header_init(rpc, (rpcconn_hdr_t*) bind_pdu);

	bind_pdu->auth_length = rpc->ntlm->outputBuffer.cbBuffer;
	bind_pdu->auth_verifier.auth_value = rpc->ntlm->outputBuffer.pvBuffer;

	bind_pdu->ptype = PTYPE_BIND;
	bind_pdu->pfc_flags = PFC_FIRST_FRAG | PFC_LAST_FRAG | PFC_PENDING_CANCEL | PFC_CONC_MPX;
	bind_pdu->call_id = 2;

	bind_pdu->max_xmit_frag = rpc->max_xmit_frag;
	bind_pdu->max_recv_frag = rpc->max_recv_frag;
	bind_pdu->assoc_group_id = 0;

	bind_pdu->p_context_elem.n_context_elem = 2;
	bind_pdu->p_context_elem.reserved = 0;
	bind_pdu->p_context_elem.reserved2 = 0;

	bind_pdu->p_context_elem.p_cont_elem = malloc(sizeof(p_cont_elem_t) * bind_pdu->p_context_elem.n_context_elem);

	p_cont_elem = &bind_pdu->p_context_elem.p_cont_elem[0];

	p_cont_elem->p_cont_id = 0;
	p_cont_elem->n_transfer_syn = 1;
	p_cont_elem->reserved = 0;
	CopyMemory(&(p_cont_elem->abstract_syntax.if_uuid), &TSGU_UUID, sizeof(p_uuid_t));
	p_cont_elem->abstract_syntax.if_version = TSGU_SYNTAX_IF_VERSION;

	p_cont_elem->transfer_syntaxes = malloc(sizeof(p_syntax_id_t));
	CopyMemory(&(p_cont_elem->transfer_syntaxes[0].if_uuid), &NDR_UUID, sizeof(p_uuid_t));
	p_cont_elem->transfer_syntaxes[0].if_version = NDR_SYNTAX_IF_VERSION;

	p_cont_elem = &bind_pdu->p_context_elem.p_cont_elem[1];

	p_cont_elem->p_cont_id = 1;
	p_cont_elem->n_transfer_syn = 1;
	p_cont_elem->reserved = 0;
	CopyMemory(&(p_cont_elem->abstract_syntax.if_uuid), &TSGU_UUID, sizeof(p_uuid_t));
	p_cont_elem->abstract_syntax.if_version = TSGU_SYNTAX_IF_VERSION;

	p_cont_elem->transfer_syntaxes = malloc(sizeof(p_syntax_id_t));
	CopyMemory(&(p_cont_elem->transfer_syntaxes[0].if_uuid), &BTFN_UUID, sizeof(p_uuid_t));
	p_cont_elem->transfer_syntaxes[0].if_version = BTFN_SYNTAX_IF_VERSION;

	offset = 116;
	bind_pdu->auth_verifier.auth_pad_length = rpc_offset_align(&offset, 4);

	bind_pdu->auth_verifier.auth_type = RPC_C_AUTHN_WINNT;
	bind_pdu->auth_verifier.auth_level = RPC_C_AUTHN_LEVEL_PKT_INTEGRITY;
	bind_pdu->auth_verifier.auth_reserved = 0x00;
	bind_pdu->auth_verifier.auth_context_id = 0x00000000;
	offset += (8 + bind_pdu->auth_length);

	bind_pdu->frag_length = offset;

	buffer = (BYTE*) malloc(bind_pdu->frag_length);

	CopyMemory(buffer, bind_pdu, 24);
	CopyMemory(&buffer[24], &bind_pdu->p_context_elem, 4);
	CopyMemory(&buffer[28], &bind_pdu->p_context_elem.p_cont_elem[0], 24);
	CopyMemory(&buffer[52], bind_pdu->p_context_elem.p_cont_elem[0].transfer_syntaxes, 20);
	CopyMemory(&buffer[72], &bind_pdu->p_context_elem.p_cont_elem[1], 24);
	CopyMemory(&buffer[96], bind_pdu->p_context_elem.p_cont_elem[1].transfer_syntaxes, 20);

	offset = 116;
	rpc_offset_pad(&offset, bind_pdu->auth_verifier.auth_pad_length);

	CopyMemory(&buffer[offset], &bind_pdu->auth_verifier.auth_type, 8);
	CopyMemory(&buffer[offset + 8], bind_pdu->auth_verifier.auth_value, bind_pdu->auth_length);
	offset += (8 + bind_pdu->auth_length);

	rpc_in_write(rpc, buffer, bind_pdu->frag_length);

	free(bind_pdu->p_context_elem.p_cont_elem[0].transfer_syntaxes);
	free(bind_pdu->p_context_elem.p_cont_elem[1].transfer_syntaxes);
	free(bind_pdu->p_context_elem.p_cont_elem);
	free(bind_pdu);
	free(buffer);

	return TRUE;
}

int rpc_recv_bind_ack_pdu(rdpRpc* rpc)
{
	int status;
	BYTE* auth_data;
	rpcconn_hdr_t* header;

	status = rpc_recv_pdu(rpc);

	if (status > 0)
	{
		header = (rpcconn_hdr_t*) rpc->buffer;

		rpc->ntlm->inputBuffer.cbBuffer = header->common.auth_length;
		rpc->ntlm->inputBuffer.pvBuffer = malloc(header->common.auth_length);

		auth_data = rpc->buffer + (header->common.frag_length - header->common.auth_length);
		CopyMemory(rpc->ntlm->inputBuffer.pvBuffer, auth_data, header->common.auth_length);

		ntlm_authenticate(rpc->ntlm);
	}

	return status;
}

BOOL rpc_send_rpc_auth_3_pdu(rdpRpc* rpc)
{
	BYTE* buffer;
	UINT32 offset;
	rpcconn_rpc_auth_3_hdr_t* auth_3_pdu;

	DEBUG_RPC("Sending auth_3 PDU");

	auth_3_pdu = (rpcconn_rpc_auth_3_hdr_t*) malloc(sizeof(rpcconn_rpc_auth_3_hdr_t));
	ZeroMemory(auth_3_pdu, sizeof(rpcconn_rpc_auth_3_hdr_t));

	rpc_pdu_header_init(rpc, (rpcconn_hdr_t*) auth_3_pdu);

	auth_3_pdu->auth_length = rpc->ntlm->outputBuffer.cbBuffer;
	auth_3_pdu->auth_verifier.auth_value = rpc->ntlm->outputBuffer.pvBuffer;

	auth_3_pdu->ptype = PTYPE_RPC_AUTH_3;
	auth_3_pdu->pfc_flags = PFC_FIRST_FRAG | PFC_LAST_FRAG | PFC_CONC_MPX;
	auth_3_pdu->call_id = 2;

	offset = 20;

	auth_3_pdu->max_xmit_frag = rpc->max_xmit_frag;
	auth_3_pdu->max_recv_frag = rpc->max_recv_frag;

	offset += 4;
	auth_3_pdu->auth_verifier.auth_pad_length = rpc_offset_align(&offset, 4);

	auth_3_pdu->auth_verifier.auth_type = RPC_C_AUTHN_WINNT;
	auth_3_pdu->auth_verifier.auth_level = RPC_C_AUTHN_LEVEL_PKT_INTEGRITY;
	auth_3_pdu->auth_verifier.auth_reserved = 0x00;
	auth_3_pdu->auth_verifier.auth_context_id = 0x00000000;

	auth_3_pdu->frag_length = 20 + 4 +
			auth_3_pdu->auth_verifier.auth_pad_length + auth_3_pdu->auth_length + 8;

	buffer = (BYTE*) malloc(auth_3_pdu->frag_length);

	CopyMemory(buffer, auth_3_pdu, 24);

	offset = 24;
	rpc_offset_pad(&offset, auth_3_pdu->auth_verifier.auth_pad_length);

	CopyMemory(&buffer[offset], &auth_3_pdu->auth_verifier.auth_type, 8);
	CopyMemory(&buffer[offset + 8], auth_3_pdu->auth_verifier.auth_value, auth_3_pdu->auth_length);
	offset += (8 + auth_3_pdu->auth_length);

	rpc_in_write(rpc, buffer, auth_3_pdu->frag_length);

	free(auth_3_pdu);
	free(buffer);

	return TRUE;
}

int rpc_recv_fault_pdu(rpcconn_hdr_t* header)
{
	int index;

	printf("RPC Fault PDU:\n");

	for (index = 0; RPC_FAULT_CODES[index].name != NULL; index++)
	{
		if (RPC_FAULT_CODES[index].code == header->fault.status)
		{
			printf("status: %s (0x%08X)\n", RPC_FAULT_CODES[index].name, header->fault.status);
			return 0;
		}
	}

	for (index = 0; RPC_FAULT_CODES[index].name != NULL; index++)
	{
		if (RPC_TSG_FAULT_CODES[index].code == header->fault.status)
		{
			printf("status: %s (0x%08X)\n", RPC_TSG_FAULT_CODES[index].name, header->fault.status);
			return 0;
		}
	}

	printf("status: %s (0x%08X)\n", "UNKNOWN", header->fault.status);

	return 0;
}

/**
 * PDU Structure with verification trailer
 *
 * MUST only appear in a request PDU!
 *  ________________________________
 * |                                |
 * |           PDU Header           |
 * |________________________________| _______
 * |                                |   /|\
 * |                                |    |
 * |           Stub Data            |    |
 * |                                |    |
 * |________________________________|    |
 * |                                | PDU Body
 * |            Stub Pad            |    |
 * |________________________________|    |
 * |                                |    |
 * |      Verification Trailer      |    |
 * |________________________________|    |
 * |                                |    |
 * |       Authentication Pad       |    |
 * |________________________________| __\|/__
 * |                                |
 * |        Security Trailer        |
 * |________________________________|
 * |                                |
 * |      Authentication Token      |
 * |________________________________|
 *
 */

BOOL rpc_get_stub_data_info(rdpRpc* rpc, BYTE* buffer, UINT32* offset, UINT32* length)
{
	UINT32 alloc_hint = 0;
	rpcconn_hdr_t* header;

	*offset = RPC_COMMON_FIELDS_LENGTH;
	header = ((rpcconn_hdr_t*) buffer);

	if (header->common.ptype == PTYPE_RESPONSE)
	{
		*offset += 4;
		rpc_offset_align(offset, 8);
		alloc_hint = header->response.alloc_hint;
	}
	else if (header->common.ptype == PTYPE_REQUEST)
	{
		*offset += 4;
		rpc_offset_align(offset, 8);
		alloc_hint = header->request.alloc_hint;
	}
	else if (header->common.ptype == PTYPE_RTS)
	{
		*offset += 4;
	}
	else
	{
		return FALSE;
	}

	if (length)
	{
		if (header->common.ptype == PTYPE_REQUEST)
		{
			UINT32 sec_trailer_offset;

			/**
			 * All PDUs that carry sec_trailer information share certain common fields:
			 * frag_length and auth_length. The beginning of the sec_trailer structure
			 * for each PDU MUST be calculated to start from offset
			 * (frag_length – auth_length – 8) from the beginning of the PDU.
			 */

			sec_trailer_offset = header->common.frag_length - header->common.auth_length - 8;
			*length = sec_trailer_offset - *offset;
		}
		else
		{
			BYTE auth_pad_length;

			auth_pad_length = *(buffer + header->common.frag_length - header->common.auth_length - 6);
			*length = header->common.frag_length - (header->common.auth_length + *offset + 8 + auth_pad_length);
		}
	}

	return TRUE;
}

int rpc_send_enqueue_pdu(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	RPC_PDU_ENTRY* PduEntry;

	PduEntry = (RPC_PDU_ENTRY*) _aligned_malloc(sizeof(RPC_PDU_ENTRY), MEMORY_ALLOCATION_ALIGNMENT);
	PduEntry->Buffer = buffer;
	PduEntry->Length = length;

	InterlockedPushEntrySList(rpc->SendQueue, &(PduEntry->ItemEntry));

	return 0;
}

int rpc_send_dequeue_pdu(rdpRpc* rpc)
{
	int status;
	RPC_PDU_ENTRY* PduEntry;

	PduEntry = (RPC_PDU_ENTRY*) InterlockedPopEntrySList(rpc->SendQueue);

	status = rpc_in_write(rpc, PduEntry->Buffer, PduEntry->Length);

	/*
	 * This protocol specifies that only RPC PDUs are subject to the flow control abstract
	 * data model. RTS PDUs and the HTTP request and response headers are not subject to flow control.
	 * Implementations of this protocol MUST NOT include them when computing any of the variables
	 * specified by this abstract data model.
	 */
	rpc->VirtualConnection->DefaultInChannel->BytesSent += status;
	rpc->VirtualConnection->DefaultInChannel->SenderAvailableWindow -= status;

	_aligned_free(PduEntry);

	return status;
}

int rpc_out_read(rdpRpc* rpc, BYTE* data, int length)
{
	int status;

	status = tls_read(rpc->TlsOut, data, length);

	return status;
}

int rpc_out_write(rdpRpc* rpc, BYTE* data, int length)
{
	int status;

	status = tls_write_all(rpc->TlsOut, data, length);

	return status;
}

int rpc_in_write(rdpRpc* rpc, BYTE* data, int length)
{
	int status;

#ifdef WITH_DEBUG_TSG
	rpc_pdu_header_print((rpcconn_hdr_t*) data);
	printf("Sending PDU (length: %d)\n", length);
	freerdp_hexdump(data, length);
#endif
	
	status = tls_write_all(rpc->TlsIn, data, length);

	return status;
}

int rpc_recv_pdu_header(rdpRpc* rpc, BYTE* header)
{
	int status;
	int bytesRead;
	UINT32 offset;
    
	/* Read common header fields */
    
	bytesRead = 0;
    
	while (bytesRead < RPC_COMMON_FIELDS_LENGTH)
	{
		status = rpc_out_read(rpc, &header[bytesRead], RPC_COMMON_FIELDS_LENGTH - bytesRead);
        
		if (status < 0)
		{
			printf("rpc_recv_pdu_header: error reading header\n");
			return status;
		}
        
		bytesRead += status;
	}

	rpc_get_stub_data_info(rpc, header, &offset, NULL);

	while (bytesRead < offset)
	{
		status = rpc_out_read(rpc, &header[bytesRead], offset - bytesRead);
        
		if (status < 0)
			return status;
        
		bytesRead += status;
	}
    
	return bytesRead;
}

int rpc_recv_pdu(rdpRpc* rpc)
{
	int status;
	int headerLength;
	int bytesRead = 0;
	rpcconn_hdr_t* header;

	status = rpc_recv_pdu_header(rpc, rpc->buffer);
    
	if (status < 1)
	{
		printf("rpc_recv_pdu_header: error reading header\n");
		return status;
	}
    
	headerLength = status;
	header = (rpcconn_hdr_t*) rpc->buffer;
	bytesRead += status;

	rpc_pdu_header_print(header);
	
	if (header->common.frag_length > rpc->length)
	{
		rpc->length = header->common.frag_length;
		rpc->buffer = (BYTE*) realloc(rpc->buffer, rpc->length);
		header = (rpcconn_hdr_t*) rpc->buffer;
	}

	while (bytesRead < header->common.frag_length)
	{
		status = rpc_out_read(rpc, &rpc->buffer[bytesRead], header->common.frag_length - bytesRead);

		if (status < 0)
		{
			printf("rpc_recv_pdu: error reading fragment\n");
			return status;
		}

		bytesRead += status;
	}

	if (!(header->common.pfc_flags & PFC_LAST_FRAG))
	{
		printf("Fragmented PDU\n");
	}

	if (header->common.ptype == PTYPE_RTS) /* RTS PDU */
	{
		if (rpc->VirtualConnection->State < VIRTUAL_CONNECTION_STATE_OPENED)
			return header->common.frag_length;

		printf("Receiving Out-of-Sequence RTS PDU\n");
		rts_recv_out_of_sequence_pdu(rpc);
		return rpc_recv_pdu(rpc);
	}
	else if (header->common.ptype == PTYPE_FAULT)
	{
		rpc_recv_fault_pdu(header);
		return -1;
	}

	rpc->VirtualConnection->DefaultOutChannel->BytesReceived += header->common.frag_length;
	rpc->VirtualConnection->DefaultOutChannel->ReceiverAvailableWindow -= header->common.frag_length;

	printf("BytesReceived: %d ReceiverAvailableWindow: %d ReceiveWindow: %d\n",
			rpc->VirtualConnection->DefaultOutChannel->BytesReceived,
			rpc->VirtualConnection->DefaultOutChannel->ReceiverAvailableWindow,
			rpc->ReceiveWindow);

	if (rpc->VirtualConnection->DefaultOutChannel->ReceiverAvailableWindow < (rpc->ReceiveWindow / 2))
	{
		printf("Sending Flow Control Ack PDU\n");
		rts_send_flow_control_ack_pdu(rpc);
	}

#ifdef WITH_DEBUG_RPC
	printf("rpc_recv_pdu: length: %d\n", header->common.frag_length);
	freerdp_hexdump(rpc->buffer, header->common.frag_length);
	printf("\n");
#endif

	return header->common.frag_length;
}

int rpc_tsg_write(rdpRpc* rpc, BYTE* data, int length, UINT16 opnum)
{
	int status;
	BYTE* buffer;
	UINT32 offset;
	rdpNtlm* ntlm;
	UINT32 stub_data_pad;
	SecBuffer Buffers[2];
	SecBufferDesc Message;
	SECURITY_STATUS encrypt_status;
	rpcconn_request_hdr_t* request_pdu;

	ntlm = rpc->ntlm;

	if (ntlm->table->QueryContextAttributes(&ntlm->context, SECPKG_ATTR_SIZES, &ntlm->ContextSizes) != SEC_E_OK)
	{
		printf("QueryContextAttributes SECPKG_ATTR_SIZES failure\n");
		return -1;
	}

	request_pdu = (rpcconn_request_hdr_t*) malloc(sizeof(rpcconn_request_hdr_t));
	ZeroMemory(request_pdu, sizeof(rpcconn_request_hdr_t));

	rpc_pdu_header_init(rpc, (rpcconn_hdr_t*) request_pdu);

	request_pdu->ptype = PTYPE_REQUEST;
	request_pdu->pfc_flags = PFC_FIRST_FRAG | PFC_LAST_FRAG;
	request_pdu->auth_length = ntlm->ContextSizes.cbMaxSignature;
	request_pdu->call_id = ++rpc->call_id;

	/* opnum 8 is TsProxySetupReceivePipe, save call_id for checking pipe responses */

	if (opnum == 8)
		rpc->pipe_call_id = rpc->call_id;

	request_pdu->alloc_hint = length;
	request_pdu->p_cont_id = 0x0000;
	request_pdu->opnum = opnum;

	request_pdu->stub_data = data;

	offset = 24;
	stub_data_pad = 0;
	stub_data_pad = rpc_offset_align(&offset, 8);

	offset += length;
	request_pdu->auth_verifier.auth_pad_length = rpc_offset_align(&offset, 4);
	request_pdu->auth_verifier.auth_type = RPC_C_AUTHN_WINNT;
	request_pdu->auth_verifier.auth_level = RPC_C_AUTHN_LEVEL_PKT_INTEGRITY;
	request_pdu->auth_verifier.auth_reserved = 0x00;
	request_pdu->auth_verifier.auth_context_id = 0x00000000;
	offset += (8 + request_pdu->auth_length);

	request_pdu->frag_length = offset;

	buffer = (BYTE*) malloc(request_pdu->frag_length);

	CopyMemory(buffer, request_pdu, 24);

	offset = 24;
	rpc_offset_pad(&offset, stub_data_pad);
	CopyMemory(&buffer[offset], request_pdu->stub_data, length);
	offset += length;

	rpc_offset_pad(&offset, request_pdu->auth_verifier.auth_pad_length);
	CopyMemory(&buffer[offset], &request_pdu->auth_verifier.auth_type, 8);
	offset += 8;

	Buffers[0].BufferType = SECBUFFER_DATA; /* auth_data */
	Buffers[1].BufferType = SECBUFFER_TOKEN; /* signature */

	Buffers[0].pvBuffer = buffer;
	Buffers[0].cbBuffer = offset;

	Buffers[1].cbBuffer = ntlm->ContextSizes.cbMaxSignature;
	Buffers[1].pvBuffer = malloc(Buffers[1].cbBuffer);
	ZeroMemory(Buffers[1].pvBuffer, Buffers[1].cbBuffer);

	Message.cBuffers = 2;
	Message.ulVersion = SECBUFFER_VERSION;
	Message.pBuffers = (PSecBuffer) &Buffers;

	encrypt_status = ntlm->table->EncryptMessage(&ntlm->context, 0, &Message, rpc->send_seq_num++);

	if (encrypt_status != SEC_E_OK)
	{
		printf("EncryptMessage status: 0x%08X\n", encrypt_status);
		return -1;
	}

	CopyMemory(&buffer[offset], Buffers[1].pvBuffer, Buffers[1].cbBuffer);
	offset += Buffers[1].cbBuffer;

	rpc_send_enqueue_pdu(rpc, buffer, request_pdu->frag_length);
	status = rpc_send_dequeue_pdu(rpc);

	free(buffer);

	if (status < 0)
		return -1;

	return length;
}

BOOL rpc_connect(rdpRpc* rpc)
{
	rpc->TlsIn = rpc->transport->TlsIn;
	rpc->TlsOut = rpc->transport->TlsOut;

	if (!rts_connect(rpc))
	{
		printf("rts_connect error!\n");
		return FALSE;
	}

	if (!rpc_send_bind_pdu(rpc))
	{
		printf("rpc_send_bind_pdu error!\n");
		return FALSE;
	}

	if (rpc_recv_bind_ack_pdu(rpc) <= 0)
	{
		printf("rpc_recv_bind_ack_pdu error!\n");
		return FALSE;
	}

	if (!rpc_send_rpc_auth_3_pdu(rpc))
	{
		printf("rpc_send_rpc_auth_3 error!\n");
		return FALSE;
	}

	return TRUE;
}

void rpc_client_virtual_connection_init(rdpRpc* rpc, RpcVirtualConnection* connection)
{
	connection->DefaultInChannel->State = CLIENT_IN_CHANNEL_STATE_INITIAL;
	connection->DefaultInChannel->BytesSent = 0;
	connection->DefaultInChannel->SenderAvailableWindow = rpc->ReceiveWindow;
	connection->DefaultInChannel->PingOriginator.ConnectionTimeout = 30;
	connection->DefaultInChannel->PingOriginator.KeepAliveInterval = 0;

	connection->DefaultOutChannel->State = CLIENT_OUT_CHANNEL_STATE_INITIAL;
	connection->DefaultOutChannel->BytesReceived = 0;
	connection->DefaultOutChannel->ReceiverAvailableWindow = rpc->ReceiveWindow;
	connection->DefaultOutChannel->ReceiveWindow = rpc->ReceiveWindow;
	connection->DefaultOutChannel->ReceiveWindowSize = rpc->ReceiveWindow;
	connection->DefaultOutChannel->AvailableWindowAdvertised = rpc->ReceiveWindow;
}

RpcVirtualConnection* rpc_client_virtual_connection_new(rdpRpc* rpc)
{
	RpcVirtualConnection* connection = (RpcVirtualConnection*) malloc(sizeof(RpcVirtualConnection));

	if (connection != NULL)
	{
		ZeroMemory(connection, sizeof(RpcVirtualConnection));
		connection->State = VIRTUAL_CONNECTION_STATE_INITIAL;
		connection->DefaultInChannel = (RpcInChannel*) malloc(sizeof(RpcInChannel));
		connection->DefaultOutChannel = (RpcOutChannel*) malloc(sizeof(RpcOutChannel));
		ZeroMemory(connection->DefaultInChannel, sizeof(RpcInChannel));
		ZeroMemory(connection->DefaultOutChannel, sizeof(RpcOutChannel));
		rpc_client_virtual_connection_init(rpc, connection);
	}

	return connection;
}

void rpc_client_virtual_connection_free(RpcVirtualConnection* virtual_connection)
{
	if (virtual_connection != NULL)
	{
		free(virtual_connection);
	}
}

/* Virtual Connection Cookie Table */

RpcVirtualConnectionCookieTable* rpc_virtual_connection_cookie_table_new(rdpRpc* rpc)
{
	RpcVirtualConnectionCookieTable* table;

	table = (RpcVirtualConnectionCookieTable*) malloc(sizeof(RpcVirtualConnectionCookieTable));

	if (table != NULL)
	{
		ZeroMemory(table, sizeof(RpcVirtualConnectionCookieTable));

		table->Count = 0;
		table->ArraySize = 32;

		table->Entries = (RpcVirtualConnectionCookieEntry*) malloc(sizeof(RpcVirtualConnectionCookieEntry) * table->ArraySize);
		ZeroMemory(table->Entries, sizeof(RpcVirtualConnectionCookieEntry) * table->ArraySize);
	}

	return table;
}

void rpc_virtual_connection_cookie_table_free(RpcVirtualConnectionCookieTable* table)
{
	if (table != NULL)
	{
		free(table->Entries);
		free(table);
	}
}

/* NTLM over HTTP */

rdpNtlmHttp* ntlm_http_new()
{
	rdpNtlmHttp* ntlm_http;

	ntlm_http = (rdpNtlmHttp*) malloc(sizeof(rdpNtlmHttp));

	if (ntlm_http != NULL)
	{
		ZeroMemory(ntlm_http, sizeof(rdpNtlmHttp));
		ntlm_http->ntlm = ntlm_new();
		ntlm_http->context = http_context_new();
	}

	return ntlm_http;
}

void rpc_ntlm_http_init_channel(rdpRpc* rpc, rdpNtlmHttp* ntlm_http, TSG_CHANNEL channel)
{
	if (channel == TSG_CHANNEL_IN)
		http_context_set_method(ntlm_http->context, "RPC_IN_DATA");
	else if (channel == TSG_CHANNEL_OUT)
		http_context_set_method(ntlm_http->context, "RPC_OUT_DATA");

	http_context_set_uri(ntlm_http->context, "/rpc/rpcproxy.dll?localhost:3388");
	http_context_set_accept(ntlm_http->context, "application/rpc");
	http_context_set_cache_control(ntlm_http->context, "no-cache");
	http_context_set_connection(ntlm_http->context, "Keep-Alive");
	http_context_set_user_agent(ntlm_http->context, "MSRPC");
	http_context_set_host(ntlm_http->context, rpc->settings->GatewayHostname);

	if (channel == TSG_CHANNEL_IN)
	{
		http_context_set_pragma(ntlm_http->context,
			"ResourceTypeUuid=44e265dd-7daf-42cd-8560-3cdb6e7a2729");
	}
	else if (channel == TSG_CHANNEL_OUT)
	{
		http_context_set_pragma(ntlm_http->context,
				"ResourceTypeUuid=44e265dd-7daf-42cd-8560-3cdb6e7a2729" ", "
				"SessionId=fbd9c34f-397d-471d-a109-1b08cc554624");
	}
}

void ntlm_http_free(rdpNtlmHttp* ntlm_http)
{
	if (ntlm_http != NULL)
	{
		ntlm_free(ntlm_http->ntlm);
		http_context_free(ntlm_http->context);
	}
}

/* RPC Core Module */

rdpRpc* rpc_new(rdpTransport* transport)
{
	rdpRpc* rpc = (rdpRpc*) malloc(sizeof(rdpRpc));

	if (rpc != NULL)
	{
		ZeroMemory(rpc, sizeof(rdpRpc));

		rpc->transport = transport;
		rpc->settings = transport->settings;

		rpc->send_seq_num = 0;
		rpc->ntlm = ntlm_new();

		rpc->NtlmHttpIn = ntlm_http_new();
		rpc->NtlmHttpOut = ntlm_http_new();

		rpc_ntlm_http_init_channel(rpc, rpc->NtlmHttpIn, TSG_CHANNEL_IN);
		rpc_ntlm_http_init_channel(rpc, rpc->NtlmHttpOut, TSG_CHANNEL_OUT);

		rpc->length = 20;
		rpc->buffer = (BYTE*) malloc(rpc->length);

		rpc->rpc_vers = 5;
		rpc->rpc_vers_minor = 0;

		/* little-endian data representation */
		rpc->packed_drep[0] = 0x10;
		rpc->packed_drep[1] = 0x00;
		rpc->packed_drep[2] = 0x00;
		rpc->packed_drep[3] = 0x00;

		rpc->max_xmit_frag = 0x0FF8;
		rpc->max_recv_frag = 0x0FF8;

		rpc->SendQueue = (PSLIST_HEADER) _aligned_malloc(sizeof(SLIST_HEADER), MEMORY_ALLOCATION_ALIGNMENT);
		InitializeSListHead(rpc->SendQueue);

		rpc->ReceiveWindow = 0x00010000;

		rpc->ChannelLifetime = 0x40000000;
		rpc->ChannelLifetimeSet = 0;

		rpc->KeepAliveInterval = 300000;
		rpc->CurrentKeepAliveInterval = rpc->KeepAliveInterval;
		rpc->CurrentKeepAliveTime = 0;

		rpc->VirtualConnection = rpc_client_virtual_connection_new(rpc);
		rpc->VirtualConnectionCookieTable = rpc_virtual_connection_cookie_table_new(rpc);

		rpc->call_id = 1;
	}

	return rpc;
}

void rpc_free(rdpRpc* rpc)
{
	if (rpc != NULL)
	{
		RPC_PDU_ENTRY* PduEntry;

		ntlm_http_free(rpc->NtlmHttpIn);
		ntlm_http_free(rpc->NtlmHttpOut);

		while ((PduEntry = (RPC_PDU_ENTRY*) InterlockedPopEntrySList(rpc->SendQueue)) != NULL)
			_aligned_free(PduEntry);

		_aligned_free(rpc->SendQueue);

		rpc_client_virtual_connection_free(rpc->VirtualConnection);
		rpc_virtual_connection_cookie_table_free(rpc->VirtualConnectionCookieTable);
		free(rpc);
	}
}
