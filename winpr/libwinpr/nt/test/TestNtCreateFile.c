
#include <winpr/crt.h>

#include <winpr/nt.h>

#ifdef _WIN32
#define TESTFILE "\\??\\C:\\Documents and Settings\\All Users\\winpr_test_nt_create_file.txt"
#else
#define TESTFILE "/tmp/winpr_test_nt_create_file.txt"
#endif

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

	int eFailure = -1;
	int eSuccess =  0;

#ifndef _WIN32
	printf("Note: %s result may currently only be trusted on Win32\n", __FUNCTION__);
	eFailure = eSuccess;
#endif

	_RtlInitAnsiString(&aString, TESTFILE);

	ntstatus = _RtlAnsiStringToUnicodeString(&uString, &aString, TRUE);
	if (ntstatus != STATUS_SUCCESS)
	{
		printf("_RtlAnsiStringToUnicodeString failure: 0x%08X\n", ntstatus);
		return eFailure;
	}

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
		printf("_NtCreateFile failure: 0x%08X\n", ntstatus);
		return eFailure;
	}

	_RtlFreeUnicodeString(&uString);

	ntstatus = _NtClose(handle);

	if (ntstatus != STATUS_SUCCESS)
	{
		printf("_NtClose failure: 0x%08X\n", ntstatus);
		return eFailure;
	}

	return eSuccess;
}
