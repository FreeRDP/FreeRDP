
#include <winpr/crt.h>

#include <winpr/nt.h>

int TestNtCreateFile(int argc, char* argv[])
{
	HANDLE handle;
	NTSTATUS ntstatus;
	ULONG CreateOptions;
	ANSI_STRING aString;
	UNICODE_STRING uString;
	ACCESS_MASK DesiredAccess = 0;
	OBJECT_ATTRIBUTES attributes;
	IO_STATUS_BLOCK ioStatusBlock;

	_RtlInitAnsiString(&aString, "\\Device\\FreeRDP\\TEST");
	_RtlAnsiStringToUnicodeString(&uString, &aString, TRUE);

	_InitializeObjectAttributes(&attributes, &uString, 0, NULL, NULL);

	DesiredAccess = GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE;
	CreateOptions = FILE_DIRECTORY_FILE | FILE_WRITE_THROUGH;

	ntstatus = _NtCreateFile(&handle, DesiredAccess, &attributes, &ioStatusBlock, 0, 0, 0, CreateOptions, 0, 0, 0);

	_RtlFreeUnicodeString(&uString);

	return 0;
}
