/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Addin Loader
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_COMMON_ADDIN_H
#define FREERDP_COMMON_ADDIN_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#define FREERDP_ADDIN_CLIENT			0x00000001
#define FREERDP_ADDIN_SERVER			0x00000002

#define FREERDP_ADDIN_STATIC			0x00000010
#define FREERDP_ADDIN_DYNAMIC			0x00000020

#define FREERDP_ADDIN_NAME			0x00000100
#define FREERDP_ADDIN_SUBSYSTEM			0x00000200
#define FREERDP_ADDIN_TYPE			0x00000400

#define FREERDP_ADDIN_CHANNEL_STATIC		0x00001000
#define FREERDP_ADDIN_CHANNEL_DYNAMIC		0x00002000
#define FREERDP_ADDIN_CHANNEL_DEVICE		0x00004000

struct _FREERDP_ADDIN
{
	DWORD dwFlags;
	CHAR cName[16];
	CHAR cType[16];
	CHAR cSubsystem[16];
};
typedef struct _FREERDP_ADDIN FREERDP_ADDIN;

typedef void* (*FREERDP_LOAD_CHANNEL_ADDIN_ENTRY_FN)(LPCSTR pszName, LPSTR pszSubsystem, LPSTR pszType, DWORD dwFlags);

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API LPSTR freerdp_get_library_install_path(void);
FREERDP_API LPSTR freerdp_get_dynamic_addin_install_path(void);

FREERDP_API int freerdp_register_addin_provider(FREERDP_LOAD_CHANNEL_ADDIN_ENTRY_FN provider, DWORD dwFlags);

FREERDP_API void* freerdp_load_dynamic_addin(LPCSTR pszFileName, LPCSTR pszPath, LPCSTR pszEntryName);
FREERDP_API void* freerdp_load_dynamic_channel_addin_entry(LPCSTR pszName, LPSTR pszSubsystem, LPSTR pszType, DWORD dwFlags);
FREERDP_API void* freerdp_load_channel_addin_entry(LPCSTR pszName, LPSTR pszSubsystem, LPSTR pszType, DWORD dwFlags);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_COMMON_ADDIN_H */

