
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/thread.h>

static const char* test_args_line_1 = "app.exe abc d e";

static const char* test_args_list_1[] = { "app.exe", "abc", "d", "e" };

static const char* test_args_line_2 = "app.exe abc  \t   def";

static const char* test_args_list_2[] = { "app.exe", "abc", "def" };

static const char* test_args_line_3 = "app.exe \"abc\" d e";

static const char* test_args_list_3[] = { "app.exe", "abc", "d", "e" };

static const char* test_args_line_4 = "app.exe a\\\\b d\"e f\"g h";

static const char* test_args_list_4[] = { "app.exe", "a\\\\b", "de fg", "h" };

static const char* test_args_line_5 = "app.exe a\\\\\\\"b c d";

static const char* test_args_list_5[] = { "app.exe", "a\\\"b", "c", "d" };

static const char* test_args_line_6 = "app.exe a\\\\\\\\\"b c\" d e";

static const char* test_args_list_6[] = { "app.exe", "a\\\\b c", "d", "e" };

static const char* test_args_line_7 = "app.exe a\\\\\\\\\"b c\" d e f\\\\\\\\\"g h\" i j";

static const char* test_args_list_7[] = { "app.exe", "a\\\\b c", "d", "e", "f\\\\g h", "i", "j" };

static BOOL test_command_line_parsing_case(const char* line, const char** list, size_t expect)
{
	BOOL rc = FALSE;
	int numArgs = 0;

	printf("Parsing: %s\n", line);

	LPSTR* pArgs = CommandLineToArgvA(line, &numArgs);
	if (numArgs < 0)
	{
		(void)fprintf(stderr, "expected %" PRIuz " arguments, got %d return\n", expect, numArgs);
		goto fail;
	}
	if (numArgs != expect)
	{
		(void)fprintf(stderr, "expected %" PRIuz " arguments, got %d from '%s'\n", expect, numArgs,
		              line);
		goto fail;
	}

	if ((numArgs > 0) && !pArgs)
	{
		(void)fprintf(stderr, "expected %d arguments, got NULL return\n", numArgs);
		goto fail;
	}

	printf("pNumArgs: %d\n", numArgs);

	for (int i = 0; i < numArgs; i++)
	{
		printf("argv[%d] = %s\n", i, pArgs[i]);
		if (strcmp(pArgs[i], list[i]) != 0)
		{
			(void)fprintf(stderr, "failed at argument %d: got '%s' but expected '%s'\n", i,
			              pArgs[i], list[i]);
			goto fail;
		}
	}

	rc = TRUE;
fail:
	free((void*)pArgs);

	return rc;
}

int TestThreadCommandLineToArgv(int argc, char* argv[])
{

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!test_command_line_parsing_case(test_args_line_1, test_args_list_1,
	                                    ARRAYSIZE(test_args_list_1)))
		return -1;
	if (!test_command_line_parsing_case(test_args_line_2, test_args_list_2,
	                                    ARRAYSIZE(test_args_list_2)))
		return -1;
	if (!test_command_line_parsing_case(test_args_line_3, test_args_list_3,
	                                    ARRAYSIZE(test_args_list_3)))
		return -1;
	if (!test_command_line_parsing_case(test_args_line_4, test_args_list_4,
	                                    ARRAYSIZE(test_args_list_4)))
		return -1;
	if (!test_command_line_parsing_case(test_args_line_5, test_args_list_5,
	                                    ARRAYSIZE(test_args_list_5)))
		return -1;
	if (!test_command_line_parsing_case(test_args_line_6, test_args_list_6,
	                                    ARRAYSIZE(test_args_list_6)))
		return -1;
	if (!test_command_line_parsing_case(test_args_line_7, test_args_list_7,
	                                    ARRAYSIZE(test_args_list_7)))
		return -1;

	return 0;
}
