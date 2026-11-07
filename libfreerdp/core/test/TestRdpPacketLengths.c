/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP packet boundary regression tests
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

#include <freerdp/config.h>

#include <freerdp/freerdp.h>
#include <freerdp/settings.h>
#include <winpr/crypto.h>
#include <winpr/stream.h>
#include <winpr/ssl.h>

#include "../connection.h"
#include "../mcs.h"
#include "../rdp.h"
#include "../security.h"

/* These are framing fixtures, not capability negotiation or cipher test vectors.
 * The zero-capability Demand Active is sufficient for the real receive parser. */
static const BYTE demandActive[] = {
	0x16, 0x00, 0x11, 0x00, 0xEA, 0x03, /* Share Control: 22 bytes, Demand Active */
	0x00, 0x00, 0x01, 0x00,             /* shareId */
	0x00, 0x00, 0x04, 0x00,             /* descriptor length, capability length */
	0x00, 0x00, 0x00, 0x00,             /* capability count and padding */
	0x00, 0x00, 0x00, 0x00              /* sessionId */
};

static const BYTE flowResponse[] = { 0x00, 0x80, PDU_TYPE_FLOW_RESPONSE, 0, 0, 0, 0, 0 };
static const BYTE shortDeactivate[] = { 0x04, 0x00, 0x16, 0x00 };

typedef enum
{
	TEST_TLS,
	TEST_SECURITY_HEADER,
	TEST_RC4,
	TEST_FIPS
} TestSecurity;

static freerdp* new_client(void)
{
	freerdp* instance = freerdp_new();
	if (!instance)
		return nullptr;
	if (!freerdp_context_new(instance))
	{
		freerdp_free(instance);
		return nullptr;
	}
	instance->context->rdp->settings->UseRdpSecurityLayer = FALSE;
	return instance;
}

static void free_client(freerdp* instance)
{
	if (instance)
	{
		freerdp_context_free(instance);
		freerdp_free(instance);
	}
}

/* A complete TPKT may be embedded in a larger buffer. Its length is measured
 * from the current position, not from the buffer start or its allocation size. */
static wStream* packet(const BYTE* payload, size_t payloadLength, UINT16 declaredLength,
                       BOOL serverMode, BOOL longLength, size_t prefix, size_t suffix)
{
	const size_t headerLength = longLength ? 15 : 14;
	wStream* s = Stream_New(nullptr, prefix + headerLength + payloadLength + suffix);
	if (!s)
		return nullptr;

	Stream_Zero(s, prefix);
	Stream_Write_UINT8(s, 3);
	Stream_Write_UINT8(s, 0);
	Stream_Write_UINT16_BE(s, (UINT16)(headerLength + payloadLength));
	Stream_Write_UINT8(s, 2);
	Stream_Write_UINT8(s, 0xF0);
	Stream_Write_UINT8(s, 0x80);
	Stream_Write_UINT8(s, serverMode ? 0x64 : 0x68); /* MCS SendDataRequest / Indication */
	Stream_Write_UINT16_BE(s, 1);
	Stream_Write_UINT16_BE(s, MCS_GLOBAL_CHANNEL_ID);
	Stream_Write_UINT8(s, 0x70);
	if (longLength)
		Stream_Write_UINT16_BE(s, declaredLength | 0x8000);
	else
		Stream_Write_UINT8(s, (BYTE)declaredLength);
	Stream_Write(s, payload, payloadLength);
	Stream_Zero(s, suffix);
	Stream_SealLength(s);
	if (!Stream_SetPosition(s, prefix))
	{
		Stream_Free(s, TRUE);
		return nullptr;
	}
	return s;
}

static BOOL test_mcs_headers(void)
{
	static const struct
	{
		UINT16 actual;
		UINT16 declared;
		BOOL longLength;
		BOOL valid;
	} cases[] = { { 0, 0, FALSE, TRUE },     { 1, 1, FALSE, TRUE },     { 26, 26, FALSE, TRUE },
		          { 127, 127, FALSE, TRUE }, { 127, 127, TRUE, TRUE },  { 128, 128, TRUE, TRUE },
		          { 386, 386, TRUE, TRUE },  { 26, 25, FALSE, FALSE },  { 26, 27, FALSE, FALSE },
		          { 386, 130, TRUE, FALSE }, { 386, 387, TRUE, FALSE }, { 0, 1, FALSE, FALSE },
		          { 1, 0, FALSE, FALSE } };
	BYTE body[386] = WINPR_C_ARRAY_INIT;
	BOOL ok = TRUE;
	freerdp* instance = new_client();
	if (!instance)
		return FALSE;
	rdpRdp* rdp = instance->context->rdp;
	for (size_t server = 0; server < 2; server++)
	{
		rdp->settings->ServerMode = (BOOL)server;
		for (size_t embedding = 0; embedding < 2; embedding++)
		{
			const size_t prefix = embedding ? 7 : 0;
			const size_t suffix = embedding ? 29 : 0;
			for (size_t x = 0; x < ARRAYSIZE(cases); x++)
			{
				UINT16 length = 0;
				UINT16 channel = 0;
				wStream* s = packet(body, cases[x].actual, cases[x].declared, (BOOL)server,
				                    cases[x].longLength, prefix, suffix);
				if (!s)
				{
					ok = FALSE;
					continue;
				}
				const BOOL result = rdp_read_header(rdp, s, &length, &channel);
				if ((result != cases[x].valid) ||
				    (result && ((length != cases[x].actual) || (channel != MCS_GLOBAL_CHANNEL_ID) ||
				                (Stream_GetRemainingLength(s) != cases[x].actual + suffix))))
				{
					fprintf(stderr, "MCS framing case %zu, server=%zu, embedding=%zu failed\n", x,
					        server, embedding);
					ok = FALSE;
				}
				Stream_Free(s, TRUE);
			}
		}
	}
	free_client(instance);
	return ok;
}

/* Exercise real RDP decryption, including FIPS block padding, before the Share
 * Control check. Independent encrypt/decrypt states use matching fixture keys. */
static BOOL setup_security(rdpRdp* rdp, TestSecurity security)
{
	static const BYTE iv[] = { 0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF };
	rdp->settings->UseRdpSecurityLayer = security != TEST_TLS;
	if (security < TEST_RC4)
		return TRUE;

	if (security == TEST_RC4)
	{
		rdp->settings->EncryptionMethods = ENCRYPTION_METHOD_128BIT;
		rdp->rc4_key_len = 16;
		memset(rdp->sign_key, 0x3A, sizeof(rdp->sign_key));
		for (size_t x = 0; x < sizeof(rdp->encrypt_key); x++)
			rdp->encrypt_key[x] = rdp->decrypt_key[x] = (BYTE)(x + 1);
		return rdp_reset_rc4_encrypt_keys(rdp) && rdp_reset_rc4_decrypt_keys(rdp);
	}

	rdp->settings->EncryptionMethods = ENCRYPTION_METHOD_FIPS;
	memset(rdp->fips_sign_key, 0x3A, sizeof(rdp->fips_sign_key));
	for (size_t x = 0; x < sizeof(rdp->fips_encrypt_key); x++)
		rdp->fips_encrypt_key[x] = rdp->fips_decrypt_key[x] = (BYTE)(x + 1);
	rdp->fips_encrypt =
	    winpr_Cipher_NewEx(WINPR_CIPHER_DES_EDE3_CBC, WINPR_ENCRYPT, rdp->fips_encrypt_key,
	                       sizeof(rdp->fips_encrypt_key), iv, sizeof(iv));
	rdp->fips_decrypt =
	    winpr_Cipher_NewEx(WINPR_CIPHER_DES_EDE3_CBC, WINPR_DECRYPT, rdp->fips_decrypt_key,
	                       sizeof(rdp->fips_decrypt_key), iv, sizeof(iv));
	return rdp->fips_encrypt && rdp->fips_decrypt;
}

static wStream* protect(rdpRdp* rdp, TestSecurity security, const BYTE* data, size_t length)
{
	BYTE signature[8] = WINPR_C_ARRAY_INIT;
	const size_t pad = (security == TEST_FIPS) ? (8 - length % 8) % 8 : 0;
	wStream* s = Stream_New(nullptr, length + 24);
	if (!s || !setup_security(rdp, security))
		goto fail;
	if (security != TEST_TLS)
	{
		Stream_Write_UINT16(s, security >= TEST_RC4 ? SEC_ENCRYPT : 0);
		Stream_Write_UINT16(s, 0);
	}
	if (security == TEST_FIPS)
	{
		Stream_Write_UINT16(s, 0x10);
		Stream_Write_UINT8(s, 1);
		Stream_Write_UINT8(s, (BYTE)pad);
		if (!security_hmac_signature(data, length, signature, sizeof(signature), rdp))
			goto fail;
	}
	else if (security == TEST_RC4)
	{
		if (!security_mac_signature(rdp, data, (UINT32)length, signature, sizeof(signature)))
			goto fail;
	}
	if (security >= TEST_RC4)
		Stream_Write(s, signature, sizeof(signature));

	BYTE* encrypted = Stream_Pointer(s);
	Stream_Write(s, data, length);
	Stream_Zero(s, pad);
	if ((security == TEST_RC4) && !security_encrypt(encrypted, length, rdp))
		goto fail;
	if ((security == TEST_FIPS) && !security_fips_encrypt(encrypted, length + pad, rdp))
		goto fail;
	Stream_SealLength(s);
	return s;
fail:
	Stream_Free(s, TRUE);
	return nullptr;
}

static BOOL test_demand_active(TestSecurity security, int shareDelta, int mcsDelta)
{
	BOOL ok = FALSE;
	wStream* protected = nullptr;
	wStream* s = nullptr;
	BYTE data[sizeof(demandActive)] = WINPR_C_ARRAY_INIT;
	freerdp* instance = new_client();
	if (!instance)
		return FALSE;
	rdpRdp* rdp = instance->context->rdp;
	memcpy(data, demandActive, sizeof(data));
	data[0] = (BYTE)(sizeof(data) + shareDelta);
	protected = protect(rdp, security, data, sizeof(data));
	if (!protected)
		goto out;
	const size_t length = Stream_Length(protected);
	s = packet(Stream_Buffer(protected), length, (UINT16)((int)length + mcsDelta), FALSE, FALSE, 0,
	           0);
	if (!s)
		goto out;
	const state_run_t result = rdp_client_connect_demand_active(rdp, s);
	if ((shareDelta == 0) && (mcsDelta == 0))
	{
		ok = (result == STATE_RUN_ACTIVE) && (Stream_GetRemainingLength(s) == 0) &&
		     (rdp->settings->ShareId == 0x10000) && (rdp->settings->PduSource == 1002);
		if (security >= TEST_RC4)
			ok = ok && (rdp->decrypt_use_count == 1);
		/* FIPS has two pad bytes on the wire, which must no longer be present. */
		if (security == TEST_FIPS)
			ok = ok && (Stream_Length(s) == length + 14 - 2);
	}
	else
		ok = state_run_failed(result);
out:
	if (!ok)
		fprintf(stderr, "Demand Active security=%d, share delta=%d, MCS delta=%d failed\n",
		        security, shareDelta, mcsDelta);
	Stream_Free(s, TRUE);
	Stream_Free(protected, TRUE);
	free_client(instance);
	return ok;
}

static BOOL test_share_compatibility(TestSecurity security, BOOL deactivate, BOOL demandState)
{
	BOOL ok = FALSE;
	BYTE payload[sizeof(flowResponse) * 2 + sizeof(shortDeactivate)] = WINPR_C_ARRAY_INIT;
	size_t length = sizeof(flowResponse);
	wStream* protected = nullptr;
	wStream* s = nullptr;
	freerdp* instance = new_client();
	if (!instance)
		return FALSE;
	rdpRdp* rdp = instance->context->rdp;
	memcpy(payload, flowResponse, sizeof(flowResponse));
	if (!demandState)
	{
		/* Multiple Share PDUs in one MCS payload remain valid. */
		memcpy(payload + length, flowResponse, sizeof(flowResponse));
		length += sizeof(flowResponse);
		if (deactivate)
		{
			memcpy(payload + length, shortDeactivate, sizeof(shortDeactivate));
			length += sizeof(shortDeactivate);
		}
	}
	protected = protect(rdp, security, payload, length);
	if (!protected)
		goto out;
	s = packet(Stream_Buffer(protected), Stream_Length(protected), (UINT16)Stream_Length(protected),
	           FALSE, FALSE, 0, 0);
	if (!s)
		goto out;
	state_run_t result = STATE_RUN_FAILED;
	if (demandState)
		result = rdp_client_connect_demand_active(rdp, s);
	else
	{
		rdp->state = CONNECTION_STATE_ACTIVE;
		result = rdp_recv_callback(rdp->transport, s, rdp);
	}
	ok = state_run_success(result) && (Stream_GetRemainingLength(s) == 0);
	if (!demandState)
	{
		ok = ok && (rdp->inPackets == (deactivate ? 3 : 2));
		if (deactivate)
			ok = ok && (rdp->state == CONNECTION_STATE_CAPABILITIES_EXCHANGE_DEMAND_ACTIVE);
	}
out:
	if (!ok)
		fprintf(stderr, "Share compatibility security=%d, deactivate=%d, demand state=%d failed\n",
		        security, deactivate, demandState);
	Stream_Free(s, TRUE);
	Stream_Free(protected, TRUE);
	free_client(instance);
	return ok;
}

int TestRdpPacketLengths(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char* argv[])
{
	if (!winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT))
		return -1;
	BOOL ok = test_mcs_headers();
	for (TestSecurity security = TEST_TLS; security <= TEST_FIPS; security++)
	{
		for (int delta = -1; delta <= 1; delta++)
		{
			if (!test_demand_active(security, delta, 0))
				ok = FALSE;
			if ((delta != 0) && !test_demand_active(security, 0, delta))
				ok = FALSE;
		}
		if (!test_share_compatibility(security, FALSE, FALSE) ||
		    !test_share_compatibility(security, TRUE, FALSE) ||
		    !test_share_compatibility(security, FALSE, TRUE))
			ok = FALSE;
	}
	return ok ? 0 : -1;
}
