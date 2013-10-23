
#include <winpr/crt.h>

#include <winpr/nt.h>

int TestNtCreateFile(int argc, char* argv[])
{
	HANDLE handle;
	NTSTATUS ntstatus;
	ULONG CreateOptions;
	ANSI_STRING aString;
	UNICODE_STRING uString;
	ULONG CreateDisposition;
	ACCESS_MASK DesiredAccess = 0;
	OBJECT_ATTRIBUTES attributes;
	IO_STATUS_BLOCK ioStatusBlock;

	_RtlInitAnsiString(&aString, "\\??\\C:\\Users\\Public\\foo.txt");
	_RtlAnsiStringToUnicodeString(&uString, &aString, TRUE);

	handle = NULL;
	ZeroMemory(&ioStatusBlock, sizeof(IO_STATUS_BLOCK));

	_InitializeObjectAttributes(&attributes, &uString, 0, NULL, NULL);

	DesiredAccess = GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE;
	CreateOptions = FILE_DIRECTORY_FILE | FILE_WRITE_THROUGH;
	CreateDisposition = FILE_OVERWRITE_IF;

	ntstatus = _NtCreateFile(&handle, DesiredAccess, &attributes, &ioStatusBlock,
		0, 0, CreateDisposition, CreateOptions, 0, 0, 0);

	if (ntstatus != STATUS_SUCCESS)
	{
		printf("NtCreateFile failure: 0x%04X\n", ntstatus);
		return -1;
	}

	_RtlFreeUnicodeString(&uString);

	ntstatus = _NtClose(handle);

	if (ntstatus != STATUS_SUCCESS)
	{
		printf("NtClose failure: 0x%04X\n", ntstatus);
		return -1;
	}

	return 0;
}
