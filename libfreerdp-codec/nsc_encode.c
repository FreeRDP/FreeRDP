/**
 * FreeRDP: A Remote Desktop Protocol client.
 * NSCodec Encoder
 *
 * Copyright 2012 Vic Lee
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <freerdp/codec/nsc.h>
#include <freerdp/utils/memory.h>

#include "nsc_types.h"
#include "nsc_encode.h"

static void nsc_context_initialize_encode(NSC_CONTEXT* context)
{
	int i;
	uint32 length;
	uint32 tempWidth;
	uint32 tempHeight;

	tempWidth = ROUND_UP_TO(context->width, 8);
	tempHeight = ROUND_UP_TO(context->height, 2);
	/* The maximum length a decoded plane can reach in all cases */
	length = tempWidth * tempHeight;
	if (length > context->priv->plane_buf_length)
	{
		for (i = 0; i < 4; i++)
			context->priv->plane_buf[i] = (uint8*) xrealloc(context->priv->plane_buf[i], length);
		context->priv->plane_buf_length = length;
	}

	if (context->nsc_stream.ChromaSubSamplingLevel > 0)
	{
		context->nsc_stream.PlaneByteCount[0] = tempWidth * context->height;
		context->nsc_stream.PlaneByteCount[1] = tempWidth * tempHeight / 4;
		context->nsc_stream.PlaneByteCount[2] = tempWidth * tempHeight / 4;
		context->nsc_stream.PlaneByteCount[3] = context->width * context->height;
	}
	else
	{
		context->nsc_stream.PlaneByteCount[0] = context->width * context->height;
		context->nsc_stream.PlaneByteCount[1] = context->width * context->height;
		context->nsc_stream.PlaneByteCount[2] = context->width * context->height;
		context->nsc_stream.PlaneByteCount[3] = context->width * context->height;
	}
}

void nsc_encode(NSC_CONTEXT* context, uint8* bmpdata, int rowstride)
{
}

void nsc_compose_message(NSC_CONTEXT* context, STREAM* s,
	uint8* bmpdata, int width, int height, int rowstride)
{
	context->width = width;
	context->height = height;
	nsc_context_initialize_encode(context);
}
