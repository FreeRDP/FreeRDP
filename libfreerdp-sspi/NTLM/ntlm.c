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

#include <freerdp/sspi/sspi.h>

#include "ntlm.h"
#include "../sspi.h"

#include "ntlm_message.h"

char* NTLM_PACKAGE_NAME = "NTLM";

void ntlm_SetContextIdentity(NTLM_CONTEXT* context, SEC_WINNT_AUTH_IDENTITY* identity)
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
		context->identity.UserLength = identity->UserLength;

		if (identity->DomainLength > 0)
		{
			context->identity.Domain = (uint16*) xmalloc(identity->DomainLength);
			memcpy(context->identity.Domain, identity->Domain, identity->DomainLength);
			context->identity.DomainLength = identity->DomainLength;
		}
		else
		{
			context->identity.Domain = (uint16*) NULL;
			context->identity.DomainLength = 0;
		}

		context->identity.Password = (uint16*) xmalloc(identity->PasswordLength);
		memcpy(context->identity.Password, identity->Password, identity->PasswordLength);
		context->identity.PasswordLength = identity->PasswordLength;
	}
}

void ntlm_SetContextWorkstation(NTLM_CONTEXT* context, char* Workstation)
{
	size_t size;
	context->Workstation = (uint16*) freerdp_uniconv_out(context->uniconv, Workstation, &size);
	context->WorkstationLength = (uint32) size;
}

void ntlm_SetContextTargetName(NTLM_CONTEXT* context, char* TargetName)
{
	size_t size;
	context->TargetName.pvBuffer = (uint16*) freerdp_uniconv_out(context->uniconv, TargetName, &size);
	context->TargetName.cbBuffer = (uint32) size;
}

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
		context->av_pairs = (AV_PAIRS*) xzalloc(sizeof(AV_PAIRS));
	}

	return context;
}

void ntlm_ContextFree(NTLM_CONTEXT* context)
{
	if (!context)
		return;

	freerdp_uniconv_free(context->uniconv);
	crypto_rc4_free(context->SendRc4Seal);
	crypto_rc4_free(context->RecvRc4Seal);
	sspi_SecBufferFree(&context->NegotiateMessage);
	sspi_SecBufferFree(&context->ChallengeMessage);
	sspi_SecBufferFree(&context->AuthenticateMessage);
	sspi_SecBufferFree(&context->TargetInfo);
	sspi_SecBufferFree(&context->TargetName);
	sspi_SecBufferFree(&context->NtChallengeResponse);
	sspi_SecBufferFree(&context->LmChallengeResponse);
	xfree(context->identity.User);
	xfree(context->identity.Password);
	xfree(context->identity.Domain);
	xfree(context->Workstation);
	xfree(context->av_pairs->Timestamp.value);
	xfree(context->av_pairs);
	xfree(context);
}

SECURITY_STATUS ntlm_AcquireCredentialsHandle(char* pszPrincipal, char* pszPackage,
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
		sspi_SecureHandleSetUpperPointer(phCredential, (void*) NTLM_PACKAGE_NAME);

		return SEC_E_OK;
	}
	else if (fCredentialUse == SECPKG_CRED_INBOUND)
	{
		credentials = sspi_CredentialsNew();

		identity = (SEC_WINNT_AUTH_IDENTITY*) pAuthData;
		memcpy(&(credentials->identity), identity, sizeof(SEC_WINNT_AUTH_IDENTITY));

		sspi_SecureHandleSetLowerPointer(phCredential, (void*) credentials);
		sspi_SecureHandleSetUpperPointer(phCredential, (void*) NTLM_PACKAGE_NAME);

		return SEC_E_OK;
	}

	return SEC_E_OK;
}

SECURITY_STATUS ntlm_FreeCredentialsHandle(CredHandle* phCredential)
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

SECURITY_STATUS ntlm_QueryCredentialsAttributes(CredHandle* phCredential, uint32 ulAttribute, void* pBuffer)
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

/* http://msdn.microsoft.com/en-us/library/windows/desktop/aa375512/ */

SECURITY_STATUS ntlm_AcceptSecurityContext(CredHandle* phCredential, CtxtHandle* phContext,
		SecBufferDesc* pInput, uint32 fContextReq, uint32 TargetDataRep, CtxtHandle* phNewContext,
		SecBufferDesc* pOutput, uint32* pfContextAttr, SEC_TIMESTAMP* ptsTimeStamp)
{
	NTLM_CONTEXT* context;
	SECURITY_STATUS status;
	CREDENTIALS* credentials;
	SecBuffer* input_buffer;
	SecBuffer* output_buffer;

	context = sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
	{
		context = ntlm_ContextNew();
		context->server = true;

		credentials = (CREDENTIALS*) sspi_SecureHandleGetLowerPointer(phCredential);
		ntlm_SetContextIdentity(context, &credentials->identity);

		ntlm_SetContextTargetName(context, "FreeRDP");

		sspi_SecureHandleSetLowerPointer(phNewContext, context);
		sspi_SecureHandleSetUpperPointer(phNewContext, (void*) NTLM_PACKAGE_NAME);
	}

	if (context->state == NTLM_STATE_INITIAL)
	{
		context->state = NTLM_STATE_NEGOTIATE;

		if (!context)
			return SEC_E_INVALID_HANDLE;

		if (!pInput)
			return SEC_E_INVALID_TOKEN;

		if (pInput->cBuffers < 1)
			return SEC_E_INVALID_TOKEN;

		input_buffer = &pInput->pBuffers[0];

		if (input_buffer->BufferType != SECBUFFER_TOKEN)
			return SEC_E_INVALID_TOKEN;

		if (input_buffer->cbBuffer < 1)
			return SEC_E_INVALID_TOKEN;

		status = ntlm_read_NegotiateMessage(context, input_buffer);

		if (context->state == NTLM_STATE_CHALLENGE)
		{
			if (!pOutput)
				return SEC_E_INVALID_TOKEN;

			if (pOutput->cBuffers < 1)
				return SEC_E_INVALID_TOKEN;

			output_buffer = &pOutput->pBuffers[0];

			if (output_buffer->BufferType != SECBUFFER_TOKEN)
				return SEC_E_INVALID_TOKEN;

			if (output_buffer->cbBuffer < 1)
				return SEC_E_INSUFFICIENT_MEMORY;

			return ntlm_write_ChallengeMessage(context, output_buffer);
		}

		return SEC_E_OUT_OF_SEQUENCE;
	}
	else if (context->state == NTLM_STATE_AUTHENTICATE)
	{
		if (!context)
			return SEC_E_INVALID_HANDLE;

		if (!pInput)
			return SEC_E_INVALID_TOKEN;

		if (pInput->cBuffers < 1)
			return SEC_E_INVALID_TOKEN;

		input_buffer = &pInput->pBuffers[0];

		if (input_buffer->BufferType != SECBUFFER_TOKEN)
			return SEC_E_INVALID_TOKEN;

		if (input_buffer->cbBuffer < 1)
			return SEC_E_INVALID_TOKEN;

		status = ntlm_read_AuthenticateMessage(context, input_buffer);

		return status;
	}

	return SEC_E_OUT_OF_SEQUENCE;
}

SECURITY_STATUS ntlm_ImpersonateSecurityContext(CtxtHandle* phContext)
{
	return SEC_E_OK;
}

SECURITY_STATUS ntlm_InitializeSecurityContext(CredHandle* phCredential, CtxtHandle* phContext,
		char* pszTargetName, uint32 fContextReq, uint32 Reserved1, uint32 TargetDataRep,
		SecBufferDesc* pInput, uint32 Reserved2, CtxtHandle* phNewContext,
		SecBufferDesc* pOutput, uint32* pfContextAttr, SEC_TIMESTAMP* ptsExpiry)
{
	NTLM_CONTEXT* context;
	SECURITY_STATUS status;
	CREDENTIALS* credentials;
	SecBuffer* input_buffer;
	SecBuffer* output_buffer;

	context = sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
	{
		context = ntlm_ContextNew();

		credentials = (CREDENTIALS*) sspi_SecureHandleGetLowerPointer(phCredential);

		ntlm_SetContextIdentity(context, &credentials->identity);
		ntlm_SetContextWorkstation(context, "WORKSTATION");

		sspi_SecureHandleSetLowerPointer(phNewContext, context);
		sspi_SecureHandleSetUpperPointer(phNewContext, (void*) NTLM_PACKAGE_NAME);
	}

	if ((!pInput) || (context->state == NTLM_STATE_AUTHENTICATE))
	{
		if (!pOutput)
			return SEC_E_INVALID_TOKEN;

		if (pOutput->cBuffers < 1)
			return SEC_E_INVALID_TOKEN;

		output_buffer = &pOutput->pBuffers[0];

		if (output_buffer->BufferType != SECBUFFER_TOKEN)
			return SEC_E_INVALID_TOKEN;

		if (output_buffer->cbBuffer < 1)
			return SEC_E_INSUFFICIENT_MEMORY;

		if (context->state == NTLM_STATE_INITIAL)
			context->state = NTLM_STATE_NEGOTIATE;

		if (context->state == NTLM_STATE_NEGOTIATE)
			return ntlm_write_NegotiateMessage(context, output_buffer);

		return SEC_E_OUT_OF_SEQUENCE;
	}
	else
	{
		if (!context)
			return SEC_E_INVALID_HANDLE;

		if (!pInput)
			return SEC_E_INVALID_TOKEN;

		if (pInput->cBuffers < 1)
			return SEC_E_INVALID_TOKEN;

		input_buffer = &pInput->pBuffers[0];

		if (input_buffer->BufferType != SECBUFFER_TOKEN)
			return SEC_E_INVALID_TOKEN;

		if (input_buffer->cbBuffer < 1)
			return SEC_E_INVALID_TOKEN;

		if (context->state == NTLM_STATE_CHALLENGE)
		{
			status = ntlm_read_ChallengeMessage(context, input_buffer);

			if (!pOutput)
				return SEC_E_INVALID_TOKEN;

			if (pOutput->cBuffers < 1)
				return SEC_E_INVALID_TOKEN;

			output_buffer = &pOutput->pBuffers[0];

			if (output_buffer->BufferType != SECBUFFER_TOKEN)
				return SEC_E_INVALID_TOKEN;

			if (output_buffer->cbBuffer < 1)
				return SEC_E_INSUFFICIENT_MEMORY;

			if (context->state == NTLM_STATE_AUTHENTICATE)
				return ntlm_write_AuthenticateMessage(context, output_buffer);
		}

		return SEC_E_OUT_OF_SEQUENCE;
	}

	return SEC_E_OUT_OF_SEQUENCE;
}

/* http://msdn.microsoft.com/en-us/library/windows/desktop/aa375354 */

SECURITY_STATUS ntlm_DeleteSecurityContext(CtxtHandle* phContext)
{
	NTLM_CONTEXT* context;

	context = sspi_SecureHandleGetLowerPointer(phContext);
	if (!context)
		return SEC_E_INVALID_HANDLE;

	ntlm_ContextFree(context);
	return SEC_E_OK;
}

/* http://msdn.microsoft.com/en-us/library/windows/desktop/aa379337/ */

SECURITY_STATUS ntlm_QueryContextAttributes(CtxtHandle* phContext, uint32 ulAttribute, void* pBuffer)
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

SECURITY_STATUS ntlm_RevertSecurityContext(CtxtHandle* phContext)
{
	return SEC_E_OK;
}

SECURITY_STATUS ntlm_EncryptMessage(CtxtHandle* phContext, uint32 fQOP, SecBufferDesc* pMessage, uint32 MessageSeqNo)
{
	int index;
	int length;
	void* data;
	HMAC_CTX hmac;
	uint8 digest[16];
	uint8 checksum[8];
	uint8* signature;
	uint32 version = 1;
	NTLM_CONTEXT* context;
	SecBuffer* data_buffer = NULL;
	SecBuffer* signature_buffer = NULL;

	context = sspi_SecureHandleGetLowerPointer(phContext);

	for (index = 0; index < (int) pMessage->cBuffers; index++)
	{
		if (pMessage->pBuffers[index].BufferType == SECBUFFER_DATA)
			data_buffer = &pMessage->pBuffers[index];
		else if (pMessage->pBuffers[index].BufferType == SECBUFFER_PADDING)
			signature_buffer = &pMessage->pBuffers[index];
	}

	if (!data_buffer)
		return SEC_E_INVALID_TOKEN;

	if (!signature_buffer)
		return SEC_E_INVALID_TOKEN;

	/* Copy original data buffer */
	length = data_buffer->cbBuffer;
	data = xmalloc(length);
	memcpy(data, data_buffer->pvBuffer, length);

	/* Compute the HMAC-MD5 hash of ConcatenationOf(seq_num,data) using the client signing key */
	HMAC_CTX_init(&hmac);
	HMAC_Init_ex(&hmac, context->SendSigningKey, 16, EVP_md5(), NULL);
	HMAC_Update(&hmac, (void*) &(MessageSeqNo), 4);
	HMAC_Update(&hmac, data, length);
	HMAC_Final(&hmac, digest, NULL);
	HMAC_CTX_cleanup(&hmac);

	/* Encrypt message using with RC4, result overwrites original buffer */
	crypto_rc4(context->SendRc4Seal, length, data, data_buffer->pvBuffer);
	xfree(data);

#ifdef WITH_DEBUG_NTLM
	printf("Data Buffer (length = %d)\n", length);
	freerdp_hexdump(data, length);
	printf("\n");

	printf("Encrypted Data Buffer (length = %d)\n", data_buffer->cbBuffer);
	freerdp_hexdump(data_buffer->pvBuffer, data_buffer->cbBuffer);
	printf("\n");
#endif

	/* RC4-encrypt first 8 bytes of digest */
	crypto_rc4(context->SendRc4Seal, 8, digest, checksum);

	signature = (uint8*) signature_buffer->pvBuffer;

	/* Concatenate version, ciphertext and sequence number to build signature */
	memcpy(signature, (void*) &version, 4);
	memcpy(&signature[4], (void*) checksum, 8);
	memcpy(&signature[12], (void*) &(MessageSeqNo), 4);
	context->SendSeqNum++;

#ifdef WITH_DEBUG_NTLM
	printf("Signature (length = %d)\n", signature_buffer->cbBuffer);
	freerdp_hexdump(signature_buffer->pvBuffer, signature_buffer->cbBuffer);
	printf("\n");
#endif

	return SEC_E_OK;
}

SECURITY_STATUS ntlm_DecryptMessage(CtxtHandle* phContext, SecBufferDesc* pMessage, uint32 MessageSeqNo, uint32* pfQOP)
{
	int index;
	int length;
	void* data;
	HMAC_CTX hmac;
	uint8 digest[16];
	uint8 checksum[8];
	uint32 version = 1;
	NTLM_CONTEXT* context;
	uint8 expected_signature[16];
	SecBuffer* data_buffer = NULL;
	SecBuffer* signature_buffer = NULL;

	context = sspi_SecureHandleGetLowerPointer(phContext);

	for (index = 0; index < (int) pMessage->cBuffers; index++)
	{
		if (pMessage->pBuffers[index].BufferType == SECBUFFER_DATA)
			data_buffer = &pMessage->pBuffers[index];
		else if (pMessage->pBuffers[index].BufferType == SECBUFFER_PADDING)
			signature_buffer = &pMessage->pBuffers[index];
	}

	if (!data_buffer)
		return SEC_E_INVALID_TOKEN;

	if (!signature_buffer)
		return SEC_E_INVALID_TOKEN;

	/* Copy original data buffer */
	length = data_buffer->cbBuffer;
	data = xmalloc(length);
	memcpy(data, data_buffer->pvBuffer, length);

	/* Decrypt message using with RC4 */
	crypto_rc4(context->RecvRc4Seal, length, data, data_buffer->pvBuffer);

	/* Compute the HMAC-MD5 hash of ConcatenationOf(seq_num,data) using the client signing key */
	HMAC_CTX_init(&hmac);
	HMAC_Init_ex(&hmac, context->RecvSigningKey, 16, EVP_md5(), NULL);
	HMAC_Update(&hmac, (void*) &(MessageSeqNo), 4);
	HMAC_Update(&hmac, data_buffer->pvBuffer, data_buffer->cbBuffer);
	HMAC_Final(&hmac, digest, NULL);
	HMAC_CTX_cleanup(&hmac);
	xfree(data);

	/* RC4-encrypt first 8 bytes of digest */
	crypto_rc4(context->RecvRc4Seal, 8, digest, checksum);

	/* Concatenate version, ciphertext and sequence number to build signature */
	memcpy(expected_signature, (void*) &version, 4);
	memcpy(&expected_signature[4], (void*) checksum, 8);
	memcpy(&expected_signature[12], (void*) &(MessageSeqNo), 4);
	context->RecvSeqNum++;

	if (memcmp(signature_buffer->pvBuffer, expected_signature, 16) != 0)
	{
		/* signature verification failed! */
		printf("signature verification failed, something nasty is going on!\n");
		return SEC_E_MESSAGE_ALTERED;
	}

	return SEC_E_OK;
}

SECURITY_STATUS ntlm_MakeSignature(CtxtHandle* phContext, uint32 fQOP, SecBufferDesc* pMessage, uint32 MessageSeqNo)
{
	return SEC_E_OK;
}

SECURITY_STATUS ntlm_VerifySignature(CtxtHandle* phContext, SecBufferDesc* pMessage, uint32 MessageSeqNo, uint32* pfQOP)
{
	return SEC_E_OK;
}

const SecPkgInfo NTLM_SecPkgInfo =
{
	0x00082B37, /* fCapabilities */
	1, /* wVersion */
	0x000A, /* wRPCID */
	0x00000B48, /* cbMaxToken */
	"NTLM", /* Name */
	"NTLM Security Package" /* Comment */
};

const SecurityFunctionTable NTLM_SecurityFunctionTable =
{
	1, /* dwVersion */
	NULL, /* EnumerateSecurityPackages */
	NULL, /* Reserved1 */
	ntlm_QueryCredentialsAttributes, /* QueryCredentialsAttributes */
	ntlm_AcquireCredentialsHandle, /* AcquireCredentialsHandle */
	ntlm_FreeCredentialsHandle, /* FreeCredentialsHandle */
	NULL, /* Reserved2 */
	ntlm_InitializeSecurityContext, /* InitializeSecurityContext */
	ntlm_AcceptSecurityContext, /* AcceptSecurityContext */
	NULL, /* CompleteAuthToken */
	ntlm_DeleteSecurityContext, /* DeleteSecurityContext */
	NULL, /* ApplyControlToken */
	ntlm_QueryContextAttributes, /* QueryContextAttributes */
	ntlm_ImpersonateSecurityContext, /* ImpersonateSecurityContext */
	ntlm_RevertSecurityContext, /* RevertSecurityContext */
	ntlm_MakeSignature, /* MakeSignature */
	ntlm_VerifySignature, /* VerifySignature */
	NULL, /* FreeContextBuffer */
	NULL, /* QuerySecurityPackageInfo */
	NULL, /* Reserved3 */
	NULL, /* Reserved4 */
	NULL, /* ExportSecurityContext */
	NULL, /* ImportSecurityContext */
	NULL, /* AddCredentials */
	NULL, /* Reserved8 */
	NULL, /* QuerySecurityContextToken */
	ntlm_EncryptMessage, /* EncryptMessage */
	ntlm_DecryptMessage, /* DecryptMessage */
	NULL, /* SetContextAttributes */
};
