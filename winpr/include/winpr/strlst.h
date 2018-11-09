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
extern "C" {
#endif

/**
* string_list_free frees each string in the string list.
*
* @param [in] string_list   A NULL-terminated array of strings.
*
*/
WINPR_API void string_list_free(char** string_list);

/**
* string_list_length return the number of strings in the string list.
*
* @param [in] string_list   A NULL-terminated array of strings.
*
*/
WINPR_API int string_list_length(const char* const* string_list);

/**
* string_list_copy return a new (deep) copy of the string list.
*
* @param [in] string_list   A NULL-terminated array of strings.
*
* If any of the strings or the array itself cannot be allocated, then
* any allocated memory is freed,  and NULL is returned.
*/
WINPR_API char** string_list_copy(const char* const* string_list);

/**
* string_list_print prints each string in the string list prefixed by its index.
*
* @param [in] out           The output file.
* @param [in] string_list   A NULL-terminated array of strings.
*
*/
WINPR_API void string_list_print(FILE* out, const char* const* string_list);

/**
* string_list_join concatenates each string in the string list, separated by the separator string.
*
* @param [in] string_list   A NULL-terminated array of strings.
* @param [in] separator     A C string.
*
*/
WINPR_API char* string_list_join(const char* const* string_list, const char* separator);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_UTILS_STRLST_H */
