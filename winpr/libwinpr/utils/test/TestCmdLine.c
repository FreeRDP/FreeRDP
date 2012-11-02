
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
	{ "w", COMMAND_LINE_VALUE_REQUIRED },
	{ "h", COMMAND_LINE_VALUE_REQUIRED },
	{ "f", COMMAND_LINE_VALUE_FLAG },
	{ "admin", COMMAND_LINE_VALUE_FLAG },
	{ "multimon", COMMAND_LINE_VALUE_FLAG },
	{ "v", COMMAND_LINE_VALUE_REQUIRED },
	{ "?", COMMAND_LINE_VALUE_FLAG },
	{ NULL, 0 }
};

#define testArgc (sizeof(testArgv) / sizeof(testArgv[0]))

int TestCmdLine(int argc, char* argv[])
{
	if (CommandLineParseArgumentsA(testArgc, testArgv, args, 0) != 0)
		return -1;

	if (strcmp("1024", args[0].Value) != 0)
		return -1;

	if (strcmp("768", args[1].Value) != 0)
		return -1;

	if (args[2].Value)
		return -1;

	if (!args[3].Value)
		return -1;

	if (!args[4].Value)
		return -1;

	if (strcmp("localhost:3389", args[5].Value) != 0)
		return -1;

	if (args[6].Value)
		return -1;

	return 0;
}
