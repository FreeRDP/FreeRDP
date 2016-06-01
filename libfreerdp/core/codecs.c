/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Codecs
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "rdp.h"

#include <freerdp/codecs.h>

#define TAG FREERDP_TAG("core.codecs")

BOOL freerdp_client_codecs_prepare(rdpCodecs* codecs, UINT32 flags, UINT32 width, UINT32 height)
{
	if ((flags & FREERDP_CODEC_INTERLEAVED) && !codecs->interleaved)
	{
		if (!(codecs->interleaved = bitmap_interleaved_context_new(FALSE)))
		{
			WLog_ERR(TAG, "Failed to create interleaved codec context");
			return FALSE;
		}

		if (!bitmap_interleaved_context_reset(codecs->interleaved))
		{
			WLog_ERR(TAG, "Failed to reset interleaved codec context");
			bitmap_interleaved_context_free(codecs->interleaved);
			return FALSE;
		}
	}

	if ((flags & FREERDP_CODEC_PLANAR) && !codecs->planar)
	{
		if (!(codecs->planar = freerdp_bitmap_planar_context_new(FALSE, 64, 64)))
		{
			WLog_ERR(TAG, "Failed to create planar bitmap codec context");
			return FALSE;
		}

		if (!freerdp_bitmap_planar_context_reset(codecs->planar))
		{
			WLog_ERR(TAG, "Failed to reset plannar bitmap codec context");
			freerdp_bitmap_planar_context_free(codecs->planar);
			return FALSE;
		}
	}

	if ((flags & FREERDP_CODEC_NSCODEC) && !codecs->nsc)
	{
		if (!(codecs->nsc = nsc_context_new()))
		{
			WLog_ERR(TAG, "Failed to create nsc codec context");
			return FALSE;
		}

		if (!nsc_context_reset(codecs->nsc, width, height))
		{
			WLog_ERR(TAG, "Failed to reset nsc codec context");
			nsc_context_free(codecs->nsc);
			return FALSE;
		}
	}

	if ((flags & FREERDP_CODEC_REMOTEFX) && !codecs->rfx)
	{
		if (!(codecs->rfx = rfx_context_new(FALSE)))
		{
			WLog_ERR(TAG, "Failed to create rfx codec context");
			return FALSE;
		}

		if (!rfx_context_reset(codecs->rfx, width, height))
		{
			WLog_ERR(TAG, "Failed to reset rfx codec context");
			rfx_context_free(codecs->rfx);
			return FALSE;
		}
	}

	if ((flags & FREERDP_CODEC_CLEARCODEC) && !codecs->clear)
	{
		if (!(codecs->clear = clear_context_new(FALSE)))
		{
			WLog_ERR(TAG, "Failed to create clear codec context");
			return FALSE;
		}

		if (!clear_context_reset(codecs->clear))
		{
			WLog_ERR(TAG, "Failed to reset clear codec context");
			clear_context_free(codecs->clear);
			return FALSE;
		}
	}

	if (flags & FREERDP_CODEC_ALPHACODEC)
	{

	}

	if ((flags & FREERDP_CODEC_PROGRESSIVE) && !codecs->progressive)
	{
		if (!(codecs->progressive = progressive_context_new(FALSE)))
		{
			WLog_ERR(TAG, "Failed to create progressive codec context");
			return FALSE;
		}

		if (!progressive_context_reset(codecs->progressive))
		{
			WLog_ERR(TAG, "Failed to reset progressive codec context");
			progressive_context_free(codecs->progressive);
			return FALSE;
		}
	}

	if ((flags & (FREERDP_CODEC_AVC420 | FREERDP_CODEC_AVC444)) && !codecs->h264)
	{
		if (!(codecs->h264 = h264_context_new(FALSE)))
		{
			WLog_ERR(TAG, "Failed to create h264 codec context");
			return FALSE;
		}

		if (!h264_context_reset(codecs->h264, width, height))
		{
			WLog_ERR(TAG, "Failed to reset h264 codec context");
			h264_context_free(codecs->h264);
			return FALSE;
		}
	}

	return TRUE;
}

BOOL freerdp_client_codecs_reset(rdpCodecs* codecs, UINT32 flags, UINT32 width, UINT32 height)
{
	BOOL rc = TRUE;

	if (flags & FREERDP_CODEC_INTERLEAVED)
	{
		if (codecs->interleaved)
		{
			rc &= bitmap_interleaved_context_reset(codecs->interleaved);
		}
	}

	if (flags & FREERDP_CODEC_PLANAR)
	{
		if (codecs->planar)
		{
			rc &= freerdp_bitmap_planar_context_reset(codecs->planar);
		}
	}

	if (flags & FREERDP_CODEC_NSCODEC)
	{
		if (codecs->nsc)
		{
			rc &= nsc_context_reset(codecs->nsc, width, height);
		}
	}

	if (flags & FREERDP_CODEC_REMOTEFX)
	{
		if (codecs->rfx)
		{
			rc &= rfx_context_reset(codecs->rfx, width, height);
		}
	}

	if (flags & FREERDP_CODEC_CLEARCODEC)
	{
		if (codecs->clear)
		{
			rc &= clear_context_reset(codecs->clear);
		}
	}

	if (flags & FREERDP_CODEC_ALPHACODEC)
	{

	}

	if (flags & FREERDP_CODEC_PROGRESSIVE)
	{
		if (codecs->progressive)
		{
			rc &= progressive_context_reset(codecs->progressive);
		}
	}

	if (flags & (FREERDP_CODEC_AVC420 | FREERDP_CODEC_AVC444))
	{
		if (codecs->h264)
		{
			rc &= h264_context_reset(codecs->h264, width, height);
		}
	}

	return rc;
}

rdpCodecs* codecs_new(rdpContext* context)
{
	rdpCodecs* codecs;

	codecs = (rdpCodecs*) calloc(1, sizeof(rdpCodecs));

	if (codecs)
		codecs->context = context;

	return codecs;
}

void codecs_free(rdpCodecs* codecs)
{
	if (!codecs)
		return;

	if (codecs->rfx)
	{
		rfx_context_free(codecs->rfx);
		codecs->rfx = NULL;
	}

	if (codecs->nsc)
	{
		nsc_context_free(codecs->nsc);
		codecs->nsc = NULL;
	}

	if (codecs->h264)
	{
		h264_context_free(codecs->h264);
		codecs->h264 = NULL;
	}

	if (codecs->clear)
	{
		clear_context_free(codecs->clear);
		codecs->clear = NULL;
	}

	if (codecs->progressive)
	{
		progressive_context_free(codecs->progressive);
		codecs->progressive = NULL;
	}

	if (codecs->planar)
	{
		freerdp_bitmap_planar_context_free(codecs->planar);
		codecs->planar = NULL;
	}

	if (codecs->interleaved)
	{
		bitmap_interleaved_context_free(codecs->interleaved);
		codecs->interleaved = NULL;
	}

	free(codecs);
}

