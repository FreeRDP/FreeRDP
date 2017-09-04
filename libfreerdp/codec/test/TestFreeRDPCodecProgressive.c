#include <winpr/wtypes.h>
#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/image.h>
#include <winpr/print.h>
#include <winpr/wlog.h>

#include <freerdp/codec/region.h>

#include <freerdp/codec/progressive.h>

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

struct _EGFX_SAMPLE_FILE
{
	BYTE* buffer;
	size_t size;
};
typedef struct _EGFX_SAMPLE_FILE EGFX_SAMPLE_FILE;

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
	int i, j;
	UINT32* pixel;

	for (i = 0; i < height; i++)
	{
		for (j = 0; j < width; j++)
		{
			pixel = (UINT32*) &data[((i * width) + j) * 4];
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

	return (void*) ptr;
}

static int test_image_fill(BYTE* pDstData, int nDstStep, int nXDst, int nYDst, int nWidth,
                           int nHeight, UINT32 color)
{
	int y;
	UINT32* pDstPixel;

	if (nDstStep < 0)
		nDstStep = 4 * nWidth;

	for (y = 0; y < nHeight; y++)
	{
		pDstPixel = (UINT32*) &pDstData[((nYDst + y) * nDstStep) + (nXDst * 4)];
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

static BYTE* test_progressive_load_file(char* path, char* file, size_t* size)
{
	FILE* fp;
	BYTE* buffer;
	char* filename;
	filename = GetCombinedPath(path, file);

	if (!filename)
		return NULL;

	fp = fopen(filename, "r");
	free(filename);

	if (!fp)
		return NULL;

	_fseeki64(fp, 0, SEEK_END);
	*size = _ftelli64(fp);
	_fseeki64(fp, 0, SEEK_SET);
	buffer = (BYTE*) malloc(*size);

	if (!buffer)
	{
		fclose(fp);
		return NULL;
	}

	if (fread(buffer, *size, 1, fp) != 1)
	{
		free(buffer);
		fclose(fp);
		return NULL;
	}

	fclose(fp);
	return buffer;
}

static int test_progressive_load_files(char* ms_sample_path, EGFX_SAMPLE_FILE files[3][4][4])
{
	int imageNo = 0;
	int quarterNo = 0;
	int passNo = 0;
	/* image 1 */
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_0_0_025_sampleimage1.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_0_0_050_sampleimage1.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_0_0_075_sampleimage1.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_0_0_100_sampleimage1.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_0_1_025_sampleimage1.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_0_1_050_sampleimage1.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_0_1_075_sampleimage1.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_0_1_100_sampleimage1.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_0_2_025_sampleimage1.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_0_2_050_sampleimage1.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_0_2_075_sampleimage1.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_0_2_100_sampleimage1.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_0_3_025_sampleimage1.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_0_3_050_sampleimage1.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_0_3_075_sampleimage1.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_0_3_100_sampleimage1.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	imageNo++;
	/* image 2 */
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_1_0_025_sampleimage2.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_1_0_050_sampleimage2.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_1_0_075_sampleimage2.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_1_0_100_sampleimage2.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_1_1_025_sampleimage2.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_1_1_050_sampleimage2.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_1_1_075_sampleimage2.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_1_1_100_sampleimage2.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_1_2_025_sampleimage2.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_1_2_050_sampleimage2.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_1_2_075_sampleimage2.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_1_2_100_sampleimage2.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_1_3_025_sampleimage2.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_1_3_050_sampleimage2.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_1_3_075_sampleimage2.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_1_3_100_sampleimage2.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	imageNo++;
	/* image 3 */
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_2_0_025_sampleimage3.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_2_0_050_sampleimage3.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_2_0_075_sampleimage3.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_2_0_100_sampleimage3.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_2_1_025_sampleimage3.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_2_1_050_sampleimage3.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_2_1_075_sampleimage3.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_2_1_100_sampleimage3.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_2_2_025_sampleimage3.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_2_2_050_sampleimage3.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_2_2_075_sampleimage3.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_2_2_100_sampleimage3.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_2_3_025_sampleimage3.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_2_3_050_sampleimage3.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_2_3_075_sampleimage3.bin", &(files[imageNo][quarterNo][passNo].size));
	passNo = (passNo + 1) % 4;
	files[imageNo][quarterNo][passNo].buffer = test_progressive_load_file(ms_sample_path,
	        "compress/enc_2_3_100_sampleimage3.bin", &(files[imageNo][quarterNo][passNo].size));

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
	int status;
	BYTE* buffer;
	wImage* image;
	char* filename;
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
	*size = image->height * image->scanline;
	test_fill_image_alpha_channel(image->data, image->width, image->height, 0xFF);
	test_image_fill_unused_quarters(image->data, image->scanline, image->width, image->height, quarter,
	                                0xFF000000);
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
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_0_0_025_sampleimage1.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_0_0_050_sampleimage1.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_0_0_075_sampleimage1.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_0_0_100_sampleimage1.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_0_1_025_sampleimage1.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_0_1_050_sampleimage1.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_0_1_075_sampleimage1.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_0_1_100_sampleimage1.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_0_2_025_sampleimage1.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_0_2_050_sampleimage1.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_0_2_075_sampleimage1.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_0_2_100_sampleimage1.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_0_3_025_sampleimage1.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_0_3_050_sampleimage1.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_0_3_075_sampleimage1.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_0_3_100_sampleimage1.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	imageNo++;
	/* image 2 */
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_1_0_025_sampleimage2.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_1_0_050_sampleimage2.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_1_0_075_sampleimage2.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_1_0_100_sampleimage2.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_1_1_025_sampleimage2.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_1_1_050_sampleimage2.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_1_1_075_sampleimage2.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_1_1_100_sampleimage2.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_1_2_025_sampleimage2.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_1_2_050_sampleimage2.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_1_2_075_sampleimage2.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_1_2_100_sampleimage2.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_1_3_025_sampleimage2.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_1_3_050_sampleimage2.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_1_3_075_sampleimage2.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_1_3_100_sampleimage2.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	imageNo++;
	/* image 3 */
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_2_0_025_sampleimage3.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_2_0_050_sampleimage3.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_2_0_075_sampleimage3.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_2_0_100_sampleimage3.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_2_1_025_sampleimage3.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_2_1_050_sampleimage3.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_2_1_075_sampleimage3.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_2_1_100_sampleimage3.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_2_2_025_sampleimage3.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_2_2_050_sampleimage3.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_2_2_075_sampleimage3.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_2_2_100_sampleimage3.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	quarterNo = (quarterNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_2_3_025_sampleimage3.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_2_3_050_sampleimage3.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_2_3_075_sampleimage3.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);
	passNo = (passNo + 1) % 4;
	bitmaps[imageNo][quarterNo][passNo].buffer = test_progressive_load_bitmap(ms_sample_path,
	        "decompress/dec_2_3_100_sampleimage3.bmp", &(bitmaps[imageNo][quarterNo][passNo].size), quarterNo);

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

static int test_memcmp_count(const BYTE* mem1, const BYTE* mem2, int size, int margin)
{
	int error;
	int count = 0;
	int index = 0;

	for (index = 0; index < size; index++)
	{
		if (*mem1 != *mem2)
		{
			error = (*mem1 > *mem2) ? *mem1 - *mem2 : *mem2 - *mem1;

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
	int cnt;
	int pass;
	int size;
	int index;
	int status;
	int nXSrc, nYSrc;
	int nXDst, nYDst;
	int nWidth, nHeight;
	RECTANGLE_16 tileRect;
	RECTANGLE_16 updateRect;
	RECTANGLE_16 clippingRect;
	RFX_PROGRESSIVE_TILE* tile;
	PROGRESSIVE_BLOCK_REGION* region;
	clippingRect.left = 0;
	clippingRect.top = 0;
	clippingRect.right = g_Width;
	clippingRect.bottom = g_Height;

	for (pass = 0; pass < count; pass++)
	{
		status = progressive_decompress(progressive, files[pass].buffer, files[pass].size,
		                                g_DstData, PIXEL_FORMAT_XRGB32, g_DstStep, 0, 0, NULL, 0);
		printf("ProgressiveDecompress: status: %d pass: %d\n", status, pass + 1);
		region = &(progressive->region);

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

		for (index = 0; index < region->numTiles; index++)
		{
			tile = region->tiles[index];
			tileRect.left = tile->x;
			tileRect.top = tile->y;
			tileRect.right = tile->x + tile->width;
			tileRect.bottom = tile->y + tile->height;
			rectangles_intersection(&tileRect, &clippingRect, &updateRect);
			nXDst = updateRect.left;
			nYDst = updateRect.top;
			nWidth = updateRect.right - updateRect.left;
			nHeight = updateRect.bottom - updateRect.top;

			if ((nWidth <= 0) || (nHeight <= 0))
				continue;

			nXSrc = nXDst - tile->x;
			nYSrc = nYDst - tile->y;
			freerdp_image_copy(g_DstData, PIXEL_FORMAT_XRGB32, g_DstStep,
			                   nXDst, nYDst, nWidth, nHeight, tile->data,
			                   PIXEL_FORMAT_XRGB32, 64 * 4, nXSrc, nYSrc, NULL, FREERDP_FLIP_NONE);
		}

		size = bitmaps[pass].size;
		cnt = test_memcmp_count(g_DstData, bitmaps[pass].buffer, size, 1);

		if (cnt)
		{
			float rate = ((float) cnt) / ((float) size) * 100.0f;
			printf("Progressive RemoteFX decompression failure\n");
			printf("Actual, Expected (%d/%d = %.3f%%):\n", cnt, size, rate);
		}

		//WLog_Image(progressive->log, WLOG_TRACE, g_DstData, g_Width, g_Height, 32);
	}

	return 1;
}

static int test_progressive_ms_sample(char* ms_sample_path)
{
	int i, j, k;
	int count;
	int status;
	EGFX_SAMPLE_FILE files[3][4][4];
	EGFX_SAMPLE_FILE bitmaps[3][4][4];
	PROGRESSIVE_CONTEXT* progressive;
	g_Width = 1920;
	g_Height = 1080;
	g_DstStep = g_Width * 4;
	ZeroMemory(files, sizeof(files));
	ZeroMemory(bitmaps, sizeof(bitmaps));
	status = test_progressive_load_files(ms_sample_path, files);

	if (status < 0)
	{
		for (i = 0; i < 3; i++)
		{
			for (j = 0; j < 4; j++)
			{
				for (k = 0; k < 4; k++)
					sample_file_free(&files[i][j][k]);
			}
		}

		return -1;
	}

	status = test_progressive_load_bitmaps(ms_sample_path, bitmaps);

	if (status < 0)
	{
		for (i = 0; i < 3; i++)
		{
			for (j = 0; j < 4; j++)
			{
				for (k = 0; k < 4; k++)
					sample_file_free(&files[i][j][k]);
			}
		}

		return -1;
	}

	count = 4;
	progressive = progressive_context_new(FALSE);
	g_DstData = _aligned_malloc(g_DstStep * g_Height, 16);
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

	for (i = 0; i < 3; i++)
	{
		for (j = 0; j < 4; j++)
		{
			for (k = 0; k < 4; k++)
			{
				sample_file_free(&bitmaps[i][j][k]);
				sample_file_free(&files[i][j][k]);
			}
		}
	}

	_aligned_free(g_DstData);
	return 0;
}

int TestFreeRDPCodecProgressive(int argc, char* argv[])
{
	char* ms_sample_path;
	ms_sample_path = GetKnownSubPath(KNOWN_PATH_TEMP, "EGFX_PROGRESSIVE_MS_SAMPLE");

	if (!ms_sample_path)
	{
		printf("Memory allocation failed\n");
		return -1;
	}

	if (PathFileExistsA(ms_sample_path))
		return test_progressive_ms_sample(ms_sample_path);

	free(ms_sample_path);
	return 0;
}
