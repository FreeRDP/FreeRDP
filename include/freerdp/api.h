/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Interface
 *
 * Copyright 2009-2011 Jay Sorg
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

#ifndef FREERDP_API_H
#define FREERDP_API_H

#include <winpr/winpr.h>
#include <winpr/wlog.h>
#include <winpr/platform.h>

/* To silence missing prototype warnings for library entry points use this macro.
 * It first declares the function as prototype and then again as function implementation
 */
#define FREERDP_ENTRY_POINT(fkt) \
	fkt;                         \
	fkt

#ifdef _WIN32
#define FREERDP_CC __cdecl
#else
#define FREERDP_CC
#endif

#if defined _WIN32 || defined __CYGWIN__
#ifdef FREERDP_EXPORTS
#ifdef __GNUC__
#define FREERDP_API __attribute__((dllexport))
#else
#define FREERDP_API __declspec(dllexport)
#endif
#else
#ifdef __GNUC__
#define FREERDP_API __attribute__((dllimport))
#else
#define FREERDP_API __declspec(dllimport)
#endif
#endif
#else
#if defined(__GNUC__) && (__GNUC__ >= 4)
#define FREERDP_API __attribute__((visibility("default")))
#else
#define FREERDP_API
#endif
#endif

#if defined(EXPORT_ALL_SYMBOLS)
#define FREERDP_LOCAL FREERDP_API
#else
#if defined _WIN32 || defined __CYGWIN__
#define FREERDP_LOCAL
#else
#if defined(__GNUC__) && (__GNUC__ >= 4)
#define FREERDP_LOCAL __attribute__((visibility("hidden")))
#else
#define FREERDP_LOCAL
#endif
#endif
#endif

#define IFCALL(_cb, ...)                                             \
	do                                                               \
	{                                                                \
		if (_cb != NULL)                                             \
			_cb(__VA_ARGS__);                                        \
		else                                                         \
			WLog_VRB("com.freerdp.api", "IFCALL(" #_cb ") == NULL"); \
	} while (0)
#define IFCALLRET(_cb, _ret, ...)                                       \
	do                                                                  \
	{                                                                   \
		if (_cb != NULL)                                                \
			_ret = _cb(__VA_ARGS__);                                    \
		else                                                            \
			WLog_VRB("com.freerdp.api", "IFCALLRET(" #_cb ") == NULL"); \
	} while (0)

#if 0 // defined(__GNUC__)
#define IFCALLRESULT(_default_return, _cb, ...)                            \
	({                                                                     \
		(_cb != NULL) ? _cb(__VA_ARGS__) : ({                              \
			WLog_VRB("com.freerdp.api", "IFCALLRESULT(" #_cb ") == NULL"); \
			(_default_return);                                             \
		});                                                                \
	})
#else
#define IFCALLRESULT(_default_return, _cb, ...) \
	((_cb != NULL) ? _cb(__VA_ARGS__) : (_default_return))
#endif

#if defined(__GNUC__) || defined(__clang__)
#define ALIGN64 __attribute__((aligned(8)))
#else
#ifdef _WIN32
#define ALIGN64 __declspec(align(8))
#else
#define ALIGN64
#endif
#endif

#endif /* FREERDP_API */
