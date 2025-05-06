#include "../sdl_dialogs.hpp"
#include "../sdl_select_list.hpp"

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

static int select_dialogs(void)
{
	const std::vector<std::string> labels{ "foo", "bar", "gaga", "blabla" };
	SdlSelectList list{ "title", labels };
	return list.run();
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

	rc = runTest(select_dialogs);

	return rc;
}
