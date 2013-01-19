/* primitives.c
 * This code queries processor features and calls the init/deinit routines.
 * vi:ts=4 sw=4
 *
 * Copyright 2011 Martin Fleisz <mfleisz@thinstuff.com>
 * (c) Copyright 2012 Hewlett-Packard Development Company, L.P.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0.
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>

#include <freerdp/primitives.h>

#include "prim_internal.h"

#ifdef ANDROID
#include "cpu-features.h"
#endif

/* Singleton pointer used throughout the program when requested. */
static primitives_t* pPrimitives = NULL;

#define D_BIT_MMX		(1<<23)
#define D_BIT_SSE		(1<<25)
#define D_BIT_SSE2		(1<<26)
#define D_BIT_3DN		(1<<30)
#define C_BIT_SSE3		(1<<0)
#define C_BIT_3DNP		(1<<8)
#define C_BIT_SSSE3		(1<<9)
#define C_BIT_SSE41		(1<<19)
#define C_BIT_SSE42		(1<<20)
#define C_BIT_XGETBV		(1<<27)
#define C_BIT_AVX		(1<<28)
#define C_BITS_AVX		(C_BIT_XGETBV|C_BIT_AVX)
#define E_BIT_XMM		(1<<1)
#define E_BIT_YMM		(1<<2)
#define E_BITS_AVX		(E_BIT_XMM|E_BIT_YMM)
#define C_BIT_FMA		(1<<11)
#define C_BIT_AVX_AES		(1<<24)

/* If x86 */
#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64) \
	|| defined(__amd64__) || defined(_M_AMD64) || defined(_M_X64) \
	|| defined(i386) || defined(__i386) || defined(__i386__) \
	|| defined(_M_IX86) || defined(_X86_)
#ifndef i386
#define i386
#endif

/* If GCC */
# ifdef __GNUC__

# define xgetbv(_func_, _lo_, _hi_) \
	__asm__ __volatile__ ("xgetbv" : "=a" (_lo_), "=d" (_hi_) : "c" (_func_))

static void cpuid(
	unsigned info, 
	unsigned *eax, 
	unsigned *ebx, 
	unsigned *ecx, 
	unsigned *edx)
{
	*eax = *ebx = *ecx = *edx = 0;
	__asm volatile
	(
		/* The EBX (or RBX register on x86_64) is used for the PIC base address
		 * and must not be corrupted by our inline assembly.
		 */
#  if defined(__i386__)
		"mov %%ebx, %%esi;"
		"cpuid;"
		"xchg %%ebx, %%esi;"
#else
		"mov %%rbx, %%rsi;"
		"cpuid;"
		"xchg %%rbx, %%rsi;"
#endif
		: "=a" (*eax), "=S" (*ebx), "=c" (*ecx), "=d" (*edx)
		: "0" (info)
	);
}

static void set_hints(primitives_hints_t* hints)
{
	unsigned a, b, c, d;

	cpuid(1, &a, &b, &c, &d);

	if (d & D_BIT_MMX)
		hints->x86_flags |= PRIM_X86_MMX_AVAILABLE;
	if (d & D_BIT_SSE)
		hints->x86_flags |= PRIM_X86_SSE_AVAILABLE;
	if (d & D_BIT_SSE2)
		hints->x86_flags |= PRIM_X86_SSE2_AVAILABLE;
	if (d & D_BIT_3DN)
		hints->x86_flags |= PRIM_X86_3DNOW_AVAILABLE;
	if (c & C_BIT_3DNP)
		hints->x86_flags |= PRIM_X86_3DNOW_PREFETCH_AVAILABLE;
	if (c & C_BIT_SSE3)
		hints->x86_flags |= PRIM_X86_SSE3_AVAILABLE;
	if (c & C_BIT_SSSE3)
		hints->x86_flags |= PRIM_X86_SSSE3_AVAILABLE;
	if (c & C_BIT_SSE41)
		hints->x86_flags |= PRIM_X86_SSE41_AVAILABLE;
	if (c & C_BIT_SSE42)
		hints->x86_flags |= PRIM_X86_SSE42_AVAILABLE;

	if ((c & C_BITS_AVX) == C_BITS_AVX)
	{
		int e, f;
		xgetbv(0, e, f);

		if ((e & E_BITS_AVX) == E_BITS_AVX)
		{
			hints->x86_flags |= PRIM_X86_AVX_AVAILABLE;

			if (c & C_BIT_FMA)
				hints->x86_flags |= PRIM_X86_FMA_AVAILABLE;
			if (c & C_BIT_AVX_AES)
				hints->x86_flags |= PRIM_X86_AVX_AES_AVAILABLE;
		}
	}
	/* TODO: AVX2: set eax=7, ecx=0, cpuid, check ebx-bit5 */
}
# else
static void set_hints(primitives_hints_t* hints)
{
	/* x86 non-GCC:  TODO */
}
# endif /* __GNUC__ */
/* ------------------------------------------------------------------------- */
#elif defined(__arm__) || defined(__ARM_ARCH_7A__) \
	|| defined(__ARM_EABI__) || defined(__ARMEL__) || defined(ANDROID)
#ifndef __arm__
#define __arm__
#endif

static UINT32 androidNeon(void)
{
#if ANDROID
	if (android_getCpuFamily() != ANDROID_CPU_FAMILY_ARM) return 0;

	UINT64 features = android_getCpuFeatures();

	if ((features & ANDROID_CPU_ARM_FEATURE_ARMv7))
	{
		if (features & ANDROID_CPU_ARM_FEATURE_NEON)
		{
			return PRIM_ARM_NEON_AVAILABLE;
		}
	}
	/* else */
#endif
	return 0;
}

static void set_hints(
	primitives_hints_t *hints)
{
	/* ARM:  TODO */
	hints->arm_flags |= androidNeon();
}

#else
static void set_hints(
	primitives_hints_t *hints)
{
}
#endif /* x86 else ARM else */

/* ------------------------------------------------------------------------- */
void primitives_init(void)
{
	primitives_hints_t* hints;

	if (pPrimitives == NULL)
	{
		pPrimitives = calloc(1, sizeof(primitives_t));

		if (pPrimitives == NULL)
			return;
	}

	hints = calloc(1, sizeof(primitives_hints_t));
	set_hints(hints);
	pPrimitives->hints = (void *) hints;

	/* Now call each section's initialization routine. */
	primitives_init_add(hints, pPrimitives);
	primitives_init_andor(hints, pPrimitives);
	primitives_init_alphaComp(hints, pPrimitives);
	primitives_init_copy(hints, pPrimitives);
	primitives_init_set(hints, pPrimitives);
	primitives_init_shift(hints, pPrimitives);
	primitives_init_sign(hints, pPrimitives);
	primitives_init_colors(hints, pPrimitives);
}

/* ------------------------------------------------------------------------- */
primitives_t* primitives_get(void)
{
	if (pPrimitives == NULL)
		primitives_init();

	return pPrimitives;
}

/* ------------------------------------------------------------------------- */
UINT32 primitives_get_flags(const primitives_t* prims)
{
	primitives_hints_t* hints = (primitives_hints_t*) (prims->hints);

#ifdef i386
	return hints->x86_flags;
#elif defined(__arm__)
	return hints->arm_flags;
#else
	return 0;
#endif
}

/* ------------------------------------------------------------------------- */

typedef struct
{
	UINT32	flag;
	const char *str;
} flagpair_t;

static const flagpair_t x86_flags[] =
{
	{ PRIM_X86_MMX_AVAILABLE,				"MMX" },
	{ PRIM_X86_3DNOW_AVAILABLE,				"3DNow" },
	{ PRIM_X86_3DNOW_PREFETCH_AVAILABLE,			"3DNow-PF" },
	{ PRIM_X86_SSE_AVAILABLE,				"SSE" },
	{ PRIM_X86_SSE2_AVAILABLE,				"SSE2" },
	{ PRIM_X86_SSE3_AVAILABLE,				"SSE3" },
	{ PRIM_X86_SSSE3_AVAILABLE,				"SSSE3" },
	{ PRIM_X86_SSE41_AVAILABLE,				"SSE4.1" },
	{ PRIM_X86_SSE42_AVAILABLE,				"SSE4.2" },
	{ PRIM_X86_AVX_AVAILABLE,				"AVX" },
	{ PRIM_X86_FMA_AVAILABLE,				"FMA" },
	{ PRIM_X86_AVX_AES_AVAILABLE,				"AVX-AES" },
	{ PRIM_X86_AVX2_AVAILABLE,				"AVX2" },
};

static const flagpair_t arm_flags[] =
{
	{ PRIM_ARM_VFP1_AVAILABLE,				"VFP1" },
	{ PRIM_ARM_VFP2_AVAILABLE,				"VFP2" },
	{ PRIM_ARM_VFP3_AVAILABLE,				"VFP3" },
	{ PRIM_ARM_VFP4_AVAILABLE,				"VFP4" },
	{ PRIM_ARM_FPA_AVAILABLE,				"FPA" },
	{ PRIM_ARM_FPE_AVAILABLE,				"FPE" },
	{ PRIM_ARM_IWMMXT_AVAILABLE,				"IWMMXT" },
	{ PRIM_ARM_NEON_AVAILABLE,				"NEON" },
};

void primitives_flags_str(const primitives_t* prims, char* str, size_t len)
{
	int i;
	primitives_hints_t* hints;

	*str = '\0';
	--len;	/* for the '/0' */

	hints = (primitives_hints_t*) (prims->hints);

	for (i = 0; i < sizeof(x86_flags) / sizeof(flagpair_t); ++i)
	{
		if (hints->x86_flags & x86_flags[i].flag)
		{
			int slen = strlen(x86_flags[i].str) + 1;

			if (len < slen)
				break;

			if (*str != '\0')
				strcat(str, " ");

			strcat(str, x86_flags[i].str);
			len -= slen;
		}
	}

	for (i = 0; i < sizeof(arm_flags) / sizeof(flagpair_t); ++i)
	{
		if (hints->arm_flags & arm_flags[i].flag)
		{
			int slen = strlen(arm_flags[i].str) + 1;

			if (len < slen)
				break;

			if (*str != '\0')
				strcat(str, " ");

			strcat(str, arm_flags[i].str);
			len -= slen;
		}
	}
}

/* ------------------------------------------------------------------------- */
void primitives_deinit(void)
{
	if (pPrimitives == NULL)
		return;

	/* Call each section's de-initialization routine. */
	primitives_deinit_add(pPrimitives);
	primitives_deinit_andor(pPrimitives);
	primitives_deinit_alphaComp(pPrimitives);
	primitives_deinit_copy(pPrimitives);
	primitives_deinit_set(pPrimitives);
	primitives_deinit_shift(pPrimitives);
	primitives_deinit_sign(pPrimitives);
	primitives_deinit_colors(pPrimitives);

	if (pPrimitives->hints != NULL)
		free((void*) (pPrimitives->hints));

	free((void*) pPrimitives);
	pPrimitives = NULL;
}
