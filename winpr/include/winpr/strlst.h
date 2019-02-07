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
*/
WINPR_API void string_list_free(char** string_list);

/**
* string_list_length return the number of strings in the string list.
*
* @param [in] string_list   A NULL-terminated array of strings.
*/
WINPR_API int string_list_length(char** string_list);

/**
* string_list_copy return a new (deep) copy of the string list.
*
* @param [in] string_list   A NULL-terminated array of strings.
*
* If any of the strings or the array itself cannot be allocated, then
* any allocated memory is freed,  and NULL is returned.
*/
WINPR_API char** string_list_copy(char** string_list);

/**
* string_list_print prints each string in the string list prefixed by its index.
*
* @param [in] out           The output file.
* @param [in] string_list   A NULL-terminated array of strings.
*/
WINPR_API void string_list_print(FILE* out, char** string_list);

/**
* string_list_join concatenates the strings in the string list, separated by the separator string.
*
* @result NULL when out of memory,  or a fresh NUL-terminated C string containing the concatenation.
* @param [in] string_list   A NULL-terminated array of strings.
* @param [in] separator     A C string.
*/
WINPR_API char* string_list_join(char** string_list, const char* separator);

/**
* string_concatenate concatenates the strings in the arguments, until NULL.
*
* @result NULL when out of memory,  or a fresh NUL-terminated C string containing the concatenation.
* @param [in] string   A NUL-terminated C string, or NULL.
* @param [in] ...      Other NUL-terminated C strings,  the last one must be NULL;
* @note The last string must be NULL,  not 0,  since on 64-bit, they're not the same parameter size!
*/
WINPR_API char* string_concatenate(const char* string, ...);

/**
* string_list_split_string splits a string into a string_list of substring,
* each substring is separated in string by the separator string.
*
* @note the result is NULL if and only if some memory couldn't be allocated.
*       if the string is empty,  then an empty string list is returned.
*       If the separtor is empty, then a string list containing the string is returned,
*       or an empty string list if the string is empty and remove_empty_subsrings is true.
*       All the strings in the returned string list are fresh strings.
*       The result can be freed by string_list_free().
*
* @param [in] string                  A C string.
* @param [in] separator               A C string.
* @param [in] remove_empty_substrings A boolean. If true, the empty substrings are not collected.
* @result A NULL-terminated array of strings containing the substrings.
*/
WINPR_API char** string_list_split_string(const char* string, const char* separator,
        int remove_empty_substrings);

/**
* string_list_mismatch compares the two string lists a and b.
*
* @note if a[result] == b[result] then a[result] == NULL and b[result] == NULL,  and a and b are equal.
*
* @param [in] a   A NULL-terminated array of strings.
* @param [in] b   A NULL-terminated array of strings.
* @result The index of the first element in a that is different in b.
*/
WINPR_API int string_list_mismatch(char** a, char** b);

/**
* string_list_equal compares the two string lists a and b.
*
* @param [in] a   A NULL-terminated array of strings.
* @param [in] b   A NULL-terminated array of strings.
* @result whether the two string lists contains the same strings in the same order.
*/
WINPR_API BOOL string_list_equal(char** a, char** b);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_UTILS_STRLST_H */
