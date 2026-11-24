/**
 * WinPR: Windows Portable Runtime
 * Kerberos SSPI DecryptMessage interoperability with RFC 4121 Wrap tokens
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

#include <stdio.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/sspi.h>

/* Include the source file directly because the SSPI entry points are static.
 * Rename the exported tables so they do not collide with the copies in libwinpr. */
#define KERBEROS_SecPkgInfoA TestKerberosWrap_SecPkgInfoA
#define KERBEROS_SecPkgInfoW TestKerberosWrap_SecPkgInfoW
#define KERBEROS_SecurityFunctionTableA TestKerberosWrap_SecurityFunctionTableA
#define KERBEROS_SecurityFunctionTableW TestKerberosWrap_SecurityFunctionTableW
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "../Kerberos/kerberos.c"

/*
 * Wrap tokens are built with synthetic keys, so no KDC is required.
 * RFC 4121 4.2.5 allows any RRC; MIT gss_wrap() emits RRC=0 while Windows
 * and WinPR emit RRC=16+trailer. CredSSP splits the blob at cbSecurityTrailer,
 * which does not have to match a Kerberos boundary.
 */

#define TEST_PLAINTEXT_LENGTH 32
#define TEST_SEQUENCE 0x1540bb1eull

static size_t checks = 0;
static size_t failures = 0;

static void check(BOOL condition, const char* name)
{
	checks++;
	if (!condition)
	{
		(void)fprintf(stderr, "FAIL: %s\n", name);
		failures++;
	}
}

static BOOL make_token(krb5_context ctx, const krb5_keyblock* key, uint16_t ec, uint16_t rrc,
                       BOOL bad_inner_header, BYTE** wire, size_t* wire_length)
{
	const size_t clear_length = TEST_PLAINTEXT_LENGTH + ec + 16ull;
	size_t encrypted_length = 0;
	if (krb5_c_encrypt_length(ctx, key->enctype, clear_length, &encrypted_length) != 0)
		return FALSE;

	BYTE* clear = calloc(1, clear_length);
	BYTE* encrypted = calloc(1, encrypted_length);
	*wire = calloc(1, 16 + encrypted_length);
	if (!clear || !encrypted || !*wire)
	{
		free(clear);
		free(encrypted);
		free(*wire);
		*wire = nullptr;
		return FALSE;
	}

	BYTE* header = *wire;
	winpr_Data_Write_UINT16_BE(header, TOK_ID_WRAP);
	header[2] = FLAG_WRAP_CONFIDENTIAL | FLAG_ACCEPTOR_SUBKEY;
	header[3] = 0xFF;
	winpr_Data_Write_UINT16_BE(&header[4], ec);
	winpr_Data_Write_UINT64_BE(&header[8], TEST_SEQUENCE);

	for (size_t x = 0; x < TEST_PLAINTEXT_LENGTH; x++)
		clear[x] = (BYTE)(x ^ 0xA5);
	memset(&clear[TEST_PLAINTEXT_LENGTH], 0xEE, ec);
	/* The encrypted copy of the header carries RRC=0 (RFC 4121 4.2.4) */
	memcpy(&clear[TEST_PLAINTEXT_LENGTH + ec], header, 16);
	if (bad_inner_header)
		clear[clear_length - 1] ^= 1;

	krb5_data input = WINPR_C_ARRAY_INIT;
	input.data = (char*)clear;
	input.length = (unsigned int)clear_length;
	krb5_enc_data output = WINPR_C_ARRAY_INIT;
	output.ciphertext.data = (char*)encrypted;
	output.ciphertext.length = (unsigned int)encrypted_length;
	const krb5_error_code rv =
	    krb5_c_encrypt(ctx, key, KG_USAGE_INITIATOR_SEAL, nullptr, &input, &output);
	if (rv == 0)
	{
		encrypted_length = output.ciphertext.length;
		/* Rotate right by RRC octets, modulo the body length */
		for (size_t x = 0; x < encrypted_length; x++)
			header[16 + ((x + rrc) % encrypted_length)] = encrypted[x];
		winpr_Data_Write_UINT16_BE(&header[6], rrc);
		*wire_length = 16 + encrypted_length;
	}

	winpr_znfree(clear, clear_length);
	free(encrypted);
	if (rv != 0)
	{
		free(*wire);
		*wire = nullptr;
	}
	return rv == 0;
}

static SECURITY_STATUS decrypt(KRB_CONTEXT* context, const BYTE* wire, size_t length, size_t split,
                               BOOL expect_success, const char* name)
{
	const size_t data_size = length - split;
	BYTE* token = calloc((split > 0) ? split : 1, sizeof(BYTE));
	BYTE* data = calloc((data_size > 0) ? data_size : 1, sizeof(BYTE));
	if (!token || !data)
	{
		check(FALSE, "allocation");
		free(token);
		free(data);
		return SEC_E_INSUFFICIENT_MEMORY;
	}
	memcpy(token, wire, split);
	memcpy(data, &wire[split], data_size);

	SecBuffer buffers[2] = {
		{ (ULONG)split, SECBUFFER_TOKEN, token },
		{ (ULONG)data_size, SECBUFFER_DATA, data },
	};
	SecBufferDesc desc = { SECBUFFER_VERSION, 2, buffers };
	CtxtHandle handle = WINPR_C_ARRAY_INIT;
	sspi_SecureHandleSetLowerPointer(&handle, context);
	sspi_SecureHandleSetUpperPointer(&handle, (void*)KERBEROS_SSP_NAME);

	ULONG qop = 123;
	const SECURITY_STATUS status = kerberos_DecryptMessage(&handle, &desc, 0, &qop);
	if (expect_success)
	{
		check(status == SEC_E_OK, name);
		if (status == SEC_E_OK)
		{
			check(buffers[1].cbBuffer == TEST_PLAINTEXT_LENGTH, "exact plaintext size");
			check(qop == 0, "confidential QOP");
			/* Only read what was actually allocated */
			BOOL equal = (buffers[1].cbBuffer == TEST_PLAINTEXT_LENGTH) &&
			             (data_size >= TEST_PLAINTEXT_LENGTH);
			for (size_t x = 0; equal && (x < TEST_PLAINTEXT_LENGTH); x++)
				equal = (data[x] == (BYTE)(x ^ 0xA5));
			check(equal, "exact plaintext bytes");
		}
	}
	else
	{
		check(status != SEC_E_OK, name);
		check((qop == 123) && (buffers[1].cbBuffer == data_size) &&
		          (memcmp(data, &wire[split], data_size) == 0),
		      "rejection leaves the output untouched");
	}

	free(token);
	free(data);
	return status;
}

/* Tokens produced by kerberos_EncryptMessage (Windows layout) must keep working */
static void check_winpr_sender(KRB_CONTEXT* receiver, size_t trailer_length)
{
	BYTE wire[256] = WINPR_C_ARRAY_INIT;
	SecBuffer buffers[2] = {
		{ (ULONG)trailer_length, SECBUFFER_TOKEN, wire },
		{ TEST_PLAINTEXT_LENGTH, SECBUFFER_DATA, &wire[trailer_length] },
	};
	for (size_t x = 0; x < TEST_PLAINTEXT_LENGTH; x++)
		wire[trailer_length + x] = (BYTE)(x ^ 0xA5);

	SecBufferDesc desc = { SECBUFFER_VERSION, 2, buffers };
	KRB_CONTEXT sender = *receiver;
	sender.acceptor = FALSE;
	sender.local_seq = receiver->remote_seq;
	CtxtHandle handle = WINPR_C_ARRAY_INIT;
	sspi_SecureHandleSetLowerPointer(&handle, &sender);
	sspi_SecureHandleSetUpperPointer(&handle, (void*)KERBEROS_SSP_NAME);

	const SECURITY_STATUS status = kerberos_EncryptMessage(&handle, 0, &desc, 0);
	check(status == SEC_E_OK, "WinPR sender encrypt");
	if (status == SEC_E_OK)
		decrypt(receiver, wire, trailer_length + TEST_PLAINTEXT_LENGTH, trailer_length, TRUE,
		        "WinPR sender (RRC=16+trailer)");
}

static void check_malformed(KRB_CONTEXT* context, krb5_context ctx, const krb5_keyblock* key)
{
	BYTE* wire = nullptr;
	size_t length = 0;
	if (!make_token(ctx, key, 0, 0, FALSE, &wire, &length))
	{
		check(FALSE, "build token");
		return;
	}
	const size_t split = length - TEST_PLAINTEXT_LENGTH;

	check_winpr_sender(context, split);

	SecBuffer oversized[2] = {
		{ (ULONG)split, SECBUFFER_TOKEN, wire },
		{ UINT32_MAX, SECBUFFER_DATA, wire },
	};
	SecBufferDesc oversized_desc = { SECBUFFER_VERSION, 2, oversized };
	CtxtHandle handle = WINPR_C_ARRAY_INIT;
	sspi_SecureHandleSetLowerPointer(&handle, context);
	sspi_SecureHandleSetUpperPointer(&handle, (void*)KERBEROS_SSP_NAME);
	ULONG qop = 123;
	check(kerberos_DecryptMessage(&handle, &oversized_desc, 0, &qop) == SEC_E_INVALID_TOKEN,
	      "reject a length beyond krb5_data before reading data");

	decrypt(context, wire, 16, 16, FALSE, "empty body");
	decrypt(context, wire, length, 15, FALSE, "short token header");
	decrypt(context, wire, length - 1, split, FALSE, "truncated token");

	winpr_Data_Write_UINT16_BE(&wire[4], UINT16_MAX);
	decrypt(context, wire, length, split, FALSE, "EC exceeds the plaintext");
	winpr_Data_Write_UINT16_BE(&wire[4], 1);
	check(decrypt(context, wire, length, split, FALSE, "EC differs from the encrypted header") ==
	          SEC_E_MESSAGE_ALTERED,
	      "EC is authenticated");
	winpr_Data_Write_UINT16_BE(&wire[4], 0);

	wire[2] |= FLAG_SENDER_IS_ACCEPTOR;
	decrypt(context, wire, length, split, FALSE, "wrong direction");
	wire[2] &= (BYTE)~FLAG_SENDER_IS_ACCEPTOR;

	wire[2] &= (BYTE)~FLAG_WRAP_CONFIDENTIAL;
	decrypt(context, wire, length, split, FALSE, "not sealed");
	wire[2] |= FLAG_WRAP_CONFIDENTIAL;

	wire[15] ^= 1;
	check(decrypt(context, wire, length, split, FALSE, "wrong sequence") == SEC_E_OUT_OF_SEQUENCE,
	      "sequence number is checked");
	free(wire);

	if (!make_token(ctx, key, 0, 0, TRUE, &wire, &length))
	{
		check(FALSE, "build token");
		return;
	}
	check(decrypt(context, wire, length, length - TEST_PLAINTEXT_LENGTH, FALSE,
	              "bad encrypted header") == SEC_E_MESSAGE_ALTERED,
	      "encrypted header is checked");
	free(wire);
}

static BOOL check_enctype(krb5_context ctx, krb5_enctype enctype)
{
	const uint16_t rotations[] = { 0, 1, 15, 28, 40, UINT16_MAX };
	const uint16_t fillers[] = { 0, 1, 16, 255, UINT16_MAX };
	krb5_keyblock key = WINPR_C_ARRAY_INIT;
	krb5_key opaque = nullptr;

	if ((krb5_c_make_random_key(ctx, enctype, &key) != 0) ||
	    (krb5_k_create_key(ctx, &key, &opaque) != 0))
	{
		(void)fprintf(stderr, "cannot create a key for enctype %d\n", enctype);
		krb5_free_keyblock_contents(ctx, &key);
		return FALSE;
	}

	KRB_CREDENTIALS creds = WINPR_C_ARRAY_INIT;
	creds.ctx = ctx;
	KRB_CONTEXT context = WINPR_C_ARRAY_INIT;
	context.credentials = &creds;
	context.acceptor = TRUE;
	context.flags = SSPI_GSS_C_CONF_FLAG | ISC_REQ_SEQUENCE_DETECT;
	context.remote_seq = TEST_SEQUENCE;
	context.keyset.acceptor_key = opaque;

	for (size_t f = 0; f < ARRAYSIZE(fillers); f++)
	{
		for (size_t r = 0; r < ARRAYSIZE(rotations); r++)
		{
			BYTE* wire = nullptr;
			size_t length = 0;
			char name[128] = WINPR_C_ARRAY_INIT;

			if (!make_token(ctx, &key, fillers[f], rotations[r], FALSE, &wire, &length))
			{
				check(FALSE, "build token");
				continue;
			}
			const size_t split = length - TEST_PLAINTEXT_LENGTH - fillers[f];
			(void)_snprintf(name, sizeof(name), "enctype=%d EC=%" PRIu16 " RRC=%" PRIu16, enctype,
			                fillers[f], rotations[r]);
			decrypt(&context, wire, length, split, TRUE, name);
			if (fillers[f] == 0)
			{
				decrypt(&context, wire, length, split - 1, TRUE, "split inside the trailer");
				decrypt(&context, wire, length, split + 1, FALSE, "output buffer too small");
			}
			wire[length - 1] ^= 1;
			decrypt(&context, wire, length, split, FALSE, "tampered ciphertext");
			free(wire);
		}
	}

	check_malformed(&context, ctx, &key);

	krb5_k_free_key(ctx, opaque);
	krb5_free_keyblock_contents(ctx, &key);
	return TRUE;
}

int TestKerberosWrap(int argc, char* argv[])
{
	const krb5_enctype enctypes[] = {
		ENCTYPE_AES128_CTS_HMAC_SHA1_96,
		ENCTYPE_AES256_CTS_HMAC_SHA1_96,
		ENCTYPE_AES128_CTS_HMAC_SHA256_128,
		ENCTYPE_AES256_CTS_HMAC_SHA384_192,
	};
	krb5_context ctx = nullptr;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (krb5_init_context(&ctx) != 0)
		return -1;

	for (size_t e = 0; e < ARRAYSIZE(enctypes); e++)
	{
		if (!check_enctype(ctx, enctypes[e]))
			failures++;
	}
	krb5_free_context(ctx);

	printf("TestKerberosWrap: %" PRIuz " checks, %" PRIuz " failures\n", checks, failures);
	return (failures > 0) ? -1 : 0;
}
