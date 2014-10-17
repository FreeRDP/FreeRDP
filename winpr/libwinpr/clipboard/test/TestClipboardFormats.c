
#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/clipboard.h>

void* synthesize_utf8_string_to_cf_unicodetext(void* context, UINT32 formatId, const void* data, UINT32* pSize)
{
	int size;
	int status;
	char* crlfStr = NULL;
	WCHAR* pDstData = NULL;

	size = (int) *pSize;
	crlfStr = ConvertLineEndingToCRLF((char*) data, &size);

	if (!crlfStr)
		return NULL;

	status = ConvertToUnicode(CP_UTF8, 0, crlfStr, size, &pDstData, 0);
	free(crlfStr);

	if (status <= 0)
		return NULL;

	*pSize = ((status + 1) * 2);

	return (void*) pDstData;
}

int TestClipboardFormats(int argc, char* argv[])
{
	UINT32 index;
	UINT32 count;
	UINT32 formatId;
	UINT32* pFormatIds;
	const char* formatName;
	wClipboard* clipboard;
	UINT32 utf8StringFormatId;

	clipboard = ClipboardCreate();

	formatId = ClipboardRegisterFormat(clipboard, "text/html");
	formatId = ClipboardRegisterFormat(clipboard, "image/bmp");
	formatId = ClipboardRegisterFormat(clipboard, "image/png");
	utf8StringFormatId = ClipboardRegisterFormat(clipboard, "UFT8_STRING");

	pFormatIds = NULL;
	count = ClipboardGetRegisteredFormatIds(clipboard, &pFormatIds);

	for (index = 0; index < count; index++)
	{
		formatId = pFormatIds[index];
		formatName = ClipboardGetFormatName(clipboard, formatId);

		fprintf(stderr, "Format: 0x%04X %s\n", formatId, formatName);
	}

	free(pFormatIds);

	if (1)
	{
		BOOL bSuccess;
		UINT32 SrcSize;
		UINT32 DstSize;
		char* pSrcData;
		char* pDstData;

		pSrcData = _strdup("this is a test string");
		SrcSize = (UINT32) (strlen(pSrcData) + 1);

		bSuccess = ClipboardSetData(clipboard, utf8StringFormatId, (void*) pSrcData, SrcSize);

		fprintf(stderr, "ClipboardSetData: %d\n", bSuccess);

		DstSize = 0;
		pDstData = (char*) ClipboardGetData(clipboard, utf8StringFormatId, &DstSize);

		fprintf(stderr, "ClipboardGetData: %s\n", pDstData);

		free(pDstData);
	}

	if (1)
	{
		BOOL bSuccess;
		UINT32 DstSize;
		char* pSrcData;
		WCHAR* pDstData;

		bSuccess = ClipboardRegisterSynthesizer(clipboard, utf8StringFormatId,
				CF_UNICODETEXT, synthesize_utf8_string_to_cf_unicodetext, NULL);

		fprintf(stderr, "ClipboardRegisterSynthesizer: %d\n", bSuccess);

		DstSize = 0;
		pDstData = (WCHAR*) ClipboardGetData(clipboard, CF_UNICODETEXT, &DstSize);

		pSrcData = NULL;
		ConvertFromUnicode(CP_UTF8, 0, pDstData, -1, &pSrcData, 0, NULL, NULL);

		fprintf(stderr, "ClipboardGetData (synthetic): %s\n", pSrcData);

		free(pDstData);
		free(pSrcData);
	}

	pFormatIds = NULL;
	count = ClipboardGetFormatIds(clipboard, &pFormatIds);

	for (index = 0; index < count; index++)
	{
		formatId = pFormatIds[index];
		formatName = ClipboardGetFormatName(clipboard, formatId);

		fprintf(stderr, "Format: 0x%04X %s\n", formatId, formatName);
	}

	free(pFormatIds);

	ClipboardDestroy(clipboard);

	return 0;
}

