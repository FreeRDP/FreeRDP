/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Authentication redirection virtual channel: redirected-call servicing
 *
 * The [MS-RDPEAR] redirected calls of the Kerberos and NTLM packages are serviced here so both
 * the in-process channel client (rdpear_main.c) and the lsa4posix daemon can dispatch them. The
 * handlers run krb5 crypto on keys carried in each request; they do not touch the ticket cache.
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

#include <krb5.h>
#include <errno.h>

#include <winpr/assert.h>
#include <winpr/wtypes.h>
#include <winpr/crt.h>
#include <winpr/wlog.h>
#include <winpr/print.h>
#include <winpr/asn1.h>
#include <winpr/sspi.h>
#include <winpr/collections.h>
#include <winpr/ndr.h>

#include <rdpear-common/rdpear_common.h>
#include <rdpear-common/rdpear_asn1.h>
#include <rdpear-common/rdpear_kerb.h>

#include <freerdp/settings.h>
#include <freerdp/channels/log.h>

#define TAG CHANNELS_TAG("rdpear.kerb")

#ifndef MAX_KEYTAB_NAME_LEN
#define MAX_KEYTAB_NAME_LEN 1100 /* Defined in MIT krb5.h */
#endif

krb5_error_code encode_krb5_authenticator(const krb5_authenticator* rep, krb5_data** code_out);
krb5_error_code encode_krb5_ap_rep(const krb5_ap_rep* rep, krb5_data** code_out);

/* What the handlers need from their caller: the handlers are the ones of the in-tree rdpear
 * client, kept as they are there so the two stay easy to compare; only this context differs. */
typedef struct
{
	krb5_context krbContext;
} RDPEAR_SERVICE;

static const BYTE payloadHeader[16] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	                                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static krb5_error_code RPC_ENCRYPTION_KEY_to_keyblock(krb5_context ctx,
                                                      const KERB_RPC_ENCRYPTION_KEY* key,
                                                      krb5_keyblock** pkeyblock)
{
	WINPR_ASSERT(ctx);
	WINPR_ASSERT(key);
	WINPR_ASSERT(pkeyblock);

	if (!key->reserved3.length)
		return KRB5KDC_ERR_NULL_KEY;

	krb5_error_code rv =
	    krb5_init_keyblock(ctx, (krb5_enctype)key->reserved2, key->reserved3.length, pkeyblock);
	if (rv)
		return rv;

	krb5_keyblock* keyblock = *pkeyblock;
	memcpy(keyblock->contents, key->reserved3.value, key->reserved3.length);
	return rv;
}

/** @brief sets the buffer of a KERB_ASN1_DATA to encode, the data is copied in a NDR referent */
static BOOL rdpear_set_asn1_buffer(KERB_ASN1_DATA* asn1, const void* data, size_t len)
{
	WINPR_ASSERT(asn1);

	if (len > UINT32_MAX)
		return FALSE;

	const WinPrNdrArrayHints hints = { (UINT32)len };
	BYTE* buffer = winpr_ndr_pointer_new(winpr_ndr_uint8Array_descr(), &hints, len);
	if (!buffer)
		return FALSE;

	if (len)
		memcpy(buffer, data, len);
	asn1->Asn1Buffer = buffer;
	asn1->Asn1BufferHints = hints;
	return TRUE;
}

static krb5_error_code kerb_do_checksum(krb5_context ctx, const KERB_RPC_ENCRYPTION_KEY* key,
                                        krb5_keyusage kusage, krb5_cksumtype cksumtype,
                                        const KERB_ASN1_DATA* plain, krb5_checksum* out)
{
	WINPR_ASSERT(ctx);
	WINPR_ASSERT(key);
	WINPR_ASSERT(plain);
	WINPR_ASSERT(out);

	krb5_keyblock* keyblock = nullptr;
	krb5_data data = WINPR_C_ARRAY_INIT;

	krb5_error_code rv = RPC_ENCRYPTION_KEY_to_keyblock(ctx, key, &keyblock);
	if (rv)
		return rv;

	data.data = (char*)plain->Asn1Buffer;
	data.length = plain->Asn1BufferHints.count;

	rv = krb5_c_make_checksum(ctx, cksumtype, keyblock, kusage, &data, out);

	krb5_free_keyblock(ctx, keyblock);
	return rv;
}

static krb5_error_code kerb_do_encrypt(krb5_context ctx, const KERB_RPC_ENCRYPTION_KEY* key,
                                       krb5_keyusage kusage, const KERB_ASN1_DATA* plain,
                                       krb5_data* out)
{
	WINPR_ASSERT(ctx);
	WINPR_ASSERT(key);
	WINPR_ASSERT(plain);
	WINPR_ASSERT(out);

	out->data = nullptr;
	out->length = 0;

	krb5_keyblock* keyblock = nullptr;
	krb5_data data = WINPR_C_ARRAY_INIT;
	krb5_enc_data enc = WINPR_C_ARRAY_INIT;
	size_t elen = 0;

	krb5_error_code rv = RPC_ENCRYPTION_KEY_to_keyblock(ctx, key, &keyblock);
	if (rv)
		return rv;

	data.data = (char*)plain->Asn1Buffer;
	data.length = plain->Asn1BufferHints.count;

	rv = krb5_c_encrypt_length(ctx, keyblock->enctype, data.length, &elen);
	if (rv)
		goto out;
	if (!elen || (elen > UINT32_MAX))
	{
		rv = KRB5_PARSE_MALFORMED;
		goto out;
	}
	enc.ciphertext.length = (unsigned int)elen;
	enc.ciphertext.data = malloc(elen);
	if (!enc.ciphertext.data)
	{
		rv = ENOMEM;
		goto out;
	}

	rv = krb5_c_encrypt(ctx, keyblock, kusage, nullptr, &data, &enc);
	if (rv)
	{
		free(enc.ciphertext.data);
		goto out;
	}

	out->data = enc.ciphertext.data;
	out->length = enc.ciphertext.length;
out:
	krb5_free_keyblock(ctx, keyblock);
	return rv;
}

static krb5_error_code kerb_do_decrypt(krb5_context ctx, const KERB_RPC_ENCRYPTION_KEY* key,
                                       krb5_keyusage kusage, const krb5_data* cipher,
                                       KERB_ASN1_DATA* plain)
{
	WINPR_ASSERT(ctx);
	WINPR_ASSERT(key);
	WINPR_ASSERT(cipher);
	WINPR_ASSERT(cipher->length);
	WINPR_ASSERT(plain);

	plain->Asn1Buffer = nullptr;
	plain->Asn1BufferHints.count = 0;

	krb5_keyblock* keyblock = nullptr;
	krb5_data data = WINPR_C_ARRAY_INIT;
	krb5_enc_data enc = WINPR_C_ARRAY_INIT;

	krb5_error_code rv = RPC_ENCRYPTION_KEY_to_keyblock(ctx, key, &keyblock);
	if (rv)
		return rv;

	enc.kvno = KRB5_PVNO;
	enc.enctype = (krb5_enctype)key->reserved2;
	enc.ciphertext.length = cipher->length;
	enc.ciphertext.data = cipher->data;

	data.length = cipher->length;
	data.data = (char*)malloc(cipher->length);
	if (!data.data)
	{
		rv = ENOMEM;
		goto out;
	}

	rv = krb5_c_decrypt(ctx, keyblock, kusage, nullptr, &enc, &data);
	if (!rv && !rdpear_set_asn1_buffer(plain, data.data, data.length))
		rv = ENOMEM;
	free(data.data);
out:
	krb5_free_keyblock(ctx, keyblock);
	return rv;
}

static UINT32 rdpear_kerb_version(UINT32 maxVersion, UINT32* pversion)
{
	WLog_DBG(TAG, "-> KerbNegotiateVersion(v=0x%x)", maxVersion);
	*pversion = 0;
	return 0;
}

static UINT32 rdpear_kerb_ComputeTgsChecksum(const RDPEAR_SERVICE* rdpear,
                                             const ComputeTgsChecksumReq* req, KERB_ASN1_DATA* resp)
{
	krb5_checksum checksum = WINPR_C_ARRAY_INIT;
	wStream* asn1Payload = nullptr;

	UINT32 status = ERROR_INVALID_DATA;
	WLog_DBG(TAG, "-> ComputeTgsChecksum");

	krb5_error_code rv =
	    kerb_do_checksum(rdpear->krbContext, req->Key, KRB5_KEYUSAGE_TGS_REQ_AUTH_CKSUM,
	                     (krb5_cksumtype)req->ChecksumType, req->requestBody, &checksum);
	if (rv)
		goto out;

	asn1Payload = rdpear_enc_Checksum(req->ChecksumType, &checksum);
	if (!asn1Payload)
		goto out;

	resp->Pdu = 8;
	if (!rdpear_set_asn1_buffer(resp, Stream_Buffer(asn1Payload), Stream_GetPosition(asn1Payload)))
		goto out;
	status = 0;

out:
	krb5_free_checksum_contents(rdpear->krbContext, &checksum);
	Stream_Free(asn1Payload, TRUE);
	return status;
}

static UINT32 rdpear_kerb_BuildEncryptedAuthData(const RDPEAR_SERVICE* rdpear,
                                                 const BuildEncryptedAuthDataReq* req,
                                                 KERB_ASN1_DATA* asn1)
{
	krb5_data encrypted = WINPR_C_ARRAY_INIT;
	wStream* asn1Payload = nullptr;
	krb5_error_code rv = 0;

	UINT32 status = ERROR_INVALID_DATA;
	WLog_DBG(TAG, "-> BuildEncryptedAuthData");

	rv = kerb_do_encrypt(rdpear->krbContext, req->Key, (krb5_keyusage)req->KeyUsage,
	                     req->PlainAuthData, &encrypted);
	if (rv)
		goto out;

	/* do the encoding */
	asn1Payload = rdpear_enc_EncryptedData(req->Key->reserved2, &encrypted);
	if (!asn1Payload)
		goto out;

	//	WLog_DBG(TAG, "rdpear_kerb_BuildEncryptedAuthData resp=");
	//	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Buffer(asn1Payload), Stream_GetPosition(asn1Payload));
	asn1->Pdu = 6;
	if (!rdpear_set_asn1_buffer(asn1, Stream_Buffer(asn1Payload), Stream_GetPosition(asn1Payload)))
		goto out;
	status = 0;

out:
	krb5_free_data_contents(rdpear->krbContext, &encrypted);
	Stream_Free(asn1Payload, TRUE);
	return status;
}

static char* KERB_RPC_UNICODESTR_to_charptr(const RPC_UNICODE_STRING* src)
{
	WINPR_ASSERT(src);
	return ConvertWCharNToUtf8Alloc(src->Buffer, src->strLength, nullptr);
}

static BOOL extractAuthData(const KERB_ASN1_DATA* src, krb5_authdata* authData, BOOL* haveData)
{
	WinPrAsn1Decoder dec = WINPR_C_ARRAY_INIT;
	WinPrAsn1Decoder dec2 = WINPR_C_ARRAY_INIT;
	WinPrAsn1Decoder dec3 = WINPR_C_ARRAY_INIT;
	WinPrAsn1Decoder_InitMem(&dec, WINPR_ASN1_DER, src->Asn1Buffer, src->Asn1BufferHints.count);
	BOOL error = FALSE;
	WinPrAsn1_INTEGER adType = 0;
	WinPrAsn1_OctetString os = WINPR_C_ARRAY_INIT;

	*haveData = FALSE;
	if (!WinPrAsn1DecReadSequence(&dec, &dec2))
		return FALSE;

	wStream subStream = WinPrAsn1DecGetStream(&dec2);
	if (!Stream_GetRemainingLength(&subStream))
		return TRUE;

	if (!WinPrAsn1DecReadSequence(&dec2, &dec3))
		return FALSE;

	if (!WinPrAsn1DecReadContextualInteger(&dec3, 0, &error, &adType) ||
	    !WinPrAsn1DecReadContextualOctetString(&dec3, 1, &error, &os, FALSE))
		return FALSE;

	if (os.len > UINT32_MAX)
		return FALSE;

	authData->ad_type = adType;
	authData->length = (unsigned int)os.len;
	authData->contents = os.data;
	*haveData = TRUE;
	return TRUE;
}

static BOOL extractChecksum(const KERB_ASN1_DATA* src, krb5_checksum* dst)
{
	WinPrAsn1Decoder dec = WINPR_C_ARRAY_INIT;
	WinPrAsn1Decoder dec2 = WINPR_C_ARRAY_INIT;
	WinPrAsn1Decoder_InitMem(&dec, WINPR_ASN1_DER, src->Asn1Buffer, src->Asn1BufferHints.count);
	BOOL error = FALSE;
	WinPrAsn1_OctetString os;

	if (!WinPrAsn1DecReadSequence(&dec, &dec2))
		return FALSE;

	WinPrAsn1_INTEGER cksumtype = 0;
	if (!WinPrAsn1DecReadContextualInteger(&dec2, 0, &error, &cksumtype) ||
	    !WinPrAsn1DecReadContextualOctetString(&dec2, 1, &error, &os, FALSE))
		return FALSE;

	if (os.len > UINT32_MAX)
		return FALSE;
	dst->checksum_type = cksumtype;
	dst->length = (unsigned int)os.len;
	dst->contents = os.data;
	return TRUE;
}

#define FILETIME_TO_UNIX_OFFSET_S 11644473600LL

static LONGLONG krb5_time_to_FILETIME(const krb5_timestamp* ts, krb5_int32 usec)
{
	WINPR_ASSERT(ts);
	return (((*ts + FILETIME_TO_UNIX_OFFSET_S) * (1000LL * 1000LL) + usec) * 10LL);
}

static void krb5_free_principal_contents(krb5_context ctx, krb5_principal principal)
{
	WINPR_ASSERT(principal);
	krb5_free_data_contents(ctx, &principal->realm);
	krb5_free_data(ctx, principal->data);
}

static UINT32 rdpear_kerb_CreateApReqAuthenticator(const RDPEAR_SERVICE* rdpear,
                                                   const CreateApReqAuthenticatorReq* req,
                                                   CreateApReqAuthenticatorResp* resp)
{
	krb5_error_code rv = 0;
	wStream* asn1EncodedAuth = nullptr;
	krb5_data authenticator = WINPR_C_ARRAY_INIT;
	krb5_data* der = nullptr;
	krb5_keyblock* subkey = nullptr;
	krb5_principal_data client = WINPR_C_ARRAY_INIT;

	UINT32 status = ERROR_INVALID_DATA;
	WLog_DBG(TAG, "-> CreateApReqAuthenticator");

	krb5_authdata authdata = WINPR_C_ARRAY_INIT;
	krb5_authdata* authDataPtr[2] = { &authdata, nullptr };
	BOOL haveData = 0;

	if (!extractAuthData(req->AuthData, &authdata, &haveData))
	{
		WLog_ERR(TAG, "error retrieving auth data");
		winpr_HexDump(TAG, WLOG_DEBUG, req->AuthData->Asn1Buffer,
		              req->AuthData->Asn1BufferHints.count);
		goto out;
	}

	if (req->SkewTime->QuadPart)
	{
		WLog_ERR(TAG, "!!!!! should handle SkewTime !!!!!");
	}

	if (req->SubKey)
	{
		rv = RPC_ENCRYPTION_KEY_to_keyblock(rdpear->krbContext, req->SubKey, &subkey);
		if (rv)
		{
			WLog_ERR(TAG, "error importing subkey");
			goto out;
		}
	}

	krb5_authenticator authent = { .checksum = nullptr,
		                           .subkey = nullptr,
		                           .seq_number = req->SequenceNumber,
		                           .authorization_data = haveData ? authDataPtr : nullptr };

	client.type = req->ClientName->NameType;
	if (req->ClientName->nameHints.count > INT32_MAX)
		goto out;

	client.length = (krb5_int32)req->ClientName->nameHints.count;
	client.data = calloc(req->ClientName->nameHints.count, sizeof(krb5_data));
	if (!client.data)
		goto out;

	for (int i = 0; i < client.length; i++)
	{
		krb5_data* cur = &client.data[i];
		cur->data = KERB_RPC_UNICODESTR_to_charptr(&req->ClientName->Names[i]);
		if (!cur->data)
			goto out;
		const size_t len = strnlen(cur->data, MAX_KEYTAB_NAME_LEN + 1);
		if (len > MAX_KEYTAB_NAME_LEN)
		{
			WLog_ERR(TAG,
			         "Invalid ClientName length %d, limited to %" PRIuz
			         " characters. ClientName: (%s)",
			         MAX_KEYTAB_NAME_LEN, len, cur->data);
			goto out;
		}
		cur->length = (unsigned int)len;
	}
	client.realm.data = KERB_RPC_UNICODESTR_to_charptr(req->ClientRealm);
	if (!client.realm.data)
		goto out;

	const size_t len = strnlen(client.realm.data, MAX_KEYTAB_NAME_LEN + 1);
	if (len > MAX_KEYTAB_NAME_LEN)
	{
		WLog_ERR(TAG, "Invalid realm length %d, limited to %" PRIuz "characters. Realm: (%s)",
		         MAX_KEYTAB_NAME_LEN, len, client.realm.data);
		goto out;
	}
	client.realm.length = (unsigned int)len;
	authent.client = &client;

	krb5_checksum checksum;
	krb5_checksum* pchecksum = nullptr;
	if (req->GssChecksum)
	{
		if (!extractChecksum(req->GssChecksum, &checksum))
		{
			WLog_ERR(TAG, "Error getting the checksum");
			goto out;
		}
		pchecksum = &checksum;
	}
	authent.checksum = pchecksum;

	krb5_us_timeofday(rdpear->krbContext, &authent.ctime, &authent.cusec);

	rv = encode_krb5_authenticator(&authent, &der);
	if (rv)
	{
		WLog_ERR(TAG, "error encoding authenticator");
		goto out;
	}

	KERB_ASN1_DATA plain_authent = { .Pdu = 0,
		                             .Asn1Buffer = (BYTE*)der->data,
		                             .Asn1BufferHints = { .count = der->length } };

	rv = kerb_do_encrypt(rdpear->krbContext, req->EncryptionKey, (krb5_keyusage)req->KeyUsage,
	                     &plain_authent, &authenticator);
	if (rv)
	{
		WLog_ERR(TAG, "error encrypting authenticator");
		goto out;
	}

	asn1EncodedAuth = rdpear_enc_EncryptedData(req->EncryptionKey->reserved2, &authenticator);
	if (!asn1EncodedAuth)
	{
		WLog_ERR(TAG, "error encoding to ASN1");
		rv = ENOMEM;
		goto out;
	}

	// WLog_DBG(TAG, "authenticator=");
	// winpr_HexDump(TAG, WLOG_DEBUG, Stream_Buffer(asn1EncodedAuth),
	// Stream_GetPosition(asn1EncodedAuth));

	if (!rdpear_set_asn1_buffer(&resp->Authenticator, Stream_Buffer(asn1EncodedAuth),
	                            Stream_GetPosition(asn1EncodedAuth)))
		goto out;
	resp->AuthenticatorTime.QuadPart = krb5_time_to_FILETIME(&authent.ctime, authent.cusec);
	status = 0;

out:
	/* Pdu is the slot of EncryptedData in the host's compiled Kerberos ASN.1 module: 6 up to
	 * Server 2022, 7 from Windows 11 24H2 / Server 2025, and an updated host rejects a mismatch.
	 * The host labels its GSS checksum with its own Checksum slot: 8 on the older module, blank
	 * on the newer one. */
	resp->Authenticator.Pdu = (req->GssChecksum && (req->GssChecksum->Pdu == 8)) ? 6 : 7;
	WLog_VRB(TAG, "host checksum PDU %" PRIu32 " -> authenticator PDU %" PRIu32,
	         req->GssChecksum ? req->GssChecksum->Pdu : 0, resp->Authenticator.Pdu);
	resp->KerbProtocolError = rv;
	krb5_free_principal_contents(rdpear->krbContext, &client);
	krb5_free_data(rdpear->krbContext, der);
	krb5_free_data_contents(rdpear->krbContext, &authenticator);
	if (subkey)
		krb5_free_keyblock(rdpear->krbContext, subkey);
	Stream_Free(asn1EncodedAuth, TRUE);
	return status;
}

static BOOL rdpear_findEncryptedData(const KERB_ASN1_DATA* src, int* penctype, krb5_data* data)
{
	WinPrAsn1Decoder dec = WINPR_C_ARRAY_INIT;
	WinPrAsn1Decoder dec2 = WINPR_C_ARRAY_INIT;
	WinPrAsn1Decoder_InitMem(&dec, WINPR_ASN1_DER, src->Asn1Buffer, src->Asn1BufferHints.count);
	BOOL error = FALSE;
	WinPrAsn1_INTEGER encType = 0;
	WinPrAsn1_OctetString os = WINPR_C_ARRAY_INIT;

	if (!WinPrAsn1DecReadSequence(&dec, &dec2) ||
	    !WinPrAsn1DecReadContextualInteger(&dec2, 0, &error, &encType) ||
	    !WinPrAsn1DecReadContextualOctetString(&dec2, 2, &error, &os, FALSE))
		return FALSE;

	if (os.len > UINT32_MAX)
		return FALSE;
	data->data = (char*)os.data;
	data->length = (unsigned int)os.len;
	*penctype = encType;
	return TRUE;
}

static UINT32 rdpear_kerb_UnpackKdcReplyBody(const RDPEAR_SERVICE* rdpear,
                                             const UnpackKdcReplyBodyReq* req,
                                             UnpackKdcReplyBodyResp* resp)
{
	if (req->StrengthenKey)
	{
		WLog_ERR(TAG, "StrengthenKey not supported yet");
		return ERROR_INVALID_DATA;
	}

	WLog_DBG(TAG, "-> UnpackKdcReplyBody: KeyUsage=0x%x PDU=0x%x", req->KeyUsage, req->Pdu);
	// WLog_DBG(TAG, "encryptedPayload=");
	// winpr_HexDump(TAG, WLOG_DEBUG, req->EncryptedData->Asn1Buffer,
	// req->EncryptedData->Asn1BufferHints.count);

	krb5_data asn1Data = WINPR_C_ARRAY_INIT;
	int encType = 0;
	if (!rdpear_findEncryptedData(req->EncryptedData, &encType, &asn1Data) || !asn1Data.length)
		return ERROR_INVALID_DATA;

	resp->KerbProtocolError = kerb_do_decrypt(
	    rdpear->krbContext, req->Key, (krb5_keyusage)req->KeyUsage, &asn1Data, &resp->ReplyBody);
	resp->ReplyBody.Pdu = req->Pdu;

	return 0;
}

static UINT32 rdpear_kerb_DecryptApReply(const RDPEAR_SERVICE* rdpear, const DecryptApReplyReq* req,
                                         KERB_ASN1_DATA* resp)
{
	WLog_DBG(TAG, "-> DecryptApReply");
	// winpr_HexDump(TAG, WLOG_DEBUG, req->EncryptedReply->Asn1Buffer,
	// req->EncryptedReply->Asn1BufferHints.count);

	krb5_data asn1Data = WINPR_C_ARRAY_INIT;
	int encType = 0;
	if (!rdpear_findEncryptedData(req->EncryptedReply, &encType, &asn1Data) || !asn1Data.length)
		return ERROR_INVALID_DATA;

	resp->Pdu = 0x31;
	krb5_error_code rv = kerb_do_decrypt(rdpear->krbContext, req->Key, KRB5_KEYUSAGE_AP_REP_ENCPART,
	                                     &asn1Data, resp);
	if (rv != 0)
	{
		WLog_ERR(TAG, "error decrypting");
		return ERROR_INVALID_DATA;
	}

	// WLog_DBG(TAG, "response=");
	// winpr_HexDump(TAG, WLOG_DEBUG, resp->Asn1Buffer, resp->Asn1BufferHints.count);
	return 0;
}

static UINT32 rdpear_kerb_PackApReply(const RDPEAR_SERVICE* rdpear, const PackApReplyReq* req,
                                      PackApReplyResp* resp)
{
	UINT32 status = ERROR_INVALID_DATA;
	krb5_data asn1Data = WINPR_C_ARRAY_INIT;
	krb5_data* out = nullptr;

	WLog_DBG(TAG, "-> PackApReply");

	krb5_error_code rv = kerb_do_encrypt(rdpear->krbContext, req->SessionKey,
	                                     KRB5_KEYUSAGE_AP_REP_ENCPART, req->ReplyBody, &asn1Data);
	if (rv)
		goto out;

	krb5_ap_rep reply;
	reply.enc_part.kvno = KRB5_PVNO;
	reply.enc_part.enctype = (krb5_enctype)req->SessionKey->reserved2;
	reply.enc_part.ciphertext.length = asn1Data.length;
	reply.enc_part.ciphertext.data = asn1Data.data;

	rv = encode_krb5_ap_rep(&reply, &out);
	if (rv)
		goto out;

	resp->PackedReplyHints.count = out->length;
	resp->PackedReply =
	    winpr_ndr_pointer_new(winpr_ndr_uint8Array_descr(), &resp->PackedReplyHints, out->length);
	if (!resp->PackedReply)
		goto out;
	memcpy(resp->PackedReply, out->data, out->length);
	status = 0;
out:
	krb5_free_data(rdpear->krbContext, out);
	krb5_free_data_contents(rdpear->krbContext, &asn1Data);
	return status;
}

/** @brief handles a Kerberos call
 *
 * @return TRUE if the call is handled, FALSE if it's not supported: it's answered without data
 */
static BOOL rdpear_kerb_call(const RDPEAR_SERVICE* rdpear, const KerbCredIsoRemoteInput* input,
                             KerbCredIsoRemoteOutput* output)
{
	const KerbCredIsoRemoteInputUnion* in = &input->Input;
	KerbCredIsoRemoteOutputUnion* out = &output->Output;

	output->CallId = input->CallId;
	switch (input->CallId)
	{
		case RemoteCallKerbNegotiateVersion:
			output->Status = rdpear_kerb_version(in->NegotiateVersion, &out->NegotiateVersion);
			return TRUE;
		case RemoteCallKerbCreateApReqAuthenticator:
			output->Status = rdpear_kerb_CreateApReqAuthenticator(
			    rdpear, &in->CreateApReqAuthenticator, &out->CreateApReqAuthenticator);
			return TRUE;
		case RemoteCallKerbDecryptApReply:
			output->Status =
			    rdpear_kerb_DecryptApReply(rdpear, &in->DecryptApReply, &out->DecryptApReply);
			return TRUE;
		case RemoteCallKerbComputeTgsChecksum:
			output->Status = rdpear_kerb_ComputeTgsChecksum(rdpear, &in->ComputeTgsChecksum,
			                                                &out->ComputeTgsChecksum);
			return TRUE;
		case RemoteCallKerbBuildEncryptedAuthData:
			output->Status = rdpear_kerb_BuildEncryptedAuthData(rdpear, &in->BuildEncryptedAuthData,
			                                                    &out->BuildEncryptedAuthData);
			return TRUE;
		case RemoteCallKerbUnpackKdcReplyBody:
			output->Status = rdpear_kerb_UnpackKdcReplyBody(rdpear, &in->UnpackKdcReplyBody,
			                                                &out->UnpackKdcReplyBody);
			return TRUE;
		case RemoteCallKerbPackApReply:
			output->Status = rdpear_kerb_PackApReply(rdpear, &in->PackApReply, &out->PackApReply);
			return TRUE;
		default:
			WLog_DBG(TAG, "Unhandled Kerberos callId=0x%x", input->CallId);
			return FALSE;
	}
}

/** @brief handles a NTLM call
 *
 * The NTLM supplemental credentials are not sent by the client, the calls using them are answered
 * with STATUS_NOT_SUPPORTED.
 *
 * @return TRUE if the call is handled, FALSE if it's not supported: it's answered without data
 */
static BOOL rdpear_ntlm_call(const NtlmCredIsoRemoteInput* input, NtlmCredIsoRemoteOutput* output)
{
	output->CallId = input->CallId;
	switch (input->CallId)
	{
		case RemoteCallNtlmNegotiateVersion:
			WLog_DBG(TAG, "-> NtlmNegotiateVersion(v=0x%x)", input->Input.NegotiateVersion);
			output->Output.NegotiateVersion = 0;
			output->Status = STATUS_SUCCESS;
			return TRUE;
		case RemoteCallNtlmLm20GetNtlm3ChallengeResponse:
		case RemoteCallNtlmCalculateNtResponse:
		case RemoteCallNtlmCalculateUserSessionKeyNt:
		case RemoteCallNtlmCompareCredentials:
			WLog_DBG(TAG, "-> NTLM callId=0x%x: no NTLM credentials", input->CallId);
			output->Status = (UINT32)STATUS_NOT_SUPPORTED;
			return TRUE;
		default:
			WLog_DBG(TAG, "Unhandled NTLM callId=0x%x", input->CallId);
			return FALSE;
	}
}

/* Decode one redirected call, run it and encode the reply: the NDR response wrapped in the
 * [MS-RDPEAR] payload envelope (SEQUENCE { [1] package, [2] payload }), ready for the caller to
 * encrypt for the wire. A call that is not supported is answered without data, as the in-tree
 * client does. */
static wStream* rdpear_kerb_dispatch(const RDPEAR_SERVICE* rdpear,
                                     const WinPrAsn1_OctetString* packageName, wStream* s)
{
	wStream* result = nullptr;
	WinPrNdrDecoder* decoder = nullptr;
	WinPrNdrEncoder* encoder = nullptr;
	KerbCredIsoRemoteInput kerbInput = WINPR_C_ARRAY_INIT;
	KerbCredIsoRemoteOutput kerbOutput = WINPR_C_ARRAY_INIT;
	NtlmCredIsoRemoteInput ntlmInput = WINPR_C_ARRAY_INIT;
	NtlmCredIsoRemoteOutput ntlmOutput = WINPR_C_ARRAY_INIT;
	BOOL encoded = FALSE;

	const RdpEarPackageType packageType = rdpear_packageType_from_name(packageName);
	if (packageType == RDPEAR_PACKAGE_UNKNOWN)
	{
		WLog_ERR(TAG, "unknown package type");
		return nullptr;
	}

	wStream* respStream = Stream_New(nullptr, 500);
	if (!respStream)
		return nullptr;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 16))
		goto out;
	Stream_Seek(s, 16); /* skip first 16 bytes */

	decoder = winpr_ndr_decoder_new_fromStream(s);
	if (!decoder)
		goto out;

	encoder = winpr_ndr_encoder_from_decoder(decoder);
	if (!encoder)
		goto out;

	Stream_Write(respStream, payloadHeader, sizeof(payloadHeader));

	if (packageType == RDPEAR_PACKAGE_KERBEROS)
	{
		if (!rdpear_kerb_input_read(decoder, s, &kerbInput))
			goto out;

		(void)rdpear_kerb_call(rdpear, &kerbInput, &kerbOutput);
		encoded = rdpear_kerb_output_write(encoder, respStream, &kerbOutput);
	}
	else
	{
		if (!rdpear_ntlm_input_read(decoder, s, &ntlmInput))
			goto out;

		(void)rdpear_ntlm_call(&ntlmInput, &ntlmOutput);
		encoded = rdpear_ntlm_output_write(encoder, respStream, &ntlmOutput);
	}

	if (!encoded)
	{
		WLog_DBG(TAG, "error encoding the reply");
		goto out;
	}

	result = rdpear_encodePayload(packageType, respStream);

out:
	rdpear_kerb_input_free(&kerbInput);
	rdpear_kerb_output_free(&kerbOutput);
	rdpear_ntlm_input_free(&ntlmInput);
	rdpear_ntlm_output_free(&ntlmOutput);
	winpr_ndr_decoder_destroy(&decoder);
	winpr_ndr_encoder_destroy(&encoder);
	Stream_Free(respStream, TRUE);
	return result;
}

wStream* rdpear_kerb_service(krb5_context krb, const BYTE* plaintext, size_t plaintextLen)
{
	WINPR_ASSERT(krb);
	WINPR_ASSERT(plaintext);

	const RDPEAR_SERVICE service = { .krbContext = krb };

	wStream decodedStream = WINPR_C_ARRAY_INIT;
	Stream_StaticConstInit(&decodedStream, plaintext, plaintextLen);

	WinPrAsn1Decoder dec = WinPrAsn1Decoder_init();
	WinPrAsn1Decoder dec2 = WinPrAsn1Decoder_init();
	WinPrAsn1Decoder_Init(&dec, WINPR_ASN1_DER, &decodedStream);

	if (!WinPrAsn1DecReadSequence(&dec, &dec2))
		return nullptr;

	WinPrAsn1_OctetString packageName = WINPR_C_ARRAY_INIT;
	WinPrAsn1_OctetString payload = WINPR_C_ARRAY_INIT;
	BOOL error = FALSE;
	if (!WinPrAsn1DecReadContextualOctetString(&dec2, 1, &error, &packageName, FALSE) ||
	    !WinPrAsn1DecReadContextualOctetString(&dec2, 2, &error, &payload, FALSE))
		return nullptr;

	wStream payloadStream = WINPR_C_ARRAY_INIT;
	Stream_StaticInit(&payloadStream, payload.data, payload.len);

	return rdpear_kerb_dispatch(&service, &packageName, &payloadStream);
}
