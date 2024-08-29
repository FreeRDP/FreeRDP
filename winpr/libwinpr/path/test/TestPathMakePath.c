#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <winpr/crt.h>
#include <winpr/crypto.h>
#include <winpr/file.h>
#include <winpr/path.h>

static UINT32 prand(UINT32 max)
{
	UINT32 tmp = 0;
	if (max <= 1)
		return 1;
	winpr_RAND(&tmp, sizeof(tmp));
	return tmp % (max - 1) + 1;
}

int TestPathMakePath(int argc, char* argv[])
{
	size_t baseLen = 0;
	BOOL success = 0;
	char tmp[64] = { 0 };
	char* path = NULL;
	char* cur = NULL;
	char delim = PathGetSeparatorA(0);
	char* base = GetKnownPath(KNOWN_PATH_TEMP);

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!base)
	{
		(void)fprintf(stderr, "Failed to get temporary directory!\n");
		return -1;
	}

	baseLen = strlen(base);

	for (int x = 0; x < 5; x++)
	{
		(void)sprintf_s(tmp, ARRAYSIZE(tmp), "%08" PRIX32, prand(UINT32_MAX));
		path = GetCombinedPath(base, tmp);
		free(base);

		if (!path)
		{
			(void)fprintf(stderr, "GetCombinedPath failed!\n");
			return -1;
		}

		base = path;
	}

	printf("Creating path %s\n", path);
	success = winpr_PathMakePath(path, NULL);

	if (!success)
	{
		(void)fprintf(stderr, "MakePath failed!\n");
		free(path);
		return -1;
	}

	success = winpr_PathFileExists(path);

	if (!success)
	{
		(void)fprintf(stderr, "MakePath lied about success!\n");
		free(path);
		return -1;
	}

	while (strlen(path) > baseLen)
	{
		if (!winpr_RemoveDirectory(path))
		{
			(void)fprintf(stderr, "winpr_RemoveDirectory %s failed!\n", path);
			free(path);
			return -1;
		}

		cur = strrchr(path, delim);

		if (cur)
			*cur = '\0';
	}

	free(path);
	printf("%s success!\n", __func__);
	return 0;
}
