#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/print.h>

#include <freerdp/codec/progressive.h>

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
	UINT32 size;
};
typedef struct _EGFX_SAMPLE_FILE EGFX_SAMPLE_FILE;

BYTE* test_progressive_load_file(char* path, char* file, UINT32* size)
{
	FILE* fp;
	BYTE* buffer;
	char* filename;

	filename = GetCombinedPath(path, file);

	fp = fopen(filename, "r");

	if (!fp)
		return NULL;

	fseek(fp, 0, SEEK_END);
	*size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	buffer = (BYTE*) malloc(*size);

	if (!buffer)
		return NULL;

	if (fread(buffer, *size, 1, fp) != 1)
		return NULL;

	free(filename);
	fclose(fp);

	return buffer;
}

int test_progressive_load_files(char* ms_sample_path, EGFX_SAMPLE_FILE files[3][4][4])
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
	passNo = (passNo + 1) % 4;

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

static int g_Width = 0;
static int g_Height = 0;
static int g_DstStep = 0;
static BYTE* g_DstData = NULL;

int test_progressive_decode(PROGRESSIVE_CONTEXT* progressive, EGFX_SAMPLE_FILE files[4], int count)
{
	int pass;
	int status;

	for (pass = 0; pass < count; pass++)
	{
		status = progressive_decompress(progressive, files[pass].buffer, files[pass].size,
				&g_DstData, PIXEL_FORMAT_XRGB32, g_DstStep, 0, 0, g_Width, g_Height, 0);

		printf("ProgressiveDecompress: status: %d pass: %d\n", status, pass + 1);
	}

	return 1;
}

int test_progressive_ms_sample(char* ms_sample_path)
{
	int count;
	int status;
	EGFX_SAMPLE_FILE files[3][4][4];
	PROGRESSIVE_CONTEXT* progressive;

	g_Width = 1920;
	g_Height = 1080;
	g_DstStep = g_Width * 4;

	status = test_progressive_load_files(ms_sample_path, files);

	if (status < 0)
		return -1;

	count = 3;

	progressive = progressive_context_new(FALSE);

	g_DstData = _aligned_malloc(g_DstStep * g_Height, 16);

	progressive_create_surface_context(progressive, 0, g_Width, g_Height);

	/* image 1 */

	if (1)
	{
		test_progressive_decode(progressive, files[0][0], count);
		test_progressive_decode(progressive, files[0][1], count);
		test_progressive_decode(progressive, files[0][2], count);
		test_progressive_decode(progressive, files[0][3], count);
	}

	/* image 2 */

	if (1)
	{
		test_progressive_decode(progressive, files[1][0], count);
		test_progressive_decode(progressive, files[1][1], count);
		test_progressive_decode(progressive, files[1][2], count);
		test_progressive_decode(progressive, files[1][3], count);
	}

	/* image 3 */

	if (1)
	{
		test_progressive_decode(progressive, files[2][0], count);
		test_progressive_decode(progressive, files[2][1], count);
		test_progressive_decode(progressive, files[2][2], count);
		test_progressive_decode(progressive, files[2][3], count);
	}

	progressive_context_free(progressive);

	_aligned_free(g_DstData);

	return 0;
}

int TestFreeRDPCodecProgressive(int argc, char* argv[])
{
	char* ms_sample_path;

	ms_sample_path = _strdup("/tmp/EGFX_PROGRESSIVE_MS_SAMPLE");

	if (PathFileExistsA(ms_sample_path))
		return test_progressive_ms_sample(ms_sample_path);

	free(ms_sample_path);

	return 0;
}
