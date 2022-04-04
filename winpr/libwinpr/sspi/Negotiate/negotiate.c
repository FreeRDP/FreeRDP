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

#define SEC_PKG_COUNT 2

extern const SecurityFunctionTableA NTLM_SecurityFunctionTableA;
extern const SecurityFunctionTableW NTLM_SecurityFunctionTableW;

extern const SecurityFunctionTableA KERBEROS_SecurityFunctionTableA;
extern const SecurityFunctionTableW KERBEROS_SecurityFunctionTableW;

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

static const SecurityFunctionTableA* negotiate_GetSecurityFunctionTableByNameA(const TCHAR* name)
{
	if (_tcscmp(name, KERBEROS_SSP_NAME) == 0)
		return &KERBEROS_SecurityFunctionTableA;
	else if (_tcscmp(name, NTLM_SSP_NAME) == 0)
		return &NTLM_SecurityFunctionTableA;
	else
		return NULL;
}

static const SecurityFunctionTableW* negotiate_GetSecurityFunctionTableByNameW(const TCHAR* name)
{
	if (_tcscmp(name, KERBEROS_SSP_NAME) == 0)
		return &KERBEROS_SecurityFunctionTableW;
	else if (_tcscmp(name, NTLM_SSP_NAME) == 0)
		return &NTLM_SecurityFunctionTableW;
	else
		return NULL;
}

static void negotiate_SetSubPackage(NEGOTIATE_CONTEXT* context, const TCHAR* name)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(name);

	if (_tcsnccmp(name, KERBEROS_SSP_NAME, ARRAYSIZE(KERBEROS_SSP_NAME)) == 0)
	{
		context->sspiA = &KERBEROS_SecurityFunctionTableA;
		context->sspiW = &KERBEROS_SecurityFunctionTableW;
		context->kerberos = TRUE;
	}
	else
	{
		context->sspiA = &NTLM_SecurityFunctionTableA;
		context->sspiW = &NTLM_SecurityFunctionTableW;
		context->kerberos = FALSE;
	}
}

static NEGOTIATE_CONTEXT* negotiate_ContextNew(void)
{
	NEGOTIATE_CONTEXT* context;
	context = (NEGOTIATE_CONTEXT*)calloc(1, sizeof(NEGOTIATE_CONTEXT));

	if (!context)
		return NULL;

	context->NegotiateFlags = 0;
	context->state = NEGOTIATE_STATE_INITIAL;
	SecInvalidateHandle(&(context->SubContext));
	negotiate_SetSubPackage(context, KERBEROS_SSP_NAME);
	context->current_cred = 0;
	return context;
}

static void negotiate_ContextFree(NEGOTIATE_CONTEXT* context)
{
	free(context);
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

static SECURITY_STATUS SEC_ENTRY negotiate_InitializeSecurityContextW(
    PCredHandle phCredential, PCtxtHandle phContext, SEC_WCHAR* pszTargetName, ULONG fContextReq,
    ULONG Reserved1, ULONG TargetDataRep, PSecBufferDesc pInput, ULONG Reserved2,
    PCtxtHandle phNewContext, PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;
	PCtxtHandle sub_context = NULL;
	PCredHandle sub_cred;
	TCHAR* pkg_name;

	CredHandle* credential_list = (PCredHandle)sspi_SecureHandleGetLowerPointer(phCredential);
	NEGOTIATE_CONTEXT* context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
	{
		context = negotiate_ContextNew();

		if (!context)
			return SEC_E_INTERNAL_ERROR;
	}
	else
	{
		sub_context = &context->SubContext;
	}

	for (; context->current_cred < SEC_PKG_COUNT; context->current_cred++)
	{
		sub_cred = &credential_list[context->current_cred];
		if (!SecIsValidHandle(sub_cred))
			continue;

		pkg_name = sspi_SecureHandleGetUpperPointer(sub_cred);
		negotiate_SetSubPackage(context, pkg_name);

		status = context->sspiW->InitializeSecurityContextW(
		    sub_cred, sub_context, pszTargetName, fContextReq, Reserved1, TargetDataRep, pInput,
		    Reserved2, &context->SubContext, pOutput, pfContextAttr, ptsExpiry);

		if (!IsSecurityStatusError(status))
		{
			sspi_SecureHandleSetLowerPointer(phNewContext, context);
			sspi_SecureHandleSetUpperPointer(phNewContext, (void*)NEGO_SSP_NAME);
			return status;
		}

		if (sub_context)
			context->sspiW->DeleteSecurityContext(sub_context);
		sub_context = NULL;
		pInput = NULL;
	}

	if (!phContext)
		negotiate_ContextFree(context);

	return SEC_E_NO_CREDENTIALS;
}

static SECURITY_STATUS SEC_ENTRY negotiate_InitializeSecurityContextA(
    PCredHandle phCredential, PCtxtHandle phContext, SEC_CHAR* pszTargetName, ULONG fContextReq,
    ULONG Reserved1, ULONG TargetDataRep, PSecBufferDesc pInput, ULONG Reserved2,
    PCtxtHandle phNewContext, PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;
	PCtxtHandle sub_context = NULL;
	PCredHandle sub_cred;
	TCHAR* pkg_name;

	CredHandle* credential_list = (PCredHandle)sspi_SecureHandleGetLowerPointer(phCredential);
	NEGOTIATE_CONTEXT* context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	/* On first call create new context and leave sub_context as null, otherwise use existing
	 * sub_context */
	if (!context)
	{
		context = negotiate_ContextNew();

		if (!context)
			return SEC_E_INTERNAL_ERROR;
	}
	else
	{
		sub_context = &context->SubContext;
	}

	for (; context->current_cred < SEC_PKG_COUNT; context->current_cred++)
	{
		sub_cred = &credential_list[context->current_cred];
		/* If we haven't successfully acquired a credential skip to the next one */
		if (!SecIsValidHandle(sub_cred))
			continue;

		/* Set the sub-package tables to match the credenetial being used */
		pkg_name = sspi_SecureHandleGetUpperPointer(sub_cred);
		negotiate_SetSubPackage(context, pkg_name);

		status = context->sspiA->InitializeSecurityContextA(
		    sub_cred, sub_context, pszTargetName, fContextReq, Reserved1, TargetDataRep, pInput,
		    Reserved2, &context->SubContext, pOutput, pfContextAttr, ptsExpiry);

		/* If the method succeeded, set the output context and return */
		if (!IsSecurityStatusError(status))
		{
			sspi_SecureHandleSetLowerPointer(phNewContext, context);
			sspi_SecureHandleSetUpperPointer(phNewContext, (void*)NEGO_SSP_NAME);
			return status;
		}

		/* Delete the sub_context and reset inputs to begin a new context using the next credential
		 */
		if (sub_context)
			context->sspiA->DeleteSecurityContext(sub_context);
		sub_context = NULL;
		pInput = NULL;
	}

	/* No credentials left to try */

	/* If this was the first call release the context */
	if (!phContext)
		negotiate_ContextFree(context);

	return SEC_E_NO_CREDENTIALS;
}

static SECURITY_STATUS SEC_ENTRY negotiate_AcceptSecurityContext(
    PCredHandle phCredential, PCtxtHandle phContext, PSecBufferDesc pInput, ULONG fContextReq,
    ULONG TargetDataRep, PCtxtHandle phNewContext, PSecBufferDesc pOutput, PULONG pfContextAttr,
    PTimeStamp ptsTimeStamp)
{
	SECURITY_STATUS status;
	NEGOTIATE_CONTEXT* context;
	context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
	{
		context = negotiate_ContextNew();

		if (!context)
			return SEC_E_INTERNAL_ERROR;

		sspi_SecureHandleSetLowerPointer(phNewContext, context);
		sspi_SecureHandleSetUpperPointer(phNewContext, (void*)NEGO_SSP_NAME);
	}

	negotiate_SetSubPackage(context, NTLM_SSP_NAME); /* server-side Kerberos not yet implemented */
	status = context->sspiA->AcceptSecurityContext(
	    phCredential, &(context->SubContext), pInput, fContextReq, TargetDataRep,
	    &(context->SubContext), pOutput, pfContextAttr, ptsTimeStamp);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "AcceptSecurityContext status %s [0x%08" PRIX32 "]",
		          GetSecurityStatusString(status), status);
	}

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

	if (context->sspiW->CompleteAuthToken)
		status = context->sspiW->CompleteAuthToken(&(context->SubContext), pToken);

	return status;
}

static SECURITY_STATUS SEC_ENTRY negotiate_DeleteSecurityContext(PCtxtHandle phContext)
{
	NEGOTIATE_CONTEXT* context;
	SECURITY_STATUS status = SEC_E_OK;
	context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
		return SEC_E_INVALID_HANDLE;

	if (context->sspiW->DeleteSecurityContext)
		status = context->sspiW->DeleteSecurityContext(&(context->SubContext));

	negotiate_ContextFree(context);
	return status;
}

static SECURITY_STATUS SEC_ENTRY negotiate_ImpersonateSecurityContext(PCtxtHandle phContext)
{
	NEGOTIATE_CONTEXT* context;
	SECURITY_STATUS status = SEC_E_OK;
	context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (context->sspiW->ImpersonateSecurityContext)
		status = context->sspiW->ImpersonateSecurityContext(&(context->SubContext));

	return status;
}

static SECURITY_STATUS SEC_ENTRY negotiate_RevertSecurityContext(PCtxtHandle phContext)
{
	NEGOTIATE_CONTEXT* context;
	SECURITY_STATUS status = SEC_E_OK;
	context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (context->sspiW->RevertSecurityContext)
		status = context->sspiW->RevertSecurityContext(&(context->SubContext));

	return status;
}

static SECURITY_STATUS SEC_ENTRY negotiate_QueryContextAttributesW(PCtxtHandle phContext,
                                                                   ULONG ulAttribute, void* pBuffer)
{
	NEGOTIATE_CONTEXT* context;
	SECURITY_STATUS status = SEC_E_OK;
	context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (!pBuffer)
		return SEC_E_INSUFFICIENT_MEMORY;

	if (context->sspiW->QueryContextAttributesW)
		status =
		    context->sspiW->QueryContextAttributesW(&(context->SubContext), ulAttribute, pBuffer);

	return status;
}

static SECURITY_STATUS SEC_ENTRY negotiate_QueryContextAttributesA(PCtxtHandle phContext,
                                                                   ULONG ulAttribute, void* pBuffer)
{
	NEGOTIATE_CONTEXT* context;
	SECURITY_STATUS status = SEC_E_OK;
	context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (!pBuffer)
		return SEC_E_INSUFFICIENT_MEMORY;

	if (context->sspiA->QueryContextAttributesA)
		status =
		    context->sspiA->QueryContextAttributesA(&(context->SubContext), ulAttribute, pBuffer);

	return status;
}

static SECURITY_STATUS SEC_ENTRY negotiate_SetContextAttributesW(PCtxtHandle phContext,
                                                                 ULONG ulAttribute, void* pBuffer,
                                                                 ULONG cbBuffer)
{
	NEGOTIATE_CONTEXT* context;
	SECURITY_STATUS status = SEC_E_OK;
	context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (!pBuffer)
		return SEC_E_INSUFFICIENT_MEMORY;

	if (context->sspiW->SetContextAttributesW)
		status = context->sspiW->SetContextAttributesW(&(context->SubContext), ulAttribute, pBuffer,
		                                               cbBuffer);

	return status;
}

static SECURITY_STATUS SEC_ENTRY negotiate_SetContextAttributesA(PCtxtHandle phContext,
                                                                 ULONG ulAttribute, void* pBuffer,
                                                                 ULONG cbBuffer)
{
	NEGOTIATE_CONTEXT* context;
	SECURITY_STATUS status = SEC_E_OK;
	context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (!pBuffer)
		return SEC_E_INSUFFICIENT_MEMORY;

	if (context->sspiA->SetContextAttributesA)
		status = context->sspiA->SetContextAttributesA(&(context->SubContext), ulAttribute, pBuffer,
		                                               cbBuffer);

	return status;
}

static SECURITY_STATUS SEC_ENTRY negotiate_AcquireCredentialsHandleW(
    SEC_WCHAR* pszPrincipal, SEC_WCHAR* pszPackage, ULONG fCredentialUse, void* pvLogonID,
    void* pAuthData, SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument, PCredHandle phCredential,
    PTimeStamp ptsExpiry)
{
	BOOL kerberos = TRUE, ntlm = TRUE;
	CredHandle* credential_list = (CredHandle*)calloc(SEC_PKG_COUNT, sizeof(CredHandle));

	if (!credential_list)
		return SEC_E_INTERNAL_ERROR;

	for (int i = 0; i < SEC_PKG_COUNT; i++)
		sspi_SecureHandleInvalidate(&credential_list[i]);

	negotiate_get_config(&kerberos, &ntlm);

	if (kerberos)
		KERBEROS_SecurityFunctionTableW.AcquireCredentialsHandleW(
		    pszPrincipal, pszPackage, fCredentialUse, pvLogonID, pAuthData, pGetKeyFn,
		    pvGetKeyArgument, &credential_list[0], ptsExpiry);

	if (ntlm)
		NTLM_SecurityFunctionTableW.AcquireCredentialsHandleW(
		    pszPrincipal, pszPackage, fCredentialUse, pvLogonID, pAuthData, pGetKeyFn,
		    pvGetKeyArgument, &credential_list[1], ptsExpiry);

	sspi_SecureHandleSetLowerPointer(phCredential, (void*)credential_list);
	sspi_SecureHandleSetUpperPointer(phCredential, (void*)NEGO_SSP_NAME);
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY negotiate_AcquireCredentialsHandleA(
    SEC_CHAR* pszPrincipal, SEC_CHAR* pszPackage, ULONG fCredentialUse, void* pvLogonID,
    void* pAuthData, SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument, PCredHandle phCredential,
    PTimeStamp ptsExpiry)
{
	BOOL kerberos = TRUE, ntlm = TRUE;
	CredHandle* credential_list = (CredHandle*)calloc(SEC_PKG_COUNT, sizeof(CredHandle));

	if (!credential_list)
		return SEC_E_INTERNAL_ERROR;

	/* Initially set each credential to invalid */
	for (int i = 0; i < SEC_PKG_COUNT; i++)
		SecInvalidateHandle(&credential_list[i]);

	negotiate_get_config(&kerberos, &ntlm);

	if (kerberos)
		KERBEROS_SecurityFunctionTableA.AcquireCredentialsHandleA(
		    pszPrincipal, pszPackage, fCredentialUse, pvLogonID, pAuthData, pGetKeyFn,
		    pvGetKeyArgument, &credential_list[0], ptsExpiry);

	if (ntlm)
		NTLM_SecurityFunctionTableA.AcquireCredentialsHandleA(
		    pszPrincipal, pszPackage, fCredentialUse, pvLogonID, pAuthData, pGetKeyFn,
		    pvGetKeyArgument, &credential_list[1], ptsExpiry);

	sspi_SecureHandleSetLowerPointer(phCredential, (void*)credential_list);
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
	TCHAR* pkg_name;
	SecurityFunctionTableA* table;
	CredHandle* credential_list = (CredHandle*)sspi_SecureHandleGetLowerPointer(phCredential);

	if (!credential_list)
		return SEC_E_INVALID_HANDLE;

	for (int i = 0; i < SEC_PKG_COUNT; i++)
	{
		pkg_name = sspi_SecureHandleGetUpperPointer(&credential_list[i]);

		// skip if credential was not acquired
		if (!pkg_name)
			continue;

		table = negotiate_GetSecurityFunctionTableByNameA(pkg_name);
		if (table)
			table->FreeCredentialsHandle(&credential_list[i]);
	}

	free(credential_list);
	sspi_SecureHandleInvalidate(phCredential);
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY negotiate_EncryptMessage(PCtxtHandle phContext, ULONG fQOP,
                                                          PSecBufferDesc pMessage,
                                                          ULONG MessageSeqNo)
{
	NEGOTIATE_CONTEXT* context;
	SECURITY_STATUS status = SEC_E_UNSUPPORTED_FUNCTION;
	context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (context->sspiW->EncryptMessage)
		status =
		    context->sspiW->EncryptMessage(&(context->SubContext), fQOP, pMessage, MessageSeqNo);
	else
		WLog_WARN(TAG, "[%s] SSPI implementation of function is missing", __FUNCTION__);
	return status;
}

static SECURITY_STATUS SEC_ENTRY negotiate_DecryptMessage(PCtxtHandle phContext,
                                                          PSecBufferDesc pMessage,
                                                          ULONG MessageSeqNo, ULONG* pfQOP)
{
	NEGOTIATE_CONTEXT* context;
	SECURITY_STATUS status = SEC_E_UNSUPPORTED_FUNCTION;
	context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (context->sspiW->DecryptMessage)
		status =
		    context->sspiW->DecryptMessage(&(context->SubContext), pMessage, MessageSeqNo, pfQOP);
	else
		WLog_WARN(TAG, "[%s] SSPI implementation of function is missing", __FUNCTION__);
	return status;
}

static SECURITY_STATUS SEC_ENTRY negotiate_MakeSignature(PCtxtHandle phContext, ULONG fQOP,
                                                         PSecBufferDesc pMessage,
                                                         ULONG MessageSeqNo)
{
	NEGOTIATE_CONTEXT* context;
	SECURITY_STATUS status = SEC_E_UNSUPPORTED_FUNCTION;
	context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (context->sspiW->MakeSignature)
		status =
		    context->sspiW->MakeSignature(&(context->SubContext), fQOP, pMessage, MessageSeqNo);
	else
		WLog_WARN(TAG, "[%s] SSPI implementation of function is missing", __FUNCTION__);
	return status;
}

static SECURITY_STATUS SEC_ENTRY negotiate_VerifySignature(PCtxtHandle phContext,
                                                           PSecBufferDesc pMessage,
                                                           ULONG MessageSeqNo, ULONG* pfQOP)
{
	NEGOTIATE_CONTEXT* context;
	SECURITY_STATUS status = SEC_E_UNSUPPORTED_FUNCTION;
	context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (context->sspiW->VerifySignature)
		status =
		    context->sspiW->VerifySignature(&(context->SubContext), pMessage, MessageSeqNo, pfQOP);
	else
		WLog_WARN(TAG, "[%s] SSPI implementation of function is missing", __FUNCTION__);
	return status;
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
