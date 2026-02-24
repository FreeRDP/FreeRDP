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

#ifndef CHANNELS_RDPEAR_NDR_H_
#define CHANNELS_RDPEAR_NDR_H_

#include <winpr/stream.h>
#include <freerdp/api.h>

#define NDR_PTR_NULL (0UL)

#define NDR_SIMPLE_TYPE_DECL(LOWER, UPPER)                                                       \
	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_read_##LOWER(NdrContext* context, wStream* s,    \
	                                                         UPPER* v);                          \
	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_read_##LOWER##_(NdrContext* context, wStream* s, \
	                                                            const void* hints, void* v);     \
	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_write_##LOWER(NdrContext* context, wStream* s,   \
	                                                          UPPER v);                          \
	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_write_##LOWER##_(                                \
	    NdrContext* context, wStream* s, const void* hints, const void* v);                      \
	FREERDP_LOCAL                                                                                \
	extern const NdrMessageDescr ndr_##LOWER##_descr_s;                                          \
	WINPR_ATTR_NODISCARD FREERDP_LOCAL NdrMessageType ndr_##LOWER##_descr(void)

#define NDR_ARRAY_OF_TYPE_DECL(TYPE, UPPERTYPE)                                        \
	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_read_##TYPE##Array(                    \
	    NdrContext* context, wStream* s, const void* hints, void* v);                  \
	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_write_##TYPE##Array(                   \
	    NdrContext* context, wStream* s, const void* hints, const void* v);            \
	FREERDP_LOCAL                                                                      \
	void ndr_destroy_##TYPE##Array(NdrContext* context, const void* hints, void* obj); \
	FREERDP_LOCAL                                                                      \
	extern const NdrMessageDescr ndr_##TYPE##Array_descr_s;                            \
	WINPR_ATTR_NODISCARD FREERDP_LOCAL NdrMessageType ndr_##TYPE##Array_descr(void);   \
                                                                                       \
	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_read_##TYPE##VaryingArray(             \
	    NdrContext* context, wStream* s, const void* hints, void* v);                  \
	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_write_##TYPE##VaryingArray(            \
	    NdrContext* context, wStream* s, const void* hints, const void* v);            \
	extern const NdrMessageDescr ndr_##TYPE##VaryingArray_descr_s;                     \
	NdrMessageType ndr_##TYPE##VaryingArray_descr(void)

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct NdrContext_s NdrContext;

	typedef UINT32 ndr_refid;

	typedef BOOL (*NDR_READER_FN)(NdrContext* context, wStream* s, const void* hints, void* target);
	typedef BOOL (*NDR_WRITER_FN)(NdrContext* context, wStream* s, const void* hints,
	                              const void* obj);
	typedef void (*NDR_DESTROY_FN)(NdrContext* context, const void* hints, void* obj);
	typedef void (*NDR_DUMP_FN)(wLog* logger, UINT32 lvl, size_t indentLevel, const void* obj);

	/** @brief arity of a message */
	typedef enum
	{
		NDR_ARITY_SIMPLE,
		NDR_ARITY_ARRAYOF,
		NDR_ARITY_VARYING_ARRAYOF,
	} NdrTypeArity;

	/** @brief message descriptor */
	typedef struct
	{
		NdrTypeArity arity;
		size_t itemSize;
		WINPR_ATTR_NODISCARD NDR_READER_FN readFn;
		WINPR_ATTR_NODISCARD NDR_WRITER_FN writeFn;
		NDR_DESTROY_FN destroyFn;
		NDR_DUMP_FN dumpFn;
	} NdrMessageDescr;

	typedef const NdrMessageDescr* NdrMessageType;

	/** @brief pointer or not and if null is accepted */
	typedef enum
	{
		NDR_NOT_POINTER,
		NDR_POINTER_NON_NULL,
		NDR_POINTER
	} NdrPointerType;

	/** @brief descriptor of a field in a structure */
	typedef struct
	{
		const char* name;
		size_t structOffset;
		NdrPointerType pointerType;
		ssize_t hintsField;
		NdrMessageType typeDescr;
	} NdrFieldStruct;

	/** @brief structure descriptor */
	typedef struct
	{
		const char* name;
		size_t nfields;
		const NdrFieldStruct* fields;
	} NdrStructDescr;

	/** @brief a deferred pointer */
	typedef struct
	{
		ndr_refid ptrId;
		const char* name;
		void* hints;
		void* target;
		NdrMessageType msg;
	} NdrDeferredEntry;

	FREERDP_LOCAL void ndr_context_free(NdrContext* context);

	static inline void ndr_context_destroy(NdrContext** pcontext)
	{
		WINPR_ASSERT(pcontext);
		ndr_context_free(*pcontext);
		*pcontext = nullptr;
	}

	WINPR_ATTR_MALLOC(ndr_context_free, 1)
	WINPR_ATTR_NODISCARD
	FREERDP_LOCAL NdrContext* ndr_context_new(BOOL bigEndianDrep, BYTE version);

	FREERDP_LOCAL void ndr_context_reset(NdrContext* context);

	WINPR_ATTR_MALLOC(ndr_context_free, 1)
	WINPR_ATTR_NODISCARD
	FREERDP_LOCAL NdrContext* ndr_context_copy(const NdrContext* src);

	WINPR_ATTR_MALLOC(ndr_context_free, 1)
	WINPR_ATTR_NODISCARD
	FREERDP_LOCAL NdrContext* ndr_read_header(wStream* s);

	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_write_header(NdrContext* context, wStream* s);

	NDR_SIMPLE_TYPE_DECL(uint8, UINT8);
	NDR_SIMPLE_TYPE_DECL(uint16, UINT16);
	NDR_SIMPLE_TYPE_DECL(uint32, UINT32);
	NDR_SIMPLE_TYPE_DECL(uint64, UINT64);

	NDR_ARRAY_OF_TYPE_DECL(uint8, BYTE);
	NDR_ARRAY_OF_TYPE_DECL(uint16, UINT16);

	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_skip_bytes(NdrContext* context, wStream* s,
	                                                       size_t nbytes);

	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_read_align(NdrContext* context, wStream* s,
	                                                       size_t sz);

	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_write_align(NdrContext* context, wStream* s,
	                                                        size_t sz);

	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_write_data(NdrContext* context, wStream* s,
	                                                       const void* data, size_t sz);

	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_read_pickle(NdrContext* context, wStream* s);

	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_write_pickle(NdrContext* context, wStream* s);

	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_read_constructed(NdrContext* context, wStream* s,
	                                                             wStream* target);

	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_write_constructed(NdrContext* context, wStream* s,
	                                                              wStream* payload);

	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_start_constructed(NdrContext* context, wStream* s);

	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_end_constructed(NdrContext* context, wStream* s);

	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_read_wchar(NdrContext* context, wStream* s,
	                                                       WCHAR* ptr);

	/** @brief hints for a varying conformant array */
	typedef struct
	{
		UINT32 length;
		UINT32 maxLength;
	} NdrVaryingArrayHints;

	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_read_uconformant_varying_array(
	    NdrContext* context, wStream* s, const NdrVaryingArrayHints* hints, NdrMessageType itemType,
	    void* ptarget);

	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_write_uconformant_varying_array(
	    NdrContext* context, wStream* s, const NdrVaryingArrayHints* hints, NdrMessageType itemType,
	    const void* src);

	/** @brief hints for a conformant array */
	typedef struct
	{
		UINT32 count;
	} NdrArrayHints;

	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_read_uconformant_array(NdrContext* context,
	                                                                   wStream* s,
	                                                                   const NdrArrayHints* hints,
	                                                                   NdrMessageType itemType,
	                                                                   void* vtarget);

	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_write_uconformant_array(NdrContext* context,
	                                                                    wStream* s, UINT32 len,
	                                                                    NdrMessageType itemType,
	                                                                    const BYTE* ptr);

	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_struct_read_fromDescr(NdrContext* context,
	                                                                  wStream* s,
	                                                                  const NdrStructDescr* descr,
	                                                                  void* target);
	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_struct_write_fromDescr(NdrContext* context,
	                                                                   wStream* s,
	                                                                   const NdrStructDescr* descr,
	                                                                   const void* src);
	FREERDP_LOCAL
	void ndr_struct_dump_fromDescr(wLog* logger, UINT32 lvl, size_t identLevel,
	                               const NdrStructDescr* descr, const void* obj);
	FREERDP_LOCAL
	void ndr_struct_destroy(NdrContext* context, const NdrStructDescr* descr, void* pptr);

	WINPR_ATTR_NODISCARD FREERDP_LOCAL ndr_refid ndr_pointer_refid(const void* ptr);

	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_read_refpointer(NdrContext* context, wStream* s,
	                                                            UINT32* refId);

	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_context_allocatePtr(NdrContext* context,
	                                                                const void* ptr,
	                                                                ndr_refid* prefId,
	                                                                BOOL* pnewPtr);

	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_read_pointedMessageEx(NdrContext* context,
	                                                                  wStream* s, ndr_refid ptrId,
	                                                                  NdrMessageType descr,
	                                                                  void* hints, void** target);

	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_push_deferreds(NdrContext* context,
	                                                           NdrDeferredEntry* deferreds,
	                                                           size_t ndeferred);

	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_treat_deferred_read(NdrContext* context,
	                                                                wStream* s);

	WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL ndr_treat_deferred_write(NdrContext* context,
	                                                                 wStream* s);

#ifdef __cplusplus
}
#endif

#endif /* CHANNELS_RDPEAR_NDR_H_ */
