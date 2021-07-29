
#include <stdio.h>
#include <stdlib.h>
#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/windows.h>

int TestFileDeleteFile(int argc, char* argv[])
{
	BOOL rc;
	int fd;
	char validA[] = "/tmp/valid-test-file-XXXXXX";
	char validW[] = "/tmp/valid-test-file-XXXXXX";
	WCHAR* validWW = NULL;
	const char* invalidA = "/xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
	const WCHAR invalidW[] = { '/', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x',
		                       'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x',
		                       'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x',
		                       'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', '\0' };

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	rc = DeleteFileA(invalidA);
	if (rc)
		return -1;

	rc = DeleteFileW(invalidW);
	if (rc)
		return -1;

	fd = mkstemp(validA);
	if (fd < 0)
		return -1;

	rc = DeleteFileA(validA);
	if (!rc)
		return -1;

	fd = mkstemp(validW);
	if (fd < 0)
		return -1;

	ConvertToUnicode(CP_UTF8, 0, validW, -1, &validWW, 0);
	rc = DeleteFileW(validWW);
	free(validWW);
	if (!rc)
		return -1;
	return 0;
}
