/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * libFuzzer harness for DVC wire framing
 */

#include <stddef.h>
#include <stdint.h>

#include <winpr/stream.h>
#include <winpr/wtypes.h>

static UINT32 fuzz_var_uint_bytes(UINT8 cbLen)
{
	switch (cbLen)
	{
		case 0:
			return 1;
		case 1:
			return 2;
		default:
			return 4;
	}
}

static UINT32 fuzz_read_variable_uint(wStream* s, UINT8 cbLen)
{
	UINT32 value = 0;

	switch (cbLen)
	{
		case 0:
		{
			UINT8 value8 = 0;
			Stream_Read_UINT8(s, value8);
			value = value8;
			break;
		}
		case 1:
		{
			UINT16 value16 = 0;
			Stream_Read_UINT16(s, value16);
			value = value16;
			break;
		}
		default:
			Stream_Read_UINT32(s, value);
			break;
	}

	return value;
}

enum
{
	CREATE_REQUEST_PDU = 0x01,
	DATA_FIRST_PDU = 0x02,
	DATA_PDU = 0x03,
	CLOSE_REQUEST_PDU = 0x04,
	CAPABILITY_REQUEST_PDU = 0x05,
	DATA_FIRST_COMPRESSED_PDU = 0x06,
	DATA_COMPRESSED_PDU = 0x07
};

static int fuzz_process_one_pdu(wStream* s)
{
	const size_t required = 1;
	UINT8 header = 0;
	UINT8 command = 0;
	UINT8 spacing = 0;
	UINT8 cbChId = 0;
	size_t needed = 0;

	if (!Stream_CheckAndLogRequiredLength("fuzz", s, required))
		return -1;

	header = Stream_Get_UINT8(s);
	command = (header & 0xf0) >> 4;
	spacing = (header & 0x0c) >> 2;
	cbChId = (header & 0x03);
	needed = fuzz_var_uint_bytes(cbChId);

	switch (command)
	{
		case DATA_FIRST_PDU:
		case DATA_FIRST_COMPRESSED_PDU:
			if (!Stream_CheckAndLogRequiredLength("fuzz", s, needed + fuzz_var_uint_bytes(spacing)))
				return -1;
			(void)fuzz_read_variable_uint(s, cbChId);
			(void)fuzz_read_variable_uint(s, spacing);
			break;
		case DATA_PDU:
		case DATA_COMPRESSED_PDU:
		case CLOSE_REQUEST_PDU:
		case CREATE_REQUEST_PDU:
			if (!Stream_CheckAndLogRequiredLength("fuzz", s, needed))
				return -1;
			(void)fuzz_read_variable_uint(s, cbChId);
			break;
		case CAPABILITY_REQUEST_PDU:
			if (!Stream_CheckAndLogRequiredLength("fuzz", s, 2))
				return -1;
			Stream_Seek(s, 2);
			break;
		default:
			break;
	}

	if (Stream_GetRemainingLength(s) < 2)
		return 0;

	{
		UINT16 bodyLen = 0;
		const size_t remaining = Stream_GetRemainingLength(s);

		Stream_Read_UINT16(s, bodyLen);
		if (bodyLen > remaining)
			bodyLen = (UINT16)remaining;

		Stream_Seek(s, bodyLen);
	}

	return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	wStream* s = NULL;

	if ((size == 0) || (size > (1u << 20)))
		return 0;

	s = Stream_New((BYTE*)data, size);
	if (!s)
		return 0;

	for (size_t index = 0; index < 64; index++)
	{
		if (Stream_GetRemainingLength(s) == 0)
			break;
		if (fuzz_process_one_pdu(s) != 0)
			break;
	}

	Stream_Free(s, FALSE);
	return 0;
}
