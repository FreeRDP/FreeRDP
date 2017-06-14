/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * H.264 Bitmap Compression
 *
 * Copyright 2015 Marc-André Moreau <marcandre.moreau@gmail.com>
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

#define NAL_UNKNOWN	X264_NAL_UNKNOWN
#define NAL_SLICE	X264_NAL_SLICE
#define NAL_SLICE_DPA	X264_NAL_SLICE_DPA
#define NAL_SLICE_DPB	X264_NAL_SLICE_DPB
#define NAL_SLICE_DPC	X264_NAL_SLICE_DPC
#define NAL_SLICE_IDR	X264_NAL_SLICE_IDR
#define NAL_SEI		X264_NAL_SEI
#define NAL_SPS		X264_NAL_SPS
#define NAL_PPS		X264_NAL_PPS
#define NAL_AUD		X264_NAL_AUD
#define NAL_FILLER	X264_NAL_FILLER

#define NAL_PRIORITY_DISPOSABLE	X264_NAL_PRIORITY_DISPOSABLE
#define NAL_PRIORITY_LOW	X264_NAL_PRIORITY_LOW
#define NAL_PRIORITY_HIGH	X264_NAL_PRIORITY_HIGH
#define NAL_PRIORITY_HIGHEST	X264_NAL_PRIORITY_HIGHEST

#include <stdint.h>
#include <x264.h>
#include <freerdp/codec/h264.h>

struct _H264_CONTEXT_X264
{
	void* dummy;
};
typedef struct _H264_CONTEXT_X264 H264_CONTEXT_X264;

static int x264_decompress(H264_CONTEXT* h264, BYTE* pSrcData, UINT32 SrcSize)
{
	//H264_CONTEXT_X264* sys = (H264_CONTEXT_X264*) h264->pSystemData;
	return -1;
}

static int x264_compress(H264_CONTEXT* h264, BYTE** ppDstData, UINT32* pDstSize)
{
	//H264_CONTEXT_X264* sys = (H264_CONTEXT_X264*) h264->pSystemData;
	return -1;
}

static void x264_uninit(H264_CONTEXT* h264)
{
	H264_CONTEXT_X264* sys = (H264_CONTEXT_X264*) h264->pSystemData;

	if (sys)
	{
		free(sys);
		h264->pSystemData = NULL;
	}
}

static BOOL x264_init(H264_CONTEXT* h264)
{
	H264_CONTEXT_X264* sys;
	h264->numSystemData = 1;
	sys = (H264_CONTEXT_X264*) calloc(h264->numSystemData,
	                                  sizeof(H264_CONTEXT_X264));

	if (!sys)
	{
		goto EXCEPTION;
	}

	h264->pSystemData = (void*) sys;

	if (h264->Compressor)
	{
	}
	else
	{
	}

	return TRUE;
EXCEPTION:
	x264_uninit(h264);
	return FALSE;
}

H264_CONTEXT_SUBSYSTEM g_Subsystem_x264 =
{
	"x264",
	x264_init,
	x264_uninit,
	x264_decompress,
	x264_compress
};

#undef NAL_UNKNOWN
#undef NAL_SLICE
#undef NAL_SLICE_DPA
#undef NAL_SLICE_DPB
#undef NAL_SLICE_DPC
#undef NAL_SLICE_IDR
#undef NAL_SEI
#undef NAL_SPS
#undef NAL_PPS
#undef NAL_AUD
#undef NAL_FILLER

#undef NAL_PRIORITY_DISPOSABLE
#undef NAL_PRIORITY_LOW
#undef NAL_PRIORITY_HIGH
#undef NAL_PRIORITY_HIGHEST
