/**
 * FreeRDP: A Remote Desktop Protocol client.
 * NSCodec Codec
 *
 * Copyright 2011 Samsung, Author Jiten Pathy
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
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

/* we store the 9th bits at the end of stream as bitstream */
void nsc_cl_expand(STREAM* stream, uint8 shiftcount, uint32 origsz)
{
	uint8* sbitstream;
	uint8* temptr;
	uint8 sign,bitoff;
	uint32 bitno;
	sbitstream = stream->data + origsz;

	do
	{
		sign = (*(stream->p) << (shiftcount - 1)) & 0x80;
		bitno = stream->p - stream->data;
		*(stream->p++) <<= shiftcount;
		temptr = sbitstream + ((bitno) >> 3);
		bitoff = bitno % 0x8; 
		(*temptr) |= (sign >> bitoff);
	}
	while (((uint32)(stream->p - stream->data)) < origsz);

	stream->p = stream->data;
}

void nsc_chroma_supersample(NSC_CONTEXT* context)
{
	int i;
	uint8* cur;
	uint8* tptr;
	uint8* nbitstream;
	uint8* sbitstream;
	uint8 val, bitoff, sign;
	uint16 w, h, pw, row;
	uint32 alloclen, orglen, bytno;
	STREAM* new_s;
	STREAM* temp;

	w = context->width;
	h = context->height;
	alloclen = orglen = w * h;
	pw = ROUND_UP_TO(context->width, 8);
	temp = stream_new(0);

	for (i = 0; i < 3; i++)
	{
		if (i != 0)
			alloclen = orglen + ((orglen & 0x7) ? (orglen >> 3) + 0x1 : (orglen >> 3));

		new_s = stream_new(alloclen);
		stream_attach(temp, context->org_buf[i]->data, context->org_buf[i]->size);

		sbitstream = temp->data + context->OrgByteCount[i];
		nbitstream = new_s->data + orglen;
		cur  = new_s->p;

		if (i == 1)
			pw >>= 1;

		while (((uint32)(temp->p - temp->data)) < context->OrgByteCount[i])
		{
			bytno = temp->p - temp->data;
			bitoff = bytno % 0x8; 
			stream_read_uint8(temp, val);
			*cur = val;
			row = (temp->p - temp->data) % pw;

			if (i == 0)
			{
				cur++;
				if (row >= w)
					stream_seek(temp, pw-row);
			}
			else
			{
				tptr = sbitstream + ((bytno) >> 3);
				sign = ((*tptr) << bitoff) & 0x80;
				bytno = cur - new_s->data;
				bitoff = bytno % 8;
				*(nbitstream + (bytno >> 3)) |= (sign >> bitoff);

				if ((bytno+w) < orglen)
				{
					*(cur + w) = val;
					bitoff = (bytno + w) % 8;
					*(nbitstream + ((bytno + w) >> 3)) |= (sign >> bitoff);
				}

				if ((bytno+1) % w)
				{
					*(cur+1) = val;
					bitoff = (bytno + 1) % 8;
					*(nbitstream + ((bytno + 1) >> 3)) |= (sign >> bitoff);
					if ((bytno+w) < orglen)
					{
						*(cur+w+1) = val;
						bitoff = (bytno + w + 1) % 8;
						*(nbitstream + ((bytno + w + 1) >> 3)) |= (sign >> bitoff);
					}
				}

				cur += 2;
				bytno = cur - new_s->data;

				if (((bytno/w) < h) && ((bytno) % w) < 2 )
				{
					if (w % 2)
						cur += w-1;
					else
						cur += w;
				}

				if ((row*2) >= w)
					stream_seek(temp, pw-row);
			}
		}

		xfree(temp->data);
		stream_detach(temp);
		stream_attach(context->org_buf[i], new_s->data, new_s->size);
		context->OrgByteCount[i] = orglen;
	}
}

void nsc_ycocg_rgb(NSC_CONTEXT* context)
{
	uint8* sbitstream[2];
	uint8 bitoff, sign[2], i, val, tmp;
	sint16 rgb[3], ycocg[3];
	uint32 bytno, size;
	size = context->OrgByteCount[0];

	for (i = 1; i < 3; i++)
		sbitstream[i-1] = context->org_buf[i]->data + context->OrgByteCount[i];

	do
	{
		for (i = 0; i < 3; i++)
			ycocg[i] = *(context->org_buf[i]->p);

		for (i = 1; i < 3; i++)
		{
			bytno = context->OrgByteCount[i] - size;
			bitoff = bytno % 8;
			sign[i-1] = (*(sbitstream[i-1] + (bytno >> 3)) >> (7 - bitoff)) & 0x1;
			ycocg[i] = (((sint16)(0 - sign[i-1])) << 8) | ycocg[i];
		}

		rgb[0] = ycocg[0] + (ycocg[1] >> 1) - (ycocg[2] >> 1);
		rgb[1] = ycocg[0] + (ycocg[2] >> 1);
		rgb[2] = ycocg[0] - (ycocg[1] >> 1) - (ycocg[2] >> 1);

		for (i = 0; i < 3; i++)
		{
			tmp = (rgb[i] >> 8) & 0xff;
			if (tmp == 0xff)
				val = 0x00;
			else if (tmp == 0x1)
				val = 0xff;
			else
				val = (uint8) rgb[i];

			stream_write_uint8(context->org_buf[i], val);
		}

		size--;
	}
	while (size);

	for (i = 0; i < 3; i++)
		context->org_buf[i]->p = context->org_buf[i]->data;
}

void nsc_colorloss_recover(NSC_CONTEXT* context)
{
	int i;
	uint8 cllvl;
	cllvl = context->nsc_stream->colorLossLevel;

	for (i = 1; i < 3; i++)
		nsc_cl_expand(context->org_buf[i], cllvl, context->OrgByteCount[i]);
}

void nsc_rle_decode(STREAM* in, STREAM* out, uint32 origsz)
{
	uint32 i;
	uint8 value;
	i = origsz;

	while (i > 4)
	{
		stream_read_uint8(in, value);

		if (i == 5)
		{
			stream_write_uint8(out,value);
			i-=1;
		}
		else if (value == *(in->p))
		{
			stream_seek(in, 1);

			if (*(in->p) < 0xFF)
			{
				uint8 len;
				stream_read_uint8(in, len);
				stream_set_byte(out, value, len+2);
				i -= (len+2);
			}
			else
			{
				uint32 len;
				stream_seek(in, 1);
				stream_read_uint32(in, len);
				stream_set_byte(out, value, len);
				i -= len;
			}
		}
		else
		{
			stream_write_uint8(out, value);
			i -= 1;
		}
	}

	stream_copy(out, in, 4);
}

void nsc_rle_decompress_data(NSC_CONTEXT* context)
{
	STREAM* rles;
	uint16 i;
	uint32 origsize;
	rles = stream_new(0);
	rles->p = rles->data = context->nsc_stream->pdata->p;
	rles->size = context->nsc_stream->pdata->size;

	for (i = 0; i < 4; i++)
	{
		origsize = context->OrgByteCount[i];

		if (i == 3 && context->nsc_stream->PlaneByteCount[i] == 0)
			stream_set_byte(context->org_buf[i], 0xff, origsize);
		else if (context->nsc_stream->PlaneByteCount[i] < origsize)
			nsc_rle_decode(rles, context->org_buf[i], origsize);
		else
			stream_copy(context->org_buf[i], rles, origsize);

		context->org_buf[i]->p = context->org_buf[i]->data;
	}
}

void nsc_combine_argb(NSC_CONTEXT* context)
{
	int i;
	uint8* bmp;
	bmp = context->bmpdata;

	for (i = 0; i < (context->width * context->height); i++)
	{
		stream_read_uint8(context->org_buf[2], *bmp++);
		stream_read_uint8(context->org_buf[1], *bmp++);
		stream_read_uint8(context->org_buf[0], *bmp++);
		stream_read_uint8(context->org_buf[3], *bmp++);
	}
}

void nsc_stream_initialize(NSC_CONTEXT* context, STREAM* s)
{
	int i;

	for (i = 0; i < 4; i++)
		stream_read_uint32(s, context->nsc_stream->PlaneByteCount[i]);

	stream_read_uint8(s, context->nsc_stream->colorLossLevel);
	stream_read_uint8(s, context->nsc_stream->ChromaSubSamplingLevel);
	stream_seek(s, 2);

	context->nsc_stream->pdata = stream_new(0);
	stream_attach(context->nsc_stream->pdata, s->p, BYTESUM(context->nsc_stream->PlaneByteCount));
}

void nsc_context_initialize(NSC_CONTEXT* context, STREAM* s)
{
	int i;
	uint32 tempsz;
	nsc_stream_initialize(context, s);
	context->bmpdata = xzalloc(context->width * context->height * 4);

	for (i = 0; i < 4; i++)
		context->OrgByteCount[i]=context->width * context->height;

	if (context->nsc_stream->ChromaSubSamplingLevel > 0)	/* [MS-RDPNSC] 2.2 */
	{
		uint32 tempWidth,tempHeight;
		tempWidth = ROUND_UP_TO(context->width, 8);
		context->OrgByteCount[0] = tempWidth * context->height;
		tempWidth = tempWidth >> 1 ;
		tempHeight = ROUND_UP_TO(context->height, 2);
		tempHeight = tempHeight >> 1;
		context->OrgByteCount[1] = tempWidth * tempHeight;
		context->OrgByteCount[2] = tempWidth * tempHeight;
	}
	for (i = 0; i < 4; i++)
	{
		tempsz = context->OrgByteCount[i];

		if (i == 1 || i == 2)
			tempsz += (tempsz & 0x7) ? (tempsz >> 3) + 0x1 : (tempsz >> 3); /* extra bytes/8 bytes for bitstream to store the 9th bit after colorloss recover */

		context->org_buf[i] = stream_new(tempsz);
	}
}

void nsc_context_destroy(NSC_CONTEXT* context)
{
	int i;

	for (i = 0;i < 4; i++)
		stream_free(context->org_buf[i]);

	stream_detach(context->nsc_stream->pdata);
	xfree(context->bmpdata);
}

NSC_CONTEXT* nsc_context_new(void)
{
	NSC_CONTEXT* nsc_context;
	nsc_context = xnew(NSC_CONTEXT);
	nsc_context->nsc_stream = xnew(NSC_STREAM);
	return nsc_context;
}

void nsc_process_message(NSC_CONTEXT* context, uint8* data, uint32 length)
{
	STREAM* s;
	s = stream_new(0);
	stream_attach(s, data, length);
	nsc_context_initialize(context, s);

	/* RLE decode */
	nsc_rle_decompress_data(context);

	/* colorloss recover */
	nsc_colorloss_recover(context);

	/* Chroma supersample */
	if (context->nsc_stream->ChromaSubSamplingLevel > 0)
		nsc_chroma_supersample(context);
	
	/* YCoCg to RGB Convert */
	nsc_ycocg_rgb(context);

	/* Combine ARGB planes */
	nsc_combine_argb(context);
}
