/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Helper functions to parse encoded types into regular ones
 *
 * Copyright 2023 Pascal Nowack <Pascal.Nowack@gmx.de>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_UTILS_ENCODED_TYPES_H
#define FREERDP_UTILS_ENCODED_TYPES_H

#include <freerdp/api.h>
#include <winpr/stream.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define FREERDP_FOUR_BYTE_SIGNED_INT_MAX 0x1FFFFFFFl
#define FREERDP_FOUR_BYTE_SIGNED_INT_MIN -0x1FFFFFFFl

#define FREERDP_FOUR_BYTE_FLOAT_MAX 0x3FFFFFF
#define FREERDP_FOUR_BYTE_FLOAT_MIN -0x3FFFFFF

	FREERDP_API BOOL freerdp_read_four_byte_signed_integer(wStream* s, INT32* value);
	FREERDP_API BOOL freerdp_write_four_byte_signed_integer(wStream* s, INT32 value);

	FREERDP_API BOOL freerdp_read_four_byte_float(wStream* s, double* value);
	FREERDP_API BOOL freerdp_read_four_byte_float_exp(wStream* s, double* value, BYTE* exp);
	FREERDP_API BOOL freerdp_write_four_byte_float(wStream* s, double value);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_UTILS_ENCODED_TYPES_H */
