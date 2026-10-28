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

typedef struct
{
	const BYTE* data;
	UINT32 length;
} RDPEXPS_XML_DOCUMENT;

WINPR_ATTR_NODISCARD BOOL rdpexps_read_request_header(wStream* s, RDPEXPS_REQUEST_HEADER* header);
WINPR_ATTR_NODISCARD BOOL rdpexps_write_response_header(wStream* s,
                                                        const RDPEXPS_REQUEST_HEADER* request);
WINPR_ATTR_NODISCARD BOOL rdpexps_write_ticket_response(wStream* s,
                                                        const RDPEXPS_REQUEST_HEADER* request);
WINPR_ATTR_NODISCARD BOOL rdpexps_write_driver_response(wStream* s,
                                                        const RDPEXPS_REQUEST_HEADER* request);
WINPR_ATTR_NODISCARD BOOL
rdpexps_write_ticket_not_implemented_response(const RDPEXPS_REQUEST_HEADER* request, wStream* s);
WINPR_ATTR_NODISCARD BOOL
rdpexps_write_driver_not_implemented_response(const RDPEXPS_REQUEST_HEADER* request, wStream* s);
WINPR_ATTR_NODISCARD BOOL rdpexps_read_xml_document(wStream* s, RDPEXPS_XML_DOCUMENT* document);
WINPR_ATTR_NODISCARD BOOL rdpexps_write_xml_response(wStream* s,
                                                     const RDPEXPS_REQUEST_HEADER* request,
                                                     const RDPEXPS_XML_DOCUMENT* document);
#endif
