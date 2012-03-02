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

#ifndef _WIN32
#include <unistd.h>
#endif

#include <time.h>
#include <freerdp/crypto/tls.h>
#include <freerdp/utils/stream.h>

#include <freerdp/auth/sspi.h>
#include <freerdp/auth/credssp.h>

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

void credssp_SetContextIdentity(rdpCredssp* context, SEC_AUTH_IDENTITY* identity)
{
	size_t size;
	context->identity.Flags = SEC_AUTH_IDENTITY_UNICODE;

	if (identity->Flags == SEC_AUTH_IDENTITY_ANSI)
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
		memcpy(context->identity.Password, identity->User, identity->PasswordLength);
	}
}

/**
 * Initialize NTLMSSP authentication module (client).
 * @param credssp
 */

int credssp_ntlmssp_client_init(rdpCredssp* credssp)
{
	freerdp* instance;
	SEC_AUTH_IDENTITY identity;
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

	identity.User = (uint16*) xstrdup(settings->username);
	identity.UserLength = strlen(settings->username);

	if (settings->domain)
	{
		identity.Domain = (uint16*) xstrdup(settings->domain);
		identity.DomainLength = strlen(settings->domain);
	}
	else
	{
		identity.Domain = (uint16*) NULL;
		identity.DomainLength = 0;
	}

	identity.Password = (uint16*) xstrdup(settings->password);
	identity.PasswordLength = strlen(settings->password);

	identity.Flags = SEC_AUTH_IDENTITY_ANSI;

	credssp_SetContextIdentity(credssp, &identity);

	sspi_SecBufferAlloc(&credssp->PublicKey, credssp->tls->public_key.length);
	memcpy(credssp->PublicKey.pvBuffer, credssp->tls->public_key.data, credssp->tls->public_key.length);

	return 1;
}

#define NTLM_PACKAGE_NAME		"NTLM"

int credssp_client_authenticate(rdpCredssp* credssp)
{
	uint32 cbMaxLen;
	uint32 fContextReq;
	uint32 pfContextAttr;
	SECURITY_STATUS status;
	CRED_HANDLE credentials;
	SEC_TIMESTAMP expiration;
	SEC_PKG_INFO* pPackageInfo;
	SEC_AUTH_IDENTITY identity;
	SEC_BUFFER* p_sec_buffer;
	SEC_BUFFER input_sec_buffer;
	SEC_BUFFER output_sec_buffer;
	SEC_BUFFER_DESC input_sec_buffer_desc;
	SEC_BUFFER_DESC output_sec_buffer_desc;
	boolean have_context;
	boolean have_input_buffer;
	boolean have_pub_key_auth;
	rdpSettings* settings = credssp->settings;

	sspi_GlobalInit();

	if (credssp_ntlmssp_client_init(credssp) == 0)
		return 0;

	credssp->table = InitSecurityInterface();

	status = QuerySecurityPackageInfo(NTLM_PACKAGE_NAME, &pPackageInfo);

	if (status != SEC_E_OK)
	{
		printf("QuerySecurityPackageInfo status: 0x%08X\n", status);
		return 0;
	}

	cbMaxLen = pPackageInfo->cbMaxToken;

	identity.User = (uint16*) xstrdup(settings->username);
	identity.UserLength = strlen(settings->username);

	if (settings->domain)
	{
		identity.Domain = (uint16*) xstrdup(settings->domain);
		identity.DomainLength = strlen(settings->domain);
	}
	else
	{
		identity.Domain = (uint16*) NULL;
		identity.DomainLength = 0;
	}

	identity.Password = (uint16*) xstrdup(settings->password);
	identity.PasswordLength = strlen(settings->password);

	identity.Flags = SEC_AUTH_IDENTITY_ANSI;

	status = credssp->table->AcquireCredentialsHandle(NULL, NTLM_PACKAGE_NAME,
			SECPKG_CRED_OUTBOUND, NULL, &identity, NULL, NULL, &credentials, &expiration);

	if (status != SEC_E_OK)
	{
		printf("AcquireCredentialsHandle status: 0x%08X\n", status);
		return 0;
	}

	have_context = false;
	have_input_buffer = false;
	have_pub_key_auth = false;
	memset(&input_sec_buffer, 0, sizeof(SEC_BUFFER));
	memset(&output_sec_buffer, 0, sizeof(SEC_BUFFER));
	memset(&credssp->ContextSizes, 0, sizeof(SEC_PKG_CONTEXT_SIZES));

	fContextReq = ISC_REQ_REPLAY_DETECT | ISC_REQ_SEQUENCE_DETECT |
			ISC_REQ_CONFIDENTIALITY | ISC_REQ_DELEGATE;

	while (true)
	{
		output_sec_buffer_desc.ulVersion = SECBUFFER_VERSION;
		output_sec_buffer_desc.cBuffers = 1;
		output_sec_buffer_desc.pBuffers = &output_sec_buffer;
		output_sec_buffer.BufferType = SECBUFFER_TOKEN;
		output_sec_buffer.cbBuffer = cbMaxLen;
		output_sec_buffer.pvBuffer = xmalloc(output_sec_buffer.cbBuffer);

		status = credssp->table->InitializeSecurityContext(&credentials,
				(have_context) ? &credssp->context : NULL,
				NULL, fContextReq, 0, SECURITY_NATIVE_DREP,
				(have_input_buffer) ? &input_sec_buffer_desc : NULL,
				0, &credssp->context, &output_sec_buffer_desc, &pfContextAttr, &expiration);

		if (input_sec_buffer.pvBuffer != NULL)
		{
			xfree(input_sec_buffer.pvBuffer);
			input_sec_buffer.pvBuffer = NULL;
		}

		if ((status == SEC_I_COMPLETE_AND_CONTINUE) || (status == SEC_I_COMPLETE_NEEDED))
		{
			if (credssp->table->CompleteAuthToken != NULL)
				credssp->table->CompleteAuthToken(&credssp->context, &output_sec_buffer_desc);

			have_pub_key_auth = true;

			if (credssp->table->QueryContextAttributes(&credssp->context, SECPKG_ATTR_SIZES, &credssp->ContextSizes) != SEC_E_OK)
			{
				printf("QueryContextAttributes SECPKG_ATTR_SIZES failure\n");
				return 0;
			}

			if (have_pub_key_auth)
			{
				uint8* p;
				SEC_BUFFER Buffers[2];
				SEC_BUFFER_DESC Message;

				Buffers[0].BufferType = SECBUFFER_DATA; /* TLS Public Key */
				Buffers[1].BufferType = SECBUFFER_PADDING; /* Signature */

				Buffers[0].cbBuffer = credssp->PublicKey.cbBuffer;
				Buffers[0].pvBuffer = xmalloc(Buffers[0].cbBuffer);
				memcpy(Buffers[0].pvBuffer, credssp->PublicKey.pvBuffer, Buffers[0].cbBuffer);

				Buffers[1].cbBuffer = credssp->ContextSizes.cbMaxSignature;
				Buffers[1].pvBuffer = xzalloc(Buffers[1].cbBuffer);

				Message.cBuffers = 2;
				Message.ulVersion = SECBUFFER_VERSION;
				Message.pBuffers = (SEC_BUFFER*) &Buffers;

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

		/* send authentication token to server */

		if (output_sec_buffer.cbBuffer > 0)
		{
			p_sec_buffer = &output_sec_buffer_desc.pBuffers[0];

			credssp->negoToken.pvBuffer = p_sec_buffer->pvBuffer;
			credssp->negoToken.cbBuffer = p_sec_buffer->cbBuffer;

#ifdef WITH_DEBUG_CREDSSP
			printf("Sending Authentication Token\n");
			freerdp_hexdump(credssp->negoToken.data, credssp->negoToken.length);
#endif

			credssp_send(credssp, &credssp->negoToken, NULL,
					(have_pub_key_auth) ? &credssp->pubKeyAuth : NULL);

			if (have_pub_key_auth)
			{
				have_pub_key_auth = false;
				sspi_SecBufferFree(&credssp->pubKeyAuth);
			}

			xfree(output_sec_buffer.pvBuffer);
			output_sec_buffer.pvBuffer = NULL;
		}

		if (status != SEC_I_CONTINUE_NEEDED)
			break;

		/* receive server response and place in input buffer */

		input_sec_buffer_desc.ulVersion = SECBUFFER_VERSION;
		input_sec_buffer_desc.cBuffers = 1;
		input_sec_buffer_desc.pBuffers = &input_sec_buffer;
		input_sec_buffer.BufferType = SECBUFFER_TOKEN;

		if (credssp_recv(credssp, &credssp->negoToken, NULL, NULL) < 0)
			return -1;

#ifdef WITH_DEBUG_CREDSSP
		printf("Receiving Authentication Token\n");
		freerdp_hexdump(credssp->negoToken.data, credssp->negoToken.length);
#endif

		p_sec_buffer = &input_sec_buffer_desc.pBuffers[0];
		p_sec_buffer->pvBuffer = credssp->negoToken.pvBuffer;
		p_sec_buffer->cbBuffer = credssp->negoToken.cbBuffer;

		have_input_buffer = true;
		have_context = true;
	}

	/* Encrypted Public Key +1 */
	if (credssp_recv(credssp, &credssp->negoToken, NULL, &credssp->pubKeyAuth) < 0)
		return -1;

	/* Verify Server Public Key Echo */

	status = credssp_verify_public_key_echo(credssp);

	if (status != SEC_E_OK)
		return 0;

	/* Send encrypted credentials */

	status = credssp_encrypt_ts_credentials(credssp);

	if (status != SEC_E_OK)
		return 0;

	credssp_send(credssp, NULL, &credssp->authInfo, NULL);

	/* Free resources */

	//sspi_SecBufferFree(&credssp->negoToken);
	sspi_SecBufferFree(&credssp->authInfo);

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
	return 0;
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
	SEC_BUFFER Buffers[2];
	SEC_BUFFER_DESC Message;
	SECURITY_STATUS status;

	length = credssp->pubKeyAuth.cbBuffer;
	pub_key_auth = (uint8*) credssp->pubKeyAuth.pvBuffer;
	public_key_length = credssp->PublicKey.cbBuffer;

	Buffers[0].BufferType = SECBUFFER_PADDING; /* Signature */
	Buffers[1].BufferType = SECBUFFER_DATA; /* Encrypted TLS Public Key */

	Buffers[0].cbBuffer = credssp->ContextSizes.cbMaxSignature;
	Buffers[0].pvBuffer = xmalloc(Buffers[0].cbBuffer);
	memcpy(Buffers[0].pvBuffer, pub_key_auth, Buffers[0].cbBuffer);

	Buffers[1].cbBuffer = length - Buffers[0].cbBuffer;
	Buffers[1].pvBuffer = xmalloc(Buffers[1].cbBuffer);
	memcpy(Buffers[1].pvBuffer, &pub_key_auth[Buffers[0].cbBuffer], Buffers[1].cbBuffer);

	Message.cBuffers = 2;
	Message.ulVersion = SECBUFFER_VERSION;
	Message.pBuffers = (SEC_BUFFER*) &Buffers;

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

	return SEC_E_OK;
}

SECURITY_STATUS credssp_encrypt_ts_credentials(rdpCredssp* credssp)
{
	uint8* p;
	SEC_BUFFER Buffers[2];
	SEC_BUFFER_DESC Message;
	SECURITY_STATUS status;

	credssp_encode_ts_credentials(credssp);

	Buffers[0].BufferType = SECBUFFER_DATA; /* TSCredentials */
	Buffers[1].BufferType = SECBUFFER_PADDING; /* Signature */

	Buffers[0].cbBuffer = credssp->ts_credentials.cbBuffer;
	Buffers[0].pvBuffer = xmalloc(Buffers[0].cbBuffer);
	memcpy(Buffers[0].pvBuffer, credssp->ts_credentials.pvBuffer, Buffers[0].cbBuffer);

	Buffers[1].cbBuffer = 16;
	Buffers[1].pvBuffer = xzalloc(Buffers[1].cbBuffer);

	Message.cBuffers = 2;
	Message.ulVersion = SECBUFFER_VERSION;
	Message.pBuffers = (SEC_BUFFER*) &Buffers;

	sspi_SecBufferAlloc(&credssp->authInfo, Buffers[0].cbBuffer + Buffers[1].cbBuffer);

	status = credssp->table->EncryptMessage(&credssp->context, 0, &Message, 1);

	if (status != SEC_E_OK)
		return status;

	p = (uint8*) credssp->authInfo.pvBuffer;
	memcpy(p, Buffers[1].pvBuffer, Buffers[1].cbBuffer); /* Message Signature */
	memcpy(&p[Buffers[1].cbBuffer], Buffers[0].pvBuffer, Buffers[0].cbBuffer); /* Encrypted TSCredentials */

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
 * @param negoToken
 * @param authInfo
 * @param pubKeyAuth
 */

void credssp_send(rdpCredssp* credssp, SEC_BUFFER* negoToken, SEC_BUFFER* authInfo, SEC_BUFFER* pubKeyAuth)
{
	STREAM* s;
	int length;
	int ts_request_length;
	int nego_tokens_length;
	int pub_key_auth_length;
	int auth_info_length;

	nego_tokens_length = (negoToken != NULL) ? credssp_skip_nego_tokens(negoToken->cbBuffer) : 0;
	pub_key_auth_length = (pubKeyAuth != NULL) ? credssp_skip_pub_key_auth(pubKeyAuth->cbBuffer) : 0;
	auth_info_length = (authInfo != NULL) ? credssp_skip_auth_info(authInfo->cbBuffer) : 0;

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
		ber_write_octet_string(s, negoToken->pvBuffer, length); /* OCTET STRING */
	}

	/* [2] authInfo (OCTET STRING) */
	if (auth_info_length > 0)
	{
		length = ber_get_content_length(auth_info_length);
		length -= ber_write_contextual_tag(s, 2, length, true);
		ber_write_octet_string(s, authInfo->pvBuffer, authInfo->cbBuffer);
	}

	/* [3] pubKeyAuth (OCTET STRING) */
	if (pub_key_auth_length > 0)
	{
		length = ber_get_content_length(pub_key_auth_length);
		length -= ber_write_contextual_tag(s, 3, length, true);
		ber_write_octet_string(s, pubKeyAuth->pvBuffer, length);
	}

	tls_write(credssp->tls, s->data, stream_get_length(s));
	stream_free(s);
}

/**
 * Receive CredSSP message.
 * @param credssp
 * @param negoToken
 * @param authInfo
 * @param pubKeyAuth
 * @return
 */

int credssp_recv(rdpCredssp* credssp, SEC_BUFFER* negoToken, SEC_BUFFER* authInfo, SEC_BUFFER* pubKeyAuth)
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
		sspi_SecBufferAlloc(negoToken, length);
		stream_read(s, negoToken->pvBuffer, length);
	}

	/* [2] authInfo (OCTET STRING) */
	if (ber_read_contextual_tag(s, 2, &length, true) != false)
	{
		ber_read_octet_string(s, &length); /* OCTET STRING */
		sspi_SecBufferAlloc(authInfo, length);
		stream_read(s, authInfo->pvBuffer, length);
	}

	/* [3] pubKeyAuth (OCTET STRING) */
	if (ber_read_contextual_tag(s, 3, &length, true) != false)
	{
		ber_read_octet_string(s, &length); /* OCTET STRING */
		sspi_SecBufferAlloc(pubKeyAuth, length);
		stream_read(s, pubKeyAuth->pvBuffer, length);
	}

	return 0;
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
		credssp->uniconv = freerdp_uniconv_new();
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
		sspi_SecBufferFree(&credssp->ts_credentials);
		freerdp_uniconv_free(credssp->uniconv);
		xfree(credssp);
	}
}

/* SSPI */

const SECURITY_FUNCTION_TABLE CREDSSP_SECURITY_FUNCTION_TABLE =
{
	1, /* dwVersion */
	NULL, /* EnumerateSecurityPackages */
	NULL, /* Reserved1 */
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
	NULL, /* SetContextAttributes */
};

const SEC_PKG_INFO CREDSSP_SEC_PKG_INFO =
{
	0x000110733, /* fCapabilities */
	1, /* wVersion */
	0xFFFF, /* wRPCID */
	0x000090A8, /* cbMaxToken */
	"CREDSSP", /* Name */
	"Microsoft CredSSP Security Provider" /* Comment */
};
