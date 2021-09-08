
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/path.h>
#include <winpr/handle.h>
#include <winpr/windows.h>
#include <winpr/sysinfo.h>

static const DWORD allflags[] = {
	0,
	FILE_ATTRIBUTE_READONLY,
	FILE_ATTRIBUTE_HIDDEN,
	FILE_ATTRIBUTE_SYSTEM,
	FILE_ATTRIBUTE_DIRECTORY,
	FILE_ATTRIBUTE_ARCHIVE,
	FILE_ATTRIBUTE_DEVICE,
	FILE_ATTRIBUTE_NORMAL,
	FILE_ATTRIBUTE_TEMPORARY,
	FILE_ATTRIBUTE_SPARSE_FILE,
	FILE_ATTRIBUTE_REPARSE_POINT,
	FILE_ATTRIBUTE_COMPRESSED,
	FILE_ATTRIBUTE_OFFLINE,
	FILE_ATTRIBUTE_NOT_CONTENT_INDEXED,
	FILE_ATTRIBUTE_ENCRYPTED,
	FILE_ATTRIBUTE_VIRTUAL,
	FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM,
	FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_DEVICE |
	    FILE_ATTRIBUTE_NORMAL,
	FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_SPARSE_FILE | FILE_ATTRIBUTE_REPARSE_POINT |
	    FILE_ATTRIBUTE_COMPRESSED | FILE_ATTRIBUTE_OFFLINE,
	FILE_ATTRIBUTE_NOT_CONTENT_INDEXED | FILE_ATTRIBUTE_ENCRYPTED | FILE_ATTRIBUTE_VIRTUAL
};

static BOOL test_SetFileAttributesA(void)
{
	BOOL rc = FALSE;
	HANDLE handle;
	DWORD x;
	const DWORD flags[] = { 0, FILE_ATTRIBUTE_READONLY };
	char* name = GetKnownSubPath(KNOWN_PATH_TEMP, "afsklhjwe4oq5iu432oijrlkejadlkhjaklhfdkahfd");
	if (!name)
		goto fail;

	for (x = 0; x < ARRAYSIZE(allflags); x++)
	{
		const DWORD flag = allflags[x];
		rc = SetFileAttributesA(NULL, flag);
		if (rc)
			goto fail;

		rc = SetFileAttributesA(name, flag);
		if (rc)
			goto fail;
	}

	handle = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_NEW,
	                     FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE)
		goto fail;
	CloseHandle(handle);

	for (x = 0; x < ARRAYSIZE(flags); x++)
	{
		DWORD attr;
		const DWORD flag = flags[x];
		rc = SetFileAttributesA(name, flag);
		if (!rc)
			goto fail;

		attr = GetFileAttributesA(name);
		if (flag != 0)
		{
			if ((attr & flag) == 0)
				goto fail;
		}
	}

fail:
	DeleteFileA(name);
	free(name);
	return rc;
}

static BOOL test_SetFileAttributesW(void)
{
	BOOL rc = FALSE;
	WCHAR* name = NULL;
	HANDLE handle;
	DWORD x;
	const DWORD flags[] = { 0, FILE_ATTRIBUTE_READONLY };
	char* base = GetKnownSubPath(KNOWN_PATH_TEMP, "afsklhjwe4oq5iu432oijrlkejadlkhjaklhfdkahfd");
	if (!base)
		goto fail;

	ConvertToUnicode(CP_UTF8, 0, base, -1, &name, 0);

	for (x = 0; x < ARRAYSIZE(allflags); x++)
	{
		const DWORD flag = allflags[x];
		rc = SetFileAttributesW(NULL, flag);
		if (rc)
			goto fail;

		rc = SetFileAttributesW(name, flag);
		if (rc)
			goto fail;
	}

	handle = CreateFileW(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_NEW,
	                     FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE)
		goto fail;
	CloseHandle(handle);

	for (x = 0; x < ARRAYSIZE(flags); x++)
	{
		DWORD attr;
		const DWORD flag = flags[x];
		rc = SetFileAttributesW(name, flag);
		if (!rc)
			goto fail;

		attr = GetFileAttributesW(name);
		if (flag != 0)
		{
			if ((attr & flag) == 0)
				goto fail;
		}
	}

fail:
	DeleteFileW(name);
	free(name);
	free(base);
	return rc;
}

int TestSetFileAttributes(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!test_SetFileAttributesA())
		return -1;
	if (!test_SetFileAttributesW())
		return -1;
	return 0;
}
