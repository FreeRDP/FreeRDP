/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Authentication redirection virtual channel
 *
 * Copyright 2023 David Fort <contact@hardening-consulting.com>
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
#include <winpr/sspi.h>
#include <winpr/collections.h>

#include <rdpear-common/ndr.h>
#include <rdpear-common/rdpear_common.h>
#include <rdpear-common/rdpear_asn1.h>

#include <freerdp/config.h>
#include <freerdp/freerdp.h>
#include <freerdp/addin.h>
#include <freerdp/client/channels.h>
#include <freerdp/channels/log.h>
#include <freerdp/channels/rdpear.h>

#define TAG CHANNELS_TAG("rdpear.client")

/* defined in libkrb5 */
krb5_error_code encode_krb5_authenticator(const krb5_authenticator* rep, krb5_data** code_out);
krb5_error_code encode_krb5_ap_rep(const krb5_ap_rep* rep, krb5_data** code_out);

typedef struct
{
	GENERIC_DYNVC_PLUGIN base;
	rdpContext* rdp_context;
	krb5_context krbContext;
} RDPEAR_PLUGIN;

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

	krb5_keyblock* keyblock = NULL;
	krb5_data data = { 0 };

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

	krb5_keyblock* keyblock = NULL;
	krb5_data data = { 0 };
	krb5_enc_data enc = { 0 };
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

	rv = krb5_c_encrypt(ctx, keyblock, kusage, NULL, &data, &enc);

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

	krb5_keyblock* keyblock = NULL;
	krb5_data data = { 0 };
	krb5_enc_data enc = { 0 };

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

	rv = krb5_c_decrypt(ctx, keyblock, kusage, NULL, &enc, &data);

	plain->Asn1Buffer = (BYTE*)data.data;
	plain->Asn1BufferHints.count = data.length;
out:
	krb5_free_keyblock(ctx, keyblock);
	return rv;
}

static BOOL rdpear_send_payload(RDPEAR_PLUGIN* rdpear, IWTSVirtualChannelCallback* pChannelCallback,
                                RdpEarPackageType packageType, wStream* payload)
{
	GENERIC_CHANNEL_CALLBACK* callback = (GENERIC_CHANNEL_CALLBACK*)pChannelCallback;
	BOOL ret = FALSE;
	wStream* finalStream = NULL;
	SecBuffer cryptedBuffer = { 0 };
	wStream* unencodedContent = rdpear_encodePayload(packageType, payload);
	if (!unencodedContent)
		goto out;

	const size_t unencodedLen = Stream_GetPosition(unencodedContent);

#if UINT32_MAX < SIZE_MAX
	if (unencodedLen > UINT32_MAX)
		goto out;
#endif

	SecBuffer inBuffer = { (ULONG)unencodedLen, SECBUFFER_DATA, Stream_Buffer(unencodedContent) };

	if (!freerdp_nla_encrypt(rdpear->rdp_context, &inBuffer, &cryptedBuffer))
		goto out;

	finalStream = Stream_New(NULL, 200);
	if (!finalStream)
		goto out;
	Stream_Write_UINT32(finalStream, 0x4EACC3C8);             /* ProtocolMagic (4 bytes) */
	Stream_Write_UINT32(finalStream, cryptedBuffer.cbBuffer); /* Length (4 bytes) */
	Stream_Write_UINT32(finalStream, 0x00000000);             /* Version (4 bytes) */
	Stream_Write_UINT32(finalStream, 0x00000000);             /* Reserved (4 bytes) */
	Stream_Write_UINT64(finalStream, 0);                      /* TsPkgContext (8 bytes) */

	/* payload */
	if (!Stream_EnsureRemainingCapacity(finalStream, cryptedBuffer.cbBuffer))
		goto out;

	Stream_Write(finalStream, cryptedBuffer.pvBuffer, cryptedBuffer.cbBuffer);

	const size_t pos = Stream_GetPosition(finalStream);
#if UINT32_MAX < SIZE_MAX
	if (pos > UINT32_MAX)
		goto out;
#endif

	UINT status =
	    callback->channel->Write(callback->channel, (ULONG)pos, Stream_Buffer(finalStream), NULL);
	ret = (status == CHANNEL_RC_OK);
	if (!ret)
		WLog_DBG(TAG, "rdpear_send_payload=0x%x", status);
out:
	sspi_SecBufferFree(&cryptedBuffer);
	Stream_Free(unencodedContent, TRUE);
	Stream_Free(finalStream, TRUE);
	return ret;
}

static BOOL rdpear_prepare_response(NdrContext* rcontext, UINT16 callId, UINT32 status,
                                    NdrContext** pwcontext, wStream* retStream)
{
	WINPR_ASSERT(rcontext);
	WINPR_ASSERT(pwcontext);

	BOOL ret = FALSE;
	*pwcontext = NULL;
	NdrContext* wcontext = ndr_context_copy(rcontext);
	if (!wcontext)
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(retStream, sizeof(payloadHeader)))
		goto out;

	Stream_Write(retStream, payloadHeader, sizeof(payloadHeader));

	if (!ndr_write_header(wcontext, retStream) || !ndr_start_constructed(wcontext, retStream) ||
	    !ndr_write_pickle(wcontext, retStream) ||         /* pickle header */
	    !ndr_write_uint16(wcontext, retStream, callId) || /* callId */
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

static BOOL rdpear_kerb_ComputeTgsChecksum(RDPEAR_PLUGIN* rdpear, NdrContext* rcontext, wStream* s,
                                           UINT32* pstatus, KERB_ASN1_DATA* resp)
{
	ComputeTgsChecksumReq req = { 0 };
	krb5_checksum checksum = { 0 };
	wStream* asn1Payload = NULL;

	*pstatus = ERROR_INVALID_DATA;
	WLog_DBG(TAG, "-> ComputeTgsChecksum");

	if (!ndr_read_ComputeTgsChecksumReq(rcontext, s, NULL, &req) ||
	    !ndr_treat_deferred_read(rcontext, s))
		goto out;
	// ComputeTgsChecksumReq_dump(WLog_Get(""), WLOG_DEBUG, &req);

	krb5_error_code rv =
	    kerb_do_checksum(rdpear->krbContext, req.Key, KRB5_KEYUSAGE_TGS_REQ_AUTH_CKSUM,
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
	ndr_destroy_ComputeTgsChecksumReq(rcontext, NULL, &req);
	krb5_free_checksum_contents(rdpear->krbContext, &checksum);
	Stream_Free(asn1Payload, FALSE);
	return TRUE;
}

static BOOL rdpear_kerb_BuildEncryptedAuthData(RDPEAR_PLUGIN* rdpear, NdrContext* rcontext,
                                               wStream* s, UINT32* pstatus, KERB_ASN1_DATA* asn1)
{
	BuildEncryptedAuthDataReq req = { 0 };
	krb5_data encrypted = { 0 };
	wStream* asn1Payload = NULL;
	krb5_error_code rv = 0;

	*pstatus = ERROR_INVALID_DATA;
	WLog_DBG(TAG, "-> BuildEncryptedAuthData");

	if (!ndr_read_BuildEncryptedAuthDataReq(rcontext, s, NULL, &req) ||
	    !ndr_treat_deferred_read(rcontext, s))
		goto out;

	rv = kerb_do_encrypt(rdpear->krbContext, req.Key, (krb5_keyusage)req.KeyUsage,
	                     req.PlainAuthData, &encrypted);
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
	krb5_free_data_contents(rdpear->krbContext, &encrypted);
	ndr_destroy_BuildEncryptedAuthDataReq(rcontext, NULL, &req);
	Stream_Free(asn1Payload, FALSE);
	return TRUE;
}

static char* KERB_RPC_UNICODESTR_to_charptr(const RPC_UNICODE_STRING* src)
{
	WINPR_ASSERT(src);
	return ConvertWCharNToUtf8Alloc(src->Buffer, src->strLength, NULL);
}

static BOOL extractAuthData(const KERB_ASN1_DATA* src, krb5_authdata* authData, BOOL* haveData)
{
	WinPrAsn1Decoder dec = { 0 };
	WinPrAsn1Decoder dec2 = { 0 };
	WinPrAsn1Decoder dec3 = { 0 };
	WinPrAsn1Decoder_InitMem(&dec, WINPR_ASN1_DER, src->Asn1Buffer, src->Asn1BufferHints.count);
	BOOL error = FALSE;
	WinPrAsn1_INTEGER adType = 0;
	WinPrAsn1_OctetString os = { 0 };

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
	WinPrAsn1Decoder dec = { 0 };
	WinPrAsn1Decoder dec2 = { 0 };
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

static BOOL rdpear_kerb_CreateApReqAuthenticator(RDPEAR_PLUGIN* rdpear, NdrContext* rcontext,
                                                 wStream* s, UINT32* pstatus,
                                                 CreateApReqAuthenticatorResp* resp)
{
	krb5_error_code rv = 0;
	wStream* asn1EncodedAuth = NULL;
	CreateApReqAuthenticatorReq req = { 0 };
	krb5_data authenticator = { 0 };
	krb5_data* der = NULL;
	krb5_keyblock* subkey = NULL;
	krb5_principal_data client = { 0 };

	*pstatus = ERROR_INVALID_DATA;
	WLog_DBG(TAG, "-> CreateApReqAuthenticator");

	if (!ndr_read_CreateApReqAuthenticatorReq(rcontext, s, NULL, &req) ||
	    !ndr_treat_deferred_read(rcontext, s))
		goto out;

	krb5_authdata authdata = { 0 };
	krb5_authdata* authDataPtr[2] = { &authdata, NULL };
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
		rv = RPC_ENCRYPTION_KEY_to_keyblock(rdpear->krbContext, req.SubKey, &subkey);
		if (rv)
		{
			WLog_ERR(TAG, "error importing subkey");
			goto out;
		}
	}

	krb5_authenticator authent = { .checksum = NULL,
		                           .subkey = NULL,
		                           .seq_number = req.SequenceNumber,
		                           .authorization_data = haveData ? authDataPtr : NULL };

	client.type = req.ClientName->NameType;
	if (req.ClientName->nameHints.count > INT32_MAX)
		goto out;

	client.length = (krb5_int32)req.ClientName->nameHints.count;
	client.data = calloc(req.ClientName->nameHints.count, sizeof(krb5_data));
	if (!client.data)
		goto out;

	for (int i = 0; i < client.length; i++)
	{
		client.data[i].data = KERB_RPC_UNICODESTR_to_charptr(&req.ClientName->Names[i]);
		if (!client.data[i].data)
			goto out;
		client.data[i].length = (unsigned int)strnlen(client.data[i].data, UINT32_MAX);
	}
	client.realm.data = KERB_RPC_UNICODESTR_to_charptr(req.ClientRealm);
	if (!client.realm.data)
		goto out;
	client.realm.length = (unsigned int)strnlen(client.realm.data, UINT32_MAX);
	authent.client = &client;

	krb5_checksum checksum;
	krb5_checksum* pchecksum = NULL;
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

	rv = kerb_do_encrypt(rdpear->krbContext, req.EncryptionKey, (krb5_keyusage)req.KeyUsage,
	                     &plain_authent, &authenticator);
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
	resp->Authenticator.Pdu = 6;
	resp->KerbProtocolError = rv;
	krb5_free_principal_contents(rdpear->krbContext, &client);
	krb5_free_data(rdpear->krbContext, der);
	krb5_free_data_contents(rdpear->krbContext, &authenticator);
	if (subkey)
		krb5_free_keyblock(rdpear->krbContext, subkey);
	ndr_destroy_CreateApReqAuthenticatorReq(rcontext, NULL, &req);
	Stream_Free(asn1EncodedAuth, FALSE);
	return TRUE;
}

static BOOL rdpear_findEncryptedData(const KERB_ASN1_DATA* src, int* penctype, krb5_data* data)
{
	WinPrAsn1Decoder dec = { 0 };
	WinPrAsn1Decoder dec2 = { 0 };
	WinPrAsn1Decoder_InitMem(&dec, WINPR_ASN1_DER, src->Asn1Buffer, src->Asn1BufferHints.count);
	BOOL error = FALSE;
	WinPrAsn1_INTEGER encType = 0;
	WinPrAsn1_OctetString os = { 0 };

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

static BOOL rdpear_kerb_UnpackKdcReplyBody(RDPEAR_PLUGIN* rdpear, NdrContext* rcontext, wStream* s,
                                           UINT32* pstatus, UnpackKdcReplyBodyResp* resp)
{
	UnpackKdcReplyBodyReq req = { 0 };

	*pstatus = ERROR_INVALID_DATA;

	if (!ndr_read_UnpackKdcReplyBodyReq(rcontext, s, NULL, &req) ||
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

	krb5_data asn1Data = { 0 };
	int encType = 0;
	if (!rdpear_findEncryptedData(req.EncryptedData, &encType, &asn1Data) || !asn1Data.length)
		goto out;

	resp->KerbProtocolError = kerb_do_decrypt(
	    rdpear->krbContext, req.Key, (krb5_keyusage)req.KeyUsage, &asn1Data, &resp->ReplyBody);
	resp->ReplyBody.Pdu = req.Pdu;

	*pstatus = 0;

out:
	ndr_destroy_UnpackKdcReplyBodyReq(rcontext, NULL, &req);
	return TRUE;
}

static BOOL rdpear_kerb_DecryptApReply(RDPEAR_PLUGIN* rdpear, NdrContext* rcontext, wStream* s,
                                       UINT32* pstatus, KERB_ASN1_DATA* resp)
{
	DecryptApReplyReq req = { 0 };

	*pstatus = ERROR_INVALID_DATA;
	if (!ndr_read_DecryptApReplyReq(rcontext, s, NULL, &req) ||
	    !ndr_treat_deferred_read(rcontext, s))
		goto out;

	WLog_DBG(TAG, "-> DecryptApReply");
	// winpr_HexDump(TAG, WLOG_DEBUG, req.EncryptedReply->Asn1Buffer,
	// req.EncryptedReply->Asn1BufferHints.count);

	krb5_data asn1Data = { 0 };
	int encType = 0;
	if (!rdpear_findEncryptedData(req.EncryptedReply, &encType, &asn1Data) || !asn1Data.length)
		goto out;

	resp->Pdu = 0x31;
	krb5_error_code rv =
	    kerb_do_decrypt(rdpear->krbContext, req.Key, KRB5_KEYUSAGE_AP_REP_ENCPART, &asn1Data, resp);
	if (rv != 0)
	{
		WLog_ERR(TAG, "error decrypting");
		goto out;
	}

	// WLog_DBG(TAG, "response=");
	// winpr_HexDump(TAG, WLOG_DEBUG, resp->Asn1Buffer, resp->Asn1BufferHints.count);
	*pstatus = 0;
out:
	ndr_destroy_DecryptApReplyReq(rcontext, NULL, &req);
	return TRUE;
}

static BOOL rdpear_kerb_PackApReply(RDPEAR_PLUGIN* rdpear, NdrContext* rcontext, wStream* s,
                                    UINT32* pstatus, PackApReplyResp* resp)
{
	PackApReplyReq req = { 0 };
	krb5_data asn1Data = { 0 };
	krb5_data* out = NULL;

	WLog_DBG(TAG, "-> PackApReply");
	*pstatus = ERROR_INVALID_DATA;
	if (!ndr_read_PackApReplyReq(rcontext, s, NULL, &req) || !ndr_treat_deferred_read(rcontext, s))
		goto out;

	krb5_error_code rv = kerb_do_encrypt(rdpear->krbContext, req.SessionKey,
	                                     KRB5_KEYUSAGE_AP_REP_ENCPART, req.ReplyBody, &asn1Data);
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
	krb5_free_data_contents(rdpear->krbContext, &asn1Data);
	ndr_destroy_PackApReplyReq(rcontext, NULL, &req);
	return TRUE;
}

static UINT rdpear_decode_payload(RDPEAR_PLUGIN* rdpear,
                                  IWTSVirtualChannelCallback* pChannelCallback, wStream* s)
{
	UINT ret = ERROR_INVALID_DATA;
	NdrContext* context = NULL;
	NdrContext* wcontext = NULL;
	UINT32 status = 0;

	UINT32 uint32Resp = 0;
	KERB_ASN1_DATA asn1Data = { 0 };
	CreateApReqAuthenticatorResp createApReqAuthenticatorResp = { 0 };
	UnpackKdcReplyBodyResp unpackKdcReplyBodyResp = { 0 };
	PackApReplyResp packApReplyResp = { 0 };

	void* resp = NULL;
	NdrMessageType respDescr = NULL;

	wStream* respStream = Stream_New(NULL, 500);
	if (!respStream)
		goto out;

	Stream_Seek(s, 16); /* skip first 16 bytes */

	wStream commandStream = { 0 };
	UINT16 callId = 0;
	UINT16 callId2 = 0;

	context = ndr_read_header(s);
	if (!context || !ndr_read_constructed(context, s, &commandStream) ||
	    !ndr_read_pickle(context, &commandStream) ||
	    !ndr_read_uint16(context, &commandStream, &callId) ||
	    !ndr_read_uint16(context, &commandStream, &callId2) || (callId != callId2))
		goto out;

	ret = CHANNEL_RC_NOT_OPEN;
	switch (callId)
	{
		case RemoteCallKerbNegotiateVersion:
			resp = &uint32Resp;
			respDescr = ndr_uint32_descr();

			if (rdpear_kerb_version(context, &commandStream, &status, &uint32Resp))
				ret = CHANNEL_RC_OK;
			break;
		case RemoteCallKerbCreateApReqAuthenticator:
			resp = &createApReqAuthenticatorResp;
			respDescr = ndr_CreateApReqAuthenticatorResp_descr();

			if (rdpear_kerb_CreateApReqAuthenticator(rdpear, context, &commandStream, &status,
			                                         &createApReqAuthenticatorResp))
				ret = CHANNEL_RC_OK;
			break;
		case RemoteCallKerbDecryptApReply:
			resp = &asn1Data;
			respDescr = ndr_KERB_ASN1_DATA_descr();

			if (rdpear_kerb_DecryptApReply(rdpear, context, &commandStream, &status, &asn1Data))
				ret = CHANNEL_RC_OK;
			break;
		case RemoteCallKerbComputeTgsChecksum:
			resp = &asn1Data;
			respDescr = ndr_KERB_ASN1_DATA_descr();

			if (rdpear_kerb_ComputeTgsChecksum(rdpear, context, &commandStream, &status, &asn1Data))
				ret = CHANNEL_RC_OK;
			break;
		case RemoteCallKerbBuildEncryptedAuthData:
			resp = &asn1Data;
			respDescr = ndr_KERB_ASN1_DATA_descr();

			if (rdpear_kerb_BuildEncryptedAuthData(rdpear, context, &commandStream, &status,
			                                       &asn1Data))
				ret = CHANNEL_RC_OK;
			break;
		case RemoteCallKerbUnpackKdcReplyBody:
			resp = &unpackKdcReplyBodyResp;
			respDescr = ndr_UnpackKdcReplyBodyResp_descr();

			if (rdpear_kerb_UnpackKdcReplyBody(rdpear, context, &commandStream, &status,
			                                   &unpackKdcReplyBodyResp))
				ret = CHANNEL_RC_OK;
			break;
		case RemoteCallKerbPackApReply:
			resp = &packApReplyResp;
			respDescr = ndr_PackApReplyResp_descr();

			if (rdpear_kerb_PackApReply(rdpear, context, &commandStream, &status, &packApReplyResp))
				ret = CHANNEL_RC_OK;
			break;

		case RemoteCallNtlmNegotiateVersion:
			WLog_ERR(TAG, "don't wanna support NTLM");
			break;

		default:
			WLog_DBG(TAG, "Unhandled callId=0x%x", callId);
			winpr_HexDump(TAG, WLOG_DEBUG, Stream_PointerAs(&commandStream, BYTE),
			              Stream_GetRemainingLength(&commandStream));
			break;
	}

	if (!rdpear_prepare_response(context, callId, status, &wcontext, respStream))
		goto out;

	if (resp && respDescr)
	{
		WINPR_ASSERT(respDescr->writeFn);

		BOOL r = respDescr->writeFn(wcontext, respStream, NULL, resp) &&
		         ndr_treat_deferred_write(wcontext, respStream);

		if (respDescr->destroyFn)
			respDescr->destroyFn(wcontext, NULL, resp);

		if (!r)
		{
			WLog_DBG(TAG, "!writeFn || !ndr_treat_deferred_write");
			goto out;
		}
	}

	if (!ndr_end_constructed(wcontext, respStream) ||
	    !rdpear_send_payload(rdpear, pChannelCallback, RDPEAR_PACKAGE_KERBEROS, respStream))
	{
		WLog_DBG(TAG, "rdpear_send_payload !!!!!!!!");
		goto out;
	}
out:
	if (context)
		ndr_context_destroy(&context);

	if (wcontext)
		ndr_context_destroy(&wcontext);

	if (respStream)
		Stream_Free(respStream, TRUE);
	return ret;
}

static UINT rdpear_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream* s)
{
	GENERIC_CHANNEL_CALLBACK* callback = (GENERIC_CHANNEL_CALLBACK*)pChannelCallback;
	WINPR_ASSERT(callback);
	UINT ret = ERROR_INVALID_DATA;

	// winpr_HexDump(TAG, WLOG_DEBUG, Stream_PointerAs(s, BYTE), Stream_GetRemainingLength(s));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 24))
		return ERROR_INVALID_DATA;

	UINT32 protocolMagic = 0;
	UINT32 Length = 0;
	UINT32 Version = 0;
	Stream_Read_UINT32(s, protocolMagic);
	if (protocolMagic != 0x4EACC3C8)
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, Length);

	Stream_Read_UINT32(s, Version);
	if (Version != 0x00000000)
		return ERROR_INVALID_DATA;

	Stream_Seek(s, 4); /* Reserved (4 bytes) */
	Stream_Seek(s, 8); /* TsPkgContext (8 bytes) */

	if (!Stream_CheckAndLogRequiredLength(TAG, s, Length))
		return ERROR_INVALID_DATA;

	SecBuffer inBuffer = { Length, SECBUFFER_TOKEN, Stream_PointerAs(s, void) };
	SecBuffer decrypted = { 0 };

	RDPEAR_PLUGIN* rdpear = (RDPEAR_PLUGIN*)callback->plugin;
	WINPR_ASSERT(rdpear);
	if (!freerdp_nla_decrypt(rdpear->rdp_context, &inBuffer, &decrypted))
		goto out;

	WinPrAsn1Decoder dec = { 0 };
	WinPrAsn1Decoder dec2 = { 0 };
	wStream decodedStream = { 0 };
	Stream_StaticInit(&decodedStream, decrypted.pvBuffer, decrypted.cbBuffer);
	WinPrAsn1Decoder_Init(&dec, WINPR_ASN1_DER, &decodedStream);

	if (!WinPrAsn1DecReadSequence(&dec, &dec2))
		goto out;

	WinPrAsn1_OctetString packageName = { 0 };
	WinPrAsn1_OctetString payload = { 0 };
	BOOL error = 0;
	if (!WinPrAsn1DecReadContextualOctetString(&dec2, 1, &error, &packageName, FALSE))
		goto out;

	if (!WinPrAsn1DecReadContextualOctetString(&dec2, 2, &error, &payload, FALSE))
		goto out;

	wStream payloadStream = { 0 };
	Stream_StaticInit(&payloadStream, payload.data, payload.len);

	ret = rdpear_decode_payload(rdpear, pChannelCallback, &payloadStream);
out:
	sspi_SecBufferFree(&decrypted);
	return ret;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpear_on_open(IWTSVirtualChannelCallback* pChannelCallback)
{
	WINPR_UNUSED(pChannelCallback);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpear_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	WINPR_UNUSED(pChannelCallback);
	return CHANNEL_RC_OK;
}

static void terminate_plugin_cb(GENERIC_DYNVC_PLUGIN* base)
{
	WINPR_ASSERT(base);

	RDPEAR_PLUGIN* rdpear = (RDPEAR_PLUGIN*)base;
	krb5_free_context(rdpear->krbContext);
}

static UINT init_plugin_cb(GENERIC_DYNVC_PLUGIN* base, rdpContext* rcontext, rdpSettings* settings)
{
	WINPR_ASSERT(base);
	WINPR_UNUSED(settings);

	RDPEAR_PLUGIN* rdpear = (RDPEAR_PLUGIN*)base;
	rdpear->rdp_context = rcontext;
	if (krb5_init_context(&rdpear->krbContext))
		return CHANNEL_RC_INITIALIZATION_ERROR;
	return CHANNEL_RC_OK;
}

static const IWTSVirtualChannelCallback rdpear_callbacks = { rdpear_on_data_received,
	                                                         rdpear_on_open, rdpear_on_close,
	                                                         NULL };

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
FREERDP_ENTRY_POINT(UINT rdpear_DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints))
{
	return freerdp_generic_DVCPluginEntry(pEntryPoints, TAG, RDPEAR_DVC_CHANNEL_NAME,
	                                      sizeof(RDPEAR_PLUGIN), sizeof(GENERIC_CHANNEL_CALLBACK),
	                                      &rdpear_callbacks, init_plugin_cb, terminate_plugin_cb);
}
