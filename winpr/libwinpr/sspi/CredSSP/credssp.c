/**
 * WinPR: Windows Portable Runtime
 * Credential Security Support Provider (CredSSP)
 *
 * Copyright 2010-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *		 http://www.apache.org/licenses/LICENSE-2.0
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

#include "credssp.h"

#include "../sspi.h"
#include "../../log.h"

#define TAG WINPR_TAG("sspi.CredSSP")

static const char* CREDSSP_PACKAGE_NAME = "CredSSP";

static SECURITY_STATUS SEC_ENTRY credssp_InitializeSecurityContextW(
    WINPR_ATTR_UNUSED PCredHandle phCredential, WINPR_ATTR_UNUSED PCtxtHandle phContext,
    WINPR_ATTR_UNUSED SEC_WCHAR* pszTargetName, WINPR_ATTR_UNUSED ULONG fContextReq,
    WINPR_ATTR_UNUSED ULONG Reserved1, WINPR_ATTR_UNUSED ULONG TargetDataRep,
    WINPR_ATTR_UNUSED PSecBufferDesc pInput, WINPR_ATTR_UNUSED ULONG Reserved2,
    WINPR_ATTR_UNUSED PCtxtHandle phNewContext, WINPR_ATTR_UNUSED PSecBufferDesc pOutput,
    WINPR_ATTR_UNUSED PULONG pfContextAttr, WINPR_ATTR_UNUSED PTimeStamp ptsExpiry)
{
	WLog_ERR(TAG, "TODO: Implement");
	return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY credssp_InitializeSecurityContextA(
    PCredHandle phCredential, PCtxtHandle phContext, WINPR_ATTR_UNUSED SEC_CHAR* pszTargetName,
    WINPR_ATTR_UNUSED ULONG fContextReq, WINPR_ATTR_UNUSED ULONG Reserved1,
    WINPR_ATTR_UNUSED ULONG TargetDataRep, WINPR_ATTR_UNUSED PSecBufferDesc pInput,
    WINPR_ATTR_UNUSED ULONG Reserved2, PCtxtHandle phNewContext,
    WINPR_ATTR_UNUSED PSecBufferDesc pOutput, WINPR_ATTR_UNUSED PULONG pfContextAttr,
    WINPR_ATTR_UNUSED PTimeStamp ptsExpiry)
{
	CREDSSP_CONTEXT* context = NULL;
	SSPI_CREDENTIALS* credentials = NULL;

	/* behave like windows SSPIs that don't want empty context */
	if (phContext && !phContext->dwLower && !phContext->dwUpper)
		return SEC_E_INVALID_HANDLE;

	context = (CREDSSP_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
	{
		union
		{
			const void* cpv;
			void* pv;
		} cnv;
		context = credssp_ContextNew();

		if (!context)
			return SEC_E_INSUFFICIENT_MEMORY;

		credentials = (SSPI_CREDENTIALS*)sspi_SecureHandleGetLowerPointer(phCredential);

		if (!credentials)
		{
			credssp_ContextFree(context);
			return SEC_E_INVALID_HANDLE;
		}

		sspi_SecureHandleSetLowerPointer(phNewContext, context);

		cnv.cpv = CREDSSP_PACKAGE_NAME;
		sspi_SecureHandleSetUpperPointer(phNewContext, cnv.pv);
	}

	return SEC_E_OK;
}

CREDSSP_CONTEXT* credssp_ContextNew(void)
{
	CREDSSP_CONTEXT* context = NULL;
	context = (CREDSSP_CONTEXT*)calloc(1, sizeof(CREDSSP_CONTEXT));

	if (!context)
		return NULL;

	return context;
}

void credssp_ContextFree(CREDSSP_CONTEXT* context)
{
	free(context);
}

static SECURITY_STATUS SEC_ENTRY credssp_QueryContextAttributes(PCtxtHandle phContext,
                                                                WINPR_ATTR_UNUSED ULONG ulAttribute,
                                                                void* pBuffer)
{
	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (!pBuffer)
		return SEC_E_INSUFFICIENT_MEMORY;

	WLog_ERR(TAG, "TODO: Implement");
	return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY credssp_AcquireCredentialsHandleW(
    WINPR_ATTR_UNUSED SEC_WCHAR* pszPrincipal, WINPR_ATTR_UNUSED SEC_WCHAR* pszPackage,
    WINPR_ATTR_UNUSED ULONG fCredentialUse, WINPR_ATTR_UNUSED void* pvLogonID,
    WINPR_ATTR_UNUSED void* pAuthData, WINPR_ATTR_UNUSED SEC_GET_KEY_FN pGetKeyFn,
    WINPR_ATTR_UNUSED void* pvGetKeyArgument, WINPR_ATTR_UNUSED PCredHandle phCredential,
    WINPR_ATTR_UNUSED PTimeStamp ptsExpiry)
{
	WLog_ERR(TAG, "TODO: Implement");
	return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY credssp_AcquireCredentialsHandleA(
    WINPR_ATTR_UNUSED SEC_CHAR* pszPrincipal, WINPR_ATTR_UNUSED SEC_CHAR* pszPackage,
    WINPR_ATTR_UNUSED ULONG fCredentialUse, WINPR_ATTR_UNUSED void* pvLogonID,
    WINPR_ATTR_UNUSED void* pAuthData, WINPR_ATTR_UNUSED SEC_GET_KEY_FN pGetKeyFn,
    WINPR_ATTR_UNUSED void* pvGetKeyArgument, WINPR_ATTR_UNUSED PCredHandle phCredential,
    WINPR_ATTR_UNUSED PTimeStamp ptsExpiry)
{
	SSPI_CREDENTIALS* credentials = NULL;
	SEC_WINNT_AUTH_IDENTITY* identity = NULL;

	if (fCredentialUse == SECPKG_CRED_OUTBOUND)
	{
		union
		{
			const void* cpv;
			void* pv;
		} cnv;
		credentials = sspi_CredentialsNew();

		if (!credentials)
			return SEC_E_INSUFFICIENT_MEMORY;

		identity = (SEC_WINNT_AUTH_IDENTITY*)pAuthData;
		CopyMemory(&(credentials->identity), identity, sizeof(SEC_WINNT_AUTH_IDENTITY));
		sspi_SecureHandleSetLowerPointer(phCredential, (void*)credentials);

		cnv.cpv = CREDSSP_PACKAGE_NAME;
		sspi_SecureHandleSetUpperPointer(phCredential, cnv.pv);
		return SEC_E_OK;
	}

	WLog_ERR(TAG, "TODO: Implement");
	return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY credssp_QueryCredentialsAttributesW(
    WINPR_ATTR_UNUSED PCredHandle phCredential, WINPR_ATTR_UNUSED ULONG ulAttribute,
    WINPR_ATTR_UNUSED void* pBuffer)
{
	WLog_ERR(TAG, "TODO: Implement");
	return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY credssp_QueryCredentialsAttributesA(
    WINPR_ATTR_UNUSED PCredHandle phCredential, WINPR_ATTR_UNUSED ULONG ulAttribute,
    WINPR_ATTR_UNUSED void* pBuffer)
{
	if (ulAttribute == SECPKG_CRED_ATTR_NAMES)
	{
		SSPI_CREDENTIALS* credentials =
		    (SSPI_CREDENTIALS*)sspi_SecureHandleGetLowerPointer(phCredential);

		if (!credentials)
			return SEC_E_INVALID_HANDLE;

		return SEC_E_OK;
	}

	WLog_ERR(TAG, "TODO: Implement");
	return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY credssp_FreeCredentialsHandle(PCredHandle phCredential)
{
	SSPI_CREDENTIALS* credentials = NULL;

	if (!phCredential)
		return SEC_E_INVALID_HANDLE;

	credentials = (SSPI_CREDENTIALS*)sspi_SecureHandleGetLowerPointer(phCredential);

	if (!credentials)
		return SEC_E_INVALID_HANDLE;

	sspi_CredentialsFree(credentials);
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY credssp_EncryptMessage(WINPR_ATTR_UNUSED PCtxtHandle phContext,
                                                        WINPR_ATTR_UNUSED ULONG fQOP,
                                                        WINPR_ATTR_UNUSED PSecBufferDesc pMessage,
                                                        WINPR_ATTR_UNUSED ULONG MessageSeqNo)
{
	WLog_ERR(TAG, "TODO: Implement");
	return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY credssp_DecryptMessage(WINPR_ATTR_UNUSED PCtxtHandle phContext,
                                                        WINPR_ATTR_UNUSED PSecBufferDesc pMessage,
                                                        WINPR_ATTR_UNUSED ULONG MessageSeqNo,
                                                        WINPR_ATTR_UNUSED ULONG* pfQOP)
{
	WLog_ERR(TAG, "TODO: Implement");
	return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY credssp_MakeSignature(WINPR_ATTR_UNUSED PCtxtHandle phContext,
                                                       WINPR_ATTR_UNUSED ULONG fQOP,
                                                       WINPR_ATTR_UNUSED PSecBufferDesc pMessage,
                                                       WINPR_ATTR_UNUSED ULONG MessageSeqNo)
{
	WLog_ERR(TAG, "TODO: Implement");
	return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY credssp_VerifySignature(WINPR_ATTR_UNUSED PCtxtHandle phContext,
                                                         WINPR_ATTR_UNUSED PSecBufferDesc pMessage,
                                                         WINPR_ATTR_UNUSED ULONG MessageSeqNo,
                                                         WINPR_ATTR_UNUSED ULONG* pfQOP)
{
	WLog_ERR(TAG, "TODO: Implement");
	return SEC_E_UNSUPPORTED_FUNCTION;
}

const SecurityFunctionTableA CREDSSP_SecurityFunctionTableA = {
	3,                                   /* dwVersion */
	NULL,                                /* EnumerateSecurityPackages */
	credssp_QueryCredentialsAttributesA, /* QueryCredentialsAttributes */
	credssp_AcquireCredentialsHandleA,   /* AcquireCredentialsHandle */
	credssp_FreeCredentialsHandle,       /* FreeCredentialsHandle */
	NULL,                                /* Reserved2 */
	credssp_InitializeSecurityContextA,  /* InitializeSecurityContext */
	NULL,                                /* AcceptSecurityContext */
	NULL,                                /* CompleteAuthToken */
	NULL,                                /* DeleteSecurityContext */
	NULL,                                /* ApplyControlToken */
	credssp_QueryContextAttributes,      /* QueryContextAttributes */
	NULL,                                /* ImpersonateSecurityContext */
	NULL,                                /* RevertSecurityContext */
	credssp_MakeSignature,               /* MakeSignature */
	credssp_VerifySignature,             /* VerifySignature */
	NULL,                                /* FreeContextBuffer */
	NULL,                                /* QuerySecurityPackageInfo */
	NULL,                                /* Reserved3 */
	NULL,                                /* Reserved4 */
	NULL,                                /* ExportSecurityContext */
	NULL,                                /* ImportSecurityContext */
	NULL,                                /* AddCredentials */
	NULL,                                /* Reserved8 */
	NULL,                                /* QuerySecurityContextToken */
	credssp_EncryptMessage,              /* EncryptMessage */
	credssp_DecryptMessage,              /* DecryptMessage */
	NULL,                                /* SetContextAttributes */
	NULL,                                /* SetCredentialsAttributes */
};

const SecurityFunctionTableW CREDSSP_SecurityFunctionTableW = {
	3,                                   /* dwVersion */
	NULL,                                /* EnumerateSecurityPackages */
	credssp_QueryCredentialsAttributesW, /* QueryCredentialsAttributes */
	credssp_AcquireCredentialsHandleW,   /* AcquireCredentialsHandle */
	credssp_FreeCredentialsHandle,       /* FreeCredentialsHandle */
	NULL,                                /* Reserved2 */
	credssp_InitializeSecurityContextW,  /* InitializeSecurityContext */
	NULL,                                /* AcceptSecurityContext */
	NULL,                                /* CompleteAuthToken */
	NULL,                                /* DeleteSecurityContext */
	NULL,                                /* ApplyControlToken */
	credssp_QueryContextAttributes,      /* QueryContextAttributes */
	NULL,                                /* ImpersonateSecurityContext */
	NULL,                                /* RevertSecurityContext */
	credssp_MakeSignature,               /* MakeSignature */
	credssp_VerifySignature,             /* VerifySignature */
	NULL,                                /* FreeContextBuffer */
	NULL,                                /* QuerySecurityPackageInfo */
	NULL,                                /* Reserved3 */
	NULL,                                /* Reserved4 */
	NULL,                                /* ExportSecurityContext */
	NULL,                                /* ImportSecurityContext */
	NULL,                                /* AddCredentials */
	NULL,                                /* Reserved8 */
	NULL,                                /* QuerySecurityContextToken */
	credssp_EncryptMessage,              /* EncryptMessage */
	credssp_DecryptMessage,              /* DecryptMessage */
	NULL,                                /* SetContextAttributes */
	NULL,                                /* SetCredentialsAttributes */
};

const SecPkgInfoA CREDSSP_SecPkgInfoA = {
	0x000110733,                          /* fCapabilities */
	1,                                    /* wVersion */
	0xFFFF,                               /* wRPCID */
	0x000090A8,                           /* cbMaxToken */
	"CREDSSP",                            /* Name */
	"Microsoft CredSSP Security Provider" /* Comment */
};

static WCHAR CREDSSP_SecPkgInfoW_NameBuffer[128] = { 0 };
static WCHAR CREDSSP_SecPkgInfoW_CommentBuffer[128] = { 0 };

const SecPkgInfoW CREDSSP_SecPkgInfoW = {
	0x000110733,                      /* fCapabilities */
	1,                                /* wVersion */
	0xFFFF,                           /* wRPCID */
	0x000090A8,                       /* cbMaxToken */
	CREDSSP_SecPkgInfoW_NameBuffer,   /* Name */
	CREDSSP_SecPkgInfoW_CommentBuffer /* Comment */
};

BOOL CREDSSP_init(void)
{
	InitializeConstWCharFromUtf8(CREDSSP_SecPkgInfoA.Name, CREDSSP_SecPkgInfoW_NameBuffer,
	                             ARRAYSIZE(CREDSSP_SecPkgInfoW_NameBuffer));
	InitializeConstWCharFromUtf8(CREDSSP_SecPkgInfoA.Comment, CREDSSP_SecPkgInfoW_CommentBuffer,
	                             ARRAYSIZE(CREDSSP_SecPkgInfoW_CommentBuffer));
	return TRUE;
}
