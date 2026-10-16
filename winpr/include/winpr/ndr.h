/**
 * WinPR: Windows Portable Runtime
 * NDR (Network Data Representation) encoding and decoding
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

/** @file
 * @brief NDR encoder and decoder ([C706] chapter 14, [MS-RPCE] 2.2.5 and 2.2.6)
 *
 * Messages are described with descriptors (WinPrNdrMessageDescr, WinPrNdrStructDescr,
 * WinPrNdrUnionDescr), and read, written, dumped and destroyed from these descriptors.
 *
 * The encoder (WinPrNdrEncoder) and the decoder (WinPrNdrDecoder) keep the alignment offset of
 * the current object and the pending deferred pointers: the referents of the embedded pointers
 * are written after the construct that contains them, so writing or reading a message is
 * followed by a call to winpr_ndr_encoder_treat_deferreds() or
 * winpr_ndr_decoder_treat_deferreds().
 *
 * All the pointers returned by the decoder are reference counted referents allocated with
 * winpr_ndr_pointer_new(), they must not be passed to free() but released with
 * winpr_ndr_pointer_release() (winpr_ndr_struct_destroy() releases the pointers of a
 * structure).
 *
 * @since version 3.33.0
 */

/** @brief the referent id of a null pointer
 *  @since version 3.33.0
 */
#define WINPR_NDR_PTR_NULL (0UL)

/** @brief declares the functions and the message descriptor of a simple type
 *
 * For a type named LOWER (stored in a UPPER) this declares:
 * - PREFIX##_decoder_read_##LOWER() and PREFIX##_encoder_write_##LOWER(), that read and write a
 *   value aligned on its size;
 * - PREFIX##_decoder_read_##LOWER##_() and PREFIX##_encoder_write_##LOWER##_(), the same with
 *   the WINPR_NDR_READER_FN / WINPR_NDR_WRITER_FN prototypes;
 * - PREFIX##_##LOWER##_descr_s and PREFIX##_##LOWER##_descr(), the message descriptor of the
 *   type.
 *
 * @since version 3.33.0
 */
#define WINPR_NDR_SIMPLE_TYPE_DECL(PREFIX, LOWER, UPPER)                                          \
	WINPR_ATTR_NODISCARD                                                                          \
	WINPR_API BOOL PREFIX##_decoder_read_##LOWER(WinPrNdrDecoder* context, wStream* s, UPPER* v); \
	WINPR_ATTR_NODISCARD                                                                          \
	WINPR_API BOOL PREFIX##_decoder_read_##LOWER##_(WinPrNdrDecoder* context, wStream* s,         \
	                                                const void* hints, void* v);                  \
	WINPR_ATTR_NODISCARD                                                                          \
	WINPR_API BOOL PREFIX##_encoder_write_##LOWER(WinPrNdrEncoder* context, wStream* s, UPPER v); \
	WINPR_ATTR_NODISCARD                                                                          \
	WINPR_API BOOL PREFIX##_encoder_write_##LOWER##_(WinPrNdrEncoder* context, wStream* s,        \
	                                                 const void* hints, const void* v);           \
	WINPR_API extern const WinPrNdrMessageDescr PREFIX##_##LOWER##_descr_s;                       \
	WINPR_ATTR_NODISCARD                                                                          \
	WINPR_API WinPrNdrMessageType PREFIX##_##LOWER##_descr(void)

/** @brief declares the functions and the message descriptors of the arrays of a simple type
 *
 * For the items of type TYPE (stored in a UPPERTYPE) this declares:
 * - the conformant array: PREFIX##_decoder_read_##TYPE##Array(),
 *   PREFIX##_encoder_write_##TYPE##Array(), PREFIX##_destroy_##TYPE##Array() and the message
 *   descriptor PREFIX##_##TYPE##Array_descr_s / PREFIX##_##TYPE##Array_descr(), its hints are a
 *   WinPrNdrArrayHints;
 * - the conformant varying array: PREFIX##_decoder_read_##TYPE##VaryingArray(),
 *   PREFIX##_encoder_write_##TYPE##VaryingArray() and the message descriptor
 *   PREFIX##_##TYPE##VaryingArray_descr_s / PREFIX##_##TYPE##VaryingArray_descr(), its hints
 *   are a WinPrNdrVaryingArrayHints.
 *
 * @since version 3.33.0
 */
#define WINPR_NDR_ARRAY_OF_TYPE_DECL(PREFIX, TYPE, UPPERTYPE)                                 \
	WINPR_ATTR_NODISCARD                                                                      \
	WINPR_API BOOL PREFIX##_decoder_read_##TYPE##Array(WinPrNdrDecoder* context, wStream* s,  \
	                                                   const void* hints, void* v);           \
	WINPR_ATTR_NODISCARD                                                                      \
	WINPR_API BOOL PREFIX##_encoder_write_##TYPE##Array(WinPrNdrEncoder* context, wStream* s, \
	                                                    const void* hints, const void* v);    \
	WINPR_API void PREFIX##_destroy_##TYPE##Array(const void* hints, void* obj);              \
	WINPR_API extern const WinPrNdrMessageDescr PREFIX##_##TYPE##Array_descr_s;               \
	WINPR_ATTR_NODISCARD                                                                      \
	WINPR_API WinPrNdrMessageType PREFIX##_##TYPE##Array_descr(void);                         \
                                                                                              \
	WINPR_ATTR_NODISCARD                                                                      \
	WINPR_API BOOL PREFIX##_decoder_read_##TYPE##VaryingArray(                                \
	    WinPrNdrDecoder* context, wStream* s, const void* hints, void* v);                    \
	WINPR_ATTR_NODISCARD                                                                      \
	WINPR_API BOOL PREFIX##_encoder_write_##TYPE##VaryingArray(                               \
	    WinPrNdrEncoder* context, wStream* s, const void* hints, const void* v);              \
	WINPR_API extern const WinPrNdrMessageDescr PREFIX##_##TYPE##VaryingArray_descr_s;        \
	WINPR_ATTR_NODISCARD                                                                      \
	WINPR_API WinPrNdrMessageType PREFIX##_##TYPE##VaryingArray_descr(void)

/** @brief defines a static message type for a structure
 *
 * Defines PREFIX##_descr_s for the CTYPE structure described by the WinPrNdrStructDescr
 * PREFIX##_struct.
 *
 * @since version 3.33.0
 */
#define WINPR_NDR_STRUCT_MESSAGE_DEFINE(PREFIX, CTYPE)                                             \
	static BOOL PREFIX##_read(WinPrNdrDecoder* context, wStream* s, const void* hints, void* obj)  \
	{                                                                                              \
		WINPR_UNUSED(hints);                                                                       \
		return winpr_ndr_decoder_read_fromDescr(context, s, &PREFIX##_struct, obj);                \
	}                                                                                              \
	static BOOL PREFIX##_write(WinPrNdrEncoder* context, wStream* s, const void* hints,            \
	                           const void* obj)                                                    \
	{                                                                                              \
		WINPR_UNUSED(hints);                                                                       \
		return winpr_ndr_encoder_write_fromDescr(context, s, &PREFIX##_struct, obj);               \
	}                                                                                              \
	static void PREFIX##_destroy(const void* hints, void* obj)                                     \
	{                                                                                              \
		WINPR_UNUSED(hints);                                                                       \
		winpr_ndr_struct_destroy(&PREFIX##_struct, obj);                                           \
	}                                                                                              \
	static void PREFIX##_dump(wLog* logger, UINT32 lvl, size_t indentLevel, const void* obj)       \
	{                                                                                              \
		winpr_ndr_struct_dump_fromDescr(logger, lvl, indentLevel, &PREFIX##_struct, obj);          \
	}                                                                                              \
	static const WinPrNdrMessageDescr PREFIX##_descr_s = { WINPR_NDR_ARITY_SIMPLE, sizeof(CTYPE),  \
		                                                   PREFIX##_read,          PREFIX##_write, \
		                                                   PREFIX##_destroy,       PREFIX##_dump }

/** @brief defines a static message type for a conformant array of ITEMPREFIX##_descr_s items
 *
 * Defines PREFIX##_descr_s, the hints of the array field are a WinPrNdrArrayHints.
 *
 * @since version 3.33.0
 */
#define WINPR_NDR_ARRAY_MESSAGE_DEFINE(PREFIX, ITEMPREFIX, CTYPE)                                 \
	static BOOL PREFIX##_read(WinPrNdrDecoder* context, wStream* s, const void* hints, void* obj) \
	{                                                                                             \
		return winpr_ndr_decoder_read_uconformant_array(context, s, hints, &ITEMPREFIX##_descr_s, \
		                                                obj);                                     \
	}                                                                                             \
	static BOOL PREFIX##_write(WinPrNdrEncoder* context, wStream* s, const void* hints,           \
	                           const void* obj)                                                   \
	{                                                                                             \
		WINPR_ASSERT(hints);                                                                      \
		const WinPrNdrArrayHints* ahints = hints;                                                 \
		return winpr_ndr_encoder_write_uconformant_array(context, s, ahints->count,               \
		                                                 &ITEMPREFIX##_descr_s, obj);             \
	}                                                                                             \
	static void PREFIX##_destroy(const void* hints, void* obj)                                    \
	{                                                                                             \
		WINPR_ASSERT(hints);                                                                      \
		const WinPrNdrArrayHints* ahints = hints;                                                 \
		CTYPE* items = obj;                                                                       \
		for (UINT32 i = 0; i < ahints->count; i++)                                                \
			ITEMPREFIX##_destroy(nullptr, &items[i]);                                             \
	}                                                                                             \
	static const WinPrNdrMessageDescr PREFIX##_descr_s = {                                        \
		WINPR_NDR_ARITY_ARRAYOF, sizeof(CTYPE),    PREFIX##_read,                                 \
		PREFIX##_write,          PREFIX##_destroy, nullptr                                        \
	}

/** @brief defines a static message type for a non-encapsulated union
 *
 * Defines PREFIX##_descr_s for the CTYPE union described by the WinPrNdrUnionDescr
 * PREFIX##_union. The hints of the union field are the field of its [switch_is()], a UINT16 or a
 * UINT32 depending on the switchSize of the union.
 *
 * @since version 3.33.0
 */
#define WINPR_NDR_UNION_MESSAGE_DEFINE(PREFIX, CTYPE)                                              \
	static BOOL PREFIX##_read(WinPrNdrDecoder* context, wStream* s, const void* hints, void* obj)  \
	{                                                                                              \
		WINPR_ASSERT(hints);                                                                       \
		return winpr_ndr_decoder_read_union(context, s, &PREFIX##_union,                           \
		                                    winpr_ndr_union_switch_value(&PREFIX##_union, hints),  \
		                                    obj);                                                  \
	}                                                                                              \
	static BOOL PREFIX##_write(WinPrNdrEncoder* context, wStream* s, const void* hints,            \
	                           const void* obj)                                                    \
	{                                                                                              \
		WINPR_ASSERT(hints);                                                                       \
		return winpr_ndr_encoder_write_union(context, s, &PREFIX##_union,                          \
		                                     winpr_ndr_union_switch_value(&PREFIX##_union, hints), \
		                                     obj);                                                 \
	}                                                                                              \
	static void PREFIX##_destroy(const void* hints, void* obj)                                     \
	{                                                                                              \
		if (hints)                                                                                 \
			winpr_ndr_union_destroy(&PREFIX##_union,                                               \
			                        winpr_ndr_union_switch_value(&PREFIX##_union, hints), obj);    \
	}                                                                                              \
	static const WinPrNdrMessageDescr PREFIX##_descr_s = { WINPR_NDR_ARITY_SIMPLE, sizeof(CTYPE),  \
		                                                   PREFIX##_read,          PREFIX##_write, \
		                                                   PREFIX##_destroy,       nullptr }

#ifdef __cplusplus
extern "C"
{
#endif

	/** @brief a NDR decoder
	 *  @since version 3.33.0
	 */
	typedef struct WinPrNdrDecoder_s WinPrNdrDecoder;

	/** @brief a NDR encoder
	 *  @since version 3.33.0
	 */
	typedef struct WinPrNdrEncoder_s WinPrNdrEncoder;

	/** @brief a referent id, the wire value of a pointer (WINPR_NDR_PTR_NULL for a null pointer)
	 *  @since version 3.33.0
	 */
	typedef UINT32 winpr_ndr_refid;

	/** @brief reads a message
	 *
	 *  @param context the decoder
	 *  @param s the stream to read from
	 *  @param hints the hints of the message (the size of an array...), can be nullptr
	 *  @param target where the message is stored
	 *  @return \b TRUE on success, \b FALSE otherwise
	 *  @since version 3.33.0
	 */
	typedef BOOL (*WINPR_NDR_READER_FN)(WinPrNdrDecoder* context, wStream* s, const void* hints,
	                                    void* target);

	/** @brief writes a message
	 *
	 *  @param context the encoder
	 *  @param s the stream to write to
	 *  @param hints the hints of the message (the size of an array...), can be nullptr
	 *  @param obj the message to write
	 *  @return \b TRUE on success, \b FALSE otherwise
	 *  @since version 3.33.0
	 */
	typedef BOOL (*WINPR_NDR_WRITER_FN)(WinPrNdrEncoder* context, wStream* s, const void* hints,
	                                    const void* obj);

	/** @brief releases the content of a message, not the message storage itself
	 *
	 *  @param hints the hints of the message, can be nullptr
	 *  @param obj the message
	 *  @since version 3.33.0
	 */
	typedef void (*WINPR_NDR_DESTROY_FN)(const void* hints, void* obj);

	/** @brief dumps a message in the logs
	 *
	 *  @param logger the logger to use
	 *  @param lvl the log level
	 *  @param indentLevel the number of tabs to prepend to each line
	 *  @param obj the message
	 *  @since version 3.33.0
	 */
	typedef void (*WINPR_NDR_DUMP_FN)(wLog* logger, UINT32 lvl, size_t indentLevel,
	                                  const void* obj);

	/** @brief arity of a message
	 *  @since version 3.33.0
	 */
	typedef enum
	{
		WINPR_NDR_ARITY_SIMPLE,          /**< a single item */
		WINPR_NDR_ARITY_ARRAYOF,         /**< a conformant array, the hints are a
		                                    WinPrNdrArrayHints */
		WINPR_NDR_ARITY_VARYING_ARRAYOF, /**< a conformant varying array, the hints are a
		                                    WinPrNdrVaryingArrayHints */
		WINPR_NDR_ARITY_SELF_ALLOCATED,  /**< the reader allocates the referent (strings...) with
		                                    winpr_ndr_pointer_new(): the target is the pointer
		                                    field, itemSize is the size of an element */
	} WinPrNdrTypeArity;

	/** @brief message descriptor
	 *  @since version 3.33.0
	 */
	typedef struct
	{
		WinPrNdrTypeArity arity;
		size_t itemSize;                /**< size of the C type (of an item for an array) */
		WINPR_NDR_READER_FN readFn;     /**< reads the message */
		WINPR_NDR_WRITER_FN writeFn;    /**< writes the message */
		WINPR_NDR_DESTROY_FN destroyFn; /**< releases its content, can be nullptr */
		WINPR_NDR_DUMP_FN dumpFn;       /**< dumps it in the logs, can be nullptr */
	} WinPrNdrMessageDescr;

	/** @brief a message type, a pointer on its descriptor
	 *  @since version 3.33.0
	 */
	typedef const WinPrNdrMessageDescr* WinPrNdrMessageType;

	/** @brief pointer or not and if null is accepted
	 *  @since version 3.33.0
	 */
	typedef enum
	{
		WINPR_NDR_NOT_POINTER,      /**< the field is stored inline, an array of fixedArraySize
		                               bytes if fixedArraySize is not 0 */
		WINPR_NDR_POINTER_NON_NULL, /**< [ref] or a unique pointer that can't be null */
		WINPR_NDR_POINTER,          /**< unique pointer ([unique]) */
		WINPR_NDR_FIXED_ARRAY,      /**< unique pointer on a conformant array stored inline in the
		                               field (fixedArraySize bytes), the pointer is null when the
		                               array is empty */
		WINPR_NDR_POINTER_FULL      /**< full pointer ([ptr]): pointers with the same referent id
		                               share the same referent */
	} WinPrNdrPointerType;

	/** @brief descriptor of a field in a structure
	 *  @since version 3.33.0
	 */
	typedef struct
	{
		const char* name;                /**< name of the field, for the logs */
		size_t structOffset;             /**< offset of the field in the structure */
		WinPrNdrPointerType pointerType; /**< how the field is stored */
		SSIZE_T hintsField; /**< index of the field holding the hints of this field (the size
		                       of an array...), -1 if none */
		WinPrNdrMessageType typeDescr; /**< type of the field (of its referent for a pointer) */
		size_t fixedArraySize;         /**< size in bytes of the inline storage of an array */
		UINT32 rangeMax; /**< for an integer field: [range(0, rangeMax)], not checked if 0 */
	} WinPrNdrFieldStruct;

	/** @brief structure descriptor
	 *  @since version 3.33.0
	 */
	typedef struct
	{
		const char* name;                  /**< name of the structure, for the logs */
		size_t nfields;                    /**< number of fields */
		const WinPrNdrFieldStruct* fields; /**< the fields, in their wire order */
		size_t alignment; /**< alignment of the structure (its largest member), 0 if unspecified */
	} WinPrNdrStructDescr;

	/** @brief an arm of a non-encapsulated union
	 *  @since version 3.33.0
	 */
	typedef struct
	{
		UINT32 discriminant;       /**< the [case()] value of the arm */
		WinPrNdrFieldStruct field; /**< the arm, its structOffset is relative to the union and it
		                              has no hints. An arm without typeDescr has no body */
	} WinPrNdrUnionArm;

	/** @brief a non-encapsulated union ([switch_type()])
	 *
	 * On the wire the union is its discriminant followed by the selected arm, aligned on its own
	 * type.
	 *
	 * @since version 3.33.0
	 */
	typedef struct
	{
		const char* name;             /**< name of the union, for the logs */
		size_t narms;                 /**< number of arms */
		const WinPrNdrUnionArm* arms; /**< the arms */
		size_t switchSize; /**< size of the discriminant and of the [switch_is()] field: 2 for a
		                      short or an enum, 4 for a long (the default when 0) */
		const WinPrNdrUnionArm* defaultArm; /**< the [default] arm, its discriminant is ignored.
		                                       Without it an unknown discriminant is an error */
	} WinPrNdrUnionDescr;

	/** @brief a deferred pointer, a referent to read after the current construct
	 *  @since version 3.33.0
	 */
	typedef struct
	{
		winpr_ndr_refid ptrId; /**< the referent id read for the pointer */
		const char* name;      /**< name of the pointer, for the logs */
		void* hints;           /**< hints of the referent, valid until the deferred is treated */
		void* target;  /**< if allocate is set, the pointer field that receives the referent,
		                  otherwise the storage the referent is read into */
		BOOL allocate; /**< the referent is allocated with winpr_ndr_pointer_new() (or by the
		                  reader of a self allocated type) */
		WinPrNdrMessageType msg; /**< the type of the referent */
	} WinPrNdrDeferredEntry;

	/** @brief frees an encoder
	 *
	 *  @param context the encoder to free, can be \b nullptr
	 *  @since version 3.33.0
	 */
	WINPR_API void winpr_ndr_encoder_free(WinPrNdrEncoder* context);

	/** @brief frees an encoder and resets the pointer
	 *
	 *  @param pcontext a pointer on the encoder to free, must not be \b nullptr
	 *  @since version 3.33.0
	 */
	static INLINE void winpr_ndr_encoder_destroy(WinPrNdrEncoder** pcontext)
	{
		WINPR_ASSERT(pcontext);
		winpr_ndr_encoder_free(*pcontext);
		*pcontext = nullptr;
	}

	/** @brief creates an encoder
	 *
	 *  @param bigEndianDrep \b TRUE to encode in big endian, \b FALSE for little endian
	 *  @param version the type serialization version written by winpr_ndr_encoder_write_header()
	 *  @return the new encoder, \b nullptr on failure
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_MALLOC(winpr_ndr_encoder_free, 1)
	WINPR_API WinPrNdrEncoder* winpr_ndr_encoder_new(BOOL bigEndianDrep, BYTE version);

	/** @brief creates an encoder with the same endianness and version as a decoder, to answer
	 *  the message it decoded
	 *
	 *  @param src the decoder
	 *  @return the new encoder, \b nullptr on failure
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_MALLOC(winpr_ndr_encoder_free, 1)
	WINPR_API WinPrNdrEncoder* winpr_ndr_encoder_from_decoder(const WinPrNdrDecoder* src);

	/** @brief writes the common type header ([MS-RPCE] 2.2.6.1): version, endianness, header
	 *  length and filler
	 *
	 *  @param context the encoder
	 *  @param s the stream to write to
	 *  @return \b TRUE on success, \b FALSE otherwise
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_ndr_encoder_write_header(WinPrNdrEncoder* context, wStream* s);

	WINPR_NDR_SIMPLE_TYPE_DECL(winpr_ndr, uint8, UINT8);
	WINPR_NDR_SIMPLE_TYPE_DECL(winpr_ndr, uint16, UINT16);
	WINPR_NDR_SIMPLE_TYPE_DECL(winpr_ndr, int32, INT32);
	WINPR_NDR_SIMPLE_TYPE_DECL(winpr_ndr, uint32, UINT32);
	WINPR_NDR_SIMPLE_TYPE_DECL(winpr_ndr, uint64, UINT64);

	WINPR_NDR_ARRAY_OF_TYPE_DECL(winpr_ndr, uint8, BYTE);
	WINPR_NDR_ARRAY_OF_TYPE_DECL(winpr_ndr, uint16, UINT16);

	/** @brief accounts for bytes written directly in the stream, for the alignments
	 *
	 *  @param context the encoder
	 *  @param len the number of bytes written
	 *  @since version 3.33.0
	 */
	WINPR_API void winpr_ndr_encoder_bytes_written(WinPrNdrEncoder* context, size_t len);

	/** @brief writes zero padding to align the current object offset
	 *
	 *  @param context the encoder
	 *  @param s the stream to write to
	 *  @param sz the alignment, must not be 0
	 *  @return \b TRUE on success, \b FALSE otherwise
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_ndr_encoder_write_align(WinPrNdrEncoder* context, wStream* s, size_t sz);

	/** @brief writes raw bytes, without alignment
	 *
	 *  @param context the encoder
	 *  @param s the stream to write to
	 *  @param data the bytes to write
	 *  @param sz the number of bytes
	 *  @return \b TRUE on success, \b FALSE otherwise
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_ndr_encoder_write_data(WinPrNdrEncoder* context, wStream* s,
	                                            const void* data, size_t sz);

	/** @brief starts a serialized object ([MS-RPCE] 2.2.6 type serialization version 1)
	 *
	 * Writes a placeholder for the private header, the object data is to be written after. Only
	 * one object can be serialized at a time.
	 *
	 *  @param context the encoder
	 *  @param s the stream to write to
	 *  @return \b TRUE on success, \b FALSE otherwise
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_ndr_encoder_start_object(WinPrNdrEncoder* context, wStream* s);

	/** @brief ends the serialized object started with winpr_ndr_encoder_start_object()
	 *
	 * Pads the object data to a multiple of 8 bytes and fills the private header.
	 *
	 *  @param context the encoder
	 *  @param s the stream given to winpr_ndr_encoder_start_object()
	 *  @return \b TRUE on success, \b FALSE otherwise
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_ndr_encoder_end_object(WinPrNdrEncoder* context, wStream* s);

	/** @brief writes a new referent id for a non-null top-level pointer
	 *
	 *  @param context the encoder
	 *  @param s the stream to write to
	 *  @return \b TRUE on success, \b FALSE otherwise
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_ndr_encoder_write_toplevel_refid(WinPrNdrEncoder* context, wStream* s);

	/** @brief hints for a conformant varying array
	 *  @since version 3.33.0
	 */
	typedef struct
	{
		UINT32 length;    /**< the actual count */
		UINT32 maxLength; /**< the maximum count */
	} WinPrNdrVaryingArrayHints;

	/** @brief writes a conformant varying array: its maximum count, offset 0, actual count and
	 *  the items
	 *
	 *  The referents of the pointers in the items are deferred after the array.
	 *
	 *  @param context the encoder
	 *  @param s the stream to write to
	 *  @param hints the counts of the array
	 *  @param itemType the type of the items
	 *  @param src the items, hints->length of them are written
	 *  @return \b TRUE on success, \b FALSE otherwise
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_ndr_encoder_write_uconformant_varying_array(
	    WinPrNdrEncoder* context, wStream* s, const WinPrNdrVaryingArrayHints* hints,
	    WinPrNdrMessageType itemType, const void* src);

	/** @brief hints for a conformant array
	 *  @since version 3.33.0
	 */
	typedef struct
	{
		UINT32 count; /**< the number of items */
	} WinPrNdrArrayHints;

	/** @brief writes a conformant array: its count and the items
	 *
	 *  The referents of the pointers in the items are deferred after the array.
	 *
	 *  @param context the encoder
	 *  @param s the stream to write to
	 *  @param len the number of items
	 *  @param itemType the type of the items
	 *  @param ptr the items
	 *  @return \b TRUE on success, \b FALSE otherwise
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_ndr_encoder_write_uconformant_array(WinPrNdrEncoder* context, wStream* s,
	                                                         UINT32 len,
	                                                         WinPrNdrMessageType itemType,
	                                                         const BYTE* ptr);

	/** @brief writes a structure from its descriptor
	 *
	 *  The structure is aligned on descr->alignment, the referents of its pointers are deferred.
	 *
	 *  @param context the encoder
	 *  @param s the stream to write to
	 *  @param descr the descriptor of the structure
	 *  @param src the structure
	 *  @return \b TRUE on success, \b FALSE otherwise
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_ndr_encoder_write_fromDescr(WinPrNdrEncoder* context, wStream* s,
	                                                 const WinPrNdrStructDescr* descr,
	                                                 const void* src);

	/** @brief writes the discriminant and the arm of a union
	 *
	 *  @param context the encoder
	 *  @param s the stream to write to
	 *  @param descr the descriptor of the union
	 *  @param switchValue the discriminant, the value of the [switch_is()] field
	 *  @param src the union
	 *  @return \b TRUE on success, \b FALSE if there's no arm for switchValue or on failure
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_ndr_encoder_write_union(WinPrNdrEncoder* context, wStream* s,
	                                             const WinPrNdrUnionDescr* descr,
	                                             UINT32 switchValue, const void* src);

	/** @brief a referent id built from the value of a pointer
	 *
	 *  @param ptr the pointer
	 *  @return the low 32 bits of ptr, WINPR_NDR_PTR_NULL for \b nullptr
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API winpr_ndr_refid winpr_ndr_pointer_refid(const void* ptr);

	/** @brief allocates the referent id of an embedded pointer
	 *
	 *  The pointers are written as unique pointers: each non-null pointer gets its own referent
	 *  id, and *pnewPtr is always set to \b TRUE.
	 *
	 *  @param context the encoder
	 *  @param ptr the pointer
	 *  @param prefId receives the referent id
	 *  @param pnewPtr receives \b TRUE if the referent is to be written
	 *  @return \b TRUE on success, \b FALSE when the referent ids are exhausted
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_ndr_encoder_allocatePtr(WinPrNdrEncoder* context, const void* ptr,
	                                             winpr_ndr_refid* prefId, BOOL* pnewPtr);

	/** @brief writes the pending deferred referents
	 *
	 *  @param context the encoder
	 *  @param s the stream to write to
	 *  @return \b TRUE on success, \b FALSE otherwise
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_ndr_encoder_treat_deferreds(WinPrNdrEncoder* context, wStream* s);

	/* ====================================== decoder ========================================= */

	/** @brief frees a decoder
	 *
	 *  The referents it decoded belong to their pointers and are not released.
	 *
	 *  @param context the decoder to free, can be \b nullptr
	 *  @since version 3.33.0
	 */
	WINPR_API void winpr_ndr_decoder_free(WinPrNdrDecoder* context);

	/** @brief frees a decoder and resets the pointer
	 *
	 *  @param pcontext a pointer on the decoder to free, must not be \b nullptr
	 *  @since version 3.33.0
	 */
	static INLINE void winpr_ndr_decoder_destroy(WinPrNdrDecoder** pcontext)
	{
		WINPR_ASSERT(pcontext);
		winpr_ndr_decoder_free(*pcontext);
		*pcontext = nullptr;
	}

	/** @brief creates a decoder
	 *
	 *  @param bigEndianDrep \b TRUE to decode big endian data, \b FALSE for little endian
	 *  @param version the type serialization version
	 *  @return the new decoder, \b nullptr on failure
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_MALLOC(winpr_ndr_decoder_free, 1)
	WINPR_API WinPrNdrDecoder* winpr_ndr_decoder_new(BOOL bigEndianDrep, BYTE version);

	/** @brief reads the common type header ([MS-RPCE] 2.2.6.1) and creates a decoder for its
	 *  version and endianness
	 *
	 *  Only the version 1 is supported.
	 *
	 *  @param s the stream to read from, positioned after the header on success
	 *  @return the new decoder, \b nullptr on failure
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_MALLOC(winpr_ndr_decoder_free, 1)
	WINPR_API WinPrNdrDecoder* winpr_ndr_decoder_new_fromStream(wStream* s);

	/** @brief accounts for bytes read directly from the stream, for the alignments
	 *
	 *  @param context the decoder
	 *  @param len the number of bytes read
	 *  @since version 3.33.0
	 */
	WINPR_API void winpr_ndr_decoder_bytes_read(WinPrNdrDecoder* context, size_t len);

	/** @brief reads the private header of a serialized object ([MS-RPCE] 2.2.6 type
	 *  serialization version 1)
	 *
	 *  The alignments are then relative to the start of the object data.
	 *
	 *  @param context the decoder
	 *  @param s the stream to read from, positioned after the object on success
	 *  @param object receives a static stream on the object data, to decode with this context.
	 *  It points in the buffer of s.
	 *  @return \b TRUE on success, \b FALSE otherwise
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_ndr_decoder_read_object(WinPrNdrDecoder* context, wStream* s,
	                                             wStream* object);

	/** @brief adds deferred pointers, read by the next winpr_ndr_decoder_treat_deferreds()
	 *
	 *  This is used by custom readers that read the referent ids of their pointers themselves.
	 *  The entries are copied, the hints they reference must stay valid until they are treated.
	 *
	 *  @param context the decoder
	 *  @param deferreds the deferred pointers, in their wire order
	 *  @param ndeferred the number of entries
	 *  @return \b TRUE on success, \b FALSE if there are too many pending deferred pointers
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_ndr_decoder_push_deferreds(WinPrNdrDecoder* context,
	                                                WinPrNdrDeferredEntry* deferreds,
	                                                size_t ndeferred);

	/** @brief reads the pending deferred referents
	 *
	 *  @param context the decoder
	 *  @param s the stream to read from
	 *  @return \b TRUE on success, \b FALSE otherwise
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_ndr_decoder_treat_deferreds(WinPrNdrDecoder* context, wStream* s);

	/** @brief reads a referent id
	 *
	 *  @param context the decoder
	 *  @param s the stream to read from
	 *  @param refId receives the referent id, WINPR_NDR_PTR_NULL for a null pointer
	 *  @return \b TRUE on success, \b FALSE otherwise
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_ndr_decoder_read_refpointer(WinPrNdrDecoder* context, wStream* s,
	                                                 UINT32* refId);

	/** @brief reads a full pointer ([ptr])
	 *
	 * The first pointer with a referent id allocates a reference counted block for a referent of
	 * type descr, the referent is read with the deferred pointers. The next pointers with the
	 * same referent id get a new reference on that block, they must have the same type and the
	 * same size. Types allocating their referent (WINPR_NDR_ARITY_SELF_ALLOCATED) can't be used.
	 *
	 * @param context the decoder
	 * @param s the stream to read from
	 * @param name the name of the pointer, for the logs
	 * @param descr the type of the referent
	 * @param hints the hints of the referent, they must be valid until the deferred pointers are
	 * treated
	 * @param target receives the referent with a reference, nullptr for a null pointer
	 * @return \b TRUE on success, \b FALSE otherwise
	 * @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_ndr_decoder_read_pointer_ex(WinPrNdrDecoder* context, wStream* s,
	                                                 const char* name, WinPrNdrMessageType descr,
	                                                 void* hints, void** target);

	/** @brief releases a referent
	 *
	 * When the last reference is released the referent is destroyed with the destroy function of
	 * its type, and freed.
	 *
	 * @param ptr the referent, can be \b nullptr
	 * @since version 3.33.0
	 */
	WINPR_API void winpr_ndr_pointer_release(void* ptr);

	/** @brief allocates a referent
	 *
	 * All the pointers returned by the decoder are reference counted referents, released with
	 * winpr_ndr_pointer_release() (winpr_ndr_struct_destroy() releases the pointers of a
	 * structure). Self allocated types allocate their referent with this function, and so must
	 * the pointers of a structure built to be encoded and destroyed with
	 * winpr_ndr_struct_destroy().
	 *
	 * @param descr the type of the referent, its destroy function is called when the referent is
	 * released, can be nullptr
	 * @param hints the hints of the referent for the destroy function, copied, can be nullptr
	 * @param size the size of the referent, it is zeroed
	 * @return the referent with a reference, nullptr on failure
	 * @since version 3.33.0
	 */
	WINPR_ATTR_MALLOC(winpr_ndr_pointer_release, 1)
	WINPR_API void* winpr_ndr_pointer_new(WinPrNdrMessageType descr, const void* hints,
	                                      size_t size);

	/** @brief takes a reference on a referent
	 *
	 * @param ptr the referent, can be \b nullptr
	 * @return ptr
	 * @since version 3.33.0
	 */
	WINPR_API void* winpr_ndr_pointer_ref(void* ptr);

	/** @brief the size of a referent, as allocated by winpr_ndr_pointer_new()
	 *
	 * @param ptr the referent, can be \b nullptr
	 * @return the size of the referent, 0 for \b nullptr
	 * @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API size_t winpr_ndr_pointer_size(const void* ptr);

	/** @brief skips bytes
	 *
	 *  @param context the decoder
	 *  @param s the stream to read from
	 *  @param nbytes the number of bytes to skip
	 *  @return \b TRUE on success, \b FALSE if the stream is too short
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_ndr_decoder_skip_bytes(WinPrNdrDecoder* context, wStream* s,
	                                            size_t nbytes);

	/** @brief skips the padding to align the current object offset
	 *
	 *  @param context the decoder
	 *  @param s the stream to read from
	 *  @param sz the alignment, must not be 0
	 *  @return \b TRUE on success, \b FALSE if the stream is too short
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_ndr_decoder_read_align(WinPrNdrDecoder* context, wStream* s, size_t sz);

	/** @brief reads a WCHAR, aligned on 2 bytes
	 *
	 *  @param context the decoder
	 *  @param s the stream to read from
	 *  @param ptr receives the character
	 *  @return \b TRUE on success, \b FALSE otherwise
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_ndr_decoder_read_wchar(WinPrNdrDecoder* context, wStream* s, WCHAR* ptr);

	/** @brief reads the referent of a unique pointer
	 *
	 *  The referent is allocated with winpr_ndr_pointer_new() (or by the reader of a self
	 *  allocated type) and read. A referent id already used by another pointer is rejected.
	 *
	 *  @param context the decoder
	 *  @param s the stream to read from
	 *  @param ptrId the referent id of the pointer, nothing is read for WINPR_NDR_PTR_NULL
	 *  @param descr the type of the referent
	 *  @param hints the hints of the referent, can be nullptr for a simple type
	 *  @param target receives the referent, nullptr for a null pointer
	 *  @return \b TRUE on success, \b FALSE otherwise
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_ndr_decoder_read_pointedMessageEx(WinPrNdrDecoder* context, wStream* s,
	                                                       winpr_ndr_refid ptrId,
	                                                       WinPrNdrMessageType descr, void* hints,
	                                                       void** target);

	/** @brief reads a structure from its descriptor
	 *
	 *  The structure is aligned on descr->alignment, the referents of its pointers are deferred.
	 *  On failure the structure may be partially filled, it is released with
	 *  winpr_ndr_struct_destroy().
	 *
	 *  @param context the decoder
	 *  @param s the stream to read from
	 *  @param descr the descriptor of the structure
	 *  @param target the structure, zeroed by the caller
	 *  @return \b TRUE on success, \b FALSE otherwise
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_ndr_decoder_read_fromDescr(WinPrNdrDecoder* context, wStream* s,
	                                                const WinPrNdrStructDescr* descr, void* target);

	/** @brief reads a union, its discriminant must be switchValue
	 *
	 *  @param context the decoder
	 *  @param s the stream to read from
	 *  @param descr the descriptor of the union
	 *  @param switchValue the value of the [switch_is()] field of the union
	 *  @param target the union
	 *  @return \b TRUE on success, \b FALSE if the discriminant doesn't match, has no arm, or on
	 *  failure
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_ndr_decoder_read_union(WinPrNdrDecoder* context, wStream* s,
	                                            const WinPrNdrUnionDescr* descr, UINT32 switchValue,
	                                            void* target);

	/** @brief reads a conformant array: its count and the items
	 *
	 *  For simple items the count must be hints->count, otherwise the items must fit in
	 *  hints->count bytes. The referents of the pointers in the items are deferred after the
	 *  array.
	 *
	 *  @param context the decoder
	 *  @param s the stream to read from
	 *  @param hints the expected size of the array, must not be nullptr
	 *  @param itemType the type of the items
	 *  @param vtarget receives the items
	 *  @return \b TRUE on success, \b FALSE otherwise
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_ndr_decoder_read_uconformant_array(WinPrNdrDecoder* context, wStream* s,
	                                                        const WinPrNdrArrayHints* hints,
	                                                        WinPrNdrMessageType itemType,
	                                                        void* vtarget);

	/** @brief reads a conformant varying array: its maximum count, offset, actual count and the
	 *  items
	 *
	 *  The actual and maximum counts of items must fit in hints->length and hints->maxLength
	 *  bytes. The referents of the pointers in the items are deferred after the array.
	 *
	 *  @param context the decoder
	 *  @param s the stream to read from
	 *  @param hints the bounds of the array, must not be nullptr
	 *  @param itemType the type of the items
	 *  @param ptarget receives the items
	 *  @return \b TRUE on success, \b FALSE otherwise
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_ndr_decoder_read_uconformant_varying_array(
	    WinPrNdrDecoder* context, wStream* s, const WinPrNdrVaryingArrayHints* hints,
	    WinPrNdrMessageType itemType, void* ptarget);

	/** @brief conformant string of char, read in a char* allocated with winpr_ndr_pointer_new()
	 *  @since version 3.33.0
	 */
	WINPR_API extern const WinPrNdrMessageDescr winpr_ndr_string_conformant_descr_s;

	/** @brief the message type of winpr_ndr_string_conformant_descr_s
	 *  @return the message type
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API WinPrNdrMessageType winpr_ndr_string_conformant_descr(void);

	/** @brief [string] char*: a conformant varying string of char, read in a char* allocated with
	 *  winpr_ndr_pointer_new()
	 *  @since version 3.33.0
	 */
	WINPR_API extern const WinPrNdrMessageDescr winpr_ndr_string_varying_descr_s;

	/** @brief the message type of winpr_ndr_string_varying_descr_s
	 *  @return the message type
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API WinPrNdrMessageType winpr_ndr_string_varying_descr(void);

	/** @brief [string] wchar_t*: a conformant varying string of WCHAR, read in a WCHAR* allocated
	 *  with winpr_ndr_pointer_new()
	 *  @since version 3.33.0
	 */
	WINPR_API extern const WinPrNdrMessageDescr winpr_ndr_wstring_varying_descr_s;

	/** @brief the message type of winpr_ndr_wstring_varying_descr_s
	 *  @return the message type
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API WinPrNdrMessageType winpr_ndr_wstring_varying_descr(void);

	/** @brief dumps a structure in the logs from its descriptor
	 *
	 *  @param logger the logger to use
	 *  @param lvl the log level
	 *  @param identLevel the number of tabs to prepend to each line
	 *  @param descr the descriptor of the structure
	 *  @param obj the structure
	 *  @since version 3.33.0
	 */
	WINPR_API void winpr_ndr_struct_dump_fromDescr(wLog* logger, UINT32 lvl, size_t identLevel,
	                                               const WinPrNdrStructDescr* descr,
	                                               const void* obj);

	/** @brief releases the content of a structure from its descriptor
	 *
	 *  The pointer fields are released with winpr_ndr_pointer_release() and reset, the inline
	 *  fields are destroyed with the destroy function of their type. The storage of the
	 *  structure itself is not freed.
	 *
	 *  @param descr the descriptor of the structure
	 *  @param pptr the structure
	 *  @since version 3.33.0
	 */
	WINPR_API void winpr_ndr_struct_destroy(const WinPrNdrStructDescr* descr, void* pptr);

	/** @brief the value of the [switch_is()] field of a union
	 *
	 *  @param descr the descriptor of the union, its switchSize gives the size of the field
	 *  @param switchField the [switch_is()] field, a UINT16 or a UINT32
	 *  @return the value of the field
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API UINT32 winpr_ndr_union_switch_value(const WinPrNdrUnionDescr* descr,
	                                              const void* switchField);

	/** @brief releases the content of the arm switchValue of a union
	 *
	 *  @param descr the descriptor of the union
	 *  @param switchValue the discriminant of the arm
	 *  @param obj the union
	 *  @since version 3.33.0
	 */
	WINPR_API void winpr_ndr_union_destroy(const WinPrNdrUnionDescr* descr, UINT32 switchValue,
	                                       void* obj);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_NDR_H_ */
