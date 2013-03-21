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

#include <freerdp/api.h>
#include <freerdp/types.h>


#include <winpr/stream.h>

/* ER type */

/* Class - bits 8 and 7 */
#define ER_CLASS_MASK		0xC0
#define ER_CLASS_UNIV		0x00 /* 0 0 */
#define ER_CLASS_APPL		0x40 /* 0 1 */
#define ER_CLASS_CTXT		0x80 /* 1 0 */
#define ER_CLASS_PRIV		0xC0 /* 1 1 */

/* P/C - bit 6 */
#define ER_PC_MASK		0x20
#define ER_PRIMITIVE		0x00 /* 0 */
#define ER_CONSTRUCT		0x20 /* 1 */

/* Tag - bits 5 to 1 */
#define ER_TAG_MASK		0x1F
#define ER_TAG_BOOLEAN		0x01
#define ER_TAG_INTEGER		0x02
#define ER_TAG_BIT_STRING	0x03
#define ER_TAG_OCTET_STRING	0x04
#define ER_TAG_OBJECT_IDENFIER	0x06
#define ER_TAG_ENUMERATED	0x0A
#define ER_TAG_SEQUENCE	0x10
#define ER_TAG_SEQUENCE_OF	0x10
#define ER_TAG_GENERAL_STRING   0x1B
#define ER_TAG_GENERALIZED_TIME 0x18

#define ER_PC(_pc)	(_pc ? ER_CONSTRUCT : ER_PRIMITIVE)

FREERDP_API void er_read_length(wStream* s, int* length);
FREERDP_API int er_write_length(wStream* s, int length, BOOL flag);
FREERDP_API int _er_skip_length(int length);
FREERDP_API int er_get_content_length(int length);
FREERDP_API BOOL er_read_universal_tag(wStream* s, BYTE tag, BOOL pc);
FREERDP_API void er_write_universal_tag(wStream* s, BYTE tag, BOOL pc);
FREERDP_API BOOL er_read_application_tag(wStream* s, BYTE tag, int* length);
FREERDP_API void er_write_application_tag(wStream* s, BYTE tag, int length, BOOL flag);
FREERDP_API BOOL er_read_application_tag(wStream* s, BYTE tag, int* length);
FREERDP_API BOOL er_read_enumerated(wStream* s, BYTE* enumerated, BYTE count);
FREERDP_API void er_write_enumerated(wStream* s, BYTE enumerated, BYTE count, BOOL flag);
FREERDP_API BOOL er_read_contextual_tag(wStream* s, BYTE tag, int* length, BOOL pc);
FREERDP_API int er_write_contextual_tag(wStream* s, BYTE tag, int length, BOOL pc, BOOL flag);
FREERDP_API int er_skip_contextual_tag(int length);
FREERDP_API BOOL er_read_sequence_tag(wStream* s, int* length);
FREERDP_API int er_write_sequence_tag(wStream* s, int length, BOOL flag);
FREERDP_API int er_skip_sequence(int length);
FREERDP_API int er_skip_sequence_tag(int length);
FREERDP_API BOOL er_read_bit_string(wStream* s, int* length, BYTE* padding);
FREERDP_API BOOL er_write_bit_string_tag(wStream* s, UINT32 length, BYTE padding, BOOL flag);
FREERDP_API BOOL er_read_octet_string(wStream* s, int* length);
FREERDP_API void er_write_octet_string(wStream* s, BYTE* oct_str, int length, BOOL flag);
FREERDP_API int er_write_octet_string_tag(wStream* s, int length, BOOL flag);
FREERDP_API int er_skip_octet_string(int length);
FREERDP_API BOOL er_read_BOOL(wStream* s, BOOL* value);
FREERDP_API void er_write_BOOL(wStream* s, BOOL value);
FREERDP_API BOOL er_read_integer(wStream* s, UINT32* value);
FREERDP_API int er_write_integer(wStream* s, INT32 value);
FREERDP_API BOOL er_read_integer_length(wStream* s, int* length);
FREERDP_API int er_skip_integer(INT32 value);

#endif /* FREERDP_CRYPTO_ER_H */
