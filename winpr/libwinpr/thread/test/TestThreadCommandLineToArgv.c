
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

	return 0;
}

int TestThreadCommandLineToArgv(int argc, char* argv[])
{
	test_command_line_parsing_case(test_args_line_1, test_args_list_1);
	test_command_line_parsing_case(test_args_line_2, test_args_list_2);
	test_command_line_parsing_case(test_args_line_3, test_args_list_3);
	test_command_line_parsing_case(test_args_line_4, test_args_list_4);
	//test_command_line_parsing_case(test_args_line_5, test_args_list_5);

	return 0;
}

