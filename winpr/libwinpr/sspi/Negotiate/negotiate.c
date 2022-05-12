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

extern const SecurityFunctionTableA NTLM_SecurityFunctionTableA;
extern const SecurityFunctionTableW NTLM_SecurityFunctionTableW;

extern const SecurityFunctionTableA KERBEROS_SecurityFunctionTableA;
extern const SecurityFunctionTableW KERBEROS_SecurityFunctionTableW;

#ifdef WITH_GSSAPI
static BOOL ErrorInitContextKerberos = FALSE;
#else
static BOOL ErrorInitContextKerberos = TRUE;
#endif

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
	BOOL ntlm = TRUE;
	BOOL kerberos = TRUE;
	SECURITY_STATUS status = SEC_E_INTERNAL_ERROR;
	NEGOTIATE_CONTEXT* context = NULL;

	if (!negotiate_get_config(&kerberos, &ntlm))
		return status;

	context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
	{
		context = negotiate_ContextNew();

		if (!context)
			return SEC_E_INTERNAL_ERROR;

		sspi_SecureHandleSetLowerPointer(phNewContext, context);
		sspi_SecureHandleSetUpperPointer(phNewContext, (void*)NEGO_SSP_NAME);
	}

	/* if Kerberos has previously failed or WITH_GSSAPI is not defined, we use NTLM directly */
	if (kerberos && !ErrorInitContextKerberos)
	{
		if (!pInput)
		{
			context->sspiW->DeleteSecurityContext(&(context->SubContext));
			SecInvalidateHandle(&context->SubContext);
			negotiate_SetSubPackage(context, KERBEROS_SSP_NAME);
		}

		status = context->sspiW->InitializeSecurityContextW(
		    phCredential, &(context->SubContext), pszTargetName, fContextReq, Reserved1,
		    TargetDataRep, pInput, Reserved2, &(context->SubContext), pOutput, pfContextAttr,
		    ptsExpiry);

#if !defined(WITH_GSS_NO_NTLM_FALLBACK)
		if (ntlm && (status == SEC_E_NO_CREDENTIALS))
		{
			WLog_WARN(TAG, "No Kerberos credentials. Retry with NTLM");
			ErrorInitContextKerberos = TRUE;
			context->sspiW->DeleteSecurityContext(&(context->SubContext));
			SecInvalidateHandle(&context->SubContext);
		}
#endif
	}

	if (ntlm && ErrorInitContextKerberos)
	{
		if (!pInput)
		{
			context->sspiW->DeleteSecurityContext(&(context->SubContext));
			SecInvalidateHandle(&context->SubContext);
			negotiate_SetSubPackage(context, NTLM_SSP_NAME);
		}

		status = context->sspiW->InitializeSecurityContextW(
		    phCredential, &(context->SubContext), pszTargetName, fContextReq, Reserved1,
		    TargetDataRep, pInput, Reserved2, &(context->SubContext), pOutput, pfContextAttr,
		    ptsExpiry);
	}

	return status;
}

static SECURITY_STATUS SEC_ENTRY negotiate_InitializeSecurityContextA(
    PCredHandle phCredential, PCtxtHandle phContext, SEC_CHAR* pszTargetName, ULONG fContextReq,
    ULONG Reserved1, ULONG TargetDataRep, PSecBufferDesc pInput, ULONG Reserved2,
    PCtxtHandle phNewContext, PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	BOOL ntlm = TRUE;
	BOOL kerberos = TRUE;
	SECURITY_STATUS status = SEC_E_INTERNAL_ERROR;
	NEGOTIATE_CONTEXT* context = NULL;

	if (!negotiate_get_config(&kerberos, &ntlm))
		return status;

	context = (NEGOTIATE_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
	{
		context = negotiate_ContextNew();

		if (!context)
			return SEC_E_INTERNAL_ERROR;

		sspi_SecureHandleSetLowerPointer(phNewContext, context);
		sspi_SecureHandleSetUpperPointer(phNewContext, (void*)NEGO_SSP_NAME);
	}

	/* if Kerberos has previously failed or WITH_GSSAPI is not defined, we use NTLM directly */
	if (kerberos && !ErrorInitContextKerberos)
	{
		if (!pInput)
		{
			context->sspiA->DeleteSecurityContext(&(context->SubContext));
			SecInvalidateHandle(&context->SubContext);
			negotiate_SetSubPackage(context, KERBEROS_SSP_NAME);
		}

		status = context->sspiA->InitializeSecurityContextA(
		    phCredential, &(context->SubContext), pszTargetName, fContextReq, Reserved1,
		    TargetDataRep, pInput, Reserved2, &(context->SubContext), pOutput, pfContextAttr,
		    ptsExpiry);

#if !defined(WITH_GSS_NO_NTLM_FALLBACK)
		if (ntlm && (status == SEC_E_NO_CREDENTIALS))
		{
			WLog_WARN(TAG, "No Kerberos credentials. Retry with NTLM");
			ErrorInitContextKerberos = TRUE;
			context->sspiA->DeleteSecurityContext(&(context->SubContext));
			SecInvalidateHandle(&context->SubContext);
		}
#endif
	}

	if (ntlm && ErrorInitContextKerberos)
	{
		if (!pInput)
		{
			context->sspiA->DeleteSecurityContext(&(context->SubContext));
			SecInvalidateHandle(&context->SubContext);
			negotiate_SetSubPackage(context, NTLM_SSP_NAME);
		}

		status = context->sspiA->InitializeSecurityContextA(
		    phCredential, &(context->SubContext), pszTargetName, fContextReq, Reserved1,
		    TargetDataRep, pInput, Reserved2, &(context->SubContext), pOutput, pfContextAttr,
		    ptsExpiry);
	}

	return status;
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
	SSPI_CREDENTIALS* credentials;

	if ((fCredentialUse != SECPKG_CRED_OUTBOUND) && (fCredentialUse != SECPKG_CRED_INBOUND) &&
	    (fCredentialUse != SECPKG_CRED_BOTH))
	{
		return SEC_E_INVALID_PARAMETER;
	}

	credentials = sspi_CredentialsNew();
	if (!credentials)
		return SEC_E_INTERNAL_ERROR;

	credentials->fCredentialUse = fCredentialUse;
	credentials->pGetKeyFn = pGetKeyFn;
	credentials->pvGetKeyArgument = pvGetKeyArgument;

	if (pAuthData)
	{
		SEC_WINNT_AUTH_IDENTITY_EXW* identityEx = (SEC_WINNT_AUTH_IDENTITY_EXW*)pAuthData;
		BOOL treated = FALSE;

		SEC_WINNT_AUTH_IDENTITY srcIdentity;
		if (identityEx->Version == SEC_WINNT_AUTH_IDENTITY_VERSION)
		{
			if (identityEx->Length >= sizeof(*identityEx))
			{
				srcIdentity.User = (UINT16*)identityEx->User;
				srcIdentity.UserLength = identityEx->UserLength;
				srcIdentity.Domain = (UINT16*)identityEx->Domain;
				srcIdentity.DomainLength = identityEx->DomainLength;
				srcIdentity.Password = (UINT16*)identityEx->Password;
				srcIdentity.PasswordLength = identityEx->PasswordLength;
				srcIdentity.Flags = identityEx->Flags;

				if (!sspi_CopyAuthIdentity(&credentials->identity, &srcIdentity))
					return SEC_E_INSUFFICIENT_MEMORY;

				if (identityEx->Length == sizeof(SEC_WINNT_AUTH_IDENTITY_WINPRA))
				{
					SEC_WINNT_AUTH_IDENTITY_WINPRW* identityWinpr =
					    (SEC_WINNT_AUTH_IDENTITY_WINPRW*)pAuthData;
					credentials->kerbSettings = identityWinpr->kerberosSettings;
				}
				treated = TRUE;
			}
		}

		if (!treated)
		{
			SEC_WINNT_AUTH_IDENTITY* identity = (SEC_WINNT_AUTH_IDENTITY*)pAuthData;
			sspi_CopyAuthIdentity(&(credentials->identity), identity);
		}
	}

	sspi_SecureHandleSetLowerPointer(phCredential, (void*)credentials);
	sspi_SecureHandleSetUpperPointer(phCredential, (void*)NEGO_SSP_NAME);
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY negotiate_AcquireCredentialsHandleA(
    SEC_CHAR* pszPrincipal, SEC_CHAR* pszPackage, ULONG fCredentialUse, void* pvLogonID,
    void* pAuthData, SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument, PCredHandle phCredential,
    PTimeStamp ptsExpiry)
{
	SSPI_CREDENTIALS* credentials;

	if ((fCredentialUse != SECPKG_CRED_OUTBOUND) && (fCredentialUse != SECPKG_CRED_INBOUND) &&
	    (fCredentialUse != SECPKG_CRED_BOTH))
	{
		return SEC_E_INVALID_PARAMETER;
	}

	credentials = sspi_CredentialsNew();

	if (!credentials)
		return SEC_E_INTERNAL_ERROR;

	credentials->fCredentialUse = fCredentialUse;
	credentials->pGetKeyFn = pGetKeyFn;
	credentials->pvGetKeyArgument = pvGetKeyArgument;

	if (pAuthData)
	{
		SEC_WINNT_AUTH_IDENTITY_EXA* identityEx = (SEC_WINNT_AUTH_IDENTITY_EXA*)pAuthData;
		BOOL treated = FALSE;

		SEC_WINNT_AUTH_IDENTITY srcIdentity;
		if (identityEx->Version == SEC_WINNT_AUTH_IDENTITY_VERSION)
		{
			if (identityEx->Length >= sizeof(*identityEx))
			{
				srcIdentity.User = (UINT16*)identityEx->User;
				srcIdentity.UserLength = identityEx->UserLength;
				srcIdentity.Domain = (UINT16*)identityEx->Domain;
				srcIdentity.DomainLength = identityEx->DomainLength;
				srcIdentity.Password = (UINT16*)identityEx->Password;
				srcIdentity.PasswordLength = identityEx->PasswordLength;
				srcIdentity.Flags = identityEx->Flags;

				if (!sspi_CopyAuthIdentity(&credentials->identity, &srcIdentity))
					return SEC_E_INSUFFICIENT_MEMORY;

				if (identityEx->Length == sizeof(SEC_WINNT_AUTH_IDENTITY_WINPRA))
				{
					SEC_WINNT_AUTH_IDENTITY_WINPRA* identityWinpr =
					    (SEC_WINNT_AUTH_IDENTITY_WINPRA*)pAuthData;
					credentials->kerbSettings = identityWinpr->kerberosSettings;
				}
				treated = TRUE;
			}
		}

		if (!treated)
		{
			SEC_WINNT_AUTH_IDENTITY* identity = (SEC_WINNT_AUTH_IDENTITY*)pAuthData;
			sspi_CopyAuthIdentity(&(credentials->identity), identity);
		}
	}

	sspi_SecureHandleSetLowerPointer(phCredential, (void*)credentials);
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
	SSPI_CREDENTIALS* credentials;

	if (!phCredential)
		return SEC_E_INVALID_HANDLE;

	credentials = (SSPI_CREDENTIALS*)sspi_SecureHandleGetLowerPointer(phCredential);

	if (!credentials)
		return SEC_E_INVALID_HANDLE;

	sspi_CredentialsFree(credentials);
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
