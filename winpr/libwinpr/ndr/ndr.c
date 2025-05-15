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
#include <winpr/assert.h>
#include <winpr/collections.h>
#include <winpr/wlog.h>
#include <winpr/ndr.h>

#include "../log.h"

#define TAG WINPR_TAG("ndr")

#define NDR_MAX_CONSTRUCTS 16
#define NDR_MAX_DEFERRED 50
#define NDR_MAX_STRUCT_DEFERRED 16

struct WinPrNdrEncoder_s
{
	BYTE version;
	BOOL bigEndianDrep;
	size_t alignBytes;

	int currentLevel;
	size_t indentLevels[16];

	int constructLevel;
	size_t constructs[NDR_MAX_CONSTRUCTS];

	wHashTable* refPointers;
	size_t ndeferred;
	WinPrNdrDeferredEntry deferred[NDR_MAX_DEFERRED];

	UINT32 refIdCounter;
};

struct WinPrNdrDecoder_s
{
	BYTE version;
	BOOL bigEndianDrep;
	size_t alignBytes;

	int currentLevel;
	size_t indentLevels[16];

	int constructLevel;
	size_t constructs[NDR_MAX_CONSTRUCTS];

	wHashTable* refPointers;
	size_t ndeferred;
	WinPrNdrDeferredEntry deferred[NDR_MAX_DEFERRED];
};

WinPrNdrEncoder* winpr_ndr_encoder_new(BOOL bigEndianDrep, BYTE version)
{
	WinPrNdrEncoder* ret = calloc(1, sizeof(*ret));
	if (!ret)
		return NULL;

	ret->version = version;
	ret->bigEndianDrep = bigEndianDrep;
	ret->alignBytes = 4;
	ret->refPointers = HashTable_New(FALSE);
	if (!ret->refPointers)
	{
		free(ret);
		return NULL;
	}

	ret->currentLevel = 0;
	ret->constructLevel = -1;
	memset(ret->indentLevels, 0, sizeof(ret->indentLevels));

	if (ret->refPointers)
		HashTable_Clear(ret->refPointers);
	ret->ndeferred = 0;
	ret->refIdCounter = 0x20000;
	return ret;
}

#if 0
void winpr_ndr_context_reset(WinPrNdrContext* context)
{
	WINPR_ASSERT(context);

	context->currentLevel = 0;
	context->constructLevel = -1;
	memset(context->indentLevels, 0, sizeof(context->indentLevels));

	if (context->refPointers)
		HashTable_Clear(context->refPointers);
	context->ndeferred = 0;
	context->refIdCounter = 0x20000;
}
#endif

WinPrNdrEncoder* winpr_ndr_encoder_from_decoder(const WinPrNdrDecoder* src)
{
	WINPR_ASSERT(src);

	return winpr_ndr_encoder_new(src->bigEndianDrep, src->version);
}

void winpr_ndr_encoder_free(WinPrNdrEncoder* context)
{
	if (context)
	{
		HashTable_Free(context->refPointers);
		free(context);
	}
}

void winpr_ndr_encoder_bytes_written(WinPrNdrEncoder* context, size_t len)
{
	context->indentLevels[context->currentLevel] += len;
}

BOOL winpr_ndr_encoder_write_header(WinPrNdrEncoder* context, wStream* s)
{
	WINPR_ASSERT(context);

	if (!Stream_EnsureRemainingCapacity(s, 8))
		return FALSE;

	Stream_Write_UINT8(s, context->version);
	Stream_Write_UINT8(s, context->bigEndianDrep ? 0x00 : 0x10);
	Stream_Write_UINT16(s, 0x8); /* header len */

	BYTE filler[] = { 0xcc, 0xcc, 0xcc, 0xcc };
	Stream_Write(s, filler, sizeof(filler));
	return TRUE;
}

BOOL winpr_ndr_encoder_write_align(WinPrNdrEncoder* context, wStream* s, size_t sz)
{
	WINPR_ASSERT(context);

	size_t rest = context->indentLevels[context->currentLevel] % sz;
	if (rest)
	{
		size_t padding = (sz - rest);

		if (!Stream_EnsureRemainingCapacity(s, padding))
			return FALSE;

		Stream_Zero(s, padding);
		context->indentLevels[context->currentLevel] += padding;
	}

	return TRUE;
}

BOOL winpr_ndr_encoder_write_pickle(WinPrNdrEncoder* context, wStream* s)
{
	WINPR_ASSERT(context);

	/* NDR format label */
	if (!winpr_ndr_encoder_write_uint32(context, s, 0x20000))
		return FALSE;

	winpr_ndr_encoder_write_uint32(context, s, 0); /* padding */
	return TRUE;
}

BOOL winpr_ndr_encoder_start_constructed(WinPrNdrEncoder* context, wStream* s)
{
	WINPR_ASSERT(context);

	if (!Stream_EnsureRemainingCapacity(s, 8))
		return FALSE;

	if (context->constructLevel == NDR_MAX_CONSTRUCTS)
		return FALSE;

	context->constructLevel++;
	context->constructs[context->constructLevel] = Stream_GetPosition(s);

	Stream_Zero(s, 8);
	return TRUE;
}

BOOL winpr_ndr_encoder_end_constructed(WinPrNdrEncoder* context, wStream* s)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->constructs);
	WINPR_ASSERT(context->constructLevel >= 0);

	size_t offset = context->constructs[context->constructLevel];

	wStream staticS = { 0 };
	Stream_StaticInit(&staticS, Stream_Buffer(s) + offset, 4);

	/* len */
	const size_t len = Stream_GetPosition(s) - (offset + 8);
	if (len > UINT32_MAX)
		return FALSE;
	if (!winpr_ndr_encoder_write_uint32(context, &staticS, (UINT32)len))
		return FALSE;

	return TRUE;
}

static size_t ndr_hintsCount(WinPrNdrMessageType msgType, const void* hints)
{
	WINPR_ASSERT(msgType);

	switch (msgType->arity)
	{
		case WINPR_NDR_ARITY_SIMPLE:
			return 1;
		case WINPR_NDR_ARITY_ARRAYOF:
			WINPR_ASSERT(hints);
			return ((const WinPrNdrArrayHints*)hints)->count;
		case WINPR_NDR_ARITY_VARYING_ARRAYOF:
			WINPR_ASSERT(hints);
			return ((const WinPrNdrVaryingArrayHints*)hints)->maxLength;
		default:
			WINPR_ASSERT(0 && "unknown arity");
			return 0;
	}
}

BOOL winpr_ndr_encoder_write_uint8(WinPrNdrEncoder* context, wStream* s, BYTE v)
{
	if (!Stream_EnsureRemainingCapacity(s, 1))
		return FALSE;

	Stream_Write_UINT8(s, v);
	winpr_ndr_encoder_bytes_written(context, 1);
	return TRUE;
}

BOOL winpr_ndr_encoder_write_uint8_(WinPrNdrEncoder* context, wStream* s, const void* hints,
                                    const void* v)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(v);
	WINPR_UNUSED(hints);

	return winpr_ndr_encoder_write_uint8(context, s, *(const BYTE*)v);
}

const WinPrNdrMessageDescr winpr_ndr_uint8_descr_s = {
	WINPR_NDR_ARITY_SIMPLE,         1,    winpr_ndr_decoder_read_uint8_,
	winpr_ndr_encoder_write_uint8_, NULL, NULL
};

WinPrNdrMessageType winpr_ndr_uint8_descr(void)
{
	return &winpr_ndr_uint8_descr_s;
}

#define SIMPLE_TYPE_IMPL(PREFIX, UPPERTYPE, LOWERTYPE)                                         \
	BOOL PREFIX##_decoder_read_##LOWERTYPE(WinPrNdrDecoder* context, wStream* s, UPPERTYPE* v) \
	{                                                                                          \
		WINPR_ASSERT(context);                                                                 \
                                                                                               \
		if (!Stream_CheckAndLogRequiredLength(TAG, s, sizeof(UPPERTYPE)))                      \
			return FALSE;                                                                      \
                                                                                               \
		if (!winpr_ndr_decoder_read_align(context, s, sizeof(UPPERTYPE)))                      \
			return FALSE;                                                                      \
                                                                                               \
		if (context->bigEndianDrep)                                                            \
			Stream_Read_##UPPERTYPE##_BE(s, *v);                                               \
		else                                                                                   \
			Stream_Read_##UPPERTYPE(s, *v);                                                    \
                                                                                               \
		winpr_ndr_decoder_bytes_read(context, sizeof(UPPERTYPE));                              \
		return TRUE;                                                                           \
	}                                                                                          \
                                                                                               \
	BOOL PREFIX##_decoder_read_##LOWERTYPE##_(WinPrNdrDecoder* context, wStream* s,            \
	                                          const void* hints, void* v)                      \
	{                                                                                          \
		WINPR_UNUSED(hints);                                                                   \
		return PREFIX##_decoder_read_##LOWERTYPE(context, s, (UPPERTYPE*)v);                   \
	}                                                                                          \
                                                                                               \
	BOOL PREFIX##_encoder_write_##LOWERTYPE(WinPrNdrEncoder* context, wStream* s, UPPERTYPE v) \
	{                                                                                          \
		if (!winpr_ndr_encoder_write_align(context, s, sizeof(UPPERTYPE)) ||                   \
		    !Stream_EnsureRemainingCapacity(s, sizeof(UPPERTYPE)))                             \
			return FALSE;                                                                      \
                                                                                               \
		if (context->bigEndianDrep)                                                            \
			Stream_Write_##UPPERTYPE##_BE(s, v);                                               \
		else                                                                                   \
			Stream_Write_##UPPERTYPE(s, v);                                                    \
                                                                                               \
		winpr_ndr_encoder_bytes_written(context, sizeof(UPPERTYPE));                           \
		return TRUE;                                                                           \
	}                                                                                          \
                                                                                               \
	BOOL PREFIX##_encoder_write_##LOWERTYPE##_(WinPrNdrEncoder* context, wStream* s,           \
	                                           const void* hints, const void* v)               \
	{                                                                                          \
		WINPR_ASSERT(context);                                                                 \
		WINPR_ASSERT(s);                                                                       \
		WINPR_ASSERT(v);                                                                       \
		WINPR_UNUSED(hints);                                                                   \
                                                                                               \
		return PREFIX##_encoder_write_##LOWERTYPE(context, s, *(const UPPERTYPE*)v);           \
	}                                                                                          \
                                                                                               \
	const WinPrNdrMessageDescr PREFIX##_##LOWERTYPE##_descr_s = {                              \
		WINPR_NDR_ARITY_SIMPLE,                                                                \
		sizeof(UPPERTYPE),                                                                     \
		PREFIX##_decoder_read_##LOWERTYPE##_,                                                  \
		PREFIX##_encoder_write_##LOWERTYPE##_,                                                 \
		NULL,                                                                                  \
		NULL                                                                                   \
	};                                                                                         \
                                                                                               \
	WinPrNdrMessageType PREFIX##_##LOWERTYPE##_descr(void)                                     \
	{                                                                                          \
		return &PREFIX##_##LOWERTYPE##_descr_s;                                                \
	}

SIMPLE_TYPE_IMPL(winpr_ndr, UINT32, uint32)
SIMPLE_TYPE_IMPL(winpr_ndr, UINT16, uint16)
SIMPLE_TYPE_IMPL(winpr_ndr, UINT64, uint64)

#define ARRAY_OF_TYPE_IMPL(TYPE, UPPERTYPE)                                                      \
	BOOL winpr_ndr_decoder_read_##TYPE##Array(WinPrNdrDecoder* context, wStream* s,              \
	                                          const void* hints, void* v)                        \
	{                                                                                            \
		WINPR_ASSERT(context);                                                                   \
		WINPR_ASSERT(s);                                                                         \
		WINPR_ASSERT(hints);                                                                     \
		return winpr_ndr_decoder_read_uconformant_array(context, s, hints,                       \
		                                                winpr_ndr_##TYPE##_descr(), v);          \
	}                                                                                            \
                                                                                                 \
	BOOL winpr_ndr_encoder_write_##TYPE##Array(WinPrNdrEncoder* context, wStream* s,             \
	                                           const void* hints, const void* v)                 \
	{                                                                                            \
		WINPR_ASSERT(context);                                                                   \
		WINPR_ASSERT(s);                                                                         \
		WINPR_ASSERT(hints);                                                                     \
		const WinPrNdrArrayHints* ahints = (const WinPrNdrArrayHints*)hints;                     \
		return winpr_ndr_encoder_write_uconformant_array(context, s, ahints->count,              \
		                                                 winpr_ndr_##TYPE##_descr(), v);         \
	}                                                                                            \
	void winpr_ndr_destroy_##TYPE##Array(const void* hints, void* obj)                           \
	{                                                                                            \
		WINPR_ASSERT(obj);                                                                       \
		WINPR_ASSERT(hints);                                                                     \
		const WinPrNdrArrayHints* ahints = (const WinPrNdrArrayHints*)hints;                     \
		WinPrNdrMessageType descr = winpr_ndr_##TYPE##_descr();                                  \
		if (descr->destroyFn)                                                                    \
		{                                                                                        \
			UPPERTYPE* ptr = (UPPERTYPE*)obj;                                                    \
			for (UINT32 i = 0; i < ahints->count; i++, ptr++)                                    \
				descr->destroyFn(NULL, ptr);                                                     \
		}                                                                                        \
	}                                                                                            \
                                                                                                 \
	const WinPrNdrMessageDescr winpr_ndr_##TYPE##Array_descr_s = {                               \
		WINPR_NDR_ARITY_ARRAYOF,                                                                 \
		sizeof(UPPERTYPE),                                                                       \
		winpr_ndr_decoder_read_##TYPE##Array,                                                    \
		winpr_ndr_encoder_write_##TYPE##Array,                                                   \
		winpr_ndr_destroy_##TYPE##Array,                                                         \
		NULL                                                                                     \
	};                                                                                           \
                                                                                                 \
	WinPrNdrMessageType winpr_ndr_##TYPE##Array_descr(void)                                      \
	{                                                                                            \
		return &winpr_ndr_##TYPE##Array_descr_s;                                                 \
	}                                                                                            \
                                                                                                 \
	BOOL winpr_ndr_decoder_read_##TYPE##VaryingArray(WinPrNdrDecoder* context, wStream* s,       \
	                                                 const void* hints, void* v)                 \
	{                                                                                            \
		WINPR_ASSERT(context);                                                                   \
		WINPR_ASSERT(s);                                                                         \
		WINPR_ASSERT(hints);                                                                     \
		return winpr_ndr_decoder_read_uconformant_varying_array(                                 \
		    context, s, (const WinPrNdrVaryingArrayHints*)hints, winpr_ndr_##TYPE##_descr(), v); \
	}                                                                                            \
	BOOL winpr_ndr_encoder_write_##TYPE##VaryingArray(WinPrNdrEncoder* context, wStream* s,      \
	                                                  const void* hints, const void* v)          \
	{                                                                                            \
		WINPR_ASSERT(context);                                                                   \
		WINPR_ASSERT(s);                                                                         \
		WINPR_ASSERT(hints);                                                                     \
		return winpr_ndr_encoder_write_uconformant_varying_array(                                \
		    context, s, (const WinPrNdrVaryingArrayHints*)hints, winpr_ndr_##TYPE##_descr(), v); \
	}                                                                                            \
                                                                                                 \
	const WinPrNdrMessageDescr winpr_ndr_##TYPE##VaryingArray_descr_s = {                        \
		WINPR_NDR_ARITY_VARYING_ARRAYOF,                                                         \
		sizeof(UPPERTYPE),                                                                       \
		winpr_ndr_decoder_read_##TYPE##VaryingArray,                                             \
		winpr_ndr_encoder_write_##TYPE##VaryingArray,                                            \
		NULL,                                                                                    \
		NULL                                                                                     \
	};                                                                                           \
                                                                                                 \
	WinPrNdrMessageType winpr_ndr_##TYPE##VaryingArray_descr(void)                               \
	{                                                                                            \
		return &winpr_ndr_##TYPE##VaryingArray_descr_s;                                          \
	}

ARRAY_OF_TYPE_IMPL(uint8, BYTE)
ARRAY_OF_TYPE_IMPL(uint16, UINT16)

BOOL winpr_ndr_encoder_write_uconformant_varying_array(WinPrNdrEncoder* context, wStream* s,
                                                       const WinPrNdrVaryingArrayHints* hints,
                                                       WinPrNdrMessageType itemType,
                                                       const void* psrc)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(hints);
	WINPR_ASSERT(itemType);
	WINPR_ASSERT(psrc);

	if (!winpr_ndr_encoder_write_uint32(context, s, hints->maxLength) ||
	    !winpr_ndr_encoder_write_uint32(context, s, 0) ||
	    !winpr_ndr_encoder_write_uint32(context, s, hints->length))
		return FALSE;

	const BYTE* src = (const BYTE*)psrc;
	for (UINT32 i = 0; i < hints->length; i++, src += itemType->itemSize)
	{
		if (!itemType->writeFn(context, s, NULL, src))
			return FALSE;
	}

	return TRUE;
}

BOOL winpr_ndr_encoder_write_uconformant_array(WinPrNdrEncoder* context, wStream* s, UINT32 len,
                                               WinPrNdrMessageType itemType, const BYTE* ptr)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(itemType);
	WINPR_ASSERT(ptr);

	size_t toWrite = len * itemType->itemSize;
	size_t padding = (4 - (toWrite % 4)) % 4;
	if (!winpr_ndr_encoder_write_uint32(context, s, len) ||
	    !Stream_EnsureRemainingCapacity(s, toWrite + padding))
		return FALSE;

	for (UINT32 i = 0; i < len; i++, ptr += itemType->itemSize)
	{
		if (!itemType->writeFn(context, s, NULL, ptr))
			return FALSE;
	}

	if (padding)
	{
		Stream_Zero(s, padding);
		winpr_ndr_encoder_bytes_written(context, padding);
	}
	return TRUE;
}

BOOL winpr_ndr_encoder_write_fromDescr(WinPrNdrEncoder* context, wStream* s,
                                       const WinPrNdrStructDescr* descr, const void* src)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(descr);
	WINPR_ASSERT(src);

	WinPrNdrDeferredEntry deferreds[NDR_MAX_STRUCT_DEFERRED] = { 0 };
	size_t ndeferred = 0;

	for (size_t i = 0; i < descr->nfields; i++)
	{
		const WinPrNdrFieldStruct* field = &descr->fields[i];
		const BYTE* ptr = (const BYTE*)src + field->structOffset;

		const void* hints = NULL;

		if (field->hintsField >= 0)
		{
			/* computes the address of the hints field if any */
			WINPR_ASSERT((size_t)field->hintsField < descr->nfields);
			const WinPrNdrFieldStruct* hintsField = &descr->fields[field->hintsField];

			hints = (const BYTE*)src + hintsField->structOffset;
		}

		switch (field->pointerType)
		{
			case WINPR_NDR_POINTER:
			case WINPR_NDR_POINTER_NON_NULL:
			{
				winpr_ndr_refid ptrId = WINPR_NDR_PTR_NULL;
				BOOL isNew = 0;
				ptr = *(WINPR_CAST_CONST_PTR_AWAY(ptr, const void**));

				if (!ptr && field->pointerType == WINPR_NDR_POINTER_NON_NULL)
				{
					WLog_ERR(TAG, "%s.%s can't be null", descr->name, field->name);
					return FALSE;
				}

				if (!winpr_ndr_encoder_allocatePtr(context, ptr, &ptrId, &isNew))
					return FALSE;

				if (isNew)
				{
					WinPrNdrDeferredEntry* deferred = &deferreds[ndeferred];
					if (ndeferred >= NDR_MAX_STRUCT_DEFERRED)
					{
						WLog_ERR(TAG,
						         "too many deferred when calling winpr_ndr_encoder_write_fromDescr "
						         "for %s",
						         descr->name);
						return FALSE;
					}

					deferred->name = field->name;
					deferred->hints = WINPR_CAST_CONST_PTR_AWAY(hints, void*);
					deferred->target = WINPR_CAST_CONST_PTR_AWAY(ptr, void*);
					deferred->msg = field->typeDescr;
					ndeferred++;
				}

				if (!winpr_ndr_encoder_write_uint32(context, s, ptrId))
					return FALSE;
				break;
			}
			case WINPR_NDR_NOT_POINTER:
				if (!field->typeDescr->writeFn(context, s, hints, ptr))
				{
					WLog_ERR(TAG, "error when writing %s.%s", descr->name, field->name);
					return FALSE;
				}
				break;
			default:
				break;
		}
	}

	return winpr_ndr_encoder_push_deferreds(context, deferreds, ndeferred);
}

winpr_ndr_refid winpr_ndr_pointer_refid(const void* ptr)
{
	return (winpr_ndr_refid)((ULONG_PTR)ptr);
}

typedef struct
{
	const void* needle;
	winpr_ndr_refid* presult;
} FindValueArgs;

static BOOL findValueRefFn(const void* key, void* value, void* parg)
{
	WINPR_ASSERT(parg);

	FindValueArgs* args = (FindValueArgs*)parg;
	if (args->needle == value)
	{
		*args->presult = (winpr_ndr_refid)(UINT_PTR)key;
		return FALSE;
	}
	return TRUE;
}

BOOL winpr_ndr_encoder_allocatePtr(WinPrNdrEncoder* context, const void* ptr,
                                   winpr_ndr_refid* prefId, BOOL* pnewPtr)
{
	WINPR_ASSERT(context);

	FindValueArgs findArgs = { ptr, prefId };
	if (!HashTable_Foreach(context->refPointers, findValueRefFn, &findArgs))
	{
		*pnewPtr = FALSE;
		return TRUE;
	}

	*pnewPtr = TRUE;
	*prefId = context->refIdCounter + 4;
	if (!HashTable_Insert(context->refPointers, (void*)(UINT_PTR)(*prefId), ptr))
		return FALSE;

	context->refIdCounter += 4;
	return TRUE;
}

BOOL winpr_ndr_encoder_push_deferreds(WinPrNdrEncoder* context, WinPrNdrDeferredEntry* deferreds,
                                      size_t ndeferred)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(deferreds);

	if (!ndeferred)
		return TRUE;

	if (context->ndeferred + ndeferred > NDR_MAX_DEFERRED)
	{
		WLog_ERR(TAG, "too many deferred");
		return FALSE;
	}

	for (size_t i = ndeferred; i > 0; i--, context->ndeferred++)
	{
		context->deferred[context->ndeferred] = deferreds[i - 1];
	}
	return TRUE;
}

BOOL winpr_ndr_encoder_treat_deferreds(WinPrNdrEncoder* context, wStream* s)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);

	while (context->ndeferred)
	{
		WinPrNdrDeferredEntry current = context->deferred[context->ndeferred - 1];
		context->ndeferred--;

		WLog_VRB(TAG, "treating write deferred for %s", current.name);
		if (!current.msg->writeFn(context, s, current.hints, current.target))
		{
			WLog_ERR(TAG, "error writing deferred %s", current.name);
			return FALSE;
		}
	}

	return TRUE;
}

BOOL winpr_ndr_encoder_write_data(WinPrNdrEncoder* context, wStream* s, const void* data, size_t sz)
{
	if (!Stream_EnsureRemainingCapacity(s, sz))
		return FALSE;

	Stream_Write(s, data, sz);
	winpr_ndr_encoder_bytes_written(context, sz);
	return TRUE;
}

/* ====================================================================================== */

WinPrNdrDecoder* winpr_ndr_decoder_new(BOOL bigEndianDrep, BYTE version)
{
	WinPrNdrDecoder* ret = calloc(1, sizeof(*ret));
	if (!ret)
		return NULL;

	ret->version = version;
	ret->bigEndianDrep = bigEndianDrep;
	ret->alignBytes = 4;
	ret->refPointers = HashTable_New(FALSE);
	if (!ret->refPointers)
	{
		free(ret);
		return NULL;
	}

	ret->currentLevel = 0;
	ret->constructLevel = -1;
	memset(ret->indentLevels, 0, sizeof(ret->indentLevels));

	if (ret->refPointers)
		HashTable_Clear(ret->refPointers);
	ret->ndeferred = 0;
	return ret;
}

WinPrNdrDecoder* winpr_ndr_decoder_new_fromStream(wStream* s)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return NULL;

	BYTE version = Stream_Get_UINT8(s);
	BYTE drep = Stream_Get_UINT8(s);
	UINT16 headerLen = Stream_Get_UINT16(s);

	if (headerLen < 4 || !Stream_CheckAndLogRequiredLength(TAG, s, headerLen - 4))
		return NULL;

	/* skip filler */
	Stream_Seek(s, headerLen - 4);

	return winpr_ndr_decoder_new((drep != 0x10), version);
}

void winpr_ndr_decoder_free(WinPrNdrDecoder* context)
{
	if (context)
	{
		HashTable_Free(context->refPointers);
		free(context);
	}
}

void winpr_ndr_decoder_bytes_read(WinPrNdrDecoder* context, size_t len)
{
	WINPR_ASSERT(context);
	context->indentLevels[context->currentLevel] += len;
}

BOOL winpr_ndr_decoder_skip_bytes(WinPrNdrDecoder* context, wStream* s, size_t nbytes)
{
	WINPR_ASSERT(context);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, nbytes))
		return FALSE;

	context->indentLevels[context->currentLevel] += nbytes;
	Stream_Seek(s, nbytes);
	return TRUE;
}

BOOL winpr_ndr_decoder_read_align(WinPrNdrDecoder* context, wStream* s, size_t sz)
{
	WINPR_ASSERT(context);

	size_t rest = context->indentLevels[context->currentLevel] % sz;
	if (rest)
	{
		size_t padding = (sz - rest);
		if (!Stream_CheckAndLogRequiredLength(TAG, s, padding))
			return FALSE;

		Stream_Seek(s, padding);
		context->indentLevels[context->currentLevel] += padding;
	}

	return TRUE;
}

BOOL winpr_ndr_decoder_read_pickle(WinPrNdrDecoder* context, wStream* s)
{
	WINPR_ASSERT(context);

	UINT32 v = 0;

	/* NDR format label */
	if (!winpr_ndr_decoder_read_uint32(context, s, &v) || v != 0x20000)
		return FALSE;

	return winpr_ndr_decoder_read_uint32(context, s, &v); // padding
}

BOOL winpr_ndr_decoder_read_refpointer(WinPrNdrDecoder* context, wStream* s, winpr_ndr_refid* refId)
{
	return winpr_ndr_decoder_read_uint32(context, s, refId);
}

BOOL winpr_ndr_decoder_read_constructed(WinPrNdrDecoder* context, wStream* s, wStream* target)
{
	WINPR_ASSERT(context);

	UINT32 len = 0;

	/* len */
	if (!winpr_ndr_decoder_read_uint32(context, s, &len))
		return FALSE;

	/* padding */
	if (!winpr_ndr_decoder_skip_bytes(context, s, 4))
		return FALSE;

	/* payload */
	if (!Stream_CheckAndLogRequiredLength(TAG, s, len))
		return FALSE;

	Stream_StaticInit(target, Stream_PointerAs(s, BYTE), len);
	Stream_Seek(s, len);
	return TRUE;
}

BOOL winpr_ndr_decoder_read_uint8(WinPrNdrDecoder* context, wStream* s, BYTE* v)
{
	WINPR_ASSERT(context);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return FALSE;

	Stream_Read_UINT8(s, *v);

	winpr_ndr_decoder_bytes_read(context, 1);
	return TRUE;
}

BOOL winpr_ndr_decoder_read_uint8_(WinPrNdrDecoder* context, wStream* s, const void* hints, void* v)
{
	WINPR_UNUSED(hints);
	return winpr_ndr_decoder_read_uint8(context, s, (BYTE*)v);
}

BOOL winpr_ndr_decoder_read_wchar(WinPrNdrDecoder* context, wStream* s, WCHAR* ptr)
{
	return winpr_ndr_decoder_read_uint16(context, s, (UINT16*)ptr);
}

BOOL winpr_ndr_decoder_read_uconformant_varying_array(WinPrNdrDecoder* context, wStream* s,
                                                      const WinPrNdrVaryingArrayHints* hints,
                                                      WinPrNdrMessageType itemType, void* ptarget)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(hints);
	WINPR_ASSERT(itemType);
	WINPR_ASSERT(ptarget);

	UINT32 maxCount = 0;
	UINT32 offset = 0;
	UINT32 length = 0;

	if (!winpr_ndr_decoder_read_uint32(context, s, &maxCount) ||
	    !winpr_ndr_decoder_read_uint32(context, s, &offset) ||
	    !winpr_ndr_decoder_read_uint32(context, s, &length))
		return FALSE;

	if ((length * itemType->itemSize) < hints->length)
		return FALSE;

	if ((maxCount * itemType->itemSize) < hints->maxLength)
		return FALSE;

	BYTE* target = (BYTE*)ptarget;
	for (UINT32 i = 0; i < length; i++, target += itemType->itemSize)
	{
		if (!itemType->readFn(context, s, NULL, target))
			return FALSE;
	}

	return winpr_ndr_decoder_read_align(context, s, 4);
}

BOOL winpr_ndr_decoder_read_uconformant_array(WinPrNdrDecoder* context, wStream* s,
                                              const WinPrNdrArrayHints* hints,
                                              WinPrNdrMessageType itemType, void* vtarget)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(itemType);
	WINPR_ASSERT(vtarget);

	UINT32 count = 0;

	if (!winpr_ndr_decoder_read_uint32(context, s, &count))
		return FALSE;

	if ((count * itemType->itemSize < hints->count))
		return FALSE;

	BYTE* target = (BYTE*)vtarget;
	for (UINT32 i = 0; i < count; i++, target += itemType->itemSize)
	{
		if (!itemType->readFn(context, s, NULL, target))
			return FALSE;
	}

	return winpr_ndr_decoder_read_align(context, s, /*context->alignBytes*/ 4);
}

BOOL winpr_ndr_decoder_read_fromDescr(WinPrNdrDecoder* context, wStream* s,
                                      const WinPrNdrStructDescr* descr, void* target)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(descr);
	WINPR_ASSERT(target);

	WinPrNdrDeferredEntry deferreds[NDR_MAX_STRUCT_DEFERRED] = { 0 };
	size_t ndeferred = 0;

	for (size_t i = 0; i < descr->nfields; i++)
	{
		const WinPrNdrFieldStruct* field = &descr->fields[i];
		BYTE* ptr = target;
		ptr += field->structOffset;
		void* hints = NULL;

		if (field->hintsField >= 0)
		{
			/* computes the address of the hints field if any */
			WINPR_ASSERT((size_t)field->hintsField < descr->nfields);
			const WinPrNdrFieldStruct* hintsField = &descr->fields[field->hintsField];

			hints = (BYTE*)target + hintsField->structOffset;
		}

		switch (field->pointerType)
		{
			case WINPR_NDR_NOT_POINTER:
				if (!field->typeDescr->readFn(context, s, hints, ptr))
				{
					WLog_ERR(TAG, "error when reading %s.%s", descr->name, field->name);
					return FALSE;
				}
				break;
			case WINPR_NDR_POINTER:
			case WINPR_NDR_POINTER_NON_NULL:
			{
				WinPrNdrDeferredEntry* deferred = &deferreds[ndeferred];
				if (ndeferred >= NDR_MAX_STRUCT_DEFERRED)
				{
					WLog_ERR(
					    TAG,
					    "too many deferred when calling winpr_ndr_decoder_read_fromDescr for %s",
					    descr->name);
					return FALSE;
				}

				deferred->name = field->name;
				deferred->hints = hints;
				deferred->target = ptr;
				deferred->msg = field->typeDescr;
				if (!winpr_ndr_decoder_read_refpointer(context, s, &deferred->ptrId))
				{
					WLog_ERR(TAG, "error when reading %s.%s", descr->name, field->name);
					return FALSE;
				}

				if (!deferred->ptrId && field->pointerType == WINPR_NDR_POINTER_NON_NULL)
				{
					WLog_ERR(TAG, "%s.%s can't be null", descr->name, field->name);
					return FALSE;
				}
				ndeferred++;
				break;
			}
			default:
				WLog_ERR(TAG, "%s.%s unknown pointer type 0x%x", descr->name, field->name,
				         field->pointerType);
				return FALSE;
		}
	}

	return winpr_ndr_decoder_push_deferreds(context, deferreds, ndeferred);
}

BOOL winpr_ndr_decoder_treat_deferreds(WinPrNdrDecoder* context, wStream* s)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);

	while (context->ndeferred)
	{
		WinPrNdrDeferredEntry current = context->deferred[context->ndeferred - 1];
		context->ndeferred--;

		WLog_VRB(TAG, "treating read deferred 0x%x for %s", current.ptrId, current.name);
		if (!winpr_ndr_decoder_read_pointedMessageEx(context, s, current.ptrId, current.msg,
		                                             current.hints, (void**)current.target))
		{
			WLog_ERR(TAG, "error parsing deferred %s", current.name);
			return FALSE;
		}
	}

	return TRUE;
}

BOOL winpr_ndr_decoder_read_pointedMessageEx(WinPrNdrDecoder* context, wStream* s,
                                             winpr_ndr_refid ptrId, WinPrNdrMessageType descr,
                                             void* hints, void** target)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(descr);
	WINPR_ASSERT(target);

	*target = NULL;
	if (!ptrId)
		return TRUE;

	void* ret = HashTable_GetItemValue(context->refPointers, (void*)(UINT_PTR)ptrId);
	if (!ret)
	{
		size_t itemCount = ndr_hintsCount(descr, hints);
		ret = calloc(itemCount, descr->itemSize);
		if (!ret)
			return FALSE;

		if (!descr->readFn(context, s, hints, ret) ||
		    !HashTable_Insert(context->refPointers, (void*)(UINT_PTR)ptrId, ret))
		{
			if (descr->destroyFn)
				descr->destroyFn(hints, ret);
			free(ret);
			return FALSE;
		}
	}

	*target = ret;
	return TRUE;
}

BOOL winpr_ndr_decoder_push_deferreds(WinPrNdrDecoder* context, WinPrNdrDeferredEntry* deferreds,
                                      size_t ndeferred)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(deferreds);

	if (!ndeferred)
		return TRUE;

	if (context->ndeferred + ndeferred > NDR_MAX_DEFERRED)
	{
		WLog_ERR(TAG, "too many deferred");
		return FALSE;
	}

	for (size_t i = ndeferred; i > 0; i--, context->ndeferred++)
	{
		context->deferred[context->ndeferred] = deferreds[i - 1];
	}
	return TRUE;
}

/* ========================================================================================================
 */

void winpr_ndr_struct_dump_fromDescr(wLog* logger, UINT32 lvl, size_t identLevel,
                                     const WinPrNdrStructDescr* descr, const void* obj)
{
	char tabArray[30 + 1];
	size_t ntabs = (identLevel <= 30) ? identLevel : 30;

	memset(tabArray, '\t', ntabs);
	tabArray[ntabs] = 0;

	WLog_Print(logger, lvl, "%s%s", tabArray, descr->name);
	for (size_t i = 0; i < descr->nfields; i++)
	{
		const WinPrNdrFieldStruct* field = &descr->fields[i];
		const BYTE* ptr = (const BYTE*)obj + field->structOffset;

		switch (field->pointerType)
		{
			case WINPR_NDR_POINTER:
			case WINPR_NDR_POINTER_NON_NULL:
				ptr = *(WINPR_CAST_CONST_PTR_AWAY(ptr, const void**));
				break;
			case WINPR_NDR_NOT_POINTER:
				break;
			default:
				WLog_ERR(TAG, "invalid field->pointerType");
				break;
		}

		WLog_Print(logger, lvl, "%s*%s:", tabArray, field->name);
		if (field->typeDescr->dumpFn)
			field->typeDescr->dumpFn(logger, lvl, identLevel + 1, ptr);
		else
			WLog_Print(logger, lvl, "%s\t<no dump function>", tabArray);
	}
}

void winpr_ndr_struct_destroy(const WinPrNdrStructDescr* descr, void* pptr)
{
	WINPR_ASSERT(descr);
	WINPR_ASSERT(pptr);

	for (size_t i = 0; i < descr->nfields; i++)
	{
		const WinPrNdrFieldStruct* field = &descr->fields[i];
		void* ptr = (BYTE*)pptr + field->structOffset;
		void* hints = NULL;

		if (field->hintsField >= 0)
		{
			/* computes the address of the hints field if any */
			WINPR_ASSERT((size_t)field->hintsField < descr->nfields);
			const WinPrNdrFieldStruct* hintsField = &descr->fields[field->hintsField];

			hints = (BYTE*)pptr + hintsField->structOffset;
		}

		if (field->pointerType != WINPR_NDR_NOT_POINTER)
			ptr = *(void**)ptr;

		if (ptr && field->typeDescr->destroyFn)
			field->typeDescr->destroyFn(hints, ptr);

		if (field->pointerType != WINPR_NDR_NOT_POINTER)
			free(ptr);
	}
}
