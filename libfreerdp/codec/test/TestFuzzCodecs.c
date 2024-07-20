/* https://github.com/ergnoorr/fuzzrdp
 *
 * MIT License
 *
 * Copyright (c) 2024 ergnoorr
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <freerdp/assistance.h>

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/platform.h>
#include <freerdp/codec/interleaved.h>
#include <freerdp/codec/planar.h>
#include <freerdp/codec/bulk.h>
#include <freerdp/codec/clear.h>
#include <freerdp/codec/zgfx.h>
#include <freerdp/log.h>
#include <winpr/bitstream.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/codec/progressive.h>

#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>

#include "../progressive.h"
#include "../mppc.h"
#include "../xcrush.h"
#include "../ncrush.h"

static BOOL test_ClearDecompressExample(UINT32 nr, UINT32 width, UINT32 height,
                                        const BYTE* pSrcData, const UINT32 SrcSize)
{
	BOOL rc = FALSE;
	int status = 0;
	BYTE* pDstData = calloc(1ull * width * height, 4);
	CLEAR_CONTEXT* clear = clear_context_new(FALSE);

	WINPR_UNUSED(nr);
	if (!clear || !pDstData)
		goto fail;

	status = clear_decompress(clear, pSrcData, SrcSize, width, height, pDstData,
	                          PIXEL_FORMAT_XRGB32, 0, 0, 0, width, height, NULL);
	// printf("clear_decompress example %" PRIu32 " status: %d\n", nr, status);
	// fflush(stdout);
	rc = (status == 0);
fail:
	clear_context_free(clear);
	free(pDstData);
	return rc;
}

static int TestFreeRDPCodecClear(const uint8_t* Data, size_t Size)
{
	if (Size > UINT32_MAX)
		return -1;
	test_ClearDecompressExample(2, 78, 17, Data, (UINT32)Size);
	test_ClearDecompressExample(3, 64, 24, Data, (UINT32)Size);
	test_ClearDecompressExample(4, 7, 15, Data, (UINT32)Size);
	return 0;
}

static int TestFreeRDPCodecXCrush(const uint8_t* Data, size_t Size)
{
	if (Size > UINT32_MAX)
		return -1;

	const BYTE* OutputBuffer = NULL;
	UINT32 DstSize = 0;
	XCRUSH_CONTEXT* xcrush = xcrush_context_new(TRUE);
	if (!xcrush)
		return 0;
	xcrush_decompress(xcrush, Data, (UINT32)Size, &OutputBuffer, &DstSize, 0);
	xcrush_context_free(xcrush);
	return 0;
}

static int test_ZGfxDecompressFoxSingle(const uint8_t* Data, size_t Size)
{
	if (Size > UINT32_MAX)
		return -1;
	int rc = -1;
	int status = 0;
	UINT32 Flags = 0;
	const BYTE* pSrcData = (const BYTE*)Data;
	UINT32 SrcSize = (UINT32)Size;
	UINT32 DstSize = 0;
	BYTE* pDstData = NULL;
	ZGFX_CONTEXT* zgfx = zgfx_context_new(TRUE);

	if (!zgfx)
		return -1;

	status = zgfx_decompress(zgfx, pSrcData, SrcSize, &pDstData, &DstSize, Flags);
	if (status < 0)
		goto fail;

	rc = 0;
fail:
	free(pDstData);
	zgfx_context_free(zgfx);
	return rc;
}

static int TestFreeRDPCodecZGfx(const uint8_t* Data, size_t Size)
{
	test_ZGfxDecompressFoxSingle(Data, Size);
	return 0;
}

static BOOL test_NCrushDecompressBells(const uint8_t* Data, size_t Size)
{
	if (Size > UINT32_MAX)
		return FALSE;

	BOOL rc = FALSE;
	int status = 0;
	UINT32 Flags = PACKET_COMPRESSED | 2;
	const BYTE* pSrcData = (const BYTE*)Data;
	UINT32 SrcSize = (UINT32)Size;
	UINT32 DstSize = 0;
	const BYTE* pDstData = NULL;
	NCRUSH_CONTEXT* ncrush = ncrush_context_new(FALSE);

	if (!ncrush)
		return rc;

	status = ncrush_decompress(ncrush, pSrcData, SrcSize, &pDstData, &DstSize, Flags);
	if (status < 0)
		goto fail;

	rc = TRUE;
fail:
	ncrush_context_free(ncrush);
	return rc;
}

static int TestFreeRDPCodecNCrush(const uint8_t* Data, size_t Size)
{
	test_NCrushDecompressBells(Data, Size);
	return 0;
}

static const size_t IMG_WIDTH = 64;
static const size_t IMG_HEIGHT = 64;
static const size_t FORMAT_SIZE = 4;
static const UINT32 FORMAT = PIXEL_FORMAT_XRGB32;

static int TestFreeRDPCodecRemoteFX(const uint8_t* Data, size_t Size)
{
	int rc = -1;
	REGION16 region = { 0 };
	RFX_CONTEXT* context = rfx_context_new(FALSE);
	BYTE* dest = calloc(IMG_WIDTH * IMG_HEIGHT, FORMAT_SIZE);
	size_t stride = FORMAT_SIZE * IMG_WIDTH;
	if (!context)
		goto fail;
	if (Size > UINT32_MAX)
		goto fail;
	if (stride > UINT32_MAX)
		goto fail;
	if (!dest)
		goto fail;

	region16_init(&region);
	if (!rfx_process_message(context, Data, (UINT32)Size, 0, 0, dest, FORMAT, (UINT32)stride,
	                         IMG_HEIGHT, &region))
		goto fail;

	region16_clear(&region);
	if (!rfx_process_message(context, Data, (UINT32)Size, 0, 0, dest, FORMAT, (UINT32)stride,
	                         IMG_HEIGHT, &region))
		goto fail;
	region16_print(&region);

	rc = 0;
fail:
	region16_uninit(&region);
	rfx_context_free(context);
	free(dest);
	return rc;
}

static int test_MppcDecompressBellsRdp5(const uint8_t* Data, size_t Size)
{
	int rc = -1;
	int status = 0;
	UINT32 Flags = PACKET_AT_FRONT | PACKET_COMPRESSED | 1;
	const BYTE* pSrcData = Data;
	UINT32 SrcSize = (UINT32)Size;
	UINT32 DstSize = 0;
	const BYTE* pDstData = NULL;
	MPPC_CONTEXT* mppc = mppc_context_new(1, FALSE);

	if (!mppc)
		return -1;

	status = mppc_decompress(mppc, pSrcData, SrcSize, &pDstData, &DstSize, Flags);

	if (status < 0)
		goto fail;

	rc = 0;

fail:
	mppc_context_free(mppc);
	return rc;
}

static int test_MppcDecompressBellsRdp4(const uint8_t* Data, size_t Size)
{
	if (Size > UINT32_MAX)
		return -1;
	int rc = -1;
	int status = 0;
	UINT32 Flags = PACKET_AT_FRONT | PACKET_COMPRESSED | 0;
	const BYTE* pSrcData = (const BYTE*)Data;
	UINT32 SrcSize = (UINT32)Size;
	UINT32 DstSize = 0;
	const BYTE* pDstData = NULL;
	MPPC_CONTEXT* mppc = mppc_context_new(0, FALSE);

	if (!mppc)
		return -1;

	status = mppc_decompress(mppc, pSrcData, SrcSize, &pDstData, &DstSize, Flags);

	if (status < 0)
		goto fail;

	rc = 0;
fail:
	mppc_context_free(mppc);
	return rc;
}

static int test_MppcDecompressBufferRdp5(const uint8_t* Data, size_t Size)
{
	if (Size > UINT32_MAX)
		return -1;
	int rc = -1;
	int status = 0;
	UINT32 Flags = PACKET_AT_FRONT | PACKET_COMPRESSED | 1;
	const BYTE* pSrcData = (const BYTE*)Data;
	UINT32 SrcSize = (UINT32)Size;
	UINT32 DstSize = 0;
	const BYTE* pDstData = NULL;
	MPPC_CONTEXT* mppc = mppc_context_new(1, FALSE);

	if (!mppc)
		return -1;

	status = mppc_decompress(mppc, pSrcData, SrcSize, &pDstData, &DstSize, Flags);

	if (status < 0)
		goto fail;

	rc = 0;
fail:
	mppc_context_free(mppc);
	return rc;
}

static int TestFreeRDPCodecMppc(const uint8_t* Data, size_t Size)
{
	test_MppcDecompressBellsRdp5(Data, Size);
	test_MppcDecompressBellsRdp4(Data, Size);
	test_MppcDecompressBufferRdp5(Data, Size);
	return 0;
}

static BOOL progressive_decode(const uint8_t* Data, size_t Size)
{
	BOOL res = FALSE;
	int rc = 0;
	BYTE* resultData = NULL;
	UINT32 ColorFormat = PIXEL_FORMAT_BGRX32;
	REGION16 invalidRegion = { 0 };
	UINT32 scanline = 4240;
	UINT32 width = 1060;
	UINT32 height = 827;
	if (Size > UINT32_MAX)
		return FALSE;

	PROGRESSIVE_CONTEXT* progressiveDec = progressive_context_new(FALSE);

	region16_init(&invalidRegion);
	if (!progressiveDec)
		goto fail;

	resultData = calloc(scanline, height);
	if (!resultData)
		goto fail;

	rc = progressive_create_surface_context(progressiveDec, 0, width, height);
	if (rc <= 0)
		goto fail;

	rc = progressive_decompress(progressiveDec, Data, (UINT32)Size, resultData, ColorFormat,
	                            scanline, 0, 0, &invalidRegion, 0, 0);
	if (rc < 0)
		goto fail;

	res = TRUE;
fail:
	region16_uninit(&invalidRegion);
	progressive_context_free(progressiveDec);
	free(resultData);
	return res;
}

static int TestFreeRDPCodecProgressive(const uint8_t* Data, size_t Size)
{
	progressive_decode(Data, Size);
	return 0;
}

static BOOL i_run_encode_decode(UINT16 bpp, BITMAP_INTERLEAVED_CONTEXT* encoder,
                                BITMAP_INTERLEAVED_CONTEXT* decoder, const uint8_t* Data,
                                size_t Size)
{
	BOOL rc2 = FALSE;
	BOOL rc = 0;
	const UINT32 w = 64;
	const UINT32 h = 64;
	const UINT32 x = 0;
	const UINT32 y = 0;
	const UINT32 format = PIXEL_FORMAT_RGBX32;
	const size_t step = (w + 13ull) * 4ull;
	const size_t SrcSize = step * h;
	BYTE* pSrcData = calloc(1, SrcSize);
	BYTE* pDstData = calloc(1, SrcSize);
	BYTE* tmp = calloc(1, SrcSize);

	WINPR_UNUSED(encoder);
	if (!pSrcData || !pDstData || !tmp)
		goto fail;

	if (Size > UINT32_MAX)
		goto fail;

	winpr_RAND(pSrcData, SrcSize);

	if (!bitmap_interleaved_context_reset(decoder))
		goto fail;

	rc = interleaved_decompress(decoder, Data, (UINT32)Size, w, h, bpp, pDstData, format, step, x,
	                            y, w, h, NULL);

	if (!rc)
		goto fail;

	rc2 = TRUE;
fail:
	free(pSrcData);
	free(pDstData);
	free(tmp);
	return rc2;
}

static int TestFreeRDPCodecInterleaved(const uint8_t* Data, size_t Size)
{
	int rc = -1;
	BITMAP_INTERLEAVED_CONTEXT* decoder = bitmap_interleaved_context_new(FALSE);

	if (!decoder)
		goto fail;

	i_run_encode_decode(24, NULL, decoder, Data, Size);
	i_run_encode_decode(16, NULL, decoder, Data, Size);
	i_run_encode_decode(15, NULL, decoder, Data, Size);
	rc = 0;
fail:
	bitmap_interleaved_context_free(decoder);
	return rc;
}

static BOOL RunTestPlanar(BITMAP_PLANAR_CONTEXT* planar, const BYTE* srcBitmap,
                          const UINT32 srcFormat, const UINT32 dstFormat, const UINT32 width,
                          const UINT32 height, const uint8_t* Data, size_t Size)
{
	BOOL rc = FALSE;
	WINPR_UNUSED(srcBitmap);
	WINPR_UNUSED(srcFormat);
	if (Size > UINT32_MAX)
		return FALSE;
	UINT32 dstSize = (UINT32)Size;
	const BYTE* compressedBitmap = Data;
	BYTE* decompressedBitmap =
	    (BYTE*)calloc(height, 1ull * width * FreeRDPGetBytesPerPixel(dstFormat));
	rc = TRUE;

	if (!decompressedBitmap)
		goto fail;

	if (!planar_decompress(planar, compressedBitmap, dstSize, width, height, decompressedBitmap,
	                       dstFormat, 0, 0, 0, width, height, FALSE))
	{
		goto fail;
	}

	rc = TRUE;
fail:
	free(decompressedBitmap);
	return rc;
}

static BOOL TestPlanar(const UINT32 format, const uint8_t* Data, size_t Size)
{
	BOOL rc = FALSE;
	const DWORD planarFlags = PLANAR_FORMAT_HEADER_NA | PLANAR_FORMAT_HEADER_RLE;
	BITMAP_PLANAR_CONTEXT* planar = freerdp_bitmap_planar_context_new(planarFlags, 64, 64);

	if (!planar)
		goto fail;

	RunTestPlanar(planar, NULL, PIXEL_FORMAT_RGBX32, format, 64, 64, Data, Size);

	RunTestPlanar(planar, NULL, PIXEL_FORMAT_RGB16, format, 32, 32, Data, Size);

	rc = TRUE;
fail:
	freerdp_bitmap_planar_context_free(planar);
	return rc;
}

static int TestFreeRDPCodecPlanar(const uint8_t* Data, size_t Size)
{
	TestPlanar(0, Data, Size);
	return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	if (Size < 4)
		return 0;

	TestFreeRDPCodecClear(Data, Size);
	TestFreeRDPCodecXCrush(Data, Size);
	TestFreeRDPCodecZGfx(Data, Size);
	TestFreeRDPCodecNCrush(Data, Size);
	TestFreeRDPCodecRemoteFX(Data, Size);
	TestFreeRDPCodecMppc(Data, Size);
	TestFreeRDPCodecProgressive(Data, Size);
	TestFreeRDPCodecInterleaved(Data, Size);
	TestFreeRDPCodecPlanar(Data, Size);

	return 0;
}
