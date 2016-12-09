/**
 * WinPR: Windows Portable Runtime
 * Schannel Security Package
 *
 * Copyright 2012-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "schannel.h"

#include "../sspi.h"

char* SCHANNEL_PACKAGE_NAME = "Schannel";

SCHANNEL_CONTEXT* schannel_ContextNew()
{
	SCHANNEL_CONTEXT* context;

	context = (SCHANNEL_CONTEXT*) calloc(1, sizeof(SCHANNEL_CONTEXT));
	if (!context)
		return NULL;

	context->openssl = schannel_openssl_new();

	if (!context->openssl)
	{
		free(context);
		return NULL;
	}

	return context;
}

void schannel_ContextFree(SCHANNEL_CONTEXT* context)
{
	if (!context)
		return;

	schannel_openssl_free(context->openssl);

	free(context);
}

SCHANNEL_CREDENTIALS* schannel_CredentialsNew()
{
	SCHANNEL_CREDENTIALS* credentials;

	credentials = (SCHANNEL_CREDENTIALS*) calloc(1, sizeof(SCHANNEL_CREDENTIALS));

	return credentials;
}

void schannel_CredentialsFree(SCHANNEL_CREDENTIALS* credentials)
{
	free(credentials);
}

static ALG_ID schannel_SupportedAlgs[] =
{
	CALG_AES_128, CALG_AES_256, CALG_RC4, CALG_DES, CALG_3DES,
	CALG_MD5, CALG_SHA1, CALG_SHA_256, CALG_SHA_384, CALG_SHA_512,
	CALG_RSA_SIGN, CALG_DH_EPHEM,
	(ALG_CLASS_KEY_EXCHANGE | ALG_TYPE_RESERVED7 | 6), /* what is this? */
	CALG_DSS_SIGN, CALG_ECDSA
};

SECURITY_STATUS SEC_ENTRY schannel_QueryCredentialsAttributesW(PCredHandle phCredential, ULONG ulAttribute, void* pBuffer)
{
	if (ulAttribute == SECPKG_ATTR_SUPPORTED_ALGS)
	{
		PSecPkgCred_SupportedAlgs SupportedAlgs = (PSecPkgCred_SupportedAlgs) pBuffer;

		SupportedAlgs->cSupportedAlgs = sizeof(schannel_SupportedAlgs) / sizeof(ALG_ID);
		SupportedAlgs->palgSupportedAlgs = (ALG_ID*) schannel_SupportedAlgs;

		return SEC_E_OK;
	}
	else if (ulAttribute == SECPKG_ATTR_CIPHER_STRENGTHS)
	{
		PSecPkgCred_CipherStrengths CipherStrengths = (PSecPkgCred_CipherStrengths) pBuffer;

		CipherStrengths->dwMinimumCipherStrength = 40;
		CipherStrengths->dwMaximumCipherStrength = 256;

		return SEC_E_OK;
	}
	else if (ulAttribute == SECPKG_ATTR_SUPPORTED_PROTOCOLS)
	{
		PSecPkgCred_SupportedProtocols SupportedProtocols = (PSecPkgCred_SupportedProtocols) pBuffer;

		/* Observed SupportedProtocols: 0x208A0 */
		SupportedProtocols->grbitProtocol = (SP_PROT_CLIENTS | SP_PROT_SERVERS);

		return SEC_E_OK;
	}

	return SEC_E_UNSUPPORTED_FUNCTION;
}

SECURITY_STATUS SEC_ENTRY schannel_QueryCredentialsAttributesA(PCredHandle phCredential, ULONG ulAttribute, void* pBuffer)
{
	return schannel_QueryCredentialsAttributesW(phCredential, ulAttribute, pBuffer);
}

SECURITY_STATUS SEC_ENTRY schannel_AcquireCredentialsHandleW(SEC_WCHAR* pszPrincipal, SEC_WCHAR* pszPackage,
		ULONG fCredentialUse, void* pvLogonID, void* pAuthData, SEC_GET_KEY_FN pGetKeyFn,
		void* pvGetKeyArgument, PCredHandle phCredential, PTimeStamp ptsExpiry)
{
	SCHANNEL_CREDENTIALS* credentials;

	if (fCredentialUse == SECPKG_CRED_OUTBOUND)
	{
		SCHANNEL_CRED* cred;

		credentials = schannel_CredentialsNew();
		credentials->fCredentialUse = fCredentialUse;

		cred = (SCHANNEL_CRED*) pAuthData;

		if (cred)
		{
			CopyMemory(&credentials->cred, cred, sizeof(SCHANNEL_CRED));
		}

		sspi_SecureHandleSetLowerPointer(phCredential, (void*) credentials);
		sspi_SecureHandleSetUpperPointer(phCredential, (void*) SCHANNEL_PACKAGE_NAME);

		return SEC_E_OK;
	}
	else if (fCredentialUse == SECPKG_CRED_INBOUND)
	{
		credentials = schannel_CredentialsNew();
		credentials->fCredentialUse = fCredentialUse;

		sspi_SecureHandleSetLowerPointer(phCredential, (void*) credentials);
		sspi_SecureHandleSetUpperPointer(phCredential, (void*) SCHANNEL_PACKAGE_NAME);

		return SEC_E_OK;
	}

	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY schannel_AcquireCredentialsHandleA(SEC_CHAR* pszPrincipal, SEC_CHAR* pszPackage,
		ULONG fCredentialUse, void* pvLogonID, void* pAuthData, SEC_GET_KEY_FN pGetKeyFn,
		void* pvGetKeyArgument, PCredHandle phCredential, PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;
	SEC_WCHAR* pszPrincipalW = NULL;
	SEC_WCHAR* pszPackageW = NULL;

	ConvertToUnicode(CP_UTF8, 0, pszPrincipal, -1, &pszPrincipalW, 0);
	ConvertToUnicode(CP_UTF8, 0, pszPackage, -1, &pszPackageW, 0);

	status = schannel_AcquireCredentialsHandleW(pszPrincipalW, pszPackageW, fCredentialUse, pvLogonID,
			pAuthData, pGetKeyFn, pvGetKeyArgument, phCredential, ptsExpiry);

	free(pszPrincipalW);
	free(pszPackageW);

	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY schannel_FreeCredentialsHandle(PCredHandle phCredential)
{
	SCHANNEL_CREDENTIALS* credentials;

	if (!phCredential)
		return SEC_E_INVALID_HANDLE;

	credentials = (SCHANNEL_CREDENTIALS*) sspi_SecureHandleGetLowerPointer(phCredential);

	if (!credentials)
		return SEC_E_INVALID_HANDLE;

	schannel_CredentialsFree(credentials);

	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY schannel_InitializeSecurityContextW(PCredHandle phCredential, PCtxtHandle phContext,
		SEC_WCHAR* pszTargetName, ULONG fContextReq, ULONG Reserved1, ULONG TargetDataRep,
		PSecBufferDesc pInput, ULONG Reserved2, PCtxtHandle phNewContext,
		PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;
	SCHANNEL_CONTEXT* context;
	SCHANNEL_CREDENTIALS* credentials;

	context = sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
	{
		context = schannel_ContextNew();

		if (!context)
			return SEC_E_INSUFFICIENT_MEMORY;

		credentials = (SCHANNEL_CREDENTIALS*) sspi_SecureHandleGetLowerPointer(phCredential);

		context->server = FALSE;
		CopyMemory(&context->cred, &credentials->cred, sizeof(SCHANNEL_CRED));

		sspi_SecureHandleSetLowerPointer(phNewContext, context);
		sspi_SecureHandleSetUpperPointer(phNewContext, (void*) SCHANNEL_PACKAGE_NAME);

		schannel_openssl_client_init(context->openssl);
	}

	status = schannel_openssl_client_process_tokens(context->openssl, pInput, pOutput);

	return status;
}

SECURITY_STATUS SEC_ENTRY schannel_InitializeSecurityContextA(PCredHandle phCredential, PCtxtHandle phContext,
		SEC_CHAR* pszTargetName, ULONG fContextReq, ULONG Reserved1, ULONG TargetDataRep,
		PSecBufferDesc pInput, ULONG Reserved2, PCtxtHandle phNewContext,
		PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;
	SEC_WCHAR* pszTargetNameW = NULL;

	if (pszTargetName != NULL)
	{
		ConvertToUnicode(CP_UTF8, 0, pszTargetName, -1, &pszTargetNameW, 0);
	}

	status = schannel_InitializeSecurityContextW(phCredential, phContext, pszTargetNameW, fContextReq,
		Reserved1, TargetDataRep, pInput, Reserved2, phNewContext, pOutput, pfContextAttr, ptsExpiry);

	free(pszTargetNameW);

	return status;
}

SECURITY_STATUS SEC_ENTRY schannel_AcceptSecurityContext(PCredHandle phCredential, PCtxtHandle phContext,
		PSecBufferDesc pInput, ULONG fContextReq, ULONG TargetDataRep, PCtxtHandle phNewContext,
		PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsTimeStamp)
{
	SECURITY_STATUS status;
	SCHANNEL_CONTEXT* context;
	SCHANNEL_CREDENTIALS* credentials;

	status = SEC_E_OK;
	context = (SCHANNEL_CONTEXT*) sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
	{
		context = schannel_ContextNew();

		if (!context)
			return SEC_E_INSUFFICIENT_MEMORY;

		credentials = (SCHANNEL_CREDENTIALS*) sspi_SecureHandleGetLowerPointer(phCredential);

		context->server = TRUE;

		sspi_SecureHandleSetLowerPointer(phNewContext, context);
		sspi_SecureHandleSetUpperPointer(phNewContext, (void*) SCHANNEL_PACKAGE_NAME);

		schannel_openssl_server_init(context->openssl);
	}

	status = schannel_openssl_server_process_tokens(context->openssl, pInput, pOutput);

	return status;
}

SECURITY_STATUS SEC_ENTRY schannel_DeleteSecurityContext(PCtxtHandle phContext)
{
	SCHANNEL_CONTEXT* context;

	context = (SCHANNEL_CONTEXT*) sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
		return SEC_E_INVALID_HANDLE;

	schannel_ContextFree(context);

	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY schannel_QueryContextAttributes(PCtxtHandle phContext, ULONG ulAttribute, void* pBuffer)
{
	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (!pBuffer)
		return SEC_E_INSUFFICIENT_MEMORY;

	if (ulAttribute == SECPKG_ATTR_SIZES)
	{
		SecPkgContext_Sizes* Sizes = (SecPkgContext_Sizes*) pBuffer;

		Sizes->cbMaxToken = 0x6000;
		Sizes->cbMaxSignature = 16;
		Sizes->cbBlockSize = 0;
		Sizes->cbSecurityTrailer = 16;

		return SEC_E_OK;
	}
	else if (ulAttribute == SECPKG_ATTR_STREAM_SIZES)
	{
		SecPkgContext_StreamSizes* StreamSizes = (SecPkgContext_StreamSizes*) pBuffer;

		StreamSizes->cbHeader = 5;
		StreamSizes->cbTrailer = 36;
		StreamSizes->cbMaximumMessage = 0x4000;
		StreamSizes->cBuffers = 4;
		StreamSizes->cbBlockSize = 16;

		return SEC_E_OK;
	}

	return SEC_E_UNSUPPORTED_FUNCTION;
}

SECURITY_STATUS SEC_ENTRY schannel_MakeSignature(PCtxtHandle phContext, ULONG fQOP, PSecBufferDesc pMessage, ULONG MessageSeqNo)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY schannel_VerifySignature(PCtxtHandle phContext, PSecBufferDesc pMessage, ULONG MessageSeqNo, ULONG* pfQOP)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY schannel_EncryptMessage(PCtxtHandle phContext, ULONG fQOP, PSecBufferDesc pMessage, ULONG MessageSeqNo)
{
	SECURITY_STATUS status;
	SCHANNEL_CONTEXT* context;

	context = (SCHANNEL_CONTEXT*) sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
		return SEC_E_INVALID_HANDLE;

	status = schannel_openssl_encrypt_message(context->openssl, pMessage);

	return status;
}

SECURITY_STATUS SEC_ENTRY schannel_DecryptMessage(PCtxtHandle phContext, PSecBufferDesc pMessage, ULONG MessageSeqNo, ULONG* pfQOP)
{
	SECURITY_STATUS status;
	SCHANNEL_CONTEXT* context;

	context = (SCHANNEL_CONTEXT*) sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
		return SEC_E_INVALID_HANDLE;

	status = schannel_openssl_decrypt_message(context->openssl, pMessage);

	return status;
}

const SecurityFunctionTableA SCHANNEL_SecurityFunctionTableA =
{
	1, /* dwVersion */
	NULL, /* EnumerateSecurityPackages */
	schannel_QueryCredentialsAttributesA, /* QueryCredentialsAttributes */
	schannel_AcquireCredentialsHandleA, /* AcquireCredentialsHandle */
	schannel_FreeCredentialsHandle, /* FreeCredentialsHandle */
	NULL, /* Reserved2 */
	schannel_InitializeSecurityContextA, /* InitializeSecurityContext */
	schannel_AcceptSecurityContext, /* AcceptSecurityContext */
	NULL, /* CompleteAuthToken */
	schannel_DeleteSecurityContext, /* DeleteSecurityContext */
	NULL, /* ApplyControlToken */
	schannel_QueryContextAttributes, /* QueryContextAttributes */
	NULL, /* ImpersonateSecurityContext */
	NULL, /* RevertSecurityContext */
	schannel_MakeSignature, /* MakeSignature */
	schannel_VerifySignature, /* VerifySignature */
	NULL, /* FreeContextBuffer */
	NULL, /* QuerySecurityPackageInfo */
	NULL, /* Reserved3 */
	NULL, /* Reserved4 */
	NULL, /* ExportSecurityContext */
	NULL, /* ImportSecurityContext */
	NULL, /* AddCredentials */
	NULL, /* Reserved8 */
	NULL, /* QuerySecurityContextToken */
	schannel_EncryptMessage, /* EncryptMessage */
	schannel_DecryptMessage, /* DecryptMessage */
	NULL, /* SetContextAttributes */
};

const SecurityFunctionTableW SCHANNEL_SecurityFunctionTableW =
{
	1, /* dwVersion */
	NULL, /* EnumerateSecurityPackages */
	schannel_QueryCredentialsAttributesW, /* QueryCredentialsAttributes */
	schannel_AcquireCredentialsHandleW, /* AcquireCredentialsHandle */
	schannel_FreeCredentialsHandle, /* FreeCredentialsHandle */
	NULL, /* Reserved2 */
	schannel_InitializeSecurityContextW, /* InitializeSecurityContext */
	schannel_AcceptSecurityContext, /* AcceptSecurityContext */
	NULL, /* CompleteAuthToken */
	schannel_DeleteSecurityContext, /* DeleteSecurityContext */
	NULL, /* ApplyControlToken */
	schannel_QueryContextAttributes, /* QueryContextAttributes */
	NULL, /* ImpersonateSecurityContext */
	NULL, /* RevertSecurityContext */
	schannel_MakeSignature, /* MakeSignature */
	schannel_VerifySignature, /* VerifySignature */
	NULL, /* FreeContextBuffer */
	NULL, /* QuerySecurityPackageInfo */
	NULL, /* Reserved3 */
	NULL, /* Reserved4 */
	NULL, /* ExportSecurityContext */
	NULL, /* ImportSecurityContext */
	NULL, /* AddCredentials */
	NULL, /* Reserved8 */
	NULL, /* QuerySecurityContextToken */
	schannel_EncryptMessage, /* EncryptMessage */
	schannel_DecryptMessage, /* DecryptMessage */
	NULL, /* SetContextAttributes */
};

const SecPkgInfoA SCHANNEL_SecPkgInfoA =
{
	0x000107B3, /* fCapabilities */
	1, /* wVersion */
	0x000E, /* wRPCID */
	SCHANNEL_CB_MAX_TOKEN, /* cbMaxToken */
	"Schannel", /* Name */
	"Schannel Security Package" /* Comment */
};

WCHAR SCHANNEL_SecPkgInfoW_Name[] = { 'S','c','h','a','n','n','e','l','\0' };

WCHAR SCHANNEL_SecPkgInfoW_Comment[] =
{
	'S','c','h','a','n','n','e','l',' ',
	'S','e','c','u','r','i','t','y',' ',
	'P','a','c','k','a','g','e','\0'
};

const SecPkgInfoW SCHANNEL_SecPkgInfoW =
{
	0x000107B3, /* fCapabilities */
	1, /* wVersion */
	0x000E, /* wRPCID */
	SCHANNEL_CB_MAX_TOKEN, /* cbMaxToken */
	SCHANNEL_SecPkgInfoW_Name, /* Name */
	SCHANNEL_SecPkgInfoW_Comment /* Comment */
};
