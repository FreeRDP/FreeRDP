/**
 * String list Manipulation (UTILS)
 *
 * Copyright 2018 Pascal Bourguignon <pjb@informatimago.com>
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

#ifndef WINPR_UTILS_STRLST_H
#define WINPR_UTILS_STRLST_H

#include <stdio.h>

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#ifdef __cplusplus
extern "C"
{
#endif

	WINPR_API void string_list_free(char** string_list);
	WINPR_API int string_list_length(const char* const* string_list);
	WINPR_API char** string_list_copy(const char* const* string_list);
	WINPR_API void string_list_print(FILE* out, const char* const* string_list);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_UTILS_STRLST_H */
