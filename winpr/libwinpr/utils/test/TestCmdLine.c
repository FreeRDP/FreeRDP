
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/cmdline.h>

const char* testArgv[] =
{
	"mstsc.exe",
	"/w:1024",
	"/h:768",
	"/admin",
	"/multimon",
	"/v:localhost:3389"
};

COMMAND_LINE_ARGUMENT_A args[] =
{
	{ "w", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL },
	{ "h", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL },
	{ "f", COMMAND_LINE_VALUE_FLAG, NULL, NULL },
	{ "admin", COMMAND_LINE_VALUE_FLAG, NULL, "console" },
	{ "multimon", COMMAND_LINE_VALUE_FLAG, NULL, NULL },
	{ "v", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL },
	{ "?", COMMAND_LINE_VALUE_FLAG, NULL, "help" },
	{ NULL, 0, NULL, NULL }
};

#define testArgc (sizeof(testArgv) / sizeof(testArgv[0]))

int TestCmdLine(int argc, char* argv[])
{
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;

	flags = COMMAND_LINE_SIGIL_SLASH | COMMAND_LINE_SEPARATOR_COLON;
	status = CommandLineParseArgumentsA(testArgc, testArgv, args, flags);

	if (status != 0)
	{
		printf("CommandLineParseArgumentsA failure: %d\n", status);
		return -1;
	}

	arg = CommandLineFindArgumentA(args, "w");

	if (strcmp("1024", arg->Value) != 0)
	{
		printf("CommandLineFindArgumentA: unexpected %s value %s\n", arg->Name, arg->Value);
		return -1;
	}

	arg = CommandLineFindArgumentA(args, "h");

	if (strcmp("768", arg->Value) != 0)
	{
		printf("CommandLineFindArgumentA: unexpected %s value %s\n", arg->Name, arg->Value);
		return -1;
	}

	arg = CommandLineFindArgumentA(args, "f");

	if (arg->Value)
	{
		printf("CommandLineFindArgumentA: unexpected %s value\n", arg->Name);
		return -1;
	}

	arg = CommandLineFindArgumentA(args, "admin");

	if (!arg->Value)
	{
		printf("CommandLineFindArgumentA: unexpected %s value\n", arg->Name);
		return -1;
	}

	arg = CommandLineFindArgumentA(args, "multimon");

	if (!arg->Value)
	{
		printf("CommandLineFindArgumentA: unexpected %s value\n", arg->Name);
		return -1;
	}

	arg = CommandLineFindArgumentA(args, "v");

	if (strcmp("localhost:3389", arg->Value) != 0)
	{
		printf("CommandLineFindArgumentA: unexpected %s value %s\n", arg->Name, arg->Value);
		return -1;
	}

	arg = CommandLineFindArgumentA(args, "?");

	if (arg->Value)
	{
		printf("CommandLineFindArgumentA: unexpected %s value\n", arg->Name);
		return -1;
	}

	return 0;
}
