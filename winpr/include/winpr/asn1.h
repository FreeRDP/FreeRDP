/**
 * WinPR: Windows Portable Runtime
 * ASN1 encoder / decoder
 *
 * Copyright 2022 David Fort <contact@hardening-consulting.com>
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

#ifndef WINPR_ASN1_H_
#define WINPR_ASN1_H_

#include <winpr/winpr.h>
#include <winpr/wtypes.h>
#include <winpr/stream.h>

#define ER_TAG_MASK 0x1F

enum
{
	ER_TAG_BOOLEAN = 0x01,
	ER_TAG_INTEGER = 0x02,
	ER_TAG_BIT_STRING = 0x03,
	ER_TAG_OCTET_STRING = 0x04,
	ER_TAG_NULL = 0x05,
	ER_TAG_OBJECT_IDENTIFIER = 0x06,
	ER_TAG_ENUMERATED = 0x0A,
	ER_TAG_UTF8STRING = 0x0C,
	ER_TAG_PRINTABLE_STRING = 0x13,
	ER_TAG_IA5STRING = 0x16,
	ER_TAG_UTCTIME = 0x17,
	ER_TAG_GENERAL_STRING = 0x1B,
	ER_TAG_GENERALIZED_TIME = 0x18,

	ER_TAG_APP = 0x60,
	ER_TAG_SEQUENCE = 0x30,
	ER_TAG_SEQUENCE_OF = 0x30,
	ER_TAG_SET = 0x31,
	ER_TAG_SET_OF = 0x31,

	ER_TAG_CONTEXTUAL = 0xA0
};

/** @brief rules for encoding */
typedef enum
{
	WINPR_ASN1_BER,
	WINPR_ASN1_DER
} WinPrAsn1EncodingRule;

typedef struct WinPrAsn1Encoder WinPrAsn1Encoder;

struct WinPrAsn1Decoder
{
	WinPrAsn1EncodingRule encoding;
	wStream source;
};

typedef struct WinPrAsn1Decoder WinPrAsn1Decoder;

typedef BYTE WinPrAsn1_tag;
typedef BYTE WinPrAsn1_tagId;
typedef BOOL WinPrAsn1_BOOL;
typedef INT32 WinPrAsn1_INTEGER;
typedef INT32 WinPrAsn1_ENUMERATED;
typedef char* WinPrAsn1_STRING;
typedef char* WinPrAsn1_IA5STRING;
typedef struct
{
	size_t len;
	BYTE* data;
} WinPrAsn1_MemoryChunk;

typedef WinPrAsn1_MemoryChunk WinPrAsn1_OID;
typedef WinPrAsn1_MemoryChunk WinPrAsn1_OctetString;

typedef struct
{
	UINT16 year;
	UINT8 month;
	UINT8 day;
	UINT8 hour;
	UINT8 minute;
	UINT8 second;
	char tz;
} WinPrAsn1_UTCTIME;

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

	WINPR_API void WinPrAsn1FreeOID(WinPrAsn1_OID* poid);
	WINPR_API void WinPrAsn1FreeOctetString(WinPrAsn1_OctetString* octets);

	/* decoder functions */

	WINPR_API void WinPrAsn1Decoder_Init(WinPrAsn1Decoder* dec, WinPrAsn1EncodingRule encoding,
	                                     wStream* source);
	WINPR_API void WinPrAsn1Decoder_InitMem(WinPrAsn1Decoder* dec, WinPrAsn1EncodingRule encoding,
	                                        const BYTE* source, size_t len);

	WINPR_API BOOL WinPrAsn1DecPeekTag(WinPrAsn1Decoder* dec, WinPrAsn1_tag* tag);
	WINPR_API size_t WinPrAsn1DecReadTagAndLen(WinPrAsn1Decoder* dec, WinPrAsn1_tag* tag,
	                                           size_t* len);
	WINPR_API size_t WinPrAsn1DecPeekTagAndLen(WinPrAsn1Decoder* dec, WinPrAsn1_tag* tag,
	                                           size_t* len);
	WINPR_API size_t WinPrAsn1DecReadTagLenValue(WinPrAsn1Decoder* dec, WinPrAsn1_tag* tag,
	                                             size_t* len, WinPrAsn1Decoder* value);
	WINPR_API size_t WinPrAsn1DecReadBoolean(WinPrAsn1Decoder* dec, WinPrAsn1_BOOL* target);
	WINPR_API size_t WinPrAsn1DecReadInteger(WinPrAsn1Decoder* dec, WinPrAsn1_INTEGER* target);
	WINPR_API size_t WinPrAsn1DecReadEnumerated(WinPrAsn1Decoder* dec,
	                                            WinPrAsn1_ENUMERATED* target);
	WINPR_API size_t WinPrAsn1DecReadOID(WinPrAsn1Decoder* dec, WinPrAsn1_OID* target,
	                                     BOOL allocate);
	WINPR_API size_t WinPrAsn1DecReadOctetString(WinPrAsn1Decoder* dec,
	                                             WinPrAsn1_OctetString* target, BOOL allocate);
	WINPR_API size_t WinPrAsn1DecReadIA5String(WinPrAsn1Decoder* dec, WinPrAsn1_IA5STRING* target);
	WINPR_API size_t WinPrAsn1DecReadGeneralString(WinPrAsn1Decoder* dec, WinPrAsn1_STRING* target);
	WINPR_API size_t WinPrAsn1DecReadUtcTime(WinPrAsn1Decoder* dec, WinPrAsn1_UTCTIME* target);
	WINPR_API size_t WinPrAsn1DecReadNull(WinPrAsn1Decoder* dec);

	WINPR_API size_t WinPrAsn1DecReadApp(WinPrAsn1Decoder* dec, WinPrAsn1_tagId* tagId,
	                                     WinPrAsn1Decoder* setDec);
	WINPR_API size_t WinPrAsn1DecReadSequence(WinPrAsn1Decoder* dec, WinPrAsn1Decoder* seqDec);
	WINPR_API size_t WinPrAsn1DecReadSet(WinPrAsn1Decoder* dec, WinPrAsn1Decoder* setDec);

	WINPR_API size_t WinPrAsn1DecReadContextualTag(WinPrAsn1Decoder* dec, WinPrAsn1_tagId* tagId,
	                                               WinPrAsn1Decoder* ctxtDec);
	WINPR_API size_t WinPrAsn1DecPeekContextualTag(WinPrAsn1Decoder* dec, WinPrAsn1_tagId* tagId,
	                                               WinPrAsn1Decoder* ctxtDec);

	WINPR_API size_t WinPrAsn1DecReadContextualBool(WinPrAsn1Decoder* dec, WinPrAsn1_tagId tagId,
	                                                BOOL* error, WinPrAsn1_BOOL* target);
	WINPR_API size_t WinPrAsn1DecReadContextualInteger(WinPrAsn1Decoder* dec, WinPrAsn1_tagId tagId,
	                                                   BOOL* error, WinPrAsn1_INTEGER* target);
	WINPR_API size_t WinPrAsn1DecReadContextualOID(WinPrAsn1Decoder* dec, WinPrAsn1_tagId tagId,
	                                               BOOL* error, WinPrAsn1_OID* target,
	                                               BOOL allocate);
	WINPR_API size_t WinPrAsn1DecReadContextualOctetString(WinPrAsn1Decoder* dec,
	                                                       WinPrAsn1_tagId tagId, BOOL* error,
	                                                       WinPrAsn1_OctetString* target,
	                                                       BOOL allocate);
	WINPR_API size_t WinPrAsn1DecReadContextualSequence(WinPrAsn1Decoder* dec,
	                                                    WinPrAsn1_tagId tagId, BOOL* error,
	                                                    WinPrAsn1Decoder* target);
	WINPR_API wStream WinPrAsn1DecGetStream(WinPrAsn1Decoder* dec);

	/* encoder functions */

	WINPR_API WinPrAsn1Encoder* WinPrAsn1Encoder_New(WinPrAsn1EncodingRule encoding);
	WINPR_API void WinPrAsn1Encoder_Reset(WinPrAsn1Encoder* enc);

	WINPR_API BOOL WinPrAsn1EncAppContainer(WinPrAsn1Encoder* enc, WinPrAsn1_tagId tagId);
	WINPR_API BOOL WinPrAsn1EncSeqContainer(WinPrAsn1Encoder* enc);
	WINPR_API BOOL WinPrAsn1EncContextualSeqContainer(WinPrAsn1Encoder* enc, WinPrAsn1_tagId tagId);
	WINPR_API BOOL WinPrAsn1EncSetContainer(WinPrAsn1Encoder* enc);
	WINPR_API BOOL WinPrAsn1EncContextualSetContainer(WinPrAsn1Encoder* enc, WinPrAsn1_tagId tagId);
	WINPR_API BOOL WinPrAsn1EncContextualContainer(WinPrAsn1Encoder* enc, WinPrAsn1_tagId tagId);
	WINPR_API BOOL WinPrAsn1EncOctetStringContainer(WinPrAsn1Encoder* enc);
	WINPR_API BOOL WinPrAsn1EncContextualOctetStringContainer(WinPrAsn1Encoder* enc,
	                                                          WinPrAsn1_tagId tagId);
	WINPR_API size_t WinPrAsn1EncEndContainer(WinPrAsn1Encoder* enc);

	WINPR_API size_t WinPrAsn1EncRawContent(WinPrAsn1Encoder* enc, const WinPrAsn1_MemoryChunk* c);
	WINPR_API size_t WinPrAsn1EncContextualRawContent(WinPrAsn1Encoder* enc, WinPrAsn1_tagId tagId,
	                                                  const WinPrAsn1_MemoryChunk* c);
	WINPR_API size_t WinPrAsn1EncInteger(WinPrAsn1Encoder* enc, WinPrAsn1_INTEGER integer);
	WINPR_API size_t WinPrAsn1EncContextualInteger(WinPrAsn1Encoder* enc, WinPrAsn1_tagId tagId,
	                                               WinPrAsn1_INTEGER integer);
	WINPR_API size_t WinPrAsn1EncBoolean(WinPrAsn1Encoder* enc, WinPrAsn1_BOOL b);
	WINPR_API size_t WinPrAsn1EncContextualBoolean(WinPrAsn1Encoder* enc, WinPrAsn1_tagId tagId,
	                                               WinPrAsn1_BOOL b);
	WINPR_API size_t WinPrAsn1EncEnumerated(WinPrAsn1Encoder* enc, WinPrAsn1_ENUMERATED e);
	WINPR_API size_t WinPrAsn1EncContextualEnumerated(WinPrAsn1Encoder* enc, WinPrAsn1_tagId tagId,
	                                                  WinPrAsn1_ENUMERATED e);

	WINPR_API size_t WinPrAsn1EncOID(WinPrAsn1Encoder* enc, const WinPrAsn1_OID* oid);
	WINPR_API size_t WinPrAsn1EncContextualOID(WinPrAsn1Encoder* enc, WinPrAsn1_tagId tagId,
	                                           const WinPrAsn1_OID* oid);
	WINPR_API size_t WinPrAsn1EncOctetString(WinPrAsn1Encoder* enc,
	                                         const WinPrAsn1_OctetString* oid);
	WINPR_API size_t WinPrAsn1EncContextualOctetString(WinPrAsn1Encoder* enc, WinPrAsn1_tagId tagId,
	                                                   const WinPrAsn1_OctetString* oid);
	WINPR_API size_t WinPrAsn1EncIA5String(WinPrAsn1Encoder* enc, WinPrAsn1_IA5STRING ia5);
	WINPR_API size_t WinPrAsn1EncGeneralString(WinPrAsn1Encoder* enc, WinPrAsn1_STRING str);
	WINPR_API size_t WinPrAsn1EncContextualIA5String(WinPrAsn1Encoder* enc, WinPrAsn1_tagId tagId,
	                                                 WinPrAsn1_IA5STRING ia5);
	WINPR_API size_t WinPrAsn1EncUtcTime(WinPrAsn1Encoder* enc, const WinPrAsn1_UTCTIME* utc);
	WINPR_API size_t WinPrAsn1EncContextualUtcTime(WinPrAsn1Encoder* enc, WinPrAsn1_tagId tagId,
	                                               const WinPrAsn1_UTCTIME* utc);

	WINPR_API BOOL WinPrAsn1EncStreamSize(WinPrAsn1Encoder* enc, size_t* s);
	WINPR_API BOOL WinPrAsn1EncToStream(WinPrAsn1Encoder* enc, wStream* s);

	WINPR_API void WinPrAsn1Encoder_Free(WinPrAsn1Encoder** penc);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* WINPR_ASN1_H_ */
