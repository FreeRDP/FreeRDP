
#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/image.h>
#include <winpr/clipboard.h>

int TestClipboardFormats(int argc, char* argv[])
{
	int rc = -1;
	UINT32 count = 0;
	UINT32* pFormatIds = NULL;
	const char* formatName = NULL;
	wClipboard* clipboard = NULL;
	UINT32 utf8StringFormatId = 0;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	clipboard = ClipboardCreate();
	if (!clipboard)
		return -1;

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
	pFormatIds = NULL;
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
		char* pDstData = NULL;

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
		char* pSrcData = NULL;
		WCHAR* pDstData = NULL;
		DstSize = 0;
		pDstData = (WCHAR*)ClipboardGetData(clipboard, CF_UNICODETEXT, &DstSize);
		pSrcData = ConvertWCharNToUtf8Alloc(pDstData, DstSize / sizeof(WCHAR), NULL);

		(void)fprintf(stderr, "ClipboardGetData (synthetic): %s\n", pSrcData);
		free(pDstData);
		free(pSrcData);
	}

	pFormatIds = NULL;
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
