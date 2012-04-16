/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Credential Security Support Provider (CredSSP)
 *
 * Copyright 2010-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <time.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <freerdp/crypto/tls.h>
#include <freerdp/utils/stream.h>

#include <freerdp/sspi/sspi.h>
#include <freerdp/sspi/credssp.h>

#include "sspi.h"

/**
 * TSRequest ::= SEQUENCE {
 * 	version    [0] INTEGER,
 * 	negoTokens [1] NegoData OPTIONAL,
 * 	authInfo   [2] OCTET STRING OPTIONAL,
 * 	pubKeyAuth [3] OCTET STRING OPTIONAL
 * }
 *
 * NegoData ::= SEQUENCE OF NegoDataItem
 *
 * NegoDataItem ::= SEQUENCE {
 * 	negoToken [0] OCTET STRING
 * }
 *
 * TSCredentials ::= SEQUENCE {
 * 	credType    [0] INTEGER,
 * 	credentials [1] OCTET STRING
 * }
 *
 * TSPasswordCreds ::= SEQUENCE {
 * 	domainName  [0] OCTET STRING,
 * 	userName    [1] OCTET STRING,
 * 	password    [2] OCTET STRING
 * }
 *
 * TSSmartCardCreds ::= SEQUENCE {
 * 	pin        [0] OCTET STRING,
 * 	cspData    [1] TSCspDataDetail,
 * 	userHint   [2] OCTET STRING OPTIONAL,
 * 	domainHint [3] OCTET STRING OPTIONAL
 * }
 *
 * TSCspDataDetail ::= SEQUENCE {
 * 	keySpec       [0] INTEGER,
 * 	cardName      [1] OCTET STRING OPTIONAL,
 * 	readerName    [2] OCTET STRING OPTIONAL,
 * 	containerName [3] OCTET STRING OPTIONAL,
 * 	cspName       [4] OCTET STRING OPTIONAL
 * }
 *
 */

#ifdef WITH_DEBUG_NLA
#define WITH_DEBUG_CREDSSP
#endif

void credssp_SetContextIdentity(rdpCredssp* context, char* user, char* domain, char* password)
{
	size_t size;
	context->identity.Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;

	context->identity.User = (uint16*) freerdp_uniconv_out(context->uniconv, user, &size);
	context->identity.UserLength = (uint32) size;

	if (domain)
	{
		context->identity.Domain = (uint16*) freerdp_uniconv_out(context->uniconv, domain, &size);
		context->identity.DomainLength = (uint32) size;
	}
	else
	{
		context->identity.Domain = (uint16*) NULL;
		context->identity.DomainLength = 0;
	}

	context->identity.Password = (uint16*) freerdp_uniconv_out(context->uniconv, (char*) password, &size);
	context->identity.PasswordLength = (uint32) size;
}

/**
 * Initialize NTLMSSP authentication module (client).
 * @param credssp
 */

int credssp_ntlm_client_init(rdpCredssp* credssp)
{
	freerdp* instance;
	rdpSettings* settings = credssp->settings;
	instance = (freerdp*) settings->instance;

	if ((settings->password == NULL) || (settings->username == NULL))
	{
		if (instance->Authenticate)
		{
			boolean proceed = instance->Authenticate(instance,
					&settings->username, &settings->password, &settings->domain);
			if (!proceed)
				return 0;
		}
	}

	credssp_SetContextIdentity(credssp, settings->username, settings->domain, settings->password);

	sspi_SecBufferAlloc(&credssp->PublicKey, credssp->tls->public_key.length);
	memcpy(credssp->PublicKey.pvBuffer, credssp->tls->public_key.data, credssp->tls->public_key.length);

	return 1;
}

/**
 * Initialize NTLMSSP authentication module (server).
 * @param credssp
 */

char* test_User = "username";
char* test_Password = "password";

int credssp_ntlm_server_init(rdpCredssp* credssp)
{
	freerdp* instance;
	rdpSettings* settings = credssp->settings;
	instance = (freerdp*) settings->instance;

	credssp_SetContextIdentity(credssp, test_User, NULL, test_Password);

	sspi_SecBufferAlloc(&credssp->PublicKey, credssp->tls->public_key.length);
	memcpy(credssp->PublicKey.pvBuffer, credssp->tls->public_key.data, credssp->tls->public_key.length);

	return 1;
}

#define NTLM_PACKAGE_NAME	_T("NTLM")

int credssp_client_authenticate(rdpCredssp* credssp)
{
	uint32 cbMaxToken;
	uint32 fContextReq;
	uint32 pfContextAttr;
	SECURITY_STATUS status;
	CredHandle credentials;
	TimeStamp expiration;
	SecPkgInfo* pPackageInfo;
	PSecBuffer p_buffer;
	SecBuffer input_buffer;
	SecBuffer output_buffer;
	SecBufferDesc input_buffer_desc;
	SecBufferDesc output_buffer_desc;
	boolean have_context;
	boolean have_input_buffer;
	boolean have_pub_key_auth;

	sspi_GlobalInit();

	if (credssp_ntlm_client_init(credssp) == 0)
		return 0;

	credssp->table = InitSecurityInterface();

	status = QuerySecurityPackageInfo(NTLM_PACKAGE_NAME, &pPackageInfo);

	if (status != SEC_E_OK)
	{
		printf("QuerySecurityPackageInfo status: 0x%08X\n", status);
		return 0;
	}

	cbMaxToken = pPackageInfo->cbMaxToken;

	status = credssp->table->AcquireCredentialsHandle(NULL, NTLM_PACKAGE_NAME,
			SECPKG_CRED_OUTBOUND, NULL, &credssp->identity, NULL, NULL, &credentials, &expiration);

	if (status != SEC_E_OK)
	{
		printf("AcquireCredentialsHandle status: 0x%08X\n", status);
		return 0;
	}

	have_context = false;
	have_input_buffer = false;
	have_pub_key_auth = false;
	memset(&input_buffer, 0, sizeof(SecBuffer));
	memset(&output_buffer, 0, sizeof(SecBuffer));
	memset(&credssp->ContextSizes, 0, sizeof(SecPkgContext_Sizes));

	fContextReq = ISC_REQ_REPLAY_DETECT | ISC_REQ_SEQUENCE_DETECT |
			ISC_REQ_CONFIDENTIALITY | ISC_REQ_DELEGATE;

	while (true)
	{
		output_buffer_desc.ulVersion = SECBUFFER_VERSION;
		output_buffer_desc.cBuffers = 1;
		output_buffer_desc.pBuffers = &output_buffer;
		output_buffer.BufferType = SECBUFFER_TOKEN;
		output_buffer.cbBuffer = cbMaxToken;
		output_buffer.pvBuffer = xmalloc(output_buffer.cbBuffer);

		status = credssp->table->InitializeSecurityContext(&credentials,
				(have_context) ? &credssp->context : NULL,
				NULL, fContextReq, 0, SECURITY_NATIVE_DREP,
				(have_input_buffer) ? &input_buffer_desc : NULL,
				0, &credssp->context, &output_buffer_desc, &pfContextAttr, &expiration);

		if (input_buffer.pvBuffer != NULL)
		{
			xfree(input_buffer.pvBuffer);
			input_buffer.pvBuffer = NULL;
		}

		if ((status == SEC_I_COMPLETE_AND_CONTINUE) || (status == SEC_I_COMPLETE_NEEDED) || (status == SEC_E_OK))
		{
			if (credssp->table->CompleteAuthToken != NULL)
				credssp->table->CompleteAuthToken(&credssp->context, &output_buffer_desc);

			have_pub_key_auth = true;

			if (credssp->table->QueryContextAttributes(&credssp->context, SECPKG_ATTR_SIZES, &credssp->ContextSizes) != SEC_E_OK)
			{
				printf("QueryContextAttributes SECPKG_ATTR_SIZES failure\n");
				return 0;
			}

			if (have_pub_key_auth)
			{
				uint8* p;
				SecBuffer Buffers[2];
				SecBufferDesc Message;
				SECURITY_STATUS encrypt_status;

				Buffers[0].BufferType = SECBUFFER_DATA; /* TLS Public Key */
				Buffers[1].BufferType = SECBUFFER_TOKEN; /* Signature */

				Buffers[0].cbBuffer = credssp->PublicKey.cbBuffer;
				Buffers[0].pvBuffer = xmalloc(Buffers[0].cbBuffer);
				memcpy(Buffers[0].pvBuffer, credssp->PublicKey.pvBuffer, Buffers[0].cbBuffer);

				Buffers[1].cbBuffer = credssp->ContextSizes.cbMaxSignature;
				Buffers[1].pvBuffer = xzalloc(Buffers[1].cbBuffer);

				Message.cBuffers = 2;
				Message.ulVersion = SECBUFFER_VERSION;
				Message.pBuffers = (PSecBuffer) &Buffers;

				sspi_SecBufferAlloc(&credssp->pubKeyAuth, Buffers[0].cbBuffer + Buffers[1].cbBuffer);

				encrypt_status = credssp->table->EncryptMessage(&credssp->context, 0, &Message, 0);

				if (encrypt_status != SEC_E_OK)
				{
					printf("EncryptMessage status: 0x%08X\n", encrypt_status);
					return 0;
				}

				p = (uint8*) credssp->pubKeyAuth.pvBuffer;
				memcpy(p, Buffers[1].pvBuffer, Buffers[1].cbBuffer); /* Message Signature */
				memcpy(&p[Buffers[1].cbBuffer], Buffers[0].pvBuffer, Buffers[0].cbBuffer); /* Encrypted Public Key */
				xfree(Buffers[0].pvBuffer);
				xfree(Buffers[1].pvBuffer);
			}

			if (status == SEC_I_COMPLETE_NEEDED)
				status = SEC_E_OK;
			else if (status == SEC_I_COMPLETE_AND_CONTINUE)
				status = SEC_I_CONTINUE_NEEDED;
		}

		/* send authentication token to server */

		if (output_buffer.cbBuffer > 0)
		{
			p_buffer = &output_buffer_desc.pBuffers[0];

			credssp->negoToken.pvBuffer = p_buffer->pvBuffer;
			credssp->negoToken.cbBuffer = p_buffer->cbBuffer;

#ifdef WITH_DEBUG_CREDSSP
			printf("Sending Authentication Token\n");
			freerdp_hexdump(credssp->negoToken.pvBuffer, credssp->negoToken.cbBuffer);
#endif

			credssp_send(credssp);
			credssp_buffer_free(credssp);
		}

		if (status != SEC_I_CONTINUE_NEEDED)
			break;

		/* receive server response and place in input buffer */

		input_buffer_desc.ulVersion = SECBUFFER_VERSION;
		input_buffer_desc.cBuffers = 1;
		input_buffer_desc.pBuffers = &input_buffer;
		input_buffer.BufferType = SECBUFFER_TOKEN;

		if (credssp_recv(credssp) < 0)
			return -1;

#ifdef WITH_DEBUG_CREDSSP
		printf("Receiving Authentication Token\n");
		freerdp_hexdump(credssp->negoToken.pvBuffer, credssp->negoToken.cbBuffer);
#endif

		p_buffer = &input_buffer_desc.pBuffers[0];
		p_buffer->pvBuffer = credssp->negoToken.pvBuffer;
		p_buffer->cbBuffer = credssp->negoToken.cbBuffer;

		have_input_buffer = true;
		have_context = true;
	}

	/* Encrypted Public Key +1 */
	if (credssp_recv(credssp) < 0)
		return -1;

	/* Verify Server Public Key Echo */

	status = credssp_verify_public_key_echo(credssp);
	credssp_buffer_free(credssp);

	if (status != SEC_E_OK)
		return 0;

	/* Send encrypted credentials */

	status = credssp_encrypt_ts_credentials(credssp);

	if (status != SEC_E_OK)
	{
		printf("credssp_encrypt_ts_credentials status: 0x%08X\n", status);
		return 0;
	}

	credssp_send(credssp);
	credssp_buffer_free(credssp);

	/* Free resources */

	FreeCredentialsHandle(&credentials);
	FreeContextBuffer(pPackageInfo);

	return 1;
}

/**
 * Authenticate with client using CredSSP (server).
 * @param credssp
 * @return 1 if authentication is successful
 */

int credssp_server_authenticate(rdpCredssp* credssp)
{
	uint32 cbMaxToken;
	uint32 fContextReq;
	uint32 pfContextAttr;
	SECURITY_STATUS status;
	CredHandle credentials;
	TimeStamp expiration;
	SecPkgInfo* pPackageInfo;
	PSecBuffer p_buffer;
	SecBuffer input_buffer;
	SecBuffer output_buffer;
	SecBufferDesc input_buffer_desc;
	SecBufferDesc output_buffer_desc;
	boolean have_context;
	boolean have_input_buffer;
	boolean have_pub_key_auth;

	sspi_GlobalInit();

	if (credssp_ntlm_server_init(credssp) == 0)
		return 0;

	credssp->table = InitSecurityInterface();

	status = QuerySecurityPackageInfo(NTLM_PACKAGE_NAME, &pPackageInfo);

	if (status != SEC_E_OK)
	{
		printf("QuerySecurityPackageInfo status: 0x%08X\n", status);
		return 0;
	}

	cbMaxToken = pPackageInfo->cbMaxToken;

	status = credssp->table->AcquireCredentialsHandle(NULL, NTLM_PACKAGE_NAME,
			SECPKG_CRED_INBOUND, NULL, &credssp->identity, NULL, NULL, &credentials, &expiration);

	if (status != SEC_E_OK)
	{
		printf("AcquireCredentialsHandle status: 0x%08X\n", status);
		return 0;
	}

	have_context = false;
	have_input_buffer = false;
	have_pub_key_auth = false;
	memset(&input_buffer, 0, sizeof(SecBuffer));
	memset(&output_buffer, 0, sizeof(SecBuffer));
	memset(&credssp->ContextSizes, 0, sizeof(SecPkgContext_Sizes));

	fContextReq = ISC_REQ_REPLAY_DETECT | ISC_REQ_SEQUENCE_DETECT |
			ISC_REQ_CONFIDENTIALITY | ISC_REQ_DELEGATE;

	while (true)
	{
		input_buffer_desc.ulVersion = SECBUFFER_VERSION;
		input_buffer_desc.cBuffers = 1;
		input_buffer_desc.pBuffers = &input_buffer;
		input_buffer.BufferType = SECBUFFER_TOKEN;

		/* receive authentication token */

		input_buffer_desc.ulVersion = SECBUFFER_VERSION;
		input_buffer_desc.cBuffers = 1;
		input_buffer_desc.pBuffers = &input_buffer;
		input_buffer.BufferType = SECBUFFER_TOKEN;

		if (credssp_recv(credssp) < 0)
			return -1;

#ifdef WITH_DEBUG_CREDSSP
		printf("Receiving Authentication Token\n");
		credssp_buffer_print(credssp);
#endif

		p_buffer = &input_buffer_desc.pBuffers[0];
		p_buffer->pvBuffer = credssp->negoToken.pvBuffer;
		p_buffer->cbBuffer = credssp->negoToken.cbBuffer;

		output_buffer_desc.ulVersion = SECBUFFER_VERSION;
		output_buffer_desc.cBuffers = 1;
		output_buffer_desc.pBuffers = &output_buffer;
		output_buffer.BufferType = SECBUFFER_TOKEN;
		output_buffer.cbBuffer = cbMaxToken;
		output_buffer.pvBuffer = xmalloc(output_buffer.cbBuffer);

		status = credssp->table->AcceptSecurityContext(&credentials,
			have_context? &credssp->context: NULL,
			&input_buffer_desc, 0, SECURITY_NATIVE_DREP, &credssp->context,
			&output_buffer_desc, &pfContextAttr, &expiration);

		if (input_buffer.pvBuffer != NULL)
		{
			xfree(input_buffer.pvBuffer);
			input_buffer.pvBuffer = NULL;
		}

		p_buffer = &output_buffer_desc.pBuffers[0];
		credssp->negoToken.pvBuffer = p_buffer->pvBuffer;
		credssp->negoToken.cbBuffer = p_buffer->cbBuffer;

		if ((status == SEC_I_COMPLETE_AND_CONTINUE) || (status == SEC_I_COMPLETE_NEEDED))
		{
			if (credssp->table->CompleteAuthToken != NULL)
				credssp->table->CompleteAuthToken(&credssp->context, &output_buffer_desc);

			have_pub_key_auth = true;

			sspi_SecBufferFree(&credssp->negoToken);
			credssp->negoToken.pvBuffer = NULL;
			credssp->negoToken.cbBuffer = 0;

			if (credssp->table->QueryContextAttributes(&credssp->context, SECPKG_ATTR_SIZES, &credssp->ContextSizes) != SEC_E_OK)
			{
				printf("QueryContextAttributes SECPKG_ATTR_SIZES failure\n");
				return 0;
			}

			if (have_pub_key_auth)
			{
				uint8* p;
				SecBuffer Buffers[2];
				SecBufferDesc Message;

				Buffers[0].BufferType = SECBUFFER_DATA; /* TLS Public Key */
				Buffers[1].BufferType = SECBUFFER_TOKEN; /* Signature */

				Buffers[0].cbBuffer = credssp->PublicKey.cbBuffer;
				Buffers[0].pvBuffer = xmalloc(Buffers[0].cbBuffer);
				memcpy(Buffers[0].pvBuffer, credssp->PublicKey.pvBuffer, Buffers[0].cbBuffer);

				Buffers[1].cbBuffer = credssp->ContextSizes.cbMaxSignature;
				Buffers[1].pvBuffer = xzalloc(Buffers[1].cbBuffer);

				Message.cBuffers = 2;
				Message.ulVersion = SECBUFFER_VERSION;
				Message.pBuffers = (PSecBuffer) &Buffers;

				p = (uint8*) Buffers[0].pvBuffer;
				p[0]++; /* Public Key +1 */

				sspi_SecBufferAlloc(&credssp->pubKeyAuth, Buffers[0].cbBuffer + Buffers[1].cbBuffer);

				credssp->table->EncryptMessage(&credssp->context, 0, &Message, 0);

				p = (uint8*) credssp->pubKeyAuth.pvBuffer;
				memcpy(p, Buffers[1].pvBuffer, Buffers[1].cbBuffer); /* Message Signature */
				memcpy(&p[Buffers[1].cbBuffer], Buffers[0].pvBuffer, Buffers[0].cbBuffer); /* Encrypted Public Key */
			}

			if (status == SEC_I_COMPLETE_NEEDED)
				status = SEC_E_OK;
			else if (status == SEC_I_COMPLETE_AND_CONTINUE)
				status = SEC_I_CONTINUE_NEEDED;
		}

		/* send authentication token */

#ifdef WITH_DEBUG_CREDSSP
		printf("Sending Authentication Token\n");
		credssp_buffer_print(credssp);
#endif

		credssp_send(credssp);
		credssp_buffer_free(credssp);

		if (status != SEC_I_CONTINUE_NEEDED)
			break;

		have_context = true;
	}

	/* Receive encrypted credentials */

	if (credssp_recv(credssp) < 0)
		return -1;

	if (status != SEC_E_OK)
	{
		printf("AcceptSecurityContext status: 0x%08X\n", status);
		return 0;
	}

	status = credssp->table->ImpersonateSecurityContext(&credssp->context);

	if (status != SEC_E_OK)
	{
		printf("ImpersonateSecurityContext status: 0x%08X\n", status);
		return 0;
	}
	else
	{
		status = credssp->table->RevertSecurityContext(&credssp->context);

		if (status != SEC_E_OK)
		{
			printf("RevertSecurityContext status: 0x%08X\n", status);
			return 0;
		}
	}

	FreeContextBuffer(pPackageInfo);

	return 1;
}

/**
 * Authenticate using CredSSP.
 * @param credssp
 * @return 1 if authentication is successful
 */

int credssp_authenticate(rdpCredssp* credssp)
{
	if (credssp->server)
		return credssp_server_authenticate(credssp);
	else
		return credssp_client_authenticate(credssp);
}

SECURITY_STATUS credssp_verify_public_key_echo(rdpCredssp* credssp)
{
	int length;
	uint32 pfQOP;
	uint8* public_key1;
	uint8* public_key2;
	uint8* pub_key_auth;
	int public_key_length;
	SecBuffer Buffers[2];
	SecBufferDesc Message;
	SECURITY_STATUS status;

	length = credssp->pubKeyAuth.cbBuffer;
	pub_key_auth = (uint8*) credssp->pubKeyAuth.pvBuffer;
	public_key_length = credssp->PublicKey.cbBuffer;

	Buffers[0].BufferType = SECBUFFER_TOKEN; /* Signature */
	Buffers[1].BufferType = SECBUFFER_DATA; /* Encrypted TLS Public Key */

	Buffers[0].cbBuffer = credssp->ContextSizes.cbMaxSignature;
	Buffers[0].pvBuffer = xmalloc(Buffers[0].cbBuffer);
	memcpy(Buffers[0].pvBuffer, pub_key_auth, Buffers[0].cbBuffer);

	Buffers[1].cbBuffer = length - Buffers[0].cbBuffer;
	Buffers[1].pvBuffer = xmalloc(Buffers[1].cbBuffer);
	memcpy(Buffers[1].pvBuffer, &pub_key_auth[Buffers[0].cbBuffer], Buffers[1].cbBuffer);

	Message.cBuffers = 2;
	Message.ulVersion = SECBUFFER_VERSION;
	Message.pBuffers = (PSecBuffer) &Buffers;

	status = credssp->table->DecryptMessage(&credssp->context, &Message, 0, &pfQOP);

	if (status != SEC_E_OK)
		return status;

	public_key1 = (uint8*) credssp->PublicKey.pvBuffer;
	public_key2 = (uint8*) Buffers[1].pvBuffer;

	public_key2[0]--; /* server echos the public key +1 */

	if (memcmp(public_key1, public_key2, public_key_length) != 0)
	{
		printf("Could not verify server's public key echo\n");

		printf("Expected (length = %d):\n", public_key_length);
		freerdp_hexdump(public_key1, public_key_length);

		printf("Actual (length = %d):\n", public_key_length);
		freerdp_hexdump(public_key2, public_key_length);

		return SEC_E_MESSAGE_ALTERED; /* DO NOT SEND CREDENTIALS! */
	}

	public_key2[0]++;

	xfree(Buffers[0].pvBuffer);
	xfree(Buffers[1].pvBuffer);

	return SEC_E_OK;
}

SECURITY_STATUS credssp_encrypt_ts_credentials(rdpCredssp* credssp)
{
	uint8* p;
	SecBuffer Buffers[2];
	SecBufferDesc Message;
	SECURITY_STATUS status;

	credssp_encode_ts_credentials(credssp);

	Buffers[0].BufferType = SECBUFFER_DATA; /* TSCredentials */
	Buffers[1].BufferType = SECBUFFER_TOKEN; /* Signature */

	Buffers[0].cbBuffer = credssp->ts_credentials.cbBuffer;
	Buffers[0].pvBuffer = xmalloc(Buffers[0].cbBuffer);
	memcpy(Buffers[0].pvBuffer, credssp->ts_credentials.pvBuffer, Buffers[0].cbBuffer);

	Buffers[1].cbBuffer = 16;
	Buffers[1].pvBuffer = xzalloc(Buffers[1].cbBuffer);

	Message.cBuffers = 2;
	Message.ulVersion = SECBUFFER_VERSION;
	Message.pBuffers = (PSecBuffer) &Buffers;

	sspi_SecBufferAlloc(&credssp->authInfo, Buffers[0].cbBuffer + Buffers[1].cbBuffer);

	status = credssp->table->EncryptMessage(&credssp->context, 0, &Message, 1);

	if (status != SEC_E_OK)
		return status;

	p = (uint8*) credssp->authInfo.pvBuffer;
	memcpy(p, Buffers[1].pvBuffer, Buffers[1].cbBuffer); /* Message Signature */
	memcpy(&p[Buffers[1].cbBuffer], Buffers[0].pvBuffer, Buffers[0].cbBuffer); /* Encrypted TSCredentials */

	xfree(Buffers[0].pvBuffer);
	xfree(Buffers[1].pvBuffer);

	return SEC_E_OK;
}

int credssp_skip_ts_password_creds(rdpCredssp* credssp)
{
	int length;
	int ts_password_creds_length = 0;

	length = ber_skip_octet_string(credssp->identity.DomainLength);
	length += ber_skip_contextual_tag(length);
	ts_password_creds_length += length;

	length = ber_skip_octet_string(credssp->identity.UserLength);
	length += ber_skip_contextual_tag(length);
	ts_password_creds_length += length;

	length = ber_skip_octet_string(credssp->identity.PasswordLength);
	length += ber_skip_contextual_tag(length);
	ts_password_creds_length += length;

	length = ber_skip_sequence(ts_password_creds_length);

	return length;
}

void credssp_write_ts_password_creds(rdpCredssp* credssp, STREAM* s)
{
	int length;

	length = credssp_skip_ts_password_creds(credssp);

	/* TSPasswordCreds (SEQUENCE) */
	length = ber_get_content_length(length);
	ber_write_sequence_tag(s, length);

	/* [0] domainName (OCTET STRING) */
	ber_write_contextual_tag(s, 0, credssp->identity.DomainLength + 2, true);
	ber_write_octet_string(s, (uint8*) credssp->identity.Domain, credssp->identity.DomainLength);

	/* [1] userName (OCTET STRING) */
	ber_write_contextual_tag(s, 1, credssp->identity.UserLength + 2, true);
	ber_write_octet_string(s, (uint8*) credssp->identity.User, credssp->identity.UserLength);

	/* [2] password (OCTET STRING) */
	ber_write_contextual_tag(s, 2, credssp->identity.PasswordLength + 2, true);
	ber_write_octet_string(s, (uint8*) credssp->identity.Password, credssp->identity.PasswordLength);
}

int credssp_skip_ts_credentials(rdpCredssp* credssp)
{
	int length;
	int ts_password_creds_length;
	int ts_credentials_length = 0;

	length = ber_skip_integer(0);
	length += ber_skip_contextual_tag(length);
	ts_credentials_length += length;

	ts_password_creds_length = credssp_skip_ts_password_creds(credssp);
	length = ber_skip_octet_string(ts_password_creds_length);
	length += ber_skip_contextual_tag(length);
	ts_credentials_length += length;

	length = ber_skip_sequence(ts_credentials_length);

	return length;
}

void credssp_write_ts_credentials(rdpCredssp* credssp, STREAM* s)
{
	int length;
	int ts_password_creds_length;

	length = credssp_skip_ts_credentials(credssp);
	ts_password_creds_length = credssp_skip_ts_password_creds(credssp);

	/* TSCredentials (SEQUENCE) */
	length = ber_get_content_length(length);
	length -= ber_write_sequence_tag(s, length);

	/* [0] credType (INTEGER) */
	length -= ber_write_contextual_tag(s, 0, 3, true);
	length -= ber_write_integer(s, 1);

	/* [1] credentials (OCTET STRING) */
	length -= 1;
	length -= ber_write_contextual_tag(s, 1, length, true);
	length -= ber_write_octet_string_tag(s, ts_password_creds_length);

	credssp_write_ts_password_creds(credssp, s);
}

/**
 * Encode TSCredentials structure.
 * @param credssp
 */

void credssp_encode_ts_credentials(rdpCredssp* credssp)
{
	STREAM* s;
	int length;

	s = stream_new(0);
	length = credssp_skip_ts_credentials(credssp);
	sspi_SecBufferAlloc(&credssp->ts_credentials, length);
	stream_attach(s, credssp->ts_credentials.pvBuffer, length);

	credssp_write_ts_credentials(credssp, s);
	stream_detach(s);
	stream_free(s);
}

int credssp_skip_nego_token(int length)
{
	length = ber_skip_octet_string(length);
	length += ber_skip_contextual_tag(length);
	return length;
}

int credssp_skip_nego_tokens(int length)
{
	length = credssp_skip_nego_token(length);
	length += ber_skip_sequence_tag(length);
	length += ber_skip_sequence_tag(length);
	length += ber_skip_contextual_tag(length);
	return length;
}

int credssp_skip_pub_key_auth(int length)
{
	length = ber_skip_octet_string(length);
	length += ber_skip_contextual_tag(length);
	return length;
}

int credssp_skip_auth_info(int length)
{
	length = ber_skip_octet_string(length);
	length += ber_skip_contextual_tag(length);
	return length;
}

int credssp_skip_ts_request(int length)
{
	length += ber_skip_integer(2);
	length += ber_skip_contextual_tag(3);
	length += ber_skip_sequence_tag(length);
	return length;
}

/**
 * Send CredSSP message.
 * @param credssp
 */

void credssp_send(rdpCredssp* credssp)
{
	STREAM* s;
	int length;
	int ts_request_length;
	int nego_tokens_length;
	int pub_key_auth_length;
	int auth_info_length;

	nego_tokens_length = (credssp->negoToken.cbBuffer > 0) ? credssp_skip_nego_tokens(credssp->negoToken.cbBuffer) : 0;
	pub_key_auth_length = (credssp->pubKeyAuth.cbBuffer > 0) ? credssp_skip_pub_key_auth(credssp->pubKeyAuth.cbBuffer) : 0;
	auth_info_length = (credssp->authInfo.cbBuffer > 0) ? credssp_skip_auth_info(credssp->authInfo.cbBuffer) : 0;

	length = nego_tokens_length + pub_key_auth_length + auth_info_length;
	ts_request_length = credssp_skip_ts_request(length);

	s = stream_new(ts_request_length);

	/* TSRequest */
	length = ber_get_content_length(ts_request_length);
	ber_write_sequence_tag(s, length); /* SEQUENCE */
	ber_write_contextual_tag(s, 0, 3, true); /* [0] version */
	ber_write_integer(s, 2); /* INTEGER */

	/* [1] negoTokens (NegoData) */
	if (nego_tokens_length > 0)
	{
		length = ber_get_content_length(nego_tokens_length);
		length -= ber_write_contextual_tag(s, 1, length, true); /* NegoData */
		length -= ber_write_sequence_tag(s, length); /* SEQUENCE OF NegoDataItem */
		length -= ber_write_sequence_tag(s, length); /* NegoDataItem */
		length -= ber_write_contextual_tag(s, 0, length, true); /* [0] negoToken */
		ber_write_octet_string(s, credssp->negoToken.pvBuffer, length); /* OCTET STRING */
	}

	/* [2] authInfo (OCTET STRING) */
	if (auth_info_length > 0)
	{
		length = ber_get_content_length(auth_info_length);
		length -= ber_write_contextual_tag(s, 2, length, true);
		ber_write_octet_string(s, credssp->authInfo.pvBuffer, credssp->authInfo.cbBuffer);
	}

	/* [3] pubKeyAuth (OCTET STRING) */
	if (pub_key_auth_length > 0)
	{
		length = ber_get_content_length(pub_key_auth_length);
		length -= ber_write_contextual_tag(s, 3, length, true);
		ber_write_octet_string(s, credssp->pubKeyAuth.pvBuffer, length);
	}

	tls_write(credssp->tls, s->data, stream_get_length(s));
	stream_free(s);
}

/**
 * Receive CredSSP message.
 * @param credssp
 * @return
 */

int credssp_recv(rdpCredssp* credssp)
{
	STREAM* s;
	int length;
	int status;
	uint32 version;

	s = stream_new(2048);
	status = tls_read(credssp->tls, s->data, stream_get_left(s));

	if (status < 0)
		return -1;

	/* TSRequest */
	ber_read_sequence_tag(s, &length);
	ber_read_contextual_tag(s, 0, &length, true);
	ber_read_integer(s, &version);

	/* [1] negoTokens (NegoData) */
	if (ber_read_contextual_tag(s, 1, &length, true) != false)
	{
		ber_read_sequence_tag(s, &length); /* SEQUENCE OF NegoDataItem */
		ber_read_sequence_tag(s, &length); /* NegoDataItem */
		ber_read_contextual_tag(s, 0, &length, true); /* [0] negoToken */
		ber_read_octet_string(s, &length); /* OCTET STRING */
		sspi_SecBufferAlloc(&credssp->negoToken, length);
		stream_read(s, credssp->negoToken.pvBuffer, length);
		credssp->negoToken.cbBuffer = length;
	}

	/* [2] authInfo (OCTET STRING) */
	if (ber_read_contextual_tag(s, 2, &length, true) != false)
	{
		ber_read_octet_string(s, &length); /* OCTET STRING */
		sspi_SecBufferAlloc(&credssp->authInfo, length);
		stream_read(s, credssp->authInfo.pvBuffer, length);
		credssp->authInfo.cbBuffer = length;
	}

	/* [3] pubKeyAuth (OCTET STRING) */
	if (ber_read_contextual_tag(s, 3, &length, true) != false)
	{
		ber_read_octet_string(s, &length); /* OCTET STRING */
		sspi_SecBufferAlloc(&credssp->pubKeyAuth, length);
		stream_read(s, credssp->pubKeyAuth.pvBuffer, length);
		credssp->pubKeyAuth.cbBuffer = length;
	}

	stream_free(s);

	return 0;
}

void credssp_buffer_print(rdpCredssp* credssp)
{
	if (credssp->negoToken.cbBuffer > 0)
	{
		printf("CredSSP.negoToken (length = %d):\n", credssp->negoToken.cbBuffer);
		freerdp_hexdump(credssp->negoToken.pvBuffer, credssp->negoToken.cbBuffer);
	}

	if (credssp->pubKeyAuth.cbBuffer > 0)
	{
		printf("CredSSP.pubKeyAuth (length = %d):\n", credssp->pubKeyAuth.cbBuffer);
		freerdp_hexdump(credssp->pubKeyAuth.pvBuffer, credssp->pubKeyAuth.cbBuffer);
	}

	if (credssp->authInfo.cbBuffer > 0)
	{
		printf("CredSSP.authInfo (length = %d):\n", credssp->authInfo.cbBuffer);
		freerdp_hexdump(credssp->authInfo.pvBuffer, credssp->authInfo.cbBuffer);
	}
}

void credssp_buffer_free(rdpCredssp* credssp)
{
	sspi_SecBufferFree(&credssp->negoToken);
	sspi_SecBufferFree(&credssp->pubKeyAuth);
	sspi_SecBufferFree(&credssp->authInfo);
}

/**
 * Create new CredSSP state machine.
 * @param transport
 * @return new CredSSP state machine.
 */

rdpCredssp* credssp_new(freerdp* instance, rdpTls* tls, rdpSettings* settings)
{
	rdpCredssp* credssp;

	credssp = (rdpCredssp*) xzalloc(sizeof(rdpCredssp));

	if (credssp != NULL)
	{
		credssp->instance = instance;
		credssp->settings = settings;
		credssp->server = settings->server_mode;
		credssp->tls = tls;
		credssp->send_seq_num = 0;
		credssp->recv_seq_num = 0;
		credssp->uniconv = freerdp_uniconv_new();
		memset(&credssp->negoToken, 0, sizeof(SecBuffer));
		memset(&credssp->pubKeyAuth, 0, sizeof(SecBuffer));
		memset(&credssp->authInfo, 0, sizeof(SecBuffer));
	}

	return credssp;
}

/**
 * Free CredSSP state machine.
 * @param credssp
 */

void credssp_free(rdpCredssp* credssp)
{
	if (credssp != NULL)
	{
		if (credssp->table)
			credssp->table->DeleteSecurityContext(&credssp->context);
		sspi_SecBufferFree(&credssp->PublicKey);
		sspi_SecBufferFree(&credssp->ts_credentials);
		freerdp_uniconv_free(credssp->uniconv);
		xfree(credssp->identity.User);
		xfree(credssp->identity.Domain);
		xfree(credssp->identity.Password);
		xfree(credssp);
	}
}

/* SSPI */

const SecurityFunctionTableA CREDSSP_SecurityFunctionTableA =
{
	1, /* dwVersion */
	NULL, /* EnumerateSecurityPackages */
	NULL, /* QueryCredentialsAttributes */
	NULL, /* AcquireCredentialsHandle */
	NULL, /* FreeCredentialsHandle */
	NULL, /* Reserved2 */
	NULL, /* InitializeSecurityContext */
	NULL, /* AcceptSecurityContext */
	NULL, /* CompleteAuthToken */
	NULL, /* DeleteSecurityContext */
	NULL, /* ApplyControlToken */
	NULL, /* QueryContextAttributes */
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
	NULL /* SetContextAttributes */
};

const SecurityFunctionTableW CREDSSP_SecurityFunctionTableW =
{
	1, /* dwVersion */
	NULL, /* EnumerateSecurityPackages */
	NULL, /* QueryCredentialsAttributes */
	NULL, /* AcquireCredentialsHandle */
	NULL, /* FreeCredentialsHandle */
	NULL, /* Reserved2 */
	NULL, /* InitializeSecurityContext */
	NULL, /* AcceptSecurityContext */
	NULL, /* CompleteAuthToken */
	NULL, /* DeleteSecurityContext */
	NULL, /* ApplyControlToken */
	NULL, /* QueryContextAttributes */
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
	NULL /* SetContextAttributes */
};

const SecPkgInfoA CREDSSP_SecPkgInfoA =
{
	0x000110733, /* fCapabilities */
	1, /* wVersion */
	0xFFFF, /* wRPCID */
	0x000090A8, /* cbMaxToken */
	"CREDSSP", /* Name */
	"Microsoft CredSSP Security Provider" /* Comment */
};

const SecPkgInfoW CREDSSP_SecPkgInfoW =
{
	0x000110733, /* fCapabilities */
	1, /* wVersion */
	0xFFFF, /* wRPCID */
	0x000090A8, /* cbMaxToken */
	L"CREDSSP", /* Name */
	L"Microsoft CredSSP Security Provider" /* Comment */
};
