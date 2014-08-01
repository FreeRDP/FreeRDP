/**
 * WinPR: Windows Portable Runtime
 * Smart Card API
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

#ifndef WINPR_SMARTCARD_WINSCARD_PRIVATE_H
#define WINPR_SMARTCARD_WINSCARD_PRIVATE_H

#ifdef _WIN32

#include <winpr/smartcard.h>

#define WINSCARD_LOAD_PROC(_name, ...) \
	WinSCard_SCardApiFunctionTable.pfn ## _name = (fn ## _name) GetProcAddress(g_WinSCardModule, #_name);

int WinSCard_InitializeSCardApi(void);
PSCardApiFunctionTable WinSCard_GetSCardApiFunctionTable(void);

#endif

#endif /* WINPR_SMARTCARD_WINSCARD_PRIVATE_H */
