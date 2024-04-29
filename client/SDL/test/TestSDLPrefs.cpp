#include "../sdl_prefs.hpp"

#include <iostream>
#include <fstream>

#include <freerdp/api.h>
#include <freerdp/log.h>
#include <winpr/winpr.h>
#include <winpr/assert.h>

#define SDL_TAG CLIENT_TAG("SDL")

int TestSDLPrefs(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

#if defined(WITH_CJSON)
	printf("implementation: cJSON\n");
#else
	printf("implementation: fallback\n");
#endif

#if defined(WITH_CJSON)
	printf("config: %s\n", sdl_get_pref_file().c_str());
#endif

	auto string_value = sdl_get_pref_string("string_key", "cba");
#if defined(WITH_CJSON)
	WINPR_ASSERT(string_value == "abc");
#else
	WINPR_ASSERT(string_value == "cba");
#endif

	auto string_value_nonexistent = sdl_get_pref_string("string_key_nonexistent", "cba");
	WINPR_ASSERT(string_value_nonexistent == "cba");

	auto int_value = sdl_get_pref_int("int_key", 321);
#if defined(WITH_CJSON)
	WINPR_ASSERT(int_value == 123);
#else
	WINPR_ASSERT(int_value == 321);
#endif

	auto int_value_nonexistent = sdl_get_pref_int("int_key_nonexistent", 321);
	WINPR_ASSERT(int_value_nonexistent == 321);

	auto bool_value = sdl_get_pref_bool("bool_key", false);
#if defined(WITH_CJSON)
	WINPR_ASSERT(bool_value);
#else
	WINPR_ASSERT(!bool_value);
#endif

	auto bool_value_nonexistent = sdl_get_pref_bool("bool_key_nonexistent", false);
	WINPR_ASSERT(!bool_value_nonexistent);

	auto array_value = sdl_get_pref_array("array_key", { "c", "b", "a" });
#if defined(WITH_CJSON)
	WINPR_ASSERT(array_value.size() == 3 && array_value[0] == "a" && array_value[1] == "b" &&
	             array_value[2] == "c");
#else
	WINPR_ASSERT(array_value.size() == 3 && array_value[0] == "c" && array_value[1] == "b" &&
	             array_value[2] == "a");
#endif

	auto array_value_nonexistent = sdl_get_pref_array("array_key_nonexistent", { "c", "b", "a" });
	WINPR_ASSERT(array_value_nonexistent.size() == 3 && array_value_nonexistent[0] == "c" &&
	             array_value_nonexistent[1] == "b" && array_value_nonexistent[2] == "a");

	return 0;
}
