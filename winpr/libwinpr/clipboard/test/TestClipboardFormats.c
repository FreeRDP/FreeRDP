
#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/image.h>
#include <winpr/clipboard.h>
#include <winpr/stream.h>
#include <winpr/user.h>

#define test_log(fmt, ...) test_log_fn(__FILE__, __func__, __LINE__, (fmt), ##__VA_ARGS__)
static void test_log_fn(const char* file, const char* func, size_t line, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	char* fstr = nullptr;
	size_t fslen = 0;
	winpr_vasprintf(&fstr, &fslen, fmt, ap);
	(void)fprintf(stderr, "[%" PRIu32 ":%s]: %s\n", line, func, fstr);
	free(fstr);
	va_end(ap);
}

#define test_ClipboardSetData(clipboard, formatId, data, size) \
	test_ClipboardSetDataFn(__FILE__, __func__, __LINE__, (clipboard), (formatId), (data), (size))
static BOOL test_ClipboardSetDataFn(const char* file, const char* func, size_t line,
                                    wClipboard* clipboard, UINT32 formatId, const void* data,
                                    UINT32 size)
{
	const BOOL rc = ClipboardSetData(clipboard, formatId, data, size);
	if (!rc)
	{
		test_log_fn(file, func, line, "ClipboardSetData %s[0x%04" PRIx32 "][%" PRIu32 "] failed",
		            ClipboardGetFormatName(clipboard, formatId), formatId, size);
	}
	return rc;
}

#define test_ClipboardGetData(clipboard, formatId, size) \
	test_ClipboardGetDataFn(__FILE__, __func__, __LINE__, (clipboard), (formatId), (size))
static void* test_ClipboardGetDataFn(const char* file, const char* func, size_t line,
                                     wClipboard* clipboard, UINT32 formatId, UINT32* pSize)
{
	void* rc = ClipboardGetData(clipboard, formatId, pSize);
	if (!rc)
	{
		test_log_fn(file, func, line, "ClipboardGetData %s[0x%04" PRIx32 "] failed",
		            ClipboardGetFormatName(clipboard, formatId), formatId);
	}
	return rc;
}

WINPR_ATTR_NODISCARD
static BOOL test_dib_to_bmp(const BYTE* dib, size_t dibSize, size_t expectedOffset)
{
	BOOL rc = FALSE;

	wClipboard* clipboard = ClipboardCreate();
	if (!clipboard)
	{
		test_log("ClipboardCreate failed");
		return FALSE;
	}

	const UINT32 bmpId = ClipboardRegisterFormat(clipboard, "image/bmp");
	if (bmpId == 0)
	{
		test_log("bmpId == 0");
		goto fail;
	}
	if (dibSize > UINT32_MAX)
	{
		test_log("dibSize %" PRIuz " > UINT32_MAX failed");
		goto fail;
	}
	if (!test_ClipboardSetData(clipboard, CF_DIB, dib, (UINT32)dibSize))
		goto fail;

	UINT32 bmpSize = 0;
	BYTE* bmp = test_ClipboardGetData(clipboard, bmpId, &bmpSize);
	if (!bmp)
		goto fail;

	/* The synthesized BMP must also be readable by the image conversion code. */
	wImage* image = winpr_image_new();
	if (!image)
	{
		test_log("winpr_image_new failed");
		goto fail_bmp;
	}
	rc = (winpr_image_read_buffer(image, bmp, bmpSize) > 0);
	if (!rc)
		test_log("winpr_image_read_buffer failed");
	winpr_image_free(image, TRUE);

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

WINPR_ATTR_NODISCARD
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

WINPR_ATTR_NODISCARD
static BOOL test_text_leading_newline(void)
{
	const char* tests[][2] = { { "\nfoo", "\r\nfoo" }, { "\n", "\r\n" } };
	BOOL rc = FALSE;

	wClipboard* clipboard = ClipboardCreate();
	if (!clipboard)
	{
		test_log("ClipboardCreate failed");
		return FALSE;
	}

	const UINT32 textId = ClipboardRegisterFormat(clipboard, "text/plain");
	for (size_t x = 0; x < ARRAYSIZE(tests); x++)
	{
		const char** cur = tests[x];
		/* local text/plain is set without a terminator */
		if (!test_ClipboardSetData(clipboard, textId, cur[0], (UINT32)strlen(cur[0])))
			goto fail;

		UINT32 size = 0;
		WCHAR* wstr = test_ClipboardGetData(clipboard, CF_UNICODETEXT, &size);
		char* str = ConvertWCharNToUtf8Alloc(wstr, size / sizeof(WCHAR), nullptr);
		const BOOL match = str && (strcmp(str, cur[1]) == 0);
		if (!match)
		{
			test_log("text/plain to CF_UNICODETEXT failed for case %" PRIuz "[%s (%" PRIu32
			         ") %p : expect %s (%" PRIuz ") : from %s (" PRIuz ")]",
			         x, str, size, wstr, cur[1], strlen(cur[1]), cur[0], strlen(cur[0]));
		}
		free(wstr);
		free(str);
		if (!match)
			goto fail;
	}
	rc = TRUE;

fail:
	ClipboardDestroy(clipboard);
	return rc;
}

WINPR_ATTR_NODISCARD
static BOOL test_text_conversion(void)
{
	wClipboard* clipboard = ClipboardCreate();
	if (!clipboard)
		return FALSE;

	typedef struct
	{
		UINT32 srcFormat;
		UINT32 dstFormat;
		const char* src;
		size_t srcLen;
		const char* expect;
		size_t expectLen;
	} test_case_t;

	const test_case_t tests[] = {
		{ ClipboardRegisterFormat(clipboard, "text/plain"),
		  ClipboardRegisterFormat(clipboard, "text/plain;charset=utf-8"), "a\nb\rc\\u1234\\u055e",
		  17, "a\nb\ncሴ՞", 10 },
		{ ClipboardRegisterFormat(clipboard, "text/plain;charset=utf-8"),
		  ClipboardRegisterFormat(clipboard, "text/plain"), "a\r\nb\nc՞՞", 10,
		  "a\r\nb\nc\\u055e\\u055e", 18 },
		{ ClipboardRegisterFormat(clipboard, "text/plain"), CF_UNICODETEXT, "a\nb\rc\\u1234\\u055e",
		  17, "\x61\x00\x0d\x00\x0a\x00\x62\x00\x0d\x0\x0a\x00\x63\x00\x34\x12\x5e\x05\x00\x00\x00",
		  18 },
		{ CF_UNICODETEXT, ClipboardRegisterFormat(clipboard, "text/plain"),
		  "\x61\x00\x0d\x00\x0a\x00\x62\x00\x0d\x0\x0a\x00\x63\x00\x34\x12\x5e\x05\x00\x00\x00", 18,
		  "a\nb\nc\\u1234\\u055e", 17 },
		{ ClipboardRegisterFormat(clipboard, "text/plain"), CF_TEXT, "a\nb\rc\\u1234\\u055e", 17,
		  "a\nb\nc\\u1234\\u055e", 17 },
		{ CF_TEXT, ClipboardRegisterFormat(clipboard, "text/plain"), "a\nb\nc\\u1234\\u055e", 17,
		  "a\nb\nc\\u1234\\u055e", 17 },
		{ ClipboardRegisterFormat(clipboard, "text/plain"), CF_OEMTEXT, "a\nb\rc\\u1234\\u055e", 17,
		  "a\nb\nc\\u1234\\u055e", 17 },
		{ CF_OEMTEXT, ClipboardRegisterFormat(clipboard, "text/plain"), "a\nb\rc\\u1234\\u055e", 17,
		  "a\nb\nc\\u1234\\u055e", 17 }
	};

	BOOL rc = FALSE;

	for (size_t x = 0; x < ARRAYSIZE(tests); x++)
	{
		const test_case_t* cur = &tests[x];

		/* local text/plain is set without a terminator */
		if (!test_ClipboardSetData(clipboard, cur->srcFormat, cur->src, cur->srcLen))
			goto fail;

		UINT32 dstSize = 0;
		BYTE* data = test_ClipboardGetData(clipboard, cur->dstFormat, &dstSize);
		if (dstSize != cur->expectLen)
		{
			winpr_znfree(data, dstSize);
			goto fail;
		}
		if (dstSize != 0)
		{
			if (!data)
			{
				winpr_znfree(data, dstSize);
				goto fail;
			}
			const int rc = memcmp(cur->expect, data, cur->expectLen);
			winpr_znfree(data, dstSize);
			if (rc != 0)
				goto fail;
		}
		else
			winpr_znfree(data, dstSize);
	}
	rc = TRUE;

fail:
	ClipboardDestroy(clipboard);
	return rc;
}

int TestClipboardFormats(int argc, char* argv[])
{
	int rc = -1;
	UINT32 count = 0;
	UINT32* pFormatIds = nullptr;
	const char* formatName = nullptr;
	UINT32 utf8StringFormatId = 0;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!test_text_leading_newline())
		return -1;

	if (!test_text_conversion())
		return -1;

	wClipboard* clipboard = ClipboardCreate();
	if (!clipboard)
		return -1;

	const char* mime_types[] = { "text/plain", "text/plain;charset=utf-8",
		                         "text/html",  "image/bmp",
		                         "image/png",  "image/webp",
		                         "image/jpeg" };
	for (size_t x = 0; x < ARRAYSIZE(mime_types); x++)
	{
		const char* mime = mime_types[x];
		UINT32 id = ClipboardRegisterFormat(clipboard, mime);
		test_log("ClipboardRegisterFormat(%s) -> 0x%08" PRIx32 "", mime, id);
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
		test_log("Format: 0x%08" PRIX32 " %s", formatId, formatName);
	}

	free(pFormatIds);

	if (1)
	{
		UINT32 SrcSize = 0;
		UINT32 DstSize = 0;
		const char pSrcData[] = "this is a test string";
		char* pDstData = nullptr;

		SrcSize = (UINT32)(strnlen(pSrcData, ARRAYSIZE(pSrcData)) + 1);
		const BOOL bSuccess =
		    test_ClipboardSetData(clipboard, utf8StringFormatId, pSrcData, SrcSize);
		if (!bSuccess)
			goto fail;

		DstSize = 0;
		pDstData = (char*)test_ClipboardGetData(clipboard, utf8StringFormatId, &DstSize);
		free(pDstData);
	}

	if (1)
	{
		UINT32 DstSize = 0;
		char* pSrcData = nullptr;
		WCHAR* pDstData = nullptr;
		DstSize = 0;
		pDstData = (WCHAR*)test_ClipboardGetData(clipboard, CF_UNICODETEXT, &DstSize);
		pSrcData = ConvertWCharNToUtf8Alloc(pDstData, DstSize / sizeof(WCHAR), nullptr);

		free(pDstData);
		free(pSrcData);
	}

	pFormatIds = nullptr;
	count = ClipboardGetFormatIds(clipboard, &pFormatIds);

	for (UINT32 index = 0; index < count; index++)
	{
		UINT32 formatId = pFormatIds[index];
		formatName = ClipboardGetFormatName(clipboard, formatId);
		test_log("Format: 0x%08" PRIX32 " %s", formatId, formatName);
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
		bSuccess = test_ClipboardSetData(clipboard, idBmp, data, bmpsize);

		free(data);
		winpr_image_free(img, TRUE);
		if (!bSuccess)
			goto fail;

		{
			UINT32 id = CF_DIB;

			UINT32 DstSize = 0;
			void* pDstData = test_ClipboardGetData(clipboard, id, &DstSize);
			if (!pDstData)
				goto fail;
			bSuccess = test_ClipboardSetData(clipboard, id, pDstData, DstSize);
			free(pDstData);
			if (!bSuccess)
				goto fail;
		}
		{
			const uint32_t id = ClipboardGetFormatId(clipboard, "HTML Format");
			UINT32 DstSize = 0;
			void* pDstData = test_ClipboardGetData(clipboard, id, &DstSize);
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
			void* pDstData = test_ClipboardGetData(clipboard, id, &DstSize);
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
			void* pDstData = test_ClipboardGetData(clipboard, id, &DstSize);
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
			bSuccess = test_ClipboardSetData(clipboard, idBmp, data, bmpsize);

			free(data);
			winpr_image_free(img, TRUE);
			if (!bSuccess)
				goto fail;
		}
		{
			UINT32 id = CF_DIB;

			UINT32 DstSize = 0;
			void* pDstData = test_ClipboardGetData(clipboard, id, &DstSize);
			if (!pDstData)
				goto fail;
			bSuccess = test_ClipboardSetData(clipboard, id, pDstData, DstSize);
			free(pDstData);
			if (!bSuccess)
				goto fail;
		}
#endif

#if defined(WINPR_UTILS_IMAGE_WEBP)
		{
			UINT32 id = ClipboardRegisterFormat(clipboard, "image/webp");

			UINT32 DstSize = 0;
			void* pDstData = test_ClipboardGetData(clipboard, id, &DstSize);
			if (!pDstData)
				goto fail;
			free(pDstData);
		}
#endif

#if defined(WINPR_UTILS_IMAGE_JPEG)
		{
			UINT32 id = ClipboardRegisterFormat(clipboard, "image/jpeg");

			UINT32 DstSize = 0;
			void* pDstData = test_ClipboardGetData(clipboard, id, &DstSize);
			if (!pDstData)
				goto fail;
			free(pDstData);
		}
#endif
	}

	if (!test_dib_offsets())
		goto fail;

	rc = 0;

fail:
	free(pFormatIds);
	ClipboardDestroy(clipboard);
	return rc;
}
