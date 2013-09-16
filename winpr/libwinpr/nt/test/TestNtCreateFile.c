
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

	RtlInitAnsiString(&aString, "\\Device\\FreeRDP\\TEST");
	RtlAnsiStringToUnicodeString(&uString, &aString, TRUE);

	InitializeObjectAttributes(&attributes, NULL, 0, NULL, NULL);

	DesiredAccess = GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE;
	CreateOptions = FILE_DIRECTORY_FILE | FILE_WRITE_THROUGH;

	ntstatus = NtCreateFile(&handle, DesiredAccess, &attributes, &ioStatusBlock, 0, 0, 0, CreateOptions, 0, 0, 0);

	RtlFreeUnicodeString(&uString);

	return 0;
}
