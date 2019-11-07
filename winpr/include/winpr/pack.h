/**
 * WinPR: Windows Portable Runtime
 * Pragma Pack
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

/**
 * This header is meant to be repeatedly included
 * after defining the operation to be done:
 *
 * #define WINPR_PACK_PUSH
 * #include <winpr/pack.h> // enables packing
 *
 * #define WINPR_PACK_POP
 * #include <winpr/pack.h> // disables packing
 *
 * On each include, WINPR_PACK_* macros are undefined.
 */

#if !defined(__APPLE__)
#ifndef WINPR_PRAGMA_PACK_EXT
#define WINPR_PRAGMA_PACK_EXT
#endif
#endif

#ifdef PRAGMA_PACK_PUSH
#ifndef PRAGMA_PACK_PUSH1
#define PRAGMA_PACK_PUSH1
#endif
#undef PRAGMA_PACK_PUSH
#endif

#ifdef PRAGMA_PACK_PUSH1
#ifdef WINPR_PRAGMA_PACK_EXT
#pragma pack(push, 1)
#else
#pragma pack(1)
#endif
#undef PRAGMA_PACK_PUSH1
#endif

#ifdef PRAGMA_PACK_PUSH2
#ifdef WINPR_PRAGMA_PACK_EXT
#pragma pack(push, 2)
#else
#pragma pack(2)
#endif
#undef PRAGMA_PACK_PUSH2
#endif

#ifdef PRAGMA_PACK_PUSH4
#ifdef WINPR_PRAGMA_PACK_EXT
#pragma pack(push, 4)
#else
#pragma pack(4)
#endif
#undef PRAGMA_PACK_PUSH4
#endif

#ifdef PRAGMA_PACK_PUSH8
#ifdef WINPR_PRAGMA_PACK_EXT
#pragma pack(push, 8)
#else
#pragma pack(8)
#endif
#undef PRAGMA_PACK_PUSH8
#endif

#ifdef PRAGMA_PACK_PUSH16
#ifdef WINPR_PRAGMA_PACK_EXT
#pragma pack(push, 16)
#else
#pragma pack(16)
#endif
#undef PRAGMA_PACK_PUSH16
#endif

#ifdef PRAGMA_PACK_POP
#ifdef WINPR_PRAGMA_PACK_EXT
#pragma pack(pop)
#else
#pragma pack()
#endif
#undef PRAGMA_PACK_POP
#endif

#undef WINPR_PRAGMA_PACK_EXT
