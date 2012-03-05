/**
 * FreeRDP: A Remote Desktop Protocol Client
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
#include <freerdp/utils/memory.h>

#define der_read_length                           er_read_length
#define _der_skip_length                          _er_skip_length
#define der_get_content_length                    er_get_content_length
#define der_read_universal_tag                    er_read_universal_tag
#define der_write_universal_tag                   er_write_universal_tag
#define der_read_application_tag                  er_read_application_tag
#define der_read_enumerated                       er_read_enumerated
#define der_read_contextual_tag                   er_read_contextual_tag
#define der_skip_contextual_tag                   er_skip_contextual_tag
#define der_read_sequence_tag                     er_read_sequence_tag
#define der_skip_sequence                         er_skip_sequence
#define der_skip_sequence_tag                     er_skip_sequence_tag
#define der_read_bit_string                       er_read_bit_string
#define der_read_octet_string                     er_read_octet_string
#define der_skip_octet_string                     er_skip_octet_string
#define der_read_boolean                          er_read_boolean
#define der_write_boolean                         er_write_boolean
#define der_read_integer                          er_read_integer
#define der_write_integer                         er_write_integer
#define der_read_integer_length                   er_read_integer_length
#define der_skip_integer                          er_skip_integer
#define der_write_sequence_tag(_a, _b)            er_write_sequence_tag(_a, _b, true)
#define der_write_octet_string_tag(_a, _b)        er_write_octet_string_tag(_a, _b, true)
#define der_write_octet_string(_a, _b, _c)        er_write_octet_string(_a, _b, _c, true)
#define der_write_bit_string_tag(_a, _b, _c)      er_write_bit_string_tag(_a, _b, _c, true);
#define der_write_contextual_tag(_a, _b, _c, _d)  er_write_contextual_tag(_a, _b, _c, _d, true);
#define der_write_enumerated(_a, _b, _c)          er_write_enumerated(_a, _b, _c, true)
#define der_write_application_tag(_a, _b, _c)     er_write_application_tag(_a, _b, _c, true)

FREERDP_API int der_write_length(STREAM* s, int length);
FREERDP_API boolean der_write_bit_string(STREAM* s, uint32 length, uint8 padding);
FREERDP_API boolean der_write_general_string(STREAM* s, char* str);
FREERDP_API char* der_read_general_string(STREAM* s, int *length);
FREERDP_API int der_write_principal_name(STREAM* s, uint8 ntype, char** name);
FREERDP_API int der_write_generalized_time(STREAM* s, char* tstr);
FREERDP_API boolean der_read_generalized_time(STREAM* s, char** tstr);

#endif /* FREERDP_CRYPTO_DER_H */
