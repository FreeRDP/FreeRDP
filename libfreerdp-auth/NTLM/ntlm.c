/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * NTLM Security Package
 *
 * Copyright 2011-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <time.h>
#include <openssl/des.h>
#include <openssl/md4.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/engine.h>
#include <freerdp/utils/memory.h>

#include <freerdp/auth/sspi.h>
#include <freerdp/auth/credssp.h>

#include "ntlm.h"
#include "../sspi.h"

#include "ntlm_message.h"

char* NTLM_PACKAGE_NAME = "NTLM";

NTLM_CONTEXT* ntlm_ContextNew()
{
	NTLM_CONTEXT* context;

	context = xnew(NTLM_CONTEXT);

	if (context != NULL)
	{
		context->ntlm_v2 = false;
		context->NegotiateFlags = 0;
		context->state = NTLM_STATE_INITIAL;
		context->uniconv = freerdp_uniconv_new();
	}

	return context;
}

void ntlm_ContextFree(NTLM_CONTEXT* context)
{
	if (!context)
		return;

	xfree(context);
}

SECURITY_STATUS ntlm_AcquireCredentialsHandle(char* pszPrincipal, char* pszPackage,
		uint32 fCredentialUse, void* pvLogonID, void* pAuthData, void* pGetKeyFn,
		void* pvGetKeyArgument, CRED_HANDLE* phCredential, SEC_TIMESTAMP* ptsExpiry)
{
	CREDENTIALS* credentials;
	SEC_AUTH_IDENTITY* identity;

	if (fCredentialUse == SECPKG_CRED_OUTBOUND)
	{
		credentials = sspi_CredentialsNew();
		identity = (SEC_AUTH_IDENTITY*) pAuthData;

		memcpy(&(credentials->identity), identity, sizeof(SEC_AUTH_IDENTITY));

		sspi_SecureHandleSetLowerPointer(phCredential, (void*) credentials);
		sspi_SecureHandleSetUpperPointer(phCredential, (void*) NTLM_PACKAGE_NAME);

		return SEC_E_OK;
	}

	return SEC_E_OK;
}

SECURITY_STATUS ntlm_FreeCredentialsHandle(CRED_HANDLE* phCredential)
{
	CREDENTIALS* credentials;

	if (!phCredential)
		return SEC_E_INVALID_HANDLE;

	credentials = (CREDENTIALS*) sspi_SecureHandleGetLowerPointer(phCredential);

	if (!credentials)
		return SEC_E_INVALID_HANDLE;

	sspi_CredentialsFree(credentials);

	return SEC_E_OK;
}

SECURITY_STATUS ntlm_QueryCredentialsAttributes(CRED_HANDLE* phCredential, uint32 ulAttribute, void* pBuffer)
{
	if (ulAttribute == SECPKG_CRED_ATTR_NAMES)
	{
		CREDENTIALS* credentials;
		SEC_PKG_CREDENTIALS_NAMES* credential_names = (SEC_PKG_CREDENTIALS_NAMES*) pBuffer;

		credentials = (CREDENTIALS*) sspi_SecureHandleGetLowerPointer(phCredential);

		if (credentials->identity.Flags == SEC_AUTH_IDENTITY_ANSI)
			credential_names->sUserName = xstrdup((char*) credentials->identity.User);

		return SEC_E_OK;
	}

	return SEC_E_UNSUPPORTED_FUNCTION;
}

/* http://msdn.microsoft.com/en-us/library/windows/desktop/aa375512/ */

SECURITY_STATUS ntlm_InitializeSecurityContext(CRED_HANDLE* phCredential, CTXT_HANDLE* phContext,
		char* pszTargetName, uint32 fContextReq, uint32 Reserved1, uint32 TargetDataRep,
		SEC_BUFFER_DESC* pInput, uint32 Reserved2, CTXT_HANDLE* phNewContext,
		SEC_BUFFER_DESC* pOutput, uint32* pfContextAttr, SEC_TIMESTAMP* ptsExpiry)
{
	NTLM_CONTEXT* context;
	SEC_BUFFER* sec_buffer;

	if (!pInput)
	{
		context = ntlm_ContextNew();

		if (!pOutput)
			return SEC_E_INVALID_TOKEN;

		if (pOutput->cBuffers < 1)
			return SEC_E_INVALID_TOKEN;

		sec_buffer = &pOutput->pBuffers[0];

		if (sec_buffer->BufferType != SECBUFFER_TOKEN)
			return SEC_E_INVALID_TOKEN;

		if (sec_buffer->cbBuffer < 1)
			return SEC_E_INSUFFICIENT_MEMORY;

		sspi_SecureHandleSetLowerPointer(phNewContext, context);

		return ntlm_write_NegotiateMessage(context, sec_buffer);
	}
	else
	{
		context = (NTLM_CONTEXT*) sspi_SecureHandleGetLowerPointer(phContext);

		if (!context)
			return SEC_E_INVALID_HANDLE;

		if (!pInput)
			return SEC_E_INVALID_TOKEN;

		if (pInput->cBuffers < 1)
			return SEC_E_INVALID_TOKEN;

		sec_buffer = &pInput->pBuffers[0];

		if (sec_buffer->BufferType != SECBUFFER_TOKEN)
			return SEC_E_INVALID_TOKEN;

		if (sec_buffer->cbBuffer < 1)
			return SEC_E_INVALID_TOKEN;
	}

	return SEC_E_OK;
}

/* http://msdn.microsoft.com/en-us/library/windows/desktop/aa379337/ */

SECURITY_STATUS ntlm_QueryContextAttributes(CTXT_HANDLE* phContext, uint32 ulAttribute, void* pBuffer)
{
	return SEC_E_OK;
}

const SEC_PKG_INFO NTLM_SEC_PKG_INFO =
{
	0x00082B37, /* fCapabilities */
	1, /* wVersion */
	0x000A, /* wRPCID */
	0x00000B48, /* cbMaxToken */
	"NTLM", /* Name */
	"NTLM Security Package" /* Comment */
};

const SECURITY_FUNCTION_TABLE NTLM_SECURITY_FUNCTION_TABLE =
{
	1, /* dwVersion */
	NULL, /* EnumerateSecurityPackages */
	NULL, /* Reserved1 */
	ntlm_QueryCredentialsAttributes, /* QueryCredentialsAttributes */
	ntlm_AcquireCredentialsHandle, /* AcquireCredentialsHandle */
	ntlm_FreeCredentialsHandle, /* FreeCredentialsHandle */
	NULL, /* Reserved2 */
	ntlm_InitializeSecurityContext, /* InitializeSecurityContext */
	NULL, /* AcceptSecurityContext */
	NULL, /* CompleteAuthToken */
	NULL, /* DeleteSecurityContext */
	NULL, /* ApplyControlToken */
	ntlm_QueryContextAttributes, /* QueryContextAttributes */
	NULL, /* ImpersonateSecurityContext */
	NULL, /* RevertSecurityContext */
	NULL, /* MakeSignature */
	NULL, /* VerifySignature */
	NULL, /* FreeContextBuffer */
	NULL, /* QuerySecurityPackageInfo */
	NULL, /* Reserved3 */
	NULL, /* Reserved4 */
	NULL, /* ExportSecurityContext */
	NULL, /* ImportSecurityContext */
	NULL, /* AddCredentials */
	NULL, /* Reserved8 */
	NULL, /* QuerySecurityContextToken */
	NULL, /* EncryptMessage */
	NULL, /* DecryptMessage */
	NULL, /* SetContextAttributes */
};
