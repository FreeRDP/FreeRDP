/**
 * WinPR: Windows Portable Runtime
 * Platform-Specific Definitions
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

#ifndef WINPR_PLATFORM_H
#define WINPR_PLATFORM_H

#include <stdlib.h>

#include <winpr/wtypes.h>

/*
 * Processor Architectures:
 * http://sourceforge.net/p/predef/wiki/Architectures/
 *
 * Visual Studio Predefined Macros:
 * http://msdn.microsoft.com/en-ca/library/vstudio/b0084kay.aspx
 */

/* Intel x86 (_M_IX86) */

#if defined(i386) || defined(__i386) || defined(__i386__) || defined(__i486__) ||   \
    defined(__i586__) || defined(__i686__) || defined(__X86__) || defined(_X86_) || \
    defined(__I86__) || defined(__IA32__) || defined(__THW_INTEL__) || defined(__INTEL__)
#ifndef _M_IX86
#define _M_IX86 1
#endif
#endif

/* AMD64 (_M_AMD64) */

#if defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__) || \
    defined(_M_X64)
#ifndef _M_AMD64
#define _M_AMD64 1
#endif
#endif

/* Intel x86 or AMD64 (_M_IX86_AMD64) */

#if defined(_M_IX86) || defined(_M_AMD64)
#ifndef _M_IX86_AMD64
#define _M_IX86_AMD64 1
#endif
#endif

/* ARM (_M_ARM) */

#if defined(__arm__) || defined(__thumb__) || defined(__TARGET_ARCH_ARM) || \
    defined(__TARGET_ARCH_THUMB)
#ifndef _M_ARM
#define _M_ARM 1
#endif
#endif

/* ARM64 (_M_ARM64) */

#if defined(__aarch64__)
#ifndef _M_ARM64
#define _M_ARM64 1
#endif
#endif

/* MIPS (_M_MIPS) */

#if defined(mips) || defined(__mips) || defined(__mips__) || defined(__MIPS__)
#ifndef _M_MIPS
#define _M_MIPS 1
#endif
#endif

/* MIPS64 (_M_MIPS64) */

#if defined(mips64) || defined(__mips64) || defined(__mips64__) || defined(__MIPS64__)
#ifndef _M_MIPS64
#define _M_MIPS64 1
#endif
#endif

/* PowerPC (_M_PPC) */

#if defined(__ppc__) || defined(__powerpc) || defined(__powerpc__) || defined(__POWERPC__) || \
    defined(_ARCH_PPC)
#ifndef _M_PPC
#define _M_PPC 1
#endif
#endif

/* Intel Itanium (_M_IA64) */

#if defined(__ia64) || defined(__ia64__) || defined(_IA64) || defined(__IA64__)
#ifndef _M_IA64
#define _M_IA64 1
#endif
#endif

/* Alpha (_M_ALPHA) */

#if defined(__alpha) || defined(__alpha__)
#ifndef _M_ALPHA
#define _M_ALPHA 1
#endif
#endif

/* SPARC (_M_SPARC) */

#if defined(__sparc) || defined(__sparc__)
#ifndef _M_SPARC
#define _M_SPARC 1
#endif
#endif

/**
 * Operating Systems:
 * http://sourceforge.net/p/predef/wiki/OperatingSystems/
 */

/* Windows (_WIN32) */

/* WinRT (_WINRT) */

#if defined(WINAPI_FAMILY)
#if (WINAPI_FAMILY == WINAPI_FAMILY_APP)
#ifndef _WINRT
#define _WINRT 1
#endif
#endif
#endif

#if defined(__cplusplus_winrt)
#ifndef _WINRT
#define _WINRT 1
#endif
#endif

/* Linux (__linux__) */

#if defined(linux) || defined(__linux)
#ifndef __linux__
#define __linux__ 1
#endif
#endif

/* GNU/Linux (__gnu_linux__) */

/* Apple Platforms (iOS, Mac OS X) */

#if (defined(__APPLE__) && defined(__MACH__))

#include <TargetConditionals.h>

#if (TARGET_OS_IPHONE == 1) || (TARGET_IPHONE_SIMULATOR == 1)

/* iOS (__IOS__) */

#ifndef __IOS__
#define __IOS__ 1
#endif

#elif (TARGET_OS_MAC == 1)

/* Mac OS X (__MACOSX__) */

#ifndef __MACOSX__
#define __MACOSX__ 1
#endif

#endif
#endif

/* Android (__ANDROID__) */

/* Cygwin (__CYGWIN__) */

/* FreeBSD (__FreeBSD__) */

/* NetBSD (__NetBSD__) */

/* OpenBSD (__OpenBSD__) */

/* DragonFly (__DragonFly__) */

/* Solaris (__sun) */

#if defined(sun)
#ifndef __sun
#define __sun 1
#endif
#endif

/* IRIX (__sgi) */

#if defined(sgi)
#ifndef __sgi
#define __sgi 1
#endif
#endif

/* AIX (_AIX) */

#if defined(__TOS_AIX__)
#ifndef _AIX
#define _AIX 1
#endif
#endif

/* HP-UX (__hpux) */

#if defined(hpux) || defined(_hpux)
#ifndef __hpux
#define __hpux 1
#endif
#endif

/* BeOS (__BEOS__) */

/* QNX (__QNXNTO__) */

/**
 * Endianness:
 * http://sourceforge.net/p/predef/wiki/Endianness/
 */

#if defined(__gnu_linux__)
#include <endian.h>
#endif

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || \
    defined(__DragonFly__) || defined(__APPLE__)
#include <sys/param.h>
#endif

/* Big-Endian */

#ifdef __BYTE_ORDER

#if (__BYTE_ORDER == __BIG_ENDIAN)
#ifndef __BIG_ENDIAN__
#define __BIG_ENDIAN__ 1
#endif
#endif

#else

#if defined(__ARMEB__) || defined(__THUMBEB__) || defined(__AARCH64EB__) || defined(_MIPSEB) || \
    defined(__MIPSEB) || defined(__MIPSEB__)
#ifndef __BIG_ENDIAN__
#define __BIG_ENDIAN__ 1
#endif
#endif

#endif /* __BYTE_ORDER */

/* Little-Endian */

#ifdef __BYTE_ORDER

#if (__BYTE_ORDER == __LITTLE_ENDIAN)
#ifndef __LITTLE_ENDIAN__
#define __LITTLE_ENDIAN__ 1
#endif
#endif

#else

#if defined(__ARMEL__) || defined(__THUMBEL__) || defined(__AARCH64EL__) || defined(_MIPSEL) || \
    defined(__MIPSEL) || defined(__MIPSEL__)
#ifndef __LITTLE_ENDIAN__
#define __LITTLE_ENDIAN__ 1
#endif
#endif

#endif /* __BYTE_ORDER */

#endif /* WINPR_PLATFORM_H */
