/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Negotiate Security Package
 *
 * Copyright 2011-2012 Jiten Pathy 
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

#include <freerdp/sspi/sspi.h>
#include <freerdp/utils/memory.h>

#include "negotiate.h"

#include "../sspi.h"

char* NEGOTIATE_PACKAGE_NAME = "Negotiate";

const SecPkgInfo NEGOTIATE_SecPkgInfo =
{
	0x00083BB3, /* fCapabilities */
	1, /* wVersion */
	0x0009, /* wRPCID */
	0x00002FE0, /* cbMaxToken */
	"Negotiate", /* Name */
	"Microsoft Package Negotiator" /* Comment */
};

void negotiate_SetContextIdentity(NEGOTIATE_CONTEXT* context, SEC_WINNT_AUTH_IDENTITY* identity)
{
	size_t size;
	context->identity.Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;

	if (identity->Flags == SEC_WINNT_AUTH_IDENTITY_ANSI)
	{
		context->identity.User = (uint16*) freerdp_uniconv_out(context->uniconv, (char*) identity->User, &size);
		context->identity.UserLength = (uint32) size;

		if (identity->DomainLength > 0)
		{
			context->identity.Domain = (uint16*) freerdp_uniconv_out(context->uniconv, (char*) identity->Domain, &size);
			context->identity.DomainLength = (uint32) size;
		}
		else
		{
			context->identity.Domain = (uint16*) NULL;
			context->identity.DomainLength = 0;
		}

		context->identity.Password = (uint16*) freerdp_uniconv_out(context->uniconv, (char*) identity->Password, &size);
		context->identity.PasswordLength = (uint32) size;
	}
	else
	{
		context->identity.User = (uint16*) xmalloc(identity->UserLength);
		memcpy(context->identity.User, identity->User, identity->UserLength);

		if (identity->DomainLength > 0)
		{
			context->identity.Domain = (uint16*) xmalloc(identity->DomainLength);
			memcpy(context->identity.Domain, identity->Domain, identity->DomainLength);
		}
		else
		{
			context->identity.Domain = (uint16*) NULL;
			context->identity.DomainLength = 0;
		}

		context->identity.Password = (uint16*) xmalloc(identity->PasswordLength);
		memcpy(context->identity.Password, identity->Password, identity->PasswordLength);
	}
}

SECURITY_STATUS negotiate_InitializeSecurityContext(CredHandle* phCredential, CtxtHandle* phContext,
		char* pszTargetName, uint32 fContextReq, uint32 Reserved1, uint32 TargetDataRep,
		SecBufferDesc* pInput, uint32 Reserved2, CtxtHandle* phNewContext,
		SecBufferDesc* pOutput, uint32* pfContextAttr, SEC_TIMESTAMP* ptsExpiry)
{
	NEGOTIATE_CONTEXT* context;
	//SECURITY_STATUS status;
	CREDENTIALS* credentials;
	//SecBuffer* input_SecBuffer;
	SecBuffer* output_SecBuffer;
	//KrbTGTREQ krb_tgtreq;

	context = sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
	{
		context = negotiate_ContextNew();

		credentials = (CREDENTIALS*)     sspi_SecureHandleGetLowerPointer(phCredential);
		negotiate_SetContextIdentity(context, &credentials->identity);

		sspi_SecureHandleSetLowerPointer(phNewContext, context);
		sspi_SecureHandleSetUpperPointer(phNewContext, (void*) NEGOTIATE_PACKAGE_NAME);
	}

	if((!pInput) && (context->state == NEGOTIATE_STATE_INITIAL))
	{
		if (!pOutput)
			return SEC_E_INVALID_TOKEN;

		if (pOutput->cBuffers < 1)
			return SEC_E_INVALID_TOKEN;

		output_SecBuffer = &pOutput->pBuffers[0];

		if (output_SecBuffer->cbBuffer < 1)
			return SEC_E_INSUFFICIENT_MEMORY;
	}	

	return SEC_E_OK;

}

NEGOTIATE_CONTEXT* negotiate_ContextNew()
{
	NEGOTIATE_CONTEXT* context;

	context = xnew(NEGOTIATE_CONTEXT);

	if (context != NULL)
	{
		context->NegotiateFlags = 0;
		context->state = NEGOTIATE_STATE_INITIAL;
		context->uniconv = freerdp_uniconv_new();
	}

	return context;
}

void negotiate_ContextFree(NEGOTIATE_CONTEXT* context)
{
	if (!context)
		return;

	xfree(context);
}

SECURITY_STATUS negotiate_QueryContextAttributes(CtxtHandle* phContext, uint32 ulAttribute, void* pBuffer)
{
	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (!pBuffer)
		return SEC_E_INSUFFICIENT_MEMORY;

	if (ulAttribute == SECPKG_ATTR_SIZES)
	{
		SecPkgContext_Sizes* ContextSizes = (SecPkgContext_Sizes*) pBuffer;

		ContextSizes->cbMaxToken = 2010;
		ContextSizes->cbMaxSignature = 16;
		ContextSizes->cbBlockSize = 0;
		ContextSizes->cbSecurityTrailer = 16;

		return SEC_E_OK;
	}

	return SEC_E_UNSUPPORTED_FUNCTION;
}

SECURITY_STATUS negotiate_AcquireCredentialsHandle(char* pszPrincipal, char* pszPackage,
		uint32 fCredentialUse, void* pvLogonID, void* pAuthData, void* pGetKeyFn,
		void* pvGetKeyArgument, CredHandle* phCredential, SEC_TIMESTAMP* ptsExpiry)
{
	CREDENTIALS* credentials;
	SEC_WINNT_AUTH_IDENTITY* identity;

	if (fCredentialUse == SECPKG_CRED_OUTBOUND)
	{
		credentials = sspi_CredentialsNew();
		identity = (SEC_WINNT_AUTH_IDENTITY*) pAuthData;

		memcpy(&(credentials->identity), identity, sizeof(SEC_WINNT_AUTH_IDENTITY));

		sspi_SecureHandleSetLowerPointer(phCredential, (void*) credentials);
		sspi_SecureHandleSetUpperPointer(phCredential, (void*) NEGOTIATE_PACKAGE_NAME);

		return SEC_E_OK;
	}

	return SEC_E_OK;
}

SECURITY_STATUS negotiate_QueryCredentialsAttributes(CredHandle* phCredential, uint32 ulAttribute, void* pBuffer)
{
	if (ulAttribute == SECPKG_CRED_ATTR_NAMES)
	{
		CREDENTIALS* credentials;
		SecPkgCredentials_Names* credential_names = (SecPkgCredentials_Names*) pBuffer;

		credentials = (CREDENTIALS*) sspi_SecureHandleGetLowerPointer(phCredential);

		if (credentials->identity.Flags == SEC_WINNT_AUTH_IDENTITY_ANSI)
			credential_names->sUserName = xstrdup((char*) credentials->identity.User);

		return SEC_E_OK;
	}

	return SEC_E_UNSUPPORTED_FUNCTION;
}

SECURITY_STATUS negotiate_FreeCredentialsHandle(CredHandle* phCredential)
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

SECURITY_STATUS negotiate_EncryptMessage(CtxtHandle* phContext, uint32 fQOP, SecBufferDesc* pMessage, uint32 MessageSeqNo)
{
	return SEC_E_OK;
}

SECURITY_STATUS negotiate_DecryptMessage(CtxtHandle* phContext, SecBufferDesc* pMessage, uint32 MessageSeqNo, uint32* pfQOP)
{
	return SEC_E_OK;
}

SECURITY_STATUS negotiate_MakeSignature(CtxtHandle* phContext, uint32 fQOP, SecBufferDesc* pMessage, uint32 MessageSeqNo)
{
	return SEC_E_OK;
}

SECURITY_STATUS negotiate_VerifySignature(CtxtHandle* phContext, SecBufferDesc* pMessage, uint32 MessageSeqNo, uint32* pfQOP)
{
	return SEC_E_OK;
}

const SecurityFunctionTable NEGOTIATE_SecurityFunctionTable =
{
	1, /* dwVersion */
	NULL, /* EnumerateSecurityPackages */
	NULL, /* Reserved1 */
	negotiate_QueryCredentialsAttributes, /* QueryCredentialsAttributes */
	negotiate_AcquireCredentialsHandle, /* AcquireCredentialsHandle */
	negotiate_FreeCredentialsHandle, /* FreeCredentialsHandle */
	NULL, /* Reserved2 */
	negotiate_InitializeSecurityContext, /* InitializeSecurityContext */
	NULL, /* AcceptSecurityContext */
	NULL, /* CompleteAuthToken */
	NULL, /* DeleteSecurityContext */
	NULL, /* ApplyControlToken */
	negotiate_QueryContextAttributes, /* QueryContextAttributes */
	NULL, /* ImpersonateSecurityContext */
	NULL, /* RevertSecurityContext */
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
	NULL, /* SetContextAttributes */
};
