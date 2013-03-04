/* primtest.h
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
 * permissions and limitations under the License.  Algorithms used by
 * this code may be covered by patents by HP, Microsoft, or other parties.
 */

#ifdef __GNUC__
# pragma once
#endif

#ifndef __PRIMTEST_H_INCLUDED__
#define __PRIMTEST_H_INCLUDED__

#include <config.h>
#include <stdint.h>
#include <winpr/wtypes.h>

#include <measure.h>
#include <string.h>
#include <stdio.h>

#include <freerdp/primitives.h>
#include <winpr/platform.h>

#ifdef WITH_IPP
#include <ipps.h>
#include <ippi.h>
#endif

#define BLOCK_ALIGNMENT 16
#ifdef __GNUC__
#define ALIGN(x) x __attribute((aligned(BLOCK_ALIGNMENT)))
#define POSSIBLY_UNUSED(x)	x __attribute((unused))
#else
/* TODO: Someone needs to finish this for non-GNU C */
#define ALIGN(x) x
#define POSSIBLY_UNUSED(x)	x
#endif
#define ABS(_x_) ((_x_) < 0 ? (-(_x_)) : (_x_))
#define MAX_TEST_SIZE 4096

extern int test_sizes[];
#define NUM_TEST_SIZES 10

extern void get_random_data(void *buffer, size_t size);

#ifndef SUCCESS
#define SUCCESS 0
#endif
#ifndef FAILURE
#define FAILURE 1
#endif

extern int test_copy8u_func(void);
extern int test_copy8u_speed(void);

extern int test_set8u_func(void);
extern int test_set32s_func(void);
extern int test_set32u_func(void);
extern int test_set8u_speed(void);
extern int test_set32s_speed(void);
extern int test_set32u_speed(void);

extern int test_sign16s_func(void);
extern int test_sign16s_speed(void);

extern int test_add16s_func(void);
extern int test_add16s_speed(void);

extern int test_lShift_16s_func(void);
extern int test_lShift_16u_func(void);
extern int test_rShift_16s_func(void);
extern int test_rShift_16u_func(void);
extern int test_lShift_16s_speed(void);
extern int test_lShift_16u_speed(void);
extern int test_rShift_16s_speed(void);
extern int test_rShift_16u_speed(void);

extern int test_RGBToRGB_16s8u_P3AC4R_func(void);
extern int test_RGBToRGB_16s8u_P3AC4R_speed(void);
extern int test_yCbCrToRGB_16s16s_P3P3_func(void);
extern int test_yCbCrToRGB_16s16s_P3P3_speed(void);

extern int test_alphaComp_func(void);
extern int test_alphaComp_speed(void);

extern int test_and_32u_func(void);
extern int test_and_32u_speed(void);
extern int test_or_32u_func(void);
extern int test_or_32u_speed(void);

/* Since so much of this code is repeated, define a macro to build 
 * functions to do speed tests.
 */
#ifdef _M_ARM
#define SIMD_TYPE "Neon"
#else
#define SIMD_TYPE "SSE"
#endif

#define DO_NORMAL_MEASUREMENTS(_funcNormal_, _prework_) \
	do { \
		for (s=0; s<num_sizes; ++s) \
		{ \
			int iter; \
			char label[256]; \
			int size = size_array[s]; \
			_prework_; \
			iter = iterations/size; \
			sprintf(label, "%s-%-4d", oplabel, size); \
			MEASURE_TIMED(label, iter, test_time, resultNormal[s],  \
				_funcNormal_); \
		} \
	} while (0)

#if (defined(_M_IX86_AMD64) && defined(WITH_SSE2)) || (defined(_M_ARM) && defined(WITH_NEON))
#define DO_OPT_MEASUREMENTS(_funcOpt_, _prework_) \
	do { \
		for (s=0; s<num_sizes; ++s) \
		{ \
			int iter; \
			char label[256]; \
			int size = size_array[s]; \
			_prework_; \
			iter = iterations/size; \
			sprintf(label, "%s-%s-%-4d", SIMD_TYPE, oplabel, size); \
			MEASURE_TIMED(label, iter, test_time, resultOpt[s],  \
				_funcOpt_); \
		} \
	} while (0)
#else
#define DO_OPT_MEASUREMENTS(_funcSSE_, _prework_)
#endif

#if defined(_M_IX86_AMD64) && defined(WITH_IPP)
#define DO_IPP_MEASUREMENTS(_funcIPP_, _prework_) \
	do { \
		for (s=0; s<num_sizes; ++s) \
		{ \
			int iter; \
			char label[256]; \
			int size = size_array[s]; \
			_prework_; \
			iter = iterations/size; \
			sprintf(label, "IPP-%s-%-4d", oplabel, size); \
			MEASURE_TIMED(label, iter, test_time, resultIPP[s],  \
				_funcIPP_); \
		} \
	} while (0)
#else
#define DO_IPP_MEASUREMENTS(_funcIPP_, _prework_)
#endif

#define PRIM_NOP do {} while (0)
/* ------------------------------------------------------------------------- */
#define STD_SPEED_TEST( \
	_name_, _srctype_, _dsttype_, _prework_, \
	_doNormal_, _funcNormal_, \
	_doOpt_,    _funcOpt_,  _flagOpt_, _flagExt_, \
	_doIPP_,    _funcIPP_) \
static void _name_( \
	const char *oplabel, const char *type, \
	const _srctype_ *src1, const _srctype_ *src2, _srctype_ constant, \
	_dsttype_ *dst, \
	const int *size_array,  int num_sizes, \
	int iterations, float test_time) \
{ \
	int s; \
	float *resultNormal, *resultOpt, *resultIPP; \
	resultNormal = (float *) calloc(num_sizes, sizeof(float)); \
	resultOpt = (float *) calloc(num_sizes, sizeof(float)); \
	resultIPP = (float *) calloc(num_sizes, sizeof(float)); \
	printf("******************** %s %s ******************\n",  \
		oplabel, type); \
	if (_doNormal_) { DO_NORMAL_MEASUREMENTS(_funcNormal_, _prework_); } \
	if (_doOpt_)  \
	{ \
		if (_flagExt_) \
		{ \
			if (IsProcessorFeaturePresentEx(_flagOpt_)) \
			{ \
				DO_OPT_MEASUREMENTS(_funcOpt_, _prework_); \
			} \
		} \
		else \
		{ \
			if (IsProcessorFeaturePresent(_flagOpt_)) \
			{ \
				DO_OPT_MEASUREMENTS(_funcOpt_, _prework_); \
			} \
		} \
	} \
	if (_doIPP_)    { DO_IPP_MEASUREMENTS(_funcIPP_, _prework_); } \
	printf("----------------------- SUMMARY ----------------------------\n"); \
	printf("%8s: %15s %15s %5s %15s %5s\n", \
		"size", "general", SIMD_TYPE, "%", "IPP", "%"); \
	for (s=0; s<num_sizes; ++s) \
	{ \
		char sN[32], sSN[32], sSNp[8], sIPP[32], sIPPp[8]; \
		strcpy(sN, "N/A"); strcpy(sSN, "N/A"); strcpy(sSNp, "N/A"); \
		strcpy(sIPP, "N/A"); strcpy(sIPPp, "N/A"); \
		if (resultNormal[s] > 0.0) _floatprint(resultNormal[s], sN); \
		if (resultOpt[s] > 0.0) \
		{ \
			_floatprint(resultOpt[s], sSN); \
			if (resultNormal[s] > 0.0) \
			{ \
				sprintf(sSNp, "%d%%", \
					(int) (resultOpt[s] / resultNormal[s] * 100.0 + 0.5)); \
			} \
		} \
		if (resultIPP[s] > 0.0) \
		{ \
			_floatprint(resultIPP[s], sIPP); \
			if (resultNormal[s] > 0.0) \
			{ \
				sprintf(sIPPp, "%d%%", \
					(int) (resultIPP[s] / resultNormal[s] * 100.0 + 0.5)); \
			} \
		} \
		printf("%8d: %15s %15s %5s %15s %5s\n",  \
			size_array[s], sN, sSN, sSNp, sIPP, sIPPp); \
	} \
	free(resultNormal); free(resultOpt);  free(resultIPP); \
}

#endif // !__PRIMTEST_H_INCLUDED__
