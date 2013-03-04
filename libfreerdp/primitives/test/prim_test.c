/* prim_test.c
 * vi:ts=4 sw=4
 *
 * (c) Copyright 2012 Hewlett-Packard Development Company, L.P.
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

#include "prim_test.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <winpr/platform.h>
#include <winpr/sysinfo.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <stdlib.h>
#include <time.h>

int test_sizes[] = { 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096 };
int Quiet = 0;



/* ------------------------------------------------------------------------- */
typedef struct
{
	UINT32	flag;
	const char *str;
} flagpair_t;

static const flagpair_t flags[] =
{
#ifdef _M_IX86_AMD64
	{ PF_MMX_INSTRUCTIONS_AVAILABLE,		"MMX" },
	{ PF_3DNOW_INSTRUCTIONS_AVAILABLE,		"3DNow" },
	{ PF_SSE_INSTRUCTIONS_AVAILABLE,		"SSE" },
	{ PF_SSE2_INSTRUCTIONS_AVAILABLE,		"SSE2" },
	{ PF_SSE3_INSTRUCTIONS_AVAILABLE,		"SSE3" },
#elif defined(_M_ARM)
	{ PF_ARM_VFP3,							"VFP3" },
	{ PF_ARM_INTEL_WMMX,					"IWMMXT" },
	{ PF_ARM_NEON_INSTRUCTIONS_AVAILABLE,	"NEON" },
#endif
};

static const flagpair_t flags_extended[] =
{
#ifdef _M_IX86_AMD64
	{ PF_EX_3DNOW_PREFETCH,	    "3DNow-PF" },
	{ PF_EX_SSSE3,				"SSSE3" },
	{ PF_EX_SSE41,				"SSE4.1" },
	{ PF_EX_SSE42,				"SSE4.2" },
	{ PF_EX_AVX,				"AVX" },
	{ PF_EX_FMA,				"FMA" },
	{ PF_EX_AVX_AES,			"AVX-AES" },
	{ PF_EX_AVX2,				"AVX2" },
#elif defined(_M_ARM)
	{ PF_EX_ARM_VFP1,			"VFP1"},
	{ PF_EX_ARM_VFP4,			"VFP4" },
#endif
};

void primitives_flags_str(char* str, size_t len)
{
	int i;

	*str = '\0';
	--len;	/* for the '/0' */

	for (i = 0; i < sizeof(flags) / sizeof(flagpair_t); ++i)
	{
		if (IsProcessorFeaturePresent(flags[i].flag))
		{
			int slen = strlen(flags[i].str) + 1;

			if (len < slen)
				break;

			if (*str != '\0')
				strcat(str, " ");

			strcat(str, flags[i].str);
			len -= slen;
		}
	}
	for (i = 0; i < sizeof(flags_extended) / sizeof(flagpair_t); ++i)
	{
		if (IsProcessorFeaturePresentEx(flags_extended[i].flag))
		{
			int slen = strlen(flags_extended[i].str) + 1;

			if (len < slen)
				break;

			if (*str != '\0')
				strcat(str, " ");

			strcat(str, flags_extended[i].str);
			len -= slen;
		}
	}
}

/* ------------------------------------------------------------------------- */
static void get_random_data_lrand(
    void *buffer,
    size_t size)
{
	static int seeded = 0;
	long int *ptr = (long int *) buffer;
	unsigned char *cptr;

	if (!seeded)
	{
		seeded = 1;
		srand48(time(NULL));
	}
	/* This isn't the perfect random number generator, but that's okay. */
	while (size >= sizeof(long int))
	{
		*ptr++ = lrand48();
		size -= sizeof(long int);
	}
	cptr = (unsigned char *) ptr;
	while (size > 0)
	{
		*cptr++ = lrand48() & 0xff;
		--size;
	}
}

/* ------------------------------------------------------------------------- */
void get_random_data(
    void *buffer,
    size_t size)
{
#ifdef linux
	size_t offset = 0;
	int fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0)
	{
		get_random_data_lrand(buffer, size);
		return;
	}

	while (size > 0)
	{
		ssize_t count = read(fd, buffer+offset, size);
		size -= count;
		offset += count;
	}
	close(fd);
#else
	get_random_data_lrand(buffer, size);
#endif
}

/* ------------------------------------------------------------------------- */
float _delta_time(
    const struct timespec *t0,
    const struct timespec *t1)
{
    INT64 secs = (INT64) (t1->tv_sec) - (INT64) (t0->tv_sec);
    long   nsecs = t1->tv_nsec - t0->tv_nsec;
	double retval;

    if (nsecs < 0)
	{
        --secs;
        nsecs += 1000000000;
    }
    retval = (double) secs + (double) nsecs / (double) 1000000000.0;
    return (retval < 0.0) ? 0.0 : (float) retval;
}

/* ------------------------------------------------------------------------- */
void _floatprint(
    float t,
    char *output)
{
    /* I don't want to link against -lm, so avoid log,exp,... */
    float f = 10.0;
	int i;
    while (t > f) f *= 10.0;
    f /= 1000.0;
    i = ((int) (t/f+0.5)) * (int) f;
    if (t < 0.0) sprintf(output, "%f", t);
    else if (i == 0) sprintf(output, "%d", (int) (t+0.5));
    else if (t < 1e+3) sprintf(output, "%3d", i);
    else if (t < 1e+6) sprintf(output, "%3d,%03d",     
		i/1000, i % 1000);
    else if (t < 1e+9) sprintf(output, "%3d,%03d,000", 
		i/1000000, (i % 1000000) / 1000);
    else if (t < 1e+12) sprintf(output, "%3d,%03d,000,000", 
		i/1000000000, (i % 1000000000) / 1000000);
    else sprintf(output, "%f", t);
}

/* ------------------------------------------------------------------------- */
/* Specific areas to test: */
#define TEST_COPY8			(1<<0)
#define TEST_SET8			(1<<1)
#define TEST_SET32S			(1<<2)
#define TEST_SET32U			(1<<3)
#define TEST_SIGN16S			(1<<4)
#define TEST_ADD16S			(1<<5)
#define TEST_LSHIFT16S			(1<<6)
#define TEST_LSHIFT16U			(1<<7)
#define TEST_RSHIFT16S			(1<<8)
#define TEST_RSHIFT16U			(1<<9)
#define TEST_RGB			(1<<10)
#define TEST_ALPHA			(1<<11)
#define TEST_AND			(1<<12)
#define TEST_OR				(1<<13)

/* Specific types of testing: */
#define TEST_FUNCTIONALITY		(1<<0)
#define TEST_PERFORMANCE		(1<<1)

/* ------------------------------------------------------------------------- */

typedef struct
{
	const char *testStr;
	UINT32 bits;
} test_t;

static const test_t testList[] =
{
	{ "all",		0xFFFFFFFFU },
	{ "copy",		TEST_COPY8 },
	{ "copy8",		TEST_COPY8 },
	{ "set",		TEST_SET8|TEST_SET32S|TEST_SET32U },
	{ "set8",		TEST_SET8 },
	{ "set32",		TEST_SET32S|TEST_SET32U },
	{ "set32s",		TEST_SET32S },
	{ "set32u",		TEST_SET32U },
	{ "sign",		TEST_SIGN16S },
	{ "sign16s",		TEST_SIGN16S },
	{ "add",		TEST_ADD16S },
	{ "add16s",		TEST_ADD16S },
	{ "lshift",		TEST_LSHIFT16S|TEST_LSHIFT16U },
	{ "rshift",		TEST_RSHIFT16S|TEST_RSHIFT16U },
	{ "shift",		TEST_LSHIFT16S|TEST_LSHIFT16U|TEST_RSHIFT16S|TEST_RSHIFT16U },
	{ "lshift16s",		TEST_LSHIFT16S },
	{ "lshift16u",		TEST_LSHIFT16U },
	{ "rshift16s",		TEST_RSHIFT16S },
	{ "rshift16u",		TEST_RSHIFT16U },
	{ "rgb",		TEST_RGB },
	{ "color",		TEST_RGB },
	{ "colors",		TEST_RGB },
	{ "alpha",		TEST_ALPHA },
	{ "and",		TEST_AND },
	{ "or",			TEST_OR }
};

#define NUMTESTS (sizeof(testList)/sizeof(test_t))

static const test_t testTypeList[] =
{
	{ "functionality",	TEST_FUNCTIONALITY },
	{ "performance",	TEST_PERFORMANCE },
};

#define NUMTESTTYPES (sizeof(testTypeList)/sizeof(test_t))

int main(int argc, char** argv)
{
	int i;
	char hints[1024];
	UINT32 testSet = 0;
	UINT32 testTypes = 0;
	int results = SUCCESS;

	/* Parse command line for the test set. */

	for (i = 1; i < argc; ++i)
	{
		int j;
		BOOL found = 0;

		for (j=0; j<NUMTESTS; ++j)
		{
			if (strcasecmp(argv[i], testList[j].testStr) == 0)
			{
				testSet |= testList[j].bits;
				found = 1;
				break;
			}
		}
		for (j=0; j<NUMTESTTYPES; ++j)
		{
			if (strcasecmp(argv[i], testTypeList[j].testStr) == 0)
			{
				testTypes |= testTypeList[j].bits;
				found = 1;
				break;
			}
		}
		if (!found)
		{
			if (strstr(argv[i], "help") != NULL)
			{
				printf("Available tests:\n");
				for (j=0; j<NUMTESTS; ++j)
				{
					printf("  %s\n", testList[j].testStr);
				}
				for (j=0; j<NUMTESTTYPES; ++j)
				{
					printf("  %s\n", testTypeList[j].testStr);
				}
			}
			else fprintf(stderr, "Unknown parameter '%s'!\n", argv[i]);
		}
	}

	if (testSet == 0)
		testSet = 0xffffffff;
	if (testTypes == 0)
		testTypes = 0xffffffff;

	primitives_init();

	primitives_flags_str(hints, sizeof(hints));
	printf("Hints: %s\n", hints);

	/* COPY */
	if (testSet & TEST_COPY8)
	{
		if (testTypes & TEST_FUNCTIONALITY)
		{
			results |= test_copy8u_func();
		}
		if (testTypes & TEST_PERFORMANCE)
		{
			results |= test_copy8u_speed();
		}
	}
	/* SET */
	if (testSet & TEST_SET8)
	{
		if (testTypes & TEST_FUNCTIONALITY)
		{
			results |= test_set8u_func();
		}
		if (testTypes & TEST_PERFORMANCE)
		{
			results |= test_set8u_speed();
		}
	}
	if (testSet & TEST_SET32S)
	{
		if (testTypes & TEST_FUNCTIONALITY)
		{
			results |= test_set32s_func();
		}
		if (testTypes & TEST_PERFORMANCE)
		{
			results |= test_set32s_speed();
		}
	}
	if (testSet & TEST_SET32U)
	{
		if (testTypes & TEST_FUNCTIONALITY)
		{
			results |= test_set32u_func();
		}
		if (testTypes & TEST_PERFORMANCE)
		{
			results |= test_set32u_speed();
		}
	}
	/* SIGN */
	if (testSet & TEST_SIGN16S)
	{
		if (testTypes & TEST_FUNCTIONALITY)
		{
			results |= test_sign16s_func();
		}
		if (testTypes & TEST_PERFORMANCE)
		{
			results |= test_sign16s_speed();
		}
	}
	/* ADD */
	if (testSet & TEST_ADD16S)
	{
		if (testTypes & TEST_FUNCTIONALITY)
		{
			results |= test_add16s_func();
		}
		if (testTypes & TEST_PERFORMANCE)
		{
			results |= test_add16s_speed();
		}
	}
	/* SHIFTS */
	if (testSet & TEST_LSHIFT16S)
	{
		if (testTypes & TEST_FUNCTIONALITY)
		{
			results |= test_lShift_16s_func();
		}
		if (testTypes & TEST_PERFORMANCE)
		{
			results |= test_lShift_16s_speed();
		}
	}
	if (testSet & TEST_LSHIFT16U)
	{
		if (testTypes & TEST_FUNCTIONALITY)
		{
			results |= test_lShift_16u_func();
		}
		if (testTypes & TEST_PERFORMANCE)
		{
			results |= test_lShift_16u_speed();
		}
	}
	if (testSet & TEST_RSHIFT16S)
	{
		if (testTypes & TEST_FUNCTIONALITY)
		{
			results |= test_rShift_16s_func();
		}
		if (testTypes & TEST_PERFORMANCE)
		{
			results |= test_rShift_16s_speed();
		}
	}
	if (testSet & TEST_RSHIFT16U)
	{
		if (testTypes & TEST_FUNCTIONALITY)
		{
			results |= test_rShift_16u_func();
		}
		if (testTypes & TEST_PERFORMANCE)
		{
			results |= test_rShift_16u_speed();
		}
	}
	/* COLORS */
	if (testSet & TEST_RGB)
	{
		if (testTypes & TEST_FUNCTIONALITY)
		{
			results |= test_RGBToRGB_16s8u_P3AC4R_func();
		}
		if (testTypes & TEST_PERFORMANCE)
		{
			results |= test_RGBToRGB_16s8u_P3AC4R_speed();
		}
		if (testTypes & TEST_FUNCTIONALITY)
		{
			results |= test_yCbCrToRGB_16s16s_P3P3_func();
		}
		if (testTypes & TEST_PERFORMANCE)
		{
			results |= test_yCbCrToRGB_16s16s_P3P3_speed();
		}
	}
	/* ALPHA COMPOSITION */
	if (testSet & TEST_ALPHA)
	{
		if (testTypes & TEST_FUNCTIONALITY)
		{
			results |= test_alphaComp_func();
		}
		if (testTypes & TEST_PERFORMANCE)
		{
			results |= test_alphaComp_speed();
		}
	}
	/* AND & OR */
	if (testSet & TEST_AND)
	{
		if (testTypes & TEST_FUNCTIONALITY)
		{
			results |= test_and_32u_func();
		}
		if (testTypes & TEST_PERFORMANCE)
		{
			results |= test_and_32u_speed();
		}
	}
	if (testSet & TEST_OR)
	{
		if (testTypes & TEST_FUNCTIONALITY)
		{
			results |= test_or_32u_func();
		}
		if (testTypes & TEST_PERFORMANCE)
		{
			results |= test_or_32u_speed();
		}
	}

	primitives_deinit();
	return results;
}
