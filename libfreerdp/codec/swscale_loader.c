/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * swscale Runtime Loading
 *
 * Copyright 2025 Devolutions Inc.
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

#include <freerdp/config.h>

#if defined(WITH_SWSCALE) && defined(WITH_SWSCALE_LOADING)

#include <winpr/library.h>
#include <winpr/assert.h>
#include <winpr/environment.h>
#include <freerdp/log.h>

#include "swscale_loader.h"

#define TAG FREERDP_TAG("codec.swscale")

// Function pointer types
typedef struct SwsContext* (*pSws_getContext)(int srcW, int srcH, int srcFormat, int dstW,
                                               int dstH, int dstFormat, int flags,
                                               void* srcFilter, void* dstFilter,
                                               const double* param);
typedef int (*pSws_scale)(struct SwsContext* c, const uint8_t* const srcSlice[],
                          const int srcStride[], int srcSliceY, int srcSliceH,
                          uint8_t* const dst[], const int dstStride[]);
typedef void (*pSws_freeContext)(struct SwsContext* c);

// libavutil function pointer types
typedef int (*pAv_image_fill_linesizes)(int linesizes[4], int pix_fmt, int width);
typedef int (*pAv_image_fill_pointers)(uint8_t* data[4], int pix_fmt, int height, uint8_t* ptr,
                                        const int linesizes[4]);

typedef struct
{
	HMODULE lib;
	pSws_getContext getContext;
	pSws_scale scale;
	pSws_freeContext freeContext;
	BOOL initialized;
	BOOL available;
} SWSCALE_LIBRARY;

typedef struct
{
	HMODULE lib;
	pAv_image_fill_linesizes fill_linesizes;
	pAv_image_fill_pointers fill_pointers;
	BOOL initialized;
	BOOL available;
} AVUTIL_LIBRARY;

static SWSCALE_LIBRARY g_swscale = { 0 };
static AVUTIL_LIBRARY g_avutil = { 0 };

static const char* swscale_library_names[] = {
#if defined(_WIN32)
	"swscale-9.dll", "swscale-8.dll", "swscale-7.dll", "swscale-6.dll", "swscale.dll"
#elif defined(__APPLE__)
	"libswscale.dylib",
	"libswscale.9.dylib",
	"libswscale.8.dylib",
	"libswscale.7.dylib",
	"libswscale.6.dylib"
#else
	"libswscale.so.9", "libswscale.so.8", "libswscale.so.7", "libswscale.so.6", "libswscale.so"
#endif
};

static BOOL swscale_load_library(const char* name)
{
	WINPR_ASSERT(name);

	WLog_DBG(TAG, "Attempting to load swscale library: %s", name);

	g_swscale.lib = LoadLibraryA(name);
	if (!g_swscale.lib)
	{
		WLog_DBG(TAG, "Failed to load %s", name);
		return FALSE;
	}

	g_swscale.getContext =
	    (pSws_getContext)(void*)GetProcAddress(g_swscale.lib, "sws_getContext");
	g_swscale.scale = (pSws_scale)(void*)GetProcAddress(g_swscale.lib, "sws_scale");
	g_swscale.freeContext =
	    (pSws_freeContext)(void*)GetProcAddress(g_swscale.lib, "sws_freeContext");

	if (!g_swscale.getContext || !g_swscale.scale || !g_swscale.freeContext)
	{
		WLog_WARN(TAG, "Failed to load required functions from %s", name);
		FreeLibrary(g_swscale.lib);
		g_swscale.lib = NULL;
		return FALSE;
	}

	WLog_INFO(TAG, "Successfully loaded swscale library: %s", name);
	return TRUE;
}

static char* swscale_library_path_from_environment(const char* name)
{
	char* env = NULL;

	WINPR_ASSERT(name);

	if (!name)
		return NULL;

	const DWORD size = GetEnvironmentVariableX(name, env, 0);

	if (size <= 1)
	{
		WLog_DBG(TAG, "No environment variable '%s'", name);
		return NULL;
	}

	env = calloc(size, sizeof(char));

	if (!env)
		return NULL;

	const DWORD rc = GetEnvironmentVariableX(name, env, size);

	if (rc != size - 1)
	{
		WLog_WARN(TAG, "Environment variable '%s' has invalid size", name);
		free(env);
		return NULL;
	}

	return env;
}

BOOL freerdp_swscale_init(void)
{
	if (g_swscale.initialized)
		return g_swscale.available;

	g_swscale.initialized = TRUE;
	g_swscale.available = FALSE;

	// Try environment variable first
	char* env_path = swscale_library_path_from_environment("FREERDP_SWSCALE_LIBRARY_PATH");
	if (env_path)
	{
		WLog_INFO(TAG, "Using swscale library path from environment: %s", env_path);
		if (swscale_load_library(env_path))
		{
			g_swscale.available = TRUE;
			free(env_path);
			return TRUE;
		}
		free(env_path);
	}

	// Try default library names
	WLog_DBG(TAG, "Searching for swscale library in default locations");
	for (size_t i = 0; i < ARRAYSIZE(swscale_library_names); i++)
	{
		if (swscale_load_library(swscale_library_names[i]))
		{
			g_swscale.available = TRUE;
			return TRUE;
		}
	}

	WLog_INFO(TAG,
	          "swscale library not found - image scaling features will not be available. "
	          "Install FFmpeg to enable these features.");
	return FALSE;
}

BOOL freerdp_swscale_available(void)
{
	return freerdp_swscale_init() && g_swscale.available;
}

struct SwsContext* freerdp_sws_getContext(int srcW, int srcH, int srcFormat, int dstW, int dstH,
                                          int dstFormat, int flags, void* srcFilter,
                                          void* dstFilter, const double* param)
{
	if (!freerdp_swscale_available())
	{
		WLog_WARN(TAG, "sws_getContext called but swscale not available");
		return NULL;
	}

	WINPR_ASSERT(g_swscale.getContext);
	return g_swscale.getContext(srcW, srcH, srcFormat, dstW, dstH, dstFormat, flags, srcFilter,
	                            dstFilter, param);
}

int freerdp_sws_scale(struct SwsContext* ctx, const uint8_t* const srcSlice[],
                      const int srcStride[], int srcSliceY, int srcSliceH, uint8_t* const dst[],
                      const int dstStride[])
{
	if (!freerdp_swscale_available())
	{
		WLog_WARN(TAG, "sws_scale called but swscale not available");
		return -1;
	}

	if (!ctx)
	{
		WLog_WARN(TAG, "sws_scale called with NULL context");
		return -1;
	}

	WINPR_ASSERT(g_swscale.scale);
	return g_swscale.scale(ctx, srcSlice, srcStride, srcSliceY, srcSliceH, dst, dstStride);
}

void freerdp_sws_freeContext(struct SwsContext* ctx)
{
	if (!freerdp_swscale_available())
		return;

	if (!ctx)
		return;

	WINPR_ASSERT(g_swscale.freeContext);
	g_swscale.freeContext(ctx);
}

// =============================================================================
// libavutil Runtime Loading
// =============================================================================

static const char* avutil_library_names[] = {
#if defined(_WIN32)
	"avutil-59.dll", "avutil-58.dll", "avutil-57.dll", "avutil-56.dll", "avutil.dll"
#elif defined(__APPLE__)
	"libavutil.dylib",
	"libavutil.59.dylib",
	"libavutil.58.dylib",
	"libavutil.57.dylib",
	"libavutil.56.dylib"
#else
	"libavutil.so.59", "libavutil.so.58", "libavutil.so.57", "libavutil.so.56", "libavutil.so"
#endif
};

static BOOL avutil_load_library(const char* name)
{
	WINPR_ASSERT(name);

	WLog_DBG(TAG, "Attempting to load avutil library: %s", name);

	g_avutil.lib = LoadLibraryA(name);
	if (!g_avutil.lib)
	{
		WLog_DBG(TAG, "Failed to load %s", name);
		return FALSE;
	}

	g_avutil.fill_linesizes =
	    (pAv_image_fill_linesizes)(void*)GetProcAddress(g_avutil.lib, "av_image_fill_linesizes");
	g_avutil.fill_pointers =
	    (pAv_image_fill_pointers)(void*)GetProcAddress(g_avutil.lib, "av_image_fill_pointers");

	if (!g_avutil.fill_linesizes || !g_avutil.fill_pointers)
	{
		WLog_WARN(TAG, "Failed to load required functions from %s", name);
		FreeLibrary(g_avutil.lib);
		g_avutil.lib = NULL;
		return FALSE;
	}

	WLog_INFO(TAG, "Successfully loaded avutil library: %s", name);
	return TRUE;
}

BOOL freerdp_avutil_init(void)
{
	if (g_avutil.initialized)
		return g_avutil.available;

	g_avutil.initialized = TRUE;
	g_avutil.available = FALSE;

	// Try environment variable first
	char* env_path = swscale_library_path_from_environment("FREERDP_AVUTIL_LIBRARY_PATH");
	if (env_path)
	{
		WLog_INFO(TAG, "Using avutil library path from environment: %s", env_path);
		if (avutil_load_library(env_path))
		{
			g_avutil.available = TRUE;
			free(env_path);
			return TRUE;
		}
		free(env_path);
	}

	// Try default library names
	WLog_DBG(TAG, "Searching for avutil library in default locations");
	for (size_t i = 0; i < ARRAYSIZE(avutil_library_names); i++)
	{
		if (avutil_load_library(avutil_library_names[i]))
		{
			g_avutil.available = TRUE;
			return TRUE;
		}
	}

	WLog_INFO(TAG,
	          "avutil library not found - image format features will be limited. "
	          "Install FFmpeg to enable full image format support.");
	return FALSE;
}

BOOL freerdp_avutil_available(void)
{
	return freerdp_avutil_init() && g_avutil.available;
}

int freerdp_av_image_fill_linesizes(int linesizes[4], int pix_fmt, int width)
{
	if (!freerdp_avutil_available())
	{
		WLog_WARN(TAG, "av_image_fill_linesizes called but avutil not available");
		return -1;
	}

	WINPR_ASSERT(g_avutil.fill_linesizes);
	return g_avutil.fill_linesizes(linesizes, pix_fmt, width);
}

int freerdp_av_image_fill_pointers(uint8_t* data[4], int pix_fmt, int height, uint8_t* ptr,
                                    const int linesizes[4])
{
	if (!freerdp_avutil_available())
	{
		WLog_WARN(TAG, "av_image_fill_pointers called but avutil not available");
		return -1;
	}

	WINPR_ASSERT(g_avutil.fill_pointers);
	return g_avutil.fill_pointers(data, pix_fmt, height, ptr, linesizes);
}

#endif /* WITH_SWSCALE && WITH_SWSCALE_LOADING */
