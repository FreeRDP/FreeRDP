
#include <stdlib.h>
#include <string.h>

#include <winpr/wtypes.h>
#include <winpr/cmdline.h>
#include <winpr/path.h>
#include <winpr/file.h>
#include <winpr/json.h>

static bool run(const char* name, int argc, char* argv[], bool expectSuccess, const char* compare)
{
	size_t count = 0;

	printf("[%s] running test...\n", name);

	char* res = CommandLineToCommaSeparatedValues(argc, argv);
	if (!res)
	{
		if (expectSuccess)
			(void)fprintf(stderr, "[%s] CSV expected success, but failed\n", name);
		return !expectSuccess;
	}

	if (compare)
	{
		if (strcmp(compare, res) != 0)
		{
			(void)fprintf(stderr, "[%s] CSV compare fail:\ngot   : %s\nexpect: %s\n", name, res,
			              compare);
			return !expectSuccess;
		}
	}

	bool rc = false;
	if (argc >= 0)
	{
		char** ares = CommandLineParseCommaSeparatedValues(res, &count);

		if (count == WINPR_ASSERTING_INT_CAST(size_t, argc))
		{
			rc = true;

			for (size_t x = 0; x < count; x++)
			{
				const char* argvx = argv[x];
				const char* aresx = ares[x];

				if (strcmp(argvx, aresx) != 0)
				{
					(void)fprintf(stderr, "[%s] ARGV compare fail:\ngot   : %s\nexpect: %s\n", name,
					              argvx, aresx);
					rc = false;
				}
			}
		}

		CommandLineParserFree(ares);
	}
	free(res);
	return rc == expectSuccess;
}

static void TestCaseFree(int argc, char** argv)
{
	if (!argv)
		return;

	for (int i = 0; i < argc; i++)
	{
		free(argv[i]);
	}
	free((void*)argv);
}

static void usage(const char* file)
{
	WINPR_ASSERT(file);
	(void)fprintf(stderr, "Failed to parse test case '%s'\n", file);
	(void)fprintf(stderr,
	              "Test cases for TestCommandLineToCommaSeparatedValues should be JSON files\n");
	(void)fprintf(stderr, "placed in the folder '%s'\n", TEST_SOURCE_DIR);
	(void)fprintf(stderr, "with '.json' (case sensitive) as extension.\n");
	(void)fprintf(stderr, "\n");
	(void)fprintf(stderr, "The JSON should be of the following format:\n");
	(void)fprintf(stderr, "\n");
	(void)fprintf(stderr, "{\n");
	(void)fprintf(stderr, "\t\"expectSuccess\": true,\n");
	(void)fprintf(stderr, "{\t\"csv\": \"\\\"string1\\\",\\\"string2\\\",...\"\n");
	(void)fprintf(stderr, "\t\"argv\": [ \"string1\", \"string2\", null, \"string3\" ]\n");
	(void)fprintf(stderr, "}\n");
}

WINPR_ATTR_MALLOC(TestCaseFree, 2)
static char** getTestcase(const char* path, const char* filename, int* pargc, bool* pexpectSuccess,
                          char** compare)
{
	WINPR_ASSERT(path);
	WINPR_ASSERT(filename);
	WINPR_ASSERT(pargc);
	WINPR_ASSERT(pexpectSuccess);

	BOOL success = FALSE;
	int argc = 0;
	char** argv = NULL;
	char* fpath = GetCombinedPath(path, filename);
	if (!fpath)
	{
		(void)fprintf(stderr, "GetCombinedPath(%s, %s) failed\n", path, filename);
		usage(filename);
		return NULL;
	}

	WINPR_JSON* json = WINPR_JSON_ParseFromFile(fpath);
	if (!json)
	{
		(void)fprintf(stderr, "WINPR_JSON_ParseFromFile(%s) failed\n", fpath);
		usage(filename);
		free(fpath);
		return NULL;
	}
	free(fpath);

	if (!WINPR_JSON_IsObject(json))
	{
		(void)fprintf(stderr, "WINPR_JSON_IsObject() failed\n");
		goto fail;
	}

	/* optional compare string for CSV version */
	{
		WINPR_JSON* jexpect = WINPR_JSON_GetObjectItem(json, "csv");
		if (WINPR_JSON_IsString(jexpect))
		{
			const char* str = WINPR_JSON_GetStringValue(jexpect);
			if (!str)
			{
				(void)fprintf(stderr, "WINPR_JSON_GetStringValue(csv) failed\n");
				goto fail;
			}
			*compare = _strdup(str);
			if (!*compare)
			{
				(void)fprintf(stderr, "_strdup(csv[%s]) failed\n", str);
				goto fail;
			}
		}
		*pexpectSuccess = WINPR_JSON_IsTrue(jexpect);
	}

	{
		WINPR_JSON* jexpect = WINPR_JSON_GetObjectItem(json, "expectSuccess");
		if (!WINPR_JSON_IsBool(jexpect))
		{
			(void)fprintf(stderr, "WINPR_JSON_GetObjectItem(expectSuccess) failed\n");
			goto fail;
		}
		*pexpectSuccess = WINPR_JSON_IsTrue(jexpect);
	}

	{
		WINPR_JSON* jargv = WINPR_JSON_GetObjectItem(json, "argv");
		if (!WINPR_JSON_IsArray(jargv))
		{
			(void)fprintf(stderr, "WINPR_JSON_IsArray(argv) failed\n");
			goto fail;
		}

		const size_t len = WINPR_JSON_GetArraySize(jargv);
		if (len == 0)
		{
			(void)fprintf(stderr, "WINPR_JSON_GetArraySize(argv) == 0\n");
			goto fail;
		}

		argv = (char**)calloc(len, sizeof(char*));
		if (!argv)
		{
			(void)fprintf(stderr, "calloc(%" PRIuz ", sizeof(char*)) failed\n", len);
			goto fail;
		}

		argc = WINPR_ASSERTING_INT_CAST(int, len);
		for (size_t x = 0; x < len; x++)
		{
			WINPR_JSON* jstr = WINPR_JSON_GetArrayItem(jargv, x);
			if (WINPR_JSON_IsNull(jstr))
				continue;

			if (!WINPR_JSON_IsString(jstr))
			{
				(void)fprintf(stderr, "WINPR_JSON_IsString(%" PRIuz ") failed", x);
				goto fail;
			}
			const char* str = WINPR_JSON_GetStringValue(jstr);
			if (str)
			{
				argv[x] = _strdup(str);
				if (!argv[x])
				{
					(void)fprintf(stderr, "WINPR_JSON_GetStringValue(%s) _strdup() failed", str);
					goto fail;
				}
			}
		}
	}
	*pargc = argc;
	success = true;

fail:
	WINPR_JSON_Delete(json);
	if (success)
		return argv;

	usage(filename);
	TestCaseFree(argc, argv);
	return NULL;
}

static int runJsonTests(void)
{
	char testdir[PATH_MAX] = { 0 };
	(void)_snprintf(testdir, sizeof(testdir), "cmdline-tests%c*.json",
	                PathGetSeparatorA(PATH_STYLE_NATIVE));
	int rc = -1;
	char* path = GetCombinedPath(TEST_SOURCE_DIR, testdir);
	if (!path)
		return 0; /* No tests, ignore */

	WIN32_FIND_DATAA FindData = { 0 };
	HANDLE hFind = FindFirstFileA(path, &FindData);
	free(path);
	path = GetCombinedPath(TEST_SOURCE_DIR, "cmdline-tests");
	if (!path)
		goto fail;

	if (hFind == INVALID_HANDLE_VALUE)
	{
		printf("FindFirstFile failure: %s (INVALID_HANDLE_VALUE -1)\n", path);
		goto fail;
	}

	do
	{
		char* compare = NULL;
		int argc = 0;
		bool expectSuccess = false;
		char** argv = getTestcase(path, FindData.cFileName, &argc, &expectSuccess, &compare);
		if (!argv)
		{
			free(compare);
			(void)fprintf(stderr, "Test case '%s/%s': could not be parsed, aborting!\n", path,
			              FindData.cFileName);
			goto fail;
		}

		rc = run(FindData.cFileName, argc, argv, expectSuccess, compare);
		TestCaseFree(argc, argv);
		free(compare);
		if (rc < 0)
		{
			(void)fprintf(stderr, "Test case '%s/%s': test run failed, aborting!\n", path,
			              FindData.cFileName);
			goto fail;
		}
	} while (FindNextFileA(hFind, &FindData));

	rc = 0;

fail:
	FindClose(hFind);
	free(path);
	return rc;
}

int TestCommandLineToCommaSeparatedValues(int argc, char* argv[])
{
	if (argc > 1)
		return run("from-commandline", argc, argv, true, NULL);

	return runJsonTests();
}
