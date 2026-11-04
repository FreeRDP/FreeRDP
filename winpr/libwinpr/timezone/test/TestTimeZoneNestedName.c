#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <winpr/wtypes.h>
#include <winpr/string.h>
#include <winpr/timezone.h>

#if !defined(_WIN32)
static BOOL test_tz(const char* iana, const char* expected)
{
	char key[128] = WINPR_C_ARRAY_INIT;
	DYNAMIC_TIME_ZONE_INFORMATION tzif = WINPR_C_ARRAY_INIT;

	// NOLINTNEXTLINE(concurrency-mt-unsafe)
	if (setenv("TZ", iana, 1) != 0)
		return FALSE;
	tzset();

	(void)GetDynamicTimeZoneInformation(&tzif);
	if (ConvertWCharNToUtf8(tzif.TimeZoneKeyName, ARRAYSIZE(tzif.TimeZoneKeyName), key,
	                        sizeof(key)) < 0)
		return FALSE;

	if (strcmp(key, expected) != 0)
	{
		(void)fprintf(stderr, "%s: expected time zone key '%s', got '%s'\n", iana, expected, key);
		return FALSE;
	}
	return TRUE;
}
#endif

int TestTimeZoneNestedName(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

#if !defined(_WIN32)
	/* zone names with more than two parts */
	if (!test_tz("America/Argentina/Salta", "Argentina Standard Time"))
		return -1;
	if (!test_tz("America/Indiana/Knox", "Central Standard Time"))
		return -1;
	if (!test_tz("America/North_Dakota/Center", "Central Standard Time"))
		return -1;
	/* and with two */
	if (!test_tz("Europe/Berlin", "W. Europe Standard Time"))
		return -1;
#endif

	return 0;
}
