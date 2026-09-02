#include <winpr/stream.h>
#include <freerdp/channels/scard.h>
#include <freerdp/utils/smartcard_operations.h>

int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	SMARTCARD_OPERATION operation = WINPR_C_ARRAY_INIT;
	wStream sbuffer = WINPR_C_ARRAY_INIT;

	wStream* s = Stream_StaticConstInit(&sbuffer, Data, Size);
	if (!s)
		return 0;

	/* Decodes the Device Control Request header, then dispatches on the
	 * IoControlCode into the matching smartcard_unpack_*_call(). */
	(void)smartcard_irp_device_control_decode_request(s, 1, 0, &operation);
	smartcard_operation_free(&operation, FALSE);

	return 0;
}
