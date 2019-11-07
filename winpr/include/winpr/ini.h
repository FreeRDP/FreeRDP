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

typedef struct _wIniFile wIniFile;

#ifdef __cplusplus
extern "C"
{
#endif

	WINPR_API int IniFile_ReadBuffer(wIniFile* ini, const char* buffer);
	WINPR_API int IniFile_ReadFile(wIniFile* ini, const char* filename);

	WINPR_API char* IniFile_WriteBuffer(wIniFile* ini);
	WINPR_API int IniFile_WriteFile(wIniFile* ini, const char* filename);

	WINPR_API char** IniFile_GetSectionNames(wIniFile* ini, int* count);
	WINPR_API char** IniFile_GetSectionKeyNames(wIniFile* ini, const char* section, int* count);

	WINPR_API const char* IniFile_GetKeyValueString(wIniFile* ini, const char* section,
	                                                const char* key);
	WINPR_API int IniFile_GetKeyValueInt(wIniFile* ini, const char* section, const char* key);

	WINPR_API int IniFile_SetKeyValueString(wIniFile* ini, const char* section, const char* key,
	                                        const char* value);
	WINPR_API int IniFile_SetKeyValueInt(wIniFile* ini, const char* section, const char* key,
	                                     int value);

	WINPR_API wIniFile* IniFile_New(void);
	WINPR_API void IniFile_Free(wIniFile* ini);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_UTILS_INI_H */
