/** FreeRDP: A Remote Desktop Protocol Implementation */
#include <freerdp/config.h>
#include <winpr/assert.h>
#include <winpr/stream.h>
#include "rdpexps_common.h"

BOOL rdpexps_read_request_header(wStream* s, RDPEXPS_REQUEST_HEADER* header)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(header);
	if (Stream_GetRemainingLength(s) < 12)
		return FALSE;
	Stream_Read_UINT32(s, header->InterfaceId);
	Stream_Read_UINT32(s, header->MessageId);
	Stream_Read_UINT32(s, header->FunctionId);
	return TRUE;
}

BOOL rdpexps_write_response_header(wStream* s, const RDPEXPS_REQUEST_HEADER* request)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(request);
	if (!Stream_EnsureRemainingCapacity(s, 8))
		return FALSE;
	Stream_Write_UINT32(s, request->InterfaceId);
	Stream_Write_UINT32(s, request->MessageId);
	return TRUE;
}
