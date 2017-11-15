
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
	int result = -1;
	_RtlInitAnsiString(&aString, TESTFILE);
	ntstatus = _RtlAnsiStringToUnicodeString(&uString, &aString, TRUE);

	if (ntstatus != STATUS_SUCCESS)
	{
		printf("_RtlAnsiStringToUnicodeString failure: 0x%08"PRIX32"\n", ntstatus);
		goto out;
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
		printf("_NtCreateFile failure: 0x%08"PRIX32"\n", ntstatus);
		goto out;
	}

	ntstatus = _NtClose(handle);

	if (ntstatus != STATUS_SUCCESS)
	{
		printf("_NtClose failure: 0x%08"PRIX32"\n", ntstatus);
		goto out;
	}

	result = 0;
out:
	_RtlFreeUnicodeString(&uString);
#ifndef _WIN32

	if (result == 0)
	{
		printf("%s: Error, this test is currently expected not to succeed on this platform.\n",
		       __FUNCTION__);
		result = -1;
	}
	else
	{
		printf("%s: This test is currently expected to fail on this platform.\n",
		       __FUNCTION__);
		result = 0;
	}

#endif
	return result;
}
