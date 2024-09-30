#include <freerdp/assistance.h>

static int parse_file_buffer(const uint8_t* Data, size_t Size)
{
	static const char TEST_MSRC_INCIDENT_PASSWORD_TYPE2[] = "48BJQ853X3B4";
	int status = -1;
	rdpAssistanceFile* file = freerdp_assistance_file_new();
	if (!file)
		return -1;

	char* buf = calloc(Size + 1, sizeof(char));
	if (buf == NULL)
		goto err;
	memcpy(buf, Data, Size);
	buf[Size] = '\0';

	status = freerdp_assistance_parse_file_buffer(file, buf, Size + 1,
	                                              TEST_MSRC_INCIDENT_PASSWORD_TYPE2);

err:
	freerdp_assistance_file_free(file);
	free(buf);

	return status >= 0 ? TRUE : FALSE;
}

int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	parse_file_buffer(Data, Size);
	return 0;
}
