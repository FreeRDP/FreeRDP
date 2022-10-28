
#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/clipboard.h>

int TestClipboardFormats(int argc, char* argv[])
{
	UINT32 index;
	UINT32 count;
	UINT32* pFormatIds;
	const char* formatName;
	wClipboard* clipboard;
	UINT32 utf8StringFormatId;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	clipboard = ClipboardCreate();
	if (!clipboard)
		return -1;

	ClipboardRegisterFormat(clipboard, "text/html");
	ClipboardRegisterFormat(clipboard, "image/bmp");
	ClipboardRegisterFormat(clipboard, "image/png");

	utf8StringFormatId = ClipboardRegisterFormat(clipboard, "UTF8_STRING");
	pFormatIds = NULL;
	count = ClipboardGetRegisteredFormatIds(clipboard, &pFormatIds);

	for (index = 0; index < count; index++)
	{
		UINT32 formatId = pFormatIds[index];
		formatName = ClipboardGetFormatName(clipboard, formatId);
		fprintf(stderr, "Format: 0x%08" PRIX32 " %s\n", formatId, formatName);
	}

	free(pFormatIds);

	if (1)
	{
		BOOL bSuccess;
		UINT32 SrcSize;
		UINT32 DstSize;
		const char* pSrcData = "this is a test string";
		char* pDstData;

		if (!pSrcData)
		{
			fprintf(stderr, "Memory allocation failed\n");
			return -1;
		}

		SrcSize = (UINT32)(strnlen(pSrcData, UINT32_MAX - 1) + 1);
		bSuccess = ClipboardSetData(clipboard, utf8StringFormatId, pSrcData, SrcSize);
		fprintf(stderr, "ClipboardSetData: %" PRId32 "\n", bSuccess);
		DstSize = 0;
		pDstData = (char*)ClipboardGetData(clipboard, utf8StringFormatId, &DstSize);
		fprintf(stderr, "ClipboardGetData: %s\n", pDstData);
		free(pDstData);
	}

	if (1)
	{
		UINT32 DstSize;
		char* pSrcData;
		WCHAR* pDstData;
		DstSize = 0;
		pDstData = (WCHAR*)ClipboardGetData(clipboard, CF_UNICODETEXT, &DstSize);
		pSrcData = ConvertWCharNToUtf8Alloc(pDstData, DstSize / sizeof(WCHAR), NULL);

		fprintf(stderr, "ClipboardGetData (synthetic): %s\n", pSrcData);
		free(pDstData);
		free(pSrcData);
	}

	pFormatIds = NULL;
	count = ClipboardGetFormatIds(clipboard, &pFormatIds);

	for (index = 0; index < count; index++)
	{
		UINT32 formatId = pFormatIds[index];
		formatName = ClipboardGetFormatName(clipboard, formatId);
		fprintf(stderr, "Format: 0x%08" PRIX32 " %s\n", formatId, formatName);
	}

	free(pFormatIds);
	ClipboardDestroy(clipboard);
	return 0;
}
