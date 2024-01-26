/**
 * WinPR: Windows Portable Runtime
 * .ini config file
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_UTILS_INI_H
#define WINPR_UTILS_INI_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

typedef struct s_wIniFile wIniFile;

#ifdef __cplusplus
extern "C"
{
#endif

	/** @brief read an ini file from a buffer
	 *
	 *  @param ini The instance to use, must not be \b NULL
	 *  @param buffer The buffer to read from, must be a '\0' terminated string.
	 *
	 *  @return > 0 for success, < 0 for failure
	 */
	WINPR_API int IniFile_ReadBuffer(wIniFile* ini, const char* buffer);

	/** @brief read an ini file from a file
	 *
	 *  @param ini The instance to use, must not be \b NULL
	 *  @param filename The name of the file to read from, must be a '\0' terminated string.
	 *
	 *  @return > 0 for success, < 0 for failure
	 */
	WINPR_API int IniFile_ReadFile(wIniFile* ini, const char* filename);

	/** @brief write an ini instance to a buffer
	 *
	 *  @param ini The instance to use, must not be \b NULL
	 *
	 *  @return A newly allocated string, use \b free after use. \b NULL in case of failure
	 */
	WINPR_API char* IniFile_WriteBuffer(wIniFile* ini);

	/** @brief write an ini instance to a file
	 *
	 *  @param ini The instance to use, must not be \b NULL
	 *  @param filename The name of the file as '\0' terminated string.
	 *
	 *  @return > 0 for success, < 0 for failure
	 */
	WINPR_API int IniFile_WriteFile(wIniFile* ini, const char* filename);

	/** @brief Get the number and names of sections in the ini instance
	 *
	 *  @param ini The instance to use, must not be \b NULL
	 *  @param count A buffer that will contain the number of sections
	 *
	 *  @return A newly allocated array of strings (size \b count). Use \b free after use
	 */
	WINPR_API char** IniFile_GetSectionNames(wIniFile* ini, size_t* count);

	/** @brief Get the number and names of keys of a section in the ini instance
	 *
	 *  @param ini The instance to use, must not be \b NULL
	 *  @param section The name of the section as '\0' terminated string.
	 *  @param count A buffer that will contain the number of sections
	 *
	 *  @return A newly allocated array of strings (size \b count). Use \b free after use
	 */
	WINPR_API char** IniFile_GetSectionKeyNames(wIniFile* ini, const char* section, size_t* count);

	/** @brief Get an ini [section/key] value of type string
	 *
	 *  @param ini The instance to use, must not be \b NULL
	 *  @param section The name of the section as '\0' terminated string.
	 *  @param key The name of the key as '\0' terminated string.
	 *
	 *  @return The value of the [section/key] as '\0' terminated string or \b NULL
	 */
	WINPR_API const char* IniFile_GetKeyValueString(wIniFile* ini, const char* section,
	                                                const char* key);

	/** @brief Get an ini [section/key] value of type int
	 *
	 *  @param ini The instance to use, must not be \b NULL
	 *  @param section The name of the section as '\0' terminated string.
	 *  @param key The name of the key as '\0' terminated string.
	 *
	 *  @return The value of the [section/key]
	 */
	WINPR_API int IniFile_GetKeyValueInt(wIniFile* ini, const char* section, const char* key);

	/** @brief Set an ini [section/key] value of type string
	 *
	 *  @param ini The instance to use, must not be \b NULL
	 *  @param section The name of the section as '\0' terminated string.
	 *  @param key The name of the key as '\0' terminated string.
	 *  @param value The value of the [section/key] as '\0' terminated string.
	 *
	 *  @return > 0 for success, < 0 for failure
	 */
	WINPR_API int IniFile_SetKeyValueString(wIniFile* ini, const char* section, const char* key,
	                                        const char* value);

	/** @brief Set an ini [section/key] value of type int
	 *
	 *  @param ini The instance to use, must not be \b NULL
	 *  @param section The name of the section as '\0' terminated string.
	 *  @param key The name of the key as '\0' terminated string.
	 *  @param value The value of the [section/key]
	 *
	 *  @return > 0 for success, < 0 for failure
	 */
	WINPR_API int IniFile_SetKeyValueInt(wIniFile* ini, const char* section, const char* key,
	                                     int value);

	/** @brief Free a ini instance
	 *
	 *  @param ini The instance to free, may be \b NULL
	 */
	WINPR_API void IniFile_Free(wIniFile* ini);

	/** @brief Create a new ini instance
	 *
	 *  @return The newly allocated instance or \b NULL if failed.
	 */
	WINPR_ATTR_MALLOC(IniFile_Free, 1)
	WINPR_API wIniFile* IniFile_New(void);

	/** @brief Clone a ini instance
	 *
	 *  @param ini The instance to free, may be \b NULL
	 *
	 *  @return the cloned instance or \b NULL in case of \b ini was \b NULL or failure
	 */
	WINPR_API wIniFile* IniFile_Clone(const wIniFile* ini);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_UTILS_INI_H */
