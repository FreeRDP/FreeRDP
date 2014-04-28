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

#ifndef WINPR_SMARTCARD_PRIVATE_H
#define WINPR_SMARTCARD_PRIVATE_H

#include <winpr/smartcard.h>

#define SCARDAPI_STUB_CALL_LONG(_name, ...) \
	if (!g_Initialized) \
		InitializeSCardApiStubs(); \
	if (!g_SCardApi || !g_SCardApi->pfn ## _name) \
		return SCARD_E_NO_SERVICE; \
	return g_SCardApi->pfn ## _name ( __VA_ARGS__ )

#define SCARDAPI_STUB_CALL_HANDLE(_name, ...) \
	if (!g_Initialized) \
		InitializeSCardApiStubs(); \
	if (!g_SCardApi || !g_SCardApi->pfn ## _name) \
		return NULL; \
	return g_SCardApi->pfn ## _name ( __VA_ARGS__ )

#define SCARDAPI_STUB_CALL_VOID(_name, ...) \
	if (!g_Initialized) \
		InitializeSCardApiStubs(); \
	if (!g_SCardApi || !g_SCardApi->pfn ## _name) \
		return; \
	g_SCardApi->pfn ## _name ( __VA_ARGS__ )

void InitializeSCardApiStubs(void);

#ifndef _WIN32
#include "smartcard_pcsc.h"
#else
#include "smartcard_winscard.h"
#endif

#endif /* WINPR_SMARTCARD_PRIVATE_H */
