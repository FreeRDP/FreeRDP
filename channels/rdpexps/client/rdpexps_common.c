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

BOOL rdpexps_read_xml_document(wStream* s, RDPEXPS_XML_DOCUMENT* document)
{
	UINT32 length = 0;

	WINPR_ASSERT(s);
	WINPR_ASSERT(document);
	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;
	Stream_Read_UINT32(s, length);
	if (Stream_GetRemainingLength(s) < length)
		return FALSE;
	document->data = Stream_Pointer(s);
	document->length = length;
	Stream_Seek(s, length);
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

BOOL rdpexps_write_ticket_response(wStream* s, const RDPEXPS_REQUEST_HEADER* request)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(request);
	if (!rdpexps_write_response_header(s, request))
		return FALSE;

	switch (request->FunctionId)
	{
		case 0x00000100: /* GET_SUPPORTED_VERSIONS */
			Stream_Write_UINT32(s, 1);
			Stream_Write_UINT32(s, 1);
			break;
		case 0x00000101: /* BIND_PRINTER */
			Stream_Write_UINT32(s, 0);
			Stream_Write_UINT32(s, 0);
			Stream_Write_UINT32(s, 0);
			break;
		case 0x00000102: /* QUERY_DEV_NS */
			Stream_Write_UINT8(s, 1);
			break;
		default:
			return FALSE;
	}
	Stream_Write_UINT32(s, 0); /* S_OK */
	return TRUE;
}

BOOL rdpexps_write_driver_response(wStream* s, const RDPEXPS_REQUEST_HEADER* request)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(request);
	if (!rdpexps_write_response_header(s, request))
		return FALSE;
	if (request->FunctionId == 0x00000101) /* GET_ALL_DEV_CAPS */
		Stream_Write_UINT32(s, 0);
	Stream_Write_UINT32(s, 0); /* S_OK */
	return TRUE;
}

BOOL rdpexps_write_ticket_not_implemented_response(const RDPEXPS_REQUEST_HEADER* request,
                                                   wStream* s)
{
	WINPR_ASSERT(request);
	WINPR_ASSERT(s);
	if (!rdpexps_write_response_header(s, request))
		return FALSE;

	switch (request->FunctionId)
	{
		case 0x00000103:               /* PRINT_TKT_TO_DEVMODE */
			Stream_Write_UINT32(s, 0); /* cbDevmodeOut */
			break;
		case 0x00000104:              /* DEVMODE_TO_PRINT_TKT */
		case 0x00000105:              /* PRINT_CAPS */
		case 0x00000106:              /* PRINT_CAPS_FROM_PRINT_TKT */
		case 0x00000107:              /* VALIDATE_PRINT_TKT */
			Stream_Write_UINT8(s, 1); /* optional XML document is absent */
			break;
		default:
			return FALSE;
	}
	Stream_Write_UINT32(s, 0x80004001U); /* E_NOTIMPL */
	return TRUE;
}

BOOL rdpexps_write_driver_not_implemented_response(const RDPEXPS_REQUEST_HEADER* request,
                                                   wStream* s)
{
	WINPR_ASSERT(request);
	WINPR_ASSERT(s);
	if ((request->FunctionId != 0x00000103) || !rdpexps_write_response_header(s, request))
		return FALSE;
	Stream_Write_UINT32(s, 0);           /* cbDevmodeOut */
	Stream_Write_UINT32(s, 0x80004001U); /* E_NOTIMPL */
	return TRUE;
}

BOOL rdpexps_write_xml_response(wStream* s, const RDPEXPS_REQUEST_HEADER* request,
                                const RDPEXPS_XML_DOCUMENT* document)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(request);
	WINPR_ASSERT(document);
	if (!rdpexps_write_response_header(s, request) ||
	    !Stream_EnsureRemainingCapacity(s, 1ULL + 4ULL + document->length + 4ULL))
		return FALSE;
	Stream_Write_UINT8(s, 0); /* XML document is present */
	Stream_Write_UINT32(s, document->length);
	Stream_Write(s, document->data, document->length);
	Stream_Write_UINT32(s, 0); /* S_OK */
	return TRUE;
}
