/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDPDR utility functions
 *
 * Copyright 2022 Armin Novak <armin.novak@thincast.com>
 * Copyright 2022 Thincast Technologies GmbH
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

#ifndef FREERDP_UTILS_CLIPRDR_H
#define FREERDP_UTILS_CLIPRDR_H

#include <winpr/wtypes.h>
#include <winpr/shell.h>
#include <freerdp/api.h>

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API UINT cliprdr_parse_file_list(const BYTE* format_data, UINT32 format_data_length,
	                                         FILEDESCRIPTORW** file_descriptor_array,
	                                         UINT32* file_descriptor_count);
	FREERDP_API UINT cliprdr_serialize_file_list(const FILEDESCRIPTORW* file_descriptor_array,
	                                             UINT32 file_descriptor_count, BYTE** format_data,
	                                             UINT32* format_data_length);
	FREERDP_API UINT cliprdr_serialize_file_list_ex(UINT32 flags,
	                                                const FILEDESCRIPTORW* file_descriptor_array,
	                                                UINT32 file_descriptor_count,
	                                                BYTE** format_data, UINT32* format_data_length);

#ifdef __cplusplus
}
#endif

#endif
