/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Authentication redirection virtual channel
 *
 * Copyright 2024 David Fort <contact@hardening-consulting.com>
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

#ifndef WINPR_NDR_H_
#define WINPR_NDR_H_

#include <winpr/stream.h>

#define WINPR_NDR_PTR_NULL (0UL)

#define WINPR_NDR_SIMPLE_TYPE_DECL(PREFIX, LOWER, UPPER)                                          \
	WINPR_API BOOL PREFIX##_decoder_read_##LOWER(WinPrNdrDecoder* context, wStream* s, UPPER* v); \
	WINPR_API BOOL PREFIX##_decoder_read_##LOWER##_(WinPrNdrDecoder* context, wStream* s,         \
	                                                const void* hints, void* v);                  \
	WINPR_API BOOL PREFIX##_encoder_write_##LOWER(WinPrNdrEncoder* context, wStream* s, UPPER v); \
	WINPR_API BOOL PREFIX##_encoder_write_##LOWER##_(WinPrNdrEncoder* context, wStream* s,        \
	                                                 const void* hints, const void* v);           \
	extern WINPR_API const WinPrNdrMessageDescr PREFIX##_##LOWER##_descr_s;                       \
	WINPR_API WinPrNdrMessageType PREFIX##_##LOWER##_descr(void)

#define WINPR_NDR_ARRAY_OF_TYPE_DECL(PREFIX, TYPE, UPPERTYPE)                                 \
	WINPR_API BOOL PREFIX##_decoder_read_##TYPE##Array(WinPrNdrDecoder* context, wStream* s,  \
	                                                   const void* hints, void* v);           \
	WINPR_API BOOL PREFIX##_encoder_write_##TYPE##Array(WinPrNdrEncoder* context, wStream* s, \
	                                                    const void* hints, const void* v);    \
	WINPR_API void PREFIX##_destroy_##TYPE##Array(const void* hints, void* obj);              \
	extern WINPR_API const WinPrNdrMessageDescr PREFIX##_##TYPE##Array_descr_s;               \
	WINPR_API WinPrNdrMessageType PREFIX##_##TYPE##Array_descr(void);                         \
                                                                                              \
	WINPR_API BOOL PREFIX##_decoder_read_##TYPE##VaryingArray(                                \
	    WinPrNdrDecoder* context, wStream* s, const void* hints, void* v);                    \
	WINPR_API BOOL PREFIX##_encoder_write_##TYPE##VaryingArray(                               \
	    WinPrNdrEncoder* context, wStream* s, const void* hints, const void* v);              \
	extern WINPR_API const WinPrNdrMessageDescr PREFIX##_##TYPE##VaryingArray_descr_s;        \
	WINPR_API WinPrNdrMessageType PREFIX##_##TYPE##VaryingArray_descr(void)

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct WinPrNdrDecoder_s WinPrNdrDecoder;
	typedef struct WinPrNdrEncoder_s WinPrNdrEncoder;

	typedef UINT32 winpr_ndr_refid;

	typedef BOOL (*WINPR_NDR_READER_FN)(WinPrNdrDecoder* context, wStream* s, const void* hints,
	                                    void* target);
	typedef BOOL (*WINPR_NDR_WRITER_FN)(WinPrNdrEncoder* context, wStream* s, const void* hints,
	                                    const void* obj);
	typedef void (*WINPR_NDR_DESTROY_FN)(const void* hints, void* obj);
	typedef void (*WINPR_NDR_DUMP_FN)(wLog* logger, UINT32 lvl, size_t indentLevel,
	                                  const void* obj);

	/** @brief arity of a message */
	typedef enum
	{
		WINPR_NDR_ARITY_SIMPLE,
		WINPR_NDR_ARITY_ARRAYOF,
		WINPR_NDR_ARITY_VARYING_ARRAYOF,
	} WinPrNdrTypeArity;

	/** @brief message descriptor */
	typedef struct
	{
		WinPrNdrTypeArity arity;
		size_t itemSize;
		WINPR_NDR_READER_FN readFn;
		WINPR_NDR_WRITER_FN writeFn;
		WINPR_NDR_DESTROY_FN destroyFn;
		WINPR_NDR_DUMP_FN dumpFn;
	} WinPrNdrMessageDescr;

	typedef const WinPrNdrMessageDescr* WinPrNdrMessageType;

	/** @brief pointer or not and if null is accepted */
	typedef enum
	{
		WINPR_NDR_NOT_POINTER,
		WINPR_NDR_POINTER_NON_NULL,
		WINPR_NDR_POINTER
	} WinPrNdrPointerType;

	/** @brief descriptor of a field in a structure */
	typedef struct
	{
		const char* name;
		size_t structOffset;
		WinPrNdrPointerType pointerType;
		SSIZE_T hintsField;
		WinPrNdrMessageType typeDescr;
	} WinPrNdrFieldStruct;

	/** @brief structure descriptor */
	typedef struct
	{
		const char* name;
		size_t nfields;
		const WinPrNdrFieldStruct* fields;
	} WinPrNdrStructDescr;

	/** @brief a deferred pointer */
	typedef struct
	{
		winpr_ndr_refid ptrId;
		const char* name;
		void* hints;
		void* target;
		WinPrNdrMessageType msg;
	} WinPrNdrDeferredEntry;

	WINPR_API void winpr_ndr_encoder_free(WinPrNdrEncoder* context);

	static INLINE void winpr_ndr_encoder_destroy(WinPrNdrEncoder** pcontext)
	{
		WINPR_ASSERT(pcontext);
		winpr_ndr_encoder_free(*pcontext);
		*pcontext = NULL;
	}

	WINPR_ATTR_MALLOC(winpr_ndr_encoder_free, 1)
	WINPR_API WinPrNdrEncoder* winpr_ndr_encoder_new(BOOL bigEndianDrep, BYTE version);

	WINPR_ATTR_MALLOC(winpr_ndr_encoder_free, 1)
	WINPR_API WinPrNdrEncoder* winpr_ndr_encoder_from_decoder(const WinPrNdrDecoder* src);

#if 0
	WINPR_API void winpr_ndr_context_reset(WinPrNdrContext* context);



	WINPR_ATTR_MALLOC(winpr_ndr_context_free, 1)
	WINPR_API WinPrNdrContext* winpr_ndr_read_header(wStream* s);
#endif

	WINPR_API BOOL winpr_ndr_encoder_write_header(WinPrNdrEncoder* context, wStream* s);

	WINPR_NDR_SIMPLE_TYPE_DECL(winpr_ndr, uint8, UINT8);
	WINPR_NDR_SIMPLE_TYPE_DECL(winpr_ndr, uint16, UINT16);
	WINPR_NDR_SIMPLE_TYPE_DECL(winpr_ndr, uint32, UINT32);
	WINPR_NDR_SIMPLE_TYPE_DECL(winpr_ndr, uint64, UINT64);

	WINPR_NDR_ARRAY_OF_TYPE_DECL(winpr_ndr, uint8, BYTE);
	WINPR_NDR_ARRAY_OF_TYPE_DECL(winpr_ndr, uint16, UINT16);

	WINPR_API void winpr_ndr_encoder_bytes_written(WinPrNdrEncoder* context, size_t len);
	WINPR_API BOOL winpr_ndr_encoder_write_align(WinPrNdrEncoder* context, wStream* s, size_t sz);
	WINPR_API BOOL winpr_ndr_encoder_write_data(WinPrNdrEncoder* context, wStream* s,
	                                            const void* data, size_t sz);

	WINPR_API BOOL winpr_ndr_encoder_write_pickle(WinPrNdrEncoder* context, wStream* s);

	WINPR_API BOOL winpr_ndr_encoder_write_constructed(WinPrNdrEncoder* context, wStream* s,
	                                                   wStream* payload);

	WINPR_API BOOL winpr_ndr_encoder_start_constructed(WinPrNdrEncoder* context, wStream* s);
	WINPR_API BOOL winpr_ndr_encoder_end_constructed(WinPrNdrEncoder* context, wStream* s);

	/** @brief hints for a varying conformant array */
	typedef struct
	{
		UINT32 length;
		UINT32 maxLength;
	} WinPrNdrVaryingArrayHints;

	WINPR_API BOOL winpr_ndr_encoder_write_uconformant_varying_array(
	    WinPrNdrEncoder* context, wStream* s, const WinPrNdrVaryingArrayHints* hints,
	    WinPrNdrMessageType itemType, const void* src);

	/** @brief hints for a conformant array */
	typedef struct
	{
		UINT32 count;
	} WinPrNdrArrayHints;

	WINPR_API BOOL winpr_ndr_encoder_write_uconformant_array(WinPrNdrEncoder* context, wStream* s,
	                                                         UINT32 len,
	                                                         WinPrNdrMessageType itemType,
	                                                         const BYTE* ptr);

	WINPR_API BOOL winpr_ndr_encoder_write_fromDescr(WinPrNdrEncoder* context, wStream* s,
	                                                 const WinPrNdrStructDescr* descr,
	                                                 const void* src);

	WINPR_API winpr_ndr_refid winpr_ndr_pointer_refid(const void* ptr);

	WINPR_API BOOL winpr_ndr_encoder_allocatePtr(WinPrNdrEncoder* context, const void* ptr,
	                                             winpr_ndr_refid* prefId, BOOL* pnewPtr);

	WINPR_API BOOL winpr_ndr_encoder_push_deferreds(WinPrNdrEncoder* context,
	                                                WinPrNdrDeferredEntry* deferreds,
	                                                size_t ndeferred);
	WINPR_API BOOL winpr_ndr_encoder_treat_deferreds(WinPrNdrEncoder* context, wStream* s);

	/* =========================================== ============================*/
	WINPR_API void winpr_ndr_decoder_free(WinPrNdrDecoder* context);

	static INLINE void winpr_ndr_decoder_destroy(WinPrNdrDecoder** pcontext)
	{
		WINPR_ASSERT(pcontext);
		winpr_ndr_decoder_free(*pcontext);
		*pcontext = NULL;
	}

	WINPR_ATTR_MALLOC(winpr_ndr_encoder_free, 1)
	WINPR_API WinPrNdrDecoder* winpr_ndr_decoder_new(BOOL bigEndianDrep, BYTE version);

	WINPR_ATTR_MALLOC(winpr_ndr_decoder_free, 1)
	WINPR_API WinPrNdrDecoder* winpr_ndr_decoder_new_fromStream(wStream* s);

	WINPR_API void winpr_ndr_decoder_bytes_read(WinPrNdrDecoder* context, size_t len);

	WINPR_API BOOL winpr_ndr_decoder_read_pickle(WinPrNdrDecoder* context, wStream* s);

	WINPR_API BOOL winpr_ndr_decoder_read_constructed(WinPrNdrDecoder* context, wStream* s,
	                                                  wStream* target);

	WINPR_API BOOL winpr_ndr_decoder_push_deferreds(WinPrNdrDecoder* context,
	                                                WinPrNdrDeferredEntry* deferreds,
	                                                size_t ndeferred);

	WINPR_API BOOL winpr_ndr_decoder_treat_deferreds(WinPrNdrDecoder* context, wStream* s);

	WINPR_API BOOL winpr_ndr_decoder_read_refpointer(WinPrNdrDecoder* context, wStream* s,
	                                                 UINT32* refId);

	WINPR_API BOOL winpr_ndr_decoder_skip_bytes(WinPrNdrDecoder* context, wStream* s,
	                                            size_t nbytes);

	WINPR_API BOOL winpr_ndr_decoder_read_align(WinPrNdrDecoder* context, wStream* s, size_t sz);

	WINPR_API BOOL winpr_ndr_decoder_read_wchar(WinPrNdrDecoder* context, wStream* s, WCHAR* ptr);

	WINPR_API BOOL winpr_ndr_decoder_read_pointedMessageEx(WinPrNdrDecoder* context, wStream* s,
	                                                       winpr_ndr_refid ptrId,
	                                                       WinPrNdrMessageType descr, void* hints,
	                                                       void** target);

	WINPR_API BOOL winpr_ndr_decoder_read_fromDescr(WinPrNdrDecoder* context, wStream* s,
	                                                const WinPrNdrStructDescr* descr, void* target);

	WINPR_API BOOL winpr_ndr_decoder_read_uconformant_array(WinPrNdrDecoder* context, wStream* s,
	                                                        const WinPrNdrArrayHints* hints,
	                                                        WinPrNdrMessageType itemType,
	                                                        void* vtarget);

	WINPR_API BOOL winpr_ndr_decoder_read_uconformant_varying_array(
	    WinPrNdrDecoder* context, wStream* s, const WinPrNdrVaryingArrayHints* hints,
	    WinPrNdrMessageType itemType, void* ptarget);

#ifdef __cplusplus
}
#endif

WINPR_API void winpr_ndr_struct_dump_fromDescr(wLog* logger, UINT32 lvl, size_t identLevel,
                                               const WinPrNdrStructDescr* descr, const void* obj);
WINPR_API void winpr_ndr_struct_destroy(const WinPrNdrStructDescr* descr, void* pptr);

#endif /* WINPR_NDR_H_ */
