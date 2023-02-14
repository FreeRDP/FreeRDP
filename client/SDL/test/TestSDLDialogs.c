#include <freerdp/config.h>

#include "../sdl_dialogs.h"
#include "../sdl_select.h"
#include "../sdl_input.h"
#include "../sdl_utils.h"

static void array_free(char** array, size_t count)
{
	if (!array)
		return;

	for (size_t x = 0; x < count; x++)
		free(array[x]);
	free(array);
}

static char* allocate_random_str(void)
{
	size_t len = (rand() % 32) + 3;
	char* str = calloc(len + 1, sizeof(char));
	if (!str)
		return NULL;

	for (size_t x = 0; x < len; x++)
	{
		char cur = 'a' + rand() % 26;
		str[x] = cur;
	}

	return str;
}

static char** allocate_random(size_t count)
{
	char** array = calloc(count, sizeof(char*));
	if (!array)
		return NULL;

	for (size_t x = 0; x < count; x++)
		array[x] = allocate_random_str();
	return array;
}

static BOOL test_select_dialog(wLog* log)
{
	BOOL res = FALSE;
	const size_t count = 7;
	char** labels = allocate_random(count);
	if (!labels)
		goto fail;

	const int irc = SDL_Init(SDL_INIT_VIDEO);
	if (sdl_log_error(irc, log, "SDL_Init"))
		goto fail;

	const int rc = sdl_select_get("sometitle", count, (const char**)labels);
	if (rc < 0)
		goto fail;

	res = TRUE;
fail:
	array_free(labels, count);
	SDL_Quit();
	return res;
}

static BOOL test_input_dialog(wLog* log)
{
	BOOL res = FALSE;
	const size_t count = 7;
	char** labels = allocate_random(count);
	char** initial = allocate_random(count);
	Uint32* flags = calloc(count, sizeof(Uint32));
	char** result = calloc(count, sizeof(char*));
	if (!labels || !initial || !flags || !result)
		goto fail;

	flags[0] = SDL_INPUT_MASK;

	const int irc = SDL_Init(SDL_INIT_VIDEO);
	if (sdl_log_error(irc, log, "SDL_Init"))
		goto fail;

	const int rc = sdl_input_get("sometitle", count, (const char**)labels, (const char**)initial,
	                             flags, result);
	if (rc < 0)
		goto fail;

	res = TRUE;
fail:
	array_free(labels, count);
	array_free(initial, count);
	array_free(result, count);
	free(flags);
	SDL_Quit();
	return res;
}

int TestSDLDialogs(int argc, char* argv[])
{
#if 0
	wLog* log = WLog_Get("TestSDLDialogs");
	if (!test_input_dialog(log))
		return -1;
	if (!test_select_dialog(log))
		return -1;
#endif
	return 0;
}
