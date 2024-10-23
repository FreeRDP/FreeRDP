/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * ASN.1 Packed Encoding Rules (BER)
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

#ifndef FREERDP_CRYPTO_PER_H
#define FREERDP_CRYPTO_PER_H

#include <freerdp/api.h>

#include <winpr/stream.h>

/**
 * \defgroup FreeRDP_PER PER read/write functions
 * @{
 */

#ifdef __cplusplus
extern "C"
{
#endif

	/**
	 * @brief Read PER length.
	 * @param s The stream to read from
	 * @param length A pointer to return the length read, must not be NULL
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_per_read_length(wStream* s, UINT16* length);

	/**
	 * @brief Write PER length.
	 * @param s The stream to write to
	 * @param length The lenght value to write
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_per_write_length(wStream* s, UINT16 length);

	/**
	 * @brief Read PER CHOICE.
	 * @param s The stream to read from
	 * @param choice A pointer receiving the choice value
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_per_read_choice(wStream* s, BYTE* choice);

	/**
	 * @brief Write PER CHOICE.
	 * @param s The stream to write to
	 * @param choice The choice value to write
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_per_write_choice(wStream* s, BYTE choice);

	/**
	 * @brief Read PER selection for OPTIONAL fields.
	 * @param s The stream to read from
	 * @param selection A pointer receiving the selection value read
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_per_read_selection(wStream* s, BYTE* selection);

	/**
	 * @brief Write PER selection for OPTIONAL fields.
	 * @param s The stream to write to
	 * @param selection The selection value to write
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_per_write_selection(wStream* s, BYTE selection);

	/**
	 * @brief Read PER number of sets for SET OF.
	 * @param s The stream to read from
	 * @param number A pointer receiving the number value read
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_per_read_number_of_sets(wStream* s, BYTE* number);

	/**
	 * @brief Write PER number of sets for SET OF.
	 * @param s The stream to write to
	 * @param number The number value to write
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_per_write_number_of_sets(wStream* s, BYTE number);

	/**
	 * @brief Read PER padding with zeros.
	 * @param s The stream to read from
	 * @param length The length of the padding
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_per_read_padding(wStream* s, UINT16 length);

	/**
	 * @brief Write PER padding with zeros.
	 * @param s The stream to write to
	 * @param length The length of the padding
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_per_write_padding(wStream* s, UINT16 length);

	/**
	 * @brief Read PER INTEGER.
	 * @param s The stream to read from
	 * @param integer The integer result variable pointer, must not be NULL
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_per_read_integer(wStream* s, UINT32* integer);

	/**
	 * @brief Read PER INTEGER (UINT16).
	 * @param s The stream to read from
	 * @param integer The integer result variable pointer, must not be NULL
	 * @param min The smallest value expected, will be added to the value read
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_per_read_integer16(wStream* s, UINT16* integer, UINT16 min);

	/**
	 * @brief Write PER INTEGER.
	 * @param s The stream to write to
	 * @param integer The integer value to write
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_per_write_integer(wStream* s, UINT32 integer);

	/**
	 * @brief Read PER INTEGER(UINT16).
	 * @param s The stream to write to
	 * @param integer The integer value to write
	 * @param min The smallest value supported, will be subtracted from \b integer
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_per_write_integer16(wStream* s, UINT16 integer, UINT16 min);

	/**
	 * @brief Read PER ENUMERATED.
	 * @param s The stream to read from
	 * @param enumerated enumerated result variable, must not be NULL
	 * @param count enumeration count
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_per_read_enumerated(wStream* s, BYTE* enumerated, BYTE count);

	/**
	 * @brief Write PER ENUMERATED.
	 * @param s The stream to write to
	 * @param enumerated enumerated variable
	 * @param count enumeration count
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_per_write_enumerated(wStream* s, BYTE enumerated, BYTE count);

	/**
	 * @brief Write PER OBJECT_IDENTIFIER (OID).
	 * @param s The stream to write to
	 * @param oid object identifier (oid)
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_per_write_object_identifier(wStream* s, const BYTE oid[6]);

	/**
	 * @brief Read PER OBJECT_IDENTIFIER (OID).
	 * @param s The stream to read from
	 * @param oid object identifier (oid)
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_per_read_object_identifier(wStream* s, const BYTE oid[6]);

	/**
	 * @brief Read PER OCTET_STRING.
	 * @param s The stream to read from
	 * @param oct_str octet string
	 * @param length string length
	 * @param min minimum length
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_per_read_octet_string(wStream* s, const BYTE* oct_str, UINT16 length,
	                                               UINT16 min);

	/**
	 * @brief Write PER OCTET_STRING.
	 * @param s The stream to write to
	 * @param oct_str octet string
	 * @param length string length
	 * @param min minimum string length
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_per_write_octet_string(wStream* s, const BYTE* oct_str, UINT16 length,
	                                                UINT16 min);

	/**
	 * @brief Read PER NumericString.
	 * @param s The stream to read from
	 * @param min minimum string length
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_per_read_numeric_string(wStream* s, UINT16 min);

	/**
	 * @brief Write PER NumericString.
	 * @param s The stream to write to
	 * @param num_str numeric string
	 * @param length string length
	 * @param min minimum string length
	 * @return \b TRUE for success, \b FALSE otherwise
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_per_write_numeric_string(wStream* s, const BYTE* num_str,
	                                                  UINT16 length, UINT16 min);

#if defined(WITH_FREERDP_3x_DEPRECATED)
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_per_read_length",
	                                 BOOL per_read_length(wStream* s, UINT16* length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_per_write_length",
	                                 BOOL per_write_length(wStream* s, UINT16 length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_per_read_choice",
	                                 BOOL per_read_choice(wStream* s, BYTE* choice));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_per_write_choice",
	                                 BOOL per_write_choice(wStream* s, BYTE choice));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_per_read_selection",
	                                 BOOL per_read_selection(wStream* s, BYTE* selection));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_per_write_selection",
	                                 BOOL per_write_selection(wStream* s, BYTE selection));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_per_read_number_of_sets",
	                                 BOOL per_read_number_of_sets(wStream* s, BYTE* number));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_per_write_number_of_sets",
	                                 BOOL per_write_number_of_sets(wStream* s, BYTE number));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_per_read_padding",
	                                 BOOL per_read_padding(wStream* s, UINT16 length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_per_write_padding",
	                                 BOOL per_write_padding(wStream* s, UINT16 length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_per_read_integer",
	                                 BOOL per_read_integer(wStream* s, UINT32* integer));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_per_read_integer16",
	                                 BOOL per_read_integer16(wStream* s, UINT16* integer,
	                                                         UINT16 min));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_per_write_integer",
	                                 BOOL per_write_integer(wStream* s, UINT32 integer));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_per_write_integer16",
	                                 BOOL per_write_integer16(wStream* s, UINT16 integer,
	                                                          UINT16 min));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_per_read_enumerated",
	                                 BOOL per_read_enumerated(wStream* s, BYTE* enumerated,
	                                                          BYTE count));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_per_write_enumerated",
	                                 BOOL per_write_enumerated(wStream* s, BYTE enumerated,
	                                                           BYTE count));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_per_write_object_identifier",
	                                 BOOL per_write_object_identifier(wStream* s,
	                                                                  const BYTE oid[6]));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_per_read_object_identifier",
	                                 BOOL per_read_object_identifier(wStream* s,
	                                                                 const BYTE oid[6]));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_per_read_octet_string",
	                                 BOOL per_read_octet_string(wStream* s, const BYTE* oct_str,
	                                                            UINT16 length, UINT16 min));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_per_write_octet_string",
	                                 BOOL per_write_octet_string(wStream* s, const BYTE* oct_str,
	                                                             UINT16 length, UINT16 min));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_per_read_numeric_string",
	                                 BOOL per_read_numeric_string(wStream* s, UINT16 min));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_per_write_numeric_string",
	                                 BOOL per_write_numeric_string(wStream* s, const BYTE* num_str,
	                                                               UINT16 length, UINT16 min));
#endif

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* FREERDP_CRYPTO_PER_H */
