/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * ASN.1 Basic Encoding Rules (BER)
 *
 * Copyright 2011-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CRYPTO_BER_H
#define FREERDP_CRYPTO_BER_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#include <winpr/stream.h>

/**
 * \defgroup FreeRDP_BER BER read/write functions
 * @{
 */

/* BER type */

/* Class - bits 8 and 7 */
#define FREERDP_BER_CLASS_MASK 0xC0                             /** @since version 3.9.0 */
#define FREERDP_BER_CLASS_UNIV 0x00 /** @since version 3.9.0 */ /* 0 0 */
#define FREERDP_BER_CLASS_APPL 0x40 /** @since version 3.9.0 */ /* 0 1 */
#define FREERDP_BER_CLASS_CTXT 0x80 /** @since version 3.9.0 */ /* 1 0 */
#define FREERDP_BER_CLASS_PRIV 0xC0 /** @since version 3.9.0 */ /* 1 1 */

/* P/C - bit 6 */
#define FREERDP_BER_PC_MASK 0x20                               /** @since version 3.9.0 */
#define FREERDP_BER_PRIMITIVE 0x00 /** @since version 3.9.0 */ /* 0 */
#define FREERDP_BER_CONSTRUCT 0x20 /** @since version 3.9.0 */ /* 1 */

/* Tag - bits 5 to 1 */
#define FREERDP_BER_TAG_MASK 0x1F            /** @since version 3.9.0 */
#define FREERDP_BER_TAG_BOOLEAN 0x01         /** @since version 3.9.0 */
#define FREERDP_BER_TAG_INTEGER 0x02         /** @since version 3.9.0 */
#define FREERDP_BER_TAG_BIT_STRING 0x03      /** @since version 3.9.0 */
#define FREERDP_BER_TAG_OCTET_STRING 0x04    /** @since version 3.9.0 */
#define FREERDP_BER_TAG_OBJECT_IDENFIER 0x06 /** @since version 3.9.0 */
#define FREERDP_BER_TAG_ENUMERATED 0x0A      /** @since version 3.9.0 */
#define FREERDP_BER_TAG_SEQUENCE 0x10        /** @since version 3.9.0 */
#define FREERDP_BER_TAG_SEQUENCE_OF 0x10     /** @since version 3.9.0 */

#define FREERDP_BER_PC(_pc) \
	(_pc ? FREERDP_BER_CONSTRUCT : FREERDP_BER_PRIMITIVE) /** @since version 3.9.0 */

#ifdef __cplusplus
extern "C"
{
#endif

	/**
	 * @brief Read BER length.
	 * @param s The stream to read from
	 * @param length A pointer set to the length read
	 * @return \b TRUE for success, \b FALSE for any failure
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_ber_read_length(wStream* s, size_t* length);

	/**
	 * @brief Write BER length.
	 * @param s The stream to write to
	 * @param length The length value to write
	 * @return Number of bytes written
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_ber_write_length(wStream* s, size_t length);

	/**
	 * @brief freerdp_ber_sizeof_length
	 * @param length the length value to write
	 * @return Size of length field
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_ber_sizeof_length(size_t length);

	/**
	 * @brief Read BER Universal tag.
	 * @param s The stream to read from
	 * @param tag BER universally-defined tag
	 * @param pc primitive \b FALSE or constructed \b TRUE
	 * @return \b TRUE for success, \b FALSE for any failure
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_ber_read_universal_tag(wStream* s, BYTE tag, BOOL pc);

	/**
	 * @brief Write BER Universal tag.
	 * @param s The stream to write to
	 * @param tag BER universally-defined tag
	 * @param pc primitive \b FALSE or constructed \b TRUE
	 * @return Number of bytes written
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_ber_write_universal_tag(wStream* s, BYTE tag, BOOL pc);

	/**
	 * @brief Read BER Application tag.
	 * @param s The stream to read from
	 * @param tag BER application-defined tag
	 * @param length A pointer set to the length read
	 * @return \b TRUE for success, \b FALSE for any failure
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_ber_read_application_tag(wStream* s, BYTE tag, size_t* length);

	/**
	 * @brief Write BER Application tag.
	 * @param s The stream to write to
	 * @param tag BER application-defined tag
	 * @param length The length value to write
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API void freerdp_ber_write_application_tag(wStream* s, BYTE tag, size_t length);

	/**
	 * @brief freerdp_ber_read_enumerated
	 * @param s The stream to read from
	 * @param enumerated A pointer set to the enumerated value read
	 * @param count The maximum expected enumerated value
	 * @return \b TRUE for success, \b FALSE for any failure
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_ber_read_enumerated(wStream* s, BYTE* enumerated, BYTE count);

	/**
	 * @brief freerdp_ber_write_enumerated
	 * @param s The stream to write to
	 * @param enumerated The enumerated value to write
	 * @param count The maximum enumerated value expected, must be larger than \b enumerated
	 * @return \b TRUE for success, \b FALSE for any failure
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_ber_write_enumerated(wStream* s, BYTE enumerated, BYTE count);

	/**
	 * @brief freerdp_ber_read_contextual_tag
	 * @param s The stream to read from
	 * @param tag BER contextual-defined tag
	 * @param length A pointer set to the length read
	 * @param pc primitive \b FALSE or constructed \b TRUE
	 * @return \b TRUE for success, \b FALSE for any failure
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_ber_read_contextual_tag(wStream* s, BYTE tag, size_t* length, BOOL pc);

	/**
	 * @brief freerdp_ber_write_contextual_tag
	 * @param s The stream to write to
	 * @param tag BER contextual-defined tag
	 * @param length The length value to write
	 * @param pc primitive \b FALSE or constructed \b TRUE
	 * @return Number of bytes written
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_ber_write_contextual_tag(wStream* s, BYTE tag, size_t length,
	                                                    BOOL pc);

	/**
	 * @brief freerdp_ber_sizeof_contextual_tag
	 * @param length The lenght value to write
	 * @return Number of bytes required
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_ber_sizeof_contextual_tag(size_t length);

	/**
	 * @brief Read BER SEQUENCE tag.
	 * @param s The stream to read from
	 * @param length A pointer set to the length read
	 * @return \b TRUE for success, \b FALSE for any failure
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_ber_read_sequence_tag(wStream* s, size_t* length);

	/**
	 * @brief Write BER SEQUENCE tag.
	 * @param s The stream to write to
	 * @param length The lenght value to write
	 * @return Number of bytes written
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_ber_write_sequence_tag(wStream* s, size_t length);

	/**
	 * @brief freerdp_ber_sizeof_sequence
	 * @param length The length value to write
	 * @return Number of bytes required
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_ber_sizeof_sequence(size_t length);

	/**
	 * @brief freerdp_ber_sizeof_sequence_tag
	 * @param length The length value to write
	 * @return Number of bytes required
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_ber_sizeof_sequence_tag(size_t length);

	/**
	 * @brief freerdp_ber_read_bit_string
	 * @param s The stream to read from
	 * @param length A pointer set to the length read
	 * @param padding A pointer set to the padding value read
	 * @return \b TRUE for success, \b FALSE for any failure
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_ber_read_bit_string(wStream* s, size_t* length, BYTE* padding);

	/**
	 * @brief freerdp_ber_read_octet_string_tag
	 * @param s The stream to read from
	 * @param length A pointer set to the length read
	 * @return \b TRUE for success, \b FALSE for any failure
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_ber_read_octet_string_tag(wStream* s, size_t* length);

	/**
	 * @brief Read a BER OCTET_STRING
	 * @param s The stream to read from
	 * @param content A pointer set to the string read
	 * @param length A pointer set to the length read
	 * @return \b TRUE for success, \b FALSE for any failure
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_ber_read_octet_string(wStream* s, BYTE** content, size_t* length);

	/**
	 * @brief freerdp_ber_write_octet_string_tag
	 * @param s The stream to write to
	 * @param length The length value to write
	 * @return Number of bytes written
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_ber_write_octet_string_tag(wStream* s, size_t length);

	/**
	 * @brief freerdp_ber_sizeof_octet_string
	 * @param length The length value to write
	 * @return Number of bytes required
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_ber_sizeof_octet_string(size_t length);

	/**
	 * @brief freerdp_ber_sizeof_contextual_octet_string
	 * @param length The length of the string
	 * @return Number of bytes required
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_ber_sizeof_contextual_octet_string(size_t length);

	/**
	 * @brief freerdp_ber_write_char_to_unicode_octet_string
	 * @param s The stream to write to
	 * @param str The string to write
	 * @return Number of bytes written
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_ber_write_char_to_unicode_octet_string(wStream* s, const char* str);

	/**
	 * @brief freerdp_ber_write_contextual_char_to_unicode_octet_string
	 * @param s The stream to write to
	 * @param tag BER contextual-defined tag
	 * @param oct_str The string to write
	 * @return Number of bytes written
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_ber_write_contextual_char_to_unicode_octet_string(
	    wStream* s, BYTE tag, const char* oct_str);

	/**
	 * @brief Write a BER OCTET_STRING
	 * @param s The stream to write to
	 * @param oct_str The string to write
	 * @param length The length of the string in bytes
	 * @return Number of bytes written
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_ber_write_octet_string(wStream* s, const BYTE* oct_str,
	                                                  size_t length);

	/**
	 * @brief freerdp_ber_read_char_from_unicode_octet_string
	 * @param s The stream to read from
	 * @param str A pointer to the string read
	 * @return \b TRUE for success, \b FALSE for any failure
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_ber_read_char_from_unicode_octet_string(wStream* s, char** str);

	/**
	 * @brief freerdp_ber_read_unicode_octet_string
	 * @param s The stream to read from
	 * @param str A pointer to the string to read
	 * @return \b TRUE for success, \b FALSE for any failure
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_ber_read_unicode_octet_string(wStream* s, LPWSTR* str);

	/**
	 * @brief freerdp_ber_write_contextual_octet_string
	 * @param s The stream to write to
	 * @param tag BER contextual-defined tag
	 * @param oct_str The string to write
	 * @param length The length of the string in bytes
	 * @return Number of bytes written
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_ber_write_contextual_octet_string(wStream* s, BYTE tag,
	                                                             const BYTE* oct_str,
	                                                             size_t length);

	/**
	 * @brief freerdp_ber_write_contextual_unicode_octet_string
	 * @param s The stream to write to
	 * @param tag BER contextual-defined tag
	 * @param str The string to write
	 * @return Number of bytes written
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_ber_write_contextual_unicode_octet_string(wStream* s, BYTE tag,
	                                                                     const WCHAR* str);

	/**
	 * @brief Read a BER BOOLEAN
	 * @param s The stream to read from
	 * @param value A pointer to the value read, must not be NULL
	 * @return \b TRUE for success, \b FALSE for any failure
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_ber_read_BOOL(wStream* s, BOOL* value);

	/**
	 * @brief Write a BER BOOLEAN
	 * @param s The stream to write to
	 * @param value The value to write
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API void freerdp_ber_write_BOOL(wStream* s, BOOL value);

	/**
	 * @brief Read a BER INTEGER
	 * @param s The stream to read from
	 * @return \b TRUE for success, \b FALSE for any failure
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_ber_read_integer(wStream* s, UINT32* value);

	/**
	 * @brief Write a BER INTEGER
	 * @param s The stream to write to
	 * @param value The value to write
	 * @return The size in bytes that were written
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_ber_write_integer(wStream* s, UINT32 value);

	/**
	 * @brief freerdp_ber_write_contextual_integer
	 * @param s The stream to write to
	 * @param tag BER contextual-defined tag
	 * @param value The value to write
	 * @return Number of bytes written
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_ber_write_contextual_integer(wStream* s, BYTE tag, UINT32 value);

	/**
	 * @brief freerdp_ber_read_integer_length
	 * @param s The stream to read from
	 * @param length A pointer set to the length read
	 * @return \b TRUE for success, \b FALSE for any failure
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_ber_read_integer_length(wStream* s, size_t* length);

	/**
	 * @brief freerdp_ber_sizeof_integer
	 * @param value The integer value to write
	 * @return Number of bytes required by \b value
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_ber_sizeof_integer(UINT32 value);

	/**
	 * @brief freerdp_ber_sizeof_contextual_integer
	 * @param value The integer value to write
	 * @return Number of byte written
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_ber_sizeof_contextual_integer(UINT32 value);

#ifdef __cplusplus
}
#endif

#if defined(WITH_FREERDP_3x_DEPRECATED)
/* Class - bits 8 and 7 */
#define BER_CLASS_MASK 0xC0
#define BER_CLASS_UNIV 0x00 /* 0 0 */
#define BER_CLASS_APPL 0x40 /* 0 1 */
#define BER_CLASS_CTXT 0x80 /* 1 0 */
#define BER_CLASS_PRIV 0xC0 /* 1 1 */

/* P/C - bit 6 */
#define BER_PC_MASK 0x20
#define BER_PRIMITIVE 0x00 /* 0 */
#define BER_CONSTRUCT 0x20 /* 1 */

/* Tag - bits 5 to 1 */
#define BER_TAG_MASK 0x1F
#define BER_TAG_BOOLEAN 0x01
#define BER_TAG_INTEGER 0x02
#define BER_TAG_BIT_STRING 0x03
#define BER_TAG_OCTET_STRING 0x04
#define BER_TAG_OBJECT_IDENFIER 0x06
#define BER_TAG_ENUMERATED 0x0A
#define BER_TAG_SEQUENCE 0x10
#define BER_TAG_SEQUENCE_OF 0x10

#define BER_PC(_pc) (_pc ? BER_CONSTRUCT : BER_PRIMITIVE)

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_read_length",
	                                 BOOL ber_read_length(wStream* s, size_t* length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_write_length",
	                                 size_t ber_write_length(wStream* s, size_t length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_sizeof_length",
	                                 size_t _ber_sizeof_length(size_t length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_read_universal_tag",
	                                 BOOL ber_read_universal_tag(wStream* s, BYTE tag, BOOL pc));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_write_universal_tag",
	                                 size_t ber_write_universal_tag(wStream* s, BYTE tag, BOOL pc));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_read_application_tag",
	                                 BOOL ber_read_application_tag(wStream* s, BYTE tag,
	                                                               size_t* length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_write_application_tag",
	                                 void ber_write_application_tag(wStream* s, BYTE tag,
	                                                                size_t length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_read_enumerated",
	                                 BOOL ber_read_enumerated(wStream* s, BYTE* enumerated,
	                                                          BYTE count));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_write_enumerated",
	                                 void ber_write_enumerated(wStream* s, BYTE enumerated,
	                                                           BYTE count));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_read_contextual_tag",
	                                 BOOL ber_read_contextual_tag(wStream* s, BYTE tag,
	                                                              size_t* length, BOOL pc));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_write_contextual_tag",
	                                 size_t ber_write_contextual_tag(wStream* s, BYTE tag,
	                                                                 size_t length, BOOL pc));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_sizeof_contextual_tag",
	                                 size_t ber_sizeof_contextual_tag(size_t length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_read_sequence_tag",
	                                 BOOL ber_read_sequence_tag(wStream* s, size_t* length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_write_sequence_tag",
	                                 size_t ber_write_sequence_tag(wStream* s, size_t length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_sizeof_sequence",
	                                 size_t ber_sizeof_sequence(size_t length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_sizeof_sequence_tag",
	                                 size_t ber_sizeof_sequence_tag(size_t length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_read_bit_string",
	                                 BOOL ber_read_bit_string(wStream* s, size_t* length,
	                                                          BYTE* padding));

	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_read_octet_string_tag",
	                                 BOOL ber_read_octet_string_tag(wStream* s, size_t* length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_read_octet_string",
	                                 BOOL ber_read_octet_string(wStream* s, BYTE** content,
	                                                            size_t* length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_write_octet_string_tag",
	                                 size_t ber_write_octet_string_tag(wStream* s, size_t length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_sizeof_octet_string",
	                                 size_t ber_sizeof_octet_string(size_t length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_sizeof_contextual_octet_string",
	                                 size_t ber_sizeof_contextual_octet_string(size_t length));
	FREERDP_API WINPR_DEPRECATED_VAR(
	    "[since 3.9.0] use freerdp_ber_write_char_to_unicode_octet_string",
	    size_t ber_write_char_to_unicode_octet_string(wStream* s, const char* str));
	FREERDP_API WINPR_DEPRECATED_VAR(
	    "[since 3.9.0] use freerdp_ber_write_contextual_char_to_unicode_octet_string",
	    size_t ber_write_contextual_char_to_unicode_octet_string(wStream* s, BYTE tag,
	                                                             const char* oct_str));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_write_octet_string",
	                                 size_t ber_write_octet_string(wStream* s, const BYTE* oct_str,
	                                                               size_t length));
	FREERDP_API
	WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_read_char_from_unicode_octet_string",
	                     BOOL ber_read_char_from_unicode_octet_string(wStream* s, char** str));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_read_unicode_octet_string",
	                                 BOOL ber_read_unicode_octet_string(wStream* s, LPWSTR* str));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_write_contextual_octet_string",
	                                 size_t ber_write_contextual_octet_string(wStream* s, BYTE tag,
	                                                                          const BYTE* oct_str,
	                                                                          size_t length));
	FREERDP_API WINPR_DEPRECATED_VAR(
	    "[since 3.9.0] use freerdp_ber_write_contextual_unicode_octet_string",
	    size_t ber_write_contextual_unicode_octet_string(wStream* s, BYTE tag, const LPWSTR str));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_read_BOOL",
	                                 BOOL ber_read_BOOL(wStream* s, BOOL* value));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_write_BOOL",
	                                 void ber_write_BOOL(wStream* s, BOOL value));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_read_integer",
	                                 BOOL ber_read_integer(wStream* s, UINT32* value));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_write_integer",
	                                 size_t ber_write_integer(wStream* s, UINT32 value));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_write_contextual_integer",
	                                 size_t ber_write_contextual_integer(wStream* s, BYTE tag,
	                                                                     UINT32 value));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_read_integer_length",
	                                 BOOL ber_read_integer_length(wStream* s, size_t* length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_sizeof_integer",
	                                 size_t ber_sizeof_integer(UINT32 value));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_ber_sizeof_contextual_integer",
	                                 size_t ber_sizeof_contextual_integer(UINT32 value));

#ifdef __cplusplus
}
#endif
#endif

/**
 * @}
 */

#endif /* FREERDP_CRYPTO_BER_H */
