
#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/image.h>
#include <winpr/clipboard.h>
#include <winpr/stream.h>
#include <winpr/user.h>

static BOOL test_dib_to_bmp(const BYTE* dib, size_t dibSize, size_t expectedOffset)
{
	BOOL rc = FALSE;

	wClipboard* clipboard = ClipboardCreate();
	if (!clipboard)
		return FALSE;

	const UINT32 bmpId = ClipboardRegisterFormat(clipboard, "image/bmp");
	if ((bmpId == 0) || (dibSize > UINT32_MAX) ||
	    !ClipboardSetData(clipboard, CF_DIB, dib, (UINT32)dibSize))
		goto fail;

	UINT32 bmpSize = 0;
	BYTE* bmp = ClipboardGetData(clipboard, bmpId, &bmpSize);
	if (!bmp)
		goto fail;

	wStream bmpBuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticConstInit(&bmpBuffer, bmp, bmpSize);
	if (!s || (bmpSize < sizeof(WINPR_BITMAP_FILE_HEADER)))
		goto fail_bmp;

	Stream_Seek(s, 10);
	UINT32 bitmapOffset = 0;
	Stream_Read_UINT32(s, bitmapOffset);
	if ((bitmapOffset != expectedOffset) ||
	    (bmpSize != sizeof(WINPR_BITMAP_FILE_HEADER) + dibSize) ||
	    (memcmp(&bmp[sizeof(WINPR_BITMAP_FILE_HEADER)], dib, dibSize) != 0))
		goto fail_bmp;

	rc = TRUE;

fail_bmp:
	free(bmp);
fail:
	ClipboardDestroy(clipboard);
	return rc;
}

static void write_dib_info_header(wStream* s, UINT32 size, UINT16 bpp, UINT32 compression)
{
	WINPR_ASSERT(s);

	Stream_Write_UINT32(s, size);
	Stream_Write_INT32(s, 1);  /* width */
	Stream_Write_INT32(s, 1);  /* height */
	Stream_Write_UINT16(s, 1); /* planes */
	Stream_Write_UINT16(s, bpp);
	Stream_Write_UINT32(s, compression);
	Stream_Write_UINT32(s, 4); /* image size */
	Stream_Zero(s, 16);        /* resolution and palette metadata */
}

static BOOL test_dib_offsets(void)
{
	BYTE v4[sizeof(BITMAPV4HEADER) + 4] = WINPR_C_ARRAY_INIT;
	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticInit(&sbuffer, v4, sizeof(v4));
	if (!s)
		return FALSE;
	write_dib_info_header(s, sizeof(BITMAPV4HEADER), 32, BI_BITFIELDS);

	Stream_Write_UINT32(s, 0x00FF0000); /* red mask */
	Stream_Write_UINT32(s, 0x0000FF00); /* green mask */
	Stream_Write_UINT32(s, 0x000000FF); /* blue mask */
	Stream_Write_UINT32(s, 0xFF000000); /* alpha mask */
	Stream_Zero(s, sizeof(BITMAPV4HEADER) - Stream_GetPosition(s));
	Stream_Write_UINT32(s, 0xFF123456); /* pixel */
	if (!test_dib_to_bmp(v4, sizeof(v4), sizeof(WINPR_BITMAP_FILE_HEADER) + sizeof(BITMAPV4HEADER)))
		return FALSE;

	BYTE bitfields[sizeof(BITMAPINFOHEADER) + 3 * sizeof(DWORD) + 4] = WINPR_C_ARRAY_INIT;
	s = Stream_StaticInit(&sbuffer, bitfields, sizeof(bitfields));
	if (!s)
		return FALSE;
	write_dib_info_header(s, sizeof(BITMAPINFOHEADER), 32, BI_BITFIELDS);
	Stream_Write_UINT32(s, 0x00FF0000); /* red mask */
	Stream_Write_UINT32(s, 0x0000FF00); /* green mask */
	Stream_Write_UINT32(s, 0x000000FF); /* blue mask */
	Stream_Write_UINT32(s, 0xFF123456); /* pixel */
	if (!test_dib_to_bmp(bitfields, sizeof(bitfields),
	                     sizeof(WINPR_BITMAP_FILE_HEADER) + sizeof(BITMAPINFOHEADER) +
	                         3 * sizeof(DWORD)))
		return FALSE;

	BYTE paletted[sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD) + 4] = WINPR_C_ARRAY_INIT;
	s = Stream_StaticInit(&sbuffer, paletted, sizeof(paletted));
	if (!s)
		return FALSE;
	write_dib_info_header(s, sizeof(BITMAPINFOHEADER), 8, BI_RGB);
	Stream_Zero(s, 256 * sizeof(RGBQUAD));
	Stream_Write_UINT32(s, 0); /* pixel row */
	return test_dib_to_bmp(paletted, sizeof(paletted),
	                       sizeof(WINPR_BITMAP_FILE_HEADER) + sizeof(BITMAPINFOHEADER) +
	                           256 * sizeof(RGBQUAD));
}

int TestClipboardFormats(int argc, char* argv[])
{
	int rc = -1;
	UINT32 count = 0;
	UINT32* pFormatIds = nullptr;
	const char* formatName = nullptr;
	wClipboard* clipboard = nullptr;
	UINT32 utf8StringFormatId = 0;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	clipboard = ClipboardCreate();
	if (!clipboard)
		return -1;
	if (!test_dib_offsets())
		goto fail;

	const char* mime_types[] = { "text/html", "text/html",  "image/bmp",
		                         "image/png", "image/webp", "image/jpeg" };
	for (size_t x = 0; x < ARRAYSIZE(mime_types); x++)
	{
		const char* mime = mime_types[x];
		UINT32 id = ClipboardRegisterFormat(clipboard, mime);
		(void)fprintf(stderr, "ClipboardRegisterFormat(%s) -> 0x%08" PRIx32 "\n", mime, id);
		if (id == 0)
			goto fail;
	}

	utf8StringFormatId = ClipboardRegisterFormat(clipboard, "UTF8_STRING");
	pFormatIds = nullptr;
	count = ClipboardGetRegisteredFormatIds(clipboard, &pFormatIds);

	for (UINT32 index = 0; index < count; index++)
	{
		UINT32 formatId = pFormatIds[index];
		formatName = ClipboardGetFormatName(clipboard, formatId);
		(void)fprintf(stderr, "Format: 0x%08" PRIX32 " %s\n", formatId, formatName);
	}

	free(pFormatIds);

	if (1)
	{
		BOOL bSuccess = 0;
		UINT32 SrcSize = 0;
		UINT32 DstSize = 0;
		const char pSrcData[] = "this is a test string";
		char* pDstData = nullptr;

		SrcSize = (UINT32)(strnlen(pSrcData, ARRAYSIZE(pSrcData)) + 1);
		bSuccess = ClipboardSetData(clipboard, utf8StringFormatId, pSrcData, SrcSize);
		(void)fprintf(stderr, "ClipboardSetData: %" PRId32 "\n", bSuccess);
		DstSize = 0;
		pDstData = (char*)ClipboardGetData(clipboard, utf8StringFormatId, &DstSize);
		(void)fprintf(stderr, "ClipboardGetData: %s\n", pDstData);
		free(pDstData);
	}

	if (1)
	{
		UINT32 DstSize = 0;
		char* pSrcData = nullptr;
		WCHAR* pDstData = nullptr;
		DstSize = 0;
		pDstData = (WCHAR*)ClipboardGetData(clipboard, CF_UNICODETEXT, &DstSize);
		pSrcData = ConvertWCharNToUtf8Alloc(pDstData, DstSize / sizeof(WCHAR), nullptr);

		(void)fprintf(stderr, "ClipboardGetData (synthetic): %s\n", pSrcData);
		free(pDstData);
		free(pSrcData);
	}

	pFormatIds = nullptr;
	count = ClipboardGetFormatIds(clipboard, &pFormatIds);

	for (UINT32 index = 0; index < count; index++)
	{
		UINT32 formatId = pFormatIds[index];
		formatName = ClipboardGetFormatName(clipboard, formatId);
		(void)fprintf(stderr, "Format: 0x%08" PRIX32 " %s\n", formatId, formatName);
	}

	if (1)
	{
		const char* name = TEST_CLIP_BMP;
		BOOL bSuccess = FALSE;
		UINT32 idBmp = ClipboardRegisterFormat(clipboard, "image/bmp");

		wImage* img = winpr_image_new();
		if (!img)
			goto fail;

		if (winpr_image_read(img, name) <= 0)
		{
			winpr_image_free(img, TRUE);
			goto fail;
		}

		size_t bmpsize = 0;
		void* data = winpr_image_write_buffer(img, WINPR_IMAGE_BITMAP, &bmpsize);
		bSuccess = ClipboardSetData(clipboard, idBmp, data, bmpsize);
		(void)fprintf(stderr, "ClipboardSetData: %" PRId32 "\n", bSuccess);

		free(data);
		winpr_image_free(img, TRUE);
		if (!bSuccess)
			goto fail;

		{
			UINT32 id = CF_DIB;

			UINT32 DstSize = 0;
			void* pDstData = ClipboardGetData(clipboard, id, &DstSize);
			(void)fprintf(stderr, "ClipboardGetData: [CF_DIB] %p [%" PRIu32 "]\n", pDstData,
			              DstSize);
			if (!pDstData)
				goto fail;
			bSuccess = ClipboardSetData(clipboard, id, pDstData, DstSize);
			free(pDstData);
			if (!bSuccess)
				goto fail;
		}
		{
			const uint32_t id = ClipboardGetFormatId(clipboard, "HTML Format");
			UINT32 DstSize = 0;
			void* pDstData = ClipboardGetData(clipboard, id, &DstSize);
			if (!pDstData)
				goto fail;
			{
				FILE* fp = fopen("test.html", "w");
				if (fp)
				{
					(void)fwrite(pDstData, 1, DstSize, fp);
					(void)fclose(fp);
				}
			}
			free(pDstData);
		}
		{
			UINT32 id = ClipboardRegisterFormat(clipboard, "image/bmp");

			UINT32 DstSize = 0;
			void* pDstData = ClipboardGetData(clipboard, id, &DstSize);
			(void)fprintf(stderr, "ClipboardGetData: [image/bmp] %p [%" PRIu32 "]\n", pDstData,
			              DstSize);
			if (!pDstData)
				goto fail;
			free(pDstData);
			if (DstSize != bmpsize)
				goto fail;
		}

#if defined(WINPR_UTILS_IMAGE_PNG)
		{
			UINT32 id = ClipboardRegisterFormat(clipboard, "image/png");

			UINT32 DstSize = 0;
			void* pDstData = ClipboardGetData(clipboard, id, &DstSize);
			(void)fprintf(stderr, "ClipboardGetData: [image/png] %p\n", pDstData);
			if (!pDstData)
				goto fail;
			free(pDstData);
		}
		{
			const char* name = TEST_CLIP_PNG;
			BOOL bSuccess = FALSE;
			UINT32 idBmp = ClipboardRegisterFormat(clipboard, "image/png");

			wImage* img = winpr_image_new();
			if (!img)
				goto fail;

			if (winpr_image_read(img, name) <= 0)
			{
				winpr_image_free(img, TRUE);
				goto fail;
			}

			size_t bmpsize = 0;
			void* data = winpr_image_write_buffer(img, WINPR_IMAGE_PNG, &bmpsize);
			bSuccess = ClipboardSetData(clipboard, idBmp, data, bmpsize);
			(void)fprintf(stderr, "ClipboardSetData: %" PRId32 "\n", bSuccess);

			free(data);
			winpr_image_free(img, TRUE);
			if (!bSuccess)
				goto fail;
		}
		{
			UINT32 id = CF_DIB;

			UINT32 DstSize = 0;
			void* pDstData = ClipboardGetData(clipboard, id, &DstSize);
			(void)fprintf(stderr, "ClipboardGetData: [CF_DIB] %p [%" PRIu32 "]\n", pDstData,
			              DstSize);
			if (!pDstData)
				goto fail;
			bSuccess = ClipboardSetData(clipboard, id, pDstData, DstSize);
			free(pDstData);
			if (!bSuccess)
				goto fail;
		}
#endif

#if defined(WINPR_UTILS_IMAGE_WEBP)
		{
			UINT32 id = ClipboardRegisterFormat(clipboard, "image/webp");

			UINT32 DstSize = 0;
			void* pDstData = ClipboardGetData(clipboard, id, &DstSize);
			(void)fprintf(stderr, "ClipboardGetData: [image/webp] %p\n", pDstData);
			if (!pDstData)
				goto fail;
			free(pDstData);
		}
#endif

#if defined(WINPR_UTILS_IMAGE_JPEG)
		{
			UINT32 id = ClipboardRegisterFormat(clipboard, "image/jpeg");

			UINT32 DstSize = 0;
			void* pDstData = ClipboardGetData(clipboard, id, &DstSize);
			(void)fprintf(stderr, "ClipboardGetData: [image/jpeg] %p\n", pDstData);
			if (!pDstData)
				goto fail;
			free(pDstData);
		}
#endif
	}

	rc = 0;

fail:
	free(pFormatIds);
	ClipboardDestroy(clipboard);
	return rc;
}
