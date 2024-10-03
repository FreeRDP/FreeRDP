#include <errno.h>

#include <winpr/wtypes.h>
#include <winpr/crt.h>
#include <winpr/string.h>
#include <winpr/path.h>
#include <winpr/image.h>
#include <winpr/print.h>
#include <winpr/wlog.h>
#include <winpr/sysinfo.h>
#include <winpr/file.h>

#include <freerdp/codec/region.h>

#include <freerdp/codecs.h>
#include <freerdp/utils/gfx.h>

#include <freerdp/codec/progressive.h>
#include <freerdp/channels/rdpgfx.h>
#include <freerdp/crypto/crypto.h>

#include "../progressive.h"

/**
 * Microsoft Progressive Codec Sample Data
 * (available under NDA only)
 *
 * <enc/dec>_<image#>_<quarter#>_<prog%>_<bitmap>.<type>
 *
 * readme.pdf
 *
 * bitmaps/
 * 	1920by1080-SampleImage1.bmp
 * 	1920by1080-SampleImage2.bmp
 * 	1920by1080-SampleImage3.bmp
 *
 * compress/
 * 	enc_0_0_025_sampleimage1.bin
 * 	enc_0_0_050_sampleimage1.bin
 * 	enc_0_0_075_sampleimage1.bin
 * 	enc_0_0_100_sampleimage1.bin
 * 	enc_0_1_025_sampleimage1.bin
 * 	enc_0_1_050_sampleimage1.bin
 * 	enc_0_1_075_sampleimage1.bin
 * 	enc_0_1_100_sampleimage1.bin
 * 	enc_0_2_025_sampleimage1.bin
 * 	enc_0_2_050_sampleimage1.bin
 * 	enc_0_2_075_sampleimage1.bin
 * 	enc_0_2_100_sampleimage1.bin
 * 	enc_0_3_025_sampleimage1.bin
 * 	enc_0_3_050_sampleimage1.bin
 * 	enc_0_3_075_sampleimage1.bin
 * 	enc_0_3_100_sampleimage1.bin
 * 	enc_1_0_025_sampleimage2.bin
 * 	enc_1_0_050_sampleimage2.bin
 * 	enc_1_0_075_sampleimage2.bin
 * 	enc_1_0_100_sampleimage2.bin
 * 	enc_1_1_025_sampleimage2.bin
 * 	enc_1_1_050_sampleimage2.bin
 * 	enc_1_1_075_sampleimage2.bin
 * 	enc_1_1_100_sampleimage2.bin
 * 	enc_1_2_025_sampleimage2.bin
 * 	enc_1_2_050_sampleimage2.bin
 * 	enc_1_2_075_sampleimage2.bin
 * 	enc_1_2_100_sampleimage2.bin
 * 	enc_1_3_025_sampleimage2.bin
 * 	enc_1_3_050_sampleimage2.bin
 * 	enc_1_3_075_sampleimage2.bin
 * 	enc_1_3_100_sampleimage2.bin
 * 	enc_2_0_025_sampleimage3.bin
 * 	enc_2_0_050_sampleimage3.bin
 * 	enc_2_0_075_sampleimage3.bin
 * 	enc_2_0_100_sampleimage3.bin
 * 	enc_2_1_025_sampleimage3.bin
 * 	enc_2_1_050_sampleimage3.bin
 * 	enc_2_1_075_sampleimage3.bin
 * 	enc_2_1_100_sampleimage3.bin
 * 	enc_2_2_025_sampleimage3.bin
 * 	enc_2_2_050_sampleimage3.bin
 * 	enc_2_2_075_sampleimage3.bin
 * 	enc_2_2_100_sampleimage3.bin
 * 	enc_2_3_025_sampleimage3.bin
 * 	enc_2_3_050_sampleimage3.bin
 * 	enc_2_3_075_sampleimage3.bin
 * 	enc_2_3_100_sampleimage3.bin
 *
 * decompress/
 * 	dec_0_0_025_sampleimage1.bmp
 * 	dec_0_0_050_sampleimage1.bmp
 * 	dec_0_0_075_sampleimage1.bmp
 * 	dec_0_0_100_sampleimage1.bmp
 * 	dec_0_1_025_sampleimage1.bmp
 * 	dec_0_1_050_sampleimage1.bmp
 * 	dec_0_1_075_sampleimage1.bmp
 * 	dec_0_1_100_sampleimage1.bmp
 * 	dec_0_2_025_sampleimage1.bmp
 * 	dec_0_2_050_sampleimage1.bmp
 * 	dec_0_2_075_sampleimage1.bmp
 * 	dec_0_2_100_sampleimage1.bmp
 * 	dec_0_3_025_sampleimage1.bmp
 * 	dec_0_3_050_sampleimage1.bmp
 * 	dec_0_3_075_sampleimage1.bmp
 * 	dec_0_3_100_sampleimage1.bmp
 * 	dec_1_0_025_sampleimage2.bmp
 * 	dec_1_0_050_sampleimage2.bmp
 * 	dec_1_0_075_sampleimage2.bmp
 * 	dec_1_0_100_sampleimage2.bmp
 * 	dec_1_1_025_sampleimage2.bmp
 * 	dec_1_1_050_sampleimage2.bmp
 * 	dec_1_1_075_sampleimage2.bmp
 * 	dec_1_1_100_sampleimage2.bmp
 * 	dec_1_2_025_sampleimage2.bmp
 * 	dec_1_2_050_sampleimage2.bmp
 * 	dec_1_2_075_sampleimage2.bmp
 * 	dec_1_2_100_sampleimage2.bmp
 * 	dec_1_3_025_sampleimage2.bmp
 * 	dec_1_3_050_sampleimage2.bmp
 * 	dec_1_3_075_sampleimage2.bmp
 * 	dec_1_3_100_sampleimage2.bmp
 * 	dec_2_0_025_sampleimage3.bmp
 * 	dec_2_0_050_sampleimage3.bmp
 * 	dec_2_0_075_sampleimage3.bmp
 * 	dec_2_0_100_sampleimage3.bmp
 * 	dec_2_1_025_sampleimage3.bmp
 * 	dec_2_1_050_sampleimage3.bmp
 * 	dec_2_1_075_sampleimage3.bmp
 * 	dec_2_1_100_sampleimage3.bmp
 * 	dec_2_2_025_sampleimage3.bmp
 * 	dec_2_2_050_sampleimage3.bmp
 * 	dec_2_2_075_sampleimage3.bmp
 * 	dec_2_2_100_sampleimage3.bmp
 * 	dec_2_3_025_sampleimage3.bmp
 * 	dec_2_3_050_sampleimage3.bmp
 * 	dec_2_3_075_sampleimage3.bmp
 * 	dec_2_3_100_sampleimage3.bmp
 */

typedef struct
{
	BYTE* buffer;
	size_t size;
} EGFX_SAMPLE_FILE;

static int g_Width = 0;
static int g_Height = 0;
static int g_DstStep = 0;
static BYTE* g_DstData = NULL;

static void sample_file_free(EGFX_SAMPLE_FILE* file)
{
	if (!file)
		return;

	free(file->buffer);
	file->buffer = NULL;
	file->size = 0;
}

static void test_fill_image_alpha_channel(BYTE* data, int width, int height, BYTE value)
{
	UINT32* pixel = NULL;

	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			pixel = (UINT32*)&data[((1ULL * i * width) + j) * 4ULL];
			*pixel = ((*pixel & 0x00FFFFFF) | (value << 24));
		}
	}
}

static void* test_image_memset32(UINT32* ptr, UINT32 fill, size_t length)
{
	while (length--)
	{
		*ptr++ = fill;
	}

	return (void*)ptr;
}

static int test_image_fill(BYTE* pDstData, int nDstStep, int nXDst, int nYDst, int nWidth,
                           int nHeight, UINT32 color)
{
	UINT32* pDstPixel = NULL;

	if (nDstStep < 0)
		nDstStep = 4 * nWidth;

	for (int y = 0; y < nHeight; y++)
	{
		pDstPixel = (UINT32*)&pDstData[((nYDst + y) * nDstStep) + (nXDst * 4)];
		test_image_memset32(pDstPixel, color, nWidth);
	}

	return 1;
}

static int test_image_fill_quarter(BYTE* pDstData, int nDstStep, int nWidth, int nHeight,
                                   UINT32 color, int quarter)
{
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;

	switch (quarter)
	{
		case 0:
			x = 0;
			y = 0;
			width = nWidth / 2;
			height = nHeight / 2;
			break;

		case 1:
			x = nWidth / 2;
			y = nHeight / 2;
			width = nWidth / 2;
			height = nHeight / 2;
			break;

		case 2:
			x = 0;
			y = nHeight / 2;
			width = nWidth / 2;
			height = nHeight / 2;
			break;

		case 3:
			x = nWidth / 2;
			y = 0;
			width = nWidth / 2;
			height = nHeight / 2;
			break;
	}

	test_image_fill(pDstData, nDstStep, x, y, width, height, 0xFF000000);
	return 1;
}

static int test_image_fill_unused_quarters(BYTE* pDstData, int nDstStep, int nWidth, int nHeight,
                                           UINT32 color, int quarter)
{
	return 1;

	if (quarter == 0)
	{
		test_image_fill_quarter(pDstData, nDstStep, nWidth, nHeight, color, 1);
		test_image_fill_quarter(pDstData, nDstStep, nWidth, nHeight, color, 2);
		test_image_fill_quarter(pDstData, nDstStep, nWidth, nHeight, color, 3);
	}
	else if (quarter == 1)
	{
		test_image_fill_quarter(pDstData, nDstStep, nWidth, nHeight, color, 0);
		test_image_fill_quarter(pDstData, nDstStep, nWidth, nHeight, color, 2);
		test_image_fill_quarter(pDstData, nDstStep, nWidth, nHeight, color, 3);
	}
	else if (quarter == 2)
	{
		test_image_fill_quarter(pDstData, nDstStep, nWidth, nHeight, color, 0);
		test_image_fill_quarter(pDstData, nDstStep, nWidth, nHeight, color, 1);
		test_image_fill_quarter(pDstData, nDstStep, nWidth, nHeight, color, 3);
	}
	else if (quarter == 3)
	{
		test_image_fill_quarter(pDstData, nDstStep, nWidth, nHeight, color, 0);
		test_image_fill_quarter(pDstData, nDstStep, nWidth, nHeight, color, 1);
		test_image_fill_quarter(pDstData, nDstStep, nWidth, nHeight, color, 2);
	}

	return 1;
}

static BYTE* test_progressive_load_file(const char* path, const char* file, size_t* size)
{
	char* filename = GetCombinedPath(path, file);

	if (!filename)
		return NULL;

	FILE* fp = winpr_fopen(filename, "r");
	free(filename);

	if (!fp)
		return NULL;

	(void)_fseeki64(fp, 0, SEEK_END);
	const INT64 pos = _ftelli64(fp);
	WINPR_ASSERT(pos >= 0);
	WINPR_ASSERT(pos <= SIZE_MAX);
	*size = (size_t)pos;
	(void)_fseeki64(fp, 0, SEEK_SET);
	BYTE* buffer = (BYTE*)malloc(*size);

	if (!buffer)
	{
		(void)fclose(fp);
		return NULL;
	}

	if (fread(buffer, *size, 1, fp) != 1)
	{
		free(buffer);
		(void)fclose(fp);
		return NULL;
	}

	(void)fclose(fp);
	return buffer;
}

static int test_progressive_load_files(char* ms_sample_path, EGFX_SAMPLE_FILE files[3][4][4])
{
	int imageNo = 0;
	int quarterNo = 0;
	int passNo = 0;
	/* image 1 */
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_0_0_025_sampleimage1.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_0_0_050_sampleimage1.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_0_0_075_sampleimage1.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_0_0_100_sampleimage1.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_0_1_025_sampleimage1.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_0_1_050_sampleimage1.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_0_1_075_sampleimage1.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_0_1_100_sampleimage1.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_0_2_025_sampleimage1.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_0_2_050_sampleimage1.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_0_2_075_sampleimage1.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_0_2_100_sampleimage1.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_0_3_025_sampleimage1.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_0_3_050_sampleimage1.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_0_3_075_sampleimage1.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_0_3_100_sampleimage1.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	imageNo++;
	/* image 2 */
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_1_0_025_sampleimage2.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_1_0_050_sampleimage2.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_1_0_075_sampleimage2.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_1_0_100_sampleimage2.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_1_1_025_sampleimage2.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_1_1_050_sampleimage2.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_1_1_075_sampleimage2.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_1_1_100_sampleimage2.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_1_2_025_sampleimage2.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_1_2_050_sampleimage2.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_1_2_075_sampleimage2.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_1_2_100_sampleimage2.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_1_3_025_sampleimage2.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_1_3_050_sampleimage2.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_1_3_075_sampleimage2.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_1_3_100_sampleimage2.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	imageNo++;
	/* image 3 */
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_2_0_025_sampleimage3.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_2_0_050_sampleimage3.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_2_0_075_sampleimage3.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_2_0_100_sampleimage3.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_2_1_025_sampleimage3.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_2_1_050_sampleimage3.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_2_1_075_sampleimage3.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_2_1_100_sampleimage3.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_2_2_025_sampleimage3.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_2_2_050_sampleimage3.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_2_2_075_sampleimage3.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_2_2_100_sampleimage3.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_2_3_025_sampleimage3.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_2_3_050_sampleimage3.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_2_3_075_sampleimage3.bin",
	                               &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_file(ms_sample_path, "compress/enc_2_3_100_sampleimage3.bin",
	                               &(files[imageNo][quarterNo][passNo].size));

	/* check if all test data has been loaded */

	for (imageNo = 0; imageNo < 3; imageNo++)
	{
		for (quarterNo = 0; quarterNo < 4; quarterNo++)
		{
			for (passNo = 0; passNo < 4; passNo++)
			{
				if (!files[imageNo][quarterNo][passNo].buffer)
					return -1;
			}
		}
	}

	return 1;
}

static BYTE* test_progressive_load_bitmap(char* path, char* file, size_t* size, int quarter)
{
	int status = 0;
	BYTE* buffer = NULL;
	wImage* image = NULL;
	char* filename = NULL;
	filename = GetCombinedPath(path, file);

	if (!filename)
		return NULL;

	image = winpr_image_new();

	if (!image)
		return NULL;

	status = winpr_image_read(image, filename);

	if (status < 0)
		return NULL;

	buffer = image->data;
	*size = 1ULL * image->height * image->scanline;
	test_fill_image_alpha_channel(image->data, image->width, image->height, 0xFF);
	test_image_fill_unused_quarters(image->data, image->scanline, image->width, image->height,
	                                quarter, 0xFF000000);
	winpr_image_free(image, FALSE);
	free(filename);
	return buffer;
}

static int test_progressive_load_bitmaps(char* ms_sample_path, EGFX_SAMPLE_FILE bitmaps[3][4][4])
{
	int imageNo = 0;
	int quarterNo = 0;
	int passNo = 0;
	/* image 1 */
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_0_0_025_sampleimage1.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_0_0_050_sampleimage1.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_0_0_075_sampleimage1.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_0_0_100_sampleimage1.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_0_1_025_sampleimage1.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_0_1_050_sampleimage1.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_0_1_075_sampleimage1.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_0_1_100_sampleimage1.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_0_2_025_sampleimage1.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_0_2_050_sampleimage1.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_0_2_075_sampleimage1.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_0_2_100_sampleimage1.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_0_3_025_sampleimage1.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_0_3_050_sampleimage1.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_0_3_075_sampleimage1.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_0_3_100_sampleimage1.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	imageNo++;
	/* image 2 */
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_1_0_025_sampleimage2.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_1_0_050_sampleimage2.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_1_0_075_sampleimage2.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_1_0_100_sampleimage2.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_1_1_025_sampleimage2.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_1_1_050_sampleimage2.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_1_1_075_sampleimage2.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_1_1_100_sampleimage2.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_1_2_025_sampleimage2.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_1_2_050_sampleimage2.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_1_2_075_sampleimage2.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_1_2_100_sampleimage2.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_1_3_025_sampleimage2.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_1_3_050_sampleimage2.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_1_3_075_sampleimage2.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_1_3_100_sampleimage2.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	imageNo++;
	/* image 3 */
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_2_0_025_sampleimage3.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_2_0_050_sampleimage3.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_2_0_075_sampleimage3.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_2_0_100_sampleimage3.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_2_1_025_sampleimage3.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_2_1_050_sampleimage3.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_2_1_075_sampleimage3.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_2_1_100_sampleimage3.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_2_2_025_sampleimage3.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_2_2_050_sampleimage3.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_2_2_075_sampleimage3.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_2_2_100_sampleimage3.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_2_3_025_sampleimage3.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_2_3_050_sampleimage3.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_2_3_075_sampleimage3.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer =
	    test_progressive_load_bitmap(ms_sample_path, "decompress/dec_2_3_100_sampleimage3.bmp",
	                                 &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);

	/* check if all test data has been loaded */

	for (imageNo = 0; imageNo < 3; imageNo++)
	{
		for (quarterNo = 0; quarterNo < 4; quarterNo++)
		{
			for (passNo = 0; passNo < 4; passNo++)
			{
				if (!bitmaps[imageNo][quarterNo][passNo].buffer)
					return -1;
			}
		}
	}

	return 1;
}

static size_t test_memcmp_count(const BYTE* mem1, const BYTE* mem2, size_t size, int margin)
{
	size_t count = 0;

	for (size_t index = 0; index < size; index++)
	{
		if (*mem1 != *mem2)
		{
			const int error = (*mem1 > *mem2) ? *mem1 - *mem2 : *mem2 - *mem1;

			if (error > margin)
				count++;
		}

		mem1++;
		mem2++;
	}

	return count;
}

static int test_progressive_decode(PROGRESSIVE_CONTEXT* progressive, EGFX_SAMPLE_FILE files[4],
                                   EGFX_SAMPLE_FILE bitmaps[4], int quarter, int count)
{
	int nXSrc = 0;
	int nYSrc = 0;

	RECTANGLE_16 clippingRect = { 0 };
	clippingRect.right = g_Width;
	clippingRect.bottom = g_Height;

	for (int pass = 0; pass < count; pass++)
	{
		const int status =
		    progressive_decompress(progressive, files[pass].buffer, files[pass].size, g_DstData,
		                           PIXEL_FORMAT_XRGB32, g_DstStep, 0, 0, NULL, 0, 0);
		printf("ProgressiveDecompress: status: %d pass: %d\n", status, pass + 1);
		PROGRESSIVE_BLOCK_REGION* region = &(progressive->region);

		switch (quarter)
		{
			case 0:
				clippingRect.left = 0;
				clippingRect.top = 0;
				clippingRect.right = g_Width / 2;
				clippingRect.bottom = g_Height / 2;
				break;

			case 1:
				clippingRect.left = g_Width / 2;
				clippingRect.top = g_Height / 2;
				clippingRect.right = g_Width;
				clippingRect.bottom = g_Height;
				break;

			case 2:
				clippingRect.left = 0;
				clippingRect.top = g_Height / 2;
				clippingRect.right = g_Width / 2;
				clippingRect.bottom = g_Height;
				break;

			case 3:
				clippingRect.left = g_Width / 2;
				clippingRect.top = 0;
				clippingRect.right = g_Width;
				clippingRect.bottom = g_Height / 2;
				break;
		}

		for (UINT16 index = 0; index < region->numTiles; index++)
		{
			RFX_PROGRESSIVE_TILE* tile = region->tiles[index];

			const RECTANGLE_16 tileRect = { tile->x, tile->y, tile->x + tile->width,
				                            tile->y + tile->height };
			RECTANGLE_16 updateRect = { 0 };
			rectangles_intersection(&tileRect, &clippingRect, &updateRect);
			const UINT16 nXDst = updateRect.left;
			const UINT16 nYDst = updateRect.top;
			const UINT16 nWidth = updateRect.right - updateRect.left;
			const UINT16 nHeight = updateRect.bottom - updateRect.top;

			if ((nWidth <= 0) || (nHeight <= 0))
				continue;

			nXSrc = nXDst - tile->x;
			nYSrc = nYDst - tile->y;
			freerdp_image_copy(g_DstData, PIXEL_FORMAT_XRGB32, g_DstStep, nXDst, nYDst, nWidth,
			                   nHeight, tile->data, PIXEL_FORMAT_XRGB32, 64 * 4, nXSrc, nYSrc, NULL,
			                   FREERDP_FLIP_NONE);
		}

		const size_t size = bitmaps[pass].size;
		const size_t cnt = test_memcmp_count(g_DstData, bitmaps[pass].buffer, size, 1);

		if (cnt)
		{
			const float rate = ((float)cnt) / ((float)size) * 100.0f;
			printf("Progressive RemoteFX decompression failure\n");
			printf("Actual, Expected (%" PRIuz "/%" PRIuz " = %.3f%%):\n", cnt, size, rate);
		}

		// WLog_Image(progressive->log, WLOG_TRACE, g_DstData, g_Width, g_Height, 32);
	}

	return 1;
}

static int test_progressive_ms_sample(char* ms_sample_path)
{
	int count = 0;
	int status = 0;
	EGFX_SAMPLE_FILE files[3][4][4] = { 0 };
	EGFX_SAMPLE_FILE bitmaps[3][4][4] = { 0 };
	PROGRESSIVE_CONTEXT* progressive = NULL;
	g_Width = 1920;
	g_Height = 1080;
	g_DstStep = g_Width * 4;
	status = test_progressive_load_files(ms_sample_path, files);

	if (status < 0)
	{
		for (int i = 0; i < 3; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				for (int k = 0; k < 4; k++)
					sample_file_free(&files[i][j][k]);
			}
		}

		return -1;
	}

	status = test_progressive_load_bitmaps(ms_sample_path, bitmaps);

	if (status < 0)
	{
		for (int i = 0; i < 3; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				for (int k = 0; k < 4; k++)
					sample_file_free(&files[i][j][k]);
			}
		}

		return -1;
	}

	count = 4;
	progressive = progressive_context_new(FALSE);
	g_DstData = winpr_aligned_malloc(1LL * g_DstStep * g_Height, 16);
	progressive_create_surface_context(progressive, 0, g_Width, g_Height);

	/* image 1 */

	if (1)
	{
		printf("\nSample Image 1\n");
		test_image_fill(g_DstData, g_DstStep, 0, 0, g_Width, g_Height, 0xFF000000);
		test_progressive_decode(progressive, files[0][0], bitmaps[0][0], 0, count);
		test_progressive_decode(progressive, files[0][1], bitmaps[0][1], 1, count);
		test_progressive_decode(progressive, files[0][2], bitmaps[0][2], 2, count);
		test_progressive_decode(progressive, files[0][3], bitmaps[0][3], 3, count);
	}

	/* image 2 */

	if (0)
	{
		printf("\nSample Image 2\n"); /* sample data is in incorrect order */
		test_image_fill(g_DstData, g_DstStep, 0, 0, g_Width, g_Height, 0xFF000000);
		test_progressive_decode(progressive, files[1][0], bitmaps[1][0], 0, count);
		test_progressive_decode(progressive, files[1][1], bitmaps[1][1], 1, count);
		test_progressive_decode(progressive, files[1][2], bitmaps[1][2], 2, count);
		test_progressive_decode(progressive, files[1][3], bitmaps[1][3], 3, count);
	}

	/* image 3 */

	if (0)
	{
		printf("\nSample Image 3\n"); /* sample data is in incorrect order */
		test_image_fill(g_DstData, g_DstStep, 0, 0, g_Width, g_Height, 0xFF000000);
		test_progressive_decode(progressive, files[2][0], bitmaps[2][0], 0, count);
		test_progressive_decode(progressive, files[2][1], bitmaps[2][1], 1, count);
		test_progressive_decode(progressive, files[2][2], bitmaps[2][2], 2, count);
		test_progressive_decode(progressive, files[2][3], bitmaps[2][3], 3, count);
	}

	progressive_context_free(progressive);

	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			for (int k = 0; k < 4; k++)
			{
				sample_file_free(&bitmaps[i][j][k]);
				sample_file_free(&files[i][j][k]);
			}
		}
	}

	winpr_aligned_free(g_DstData);
	return 0;
}

static BOOL diff(BYTE a, BYTE b)
{
	BYTE big = MAX(a, b);
	BYTE little = MIN(a, b);
	if (big - little <= 0x25)
		return TRUE;
	return FALSE;
}

static BOOL colordiff(UINT32 format, UINT32 a, UINT32 b)
{
	BYTE ar = 0;
	BYTE ag = 0;
	BYTE ab = 0;
	BYTE aa = 0;
	BYTE br = 0;
	BYTE bg = 0;
	BYTE bb = 0;
	BYTE ba = 0;
	FreeRDPSplitColor(a, format, &ar, &ag, &ab, &aa, NULL);
	FreeRDPSplitColor(b, format, &br, &bg, &bb, &ba, NULL);
	if (!diff(aa, ba) || !diff(ar, br) || !diff(ag, bg) || !diff(ab, bb))
		return FALSE;
	return TRUE;
}

static BOOL test_encode_decode(const char* path)
{
	BOOL res = FALSE;
	int rc = 0;
	BYTE* resultData = NULL;
	BYTE* dstData = NULL;
	UINT32 dstSize = 0;
	UINT32 ColorFormat = PIXEL_FORMAT_BGRX32;
	REGION16 invalidRegion = { 0 };
	wImage* image = winpr_image_new();
	wImage* dstImage = winpr_image_new();
	char* name = GetCombinedPath(path, "progressive.bmp");
	PROGRESSIVE_CONTEXT* progressiveEnc = progressive_context_new(TRUE);
	PROGRESSIVE_CONTEXT* progressiveDec = progressive_context_new(FALSE);

	region16_init(&invalidRegion);
	if (!image || !dstImage || !name || !progressiveEnc || !progressiveDec)
		goto fail;

	rc = winpr_image_read(image, name);
	if (rc <= 0)
		goto fail;

	resultData = calloc(image->scanline, image->height);
	if (!resultData)
		goto fail;

	// Progressive encode
	rc = progressive_compress(progressiveEnc, image->data, image->scanline * image->height,
	                          ColorFormat, image->width, image->height, image->scanline, NULL,
	                          &dstData, &dstSize);
	if (rc < 0)
		goto fail;

	// Progressive decode
	rc = progressive_create_surface_context(progressiveDec, 0, image->width, image->height);
	if (rc <= 0)
		goto fail;

	rc = progressive_decompress(progressiveDec, dstData, dstSize, resultData, ColorFormat,
	                            image->scanline, 0, 0, &invalidRegion, 0, 0);
	if (rc < 0)
		goto fail;

	// Compare result
	if (0) // Dump result image for manual inspection
	{
		*dstImage = *image;
		dstImage->data = resultData;
		winpr_image_write(dstImage, "/tmp/test.bmp");
	}
	for (size_t y = 0; y < image->height; y++)
	{
		const BYTE* orig = &image->data[y * image->scanline];
		const BYTE* dec = &resultData[y * image->scanline];
		for (size_t x = 0; x < image->width; x++)
		{
			const BYTE* po = &orig[x * 4];
			const BYTE* pd = &dec[x * 4];

			const DWORD a = FreeRDPReadColor(po, ColorFormat);
			const DWORD b = FreeRDPReadColor(pd, ColorFormat);
			if (!colordiff(ColorFormat, a, b))
			{
				printf("xxxxxxx [%u:%u] [%s] %08X != %08X\n", x, y,
				       FreeRDPGetColorFormatName(ColorFormat), a, b);
				goto fail;
			}
		}
	}
	res = TRUE;
fail:
	region16_uninit(&invalidRegion);
	progressive_context_free(progressiveEnc);
	progressive_context_free(progressiveDec);
	winpr_image_free(image, TRUE);
	winpr_image_free(dstImage, FALSE);
	free(resultData);
	free(name);
	return res;
}

static BOOL read_cmd(FILE* fp, RDPGFX_SURFACE_COMMAND* cmd, UINT32* frameId)
{
	WINPR_ASSERT(fp);
	WINPR_ASSERT(cmd);
	WINPR_ASSERT(frameId);

	// NOLINTBEGIN(cert-err34-c)
	if (1 != fscanf(fp, "frameid: %" PRIu32 "\n", frameId))
		return FALSE;
	if (1 != fscanf(fp, "surfaceId: %" PRIu32 "\n", &cmd->surfaceId))
		return FALSE;
	if (1 != fscanf(fp, "codecId: %" PRIu32 "\n", &cmd->codecId))
		return FALSE;
	if (1 != fscanf(fp, "contextId: %" PRIu32 "\n", &cmd->contextId))
		return FALSE;
	if (1 != fscanf(fp, "format: %" PRIu32 "\n", &cmd->format))
		return FALSE;
	if (1 != fscanf(fp, "left: %" PRIu32 "\n", &cmd->left))
		return FALSE;
	if (1 != fscanf(fp, "top: %" PRIu32 "\n", &cmd->top))
		return FALSE;
	if (1 != fscanf(fp, "right: %" PRIu32 "\n", &cmd->right))
		return FALSE;
	if (1 != fscanf(fp, "bottom: %" PRIu32 "\n", &cmd->bottom))
		return FALSE;
	if (1 != fscanf(fp, "width: %" PRIu32 "\n", &cmd->width))
		return FALSE;
	if (1 != fscanf(fp, "height: %" PRIu32 "\n", &cmd->height))
		return FALSE;
	if (1 != fscanf(fp, "length: %" PRIu32 "\n", &cmd->length))
		return FALSE;
	// NOLINTEND(cert-err34-c)

	char* data = NULL;

	size_t dlen = SIZE_MAX;
	SSIZE_T slen = GetLine(&data, &slen, fp);
	if (slen < 0)
		return FALSE;

	if (slen >= 7)
	{
		const char* b64 = &data[6];
		slen -= 7;
		crypto_base64_decode(b64, slen, &cmd->data, &dlen);
	}
	free(data);

	return cmd->length == dlen;
}

static void free_cmd(RDPGFX_SURFACE_COMMAND* cmd)
{
	free(cmd->data);
}

static WINPR_NORETURN(void usage(const char* name))
{
	FILE* fp = stdout;
	(void)fprintf(fp, "%s <directory> <width> <height>\n", name);
	exit(-1);
}

static void print_codec_stats(const char* name, UINT64 timeNS)
{
	const double dectimems = timeNS / 1000000.0;
	(void)fprintf(stderr, "[%s] took %lf ms to decode\n", name, dectimems);
}

static int test_dump(int argc, char* argv[])
{
	int success = -1;
	UINT32 count = 0;

	UINT64 CAPROGRESSIVE_dectime = 0;
	UINT64 UNCOMPRESSED_dectime = 0;
	UINT64 CAVIDEO_dectime = 0;
	UINT64 CLEARCODEC_dectime = 0;
	UINT64 PLANAR_dectime = 0;
	UINT64 AVC420_dectime = 0;
	UINT64 ALPHA_dectime = 0;
	UINT64 AVC444_dectime = 0;
	UINT64 AVC444v2_dectime = 0;
	UINT64 copytime = 0;

	if (argc < 4)
		usage(argv[0]);

	const char* path = argv[1];
	errno = 0;
	const unsigned long width = strtoul(argv[2], NULL, 0);
	if ((errno != 0) || (width <= 0))
		usage(argv[0]);
	const unsigned long height = strtoul(argv[3], NULL, 0);
	if ((errno != 0) || (height <= 0))
		usage(argv[0]);

	rdpCodecs* codecs = freerdp_client_codecs_new(0);
	if (!codecs)
		return -2;

	UINT32 DstFormat = PIXEL_FORMAT_BGRA32;
	const UINT32 stride = (width + 16) * FreeRDPGetBytesPerPixel(DstFormat);

	BYTE* dst = calloc(stride, height);
	BYTE* output = calloc(stride, height);
	if (!dst || !output)
		goto fail;

	if (!freerdp_client_codecs_prepare(codecs, FREERDP_CODEC_ALL, width, height))
		goto fail;

	success = 0;
	while (success >= 0)
	{
		char* fname = NULL;
		size_t flen = 0;
		winpr_asprintf(&fname, &flen, "%s/%08" PRIx32 ".raw", path, count++);
		FILE* fp = fopen(fname, "r");
		free(fname);

		if (!fp)
			break;

		UINT32 frameId = 0;
		RDPGFX_SURFACE_COMMAND cmd = { 0 };

		if (read_cmd(fp, &cmd, &frameId))
		{
			REGION16 invalid = { 0 };
			region16_init(&invalid);

			const char* cname = rdpgfx_get_codec_id_string(cmd.codecId);
			switch (cmd.codecId)
			{
				case RDPGFX_CODECID_CAPROGRESSIVE:
				{
					const UINT64 start = winpr_GetTickCount64NS();
					success = progressive_create_surface_context(codecs->progressive, cmd.surfaceId,
					                                             width, height);
					if (success >= 0)
						success = progressive_decompress(codecs->progressive, cmd.data, cmd.length,
						                                 dst, DstFormat, 0, cmd.left, cmd.top,
						                                 &invalid, cmd.surfaceId, frameId);
					const UINT64 end = winpr_GetTickCount64NS();
					const UINT64 diff = end - start;
					const double ddiff = diff / 1000000.0;
					(void)fprintf(stderr, "frame [%s] %" PRIu32 " took %lf ms\n", cname, frameId,
					              ddiff);
					CAPROGRESSIVE_dectime += diff;
				}
				break;

				case RDPGFX_CODECID_UNCOMPRESSED:
				{
					const UINT64 start = winpr_GetTickCount64NS();
					if (!freerdp_image_copy_no_overlap(dst, DstFormat, stride, cmd.left, cmd.top,
					                                   cmd.width, cmd.height, cmd.data, cmd.format,
					                                   0, 0, 0, NULL, FREERDP_FLIP_NONE))
						success = -1;

					RECTANGLE_16 invalidRect = { .left = (UINT16)MIN(UINT16_MAX, cmd.left),
						                         .top = (UINT16)MIN(UINT16_MAX, cmd.top),
						                         .right = (UINT16)MIN(UINT16_MAX, cmd.right),
						                         .bottom = (UINT16)MIN(UINT16_MAX, cmd.bottom) };
					region16_union_rect(&invalid, &invalid, &invalidRect);
					const UINT64 end = winpr_GetTickCount64NS();
					const UINT64 diff = end - start;
					const double ddiff = diff / 1000000.0;
					(void)fprintf(stderr, "frame [%s] %" PRIu32 " took %lf ms\n", cname, frameId,
					              ddiff);
					UNCOMPRESSED_dectime += diff;
				}
				break;
				case RDPGFX_CODECID_CAVIDEO:
				{
					const UINT64 start = winpr_GetTickCount64NS();
					if (!rfx_process_message(codecs->rfx, cmd.data, cmd.length, cmd.left, cmd.top,
					                         dst, DstFormat, stride, height, &invalid))
						success = -1;

					const UINT64 end = winpr_GetTickCount64NS();
					const UINT64 diff = end - start;
					const double ddiff = diff / 1000000.0;
					(void)fprintf(stderr, "frame [%s] %" PRIu32 " took %lf ms\n", cname, frameId,
					              ddiff);
					CAVIDEO_dectime += diff;
				}
				break;
				case RDPGFX_CODECID_CLEARCODEC:
				{
					const UINT64 start = winpr_GetTickCount64NS();
					success = clear_decompress(codecs->clear, cmd.data, cmd.length, cmd.width,
					                           cmd.height, dst, DstFormat, stride, cmd.left,
					                           cmd.top, width, height, NULL);

					const RECTANGLE_16 invalidRect = { .left = (UINT16)MIN(UINT16_MAX, cmd.left),
						                               .top = (UINT16)MIN(UINT16_MAX, cmd.top),
						                               .right = (UINT16)MIN(UINT16_MAX, cmd.right),
						                               .bottom =
						                                   (UINT16)MIN(UINT16_MAX, cmd.bottom) };
					region16_union_rect(&invalid, &invalid, &invalidRect);
					const UINT64 end = winpr_GetTickCount64NS();
					const UINT64 diff = end - start;
					const double ddiff = diff / 1000000.0;
					(void)fprintf(stderr, "frame [%s] %" PRIu32 " took %lf ms\n", cname, frameId,
					              ddiff);
					CLEARCODEC_dectime += diff;
				}
				break;
				case RDPGFX_CODECID_PLANAR:
				{
					const UINT64 start = winpr_GetTickCount64NS();

					if (!planar_decompress(codecs->planar, cmd.data, cmd.length, cmd.width,
					                       cmd.height, dst, DstFormat, stride, cmd.left, cmd.top,
					                       cmd.width, cmd.height, FALSE))
						success = -1;

					const RECTANGLE_16 invalidRect = { .left = (UINT16)MIN(UINT16_MAX, cmd.left),
						                               .top = (UINT16)MIN(UINT16_MAX, cmd.top),
						                               .right = (UINT16)MIN(UINT16_MAX, cmd.right),
						                               .bottom =
						                                   (UINT16)MIN(UINT16_MAX, cmd.bottom) };
					region16_union_rect(&invalid, &invalid, &invalidRect);

					const UINT64 end = winpr_GetTickCount64NS();
					const UINT64 diff = end - start;
					const double ddiff = diff / 1000000.0;
					(void)fprintf(stderr, "frame [%s] %" PRIu32 " took %lf ms\n", cname, frameId,
					              ddiff);
					PLANAR_dectime += diff;
				}
				break;
				case RDPGFX_CODECID_AVC420:
				{
					const UINT64 start = winpr_GetTickCount64NS();

					const UINT64 end = winpr_GetTickCount64NS();
					const UINT64 diff = end - start;
					const double ddiff = diff / 1000000.0;
					(void)fprintf(stderr, "frame [%s] %" PRIu32 " took %lf ms\n", cname, frameId,
					              ddiff);
					AVC420_dectime += diff;
					success = -1;
				}
				break;
				case RDPGFX_CODECID_ALPHA:
				{
					const UINT64 start = winpr_GetTickCount64NS();

					const UINT64 end = winpr_GetTickCount64NS();
					const UINT64 diff = end - start;
					const double ddiff = diff / 1000000.0;
					(void)fprintf(stderr, "frame [%s] %" PRIu32 " took %lf ms\n", cname, frameId,
					              ddiff);
					ALPHA_dectime += diff;
					success = -1;
				}
				break;
				case RDPGFX_CODECID_AVC444:
				{
					const UINT64 start = winpr_GetTickCount64NS();

					const UINT64 end = winpr_GetTickCount64NS();
					const UINT64 diff = end - start;
					const double ddiff = diff / 1000000.0;
					(void)fprintf(stderr, "frame [%s] %" PRIu32 " took %lf ms\n", cname, frameId,
					              ddiff);
					AVC444_dectime += diff;
					success = -1;
				}
				break;
				case RDPGFX_CODECID_AVC444v2:
				{
					const UINT64 start = winpr_GetTickCount64NS();

					const UINT64 end = winpr_GetTickCount64NS();
					const UINT64 diff = end - start;
					const double ddiff = diff / 1000000.0;
					(void)fprintf(stderr, "frame [%s] %" PRIu32 " took %lf ms\n", cname, frameId,
					              ddiff);
					AVC444v2_dectime += diff;
					success = -1;
				}
				break;
				default:
					(void)fprintf(stderr, "unexpected codec %s [0x%08" PRIx32 "]",
					              rdpgfx_get_codec_id_string(cmd.codecId), cmd.codecId);
					success = -1;
					break;
			}

			if (success >= 0)
			{
				UINT32 nbRects = 0;
				const UINT64 start = winpr_GetTickCount64NS();

				const RECTANGLE_16* rects = region16_rects(&invalid, &nbRects);
				for (size_t x = 0; x < nbRects; x++)
				{
					RECTANGLE_16* rect = &rects[x];
					const UINT32 w = rect->right - rect->left;
					const UINT32 h = rect->bottom - rect->top;
					if (!freerdp_image_copy_no_overlap(output, DstFormat, stride, rect->left,
					                                   rect->top, w, h, dst, DstFormat, stride,
					                                   rect->left, rect->top, NULL, 0))
						success = -42;
				}
				const UINT64 end = winpr_GetTickCount64NS();
				const UINT64 diff = end - start;
				const double ddiff = diff / 1000000.0;
				(void)fprintf(stderr, "frame %" PRIu32 " copy took %lf ms\n", frameId, ddiff);
				copytime += diff;
			}
			region16_clear(&invalid);
		}
		free_cmd(&cmd);
		(void)fclose(fp);
	}

fail:
	freerdp_client_codecs_free(codecs);
	free(output);
	free(dst);

	print_codec_stats(rdpgfx_get_codec_id_string(RDPGFX_CODECID_UNCOMPRESSED),
	                  UNCOMPRESSED_dectime);
	print_codec_stats(rdpgfx_get_codec_id_string(RDPGFX_CODECID_CAPROGRESSIVE),
	                  CAPROGRESSIVE_dectime);
	print_codec_stats(rdpgfx_get_codec_id_string(RDPGFX_CODECID_CAVIDEO), CAVIDEO_dectime);
	print_codec_stats(rdpgfx_get_codec_id_string(RDPGFX_CODECID_CLEARCODEC), CLEARCODEC_dectime);
	print_codec_stats(rdpgfx_get_codec_id_string(RDPGFX_CODECID_PLANAR), PLANAR_dectime);
	print_codec_stats(rdpgfx_get_codec_id_string(RDPGFX_CODECID_AVC420), AVC420_dectime);
	print_codec_stats(rdpgfx_get_codec_id_string(RDPGFX_CODECID_AVC444), AVC444_dectime);
	print_codec_stats(rdpgfx_get_codec_id_string(RDPGFX_CODECID_AVC444v2), AVC444v2_dectime);
	print_codec_stats(rdpgfx_get_codec_id_string(RDPGFX_CODECID_ALPHA), ALPHA_dectime);

	const UINT64 decodetime = UNCOMPRESSED_dectime + CAPROGRESSIVE_dectime + CAVIDEO_dectime +
	                          CLEARCODEC_dectime + PLANAR_dectime + AVC420_dectime +
	                          AVC444_dectime + AVC444v2_dectime + ALPHA_dectime;
	print_codec_stats("surface copy", copytime);
	print_codec_stats("total decode", decodetime);
	print_codec_stats("total", decodetime + copytime);

	return success;
}

int TestFreeRDPCodecProgressive(int argc, char* argv[])
{
	if (argc > 1)
		return test_dump(argc, argv);

	int rc = -1;
	char* ms_sample_path = NULL;
	char name[8192];
	SYSTEMTIME systemTime;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	GetSystemTime(&systemTime);
	(void)sprintf_s(name, sizeof(name),
	                "EGFX_PROGRESSIVE_MS_SAMPLE-%04" PRIu16 "%02" PRIu16 "%02" PRIu16 "%02" PRIu16
	                "%02" PRIu16 "%02" PRIu16 "%04" PRIu16,
	                systemTime.wYear, systemTime.wMonth, systemTime.wDay, systemTime.wHour,
	                systemTime.wMinute, systemTime.wSecond, systemTime.wMilliseconds);
	ms_sample_path = _strdup(CMAKE_CURRENT_SOURCE_DIR);

	if (!ms_sample_path)
	{
		printf("Memory allocation failed\n");
		goto fail;
	}

	if (winpr_PathFileExists(ms_sample_path))
	{
		/*
		if (test_progressive_ms_sample(ms_sample_path) < 0)
		    goto fail;
		    */
		if (!test_encode_decode(ms_sample_path))
			goto fail;
		rc = 0;
	}

fail:
	free(ms_sample_path);
	return rc;
}
