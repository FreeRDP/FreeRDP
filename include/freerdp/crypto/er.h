/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * ASN.1 Encoding Rules (BER/DER common functions)
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Modified by Jiten Pathy
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

#ifndef FREERDP_CRYPTO_ER_H
#define FREERDP_CRYPTO_ER_H

#include <winpr/wtypes.h>

#include <freerdp/api.h>
#include <freerdp/types.h>

#include <winpr/stream.h>

/**
 * \defgroup FreeRDP_ER ER read/write functions
 * @{
 */

/* ER type */

/* Class - bits 8 and 7 */
#define FREERDP_ER_CLASS_MASK 0xC0                             /** @since version 3.9.0 */
#define FREERDP_ER_CLASS_UNIV 0x00 /** @since version 3.9.0 */ /* 0 0 */
#define FREERDP_ER_CLASS_APPL 0x40 /** @since version 3.9.0 */ /* 0 1 */
#define FREERDP_ER_CLASS_CTXT 0x80 /** @since version 3.9.0 */ /* 1 0 */
#define FREERDP_ER_CLASS_PRIV 0xC0 /** @since version 3.9.0 */ /* 1 1 */

/* P/C - bit 6 */
#define FREERDP_ER_PC_MASK 0x20                               /** @since version 3.9.0 */
#define FREERDP_ER_PRIMITIVE 0x00 /** @since version 3.9.0 */ /* 0 */
#define FREERDP_ER_CONSTRUCT 0x20 /** @since version 3.9.0 */ /* 1 */

/* Tag - bits 5 to 1 */
#define FREERDP_ER_TAG_MASK 0x1F              /** @since version 3.9.0 */
#define FREERDP_ER_TAG_BOOLEAN 0x01           /** @since version 3.9.0 */
#define FREERDP_ER_TAG_INTEGER 0x02           /** @since version 3.9.0 */
#define FREERDP_ER_TAG_BIT_STRING 0x03        /** @since version 3.9.0 */
#define FREERDP_ER_TAG_OCTET_STRING 0x04      /** @since version 3.9.0 */
#define FREERDP_ER_TAG_OBJECT_IDENTIFIER 0x06 /** @since version 3.9.0 */
#define FREERDP_ER_TAG_ENUMERATED 0x0A        /** @since version 3.9.0 */
#define FREERDP_ER_TAG_SEQUENCE 0x10          /** @since version 3.9.0 */
#define FREERDP_ER_TAG_SEQUENCE_OF 0x10       /** @since version 3.9.0 */
#define FREERDP_ER_TAG_GENERAL_STRING 0x1B    /** @since version 3.9.0 */
#define FREERDP_ER_TAG_GENERALIZED_TIME 0x18  /** @since version 3.9.0 */

#define FREERDP_ER_PC(_pc) \
	(_pc ? FREERDP_ER_CONSTRUCT : FREERDP_ER_PRIMITIVE) /** @since version 3.9.0 */

#ifdef __cplusplus
extern "C"
{
#endif

	/**
	 * @brief freerdp_er_read_length
	 * @param s The stream to read from
	 * @param length A pointer receiving the lenght read
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API void freerdp_er_read_length(wStream* s, size_t* length);

	/**
	 * @brief freerdp_er_write_length
	 * @param s The stream to write to
	 * @param length The length to write
	 * @param flag \b TRUE write DER, \b FALSE write BER
	 * @return The number of bytes written
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_er_write_length(wStream* s, size_t length, BOOL flag);

	/**
	 * @brief freerdp_er_skip_length
	 * @param length The value of the length field
	 * @return The number of bytes used by the length field
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_er_skip_length(size_t length);

	/**
	 * @brief freerdp_er_get_content_length
	 * @param length Extract the contents length form the length of the field including headers
	 * @return The number of bytes used by contents
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_er_get_content_length(size_t length);

	/**
	 * @brief freerdp_er_read_universal_tag
	 * @param s The stream to read from
	 * @param tag A tag value to write to the header. Range limited by \b FREERDP_ER_TAG_MASK
	 * @param pc Type of tag. See \b FREERDP_ER_PC for details
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_er_read_universal_tag(wStream* s, BYTE tag, BOOL pc);

	/**
	 * @brief freerdp_er_write_universal_tag
	 * @param s The stream to write to
	 * @param tag A tag value to write to the header. Range limited by \b FREERDP_ER_TAG_MASK
	 * @param pc Type of tag. See \b FREERDP_ER_PC for details
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API void freerdp_er_write_universal_tag(wStream* s, BYTE tag, BOOL pc);

	/**
	 * @brief freerdp_er_read_application_tag
	 * @param s The stream to read from
	 * @param tag A tag value to write to the header. Range limited by \b FREERDP_ER_TAG_MASK
	 * @param length A pointer receiving the length of the field
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_er_read_application_tag(wStream* s, BYTE tag, size_t* length);

	/**
	 * @brief freerdp_er_write_application_tag
	 * @param s The stream to write to
	 * @param tag A tag value to write to the header. Range limited by \b FREERDP_ER_TAG_MASK
	 * @param length Content length in bytes
	 * @param flag \b TRUE write DER, \b FALSE write BER
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API void freerdp_er_write_application_tag(wStream* s, BYTE tag, size_t length,
	                                                  BOOL flag);

	/**
	 * @brief freerdp_er_read_enumerated
	 * @param s The stream to read from
	 * @param enumerated A pointer receiving the enumerated value
	 * @param count The upper bound for expected enumerated values
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_er_read_enumerated(wStream* s, BYTE* enumerated, BYTE count);

	/**
	 * @brief freerdp_er_write_enumerated
	 * @param s The stream to write to
	 * @param enumerated The enumerated value to write
	 * @param count \b UNUSED
	 * @param flag \b TRUE write DER, \b FALSE write BER
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API void freerdp_er_write_enumerated(wStream* s, BYTE enumerated, BYTE count,
	                                             BOOL flag);

	/**
	 * @brief freerdp_er_read_contextual_tag
	 * @param s The stream to read from
	 * @param tag A tag value to write to the header. Range limited by \b FREERDP_ER_TAG_MASK
	 * @param length A pointer receiving the length of the field
	 * @param pc Type of tag. See \b FREERDP_ER_PC for details
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_er_read_contextual_tag(wStream* s, BYTE tag, size_t* length, BOOL pc);

	/**
	 * @brief freerdp_er_write_contextual_tag
	 * @param s The stream to write to
	 * @param tag A tag value to write to the header. Range limited by \b FREERDP_ER_TAG_MASK
	 * @param length Content length in bytes
	 * @param pc Type of tag. See \b FREERDP_ER_PC for details
	 * @param flag \b TRUE write DER, \b FALSE write BER
	 * @return Number of bytes used by tag
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_er_write_contextual_tag(wStream* s, BYTE tag, size_t length, BOOL pc,
	                                                   BOOL flag);

	/**
	 * @brief freerdp_er_skip_contextual_tag
	 * @param length The content length of the tag
	 * @return Number of bytes used by tag
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_er_skip_contextual_tag(size_t length);

	/**
	 * @brief freerdp_er_read_sequence_tag
	 * @param s The stream to read from
	 * @param length A pointer receiving the length of the field
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_er_read_sequence_tag(wStream* s, size_t* length);

	/**
	 * @brief freerdp_er_write_sequence_tag
	 * @param s The stream to write to
	 * @param length Content length in bytes
	 * @param flag \b TRUE write DER, \b FALSE write BER
	 * @return Number of bytes written
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_er_write_sequence_tag(wStream* s, size_t length, BOOL flag);

	/**
	 * @brief freerdp_er_skip_sequence
	 * @param length The content length of the tag
	 * @return Number of bytes used by sequence and headers
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_er_skip_sequence(size_t length);

	/**
	 * @brief freerdp_er_skip_sequence_tag
	 * @param length The content length of the tag
	 * @return Number of bytes used by tag
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_er_skip_sequence_tag(size_t length);

	/**
	 * @brief freerdp_er_read_bit_string
	 * @param s The stream to read from
	 * @param length A pointer receiving the length of the field
	 * @param padding A pointer receiving the value of the padding byte
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_er_read_bit_string(wStream* s, size_t* length, BYTE* padding);

	/**
	 * @brief freerdp_er_write_bit_string_tag
	 * @param s The stream to write to
	 * @param length Content length in bytes
	 * @param padding Value of padding byte to write after the field.
	 * @param flag \b TRUE write DER, \b FALSE write BER
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_er_write_bit_string_tag(wStream* s, UINT32 length, BYTE padding,
	                                                 BOOL flag);

	/**
	 * @brief freerdp_er_read_octet_string
	 * @param s The stream to read from
	 * @param length A pointer receiving the length of the string
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_er_read_octet_string(wStream* s, size_t* length);

	/**
	 * @brief freerdp_er_write_octet_string
	 * @param s The stream to write to
	 * @param oct_str A pointer to the string to write
	 * @param length The byte length of the string
	 * @param flag \b TRUE write DER, \b FALSE write BER
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API void freerdp_er_write_octet_string(wStream* s, const BYTE* oct_str, size_t length,
	                                               BOOL flag);

	/**
	 * @brief freerdp_er_write_octet_string_tag
	 * @param s The stream to write to
	 * @param length The length of the string
	 * @param flag \b TRUE write DER, \b FALSE write BER
	 * @return The number of bytes written
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_er_write_octet_string_tag(wStream* s, size_t length, BOOL flag);

	/**
	 * @brief freerdp_er_skip_octet_string
	 * @param length The length of the contents
	 * @return The number of bytes to skip including headers
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_er_skip_octet_string(size_t length);

	/**
	 * @brief freerdp_er_read_BOOL
	 * @param s The stream to read from
	 * @param value A pointer receiving the BOOL value read
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_er_read_BOOL(wStream* s, BOOL* value);

	/**
	 * @brief freerdp_er_write_BOOL
	 * @param s The stream to write to
	 * @param value The value to write
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API void freerdp_er_write_BOOL(wStream* s, BOOL value);

	/**
	 * @brief freerdp_er_read_integer
	 * @param s The stream to read from
	 * @param value A pointer receiving the integer value read
	 * @return Number of bytes read
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_er_read_integer(wStream* s, INT32* value);

	/**
	 * @brief freerdp_er_write_integer
	 * @param s The stream to write to
	 * @param value The integer value to write
	 * @return Number of bytes written
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_er_write_integer(wStream* s, INT32 value);

	/**
	 * @brief freerdp_er_read_integer_length
	 * @param s The stream to read from
	 * @param length receives the byte length of the integer and headers
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_er_read_integer_length(wStream* s, size_t* length);

	/**
	 * @brief freerdp_er_skip_integer
	 * @param value The value skip
	 * @return The bytes to skip including the header bytes
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_er_skip_integer(INT32 value);

#ifdef __cplusplus
}
#endif

#if defined(WITH_FREERDP_3x_DEPRECATED)
/* Class - bits 8 and 7 */
#define ER_CLASS_MASK 0xC0
#define ER_CLASS_UNIV 0x00 /* 0 0 */
#define ER_CLASS_APPL 0x40 /* 0 1 */
#define ER_CLASS_CTXT 0x80 /* 1 0 */
#define ER_CLASS_PRIV 0xC0 /* 1 1 */

/* P/C - bit 6 */
#define ER_PC_MASK 0x20
#define ER_PRIMITIVE 0x00 /* 0 */
#define ER_CONSTRUCT 0x20 /* 1 */

/* Tag - bits 5 to 1 */
#define ER_TAG_MASK 0x1F
#define ER_TAG_BOOLEAN 0x01
#define ER_TAG_INTEGER 0x02
#define ER_TAG_BIT_STRING 0x03
#define ER_TAG_OCTET_STRING 0x04
#define ER_TAG_OBJECT_IDENTIFIER 0x06
#define ER_TAG_ENUMERATED 0x0A
#define ER_TAG_SEQUENCE 0x10
#define ER_TAG_SEQUENCE_OF 0x10
#define ER_TAG_GENERAL_STRING 0x1B
#define ER_TAG_GENERALIZED_TIME 0x18

#define ER_PC(_pc) (_pc ? ER_CONSTRUCT : ER_PRIMITIVE)

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_read_length",
	                                 void er_read_length(wStream* s, int* length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_write_length",
	                                 int er_write_length(wStream* s, int length, BOOL flag));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp__er_skip_length",
	                                 int _er_skip_length(int length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_get_content_length",
	                                 int er_get_content_length(int length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_read_universal_tag",
	                                 BOOL er_read_universal_tag(wStream* s, BYTE tag, BOOL pc));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_write_universal_tag",
	                                 void er_write_universal_tag(wStream* s, BYTE tag, BOOL pc));
	FREERDP_API
	WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_read_application_tag",
	                     BOOL er_read_application_tag(wStream* s, BYTE tag, int* length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_write_application_tag",
	                                 void er_write_application_tag(wStream* s, BYTE tag, int length,
	                                                               BOOL flag));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_read_enumerated",
	                                 BOOL er_read_enumerated(wStream* s, BYTE* enumerated,
	                                                         BYTE count));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_write_enumerated",
	                                 void er_write_enumerated(wStream* s, BYTE enumerated,
	                                                          BYTE count, BOOL flag));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_read_contextual_tag",
	                                 BOOL er_read_contextual_tag(wStream* s, BYTE tag, int* length,
	                                                             BOOL pc));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_write_contextual_tag",
	                                 int er_write_contextual_tag(wStream* s, BYTE tag, int length,
	                                                             BOOL pc, BOOL flag));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_skip_contextual_tag",
	                                 int er_skip_contextual_tag(int length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_read_sequence_tag",
	                                 BOOL er_read_sequence_tag(wStream* s, int* length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_write_sequence_tag",
	                                 int er_write_sequence_tag(wStream* s, int length, BOOL flag));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_skip_sequence",
	                                 int er_skip_sequence(int length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_skip_sequence_tag",
	                                 int er_skip_sequence_tag(int length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_read_bit_string",
	                                 BOOL er_read_bit_string(wStream* s, int* length,
	                                                         BYTE* padding));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_write_bit_string_tag",
	                                 BOOL er_write_bit_string_tag(wStream* s, UINT32 length,
	                                                              BYTE padding, BOOL flag));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_read_octet_string",
	                                 BOOL er_read_octet_string(wStream* s, int* length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_write_octet_string",
	                                 void er_write_octet_string(wStream* s, BYTE* oct_str,
	                                                            int length, BOOL flag));
	FREERDP_API
	WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_write_octet_string_tag",
	                     int er_write_octet_string_tag(wStream* s, int length, BOOL flag));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_skip_octet_string",
	                                 int er_skip_octet_string(int length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_read_BOOL",
	                                 BOOL er_read_BOOL(wStream* s, BOOL* value));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_write_BOOL",
	                                 void er_write_BOOL(wStream* s, BOOL value));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_read_integer",
	                                 BOOL er_read_integer(wStream* s, INT32* value));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_write_integer",
	                                 int er_write_integer(wStream* s, INT32 value));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_read_integer_length",
	                                 BOOL er_read_integer_length(wStream* s, int* length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_er_skip_integer",
	                                 int er_skip_integer(INT32 value));

#ifdef __cplusplus
}
#endif
#endif

/**
 * @}
 */

#endif /* FREERDP_CRYPTO_ER_H */
