
#include <errno.h>
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/cmdline.h>

static const char* testArgv[] =
{
	"mstsc.exe",
	"+z",
	"/w:1024",
	"/h:768",
	"/bpp:32",
	"/admin",
	"/multimon",
	"+fonts",
	"-wallpaper",
	"/v:localhost:3389"
};

static COMMAND_LINE_ARGUMENT_A args[] =
{
	{ "v", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "destination server" },
	{ "port", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "server port" },
	{ "w", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "width" },
	{ "h", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "height" },
	{ "f", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "fullscreen" },
	{ "bpp", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "session bpp (color depth)" },
	{ "admin", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, "console", "admin (or console) session" },
	{ "multimon", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "multi-monitor" },
	{ "a", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, "addin", "addin" },
	{ "u", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "username" },
	{ "p", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "password" },
	{ "d", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "domain" },
	{ "z", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "compression" },
	{ "audio", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "audio output mode" },
	{ "mic", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "audio input (microphone)" },
	{ "fonts", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "smooth fonts (cleartype)" },
	{ "aero", COMMAND_LINE_VALUE_BOOL, NULL, NULL, BoolValueFalse, -1, NULL, "desktop composition" },
	{ "window-drag", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "full window drag" },
	{ "menu-anims", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "menu animations" },
	{ "themes", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "themes" },
	{ "wallpaper", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "wallpaper" },
	{ "codec", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "codec" },
	{ "nego", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "protocol security negotiation" },
	{ "sec", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "force specific protocol security" },
	{ "sec-rdp", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "rdp protocol security" },
	{ "sec-tls", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "tls protocol security" },
	{ "sec-nla", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "nla protocol security" },
	{ "sec-ext", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "nla extended protocol security" },
	{ "cert-name", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "certificate name" },
	{ "cert-ignore", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "ignore certificate" },
	{ "version", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_VERSION, NULL, NULL, NULL, -1, NULL, "print version" },
	{ "help", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_HELP, NULL, NULL, NULL, -1, "?", "print help" },
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

#define testArgc (sizeof(testArgv) / sizeof(testArgv[0]))

int TestCmdLine(int argc, char* argv[])
{
	int status;
	DWORD flags;
	long width = 0;
	long height = 0;
	COMMAND_LINE_ARGUMENT_A* arg;
	flags = COMMAND_LINE_SIGIL_SLASH | COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_SIGIL_PLUS_MINUS;
	status = CommandLineParseArgumentsA(testArgc, testArgv, args, flags, NULL, NULL, NULL);

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

	arg = CommandLineFindArgumentA(args, "fonts");

	if (!arg->Value)
	{
		printf("CommandLineFindArgumentA: unexpected %s value\n", arg->Name);
		return -1;
	}

	arg = CommandLineFindArgumentA(args, "wallpaper");

	if (arg->Value)
	{
		printf("CommandLineFindArgumentA: unexpected %s value\n", arg->Name);
		return -1;
	}

	arg = CommandLineFindArgumentA(args, "help");

	if (arg->Value)
	{
		printf("CommandLineFindArgumentA: unexpected %s value\n", arg->Name);
		return -1;
	}

	arg = args;
	errno = 0;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		printf("Argument: %s\n", arg->Name);
		CommandLineSwitchStart(arg)
		CommandLineSwitchCase(arg, "v")
		{
		}
		CommandLineSwitchCase(arg, "w")
		{
			width = strtol(arg->Value, NULL, 0);

			if (errno != 0)
				return -1;
		}
		CommandLineSwitchCase(arg, "h")
		{
			height = strtol(arg->Value, NULL, 0);

			if (errno != 0)
				return -1;
		}
		CommandLineSwitchDefault(arg)
		{
		}
		CommandLineSwitchEnd(arg)
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	if ((width != 1024) || (height != 768))
	{
		printf("Unexpected width and height: Actual: (%ldx%ld), Expected: (1024x768)\n", width, height);
		return -1;
	}

	return 0;
}
