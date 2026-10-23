/**
 * WinPR: Windows Portable Runtime
 * Credential Security Support Provider (CredSSP) client
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
#include <winpr/assert.h>
#include <winpr/sspi.h>
#include <winpr/credssp.h>
#include <winpr/custom-crypto.h>
#include <winpr/endian.h>

#ifdef WITH_OPENSSL
#include <openssl/x509.h>
#endif

#include "credssp.h"

#include "../sspi.h"
#include "../../log.h"

#define TAG WINPR_TAG("sspi.CredSSP")

#define CREDSSP_VERSION 6
#define CREDSSP_NONCE_LENGTH 32
/* Floor for the inner Negotiate output token: a Kerberos AP-REQ with a PAC can exceed the
 * Negotiate package's advertised cbMaxToken, so never allocate less than this. */
#define CREDSSP_MIN_NEGO_TOKEN 0x10000

static const WCHAR NEGOTIATE_NAMEW[] = { 'N', 'e', 'g', 'o', 't', 'i', 'a', 't', 'e', '\0' };

/* Negotiate auth package list for Remote Credential Guard: Kerberos only (no U2U, no NTLM). */
static const WCHAR CREDSSP_REDIRECT_PACKAGESW[] = { 'k', 'e', 'r', 'b', 'e', 'r', 'o',
	                                                's', ',', '!', 'u', '2', 'u', ',',
	                                                '!', 'n', 't', 'l', 'm', '\0' };

static PSecurityFunctionTableW credssp_winpr_table(void)
{
	return InitSecurityInterfaceExW(SSPI_INTERFACE_WINPR);
}

/* Wrap plaintext with the Negotiate context: a SECBUFFER_TOKEN signature followed by the
 * encrypted data, laid out consecutively as the CredSSP TSRequest fields expect. */
static BOOL credssp_encrypt(CREDSSP_CONTEXT* context, const SecBuffer* plaintext, SecBuffer* out,
                            ULONG sequence)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->nego && context->nego->EncryptMessage);

	const ULONG trailer = context->sizes.cbSecurityTrailer;
	BYTE* buf = (BYTE*)calloc(1, 1ull * plaintext->cbBuffer + trailer);
	if (!buf)
		return FALSE;

	SecBuffer buffers[2] = { 0 };
	buffers[0].BufferType = SECBUFFER_TOKEN;
	buffers[0].cbBuffer = trailer;
	buffers[0].pvBuffer = buf;
	buffers[1].BufferType = SECBUFFER_DATA;
	buffers[1].cbBuffer = plaintext->cbBuffer;
	buffers[1].pvBuffer = buf + trailer;
	CopyMemory(buffers[1].pvBuffer, plaintext->pvBuffer, plaintext->cbBuffer);

	SecBufferDesc desc = { SECBUFFER_VERSION, 2, buffers };
	const SECURITY_STATUS status =
	    context->nego->EncryptMessage(&context->negoCtx, 0, &desc, sequence);
	if (status != SEC_E_OK)
	{
		free(buf);
		return FALSE;
	}

	if (buffers[0].cbBuffer < trailer)
	{
		MoveMemory(buf + buffers[0].cbBuffer, buffers[1].pvBuffer, buffers[1].cbBuffer);
		context->sizes.cbSecurityTrailer = buffers[0].cbBuffer;
	}

	sspi_SecBufferFree(out);
	out->BufferType = SECBUFFER_TOKEN;
	out->pvBuffer = buf;
	out->cbBuffer = buffers[0].cbBuffer + buffers[1].cbBuffer;
	return TRUE;
}

static BOOL credssp_decrypt(CREDSSP_CONTEXT* context, const SecBuffer* ciphertext, SecBuffer* out,
                            ULONG sequence)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->nego && context->nego->DecryptMessage);

	if (ciphertext->cbBuffer < context->sizes.cbSecurityTrailer)
		return FALSE;

	BYTE* buf = (BYTE*)malloc(ciphertext->cbBuffer);
	if (!buf)
		return FALSE;
	CopyMemory(buf, ciphertext->pvBuffer, ciphertext->cbBuffer);

	const ULONG trailer = context->sizes.cbSecurityTrailer;
	SecBuffer buffers[2] = { 0 };
	buffers[0].BufferType = SECBUFFER_TOKEN;
	buffers[0].cbBuffer = trailer;
	buffers[0].pvBuffer = buf;
	buffers[1].BufferType = SECBUFFER_DATA;
	buffers[1].cbBuffer = ciphertext->cbBuffer - trailer;
	buffers[1].pvBuffer = buf + trailer;

	SecBufferDesc desc = { SECBUFFER_VERSION, 2, buffers };
	ULONG qop = 0;
	const SECURITY_STATUS status =
	    context->nego->DecryptMessage(&context->negoCtx, &desc, sequence, &qop);
	if (status != SEC_E_OK)
	{
		free(buf);
		return FALSE;
	}

	sspi_SecBufferFree(out);
	if (!sspi_SecBufferAlloc(out, buffers[1].cbBuffer))
	{
		free(buf);
		return FALSE;
	}
	CopyMemory(out->pvBuffer, buffers[1].pvBuffer, buffers[1].cbBuffer);
	free(buf);
	return TRUE;
}

CREDSSP_CONTEXT* credssp_ContextNew(void)
{
	CREDSSP_CONTEXT* context = (CREDSSP_CONTEXT*)calloc(1, sizeof(CREDSSP_CONTEXT));
	if (!context)
		return nullptr;
	context->version = CREDSSP_VERSION;
	context->state = CREDSSP_STATE_NEGO;
	return context;
}

void credssp_ContextFree(CREDSSP_CONTEXT* context)
{
	if (!context)
		return;
	if (context->nego && context->haveNegoCtx)
		(void)context->nego->DeleteSecurityContext(&context->negoCtx);
	free(context->targetName);
	sspi_SecBufferFree(&context->publicKey);
	sspi_SecBufferFree(&context->clientNonce);
	free(context);
}

static SECURITY_STATUS SEC_ENTRY credssp_AcquireCredentialsHandleW(
    WINPR_ATTR_UNUSED SEC_WCHAR* pszPrincipal, WINPR_ATTR_UNUSED SEC_WCHAR* pszPackage,
    ULONG fCredentialUse, void* pvLogonID, void* pAuthData, SEC_GET_KEY_FN pGetKeyFn,
    void* pvGetKeyArgument, PCredHandle phCredential, PTimeStamp ptsExpiry)
{
	if (fCredentialUse != SECPKG_CRED_OUTBOUND)
		return SEC_E_UNSUPPORTED_FUNCTION;

	PSecurityFunctionTableW table = credssp_winpr_table();
	if (!table || !table->AcquireCredentialsHandleW)
		return SEC_E_SECPKG_NOT_FOUND;

	/* pAuthData is a CREDSSP_CRED_EX; the inner SPNEGO credential is the identity. */
	SEC_WINNT_AUTH_IDENTITY_W* identity = nullptr;
	DWORD flags = 0;
	if (pAuthData)
	{
		CREDSSP_CRED_EX* credEx = (CREDSSP_CRED_EX*)pAuthData;
		flags = credEx->Flags;
		identity = (SEC_WINNT_AUTH_IDENTITY_W*)credEx->Cred.pSpnegoCred;
	}

	CREDSSP_CREDENTIALS* creds = (CREDSSP_CREDENTIALS*)calloc(1, sizeof(CREDSSP_CREDENTIALS));
	if (!creds)
		return SEC_E_INSUFFICIENT_MEMORY;

	creds->nego = table;
	creds->flags = flags;
	if (identity)
	{
		creds->identity = *identity;
		creds->haveIdentity = TRUE;
	}

	/* Remote Credential Guard is Kerberos-only. Restrict the inner Negotiate to Kerberos so it
	 * does not optimistically offer U2U (which the server rejects, forcing a mechListMIC exchange
	 * that desynchronises the pubKeyAuth sequence number) or fall back to NTLM. The restriction
	 * travels as an auth package list on an extended identity, which also carries the explicit
	 * credentials when present and otherwise leaves the user fields empty for ccache SSO. */
	void* negoAuthData = identity;
	SEC_WINNT_AUTH_IDENTITY_EXW redirectIdentity = { 0 };
	if (flags & CREDSSP_FLAG_REDIRECT)
	{
		redirectIdentity.Version = SEC_WINNT_AUTH_IDENTITY_VERSION;
		redirectIdentity.Length = sizeof(redirectIdentity);
		if (identity)
		{
			redirectIdentity.User = identity->User;
			redirectIdentity.UserLength = identity->UserLength;
			redirectIdentity.Domain = identity->Domain;
			redirectIdentity.DomainLength = identity->DomainLength;
			redirectIdentity.Password = identity->Password;
			redirectIdentity.PasswordLength = identity->PasswordLength;
		}
		redirectIdentity.Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;
		redirectIdentity.PackageList = (UINT16*)CREDSSP_REDIRECT_PACKAGESW;
		redirectIdentity.PackageListLength = (UINT32)_wcslen(CREDSSP_REDIRECT_PACKAGESW);
		negoAuthData = &redirectIdentity;
	}

	const SECURITY_STATUS status = table->AcquireCredentialsHandleW(
	    pszPrincipal, (SEC_WCHAR*)NEGOTIATE_NAMEW, SECPKG_CRED_OUTBOUND, pvLogonID, negoAuthData,
	    pGetKeyFn, pvGetKeyArgument, &creds->negoCred, ptsExpiry);
	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "inner Negotiate AcquireCredentialsHandle failed: 0x%08" PRIx32,
		         (UINT32)status);
		free(creds);
		return status;
	}
	creds->haveNegoCred = TRUE;

	sspi_SecureHandleSetLowerPointer(phCredential, creds);
	sspi_SecureHandleSetPackageId(phCredential, SSPI_PACKAGE_CREDSSP);
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY credssp_FreeCredentialsHandle(PCredHandle phCredential)
{
	if (!phCredential)
		return SEC_E_INVALID_HANDLE;

	CREDSSP_CREDENTIALS* creds =
	    (CREDSSP_CREDENTIALS*)sspi_SecureHandleGetLowerPointer(phCredential);
	sspi_SecureHandleInvalidate(phCredential);
	if (!creds)
		return SEC_E_INVALID_HANDLE;

	if (creds->nego && creds->haveNegoCred)
		(void)creds->nego->FreeCredentialsHandle(&creds->negoCred);
	free(creds);
	return SEC_E_OK;
}

/* The caller passes the server's leaf certificate as [UINT32 len][DER], matching what the
 * native tspkg receives. pubKeyAuth is computed over the SubjectPublicKey, so extract the raw
 * public key (as freerdp_certificate_get_public_key does: X509_get0_pubkey + i2d_PublicKey)
 * and store that, not the whole certificate. */
static BOOL credssp_store_cert_public_key(CREDSSP_CONTEXT* context, const BYTE* buffer,
                                          size_t bufferLen)
{
	if (bufferLen <= sizeof(UINT32))
		return FALSE;
	const UINT32 derLen = winpr_Data_Get_UINT32(buffer);
	if ((derLen == 0) || (derLen > bufferLen - sizeof(UINT32)))
		return FALSE;
	const BYTE* der = &buffer[sizeof(UINT32)];

#ifdef WITH_OPENSSL
	const unsigned char* p = der;
	X509* x509 = d2i_X509(nullptr, &p, (long)derLen);
	if (!x509)
	{
		WLog_ERR(TAG, "failed to parse the server certificate");
		return FALSE;
	}

	BOOL rc = FALSE;
	EVP_PKEY* pkey = X509_get0_pubkey(x509);
	int keyLen = pkey ? i2d_PublicKey(pkey, nullptr) : -1;
	if (keyLen > 0)
	{
		sspi_SecBufferFree(&context->publicKey);
		if (sspi_SecBufferAlloc(&context->publicKey, (ULONG)keyLen))
		{
			BYTE* out = context->publicKey.pvBuffer;
			rc = (i2d_PublicKey(pkey, &out) == keyLen);
		}
	}
	if (!rc)
		WLog_ERR(TAG, "failed to extract the server public key");
	X509_free(x509);
	return rc;
#else
	WLog_ERR(TAG, "CredSSP public-key extraction requires OpenSSL");
	return FALSE;
#endif
}

/* Grab the server certificate from the READONLY|TOKEN input buffer, if present, and keep its
 * SubjectPublicKey for the pubKeyAuth hash. */
static BOOL credssp_store_public_key(CREDSSP_CONTEXT* context, PSecBufferDesc pInput)
{
	if (!pInput)
		return TRUE;
	for (ULONG i = 0; i < pInput->cBuffers; i++)
	{
		const SecBuffer* b = &pInput->pBuffers[i];
		if (((b->BufferType & ~SECBUFFER_ATTRMASK) == SECBUFFER_TOKEN) &&
		    (b->BufferType & SECBUFFER_READONLY) && b->pvBuffer && (b->cbBuffer > 0))
			return credssp_store_cert_public_key(context, b->pvBuffer, b->cbBuffer);
	}
	return TRUE;
}

static const SecBuffer* credssp_token_input(PSecBufferDesc pInput)
{
	if (!pInput)
		return nullptr;
	for (ULONG i = 0; i < pInput->cBuffers; i++)
	{
		const SecBuffer* b = &pInput->pBuffers[i];
		if ((b->BufferType == SECBUFFER_TOKEN) && b->pvBuffer && (b->cbBuffer > 0))
			return b;
	}
	return nullptr;
}

/* Hand the finished TSRequest back to the caller through a WinPR context buffer. */
static SECURITY_STATUS credssp_set_output(const SecBuffer* request, PSecBufferDesc pOutput)
{
	if (!pOutput || (pOutput->cBuffers < 1))
		return SEC_E_INVALID_TOKEN;

	SecBuffer* out = &pOutput->pBuffers[0];
	void* buf = sspi_ContextBufferAlloc(InitializeSecurityContextIndex, request->cbBuffer);
	if (!buf)
		return SEC_E_INSUFFICIENT_MEMORY;
	CopyMemory(buf, request->pvBuffer, request->cbBuffer);
	out->BufferType = SECBUFFER_TOKEN;
	out->pvBuffer = buf;
	out->cbBuffer = request->cbBuffer;
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY credssp_InitializeSecurityContextW(
    PCredHandle phCredential, PCtxtHandle phContext, SEC_WCHAR* pszTargetName, ULONG fContextReq,
    ULONG Reserved1, ULONG TargetDataRep, PSecBufferDesc pInput, ULONG Reserved2,
    PCtxtHandle phNewContext, PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	CREDSSP_CREDENTIALS* creds =
	    (CREDSSP_CREDENTIALS*)sspi_SecureHandleGetLowerPointer(phCredential);
	if (!creds)
		return SEC_E_INVALID_HANDLE;

	CREDSSP_CONTEXT* context =
	    phContext ? (CREDSSP_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext) : nullptr;
	const BOOL fresh = (context == nullptr);
	if (fresh)
	{
		context = credssp_ContextNew();
		if (!context)
			return SEC_E_INSUFFICIENT_MEMORY;
		context->nego = creds->nego;
		context->flags = creds->flags;
		context->identity = creds->identity;
		context->haveIdentity = creds->haveIdentity;
		/* The inner Negotiate needs the target SPN to request the service ticket. */
		if (pszTargetName)
		{
			context->targetName = _wcsdup(pszTargetName);
			if (!context->targetName)
			{
				credssp_ContextFree(context);
				return SEC_E_INSUFFICIENT_MEMORY;
			}
		}
	}

	if (!credssp_store_public_key(context, pInput))
		goto internal_error;

	SECURITY_STATUS rc = SEC_E_INTERNAL_ERROR;
	WINPR_TSREQUEST in = { 0 };
	WINPR_TSREQUEST out = { 0 };
	SecBuffer request = { 0 };

	const SecBuffer* inToken = credssp_token_input(pInput);
	if (inToken)
	{
		wStream ss = { 0 };
		wStream* s = Stream_StaticConstInit(&ss, inToken->pvBuffer, inToken->cbBuffer);
		if (!winpr_tsrequest_decode(s, &in))
		{
			rc = SEC_E_INVALID_TOKEN;
			goto cleanup;
		}
		if (context->peerVersion == 0)
			context->peerVersion = in.version;
	}

	out.version = context->version;

	if (context->state == CREDSSP_STATE_NEGO)
	{
		/* drive the inner Negotiate exchange. The Kerberos/Negotiate ISC does not honour
		 * ISC_REQ_ALLOCATE_MEMORY for its output token: the caller must hand it a buffer large
		 * enough to hold the result (the same way credssp_auth does in nla.c). Size it from the
		 * package's cbMaxToken, with a floor that accommodates a Kerberos AP-REQ carrying a PAC. */
		PSecPkgInfoW negoInfo = nullptr;
		if (context->nego->QuerySecurityPackageInfoW((SEC_WCHAR*)NEGOTIATE_NAMEW, &negoInfo) !=
		    SEC_E_OK)
			goto internal_error;
		ULONG maxToken = negoInfo->cbMaxToken;
		(void)context->nego->FreeContextBuffer(negoInfo);
		if (maxToken < CREDSSP_MIN_NEGO_TOKEN)
			maxToken = CREDSSP_MIN_NEGO_TOKEN;

		/* The decoder leaves BufferType unset; the inner Negotiate locates its input by
		 * SECBUFFER_TOKEN, so tag it explicitly. */
		SecBuffer negoIn = in.negoToken;
		negoIn.BufferType = SECBUFFER_TOKEN;
		SecBufferDesc negoInDesc = { SECBUFFER_VERSION, 1, &negoIn };
		SecBuffer negoOutBuf = { 0, SECBUFFER_TOKEN, nullptr };
		if (!sspi_SecBufferAlloc(&negoOutBuf, maxToken))
			goto internal_error;
		negoOutBuf.BufferType = SECBUFFER_TOKEN;
		SecBufferDesc negoOutDesc = { SECBUFFER_VERSION, 1, &negoOutBuf };

		CtxtHandle* negoInCtx = context->haveNegoCtx ? &context->negoCtx : nullptr;
		const SECURITY_STATUS ns = context->nego->InitializeSecurityContextW(
		    &creds->negoCred, negoInCtx, context->targetName, fContextReq, Reserved1, TargetDataRep,
		    inToken ? &negoInDesc : nullptr, Reserved2, &context->negoCtx, &negoOutDesc,
		    pfContextAttr, ptsExpiry);
		context->haveNegoCtx = TRUE;

		if ((ns != SEC_E_OK) && (ns != SEC_I_CONTINUE_NEEDED))
		{
			WLog_ERR(TAG, "inner Negotiate ISC failed: 0x%08" PRIx32, (UINT32)ns);
			sspi_SecBufferFree(&negoOutBuf);
			rc = ns;
			goto cleanup;
		}

		if (negoOutBuf.cbBuffer > 0)
		{
			if (!sspi_SecBufferAlloc(&out.negoToken, negoOutBuf.cbBuffer))
			{
				sspi_SecBufferFree(&negoOutBuf);
				goto internal_error_cleanup;
			}
			CopyMemory(out.negoToken.pvBuffer, negoOutBuf.pvBuffer, negoOutBuf.cbBuffer);
		}
		sspi_SecBufferFree(&negoOutBuf);

		if (ns == SEC_E_OK)
		{
			/* Negotiate is done: capture sizes, nonce, and send pubKeyAuth with this message. */
			if (context->nego->QueryContextAttributesW(&context->negoCtx, SECPKG_ATTR_SIZES,
			                                           &context->sizes) != SEC_E_OK)
				goto internal_error_cleanup;
			context->haveSizes = TRUE;

			if (!sspi_SecBufferAlloc(&context->clientNonce, CREDSSP_NONCE_LENGTH) ||
			    (winpr_RAND(context->clientNonce.pvBuffer, CREDSSP_NONCE_LENGTH) != 0))
				goto internal_error_cleanup;

			SecBuffer hash = { 0 };
			if (!winpr_credssp_public_key_hash(FALSE, &context->clientNonce, &context->publicKey,
			                                   &hash))
				goto internal_error_cleanup;
			const BOOL enc =
			    credssp_encrypt(context, &hash, &out.pubKeyAuth, context->sendSeqNum++);
			sspi_SecBufferFree(&hash);
			if (!enc)
				goto internal_error_cleanup;

			if (!sspi_SecBufferAlloc(&out.clientNonce, CREDSSP_NONCE_LENGTH))
				goto internal_error_cleanup;
			CopyMemory(out.clientNonce.pvBuffer, context->clientNonce.pvBuffer,
			           CREDSSP_NONCE_LENGTH);

			context->state = CREDSSP_STATE_PUBKEY;
		}

		if (!winpr_tsrequest_encode(&out, context->peerVersion, FALSE, &request))
			goto internal_error_cleanup;
		rc = credssp_set_output(&request, pOutput);
		if (rc == SEC_E_OK)
			rc = SEC_I_CONTINUE_NEEDED;
		goto finish;
	}

	if (context->state == CREDSSP_STATE_PUBKEY)
	{
		/* verify the server's pubKeyAuth, then send the encrypted TSCredentials */
		if (in.pubKeyAuth.cbBuffer == 0)
		{
			rc = SEC_E_INVALID_TOKEN;
			goto cleanup;
		}

		SecBuffer serverHash = { 0 };
		SecBuffer expected = { 0 };
		if (!credssp_decrypt(context, &in.pubKeyAuth, &serverHash, context->recvSeqNum++))
		{
			rc = SEC_E_MESSAGE_ALTERED;
			goto cleanup;
		}
		const BOOL hok = winpr_credssp_public_key_hash(TRUE, &context->clientNonce,
		                                               &context->publicKey, &expected) &&
		                 (serverHash.cbBuffer == expected.cbBuffer) &&
		                 (memcmp(serverHash.pvBuffer, expected.pvBuffer, expected.cbBuffer) == 0);
		sspi_SecBufferFree(&serverHash);
		sspi_SecBufferFree(&expected);
		if (!hok)
		{
			WLog_ERR(TAG, "server pubKeyAuth verification failed");
			rc = SEC_E_MESSAGE_ALTERED;
			goto cleanup;
		}

		/* build and encrypt the delegated credentials */
		SecBuffer tsCreds = { 0 };
		BOOL built = FALSE;
		if (context->flags & CREDSSP_FLAG_REDIRECT)
		{
			KERB_TICKET_LOGON ticket = { 0 };
			if (context->nego->QueryContextAttributesW(
			        &context->negoCtx, SECPKG_CRED_ATTR_TICKET_LOGON, &ticket) == SEC_E_OK)
			{
				built = winpr_tscredentials_encode_remoteguard_kerb(&ticket, &tsCreds);
				free(ticket.ServiceTicket);
				free(ticket.TicketGrantingTicket);
			}
		}
		else
		{
			built = winpr_tscredentials_encode_password(
			    context->haveIdentity ? &context->identity : nullptr, &tsCreds);
		}
		if (!built)
			goto internal_error_cleanup;

		const BOOL enc = credssp_encrypt(context, &tsCreds, &out.authInfo, context->sendSeqNum++);
		sspi_SecBufferFree(&tsCreds);
		if (!enc)
			goto internal_error_cleanup;

		if (!winpr_tsrequest_encode(&out, context->peerVersion, FALSE, &request))
			goto internal_error_cleanup;
		rc = credssp_set_output(&request, pOutput);
		context->state = CREDSSP_STATE_DONE;
		goto finish;
	}

	rc = SEC_E_OUT_OF_SEQUENCE;
	goto cleanup;

internal_error_cleanup:
	rc = SEC_E_INTERNAL_ERROR;
	goto cleanup;

internal_error:
	if (fresh)
		credssp_ContextFree(context);
	return SEC_E_INTERNAL_ERROR;

finish:
	sspi_SecureHandleSetLowerPointer(phNewContext, context);
	sspi_SecureHandleSetPackageId(phNewContext, SSPI_PACKAGE_CREDSSP);

cleanup:
	winpr_tsrequest_free_contents(&in);
	winpr_tsrequest_free_contents(&out);
	sspi_SecBufferFree(&request);
	if ((rc != SEC_E_OK) && (rc != SEC_I_CONTINUE_NEEDED) && fresh)
	{
		sspi_SecureHandleInvalidate(phNewContext);
		credssp_ContextFree(context);
	}
	return rc;
}

static SECURITY_STATUS SEC_ENTRY credssp_DeleteSecurityContext(PCtxtHandle phContext)
{
	if (!phContext)
		return SEC_E_INVALID_HANDLE;
	CREDSSP_CONTEXT* context = (CREDSSP_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);
	sspi_SecureHandleInvalidate(phContext);
	if (!context)
		return SEC_E_INVALID_HANDLE;
	credssp_ContextFree(context);
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY credssp_QueryContextAttributesW(PCtxtHandle phContext,
                                                                 ULONG ulAttribute, void* pBuffer)
{
	if (!phContext || !pBuffer)
		return SEC_E_INVALID_HANDLE;

	CREDSSP_CONTEXT* context = (CREDSSP_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);
	if (!context)
		return SEC_E_INVALID_HANDLE;

	if (ulAttribute == SECPKG_ATTR_SIZES)
	{
		SecPkgContext_Sizes* sizes = (SecPkgContext_Sizes*)pBuffer;
		*sizes = context->sizes;
		return SEC_E_OK;
	}

	if (context->nego && context->nego->QueryContextAttributesW)
		return context->nego->QueryContextAttributesW(&context->negoCtx, ulAttribute, pBuffer);
	return SEC_E_UNSUPPORTED_FUNCTION;
}

/* The message sequence number continues the one established during authentication: the inner
 * Kerberos context expects remote_seq + MessageSeqNo, so post-authentication traffic (the RDPEAR
 * redirected calls) must keep counting from where pubKeyAuth/TSCredentials left off. The context
 * owns that counter, so ignore the caller's MessageSeqNo and advance sendSeqNum/recvSeqNum here. */
static SECURITY_STATUS SEC_ENTRY credssp_EncryptMessage(PCtxtHandle phContext, ULONG fQOP,
                                                        PSecBufferDesc pMessage,
                                                        WINPR_ATTR_UNUSED ULONG MessageSeqNo)
{
	CREDSSP_CONTEXT* context = (CREDSSP_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);
	if (!context || !context->nego)
		return SEC_E_INVALID_HANDLE;
	return context->nego->EncryptMessage(&context->negoCtx, fQOP, pMessage, context->sendSeqNum++);
}

static SECURITY_STATUS SEC_ENTRY credssp_DecryptMessage(PCtxtHandle phContext,
                                                        PSecBufferDesc pMessage,
                                                        WINPR_ATTR_UNUSED ULONG MessageSeqNo,
                                                        PULONG pfQOP)
{
	CREDSSP_CONTEXT* context = (CREDSSP_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);
	if (!context || !context->nego)
		return SEC_E_INVALID_HANDLE;
	return context->nego->DecryptMessage(&context->negoCtx, pMessage, context->recvSeqNum++, pfQOP);
}

const SecurityFunctionTableA CREDSSP_SecurityFunctionTableA = {
	3,                             /* dwVersion */
	nullptr,                       /* EnumerateSecurityPackages */
	nullptr,                       /* QueryCredentialsAttributes */
	nullptr,                       /* AcquireCredentialsHandle */
	credssp_FreeCredentialsHandle, /* FreeCredentialsHandle */
	nullptr,                       /* Reserved2 */
	nullptr,                       /* InitializeSecurityContext */
	nullptr,                       /* AcceptSecurityContext */
	nullptr,                       /* CompleteAuthToken */
	credssp_DeleteSecurityContext, /* DeleteSecurityContext */
	nullptr,                       /* ApplyControlToken */
	nullptr,                       /* QueryContextAttributes */
	nullptr,                       /* ImpersonateSecurityContext */
	nullptr,                       /* RevertSecurityContext */
	nullptr,                       /* MakeSignature */
	nullptr,                       /* VerifySignature */
	nullptr,                       /* FreeContextBuffer */
	nullptr,                       /* QuerySecurityPackageInfo */
	nullptr,                       /* Reserved3 */
	nullptr,                       /* Reserved4 */
	nullptr,                       /* ExportSecurityContext */
	nullptr,                       /* ImportSecurityContext */
	nullptr,                       /* AddCredentials */
	nullptr,                       /* Reserved8 */
	nullptr,                       /* QuerySecurityContextToken */
	credssp_EncryptMessage,        /* EncryptMessage */
	credssp_DecryptMessage,        /* DecryptMessage */
	nullptr,                       /* SetContextAttributes */
	nullptr,                       /* SetCredentialsAttributes */
};

const SecurityFunctionTableW CREDSSP_SecurityFunctionTableW = {
	3,                                  /* dwVersion */
	nullptr,                            /* EnumerateSecurityPackages */
	nullptr,                            /* QueryCredentialsAttributes */
	credssp_AcquireCredentialsHandleW,  /* AcquireCredentialsHandle */
	credssp_FreeCredentialsHandle,      /* FreeCredentialsHandle */
	nullptr,                            /* Reserved2 */
	credssp_InitializeSecurityContextW, /* InitializeSecurityContext */
	nullptr,                            /* AcceptSecurityContext */
	nullptr,                            /* CompleteAuthToken */
	credssp_DeleteSecurityContext,      /* DeleteSecurityContext */
	nullptr,                            /* ApplyControlToken */
	credssp_QueryContextAttributesW,    /* QueryContextAttributes */
	nullptr,                            /* ImpersonateSecurityContext */
	nullptr,                            /* RevertSecurityContext */
	nullptr,                            /* MakeSignature */
	nullptr,                            /* VerifySignature */
	nullptr,                            /* FreeContextBuffer */
	nullptr,                            /* QuerySecurityPackageInfo */
	nullptr,                            /* Reserved3 */
	nullptr,                            /* Reserved4 */
	nullptr,                            /* ExportSecurityContext */
	nullptr,                            /* ImportSecurityContext */
	nullptr,                            /* AddCredentials */
	nullptr,                            /* Reserved8 */
	nullptr,                            /* QuerySecurityContextToken */
	credssp_EncryptMessage,             /* EncryptMessage */
	credssp_DecryptMessage,             /* DecryptMessage */
	nullptr,                            /* SetContextAttributes */
	nullptr,                            /* SetCredentialsAttributes */
};

const SecPkgInfoA CREDSSP_SecPkgInfoA = {
	0x000110733,                          /* fCapabilities */
	1,                                    /* wVersion */
	0xFFFF,                               /* wRPCID */
	0x000090A8,                           /* cbMaxToken */
	"CREDSSP",                            /* Name */
	"Microsoft CredSSP Security Provider" /* Comment */
};

static WCHAR CREDSSP_SecPkgInfoW_NameBuffer[128] = WINPR_C_ARRAY_INIT;
static WCHAR CREDSSP_SecPkgInfoW_CommentBuffer[128] = WINPR_C_ARRAY_INIT;

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
