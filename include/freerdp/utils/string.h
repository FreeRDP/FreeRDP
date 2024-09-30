/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * String Utils - Helper functions converting something to string
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

#ifndef FREERDP_UTILS_STRING_H
#define FREERDP_UTILS_STRING_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API const char* rdp_redirection_flags_to_string(UINT32 flags, char* buffer,
	                                                        size_t size);
	FREERDP_API const char* rdp_cluster_info_flags_to_string(UINT32 flags, char* buffer,
	                                                         size_t size);

	/** @brief extracts <key>=<value> pairs from a string
	 *
	 * @param str The string to extract data from, must not be \b NULL
	 * @param pkey A pointer to store the key value, must not be \b NULL
	 * @param pvalue A pointer to store the value, must not be \b NULL
	 *
	 * @return \b TRUE if successfully extracted, \b FALSE if no matching data was found
	 *
	 * @since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_extract_key_value(const char* str, UINT32* pkey, UINT32* pvalue);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_UTILS_STRING_H */
