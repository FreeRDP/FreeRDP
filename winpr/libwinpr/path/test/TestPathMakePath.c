#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/path.h>

int TestPathMakePath(int argc, char* argv[])
{
	int x;
	size_t baseLen;
	BOOL success;
	char tmp[64] = { 0 };
	char* path = NULL;
	char* cur;
	char delim = PathGetSeparatorA(0);
	char* base = GetKnownPath(KNOWN_PATH_TEMP);

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!base)
	{
		fprintf(stderr, "Failed to get temporary directory!\n");
		return -1;
	}

	baseLen = strlen(base);
	srand(time(NULL));

	for (x = 0; x < 5; x++)
	{
		sprintf_s(tmp, ARRAYSIZE(tmp), "%08X", rand());
		path = GetCombinedPath(base, tmp);
		free(base);

		if (!path)
		{
			fprintf(stderr, "GetCombinedPath failed!\n");
			return -1;
		}

		base = path;
	}

	printf("Creating path %s\n", path);
	success = winpr_PathMakePath(path, NULL);

	if (!success)
	{
		fprintf(stderr, "MakePath failed!\n");
		free(path);
		return -1;
	}

	success = winpr_PathFileExists(path);

	if (!success)
	{
		fprintf(stderr, "MakePath lied about success!\n");
		free(path);
		return -1;
	}

	while (strlen(path) > baseLen)
	{
		if (!winpr_RemoveDirectory(path))
		{
			fprintf(stderr, "winpr_RemoveDirectory %s failed!\n", path);
			free(path);
			return -1;
		}

		cur = strrchr(path, delim);

		if (cur)
			*cur = '\0';
	}

	free(path);
	printf("%s success!\n", __FUNCTION__);
	return 0;
}
