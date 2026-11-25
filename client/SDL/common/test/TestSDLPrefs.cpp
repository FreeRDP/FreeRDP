#include "../sdl_prefs.hpp"

#include <iostream>
#include <fstream>

#include <winpr/config.h>
#include <winpr/winpr.h>

#include "../filesystem.hpp"

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

#if defined(WITH_WINPR_JSON)
static std::string test_file(const char* name)
{
	fs::path src(TEST_SRC_AREA);
	src /= name;
	return src.string();
}
#endif

/* user and system configuration together: the user file wins, the system file fills in, and
 * the system file alone decides the locks. */
static bool test_system_config()
{
#if defined(WITH_WINPR_JSON)
	auto prefs =
	    SdlPref::instance(test_file("sdl-freerdp-user.json"), test_file("sdl-freerdp-system.json"));
	if (prefs->get_string("string_key", "x") != "user")
		return false;
	if (prefs->get_int("int_key", 0) != 1)
		return false;
	if (prefs->get_bool("bool_key", true))
		return false;
	if (!prefs->get_bool("system_bool_key", false))
		return false;
	if (prefs->get_string("string_key", "x", true) != "system")
		return false;
	/* only the system file may set this, the user file sets it to false */
	if (!prefs->is_commandline_override_allowed())
		return false;

	prefs = SdlPref::instance(test_file("sdl-freerdp-user.json"),
	                          test_file("sdl-freerdp-system-locked.json"));
	if (prefs->get_string("string_key", "x") != "system")
		return false;
	if (prefs->get_int("int_key", 0) != 0)
		return false;
	if (prefs->is_commandline_override_allowed())
		return false;
#endif
	return true;
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
	if (array_value.at(0) != "a")
		return -1;
	if (array_value.at(1) != "b")
		return -1;
	if (array_value.at(2) != "c")
		return -1;
#else
	if (array_value.at(0) != "c")
		return -1;
	if (array_value.at(1) != "b")
		return -1;
	if (array_value.at(2) != "a")
		return -1;
#endif

	auto array_value_nonexistent =
	    instance()->get_array("array_key_nonexistent", { "c", "b", "a" });
	if (array_value_nonexistent.size() != 3)
		return -1;

	if (array_value_nonexistent.at(0) != "c")
		return -1;
	if (array_value_nonexistent.at(1) != "b")
		return -1;
	if (array_value_nonexistent.at(2) != "a")
		return -1;

	if (!test_system_config())
		return -1;

	return 0;
}
