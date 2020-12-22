#include <freerdp/assistance.h>

static int parse_file_buffer(const uint8_t* Data, size_t Size)
{
	static const char TEST_MSRC_INCIDENT_PASSWORD_TYPE2[] = "48BJQ853X3B4";
	int status = -1;
	rdpAssistanceFile* file = freerdp_assistance_file_new();
	if (!file)
		return -1;
	status = freerdp_assistance_parse_file_buffer(file, (char*)Data, Size,
	                                              TEST_MSRC_INCIDENT_PASSWORD_TYPE2);
	freerdp_assistance_file_free(file);

	return status >= 0 ? TRUE : FALSE;
}

int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	parse_file_buffer(Data, Size);
	return 0;
}
