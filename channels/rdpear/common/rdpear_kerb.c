/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Authentication redirection virtual channel: Kerberos redirected-call servicing
 *
 * The [MS-RDPEAR] redirected calls are serviced here so both the in-process channel client
 * (rdpear_main.c) and the lsa4posix daemon can dispatch them. The handlers run krb5 crypto on
 * keys carried in each request; they do not touch the ticket cache.
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
#include <krb5.h>
#include <errno.h>

#include <winpr/assert.h>
#include <winpr/wtypes.h>
#include <winpr/crt.h>
#include <winpr/wlog.h>
#include <winpr/print.h>
#include <winpr/asn1.h>
#include <rdpear-common/ndr.h>

#include <rdpear-common/rdpear_common.h>
#include <rdpear-common/rdpear_asn1.h>
#include <rdpear-common/rdpear_kerb.h>

#include <freerdp/channels/log.h>

#define TAG CHANNELS_TAG("rdpear.kerb")

#ifndef MAX_KEYTAB_NAME_LEN
#define MAX_KEYTAB_NAME_LEN 1100 /* Defined in MIT krb5.h */
#endif

/* defined in libkrb5 */
krb5_error_code encode_krb5_authenticator(const krb5_authenticator* rep, krb5_data** code_out);
krb5_error_code encode_krb5_ap_rep(const krb5_ap_rep* rep, krb5_data** code_out);

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
	if (rv)
	{
		free(data.data);
		goto out;
	}

	plain->Asn1Buffer = (BYTE*)data.data;
	plain->Asn1BufferHints.count = data.length;
out:
	krb5_free_keyblock(ctx, keyblock);
	return rv;
}

static BOOL rdpear_prepare_response(NdrContext* rcontext, BOOL isKerb, UINT16 callId, UINT32 status,
                                    NdrContext** pwcontext, wStream* retStream)
{
	WINPR_ASSERT(rcontext);
	WINPR_ASSERT(pwcontext);

	BOOL ret = FALSE;
	*pwcontext = nullptr;
	NdrContext* wcontext = ndr_context_copy(rcontext);
	if (!wcontext)
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(retStream, sizeof(payloadHeader)))
		goto out;

	Stream_Write(retStream, payloadHeader, sizeof(payloadHeader));

	if (!ndr_write_header(wcontext, retStream) || !ndr_start_constructed(wcontext, retStream) ||
	    !ndr_write_pickle(wcontext, retStream)) /* pickle header */
		goto out;
	if (isKerb)
	{
		/* for some reason there's 4 zero undocumented bytes here after the pickle record
		 * in the kerberos package packets */
		UINT32 v = 0;
		if (!ndr_write_uint32(wcontext, retStream, v))
			goto out;
	}
	if (!ndr_write_uint16(wcontext, retStream, callId) || /* callId */
	    !ndr_write_uint16(wcontext, retStream, 0x0000) || /* align padding */
	    !ndr_write_uint32(wcontext, retStream, status) || /* status */
	    !ndr_write_uint16(wcontext, retStream, callId) || /* callId */
	    !ndr_write_uint16(wcontext, retStream, 0x0000))   /* align padding */
		goto out;

	*pwcontext = wcontext;
	ret = TRUE;
out:
	if (!ret)
		ndr_context_destroy(&wcontext);
	return ret;
}

static BOOL rdpear_kerb_version(NdrContext* rcontext, wStream* s, UINT32* pstatus, UINT32* pversion)
{
	*pstatus = ERROR_INVALID_DATA;

	if (!ndr_read_uint32(rcontext, s, pversion))
		return TRUE;

	WLog_DBG(TAG, "-> KerbNegotiateVersion(v=0x%x)", *pversion);
	*pstatus = 0;

	return TRUE;
}

static BOOL rdpear_kerb_ComputeTgsChecksum(krb5_context krb, NdrContext* rcontext, wStream* s,
                                           UINT32* pstatus, KERB_ASN1_DATA* resp)
{
	ComputeTgsChecksumReq req = WINPR_C_ARRAY_INIT;
	krb5_checksum checksum = WINPR_C_ARRAY_INIT;
	wStream* asn1Payload = nullptr;

	*pstatus = ERROR_INVALID_DATA;
	WLog_DBG(TAG, "-> ComputeTgsChecksum");

	if (!ndr_read_ComputeTgsChecksumReq(rcontext, s, nullptr, &req) ||
	    !ndr_treat_deferred_read(rcontext, s))
		goto out;
	// ComputeTgsChecksumReq_dump(WLog_Get(""), WLOG_DEBUG, &req);

	krb5_error_code rv =
	    kerb_do_checksum(krb, req.Key, KRB5_KEYUSAGE_TGS_REQ_AUTH_CKSUM,
	                     (krb5_cksumtype)req.ChecksumType, req.requestBody, &checksum);
	if (rv)
		goto out;

	asn1Payload = rdpear_enc_Checksum(req.ChecksumType, &checksum);
	if (!asn1Payload)
		goto out;

	resp->Pdu = 8;
	resp->Asn1Buffer = Stream_Buffer(asn1Payload);
	const size_t pos = Stream_GetPosition(asn1Payload);
	if (pos > UINT32_MAX)
		goto out;
	resp->Asn1BufferHints.count = (UINT32)pos;
	*pstatus = 0;

out:
	ndr_destroy_ComputeTgsChecksumReq(rcontext, nullptr, &req);
	krb5_free_checksum_contents(krb, &checksum);
	Stream_Free(asn1Payload, FALSE);
	return TRUE;
}

static BOOL rdpear_kerb_BuildEncryptedAuthData(krb5_context krb, NdrContext* rcontext, wStream* s,
                                               UINT32* pstatus, KERB_ASN1_DATA* asn1)
{
	BuildEncryptedAuthDataReq req = WINPR_C_ARRAY_INIT;
	krb5_data encrypted = WINPR_C_ARRAY_INIT;
	wStream* asn1Payload = nullptr;
	krb5_error_code rv = 0;

	*pstatus = ERROR_INVALID_DATA;
	WLog_DBG(TAG, "-> BuildEncryptedAuthData");

	if (!ndr_read_BuildEncryptedAuthDataReq(rcontext, s, nullptr, &req) ||
	    !ndr_treat_deferred_read(rcontext, s))
		goto out;

	rv = kerb_do_encrypt(krb, req.Key, (krb5_keyusage)req.KeyUsage, req.PlainAuthData, &encrypted);
	if (rv)
		goto out;

	/* do the encoding */
	asn1Payload = rdpear_enc_EncryptedData(req.Key->reserved2, &encrypted);
	if (!asn1Payload)
		goto out;

	//	WLog_DBG(TAG, "rdpear_kerb_BuildEncryptedAuthData resp=");
	//	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Buffer(asn1Payload), Stream_GetPosition(asn1Payload));
	asn1->Pdu = 6;
	asn1->Asn1Buffer = Stream_Buffer(asn1Payload);
	const size_t pos = Stream_GetPosition(asn1Payload);
	if (pos > UINT32_MAX)
		goto out;
	asn1->Asn1BufferHints.count = (UINT32)pos;
	*pstatus = 0;

out:
	krb5_free_data_contents(krb, &encrypted);
	ndr_destroy_BuildEncryptedAuthDataReq(rcontext, nullptr, &req);
	Stream_Free(asn1Payload, FALSE);
	return TRUE;
}

static char* KERB_RPC_UNICODESTR_to_charptr(const RPC_UNICODE_STRING* src)
{
	WINPR_ASSERT(src);
	return ConvertWCharNToUtf8Alloc(src->Buffer, src->strLength, nullptr);
}

static BOOL extractAuthData(const KERB_ASN1_DATA* src, krb5_authdata* authData, BOOL* haveData)
{
	WinPrAsn1Decoder dec = WinPrAsn1Decoder_init();
	WinPrAsn1Decoder dec2 = WinPrAsn1Decoder_init();
	WinPrAsn1Decoder dec3 = WinPrAsn1Decoder_init();
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
	WinPrAsn1Decoder dec = WinPrAsn1Decoder_init();
	WinPrAsn1Decoder dec2 = WinPrAsn1Decoder_init();
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

static BOOL rdpear_kerb_CreateApReqAuthenticator(krb5_context krb, NdrContext* rcontext, wStream* s,
                                                 UINT32* pstatus,
                                                 CreateApReqAuthenticatorResp* resp)
{
	krb5_error_code rv = 0;
	wStream* asn1EncodedAuth = nullptr;
	CreateApReqAuthenticatorReq req = WINPR_C_ARRAY_INIT;
	krb5_data authenticator = WINPR_C_ARRAY_INIT;
	krb5_data* der = nullptr;
	krb5_keyblock* subkey = nullptr;
	krb5_principal_data client = WINPR_C_ARRAY_INIT;

	*pstatus = ERROR_INVALID_DATA;
	WLog_DBG(TAG, "-> CreateApReqAuthenticator");

	if (!ndr_read_CreateApReqAuthenticatorReq(rcontext, s, nullptr, &req) ||
	    !ndr_treat_deferred_read(rcontext, s))
		goto out;

	krb5_authdata authdata = WINPR_C_ARRAY_INIT;
	krb5_authdata* authDataPtr[2] = { &authdata, nullptr };
	BOOL haveData = 0;

	if (!extractAuthData(req.AuthData, &authdata, &haveData))
	{
		WLog_ERR(TAG, "error retrieving auth data");
		winpr_HexDump(TAG, WLOG_DEBUG, req.AuthData->Asn1Buffer,
		              req.AuthData->Asn1BufferHints.count);
		goto out;
	}

	if (req.SkewTime->QuadPart)
	{
		WLog_ERR(TAG, "!!!!! should handle SkewTime !!!!!");
	}

	if (req.SubKey)
	{
		rv = RPC_ENCRYPTION_KEY_to_keyblock(krb, req.SubKey, &subkey);
		if (rv)
		{
			WLog_ERR(TAG, "error importing subkey");
			goto out;
		}
	}

	krb5_authenticator authent = { .checksum = nullptr,
		                           .subkey = nullptr,
		                           .seq_number = req.SequenceNumber,
		                           .authorization_data = haveData ? authDataPtr : nullptr };

	client.type = req.ClientName->NameType;
	if (req.ClientName->nameHints.count > INT32_MAX)
		goto out;

	client.length = (krb5_int32)req.ClientName->nameHints.count;
	client.data = calloc(req.ClientName->nameHints.count, sizeof(krb5_data));
	if (!client.data)
		goto out;

	for (int i = 0; i < client.length; i++)
	{
		krb5_data* cur = &client.data[i];
		cur->data = KERB_RPC_UNICODESTR_to_charptr(&req.ClientName->Names[i]);
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
	client.realm.data = KERB_RPC_UNICODESTR_to_charptr(req.ClientRealm);
	if (!client.realm.data)
		goto out;

	const size_t len = strnlen(client.realm.data, MAX_KEYTAB_NAME_LEN + 1);
	if (len > MAX_KEYTAB_NAME_LEN)
	{
		WLog_ERR(TAG, "Invalid realm length %d, limited to %" PRIuz " characters. Realm: (%s)",
		         MAX_KEYTAB_NAME_LEN, len, client.realm.data);
		goto out;
	}
	client.realm.length = (unsigned int)len;
	authent.client = &client;

	krb5_checksum checksum;
	krb5_checksum* pchecksum = nullptr;
	if (req.GssChecksum)
	{
		if (!extractChecksum(req.GssChecksum, &checksum))
		{
			WLog_ERR(TAG, "Error getting the checksum");
			goto out;
		}
		pchecksum = &checksum;
	}
	authent.checksum = pchecksum;

	krb5_us_timeofday(krb, &authent.ctime, &authent.cusec);

	rv = encode_krb5_authenticator(&authent, &der);
	if (rv)
	{
		WLog_ERR(TAG, "error encoding authenticator");
		goto out;
	}

	KERB_ASN1_DATA plain_authent = { .Pdu = 0,
		                             .Asn1Buffer = (BYTE*)der->data,
		                             .Asn1BufferHints = { .count = der->length } };

	rv = kerb_do_encrypt(krb, req.EncryptionKey, (krb5_keyusage)req.KeyUsage, &plain_authent,
	                     &authenticator);
	if (rv)
	{
		WLog_ERR(TAG, "error encrypting authenticator");
		goto out;
	}

	asn1EncodedAuth = rdpear_enc_EncryptedData(req.EncryptionKey->reserved2, &authenticator);
	if (!asn1EncodedAuth)
	{
		WLog_ERR(TAG, "error encoding to ASN1");
		rv = ENOMEM;
		goto out;
	}

	// WLog_DBG(TAG, "authenticator=");
	// winpr_HexDump(TAG, WLOG_DEBUG, Stream_Buffer(asn1EncodedAuth),
	// Stream_GetPosition(asn1EncodedAuth));

	const size_t size = Stream_GetPosition(asn1EncodedAuth);
	if (size > UINT32_MAX)
		goto out;
	resp->Authenticator.Asn1BufferHints.count = (UINT32)size;
	resp->Authenticator.Asn1Buffer = Stream_Buffer(asn1EncodedAuth);
	resp->AuthenticatorTime.QuadPart = krb5_time_to_FILETIME(&authent.ctime, authent.cusec);
	*pstatus = 0;

out:
	resp->Authenticator.Pdu = 7;
	resp->KerbProtocolError = rv;
	krb5_free_principal_contents(krb, &client);
	krb5_free_data(krb, der);
	krb5_free_data_contents(krb, &authenticator);
	if (subkey)
		krb5_free_keyblock(krb, subkey);
	ndr_destroy_CreateApReqAuthenticatorReq(rcontext, nullptr, &req);
	Stream_Free(asn1EncodedAuth, FALSE);
	return TRUE;
}

static BOOL rdpear_findEncryptedData(const KERB_ASN1_DATA* src, int* penctype, krb5_data* data)
{
	WinPrAsn1Decoder dec = WinPrAsn1Decoder_init();
	WinPrAsn1Decoder dec2 = WinPrAsn1Decoder_init();
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

static BOOL rdpear_kerb_UnpackKdcReplyBody(krb5_context krb, NdrContext* rcontext, wStream* s,
                                           UINT32* pstatus, UnpackKdcReplyBodyResp* resp)
{
	UnpackKdcReplyBodyReq req = WINPR_C_ARRAY_INIT;

	*pstatus = ERROR_INVALID_DATA;

	if (!ndr_read_UnpackKdcReplyBodyReq(rcontext, s, nullptr, &req) ||
	    !ndr_treat_deferred_read(rcontext, s))
		goto out;

	if (req.StrengthenKey)
	{
		WLog_ERR(TAG, "StrengthenKey not supported yet");
		goto out;
	}

	WLog_DBG(TAG, "-> UnpackKdcReplyBody: KeyUsage=0x%x PDU=0x%x", req.KeyUsage, req.Pdu);
	// WLog_DBG(TAG, "encryptedPayload=");
	// winpr_HexDump(TAG, WLOG_DEBUG, req.EncryptedData->Asn1Buffer,
	// req.EncryptedData->Asn1BufferHints.count);

	krb5_data asn1Data = WINPR_C_ARRAY_INIT;
	int encType = 0;
	if (!rdpear_findEncryptedData(req.EncryptedData, &encType, &asn1Data) || !asn1Data.length)
		goto out;

	resp->KerbProtocolError =
	    kerb_do_decrypt(krb, req.Key, (krb5_keyusage)req.KeyUsage, &asn1Data, &resp->ReplyBody);
	resp->ReplyBody.Pdu = req.Pdu;

	*pstatus = 0;

out:
	ndr_destroy_UnpackKdcReplyBodyReq(rcontext, nullptr, &req);
	return TRUE;
}

static BOOL rdpear_kerb_DecryptApReply(krb5_context krb, NdrContext* rcontext, wStream* s,
                                       UINT32* pstatus, KERB_ASN1_DATA* resp)
{
	DecryptApReplyReq req = WINPR_C_ARRAY_INIT;

	*pstatus = ERROR_INVALID_DATA;
	if (!ndr_read_DecryptApReplyReq(rcontext, s, nullptr, &req) ||
	    !ndr_treat_deferred_read(rcontext, s))
		goto out;

	WLog_DBG(TAG, "-> DecryptApReply");
	// winpr_HexDump(TAG, WLOG_DEBUG, req.EncryptedReply->Asn1Buffer,
	// req.EncryptedReply->Asn1BufferHints.count);

	krb5_data asn1Data = WINPR_C_ARRAY_INIT;
	int encType = 0;
	if (!rdpear_findEncryptedData(req.EncryptedReply, &encType, &asn1Data) || !asn1Data.length)
		goto out;

	resp->Pdu = 0x31;
	krb5_error_code rv =
	    kerb_do_decrypt(krb, req.Key, KRB5_KEYUSAGE_AP_REP_ENCPART, &asn1Data, resp);
	if (rv != 0)
	{
		WLog_ERR(TAG, "error decrypting");
		goto out;
	}

	// WLog_DBG(TAG, "response=");
	// winpr_HexDump(TAG, WLOG_DEBUG, resp->Asn1Buffer, resp->Asn1BufferHints.count);
	*pstatus = 0;
out:
	ndr_destroy_DecryptApReplyReq(rcontext, nullptr, &req);
	return TRUE;
}

static BOOL rdpear_kerb_PackApReply(krb5_context krb, NdrContext* rcontext, wStream* s,
                                    UINT32* pstatus, PackApReplyResp* resp)
{
	PackApReplyReq req = WINPR_C_ARRAY_INIT;
	krb5_data asn1Data = WINPR_C_ARRAY_INIT;
	krb5_data* out = nullptr;

	WLog_DBG(TAG, "-> PackApReply");
	*pstatus = ERROR_INVALID_DATA;
	if (!ndr_read_PackApReplyReq(rcontext, s, nullptr, &req) ||
	    !ndr_treat_deferred_read(rcontext, s))
		goto out;

	krb5_error_code rv = kerb_do_encrypt(krb, req.SessionKey, KRB5_KEYUSAGE_AP_REP_ENCPART,
	                                     req.ReplyBody, &asn1Data);
	if (rv)
		goto out;

	krb5_ap_rep reply;
	reply.enc_part.kvno = KRB5_PVNO;
	reply.enc_part.enctype = (krb5_enctype)req.SessionKey->reserved2;
	reply.enc_part.ciphertext.length = asn1Data.length;
	reply.enc_part.ciphertext.data = asn1Data.data;

	rv = encode_krb5_ap_rep(&reply, &out);
	if (rv)
		goto out;

	resp->PackedReply = (BYTE*)out->data;
	resp->PackedReplyHints.count = out->length;
	*pstatus = 0;
out:
	free(out);
	krb5_free_data_contents(krb, &asn1Data);
	ndr_destroy_PackApReplyReq(rcontext, nullptr, &req);
	return TRUE;
}

static BOOL rdpear_ntlm_version(NdrContext* rcontext, wStream* s, UINT32* pstatus, UINT32* pversion)
{
	*pstatus = ERROR_INVALID_DATA;

	if (!ndr_read_uint32(rcontext, s, pversion))
		return TRUE;

	WLog_DBG(TAG, "-> NtlmNegotiateVersion(v=0x%x)", *pversion);
	*pstatus = 0;

	return TRUE;
}

/* Dispatch one decrypted [MS-RDPEAR] payload (the [2] payload octet string) to the matching
 * Kerberos/NTLM handler and encode the NDR response. Returns the inner response stream. */
static wStream* rdpear_kerb_dispatch(krb5_context krb, const WinPrAsn1_OctetString* packageName,
                                     wStream* s, BOOL* pIsKerb)
{
	NdrContext* context = nullptr;
	NdrContext* wcontext = nullptr;
	UINT32 status = 0;

	UINT32 uint32Resp = 0;
	KERB_ASN1_DATA asn1Data = WINPR_C_ARRAY_INIT;
	CreateApReqAuthenticatorResp createApReqAuthenticatorResp = WINPR_C_ARRAY_INIT;
	UnpackKdcReplyBodyResp unpackKdcReplyBodyResp = WINPR_C_ARRAY_INIT;
	PackApReplyResp packApReplyResp = WINPR_C_ARRAY_INIT;
	void* resp = nullptr;
	NdrMessageType respDescr = nullptr;
	BOOL ok = FALSE;

	wStream* respStream = Stream_New(nullptr, 500);
	if (!respStream)
		goto out;

	BOOL isKerb = FALSE;
	switch (rdpear_packageType_from_name(packageName))
	{
		case RDPEAR_PACKAGE_KERBEROS:
			isKerb = TRUE;
			break;
		case RDPEAR_PACKAGE_NTLM:
			isKerb = FALSE;
			break;
		default:
			WLog_ERR(TAG, "unknown package type");
			goto out;
	}
	*pIsKerb = isKerb;

	Stream_Seek(s, 16); /* skip first 16 bytes */
	wStream commandStream = WINPR_C_ARRAY_INIT;
	UINT16 callId = 0;
	UINT16 callId2 = 0;

	context = ndr_read_header(s);
	if (!context || !ndr_read_constructed(context, s, &commandStream) ||
	    !ndr_read_pickle(context, &commandStream))
		goto out;

	if (isKerb)
	{
		/* 4 zero undocumented bytes after the pickle record in the kerberos package packets */
		UINT32 v = 0;
		if (!ndr_read_uint32(context, &commandStream, &v))
			goto out;
	}

	if (!ndr_read_uint16(context, &commandStream, &callId) ||
	    !ndr_read_uint16(context, &commandStream, &callId2) || (callId != callId2))
		goto out;

	switch (callId)
	{
		case RemoteCallKerbNegotiateVersion:
			resp = &uint32Resp;
			respDescr = ndr_uint32_descr();
			rdpear_kerb_version(context, &commandStream, &status, &uint32Resp);
			break;
		case RemoteCallKerbCreateApReqAuthenticator:
			resp = &createApReqAuthenticatorResp;
			respDescr = ndr_CreateApReqAuthenticatorResp_descr();
			rdpear_kerb_CreateApReqAuthenticator(krb, context, &commandStream, &status,
			                                     &createApReqAuthenticatorResp);
			break;
		case RemoteCallKerbDecryptApReply:
			resp = &asn1Data;
			respDescr = ndr_KERB_ASN1_DATA_descr();
			rdpear_kerb_DecryptApReply(krb, context, &commandStream, &status, &asn1Data);
			break;
		case RemoteCallKerbComputeTgsChecksum:
			resp = &asn1Data;
			respDescr = ndr_KERB_ASN1_DATA_descr();
			rdpear_kerb_ComputeTgsChecksum(krb, context, &commandStream, &status, &asn1Data);
			break;
		case RemoteCallKerbBuildEncryptedAuthData:
			resp = &asn1Data;
			respDescr = ndr_KERB_ASN1_DATA_descr();
			rdpear_kerb_BuildEncryptedAuthData(krb, context, &commandStream, &status, &asn1Data);
			break;
		case RemoteCallKerbUnpackKdcReplyBody:
			resp = &unpackKdcReplyBodyResp;
			respDescr = ndr_UnpackKdcReplyBodyResp_descr();
			rdpear_kerb_UnpackKdcReplyBody(krb, context, &commandStream, &status,
			                               &unpackKdcReplyBodyResp);
			break;
		case RemoteCallKerbPackApReply:
			resp = &packApReplyResp;
			respDescr = ndr_PackApReplyResp_descr();
			rdpear_kerb_PackApReply(krb, context, &commandStream, &status, &packApReplyResp);
			break;
		case RemoteCallNtlmNegotiateVersion:
			resp = &uint32Resp;
			respDescr = ndr_uint32_descr();
			rdpear_ntlm_version(context, &commandStream, &status, &uint32Resp);
			break;
		default:
			WLog_DBG(TAG, "Unhandled callId=0x%x", callId);
			goto out;
	}

	if (!rdpear_prepare_response(context, isKerb, callId, status, &wcontext, respStream))
		goto out;

	if (resp && respDescr)
	{
		WINPR_ASSERT(respDescr->writeFn);
		BOOL r = respDescr->writeFn(wcontext, respStream, nullptr, resp) &&
		         ndr_treat_deferred_write(wcontext, respStream);
		if (respDescr->destroyFn)
			respDescr->destroyFn(wcontext, nullptr, resp);
		if (!r)
			goto out;
	}

	if (!ndr_end_constructed(wcontext, respStream))
		goto out;

	ok = TRUE;
out:
	if (context)
		ndr_context_destroy(&context);
	if (wcontext)
		ndr_context_destroy(&wcontext);
	if (!ok)
	{
		Stream_Free(respStream, TRUE);
		return nullptr;
	}
	return respStream;
}

wStream* rdpear_kerb_service(krb5_context krb, const BYTE* plaintext, size_t plaintextLen)
{
	WINPR_ASSERT(krb);
	WINPR_ASSERT(plaintext);

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

	BOOL isKerb = FALSE;
	wStream* respStream = rdpear_kerb_dispatch(krb, &packageName, &payloadStream, &isKerb);
	if (!respStream)
		return nullptr;

	/* Wrap the NDR response in the [MS-RDPEAR] payload envelope (SEQUENCE { [1] package,
	 * [2] payload }); the caller encrypts the result for the wire. */
	wStream* encoded = rdpear_encodePayload(isKerb, respStream);
	Stream_Free(respStream, TRUE);
	return encoded;
}
