/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * libFuzzer harness for cliprdr PDU parsing
 */

#include <stddef.h>
#include <stdint.h>

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/wlog.h>

#include <freerdp/channels/cliprdr.h>

#include "../../cliprdr_common.h"

static wLog* g_Log = nullptr;

static UINT32 fuzz_stream_len_u32(wStream* s)
{
	const size_t remaining = Stream_GetRemainingLength(s);
	return (remaining > UINT32_MAX) ? UINT32_MAX : (UINT32)remaining;
}

static void fuzz_format_list(wStream* s, BOOL longNames)
{
	CLIPRDR_FORMAT_LIST list = WINPR_C_ARRAY_INIT;
	list.common.dataLen = fuzz_stream_len_u32(s);

	if (cliprdr_read_format_list(g_Log, s, &list, longNames) == CHANNEL_RC_OK)
		cliprdr_free_format_list(&list);
}

static void fuzz_format_data_request(wStream* s)
{
	CLIPRDR_FORMAT_DATA_REQUEST request = WINPR_C_ARRAY_INIT;
	request.common.dataLen = fuzz_stream_len_u32(s);
	(void)cliprdr_read_format_data_request(s, &request);
}

static void fuzz_format_data_response(wStream* s)
{
	CLIPRDR_FORMAT_DATA_RESPONSE response = WINPR_C_ARRAY_INIT;
	response.common.dataLen = fuzz_stream_len_u32(s);
	(void)cliprdr_read_format_data_response(s, &response);
}

static void fuzz_file_contents_request(wStream* s)
{
	CLIPRDR_FILE_CONTENTS_REQUEST request = WINPR_C_ARRAY_INIT;
	request.common.dataLen = fuzz_stream_len_u32(s);
	(void)cliprdr_read_file_contents_request(s, &request);
}

static void fuzz_file_contents_response(wStream* s)
{
	CLIPRDR_FILE_CONTENTS_RESPONSE response = WINPR_C_ARRAY_INIT;
	const UINT32 dataLen = fuzz_stream_len_u32(s);

	response.common.dataLen = (dataLen >= 4) ? dataLen : 4;
	(void)cliprdr_read_file_contents_response(s, &response);
}

static void fuzz_unlock(wStream* s)
{
	CLIPRDR_UNLOCK_CLIPBOARD_DATA data = WINPR_C_ARRAY_INIT;
	data.common.dataLen = fuzz_stream_len_u32(s);
	(void)cliprdr_read_unlock_clipdata(s, &data);
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size < 2)
		return 0;
	if (size > (1u << 20))
		return 0;

	if (!g_Log)
		g_Log = WLog_Get("fuzz.cliprdr");

	BOOL longNames = (data[1] & 0x1) != 0;
	const uint8_t* body = data + 2;

	wStream buffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticConstInit(&buffer, body, size - 2);
	if (!s)
		return 0;

	switch (data[0] % 6)
	{
		case 0:
			fuzz_format_list(s, longNames);
			break;
		case 1:
			fuzz_format_data_request(s);
			break;
		case 2:
			fuzz_format_data_response(s);
			break;
		case 3:
			fuzz_file_contents_request(s);
			break;
		case 4:
			fuzz_file_contents_response(s);
			break;
		case 5:
			fuzz_unlock(s);
			break;
		default:
			break;
	}

	return 0;
}
