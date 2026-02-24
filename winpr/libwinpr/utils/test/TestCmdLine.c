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
	                              nullptr };

static const char testListAppName[] = "test app name";
static const char* testListArgs[] = {
	"g:some.gateway.server,u:some\\\"user,p:some\\\"password,d:some\\\"domain,type:auto",
	"a,b,c,d",
	"a:,\"b:xxx, yyy\",c",
	"a:,,,b",
	"a:,\",b",
	"\"a,b,c,d d d,fff\"",
	"",
	nullptr,
	"'a,b,\",c'",
	"\"a,b,',c\"",
	"', a, ', b,c'",
	"\"a,b,\",c\"",

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
static const char* testListArgs12[] = { testListAppName,    "g:some.gateway.server",
	                                    "u:some\\\"user",   "p:some\\\"password",
	                                    "d:some\\\"domain", "type:auto" };

static const char** testListArgsResult[] = {
	testListArgs12,
	testListArgs1,
	testListArgs2,
	nullptr /* testListArgs3 */,
	nullptr /* testListArgs4 */,
	testListArgs5,
	testListArgs6,
	testListArgs7,
	testListArgs8,
	testListArgs9,
	nullptr /* testListArgs10 */,
	nullptr /* testListArgs11 */
};
static const size_t testListArgsCount[] = {
	ARRAYSIZE(testListArgs12),         ARRAYSIZE(testListArgs1),
	ARRAYSIZE(testListArgs2),          0 /* ARRAYSIZE(testListArgs3) */,
	0 /* ARRAYSIZE(testListArgs4) */,  ARRAYSIZE(testListArgs5),
	ARRAYSIZE(testListArgs6),          ARRAYSIZE(testListArgs7),
	ARRAYSIZE(testListArgs8),          ARRAYSIZE(testListArgs9),
	0 /* ARRAYSIZE(testListArgs10) */, 0 /* ARRAYSIZE(testListArgs11) */

};

static BOOL checkResult(size_t index, char** actual, size_t actualCount)
{
	const char** result = testListArgsResult[index];
	const size_t resultCount = testListArgsCount[index];

	if (resultCount != actualCount)
		return FALSE;

	if (actualCount == 0)
	{
		return (actual == nullptr);
	}
	else
	{
		if (!actual)
			return FALSE;

		for (size_t x = 0; x < actualCount; x++)
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
	WINPR_ASSERT(ARRAYSIZE(testListArgs) == ARRAYSIZE(testListArgsResult));
	WINPR_ASSERT(ARRAYSIZE(testListArgs) == ARRAYSIZE(testListArgsCount));

	for (size_t x = 0; x < ARRAYSIZE(testListArgs); x++)
	{
		union
		{
			char* p;
			char** pp;
			const char** ppc;
		} ptr;
		const char* list = testListArgs[x];
		size_t count = 42;
		ptr.pp = CommandLineParseCommaSeparatedValuesEx(testListAppName, list, &count);
		BOOL valid = checkResult(x, ptr.pp, count);
		free(ptr.p);
		if (!valid)
			return FALSE;
	}

	return TRUE;
}

int TestCmdLine(int argc, char* argv[])
{
	int status = 0;
	int ret = -1;
	DWORD flags = 0;
	long width = 0;
	long height = 0;
	const COMMAND_LINE_ARGUMENT_A* arg = nullptr;
	int testArgc = 0;
	char** command_line = nullptr;
	COMMAND_LINE_ARGUMENT_A args[] = {
		{ "v", COMMAND_LINE_VALUE_REQUIRED, nullptr, nullptr, nullptr, -1, nullptr,
		  "destination server" },
		{ "port", COMMAND_LINE_VALUE_REQUIRED, nullptr, nullptr, nullptr, -1, nullptr,
		  "server port" },
		{ "w", COMMAND_LINE_VALUE_REQUIRED, nullptr, nullptr, nullptr, -1, nullptr, "width" },
		{ "h", COMMAND_LINE_VALUE_REQUIRED, nullptr, nullptr, nullptr, -1, nullptr, "height" },
		{ "f", COMMAND_LINE_VALUE_FLAG, nullptr, nullptr, nullptr, -1, nullptr, "fullscreen" },
		{ "bpp", COMMAND_LINE_VALUE_REQUIRED, nullptr, nullptr, nullptr, -1, nullptr,
		  "session bpp (color depth)" },
		{ "admin", COMMAND_LINE_VALUE_FLAG, nullptr, nullptr, nullptr, -1, "console",
		  "admin (or console) session" },
		{ "multimon", COMMAND_LINE_VALUE_FLAG, nullptr, nullptr, nullptr, -1, nullptr,
		  "multi-monitor" },
		{ "a", COMMAND_LINE_VALUE_REQUIRED, nullptr, nullptr, nullptr, -1, "addin", "addin" },
		{ "u", COMMAND_LINE_VALUE_REQUIRED, nullptr, nullptr, nullptr, -1, nullptr, "username" },
		{ "p", COMMAND_LINE_VALUE_REQUIRED, nullptr, nullptr, nullptr, -1, nullptr, "password" },
		{ "d", COMMAND_LINE_VALUE_REQUIRED, nullptr, nullptr, nullptr, -1, nullptr, "domain" },
		{ "z", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
		  "compression" },
		{ "audio", COMMAND_LINE_VALUE_REQUIRED, nullptr, nullptr, nullptr, -1, nullptr,
		  "audio output mode" },
		{ "mic", COMMAND_LINE_VALUE_FLAG, nullptr, nullptr, nullptr, -1, nullptr,
		  "audio input (microphone)" },
		{ "fonts", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
		  "smooth fonts (cleartype)" },
		{ "aero", COMMAND_LINE_VALUE_BOOL, nullptr, nullptr, BoolValueFalse, -1, nullptr,
		  "desktop composition" },
		{ "window-drag", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
		  "full window drag" },
		{ "menu-anims", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
		  "menu animations" },
		{ "themes", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
		  "themes" },
		{ "wallpaper", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
		  "wallpaper" },
		{ "codec", COMMAND_LINE_VALUE_REQUIRED, nullptr, nullptr, nullptr, -1, nullptr, "codec" },
		{ "nego", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
		  "protocol security negotiation" },
		{ "sec", COMMAND_LINE_VALUE_REQUIRED, nullptr, nullptr, nullptr, -1, nullptr,
		  "force specific protocol security" },
#if defined(WITH_FREERDP_DEPRECATED)
		{ "sec-rdp", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
		  "rdp protocol security" },
		{ "sec-tls", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
		  "tls protocol security" },
		{ "sec-nla", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
		  "nla protocol security" },
		{ "sec-ext", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
		  "nla extended protocol security" },
		{ "cert-name", COMMAND_LINE_VALUE_REQUIRED, nullptr, nullptr, nullptr, -1, nullptr,
		  "certificate name" },
		{ "cert-ignore", COMMAND_LINE_VALUE_FLAG, nullptr, nullptr, nullptr, -1, nullptr,
		  "ignore certificate" },
#endif
		{ "valuelist", COMMAND_LINE_VALUE_REQUIRED, "<val1>,<val2>", nullptr, nullptr, -1, nullptr,
		  "List of comma separated values." },
		{ "valuelist-empty", COMMAND_LINE_VALUE_REQUIRED, "<val1>,<val2>", nullptr, nullptr, -1,
		  nullptr,
		  "List of comma separated values. Used to test correct behavior if an empty list was "
		  "passed." },
		{ "version", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_VERSION, nullptr, nullptr,
		  nullptr, -1, nullptr, "print version" },
		{ "help", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_HELP, nullptr, nullptr, nullptr, -1,
		  "?", "print help" },
		{ nullptr, 0, nullptr, nullptr, nullptr, -1, nullptr, nullptr }
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

	status =
	    CommandLineParseArgumentsA(testArgc, command_line, args, flags, nullptr, nullptr, nullptr);

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
			width = strtol(arg->Value, nullptr, 0);

			if (errno != 0)
				goto out;
		}
		CommandLineSwitchCase(arg, "h")
		{
			height = strtol(arg->Value, nullptr, 0);

			if (errno != 0)
				goto out;
		}
		CommandLineSwitchCase(arg, "valuelist")
		{
			size_t count = 0;
			char** p = CommandLineParseCommaSeparatedValuesEx(arg->Name, arg->Value, &count);
			free((void*)p);

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
			size_t count = 0;
			char** p = CommandLineParseCommaSeparatedValuesEx(arg->Name, arg->Value, &count);
			free((void*)p);

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
	} while ((arg = CommandLineFindNextArgumentA(arg)) != nullptr);

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
