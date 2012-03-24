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

const SecPkgInfoA NEGOTIATE_SecPkgInfoA =
{
	0x00083BB3, /* fCapabilities */
	1, /* wVersion */
	0x0009, /* wRPCID */
	0x00002FE0, /* cbMaxToken */
	"Negotiate", /* Name */
	"Microsoft Package Negotiator" /* Comment */
};

const SecPkgInfoW NEGOTIATE_SecPkgInfoW =
{
	0x00083BB3, /* fCapabilities */
	1, /* wVersion */
	0x0009, /* wRPCID */
	0x00002FE0, /* cbMaxToken */
	L"Negotiate", /* Name */
	L"Microsoft Package Negotiator" /* Comment */
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

SECURITY_STATUS SEC_ENTRY negotiate_InitializeSecurityContextW(PCredHandle phCredential, PCtxtHandle phContext,
		SEC_WCHAR* pszTargetName, uint32 fContextReq, uint32 Reserved1, uint32 TargetDataRep,
		PSecBufferDesc pInput, uint32 Reserved2, PCtxtHandle phNewContext,
		PSecBufferDesc pOutput, uint32* pfContextAttr, PTimeStamp ptsExpiry)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY negotiate_InitializeSecurityContextA(PCredHandle phCredential, PCtxtHandle phContext,
		SEC_CHAR* pszTargetName, uint32 fContextReq, uint32 Reserved1, uint32 TargetDataRep,
		PSecBufferDesc pInput, uint32 Reserved2, PCtxtHandle phNewContext,
		PSecBufferDesc pOutput, uint32* pfContextAttr, PTimeStamp ptsExpiry)
{
	NEGOTIATE_CONTEXT* context;
	//SECURITY_STATUS status;
	CREDENTIALS* credentials;
	//PSecBuffer input_SecBuffer;
	PSecBuffer output_SecBuffer;
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

SECURITY_STATUS SEC_ENTRY negotiate_QueryContextAttributes(PCtxtHandle phContext, uint32 ulAttribute, void* pBuffer)
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

SECURITY_STATUS SEC_ENTRY negotiate_AcquireCredentialsHandleW(SEC_WCHAR* pszPrincipal, SEC_WCHAR* pszPackage,
		uint32 fCredentialUse, void* pvLogonID, void* pAuthData, void* pGetKeyFn,
		void* pvGetKeyArgument, PCredHandle phCredential, PTimeStamp ptsExpiry)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY negotiate_AcquireCredentialsHandleA(SEC_CHAR* pszPrincipal, SEC_CHAR* pszPackage,
		uint32 fCredentialUse, void* pvLogonID, void* pAuthData, void* pGetKeyFn,
		void* pvGetKeyArgument, PCredHandle phCredential, PTimeStamp ptsExpiry)
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

SECURITY_STATUS SEC_ENTRY negotiate_QueryCredentialsAttributesW(PCredHandle phCredential, uint32 ulAttribute, void* pBuffer)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY negotiate_QueryCredentialsAttributesA(PCredHandle phCredential, uint32 ulAttribute, void* pBuffer)
{
	if (ulAttribute == SECPKG_CRED_ATTR_NAMES)
	{
		CREDENTIALS* credentials;
		//SecPkgCredentials_Names* credential_names = (SecPkgCredentials_Names*) pBuffer;

		credentials = (CREDENTIALS*) sspi_SecureHandleGetLowerPointer(phCredential);

		//if (credentials->identity.Flags == SEC_WINNT_AUTH_IDENTITY_ANSI)
		//	credential_names->sUserName = xstrdup((char*) credentials->identity.User);

		return SEC_E_OK;
	}

	return SEC_E_UNSUPPORTED_FUNCTION;
}

SECURITY_STATUS SEC_ENTRY negotiate_FreeCredentialsHandle(PCredHandle phCredential)
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

SECURITY_STATUS SEC_ENTRY negotiate_EncryptMessage(PCtxtHandle phContext, uint32 fQOP, PSecBufferDesc pMessage, uint32 MessageSeqNo)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY negotiate_DecryptMessage(PCtxtHandle phContext, PSecBufferDesc pMessage, uint32 MessageSeqNo, uint32* pfQOP)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY negotiate_MakeSignature(PCtxtHandle phContext, uint32 fQOP, PSecBufferDesc pMessage, uint32 MessageSeqNo)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY negotiate_VerifySignature(PCtxtHandle phContext, PSecBufferDesc pMessage, uint32 MessageSeqNo, uint32* pfQOP)
{
	return SEC_E_OK;
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

const SecurityFunctionTableW NEGOTIATE_SecurityFunctionTableW =
{
	1, /* dwVersion */
	NULL, /* EnumerateSecurityPackages */
	negotiate_QueryCredentialsAttributesW, /* QueryCredentialsAttributes */
	negotiate_AcquireCredentialsHandleW, /* AcquireCredentialsHandle */
	negotiate_FreeCredentialsHandle, /* FreeCredentialsHandle */
	NULL, /* Reserved2 */
	negotiate_InitializeSecurityContextW, /* InitializeSecurityContext */
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
