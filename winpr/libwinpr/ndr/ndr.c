/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * NDR decoder / encoder
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
#include <stddef.h>

#include <winpr/assert.h>
#include <winpr/collections.h>
#include <winpr/interlocked.h>
#include <winpr/wlog.h>
#include <winpr/string.h>
#include <winpr/ndr.h>

#include "../log.h"

#define TAG WINPR_TAG("ndr")

#define NDR_MAX_DEFERRED 128

/** @brief pending deferred pointers
 *
 * NDR writes the referents of embedded pointers after the construct (structure, array or
 * referent) that contains the pointers, in the order of the pointers, each referent being
 * followed by its own referents.
 *
 * While a construct is walked the deferred pointers are collected in a block. When the
 * outermost construct is done the block is pushed on a stack in reverse order: popping the stack
 * treats the entries in order, and the entries pushed while treating one are treated before the
 * next one.
 */
typedef struct
{
	size_t nstack;
	WinPrNdrDeferredEntry stack[NDR_MAX_DEFERRED];

	size_t depth;
	size_t nblock;
	WinPrNdrDeferredEntry block[NDR_MAX_DEFERRED];
} NdrDeferreds;

struct WinPrNdrEncoder_s
{
	BYTE version;
	BOOL bigEndianDrep;

	size_t offset; /** bytes written in the current object, for the alignments */
	BOOL inObject;
	size_t objectStart; /** stream position of the private header of the current object */

	UINT32 refIdCounter;
	NdrDeferreds deferreds;
};

struct WinPrNdrDecoder_s
{
	BYTE version;
	BOOL bigEndianDrep;

	size_t offset; /** bytes read in the current object, for the alignments */

	wHashTable* referents; /** referent id => NdrReferent */
	NdrDeferreds deferreds;
};

#define NDR_REFERENT_MAGIC 0x46455244 /* "DREF" */

/** @brief a reference counted referent, the referent data follows the header
 *
 * All the pointers returned by the decoder are allocated this way, they are released with
 * winpr_ndr_pointer_release().
 */
typedef struct
{
	UINT32 magic;
	winpr_ndr_refid refId;
	BOOL full; /** referent of full pointers, that can be aliased */
	volatile LONG refCount;
	WinPrNdrMessageType typeDescr;
	size_t size; /** size of the referent */
	BOOL hasHints;
	union
	{
		WinPrNdrArrayHints array;
		WinPrNdrVaryingArrayHints varying;
	} hints; /** copy of the hints, the destroy function of the type needs them */
	union
	{
		/* max_align_t is not available with all the compilers (MSVC C) */
		UINT64 u64;
		double d;
		long double ld;
		void* ptr;
		BYTE data[1];
	} payload;
} NdrReferent;

static NdrReferent* ndr_referent(void* ptr)
{
	WINPR_ASSERT(ptr);

	/* computed on the address: the referent header precedes the payload */
	NdrReferent* ref = (NdrReferent*)((uintptr_t)ptr - offsetof(NdrReferent, payload));
	WINPR_ASSERT(ref->magic == NDR_REFERENT_MAGIC);
	return ref;
}

static void ndr_deferreds_enter(NdrDeferreds* d)
{
	WINPR_ASSERT(d);
	d->depth++;
}

static BOOL ndr_deferreds_add(NdrDeferreds* d, const WinPrNdrDeferredEntry* entry)
{
	WINPR_ASSERT(d);
	WINPR_ASSERT(d->depth);
	WINPR_ASSERT(entry);

	if (d->nblock == NDR_MAX_DEFERRED)
	{
		WLog_ERR(TAG, "too many deferred pointers when adding %s", entry->name);
		return FALSE;
	}

	d->block[d->nblock++] = *entry;
	return TRUE;
}

static BOOL ndr_deferreds_leave(NdrDeferreds* d, BOOL success)
{
	WINPR_ASSERT(d);
	WINPR_ASSERT(d->depth);

	d->depth--;
	if (d->depth)
		return success;

	const size_t nblock = d->nblock;
	d->nblock = 0;
	if (!success)
		return FALSE;

	if (d->nstack + nblock > NDR_MAX_DEFERRED)
	{
		WLog_ERR(TAG, "too many deferred pointers");
		return FALSE;
	}

	for (size_t i = nblock; i > 0; i--)
		d->stack[d->nstack++] = d->block[i - 1];
	return TRUE;
}

static BOOL ndr_deferreds_pop(NdrDeferreds* d, WinPrNdrDeferredEntry* entry)
{
	WINPR_ASSERT(d);
	WINPR_ASSERT(entry);

	if (!d->nstack)
		return FALSE;

	d->nstack--;
	*entry = d->stack[d->nstack];
	return TRUE;
}

/** @brief computes the address of the hints of a field, if any */
static void* ndr_field_hints(const WinPrNdrStructDescr* descr, const WinPrNdrFieldStruct* field,
                             const void* obj)
{
	if (field->hintsField < 0)
		return nullptr;

	WINPR_ASSERT((size_t)field->hintsField < descr->nfields);
	const WinPrNdrFieldStruct* hintsField = &descr->fields[field->hintsField];
	const BYTE* hints = (const BYTE*)obj + hintsField->structOffset;
	return WINPR_CAST_CONST_PTR_AWAY(hints, void*);
}

WinPrNdrEncoder* winpr_ndr_encoder_new(BOOL bigEndianDrep, BYTE version)
{
	WinPrNdrEncoder* ret = calloc(1, sizeof(*ret));
	if (!ret)
		return nullptr;

	ret->version = version;
	ret->bigEndianDrep = bigEndianDrep;
	ret->refIdCounter = 0x20000;
	return ret;
}

WinPrNdrEncoder* winpr_ndr_encoder_from_decoder(const WinPrNdrDecoder* src)
{
	WINPR_ASSERT(src);

	return winpr_ndr_encoder_new(src->bigEndianDrep, src->version);
}

void winpr_ndr_encoder_free(WinPrNdrEncoder* context)
{
	free(context);
}

void winpr_ndr_encoder_bytes_written(WinPrNdrEncoder* context, size_t len)
{
	WINPR_ASSERT(context);
	context->offset += len;
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

	size_t rest = context->offset % sz;
	if (rest)
	{
		size_t padding = (sz - rest);

		if (!Stream_EnsureRemainingCapacity(s, padding))
			return FALSE;

		Stream_Zero(s, padding);
		context->offset += padding;
	}

	return TRUE;
}

BOOL winpr_ndr_encoder_start_object(WinPrNdrEncoder* context, wStream* s)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);

	if (context->inObject)
	{
		WLog_ERR(TAG, "an object is already being serialized");
		return FALSE;
	}

	if (!Stream_EnsureRemainingCapacity(s, 8))
		return FALSE;

	/* ObjectBufferLength, filled by winpr_ndr_encoder_end_object, and Filler */
	context->objectStart = Stream_GetPosition(s);
	Stream_Zero(s, 8);

	context->inObject = TRUE;
	context->offset = 0;
	return TRUE;
}

BOOL winpr_ndr_encoder_end_object(WinPrNdrEncoder* context, wStream* s)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);

	if (!context->inObject)
	{
		WLog_ERR(TAG, "no object being serialized");
		return FALSE;
	}

	const size_t start = context->objectStart + 8;
	WINPR_ASSERT(Stream_GetPosition(s) >= start);

	/* the length of the object buffer must be a multiple of 8 */
	const size_t padding = (8 - ((Stream_GetPosition(s) - start) % 8)) % 8;
	if (!Stream_EnsureRemainingCapacity(s, padding))
		return FALSE;
	Stream_Zero(s, padding);
	context->offset += padding;

	const size_t len = Stream_GetPosition(s) - start;
	if (len > UINT32_MAX)
		return FALSE;

	BYTE* header = Stream_Buffer(s) + context->objectStart;
	if (context->bigEndianDrep)
		winpr_Data_Write_UINT32_BE(header, (UINT32)len);
	else
		winpr_Data_Write_UINT32(header, (UINT32)len);

	context->inObject = FALSE;
	return TRUE;
}

BOOL winpr_ndr_encoder_write_toplevel_refid(WinPrNdrEncoder* context, wStream* s)
{
	WINPR_ASSERT(context);

	const winpr_ndr_refid refId = context->refIdCounter;
	context->refIdCounter += 4;
	return winpr_ndr_encoder_write_uint32(context, s, refId);
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
		case WINPR_NDR_ARITY_SELF_ALLOCATED:
			return 1;
		default:
			WINPR_ASSERT(0 && "unknown arity");
			return 0;
	}
}

/** @brief checks that the array described by hints fits in the inline storage of a fixed array */
static BOOL ndr_check_fixed_array(const WinPrNdrStructDescr* descr,
                                  const WinPrNdrFieldStruct* field, const void* hints)
{
	if (!hints || !field->fixedArraySize)
	{
		WLog_ERR(TAG, "%s.%s: fixed array without hints or size", descr->name, field->name);
		return FALSE;
	}

	const UINT64 needed =
	    1ull * ndr_hintsCount(field->typeDescr, hints) * field->typeDescr->itemSize;
	if (needed > field->fixedArraySize)
	{
		WLog_ERR(TAG, "%s.%s: %" PRIu64 " bytes don't fit in a fixed array of %" PRIuz " bytes",
		         descr->name, field->name, needed, field->fixedArraySize);
		return FALSE;
	}
	return TRUE;
}

/** @brief checks the [range(0, rangeMax)] attribute of an integer field */
static BOOL ndr_check_range(const WinPrNdrStructDescr* descr, const WinPrNdrFieldStruct* field,
                            const void* ptr)
{
	if (!field->rangeMax)
		return TRUE;

	UINT64 v = 0;
	switch (field->typeDescr->itemSize)
	{
		case 1:
			v = *(const UINT8*)ptr;
			break;
		case 2:
		{
			UINT16 v16 = 0;
			memcpy(&v16, ptr, sizeof(v16));
			v = v16;
		}
		break;
		case 4:
		{
			UINT32 v32 = 0;
			memcpy(&v32, ptr, sizeof(v32));
			v = v32;
		}
		break;
		case 8:
			memcpy(&v, ptr, sizeof(v));
			break;
		default:
			WLog_ERR(TAG, "%s.%s: range on a field of %" PRIuz " bytes", descr->name, field->name,
			         field->typeDescr->itemSize);
			return FALSE;
	}

	if (v > field->rangeMax)
	{
		WLog_ERR(TAG, "%s.%s: %" PRIu64 " is out of range(0, %" PRIu32 ")", descr->name,
		         field->name, v, field->rangeMax);
		return FALSE;
	}
	return TRUE;
}

/** @brief number of items of an inline array field */
static size_t ndr_inline_count(const WinPrNdrFieldStruct* field)
{
	WINPR_ASSERT(field->typeDescr->itemSize);
	return field->fixedArraySize / field->typeDescr->itemSize;
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
	WINPR_NDR_ARITY_SIMPLE,         1,       winpr_ndr_decoder_read_uint8_,
	winpr_ndr_encoder_write_uint8_, nullptr, nullptr
};

WinPrNdrMessageType winpr_ndr_uint8_descr(void)
{
	return &winpr_ndr_uint8_descr_s;
}

BOOL winpr_ndr_encoder_write_int8(WinPrNdrEncoder* context, wStream* s, INT8 v)
{
	return winpr_ndr_encoder_write_uint8(context, s, (BYTE)v);
}

BOOL winpr_ndr_encoder_write_int8_(WinPrNdrEncoder* context, wStream* s, const void* hints,
                                   const void* v)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(v);
	WINPR_UNUSED(hints);

	return winpr_ndr_encoder_write_int8(context, s, *(const INT8*)v);
}

const WinPrNdrMessageDescr winpr_ndr_int8_descr_s = {
	WINPR_NDR_ARITY_SIMPLE,        1,       winpr_ndr_decoder_read_int8_,
	winpr_ndr_encoder_write_int8_, nullptr, nullptr
};

WinPrNdrMessageType winpr_ndr_int8_descr(void)
{
	return &winpr_ndr_int8_descr_s;
}

#define SIMPLE_TYPE_IMPL(PREFIX, UPPERTYPE, LOWERTYPE)                                         \
	BOOL PREFIX##_decoder_read_##LOWERTYPE(WinPrNdrDecoder* context, wStream* s, UPPERTYPE* v) \
	{                                                                                          \
		WINPR_ASSERT(context);                                                                 \
                                                                                               \
		if (!winpr_ndr_decoder_read_align(context, s, sizeof(UPPERTYPE)))                      \
			return FALSE;                                                                      \
		if (!Stream_CheckAndLogRequiredLength(TAG, s, sizeof(UPPERTYPE)))                      \
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
		nullptr,                                                                               \
		nullptr                                                                                \
	};                                                                                         \
                                                                                               \
	WinPrNdrMessageType PREFIX##_##LOWERTYPE##_descr(void)                                     \
	{                                                                                          \
		return &PREFIX##_##LOWERTYPE##_descr_s;                                                \
	}

SIMPLE_TYPE_IMPL(winpr_ndr, UINT32, uint32)
SIMPLE_TYPE_IMPL(winpr_ndr, INT32, int32)
SIMPLE_TYPE_IMPL(winpr_ndr, UINT16, uint16)
SIMPLE_TYPE_IMPL(winpr_ndr, INT16, int16)
SIMPLE_TYPE_IMPL(winpr_ndr, UINT64, uint64)
SIMPLE_TYPE_IMPL(winpr_ndr, INT64, int64)

#define ARRAY_OF_TYPE_IMPL(TYPE, UPPERTYPE)                                                      \
	BOOL winpr_ndr_decoder_read_##TYPE##Array(WinPrNdrDecoder* context, wStream* s,              \
	                                          const void* hints, void* v)                        \
	{                                                                                            \
		WINPR_ASSERT(context);                                                                   \
		WINPR_ASSERT(s);                                                                         \
		return winpr_ndr_decoder_read_uconformant_array(context, s, hints,                       \
		                                                winpr_ndr_##TYPE##_descr(), v);          \
	}                                                                                            \
                                                                                                 \
	BOOL winpr_ndr_encoder_write_##TYPE##Array(WinPrNdrEncoder* context, wStream* s,             \
	                                           const void* hints, const void* v)                 \
	{                                                                                            \
		WINPR_ASSERT(context);                                                                   \
		WINPR_ASSERT(s);                                                                         \
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
				descr->destroyFn(nullptr, ptr);                                                  \
		}                                                                                        \
	}                                                                                            \
                                                                                                 \
	const WinPrNdrMessageDescr winpr_ndr_##TYPE##Array_descr_s = {                               \
		WINPR_NDR_ARITY_ARRAYOF,                                                                 \
		sizeof(UPPERTYPE),                                                                       \
		winpr_ndr_decoder_read_##TYPE##Array,                                                    \
		winpr_ndr_encoder_write_##TYPE##Array,                                                   \
		winpr_ndr_destroy_##TYPE##Array,                                                         \
		nullptr                                                                                  \
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
		return winpr_ndr_decoder_read_uconformant_varying_array(                                 \
		    context, s, (const WinPrNdrVaryingArrayHints*)hints, winpr_ndr_##TYPE##_descr(), v); \
	}                                                                                            \
	BOOL winpr_ndr_encoder_write_##TYPE##VaryingArray(WinPrNdrEncoder* context, wStream* s,      \
	                                                  const void* hints, const void* v)          \
	{                                                                                            \
		WINPR_ASSERT(context);                                                                   \
		WINPR_ASSERT(s);                                                                         \
		return winpr_ndr_encoder_write_uconformant_varying_array(                                \
		    context, s, (const WinPrNdrVaryingArrayHints*)hints, winpr_ndr_##TYPE##_descr(), v); \
	}                                                                                            \
                                                                                                 \
	const WinPrNdrMessageDescr winpr_ndr_##TYPE##VaryingArray_descr_s = {                        \
		WINPR_NDR_ARITY_VARYING_ARRAYOF,                                                         \
		sizeof(UPPERTYPE),                                                                       \
		winpr_ndr_decoder_read_##TYPE##VaryingArray,                                             \
		winpr_ndr_encoder_write_##TYPE##VaryingArray,                                            \
		nullptr,                                                                                 \
		nullptr                                                                                  \
	};                                                                                           \
                                                                                                 \
	WinPrNdrMessageType winpr_ndr_##TYPE##VaryingArray_descr(void)                               \
	{                                                                                            \
		return &winpr_ndr_##TYPE##VaryingArray_descr_s;                                          \
	}

ARRAY_OF_TYPE_IMPL(uint8, BYTE)
ARRAY_OF_TYPE_IMPL(uint16, UINT16)

static BOOL winpr_ndr_decoder_read_string_conformant(WinPrNdrDecoder* context, wStream* s,
                                                     const void* hints, void* v)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(v);
	WINPR_UNUSED(hints);

	UINT32 sz = 0;
	if (!winpr_ndr_decoder_read_uint32(context, s, &sz))
		return FALSE;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, sz))
		return FALSE;

	char* ret = winpr_ndr_pointer_new(&winpr_ndr_string_conformant_descr_s, nullptr, 1ull * sz + 1);
	if (!ret)
		return FALSE;

	Stream_Read(s, ret, sz);
	ret[sz] = 0;

	winpr_ndr_decoder_bytes_read(context, sz);
	char** pstr = (char**)v;
	*pstr = ret;
	return TRUE;
}

static BOOL winpr_ndr_encoder_write_string_conformant(WinPrNdrEncoder* context, wStream* s,
                                                      const void* hints, const void* v)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(v);
	WINPR_UNUSED(hints);

	const char* str = *(const char* const*)v;
	const size_t l = str ? strlen(str) + 1 : 0;
	if (l > UINT32_MAX)
		return FALSE;

	if (!winpr_ndr_encoder_write_uint32(context, s, (UINT32)l) ||
	    !Stream_EnsureRemainingCapacity(s, l))
		return FALSE;
	Stream_Write(s, str, l);
	winpr_ndr_encoder_bytes_written(context, l);
	return TRUE;
}

const WinPrNdrMessageDescr winpr_ndr_string_conformant_descr_s = {
	WINPR_NDR_ARITY_SELF_ALLOCATED,
	sizeof(char),
	winpr_ndr_decoder_read_string_conformant,
	winpr_ndr_encoder_write_string_conformant,
	nullptr /* destroy */,
	nullptr /* dump */
};

WinPrNdrMessageType winpr_ndr_string_conformant_descr(void)
{
	return &winpr_ndr_string_conformant_descr_s;
}

/** @brief reads the header of a conformant varying string, returns the number of characters */
static BOOL ndr_read_string_varying_header(WinPrNdrDecoder* context, wStream* s, size_t charSize,
                                           UINT32* plength)
{
	UINT32 maxCount = 0;
	UINT32 offset = 0;
	UINT32 length = 0;
	if (!winpr_ndr_decoder_read_uint32(context, s, &maxCount) ||
	    !winpr_ndr_decoder_read_uint32(context, s, &offset) ||
	    !winpr_ndr_decoder_read_uint32(context, s, &length))
		return FALSE;

	if (offset != 0)
	{
		WLog_ERR(TAG, "offset != 0 not supported yet");
		return FALSE;
	}

	if (length > maxCount)
	{
		WLog_ERR(TAG, "varying string length %" PRIu32 " > maximum count %" PRIu32, length,
		         maxCount);
		return FALSE;
	}

	if (!Stream_CheckAndLogRequiredLengthOfSize(TAG, s, length, charSize))
		return FALSE;

	*plength = length;
	return TRUE;
}

static BOOL winpr_ndr_decoder_read_string_varying(WinPrNdrDecoder* context, wStream* s,
                                                  const void* hints, void* v)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(v);
	WINPR_UNUSED(hints);

	UINT32 length = 0;
	if (!ndr_read_string_varying_header(context, s, sizeof(char), &length))
		return FALSE;

	char* ret =
	    winpr_ndr_pointer_new(&winpr_ndr_string_varying_descr_s, nullptr, 1ull * length + 1);
	if (!ret)
		return FALSE;

	Stream_Read(s, ret, length);
	ret[length] = 0;

	winpr_ndr_decoder_bytes_read(context, length);
	char** pstr = (char**)v;
	*pstr = ret;
	return TRUE;
}

static BOOL winpr_ndr_encoder_write_string_varying(WinPrNdrEncoder* context, wStream* s,
                                                   const void* hints, const void* v)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(v);
	WINPR_UNUSED(hints);

	const char* str = *(const char* const*)v;
	const size_t l = str ? strlen(str) + 1 : 0;
	if (l > UINT32_MAX)
		return FALSE;

	if (!winpr_ndr_encoder_write_uint32(context, s, (UINT32)l) ||
	    !winpr_ndr_encoder_write_uint32(context, s, 0) ||
	    !winpr_ndr_encoder_write_uint32(context, s, (UINT32)l) ||
	    !Stream_EnsureRemainingCapacity(s, l))
		return FALSE;

	Stream_Write(s, str, l);
	winpr_ndr_encoder_bytes_written(context, l);
	return TRUE;
}

const WinPrNdrMessageDescr winpr_ndr_string_varying_descr_s = {
	WINPR_NDR_ARITY_SELF_ALLOCATED,
	sizeof(char),
	winpr_ndr_decoder_read_string_varying,
	winpr_ndr_encoder_write_string_varying,
	nullptr /* destroy */,
	nullptr /* dump */
};

WinPrNdrMessageType winpr_ndr_string_varying_descr(void)
{
	return &winpr_ndr_string_varying_descr_s;
}

static BOOL ndr_decoder_read_wstring_varying(WinPrNdrDecoder* context, wStream* s,
                                             const void* hints, void* v)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(v);
	WINPR_UNUSED(hints);

	UINT32 length = 0;
	if (!ndr_read_string_varying_header(context, s, sizeof(WCHAR), &length))
		return FALSE;

	WCHAR* ret = winpr_ndr_pointer_new(&winpr_ndr_wstring_varying_descr_s, nullptr,
	                                   (1ull * length + 1) * sizeof(WCHAR));
	if (!ret)
		return FALSE;

	for (UINT32 i = 0; i < length; i++)
	{
		if (!winpr_ndr_decoder_read_wchar(context, s, &ret[i]))
		{
			winpr_ndr_pointer_release(ret);
			return FALSE;
		}
	}

	WCHAR** pstr = (WCHAR**)v;
	*pstr = ret;
	return TRUE;
}

static BOOL ndr_encoder_write_wstring_varying(WinPrNdrEncoder* context, wStream* s,
                                              const void* hints, const void* v)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(v);
	WINPR_UNUSED(hints);

	const WCHAR* str = *(const WCHAR* const*)v;
	const size_t l = str ? _wcslen(str) + 1 : 0;
	if (l > UINT32_MAX)
		return FALSE;

	if (!winpr_ndr_encoder_write_uint32(context, s, (UINT32)l) ||
	    !winpr_ndr_encoder_write_uint32(context, s, 0) ||
	    !winpr_ndr_encoder_write_uint32(context, s, (UINT32)l))
		return FALSE;

	for (size_t i = 0; i < l; i++)
	{
		if (!winpr_ndr_encoder_write_uint16(context, s, str[i]))
			return FALSE;
	}
	return TRUE;
}

const WinPrNdrMessageDescr winpr_ndr_wstring_varying_descr_s = {
	WINPR_NDR_ARITY_SELF_ALLOCATED,    sizeof(WCHAR),         ndr_decoder_read_wstring_varying,
	ndr_encoder_write_wstring_varying, nullptr /* destroy */, nullptr /* dump */
};

WinPrNdrMessageType winpr_ndr_wstring_varying_descr(void)
{
	return &winpr_ndr_wstring_varying_descr_s;
}

/** @brief writes the items of an array, their deferred pointers are treated after the array */
static BOOL ndr_encoder_write_items(WinPrNdrEncoder* context, wStream* s, UINT32 count,
                                    WinPrNdrMessageType itemType, const BYTE* src)
{
	BOOL ok = TRUE;

	ndr_deferreds_enter(&context->deferreds);
	for (UINT32 i = 0; ok && (i < count); i++, src += itemType->itemSize)
		ok = itemType->writeFn(context, s, nullptr, src);

	return ndr_deferreds_leave(&context->deferreds, ok);
}

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

	return ndr_encoder_write_items(context, s, hints->length, itemType, psrc);
}

BOOL winpr_ndr_encoder_write_uconformant_array(WinPrNdrEncoder* context, wStream* s, UINT32 len,
                                               WinPrNdrMessageType itemType, const BYTE* ptr)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(itemType);
	WINPR_ASSERT(ptr);

	if (!winpr_ndr_encoder_write_uint32(context, s, len))
		return FALSE;

	return ndr_encoder_write_items(context, s, len, itemType, ptr);
}

static BOOL ndr_encoder_write_field(WinPrNdrEncoder* encoder, wStream* s,
                                    const WinPrNdrStructDescr* descr,
                                    const WinPrNdrFieldStruct* field, const void* src)
{
	const BYTE* ptr = (const BYTE*)src + field->structOffset;
	void* hints = ndr_field_hints(descr, field, src);

	switch (field->pointerType)
	{
		case WINPR_NDR_NOT_POINTER:
		{
			BOOL ok = FALSE;
			if (field->fixedArraySize)
				ok = ndr_encoder_write_items(encoder, s, (UINT32)ndr_inline_count(field),
				                             field->typeDescr, ptr);
			else
				ok = ndr_check_range(descr, field, ptr) &&
				     field->typeDescr->writeFn(encoder, s, hints, ptr);

			if (!ok)
			{
				WLog_ERR(TAG, "error when writing %s.%s", descr->name, field->name);
				return FALSE;
			}
			return TRUE;
		}

		case WINPR_NDR_POINTER:
		case WINPR_NDR_POINTER_NON_NULL:
		case WINPR_NDR_POINTER_FULL: /* each pointer has its own referent */
		case WINPR_NDR_FIXED_ARRAY:
			break;

		default:
			WLog_ERR(TAG, "%s.%s unknown pointer type 0x%x", descr->name, field->name,
			         field->pointerType);
			return FALSE;
	}

	/* the target of the deferred: the referent, or the pointer itself for a self allocated type */
	const void* target = ptr;
	if (field->pointerType == WINPR_NDR_FIXED_ARRAY)
	{
		if (!ndr_check_fixed_array(descr, field, hints))
			return FALSE;

		/* an empty array is sent as a null pointer */
		if (ndr_hintsCount(field->typeDescr, hints) == 0)
			ptr = nullptr;
	}
	else
	{
		ptr = *(WINPR_CAST_CONST_PTR_AWAY(ptr, const void**));
		if (field->typeDescr->arity != WINPR_NDR_ARITY_SELF_ALLOCATED)
			target = ptr;

		if (!ptr && field->pointerType == WINPR_NDR_POINTER_NON_NULL)
		{
			WLog_ERR(TAG, "%s.%s can't be null", descr->name, field->name);
			return FALSE;
		}
	}

	winpr_ndr_refid ptrId = WINPR_NDR_PTR_NULL;
	BOOL isNew = FALSE;
	if (ptr && !winpr_ndr_encoder_allocatePtr(encoder, ptr, &ptrId, &isNew))
	{
		WLog_ERR(TAG, "error allocating pointer for %s.%s", descr->name, field->name);
		return FALSE;
	}

	if (isNew)
	{
		const WinPrNdrDeferredEntry deferred = { ptrId, field->name,
			                                     hints, WINPR_CAST_CONST_PTR_AWAY(target, void*),
			                                     FALSE, field->typeDescr };
		if (!ndr_deferreds_add(&encoder->deferreds, &deferred))
			return FALSE;
	}

	return winpr_ndr_encoder_write_uint32(encoder, s, ptrId);
}

BOOL winpr_ndr_encoder_write_fromDescr(WinPrNdrEncoder* context, wStream* s,
                                       const WinPrNdrStructDescr* descr, const void* src)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(descr);
	WINPR_ASSERT(src);

	if ((descr->alignment > 1) && !winpr_ndr_encoder_write_align(context, s, descr->alignment))
		return FALSE;

	BOOL ok = TRUE;
	ndr_deferreds_enter(&context->deferreds);
	for (size_t i = 0; ok && (i < descr->nfields); i++)
		ok = ndr_encoder_write_field(context, s, descr, &descr->fields[i], src);

	return ndr_deferreds_leave(&context->deferreds, ok);
}

winpr_ndr_refid winpr_ndr_pointer_refid(const void* ptr)
{
	return (winpr_ndr_refid)((ULONG_PTR)ptr);
}

BOOL winpr_ndr_encoder_allocatePtr(WinPrNdrEncoder* context, const void* ptr,
                                   winpr_ndr_refid* prefId, BOOL* pnewPtr)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(prefId);
	WINPR_ASSERT(pnewPtr);
	WINPR_UNUSED(ptr);

	/* pointers are unique pointers: each non-null pointer has its own referent, the decoder
	 * rejects aliased pointers */
	if (context->refIdCounter > UINT32_MAX - 4)
		return FALSE;

	*pnewPtr = TRUE;
	*prefId = context->refIdCounter;
	context->refIdCounter += 4;
	return TRUE;
}

BOOL winpr_ndr_encoder_treat_deferreds(WinPrNdrEncoder* context, wStream* s)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);

	WinPrNdrDeferredEntry current = { 0 };
	while (ndr_deferreds_pop(&context->deferreds, &current))
	{
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
		return nullptr;

	ret->version = version;
	ret->bigEndianDrep = bigEndianDrep;
	ret->referents = HashTable_New(FALSE);
	if (!ret->referents)
	{
		winpr_ndr_decoder_free(ret);
		return nullptr;
	}

	return ret;
}

WinPrNdrDecoder* winpr_ndr_decoder_new_fromStream(wStream* s)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return nullptr;

	BYTE version = Stream_Get_UINT8(s);
	BYTE drep = Stream_Get_UINT8(s);
	UINT16 headerLen = Stream_Get_UINT16(s);

	if (version != 1)
	{
		WLog_ERR(TAG, "unsupported type serialization version %" PRIu8, version);
		return nullptr;
	}

	if (headerLen < 4 || !Stream_CheckAndLogRequiredLength(TAG, s, headerLen - 4))
		return nullptr;

	/* skip filler */
	Stream_Seek(s, headerLen - 4);

	return winpr_ndr_decoder_new((drep != 0x10), version);
}

void winpr_ndr_decoder_free(WinPrNdrDecoder* context)
{
	if (context)
	{
		/* the referents belong to the pointers that reference them */
		HashTable_Free(context->referents);
		free(context);
	}
}

void winpr_ndr_decoder_bytes_read(WinPrNdrDecoder* context, size_t len)
{
	WINPR_ASSERT(context);
	context->offset += len;
}

BOOL winpr_ndr_decoder_skip_bytes(WinPrNdrDecoder* context, wStream* s, size_t nbytes)
{
	WINPR_ASSERT(context);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, nbytes))
		return FALSE;

	context->offset += nbytes;
	Stream_Seek(s, nbytes);
	return TRUE;
}

BOOL winpr_ndr_decoder_read_align(WinPrNdrDecoder* context, wStream* s, size_t sz)
{
	WINPR_ASSERT(context);

	size_t rest = context->offset % sz;
	if (rest)
	{
		size_t padding = (sz - rest);
		if (!Stream_CheckAndLogRequiredLength(TAG, s, padding))
			return FALSE;

		Stream_Seek(s, padding);
		context->offset += padding;
	}

	return TRUE;
}

BOOL winpr_ndr_decoder_read_refpointer(WinPrNdrDecoder* context, wStream* s, winpr_ndr_refid* refId)
{
	return winpr_ndr_decoder_read_uint32(context, s, refId);
}

BOOL winpr_ndr_decoder_read_object(WinPrNdrDecoder* context, wStream* s, wStream* object)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(object);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return FALSE;

	UINT32 len = 0;
	if (context->bigEndianDrep)
		Stream_Read_UINT32_BE(s, len);
	else
		Stream_Read_UINT32(s, len);
	Stream_Seek(s, 4); /* Filler */

	if (!Stream_CheckAndLogRequiredLength(TAG, s, len))
		return FALSE;

	Stream_StaticInit(object, Stream_PointerAs(s, BYTE), len);
	Stream_Seek(s, len);

	/* alignments are relative to the start of the object data */
	context->offset = 0;
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

BOOL winpr_ndr_decoder_read_int8(WinPrNdrDecoder* context, wStream* s, INT8* v)
{
	WINPR_ASSERT(v);

	BYTE b = 0;
	if (!winpr_ndr_decoder_read_uint8(context, s, &b))
		return FALSE;
	*v = (INT8)b;
	return TRUE;
}

BOOL winpr_ndr_decoder_read_int8_(WinPrNdrDecoder* context, wStream* s, const void* hints, void* v)
{
	WINPR_UNUSED(hints);
	return winpr_ndr_decoder_read_int8(context, s, (INT8*)v);
}

BOOL winpr_ndr_decoder_read_wchar(WinPrNdrDecoder* context, wStream* s, WCHAR* ptr)
{
	return winpr_ndr_decoder_read_uint16(context, s, (UINT16*)ptr);
}

/** @brief reads the items of an array, their deferred pointers are treated after the array */
static BOOL ndr_decoder_read_items(WinPrNdrDecoder* context, wStream* s, UINT32 count,
                                   WinPrNdrMessageType itemType, BYTE* target)
{
	BOOL ok = TRUE;

	ndr_deferreds_enter(&context->deferreds);
	for (UINT32 i = 0; ok && (i < count); i++, target += itemType->itemSize)
		ok = itemType->readFn(context, s, nullptr, target);

	return ndr_deferreds_leave(&context->deferreds, ok);
}

/** @brief checks the count of a conformant array against the hints
 *
 * For an array of simple items the hint is the size_is() of the array: the conformance must
 * match it. Otherwise it's a number of bytes the items must fit in.
 */
static BOOL ndr_check_array_bound(const char* what, UINT32 count, WinPrNdrMessageType itemType,
                                  UINT32 limit)
{
	if (itemType->arity == WINPR_NDR_ARITY_SIMPLE)
	{
		if (count != limit)
		{
			WLog_ERR(TAG, "%s: conformance %" PRIu32 " doesn't match the expected count %" PRIu32,
			         what, count, limit);
			return FALSE;
		}
		return TRUE;
	}

	if ((1ull * count * itemType->itemSize) > limit)
	{
		WLog_ERR(TAG, "%s: %" PRIu32 " items don't fit in %" PRIu32, what, count, limit);
		return FALSE;
	}
	return TRUE;
}

/** @brief checks the header of a conformant varying array, returns the number of items to read */
static BOOL ndr_read_varying_array_header(WinPrNdrDecoder* context, wStream* s,
                                          const WinPrNdrVaryingArrayHints* hints,
                                          WinPrNdrMessageType itemType, UINT32* pmaxCount,
                                          UINT32* plength)
{
	if (itemType->itemSize == 0)
		return FALSE;

	UINT32 maxCount = 0;
	UINT32 offset = 0;
	UINT32 length = 0;

	if (!winpr_ndr_decoder_read_uint32(context, s, &maxCount) ||
	    !winpr_ndr_decoder_read_uint32(context, s, &offset) ||
	    !winpr_ndr_decoder_read_uint32(context, s, &length))
		return FALSE;

	if (length > maxCount)
	{
		WLog_ERR(TAG, "varying array length %" PRIu32 " > maximum count %" PRIu32, length,
		         maxCount);
		return FALSE;
	}

	if (hints)
	{
		if ((1ull * length * itemType->itemSize) > hints->length)
			return FALSE;

		if ((1ull * maxCount * itemType->itemSize) > hints->maxLength)
			return FALSE;
	}

	*pmaxCount = maxCount;
	*plength = length;
	return TRUE;
}

BOOL winpr_ndr_decoder_read_uconformant_varying_array(WinPrNdrDecoder* context, wStream* s,
                                                      const WinPrNdrVaryingArrayHints* hints,
                                                      WinPrNdrMessageType itemType, void* target)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(itemType);
	WINPR_ASSERT(target);

	if (!hints)
		return FALSE;

	UINT32 maxCount = 0;
	UINT32 length = 0;
	if (!ndr_read_varying_array_header(context, s, hints, itemType, &maxCount, &length))
		return FALSE;

	return ndr_decoder_read_items(context, s, length, itemType, target);
}

BOOL winpr_ndr_decoder_read_uconformant_array(WinPrNdrDecoder* context, wStream* s,
                                              const WinPrNdrArrayHints* hints,
                                              WinPrNdrMessageType itemType, void* vtarget)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(itemType);
	WINPR_ASSERT(vtarget);

	if (!hints)
		return FALSE;

	if (itemType->itemSize == 0)
		return FALSE;

	UINT32 count = 0;
	if (!winpr_ndr_decoder_read_uint32(context, s, &count))
		return FALSE;

	if (!ndr_check_array_bound("conformant array", count, itemType, hints->count))
		return FALSE;

	return ndr_decoder_read_items(context, s, count, itemType, vtarget);
}

/** @brief size of the referent of a pointer */
static BOOL ndr_referent_size(const char* name, WinPrNdrMessageType descr, const void* hints,
                              size_t* psize)
{
	if (descr->arity == WINPR_NDR_ARITY_SELF_ALLOCATED)
	{
		WLog_ERR(TAG, "%s: the size of a self allocated type isn't known", name);
		return FALSE;
	}

	if ((descr->arity != WINPR_NDR_ARITY_SIMPLE) && !hints)
	{
		WLog_ERR(TAG, "%s: no hints for an array", name);
		return FALSE;
	}

	if (descr->itemSize == 0)
		return FALSE;

	const size_t count = ndr_hintsCount(descr, hints);
	if (count > SIZE_MAX / descr->itemSize)
		return FALSE;

	/* an empty array is still a valid referent */
	*psize = (count ? count : 1) * descr->itemSize;
	return TRUE;
}

static void ndr_referent_copy_hints(NdrReferent* ref, const void* hints)
{
	ref->hasHints = FALSE;
	if (!ref->typeDescr || !hints)
		return;

	switch (ref->typeDescr->arity)
	{
		case WINPR_NDR_ARITY_ARRAYOF:
			ref->hints.array = *(const WinPrNdrArrayHints*)hints;
			ref->hasHints = TRUE;
			break;
		case WINPR_NDR_ARITY_VARYING_ARRAYOF:
			ref->hints.varying = *(const WinPrNdrVaryingArrayHints*)hints;
			ref->hasHints = TRUE;
			break;
		default:
			break;
	}
}

static NdrReferent* ndr_referent_new(WinPrNdrMessageType descr, const void* hints, size_t size)
{
	const size_t headerSize = offsetof(NdrReferent, payload);
	const size_t payloadSize = sizeof(((NdrReferent*)nullptr)->payload);
	if (size > SIZE_MAX - headerSize - payloadSize)
		return nullptr;

	NdrReferent* ref = calloc(1, headerSize + ((size > payloadSize) ? size : payloadSize));
	if (!ref)
		return nullptr;

	ref->magic = NDR_REFERENT_MAGIC;
	ref->refCount = 1;
	ref->typeDescr = descr;
	ref->size = size;
	ndr_referent_copy_hints(ref, hints);
	return ref;
}

void* winpr_ndr_pointer_new(WinPrNdrMessageType descr, const void* hints, size_t size)
{
	NdrReferent* ref = ndr_referent_new(descr, hints, size);
	return ref ? ref->payload.data : nullptr;
}

size_t winpr_ndr_pointer_size(const void* ptr)
{
	if (!ptr)
		return 0;

	return ndr_referent(WINPR_CAST_CONST_PTR_AWAY(ptr, void*))->size;
}

void* winpr_ndr_pointer_ref(void* ptr)
{
	if (!ptr)
		return nullptr;

	NdrReferent* ref = ndr_referent(ptr);
	InterlockedIncrement(&ref->refCount);
	return ptr;
}

void winpr_ndr_pointer_release(void* ptr)
{
	if (!ptr)
		return;

	NdrReferent* ref = ndr_referent(ptr);
	if (InterlockedDecrement(&ref->refCount) > 0)
		return;

	if (ref->typeDescr && ref->typeDescr->destroyFn)
		ref->typeDescr->destroyFn(ref->hasHints ? &ref->hints : nullptr, ref->payload.data);
	ref->magic = 0;
	free(ref);
}

/** @brief records the referent of a referent id */
static BOOL ndr_decoder_register(WinPrNdrDecoder* decoder, NdrReferent* ref, winpr_ndr_refid refId,
                                 BOOL full)
{
	ref->refId = refId;
	ref->full = full;
	return HashTable_Insert(decoder->referents, (void*)(UINT_PTR)refId, ref);
}

/** @brief checks that another full pointer to a referent has the same type and size */
static BOOL ndr_referent_check(const char* name, const NdrReferent* ref, WinPrNdrMessageType descr,
                               const void* hints, size_t size)
{
	if (!ref->full)
	{
		WLog_ERR(TAG, "%s: referent 0x%" PRIx32 " is the referent of a unique pointer", name,
		         ref->refId);
		return FALSE;
	}

	if ((ref->typeDescr != descr) || (ref->size != size))
	{
		WLog_ERR(TAG,
		         "%s: referent 0x%" PRIx32 " is aliased with another type or size (%" PRIuz
		         " vs %" PRIuz " bytes)",
		         name, ref->refId, size, ref->size);
		return FALSE;
	}

	BOOL same = TRUE;
	switch (descr->arity)
	{
		case WINPR_NDR_ARITY_ARRAYOF:
			same = memcmp(&ref->hints.array, hints, sizeof(ref->hints.array)) == 0;
			break;
		case WINPR_NDR_ARITY_VARYING_ARRAYOF:
			same = memcmp(&ref->hints.varying, hints, sizeof(ref->hints.varying)) == 0;
			break;
		default:
			break;
	}

	if (!same)
		WLog_ERR(TAG, "%s: referent 0x%" PRIx32 " is aliased with other hints", name, ref->refId);
	return same;
}

/** @brief reads a full pointer, the caller has entered a deferred block */
static BOOL ndr_decoder_read_full_pointer(WinPrNdrDecoder* decoder, wStream* s, const char* name,
                                          WinPrNdrMessageType descr, void* hints, void** target)
{
	WINPR_ASSERT(decoder);
	WINPR_ASSERT(descr);
	WINPR_ASSERT(target);

	*target = nullptr;

	winpr_ndr_refid refId = 0;
	if (!winpr_ndr_decoder_read_refpointer(decoder, s, &refId))
		return FALSE;
	if (!refId)
		return TRUE;

	size_t size = 0;
	if (!ndr_referent_size(name, descr, hints, &size))
		return FALSE;

	NdrReferent* ref = HashTable_GetItemValue(decoder->referents, (void*)(UINT_PTR)refId);
	if (ref)
	{
		if (!ndr_referent_check(name, ref, descr, hints, size))
			return FALSE;

		*target = winpr_ndr_pointer_ref(ref->payload.data);
		return TRUE;
	}

	/* each item of an array takes at least one byte on the wire */
	if ((descr->arity == WINPR_NDR_ARITY_ARRAYOF) &&
	    !Stream_CheckAndLogRequiredLength(TAG, s, ndr_hintsCount(descr, hints)))
		return FALSE;

	ref = ndr_referent_new(descr, hints, size);
	if (!ref)
		return FALSE;

	if (!ndr_decoder_register(decoder, ref, refId, TRUE))
	{
		free(ref);
		return FALSE;
	}

	/* the referent is read with the deferred pointers */
	*target = ref->payload.data;
	const WinPrNdrDeferredEntry deferred = { refId, name, hints, ref->payload.data, FALSE, descr };
	return ndr_deferreds_add(&decoder->deferreds, &deferred);
}

BOOL winpr_ndr_decoder_read_pointer_ex(WinPrNdrDecoder* context, wStream* s, const char* name,
                                       WinPrNdrMessageType descr, void* hints, void** target)
{
	WINPR_ASSERT(context);

	ndr_deferreds_enter(&context->deferreds);
	const BOOL ok = ndr_decoder_read_full_pointer(context, s, name, descr, hints, target);
	return ndr_deferreds_leave(&context->deferreds, ok);
}

static BOOL ndr_decoder_read_field(WinPrNdrDecoder* decoder, wStream* s,
                                   const WinPrNdrStructDescr* descr,
                                   const WinPrNdrFieldStruct* field, void* target)
{
	BYTE* ptr = (BYTE*)target + field->structOffset;
	void* hints = ndr_field_hints(descr, field, target);

	switch (field->pointerType)
	{
		case WINPR_NDR_NOT_POINTER:
		{
			BOOL ok = FALSE;
			if (field->fixedArraySize)
				ok = ndr_decoder_read_items(decoder, s, (UINT32)ndr_inline_count(field),
				                            field->typeDescr, ptr);
			else
				ok = field->typeDescr->readFn(decoder, s, hints, ptr) &&
				     ndr_check_range(descr, field, ptr);

			if (!ok)
			{
				WLog_ERR(TAG, "error when reading %s.%s", descr->name, field->name);
				return FALSE;
			}
			return TRUE;
		}

		case WINPR_NDR_POINTER_FULL:
		{
			/* the field is a pointer, possibly not aligned in the byte view of the struct */
			void* referent = nullptr;
			const BOOL ok = ndr_decoder_read_full_pointer(decoder, s, field->name, field->typeDescr,
			                                              hints, &referent);

			/* stored even on failure, the referent is released with the structure */
			memcpy(ptr, (const void*)&referent, sizeof(referent));
			if (!ok)
			{
				WLog_ERR(TAG, "error when reading %s.%s", descr->name, field->name);
				return FALSE;
			}
			return TRUE;
		}

		case WINPR_NDR_POINTER:
		case WINPR_NDR_POINTER_NON_NULL:
		case WINPR_NDR_FIXED_ARRAY:
			break;

		default:
			WLog_ERR(TAG, "%s.%s unknown pointer type 0x%x", descr->name, field->name,
			         field->pointerType);
			return FALSE;
	}

	WinPrNdrDeferredEntry deferred = {
		WINPR_NDR_PTR_NULL, field->name, hints, ptr, field->pointerType != WINPR_NDR_FIXED_ARRAY,
		field->typeDescr
	};
	if (!winpr_ndr_decoder_read_refpointer(decoder, s, &deferred.ptrId))
	{
		WLog_ERR(TAG, "error when reading %s.%s", descr->name, field->name);
		return FALSE;
	}

	if (!deferred.ptrId)
	{
		if (field->pointerType == WINPR_NDR_POINTER_NON_NULL)
		{
			WLog_ERR(TAG, "%s.%s can't be null", descr->name, field->name);
			return FALSE;
		}

		/* the inline storage of a fixed array can't hold a null pointer to a non empty array */
		if ((field->pointerType == WINPR_NDR_FIXED_ARRAY) &&
		    (!hints || (ndr_hintsCount(field->typeDescr, hints) != 0)))
		{
			WLog_ERR(TAG, "%s.%s is null but the array is not empty", descr->name, field->name);
			return FALSE;
		}
		return TRUE;
	}

	if (!deferred.allocate && !ndr_check_fixed_array(descr, field, hints))
		return FALSE;

	return ndr_deferreds_add(&decoder->deferreds, &deferred);
}

BOOL winpr_ndr_decoder_read_fromDescr(WinPrNdrDecoder* context, wStream* s,
                                      const WinPrNdrStructDescr* descr, void* target)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(descr);
	WINPR_ASSERT(target);

	if ((descr->alignment > 1) && !winpr_ndr_decoder_read_align(context, s, descr->alignment))
		return FALSE;

	BOOL ok = TRUE;
	ndr_deferreds_enter(&context->deferreds);
	for (size_t i = 0; ok && (i < descr->nfields); i++)
		ok = ndr_decoder_read_field(context, s, descr, &descr->fields[i], target);

	return ndr_deferreds_leave(&context->deferreds, ok);
}

BOOL winpr_ndr_decoder_treat_deferreds(WinPrNdrDecoder* context, wStream* s)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(s);

	WinPrNdrDeferredEntry current = { 0 };
	while (ndr_deferreds_pop(&context->deferreds, &current))
	{
		WLog_VRB(TAG, "treating read deferred 0x%x for %s", current.ptrId, current.name);

		BOOL ok = FALSE;
		if (current.allocate)
			ok = winpr_ndr_decoder_read_pointedMessageEx(context, s, current.ptrId, current.msg,
			                                             current.hints, (void**)current.target);
		else
			ok = current.msg->readFn(context, s, current.hints, current.target);

		if (!ok)
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

	*target = nullptr;
	if (!ptrId)
		return TRUE;

	/* a unique pointer can't share its referent */
	if (HashTable_Contains(context->referents, (void*)(UINT_PTR)ptrId))
	{
		WLog_ERR(TAG, "unique pointer 0x%" PRIx32 " is aliased", ptrId);
		return FALSE;
	}

	void* ret = nullptr;
	if (descr->arity == WINPR_NDR_ARITY_SELF_ALLOCATED)
	{
		/* the reader allocates the referent with winpr_ndr_pointer_new() */
		if (!descr->readFn(context, s, hints, (void*)&ret))
			return FALSE;
		if (!ret)
			return FALSE;
	}
	else
	{
		size_t size = 0;
		if (!ndr_referent_size("unique pointer", descr, hints, &size))
			return FALSE;

		/* each item takes at least one byte on the wire, don't allocate for data that isn't
		 * there */
		if ((descr->arity == WINPR_NDR_ARITY_ARRAYOF) &&
		    !Stream_CheckAndLogRequiredLength(TAG, s, ndr_hintsCount(descr, hints)))
			return FALSE;

		ret = winpr_ndr_pointer_new(descr, hints, size);
		if (!ret)
			return FALSE;

		if (!descr->readFn(context, s, hints, ret))
		{
			winpr_ndr_pointer_release(ret);
			return FALSE;
		}
	}

	if (!ndr_decoder_register(context, ndr_referent(ret), ptrId, FALSE))
	{
		winpr_ndr_pointer_release(ret);
		return FALSE;
	}

	*target = ret;
	return TRUE;
}

BOOL winpr_ndr_decoder_push_deferreds(WinPrNdrDecoder* context, WinPrNdrDeferredEntry* deferreds,
                                      size_t ndeferred)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(deferreds);

	BOOL ok = TRUE;
	ndr_deferreds_enter(&context->deferreds);
	for (size_t i = 0; ok && (i < ndeferred); i++)
		ok = ndr_deferreds_add(&context->deferreds, &deferreds[i]);

	return ndr_deferreds_leave(&context->deferreds, ok);
}

/* ========================================================================================================
 */

/* ====================================== unions ========================================== */

/** @brief size of the discriminant of a union */
static size_t ndr_union_switch_size(const WinPrNdrUnionDescr* descr)
{
	WINPR_ASSERT(descr);
	WINPR_ASSERT((descr->switchSize == 0) || (descr->switchSize == 2) || (descr->switchSize == 4));
	return descr->switchSize ? descr->switchSize : 4;
}

UINT32 winpr_ndr_union_switch_value(const WinPrNdrUnionDescr* descr, const void* switchField)
{
	WINPR_ASSERT(switchField);

	if (ndr_union_switch_size(descr) == 2)
		return *(const UINT16*)switchField;
	return *(const UINT32*)switchField;
}

/** @brief the arm of a union for a discriminant, the default arm if there's none */
static const WinPrNdrUnionArm* ndr_union_find_arm(const WinPrNdrUnionDescr* descr,
                                                  UINT32 switchValue)
{
	WINPR_ASSERT(descr);

	for (size_t i = 0; i < descr->narms; i++)
	{
		const WinPrNdrUnionArm* arm = &descr->arms[i];
		if (arm->discriminant == switchValue)
			return arm;
	}
	return descr->defaultArm;
}

/** @brief the arm of a union for a discriminant, as a structure of one field */
static const WinPrNdrUnionArm* ndr_union_arm(const WinPrNdrUnionDescr* descr, UINT32 switchValue,
                                             WinPrNdrStructDescr* armStruct)
{
	WINPR_ASSERT(armStruct);

	const WinPrNdrUnionArm* arm = ndr_union_find_arm(descr, switchValue);
	if (!arm)
	{
		WLog_ERR(TAG, "%s: no arm for the discriminant 0x%08" PRIx32, descr->name, switchValue);
		return nullptr;
	}

	WINPR_ASSERT(arm->field.hintsField < 0);
	armStruct->name = descr->name;
	armStruct->nfields = 1;
	armStruct->fields = &arm->field;
	armStruct->alignment = 0;
	return arm;
}

static BOOL ndr_encoder_write_discriminant(WinPrNdrEncoder* context, wStream* s,
                                           const WinPrNdrUnionDescr* descr, UINT32 switchValue)
{
	if (ndr_union_switch_size(descr) == 4)
		return winpr_ndr_encoder_write_uint32(context, s, switchValue);

	if (switchValue > UINT16_MAX)
	{
		WLog_ERR(TAG, "%s: discriminant 0x%08" PRIx32 " doesn't fit in 16 bits", descr->name,
		         switchValue);
		return FALSE;
	}
	return winpr_ndr_encoder_write_uint16(context, s, (UINT16)switchValue);
}

static BOOL ndr_decoder_read_discriminant(WinPrNdrDecoder* context, wStream* s,
                                          const WinPrNdrUnionDescr* descr, UINT32* discriminant)
{
	if (ndr_union_switch_size(descr) == 4)
		return winpr_ndr_decoder_read_uint32(context, s, discriminant);

	UINT16 v = 0;
	if (!winpr_ndr_decoder_read_uint16(context, s, &v))
		return FALSE;
	*discriminant = v;
	return TRUE;
}

BOOL winpr_ndr_encoder_write_union(WinPrNdrEncoder* context, wStream* s,
                                   const WinPrNdrUnionDescr* descr, UINT32 switchValue,
                                   const void* src)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(src);

	WinPrNdrStructDescr armStruct = WINPR_C_ARRAY_INIT;
	const WinPrNdrUnionArm* arm = ndr_union_arm(descr, switchValue, &armStruct);
	if (!arm || !ndr_encoder_write_discriminant(context, s, descr, switchValue))
		return FALSE;

	BOOL ok = TRUE;
	ndr_deferreds_enter(&context->deferreds);
	if (arm->field.typeDescr)
		ok = ndr_encoder_write_field(context, s, &armStruct, &arm->field, src);
	return ndr_deferreds_leave(&context->deferreds, ok);
}

BOOL winpr_ndr_decoder_read_union(WinPrNdrDecoder* context, wStream* s,
                                  const WinPrNdrUnionDescr* descr, UINT32 switchValue, void* target)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(target);

	UINT32 discriminant = 0;
	if (!ndr_decoder_read_discriminant(context, s, descr, &discriminant))
		return FALSE;

	if (discriminant != switchValue)
	{
		WLog_ERR(TAG,
		         "%s: discriminant 0x%08" PRIx32 " doesn't match the switch value 0x%08" PRIx32,
		         descr->name, discriminant, switchValue);
		return FALSE;
	}

	WinPrNdrStructDescr armStruct = WINPR_C_ARRAY_INIT;
	const WinPrNdrUnionArm* arm = ndr_union_arm(descr, switchValue, &armStruct);
	if (!arm)
		return FALSE;

	BOOL ok = TRUE;
	ndr_deferreds_enter(&context->deferreds);
	if (arm->field.typeDescr)
		ok = ndr_decoder_read_field(context, s, &armStruct, &arm->field, target);
	return ndr_deferreds_leave(&context->deferreds, ok);
}

void winpr_ndr_union_destroy(const WinPrNdrUnionDescr* descr, UINT32 switchValue, void* obj)
{
	WINPR_ASSERT(descr);
	WINPR_ASSERT(obj);

	const WinPrNdrUnionArm* arm = ndr_union_find_arm(descr, switchValue);
	if (!arm || !arm->field.typeDescr)
		return;

	const WinPrNdrStructDescr armStruct = { descr->name, 1, &arm->field, 0 };
	winpr_ndr_struct_destroy(&armStruct, obj);
}

/* ======================================================================================== */

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
			case WINPR_NDR_POINTER_FULL:
				ptr = *(WINPR_CAST_CONST_PTR_AWAY(ptr, const void**));
				break;
			case WINPR_NDR_NOT_POINTER:
			case WINPR_NDR_FIXED_ARRAY:
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
		void* hints = nullptr;

		if (field->hintsField >= 0)
		{
			/* computes the address of the hints field if any */
			WINPR_ASSERT((size_t)field->hintsField < descr->nfields);
			const WinPrNdrFieldStruct* hintsField = &descr->fields[field->hintsField];

			hints = (BYTE*)pptr + hintsField->structOffset;
		}

		switch (field->pointerType)
		{
			case WINPR_NDR_POINTER:
			case WINPR_NDR_POINTER_NON_NULL:
			case WINPR_NDR_POINTER_FULL:
			{
				/* the referent is destroyed with its last reference */
				void** pptrField = (void**)ptr;
				winpr_ndr_pointer_release(*pptrField);
				*pptrField = nullptr;
			}
			break;

			default:
				/* NOT_POINTER and FIXED_ARRAY fields are stored inline in the struct */
				if (field->typeDescr->destroyFn)
					field->typeDescr->destroyFn(hints, ptr);
				break;
		}
	}
}
