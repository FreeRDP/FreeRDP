
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
	{ "w", COMMAND_LINE_VALUE_REQUIRED, NULL },
	{ "h", COMMAND_LINE_VALUE_REQUIRED, NULL },
	{ "f", COMMAND_LINE_VALUE_FLAG, NULL },
	{ "admin", COMMAND_LINE_VALUE_FLAG, NULL },
	{ "multimon", COMMAND_LINE_VALUE_FLAG, NULL },
	{ "v", COMMAND_LINE_VALUE_REQUIRED, NULL },
	{ "?", COMMAND_LINE_VALUE_FLAG, NULL },
	{ NULL, 0, NULL }
};

#define testArgc (sizeof(testArgv) / sizeof(testArgv[0]))

int TestCmdLine(int argc, char* argv[])
{
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;

	flags = COMMAND_LINE_SIGIL_SLASH | COMMAND_LINE_SEPARATOR_COLON;

	if (CommandLineParseArgumentsA(testArgc, testArgv, args, flags) != 0)
		return -1;

	arg = CommandLineFindArgumentA(args, "w");

	if (strcmp("1024", arg->Value) != 0)
		return -1;

	arg = CommandLineFindArgumentA(args, "h");

	if (strcmp("768", arg->Value) != 0)
		return -1;

	arg = CommandLineFindArgumentA(args, "f");

	if (arg->Value)
		return -1;

	arg = CommandLineFindArgumentA(args, "admin");

	if (!arg->Value)
		return -1;

	arg = CommandLineFindArgumentA(args, "multimon");

	if (!arg->Value)
		return -1;

	arg = CommandLineFindArgumentA(args, "v");

	if (strcmp("localhost:3389", arg->Value) != 0)
		return -1;

	arg = CommandLineFindArgumentA(args, "?");

	if (arg->Value)
		return -1;

	return 0;
}
