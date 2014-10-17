
#include <winpr/crt.h>
#include <winpr/clipboard.h>

int TestClipboardFormats(int argc, char* argv[])
{
	UINT32 index;
	UINT32 count;
	UINT32 formatId;
	UINT32* pFormatIds;
	const char* formatName;
	wClipboard* clipboard;

	clipboard = ClipboardCreate();

	formatId = ClipboardRegisterFormat(clipboard, "UFT8_STRING");
	formatId = ClipboardRegisterFormat(clipboard, "text/html");
	formatId = ClipboardRegisterFormat(clipboard, "image/bmp");
	formatId = ClipboardRegisterFormat(clipboard, "image/png");

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

