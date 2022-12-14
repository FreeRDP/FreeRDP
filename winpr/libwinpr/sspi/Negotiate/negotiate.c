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
#include <winpr/assert.h>
#include <winpr/sspi.h>
#include <winpr/tchar.h>
#include <winpr/assert.h>
#include <winpr/registry.h>
#include <winpr/build-config.h>
#include <winpr/asn1.h>

#include "negotiate.h"

#include "../NTLM/ntlm.h"
#include "../NTLM/ntlm_export.h"
#include "../Kerberos/kerberos.h"
#include "../sspi.h"
#include "../../log.h"
#define TAG WINPR_TAG("negotiate")

static const char NEGO_REG_KEY[] =
    "Software\\" WINPR_VENDOR_STRING "\\" WINPR_PRODUCT_STRING "\\SSPI\\Negotiate";

typedef struct
{
	const TCHAR* name;
	const SecurityFunctionTableA* table;
	const SecurityFunctionTableW* table_w;
} SecPkg;

struct Mech_st
{
	const WinPrAsn1_OID* oid;
	const SecPkg* pkg;
	const UINT flags;
	const BOOL preferred;
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

static const WinPrAsn1_OID spnego_OID = { 6, (BYTE*)"\x2b\x06\x01\x05\x05\x02" };
static const WinPrAsn1_OID kerberos_u2u_OID = { 10,
	                                            (BYTE*)"\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x03" };
static const WinPrAsn1_OID kerberos_OID = { 9, (BYTE*)"\x2a\x86\x48\x86\xf7\x12\x01\x02\x02" };
static const WinPrAsn1_OID kerberos_wrong_OID = { 9,
	                                              (BYTE*)"\x2a\x86\x48\x82\xf7\x12\x01\x02\x02" };
static const WinPrAsn1_OID ntlm_OID = { 10, (BYTE*)"\x2b\x06\x01\x04\x01\x82\x37\x02\x02\x0a" };

#ifdef WITH_KRB5
static const SecPkg SecPkgTable[] = {
	{ KERBEROS_SSP_NAME, &KERBEROS_SecurityFunctionTableA, &KERBEROS_SecurityFunctionTableW },
	{ NTLM_SSP_NAME, &NTLM_SecurityFunctionTableA, &NTLM_SecurityFunctionTableW }
};

static const Mech MechTable[] = {
	{ &kerberos_u2u_OID, &SecPkgTable[0], ISC_REQ_INTEGRITY | ISC_REQ_USE_SESSION_KEY, TRUE },
	{ &kerberos_OID, &SecPkgTable[0], ISC_REQ_INTEGRITY, TRUE },
	{ &ntlm_OID, &SecPkgTable[1], 0, FALSE },
};
#else
static const SecPkg SecPkgTable[] = { { NTLM_SSP_NAME, &NTLM_SecurityFunctionTableA,
	                                    &NTLM_SecurityFunctionTableW } };

static const Mech MechTable[] = {
	{ &ntlm_OID, &SecPkgTable[0], 0, FALSE },
};
#endif

static const size_t MECH_COUNT = sizeof(MechTable) / sizeof(Mech);

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
	WinPrAsn1_OID supportedMech;
	SecBuffer mechTypes;
	SecBuffer mechToken;
	SecBuffer mic;
} NegToken;

static const NegToken empty_neg_token = { NOSTATE,        FALSE,          { 0, NULL },
	                                      { 0, 0, NULL }, { 0, 0, NULL }, { 0, 0, NULL } };

static NEGOTIATE_CONTEXT* negotiate_ContextNew(NEGOTIATE_CONTEXT* init_context)
{
	NEGOTIATE_CONTEXT* context = NULL;

	WINPR_ASSERT(init_context);

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

	*context = *init_context;

	return context;
}

static void negotiate_ContextFree(NEGOTIATE_CONTEXT* context)
{
	WINPR_ASSERT(context);

	if (context->mechTypes.pvBuffer)
		free(context->mechTypes.pvBuffer);
	free(context);
}

static const char* negotiate_mech_name(const WinPrAsn1_OID* oid)
{
	if (sspi_gss_oid_compare(oid, &spnego_OID))
		return "SPNEGO (1.3.6.1.5.5.2)";
	else if (sspi_gss_oid_compare(oid, &kerberos_u2u_OID))
		return "Kerberos user to user (1.2.840.113554.1.2.2.3)";
	else if (sspi_gss_oid_compare(oid, &kerberos_OID))
		return "Kerberos (1.2.840.113554.1.2.2)";
	else if (sspi_gss_oid_compare(oid, &kerberos_wrong_OID))
		return "Kerberos [wrong OID] (1.2.840.48018.1.2.2)";
	else if (sspi_gss_oid_compare(oid, &ntlm_OID))
		return "NTLM (1.3.6.1.4.1.311.2.2.10)";
	else
		return "Unknown mechanism";
}

static const Mech* negotiate_GetMechByOID(const WinPrAsn1_OID* oid)
{
	WINPR_ASSERT(oid);

	WinPrAsn1_OID testOid = *oid;

	if (sspi_gss_oid_compare(&testOid, &kerberos_wrong_OID))
	{
		testOid.len = kerberos_OID.len;
		testOid.data = kerberos_OID.data;
	}

	for (size_t i = 0; i < MECH_COUNT; i++)
	{
		if (sspi_gss_oid_compare(&testOid, MechTable[i].oid))
			return &MechTable[i];
	}
	return NULL;
}

static PSecHandle negotiate_FindCredential(MechCred* creds, const Mech* mech)
{
	WINPR_ASSERT(creds);

	if (!mech)
		return NULL;

	for (size_t i = 0; i < MECH_COUNT; i++)
	{
		MechCred* cred = &creds[i];

		if (cred->mech == mech)
		{
			if (cred->valid)
				return &cred->cred;
			return NULL;
		}
	}

	return NULL;
}

static BOOL negotiate_get_dword(HKEY hKey, const char* subkey, DWORD* pdwValue)
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

static BOOL negotiate_get_config_from_auth_package_list(void* pAuthData, BOOL* kerberos, BOOL* ntlm)
{
	char* tok_ctx = NULL;
	char* tok_ptr = NULL;
	char* PackageList = NULL;

	if (!sspi_CopyAuthPackageListA((const SEC_WINNT_AUTH_IDENTITY_INFO*)pAuthData, &PackageList))
		return FALSE;

	tok_ptr = strtok_s(PackageList, ",", &tok_ctx);

	while (tok_ptr)
	{
		char* PackageName = tok_ptr;
		BOOL PackageInclude = TRUE;

		if (PackageName[0] == '!')
		{
			PackageName = &PackageName[1];
			PackageInclude = FALSE;
		}

		if (!_stricmp(PackageName, "ntlm"))
		{
			*ntlm = PackageInclude;
		}
		else if (!_stricmp(PackageName, "kerberos"))
		{
			*kerberos = PackageInclude;
		}
		else
		{
			WLog_WARN(TAG, "Unknown authentication package name: %s", PackageName);
		}

		tok_ptr = strtok_s(NULL, ",", &tok_ctx);
	}

	free(PackageList);
	return TRUE;
}

static BOOL negotiate_get_config(void* pAuthData, BOOL* kerberos, BOOL* ntlm)
{
	HKEY hKey = NULL;
	LONG rc;

	WINPR_ASSERT(kerberos);
	WINPR_ASSERT(ntlm);

#if !defined(WITH_KRB5_NO_NTLM_FALLBACK)
	*ntlm = TRUE;
#else
	*ntlm = FALSE;
#endif
	*kerberos = TRUE;

	if (negotiate_get_config_from_auth_package_list(pAuthData, kerberos, ntlm))
	{
		return TRUE; // use explicit authentication package list
	}

	rc = RegOpenKeyExA(HKEY_LOCAL_MACHINE, NEGO_REG_KEY, 0, KEY_READ | KEY_WOW64_64KEY, &hKey);
	if (rc == ERROR_SUCCESS)
	{
		DWORD dwValue;

		if (negotiate_get_dword(hKey, "kerberos", &dwValue))
			*kerberos = (dwValue != 0) ? TRUE : FALSE;

#if !defined(WITH_KRB5_NO_NTLM_FALLBACK)
		if (negotiate_get_dword(hKey, "ntlm", &dwValue))
			*ntlm = (dwValue != 0) ? TRUE : FALSE;
#endif

		RegCloseKey(hKey);
	}

	return TRUE;
}

static BOOL negotiate_write_neg_token(PSecBuffer output_buffer, NegToken* token)
{
	WINPR_ASSERT(output_buffer);
	WINPR_ASSERT(token);

	BOOL ret = FALSE;
	WinPrAsn1Encoder* enc = NULL;
	WinPrAsn1_MemoryChunk mechTypes = { token->mechTypes.cbBuffer, token->mechTypes.pvBuffer };
	WinPrAsn1_OctetString mechToken = { token->mechToken.cbBuffer, token->mechToken.pvBuffer };
	WinPrAsn1_OctetString mechListMic = { token->mic.cbBuffer, token->mic.pvBuffer };
	wStream s;
	size_t len;

	enc = WinPrAsn1Encoder_New(WINPR_ASN1_DER);
	if (!enc)
		return FALSE;

	/* For NegTokenInit wrap in an initialContextToken */
	if (token->init)
	{
		/* InitialContextToken [APPLICATION 0] IMPLICIT SEQUENCE */
		if (!WinPrAsn1EncAppContainer(enc, 0))
			goto cleanup;

		/* thisMech MechType OID */
		if (!WinPrAsn1EncOID(enc, &spnego_OID))
			goto cleanup;
	}

	/* innerContextToken [0] NegTokenInit or [1] NegTokenResp */
	if (!WinPrAsn1EncContextualSeqContainer(enc, token->init ? 0 : 1))
		goto cleanup;

	WLog_DBG(TAG, token->init ? "Writing negTokenInit..." : "Writing negTokenResp...");

	/* mechTypes [0] MechTypeList (mechTypes already contains the SEQUENCE tag) */
	if (token->init)
	{
		if (!WinPrAsn1EncContextualRawContent(enc, 0, &mechTypes))
			goto cleanup;
		WLog_DBG(TAG, "\tmechTypes [0] (%li bytes)", token->mechTypes.cbBuffer);
	}
	/* negState [0] ENUMERATED */
	else if (token->negState != NOSTATE)
	{
		if (!WinPrAsn1EncContextualEnumerated(enc, 0, token->negState))
			goto cleanup;
		WLog_DBG(TAG, "\tnegState [0] (%d)", token->negState);
	}

	/* supportedMech [1] OID */
	if (token->supportedMech.len)
	{
		if (!WinPrAsn1EncContextualOID(enc, 1, &token->supportedMech))
			goto cleanup;
		WLog_DBG(TAG, "\tsupportedMech [1] (%s)", negotiate_mech_name(&token->supportedMech));
	}

	/* mechToken [2] OCTET STRING */
	if (token->mechToken.cbBuffer)
	{
		if (WinPrAsn1EncContextualOctetString(enc, 2, &mechToken) == 0)
			goto cleanup;
		WLog_DBG(TAG, "\tmechToken [2] (%li bytes)", token->mechToken.cbBuffer);
	}

	/* mechListMIC [3] OCTET STRING */
	if (token->mic.cbBuffer)
	{
		if (WinPrAsn1EncContextualOctetString(enc, 3, &mechListMic) == 0)
			goto cleanup;
		WLog_DBG(TAG, "\tmechListMIC [3] (%li bytes)", token->mic.cbBuffer);
	}

	/* NegTokenInit or NegTokenResp */
	if (!WinPrAsn1EncEndContainer(enc))
		goto cleanup;

	if (token->init)
	{
		/* initialContextToken */
		if (!WinPrAsn1EncEndContainer(enc))
			goto cleanup;
	}

	if (!WinPrAsn1EncStreamSize(enc, &len) || len > output_buffer->cbBuffer)
		goto cleanup;

	Stream_StaticInit(&s, output_buffer->pvBuffer, len);

	if (WinPrAsn1EncToStream(enc, &s))
	{
		output_buffer->cbBuffer = len;
		ret = TRUE;
	}

cleanup:
	WinPrAsn1Encoder_Free(&enc);
	return ret;
}

static BOOL negotiate_read_neg_token(PSecBuffer input, NegToken* token)
{
	WinPrAsn1Decoder dec;
	WinPrAsn1Decoder dec2;
	WinPrAsn1_OID oid;
	WinPrAsn1_tagId contextual;
	WinPrAsn1_tag tag;
	size_t len;
	WinPrAsn1_OctetString octet_string;
	BOOL err;

	WINPR_ASSERT(input);
	WINPR_ASSERT(token);

	WinPrAsn1Decoder_InitMem(&dec, WINPR_ASN1_DER, input->pvBuffer, input->cbBuffer);

	if (!WinPrAsn1DecPeekTag(&dec, &tag))
		return FALSE;

	if (tag == 0x60)
	{
		/* initialContextToken [APPLICATION 0] */
		if (!WinPrAsn1DecReadApp(&dec, &tag, &dec2) || tag != 0)
			return FALSE;
		dec = dec2;

		/* thisMech OID */
		if (!WinPrAsn1DecReadOID(&dec, &oid, FALSE))
			return FALSE;

		if (!sspi_gss_oid_compare(&spnego_OID, &oid))
			return FALSE;

		/* [0] NegTokenInit */
		if (!WinPrAsn1DecReadContextualSequence(&dec, 0, &err, &dec2))
			return FALSE;

		token->init = TRUE;
	}
	/* [1] NegTokenResp */
	else if (!WinPrAsn1DecReadContextualSequence(&dec, 1, &err, &dec2))
		return FALSE;
	dec = dec2;

	WLog_DBG(TAG, token->init ? "Reading negTokenInit..." : "Reading negTokenResp...");

	/* Read NegTokenResp sequence members */
	do
	{
		if (!WinPrAsn1DecReadContextualTag(&dec, &contextual, &dec2))
			return FALSE;

		switch (contextual)
		{
			case 0:
				if (token->init)
				{
					/* mechTypes [0] MechTypeList */
					wStream s = WinPrAsn1DecGetStream(&dec2);
					token->mechTypes.BufferType = SECBUFFER_TOKEN;
					token->mechTypes.cbBuffer = Stream_Length(&s);
					token->mechTypes.pvBuffer = Stream_Buffer(&s);
					WLog_DBG(TAG, "\tmechTypes [0] (%li bytes)", token->mechTypes.cbBuffer);
				}
				else
				{
					/* negState [0] ENUMERATED */
					WinPrAsn1_ENUMERATED rd;
					if (!WinPrAsn1DecReadEnumerated(&dec2, &rd))
						return FALSE;
					token->negState = rd;
					WLog_DBG(TAG, "\tnegState [0] (%d)", token->negState);
				}
				break;
			case 1:
				if (token->init)
				{
					/* reqFlags [1] ContextFlags BIT STRING (ignored) */
					if (!WinPrAsn1DecPeekTagAndLen(&dec2, &tag, &len) || (tag != ER_TAG_BIT_STRING))
						return FALSE;
					WLog_DBG(TAG, "\treqFlags [1] (%li bytes)", len);
				}
				else
				{
					/* supportedMech [1] MechType */
					if (!WinPrAsn1DecReadOID(&dec2, &token->supportedMech, FALSE))
						return FALSE;
					WLog_DBG(TAG, "\tsupportedMech [1] (%s)",
					         negotiate_mech_name(&token->supportedMech));
				}
				break;
			case 2:
				/* mechToken [2] OCTET STRING */
				if (!WinPrAsn1DecReadOctetString(&dec2, &octet_string, FALSE))
					return FALSE;
				token->mechToken.cbBuffer = octet_string.len;
				token->mechToken.pvBuffer = octet_string.data;
				token->mechToken.BufferType = SECBUFFER_TOKEN;
				WLog_DBG(TAG, "\tmechToken [2] (%li bytes)", octet_string.len);
				break;
			case 3:
				/* mechListMic [3] OCTET STRING */
				if (!WinPrAsn1DecReadOctetString(&dec2, &octet_string, FALSE))
					return FALSE;
				token->mic.cbBuffer = octet_string.len;
				token->mic.pvBuffer = octet_string.data;
				token->mic.BufferType = SECBUFFER_TOKEN;
				WLog_DBG(TAG, "\tmechListMIC [3] (%li bytes)", octet_string.len);
				break;
			default:
				WLog_ERR(TAG, "unknown contextual item %d", contextual);
				return FALSE;
		}
	} while (WinPrAsn1DecPeekTag(&dec, &tag));

	return TRUE;
}

static SECURITY_STATUS negotiate_mic_exchange(NEGOTIATE_CONTEXT* context, NegToken* input_token,
                                              NegToken* output_token, PSecBuffer output_buffer)
{
	SecBuffer mic_buffers[2] = { 0 };
	SecBufferDesc mic_buffer_desc = { SECBUFFER_VERSION, 2, mic_buffers };
	SECURITY_STATUS status;

	WINPR_ASSERT(context);
	WINPR_ASSERT(input_token);
	WINPR_ASSERT(output_token);
	WINPR_ASSERT(context->mech);
	WINPR_ASSERT(context->mech->pkg);

	const SecurityFunctionTableA* table = context->mech->pkg->table;
	WINPR_ASSERT(table);

	mic_buffers[0] = context->mechTypes;

	/* Verify MIC if we received one */
	if (input_token->mic.cbBuffer > 0)
	{
		mic_buffers[1] = input_token->mic;

		status = table->VerifySignature(&context->sub_context, &mic_buffer_desc, 0, 0);
		if (status != SEC_E_OK)
			return status;

		output_token->negState = ACCEPT_COMPLETED;
	}

	/* If peer expects a MIC then generate it */
	if (input_token->negState != ACCEPT_COMPLETED)
	{
		/* Store the mic token after the mech token in the output buffer */
		output_token->mic.BufferType = SECBUFFER_TOKEN;
		output_token->mic.cbBuffer = output_buffer->cbBuffer - output_token->mechToken.cbBuffer;
		output_token->mic.pvBuffer =
		    (BYTE*)output_buffer->pvBuffer + output_token->mechToken.cbBuffer;

		mic_buffers[1] = output_token->mic;

		status = table->MakeSignature(&context->sub_context, 0, &mic_buffer_desc, 0);
		if (status != SEC_E_OK)
			return status;

		output_token->mic = mic_buffers[1];
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
	NegToken input_token = empty_neg_token;
	NegToken output_token = empty_neg_token;
	PSecBuffer input_buffer = NULL;
	PSecBuffer output_buffer = NULL;
	PSecBuffer bindings_buffer = NULL;
	SecBuffer mech_input_buffers[2] = { 0 };
	SecBufferDesc mech_input = { SECBUFFER_VERSION, 2, mech_input_buffers };
	SecBufferDesc mech_output = { SECBUFFER_VERSION, 1, &output_token.mechToken };
	SECURITY_STATUS status = SEC_E_INTERNAL_ERROR;
	SECURITY_STATUS sub_status = SEC_E_INTERNAL_ERROR;
	WinPrAsn1Encoder* enc = NULL;
	wStream s;
	const Mech* mech;

	if (!phCredential || !SecIsValidHandle(phCredential))
		return SEC_E_NO_CREDENTIALS;

	creds = sspi_SecureHandleGetLowerPointer(phCredential);

	/* behave like windows SSPIs that don't want empty context */
	if (phContext && !phContext->dwLower && !phContext->dwUpper)
		return SEC_E_INVALID_HANDLE;

	context = sspi_SecureHandleGetLowerPointer(phContext);

	if (pInput)
	{
		input_buffer = sspi_FindSecBuffer(pInput, SECBUFFER_TOKEN);
		bindings_buffer = sspi_FindSecBuffer(pInput, SECBUFFER_CHANNEL_BINDINGS);
	}
	if (pOutput)
		output_buffer = sspi_FindSecBuffer(pOutput, SECBUFFER_TOKEN);

	if (!context)
	{
		enc = WinPrAsn1Encoder_New(WINPR_ASN1_DER);
		if (!enc)
			return SEC_E_INSUFFICIENT_MEMORY;

		if (!WinPrAsn1EncSeqContainer(enc))
			goto cleanup;

		for (size_t i = 0; i < MECH_COUNT; i++)
		{
			MechCred* cred = &creds[i];

			if (!cred->valid)
				continue;

			/* Send an optimistic token for the first valid mechanism */
			if (!init_context.mech)
			{
				/* Use the output buffer to store the optimistic token */
				CopyMemory(&output_token.mechToken, output_buffer, sizeof(SecBuffer));

				if (bindings_buffer)
					mech_input_buffers[0] = *bindings_buffer;

				sub_status = MechTable[i].pkg->table_w->InitializeSecurityContextW(
				    &cred->cred, NULL, pszTargetName, fContextReq | cred->mech->flags, Reserved1,
				    TargetDataRep, &mech_input, Reserved2, &init_context.sub_context, &mech_output,
				    pfContextAttr, ptsExpiry);

				/* If the mechanism failed we can't use it; skip */
				if (IsSecurityStatusError(sub_status))
				{
					cred->valid = FALSE;
					continue;
				}

				init_context.mech = cred->mech;
			}

			if (!WinPrAsn1EncOID(enc, cred->mech->oid))
				goto cleanup;
			WLog_DBG(TAG, "Available mechanism: %s", negotiate_mech_name(cred->mech->oid));
		}

		/* No usable mechanisms were found */
		if (!init_context.mech)
			goto cleanup;

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
			init_context.mechTypes.cbBuffer = WinPrAsn1EncEndContainer(enc);
		}

		/* Allocate memory for the new context */
		context = negotiate_ContextNew(&init_context);
		if (!context)
		{
			init_context.mech->pkg->table->DeleteSecurityContext(&init_context.sub_context);
			WinPrAsn1Encoder_Free(&enc);
			return SEC_E_INSUFFICIENT_MEMORY;
		}

		sspi_SecureHandleSetUpperPointer(phNewContext, NEGO_SSP_NAME);
		sspi_SecureHandleSetLowerPointer(phNewContext, context);

		if (!context->spnego)
		{
			status = sub_status;
			goto cleanup;
		}

		/* Write mechTypesList */
		Stream_StaticInit(&s, context->mechTypes.pvBuffer, context->mechTypes.cbBuffer);
		if (!WinPrAsn1EncToStream(enc, &s))
			goto cleanup;

		output_token.mechTypes.cbBuffer = context->mechTypes.cbBuffer;
		output_token.mechTypes.pvBuffer = context->mechTypes.pvBuffer;
		output_token.init = TRUE;

		if (sub_status == SEC_E_OK)
			context->state = NEGOTIATE_STATE_FINAL_OPTIMISTIC;
	}
	else
	{
		if (!input_buffer)
			return SEC_E_INVALID_TOKEN;

		sub_context = &context->sub_context;
		sub_cred = negotiate_FindCredential(creds, context->mech);

		if (!context->spnego)
		{
			return context->mech->pkg->table_w->InitializeSecurityContextW(
			    sub_cred, sub_context, pszTargetName, fContextReq | context->mech->flags, Reserved1,
			    TargetDataRep, pInput, Reserved2, sub_context, pOutput, pfContextAttr, ptsExpiry);
		}

		if (!negotiate_read_neg_token(input_buffer, &input_token))
			return SEC_E_INVALID_TOKEN;

		/* On first response check if the server doesn't like out prefered mech */
		if (context->state < NEGOTIATE_STATE_NEGORESP && input_token.supportedMech.len &&
		    !sspi_gss_oid_compare(&input_token.supportedMech, context->mech->oid))
		{
			mech = negotiate_GetMechByOID(&input_token.supportedMech);
			if (!mech)
				return SEC_E_INVALID_TOKEN;

			/* Make sure the specified mech is supported and get the appropriate credential */
			sub_cred = negotiate_FindCredential(creds, mech);
			if (!sub_cred)
				return SEC_E_INVALID_TOKEN;

			/* Clean up the optimistic mech */
			context->mech->pkg->table_w->DeleteSecurityContext(&context->sub_context);
			sub_context = NULL;

			context->mech = mech;
			context->mic = TRUE;
		}

		/* Check neg_state (required on first response) */
		if (context->state < NEGOTIATE_STATE_NEGORESP)
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
					context->state = NEGOTIATE_STATE_NEGORESP;
					break;
				case ACCEPT_COMPLETED:
					if (context->state == NEGOTIATE_STATE_INITIAL)
						context->state = NEGOTIATE_STATE_NEGORESP;
					else
						context->state = NEGOTIATE_STATE_FINAL;
					break;
			}

			WLog_DBG(TAG, "Negotiated mechanism: %s", negotiate_mech_name(context->mech->oid));
		}

		if (context->state == NEGOTIATE_STATE_NEGORESP)
		{
			/* Store the mech token in the output buffer */
			CopyMemory(&output_token.mechToken, output_buffer, sizeof(SecBuffer));

			mech_input_buffers[0] = input_token.mechToken;
			if (bindings_buffer)
				mech_input_buffers[1] = *bindings_buffer;

			status = context->mech->pkg->table_w->InitializeSecurityContextW(
			    sub_cred, sub_context, pszTargetName, fContextReq | context->mech->flags, Reserved1,
			    TargetDataRep, input_token.mechToken.cbBuffer ? &mech_input : NULL, Reserved2,
			    &context->sub_context, &mech_output, pfContextAttr, ptsExpiry);

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

			output_buffer->cbBuffer = 0;
			return SEC_E_OK;
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

	if (!negotiate_write_neg_token(output_buffer, &output_token))
		status = SEC_E_INTERNAL_ERROR;

cleanup:
	WinPrAsn1Encoder_Free(&enc);
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
		pszTargetNameW = ConvertUtf8ToWCharAlloc(pszTargetName, NULL);
		if (!pszTargetNameW)
			return SEC_E_INTERNAL_ERROR;
	}

	status = negotiate_InitializeSecurityContextW(
	    phCredential, phContext, pszTargetNameW, fContextReq, Reserved1, TargetDataRep, pInput,
	    Reserved2, phNewContext, pOutput, pfContextAttr, ptsExpiry);
	free(pszTargetNameW);
	return status;
}

static const Mech* guessMech(PSecBuffer input_buffer, BOOL* spNego, WinPrAsn1_OID* oid)
{
	WinPrAsn1Decoder decoder;
	WinPrAsn1Decoder appDecoder;
	WinPrAsn1_tagId tag;

	*spNego = FALSE;

	/* Check for NTLM token */
	if (input_buffer->cbBuffer >= 8 && strncmp(input_buffer->pvBuffer, "NTLMSSP", 8) == 0)
	{
		*oid = ntlm_OID;
		return negotiate_GetMechByOID(&ntlm_OID);
	}

	/* Read initialContextToken or raw Kerberos token */
	WinPrAsn1Decoder_InitMem(&decoder, WINPR_ASN1_DER, input_buffer->pvBuffer,
	                         input_buffer->cbBuffer);

	if (!WinPrAsn1DecReadApp(&decoder, &tag, &appDecoder) || tag != 0)
		return NULL;

	if (!WinPrAsn1DecReadOID(&appDecoder, oid, FALSE))
		return NULL;

	if (sspi_gss_oid_compare(oid, &spnego_OID))
	{
		*spNego = TRUE;
		return NULL;
	}

	return negotiate_GetMechByOID(oid);
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
	NegToken input_token = empty_neg_token;
	NegToken output_token = empty_neg_token;
	PSecBuffer input_buffer = NULL;
	PSecBuffer output_buffer = NULL;
	SecBufferDesc mech_input = { SECBUFFER_VERSION, 1, &input_token.mechToken };
	SecBufferDesc mech_output = { SECBUFFER_VERSION, 1, &output_token.mechToken };
	SECURITY_STATUS status = SEC_E_INTERNAL_ERROR;
	WinPrAsn1Decoder dec, dec2;
	WinPrAsn1_tagId tag;
	WinPrAsn1_OID oid = { 0 };
	const Mech* first_mech = NULL;

	if (!phCredential || !SecIsValidHandle(phCredential))
		return SEC_E_NO_CREDENTIALS;

	creds = sspi_SecureHandleGetLowerPointer(phCredential);

	if (!pInput)
		return SEC_E_INVALID_TOKEN;

	/* behave like windows SSPIs that don't want empty context */
	if (phContext && !phContext->dwLower && !phContext->dwUpper)
		return SEC_E_INVALID_HANDLE;

	context = sspi_SecureHandleGetLowerPointer(phContext);

	input_buffer = sspi_FindSecBuffer(pInput, SECBUFFER_TOKEN);
	if (pOutput)
		output_buffer = sspi_FindSecBuffer(pOutput, SECBUFFER_TOKEN);

	if (!context)
	{
		init_context.mech = guessMech(input_buffer, &init_context.spnego, &oid);
		if (!init_context.mech && !init_context.spnego)
			return SEC_E_INVALID_TOKEN;

		WLog_DBG(TAG, "Mechanism: %s", negotiate_mech_name(&oid));

		if (init_context.spnego)
		{
			/* Process spnego token */
			if (!negotiate_read_neg_token(input_buffer, &input_token))
				return SEC_E_INVALID_TOKEN;

			/* First token must be negoTokenInit and must contain a mechList */
			if (!input_token.init || input_token.mechTypes.cbBuffer == 0)
				return SEC_E_INVALID_TOKEN;

			init_context.mechTypes.BufferType = SECBUFFER_DATA;
			init_context.mechTypes.cbBuffer = input_token.mechTypes.cbBuffer;

			/* Prepare to read mechList */
			WinPrAsn1Decoder_InitMem(&dec, WINPR_ASN1_DER, input_token.mechTypes.pvBuffer,
			                         input_token.mechTypes.cbBuffer);

			if (!WinPrAsn1DecReadSequence(&dec, &dec2))
				return SEC_E_INVALID_TOKEN;
			dec = dec2;

			/* If an optimistic token was provided pass it into the first mech */
			if (input_token.mechToken.cbBuffer)
			{
				if (!WinPrAsn1DecReadOID(&dec, &oid, FALSE))
					return SEC_E_INVALID_TOKEN;

				init_context.mech = negotiate_GetMechByOID(&oid);

				if (init_context.mech)
				{
					output_token.mechToken = *output_buffer;
					WLog_DBG(TAG, "Requested mechanism: %s",
					         negotiate_mech_name(init_context.mech->oid));
				}
			}
		}

		if (init_context.mech)
		{
			sub_cred = negotiate_FindCredential(creds, init_context.mech);

			status = init_context.mech->pkg->table->AcceptSecurityContext(
			    sub_cred, NULL, init_context.spnego ? &mech_input : pInput, fContextReq,
			    TargetDataRep, &init_context.sub_context,
			    init_context.spnego ? &mech_output : pOutput, pfContextAttr, ptsTimeStamp);
		}

		if (IsSecurityStatusError(status))
		{
			if (!init_context.spnego)
				return status;

			init_context.mic = TRUE;
			first_mech = init_context.mech;
			init_context.mech = NULL;
			output_token.mechToken.cbBuffer = 0;
		}

		while (!init_context.mech && WinPrAsn1DecPeekTag(&dec, &tag))
		{
			/* Read each mechanism */
			if (!WinPrAsn1DecReadOID(&dec, &oid, FALSE))
				return SEC_E_INVALID_TOKEN;

			init_context.mech = negotiate_GetMechByOID(&oid);
			WLog_DBG(TAG, "Requested mechanism: %s", negotiate_mech_name(init_context.mech->oid));

			/* Microsoft may send two versions of the kerberos OID */
			if (init_context.mech == first_mech)
				init_context.mech = NULL;

			if (init_context.mech && !negotiate_FindCredential(creds, init_context.mech))
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

		if (!context->mech->preferred)
		{
			output_token.negState = REQUEST_MIC;
			context->mic = TRUE;
		}
		else
		{
			output_token.negState = ACCEPT_INCOMPLETE;
		}

		if (status == SEC_E_OK)
			context->state = NEGOTIATE_STATE_FINAL;
		else
			context->state = NEGOTIATE_STATE_NEGORESP;

		output_token.supportedMech = oid;
		WLog_DBG(TAG, "Accepted mechanism: %s", negotiate_mech_name(&output_token.supportedMech));
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
			    sub_cred, &context->sub_context, &mech_input, fContextReq | context->mech->flags,
			    TargetDataRep, &context->sub_context, &mech_output, pfContextAttr, ptsTimeStamp);

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

	if (!negotiate_write_neg_token(output_buffer, &output_token))
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

	WINPR_ASSERT(context->mech);
	WINPR_ASSERT(context->mech->pkg);
	WINPR_ASSERT(context->mech->pkg->table);
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

	WINPR_ASSERT(context->mech);
	WINPR_ASSERT(context->mech->pkg);
	WINPR_ASSERT(context->mech->pkg->table);
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

	WINPR_ASSERT(context->mech);
	WINPR_ASSERT(context->mech->pkg);
	WINPR_ASSERT(context->mech->pkg->table_w);
	if (context->mech->pkg->table_w->QueryContextAttributesW)
		return context->mech->pkg->table_w->QueryContextAttributesW(&context->sub_context,
		                                                            ulAttribute, pBuffer);

	return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY negotiate_QueryContextAttributesA(PCtxtHandle phContext,
                                                                   ULONG ulAttribute, void* pBuffer)
{
	NEGOTIATE_CONTEXT* context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
		return SEC_E_INVALID_HANDLE;

	WINPR_ASSERT(context->mech);
	WINPR_ASSERT(context->mech->pkg);
	WINPR_ASSERT(context->mech->pkg->table);
	if (context->mech->pkg->table->QueryContextAttributesA)
		return context->mech->pkg->table->QueryContextAttributesA(&context->sub_context,
		                                                          ulAttribute, pBuffer);

	return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY negotiate_SetContextAttributesW(PCtxtHandle phContext,
                                                                 ULONG ulAttribute, void* pBuffer,
                                                                 ULONG cbBuffer)
{
	NEGOTIATE_CONTEXT* context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
		return SEC_E_INVALID_HANDLE;

	WINPR_ASSERT(context->mech);
	WINPR_ASSERT(context->mech->pkg);
	WINPR_ASSERT(context->mech->pkg->table_w);
	if (context->mech->pkg->table_w->SetContextAttributesW)
		return context->mech->pkg->table_w->SetContextAttributesW(&context->sub_context,
		                                                          ulAttribute, pBuffer, cbBuffer);

	return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY negotiate_SetContextAttributesA(PCtxtHandle phContext,
                                                                 ULONG ulAttribute, void* pBuffer,
                                                                 ULONG cbBuffer)
{
	NEGOTIATE_CONTEXT* context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
		return SEC_E_INVALID_HANDLE;

	WINPR_ASSERT(context->mech);
	WINPR_ASSERT(context->mech->pkg);
	WINPR_ASSERT(context->mech->pkg->table);
	if (context->mech->pkg->table->SetContextAttributesA)
		return context->mech->pkg->table->SetContextAttributesA(&context->sub_context, ulAttribute,
		                                                        pBuffer, cbBuffer);

	return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY negotiate_SetCredentialsAttributesW(PCredHandle phCredential,
                                                                     ULONG ulAttribute,
                                                                     void* pBuffer, ULONG cbBuffer)
{
	MechCred* creds;
	BOOL success = FALSE;
	SECURITY_STATUS secStatus;

	creds = sspi_SecureHandleGetLowerPointer(phCredential);

	if (!creds)
		return SEC_E_INVALID_HANDLE;

	for (size_t i = 0; i < MECH_COUNT; i++)
	{
		MechCred* cred = &creds[i];

		WINPR_ASSERT(cred->mech);
		WINPR_ASSERT(cred->mech->pkg);
		WINPR_ASSERT(cred->mech->pkg->table);
		WINPR_ASSERT(cred->mech->pkg->table_w->SetCredentialsAttributesW);
		secStatus = cred->mech->pkg->table_w->SetCredentialsAttributesW(&cred->cred, ulAttribute,
		                                                                pBuffer, cbBuffer);

		if (secStatus == SEC_E_OK)
		{
			success = TRUE;
		}
	}

	// return success if at least one submodule accepts the credential attribute
	return (success ? SEC_E_OK : SEC_E_UNSUPPORTED_FUNCTION);
}

static SECURITY_STATUS SEC_ENTRY negotiate_SetCredentialsAttributesA(PCredHandle phCredential,
                                                                     ULONG ulAttribute,
                                                                     void* pBuffer, ULONG cbBuffer)
{
	MechCred* creds;
	BOOL success = FALSE;
	SECURITY_STATUS secStatus;

	creds = sspi_SecureHandleGetLowerPointer(phCredential);

	if (!creds)
		return SEC_E_INVALID_HANDLE;

	for (size_t i = 0; i < MECH_COUNT; i++)
	{
		MechCred* cred = &creds[i];

		if (!cred->valid)
			continue;

		WINPR_ASSERT(cred->mech);
		WINPR_ASSERT(cred->mech->pkg);
		WINPR_ASSERT(cred->mech->pkg->table);
		WINPR_ASSERT(cred->mech->pkg->table->SetCredentialsAttributesA);
		secStatus = cred->mech->pkg->table->SetCredentialsAttributesA(&cred->cred, ulAttribute,
		                                                              pBuffer, cbBuffer);

		if (secStatus == SEC_E_OK)
		{
			success = TRUE;
		}
	}

	// return success if at least one submodule accepts the credential attribute
	return (success ? SEC_E_OK : SEC_E_UNSUPPORTED_FUNCTION);
}

static SECURITY_STATUS SEC_ENTRY negotiate_AcquireCredentialsHandleW(
    SEC_WCHAR* pszPrincipal, SEC_WCHAR* pszPackage, ULONG fCredentialUse, void* pvLogonID,
    void* pAuthData, SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument, PCredHandle phCredential,
    PTimeStamp ptsExpiry)
{
	BOOL kerberos, ntlm;

	if (!negotiate_get_config(pAuthData, &kerberos, &ntlm))
		return SEC_E_INTERNAL_ERROR;

	MechCred* creds = calloc(MECH_COUNT, sizeof(MechCred));

	if (!creds)
		return SEC_E_INTERNAL_ERROR;

	for (size_t i = 0; i < MECH_COUNT; i++)
	{
		MechCred* cred = &creds[i];
		const SecPkg* pkg = MechTable[i].pkg;
		cred->mech = &MechTable[i];

		if (!kerberos && _tcscmp(pkg->name, KERBEROS_SSP_NAME) == 0)
			continue;
		if (!ntlm && _tcscmp(SecPkgTable[i].name, NTLM_SSP_NAME) == 0)
			continue;

		WINPR_ASSERT(pkg->table_w);
		WINPR_ASSERT(pkg->table_w->AcquireCredentialsHandleW);
		if (pkg->table_w->AcquireCredentialsHandleW(
		        pszPrincipal, pszPackage, fCredentialUse, pvLogonID, pAuthData, pGetKeyFn,
		        pvGetKeyArgument, &cred->cred, ptsExpiry) != SEC_E_OK)
			continue;

		cred->valid = TRUE;
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

	if (!negotiate_get_config(pAuthData, &kerberos, &ntlm))
		return SEC_E_INTERNAL_ERROR;

	MechCred* creds = calloc(MECH_COUNT, sizeof(MechCred));

	if (!creds)
		return SEC_E_INTERNAL_ERROR;

	for (size_t i = 0; i < MECH_COUNT; i++)
	{
		const SecPkg* pkg = MechTable[i].pkg;
		MechCred* cred = &creds[i];

		cred->mech = &MechTable[i];

		if (!kerberos && _tcscmp(pkg->name, KERBEROS_SSP_NAME) == 0)
			continue;
		if (!ntlm && _tcscmp(SecPkgTable[i].name, NTLM_SSP_NAME) == 0)
			continue;

		WINPR_ASSERT(pkg->table);
		WINPR_ASSERT(pkg->table->AcquireCredentialsHandleA);
		if (pkg->table->AcquireCredentialsHandleA(pszPrincipal, pszPackage, fCredentialUse,
		                                          pvLogonID, pAuthData, pGetKeyFn, pvGetKeyArgument,
		                                          &cred->cred, ptsExpiry) != SEC_E_OK)
			continue;

		cred->valid = TRUE;
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

	for (size_t i = 0; i < MECH_COUNT; i++)
	{
		MechCred* cred = &creds[i];

		WINPR_ASSERT(cred->mech);
		WINPR_ASSERT(cred->mech->pkg);
		WINPR_ASSERT(cred->mech->pkg->table);
		WINPR_ASSERT(cred->mech->pkg->table->FreeCredentialsHandle);
		cred->mech->pkg->table->FreeCredentialsHandle(&cred->cred);
	}
	free(creds);

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

	WINPR_ASSERT(context->mech);
	WINPR_ASSERT(context->mech->pkg);
	WINPR_ASSERT(context->mech->pkg->table);
	if (context->mech->pkg->table->EncryptMessage)
		return context->mech->pkg->table->EncryptMessage(&context->sub_context, fQOP, pMessage,
		                                                 MessageSeqNo);

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

	WINPR_ASSERT(context->mech);
	WINPR_ASSERT(context->mech->pkg);
	WINPR_ASSERT(context->mech->pkg->table);
	if (context->mech->pkg->table->DecryptMessage)
		return context->mech->pkg->table->DecryptMessage(&context->sub_context, pMessage,
		                                                 MessageSeqNo, pfQOP);

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

	WINPR_ASSERT(context->mech);
	WINPR_ASSERT(context->mech->pkg);
	WINPR_ASSERT(context->mech->pkg->table);
	if (context->mech->pkg->table->MakeSignature)
		return context->mech->pkg->table->MakeSignature(&context->sub_context, fQOP, pMessage,
		                                                MessageSeqNo);

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

	WINPR_ASSERT(context->mech);
	WINPR_ASSERT(context->mech->pkg);
	WINPR_ASSERT(context->mech->pkg->table);
	if (context->mech->pkg->table->VerifySignature)
		return context->mech->pkg->table->VerifySignature(&context->sub_context, pMessage,
		                                                  MessageSeqNo, pfQOP);

	return SEC_E_UNSUPPORTED_FUNCTION;
}

const SecurityFunctionTableA NEGOTIATE_SecurityFunctionTableA = {
	3,                                     /* dwVersion */
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
	negotiate_SetCredentialsAttributesA,   /* SetCredentialsAttributes */
};

const SecurityFunctionTableW NEGOTIATE_SecurityFunctionTableW = {
	3,                                     /* dwVersion */
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
	negotiate_SetCredentialsAttributesW,   /* SetCredentialsAttributes */
};
