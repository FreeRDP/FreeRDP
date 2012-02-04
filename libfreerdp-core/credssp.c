/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Credential Security Support Provider (CredSSP)
 *
 * Copyright 2010 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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
#include "ntlmssp.h"

#include "credssp.h"
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

/**
 * Initialize NTLMSSP authentication module.
 * @param credssp
 */

int credssp_ntlmssp_init(rdpCredssp* credssp)
{
	freerdp* instance;
	NTLMSSP* ntlmssp = credssp->ntlmssp;
	rdpSettings* settings = credssp->transport->settings;
	instance = (freerdp*) settings->instance;

	if ((settings->password == NULL) || (settings->username == NULL))
	{
		if(instance->Authenticate)
		{
			boolean proceed = instance->Authenticate(instance,
					&settings->username, &settings->password, &settings->domain);
			if (!proceed)
				return 0;
		}
	}

	if (settings->ntlm_version == 2)
		ntlmssp->ntlm_v2 = 1;

	ntlmssp_set_password(ntlmssp, settings->password);
	ntlmssp_set_username(ntlmssp, settings->username);

	if (ntlmssp->ntlm_v2)
	{
		ntlmssp_set_workstation(ntlmssp, "WORKSTATION");
	}

	if (settings->domain != NULL)
	{
		if (strlen(settings->domain) > 0)
			ntlmssp_set_domain(ntlmssp, settings->domain);
	}
	else
	{
		ntlmssp_set_domain(ntlmssp, NULL);
	}

	ntlmssp_generate_client_challenge(ntlmssp);
	ntlmssp_generate_random_session_key(ntlmssp);
	ntlmssp_generate_exported_session_key(ntlmssp);

	return 1;
}

/**
 * Get TLS public key.
 * @param credssp
 */

int credssp_get_public_key(rdpCredssp* credssp)
{
	int status;
	CryptoCert cert;
	
	cert = tls_get_certificate(credssp->transport->tls);

	if (cert == NULL)
	{
		printf("credssp_get_public_key: tls_get_certificate failed to return the server certificate.\n");
		return 0;
	}

	if (!tls_verify_certificate(credssp->transport->tls, cert, credssp->transport->settings->hostname))
		tls_disconnect(credssp->transport->tls);

	status = crypto_cert_get_public_key(cert, &credssp->public_key);
	crypto_cert_free(cert);

	return status;
}

/**
 * Authenticate with server using CredSSP.
 * @param credssp
 * @return 1 if authentication is successful
 */

int credssp_authenticate(rdpCredssp* credssp)
{
	NTLMSSP* ntlmssp = credssp->ntlmssp;
	STREAM* s = stream_new(0);
	uint8* negoTokenBuffer = (uint8*) xmalloc(2048);

	if (credssp_ntlmssp_init(credssp) == 0)
		return 0;

	if (credssp_get_public_key(credssp) == 0)
		return 0;

	/* NTLMSSP NEGOTIATE MESSAGE */
	stream_attach(s, negoTokenBuffer, 2048);
	ntlmssp_send(ntlmssp, s);
	credssp->negoToken.data = stream_get_head(s);
	credssp->negoToken.length = stream_get_length(s);
	credssp_send(credssp, &credssp->negoToken, NULL, NULL);

	/* NTLMSSP CHALLENGE MESSAGE */
	if (credssp_recv(credssp, &credssp->negoToken, NULL, NULL) < 0)
		return -1;

	stream_attach(s, credssp->negoToken.data, credssp->negoToken.length);
	ntlmssp_recv(ntlmssp, s);

	freerdp_blob_free(&credssp->negoToken);

	/* NTLMSSP AUTHENTICATE MESSAGE */
	stream_attach(s, negoTokenBuffer, 2048);
	ntlmssp_send(ntlmssp, s);

	/* The last NTLMSSP message is sent with the encrypted public key */
	credssp->negoToken.data = stream_get_head(s);
	credssp->negoToken.length = stream_get_length(s);
	credssp_encrypt_public_key(credssp, &credssp->pubKeyAuth);
	credssp_send(credssp, &credssp->negoToken, NULL, &credssp->pubKeyAuth);
	freerdp_blob_free(&credssp->pubKeyAuth);

	/* Encrypted Public Key +1 */
	if (credssp_recv(credssp, &credssp->negoToken, NULL, &credssp->pubKeyAuth) < 0)
		return -1;

	if (credssp_verify_public_key(credssp, &credssp->pubKeyAuth) == 0)
	{
		/* Failed to verify server public key echo */
		return 0; /* DO NOT SEND CREDENTIALS! */
	}

	freerdp_blob_free(&credssp->negoToken);
	freerdp_blob_free(&credssp->pubKeyAuth);

	/* Send encrypted credentials */
	credssp_encode_ts_credentials(credssp);
	credssp_encrypt_ts_credentials(credssp, &credssp->authInfo);
	credssp_send(credssp, NULL, &credssp->authInfo, NULL);
	freerdp_blob_free(&credssp->authInfo);

	xfree(s);

	return 1;
}

/**
 * Encrypt TLS public key using CredSSP.
 * @param credssp
 * @param s
 */

void credssp_encrypt_public_key(rdpCredssp* credssp, rdpBlob* d)
{
	uint8* p;
	uint8 signature[16];
	rdpBlob encrypted_public_key;
	NTLMSSP *ntlmssp = credssp->ntlmssp;

	freerdp_blob_alloc(d, credssp->public_key.length + 16);
	ntlmssp_encrypt_message(ntlmssp, &credssp->public_key, &encrypted_public_key, signature);

#ifdef WITH_DEBUG_NLA
	printf("Public Key (length = %d)\n", credssp->public_key.length);
	freerdp_hexdump(credssp->public_key.data, credssp->public_key.length);
	printf("\n");

	printf("Encrypted Public Key (length = %d)\n", encrypted_public_key.length);
	freerdp_hexdump(encrypted_public_key.data, encrypted_public_key.length);
	printf("\n");

	printf("Signature\n");
	freerdp_hexdump(signature, 16);
	printf("\n");
#endif

	p = (uint8*) d->data;
	memcpy(p, signature, 16); /* Message Signature */
	memcpy(&p[16], encrypted_public_key.data, encrypted_public_key.length); /* Encrypted Public Key */

	freerdp_blob_free(&encrypted_public_key);
}

/**
 * Verify TLS public key using CredSSP.
 * @param credssp
 * @param s
 * @return 1 if verification is successful, 0 otherwise
 */

int credssp_verify_public_key(rdpCredssp* credssp, rdpBlob* d)
{
	uint8 *p1, *p2;
	uint8* signature;
	rdpBlob public_key;
	rdpBlob encrypted_public_key;

	signature = d->data;
	encrypted_public_key.data = (void*) (signature + 16);
	encrypted_public_key.length = d->length - 16;

	ntlmssp_decrypt_message(credssp->ntlmssp, &encrypted_public_key, &public_key, signature);

	p1 = (uint8*) credssp->public_key.data;
	p2 = (uint8*) public_key.data;

	p2[0]--;

	if (memcmp(p1, p2, public_key.length) != 0)
	{
		printf("Could not verify server's public key echo\n");
		return 0;
	}

	p2[0]++;
	freerdp_blob_free(&public_key);
	return 1;
}

/**
 * Encrypt and sign TSCredentials structure.
 * @param credssp
 * @param s
 */

void credssp_encrypt_ts_credentials(rdpCredssp* credssp, rdpBlob* d)
{
	uint8* p;
	uint8 signature[16];
	rdpBlob encrypted_ts_credentials;
	NTLMSSP* ntlmssp = credssp->ntlmssp;

	freerdp_blob_alloc(d, credssp->ts_credentials.length + 16);
	ntlmssp_encrypt_message(ntlmssp, &credssp->ts_credentials, &encrypted_ts_credentials, signature);

#ifdef WITH_DEBUG_NLA
	printf("TSCredentials (length = %d)\n", credssp->ts_credentials.length);
	freerdp_hexdump(credssp->ts_credentials.data, credssp->ts_credentials.length);
	printf("\n");

	printf("Encrypted TSCredentials (length = %d)\n", encrypted_ts_credentials.length);
	freerdp_hexdump(encrypted_ts_credentials.data, encrypted_ts_credentials.length);
	printf("\n");

	printf("Signature\n");
	freerdp_hexdump(signature, 16);
	printf("\n");
#endif

	p = (uint8*) d->data;
	memcpy(p, signature, 16); /* Message Signature */
	memcpy(&p[16], encrypted_ts_credentials.data, encrypted_ts_credentials.length); /* Encrypted TSCredentials */

	freerdp_blob_free(&encrypted_ts_credentials);
}

int credssp_skip_ts_password_creds(rdpCredssp* credssp)
{
	int length;
	int ts_password_creds_length = 0;

	length = ber_skip_octet_string(credssp->ntlmssp->domain.length);
	length += ber_skip_contextual_tag(length);
	ts_password_creds_length += length;

	length = ber_skip_octet_string(credssp->ntlmssp->username.length);
	length += ber_skip_contextual_tag(length);
	ts_password_creds_length += length;

	length = ber_skip_octet_string(credssp->ntlmssp->password.length);
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
	ber_write_contextual_tag(s, 0, credssp->ntlmssp->domain.length + 2, true);
	ber_write_octet_string(s, credssp->ntlmssp->domain.data, credssp->ntlmssp->domain.length);

	/* [1] userName (OCTET STRING) */
	ber_write_contextual_tag(s, 1, credssp->ntlmssp->username.length + 2, true);
	ber_write_octet_string(s, credssp->ntlmssp->username.data, credssp->ntlmssp->username.length);

	/* [2] password (OCTET STRING) */
	ber_write_contextual_tag(s, 2, credssp->ntlmssp->password.length + 2, true);
	ber_write_octet_string(s, credssp->ntlmssp->password.data, credssp->ntlmssp->password.length);
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
	freerdp_blob_alloc(&credssp->ts_credentials, length);
	stream_attach(s, credssp->ts_credentials.data, length);

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

void credssp_send(rdpCredssp* credssp, rdpBlob* negoToken, rdpBlob* authInfo, rdpBlob* pubKeyAuth)
{
	STREAM* s;
	int length;
	int ts_request_length;
	int nego_tokens_length;
	int pub_key_auth_length;
	int auth_info_length;

	nego_tokens_length = (negoToken != NULL) ? credssp_skip_nego_tokens(negoToken->length) : 0;
	pub_key_auth_length = (pubKeyAuth != NULL) ? credssp_skip_pub_key_auth(pubKeyAuth->length) : 0;
	auth_info_length = (authInfo != NULL) ? credssp_skip_auth_info(authInfo->length) : 0;

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
		ber_write_octet_string(s, negoToken->data, length); /* OCTET STRING */
	}

	/* [2] authInfo (OCTET STRING) */
	if (auth_info_length > 0)
	{
		length = ber_get_content_length(auth_info_length);
		length -= ber_write_contextual_tag(s, 2, length, true);
		ber_write_octet_string(s, authInfo->data, authInfo->length);
	}

	/* [3] pubKeyAuth (OCTET STRING) */
	if (pub_key_auth_length > 0)
	{
		length = ber_get_content_length(pub_key_auth_length);
		length -= ber_write_contextual_tag(s, 3, length, true);
		ber_write_octet_string(s, pubKeyAuth->data, length);
	}

	transport_write(credssp->transport, s);
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

int credssp_recv(rdpCredssp* credssp, rdpBlob* negoToken, rdpBlob* authInfo, rdpBlob* pubKeyAuth)
{
	STREAM* s;
	int length;
	int status;
	uint32 version;

	s = transport_recv_stream_init(credssp->transport, 2048);
	status = transport_read(credssp->transport, s);

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
		freerdp_blob_alloc(negoToken, length);
		stream_read(s, negoToken->data, length);
	}

	/* [2] authInfo (OCTET STRING) */
	if (ber_read_contextual_tag(s, 2, &length, true) != false)
	{
		ber_read_octet_string(s, &length); /* OCTET STRING */
		freerdp_blob_alloc(authInfo, length);
		stream_read(s, authInfo->data, length);
	}

	/* [3] pubKeyAuth (OCTET STRING) */
	if (ber_read_contextual_tag(s, 3, &length, true) != false)
	{
		ber_read_octet_string(s, &length); /* OCTET STRING */
		freerdp_blob_alloc(pubKeyAuth, length);
		stream_read(s, pubKeyAuth->data, length);
	}

	return 0;
}

/**
 * Encrypt the given plain text using RC4 and the given key.
 * @param key RC4 key
 * @param length text length
 * @param plaintext plain text
 * @param ciphertext cipher text
 */

void credssp_rc4k(uint8* key, int length, uint8* plaintext, uint8* ciphertext)
{
	CryptoRc4 rc4;

	/* Initialize RC4 cipher with key */
	rc4 = crypto_rc4_init((void*) key, 16);

	/* Encrypt plaintext with key */
	crypto_rc4(rc4, length, (void*) plaintext, (void*) ciphertext);

	/* Free RC4 Cipher */
	crypto_rc4_free(rc4);
}

/**
 * Get current time, in tenths of microseconds since midnight of January 1, 1601.
 * @param[out] timestamp 64-bit little-endian timestamp
 */

void credssp_current_time(uint8* timestamp)
{
	uint64 time64;

	/* Timestamp (8 bytes), represented as the number of tenths of microseconds since midnight of January 1, 1601 */
	time64 = time(NULL) + 11644473600LL; /* Seconds since January 1, 1601 */
	time64 *= 10000000; /* Convert timestamp to tenths of a microsecond */

	memcpy(timestamp, &time64, 8); /* Copy into timestamp in little-endian */
}

/**
 * Create new CredSSP state machine.
 * @param transport
 * @return new CredSSP state machine.
 */

rdpCredssp* credssp_new(rdpTransport* transport)
{
	rdpCredssp* self;

	self = (rdpCredssp*) xzalloc(sizeof(rdpCredssp));

	if (self != NULL)
	{
		self->transport = transport;
		self->send_seq_num = 0;
		self->ntlmssp = ntlmssp_new();
		self->settings = transport->settings;
	}

	return self;
}

/**
 * Free CredSSP state machine.
 * @param credssp
 */

void credssp_free(rdpCredssp* credssp)
{
	if (credssp != NULL)
	{
		freerdp_blob_free(&credssp->public_key);
		freerdp_blob_free(&credssp->ts_credentials);

		ntlmssp_free(credssp->ntlmssp);
		xfree(credssp);
	}
}
