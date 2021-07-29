
#include <winpr/io.h>
#include <winpr/nt.h>
#include <winpr/crt.h>
#include <winpr/windows.h>

int TestIoDevice(int argc, char* argv[])
{
#ifndef _WIN32
	NTSTATUS NtStatus;
	ANSI_STRING aString;
	UNICODE_STRING uString;
	PDEVICE_OBJECT_EX pDeviceObject = NULL;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	_RtlInitAnsiString(&aString, "\\Device\\Example");
	_RtlAnsiStringToUnicodeString(&uString, &aString, TRUE);

	NtStatus = _IoCreateDeviceEx(NULL, 0, &uString, FILE_DEVICE_UNKNOWN, 0, FALSE, &pDeviceObject);

	if (NtStatus != STATUS_SUCCESS)
		return -1;

	_IoDeleteDeviceEx(pDeviceObject);

	_RtlFreeUnicodeString(&uString);
#endif
	return 0;
}
