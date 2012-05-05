/**
 * FreeRDP: A Remote Desktop Protocol Client
 * RPC over HTTP
 *
 * Copyright 2012 Fujitsu Technology Solutions GmbH
 * Copyright 2012 Dmitrij Jasnov <dmitrij.jasnov@ts.fujitsu.com>
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/rand.h>

#include "http.h"

#include "rpc.h"

#define NTLM_PACKAGE_NAME	_T("NTLM")

boolean ntlm_client_init(rdpNtlm* ntlm, boolean confidentiality, char* user, char* domain, char* password)
{
	size_t size;
	SECURITY_STATUS status;

	sspi_GlobalInit();

	ntlm->confidentiality = confidentiality;

	ntlm->table = InitSecurityInterface();

	ntlm->identity.Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;

	ntlm->identity.User = (uint16*) freerdp_uniconv_out(ntlm->uniconv, user, &size);
	ntlm->identity.UserLength = (uint32) size;

	if (domain)
	{
		ntlm->identity.Domain = (uint16*) freerdp_uniconv_out(ntlm->uniconv, domain, &size);
		ntlm->identity.DomainLength = (uint32) size;
	}
	else
	{
		ntlm->identity.Domain = (uint16*) NULL;
		ntlm->identity.DomainLength = 0;
	}

	ntlm->identity.Password = (uint16*) freerdp_uniconv_out(ntlm->uniconv, (char*) password, &size);
	ntlm->identity.PasswordLength = (uint32) size;

	status = QuerySecurityPackageInfo(NTLM_PACKAGE_NAME, &ntlm->pPackageInfo);

	if (status != SEC_E_OK)
	{
		printf("QuerySecurityPackageInfo status: 0x%08X\n", status);
		return false;
	}

	ntlm->cbMaxToken = ntlm->pPackageInfo->cbMaxToken;

	status = ntlm->table->AcquireCredentialsHandle(NULL, NTLM_PACKAGE_NAME,
			SECPKG_CRED_OUTBOUND, NULL, &ntlm->identity, NULL, NULL, &ntlm->credentials, &ntlm->expiration);

	if (status != SEC_E_OK)
	{
		printf("AcquireCredentialsHandle status: 0x%08X\n", status);
		return false;
	}

	ntlm->haveContext = false;
	ntlm->haveInputBuffer = false;
	memset(&ntlm->inputBuffer, 0, sizeof(SecBuffer));
	memset(&ntlm->outputBuffer, 0, sizeof(SecBuffer));
	memset(&ntlm->ContextSizes, 0, sizeof(SecPkgContext_Sizes));

	ntlm->fContextReq = ISC_REQ_REPLAY_DETECT | ISC_REQ_SEQUENCE_DETECT | ISC_REQ_DELEGATE;

	if (ntlm->confidentiality)
		ntlm->fContextReq |= ISC_REQ_CONFIDENTIALITY;

	return true;
}

boolean ntlm_authenticate(rdpNtlm* ntlm)
{
	SECURITY_STATUS status;

	ntlm->outputBufferDesc.ulVersion = SECBUFFER_VERSION;
	ntlm->outputBufferDesc.cBuffers = 1;
	ntlm->outputBufferDesc.pBuffers = &ntlm->outputBuffer;
	ntlm->outputBuffer.BufferType = SECBUFFER_TOKEN;
	ntlm->outputBuffer.cbBuffer = ntlm->cbMaxToken;
	ntlm->outputBuffer.pvBuffer = xmalloc(ntlm->outputBuffer.cbBuffer);

	if (ntlm->haveInputBuffer)
	{
		ntlm->inputBufferDesc.ulVersion = SECBUFFER_VERSION;
		ntlm->inputBufferDesc.cBuffers = 1;
		ntlm->inputBufferDesc.pBuffers = &ntlm->inputBuffer;
		ntlm->inputBuffer.BufferType = SECBUFFER_TOKEN;
	}

	status = ntlm->table->InitializeSecurityContext(&ntlm->credentials,
			(ntlm->haveContext) ? &ntlm->context : NULL,
			NULL, ntlm->fContextReq, 0, SECURITY_NATIVE_DREP,
			(ntlm->haveInputBuffer) ? &ntlm->inputBufferDesc : NULL,
			0, &ntlm->context, &ntlm->outputBufferDesc,
			&ntlm->pfContextAttr, &ntlm->expiration);

	if ((status == SEC_I_COMPLETE_AND_CONTINUE) || (status == SEC_I_COMPLETE_NEEDED) || (status == SEC_E_OK))
	{
		if (ntlm->table->CompleteAuthToken != NULL)
			ntlm->table->CompleteAuthToken(&ntlm->context, &ntlm->outputBufferDesc);

		if (ntlm->table->QueryContextAttributes(&ntlm->context, SECPKG_ATTR_SIZES, &ntlm->ContextSizes) != SEC_E_OK)
		{
			printf("QueryContextAttributes SECPKG_ATTR_SIZES failure\n");
			return 0;
		}

		if (status == SEC_I_COMPLETE_NEEDED)
			status = SEC_E_OK;
		else if (status == SEC_I_COMPLETE_AND_CONTINUE)
			status = SEC_I_CONTINUE_NEEDED;
	}

	ntlm->haveInputBuffer = true;
	ntlm->haveContext = true;

	return true;
}

void ntlm_client_uninit(rdpNtlm* ntlm)
{
	FreeCredentialsHandle(&ntlm->credentials);
	FreeContextBuffer(ntlm->pPackageInfo);
}

rdpNtlm* ntlm_new()
{
	rdpNtlm* ntlm = xnew(rdpNtlm);

	if (ntlm != NULL)
	{
		ntlm->uniconv = freerdp_uniconv_new();
	}

	return ntlm;
}

void ntlm_free(rdpNtlm* ntlm)
{
	if (ntlm != NULL)
	{
		freerdp_uniconv_free(ntlm->uniconv);
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
		http_context = rpc->ntlm_http_in->context;
		http_request_set_method(http_request, "RPC_IN_DATA");
	}
	else if (channel == TSG_CHANNEL_OUT)
	{
		http_context = rpc->ntlm_http_out->context;
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

	return s;
}

boolean rpc_ntlm_http_out_connect(rdpRpc* rpc)
{
	STREAM* s;
	int ntlm_token_length;
	uint8* ntlm_token_data;
	HttpResponse* http_response;
	rdpNtlm* ntlm = rpc->ntlm_http_out->ntlm;

	ntlm_client_init(ntlm, true, rpc->settings->username,
			rpc->settings->domain, rpc->settings->password);

	ntlm_authenticate(ntlm);

	s = rpc_ntlm_http_request(rpc, &ntlm->outputBuffer, 0, TSG_CHANNEL_OUT);

	/* Send OUT Channel Request */

	DEBUG_RPC("\n%s", s->data);
	tls_write_all(rpc->tls_out, s->data, s->size);
	stream_free(s);

	/* Receive OUT Channel Response */

	http_response = http_response_recv(rpc->tls_out);

	ntlm_token_data = NULL;
	crypto_base64_decode((uint8*) http_response->AuthParam, strlen(http_response->AuthParam),
			&ntlm_token_data, &ntlm_token_length);

	ntlm->inputBuffer.pvBuffer = ntlm_token_data;
	ntlm->inputBuffer.cbBuffer = ntlm_token_length;

	ntlm_authenticate(ntlm);

	http_response_free(http_response);

	s = rpc_ntlm_http_request(rpc, &ntlm->outputBuffer, 76, TSG_CHANNEL_OUT);

	/* Send OUT Channel Request */

	DEBUG_RPC("\n%s", s->data);
	tls_write_all(rpc->tls_out, s->data, s->size);
	stream_free(s);

	ntlm_client_uninit(ntlm);
	ntlm_free(ntlm);

	return true;
}

boolean rpc_ntlm_http_in_connect(rdpRpc* rpc)
{
	STREAM* s;
	int ntlm_token_length;
	uint8* ntlm_token_data;
	HttpResponse* http_response;
	rdpNtlm* ntlm = rpc->ntlm_http_in->ntlm;

	ntlm_client_init(ntlm, true, rpc->settings->username,
			rpc->settings->domain, rpc->settings->password);

	ntlm_authenticate(ntlm);

	s = rpc_ntlm_http_request(rpc, &ntlm->outputBuffer, 0, TSG_CHANNEL_IN);

	/* Send IN Channel Request */

	DEBUG_RPC("\n%s", s->data);
	tls_write_all(rpc->tls_in, s->data, s->size);
	stream_free(s);

	/* Receive IN Channel Response */

	http_response = http_response_recv(rpc->tls_in);

	ntlm_token_data = NULL;
	crypto_base64_decode((uint8*) http_response->AuthParam, strlen(http_response->AuthParam),
			&ntlm_token_data, &ntlm_token_length);

	ntlm->inputBuffer.pvBuffer = ntlm_token_data;
	ntlm->inputBuffer.cbBuffer = ntlm_token_length;

	ntlm_authenticate(ntlm);

	http_response_free(http_response);

	s = rpc_ntlm_http_request(rpc, &ntlm->outputBuffer, 0x40000000, TSG_CHANNEL_IN);

	/* Send IN Channel Request */

	DEBUG_RPC("\n%s", s->data);
	tls_write_all(rpc->tls_in, s->data, s->size);
	stream_free(s);

	ntlm_client_uninit(ntlm);
	ntlm_free(ntlm);

	return true;
}

void rpc_pdu_header_read(STREAM* s, RPC_PDU_HEADER* header)
{
	stream_read_uint8(s, header->rpc_vers); /* rpc_vers (1 byte) */
	stream_read_uint8(s, header->rpc_vers_minor); /* rpc_vers_minor (1 byte) */
	stream_read_uint8(s, header->ptype); /* PTYPE (1 byte) */
	stream_read_uint8(s, header->pfc_flags); /* pfc_flags (1 byte) */
	stream_read_uint8(s, header->packed_drep[0]); /* packet_drep[0] (1 byte) */
	stream_read_uint8(s, header->packed_drep[1]); /* packet_drep[1] (1 byte) */
	stream_read_uint8(s, header->packed_drep[2]); /* packet_drep[2] (1 byte) */
	stream_read_uint8(s, header->packed_drep[3]); /* packet_drep[3] (1 byte) */
	stream_read_uint16(s, header->frag_length); /* frag_length (2 bytes) */
	stream_read_uint16(s, header->auth_length); /* auth_length (2 bytes) */
	stream_read_uint32(s, header->call_id); /* call_id (4 bytes) */
}

int rpc_out_write(rdpRpc* rpc, uint8* data, int length)
{
	int status;

#ifdef WITH_DEBUG_RPC
	printf("rpc_out_write(): length: %d\n", length);
	freerdp_hexdump(data, length);
	printf("\n");
#endif

	status = tls_write_all(rpc->tls_out, data, length);

	return status;
}

int rpc_in_write(rdpRpc* rpc, uint8* data, int length)
{
	int status;

#ifdef WITH_DEBUG_RPC
	printf("rpc_in_write() length: %d\n", length);
	freerdp_hexdump(data, length);
	printf("\n");
#endif

	status = tls_write_all(rpc->tls_in, data, length);

	if (status > 0)
		rpc->VirtualConnection->DefaultInChannel->BytesSent += status;

	return status;
}

boolean rpc_send_bind_pdu(rdpRpc* rpc)
{
	STREAM* pdu;
	rpcconn_bind_hdr_t* bind_pdu;
	rdpSettings* settings = rpc->settings;
	STREAM* ntlm_stream = stream_new(0xFFFF);

	rpc->ntlm = ntlm_new();

	DEBUG_RPC("Sending bind PDU");

	ntlm_client_init(rpc->ntlm, false, settings->username, settings->domain, settings->password);

	ntlm_authenticate(rpc->ntlm);
	ntlm_stream->size = rpc->ntlm->outputBuffer.cbBuffer;
	ntlm_stream->p = ntlm_stream->data = rpc->ntlm->outputBuffer.pvBuffer;

	bind_pdu = xnew(rpcconn_bind_hdr_t);
	bind_pdu->rpc_vers = 5;
	bind_pdu->rpc_vers_minor = 0;
	bind_pdu->PTYPE = PTYPE_BIND;
	bind_pdu->pfc_flags = PFC_FIRST_FRAG | PFC_LAST_FRAG | PFC_PENDING_CANCEL | PFC_CONC_MPX;
	bind_pdu->packed_drep[0] = 0x10;
	bind_pdu->packed_drep[1] = 0x00;
	bind_pdu->packed_drep[2] = 0x00;
	bind_pdu->packed_drep[3] = 0x00;
	bind_pdu->frag_length = 124 + ntlm_stream->size;
	bind_pdu->auth_length = ntlm_stream->size;
	bind_pdu->call_id = 2;
	bind_pdu->max_xmit_frag = 0x0FF8;
	bind_pdu->max_recv_frag = 0x0FF8;
	bind_pdu->assoc_group_id = 0;
	bind_pdu->p_context_elem.n_context_elem = 2;
	bind_pdu->p_context_elem.reserved = 0;
	bind_pdu->p_context_elem.reserved2 = 0;
	bind_pdu->p_context_elem.p_cont_elem = xmalloc(sizeof(p_cont_elem_t) * bind_pdu->p_context_elem.n_context_elem);
	bind_pdu->p_context_elem.p_cont_elem[0].p_cont_id = 0;
	bind_pdu->p_context_elem.p_cont_elem[0].n_transfer_syn = 1;
	bind_pdu->p_context_elem.p_cont_elem[0].reserved = 0;
	bind_pdu->p_context_elem.p_cont_elem[0].abstract_syntax.if_uuid.time_low = 0x44e265dd;
	bind_pdu->p_context_elem.p_cont_elem[0].abstract_syntax.if_uuid.time_mid = 0x7daf;
	bind_pdu->p_context_elem.p_cont_elem[0].abstract_syntax.if_uuid.time_hi_and_version = 0x42cd;
	bind_pdu->p_context_elem.p_cont_elem[0].abstract_syntax.if_uuid.clock_seq_hi_and_reserved = 0x85;
	bind_pdu->p_context_elem.p_cont_elem[0].abstract_syntax.if_uuid.clock_seq_low = 0x60;
	bind_pdu->p_context_elem.p_cont_elem[0].abstract_syntax.if_uuid.node[0] = 0x3c;
	bind_pdu->p_context_elem.p_cont_elem[0].abstract_syntax.if_uuid.node[1] = 0xdb;
	bind_pdu->p_context_elem.p_cont_elem[0].abstract_syntax.if_uuid.node[2] = 0x6e;
	bind_pdu->p_context_elem.p_cont_elem[0].abstract_syntax.if_uuid.node[3] = 0x7a;
	bind_pdu->p_context_elem.p_cont_elem[0].abstract_syntax.if_uuid.node[4] = 0x27;
	bind_pdu->p_context_elem.p_cont_elem[0].abstract_syntax.if_uuid.node[5] = 0x29;
	bind_pdu->p_context_elem.p_cont_elem[0].abstract_syntax.if_version = 0x00030001;
	bind_pdu->p_context_elem.p_cont_elem[0].transfer_syntaxes = xmalloc(sizeof(p_syntax_id_t));
	bind_pdu->p_context_elem.p_cont_elem[0].transfer_syntaxes[0].if_uuid.time_low = 0x8a885d04;
	bind_pdu->p_context_elem.p_cont_elem[0].transfer_syntaxes[0].if_uuid.time_mid = 0x1ceb;
	bind_pdu->p_context_elem.p_cont_elem[0].transfer_syntaxes[0].if_uuid.time_hi_and_version = 0x11c9;
	bind_pdu->p_context_elem.p_cont_elem[0].transfer_syntaxes[0].if_uuid.clock_seq_hi_and_reserved = 0x9f;
	bind_pdu->p_context_elem.p_cont_elem[0].transfer_syntaxes[0].if_uuid.clock_seq_low = 0xe8;
	bind_pdu->p_context_elem.p_cont_elem[0].transfer_syntaxes[0].if_uuid.node[0] = 0x08;
	bind_pdu->p_context_elem.p_cont_elem[0].transfer_syntaxes[0].if_uuid.node[1] = 0x00;
	bind_pdu->p_context_elem.p_cont_elem[0].transfer_syntaxes[0].if_uuid.node[2] = 0x2b;
	bind_pdu->p_context_elem.p_cont_elem[0].transfer_syntaxes[0].if_uuid.node[3] = 0x10;
	bind_pdu->p_context_elem.p_cont_elem[0].transfer_syntaxes[0].if_uuid.node[4] = 0x48;
	bind_pdu->p_context_elem.p_cont_elem[0].transfer_syntaxes[0].if_uuid.node[5] = 0x60;
	bind_pdu->p_context_elem.p_cont_elem[0].transfer_syntaxes[0].if_version = 0x00000002;
	bind_pdu->p_context_elem.p_cont_elem[1].p_cont_id = 1;
	bind_pdu->p_context_elem.p_cont_elem[1].n_transfer_syn = 1;
	bind_pdu->p_context_elem.p_cont_elem[1].reserved = 0;
	bind_pdu->p_context_elem.p_cont_elem[1].abstract_syntax.if_uuid.time_low = 0x44e265dd;
	bind_pdu->p_context_elem.p_cont_elem[1].abstract_syntax.if_uuid.time_mid = 0x7daf;
	bind_pdu->p_context_elem.p_cont_elem[1].abstract_syntax.if_uuid.time_hi_and_version = 0x42cd;
	bind_pdu->p_context_elem.p_cont_elem[1].abstract_syntax.if_uuid.clock_seq_hi_and_reserved = 0x85;
	bind_pdu->p_context_elem.p_cont_elem[1].abstract_syntax.if_uuid.clock_seq_low = 0x60;
	bind_pdu->p_context_elem.p_cont_elem[1].abstract_syntax.if_uuid.node[0] = 0x3c;
	bind_pdu->p_context_elem.p_cont_elem[1].abstract_syntax.if_uuid.node[1] = 0xdb;
	bind_pdu->p_context_elem.p_cont_elem[1].abstract_syntax.if_uuid.node[2] = 0x6e;
	bind_pdu->p_context_elem.p_cont_elem[1].abstract_syntax.if_uuid.node[3] = 0x7a;
	bind_pdu->p_context_elem.p_cont_elem[1].abstract_syntax.if_uuid.node[4] = 0x27;
	bind_pdu->p_context_elem.p_cont_elem[1].abstract_syntax.if_uuid.node[5] = 0x29;
	bind_pdu->p_context_elem.p_cont_elem[1].abstract_syntax.if_version = 0x00030001;
	bind_pdu->p_context_elem.p_cont_elem[1].transfer_syntaxes = xmalloc(sizeof(p_syntax_id_t));
	bind_pdu->p_context_elem.p_cont_elem[1].transfer_syntaxes[0].if_uuid.time_low = 0x6cb71c2c;
	bind_pdu->p_context_elem.p_cont_elem[1].transfer_syntaxes[0].if_uuid.time_mid = 0x9812;
	bind_pdu->p_context_elem.p_cont_elem[1].transfer_syntaxes[0].if_uuid.time_hi_and_version = 0x4540;
	bind_pdu->p_context_elem.p_cont_elem[1].transfer_syntaxes[0].if_uuid.clock_seq_hi_and_reserved = 0x03;
	bind_pdu->p_context_elem.p_cont_elem[1].transfer_syntaxes[0].if_uuid.clock_seq_low = 0x00;
	bind_pdu->p_context_elem.p_cont_elem[1].transfer_syntaxes[0].if_uuid.node[0] = 0x00;
	bind_pdu->p_context_elem.p_cont_elem[1].transfer_syntaxes[0].if_uuid.node[1] = 0x00;
	bind_pdu->p_context_elem.p_cont_elem[1].transfer_syntaxes[0].if_uuid.node[2] = 0x00;
	bind_pdu->p_context_elem.p_cont_elem[1].transfer_syntaxes[0].if_uuid.node[3] = 0x00;
	bind_pdu->p_context_elem.p_cont_elem[1].transfer_syntaxes[0].if_uuid.node[4] = 0x00;
	bind_pdu->p_context_elem.p_cont_elem[1].transfer_syntaxes[0].if_uuid.node[5] = 0x00;
	bind_pdu->p_context_elem.p_cont_elem[1].transfer_syntaxes[0].if_version = 0x00000001;
	bind_pdu->auth_verifier.auth_pad = NULL; /* align(4); size_is(auth_pad_length) p*/
	bind_pdu->auth_verifier.auth_type = 0x0a;       /* :01  which authent service */
	bind_pdu->auth_verifier.auth_level = 0x05;      /* :01  which level within service */
	bind_pdu->auth_verifier.auth_pad_length = 0x00; /* :01 */
	bind_pdu->auth_verifier.auth_reserved = 0x00;   /* :01 reserved, m.b.z. */
	bind_pdu->auth_verifier.auth_context_id = 0x00000000; /* :04 */
	bind_pdu->auth_verifier.auth_value = xmalloc(bind_pdu->auth_length); /* credentials; size_is(auth_length) p*/;
	memcpy(bind_pdu->auth_verifier.auth_value, ntlm_stream->data, bind_pdu->auth_length);

	stream_free(ntlm_stream);

	pdu = stream_new(bind_pdu->frag_length);

	stream_write(pdu, bind_pdu, 24);
	stream_write(pdu, &bind_pdu->p_context_elem, 4);
	stream_write(pdu, bind_pdu->p_context_elem.p_cont_elem, 24);
	stream_write(pdu, bind_pdu->p_context_elem.p_cont_elem[0].transfer_syntaxes, 20);
	stream_write(pdu, bind_pdu->p_context_elem.p_cont_elem + 1, 24);
	stream_write(pdu, bind_pdu->p_context_elem.p_cont_elem[1].transfer_syntaxes, 20);

	if (bind_pdu->auth_verifier.auth_pad_length > 0)
		stream_write(pdu, bind_pdu->auth_verifier.auth_pad, bind_pdu->auth_verifier.auth_pad_length);

	stream_write(pdu, &bind_pdu->auth_verifier.auth_type, 8); /* assumed that uint8 pointer is 32bit long (4 bytes) */
	stream_write(pdu, bind_pdu->auth_verifier.auth_value, bind_pdu->auth_length);
	stream_seal(pdu);

	rpc_in_write(rpc, pdu->data, pdu->size);

	xfree(bind_pdu);

	return true;
}

int rpc_recv_bind_ack_pdu(rdpRpc* rpc)
{
	uint8* p;
	STREAM* s;
	int status;
	uint8* pdu;
	uint8* auth_data;
	RPC_PDU_HEADER header;
	int pdu_length = 0x8FFF;

	pdu = xmalloc(pdu_length);
	status = rpc_out_read(rpc, pdu, pdu_length);

	if (status > 0)
	{
		s = stream_new(0);
		stream_attach(s, pdu, pdu_length);
		rpc_pdu_header_read(s, &header);
		stream_detach(s);
		stream_free(s);

		auth_data = xmalloc(header.auth_length);
		p = (pdu + (header.frag_length - header.auth_length));
		memcpy(auth_data, p, header.auth_length);

		rpc->ntlm->inputBuffer.pvBuffer = auth_data;
		rpc->ntlm->inputBuffer.cbBuffer = header.auth_length;

		ntlm_authenticate(rpc->ntlm);
	}

	xfree(pdu);
	return status;
}

boolean rpc_send_rpc_auth_3_pdu(rdpRpc* rpc)
{
	STREAM* pdu;
	STREAM* s = stream_new(0);
	rpcconn_rpc_auth_3_hdr_t* rpc_auth_3_pdu;

	DEBUG_RPC("Sending auth_3 PDU");

	s->size = rpc->ntlm->outputBuffer.cbBuffer;
	s->p = s->data = rpc->ntlm->outputBuffer.pvBuffer;

	rpc_auth_3_pdu = xnew(rpcconn_rpc_auth_3_hdr_t);
	rpc_auth_3_pdu->rpc_vers = 5;
	rpc_auth_3_pdu->rpc_vers_minor = 0;
	rpc_auth_3_pdu->PTYPE = PTYPE_RPC_AUTH_3;
	rpc_auth_3_pdu->pfc_flags = PFC_FIRST_FRAG | PFC_LAST_FRAG | PFC_CONC_MPX;
	rpc_auth_3_pdu->packed_drep[0] = 0x10;
	rpc_auth_3_pdu->packed_drep[1] = 0x00;
	rpc_auth_3_pdu->packed_drep[2] = 0x00;
	rpc_auth_3_pdu->packed_drep[3] = 0x00;
	rpc_auth_3_pdu->frag_length = 28 + s->size;
	rpc_auth_3_pdu->auth_length = s->size;
	rpc_auth_3_pdu->call_id = 2;

	rpc_auth_3_pdu->max_xmit_frag = 0x0FF8;
	rpc_auth_3_pdu->max_recv_frag = 0x0FF8;
	rpc_auth_3_pdu->auth_verifier.auth_pad = NULL; /* align(4); size_is(auth_pad_length) p */
	rpc_auth_3_pdu->auth_verifier.auth_type = 0x0a;       /* :01  which authent service */
	rpc_auth_3_pdu->auth_verifier.auth_level = 0x05;      /* :01  which level within service */
	rpc_auth_3_pdu->auth_verifier.auth_pad_length = 0x00; /* :01 */
	rpc_auth_3_pdu->auth_verifier.auth_reserved = 0x00;   /* :01 reserved, m.b.z. */
	rpc_auth_3_pdu->auth_verifier.auth_context_id = 0x00000000; /* :04 */
	rpc_auth_3_pdu->auth_verifier.auth_value = xmalloc(rpc_auth_3_pdu->auth_length); /* credentials; size_is(auth_length) p */
	memcpy(rpc_auth_3_pdu->auth_verifier.auth_value, s->data, rpc_auth_3_pdu->auth_length);

	stream_free(s);

	pdu = stream_new(rpc_auth_3_pdu->frag_length);

	stream_write(pdu, rpc_auth_3_pdu, 20);

	if (rpc_auth_3_pdu->auth_verifier.auth_pad_length > 0)
		stream_write(pdu, rpc_auth_3_pdu->auth_verifier.auth_pad, rpc_auth_3_pdu->auth_verifier.auth_pad_length);

	stream_write(pdu, &rpc_auth_3_pdu->auth_verifier.auth_type, 8);
	stream_write(pdu, rpc_auth_3_pdu->auth_verifier.auth_value, rpc_auth_3_pdu->auth_length);

	rpc_in_write(rpc, pdu->data, stream_get_length(pdu));

	xfree(rpc_auth_3_pdu);

	return true;
}

int rpc_out_read(rdpRpc* rpc, uint8* data, int length)
{
	STREAM* s;
	int status;
	uint8* pdu;
	int content_length;
	RPC_PDU_HEADER header;

	if (rpc->VirtualConnection->DefaultOutChannel->ReceiverAvailableWindow < 0x00008FFF) /* Just a simple workaround */
		rts_send_flow_control_ack_pdu(rpc);  /* Send FlowControlAck every time AvailableWindow reaches the half */

	pdu = xmalloc(0xFFFF);

	status = tls_read(rpc->tls_out, pdu, 16); /* read first 16 bytes to get RPC PDU Header */

	if (status <= 0)
	{
		xfree(pdu);
		return status;
	}

	s = stream_new(0);
	stream_attach(s, pdu, 16);

	rpc_pdu_header_read(s, &header);

	stream_detach(s);
	stream_free(s);

	content_length = header.frag_length - 16;
	status = tls_read(rpc->tls_out, pdu + 16, content_length);

	if (status < 0)
	{
		xfree(pdu);
		return status;
	}

	if (header.ptype == PTYPE_RTS) /* RTS PDU */
	{
		printf("rpc_out_read error: Unexpected RTS PDU\n");
		return -1;
	}
	else
	{
		/* RTS PDUs are not subject to flow control */
		rpc->VirtualConnection->DefaultOutChannel->BytesReceived += header.frag_length;
		rpc->VirtualConnection->DefaultOutChannel->ReceiverAvailableWindow -= header.frag_length;
	}

	if (length < header.frag_length)
	{
		printf("rpc_out_read error! receive buffer is not large enough\n");
		return -1;
	}

	memcpy(data, pdu, header.frag_length);

#ifdef WITH_DEBUG_RPC
	printf("rpc_out_read(): length: %d\n", header.frag_length);
	freerdp_hexdump(data, header.frag_length);
	printf("\n");
#endif

	xfree(pdu);

	return header.frag_length;
}

int rpc_tsg_write(rdpRpc* rpc, uint8* data, int length, uint16 opnum)
{
	int i;
	int status;
	STREAM* pdu;
	rdpNtlm* ntlm;
	SecBuffer Buffers[2];
	SecBufferDesc Message;
	SECURITY_STATUS encrypt_status;
	rpcconn_request_hdr_t* request_pdu;

	uint8 auth_pad_length = 16 - ((24 + length + 8 + 16) % 16);

	ntlm = rpc->ntlm;

	if (auth_pad_length == 16)
		auth_pad_length = 0;

	request_pdu = xnew(rpcconn_request_hdr_t);
	request_pdu->rpc_vers = 5;
	request_pdu->rpc_vers_minor = 0;
	request_pdu->PTYPE = PTYPE_REQUEST;
	request_pdu->pfc_flags = PFC_FIRST_FRAG | PFC_LAST_FRAG;
	request_pdu->packed_drep[0] = 0x10;
	request_pdu->packed_drep[1] = 0x00;
	request_pdu->packed_drep[2] = 0x00;
	request_pdu->packed_drep[3] = 0x00;
	request_pdu->frag_length = 24 + length + auth_pad_length + 8 + 16;
	request_pdu->auth_length = 16;
	request_pdu->call_id = ++rpc->call_id;

	/* opnum=8 means [MS-TSGU] TsProxySetupReceivePipe, save call_id for checking pipe responses */

	if (opnum == 8)
		rpc->pipe_call_id = rpc->call_id;

	request_pdu->alloc_hint = length;
	request_pdu->p_cont_id = 0x0000;
	request_pdu->opnum = opnum;
	request_pdu->stub_data = data;
	request_pdu->auth_verifier.auth_type = 0x0A; /* :01  which authentication service */
	request_pdu->auth_verifier.auth_level = 0x05; /* :01  which level within service */
	request_pdu->auth_verifier.auth_pad_length = auth_pad_length; /* :01 */
	request_pdu->auth_verifier.auth_pad = xmalloc(auth_pad_length); /* align(4); size_is(auth_pad_length) p */

	for (i = 0; i < auth_pad_length; i++)
	{
		request_pdu->auth_verifier.auth_pad[i] = 0x00;
	}

	request_pdu->auth_verifier.auth_reserved = 0x00; /* :01 reserved, m.b.z. */
	request_pdu->auth_verifier.auth_context_id = 0x00000000; /* :04 */
	request_pdu->auth_verifier.auth_value = xmalloc(request_pdu->auth_length); /* credentials; size_is(auth_length) p */

	pdu = stream_new(request_pdu->frag_length);

	stream_write(pdu, request_pdu, 24);
	stream_write(pdu, request_pdu->stub_data, request_pdu->alloc_hint);

	if (request_pdu->auth_verifier.auth_pad_length > 0)
		stream_write(pdu, request_pdu->auth_verifier.auth_pad, request_pdu->auth_verifier.auth_pad_length);

	stream_write(pdu, &request_pdu->auth_verifier.auth_type, 8);

	if (ntlm->table->QueryContextAttributes(&ntlm->context, SECPKG_ATTR_SIZES, &ntlm->ContextSizes) != SEC_E_OK)
	{
		printf("QueryContextAttributes SECPKG_ATTR_SIZES failure\n");
		return 0;
	}

	Buffers[0].BufferType = SECBUFFER_DATA; /* auth_data */
	Buffers[1].BufferType = SECBUFFER_TOKEN; /* signature */

	Buffers[0].pvBuffer = pdu->data;
	Buffers[0].cbBuffer = stream_get_length(pdu);

	Buffers[1].cbBuffer = ntlm->ContextSizes.cbMaxSignature;
	Buffers[1].pvBuffer = xzalloc(Buffers[1].cbBuffer);

	Message.cBuffers = 2;
	Message.ulVersion = SECBUFFER_VERSION;
	Message.pBuffers = (PSecBuffer) &Buffers;

	encrypt_status = ntlm->table->EncryptMessage(&ntlm->context, 0, &Message, rpc->send_seq_num++);

	if (encrypt_status != SEC_E_OK)
	{
		printf("EncryptMessage status: 0x%08X\n", encrypt_status);
		return 0;
	}

	stream_write(pdu, Buffers[1].pvBuffer, Buffers[1].cbBuffer);

	status = rpc_in_write(rpc, pdu->data, pdu->p - pdu->data);

	xfree(request_pdu->auth_verifier.auth_value);
	xfree(request_pdu->auth_verifier.auth_pad);
	xfree(request_pdu);

	if (status < 0)
	{
		printf("rpc_write(): Error! rpc_tsg_write returned negative value.\n");
		return -1;
	}

	return length;
}

int rpc_read(rdpRpc* rpc, uint8* data, int length)
{
	int status;
	int read = 0;
	int data_length;
	uint16 frag_length;
	uint16 auth_length;
	uint8 auth_pad_length;
	uint32 call_id = -1;
	int rpc_length = length + 0xFF;
	uint8* rpc_data = xmalloc(rpc_length);

	if (rpc->read_buffer_len > 0)
	{
		if (rpc->read_buffer_len > (uint32) length)
		{
			printf("rpc_read error: receiving buffer is not large enough\n");
			return -1;
		}

		memcpy(data, rpc->read_buffer, rpc->read_buffer_len);
		read += rpc->read_buffer_len;
		xfree(rpc->read_buffer);
		rpc->read_buffer_len = 0;
	}

	while (true)
	{
		status = rpc_out_read(rpc, rpc_data, rpc_length);

		if (status == 0)
		{
			xfree(rpc_data);
			return read;
		}
		else if (status < 0)
		{
			printf("Error! rpc_out_read() returned negative value. BytesSent: %d, BytesReceived: %d\n",
					rpc->VirtualConnection->DefaultInChannel->BytesSent,
					rpc->VirtualConnection->DefaultOutChannel->BytesReceived);

			xfree(rpc_data);
			return status;
		}

		frag_length = *(uint16*)(rpc_data + 8);
		auth_length = *(uint16*)(rpc_data + 10);
		call_id = *(uint32*)(rpc_data + 12);
		status = *(uint32*)(rpc_data + 16); /* alloc_hint */
		auth_pad_length = *(rpc_data + frag_length - auth_length - 6); /* -6 = -8 + 2 (sec_trailer + 2) */

		/* data_length must be calculated because alloc_hint carries size of more than one pdu */
		data_length = frag_length - auth_length - 24 - 8 - auth_pad_length; /* 24 is header; 8 is sec_trailer */

		if (status == 4)
			continue;

		if (read + data_length > length) /* if read data is greater then given buffer */
		{
			rpc->read_buffer_len = read + data_length - length;
			rpc->read_buffer = xmalloc(rpc->read_buffer_len);

			data_length -= rpc->read_buffer_len;

			memcpy(rpc->read_buffer, rpc_data + 24 + data_length, rpc->read_buffer_len);
		}

		memcpy(data + read, rpc_data + 24, data_length);

		read += data_length;

		if (status > data_length && read < length)
			continue;

		break;
	}

	xfree(rpc_data);

	return read;
}

boolean rpc_connect(rdpRpc* rpc)
{
	rpc->tls_in = rpc->transport->tls_in;
	rpc->tls_out = rpc->transport->tls_out;

	if (!rts_connect(rpc))
	{
		printf("rts_connect error!\n");
		return false;
	}

	if (!rpc_send_bind_pdu(rpc))
	{
		printf("rpc_send_bind_pdu error!\n");
		return false;
	}

	if (!rpc_recv_bind_ack_pdu(rpc))
	{
		printf("rpc_recv_bind_ack_pdu error!\n");
		return false;
	}

	if (!rpc_send_rpc_auth_3_pdu(rpc))
	{
		printf("rpc_send_rpc_auth_3 error!\n");
		return false;
	}

	return true;
}

void rpc_client_virtual_connection_init(rdpRpc* rpc, RpcVirtualConnection* virtual_connection)
{
	virtual_connection->DefaultInChannel->BytesSent = 0;
	virtual_connection->DefaultOutChannel->BytesReceived = 0;
	virtual_connection->DefaultOutChannel->ReceiverAvailableWindow = rpc->ReceiveWindow;
	virtual_connection->DefaultOutChannel->ReceiveWindow = rpc->ReceiveWindow;
	virtual_connection->DefaultOutChannel->ReceiveWindowSize = rpc->ReceiveWindow;
	virtual_connection->DefaultInChannel->SenderAvailableWindow = rpc->ReceiveWindow;
	virtual_connection->DefaultInChannel->PingOriginator.ConnectionTimeout = 30;
	virtual_connection->DefaultInChannel->PingOriginator.KeepAliveInterval = 0;
}

RpcVirtualConnection* rpc_client_virtual_connection_new(rdpRpc* rpc)
{
	RpcVirtualConnection* virtual_connection = xnew(RpcVirtualConnection);

	if (virtual_connection != NULL)
	{
		virtual_connection->State = VIRTUAL_CONNECTION_STATE_INITIAL;
		virtual_connection->DefaultInChannel = xnew(RpcInChannel);
		virtual_connection->DefaultOutChannel = xnew(RpcOutChannel);
		rpc_client_virtual_connection_init(rpc, virtual_connection);
	}

	return virtual_connection;
}

void rpc_client_virtual_connection_free(RpcVirtualConnection* virtual_connection)
{
	if (virtual_connection != NULL)
	{
		xfree(virtual_connection);
	}
}

rdpNtlmHttp* ntlm_http_new()
{
	rdpNtlmHttp* ntlm_http;

	ntlm_http = xnew(rdpNtlmHttp);

	if (ntlm_http != NULL)
	{
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
	http_context_set_host(ntlm_http->context, rpc->settings->tsg_hostname);

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

rdpRpc* rpc_new(rdpTransport* transport)
{
	rdpRpc* rpc = (rdpRpc*) xnew(rdpRpc);

	if (rpc != NULL)
	{
		rpc->transport = transport;
		rpc->settings = transport->settings;

		rpc->send_seq_num = 0;
		rpc->ntlm = ntlm_new();

		rpc->ntlm_http_in = ntlm_http_new();
		rpc->ntlm_http_out = ntlm_http_new();

		rpc_ntlm_http_init_channel(rpc, rpc->ntlm_http_in, TSG_CHANNEL_IN);
		rpc_ntlm_http_init_channel(rpc, rpc->ntlm_http_out, TSG_CHANNEL_OUT);

		rpc->read_buffer = NULL;
		rpc->write_buffer = NULL;
		rpc->read_buffer_len = 0;
		rpc->write_buffer_len = 0;

		rpc->ReceiveWindow = 0x00010000;
		rpc->VirtualConnection = rpc_client_virtual_connection_new(rpc);

		rpc->call_id = 0;
	}

	return rpc;
}

void rpc_free(rdpRpc* rpc)
{
	if (rpc != NULL)
	{
		ntlm_http_free(rpc->ntlm_http_in);
		ntlm_http_free(rpc->ntlm_http_out);
		rpc_client_virtual_connection_free(rpc->VirtualConnection);
		xfree(rpc);
	}
}
