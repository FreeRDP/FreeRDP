/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#include <winpr/ndr.h>
#include <winpr/print.h>

#include <rdpear-common/rdpear_common.h>
#include <rdpear-common/rdpear_asn1.h>

#include <stddef.h>
#include <freerdp/channels/log.h>

#define TAG CHANNELS_TAG("rdpear")

static char kerberosPackageName[] = {
	'K', 0, 'e', 0, 'r', 0, 'b', 0, 'e', 0, 'r', 0, 'o', 0, 's', 0
};
static char ntlmPackageName[] = { 'N', 0, 'T', 0, 'L', 0, 'M', 0 };

RdpEarPackageType rdpear_packageType_from_name(const WinPrAsn1_OctetString* package)
{
	if (package->len == sizeof(kerberosPackageName) &&
	    memcmp(package->data, kerberosPackageName, package->len) == 0)
		return RDPEAR_PACKAGE_KERBEROS;

	if (package->len == sizeof(ntlmPackageName) &&
	    memcmp(package->data, ntlmPackageName, package->len) == 0)
		return RDPEAR_PACKAGE_NTLM;

	return RDPEAR_PACKAGE_UNKNOWN;
}

wStream* rdpear_encodePayload(RdpEarPackageType packageType, wStream* payload)
{
	wStream* ret = nullptr;
	WinPrAsn1Encoder* enc = WinPrAsn1Encoder_New(WINPR_ASN1_DER);
	if (!enc)
		return nullptr;

	/* TSRemoteGuardInnerPacket ::= SEQUENCE { */
	if (!WinPrAsn1EncSeqContainer(enc))
		goto out;

	/* packageName [1] OCTET STRING */
	WinPrAsn1_OctetString packageOctetString;
	switch (packageType)
	{
		case RDPEAR_PACKAGE_KERBEROS:
			packageOctetString.data = (BYTE*)kerberosPackageName;
			packageOctetString.len = sizeof(kerberosPackageName);
			break;
		case RDPEAR_PACKAGE_NTLM:
			packageOctetString.data = (BYTE*)ntlmPackageName;
			packageOctetString.len = sizeof(ntlmPackageName);
			break;
		default:
			goto out;
	}

	if (!WinPrAsn1EncContextualOctetString(enc, 1, &packageOctetString))
		goto out;

	/* buffer [2] OCTET STRING*/
	WinPrAsn1_OctetString payloadOctetString = { Stream_GetPosition(payload),
		                                         Stream_Buffer(payload) };
	if (!WinPrAsn1EncContextualOctetString(enc, 2, &payloadOctetString))
		goto out;

	/* } */
	if (!WinPrAsn1EncEndContainer(enc))
		goto out;

	ret = Stream_New(nullptr, 100);
	if (!ret)
		goto out;

	if (!WinPrAsn1EncToStream(enc, ret))
	{
		Stream_Free(ret, TRUE);
		ret = nullptr;
		goto out;
	}
out:
	WinPrAsn1Encoder_Free(&enc);
	return ret;
}

#define RDPEAR_SIMPLE_MESSAGE_TYPE(V)                                                             \
	FREERDP_LOCAL BOOL ndr_decoder_read_##V(WinPrNdrDecoder* context, wStream* s,                 \
	                                        const void* hints, V* obj)                            \
	{                                                                                             \
		WINPR_UNUSED(hints);                                                                      \
		return winpr_ndr_decoder_read_fromDescr(context, s, &V##_struct, obj);                    \
	}                                                                                             \
	FREERDP_LOCAL BOOL ndr_encoder_write_##V(WinPrNdrEncoder* context, wStream* s,                \
	                                         const void* hints, const V* obj)                     \
	{                                                                                             \
		WINPR_UNUSED(hints);                                                                      \
		return winpr_ndr_encoder_write_fromDescr(context, s, &V##_struct, obj);                   \
	}                                                                                             \
	FREERDP_LOCAL void ndr_destroy_##V(const void* hints, V* obj)                                 \
	{                                                                                             \
		WINPR_UNUSED(hints);                                                                      \
		winpr_ndr_struct_destroy(&V##_struct, obj);                                               \
	}                                                                                             \
	FREERDP_LOCAL void ndr_dump_##V(wLog* logger, UINT32 lvl, size_t indentLevel, const V* obj)   \
	{                                                                                             \
		winpr_ndr_struct_dump_fromDescr(logger, lvl, indentLevel, &V##_struct, obj);              \
	}                                                                                             \
                                                                                                  \
	static BOOL ndr_descr_read_##V(WinPrNdrDecoder* context, wStream* s, const void* hints,       \
	                               void* obj)                                                     \
	{                                                                                             \
		return ndr_decoder_read_##V(context, s, hints, obj);                                      \
	}                                                                                             \
	static BOOL ndr_descr_write_##V(WinPrNdrEncoder* context, wStream* s, const void* hints,      \
	                                const void* obj)                                              \
	{                                                                                             \
		return ndr_encoder_write_##V(context, s, hints, obj);                                     \
	}                                                                                             \
	static void ndr_descr_destroy_##V(const void* hints, void* obj)                               \
	{                                                                                             \
		WINPR_UNUSED(hints);                                                                      \
		winpr_ndr_struct_destroy(&V##_struct, obj);                                               \
	}                                                                                             \
	static void ndr_descr_dump_##V(wLog* logger, UINT32 lvl, size_t indentLevel, const void* obj) \
	{                                                                                             \
		winpr_ndr_struct_dump_fromDescr(logger, lvl, indentLevel, &V##_struct, obj);              \
	}                                                                                             \
                                                                                                  \
	static WinPrNdrMessageDescr ndr_##V##_descr_s = {                                             \
		WINPR_NDR_ARITY_SIMPLE, sizeof(V),          ndr_descr_read_##V, ndr_descr_write_##V,      \
		ndr_descr_destroy_##V,  ndr_descr_dump_##V,                                               \
	};                                                                                            \
                                                                                                  \
	WinPrNdrMessageType ndr_##V##_descr(void)                                                     \
	{                                                                                             \
		return &ndr_##V##_descr_s;                                                                \
	}

static const WinPrNdrFieldStruct KERB_RPC_OCTET_STRING_fields[] = {
	{ "Length", offsetof(KERB_RPC_OCTET_STRING, length), WINPR_NDR_NOT_POINTER, -1,
	  &winpr_ndr_uint32_descr_s, 0, 0 },
	{ "value", offsetof(KERB_RPC_OCTET_STRING, value), WINPR_NDR_POINTER_NON_NULL, 0,
	  &winpr_ndr_uint8Array_descr_s, 0, 0 }
};
static const WinPrNdrStructDescr KERB_RPC_OCTET_STRING_struct = { "KERB_RPC_OCTET_STRING", 2,
	                                                              KERB_RPC_OCTET_STRING_fields, 0 };

RDPEAR_SIMPLE_MESSAGE_TYPE(KERB_RPC_OCTET_STRING)

/* ============================= KERB_ASN1_DATA ============================== */

static const WinPrNdrFieldStruct KERB_ASN1_DATA_fields[] = {
	{ "Pdu", offsetof(KERB_ASN1_DATA, Pdu), WINPR_NDR_NOT_POINTER, -1, &winpr_ndr_uint32_descr_s, 0,
	  0 },
	{ "Count", offsetof(KERB_ASN1_DATA, Asn1BufferHints.count), WINPR_NDR_NOT_POINTER, -1,
	  &winpr_ndr_uint32_descr_s, 0, 0 },
	{ "Asn1Buffer", offsetof(KERB_ASN1_DATA, Asn1Buffer), WINPR_NDR_POINTER_NON_NULL, 1,
	  &winpr_ndr_uint8Array_descr_s, 0, 0 }
};
static const WinPrNdrStructDescr KERB_ASN1_DATA_struct = { "KERB_ASN1_DATA",
	                                                       ARRAYSIZE(KERB_ASN1_DATA_fields),
	                                                       KERB_ASN1_DATA_fields, 0 };

RDPEAR_SIMPLE_MESSAGE_TYPE(KERB_ASN1_DATA)

/* ============================ RPC_UNICODE_STRING ========================== */

BOOL ndr_decoder_read_RPC_UNICODE_STRING(WinPrNdrDecoder* context, wStream* s, const void* hints,
                                         RPC_UNICODE_STRING* res)
{
	WINPR_ASSERT(res);
	WinPrNdrDeferredEntry bufferDesc = { WINPR_NDR_PTR_NULL,
		                                 "RPC_UNICODE_STRING.Buffer",
		                                 &res->lenHints,
		                                 (void*)&res->Buffer,
		                                 TRUE,
		                                 winpr_ndr_uint16VaryingArray_descr() };
	UINT16 Length = 0;
	UINT16 MaximumLength = 0;

	WINPR_UNUSED(hints);
	/* the structure is aligned on its Buffer pointer */
	if (!winpr_ndr_decoder_read_align(context, s, 4) ||
	    !winpr_ndr_decoder_read_uint16(context, s, &Length) ||
	    !winpr_ndr_decoder_read_uint16(context, s, &MaximumLength) ||
	    !winpr_ndr_decoder_read_refpointer(context, s, &bufferDesc.ptrId) || Length > MaximumLength)
		return FALSE;

	res->lenHints.length = Length;
	res->lenHints.maxLength = MaximumLength;
	res->strLength = Length / 2;

	return winpr_ndr_decoder_push_deferreds(context, &bufferDesc, 1);
}

static BOOL ndr_decoder_read_RPC_UNICODE_STRING_(WinPrNdrDecoder* context, wStream* s,
                                                 const void* hints, void* res)
{
	return ndr_decoder_read_RPC_UNICODE_STRING(context, s, hints, res);
}

BOOL ndr_encoder_write_RPC_UNICODE_STRING(WinPrNdrEncoder* context, wStream* s,
                                          WINPR_ATTR_UNUSED const void* hints,
                                          const RPC_UNICODE_STRING* res)
{
	WINPR_ASSERT(res);
	if (!winpr_ndr_encoder_write_uint32(context, s, res->lenHints.length))
		return FALSE;

	if (!winpr_ndr_encoder_write_uint32(context, s, res->lenHints.maxLength))
		return FALSE;

	return winpr_ndr_encoder_write_data(context, s, res->Buffer, res->strLength);
}

static BOOL ndr_encoder_write_RPC_UNICODE_STRING_(WinPrNdrEncoder* context, wStream* s,
                                                  const void* hints, const void* pvres)
{
	const RPC_UNICODE_STRING* res = pvres;
	return ndr_encoder_write_RPC_UNICODE_STRING(context, s, hints, res);
}

void ndr_dump_RPC_UNICODE_STRING(wLog* logger, UINT32 lvl, size_t indentLevel,
                                 const RPC_UNICODE_STRING* obj)
{
	WINPR_UNUSED(indentLevel);
	WLog_Print(logger, lvl, "\tLength=%" PRIu32 " MaximumLength=%" PRIu32, obj->lenHints.length,
	           obj->lenHints.maxLength);
	winpr_HexLogDump(logger, lvl, obj->Buffer, obj->lenHints.length);
}

static void ndr_dump_RPC_UNICODE_STRING_(wLog* logger, UINT32 lvl, size_t indentLevel,
                                         const void* obj)
{
	ndr_dump_RPC_UNICODE_STRING(logger, lvl, indentLevel, obj);
}

void ndr_destroy_RPC_UNICODE_STRING(const void* hints, RPC_UNICODE_STRING* obj)
{
	WINPR_UNUSED(hints);
	if (!obj)
		return;
	winpr_ndr_pointer_release(obj->Buffer);
	obj->Buffer = nullptr;
}

static void ndr_descr_destroy_RPC_UNICODE_STRING_(const void* hints, void* obj)
{
	ndr_destroy_RPC_UNICODE_STRING(hints, obj);
}

static const WinPrNdrMessageDescr RPC_UNICODE_STRING_descr_s = {
	WINPR_NDR_ARITY_SIMPLE,
	sizeof(RPC_UNICODE_STRING),
	ndr_decoder_read_RPC_UNICODE_STRING_,
	ndr_encoder_write_RPC_UNICODE_STRING_,
	ndr_descr_destroy_RPC_UNICODE_STRING_,
	ndr_dump_RPC_UNICODE_STRING_
};

WinPrNdrMessageType ndr_RPC_UNICODE_STRING_descr(void)
{
	return &RPC_UNICODE_STRING_descr_s;
}

/* ========================= RPC_UNICODE_STRING_Array ======================== */

static BOOL ndr_decoder_read_RPC_UNICODE_STRING_Array(WinPrNdrDecoder* context, wStream* s,
                                                      const void* hints, void* v)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(hints);
	return winpr_ndr_decoder_read_uconformant_array(context, s, hints,
	                                                ndr_RPC_UNICODE_STRING_descr(), v);
}

static BOOL ndr_encoder_write_RPC_UNICODE_STRING_Array(WinPrNdrEncoder* context, wStream* s,
                                                       const void* ghints, const void* v)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(ghints);

	const WinPrNdrArrayHints* hints = (const WinPrNdrArrayHints*)ghints;

	return winpr_ndr_encoder_write_uconformant_array(context, s, hints->count,
	                                                 ndr_RPC_UNICODE_STRING_descr(), v);
}

static const WinPrNdrMessageDescr RPC_UNICODE_STRING_Array_descr_s = {
	WINPR_NDR_ARITY_ARRAYOF,
	sizeof(RPC_UNICODE_STRING),
	ndr_decoder_read_RPC_UNICODE_STRING_Array,
	ndr_encoder_write_RPC_UNICODE_STRING_Array,
	nullptr,
	nullptr
};

static WinPrNdrMessageType ndr_RPC_UNICODE_STRING_Array_descr(void)
{
	return &RPC_UNICODE_STRING_Array_descr_s;
}

/* ========================== KERB_RPC_INTERNAL_NAME ======================== */

BOOL ndr_decoder_read_KERB_RPC_INTERNAL_NAME(WinPrNdrDecoder* context, wStream* s,
                                             const void* hints, KERB_RPC_INTERNAL_NAME* res)
{
	WINPR_ASSERT(res);

	union
	{
		RPC_UNICODE_STRING** ppstr;
		void* pv;
	} cnv;
	cnv.ppstr = &res->Names;
	WinPrNdrDeferredEntry names = { WINPR_NDR_PTR_NULL,
		                            "KERB_RPC_INTERNAL_NAME.Names",
		                            &res->nameHints,
		                            cnv.pv,
		                            TRUE,
		                            ndr_RPC_UNICODE_STRING_Array_descr() };

	UINT16 nameCount = 0;
	WINPR_UNUSED(hints);

	if (!winpr_ndr_decoder_read_uint16(context, s, &res->NameType) ||
	    !winpr_ndr_decoder_read_uint16(context, s, &nameCount))
		return FALSE;

	res->nameHints.count = nameCount;

	return winpr_ndr_decoder_read_refpointer(context, s, &names.ptrId) &&
	       winpr_ndr_decoder_push_deferreds(context, &names, 1);
}

static BOOL ndr_descr_read_KERB_RPC_INTERNAL_NAME(WinPrNdrDecoder* context, wStream* s,
                                                  const void* hints, void* res)
{
	return ndr_decoder_read_KERB_RPC_INTERNAL_NAME(context, s, hints, res);
}

BOOL ndr_encoder_write_KERB_RPC_INTERNAL_NAME(WinPrNdrEncoder* context, wStream* s,
                                              const void* hints, const KERB_RPC_INTERNAL_NAME* res)
{
	WINPR_UNUSED(context);
	WINPR_UNUSED(s);
	WINPR_UNUSED(hints);
	WINPR_UNUSED(res);
	WLog_ERR(TAG, "TODO: implement this");
	return FALSE;
}

void ndr_dump_KERB_RPC_INTERNAL_NAME(wLog* logger, UINT32 lvl, size_t indentLevel,
                                     const KERB_RPC_INTERNAL_NAME* obj)
{
	WINPR_UNUSED(indentLevel);
	WINPR_UNUSED(obj);
	WLog_Print(logger, lvl, "TODO: implement this");
}

static void ndr_descr_dump_KERB_RPC_INTERNAL_NAME(wLog* logger, UINT32 lvl, size_t indentLevel,
                                                  const void* obj)
{
	ndr_dump_KERB_RPC_INTERNAL_NAME(logger, lvl, indentLevel, obj);
}

void ndr_destroy_KERB_RPC_INTERNAL_NAME(const void* hints, KERB_RPC_INTERNAL_NAME* obj)
{
	WINPR_UNUSED(hints);
	if (!obj)
		return;

	if (obj->Names)
	{
		for (UINT32 i = 0; i < obj->nameHints.count; i++)
			ndr_destroy_RPC_UNICODE_STRING(nullptr, &obj->Names[i]);
	}

	winpr_ndr_pointer_release(obj->Names);
	obj->Names = nullptr;
	obj->nameHints.count = 0;
}

static void ndr_descr_destroy_KERB_RPC_INTERNAL_NAME(const void* hints, void* obj)
{
	ndr_destroy_KERB_RPC_INTERNAL_NAME(hints, obj);
}

static WinPrNdrMessageDescr KERB_RPC_INTERNAL_NAME_descr_s = {
	WINPR_NDR_ARITY_SIMPLE,
	sizeof(KERB_RPC_INTERNAL_NAME),
	ndr_descr_read_KERB_RPC_INTERNAL_NAME,
	nullptr,
	ndr_descr_destroy_KERB_RPC_INTERNAL_NAME,
	ndr_descr_dump_KERB_RPC_INTERNAL_NAME
};

WinPrNdrMessageType ndr_KERB_RPC_INTERNAL_NAME_descr(void)
{
	return &KERB_RPC_INTERNAL_NAME_descr_s;
}

/* ========================== KERB_RPC_ENCRYPTION_KEY ======================== */

static const WinPrNdrFieldStruct KERB_RPC_ENCRYPTION_KEY_fields[] = {
	{ "reserved1", offsetof(KERB_RPC_ENCRYPTION_KEY, reserved1), WINPR_NDR_NOT_POINTER, -1,
	  &winpr_ndr_uint32_descr_s, 0, 0 },
	{ "reserved2", offsetof(KERB_RPC_ENCRYPTION_KEY, reserved2), WINPR_NDR_NOT_POINTER, -1,
	  &winpr_ndr_uint32_descr_s, 0, 0 },
	{ "reserved3", offsetof(KERB_RPC_ENCRYPTION_KEY, reserved3), WINPR_NDR_NOT_POINTER, -1,
	  &ndr_KERB_RPC_OCTET_STRING_descr_s, 0, 0 }
};
static const WinPrNdrStructDescr KERB_RPC_ENCRYPTION_KEY_struct = {
	"KERB_RPC_ENCRYPTION_KEY", ARRAYSIZE(KERB_RPC_ENCRYPTION_KEY_fields),
	KERB_RPC_ENCRYPTION_KEY_fields, 0
};

RDPEAR_SIMPLE_MESSAGE_TYPE(KERB_RPC_ENCRYPTION_KEY)

/* ========================== BuildEncryptedAuthDataReq ======================== */

static const WinPrNdrFieldStruct BuildEncryptedAuthDataReq_fields[] = {
	{ "KeyUsage", offsetof(BuildEncryptedAuthDataReq, KeyUsage), WINPR_NDR_NOT_POINTER, -1,
	  &winpr_ndr_uint32_descr_s, 0, 0 },
	{ "key", offsetof(BuildEncryptedAuthDataReq, Key), WINPR_NDR_POINTER_NON_NULL, -1,
	  &ndr_KERB_RPC_ENCRYPTION_KEY_descr_s, 0, 0 },
	{ "plainAuthData", offsetof(BuildEncryptedAuthDataReq, PlainAuthData),
	  WINPR_NDR_POINTER_NON_NULL, -1, &ndr_KERB_ASN1_DATA_descr_s, 0, 0 }
};
static const WinPrNdrStructDescr BuildEncryptedAuthDataReq_struct = {
	"BuildEncryptedAuthDataReq", ARRAYSIZE(BuildEncryptedAuthDataReq_fields),
	BuildEncryptedAuthDataReq_fields, 0
};

RDPEAR_SIMPLE_MESSAGE_TYPE(BuildEncryptedAuthDataReq)

/* ========================== ComputeTgsChecksumReq ======================== */

static const WinPrNdrFieldStruct ComputeTgsChecksumReq_fields[] = {
	{ "requestBody", offsetof(ComputeTgsChecksumReq, requestBody), WINPR_NDR_POINTER_NON_NULL, -1,
	  &ndr_KERB_ASN1_DATA_descr_s, 0, 0 },
	{ "key", offsetof(ComputeTgsChecksumReq, Key), WINPR_NDR_POINTER_NON_NULL, -1,
	  &ndr_KERB_RPC_ENCRYPTION_KEY_descr_s, 0, 0 },
	{ "ChecksumType", offsetof(ComputeTgsChecksumReq, ChecksumType), WINPR_NDR_NOT_POINTER, -1,
	  &winpr_ndr_uint32_descr_s, 0, 0 }
};
static const WinPrNdrStructDescr ComputeTgsChecksumReq_struct = {
	"ComputeTgsChecksumReq", ARRAYSIZE(ComputeTgsChecksumReq_fields), ComputeTgsChecksumReq_fields,
	0
};

RDPEAR_SIMPLE_MESSAGE_TYPE(ComputeTgsChecksumReq)

/* ========================== CreateApReqAuthenticatorReq ======================== */

static const WinPrNdrFieldStruct CreateApReqAuthenticatorReq_fields[] = {
	{ "EncryptionKey", offsetof(CreateApReqAuthenticatorReq, EncryptionKey),
	  WINPR_NDR_POINTER_NON_NULL, -1, &ndr_KERB_RPC_ENCRYPTION_KEY_descr_s, 0, 0 },
	{ "SequenceNumber", offsetof(CreateApReqAuthenticatorReq, SequenceNumber),
	  WINPR_NDR_NOT_POINTER, -1, &winpr_ndr_uint32_descr_s, 0, 0 },
	{ "ClientName", offsetof(CreateApReqAuthenticatorReq, ClientName), WINPR_NDR_POINTER_NON_NULL,
	  -1, &KERB_RPC_INTERNAL_NAME_descr_s, 0, 0 },
	{ "ClientRealm", offsetof(CreateApReqAuthenticatorReq, ClientRealm), WINPR_NDR_POINTER_NON_NULL,
	  -1, &RPC_UNICODE_STRING_descr_s, 0, 0 },
	{ "SkewTime", offsetof(CreateApReqAuthenticatorReq, SkewTime), WINPR_NDR_POINTER_NON_NULL, -1,
	  &winpr_ndr_uint64_descr_s, 0, 0 },
	{ "SubKey", offsetof(CreateApReqAuthenticatorReq, SubKey), WINPR_NDR_POINTER, -1,
	  &ndr_KERB_RPC_ENCRYPTION_KEY_descr_s, 0, 0 },
	{ "AuthData", offsetof(CreateApReqAuthenticatorReq, AuthData), WINPR_NDR_POINTER_NON_NULL, -1,
	  &ndr_KERB_ASN1_DATA_descr_s, 0, 0 },
	{ "GssChecksum", offsetof(CreateApReqAuthenticatorReq, GssChecksum), WINPR_NDR_POINTER, -1,
	  &ndr_KERB_ASN1_DATA_descr_s, 0, 0 },
	{ "KeyUsage", offsetof(CreateApReqAuthenticatorReq, KeyUsage), WINPR_NDR_NOT_POINTER, -1,
	  &winpr_ndr_uint32_descr_s, 0, 0 },
};
static const WinPrNdrStructDescr CreateApReqAuthenticatorReq_struct = {
	"CreateApReqAuthenticatorReq", ARRAYSIZE(CreateApReqAuthenticatorReq_fields),
	CreateApReqAuthenticatorReq_fields, 0
};

RDPEAR_SIMPLE_MESSAGE_TYPE(CreateApReqAuthenticatorReq)

/* ========================== CreateApReqAuthenticatorResp ======================== */

static const WinPrNdrFieldStruct CreateApReqAuthenticatorResp_fields[] = {
	{ "AuthenticatorTime", offsetof(CreateApReqAuthenticatorResp, AuthenticatorTime),
	  WINPR_NDR_NOT_POINTER, -1, &winpr_ndr_uint64_descr_s, 0, 0 },
	{ "Authenticator", offsetof(CreateApReqAuthenticatorResp, Authenticator), WINPR_NDR_NOT_POINTER,
	  -1, &ndr_KERB_ASN1_DATA_descr_s, 0, 0 },
	{ "KerbProtocolError", offsetof(CreateApReqAuthenticatorResp, KerbProtocolError),
	  WINPR_NDR_NOT_POINTER, -1, &winpr_ndr_uint32_descr_s, 0, 0 },
};

static const WinPrNdrStructDescr CreateApReqAuthenticatorResp_struct = {
	"CreateApReqAuthenticatorResp", ARRAYSIZE(CreateApReqAuthenticatorResp_fields),
	CreateApReqAuthenticatorResp_fields, 0
};

RDPEAR_SIMPLE_MESSAGE_TYPE(CreateApReqAuthenticatorResp)

/* ========================== UnpackKdcReplyBodyReq ======================== */

static const WinPrNdrFieldStruct UnpackKdcReplyBodyReq_fields[] = {
	{ "EncryptedData", offsetof(UnpackKdcReplyBodyReq, EncryptedData), WINPR_NDR_POINTER_NON_NULL,
	  -1, &ndr_KERB_ASN1_DATA_descr_s, 0, 0 },
	{ "Key", offsetof(UnpackKdcReplyBodyReq, Key), WINPR_NDR_POINTER_NON_NULL, -1,
	  &ndr_KERB_RPC_ENCRYPTION_KEY_descr_s, 0, 0 },
	{ "StrenghtenKey", offsetof(UnpackKdcReplyBodyReq, StrengthenKey), WINPR_NDR_POINTER, -1,
	  &ndr_KERB_RPC_ENCRYPTION_KEY_descr_s, 0, 0 },
	{ "Pdu", offsetof(UnpackKdcReplyBodyReq, Pdu), WINPR_NDR_NOT_POINTER, -1,
	  &winpr_ndr_uint32_descr_s, 0, 0 },
	{ "KeyUsage", offsetof(UnpackKdcReplyBodyReq, KeyUsage), WINPR_NDR_NOT_POINTER, -1,
	  &winpr_ndr_uint32_descr_s, 0, 0 },
};

static const WinPrNdrStructDescr UnpackKdcReplyBodyReq_struct = {
	"UnpackKdcReplyBodyReq", ARRAYSIZE(UnpackKdcReplyBodyReq_fields), UnpackKdcReplyBodyReq_fields,
	0
};

RDPEAR_SIMPLE_MESSAGE_TYPE(UnpackKdcReplyBodyReq)

/* ========================== UnpackKdcReplyBodyResp ======================== */

static const WinPrNdrFieldStruct UnpackKdcReplyBodyResp_fields[] = {
	{ "KerbProtocolError", offsetof(UnpackKdcReplyBodyResp, KerbProtocolError),
	  WINPR_NDR_NOT_POINTER, -1, &winpr_ndr_uint32_descr_s, 0, 0 },
	{ "ReplyBody", offsetof(UnpackKdcReplyBodyResp, ReplyBody), WINPR_NDR_NOT_POINTER, -1,
	  &ndr_KERB_ASN1_DATA_descr_s, 0, 0 }
};

static const WinPrNdrStructDescr UnpackKdcReplyBodyResp_struct = {
	"UnpackKdcReplyBodyResp", ARRAYSIZE(UnpackKdcReplyBodyResp_fields),
	UnpackKdcReplyBodyResp_fields, 0
};

RDPEAR_SIMPLE_MESSAGE_TYPE(UnpackKdcReplyBodyResp)

/* ========================== DecryptApReplyReq ======================== */

static const WinPrNdrFieldStruct DecryptApReplyReq_fields[] = {
	{ "EncryptedReply", offsetof(DecryptApReplyReq, EncryptedReply), WINPR_NDR_POINTER_NON_NULL, -1,
	  &ndr_KERB_ASN1_DATA_descr_s, 0, 0 },
	{ "Key", offsetof(DecryptApReplyReq, Key), WINPR_NDR_POINTER_NON_NULL, -1,
	  &ndr_KERB_RPC_ENCRYPTION_KEY_descr_s, 0, 0 }
};

static const WinPrNdrStructDescr DecryptApReplyReq_struct = { "DecryptApReplyReq",
	                                                          ARRAYSIZE(DecryptApReplyReq_fields),
	                                                          DecryptApReplyReq_fields, 0 };

RDPEAR_SIMPLE_MESSAGE_TYPE(DecryptApReplyReq)

/* ========================== PackApReplyReq ======================== */

static const WinPrNdrFieldStruct PackApReplyReq_fields[] = {
	{ "Reply", offsetof(PackApReplyReq, Reply), WINPR_NDR_POINTER_NON_NULL, -1,
	  &ndr_KERB_ASN1_DATA_descr_s, 0, 0 },
	{ "ReplyBody", offsetof(PackApReplyReq, ReplyBody), WINPR_NDR_POINTER_NON_NULL, -1,
	  &ndr_KERB_ASN1_DATA_descr_s, 0, 0 },
	{ "SessionKey", offsetof(PackApReplyReq, SessionKey), WINPR_NDR_POINTER_NON_NULL, -1,
	  &ndr_KERB_RPC_ENCRYPTION_KEY_descr_s, 0, 0 }
};

static const WinPrNdrStructDescr PackApReplyReq_struct = { "PackApReplyReq",
	                                                       ARRAYSIZE(PackApReplyReq_fields),
	                                                       PackApReplyReq_fields, 0 };

RDPEAR_SIMPLE_MESSAGE_TYPE(PackApReplyReq)

/* ========================== PackApReplyResp ======================== */

static const WinPrNdrFieldStruct PackApReplyResp_fields[] = {
	{ "PackedReplySize", offsetof(PackApReplyResp, PackedReplyHints), WINPR_NDR_NOT_POINTER, -1,
	  &winpr_ndr_uint32_descr_s, 0, 0 },
	{ "PackedReply", offsetof(PackApReplyResp, PackedReply), WINPR_NDR_POINTER_NON_NULL, 0,
	  &winpr_ndr_uint8Array_descr_s, 0, 0 },
};

static const WinPrNdrStructDescr PackApReplyResp_struct = { "PackApReplyResp",
	                                                        ARRAYSIZE(PackApReplyResp_fields),
	                                                        PackApReplyResp_fields, 0 };

RDPEAR_SIMPLE_MESSAGE_TYPE(PackApReplyResp)

/* ========================== messages ======================== */

/** @brief reads a type serialized message: the object holding the top-level pointer on it */
static BOOL rdpear_read_message(WinPrNdrDecoder* decoder, wStream* s,
                                const WinPrNdrStructDescr* descr, void* msg)
{
	WINPR_ASSERT(decoder);
	WINPR_ASSERT(s);

	wStream object = WINPR_C_ARRAY_INIT;
	winpr_ndr_refid refId = WINPR_NDR_PTR_NULL;
	if (!winpr_ndr_decoder_read_object(decoder, s, &object) ||
	    !winpr_ndr_decoder_read_refpointer(decoder, &object, &refId))
		return FALSE;

	if (!refId)
	{
		WLog_ERR(TAG, "%s: null top-level pointer", descr->name);
		return FALSE;
	}

	return winpr_ndr_decoder_read_fromDescr(decoder, &object, descr, msg) &&
	       winpr_ndr_decoder_treat_deferreds(decoder, &object);
}

/** @brief writes a type serialized message: the header and the object holding the top-level
 * pointer on it */
static BOOL rdpear_write_message(WinPrNdrEncoder* encoder, wStream* s,
                                 const WinPrNdrStructDescr* descr, const void* msg)
{
	WINPR_ASSERT(encoder);
	WINPR_ASSERT(s);

	return winpr_ndr_encoder_write_header(encoder, s) &&
	       winpr_ndr_encoder_start_object(encoder, s) &&
	       winpr_ndr_encoder_write_toplevel_refid(encoder, s) &&
	       winpr_ndr_encoder_write_fromDescr(encoder, s, descr, msg) &&
	       winpr_ndr_encoder_treat_deferreds(encoder, s) &&
	       winpr_ndr_encoder_end_object(encoder, s);
}

#define RDPEAR_ARM(CALLID, NAME, DESCR) \
	{ CALLID, { #NAME, 0, WINPR_NDR_NOT_POINTER, -1, DESCR, 0, 0 } }

/* the calls without a description are encoded without their arm */
static const WinPrNdrUnionArm rdpear_default_arm = {
	0, { "default", 0, WINPR_NDR_NOT_POINTER, -1, nullptr, 0, 0 }
};

/* ========================== KerbCredIsoRemoteInput ======================== */

static const WinPrNdrUnionArm KerbCredIsoRemoteInputUnion_arms[] = {
	RDPEAR_ARM(RemoteCallKerbNegotiateVersion, NegotiateVersion, &winpr_ndr_uint32_descr_s),
	RDPEAR_ARM(RemoteCallKerbCreateApReqAuthenticator, CreateApReqAuthenticator,
	           &ndr_CreateApReqAuthenticatorReq_descr_s),
	RDPEAR_ARM(RemoteCallKerbDecryptApReply, DecryptApReply, &ndr_DecryptApReplyReq_descr_s),
	RDPEAR_ARM(RemoteCallKerbUnpackKdcReplyBody, UnpackKdcReplyBody,
	           &ndr_UnpackKdcReplyBodyReq_descr_s),
	RDPEAR_ARM(RemoteCallKerbComputeTgsChecksum, ComputeTgsChecksum,
	           &ndr_ComputeTgsChecksumReq_descr_s),
	RDPEAR_ARM(RemoteCallKerbBuildEncryptedAuthData, BuildEncryptedAuthData,
	           &ndr_BuildEncryptedAuthDataReq_descr_s),
	RDPEAR_ARM(RemoteCallKerbPackApReply, PackApReply, &ndr_PackApReplyReq_descr_s),
};

static const WinPrNdrUnionDescr KerbCredIsoRemoteInputUnion_union = {
	"KerbCredIsoRemoteInputUnion", ARRAYSIZE(KerbCredIsoRemoteInputUnion_arms),
	KerbCredIsoRemoteInputUnion_arms, sizeof(UINT16), &rdpear_default_arm
};

WINPR_NDR_UNION_MESSAGE_DEFINE(KerbCredIsoRemoteInputUnion, KerbCredIsoRemoteInputUnion);

static const WinPrNdrFieldStruct KerbCredIsoRemoteInput_fields[] = {
	{ "CallId", offsetof(KerbCredIsoRemoteInput, CallId), WINPR_NDR_NOT_POINTER, -1,
	  &winpr_ndr_uint16_descr_s, 0, 0 },
	{ "Input", offsetof(KerbCredIsoRemoteInput, Input), WINPR_NDR_NOT_POINTER, 0,
	  &KerbCredIsoRemoteInputUnion_descr_s, 0, 0 },
};

/* the largest arm alignment is the one of CreateApReqAuthenticator.AuthenticatorTime */
static const WinPrNdrStructDescr KerbCredIsoRemoteInput_struct = {
	"KerbCredIsoRemoteInput", ARRAYSIZE(KerbCredIsoRemoteInput_fields),
	KerbCredIsoRemoteInput_fields, 8
};

BOOL rdpear_kerb_input_read(WinPrNdrDecoder* decoder, wStream* s, KerbCredIsoRemoteInput* input)
{
	WINPR_ASSERT(input);
	return rdpear_read_message(decoder, s, &KerbCredIsoRemoteInput_struct, input);
}

void rdpear_kerb_input_free(KerbCredIsoRemoteInput* input)
{
	if (input)
		winpr_ndr_struct_destroy(&KerbCredIsoRemoteInput_struct, input);
}

/* ========================== KerbCredIsoRemoteOutput ======================== */

static const WinPrNdrUnionArm KerbCredIsoRemoteOutputUnion_arms[] = {
	RDPEAR_ARM(RemoteCallKerbNegotiateVersion, NegotiateVersion, &winpr_ndr_uint32_descr_s),
	RDPEAR_ARM(RemoteCallKerbCreateApReqAuthenticator, CreateApReqAuthenticator,
	           &ndr_CreateApReqAuthenticatorResp_descr_s),
	RDPEAR_ARM(RemoteCallKerbDecryptApReply, DecryptApReply, &ndr_KERB_ASN1_DATA_descr_s),
	RDPEAR_ARM(RemoteCallKerbUnpackKdcReplyBody, UnpackKdcReplyBody,
	           &ndr_UnpackKdcReplyBodyResp_descr_s),
	RDPEAR_ARM(RemoteCallKerbComputeTgsChecksum, ComputeTgsChecksum, &ndr_KERB_ASN1_DATA_descr_s),
	RDPEAR_ARM(RemoteCallKerbBuildEncryptedAuthData, BuildEncryptedAuthData,
	           &ndr_KERB_ASN1_DATA_descr_s),
	RDPEAR_ARM(RemoteCallKerbPackApReply, PackApReply, &ndr_PackApReplyResp_descr_s),
};

static const WinPrNdrUnionDescr KerbCredIsoRemoteOutputUnion_union = {
	"KerbCredIsoRemoteOutputUnion", ARRAYSIZE(KerbCredIsoRemoteOutputUnion_arms),
	KerbCredIsoRemoteOutputUnion_arms, sizeof(UINT16), &rdpear_default_arm
};

WINPR_NDR_UNION_MESSAGE_DEFINE(KerbCredIsoRemoteOutputUnion, KerbCredIsoRemoteOutputUnion);

static const WinPrNdrFieldStruct KerbCredIsoRemoteOutput_fields[] = {
	{ "CallId", offsetof(KerbCredIsoRemoteOutput, CallId), WINPR_NDR_NOT_POINTER, -1,
	  &winpr_ndr_uint16_descr_s, 0, 0 },
	{ "Status", offsetof(KerbCredIsoRemoteOutput, Status), WINPR_NDR_NOT_POINTER, -1,
	  &winpr_ndr_uint32_descr_s, 0, 0 },
	{ "Output", offsetof(KerbCredIsoRemoteOutput, Output), WINPR_NDR_NOT_POINTER, 0,
	  &KerbCredIsoRemoteOutputUnion_descr_s, 0, 0 },
};

static const WinPrNdrStructDescr KerbCredIsoRemoteOutput_struct = {
	"KerbCredIsoRemoteOutput", ARRAYSIZE(KerbCredIsoRemoteOutput_fields),
	KerbCredIsoRemoteOutput_fields, 8
};

BOOL rdpear_kerb_output_write(WinPrNdrEncoder* encoder, wStream* s,
                              const KerbCredIsoRemoteOutput* output)
{
	WINPR_ASSERT(output);
	return rdpear_write_message(encoder, s, &KerbCredIsoRemoteOutput_struct, output);
}

void rdpear_kerb_output_free(KerbCredIsoRemoteOutput* output)
{
	if (output)
		winpr_ndr_struct_destroy(&KerbCredIsoRemoteOutput_struct, output);
}

/* ========================== NTLM types ======================== */

#define RDPEAR_BYTES_FIELD(TYPE, NAME, SIZE) \
	{ #NAME, offsetof(TYPE, NAME), WINPR_NDR_NOT_POINTER, -1, &winpr_ndr_uint8_descr_s, SIZE, 0 }

/** @brief a USHORT count of a conformant array stored in a WinPrNdrArrayHints */
static BOOL ndr_decoder_read_uint16_count(WinPrNdrDecoder* context, wStream* s, const void* hints,
                                          void* v)
{
	WINPR_UNUSED(hints);
	UINT16 count = 0;
	if (!winpr_ndr_decoder_read_uint16(context, s, &count))
		return FALSE;
	((WinPrNdrArrayHints*)v)->count = count;
	return TRUE;
}

static BOOL ndr_encoder_write_uint16_count(WinPrNdrEncoder* context, wStream* s, const void* hints,
                                           const void* v)
{
	WINPR_UNUSED(hints);
	const UINT32 count = ((const WinPrNdrArrayHints*)v)->count;
	if (count > UINT16_MAX)
	{
		WLog_ERR(TAG, "count %" PRIu32 " doesn't fit in a USHORT", count);
		return FALSE;
	}
	return winpr_ndr_encoder_write_uint16(context, s, (UINT16)count);
}

static const WinPrNdrMessageDescr ndr_uint16_count_descr_s = { WINPR_NDR_ARITY_SIMPLE,
	                                                           sizeof(WinPrNdrArrayHints),
	                                                           ndr_decoder_read_uint16_count,
	                                                           ndr_encoder_write_uint16_count,
	                                                           nullptr,
	                                                           nullptr };

static const WinPrNdrFieldStruct MSV1_0_REMOTE_ENCRYPTED_SECRETS_fields[] = {
	{ "reserved1", offsetof(MSV1_0_REMOTE_ENCRYPTED_SECRETS, reserved1), WINPR_NDR_NOT_POINTER, -1,
	  &winpr_ndr_uint8_descr_s, 0, 0 },
	{ "reserved2", offsetof(MSV1_0_REMOTE_ENCRYPTED_SECRETS, reserved2), WINPR_NDR_NOT_POINTER, -1,
	  &winpr_ndr_uint8_descr_s, 0, 0 },
	{ "reserved3", offsetof(MSV1_0_REMOTE_ENCRYPTED_SECRETS, reserved3), WINPR_NDR_NOT_POINTER, -1,
	  &winpr_ndr_uint8_descr_s, 0, 0 },
	{ "reserved4", offsetof(MSV1_0_REMOTE_ENCRYPTED_SECRETS, reserved4), WINPR_NDR_NOT_POINTER, -1,
	  &winpr_ndr_uint16_descr_s, 0, 0 },
	RDPEAR_BYTES_FIELD(MSV1_0_REMOTE_ENCRYPTED_SECRETS, reserved5,
	                   RDPEAR_MSV1_0_CREDENTIAL_KEY_LENGTH),
	{ "reservedSize", offsetof(MSV1_0_REMOTE_ENCRYPTED_SECRETS, reservedSize),
	  WINPR_NDR_NOT_POINTER, -1, &winpr_ndr_uint32_descr_s, 0, 0 },
	{ "reserved6", offsetof(MSV1_0_REMOTE_ENCRYPTED_SECRETS, reserved6), WINPR_NDR_POINTER, 5,
	  &winpr_ndr_uint8Array_descr_s, 0, 0 },
};

static const WinPrNdrStructDescr MSV1_0_REMOTE_ENCRYPTED_SECRETS_struct = {
	"MSV1_0_REMOTE_ENCRYPTED_SECRETS", ARRAYSIZE(MSV1_0_REMOTE_ENCRYPTED_SECRETS_fields),
	MSV1_0_REMOTE_ENCRYPTED_SECRETS_fields, 4
};

WINPR_NDR_STRUCT_MESSAGE_DEFINE(MSV1_0_REMOTE_ENCRYPTED_SECRETS, MSV1_0_REMOTE_ENCRYPTED_SECRETS);

static const WinPrNdrFieldStruct NT_CHALLENGE_fields[] = {
	RDPEAR_BYTES_FIELD(NT_CHALLENGE, Data, RDPEAR_MSV1_0_CHALLENGE_LENGTH),
};

static const WinPrNdrStructDescr NT_CHALLENGE_struct = { "NT_CHALLENGE",
	                                                     ARRAYSIZE(NT_CHALLENGE_fields),
	                                                     NT_CHALLENGE_fields, 1 };

WINPR_NDR_STRUCT_MESSAGE_DEFINE(NT_CHALLENGE, NT_CHALLENGE);

static const WinPrNdrFieldStruct NT_RESPONSE_fields[] = {
	RDPEAR_BYTES_FIELD(NT_RESPONSE, Data, RDPEAR_MSV1_0_RESPONSE_LENGTH),
};

static const WinPrNdrStructDescr NT_RESPONSE_struct = { "NT_RESPONSE",
	                                                    ARRAYSIZE(NT_RESPONSE_fields),
	                                                    NT_RESPONSE_fields, 1 };

WINPR_NDR_STRUCT_MESSAGE_DEFINE(NT_RESPONSE, NT_RESPONSE);

static const WinPrNdrFieldStruct USER_SESSION_KEY_fields[] = {
	RDPEAR_BYTES_FIELD(USER_SESSION_KEY, Data, RDPEAR_MSV1_0_USER_SESSION_KEY_LENGTH),
};

static const WinPrNdrStructDescr USER_SESSION_KEY_struct = { "USER_SESSION_KEY",
	                                                         ARRAYSIZE(USER_SESSION_KEY_fields),
	                                                         USER_SESSION_KEY_fields, 1 };

WINPR_NDR_STRUCT_MESSAGE_DEFINE(USER_SESSION_KEY, USER_SESSION_KEY);

/* ========================== NTLM calls ======================== */

static const WinPrNdrFieldStruct Lm20GetNtlm3ChallengeResponseReq_fields[] = {
	{ "Credential", offsetof(Lm20GetNtlm3ChallengeResponseReq, Credential), WINPR_NDR_POINTER, -1,
	  &MSV1_0_REMOTE_ENCRYPTED_SECRETS_descr_s, 0, 0 },
	{ "UserName", offsetof(Lm20GetNtlm3ChallengeResponseReq, UserName), WINPR_NDR_POINTER, -1,
	  &RPC_UNICODE_STRING_descr_s, 0, 0 },
	{ "LogonDomainName", offsetof(Lm20GetNtlm3ChallengeResponseReq, LogonDomainName),
	  WINPR_NDR_POINTER, -1, &RPC_UNICODE_STRING_descr_s, 0, 0 },
	{ "ServerName", offsetof(Lm20GetNtlm3ChallengeResponseReq, ServerName), WINPR_NDR_POINTER, -1,
	  &RPC_UNICODE_STRING_descr_s, 0, 0 },
	RDPEAR_BYTES_FIELD(Lm20GetNtlm3ChallengeResponseReq, ChallengeToClient,
	                   RDPEAR_MSV1_0_CHALLENGE_LENGTH),
};

static const WinPrNdrStructDescr Lm20GetNtlm3ChallengeResponseReq_struct = {
	"Lm20GetNtlm3ChallengeResponseReq", ARRAYSIZE(Lm20GetNtlm3ChallengeResponseReq_fields),
	Lm20GetNtlm3ChallengeResponseReq_fields, 4
};

WINPR_NDR_STRUCT_MESSAGE_DEFINE(Lm20GetNtlm3ChallengeResponseReq, Lm20GetNtlm3ChallengeResponseReq);

static const WinPrNdrFieldStruct Lm20GetNtlm3ChallengeResponseResp_fields[] = {
	{ "Ntlm3ResponseLength", offsetof(Lm20GetNtlm3ChallengeResponseResp, Ntlm3ResponseLength),
	  WINPR_NDR_NOT_POINTER, -1, &ndr_uint16_count_descr_s, 0, 0 },
	{ "Ntlm3Response", offsetof(Lm20GetNtlm3ChallengeResponseResp, Ntlm3Response),
	  WINPR_NDR_POINTER, 0, &winpr_ndr_uint8Array_descr_s, 0, 0 },
	RDPEAR_BYTES_FIELD(Lm20GetNtlm3ChallengeResponseResp, Lm3Response.Response,
	                   RDPEAR_MSV1_0_NTLM3_RESPONSE_LENGTH),
	RDPEAR_BYTES_FIELD(Lm20GetNtlm3ChallengeResponseResp, Lm3Response.ChallengeFromClient,
	                   RDPEAR_MSV1_0_CHALLENGE_LENGTH),
	RDPEAR_BYTES_FIELD(Lm20GetNtlm3ChallengeResponseResp, UserSessionKey.Data,
	                   RDPEAR_MSV1_0_USER_SESSION_KEY_LENGTH),
	RDPEAR_BYTES_FIELD(Lm20GetNtlm3ChallengeResponseResp, LmSessionKey.Data,
	                   RDPEAR_MSV1_0_CHALLENGE_LENGTH),
};

static const WinPrNdrStructDescr Lm20GetNtlm3ChallengeResponseResp_struct = {
	"Lm20GetNtlm3ChallengeResponseResp", ARRAYSIZE(Lm20GetNtlm3ChallengeResponseResp_fields),
	Lm20GetNtlm3ChallengeResponseResp_fields, 4
};

WINPR_NDR_STRUCT_MESSAGE_DEFINE(Lm20GetNtlm3ChallengeResponseResp,
                                Lm20GetNtlm3ChallengeResponseResp);

static const WinPrNdrFieldStruct CalculateNtResponseReq_fields[] = {
	{ "NtChallenge", offsetof(CalculateNtResponseReq, NtChallenge), WINPR_NDR_POINTER, -1,
	  &NT_CHALLENGE_descr_s, 0, 0 },
	{ "Credential", offsetof(CalculateNtResponseReq, Credential), WINPR_NDR_POINTER, -1,
	  &MSV1_0_REMOTE_ENCRYPTED_SECRETS_descr_s, 0, 0 },
};

static const WinPrNdrStructDescr CalculateNtResponseReq_struct = {
	"CalculateNtResponseReq", ARRAYSIZE(CalculateNtResponseReq_fields),
	CalculateNtResponseReq_fields, 4
};

WINPR_NDR_STRUCT_MESSAGE_DEFINE(CalculateNtResponseReq, CalculateNtResponseReq);

static const WinPrNdrFieldStruct CalculateUserSessionKeyNtReq_fields[] = {
	{ "NtResponse", offsetof(CalculateUserSessionKeyNtReq, NtResponse), WINPR_NDR_POINTER, -1,
	  &NT_RESPONSE_descr_s, 0, 0 },
	{ "Credential", offsetof(CalculateUserSessionKeyNtReq, Credential), WINPR_NDR_POINTER, -1,
	  &MSV1_0_REMOTE_ENCRYPTED_SECRETS_descr_s, 0, 0 },
};

static const WinPrNdrStructDescr CalculateUserSessionKeyNtReq_struct = {
	"CalculateUserSessionKeyNtReq", ARRAYSIZE(CalculateUserSessionKeyNtReq_fields),
	CalculateUserSessionKeyNtReq_fields, 4
};

WINPR_NDR_STRUCT_MESSAGE_DEFINE(CalculateUserSessionKeyNtReq, CalculateUserSessionKeyNtReq);

static const WinPrNdrFieldStruct CompareCredentialsReq_fields[] = {
	{ "LhsCredential", offsetof(CompareCredentialsReq, LhsCredential), WINPR_NDR_POINTER, -1,
	  &MSV1_0_REMOTE_ENCRYPTED_SECRETS_descr_s, 0, 0 },
	{ "RhsCredential", offsetof(CompareCredentialsReq, RhsCredential), WINPR_NDR_POINTER, -1,
	  &MSV1_0_REMOTE_ENCRYPTED_SECRETS_descr_s, 0, 0 },
};

static const WinPrNdrStructDescr CompareCredentialsReq_struct = {
	"CompareCredentialsReq", ARRAYSIZE(CompareCredentialsReq_fields), CompareCredentialsReq_fields,
	4
};

WINPR_NDR_STRUCT_MESSAGE_DEFINE(CompareCredentialsReq, CompareCredentialsReq);

static const WinPrNdrFieldStruct CompareCredentialsResp_fields[] = {
	{ "AreNtOwfsEqual", offsetof(CompareCredentialsResp, AreNtOwfsEqual), WINPR_NDR_NOT_POINTER, -1,
	  &winpr_ndr_uint32_descr_s, 0, 0 },
	{ "AreLmOwfsEqual", offsetof(CompareCredentialsResp, AreLmOwfsEqual), WINPR_NDR_NOT_POINTER, -1,
	  &winpr_ndr_uint32_descr_s, 0, 0 },
	{ "AreShaOwfsEqual", offsetof(CompareCredentialsResp, AreShaOwfsEqual), WINPR_NDR_NOT_POINTER,
	  -1, &winpr_ndr_uint32_descr_s, 0, 0 },
};

static const WinPrNdrStructDescr CompareCredentialsResp_struct = {
	"CompareCredentialsResp", ARRAYSIZE(CompareCredentialsResp_fields),
	CompareCredentialsResp_fields, 4
};

WINPR_NDR_STRUCT_MESSAGE_DEFINE(CompareCredentialsResp, CompareCredentialsResp);

/* ========================== NtlmCredIsoRemoteInput ======================== */

static const WinPrNdrUnionArm NtlmCredIsoRemoteInputUnion_arms[] = {
	RDPEAR_ARM(RemoteCallNtlmNegotiateVersion, NegotiateVersion, &winpr_ndr_uint32_descr_s),
	RDPEAR_ARM(RemoteCallNtlmLm20GetNtlm3ChallengeResponse, Lm20GetNtlm3ChallengeResponse,
	           &Lm20GetNtlm3ChallengeResponseReq_descr_s),
	RDPEAR_ARM(RemoteCallNtlmCalculateNtResponse, CalculateNtResponse,
	           &CalculateNtResponseReq_descr_s),
	RDPEAR_ARM(RemoteCallNtlmCalculateUserSessionKeyNt, CalculateUserSessionKeyNt,
	           &CalculateUserSessionKeyNtReq_descr_s),
	RDPEAR_ARM(RemoteCallNtlmCompareCredentials, CompareCredentials,
	           &CompareCredentialsReq_descr_s),
};

static const WinPrNdrUnionDescr NtlmCredIsoRemoteInputUnion_union = {
	"NtlmCredIsoRemoteInputUnion", ARRAYSIZE(NtlmCredIsoRemoteInputUnion_arms),
	NtlmCredIsoRemoteInputUnion_arms, sizeof(UINT16), &rdpear_default_arm
};

WINPR_NDR_UNION_MESSAGE_DEFINE(NtlmCredIsoRemoteInputUnion, NtlmCredIsoRemoteInputUnion);

static const WinPrNdrFieldStruct NtlmCredIsoRemoteInput_fields[] = {
	{ "CallId", offsetof(NtlmCredIsoRemoteInput, CallId), WINPR_NDR_NOT_POINTER, -1,
	  &winpr_ndr_uint16_descr_s, 0, 0 },
	{ "Input", offsetof(NtlmCredIsoRemoteInput, Input), WINPR_NDR_NOT_POINTER, 0,
	  &NtlmCredIsoRemoteInputUnion_descr_s, 0, 0 },
};

static const WinPrNdrStructDescr NtlmCredIsoRemoteInput_struct = {
	"NtlmCredIsoRemoteInput", ARRAYSIZE(NtlmCredIsoRemoteInput_fields),
	NtlmCredIsoRemoteInput_fields, 4
};

BOOL rdpear_ntlm_input_read(WinPrNdrDecoder* decoder, wStream* s, NtlmCredIsoRemoteInput* input)
{
	WINPR_ASSERT(input);
	return rdpear_read_message(decoder, s, &NtlmCredIsoRemoteInput_struct, input);
}

void rdpear_ntlm_input_free(NtlmCredIsoRemoteInput* input)
{
	if (input)
		winpr_ndr_struct_destroy(&NtlmCredIsoRemoteInput_struct, input);
}

/* ========================== NtlmCredIsoRemoteOutput ======================== */

static const WinPrNdrUnionArm NtlmCredIsoRemoteOutputUnion_arms[] = {
	RDPEAR_ARM(RemoteCallNtlmNegotiateVersion, NegotiateVersion, &winpr_ndr_uint32_descr_s),
	RDPEAR_ARM(RemoteCallNtlmLm20GetNtlm3ChallengeResponse, Lm20GetNtlm3ChallengeResponse,
	           &Lm20GetNtlm3ChallengeResponseResp_descr_s),
	RDPEAR_ARM(RemoteCallNtlmCalculateNtResponse, CalculateNtResponse, &NT_RESPONSE_descr_s),
	RDPEAR_ARM(RemoteCallNtlmCalculateUserSessionKeyNt, CalculateUserSessionKeyNt,
	           &USER_SESSION_KEY_descr_s),
	RDPEAR_ARM(RemoteCallNtlmCompareCredentials, CompareCredentials,
	           &CompareCredentialsResp_descr_s),
};

static const WinPrNdrUnionDescr NtlmCredIsoRemoteOutputUnion_union = {
	"NtlmCredIsoRemoteOutputUnion", ARRAYSIZE(NtlmCredIsoRemoteOutputUnion_arms),
	NtlmCredIsoRemoteOutputUnion_arms, sizeof(UINT16), &rdpear_default_arm
};

WINPR_NDR_UNION_MESSAGE_DEFINE(NtlmCredIsoRemoteOutputUnion, NtlmCredIsoRemoteOutputUnion);

static const WinPrNdrFieldStruct NtlmCredIsoRemoteOutput_fields[] = {
	{ "CallId", offsetof(NtlmCredIsoRemoteOutput, CallId), WINPR_NDR_NOT_POINTER, -1,
	  &winpr_ndr_uint16_descr_s, 0, 0 },
	{ "Status", offsetof(NtlmCredIsoRemoteOutput, Status), WINPR_NDR_NOT_POINTER, -1,
	  &winpr_ndr_uint32_descr_s, 0, 0 },
	{ "Output", offsetof(NtlmCredIsoRemoteOutput, Output), WINPR_NDR_NOT_POINTER, 0,
	  &NtlmCredIsoRemoteOutputUnion_descr_s, 0, 0 },
};

static const WinPrNdrStructDescr NtlmCredIsoRemoteOutput_struct = {
	"NtlmCredIsoRemoteOutput", ARRAYSIZE(NtlmCredIsoRemoteOutput_fields),
	NtlmCredIsoRemoteOutput_fields, 4
};

BOOL rdpear_ntlm_output_write(WinPrNdrEncoder* encoder, wStream* s,
                              const NtlmCredIsoRemoteOutput* output)
{
	WINPR_ASSERT(output);
	return rdpear_write_message(encoder, s, &NtlmCredIsoRemoteOutput_struct, output);
}

void rdpear_ntlm_output_free(NtlmCredIsoRemoteOutput* output)
{
	if (output)
		winpr_ndr_struct_destroy(&NtlmCredIsoRemoteOutput_struct, output);
}
