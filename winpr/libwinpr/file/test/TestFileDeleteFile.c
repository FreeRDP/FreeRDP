
#include <stdio.h>
#include <stdlib.h>
#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/path.h>
#include <winpr/windows.h>

#if !defined(_WIN32)
#include <sys/stat.h>
#endif

static int secure_mkstemp(char* tmpname)
{
#if !defined(_WIN32)
	const mode_t mask = umask(S_IRWXU);
#endif
	int fd = mkstemp(tmpname);
#if !defined(_WIN32)
	(void)umask(mask);
#endif
	return fd;
}

int TestFileDeleteFile(int argc, char* argv[])
{
	BOOL rc = FALSE;
	int fd = 0;
	char validA[] = "/tmp/valid-test-file-XXXXXX";
	char validW[] = "/tmp/valid-test-file-XXXXXX";
	WCHAR* validWW = NULL;
	const char invalidA[] = "/xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
	WCHAR invalidW[sizeof(invalidA)] = { 0 };

	(void)ConvertUtf8NToWChar(invalidA, ARRAYSIZE(invalidA), invalidW, ARRAYSIZE(invalidW));

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	rc = winpr_DeleteFile(invalidA);
	if (rc)
		return -1;

	rc = DeleteFileW(invalidW);
	if (rc)
		return -1;

	fd = secure_mkstemp(validA);
	if (fd < 0)
		return -1;

	rc = winpr_DeleteFile(validA);
	if (!rc)
		return -1;

	fd = secure_mkstemp(validW);
	if (fd < 0)
		return -1;

	validWW = ConvertUtf8NToWCharAlloc(validW, ARRAYSIZE(validW), NULL);
	if (validWW)
		rc = DeleteFileW(validWW);
	free(validWW);
	if (!rc)
		return -1;
	return 0;
}
