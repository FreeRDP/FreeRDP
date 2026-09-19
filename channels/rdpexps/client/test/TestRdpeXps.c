#include <winpr/stream.h>

#include "../rdpexps_common.h"

int TestRdpeXps(int argc, char* argv[])
{
	RDPEXPS_REQUEST_HEADER request = { 0x12345678, 0x87654321, 0x00000002 };
	RDPEXPS_REQUEST_HEADER actual = WINPR_C_ARRAY_INIT;
	wStream* input = nullptr;
	wStream* output = nullptr;
	RDPEXPS_XML_DOCUMENT document = WINPR_C_ARRAY_INIT;
	static const BYTE xml[] = "<PrintTicket/>";
	int rc = -1;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	input = Stream_New(nullptr, 12);
	output = Stream_New(nullptr, 32);
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
	request.FunctionId = 0x00000102;
	if (!Stream_SetPosition(output, 0) || !rdpexps_write_ticket_response(output, &request))
		goto out;
	if ((Stream_GetPosition(output) != 13) || !Stream_SetPosition(output, 8) ||
	    (Stream_Get_UINT8(output) != 1) || (Stream_Get_UINT32(output) != 0))
		goto out;
	request.FunctionId = 0x00000100;
	if (!Stream_SetPosition(output, 0) || !rdpexps_write_ticket_response(output, &request))
		goto out;
	if ((Stream_GetPosition(output) != 20) || !Stream_SetPosition(output, 8) ||
	    (Stream_Get_UINT32(output) != 1) || (Stream_Get_UINT32(output) != 1) ||
	    (Stream_Get_UINT32(output) != 0))
		goto out;
	request.FunctionId = 0x00000101;
	if (!Stream_SetPosition(output, 0) || !rdpexps_write_driver_response(output, &request))
		goto out;
	if ((Stream_GetPosition(output) != 16) || !Stream_SetPosition(output, 8) ||
	    (Stream_Get_UINT32(output) != 0) || (Stream_Get_UINT32(output) != 0))
		goto out;
	request.FunctionId = 0x00000103;
	if (!Stream_SetPosition(output, 0) ||
	    !rdpexps_write_ticket_not_implemented_response(&request, output))
		goto out;
	if ((Stream_GetPosition(output) != 16) || !Stream_SetPosition(output, 8) ||
	    (Stream_Get_UINT32(output) != 0) || (Stream_Get_UINT32(output) != 0x80004001U))
		goto out;
	request.FunctionId = 0x00000107;
	if (!Stream_SetPosition(output, 0) ||
	    !rdpexps_write_ticket_not_implemented_response(&request, output))
		goto out;
	if ((Stream_GetPosition(output) != 13) || !Stream_SetPosition(output, 8) ||
	    (Stream_Get_UINT8(output) != 1) || (Stream_Get_UINT32(output) != 0x80004001U))
		goto out;
	request.FunctionId = 0x00000103;
	if (!Stream_SetPosition(output, 0) ||
	    !rdpexps_write_driver_not_implemented_response(&request, output))
		goto out;
	if ((Stream_GetPosition(output) != 16) || !Stream_SetPosition(output, 8) ||
	    (Stream_Get_UINT32(output) != 0) || (Stream_Get_UINT32(output) != 0x80004001U))
		goto out;
	if (!Stream_SetPosition(input, 0) || !Stream_SetLength(input, 0) ||
	    !Stream_EnsureRemainingCapacity(input, 4 + ARRAYSIZE(xml) - 1))
		goto out;
	Stream_Write_UINT32(input, ARRAYSIZE(xml) - 1);
	Stream_Write(input, xml, ARRAYSIZE(xml) - 1);
	if (!Stream_SetPosition(input, 0) || !rdpexps_read_xml_document(input, &document) ||
	    (document.length != ARRAYSIZE(xml) - 1) ||
	    (memcmp(document.data, xml, ARRAYSIZE(xml) - 1) != 0))
		goto out;
	request.FunctionId = 0x00000107;
	if (!Stream_SetPosition(output, 0) || !rdpexps_write_xml_response(output, &request, &document))
		goto out;
	if ((Stream_GetPosition(output) != 17 + ARRAYSIZE(xml) - 1) || !Stream_SetPosition(output, 8) ||
	    (Stream_Get_UINT8(output) != 0) || (Stream_Get_UINT32(output) != ARRAYSIZE(xml) - 1) ||
	    (memcmp(Stream_Pointer(output), xml, ARRAYSIZE(xml) - 1) != 0))
		goto out;
	Stream_Seek(output, ARRAYSIZE(xml) - 1);
	if (Stream_Get_UINT32(output) != 0)
		goto out;
	document.data = nullptr;
	if (rdpexps_write_xml_response(output, &request, &document))
		goto out;

	rc = 0;
out:
	Stream_Free(input, TRUE);
	Stream_Free(output, TRUE);
	return rc;
}
