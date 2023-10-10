#include "../sdl_selectlist.hpp"
#include "../sdl_input_widgets.hpp"

#include <freerdp/api.h>
#include <winpr/wlog.h>

BOOL sdl_log_error_ex(Uint32 res, wLog* log, const char* what, const char* file, size_t line,
                      const char* fkt)
{
	return FALSE;
}

static bool test_input_dialog()
{
	const auto title = "sometitle";
	std::vector<std::string> labels;
	std::vector<std::string> initial;
	std::vector<Uint32> flags;
	for (size_t x = 0; x < 12; x++)
	{
		labels.push_back("label" + std::to_string(x));
		initial.push_back(std::to_string(x));

		Uint32 flag = 0;
		if ((x % 2) != 0)
			flag |= SdlInputWidget::SDL_INPUT_MASK;
		if ((x % 3) == 0)
			flag |= SdlInputWidget::SDL_INPUT_READONLY;

		flags.push_back(flag);
	}
	SdlInputWidgetList list{ title, labels, initial, flags };
	std::vector<std::string> result;
	auto rc = list.run(result);
	if (rc < 0)
	{
		return false;
	}
	if (result.size() != labels.size())
	{
		return false;
	}
	return true;
}

static bool test_select_dialog()
{
	const auto title = "sometitle";
	std::vector<std::string> labels;
	for (size_t x = 0; x < 12; x++)
	{
		labels.push_back("label" + std::to_string(x));
	}
	SdlSelectList list{ title, labels };
	auto rc = list.run();
	if (rc < 0)
	{
		return false;
	}
	if (static_cast<size_t>(rc) >= labels.size())
		return false;

	return true;
}

extern "C"
{
	FREERDP_API int TestSDLDialogs(int argc, char* argv[]);
}

int TestSDLDialogs(int argc, char* argv[])
{
	int rc = 0;

	(void)argc;
	(void)argv;

#if 0
	SDL_Init(SDL_INIT_VIDEO);
	try
	{
#if 1
		if (!test_input_dialog())
			throw -1;
#endif
#if 1
		if (!test_select_dialog())
			throw -2;
#endif
	}
	catch (int e)
	{
		rc = e;
	}
	SDL_Quit();

#endif
	return rc;
}
