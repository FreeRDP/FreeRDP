/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Compressed jpeg
 *
 * Copyright 2012 Jay Sorg <jay.sorg@gmail.com>
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

#include <winpr/stream.h>

#include <freerdp/codec/color.h>

#include <freerdp/codec/jpeg.h>

#ifdef WITH_JPEG

#define XMD_H

#include <jpeglib.h>

struct mydata_decomp
{
	char* data;
	int data_bytes;
};

/*****************************************************************************/
static void my_init_source(j_decompress_ptr cinfo)
{
}

/*****************************************************************************/
static boolean my_fill_input_buffer(j_decompress_ptr cinfo)
{
	struct mydata_decomp* md;

	md = (struct mydata_decomp*)(cinfo->client_data);
	cinfo->src->next_input_byte = (unsigned char*)(md->data);
	cinfo->src->bytes_in_buffer = md->data_bytes;
	return 1;
}

/*****************************************************************************/
static void my_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
}

/*****************************************************************************/
static boolean my_resync_to_restart(j_decompress_ptr cinfo, int desired)
{
	return 1;
}

/*****************************************************************************/
static void my_term_source(j_decompress_ptr cinfo)
{
}

/*****************************************************************************/
static int
do_decompress(char* comp_data, int comp_data_bytes,
              int* width, int* height, int* bpp,
              char* decomp_data, int* decomp_data_bytes)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	struct jpeg_source_mgr src_mgr;
	struct mydata_decomp md;
	JSAMPROW row_pointer[1];

	memset(&cinfo, 0, sizeof(cinfo));
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);

	memset(&src_mgr, 0, sizeof(src_mgr));
	cinfo.src = &src_mgr;
	src_mgr.init_source = my_init_source;
	src_mgr.fill_input_buffer = my_fill_input_buffer;
	src_mgr.skip_input_data = my_skip_input_data;
	src_mgr.resync_to_restart = my_resync_to_restart;
	src_mgr.term_source = my_term_source;

	memset(&md, 0, sizeof(md));
	md.data = comp_data;
	md.data_bytes = comp_data_bytes;
	cinfo.client_data = &md;

	jpeg_read_header(&cinfo, 1);

	cinfo.out_color_space = JCS_RGB;

	*width = cinfo.image_width;
	*height = cinfo.image_height;
	*bpp = cinfo.num_components * 8;

	jpeg_start_decompress(&cinfo);

	while(cinfo.output_scanline < cinfo.image_height)
	{
		row_pointer[0] = (JSAMPROW) decomp_data;
		jpeg_read_scanlines(&cinfo, row_pointer, 1);
		decomp_data += cinfo.image_width * cinfo.num_components;
	}
	*decomp_data_bytes = cinfo.output_width *
			cinfo.output_height * cinfo.num_components;
	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
	return 0;
}

/* jpeg decompress */
BOOL jpeg_decompress(BYTE* input, BYTE* output, int width, int height, int size, int bpp)
{
	int lwidth;
	int lheight;
	int lbpp;
	int ldecomp_data_bytes;

	if (bpp != 24)
	{
		return 0;
	}
	if (do_decompress((char*)input, size,
			&lwidth, &lheight, &lbpp,
			(char*)output, &ldecomp_data_bytes) != 0)
	{
		return 0;
	}
	if (lwidth != width || lheight != height || lbpp != bpp)
	{
		return 0;
	}
	return 1;
}

#else

BOOL jpeg_decompress(BYTE* input, BYTE* output, int width, int height, int size, int bpp)
{
	return 0;
}

#endif
