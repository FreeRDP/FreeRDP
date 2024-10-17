/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * ASN.1 Basic Encoding Rules (DER)
 *
 * Copyright 2011 Samsung, Author Jiten Pathy
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

#ifndef FREERDP_CRYPTO_DER_H
#define FREERDP_CRYPTO_DER_H

#include <freerdp/crypto/er.h>

/**
 * \defgroup FreeRDP_DER DER read/write functions
 * @{
 */

#ifdef __cplusplus
extern "C"
{
#endif

	/**
	 * @brief freerdp_der_skip_length
	 * @param length The length of the sequence in bytes, limited to \b UINT16_MAX
	 * @return Number of bytes skipped
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_der_skip_length(size_t length);

	/**
	 * @brief freerdp_der_write_length
	 * @param s The stream to write to
	 * @param length The length of the sequence in bytes, limited to \b UINT16_MAX
	 * @return Number of bytes written
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_der_write_length(wStream* s, size_t length);

	/**
	 * @brief freerdp_der_get_content_length
	 * @param length The length of the sequence in bytes, limited to \b UINT16_MAX
	 * @return Length of the contents without header bytes
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_der_get_content_length(size_t length);

	/**
	 * @brief freerdp_der_skip_octet_string
	 * @param length The length of the sequence in bytes, limited to \b UINT16_MAX
	 * @return Number of bytes skipped
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_der_skip_octet_string(size_t length);

	/**
	 * @brief freerdp_der_skip_sequence_tag
	 * @param length The length of the sequence in bytes, limited to \b UINT16_MAX
	 * @return The number of bytes skipped.
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_der_skip_sequence_tag(size_t length);

	/**
	 * @brief freerdp_der_write_sequence_tag
	 * @param s The stream to write to.
	 * @param length The length of the sequence in bytes, limited to \b UINT16_MAX
	 * @return The number of bytes written
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_der_write_sequence_tag(wStream* s, size_t length);

	/**
	 * @brief freerdp_der_skip_contextual_tag
	 * @param length The length of the sequence in bytes, limited to \b UINT16_MAX
	 * @return Number of bytes skipped
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_der_skip_contextual_tag(size_t length);

	/**
	 * @brief freerdp_der_write_contextual_tag
	 * @param s The stream to write to
	 * @param tag A tag to write
	 * @param length The length of the sequence in bytes, limited to \b UINT16_MAX
	 * @param pc Switch types, \b TRUE for \b ER_CONSTRUCT else \b ER_PRIMITIVE
	 * @return Number of bytes written
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API size_t freerdp_der_write_contextual_tag(wStream* s, BYTE tag, size_t length,
	                                                    BOOL pc);

	/**
	 * @brief freerdp_der_write_octet_string
	 * @param s The stream to write to
	 * @param oct_str The string to write
	 * @param length The length of the sequence in bytes, limited to \b UINT16_MAX
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API void freerdp_der_write_octet_string(wStream* s, const BYTE* oct_str, size_t length);

#if defined(WITH_FREERDP_3x_DEPRECATED)
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_der_skip_length",
	                                 int _der_skip_length(int length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_der_write_length",
	                                 int der_write_length(wStream* s, int length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_der_get_content_length",
	                                 int der_get_content_length(int length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_der_skip_octet_string",
	                                 int der_skip_octet_string(int length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_der_skip_sequence_tag",
	                                 int der_skip_sequence_tag(int length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_der_write_sequence_tag",
	                                 int der_write_sequence_tag(wStream* s, int length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_der_skip_contextual_tag",
	                                 int der_skip_contextual_tag(int length));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_der_write_contextual_tag",
	                                 int der_write_contextual_tag(wStream* s, BYTE tag, int length,
	                                                              BOOL pc));
	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.9.0] use freerdp_der_write_octet_string",
	                                 void der_write_octet_string(wStream* s, const BYTE* oct_str,
	                                                             int length));
#endif

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* FREERDP_CRYPTO_DER_H */
