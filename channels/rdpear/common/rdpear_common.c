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

#include <rdpear-common/rdpear_common.h>
#include <rdpear-common/rdpear_asn1.h>
#include <rdpear-common/ndr.h>

#include <stddef.h>
#include <winpr/print.h>
#include <freerdp/channels/log.h>

#define TAG CHANNELS_TAG("rdpear")

static char kerberosPackageName[] = {
	'K', 0, 'e', 0, 'r', 0, 'b', 0, 'e', 0, 'r', 0, 'o', 0, 's', 0
};
static char ntlmPackageName[] = { 'N', 0, 'T', 0, 'L', 0, 'M', 0 };

RdpEarPackageType rdpear_packageType_from_name(WinPrAsn1_OctetString* package)
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
	wStream* ret = NULL;
	WinPrAsn1Encoder* enc = WinPrAsn1Encoder_New(WINPR_ASN1_DER);
	if (!enc)
		return NULL;

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

	ret = Stream_New(NULL, 100);
	if (!ret)
		goto out;

	if (!WinPrAsn1EncToStream(enc, ret))
	{
		Stream_Free(ret, TRUE);
		ret = NULL;
		goto out;
	}
out:
	WinPrAsn1Encoder_Free(&enc);
	return ret;
}

#define RDPEAR_SIMPLE_MESSAGE_TYPE(V)                                                             \
	BOOL ndr_read_##V(NdrContext* context, wStream* s, const void* hints, V* obj)                 \
	{                                                                                             \
		WINPR_UNUSED(hints);                                                                      \
		return ndr_struct_read_fromDescr(context, s, &V##_struct, obj);                           \
	}                                                                                             \
	BOOL ndr_write_##V(NdrContext* context, wStream* s, const void* hints, const V* obj)          \
	{                                                                                             \
		WINPR_UNUSED(hints);                                                                      \
		return ndr_struct_write_fromDescr(context, s, &V##_struct, obj);                          \
	}                                                                                             \
	void ndr_destroy_##V(NdrContext* context, const void* hints, V* obj)                          \
	{                                                                                             \
		WINPR_UNUSED(hints);                                                                      \
		ndr_struct_destroy(context, &V##_struct, obj);                                            \
	}                                                                                             \
	void ndr_dump_##V(wLog* logger, UINT32 lvl, size_t indentLevel, const V* obj)                 \
	{                                                                                             \
		ndr_struct_dump_fromDescr(logger, lvl, indentLevel, &V##_struct, obj);                    \
	}                                                                                             \
                                                                                                  \
	static BOOL ndr_descr_read_##V(NdrContext* context, wStream* s, const void* hints, void* obj) \
	{                                                                                             \
		WINPR_UNUSED(hints);                                                                      \
		return ndr_struct_read_fromDescr(context, s, &V##_struct, obj);                           \
	}                                                                                             \
	static BOOL ndr_descr_write_##V(NdrContext* context, wStream* s, const void* hints,           \
	                                const void* obj)                                              \
	{                                                                                             \
		WINPR_UNUSED(hints);                                                                      \
		return ndr_struct_write_fromDescr(context, s, &V##_struct, obj);                          \
	}                                                                                             \
	static void ndr_descr_destroy_##V(NdrContext* context, const void* hints, void* obj)          \
	{                                                                                             \
		WINPR_UNUSED(hints);                                                                      \
		ndr_struct_destroy(context, &V##_struct, obj);                                            \
	}                                                                                             \
	static void ndr_descr_dump_##V(wLog* logger, UINT32 lvl, size_t indentLevel, const void* obj) \
	{                                                                                             \
		ndr_struct_dump_fromDescr(logger, lvl, indentLevel, &V##_struct, obj);                    \
	}                                                                                             \
                                                                                                  \
	static NdrMessageDescr ndr_##V##_descr_s = {                                                  \
		NDR_ARITY_SIMPLE,      sizeof(V),          ndr_descr_read_##V, ndr_descr_write_##V,       \
		ndr_descr_destroy_##V, ndr_descr_dump_##V,                                                \
	};                                                                                            \
                                                                                                  \
	NdrMessageType ndr_##V##_descr(void)                                                          \
	{                                                                                             \
		return &ndr_##V##_descr_s;                                                                \
	}

static const NdrFieldStruct KERB_RPC_OCTET_STRING_fields[] = {
	{ "Length", offsetof(KERB_RPC_OCTET_STRING, length), NDR_NOT_POINTER, -1, &ndr_uint32_descr_s },
	{ "value", offsetof(KERB_RPC_OCTET_STRING, value), NDR_POINTER_NON_NULL, 0,
	  &ndr_uint8Array_descr_s }
};
static const NdrStructDescr KERB_RPC_OCTET_STRING_struct = { "KERB_RPC_OCTET_STRING", 2,
	                                                         KERB_RPC_OCTET_STRING_fields };

RDPEAR_SIMPLE_MESSAGE_TYPE(KERB_RPC_OCTET_STRING)

/* ============================= KERB_ASN1_DATA ============================== */

static const NdrFieldStruct KERB_ASN1_DATA_fields[] = {
	{ "Pdu", offsetof(KERB_ASN1_DATA, Pdu), NDR_NOT_POINTER, -1, &ndr_uint32_descr_s },
	{ "Count", offsetof(KERB_ASN1_DATA, Asn1BufferHints.count), NDR_NOT_POINTER, -1,
	  &ndr_uint32_descr_s },
	{ "Asn1Buffer", offsetof(KERB_ASN1_DATA, Asn1Buffer), NDR_POINTER_NON_NULL, 1,
	  &ndr_uint8Array_descr_s }
};
static const NdrStructDescr KERB_ASN1_DATA_struct = { "KERB_ASN1_DATA",
	                                                  ARRAYSIZE(KERB_ASN1_DATA_fields),
	                                                  KERB_ASN1_DATA_fields };

RDPEAR_SIMPLE_MESSAGE_TYPE(KERB_ASN1_DATA)

/* ============================ RPC_UNICODE_STRING ========================== */

BOOL ndr_read_RPC_UNICODE_STRING(NdrContext* context, wStream* s, const void* hints,
                                 RPC_UNICODE_STRING* res)
{
	NdrDeferredEntry bufferDesc = { NDR_PTR_NULL, "RPC_UNICODE_STRING.Buffer", &res->lenHints,
		                            (void*)&res->Buffer, ndr_uint16VaryingArray_descr() };
	UINT16 Length = 0;
	UINT16 MaximumLength = 0;

	WINPR_UNUSED(hints);
	if (!ndr_read_uint16(context, s, &Length) || !ndr_read_uint16(context, s, &MaximumLength) ||
	    !ndr_read_refpointer(context, s, &bufferDesc.ptrId) || Length > MaximumLength)
		return FALSE;

	res->lenHints.length = Length;
	res->lenHints.maxLength = MaximumLength;
	res->strLength = Length / 2;

	return ndr_push_deferreds(context, &bufferDesc, 1);
}

static BOOL ndr_descr_read_RPC_UNICODE_STRING(NdrContext* context, wStream* s, const void* hints,
                                              void* res)
{
	return ndr_read_RPC_UNICODE_STRING(context, s, hints, res);
}

#if 0
BOOL ndr_write_RPC_UNICODE_STRING(NdrContext* context, wStream* s, const void* hints,
                                  const RPC_UNICODE_STRING* res)
{
	return ndr_write_uint32(context, s, res->lenHints.length) &&
	       ndr_write_uint32(context, s, res->lenHints.maxLength) /*&&
	       ndr_write_BYTE_ptr(context, s, (BYTE*)res->Buffer, res->Length)*/
	    ;
}
#endif

void ndr_dump_RPC_UNICODE_STRING(wLog* logger, UINT32 lvl, size_t indentLevel,
                                 const RPC_UNICODE_STRING* obj)
{
	WINPR_UNUSED(indentLevel);
	WLog_Print(logger, lvl, "\tLength=%d MaximumLength=%d", obj->lenHints.length,
	           obj->lenHints.maxLength);
	winpr_HexLogDump(logger, lvl, obj->Buffer, obj->lenHints.length);
}

static void ndr_descr_dump_RPC_UNICODE_STRING(wLog* logger, UINT32 lvl, size_t indentLevel,
                                              const void* obj)
{
	ndr_dump_RPC_UNICODE_STRING(logger, lvl, indentLevel, obj);
}

void ndr_destroy_RPC_UNICODE_STRING(NdrContext* context, const void* hints, RPC_UNICODE_STRING* obj)
{
	WINPR_UNUSED(context);
	WINPR_UNUSED(hints);
	if (!obj)
		return;
	free(obj->Buffer);
	obj->Buffer = NULL;
}

static void ndr_descr_destroy_RPC_UNICODE_STRING(NdrContext* context, const void* hints, void* obj)
{
	ndr_destroy_RPC_UNICODE_STRING(context, hints, obj);
}

static const NdrMessageDescr RPC_UNICODE_STRING_descr_s = { NDR_ARITY_SIMPLE,
	                                                        sizeof(RPC_UNICODE_STRING),
	                                                        ndr_descr_read_RPC_UNICODE_STRING,
	                                                        /*ndr_write_RPC_UNICODE_STRING*/ NULL,
	                                                        ndr_descr_destroy_RPC_UNICODE_STRING,
	                                                        ndr_descr_dump_RPC_UNICODE_STRING };

NdrMessageType ndr_RPC_UNICODE_STRING_descr(void)
{
	return &RPC_UNICODE_STRING_descr_s;
}

/* ========================= RPC_UNICODE_STRING_Array ======================== */

static BOOL ndr_read_RPC_UNICODE_STRING_Array(NdrContext* context, wStream* s, const void* hints,
                                              void* v)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(hints);
	return ndr_read_uconformant_array(context, s, hints, ndr_RPC_UNICODE_STRING_descr(), v);
}

static BOOL ndr_write_RPC_UNICODE_STRING_Array(NdrContext* context, wStream* s, const void* ghints,
                                               const void* v)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(ghints);

	const NdrArrayHints* hints = (const NdrArrayHints*)ghints;

	return ndr_write_uconformant_array(context, s, hints->count, ndr_RPC_UNICODE_STRING_descr(), v);
}

static const NdrMessageDescr RPC_UNICODE_STRING_Array_descr_s = {
	NDR_ARITY_ARRAYOF,
	sizeof(RPC_UNICODE_STRING),
	ndr_read_RPC_UNICODE_STRING_Array,
	ndr_write_RPC_UNICODE_STRING_Array,
	NULL,
	NULL
};

static NdrMessageType ndr_RPC_UNICODE_STRING_Array_descr(void)
{
	return &RPC_UNICODE_STRING_Array_descr_s;
}

/* ========================== KERB_RPC_INTERNAL_NAME ======================== */

BOOL ndr_read_KERB_RPC_INTERNAL_NAME(NdrContext* context, wStream* s, const void* hints,
                                     KERB_RPC_INTERNAL_NAME* res)
{
	WINPR_ASSERT(res);

	union
	{
		RPC_UNICODE_STRING** ppstr;
		void* pv;
	} cnv;
	cnv.ppstr = &res->Names;
	NdrDeferredEntry names = { NDR_PTR_NULL, "KERB_RPC_INTERNAL_NAME.Names", &res->nameHints,
		                       cnv.pv, ndr_RPC_UNICODE_STRING_Array_descr() };

	UINT16 nameCount = 0;
	WINPR_UNUSED(hints);

	if (!ndr_read_uint16(context, s, &res->NameType) || !ndr_read_uint16(context, s, &nameCount))
		return FALSE;

	res->nameHints.count = nameCount;

	return ndr_read_refpointer(context, s, &names.ptrId) && ndr_push_deferreds(context, &names, 1);
}

static BOOL ndr_descr_read_KERB_RPC_INTERNAL_NAME(NdrContext* context, wStream* s,
                                                  const void* hints, void* res)
{
	return ndr_read_KERB_RPC_INTERNAL_NAME(context, s, hints, res);
}

BOOL ndr_write_KERB_RPC_INTERNAL_NAME(NdrContext* context, wStream* s, const void* hints,
                                      const KERB_RPC_INTERNAL_NAME* res)
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

void ndr_destroy_KERB_RPC_INTERNAL_NAME(NdrContext* context, const void* hints,
                                        KERB_RPC_INTERNAL_NAME* obj)
{
	WINPR_UNUSED(hints);
	if (!obj)
		return;

	for (UINT32 i = 0; i < obj->nameHints.count; i++)
		ndr_destroy_RPC_UNICODE_STRING(context, NULL, &obj->Names[i]);

	free(obj->Names);
	obj->Names = NULL;
}

static void ndr_descr_destroy_KERB_RPC_INTERNAL_NAME(NdrContext* context, const void* hints,
                                                     void* obj)
{
	ndr_destroy_KERB_RPC_INTERNAL_NAME(context, hints, obj);
}

static NdrMessageDescr KERB_RPC_INTERNAL_NAME_descr_s = { NDR_ARITY_SIMPLE,
	                                                      sizeof(KERB_RPC_INTERNAL_NAME),
	                                                      ndr_descr_read_KERB_RPC_INTERNAL_NAME,
	                                                      NULL,
	                                                      ndr_descr_destroy_KERB_RPC_INTERNAL_NAME,
	                                                      ndr_descr_dump_KERB_RPC_INTERNAL_NAME };

NdrMessageType ndr_KERB_RPC_INTERNAL_NAME_descr(void)
{
	return &KERB_RPC_INTERNAL_NAME_descr_s;
}

/* ========================== KERB_RPC_ENCRYPTION_KEY ======================== */

static const NdrFieldStruct KERB_RPC_ENCRYPTION_KEY_fields[] = {
	{ "reserved1", offsetof(KERB_RPC_ENCRYPTION_KEY, reserved1), NDR_NOT_POINTER, -1,
	  &ndr_uint32_descr_s },
	{ "reserved2", offsetof(KERB_RPC_ENCRYPTION_KEY, reserved2), NDR_NOT_POINTER, -1,
	  &ndr_uint32_descr_s },
	{ "reserved3", offsetof(KERB_RPC_ENCRYPTION_KEY, reserved3), NDR_NOT_POINTER, -1,
	  &ndr_KERB_RPC_OCTET_STRING_descr_s }
};
static const NdrStructDescr KERB_RPC_ENCRYPTION_KEY_struct = {
	"KERB_RPC_ENCRYPTION_KEY", ARRAYSIZE(KERB_RPC_ENCRYPTION_KEY_fields),
	KERB_RPC_ENCRYPTION_KEY_fields
};

RDPEAR_SIMPLE_MESSAGE_TYPE(KERB_RPC_ENCRYPTION_KEY)

/* ========================== BuildEncryptedAuthDataReq ======================== */

static const NdrFieldStruct BuildEncryptedAuthDataReq_fields[] = {
	{ "KeyUsage", offsetof(BuildEncryptedAuthDataReq, KeyUsage), NDR_NOT_POINTER, -1,
	  &ndr_uint32_descr_s },
	{ "key", offsetof(BuildEncryptedAuthDataReq, Key), NDR_POINTER_NON_NULL, -1,
	  &ndr_KERB_RPC_ENCRYPTION_KEY_descr_s },
	{ "plainAuthData", offsetof(BuildEncryptedAuthDataReq, PlainAuthData), NDR_POINTER_NON_NULL, -1,
	  &ndr_KERB_ASN1_DATA_descr_s }
};
static const NdrStructDescr BuildEncryptedAuthDataReq_struct = {
	"BuildEncryptedAuthDataReq", ARRAYSIZE(BuildEncryptedAuthDataReq_fields),
	BuildEncryptedAuthDataReq_fields
};

RDPEAR_SIMPLE_MESSAGE_TYPE(BuildEncryptedAuthDataReq)

/* ========================== ComputeTgsChecksumReq ======================== */

static const NdrFieldStruct ComputeTgsChecksumReq_fields[] = {
	{ "requestBody", offsetof(ComputeTgsChecksumReq, requestBody), NDR_POINTER_NON_NULL, -1,
	  &ndr_KERB_ASN1_DATA_descr_s },
	{ "key", offsetof(ComputeTgsChecksumReq, Key), NDR_POINTER_NON_NULL, -1,
	  &ndr_KERB_RPC_ENCRYPTION_KEY_descr_s },
	{ "ChecksumType", offsetof(ComputeTgsChecksumReq, ChecksumType), NDR_NOT_POINTER, -1,
	  &ndr_uint32_descr_s }
};
static const NdrStructDescr ComputeTgsChecksumReq_struct = {
	"ComputeTgsChecksumReq", ARRAYSIZE(ComputeTgsChecksumReq_fields), ComputeTgsChecksumReq_fields
};

RDPEAR_SIMPLE_MESSAGE_TYPE(ComputeTgsChecksumReq)

/* ========================== CreateApReqAuthenticatorReq ======================== */

static const NdrFieldStruct CreateApReqAuthenticatorReq_fields[] = {
	{ "EncryptionKey", offsetof(CreateApReqAuthenticatorReq, EncryptionKey), NDR_POINTER_NON_NULL,
	  -1, &ndr_KERB_RPC_ENCRYPTION_KEY_descr_s },
	{ "SequenceNumber", offsetof(CreateApReqAuthenticatorReq, SequenceNumber), NDR_NOT_POINTER, -1,
	  &ndr_uint32_descr_s },
	{ "ClientName", offsetof(CreateApReqAuthenticatorReq, ClientName), NDR_POINTER_NON_NULL, -1,
	  &KERB_RPC_INTERNAL_NAME_descr_s },
	{ "ClientRealm", offsetof(CreateApReqAuthenticatorReq, ClientRealm), NDR_POINTER_NON_NULL, -1,
	  &RPC_UNICODE_STRING_descr_s },
	{ "SkewTime", offsetof(CreateApReqAuthenticatorReq, SkewTime), NDR_POINTER_NON_NULL, -1,
	  &ndr_uint64_descr_s },
	{ "SubKey", offsetof(CreateApReqAuthenticatorReq, SubKey), NDR_POINTER, -1,
	  &ndr_KERB_RPC_ENCRYPTION_KEY_descr_s },
	{ "AuthData", offsetof(CreateApReqAuthenticatorReq, AuthData), NDR_POINTER_NON_NULL, -1,
	  &ndr_KERB_ASN1_DATA_descr_s },
	{ "GssChecksum", offsetof(CreateApReqAuthenticatorReq, GssChecksum), NDR_POINTER, -1,
	  &ndr_KERB_ASN1_DATA_descr_s },
	{ "KeyUsage", offsetof(CreateApReqAuthenticatorReq, KeyUsage), NDR_NOT_POINTER, -1,
	  &ndr_uint32_descr_s },
};
static const NdrStructDescr CreateApReqAuthenticatorReq_struct = {
	"CreateApReqAuthenticatorReq", ARRAYSIZE(CreateApReqAuthenticatorReq_fields),
	CreateApReqAuthenticatorReq_fields
};

RDPEAR_SIMPLE_MESSAGE_TYPE(CreateApReqAuthenticatorReq)

/* ========================== CreateApReqAuthenticatorResp ======================== */

static const NdrFieldStruct CreateApReqAuthenticatorResp_fields[] = {
	{ "AuthenticatorTime", offsetof(CreateApReqAuthenticatorResp, AuthenticatorTime),
	  NDR_NOT_POINTER, -1, &ndr_uint64_descr_s },
	{ "Authenticator", offsetof(CreateApReqAuthenticatorResp, Authenticator), NDR_NOT_POINTER, -1,
	  &ndr_KERB_ASN1_DATA_descr_s },
	{ "KerbProtocolError", offsetof(CreateApReqAuthenticatorResp, KerbProtocolError),
	  NDR_NOT_POINTER, -1, &ndr_uint32_descr_s },
};

static const NdrStructDescr CreateApReqAuthenticatorResp_struct = {
	"CreateApReqAuthenticatorResp", ARRAYSIZE(CreateApReqAuthenticatorResp_fields),
	CreateApReqAuthenticatorResp_fields
};

RDPEAR_SIMPLE_MESSAGE_TYPE(CreateApReqAuthenticatorResp)

/* ========================== UnpackKdcReplyBodyReq ======================== */

static const NdrFieldStruct UnpackKdcReplyBodyReq_fields[] = {
	{ "EncryptedData", offsetof(UnpackKdcReplyBodyReq, EncryptedData), NDR_POINTER_NON_NULL, -1,
	  &ndr_KERB_ASN1_DATA_descr_s },
	{ "Key", offsetof(UnpackKdcReplyBodyReq, Key), NDR_POINTER_NON_NULL, -1,
	  &ndr_KERB_RPC_ENCRYPTION_KEY_descr_s },
	{ "StrenghtenKey", offsetof(UnpackKdcReplyBodyReq, StrengthenKey), NDR_POINTER, -1,
	  &ndr_KERB_RPC_ENCRYPTION_KEY_descr_s },
	{ "Pdu", offsetof(UnpackKdcReplyBodyReq, Pdu), NDR_NOT_POINTER, -1, &ndr_uint32_descr_s },
	{ "KeyUsage", offsetof(UnpackKdcReplyBodyReq, KeyUsage), NDR_NOT_POINTER, -1,
	  &ndr_uint32_descr_s },
};

static const NdrStructDescr UnpackKdcReplyBodyReq_struct = {
	"UnpackKdcReplyBodyReq", ARRAYSIZE(UnpackKdcReplyBodyReq_fields), UnpackKdcReplyBodyReq_fields
};

RDPEAR_SIMPLE_MESSAGE_TYPE(UnpackKdcReplyBodyReq)

/* ========================== UnpackKdcReplyBodyResp ======================== */

static const NdrFieldStruct UnpackKdcReplyBodyResp_fields[] = {
	{ "KerbProtocolError", offsetof(UnpackKdcReplyBodyResp, KerbProtocolError), NDR_NOT_POINTER, -1,
	  &ndr_uint32_descr_s },
	{ "ReplyBody", offsetof(UnpackKdcReplyBodyResp, ReplyBody), NDR_NOT_POINTER, -1,
	  &ndr_KERB_ASN1_DATA_descr_s }
};

static const NdrStructDescr UnpackKdcReplyBodyResp_struct = {
	"UnpackKdcReplyBodyResp", ARRAYSIZE(UnpackKdcReplyBodyResp_fields),
	UnpackKdcReplyBodyResp_fields
};

RDPEAR_SIMPLE_MESSAGE_TYPE(UnpackKdcReplyBodyResp)

/* ========================== DecryptApReplyReq ======================== */

static const NdrFieldStruct DecryptApReplyReq_fields[] = {
	{ "EncryptedReply", offsetof(DecryptApReplyReq, EncryptedReply), NDR_POINTER_NON_NULL, -1,
	  &ndr_KERB_ASN1_DATA_descr_s },
	{ "Key", offsetof(DecryptApReplyReq, Key), NDR_POINTER_NON_NULL, -1,
	  &ndr_KERB_RPC_ENCRYPTION_KEY_descr_s }
};

static const NdrStructDescr DecryptApReplyReq_struct = { "DecryptApReplyReq",
	                                                     ARRAYSIZE(DecryptApReplyReq_fields),
	                                                     DecryptApReplyReq_fields };

RDPEAR_SIMPLE_MESSAGE_TYPE(DecryptApReplyReq)

/* ========================== PackApReplyReq ======================== */

static const NdrFieldStruct PackApReplyReq_fields[] = {
	{ "Reply", offsetof(PackApReplyReq, Reply), NDR_POINTER_NON_NULL, -1,
	  &ndr_KERB_ASN1_DATA_descr_s },
	{ "ReplyBody", offsetof(PackApReplyReq, ReplyBody), NDR_POINTER_NON_NULL, -1,
	  &ndr_KERB_ASN1_DATA_descr_s },
	{ "SessionKey", offsetof(PackApReplyReq, SessionKey), NDR_POINTER_NON_NULL, -1,
	  &ndr_KERB_RPC_ENCRYPTION_KEY_descr_s }
};

static const NdrStructDescr PackApReplyReq_struct = { "PackApReplyReq",
	                                                  ARRAYSIZE(PackApReplyReq_fields),
	                                                  PackApReplyReq_fields };

RDPEAR_SIMPLE_MESSAGE_TYPE(PackApReplyReq)

/* ========================== PackApReplyResp ======================== */

static const NdrFieldStruct PackApReplyResp_fields[] = {
	{ "PackedReplySize", offsetof(PackApReplyResp, PackedReplyHints), NDR_NOT_POINTER, -1,
	  &ndr_uint32_descr_s },
	{ "PackedReply", offsetof(PackApReplyResp, PackedReply), NDR_POINTER_NON_NULL, 0,
	  &ndr_uint8Array_descr_s },
};

static const NdrStructDescr PackApReplyResp_struct = { "PackApReplyResp",
	                                                   ARRAYSIZE(PackApReplyResp_fields),
	                                                   PackApReplyResp_fields };

RDPEAR_SIMPLE_MESSAGE_TYPE(PackApReplyResp)
