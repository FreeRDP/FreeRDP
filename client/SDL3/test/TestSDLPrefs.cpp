#include "../sdl_prefs.hpp"

#include <iostream>
#include <fstream>

#include <winpr/config.h>
#include <winpr/winpr.h>

#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#error Could not find system header "<filesystem>" or "<experimental/filesystem>"
#endif

static std::shared_ptr<SdlPref> instance()
{
#if !defined(TEST_SRC_AREA)
#error "Please define TEST_SRC_AREA to the source directory of this test case"
#endif
	fs::path src(TEST_SRC_AREA);
	src /= "sdl-freerdp.json";
	if (!fs::exists(src))
	{
		std::cerr << "[ERROR] test configuration file '" << src << "' does not exist" << std::endl;
		return nullptr;
	}

	return SdlPref::instance(src.string());
}

int TestSDLPrefs(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

#if defined(WITH_WINPR_JSON)
	printf("implementation: json\n");
#else
	printf("implementation: fallback\n");
#endif

#if defined(WITH_WINPR_JSON)
	printf("config: %s\n", instance()->get_pref_file().c_str());
#endif

	auto string_value = instance()->get_string("string_key", "cba");
#if defined(WITH_WINPR_JSON)
	if (string_value != "abc")
		return -1;
#else
	if (string_value != "cba")
		return -1;
#endif

	auto string_value_nonexistent = instance()->get_string("string_key_nonexistent", "cba");
	if (string_value_nonexistent != "cba")
		return -1;

	auto int_value = instance()->get_int("int_key", 321);
#if defined(WITH_WINPR_JSON)
	if (int_value != 123)
		return -1;
#else
	if (int_value != 321)
		return -1;
#endif

	auto int_value_nonexistent = instance()->get_int("int_key_nonexistent", 321);
	if (int_value_nonexistent != 321)
		return -1;

	auto bool_value = instance()->get_bool("bool_key", false);
#if defined(WITH_WINPR_JSON)
	if (!bool_value)
		return -1;
#else
	if (bool_value)
		return -1;
#endif

	auto bool_value_nonexistent = instance()->get_bool("bool_key_nonexistent", false);
	if (bool_value_nonexistent)
		return -1;

	auto array_value = instance()->get_array("array_key", { "c", "b", "a" });
	if (array_value.size() != 3)
		return -1;
#if defined(WITH_WINPR_JSON)
	if (array_value[0] != "a")
		return -1;
	if (array_value[1] != "b")
		return -1;
	if (array_value[2] != "c")
		return -1;
#else
	if (array_value[0] != "c")
		return -1;
	if (array_value[1] != "b")
		return -1;
	if (array_value[2] != "a")
		return -1;
#endif

	auto array_value_nonexistent =
	    instance()->get_array("array_key_nonexistent", { "c", "b", "a" });
	if (array_value_nonexistent.size() != 3)
		return -1;

	if (array_value_nonexistent[0] != "c")
		return -1;
	if (array_value_nonexistent[1] != "b")
		return -1;
	if (array_value_nonexistent[2] != "a")
		return -1;

	return 0;
}
