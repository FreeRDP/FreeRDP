
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/thread.h>

const char* test_args_line_1 = "app.exe abc d e";

const char* test_args_list_1[] = { "app.exe", "abc", "d", "e", NULL };

const char* test_args_line_2 = "app.exe abc  \t   def";

const char* test_args_list_2[] = { "app.exe", "abc", "def", NULL };

const char* test_args_line_3 = "app.exe \"abc\" d e";

const char* test_args_list_3[] = { "app.exe", "abc", "d", "e", NULL };

const char* test_args_line_4 = "app.exe a\\\\b d\"e f\"g h";

const char* test_args_list_4[] = { "app.exe", "a\\\\b", "de fg", "h", NULL };

const char* test_args_line_5 = "app.exe a\\\\\\\"b c d";

const char* test_args_list_5[] = { "app.exe", "a\\\"b", "c", "d", NULL };

const char* test_args_line_6 = "app.exe a\\\\\\\\\"b c\" d e";

const char* test_args_list_6[] = { "app.exe", "a\\\\b c", "d", "e", NULL };

const char* test_args_line_7 = "app.exe a\\\\\\\\\"b c\" d e f\\\\\\\\\"g h\" i j";

const char* test_args_list_7[] = { "app.exe", "a\\\\b c", "d", "e", "f\\\\g h", "i", "j", NULL };

static int test_command_line_parsing_case(const char* line, const char** list)
{
	int i;
	LPSTR* pArgs;
	int numArgs;

	pArgs = NULL;
	numArgs = 0;

	printf("Parsing: %s\n", line);

	pArgs = CommandLineToArgvA(line, &numArgs);

	printf("pNumArgs: %d\n", numArgs);

	for (i = 0; i < numArgs; i++)
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
