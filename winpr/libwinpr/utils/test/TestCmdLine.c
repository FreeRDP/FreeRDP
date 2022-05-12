#include <errno.h>
#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/tchar.h>
#include <winpr/cmdline.h>
#include <winpr/strlst.h>

static const char* testArgv[] = { "mstsc.exe",
	                              "+z",
	                              "/w:1024",
	                              "/h:768",
	                              "/bpp:32",
	                              "/admin",
	                              "/multimon",
	                              "+fonts",
	                              "-wallpaper",
	                              "/v:localhost:3389",
	                              "/valuelist:value1,value2",
	                              "/valuelist-empty:",
	                              0 };

static const char testListAppName[] = "test app name";
static const char* testListArgs[] = {
	"a,b,c,d", "a:,\"b:xxx, yyy\",c", "a:,,,b",      "a:,\",b",       "\"a,b,c,d d d,fff\"", "",
	NULL,      "'a,b,\",c'",          "\"a,b,',c\"", "', a, ', b,c'", "\"a,b,\",c\""
};

static const char* testListArgs1[] = { testListAppName, "a", "b", "c", "d" };
static const char* testListArgs2[] = { testListAppName, "a:", "b:xxx, yyy", "c" };
// static const char* testListArgs3[] = {};
// static const char* testListArgs4[] = {};
static const char* testListArgs5[] = { testListAppName, "a", "b", "c", "d d d", "fff" };
static const char* testListArgs6[] = { testListAppName };
static const char* testListArgs7[] = { testListAppName };
static const char* testListArgs8[] = { testListAppName, "a", "b", "\"", "c" };
static const char* testListArgs9[] = { testListAppName, "a", "b", "'", "c" };
// static const char* testListArgs10[] = {};
// static const char* testListArgs11[] = {};

static const char** testListArgsResult[] = { testListArgs1,
	                                         testListArgs2,
	                                         NULL /* testListArgs3 */,
	                                         NULL /* testListArgs4 */,
	                                         testListArgs5,
	                                         testListArgs6,
	                                         testListArgs7,
	                                         testListArgs8,
	                                         testListArgs9,
	                                         NULL /* testListArgs10 */,
	                                         NULL /* testListArgs11 */ };
static const size_t testListArgsCount[] = {
	ARRAYSIZE(testListArgs1),
	ARRAYSIZE(testListArgs2),
	0 /* ARRAYSIZE(testListArgs3) */,
	0 /* ARRAYSIZE(testListArgs4) */,
	ARRAYSIZE(testListArgs5),
	ARRAYSIZE(testListArgs6),
	ARRAYSIZE(testListArgs7),
	ARRAYSIZE(testListArgs8),
	ARRAYSIZE(testListArgs9),
	0 /* ARRAYSIZE(testListArgs10) */,
	0 /* ARRAYSIZE(testListArgs11) */
};

static BOOL checkResult(size_t index, char** actual, size_t actualCount)
{
	const char** result = testListArgsResult[index];
	const size_t resultCount = testListArgsCount[index];

	if (resultCount != actualCount)
		return FALSE;

	if (actualCount == 0)
	{
		return (actual == NULL);
	}
	else
	{
		size_t x;

		if (!actual)
			return FALSE;

		for (x = 0; x < actualCount; x++)
		{
			const char* a = result[x];
			const char* b = actual[x];

			if (strcmp(a, b) != 0)
				return FALSE;
		}
	}

	return TRUE;
}

static BOOL TestCommandLineParseCommaSeparatedValuesEx(void)
{
	size_t x;

	WINPR_ASSERT(ARRAYSIZE(testListArgs) == ARRAYSIZE(testListArgsResult));
	WINPR_ASSERT(ARRAYSIZE(testListArgs) == ARRAYSIZE(testListArgsCount));

	for (x = 0; x < ARRAYSIZE(testListArgs); x++)
	{
		const char* list = testListArgs[x];
		size_t count = 42;
		char** ptr = CommandLineParseCommaSeparatedValuesEx(testListAppName, list, &count);
		BOOL valid = checkResult(x, ptr, count);
		free(ptr);
		if (!valid)
			return FALSE;
	}

	return TRUE;
}

int TestCmdLine(int argc, char* argv[])
{
	int status;
	int ret = -1;
	DWORD flags;
	long width = 0;
	long height = 0;
	const COMMAND_LINE_ARGUMENT_A* arg;
	int testArgc;
	char** command_line;
	COMMAND_LINE_ARGUMENT_A args[] = {
		{ "v", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "destination server" },
		{ "port", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "server port" },
		{ "w", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "width" },
		{ "h", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "height" },
		{ "f", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "fullscreen" },
		{ "bpp", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL,
		  "session bpp (color depth)" },
		{ "admin", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, "console",
		  "admin (or console) session" },
		{ "multimon", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "multi-monitor" },
		{ "a", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, "addin", "addin" },
		{ "u", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "username" },
		{ "p", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "password" },
		{ "d", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "domain" },
		{ "z", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "compression" },
		{ "audio", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "audio output mode" },
		{ "mic", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "audio input (microphone)" },
		{ "fonts", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL,
		  "smooth fonts (cleartype)" },
		{ "aero", COMMAND_LINE_VALUE_BOOL, NULL, NULL, BoolValueFalse, -1, NULL,
		  "desktop composition" },
		{ "window-drag", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL,
		  "full window drag" },
		{ "menu-anims", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL,
		  "menu animations" },
		{ "themes", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "themes" },
		{ "wallpaper", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "wallpaper" },
		{ "codec", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "codec" },
		{ "nego", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL,
		  "protocol security negotiation" },
		{ "sec", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL,
		  "force specific protocol security" },
		{ "sec-rdp", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL,
		  "rdp protocol security" },
		{ "sec-tls", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL,
		  "tls protocol security" },
		{ "sec-nla", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL,
		  "nla protocol security" },
		{ "sec-ext", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL,
		  "nla extended protocol security" },
		{ "cert-name", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL,
		  "certificate name" },
		{ "cert-ignore", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL,
		  "ignore certificate" },
		{ "valuelist", COMMAND_LINE_VALUE_REQUIRED, "<val1>,<val2>", NULL, NULL, -1, NULL,
		  "List of comma separated values." },
		{ "valuelist-empty", COMMAND_LINE_VALUE_REQUIRED, "<val1>,<val2>", NULL, NULL, -1, NULL,
		  "List of comma separated values. Used to test correct behavior if an empty list was "
		  "passed." },
		{ "version", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_VERSION, NULL, NULL, NULL, -1,
		  NULL, "print version" },
		{ "help", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_HELP, NULL, NULL, NULL, -1, "?",
		  "print help" },
		{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
	};

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	flags = COMMAND_LINE_SIGIL_SLASH | COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_SIGIL_PLUS_MINUS;
	testArgc = string_list_length(testArgv);
	command_line = string_list_copy(testArgv);

	if (!command_line)
	{
		printf("Argument duplication failed (not enough memory?)\n");
		return ret;
	}

	status = CommandLineParseArgumentsA(testArgc, command_line, args, flags, NULL, NULL, NULL);

	if (status != 0)
	{
		printf("CommandLineParseArgumentsA failure: %d\n", status);
		goto out;
	}

	arg = CommandLineFindArgumentA(args, "w");

	if (strcmp("1024", arg->Value) != 0)
	{
		printf("CommandLineFindArgumentA: unexpected %s value %s\n", arg->Name, arg->Value);
		goto out;
	}

	arg = CommandLineFindArgumentA(args, "h");

	if (strcmp("768", arg->Value) != 0)
	{
		printf("CommandLineFindArgumentA: unexpected %s value %s\n", arg->Name, arg->Value);
		goto out;
	}

	arg = CommandLineFindArgumentA(args, "f");

	if (arg->Value)
	{
		printf("CommandLineFindArgumentA: unexpected %s value\n", arg->Name);
		goto out;
	}

	arg = CommandLineFindArgumentA(args, "admin");

	if (!arg->Value)
	{
		printf("CommandLineFindArgumentA: unexpected %s value\n", arg->Name);
		goto out;
	}

	arg = CommandLineFindArgumentA(args, "multimon");

	if (!arg->Value)
	{
		printf("CommandLineFindArgumentA: unexpected %s value\n", arg->Name);
		goto out;
	}

	arg = CommandLineFindArgumentA(args, "v");

	if (strcmp("localhost:3389", arg->Value) != 0)
	{
		printf("CommandLineFindArgumentA: unexpected %s value %s\n", arg->Name, arg->Value);
		goto out;
	}

	arg = CommandLineFindArgumentA(args, "fonts");

	if (!arg->Value)
	{
		printf("CommandLineFindArgumentA: unexpected %s value\n", arg->Name);
		goto out;
	}

	arg = CommandLineFindArgumentA(args, "wallpaper");

	if (arg->Value)
	{
		printf("CommandLineFindArgumentA: unexpected %s value\n", arg->Name);
		goto out;
	}

	arg = CommandLineFindArgumentA(args, "help");

	if (arg->Value)
	{
		printf("CommandLineFindArgumentA: unexpected %s value\n", arg->Name);
		goto out;
	}

	arg = args;
	errno = 0;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		printf("Argument: %s\n", arg->Name);
		CommandLineSwitchStart(arg) CommandLineSwitchCase(arg, "v")
		{
		}
		CommandLineSwitchCase(arg, "w")
		{
			width = strtol(arg->Value, NULL, 0);

			if (errno != 0)
				goto out;
		}
		CommandLineSwitchCase(arg, "h")
		{
			height = strtol(arg->Value, NULL, 0);

			if (errno != 0)
				goto out;
		}
		CommandLineSwitchCase(arg, "valuelist")
		{
			char** p;
			size_t count;
			p = CommandLineParseCommaSeparatedValuesEx(arg->Name, arg->Value, &count);
			free(p);

			if (!p || count != 3)
			{
				printf("CommandLineParseCommaSeparatedValuesEx: invalid p or count (%" PRIuz
				       "!=3)\n",
				       count);
				goto out;
			}
		}
		CommandLineSwitchCase(arg, "valuelist-empty")
		{
			char** p;
			size_t count;
			p = CommandLineParseCommaSeparatedValuesEx(arg->Name, arg->Value, &count);
			free(p);

			if (!p || count != 1)
			{
				printf("CommandLineParseCommaSeparatedValuesEx: invalid p or count (%" PRIuz
				       "!=1)\n",
				       count);
				goto out;
			}
		}
		CommandLineSwitchDefault(arg)
		{
		}
		CommandLineSwitchEnd(arg)
	} while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	if ((width != 1024) || (height != 768))
	{
		printf("Unexpected width and height: Actual: (%ldx%ld), Expected: (1024x768)\n", width,
		       height);
		goto out;
	}
	ret = 0;

out:
	string_list_free(command_line);

	if (!TestCommandLineParseCommaSeparatedValuesEx())
		return -1;
	return ret;
}
