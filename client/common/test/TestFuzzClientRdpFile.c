/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * libFuzzer harness for .rdp connection file parsing
 *
 * A .rdp file is untrusted input: they are mailed around, published for
 * download and shared between users, and opening one is the ordinary way a
 * client gets configured. client/common/file.c parses that key/value format,
 * and freerdp_client_populate_settings_from_rdp_file() then converts and range
 * checks every option on its way into the settings store.
 *
 * Neither is otherwise fuzzed. The existing assistance harness covers the
 * unrelated Remote Assistance (.msrcIncident) file format.
 *
 * Both the checked and the unchecked populate path run on every input, so a
 * mutation always means a different file rather than a different code path.
 */

#include <stddef.h>
#include <stdint.h>

#include <freerdp/client/file.h>
#include <freerdp/settings.h>

static void populate_settings(const rdpFile* file, BOOL unchecked)
{
	rdpSettings* settings = freerdp_settings_new(0);
	if (!settings)
		return;

	if (unchecked)
		(void)freerdp_client_populate_settings_from_rdp_file_unchecked(file, settings);
	else
		(void)freerdp_client_populate_settings_from_rdp_file(file, settings);

	freerdp_settings_free(settings);
}

int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	rdpFile* file = freerdp_client_rdp_file_new();
	if (!file)
		return 0;

	char* buf = calloc(Size + 1, sizeof(char));
	if (buf == nullptr)
		goto err;
	memcpy(buf, Data, Size);
	buf[Size] = '\0';

	if (freerdp_client_parse_rdp_file_buffer(file, (const BYTE*)buf, Size + 1))
	{
		populate_settings(file, FALSE);
		populate_settings(file, TRUE);
	}

err:
	freerdp_client_rdp_file_free(file);
	free(buf);

	return 0;
}
