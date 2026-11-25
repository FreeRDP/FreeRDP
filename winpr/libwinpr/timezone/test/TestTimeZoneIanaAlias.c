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

int TestTimeZoneIanaAlias(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

#if !defined(_WIN32)
	/* current IANA names that CLDR's windowsZones.xml still spells the old way */
	if (!test_tz("Asia/Kolkata", "India Standard Time"))
		return -1;
	if (!test_tz("Europe/Kyiv", "FLE Standard Time"))
		return -1;
	if (!test_tz("Asia/Kathmandu", "Nepal Standard Time"))
		return -1;
	if (!test_tz("Asia/Yangon", "Myanmar Standard Time"))
		return -1;
	/* in a multi zone entry of the fallback table (where the system does not resolve it to its
	 * tzdata link target Pacific/Guadalcanal, which maps to the same key) */
	if (!test_tz("Pacific/Pohnpei", "Central Pacific Standard Time"))
		return -1;
	/* the old spelling keeps working */
	if (!test_tz("Asia/Calcutta", "India Standard Time"))
		return -1;
	if (!test_tz("Europe/Kiev", "FLE Standard Time"))
		return -1;
	if (!test_tz("Asia/Katmandu", "Nepal Standard Time"))
		return -1;
	if (!test_tz("Asia/Rangoon", "Myanmar Standard Time"))
		return -1;
#endif

	return 0;
}
