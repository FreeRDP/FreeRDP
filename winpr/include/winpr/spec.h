/**
 * WinPR: Windows Portable Runtime
 * Compiler Specification Strings
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

#ifndef WINPR_SPEC_H
#define WINPR_SPEC_H

#if defined(__x86_64) && \
		!(defined(_X86_) || defined(__i386__) || defined(_IA64_))
#if !defined(_AMD64_)
#define _AMD64_
#endif
#endif /* _AMD64_ */

#ifdef _AMD64_
#define _WIN64
#endif

#ifndef DECLSPEC_ALIGN
#if defined(_MSC_VER) && (_MSC_VER >= 1300) && !defined(MIDL_PASS)
#define DECLSPEC_ALIGN(x) __declspec(align(x))
#elif defined(__GNUC__)
#define DECLSPEC_ALIGN(x) __attribute__ ((__aligned__ (x)))
#else
#define DECLSPEC_ALIGN(x)
#endif
#endif /* DECLSPEC_ALIGN */

#ifdef _WIN64
#define MEMORY_ALLOCATION_ALIGNMENT 16
#else
#define MEMORY_ALLOCATION_ALIGNMENT 8
#endif

#define DUMMYSTRUCTNAME		s

#endif /* WINPR_SPEC_H */

