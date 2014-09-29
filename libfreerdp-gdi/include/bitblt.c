/**
 * FreeRDP: A Remote Desktop Protocol Client
 * GDI LineTo
 *
 * Copyright 2010-2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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
/* do not include this file directly! */


//NB: memcpy and memset optimized better if compiler can see aligning 
//https://gcc.gnu.org/ml/gcc-bugs/2000-03/msg00156.html
//http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.faqs/ka3934.html


//#define BITBLT_PIXELBYTES 2
//#define BITBLT_ALIGN uint32


#define MAKEFN_INNER(begin, middle, end) begin ## _ ## middle ## _ ## end
#define MAKEFN(begin, middle, end) MAKEFN_INNER(begin, middle, end)


static int MAKEFN(BitBlt_BLACKNESS, BITBLT_PIXELBYTES, BITBLT_ALIGN) 
			(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
{
	uint w;
	BITBLT_ALIGN* dstp, *end;
	const size_t BytesPerLine = nWidth * BITBLT_PIXELBYTES;// * hdcDest->bytesPerPixel	

	dstp = (BITBLT_ALIGN *)gdi_get_bitmap_pointer_ex(hdcDest, nXDest, nYDest, (uint8**)&end, &w);
	if (!dstp)
		return 0;

	for (; nHeight; --nHeight)
	{
		memset((BITBLT_ALIGN *)dstp, 0, BytesPerLine);

		if ((dstp+=w)>=end) break;
	}

	return 0;
}

static int MAKEFN(BitBlt_WHITENESS, BITBLT_PIXELBYTES, BITBLT_ALIGN) 
			(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
{
	uint w;
	BITBLT_ALIGN* dstp, *end;
	const size_t BytesPerLine = nWidth * BITBLT_PIXELBYTES;// * hdcDest->bytesPerPixel	

	dstp = (BITBLT_ALIGN *)gdi_get_bitmap_pointer_ex(hdcDest, nXDest, nYDest, (uint8**)&end, &w);
	if (!dstp)
		return 0;

	for (; nHeight; --nHeight)
	{
		memset((BITBLT_ALIGN *)dstp, 0xff, BytesPerLine);

		if ((dstp+=w)>=end) break;
	}

	return 0;
}

static int MAKEFN(BitBlt_SRCCOPY, BITBLT_PIXELBYTES, BITBLT_ALIGN) 
			(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	uint i, srcw, dstw;
	BITBLT_ALIGN *srcp, *srce;
	BITBLT_ALIGN *dstp, *dste;
	const int BytesPerLine = nWidth * BITBLT_PIXELBYTES;// * hdcDest->bytesPerPixel

	srcp = (BITBLT_ALIGN*)gdi_get_bitmap_pointer_ex(hdcSrc, nXSrc, nYSrc, (uint8**)&srce, &srcw);
	if (!srcp)
		return 0;

	dstp = (BITBLT_ALIGN*)gdi_get_bitmap_pointer_ex(hdcDest, nXDest, nYDest, (uint8**)&dste, &dstw);
	if (!dstp)
		return 0;


	if ((hdcDest->selectedObject != hdcSrc->selectedObject) ||
	    gdi_CopyOverlap(nXDest, nYDest, nWidth, nHeight, nXSrc, nYSrc) == 0)
	{
		for (; nHeight; --nHeight)
		{
			memcpy(dstp, srcp, BytesPerLine);
			if ((dstp+= dstw)>=dste || (srcp+= srcw)>=srce) break;
		}
	}
	else if (nYSrc < nYDest)
	{
		/* copy down */
		for (i = 0; i<nHeight; ++i)
		{
			if ((dstp+= dstw)>=dste || (srcp+= srcw)>=srce) break;
		}

		for ( ; i; --i)
		{
			dstp-= dstw;
			srcp-= srcw;
			memcpy((BITBLT_ALIGN *)dstp, (BITBLT_ALIGN *)srcp, BytesPerLine);
		}
	}
	else if (nYSrc > nYDest)
	{
		/* copy up */

		for (; nHeight; --nHeight)
		{
			memcpy(dstp, srcp, BytesPerLine);
			if ((dstp+= dstw)>=dste || (srcp+= srcw)>=srce) break;
		}
	}
	else if (nXDest!=nXSrc)
	{
		/* horizontal shift*/

		for (; nHeight; --nHeight)
		{
			memmove(dstp, srcp, BytesPerLine);
			if ((dstp+= dstw)>=dste || (srcp+= srcw)>=srce) break;
		}
	}
	
	return 0;
}
