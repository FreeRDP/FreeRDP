#include <winpr/stream.h>

#include "../rdpexps_common.h"

int TestRdpeXps(int argc, char* argv[])
{
	RDPEXPS_REQUEST_HEADER request = { 0x12345678, 0x87654321, 0x00000002 };
	RDPEXPS_REQUEST_HEADER actual = WINPR_C_ARRAY_INIT;
	wStream* input = nullptr;
	wStream* output = nullptr;
	int rc = -1;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	input = Stream_New(nullptr, 12);
	output = Stream_New(nullptr, 8);
	if (!input || !output)
		goto out;

	Stream_Write_UINT32(input, request.InterfaceId);
	Stream_Write_UINT32(input, request.MessageId);
	Stream_Write_UINT32(input, request.FunctionId);
	if (!Stream_SetPosition(input, 0))
		goto out;
	if (!rdpexps_read_request_header(input, &actual))
		goto out;
	if ((actual.InterfaceId != request.InterfaceId) || (actual.MessageId != request.MessageId) ||
	    (actual.FunctionId != request.FunctionId))
		goto out;
	if (!rdpexps_write_response_header(output, &actual))
		goto out;
	if (Stream_GetPosition(output) != 8)
		goto out;
	if (!Stream_SetPosition(output, 0))
		goto out;
	if ((Stream_Get_UINT32(output) != request.InterfaceId) ||
	    (Stream_Get_UINT32(output) != request.MessageId))
		goto out;
	if (!Stream_SetPosition(input, 0))
		goto out;
	if (!Stream_SetLength(input, 11))
		goto out;
	if (rdpexps_read_request_header(input, &actual))
		goto out;

	rc = 0;
out:
	Stream_Free(input, TRUE);
	Stream_Free(output, TRUE);
	return rc;
}
