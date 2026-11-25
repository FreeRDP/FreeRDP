#include <stdio.h>
#include <string.h>

#include <winpr/wtypes.h>
#include <winpr/string.h>
#include <winpr/timezone.h>

#if defined(__EMSCRIPTEN__)
/* in TestTimeZoneBrowser.js */
extern void test_timezone_js_set(const char* tz);
extern int test_timezone_js_is(const char* tz);

/* node re-reads its time zone when process.env.TZ changes on its main thread */
static BOOL set_browser_timezone(const char* tz)
{
	test_timezone_js_set(tz);
	return test_timezone_js_is(tz) != 0;
}

static BOOL get_time_zone_key(char* key, size_t size)
{
	DYNAMIC_TIME_ZONE_INFORMATION tzif = WINPR_C_ARRAY_INIT;

	(void)GetDynamicTimeZoneInformation(&tzif);
	return ConvertWCharNToUtf8(tzif.TimeZoneKeyName, ARRAYSIZE(tzif.TimeZoneKeyName), key, size) >=
	       0;
}

static BOOL test_browser_time_zone(const char* iana, const char* expected)
{
	char key[128] = WINPR_C_ARRAY_INIT;

	if (!set_browser_timezone(iana))
	{
		(void)printf("skipping %s, cannot change the time zone here\n", iana);
		return TRUE;
	}

	if (!get_time_zone_key(key, sizeof(key)))
	{
		(void)fprintf(stderr, "GetDynamicTimeZoneInformation failed for %s\n", iana);
		return FALSE;
	}

	if (strcmp(key, expected) != 0)
	{
		(void)fprintf(stderr, "%s: expected time zone key '%s', got '%s'\n", iana, expected, key);
		return FALSE;
	}
	return TRUE;
}
#endif

int TestTimeZoneBrowser(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

#if defined(__EMSCRIPTEN__)
	/* A browser always knows its time zone, so it must map to a Windows one */
	char key[128] = WINPR_C_ARRAY_INIT;
	if (!get_time_zone_key(key, sizeof(key)) || (strlen(key) == 0))
	{
		(void)fprintf(stderr, "no Windows time zone key for the browser time zone\n");
		return -1;
	}

	if (!test_browser_time_zone("Europe/Lisbon", "GMT Standard Time"))
		return -1;
	if (!test_browser_time_zone("America/New_York", "Eastern Standard Time"))
		return -1;
	if (!test_browser_time_zone("UTC", "UTC"))
		return -1;
#endif

	return 0;
}
