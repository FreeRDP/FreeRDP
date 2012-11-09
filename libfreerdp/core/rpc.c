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

#include "rpc.h"

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

void rpc_pdu_header_print(RPC_PDU_HEADER* header)
{
	printf("rpc_vers: %d\n", header->rpc_vers);
	printf("rpc_vers_minor: %d\n", header->rpc_vers_minor);

	if (header->ptype > PTYPE_RTS)
		printf("ptype: %s (%d)\n", "PTYPE_UNKNOWN", header->ptype);
	else
		printf("ptype: %s (%d)\n", PTYPE_STRINGS[header->ptype], header->ptype);

	printf("pfc_flags (0x%02X) = {", header->pfc_flags);
	if (header->pfc_flags & PFC_FIRST_FRAG)
		printf(" PFC_FIRST_FRAG");
	if (header->pfc_flags & PFC_LAST_FRAG)
		printf(" PFC_LAST_FRAG");
	if (header->pfc_flags & PFC_PENDING_CANCEL)
		printf(" PFC_PENDING_CANCEL");
	if (header->pfc_flags & PFC_RESERVED_1)
		printf(" PFC_RESERVED_1");
	if (header->pfc_flags & PFC_CONC_MPX)
		printf(" PFC_CONC_MPX");
	if (header->pfc_flags & PFC_DID_NOT_EXECUTE)
		printf(" PFC_DID_NOT_EXECUTE");
	if (header->pfc_flags & PFC_OBJECT_UUID)
		printf(" PFC_OBJECT_UUID");
	printf(" }\n");

	printf("packed_drep[4]: %02X %02X %02X %02X\n",
			header->packed_drep[0], header->packed_drep[1],
			header->packed_drep[2], header->packed_drep[3]);

	printf("frag_length: %d\n", header->frag_length);
	printf("auth_length: %d\n", header->auth_length);
	printf("call_id: %d\n", header->call_id);

	if (header->ptype == PTYPE_RESPONSE)
	{
		rpcconn_response_hdr_t* response;

		response = (rpcconn_response_hdr_t*) header;
		printf("alloc_hint: %d\n", response->alloc_hint);
		printf("p_cont_id: %d\n", response->p_cont_id);
		printf("cancel_count: %d\n", response->cancel_count);
		printf("reserved: %d\n", response->reserved);
	}
}

/**
 * The Security Support Provider Interface:
 * http://technet.microsoft.com/en-us/library/bb742535/
 */

BOOL ntlm_client_init(rdpNtlm* ntlm, BOOL http, char* user, char* domain, char* password)
{
	SECURITY_STATUS status;

	sspi_GlobalInit();

#ifdef WITH_NATIVE_SSPI
	{
		HMODULE hSSPI;
		INIT_SECURITY_INTERFACE InitSecurityInterface;
		PSecurityFunctionTable pSecurityInterface = NULL;

		hSSPI = LoadLibrary(_T("secur32.dll"));

#ifdef UNICODE
		InitSecurityInterface = (INIT_SECURITY_INTERFACE) GetProcAddress(hSSPI, "InitSecurityInterfaceW");
#else
		InitSecurityInterface = (INIT_SECURITY_INTERFACE) GetProcAddress(hSSPI, "InitSecurityInterfaceA");
#endif
		ntlm->table = (*InitSecurityInterface)();
	}
#else
	ntlm->table = InitSecurityInterface();
#endif

	sspi_SetAuthIdentity(&(ntlm->identity), user, domain, password);

	if (http)
	{
		DWORD status;
		DWORD SpnLength;

		SpnLength = 0;
		status = DsMakeSpn(_T("HTTP"), _T("LAB1-W2K8R2-GW.lab1.awake.local"), NULL, 0, NULL, &SpnLength, NULL);

		if (status != ERROR_BUFFER_OVERFLOW)
		{
			_tprintf(_T("DsMakeSpn: expected ERROR_BUFFER_OVERFLOW\n"));
			return -1;
		}

		ntlm->ServicePrincipalName = (LPTSTR) malloc(SpnLength * sizeof(TCHAR));

		status = DsMakeSpn(_T("HTTP"), _T("LAB1-W2K8R2-GW.lab1.awake.local"), NULL, 0, NULL, &SpnLength, ntlm->ServicePrincipalName);
	}

	status = ntlm->table->QuerySecurityPackageInfo(NTLMSP_NAME, &ntlm->pPackageInfo);

	if (status != SEC_E_OK)
	{
		printf("QuerySecurityPackageInfo status: 0x%08X\n", status);
		return FALSE;
	}

	ntlm->cbMaxToken = ntlm->pPackageInfo->cbMaxToken;

	status = ntlm->table->AcquireCredentialsHandle(NULL, NTLMSP_NAME,
			SECPKG_CRED_OUTBOUND, NULL, &ntlm->identity, NULL, NULL, &ntlm->credentials, &ntlm->expiration);

	if (status != SEC_E_OK)
	{
		printf("AcquireCredentialsHandle status: 0x%08X\n", status);
		return FALSE;
	}

	ntlm->haveContext = FALSE;
	ntlm->haveInputBuffer = FALSE;
	ZeroMemory(&ntlm->inputBuffer, sizeof(SecBuffer));
	ZeroMemory(&ntlm->outputBuffer, sizeof(SecBuffer));
	ZeroMemory(&ntlm->ContextSizes, sizeof(SecPkgContext_Sizes));

	ntlm->fContextReq = 0;

	if (http)
	{
		/* flags for HTTP authentication */
		ntlm->fContextReq |= ISC_REQ_CONFIDENTIALITY;
	}
	else
	{
		/** 
		 * flags for RPC authentication:
		 * RPC_C_AUTHN_LEVEL_PKT_INTEGRITY:
		 * ISC_REQ_USE_DCE_STYLE | ISC_REQ_DELEGATE | ISC_REQ_MUTUAL_AUTH |
		 * ISC_REQ_REPLAY_DETECT | ISC_REQ_SEQUENCE_DETECT
		 */

		ntlm->fContextReq |= ISC_REQ_USE_DCE_STYLE;
		ntlm->fContextReq |= ISC_REQ_DELEGATE | ISC_REQ_MUTUAL_AUTH;
		ntlm->fContextReq |= ISC_REQ_REPLAY_DETECT | ISC_REQ_SEQUENCE_DETECT;
	}

	return TRUE;
}

BOOL ntlm_client_make_spn(rdpNtlm* ntlm, LPCTSTR ServiceClass, char* hostname)
{
	int length;
	DWORD status;
	DWORD SpnLength;
	LPTSTR hostnameX;

	length = 0;

#ifdef UNICODE
	length = strlen(hostname);
	hostnameX = (LPWSTR) malloc(length * sizeof(TCHAR));
	MultiByteToWideChar(CP_ACP, 0, hostname, length, hostnameX, length);
	hostnameX[length] = 0;
#else
	hostnameX = hostname;
#endif

	SpnLength = 0;
	status = DsMakeSpn(ServiceClass, hostnameX, NULL, 0, NULL, &SpnLength, NULL);

	if (status != ERROR_BUFFER_OVERFLOW)
		return FALSE;

	ntlm->ServicePrincipalName = (LPTSTR) malloc(SpnLength * sizeof(TCHAR));

	status = DsMakeSpn(ServiceClass, hostnameX, NULL, 0, NULL, &SpnLength, ntlm->ServicePrincipalName);

	if (status != ERROR_SUCCESS)
		return -1;

	return TRUE;
}

BOOL ntlm_authenticate(rdpNtlm* ntlm)
{
	SECURITY_STATUS status;

	ntlm->outputBufferDesc.ulVersion = SECBUFFER_VERSION;
	ntlm->outputBufferDesc.cBuffers = 1;
	ntlm->outputBufferDesc.pBuffers = &ntlm->outputBuffer;
	ntlm->outputBuffer.BufferType = SECBUFFER_TOKEN;
	ntlm->outputBuffer.cbBuffer = ntlm->cbMaxToken;
	ntlm->outputBuffer.pvBuffer = malloc(ntlm->outputBuffer.cbBuffer);

	if (ntlm->haveInputBuffer)
	{
		ntlm->inputBufferDesc.ulVersion = SECBUFFER_VERSION;
		ntlm->inputBufferDesc.cBuffers = 1;
		ntlm->inputBufferDesc.pBuffers = &ntlm->inputBuffer;
		ntlm->inputBuffer.BufferType = SECBUFFER_TOKEN;
	}

	status = ntlm->table->InitializeSecurityContext(&ntlm->credentials,
			(ntlm->haveContext) ? &ntlm->context : NULL,
			(ntlm->ServicePrincipalName) ? ntlm->ServicePrincipalName : NULL,
			ntlm->fContextReq, 0, SECURITY_NATIVE_DREP,
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
			return FALSE ;
		}

		if (status == SEC_I_COMPLETE_NEEDED)
			status = SEC_E_OK;
		else if (status == SEC_I_COMPLETE_AND_CONTINUE)
			status = SEC_I_CONTINUE_NEEDED;
	}

	ntlm->haveInputBuffer = TRUE;
	ntlm->haveContext = TRUE;

	return TRUE;
}

void ntlm_client_uninit(rdpNtlm* ntlm)
{
	ntlm->table->FreeCredentialsHandle(&ntlm->credentials);
	ntlm->table->FreeContextBuffer(ntlm->pPackageInfo);
}

rdpNtlm* ntlm_new()
{
	rdpNtlm* ntlm = (rdpNtlm*) malloc(sizeof(rdpNtlm));

	if (ntlm != NULL)
	{
		ZeroMemory(ntlm, sizeof(rdpNtlm));
	}

	return ntlm;
}

void ntlm_free(rdpNtlm* ntlm)
{
	if (ntlm != NULL)
	{

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
	rdpNtlm* ntlm = rpc->ntlm_http_out->ntlm;

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
	tls_write_all(rpc->tls_out, s->data, s->size);
	stream_free(s);

	/* Receive OUT Channel Response */

	http_response = http_response_recv(rpc->tls_out);

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
	tls_write_all(rpc->tls_out, s->data, s->size);
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
	rdpNtlm* ntlm = rpc->ntlm_http_in->ntlm;

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
	tls_write_all(rpc->tls_in, s->data, s->size);
	stream_free(s);

	/* Receive IN Channel Response */

	http_response = http_response_recv(rpc->tls_in);

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
	tls_write_all(rpc->tls_in, s->data, s->size);
	stream_free(s);

	ntlm_client_uninit(ntlm);
	ntlm_free(ntlm);

	return TRUE;
}

void rpc_pdu_header_init(rdpRpc* rpc, RPC_PDU_HEADER* header)
{
	header->rpc_vers = rpc->rpc_vers;
	header->rpc_vers_minor = rpc->rpc_vers_minor;
	header->packed_drep[0] = rpc->packed_drep[0];
	header->packed_drep[1] = rpc->packed_drep[1];
	header->packed_drep[2] = rpc->packed_drep[2];
	header->packed_drep[3] = rpc->packed_drep[3];
}

int rpc_out_write(rdpRpc* rpc, BYTE* data, int length)
{
	int status;

	rpc_pdu_header_print((RPC_PDU_HEADER*) data);

#ifdef WITH_DEBUG_RPC
	printf("rpc_out_write(): length: %d\n", length);
	freerdp_hexdump(data, length);
	printf("\n");
#endif

	status = tls_write_all(rpc->tls_out, data, length);

	return status;
}

int rpc_in_write(rdpRpc* rpc, BYTE* data, int length)
{
	int status;

	rpc_pdu_header_print((RPC_PDU_HEADER*) data);

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

	rpc_pdu_header_init(rpc, (RPC_PDU_HEADER*) bind_pdu);

	bind_pdu->auth_length = rpc->ntlm->outputBuffer.cbBuffer;
	bind_pdu->auth_verifier.auth_value = rpc->ntlm->outputBuffer.pvBuffer;

	bind_pdu->ptype = PTYPE_BIND;
	bind_pdu->pfc_flags = PFC_FIRST_FRAG | PFC_LAST_FRAG | PFC_PENDING_CANCEL | PFC_CONC_MPX;
	bind_pdu->call_id = 2;

	bind_pdu->max_xmit_frag = 0x0FF8;
	bind_pdu->max_recv_frag = 0x0FF8;
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
	RPC_PDU_HEADER* header;

	status = rpc_recv_pdu(rpc);

	if (status > 0)
	{
		header = (RPC_PDU_HEADER*) rpc->buffer;

		rpc->ntlm->inputBuffer.cbBuffer = header->auth_length;
		rpc->ntlm->inputBuffer.pvBuffer = malloc(header->auth_length);

		auth_data = rpc->buffer + (header->frag_length - header->auth_length);
		CopyMemory(rpc->ntlm->inputBuffer.pvBuffer, auth_data, header->auth_length);

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

	rpc_pdu_header_init(rpc, (RPC_PDU_HEADER*) auth_3_pdu);

	auth_3_pdu->auth_length = rpc->ntlm->outputBuffer.cbBuffer;
	auth_3_pdu->auth_verifier.auth_value = rpc->ntlm->outputBuffer.pvBuffer;

	auth_3_pdu->ptype = PTYPE_RPC_AUTH_3;
	auth_3_pdu->pfc_flags = PFC_FIRST_FRAG | PFC_LAST_FRAG | PFC_CONC_MPX;
	auth_3_pdu->call_id = 2;

	offset = 20;

	auth_3_pdu->max_xmit_frag = 0x0FF8;
	auth_3_pdu->max_recv_frag = 0x0FF8;

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

int rpc_out_read(rdpRpc* rpc, BYTE* data, int length)
{
	int status;
	RPC_PDU_HEADER* header;

	//if (rpc->VirtualConnection->DefaultOutChannel->ReceiverAvailableWindow < 0x00008FFF) /* Just a simple workaround */
	//	rts_send_flow_control_ack_pdu(rpc);  /* Send FlowControlAck every time AvailableWindow reaches the half */

	/* read first 20 bytes to get RPC PDU Header */
	status = tls_read(rpc->tls_out, data, 20);

	if (status <= 0)
		return status;

	header = (RPC_PDU_HEADER*) data;

	rpc_pdu_header_print(header);

	status = tls_read(rpc->tls_out, &data[20], header->frag_length - 20);

	if (status < 0)
		return status;

	if (header->ptype == PTYPE_RTS) /* RTS PDU */
	{
		printf("rpc_out_read error: Unexpected RTS PDU\n");
		return -1;
	}
	else
	{
		/* RTS PDUs are not subject to flow control */
		rpc->VirtualConnection->DefaultOutChannel->BytesReceived += header->frag_length;
		rpc->VirtualConnection->DefaultOutChannel->ReceiverAvailableWindow -= header->frag_length;
	}

	if (length < header->frag_length)
	{
		printf("rpc_out_read error! receive buffer is not large enough: %d < %d\n", length, header->frag_length);
		return -1;
	}

#ifdef WITH_DEBUG_RPC
	printf("rpc_out_read(): length: %d\n", header->frag_length);
	freerdp_hexdump(data, header->frag_length);
	printf("\n");
#endif

	return header->frag_length;
}

int rpc_recv_fault_pdu(RPC_PDU_HEADER* header)
{
	int index;
	rpcconn_fault_hdr_t* fault_pdu;

	fault_pdu = (rpcconn_fault_hdr_t*) header;

	printf("RPC Fault PDU:\n");

	for (index = 0; RPC_FAULT_CODES[index].name != NULL; index++)
	{
		if (RPC_FAULT_CODES[index].code == fault_pdu->status)
		{
			printf("status: %s (0x%08X)\n", RPC_FAULT_CODES[index].name, fault_pdu->status);
			return 0;
		}
	}

	for (index = 0; RPC_FAULT_CODES[index].name != NULL; index++)
	{
		if (RPC_TSG_FAULT_CODES[index].code == fault_pdu->status)
		{
			printf("status: %s (0x%08X)\n", RPC_TSG_FAULT_CODES[index].name, fault_pdu->status);
			return 0;
		}
	}

	printf("status: %s (0x%08X)\n", "UNKNOWN", fault_pdu->status);

	return 0;
}

int rpc_recv_pdu(rdpRpc* rpc)
{
	int status;
	int bytesRead = 0;
	int totalBytesRead = 0;
	RPC_PDU_HEADER* header;
	RPC_PDU_HEADER fragHeader;

	/* read first 20 bytes to get RPC PDU Header */

	ZeroMemory(rpc->buffer, 20);

	while (bytesRead < 20)
	{
		status = tls_read(rpc->tls_out, &rpc->buffer[bytesRead], 20 - bytesRead);

		if (status < 0)
		{
			printf("rpc_recv_pdu: error reading header\n");
			return status;
		}

		bytesRead += status;
	}

	header = (RPC_PDU_HEADER*) rpc->buffer;
	rpc_pdu_header_print(header);

	if (header->frag_length > rpc->length)
	{
		rpc->length = header->frag_length;
		rpc->buffer = (BYTE*) realloc(rpc->buffer, rpc->length);
		header = (RPC_PDU_HEADER*) rpc->buffer;
	}

	while (bytesRead < header->frag_length)
	{
		status = tls_read(rpc->tls_out, &rpc->buffer[bytesRead], header->frag_length - bytesRead);

		if (status < 0)
		{
			printf("rpc_recv_pdu: error reading fragment\n");
			return status;
		}

		bytesRead += status;
	}

	if (!(header->pfc_flags & PFC_LAST_FRAG))
	{
		totalBytesRead = bytesRead;
		CopyMemory(&fragHeader, header, sizeof(RPC_PDU_HEADER));

		printf("Fragmented PDU\n");

		while (!(fragHeader.pfc_flags & PFC_LAST_FRAG))
		{
			bytesRead = 0;

			printf("Reading Fragment Header\n");

			while (bytesRead < 20)
			{
				status = tls_read(rpc->tls_out, &((BYTE*) &fragHeader)[bytesRead], 20 - bytesRead);

				if (status < 0)
				{
					printf("rpc_recv_pdu: error reading fragment header\n");
					return status;
				}

				bytesRead += status;
			}

			if (fragHeader.frag_length > rpc->length)
			{
				rpc->length = header->frag_length;
				rpc->buffer = (BYTE*) realloc(rpc->buffer, rpc->length);
				header = (RPC_PDU_HEADER*) rpc->buffer;
			}

			while (totalBytesRead < (fragHeader.frag_length - 20))
			{
				status = tls_read(rpc->tls_out, &rpc->buffer[totalBytesRead], fragHeader.frag_length - totalBytesRead);

				if (status < 0)
				{
					printf("rpc_recv_pdu: error reading fragment\n");
					return status;
				}

				totalBytesRead += status;
			}
		}
	}

	if (header->ptype == PTYPE_RTS) /* RTS PDU */
	{
		printf("rpc_recv_pdu error: Unexpected RTS PDU\n");
		return -1;
	}
	else if (header->ptype == PTYPE_FAULT)
	{
		rpc_recv_fault_pdu(header);
		return -1;
	}
	else
	{
		/* RTS PDUs are not subject to flow control */
		rpc->VirtualConnection->DefaultOutChannel->BytesReceived += header->frag_length;
		rpc->VirtualConnection->DefaultOutChannel->ReceiverAvailableWindow -= header->frag_length;
	}

#ifdef WITH_DEBUG_RPC
	printf("rpc_recv_pdu: length: %d\n", header->frag_length);
	freerdp_hexdump(rpc->buffer, header->frag_length);
	printf("\n");
#endif

	return header->frag_length;
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

	rpc_pdu_header_init(rpc, (RPC_PDU_HEADER*) request_pdu);

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

	printf("stub_data_pad: %d\n", stub_data_pad);

	offset += length;
	request_pdu->auth_verifier.auth_pad_length = rpc_offset_align(&offset, 4);

	printf("auth_pad_length: %d\n", request_pdu->auth_verifier.auth_pad_length);

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

	status = rpc_in_write(rpc, buffer, request_pdu->frag_length);

	if (status < 0)
	{
		printf("rpc_tsg_write(): Error! rpc_tsg_write returned negative value.\n");
		return -1;
	}

	free(buffer);

	return length;
}

int rpc_read(rdpRpc* rpc, BYTE* data, int length)
{
	int status;
	RPC_PDU_HEADER header;

	status = rpc_out_read(rpc, data, length);

	if (status > 0)
	{
		CopyMemory(&header, data, 20);
		status = rpc_out_read(rpc, data, header.frag_length - 20);
	}

	return status;
}

BOOL rpc_connect(rdpRpc* rpc)
{
	rpc->tls_in = rpc->transport->tls_in;
	rpc->tls_out = rpc->transport->tls_out;

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
	RpcVirtualConnection* virtual_connection = (RpcVirtualConnection*) malloc(sizeof(RpcVirtualConnection));

	if (virtual_connection != NULL)
	{
		ZeroMemory(virtual_connection, sizeof(RpcVirtualConnection));
		virtual_connection->State = VIRTUAL_CONNECTION_STATE_INITIAL;
		virtual_connection->DefaultInChannel = (RpcInChannel*) malloc(sizeof(RpcInChannel));
		virtual_connection->DefaultOutChannel = (RpcOutChannel*) malloc(sizeof(RpcOutChannel));
		ZeroMemory(virtual_connection->DefaultInChannel, sizeof(RpcInChannel));
		ZeroMemory(virtual_connection->DefaultOutChannel, sizeof(RpcOutChannel));
		rpc_client_virtual_connection_init(rpc, virtual_connection);
	}

	return virtual_connection;
}

void rpc_client_virtual_connection_free(RpcVirtualConnection* virtual_connection)
{
	if (virtual_connection != NULL)
	{
		free(virtual_connection);
	}
}

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

		rpc->ntlm_http_in = ntlm_http_new();
		rpc->ntlm_http_out = ntlm_http_new();

		rpc_ntlm_http_init_channel(rpc, rpc->ntlm_http_in, TSG_CHANNEL_IN);
		rpc_ntlm_http_init_channel(rpc, rpc->ntlm_http_out, TSG_CHANNEL_OUT);

		rpc->length = 20;
		rpc->buffer = (BYTE*) malloc(rpc->length);

		rpc->rpc_vers = 5;
		rpc->rpc_vers_minor = 0;

		/* little-endian data representation */
		rpc->packed_drep[0] = 0x10;
		rpc->packed_drep[1] = 0x00;
		rpc->packed_drep[2] = 0x00;
		rpc->packed_drep[3] = 0x00;

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
		free(rpc);
	}
}
