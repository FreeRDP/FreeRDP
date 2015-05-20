/**
 * WinPR: Windows Portable Runtime
 * Shell Functions
 *
 * Copyright 2015 Dell Software <Mike.McDonald@software.dell.com>
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

#ifndef WINPR_SHELL_H
#define WINPR_SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#ifndef _WIN32

#ifdef __cplusplus
extern "C" {
#endif

WINPR_API BOOL GetUserProfileDirectoryA(HANDLE hToken, LPSTR lpProfileDir, LPDWORD lpcchSize);

WINPR_API BOOL GetUserProfileDirectoryW(HANDLE hToken, LPWSTR lpProfileDir, LPDWORD lpcchSize);

#ifdef __cplusplus
}
#endif

#ifdef UNICODE
#define GetUserProfileDirectory GetUserProfileDirectoryW
#else
#define GetUserProfileDirectory GetUserProfileDirectoryA
#endif

#endif

#endif /* WINPR_SHELL_H */

