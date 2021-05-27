/**
 * WinPR: Windows Portable Runtime
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

#ifndef WINPR_H
#define WINPR_H

#include <winpr/platform.h>

#ifdef WINPR_DLL
#if defined _WIN32 || defined __CYGWIN__
#ifdef WINPR_EXPORTS
#ifdef __GNUC__
#define WINPR_API __attribute__((dllexport))
#else
#define WINPR_API __declspec(dllexport)
#endif
#else
#ifdef __GNUC__
#define WINPR_API __attribute__((dllimport))
#else
#define WINPR_API __declspec(dllimport)
#endif
#endif
#else
#if __GNUC__ >= 4
#define WINPR_API __attribute__((visibility("default")))
#else
#define WINPR_API
#endif
#endif
#else /* WINPR_DLL */
#define WINPR_API
#endif

#if defined(WIN32) && !defined(__CYGWIN__)
#define WINPR_DEPRECATED(obj) __declspec(deprecated) obj
#elif defined(__GNUC__)
#define WINPR_DEPRECATED(obj) obj __attribute__((deprecated))
#else
#define WINPR_DEPRECATED(obj) obj
#endif

// WARNING: *do not* use thread-local storage for new code because it is not portable
// It is only used for VirtualChannelInit, and all FreeRDP channels use VirtualChannelInitEx
// The old virtual channel API is only realistically used on Windows where TLS is available
#if defined _WIN32 || defined __CYGWIN__
#ifdef __GNUC__
#define WINPR_TLS __thread
#else
#define WINPR_TLS __declspec(thread)
#endif
#elif !defined(__IOS__)
#define WINPR_TLS __thread
#else
// thread-local storage is not supported on iOS
// don't warn because it isn't actually used on iOS
#define WINPR_TLS
#endif

#ifdef _WIN32
#define INLINE __inline
#else
#define INLINE inline
#endif

WINPR_API void winpr_get_version(int* major, int* minor, int* revision);
WINPR_API const char* winpr_get_version_string(void);
WINPR_API const char* winpr_get_build_revision(void);
WINPR_API const char* winpr_get_build_config(void);

#define WINPR_UNUSED(x) (void)(x)

#endif /* WINPR_H */
