/**
 * WinPR: Windows Portable Runtime
 * ASN.1 Encoding & Decoding Engine
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_ASN1_H
#define WINPR_ASN1_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char	ASN1uint8_t;
typedef signed char	ASN1int8_t;

typedef unsigned short	ASN1uint16_t;
typedef signed short	ASN1int16_t;

typedef unsigned long	ASN1uint32_t;
typedef signed long	ASN1int32_t;

typedef ASN1uint8_t	ASN1octet_t;

typedef ASN1uint8_t	ASN1bool_t;

struct tagASN1intx_t
{
	ASN1uint32_t length;
	ASN1octet_t* value;
};
typedef struct tagASN1intx_t ASN1intx_t;

struct tagASN1octetstring_t
{
	ASN1uint32_t length;
	ASN1octet_t* value;
};
typedef struct tagASN1octetstring_t ASN1octetstring_t;

struct tagASN1octetstring2_t
{
	ASN1uint32_t length;
	ASN1octet_t value[1];
};
typedef struct tagASN1octetstring2_t ASN1octetstring2_t;

struct ASN1iterator_s
{
	struct ASN1iterator_s* next;
	void* value;
};
typedef struct ASN1iterator_s ASN1iterator_t;

struct tagASN1bitstring_t
{
	ASN1uint32_t length;
	ASN1octet_t* value;
};
typedef struct tagASN1bitstring_t ASN1bitstring_t;

typedef char ASN1char_t;

struct tagASN1charstring_t
{
	ASN1uint32_t length;
	ASN1char_t* value;
};
typedef struct tagASN1charstring_t ASN1charstring_t;

typedef ASN1uint16_t ASN1char16_t;

struct tagASN1char16string_t
{
	ASN1uint32_t length;
	ASN1char16_t* value;
};
typedef struct tagASN1char16string_t ASN1char16string_t;

typedef ASN1uint32_t	ASN1char32_t;

struct tagASN1char32string_t
{
	ASN1uint32_t length;
	ASN1char32_t* value;
};
typedef struct tagASN1char32string_t ASN1char32string_t;

typedef ASN1char_t*	ASN1ztcharstring_t;
typedef ASN1char16_t*	ASN1ztchar16string_t;
typedef ASN1char32_t*	ASN1ztchar32string_t;

struct tagASN1wstring_t
{
	ASN1uint32_t length;
	WCHAR* value;
};
typedef struct tagASN1wstring_t ASN1wstring_t;

struct ASN1objectidentifier_s
{
	struct ASN1objectidentifier_s* next;
	ASN1uint32_t value;
};
typedef struct ASN1objectidentifier_s* ASN1objectidentifier_t;

struct tagASN1objectidentifier2_t
{
	ASN1uint16_t count;
	ASN1uint32_t value[16];
};
typedef struct tagASN1objectidentifier2_t ASN1objectidentifier2_t;

struct tagASN1encodedOID_t
{
	ASN1uint16_t length;
	ASN1octet_t* value;
};
typedef struct tagASN1encodedOID_t ASN1encodedOID_t;

typedef ASN1ztcharstring_t ASN1objectdescriptor_t;

struct tagASN1generalizedtime_t
{
	ASN1uint16_t year;
	ASN1uint8_t month;
	ASN1uint8_t day;
	ASN1uint8_t hour;
	ASN1uint8_t minute;
	ASN1uint8_t second;
	ASN1uint16_t millisecond;
	ASN1bool_t universal;
	ASN1int16_t diff;
};
typedef struct tagASN1generalizedtime_t	ASN1generalizedtime_t;

struct tagASN1utctime_t
{
	ASN1uint8_t year;
	ASN1uint8_t month;
	ASN1uint8_t day;
	ASN1uint8_t hour;
	ASN1uint8_t minute;
	ASN1uint8_t second;
	ASN1bool_t universal;
	ASN1int16_t diff;
};
typedef struct tagASN1utctime_t ASN1utctime_t;

struct tagASN1open_t
{
	ASN1uint32_t length;

	union
	{ 
		void* encoded;
		void* value;
	};
};
typedef struct tagASN1open_t ASN1open_t;

enum tagASN1blocktype_e
{
	ASN1_DER_SET_OF_BLOCK,
};
typedef enum tagASN1blocktype_e ASN1blocktype_e;

typedef ASN1int32_t	ASN1enum_t;
typedef ASN1uint16_t	ASN1choice_t;
typedef ASN1uint32_t	ASN1magic_t;

enum
{
	ASN1_CHOICE_BASE	= 1,
	ASN1_CHOICE_INVALID	= -1,
	ASN1_CHOICE_EXTENSION	= 0,
};

enum tagASN1error_e
{
	ASN1_SUCCESS		= 0,
	ASN1_ERR_INTERNAL	= -1001,
	ASN1_ERR_EOD		= -1002,
	ASN1_ERR_CORRUPT	= -1003,
	ASN1_ERR_LARGE		= -1004,
	ASN1_ERR_CONSTRAINT	= -1005,
	ASN1_ERR_MEMORY		= -1006,
	ASN1_ERR_OVERFLOW	= -1007,
	ASN1_ERR_BADPDU		= -1008,
	ASN1_ERR_BADARGS	= -1009,
	ASN1_ERR_BADREAL	= -1010,
	ASN1_ERR_BADTAG		= -1011,
	ASN1_ERR_CHOICE		= -1012,
	ASN1_ERR_RULE		= -1013,
	ASN1_ERR_UTF8		= -1014,
	ASN1_ERR_PDU_TYPE	= -1051,
	ASN1_ERR_NYI		= -1052,
	ASN1_WRN_EXTENDED	= 1001,
	ASN1_WRN_NOEOD		= 1002,
};
typedef enum tagASN1error_e ASN1error_e;

enum tagASN1encodingrule_e
{
	ASN1_BER_RULE_BER	= 0x0100,
	ASN1_BER_RULE_CER	= 0x0200,
	ASN1_BER_RULE_DER	= 0x0400,
	ASN1_BER_RULE		= ASN1_BER_RULE_BER | ASN1_BER_RULE_CER | ASN1_BER_RULE_DER,
};
typedef enum tagASN1encodingrule_e ASN1encodingrule_e;

typedef struct ASN1encoding_s* ASN1encoding_t;
typedef struct ASN1decoding_s* ASN1decoding_t;

typedef ASN1int32_t (*ASN1BerEncFun_t)(ASN1encoding_t enc, ASN1uint32_t tag, void* data);
typedef ASN1int32_t (*ASN1BerDecFun_t)(ASN1decoding_t enc, ASN1uint32_t tag, void* data);

struct tagASN1BerFunArr_t
{
	const ASN1BerEncFun_t* apfnEncoder;
	const ASN1BerDecFun_t* apfnDecoder;
};
typedef struct tagASN1BerFunArr_t ASN1BerFunArr_t;

typedef void (*ASN1GenericFun_t)(void);
typedef void (*ASN1FreeFun_t)(void* data);

struct tagASN1module_t
{
	ASN1magic_t nModuleName;
	ASN1encodingrule_e eRule;
	ASN1uint32_t dwFlags;
	ASN1uint32_t cPDUs;
	const ASN1FreeFun_t* apfnFreeMemory;
	const ASN1uint32_t* acbStructSize;
	ASN1BerFunArr_t BER;
};
typedef struct tagASN1module_t* ASN1module_t;

struct ASN1encoding_s
{
	ASN1magic_t magic;
	ASN1uint32_t version;
	ASN1module_t module;
	ASN1octet_t* buf;
	ASN1uint32_t size;
	ASN1uint32_t len;
	ASN1error_e err;
	ASN1uint32_t bit;
	ASN1octet_t* pos;
	ASN1uint32_t cbExtraHeader;
	ASN1encodingrule_e eRule;
	ASN1uint32_t dwFlags;
};

struct ASN1decoding_s
{
	ASN1magic_t magic;
	ASN1uint32_t version;
	ASN1module_t module;
	ASN1octet_t* buf;
	ASN1uint32_t size;
	ASN1uint32_t len;
	ASN1error_e err;
	ASN1uint32_t bit;
	ASN1octet_t* pos;
	ASN1encodingrule_e eRule;
	ASN1uint32_t dwFlags;
};

enum
{
	ASN1FLAGS_NONE			= 0x00000000L,
	ASN1FLAGS_NOASSERT		= 0x00001000L,
};

enum
{
	ASN1ENCODE_APPEND		= 0x00000001L,
	ASN1ENCODE_REUSEBUFFER		= 0x00000004L,
	ASN1ENCODE_SETBUFFER		= 0x00000008L,
	ASN1ENCODE_ALLOCATEBUFFER	= 0x00000010L,
	ASN1ENCODE_NOASSERT		= ASN1FLAGS_NOASSERT,
};

enum
{
	ASN1DECODE_APPENDED		= 0x00000001L,
	ASN1DECODE_REWINDBUFFER		= 0x00000004L,
	ASN1DECODE_SETBUFFER		= 0x00000008L,
	ASN1DECODE_AUTOFREEBUFFER	= 0x00000010L,
	ASN1DECODE_NOASSERT		= ASN1FLAGS_NOASSERT,
};


WINPR_API ASN1module_t ASN1_CreateModule(ASN1uint32_t nVersion, ASN1encodingrule_e eRule,
	ASN1uint32_t dwFlags, ASN1uint32_t cPDU, const ASN1GenericFun_t apfnEncoder[],
	const ASN1GenericFun_t apfnDecoder[], const ASN1FreeFun_t apfnFreeMemory[],
	const ASN1uint32_t acbStructSize[], ASN1magic_t nModuleName);

WINPR_API void ASN1_CloseModule(ASN1module_t pModule);

WINPR_API ASN1error_e ASN1_CreateEncoder(ASN1module_t pModule, ASN1encoding_t* ppEncoderInfo,
	ASN1octet_t* pbBuf, ASN1uint32_t cbBufSize, ASN1encoding_t pParent);

WINPR_API ASN1error_e ASN1_Encode(ASN1encoding_t pEncoderInfo, void* pDataStruct, ASN1uint32_t nPduNum,
	ASN1uint32_t dwFlags, ASN1octet_t* pbBuf, ASN1uint32_t cbBufSize);

WINPR_API void ASN1_CloseEncoder(ASN1encoding_t pEncoderInfo);

WINPR_API void ASN1_CloseEncoder2(ASN1encoding_t pEncoderInfo);

WINPR_API ASN1error_e ASN1_CreateDecoder(ASN1module_t pModule, ASN1decoding_t* ppDecoderInfo,
	ASN1octet_t* pbBuf, ASN1uint32_t cbBufSize, ASN1decoding_t pParent);

WINPR_API ASN1error_e ASN1_Decode(ASN1decoding_t pDecoderInfo, void** ppDataStruct, ASN1uint32_t nPduNum,
	ASN1uint32_t dwFlags, ASN1octet_t* pbBuf, ASN1uint32_t cbBufSize);

WINPR_API void ASN1_CloseDecoder(ASN1decoding_t pDecoderInfo);

WINPR_API void ASN1_FreeEncoded(ASN1encoding_t pEncoderInfo, void* pBuf);

WINPR_API void ASN1_FreeDecoded(ASN1decoding_t pDecoderInfo, void* pDataStruct, ASN1uint32_t nPduNum);

enum tagASN1option_e
{
	ASN1OPT_CHANGE_RULE		= 0x101,
	ASN1OPT_GET_RULE		= 0x201,
	ASN1OPT_NOT_REUSE_BUFFER	= 0x301,
	ASN1OPT_REWIND_BUFFER		= 0x302,
	ASN1OPT_SET_DECODED_BUFFER	= 0x501,
	ASN1OPT_DEL_DECODED_BUFFER	= 0x502,
	ASN1OPT_GET_DECODED_BUFFER_SIZE	= 0x601,
};
typedef enum tagASN1option_e ASN1option_e;

struct tagASN1optionparam_t
{
	ASN1option_e eOption;

	union
	{
		ASN1encodingrule_e eRule;
		ASN1uint32_t cbRequiredDecodedBufSize;

		struct
		{
			ASN1octet_t* pbBuf;
			ASN1uint32_t cbBufSize;
		} Buffer;
	};
};
typedef struct tagASN1optionparam_t ASN1optionparam_t;
typedef struct tagASN1optionparam_t ASN1optionparam_s;

WINPR_API ASN1error_e ASN1_SetEncoderOption(ASN1encoding_t pEncoderInfo, ASN1optionparam_t* pOptParam);
WINPR_API ASN1error_e ASN1_GetEncoderOption(ASN1encoding_t pEncoderInfo, ASN1optionparam_t* pOptParam);

WINPR_API ASN1error_e ASN1_SetDecoderOption(ASN1decoding_t pDecoderInfo, ASN1optionparam_t* pOptParam);
WINPR_API ASN1error_e ASN1_GetDecoderOption(ASN1decoding_t pDecoderInfo, ASN1optionparam_t* pOptParam);

WINPR_API void ASN1bitstring_free(ASN1bitstring_t* bitstring);
WINPR_API void ASN1octetstring_free(ASN1octetstring_t* octetstring);
WINPR_API void ASN1objectidentifier_free(ASN1objectidentifier_t* objectidentifier);
WINPR_API void ASN1charstring_free(ASN1charstring_t* charstring);
WINPR_API void ASN1char16string_free(ASN1char16string_t* char16string);
WINPR_API void ASN1char32string_free(ASN1char32string_t* char32string);
WINPR_API void ASN1ztcharstring_free(ASN1ztcharstring_t ztcharstring);
WINPR_API void ASN1ztchar16string_free(ASN1ztchar16string_t ztchar16string);
WINPR_API void ASN1ztchar32string_free(ASN1ztchar32string_t ztchar32string);
WINPR_API void ASN1open_free(ASN1open_t* open);
WINPR_API void ASN1utf8string_free(ASN1wstring_t* utf8string);

WINPR_API LPVOID ASN1DecAlloc(ASN1decoding_t dec, ASN1uint32_t size);
WINPR_API LPVOID ASN1DecRealloc(ASN1decoding_t dec, LPVOID ptr, ASN1uint32_t size);

WINPR_API void ASN1Free(LPVOID ptr);

WINPR_API ASN1error_e ASN1EncSetError(ASN1encoding_t enc, ASN1error_e err);
WINPR_API ASN1error_e ASN1DecSetError(ASN1decoding_t dec, ASN1error_e err);

WINPR_API ASN1uint32_t ASN1intx_uoctets(ASN1intx_t* intx);
WINPR_API void ASN1intx_free(ASN1intx_t* intx);
WINPR_API ASN1int32_t ASN1intx2int32(ASN1intx_t* intx);
WINPR_API ASN1uint32_t ASN1intx2uint32(ASN1intx_t* intx);
WINPR_API int ASN1intxisuint32(ASN1intx_t* intx);
WINPR_API void ASN1intx_setuint32(ASN1intx_t* dst, ASN1uint32_t val);

WINPR_API ASN1uint32_t ASN1uint32_uoctets(ASN1uint32_t val);

WINPR_API int ASN1intx_cmp(ASN1intx_t* intx1, ASN1intx_t* intx2);
WINPR_API int ASN1objectidentifier_cmp(ASN1objectidentifier_t* oid1, ASN1objectidentifier_t* oid2);
WINPR_API int ASN1objectidentifier2_cmp(ASN1objectidentifier2_t* oid1, ASN1objectidentifier2_t* oid2);
WINPR_API int ASN1bitstring_cmp(ASN1bitstring_t* str1, ASN1bitstring_t* str2, int length);
WINPR_API int ASN1octetstring_cmp(ASN1octetstring_t* str1, ASN1octetstring_t* str2);
WINPR_API int ASN1charstring_cmp(ASN1charstring_t* str1, ASN1charstring_t* str2);
WINPR_API int ASN1char16string_cmp(ASN1char16string_t* str1, ASN1char16string_t* str2);
WINPR_API int ASN1char32string_cmp(ASN1char32string_t* str1, ASN1char32string_t* str2);
WINPR_API int ASN1ztcharstring_cmp(ASN1ztcharstring_t str1, ASN1ztcharstring_t str2);
WINPR_API int ASN1ztchar16string_cmp(ASN1ztchar16string_t str1, ASN1ztchar16string_t str2);
WINPR_API int ASN1ztchar32string_cmp(ASN1ztchar32string_t str1, ASN1ztchar32string_t str2);
WINPR_API int ASN1open_cmp(ASN1open_t* open1, ASN1open_t* open2);
WINPR_API int ASN1generalizedtime_cmp(ASN1generalizedtime_t* t1, ASN1generalizedtime_t* t2);
WINPR_API int ASN1utctime_cmp(ASN1utctime_t* t1, ASN1utctime_t* t2);

WINPR_API int ASN1BEREncCharString(ASN1encoding_t enc, ASN1uint32_t tag, ASN1uint32_t len, ASN1char_t* val);
WINPR_API int ASN1DEREncCharString(ASN1encoding_t enc, ASN1uint32_t tag, ASN1uint32_t len, ASN1char_t* val);
WINPR_API int ASN1BEREncChar16String(ASN1encoding_t enc, ASN1uint32_t tag, ASN1uint32_t len, ASN1char16_t* val);
WINPR_API int ASN1DEREncChar16String(ASN1encoding_t enc, ASN1uint32_t tag, ASN1uint32_t len, ASN1char16_t* val);
WINPR_API int ASN1BEREncChar32String(ASN1encoding_t enc, ASN1uint32_t tag, ASN1uint32_t len, ASN1char32_t* val);
WINPR_API int ASN1DEREncChar32String(ASN1encoding_t enc, ASN1uint32_t tag, ASN1uint32_t len, ASN1char32_t* val);
WINPR_API int ASN1BEREncOctetString(ASN1encoding_t enc, ASN1uint32_t tag, ASN1uint32_t len, ASN1octet_t* val);
WINPR_API int ASN1DEREncOctetString(ASN1encoding_t enc, ASN1uint32_t tag, ASN1uint32_t len, ASN1octet_t* val);
WINPR_API int ASN1BEREncBitString(ASN1encoding_t enc, ASN1uint32_t tag, ASN1uint32_t bitlen, ASN1octet_t* val);
WINPR_API int ASN1DEREncBitString(ASN1encoding_t enc, ASN1uint32_t tag, ASN1uint32_t bitlen, ASN1octet_t* val);
WINPR_API int ASN1BEREncGeneralizedTime(ASN1encoding_t enc, ASN1uint32_t tag, ASN1generalizedtime_t* val);
WINPR_API int ASN1DEREncGeneralizedTime(ASN1encoding_t enc, ASN1uint32_t tag, ASN1generalizedtime_t* val);
WINPR_API int ASN1BEREncS32(ASN1encoding_t enc, ASN1uint32_t tag, ASN1int32_t val);
WINPR_API int ASN1BEREncSX(ASN1encoding_t enc, ASN1uint32_t tag, ASN1intx_t* val);
WINPR_API int ASN1BEREncZeroMultibyteString(ASN1encoding_t enc, ASN1uint32_t tag, ASN1ztcharstring_t val);
WINPR_API int ASN1DEREncZeroMultibyteString(ASN1encoding_t enc, ASN1uint32_t tag, ASN1ztcharstring_t val);
WINPR_API int ASN1BEREncMultibyteString(ASN1encoding_t enc, ASN1uint32_t tag, ASN1charstring_t* val);
WINPR_API int ASN1DEREncMultibyteString(ASN1encoding_t enc, ASN1uint32_t tag, ASN1charstring_t* val);
WINPR_API int ASN1BEREncNull(ASN1encoding_t enc, ASN1uint32_t tag);
WINPR_API int ASN1BEREncObjectIdentifier(ASN1encoding_t enc, ASN1uint32_t tag, ASN1objectidentifier_t* val);
WINPR_API int ASN1BEREncObjectIdentifier2(ASN1encoding_t enc, ASN1uint32_t tag, ASN1objectidentifier2_t* val);
WINPR_API int ASN1BEREncRemoveZeroBits(ASN1uint32_t* nbits,ASN1octet_t* val);
WINPR_API int ASN1BEREncRemoveZeroBits2(ASN1uint32_t* nbits,ASN1octet_t* val,ASN1uint32_t minlen);
WINPR_API int ASN1BEREncUTCTime(ASN1encoding_t enc, ASN1uint32_t tag, ASN1utctime_t* val);
WINPR_API int ASN1DEREncUTCTime(ASN1encoding_t enc, ASN1uint32_t tag, ASN1utctime_t* val);
WINPR_API int ASN1BEREncFlush(ASN1encoding_t enc);
WINPR_API int ASN1BEREncOpenType(ASN1encoding_t enc, ASN1open_t* val);
WINPR_API int ASN1BERDecExplicitTag(ASN1decoding_t dec, ASN1uint32_t tag, ASN1decoding_t* dd, ASN1octet_t* *ppBufEnd);

WINPR_API int ASN1BERDecEndOfContents(ASN1decoding_t dec, ASN1decoding_t dd, ASN1octet_t* pBufEnd);
WINPR_API int ASN1BERDecOctetString(ASN1decoding_t dec, ASN1uint32_t tag, ASN1octetstring_t* val);
WINPR_API int ASN1BERDecOctetString2(ASN1decoding_t dec, ASN1uint32_t tag, ASN1octetstring_t* val);
WINPR_API int ASN1BERDecBitString(ASN1decoding_t dec, ASN1uint32_t tag, ASN1bitstring_t* val);
WINPR_API int ASN1BERDecBitString2(ASN1decoding_t dec, ASN1uint32_t tag, ASN1bitstring_t* val);
WINPR_API int ASN1BERDecChar16String(ASN1decoding_t dec, ASN1uint32_t tag, ASN1char16string_t* val);
WINPR_API int ASN1BERDecChar32String(ASN1decoding_t dec, ASN1uint32_t tag, ASN1char32string_t* val);
WINPR_API int ASN1BERDecCharString(ASN1decoding_t dec, ASN1uint32_t tag, ASN1charstring_t* val);
WINPR_API int ASN1BERDecGeneralizedTime(ASN1decoding_t dec, ASN1uint32_t tag, ASN1generalizedtime_t* val);
WINPR_API int ASN1BERDecZeroMultibyteString(ASN1decoding_t dec, ASN1uint32_t tag, ASN1ztcharstring_t* val);
WINPR_API int ASN1BERDecMultibyteString(ASN1decoding_t dec, ASN1uint32_t tag, ASN1charstring_t* val);
WINPR_API int ASN1BERDecNull(ASN1decoding_t dec, ASN1uint32_t tag);
WINPR_API int ASN1BERDecObjectIdentifier(ASN1decoding_t dec, ASN1uint32_t tag, ASN1objectidentifier_t* val);
WINPR_API int ASN1BERDecObjectIdentifier2(ASN1decoding_t dec, ASN1uint32_t tag, ASN1objectidentifier2_t* val);
WINPR_API int ASN1BERDecS8Val(ASN1decoding_t dec, ASN1uint32_t tag, ASN1int8_t* val);
WINPR_API int ASN1BERDecS16Val(ASN1decoding_t dec, ASN1uint32_t tag, ASN1int16_t* val);
WINPR_API int ASN1BERDecS32Val(ASN1decoding_t dec, ASN1uint32_t tag, ASN1int32_t* val);
WINPR_API int ASN1BERDecSXVal(ASN1decoding_t dec, ASN1uint32_t tag, ASN1intx_t* val);
WINPR_API int ASN1BERDecU8Val(ASN1decoding_t dec, ASN1uint32_t tag, ASN1uint8_t* val);
WINPR_API int ASN1BERDecU16Val(ASN1decoding_t dec, ASN1uint32_t tag, ASN1uint16_t* val);
WINPR_API int ASN1BERDecU32Val(ASN1decoding_t dec, ASN1uint32_t tag, ASN1uint32_t* val);
WINPR_API int ASN1BERDecUTCTime(ASN1decoding_t dec, ASN1uint32_t tag, ASN1utctime_t* val);
WINPR_API int ASN1BERDecZeroCharString(ASN1decoding_t dec, ASN1uint32_t tag, ASN1ztcharstring_t* val);
WINPR_API int ASN1BERDecZeroChar16String(ASN1decoding_t dec, ASN1uint32_t tag, ASN1ztchar16string_t* val);
WINPR_API int ASN1BERDecZeroChar32String(ASN1decoding_t dec, ASN1uint32_t tag, ASN1ztchar32string_t* val);
WINPR_API int ASN1BERDecSkip(ASN1decoding_t dec);
WINPR_API int ASN1BERDecFlush(ASN1decoding_t dec);
WINPR_API int ASN1BERDecOpenType(ASN1decoding_t dec, ASN1open_t* val);
WINPR_API int ASN1BERDecOpenType2(ASN1decoding_t dec, ASN1open_t* val);

WINPR_API int ASN1BEREncCheck(ASN1encoding_t enc, ASN1uint32_t noctets);
WINPR_API int ASN1BEREncTag(ASN1encoding_t enc, ASN1uint32_t tag);
WINPR_API int ASN1BEREncExplicitTag(ASN1encoding_t enc, ASN1uint32_t tag, ASN1uint32_t* pnLenOff);
WINPR_API int ASN1BEREncEndOfContents(ASN1encoding_t enc, ASN1uint32_t nLenOff);
WINPR_API int ASN1BEREncLength(ASN1encoding_t enc, ASN1uint32_t len);

WINPR_API int ASN1BERDecCheck(ASN1decoding_t dec, ASN1uint32_t len);
WINPR_API int ASN1BERDecTag(ASN1decoding_t dec, ASN1uint32_t tag, ASN1uint32_t* constructed);
WINPR_API int ASN1BERDecLength(ASN1decoding_t dec, ASN1uint32_t* len, ASN1uint32_t* infinite);
WINPR_API int ASN1BERDecNotEndOfContents(ASN1decoding_t dec, ASN1octet_t* pBufEnd);
WINPR_API int ASN1BERDecPeekTag(ASN1decoding_t dec, ASN1uint32_t* tag);

WINPR_API int ASN1BEREncU32(ASN1encoding_t enc, ASN1uint32_t tag, ASN1uint32_t val);
WINPR_API int ASN1BEREncBool(ASN1encoding_t enc, ASN1uint32_t tag, ASN1bool_t val);
WINPR_API int ASN1BERDecBool(ASN1decoding_t dec, ASN1uint32_t tag, ASN1bool_t* val);
WINPR_API int ASN1BEREncEoid(ASN1encoding_t enc, ASN1uint32_t tag, ASN1encodedOID_t* val);
WINPR_API int ASN1BERDecEoid(ASN1decoding_t dec, ASN1uint32_t tag, ASN1encodedOID_t* val);
WINPR_API int ASN1BERDotVal2Eoid(ASN1encoding_t enc, char* pszDotVal, ASN1encodedOID_t* pOut);
WINPR_API int ASN1BEREoid2DotVal(ASN1decoding_t dec, ASN1encodedOID_t* pIn, char** ppszDotVal);
WINPR_API void ASN1BEREoid_free(ASN1encodedOID_t* val);
WINPR_API int ASN1BEREncUTF8String(ASN1encoding_t enc, ASN1uint32_t tag, ASN1uint32_t length, WCHAR* value);
WINPR_API int ASN1DEREncUTF8String(ASN1encoding_t enc, ASN1uint32_t tag, ASN1uint32_t length, WCHAR* value);
WINPR_API int ASN1BERDecUTF8String(ASN1decoding_t dec, ASN1uint32_t tag, ASN1wstring_t* val);
WINPR_API int ASN1DEREncBeginBlk(ASN1encoding_t enc, ASN1blocktype_e eBlkType, void** ppBlk);
WINPR_API int ASN1DEREncNewBlkElement(void* pBlk, ASN1encoding_t* enc2);
WINPR_API int ASN1DEREncFlushBlkElement(void* pBlk);
WINPR_API int ASN1DEREncEndBlk(void* pBlk);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_ASN1_H */

