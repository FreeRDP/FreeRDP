
#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/sysinfo.h>

int TestGetSystemPowerStatus(int argc, char* argv[])
{
	SYSTEM_POWER_STATUS status = WINPR_C_ARRAY_INIT;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	/* Battery/AC state is host-dependent (and entirely absent on CI runners), so this only
	 * checks that the call succeeds and fills in something - not what it reports. */
	if (!GetSystemPowerStatus(&status))
	{
		printf("GetSystemPowerStatus failed, error=%" PRIu32 "\n", GetLastError());
		return -1;
	}

	printf("SystemPowerStatus:\n");
	printf("\tACLineStatus: %" PRIu8 "\n", status.ACLineStatus);
	printf("\tBatteryFlag: 0x%02" PRIX8 "\n", status.BatteryFlag);
	printf("\tBatteryLifePercent: %" PRIu8 "\n", status.BatteryLifePercent);
	printf("\tBatteryLifeTime: %" PRIu32 "\n", status.BatteryLifeTime);
	printf("\tBatteryFullLifeTime: %" PRIu32 "\n", status.BatteryFullLifeTime);
	printf("\n");

	if ((status.BatteryLifePercent != BATTERY_PERCENTAGE_UNKNOWN) &&
	    (status.BatteryLifePercent > 100))
	{
		printf("BatteryLifePercent out of range: %" PRIu8 "\n", status.BatteryLifePercent);
		return -1;
	}

	return 0;
}
