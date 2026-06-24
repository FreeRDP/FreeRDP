/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * libFuzzer harness for the WinPR clipboard format synthesizers
 *
 * The cliprdr channel hands remote (attacker-controlled) clipboard payloads to
 * WinPR's clipboard subsystem, which converts between formats on demand using
 * the synthesizers in synthetic.c / synthetic_file.c (CF_DIB <-> bitmap file,
 * "HTML Format" <-> text/html, FileGroupDescriptorW <-> text/uri-list, ...).
 * Those parsers consume untrusted bytes but are not otherwise fuzzed.
 *
 * This harness registers a source format, stores the fuzz input under it, then
 * requests every other registered format so each applicable synthesizer parses
 * the payload.
 */

#include <stddef.h>
#include <stdint.h>

#include <winpr/crt.h>
#include <winpr/clipboard.h>
#include <winpr/wlog.h>

static const char* kSourceFormats[] = { "CF_DIB",
	                                    "CF_DIBV5",
	                                    "HTML Format",
	                                    "text/html",
	                                    "image/bmp",
	                                    "image/png",
	                                    "FileGroupDescriptorW",
	                                    "text/uri-list",
	                                    "CF_UNICODETEXT",
	                                    "CF_TEXT",
	                                    "CF_OEMTEXT" };

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	static BOOL loggingInitialized = FALSE;

	if (!loggingInitialized)
	{
		(void)WLog_SetLogLevel(WLog_GetRoot(), WLOG_OFF);
		loggingInitialized = TRUE;
	}

	if (size < 2)
		return 0;
	if (size > (1u << 20))
		return 0;

	wClipboard* clipboard = ClipboardCreate();
	if (!clipboard)
		return 0;

	const size_t count = sizeof(kSourceFormats) / sizeof(kSourceFormats[0]);
	const char* srcName = kSourceFormats[data[0] % count];

	UINT32 srcId = ClipboardRegisterFormat(clipboard, srcName);
	if (srcId != 0)
	{
		/* Store the remaining bytes as the (attacker) payload for srcName. */
		(void)ClipboardSetData(clipboard, srcId, data + 1, (UINT32)(size - 1));

		UINT32* formatIds = NULL;
		UINT32 numFormats = ClipboardGetFormatIds(clipboard, &formatIds);

		for (UINT32 i = 0; i < numFormats; i++)
		{
			UINT32 outSize = 0;
			void* out = ClipboardGetData(clipboard, formatIds[i], &outSize);
			free(out);
		}
		free(formatIds);
	}

	ClipboardDestroy(clipboard);
	return 0;
}
