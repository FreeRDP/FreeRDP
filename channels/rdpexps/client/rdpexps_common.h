/** FreeRDP: A Remote Desktop Protocol Implementation */
#ifndef FREERDP_CHANNEL_RDPEXPS_COMMON_H
#define FREERDP_CHANNEL_RDPEXPS_COMMON_H

#include <freerdp/types.h>
#include <winpr/stream.h>

typedef struct
{
	UINT32 InterfaceId;
	UINT32 MessageId;
	UINT32 FunctionId;
} RDPEXPS_REQUEST_HEADER;

WINPR_ATTR_NODISCARD BOOL rdpexps_read_request_header(wStream* s,
	                                                     RDPEXPS_REQUEST_HEADER* header);
WINPR_ATTR_NODISCARD BOOL rdpexps_write_response_header(wStream* s,
	                                                        const RDPEXPS_REQUEST_HEADER* request);
#endif
