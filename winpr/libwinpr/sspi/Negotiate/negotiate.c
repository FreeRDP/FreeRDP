/**
 * WinPR: Windows Portable Runtime
 * Negotiate Security Package
 *
 * Copyright 2011-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2017 Dorian Ducournau <dorian.ducournau@gmail.com>
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
#include <winpr/sspi.h>
#include <winpr/tchar.h>
#include <winpr/assert.h>
#include <winpr/registry.h>
#include <winpr/build-config.h>

#include "negotiate.h"

#include "../sspi.h"
#include "../../log.h"
#define TAG WINPR_TAG("negotiate")

static const char NEGO_REG_KEY[] =
    "Software\\" WINPR_VENDOR_STRING "\\" WINPR_PRODUCT_STRING "\\SSPI\\Negotiate";

#define ASN_LENGTH_BYTES(len)  \
	((len) <= 0x0000007F   ? 0 \
	 : (len) <= 0x000000FF ? 1 \
	 : (len) <= 0x0000FFFF ? 2 \
	 : (len) <= 0x00FFFFFF ? 3 \
	                       : 4)
#define ASN_TLV_LENGTH(len) (2 + ASN_LENGTH_BYTES(len) + (len))
#define ASN_CONTEXTUAL_LENGTH(len) ASN_TLV_LENGTH(ASN_TLV_LENGTH(len))

extern const SecurityFunctionTableA NTLM_SecurityFunctionTableA;
extern const SecurityFunctionTableW NTLM_SecurityFunctionTableW;

extern const SecurityFunctionTableA KERBEROS_SecurityFunctionTableA;
extern const SecurityFunctionTableW KERBEROS_SecurityFunctionTableW;

extern void ntlm_reset_cipher_state(PSecHandle);

typedef struct
{
	const TCHAR* name;
	const SecurityFunctionTableA* table;
	const SecurityFunctionTableW* table_w;
} SecPkg;

struct Mech_st
{
	const sspi_gss_OID_desc* oid;
	const SecPkg* pkg;
	const UINT init_flags;
	const UINT accept_flags;
};

typedef struct
{
	const Mech* mech;
	CredHandle cred;
	BOOL valid;
} MechCred;

const SecPkgInfoA NEGOTIATE_SecPkgInfoA = {
	0x00083BB3,                    /* fCapabilities */
	1,                             /* wVersion */
	0x0009,                        /* wRPCID */
	0x00002FE0,                    /* cbMaxToken */
	"Negotiate",                   /* Name */
	"Microsoft Package Negotiator" /* Comment */
};

static WCHAR NEGOTIATE_SecPkgInfoW_Name[] = { 'N', 'e', 'g', 'o', 't', 'i', 'a', 't', 'e', '\0' };

static WCHAR NEGOTIATE_SecPkgInfoW_Comment[] = { 'M', 'i', 'c', 'r', 'o', 's', 'o', 'f', 't', ' ',
	                                             'P', 'a', 'c', 'k', 'a', 'g', 'e', ' ', 'N', 'e',
	                                             'g', 'o', 't', 'i', 'a', 't', 'o', 'r', '\0' };

const SecPkgInfoW NEGOTIATE_SecPkgInfoW = {
	0x00083BB3,                   /* fCapabilities */
	1,                            /* wVersion */
	0x0009,                       /* wRPCID */
	0x00002FE0,                   /* cbMaxToken */
	NEGOTIATE_SecPkgInfoW_Name,   /* Name */
	NEGOTIATE_SecPkgInfoW_Comment /* Comment */
};

static const sspi_gss_OID_desc spnego_OID = { 6, "\x2b\x06\x01\x05\x05\x02" };
static const sspi_gss_OID_desc kerberos_OID = { 9, "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02" };
static const sspi_gss_OID_desc ntlm_OID = { 10, "\x2b\x06\x01\x04\x01\x82\x37\x02\x02\x0a" };

static const SecPkg SecPkgTable[] = {
	{ KERBEROS_SSP_NAME, &KERBEROS_SecurityFunctionTableA, &KERBEROS_SecurityFunctionTableW },
	{ NTLM_SSP_NAME, &NTLM_SecurityFunctionTableA, &NTLM_SecurityFunctionTableW }
};

static const Mech MechTable[] = {
	{ &kerberos_OID, &SecPkgTable[0], 0, 0 },
	{ &ntlm_OID, &SecPkgTable[1], 0, 0 },
};

static const int MECH_COUNT = sizeof(MechTable) / sizeof(Mech);

enum NegState
{
	NOSTATE = -1,
	ACCEPT_COMPLETED,
	ACCEPT_INCOMPLETE,
	REJECT,
	REQUEST_MIC
};

typedef struct
{
	enum NegState negState;
	BOOL init;
	const Mech* supportedMech;
	SecBuffer mechTypes;
	SecBuffer mechToken;
	SecBuffer mic;
} NegToken;

static BYTE* negotiate_write_tlv(BYTE* buf, BYTE tag, ULONG len, BYTE* value)
{
	int bytes = 0;

	*buf++ = tag;

	if (len <= 0x7F)
		*buf++ = len;
	else
	{
		while (len >> (++bytes * 8))
			;
		*buf++ = bytes | 0x80;
		for (int i = 0; i < bytes; i++)
			buf[bytes - i - 1] = (BYTE)(len >> (i * 8));
		buf += bytes;
	}

	if (value)
	{
		MoveMemory(buf, value, len);
		buf += len;
	}

	return buf;
}

static BYTE* negotiate_write_contextual_tlv(BYTE* buf, BYTE contextual, BYTE tag, ULONG len,
                                            BYTE* value)
{
	buf = negotiate_write_tlv(buf, contextual, ASN_TLV_LENGTH(len), NULL);
	buf = negotiate_write_tlv(buf, tag, len, value);

	return buf;
}

static BYTE* negotiate_read_tlv(BYTE* buf, BYTE* tag, ULONG* len, ULONG* bytes_remain)
{
	int len_bytes = 0;
	*len = 0;

	*bytes_remain -= 2;
	if (*bytes_remain < 0)
		return buf;

	*tag = *buf++;

	if (*buf <= 0x7F)
		*len = *buf++;
	else
	{
		len_bytes = *buf++ & 0x7F;
		*bytes_remain -= len_bytes;
		if (*bytes_remain < 0)
			return buf;
		for (int i = len_bytes - 1; i >= 0; i--)
			*len |= *buf++ << (i * 8);
	}

	return buf;
}

static NEGOTIATE_CONTEXT* negotiate_ContextNew(NEGOTIATE_CONTEXT* init_context)
{
	NEGOTIATE_CONTEXT* context = NULL;

	context = calloc(1, sizeof(NEGOTIATE_CONTEXT));
	if (!context)
		return NULL;

	if (init_context->spnego)
	{
		init_context->mechTypes.pvBuffer = malloc(init_context->mechTypes.cbBuffer);
		if (!init_context->mechTypes.pvBuffer)
		{
			free(context);
			return NULL;
		}
	}

	CopyMemory(context, init_context, sizeof(NEGOTIATE_CONTEXT));

	return context;
}

static void negotiate_ContextFree(NEGOTIATE_CONTEXT* context)
{
	if (context->mechTypes.pvBuffer)
		free(context->mechTypes.pvBuffer);
	free(context);
}

static const char* negotiate_mech_name(const Mech* mech)
{
	if (mech->oid == &spnego_OID)
		return "SPNEGO (1.3.6.1.5.5.2)";
	else if (mech->oid == &kerberos_OID)
		return "Kerberos (1.2.840.113554.1.2.2)";
	else if (mech->oid == &ntlm_OID)
		return "NTLM (1.3.6.1.4.1.311.2.2.10)";
	else
		return "Unknown mechanism";
}

static const Mech* negotiate_GetMechByOID(const sspi_gss_OID_desc oid)
{
	for (int i = 0; i < MECH_COUNT; i++)
	{
		if (oid.length == MechTable[i].oid->length &&
		    memcmp(oid.elements, MechTable[i].oid->elements, oid.length) == 0)
			return &MechTable[i];
	}
	return NULL;
}

static PSecHandle negotiate_FindCredential(MechCred* creds, const Mech* mech)
{
	for (int i = 0; i < MECH_COUNT; i++)
	{
		if (creds[i].mech == mech)
		{
			if (creds[i].valid)
				return &creds[i].cred;
			break;
		}
	}

	return NULL;
}

static BOOL negotaite_get_dword(HKEY hKey, const char* subkey, DWORD* pdwValue)
{
	DWORD dwValue = 0, dwType = 0;
	DWORD dwSize = sizeof(dwValue);
	LONG rc = RegQueryValueExA(hKey, subkey, NULL, &dwType, (BYTE*)&dwValue, &dwSize);

	if (rc != ERROR_SUCCESS)
		return FALSE;
	if (dwType != REG_DWORD)
		return FALSE;

	*pdwValue = dwValue;
	return TRUE;
}

static BOOL negotiate_get_config(BOOL* kerberos, BOOL* ntlm)
{
	HKEY hKey = NULL;
	LONG rc;

	WINPR_ASSERT(kerberos);
	WINPR_ASSERT(ntlm);

#if !defined(WITH_GSS_NO_NTLM_FALLBACK)
	*ntlm = TRUE;
#else
	*ntlm = FALSE;
#endif
	*kerberos = TRUE;

	rc = RegOpenKeyExA(HKEY_LOCAL_MACHINE, NEGO_REG_KEY, 0, KEY_READ | KEY_WOW64_64KEY, &hKey);
	if (rc == ERROR_SUCCESS)
	{
		DWORD dwValue;

		if (negotaite_get_dword(hKey, "kerberos", &dwValue))
			*kerberos = (dwValue != 0) ? TRUE : FALSE;

#if !defined(WITH_GSS_NO_NTLM_FALLBACK)
		if (negotaite_get_dword(hKey, "ntlm", &dwValue))
			*ntlm = (dwValue != 0) ? TRUE : FALSE;
#endif

		RegCloseKey(hKey);
	}

	return TRUE;
}

static BOOL negotiate_write_neg_token(PSecBuffer output_buffer, NegToken token)
{
	ULONG token_len = 0;
	ULONG init_token_len = 0;
	ULONG inner_token_len = 0;
	ULONG total_len = 0;
	BYTE* p = output_buffer->pvBuffer;
	BYTE *mech_offset, *mic_offset;

	/* Length of [0] MechTypeList (SEQUENCE tag already included in buffer) */
	if (token.init)
		inner_token_len += ASN_TLV_LENGTH(token.mechTypes.cbBuffer);

	/* Lenght of negState [0] ENUMERATED */
	if (token.negState != NOSTATE)
		inner_token_len += ASN_CONTEXTUAL_LENGTH(1);

	/* Length of supportedMech [1] OID */
	if (token.supportedMech)
		inner_token_len += ASN_CONTEXTUAL_LENGTH(token.supportedMech->oid->length);

	/* Length of [2] OCTET STRING */
	if (token.mechToken.cbBuffer)
	{
		inner_token_len += ASN_CONTEXTUAL_LENGTH(token.mechToken.cbBuffer);
		mech_offset = p + inner_token_len - token.mechToken.cbBuffer;
	}

	/* Length of [3] OCTET S */
	if (token.mic.cbBuffer)
	{
		inner_token_len += ASN_CONTEXTUAL_LENGTH(token.mic.cbBuffer);
		mic_offset = p + inner_token_len - token.mic.cbBuffer;
	}

	/* Length of [0] NegTokenInit | [1] NegTokenResp */
	token_len += ASN_CONTEXTUAL_LENGTH(inner_token_len);

	total_len = token_len;

	if (token.init)
	{
		/* Length of MechType OID */
		init_token_len = total_len + ASN_TLV_LENGTH(spnego_OID.length);

		/* Length of initialContextToken */
		total_len = ASN_TLV_LENGTH(init_token_len);
	}

	/* Adjust token offsets */
	mech_offset += total_len - inner_token_len;
	mic_offset += total_len - inner_token_len;

	if (output_buffer->cbBuffer < total_len)
		return FALSE;
	output_buffer->cbBuffer = total_len;

	/* Write the tokens stored in the buffer first so as not to be overwritten */
	if (token.mic.cbBuffer)
		MoveMemory(mic_offset, token.mic.pvBuffer, token.mic.cbBuffer);

	if (token.mechToken.cbBuffer)
		MoveMemory(mech_offset, token.mechToken.pvBuffer, token.mechToken.cbBuffer);

	/* For NegTokenInit wrap in an initialContextToken */
	if (token.init)
	{
		/* InitialContextToken [APPLICATION 0] IMPLICIT SEQUENCE */
		p = negotiate_write_tlv(p, 0x60, init_token_len, NULL);

		/* thisMech MechType OID */
		p = negotiate_write_tlv(p, 0x06, spnego_OID.length, spnego_OID.elements);
	}

	/* innerContextToken [0] NegTokenInit or [1] NegTokenResp */
	p = negotiate_write_contextual_tlv(p, token.init ? 0xA0 : 0xA1, 0x30, inner_token_len, NULL);
	WLog_DBG(TAG, token.init ? "Writing negTokenInit..." : "Writing negTokenResp...");

	/* mechTypes [0] MechTypeList (mechTypes already contains the SEQUENCE tag) */
	if (token.init)
	{
		p = negotiate_write_tlv(p, 0xA0, token.mechTypes.cbBuffer, token.mechTypes.pvBuffer);
		WLog_DBG(TAG, "\tmechTypes [0] (%li bytes)", token.mechTypes.cbBuffer);
	}
	/* negState [0] ENUMERATED */
	else if (token.negState != NOSTATE)
	{
		p = negotiate_write_contextual_tlv(p, 0xA0, 0x0A, 1, (BYTE*)&token.negState);
		WLog_DBG(TAG, "\tnegState [0] (%d)", token.negState);
	}

	/* supportedMech [1] OID */
	if (token.supportedMech)
	{
		p = negotiate_write_contextual_tlv(p, 0xA1, 0x06, token.supportedMech->oid->length,
		                                   token.supportedMech->oid->elements);
		WLog_DBG(TAG, "\tsupportedMech [1] (%s)", negotiate_mech_name(token.supportedMech));
	}

	/* mechToken [2] OCTET STRING */
	if (token.mechToken.cbBuffer)
	{
		p = negotiate_write_contextual_tlv(p, 0xA2, 0x04, token.mechToken.cbBuffer, NULL);
		p += token.mechToken.cbBuffer;
		WLog_DBG(TAG, "\tmechToken [2] (%li bytes)", token.mechToken.cbBuffer);
	}

	/* mechListMIC [3] OCTET STRING */
	if (token.mic.cbBuffer)
	{
		p = negotiate_write_contextual_tlv(p, 0xA3, 0x04, token.mic.cbBuffer, NULL);
		p += token.mic.cbBuffer;
		WLog_DBG(TAG, "\tmechListMIC [3] (%li bytes)", token.mic.cbBuffer);
	}

	return TRUE;
}

static BOOL negotiate_read_neg_token(PSecBuffer input, NegToken* token)
{
	BYTE tag;
	BYTE contextual;
	ULONG len;
	BYTE* buf = input->pvBuffer;
	BYTE* p;
	ULONG bytes_remain = input->cbBuffer;

	if (token->init)
	{
		/* initContextToken */
		buf = negotiate_read_tlv(buf, &tag, &len, &bytes_remain);
		if (len > bytes_remain || tag != 0x60)
			return FALSE;

		/* thisMech */
		buf = negotiate_read_tlv(buf, &tag, &len, &bytes_remain);
		if (len > bytes_remain || tag != 0x06)
			return FALSE;

		buf += len;
		bytes_remain -= len;
	}

	/* [0] NegTokenInit or [1] NegTokenResp */
	buf = negotiate_read_tlv(buf, &contextual, &len, &bytes_remain);
	buf = negotiate_read_tlv(buf, &tag, &len, &bytes_remain);
	if (len > bytes_remain)
		return FALSE;
	else if (contextual == 0xA0 && tag == 0x30)
		token->init = TRUE;
	else if (contextual == 0xA1 && tag == 0x30)
		token->init = FALSE;
	else
		return FALSE;

	WLog_DBG(TAG, token->init ? "Reading negTokenInit..." : "Reading negTokenResp...");

	/* Read NegTokenResp sequence members */
	do
	{
		p = negotiate_read_tlv(buf, &contextual, &len, &bytes_remain);
		buf = negotiate_read_tlv(p, &tag, &len, &bytes_remain);
		if (len > bytes_remain)
			return FALSE;

		switch (contextual)
		{
			case 0xA0:
				/* mechTypes [0] MechTypeList */
				if (tag == 0x30 && token->init)
				{
					token->mechTypes.pvBuffer = p;
					token->mechTypes.cbBuffer = ASN_TLV_LENGTH(len);
					token->mechTypes.BufferType = SECBUFFER_DATA;
					WLog_DBG(TAG, "\tmechTypes [0] (%li bytes)", len);
				}
				/* negState [0] ENUMERATED */
				else if (tag == 0x0A && len == 1 && !token->init)
				{
					token->negState = *buf;
					WLog_DBG(TAG, "\tnegState [0] (%d)", token->negState);
				}
				else
					return FALSE;
				break;
			case 0xA1:
				/* reqFlags [1] ContextFlags BIT STRING (ignored) */
				if (tag == 0x03 && token->init)
				{
					WLog_DBG(TAG, "\treqFlags [1] (%li bytes)", len);
				}
				/* supportedMech [1] MechType */
				else if (tag == 0x06 && !token->init)
				{
					token->supportedMech = negotiate_GetMechByOID((sspi_gss_OID_desc){ len, buf });
					WLog_DBG(TAG, "\tsupportedMech [1] (%s)",
					         negotiate_mech_name(token->supportedMech));
				}
				else
					return FALSE;
				break;
			case 0xA2:
				/* mechToken [2] OCTET STRING */
				if (tag != 0x04)
					return FALSE;
				token->mechToken.cbBuffer = len;
				token->mechToken.pvBuffer = buf;
				token->mechToken.BufferType = SECBUFFER_TOKEN;
				WLog_DBG(TAG, "\tmechToken [2] (%li bytes)", len);
				break;
			case 0xA3:
				/* mechListMic [3] OCTET STRING */
				if (tag != 0x04)
					return FALSE;
				token->mic.cbBuffer = len;
				token->mic.pvBuffer = buf;
				token->mic.BufferType = SECBUFFER_TOKEN;
				WLog_DBG(TAG, "\tmechListMIC [3] (%li bytes)", len);
				break;
			default:
				return FALSE;
		}
		buf += len;
		bytes_remain -= len;
	} while (bytes_remain > 0);

	if (bytes_remain < 0)
		return FALSE;
	return TRUE;
}

static SECURITY_STATUS negotiate_mic_exchange(NEGOTIATE_CONTEXT* context, NegToken* input_token,
                                              NegToken* output_token, PSecBuffer output_buffer)
{
	SecBuffer mic_buffers[2];
	SecBufferDesc mic_buffer_desc = { SECBUFFER_VERSION, 2, mic_buffers };
	SecPkgContext_Sizes sizes;
	SECURITY_STATUS status;

	CopyMemory(mic_buffers, &context->mechTypes, sizeof(SecBuffer));

	/* Verify MIC if we received one */
	if (input_token->mic.cbBuffer > 0)
	{
		CopyMemory(&mic_buffers[1], &input_token->mic, sizeof(SecBuffer));

		status = context->mech->pkg->table->VerifySignature(&context->sub_context, &mic_buffer_desc,
		                                                    0, 0);
		if (status != SEC_E_OK)
			return status;

		output_token->negState = ACCEPT_COMPLETED;
	}

	/* If peer expects a MIC then generate it */
	if (input_token->negState != ACCEPT_COMPLETED)
	{
		if (context->mech->pkg->table->QueryContextAttributesA(
		        &context->sub_context, SECPKG_ATTR_SIZES, &sizes) != SEC_E_OK)
			return SEC_E_INTERNAL_ERROR;

		/* Store the mic token after the mech token in the output buffer */
		output_token->mic.BufferType = SECBUFFER_TOKEN;
		output_token->mic.cbBuffer = output_buffer->cbBuffer - output_token->mechToken.cbBuffer;
		output_token->mic.pvBuffer = output_buffer->pvBuffer + output_token->mechToken.cbBuffer;

		CopyMemory(&mic_buffers[1], &output_token->mic, sizeof(SecBuffer));

		status =
		    context->mech->pkg->table->MakeSignature(&context->sub_context, 0, &mic_buffer_desc, 0);
		if (status != SEC_E_OK)
			return status;

		CopyMemory(&output_token->mic, &mic_buffers[1], sizeof(SecBuffer));
	}

	/* When using NTLM cipher states need to be reset after mic exchange */
	if (_tcscmp(sspi_SecureHandleGetUpperPointer(&context->sub_context), NTLM_SSP_NAME) == 0)
		ntlm_reset_cipher_state(&context->sub_context);

	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY negotiate_InitializeSecurityContextW(
    PCredHandle phCredential, PCtxtHandle phContext, SEC_WCHAR* pszTargetName, ULONG fContextReq,
    ULONG Reserved1, ULONG TargetDataRep, PSecBufferDesc pInput, ULONG Reserved2,
    PCtxtHandle phNewContext, PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	NEGOTIATE_CONTEXT* context = NULL;
	NEGOTIATE_CONTEXT init_context = { 0 };
	MechCred* creds;
	PCtxtHandle sub_context = NULL;
	PCredHandle sub_cred = NULL;
	NegToken input_token = { NOSTATE, 0 };
	NegToken output_token = { NOSTATE, 0 };
	PSecBuffer input_buffer = NULL;
	PSecBuffer output_buffer = NULL;
	SecBufferDesc mech_input = { SECBUFFER_VERSION, 1, &input_token.mechToken };
	SecBufferDesc mech_output = { SECBUFFER_VERSION, 1, &output_token.mechToken };
	SECURITY_STATUS status;
	ULONG inner_mech_list_len = 0;
	BYTE* p;

	if (!phCredential || !SecIsValidHandle(phCredential))
		return SEC_E_NO_CREDENTIALS;

	context = sspi_SecureHandleGetLowerPointer(phContext);
	creds = sspi_SecureHandleGetLowerPointer(phCredential);
	if (pInput)
		input_buffer = sspi_FindSecBuffer(pInput, SECBUFFER_TOKEN);
	if (pOutput)
		output_buffer = sspi_FindSecBuffer(pOutput, SECBUFFER_TOKEN);

	if (!context)
	{
		for (int i = 0; i < MECH_COUNT; i++)
		{
			if (!creds[i].valid)
				continue;

			inner_mech_list_len += ASN_TLV_LENGTH(creds[i].mech->oid->length);

			if (init_context.mech) /* We already have an optimistic mechanism */
				continue;

			/* Use the output buffer to store the optimistic token */
			CopyMemory(&output_token.mechToken, output_buffer, sizeof(SecBuffer));

			status = MechTable[i].pkg->table_w->InitializeSecurityContextW(
			    &creds[i].cred, NULL, pszTargetName, fContextReq | creds[i].mech->init_flags,
			    Reserved1, TargetDataRep, NULL, Reserved2, &init_context.sub_context, &mech_output,
			    pfContextAttr, ptsExpiry);

			/* If the mechanism failed we can't use it; skip */
			if (IsSecurityStatusError(status))
				creds[i].valid = FALSE;
			else
				init_context.mech = creds[i].mech;
		}

		/* No usable mechanisms were found */
		if (!init_context.mech)
			return status;

#ifdef WITH_SPNEGO
		/* If the only available mech is NTLM use it directly otherwise use spnego */
		if (init_context.mech->oid == &ntlm_OID)
		{
			init_context.spnego = FALSE;
			output_buffer->cbBuffer = output_token.mechToken.cbBuffer;
			WLog_DBG(TAG, "Using direct NTLM");
		}
		else
		{
			init_context.spnego = TRUE;
			init_context.mechTypes.BufferType = SECBUFFER_DATA;
			init_context.mechTypes.cbBuffer = ASN_TLV_LENGTH(inner_mech_list_len);
		}
#else
		init_context.spnego = FALSE;
		output_buffer->cbBuffer = output_token.mechToken.cbBuffer;
#endif

		/* Allocate memory for the new context */
		context = negotiate_ContextNew(&init_context);
		if (!context)
		{
			init_context.mech->pkg->table->DeleteSecurityContext(&init_context.sub_context);
			return SEC_E_INSUFFICIENT_MEMORY;
		}

		sspi_SecureHandleSetUpperPointer(phNewContext, NEGO_SSP_NAME);
		sspi_SecureHandleSetLowerPointer(phNewContext, context);

		if (!context->spnego)
			return status;

		/* Write the SEQUENCE tag */
		p = negotiate_write_tlv(context->mechTypes.pvBuffer, 0x30, inner_mech_list_len, NULL);

		/* Write each enabled mechanism */
		for (int i = 0; i < MECH_COUNT; i++)
		{
			if (creds[i].valid)
			{
				p = negotiate_write_tlv(p, 0x06, creds[i].mech->oid->length,
				                        creds[i].mech->oid->elements);
				WLog_DBG(TAG, "Available mechanism: %s", negotiate_mech_name(creds[i].mech));
			}
		}

		output_token.mechTypes.cbBuffer = context->mechTypes.cbBuffer;
		output_token.mechTypes.pvBuffer = context->mechTypes.pvBuffer;
		output_token.init = TRUE;
	}
	else
	{
		if (!input_buffer)
			return SEC_E_INVALID_TOKEN;

		sub_context = &context->sub_context;
		sub_cred = negotiate_FindCredential(creds, init_context.mech);

		if (!context->spnego)
		{
			return context->mech->pkg->table_w->InitializeSecurityContextW(
			    sub_cred, sub_context, pszTargetName, fContextReq, Reserved1, TargetDataRep, pInput,
			    Reserved2, sub_context, pOutput, pfContextAttr, ptsExpiry);
		}

		if (!negotiate_read_neg_token(input_buffer, &input_token))
			return SEC_E_INVALID_TOKEN;

		/* On first response check if the server doesn't like out prefered mech */
		if (context->state == NEGOTIATE_STATE_INITIAL && input_token.supportedMech &&
		    input_token.supportedMech != context->mech)
		{
			/* Make sure the specified mech is supported and get the appropriate credential */
			sub_cred = negotiate_FindCredential(creds, input_token.supportedMech);
			if (!sub_cred)
				return SEC_E_INVALID_TOKEN;

			/* Clean up the optimistic mech */
			context->mech->pkg->table_w->DeleteSecurityContext(&context->sub_context);
			sub_context = NULL;

			context->mech = input_token.supportedMech;
			context->mic = TRUE;
		}

		/* Check neg_state (required on first response) */
		if (context->state == NEGOTIATE_STATE_INITIAL)
		{
			switch (input_token.negState)
			{
				case NOSTATE:
					return SEC_E_INVALID_TOKEN;
				case REJECT:
					return SEC_E_LOGON_DENIED;
				case REQUEST_MIC:
					context->mic = TRUE; /* fallthrough */
				case ACCEPT_INCOMPLETE:
				case ACCEPT_COMPLETED:
					context->state = NEGOTIATE_STATE_NEGORESP;
					break;
			}

			WLog_DBG(TAG, "Negotiated mechanism: %s", negotiate_mech_name(context->mech));
		}

		if (context->state == NEGOTIATE_STATE_NEGORESP)
		{
			/* Store the mech token in the output buffer */
			CopyMemory(&output_token.mechToken, output_buffer, sizeof(SecBuffer));

			status = context->mech->pkg->table_w->InitializeSecurityContextW(
			    sub_cred, sub_context, pszTargetName, fContextReq | context->mech->init_flags,
			    Reserved1, TargetDataRep, input_token.mechToken.cbBuffer ? &mech_input : NULL,
			    Reserved2, &context->sub_context, &mech_output, pfContextAttr, ptsExpiry);

			if (IsSecurityStatusError(status))
				return status;
		}

		if (status == SEC_E_OK)
		{
			if (output_token.mechToken.cbBuffer > 0)
				context->state = NEGOTIATE_STATE_MIC;
			else
				context->state = NEGOTIATE_STATE_FINAL;
		}

		/* Check if the acceptor sent its final token without a mic */
		if (context->state == NEGOTIATE_STATE_FINAL && input_token.mic.cbBuffer == 0)
		{
			if (context->mic || input_token.negState != ACCEPT_COMPLETED)
				return SEC_E_INVALID_TOKEN;
			else
			{
				output_buffer->cbBuffer = 0;
				return SEC_E_OK;
			}
		}

		if ((context->state == NEGOTIATE_STATE_MIC && context->mic) ||
		    context->state == NEGOTIATE_STATE_FINAL)
		{
			status = negotiate_mic_exchange(context, &input_token, &output_token, output_buffer);
			if (status != SEC_E_OK)
				return status;
		}
	}

	if (input_token.negState == ACCEPT_COMPLETED)
	{
		output_buffer->cbBuffer = 0;
		return SEC_E_OK;
	}

	if (output_token.negState == ACCEPT_COMPLETED)
		status = SEC_E_OK;
	else
		status = SEC_I_CONTINUE_NEEDED;

	if (!negotiate_write_neg_token(output_buffer, output_token))
		status = SEC_E_INTERNAL_ERROR;

	return status;
}

static SECURITY_STATUS SEC_ENTRY negotiate_InitializeSecurityContextA(
    PCredHandle phCredential, PCtxtHandle phContext, SEC_CHAR* pszTargetName, ULONG fContextReq,
    ULONG Reserved1, ULONG TargetDataRep, PSecBufferDesc pInput, ULONG Reserved2,
    PCtxtHandle phNewContext, PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;
	SEC_WCHAR* pszTargetNameW = NULL;

	if (pszTargetName)
	{
		if (ConvertToUnicode(CP_UTF8, 0, pszTargetName, -1, &pszTargetNameW, 0) <= 0)
			return SEC_E_INTERNAL_ERROR;
	}

	status = negotiate_InitializeSecurityContextW(
	    phCredential, phContext, pszTargetNameW, fContextReq, Reserved1, TargetDataRep, pInput,
	    Reserved2, phNewContext, pOutput, pfContextAttr, ptsExpiry);
	free(pszTargetNameW);
	return status;
}

static SECURITY_STATUS SEC_ENTRY negotiate_AcceptSecurityContext(
    PCredHandle phCredential, PCtxtHandle phContext, PSecBufferDesc pInput, ULONG fContextReq,
    ULONG TargetDataRep, PCtxtHandle phNewContext, PSecBufferDesc pOutput, PULONG pfContextAttr,
    PTimeStamp ptsTimeStamp)
{
	NEGOTIATE_CONTEXT* context = NULL;
	NEGOTIATE_CONTEXT init_context = { 0 };
	MechCred* creds;
	PCredHandle sub_cred = NULL;
	NegToken input_token = { NOSTATE, 0 };
	NegToken output_token = { NOSTATE, 0 };
	PSecBuffer input_buffer = NULL;
	PSecBuffer output_buffer = NULL;
	SecBufferDesc mech_input = { SECBUFFER_VERSION, 1, &input_token.mechToken };
	SecBufferDesc mech_output = { SECBUFFER_VERSION, 1, &output_token.mechToken };
	SECURITY_STATUS status;
	BYTE *p, tag;
	ULONG bytes_remain, len;

	if (!phCredential || !SecIsValidHandle(phCredential))
		return SEC_E_NO_CREDENTIALS;

	if (!pInput)
		return SEC_E_INVALID_TOKEN;

	context = sspi_SecureHandleGetLowerPointer(phContext);
	creds = sspi_SecureHandleGetLowerPointer(phCredential);
	input_buffer = sspi_FindSecBuffer(pInput, SECBUFFER_TOKEN);
	if (pOutput)
		output_buffer = sspi_FindSecBuffer(pOutput, SECBUFFER_TOKEN);

	if (!context)
	{
		/* Check for NTLM token */
		if (strncmp(input_buffer->pvBuffer, "NTLMSSP", 8) == 0)
		{
			init_context.mech = negotiate_GetMechByOID(ntlm_OID);
		}
		else
		{
			/* Read initialContextToken */
			bytes_remain = input_buffer->cbBuffer;
			p = negotiate_read_tlv(input_buffer->pvBuffer, &tag, &len, &bytes_remain);
			if (len > bytes_remain || tag != 0x60)
				return SEC_E_INVALID_TOKEN;

			/* Read thisMech */
			p = negotiate_read_tlv(p, &tag, &len, &bytes_remain);
			if (len > bytes_remain || tag != 0x06)
				return SEC_E_INVALID_TOKEN;

			/* Check if it's a spnego token */
			if (len == spnego_OID.length || memcmp(p, spnego_OID.elements, len) == 0)
			{
				init_context.spnego = TRUE;
			}
			else
			{
				init_context.mech = negotiate_GetMechByOID((sspi_gss_OID_desc){ len, p });
				if (!init_context.mech)
					return SEC_E_INVALID_TOKEN;
			}
		}

		WLog_DBG(TAG, "Mechanism: %s", negotiate_mech_name(init_context.mech));

		if (init_context.spnego)
		{
			/* Process spnego token */
			input_token.init = TRUE;
			if (!negotiate_read_neg_token(input_buffer, &input_token))
				return SEC_E_INVALID_TOKEN;

			/* First token must be negoTokenInit and must contain a mechList */
			if (!input_token.init || input_token.mechTypes.cbBuffer == 0)
				return SEC_E_INVALID_TOKEN;

			init_context.mechTypes.BufferType = SECBUFFER_DATA;
			init_context.mechTypes.cbBuffer = input_token.mechTypes.cbBuffer;

			p = negotiate_read_tlv(p, &tag, &len, &bytes_remain);
			if (len > bytes_remain || tag != 0x06)
				return SEC_E_INVALID_TOKEN;

			init_context.mech = negotiate_GetMechByOID((sspi_gss_OID_desc){ len, p });
			sub_cred = negotiate_FindCredential(creds, init_context.mech);

			if (sub_cred)
			{
				/* Use the output buffer to store the optimistic token */
				CopyMemory(&output_token.mechToken, output_buffer, sizeof(SecBuffer));

				status = init_context.mech->pkg->table->AcceptSecurityContext(
				    sub_cred, NULL, &mech_input, fContextReq, TargetDataRep,
				    &init_context.sub_context, &mech_output, pfContextAttr, ptsTimeStamp);
			}
			else
			{
				status = SEC_E_NO_CREDENTIALS;
			}

			WLog_DBG(TAG, "Initiators preferred mechanism: %s",
			         negotiate_mech_name(init_context.mech));
		}
		else
		{
			sub_cred = negotiate_FindCredential(creds, init_context.mech);

			status = init_context.mech->pkg->table->AcceptSecurityContext(
			    sub_cred, NULL, pInput, fContextReq, TargetDataRep, &init_context.sub_context,
			    pOutput, pfContextAttr, ptsTimeStamp);
		}

		if (IsSecurityStatusError(status))
		{
			if (!init_context.spnego)
				return status;

			init_context.mic = TRUE;
			init_context.mech = NULL;
		}

		while (!init_context.mech && bytes_remain > 0)
		{
			/* Prepare to read mechList */
			bytes_remain = input_token.mechTypes.cbBuffer;
			p = negotiate_read_tlv(input_token.mechTypes.pvBuffer, &tag, &len, &bytes_remain);
			if (len > bytes_remain || tag != 0x30)
				return SEC_E_INVALID_TOKEN;

			/* Read each mechanism */
			p = negotiate_read_tlv(p, &tag, &len, &bytes_remain);
			if (len > bytes_remain || tag != 0x06)
				return SEC_E_INVALID_TOKEN;

			init_context.mech = negotiate_GetMechByOID((sspi_gss_OID_desc){ len, p });
			if (!negotiate_FindCredential(creds, init_context.mech))
				init_context.mech = NULL;
		}

		if (!init_context.mech)
			return SEC_E_INTERNAL_ERROR;

		context = negotiate_ContextNew(&init_context);
		if (!context)
		{
			if (!IsSecurityStatusError(status))
				init_context.mech->pkg->table->DeleteSecurityContext(&init_context.sub_context);
			return SEC_E_INSUFFICIENT_MEMORY;
		}

		sspi_SecureHandleSetUpperPointer(phNewContext, NEGO_SSP_NAME);
		sspi_SecureHandleSetLowerPointer(phNewContext, context);

		if (!init_context.spnego)
			return status;

		CopyMemory(init_context.mechTypes.pvBuffer, input_token.mechTypes.pvBuffer,
		           input_token.mechTypes.cbBuffer);

		/* Check if the chosen mechanism is our most preferred; otherwise request mic */
		for (int i = 0; i < MECH_COUNT; i++)
		{
			if (context->mech != creds[i].mech)
			{
				output_token.negState = REQUEST_MIC;
				context->mic = TRUE;
			}
			else
			{
				output_token.negState = ACCEPT_INCOMPLETE;
			}
			break;
		}

		context->state = NEGOTIATE_STATE_NEGORESP;
		output_token.supportedMech = context->mech;
		WLog_DBG(TAG, "Accepted mechanism: %s", negotiate_mech_name(context->mech));
	}
	else
	{
		sub_cred = negotiate_FindCredential(creds, context->mech);
		if (!sub_cred)
			return SEC_E_NO_CREDENTIALS;

		if (!context->spnego)
		{
			return context->mech->pkg->table->AcceptSecurityContext(
			    sub_cred, &context->sub_context, pInput, fContextReq, TargetDataRep,
			    &context->sub_context, pOutput, pfContextAttr, ptsTimeStamp);
		}

		if (!negotiate_read_neg_token(input_buffer, &input_token))
			return SEC_E_INVALID_TOKEN;

		/* Process the mechanism token */
		if (input_token.mechToken.cbBuffer > 0)
		{
			if (context->state != NEGOTIATE_STATE_NEGORESP)
				return SEC_E_INVALID_TOKEN;

			/* Use the output buffer to store the optimistic token */
			CopyMemory(&output_token.mechToken, output_buffer, sizeof(SecBuffer));

			status = context->mech->pkg->table->AcceptSecurityContext(
			    sub_cred, &context->sub_context, &mech_input,
			    fContextReq | context->mech->accept_flags, TargetDataRep, &context->sub_context,
			    pOutput, pfContextAttr, ptsTimeStamp);

			if (IsSecurityStatusError(status))
				return status;

			if (status == SEC_E_OK)
				context->state = NEGOTIATE_STATE_FINAL;
		}
		else if (context->state == NEGOTIATE_STATE_NEGORESP)
			return SEC_E_INVALID_TOKEN;
	}

	if (context->state == NEGOTIATE_STATE_FINAL)
	{
		/* Check if initiator sent the last mech token without a mic and a mic was required */
		if (context->mic && output_token.mechToken.cbBuffer == 0 && input_token.mic.cbBuffer == 0)
			return SEC_E_INVALID_TOKEN;

		if (context->mic || input_token.mic.cbBuffer > 0)
		{
			status = negotiate_mic_exchange(context, &input_token, &output_token, output_buffer);
			if (status != SEC_E_OK)
				return status;
		}
		else
			output_token.negState = ACCEPT_COMPLETED;
	}

	if (input_token.negState == ACCEPT_COMPLETED)
	{
		output_buffer->cbBuffer = 0;
		return SEC_E_OK;
	}

	if (output_token.negState == ACCEPT_COMPLETED)
		status = SEC_E_OK;
	else
		status = SEC_I_CONTINUE_NEEDED;

	if (!negotiate_write_neg_token(output_buffer, output_token))
		return SEC_E_INTERNAL_ERROR;

	return status;
}

static SECURITY_STATUS SEC_ENTRY negotiate_CompleteAuthToken(PCtxtHandle phContext,
                                                             PSecBufferDesc pToken)
{
	NEGOTIATE_CONTEXT* context;
	SECURITY_STATUS status = SEC_E_OK;
	context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
		return SEC_E_INVALID_HANDLE;

	if (context->mech->pkg->table->CompleteAuthToken)
		status = context->mech->pkg->table->CompleteAuthToken(&context->sub_context, pToken);

	return status;
}

static SECURITY_STATUS SEC_ENTRY negotiate_DeleteSecurityContext(PCtxtHandle phContext)
{
	NEGOTIATE_CONTEXT* context;
	SECURITY_STATUS status = SEC_E_OK;
	context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);
	const SecPkg* pkg;

	if (!context)
		return SEC_E_INVALID_HANDLE;

	pkg = context->mech->pkg;

	if (pkg->table->DeleteSecurityContext)
		status = pkg->table->DeleteSecurityContext(&context->sub_context);

	negotiate_ContextFree(context);
	return status;
}

static SECURITY_STATUS SEC_ENTRY negotiate_ImpersonateSecurityContext(PCtxtHandle phContext)
{
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY negotiate_RevertSecurityContext(PCtxtHandle phContext)
{
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY negotiate_QueryContextAttributesW(PCtxtHandle phContext,
                                                                   ULONG ulAttribute, void* pBuffer)
{
	NEGOTIATE_CONTEXT* context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
		return SEC_E_INVALID_HANDLE;

	if (context->mech->pkg->table_w->QueryContextAttributesW)
		return context->mech->pkg->table_w->QueryContextAttributesW(&context->sub_context,
		                                                            ulAttribute, pBuffer);
	else
		return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY negotiate_QueryContextAttributesA(PCtxtHandle phContext,
                                                                   ULONG ulAttribute, void* pBuffer)
{
	NEGOTIATE_CONTEXT* context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
		return SEC_E_INVALID_HANDLE;

	if (context->mech->pkg->table->QueryContextAttributesA)
		return context->mech->pkg->table->QueryContextAttributesA(&context->sub_context,
		                                                          ulAttribute, pBuffer);
	else
		return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY negotiate_SetContextAttributesW(PCtxtHandle phContext,
                                                                 ULONG ulAttribute, void* pBuffer,
                                                                 ULONG cbBuffer)
{
	NEGOTIATE_CONTEXT* context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
		return SEC_E_INVALID_HANDLE;

	if (context->mech->pkg->table_w->SetContextAttributesW)
		return context->mech->pkg->table_w->SetContextAttributesW(&context->sub_context,
		                                                          ulAttribute, pBuffer, cbBuffer);
	else
		return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY negotiate_SetContextAttributesA(PCtxtHandle phContext,
                                                                 ULONG ulAttribute, void* pBuffer,
                                                                 ULONG cbBuffer)
{
	NEGOTIATE_CONTEXT* context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
		return SEC_E_INVALID_HANDLE;

	if (context->mech->pkg->table->SetContextAttributesA)
		return context->mech->pkg->table->SetContextAttributesA(&context->sub_context, ulAttribute,
		                                                        pBuffer, cbBuffer);
	else
		return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY negotiate_AcquireCredentialsHandleW(
    SEC_WCHAR* pszPrincipal, SEC_WCHAR* pszPackage, ULONG fCredentialUse, void* pvLogonID,
    void* pAuthData, SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument, PCredHandle phCredential,
    PTimeStamp ptsExpiry)
{
	BOOL kerberos, ntlm;
	MechCred* creds = calloc(MECH_COUNT, sizeof(MechCred));
	const SecPkg* pkg;

	if (!creds)
		return SEC_E_INTERNAL_ERROR;

	negotiate_get_config(&kerberos, &ntlm);

	for (int i = 0; i < MECH_COUNT; i++)
	{
		pkg = MechTable[i].pkg;
		creds[i].mech = &MechTable[i];

		if (!kerberos && _tcscmp(pkg->name, KERBEROS_SSP_NAME) == 0)
			continue;
		if (!ntlm && _tcscmp(SecPkgTable[i].name, NTLM_SSP_NAME) == 0)
			continue;

		if (pkg->table_w->AcquireCredentialsHandleW(
		        pszPrincipal, pszPackage, fCredentialUse, pvLogonID, pAuthData, pGetKeyFn,
		        pvGetKeyArgument, &creds[i].cred, ptsExpiry) != SEC_E_OK)
			continue;

		creds[i].valid = TRUE;
	}

	sspi_SecureHandleSetLowerPointer(phCredential, (void*)creds);
	sspi_SecureHandleSetUpperPointer(phCredential, (void*)NEGO_SSP_NAME);
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY negotiate_AcquireCredentialsHandleA(
    SEC_CHAR* pszPrincipal, SEC_CHAR* pszPackage, ULONG fCredentialUse, void* pvLogonID,
    void* pAuthData, SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument, PCredHandle phCredential,
    PTimeStamp ptsExpiry)
{
	BOOL kerberos, ntlm;
	MechCred* creds = calloc(MECH_COUNT, sizeof(MechCred));
	const SecPkg* pkg;

	if (!creds)
		return SEC_E_INTERNAL_ERROR;

	negotiate_get_config(&kerberos, &ntlm);

	for (int i = 0; i < MECH_COUNT; i++)
	{
		pkg = MechTable[i].pkg;
		creds[i].mech = &MechTable[i];

		if (!kerberos && _tcscmp(pkg->name, KERBEROS_SSP_NAME) == 0)
			continue;
		if (!ntlm && _tcscmp(SecPkgTable[i].name, NTLM_SSP_NAME) == 0)
			continue;

		if (pkg->table->AcquireCredentialsHandleA(pszPrincipal, pszPackage, fCredentialUse,
		                                          pvLogonID, pAuthData, pGetKeyFn, pvGetKeyArgument,
		                                          &creds[i].cred, ptsExpiry) != SEC_E_OK)
			continue;

		creds[i].valid = TRUE;
	}

	sspi_SecureHandleSetLowerPointer(phCredential, (void*)creds);
	sspi_SecureHandleSetUpperPointer(phCredential, (void*)NEGO_SSP_NAME);
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY negotiate_QueryCredentialsAttributesW(PCredHandle phCredential,
                                                                       ULONG ulAttribute,
                                                                       void* pBuffer)
{
	WLog_ERR(TAG, "[%s]: TODO: Implement", __FUNCTION__);
	return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY negotiate_QueryCredentialsAttributesA(PCredHandle phCredential,
                                                                       ULONG ulAttribute,
                                                                       void* pBuffer)
{
	WLog_ERR(TAG, "[%s]: TODO: Implement", __FUNCTION__);
	return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY negotiate_FreeCredentialsHandle(PCredHandle phCredential)
{
	MechCred* creds;

	creds = sspi_SecureHandleGetLowerPointer(phCredential);
	if (!creds)
		return SEC_E_INVALID_HANDLE;

	for (int i = 0; i < MECH_COUNT; i++)
		creds[i].mech->pkg->table->FreeCredentialsHandle(&creds[i].cred);

	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY negotiate_EncryptMessage(PCtxtHandle phContext, ULONG fQOP,
                                                          PSecBufferDesc pMessage,
                                                          ULONG MessageSeqNo)
{
	NEGOTIATE_CONTEXT* context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
		return SEC_E_INVALID_HANDLE;

	if (context->mic)
		MessageSeqNo++;

	if (context->mech->pkg->table->EncryptMessage)
		return context->mech->pkg->table->EncryptMessage(&context->sub_context, fQOP, pMessage,
		                                                 MessageSeqNo);
	else
		return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY negotiate_DecryptMessage(PCtxtHandle phContext,
                                                          PSecBufferDesc pMessage,
                                                          ULONG MessageSeqNo, ULONG* pfQOP)
{
	NEGOTIATE_CONTEXT* context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
		return SEC_E_INVALID_HANDLE;

	if (context->mic)
		MessageSeqNo++;

	if (context->mech->pkg->table->DecryptMessage)
		return context->mech->pkg->table->DecryptMessage(&context->sub_context, pMessage,
		                                                 MessageSeqNo, pfQOP);
	else
		return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY negotiate_MakeSignature(PCtxtHandle phContext, ULONG fQOP,
                                                         PSecBufferDesc pMessage,
                                                         ULONG MessageSeqNo)
{
	NEGOTIATE_CONTEXT* context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
		return SEC_E_INVALID_HANDLE;

	if (context->mic)
		MessageSeqNo++;

	if (context->mech->pkg->table->MakeSignature)
		return context->mech->pkg->table->MakeSignature(&context->sub_context, fQOP, pMessage,
		                                                MessageSeqNo);
	else
		return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY negotiate_VerifySignature(PCtxtHandle phContext,
                                                           PSecBufferDesc pMessage,
                                                           ULONG MessageSeqNo, ULONG* pfQOP)
{
	NEGOTIATE_CONTEXT* context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
		return SEC_E_INVALID_HANDLE;

	if (context->mic)
		MessageSeqNo++;

	if (context->mech->pkg->table->VerifySignature)
		return context->mech->pkg->table->VerifySignature(&context->sub_context, pMessage,
		                                                  MessageSeqNo, pfQOP);
	else
		return SEC_E_UNSUPPORTED_FUNCTION;
}

const SecurityFunctionTableA NEGOTIATE_SecurityFunctionTableA = {
	1,                                     /* dwVersion */
	NULL,                                  /* EnumerateSecurityPackages */
	negotiate_QueryCredentialsAttributesA, /* QueryCredentialsAttributes */
	negotiate_AcquireCredentialsHandleA,   /* AcquireCredentialsHandle */
	negotiate_FreeCredentialsHandle,       /* FreeCredentialsHandle */
	NULL,                                  /* Reserved2 */
	negotiate_InitializeSecurityContextA,  /* InitializeSecurityContext */
	negotiate_AcceptSecurityContext,       /* AcceptSecurityContext */
	negotiate_CompleteAuthToken,           /* CompleteAuthToken */
	negotiate_DeleteSecurityContext,       /* DeleteSecurityContext */
	NULL,                                  /* ApplyControlToken */
	negotiate_QueryContextAttributesA,     /* QueryContextAttributes */
	negotiate_ImpersonateSecurityContext,  /* ImpersonateSecurityContext */
	negotiate_RevertSecurityContext,       /* RevertSecurityContext */
	negotiate_MakeSignature,               /* MakeSignature */
	negotiate_VerifySignature,             /* VerifySignature */
	NULL,                                  /* FreeContextBuffer */
	NULL,                                  /* QuerySecurityPackageInfo */
	NULL,                                  /* Reserved3 */
	NULL,                                  /* Reserved4 */
	NULL,                                  /* ExportSecurityContext */
	NULL,                                  /* ImportSecurityContext */
	NULL,                                  /* AddCredentials */
	NULL,                                  /* Reserved8 */
	NULL,                                  /* QuerySecurityContextToken */
	negotiate_EncryptMessage,              /* EncryptMessage */
	negotiate_DecryptMessage,              /* DecryptMessage */
	negotiate_SetContextAttributesA,       /* SetContextAttributes */
};

const SecurityFunctionTableW NEGOTIATE_SecurityFunctionTableW = {
	1,                                     /* dwVersion */
	NULL,                                  /* EnumerateSecurityPackages */
	negotiate_QueryCredentialsAttributesW, /* QueryCredentialsAttributes */
	negotiate_AcquireCredentialsHandleW,   /* AcquireCredentialsHandle */
	negotiate_FreeCredentialsHandle,       /* FreeCredentialsHandle */
	NULL,                                  /* Reserved2 */
	negotiate_InitializeSecurityContextW,  /* InitializeSecurityContext */
	negotiate_AcceptSecurityContext,       /* AcceptSecurityContext */
	negotiate_CompleteAuthToken,           /* CompleteAuthToken */
	negotiate_DeleteSecurityContext,       /* DeleteSecurityContext */
	NULL,                                  /* ApplyControlToken */
	negotiate_QueryContextAttributesW,     /* QueryContextAttributes */
	negotiate_ImpersonateSecurityContext,  /* ImpersonateSecurityContext */
	negotiate_RevertSecurityContext,       /* RevertSecurityContext */
	negotiate_MakeSignature,               /* MakeSignature */
	negotiate_VerifySignature,             /* VerifySignature */
	NULL,                                  /* FreeContextBuffer */
	NULL,                                  /* QuerySecurityPackageInfo */
	NULL,                                  /* Reserved3 */
	NULL,                                  /* Reserved4 */
	NULL,                                  /* ExportSecurityContext */
	NULL,                                  /* ImportSecurityContext */
	NULL,                                  /* AddCredentials */
	NULL,                                  /* Reserved8 */
	NULL,                                  /* QuerySecurityContextToken */
	negotiate_EncryptMessage,              /* EncryptMessage */
	negotiate_DecryptMessage,              /* DecryptMessage */
	negotiate_SetContextAttributesW,       /* SetContextAttributes */
};
