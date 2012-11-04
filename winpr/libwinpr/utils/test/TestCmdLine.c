
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/cmdline.h>

const char* testArgv[] =
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

COMMAND_LINE_ARGUMENT_A args[] =
{
	{ "w", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, "width" },
	{ "h", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, "height" },
	{ "f", COMMAND_LINE_VALUE_FLAG, NULL, NULL, "fullscreen" },
	{ "bpp", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, "session bpp (color depth)" },
	{ "v", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, "destination server" },
	{ "admin", COMMAND_LINE_VALUE_FLAG, NULL, "console", "admin (or console) session" },
	{ "multimon", COMMAND_LINE_VALUE_FLAG, NULL, NULL, "multi-monitor" },
	{ "a", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, "addin" },
	{ "u", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, "username" },
	{ "p", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, "password" },
	{ "d", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, "domain" },
	{ "z", COMMAND_LINE_VALUE_BOOL, NULL, NULL, "compression" },
	{ "audio", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, "audio output mode" },
	{ "mic", COMMAND_LINE_VALUE_FLAG, NULL, NULL, "audio input (microphone)" },
	{ "fonts", COMMAND_LINE_VALUE_BOOL, NULL, NULL, "smooth fonts (cleartype)" },
	{ "aero", COMMAND_LINE_VALUE_BOOL, NULL, NULL, "desktop composition" },
	{ "window-drag", COMMAND_LINE_VALUE_BOOL, NULL, NULL, "full window drag" },
	{ "menu-anims", COMMAND_LINE_VALUE_BOOL, NULL, NULL, "menu animations" },
	{ "themes", COMMAND_LINE_VALUE_BOOL, NULL, NULL, "themes" },
	{ "wallpaper", COMMAND_LINE_VALUE_BOOL, NULL, NULL, "wallpaper" },
	{ "codec", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, "codec" },
	{ "nego", COMMAND_LINE_VALUE_BOOL, NULL, NULL, "protocol security negotiation" },
	{ "sec", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, "force specific protocol security" },
	{ "sec-rdp", COMMAND_LINE_VALUE_BOOL, NULL, NULL, "rdp protocol security" },
	{ "sec-tls", COMMAND_LINE_VALUE_BOOL, NULL, NULL, "tls protocol security" },
	{ "sec-nla", COMMAND_LINE_VALUE_BOOL, NULL, NULL, "nla protocol security" },
	{ "sec-ext", COMMAND_LINE_VALUE_BOOL, NULL, NULL, "nla extended protocol security" },
	{ "cert-name", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, "certificate name" },
	{ "cert-ignore", COMMAND_LINE_VALUE_FLAG, NULL, NULL, "ignore certificate" },
	{ "ver", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_FINAL_ARGUMENT, NULL, "version", "print version" },
	{ "?", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_FINAL_ARGUMENT, NULL, "help", "print help" },
	{ NULL, 0, NULL, NULL, NULL }
};

#define testArgc (sizeof(testArgv) / sizeof(testArgv[0]))

int TestCmdLine(int argc, char* argv[])
{
	int status;
	DWORD flags;
	int width, height;
	COMMAND_LINE_ARGUMENT_A* arg;

	flags = COMMAND_LINE_SIGIL_SLASH | COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_SIGIL_PLUS_MINUS;
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

	arg = CommandLineFindArgumentA(args, "?");

	if (arg->Value)
	{
		printf("CommandLineFindArgumentA: unexpected %s value\n", arg->Name);
		return -1;
	}

	arg = args;

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
			width = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "h")
		{
			height = atoi(arg->Value);
		}
		CommandLineSwitchDefault(arg)
		{

		}

		CommandLineSwitchEnd(arg)
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	if ((width != 1024) || (height != 768))
	{
		printf("Unexpected width and height: Actual: (%dx%d), Expected: (%dx%d)\n", width, height, 1024, 768);
		return -1;
	}

	return 0;
}
