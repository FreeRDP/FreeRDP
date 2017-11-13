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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/sspi.h>

#include "negotiate.h"

#include "../sspi.h"
#include "../log.h"
#define TAG WINPR_TAG("negotiate")

extern const SecurityFunctionTableA NTLM_SecurityFunctionTableA;
extern const SecurityFunctionTableW NTLM_SecurityFunctionTableW;

extern const SecurityFunctionTableA KERBEROS_SecurityFunctionTableA;
extern const SecurityFunctionTableW KERBEROS_SecurityFunctionTableW;

#ifdef WITH_GSSAPI
static BOOL ErrorInitContextKerberos = FALSE;
#else
static BOOL ErrorInitContextKerberos = TRUE;
#endif

const SecPkgInfoA NEGOTIATE_SecPkgInfoA =
{
	0x00083BB3, /* fCapabilities */
	1, /* wVersion */
	0x0009, /* wRPCID */
	0x00002FE0, /* cbMaxToken */
	"Negotiate", /* Name */
	"Microsoft Package Negotiator" /* Comment */
};

WCHAR NEGOTIATE_SecPkgInfoW_Name[] = { 'N', 'e', 'g', 'o', 't', 'i', 'a', 't', 'e', '\0' };

WCHAR NEGOTIATE_SecPkgInfoW_Comment[] =
{
	'M', 'i', 'c', 'r', 'o', 's', 'o', 'f', 't', ' ',
	'P', 'a', 'c', 'k', 'a', 'g', 'e', ' ',
	'N', 'e', 'g', 'o', 't', 'i', 'a', 't', 'o', 'r', '\0'
};

const SecPkgInfoW NEGOTIATE_SecPkgInfoW =
{
	0x00083BB3, /* fCapabilities */
	1, /* wVersion */
	0x0009, /* wRPCID */
	0x00002FE0, /* cbMaxToken */
	NEGOTIATE_SecPkgInfoW_Name, /* Name */
	NEGOTIATE_SecPkgInfoW_Comment /* Comment */
};


void negotiate_SetSubPackage(NEGOTIATE_CONTEXT* context, const char* name)
{
	if (strcmp(name, KERBEROS_SSP_NAME) == 0)
	{
		context->sspiA = (SecurityFunctionTableA*) &KERBEROS_SecurityFunctionTableA;
		context->sspiW = (SecurityFunctionTableW*) &KERBEROS_SecurityFunctionTableW;
		context->kerberos = TRUE;
	}
	else
	{
		context->sspiA = (SecurityFunctionTableA*) &NTLM_SecurityFunctionTableA;
		context->sspiW = (SecurityFunctionTableW*) &NTLM_SecurityFunctionTableW;
		context->kerberos = FALSE;
	}

	sspi_SecureHandleSetLowerPointer(&(context->SubContext), NULL);
	sspi_SecureHandleSetUpperPointer(&(context->SubContext), NULL);
}

NEGOTIATE_CONTEXT* negotiate_ContextNew()
{
	NEGOTIATE_CONTEXT* context;
	context = (NEGOTIATE_CONTEXT*) calloc(1, sizeof(NEGOTIATE_CONTEXT));

	if (!context)
		return NULL;

	context->NegotiateFlags = 0;
	context->state = NEGOTIATE_STATE_INITIAL;
	SecInvalidateHandle(&(context->SubContext));
	negotiate_SetSubPackage(context, (const char*) KERBEROS_SSP_NAME);
	return context;
}

void negotiate_ContextFree(NEGOTIATE_CONTEXT* context)
{
	free(context);
}

SECURITY_STATUS SEC_ENTRY negotiate_InitializeSecurityContextW(PCredHandle phCredential,
        PCtxtHandle phContext,
        SEC_WCHAR* pszTargetName, ULONG fContextReq, ULONG Reserved1, ULONG TargetDataRep,
        PSecBufferDesc pInput, ULONG Reserved2, PCtxtHandle phNewContext,
        PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;
	NEGOTIATE_CONTEXT* context;
	context = (NEGOTIATE_CONTEXT*) sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
	{
		context = negotiate_ContextNew();

		if (!context)
			return SEC_E_INTERNAL_ERROR;

		sspi_SecureHandleSetLowerPointer(phNewContext, context);
		sspi_SecureHandleSetUpperPointer(phNewContext, (void*) NEGO_SSP_NAME);
	}

	/* if Kerberos has previously failed or WITH_GSSAPI is not defined, we use NTLM directly */
	if (ErrorInitContextKerberos == FALSE)
	{
		if (!pInput)
		{
			negotiate_SetSubPackage(context, (const char*) KERBEROS_SSP_NAME);
		}

		status = context->sspiW->InitializeSecurityContextW(phCredential, &(context->SubContext),
		         pszTargetName, fContextReq, Reserved1, TargetDataRep, pInput, Reserved2, &(context->SubContext),
		         pOutput, pfContextAttr, ptsExpiry);

		if (status == SEC_E_NO_CREDENTIALS)
		{
			WLog_WARN(TAG, "No Kerberos credentials. Retry with NTLM");
			ErrorInitContextKerberos = TRUE;
			context->sspiA->DeleteSecurityContext(&(context->SubContext));
			negotiate_ContextFree(context);
			return status;
		}
	}
	else
	{
		if (!pInput)
		{
			context->sspiA->DeleteSecurityContext(&(context->SubContext));
			negotiate_SetSubPackage(context, (const char*) NTLM_SSP_NAME);
		}

		status = context->sspiW->InitializeSecurityContextW(phCredential, &(context->SubContext),
		         pszTargetName, fContextReq, Reserved1, TargetDataRep, pInput, Reserved2, &(context->SubContext),
		         pOutput, pfContextAttr, ptsExpiry);
	}

	return status;
}

SECURITY_STATUS SEC_ENTRY negotiate_InitializeSecurityContextA(PCredHandle phCredential,
        PCtxtHandle phContext,
        SEC_CHAR* pszTargetName, ULONG fContextReq, ULONG Reserved1, ULONG TargetDataRep,
        PSecBufferDesc pInput, ULONG Reserved2, PCtxtHandle phNewContext,
        PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;
	NEGOTIATE_CONTEXT* context;
	context = (NEGOTIATE_CONTEXT*) sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
	{
		context = negotiate_ContextNew();

		if (!context)
			return SEC_E_INTERNAL_ERROR;

		sspi_SecureHandleSetLowerPointer(phNewContext, context);
		sspi_SecureHandleSetUpperPointer(phNewContext, (void*) NEGO_SSP_NAME);
	}

	/* if Kerberos has previously failed or WITH_GSSAPI is not defined, we use NTLM directly */
	if (ErrorInitContextKerberos == FALSE)
	{
		if (!pInput)
		{
			negotiate_SetSubPackage(context, (const char*) KERBEROS_SSP_NAME);
		}

		status = context->sspiA->InitializeSecurityContextA(phCredential, &(context->SubContext),
		         pszTargetName, fContextReq, Reserved1, TargetDataRep, pInput, Reserved2, &(context->SubContext),
		         pOutput, pfContextAttr, ptsExpiry);

		if (status == SEC_E_NO_CREDENTIALS)
		{
			WLog_WARN(TAG, "No Kerberos credentials. Retry with NTLM");
			ErrorInitContextKerberos = TRUE;
			context->sspiA->DeleteSecurityContext(&(context->SubContext));
			negotiate_ContextFree(context);
			return status;
		}
	}
	else
	{
		if (!pInput)
		{
			context->sspiA->DeleteSecurityContext(&(context->SubContext));
			negotiate_SetSubPackage(context, (const char*) NTLM_SSP_NAME);
		}

		status = context->sspiA->InitializeSecurityContextA(phCredential, &(context->SubContext),
		         pszTargetName, fContextReq, Reserved1, TargetDataRep, pInput, Reserved2, &(context->SubContext),
		         pOutput, pfContextAttr, ptsExpiry);
	}

	return status;
}

SECURITY_STATUS SEC_ENTRY negotiate_AcceptSecurityContext(PCredHandle phCredential,
        PCtxtHandle phContext,
        PSecBufferDesc pInput, ULONG fContextReq, ULONG TargetDataRep, PCtxtHandle phNewContext,
        PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsTimeStamp)
{
	SECURITY_STATUS status;
	NEGOTIATE_CONTEXT* context;
	context = (NEGOTIATE_CONTEXT*) sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
	{
		context = negotiate_ContextNew();

		if (!context)
			return SEC_E_INTERNAL_ERROR;

		sspi_SecureHandleSetLowerPointer(phNewContext, context);
		sspi_SecureHandleSetUpperPointer(phNewContext, (void*) NEGO_SSP_NAME);
	}

	negotiate_SetSubPackage(context,
	                        (const char*) NTLM_SSP_NAME); /* server-side Kerberos not yet implemented */
	status = context->sspiA->AcceptSecurityContext(phCredential, &(context->SubContext),
	         pInput, fContextReq, TargetDataRep, &(context->SubContext),
	         pOutput, pfContextAttr, ptsTimeStamp);

	if (status != SEC_E_OK)
	{
		WLog_WARN(TAG, "AcceptSecurityContext status %s [0x%08"PRIX32"]",
		          GetSecurityStatusString(status), status);
	}

	return status;
}

SECURITY_STATUS SEC_ENTRY negotiate_CompleteAuthToken(PCtxtHandle phContext, PSecBufferDesc pToken)
{
	NEGOTIATE_CONTEXT* context;
	SECURITY_STATUS status = SEC_E_OK;
	context = (NEGOTIATE_CONTEXT*) sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
		return SEC_E_INVALID_HANDLE;

	if (context->sspiW->CompleteAuthToken)
		status = context->sspiW->CompleteAuthToken(&(context->SubContext), pToken);

	return status;
}

SECURITY_STATUS SEC_ENTRY negotiate_DeleteSecurityContext(PCtxtHandle phContext)
{
	NEGOTIATE_CONTEXT* context;
	SECURITY_STATUS status = SEC_E_OK;
	context = (NEGOTIATE_CONTEXT*) sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
		return SEC_E_INVALID_HANDLE;

	if (context->sspiW->DeleteSecurityContext)
		status = context->sspiW->DeleteSecurityContext(&(context->SubContext));

	negotiate_ContextFree(context);
	return status;
}

SECURITY_STATUS SEC_ENTRY negotiate_ImpersonateSecurityContext(PCtxtHandle phContext)
{
	NEGOTIATE_CONTEXT* context;
	SECURITY_STATUS status = SEC_E_OK;
	context = (NEGOTIATE_CONTEXT*) sspi_SecureHandleGetLowerPointer(phContext);

	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (context->sspiW->ImpersonateSecurityContext)
		status = context->sspiW->ImpersonateSecurityContext(&(context->SubContext));

	return status;
}

SECURITY_STATUS SEC_ENTRY negotiate_RevertSecurityContext(PCtxtHandle phContext)
{
	NEGOTIATE_CONTEXT* context;
	SECURITY_STATUS status = SEC_E_OK;
	context = (NEGOTIATE_CONTEXT*) sspi_SecureHandleGetLowerPointer(phContext);

	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (context->sspiW->RevertSecurityContext)
		status = context->sspiW->RevertSecurityContext(&(context->SubContext));

	return status;
}

SECURITY_STATUS SEC_ENTRY negotiate_QueryContextAttributesW(PCtxtHandle phContext,
        ULONG ulAttribute, void* pBuffer)
{
	NEGOTIATE_CONTEXT* context;
	SECURITY_STATUS status = SEC_E_OK;
	context = (NEGOTIATE_CONTEXT*) sspi_SecureHandleGetLowerPointer(phContext);

	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (!pBuffer)
		return SEC_E_INSUFFICIENT_MEMORY;

	if (context->sspiW->QueryContextAttributesW)
		status = context->sspiW->QueryContextAttributesW(&(context->SubContext), ulAttribute, pBuffer);

	return status;
}

SECURITY_STATUS SEC_ENTRY negotiate_QueryContextAttributesA(PCtxtHandle phContext,
        ULONG ulAttribute, void* pBuffer)
{
	NEGOTIATE_CONTEXT* context;
	SECURITY_STATUS status = SEC_E_OK;
	context = (NEGOTIATE_CONTEXT*) sspi_SecureHandleGetLowerPointer(phContext);

	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (!pBuffer)
		return SEC_E_INSUFFICIENT_MEMORY;

	if (context->sspiA->QueryContextAttributesA)
		status = context->sspiA->QueryContextAttributesA(&(context->SubContext), ulAttribute, pBuffer);

	return status;
}

SECURITY_STATUS SEC_ENTRY negotiate_SetContextAttributesW(PCtxtHandle phContext, ULONG ulAttribute,
        void* pBuffer, ULONG cbBuffer)
{
	NEGOTIATE_CONTEXT* context;
	SECURITY_STATUS status = SEC_E_OK;
	context = (NEGOTIATE_CONTEXT*) sspi_SecureHandleGetLowerPointer(phContext);

	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (!pBuffer)
		return SEC_E_INSUFFICIENT_MEMORY;

	if (context->sspiW->SetContextAttributesW)
		status = context->sspiW->SetContextAttributesW(&(context->SubContext), ulAttribute, pBuffer,
		         cbBuffer);

	return status;
}

SECURITY_STATUS SEC_ENTRY negotiate_SetContextAttributesA(PCtxtHandle phContext, ULONG ulAttribute,
        void* pBuffer, ULONG cbBuffer)
{
	NEGOTIATE_CONTEXT* context;
	SECURITY_STATUS status = SEC_E_OK;
	context = (NEGOTIATE_CONTEXT*) sspi_SecureHandleGetLowerPointer(phContext);

	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (!pBuffer)
		return SEC_E_INSUFFICIENT_MEMORY;

	if (context->sspiA->SetContextAttributesA)
		status = context->sspiA->SetContextAttributesA(&(context->SubContext), ulAttribute, pBuffer,
		         cbBuffer);

	return status;
}

SECURITY_STATUS SEC_ENTRY negotiate_AcquireCredentialsHandleW(SEC_WCHAR* pszPrincipal,
        SEC_WCHAR* pszPackage,
        ULONG fCredentialUse, void* pvLogonID, void* pAuthData, SEC_GET_KEY_FN pGetKeyFn,
        void* pvGetKeyArgument, PCredHandle phCredential, PTimeStamp ptsExpiry)
{
	SSPI_CREDENTIALS* credentials;
	SEC_WINNT_AUTH_IDENTITY* identity;

	if ((fCredentialUse != SECPKG_CRED_OUTBOUND) &&
	    (fCredentialUse != SECPKG_CRED_INBOUND) &&
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
	identity = (SEC_WINNT_AUTH_IDENTITY*) pAuthData;

	if (identity)
		sspi_CopyAuthIdentity(&(credentials->identity), identity);

	sspi_SecureHandleSetLowerPointer(phCredential, (void*) credentials);
	sspi_SecureHandleSetUpperPointer(phCredential, (void*) NEGO_SSP_NAME);
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY negotiate_AcquireCredentialsHandleA(SEC_CHAR* pszPrincipal,
        SEC_CHAR* pszPackage,
        ULONG fCredentialUse, void* pvLogonID, void* pAuthData, SEC_GET_KEY_FN pGetKeyFn,
        void* pvGetKeyArgument, PCredHandle phCredential, PTimeStamp ptsExpiry)
{
	SSPI_CREDENTIALS* credentials;
	SEC_WINNT_AUTH_IDENTITY* identity;

	if ((fCredentialUse != SECPKG_CRED_OUTBOUND) &&
	    (fCredentialUse != SECPKG_CRED_INBOUND) &&
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
	identity = (SEC_WINNT_AUTH_IDENTITY*) pAuthData;

	if (identity)
		sspi_CopyAuthIdentity(&(credentials->identity), identity);

	sspi_SecureHandleSetLowerPointer(phCredential, (void*) credentials);
	sspi_SecureHandleSetUpperPointer(phCredential, (void*) NEGO_SSP_NAME);
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY negotiate_QueryCredentialsAttributesW(PCredHandle phCredential,
        ULONG ulAttribute, void* pBuffer)
{
	return SEC_E_UNSUPPORTED_FUNCTION;
}

SECURITY_STATUS SEC_ENTRY negotiate_QueryCredentialsAttributesA(PCredHandle phCredential,
        ULONG ulAttribute, void* pBuffer)
{
	return SEC_E_UNSUPPORTED_FUNCTION;
}

SECURITY_STATUS SEC_ENTRY negotiate_FreeCredentialsHandle(PCredHandle phCredential)
{
	SSPI_CREDENTIALS* credentials;

	if (!phCredential)
		return SEC_E_INVALID_HANDLE;

	credentials = (SSPI_CREDENTIALS*) sspi_SecureHandleGetLowerPointer(phCredential);

	if (!credentials)
		return SEC_E_INVALID_HANDLE;

	sspi_CredentialsFree(credentials);
	sspi_SecureHandleInvalidate(phCredential);
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY negotiate_EncryptMessage(PCtxtHandle phContext, ULONG fQOP,
        PSecBufferDesc pMessage, ULONG MessageSeqNo)
{
	NEGOTIATE_CONTEXT* context;
	SECURITY_STATUS status = SEC_E_UNSUPPORTED_FUNCTION;
	context = (NEGOTIATE_CONTEXT*) sspi_SecureHandleGetLowerPointer(phContext);

	if (context->sspiW->EncryptMessage)
		status = context->sspiW->EncryptMessage(&(context->SubContext), fQOP, pMessage, MessageSeqNo);

	return status;
}

SECURITY_STATUS SEC_ENTRY negotiate_DecryptMessage(PCtxtHandle phContext, PSecBufferDesc pMessage,
        ULONG MessageSeqNo, ULONG* pfQOP)
{
	NEGOTIATE_CONTEXT* context;
	SECURITY_STATUS status = SEC_E_UNSUPPORTED_FUNCTION;
	context = (NEGOTIATE_CONTEXT*) sspi_SecureHandleGetLowerPointer(phContext);

	if (context->sspiW->DecryptMessage)
		status = context->sspiW->DecryptMessage(&(context->SubContext), pMessage, MessageSeqNo, pfQOP);

	return status;
}

SECURITY_STATUS SEC_ENTRY negotiate_MakeSignature(PCtxtHandle phContext, ULONG fQOP,
        PSecBufferDesc pMessage, ULONG MessageSeqNo)
{
	NEGOTIATE_CONTEXT* context;
	SECURITY_STATUS status = SEC_E_UNSUPPORTED_FUNCTION;
	context = (NEGOTIATE_CONTEXT*) sspi_SecureHandleGetLowerPointer(phContext);

	if (context->sspiW->MakeSignature)
		status = context->sspiW->MakeSignature(&(context->SubContext), fQOP, pMessage, MessageSeqNo);

	return status;
}

SECURITY_STATUS SEC_ENTRY negotiate_VerifySignature(PCtxtHandle phContext, PSecBufferDesc pMessage,
        ULONG MessageSeqNo, ULONG* pfQOP)
{
	NEGOTIATE_CONTEXT* context;
	SECURITY_STATUS status = SEC_E_UNSUPPORTED_FUNCTION;
	context = (NEGOTIATE_CONTEXT*) sspi_SecureHandleGetLowerPointer(phContext);

	if (context->sspiW->VerifySignature)
		status = context->sspiW->VerifySignature(&(context->SubContext), pMessage, MessageSeqNo, pfQOP);

	return status;
}

const SecurityFunctionTableA NEGOTIATE_SecurityFunctionTableA =
{
	1, /* dwVersion */
	NULL, /* EnumerateSecurityPackages */
	negotiate_QueryCredentialsAttributesA, /* QueryCredentialsAttributes */
	negotiate_AcquireCredentialsHandleA, /* AcquireCredentialsHandle */
	negotiate_FreeCredentialsHandle, /* FreeCredentialsHandle */
	NULL, /* Reserved2 */
	negotiate_InitializeSecurityContextA, /* InitializeSecurityContext */
	negotiate_AcceptSecurityContext, /* AcceptSecurityContext */
	negotiate_CompleteAuthToken, /* CompleteAuthToken */
	negotiate_DeleteSecurityContext, /* DeleteSecurityContext */
	NULL, /* ApplyControlToken */
	negotiate_QueryContextAttributesA, /* QueryContextAttributes */
	negotiate_ImpersonateSecurityContext, /* ImpersonateSecurityContext */
	negotiate_RevertSecurityContext, /* RevertSecurityContext */
	negotiate_MakeSignature, /* MakeSignature */
	negotiate_VerifySignature, /* VerifySignature */
	NULL, /* FreeContextBuffer */
	NULL, /* QuerySecurityPackageInfo */
	NULL, /* Reserved3 */
	NULL, /* Reserved4 */
	NULL, /* ExportSecurityContext */
	NULL, /* ImportSecurityContext */
	NULL, /* AddCredentials */
	NULL, /* Reserved8 */
	NULL, /* QuerySecurityContextToken */
	negotiate_EncryptMessage, /* EncryptMessage */
	negotiate_DecryptMessage, /* DecryptMessage */
	negotiate_SetContextAttributesA, /* SetContextAttributes */
};

const SecurityFunctionTableW NEGOTIATE_SecurityFunctionTableW =
{
	1, /* dwVersion */
	NULL, /* EnumerateSecurityPackages */
	negotiate_QueryCredentialsAttributesW, /* QueryCredentialsAttributes */
	negotiate_AcquireCredentialsHandleW, /* AcquireCredentialsHandle */
	negotiate_FreeCredentialsHandle, /* FreeCredentialsHandle */
	NULL, /* Reserved2 */
	negotiate_InitializeSecurityContextW, /* InitializeSecurityContext */
	negotiate_AcceptSecurityContext, /* AcceptSecurityContext */
	negotiate_CompleteAuthToken, /* CompleteAuthToken */
	negotiate_DeleteSecurityContext, /* DeleteSecurityContext */
	NULL, /* ApplyControlToken */
	negotiate_QueryContextAttributesW, /* QueryContextAttributes */
	negotiate_ImpersonateSecurityContext, /* ImpersonateSecurityContext */
	negotiate_RevertSecurityContext, /* RevertSecurityContext */
	negotiate_MakeSignature, /* MakeSignature */
	negotiate_VerifySignature, /* VerifySignature */
	NULL, /* FreeContextBuffer */
	NULL, /* QuerySecurityPackageInfo */
	NULL, /* Reserved3 */
	NULL, /* Reserved4 */
	NULL, /* ExportSecurityContext */
	NULL, /* ImportSecurityContext */
	NULL, /* AddCredentials */
	NULL, /* Reserved8 */
	NULL, /* QuerySecurityContextToken */
	negotiate_EncryptMessage, /* EncryptMessage */
	negotiate_DecryptMessage, /* DecryptMessage */
	negotiate_SetContextAttributesW, /* SetContextAttributes */
};

