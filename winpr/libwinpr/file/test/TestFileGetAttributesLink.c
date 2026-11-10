#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/path.h>
#include <winpr/sysinfo.h>
#include <winpr/thread.h>

/* GetFileAttributes on a symbolic link: the link is a reparse point, and a link to a
 * directory is a directory too, as on Windows (drive redirection opens it as one) */
WINPR_ATTR_NODISCARD
static BOOL check(const char* path, BOOL directory, BOOL reparse)
{
	const DWORD attr = GetFileAttributesA(path);
	if (attr == INVALID_FILE_ATTRIBUTES)
	{
		(void)fprintf(stderr, "%s: no attributes\n", path);
		return FALSE;
	}
	const BOOL isDir = (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
	const BOOL isReparse = (attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
	if ((isDir != directory) || (isReparse != reparse))
	{
		(void)fprintf(stderr, "%s: attributes 0x%08" PRIx32 ", want directory %d reparse %d\n",
		              path, attr, directory, reparse);
		return FALSE;
	}
	return TRUE;
}

int TestFileGetAttributesLink(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	int rc = -1;
	char name[64] = WINPR_C_ARRAY_INIT;
	(void)_snprintf(name, sizeof(name), "TestFileGetAttributesLink-%" PRIu32,
	                GetCurrentProcessId());
	char* base = GetKnownSubPath(KNOWN_PATH_TEMP, name);
	char* dir = nullptr;
	char* file = nullptr;
	char* linkDir = nullptr;
	char* linkFile = nullptr;
	char* dangling = nullptr;
	if (!base)
		goto out;

	dir = GetCombinedPath(base, "dir");
	file = GetCombinedPath(base, "file");
	linkDir = GetCombinedPath(base, "link-to-dir");
	linkFile = GetCombinedPath(base, "link-to-file");
	dangling = GetCombinedPath(base, "link-to-nothing");
	if (!dir || !file || !linkDir || !linkFile || !dangling)
		goto out;

	if ((mkdir(base, 0700) != 0) || (mkdir(dir, 0700) != 0))
		goto out;
	FILE* fp = winpr_fopen(file, "w");
	if (!fp)
		goto out;
	(void)fclose(fp);
	if ((symlink(dir, linkDir) != 0) || (symlink(file, linkFile) != 0) ||
	    (symlink("does-not-exist", dangling) != 0))
		goto out;

	if (!check(dir, TRUE, FALSE) || !check(file, FALSE, FALSE) || !check(linkDir, TRUE, TRUE) ||
	    !check(linkFile, FALSE, TRUE) || !check(dangling, FALSE, TRUE))
		goto out;

	rc = 0;
out:
	if (base && !winpr_RemoveDirectory_RecursiveA(base))
		(void)fprintf(stderr, "cannot remove %s\n", base);
	free(dangling);
	free(linkFile);
	free(linkDir);
	free(file);
	free(dir);
	free(base);
	return rc;
}
