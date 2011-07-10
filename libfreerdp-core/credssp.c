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
 *     http://www.apache.org/licenses/LICENSE-2.0
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

#include "TSRequest.h"
#include "NegoData.h"
#include "NegotiationToken.h"
#include "TSCredentials.h"
#include "TSPasswordCreds.h"

#include <time.h>
#include "ntlmssp.h"

#include "credssp.h"

static int
asn1_write(const void *buffer, size_t size, void *fd)
{
	/* this is used to get the size of the ASN.1 encoded result */
	return 0;
}

/**
 * Initialize NTLMSSP authentication module.
 * @param credssp
 */

void credssp_ntlmssp_init(rdpCredssp* credssp)
{
	NTLMSSP *ntlmssp = credssp->ntlmssp;
	rdpSettings *settings = credssp->transport->settings;

	ntlmssp_set_password(ntlmssp, settings->password);
	ntlmssp_set_username(ntlmssp, settings->username);

	if (settings->domain != NULL)
	{
		if (strlen(settings->domain) > 0)
		{
			ntlmssp_set_domain(ntlmssp, settings->domain);
		}
	}
	else
	{
		ntlmssp_set_domain(ntlmssp, NULL);
	}

	ntlmssp_generate_client_challenge(ntlmssp);
	ntlmssp_generate_random_session_key(ntlmssp);
	ntlmssp_generate_exported_session_key(ntlmssp);
	
	ntlmssp->ntlm_v2 = 0;
}

/**
 * Get TLS public key.
 * @param credssp
 */

int credssp_get_public_key(rdpCredssp* credssp)
{
	int ret;
	CryptoCert cert;

	cert = tls_get_certificate(credssp->transport->tls);

	if (cert == NULL)
	{
		printf("credssp_get_public_key: tls_get_certificate failed to return the server certificate.\n");
		return 0;
	}

	ret = crypto_cert_get_public_key(cert, &credssp->public_key);
	crypto_cert_free(cert);

	return ret;
}

/**
 * Authenticate with server using CredSSP.
 * @param credssp
 * @return 1 if authentication is successful
 */

int credssp_authenticate(rdpCredssp* credssp)
{
	NTLMSSP *ntlmssp = credssp->ntlmssp;
	STREAM* s = stream_new(0);
	uint8* negoTokenBuffer = (uint8*) xmalloc(2048);

	credssp_ntlmssp_init(credssp);

	if (credssp_get_public_key(credssp) == 0)
		return 0;

	/* NTLMSSP NEGOTIATE MESSAGE */
	s->p = s->data = negoTokenBuffer;
	ntlmssp_send(ntlmssp, s);
	credssp->negoToken.data = s->data;
	credssp->negoToken.length = s->p - s->data;
	credssp_send(credssp, &credssp->negoToken, NULL, NULL);

	/* NTLMSSP CHALLENGE MESSAGE */
	if (credssp_recv(credssp, &credssp->negoToken, NULL, NULL) < 0)
		return -1;

	s->p = s->data = credssp->negoToken.data;
	ntlmssp_recv(ntlmssp, s);

	freerdp_blob_free(&credssp->negoToken);

	/* NTLMSSP AUTHENTICATE MESSAGE */
	s->p = s->data = negoTokenBuffer;
	ntlmssp_send(ntlmssp, s);

	/* The last NTLMSSP message is sent with the encrypted public key */
	credssp->negoToken.data = s->data;
	credssp->negoToken.length = s->p - s->data;
	credssp_encrypt_public_key(credssp, &credssp->pubKeyAuth);
	credssp_send(credssp, &credssp->negoToken, &credssp->pubKeyAuth, NULL);

	/* Encrypted Public Key +1 */
	if (credssp_recv(credssp, &credssp->negoToken, &credssp->pubKeyAuth, NULL) < 0)
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
	credssp_send(credssp, NULL, NULL, &credssp->authInfo);

	xfree(s);

	return 1;
}

/**
 * Encrypt TLS public key using CredSSP.
 * @param credssp
 * @param s
 */

void credssp_encrypt_public_key(rdpCredssp* credssp, BLOB* d)
{
	uint8 *p;
	uint8 signature[16];
	BLOB encrypted_public_key;
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

int credssp_verify_public_key(rdpCredssp* credssp, BLOB* d)
{
	uint8 *p1, *p2;
	uint8 *signature;
	BLOB public_key;
	BLOB encrypted_public_key;

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

void credssp_encrypt_ts_credentials(rdpCredssp* credssp, BLOB* d)
{
	uint8 *p;
	uint8 signature[16];
	BLOB encrypted_ts_credentials;
	NTLMSSP *ntlmssp = credssp->ntlmssp;

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

/**
 * Encode TSCredentials structure.
 * @param credssp
 */

void credssp_encode_ts_credentials(rdpCredssp* credssp)
{
	asn_enc_rval_t enc_rval;
	TSCredentials_t *ts_credentials;
	TSPasswordCreds_t *ts_passwoFRDP_creds;
	BLOB ts_passwoFRDP_creds_buffer = { 0 };

	ts_credentials = calloc(1, sizeof(TSCredentials_t));
	ts_credentials->credType = 1; /* TSPasswordCreds */

	ts_passwoFRDP_creds = calloc(1, sizeof(TSPasswordCreds_t));

	/* Domain */
	ts_passwoFRDP_creds->domainName.buf = credssp->ntlmssp->domain.data;
	ts_passwoFRDP_creds->domainName.size = credssp->ntlmssp->domain.length;

	/* Username */
	ts_passwoFRDP_creds->userName.buf = credssp->ntlmssp->username.data;
	ts_passwoFRDP_creds->userName.size = credssp->ntlmssp->username.length;

	/* Password */
	ts_passwoFRDP_creds->password.buf = credssp->ntlmssp->password.data;
	ts_passwoFRDP_creds->password.size = credssp->ntlmssp->password.length;

	/* get size ASN.1 encoded TSPasswordCreds */
	enc_rval = der_encode(&asn_DEF_TSPasswordCreds, ts_passwoFRDP_creds, asn1_write, 0);

	if (enc_rval.encoded != -1)
	{
		freerdp_blob_alloc(&ts_passwoFRDP_creds_buffer, enc_rval.encoded);

		enc_rval = der_encode_to_buffer(&asn_DEF_TSPasswordCreds, ts_passwoFRDP_creds,
			ts_passwoFRDP_creds_buffer.data, ts_passwoFRDP_creds_buffer.length);
	}

	ts_credentials->credentials.buf = ts_passwoFRDP_creds_buffer.data;
	ts_credentials->credentials.size = ts_passwoFRDP_creds_buffer.length;

	/* get size ASN.1 encoded TSCredentials */
	enc_rval = der_encode(&asn_DEF_TSCredentials, ts_credentials, asn1_write, 0);

	if (enc_rval.encoded != -1)
	{
		freerdp_blob_alloc(&credssp->ts_credentials, enc_rval.encoded);

		enc_rval = der_encode_to_buffer(&asn_DEF_TSCredentials, ts_credentials,
			credssp->ts_credentials.data, credssp->ts_credentials.length);
	}

	freerdp_blob_free(&ts_passwoFRDP_creds_buffer);
	free(ts_credentials);
	free(ts_passwoFRDP_creds);
}

/**
 * Send CredSSP message.
 * @param credssp
 * @param negoToken
 * @param pubKeyAuth
 * @param authInfo
 */

void credssp_send(rdpCredssp* credssp, BLOB* negoToken, BLOB* pubKeyAuth, BLOB* authInfo)
{
	STREAM* s;
	size_t size;
	TSRequest_t *ts_request;
	OCTET_STRING_t *nego_token;
	asn_enc_rval_t enc_rval;

	ts_request = calloc(1, sizeof(TSRequest_t));
	ts_request->version = 2;

	if (negoToken != NULL)
	{
		ts_request->negoTokens = calloc(1, sizeof(NegoData_t));
		nego_token = calloc(1, sizeof(OCTET_STRING_t));
		nego_token->size = negoToken->length;
		nego_token->buf = malloc(nego_token->size);
		memcpy(nego_token->buf, negoToken->data, nego_token->size);
		ASN_SEQUENCE_ADD(ts_request->negoTokens, nego_token);
	}

	if (pubKeyAuth != NULL)
	{
		ts_request->pubKeyAuth = calloc(1, sizeof(OCTET_STRING_t));
		ts_request->pubKeyAuth->buf = pubKeyAuth->data;
		ts_request->pubKeyAuth->size = pubKeyAuth->length;
	}

	if (authInfo != NULL)
	{
		ts_request->authInfo = calloc(1, sizeof(OCTET_STRING_t));
		ts_request->authInfo->buf = authInfo->data;
		ts_request->authInfo->size = authInfo->length;
	}

	/* get size of the encoded ASN.1 payload */
	enc_rval = der_encode(&asn_DEF_TSRequest, ts_request, asn1_write, 0);

	if (enc_rval.encoded != -1)
	{
		size = enc_rval.encoded;
		s = transport_send_stream_init(credssp->transport, size);

		enc_rval = der_encode_to_buffer(&asn_DEF_TSRequest, ts_request, s->data, size);

		if (enc_rval.encoded != -1)
		{
			s->p = s->data + size;
			transport_write(credssp->transport, s);
		}

		asn_DEF_TSRequest.free_struct(&asn_DEF_TSRequest, ts_request, 0);
	}
}

/**
 * Receive CredSSP message.
 * @param credssp
 * @param negoToken
 * @param pubKeyAuth
 * @param authInfo
 * @return
 */

int credssp_recv(rdpCredssp* credssp, BLOB* negoToken, BLOB* pubKeyAuth, BLOB* authInfo)
{
	STREAM* s;
	int status;
	asn_dec_rval_t dec_rval;
	TSRequest_t *ts_request = 0;

	s = transport_recv_stream_init(credssp->transport, 2048);
	status = transport_read(credssp->transport, s);

	if (status < 0)
		return -1;

	dec_rval = ber_decode(0, &asn_DEF_TSRequest, (void **)&ts_request, s->data, status);

	if(dec_rval.code == RC_OK)
	{
		if (ts_request->negoTokens != NULL)
		{
			if (ts_request->negoTokens->list.count > 0)
			{
				freerdp_blob_alloc(negoToken, ts_request->negoTokens->list.array[0]->negoToken.size);
				memcpy(negoToken->data, ts_request->negoTokens->list.array[0]->negoToken.buf, negoToken->length);
			}
		}

		if (ts_request->pubKeyAuth != NULL)
		{
			freerdp_blob_alloc(pubKeyAuth, ts_request->pubKeyAuth->size);
			memcpy(pubKeyAuth->data, ts_request->pubKeyAuth->buf, pubKeyAuth->length);
		}

		asn_DEF_TSRequest.free_struct(&asn_DEF_TSRequest, ts_request, 0);
	}
	else
	{
		printf("Failed to decode TSRequest\n");
		asn_DEF_TSRequest.free_struct(&asn_DEF_TSRequest, ts_request, 0);
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
