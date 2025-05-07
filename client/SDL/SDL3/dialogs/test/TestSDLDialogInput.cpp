#include "../sdl_dialogs.hpp"
#include "../sdl_input_widget_pair_list.hpp"

#include <freerdp/api.h>
#include <winpr/wlog.h>

typedef int (*fkt_t)(void);

BOOL sdl_log_error_ex(Sint32 res, wLog* log, const char* what, const char* file, size_t line,
                      const char* fkt)
{
	WINPR_UNUSED(file);

	WLog_Print(log, WLOG_ERROR, "[%s:%" PRIuz "][%s]: %s", fkt, line, what, "xxx");
	return TRUE;
}

static int auth_dialogs(void)
{
	const std::string title = "sometitle";
	std::vector<std::string> result;

	std::vector<std::string> initial{ "Smartcard", "abc", "def" };
	std::vector<Uint32> flags = { SdlInputWidgetPair::SDL_INPUT_READONLY,
		                          SdlInputWidgetPair::SDL_INPUT_MASK, 0 };

	const std::vector<std::string>& labels{ "foo", "bar", "gaga" };

	SdlInputWidgetPairList ilist(title.c_str(), labels, initial, flags);
	auto rc = ilist.run(result);

	if ((result.size() < labels.size()))
		rc = -1;
	return rc;
}

static int runTest(fkt_t fkt)
{
	int rc;

	SDL_Init(SDL_INIT_VIDEO);
	sdl_dialogs_init();
	try
	{
		rc = fkt();
	}
	catch (int e)
	{
		rc = e;
	}
	sdl_dialogs_uninit();
	SDL_Quit();
	return rc;
}

int main(int argc, char* argv[])
{
	int rc = 0;

	(void)argc;
	(void)argv;

	rc = runTest(auth_dialogs);

	return rc;
}
