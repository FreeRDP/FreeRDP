
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/thread.h>

const char* test_args_line_1 = "abc d e";

const char* test_args_list_1[] =
{
	"abc",
	"d",
	"e",
	NULL
};

const char* test_args_line_2 = "abc  \t   def";

const char* test_args_list_2[] =
{
	"abc",
	"def",
	NULL
};

const char* test_args_line_3 = "\"abc\" d e";

const char* test_args_list_3[] =
{
	"abc",
	"d",
	"e",
	NULL
};

const char* test_args_line_4 = "a\\\\b d\"e f\"g h";

const char* test_args_list_4[] =
{
	"a\\\\b",
	"de fg",
	"h",
	NULL
};

const char* test_args_line_5 = "a\\\\\\\"b c d";

const char* test_args_list_5[] =
{
	"a\\\"b",
	"c",
	"d",
	NULL
};

const char* test_args_line_6 = "a\\\\\\\\\"b c\" d e";

const char* test_args_list_6[] =
{
	"a\\\\b c",
	"d",
	"e",
	NULL
};

const char* test_args_line_7 = "a\\\\\\\\\"b c\" d e f\\\\\\\\\"g h\" i j";

const char* test_args_list_7[] =
{
	"a\\\\b c",
	"d",
	"e",
	"f\\\\g h",
	"i",
	"j",
	NULL
};

static int test_command_line_parsing_case(const char* line, const char** list)
{
	int i;
	LPSTR* pArgs;
	int pNumArgs;

	pArgs = NULL;
	pNumArgs = 0;

	printf("Parsing: %s\n", line);

	pArgs = CommandLineToArgvA(line, &pNumArgs);

	printf("pNumArgs: %d\n", pNumArgs);

	for (i = 0; i < pNumArgs; i++)
	{
		printf("argv[%d] = %s\n", i, pArgs[i]);
	}

	HeapFree(GetProcessHeap(), 0, pArgs);

	return 0;
}

int TestThreadCommandLineToArgv(int argc, char* argv[])
{
	test_command_line_parsing_case(test_args_line_1, test_args_list_1);
	test_command_line_parsing_case(test_args_line_2, test_args_list_2);
	test_command_line_parsing_case(test_args_line_3, test_args_list_3);
	test_command_line_parsing_case(test_args_line_4, test_args_list_4);
	test_command_line_parsing_case(test_args_line_5, test_args_list_5);
	test_command_line_parsing_case(test_args_line_6, test_args_list_6);
	test_command_line_parsing_case(test_args_line_7, test_args_list_7);

	return 0;
}

