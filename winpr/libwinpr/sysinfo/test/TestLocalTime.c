
#include <winpr/crt.h>
#include <winpr/sysinfo.h>

int TestLocalTime(int argc, char* argv[])
{
	SYSTEMTIME lTime;

	GetLocalTime(&lTime);

	printf("GetLocalTime: wYear: %" PRIu16 " wMonth: %" PRIu16 " wDayOfWeek: %" PRIu16
	       " wDay: %" PRIu16 " wHour: %" PRIu16 " wMinute: %" PRIu16 " wSecond: %" PRIu16
	       " wMilliseconds: %" PRIu16 "\n",
	       lTime.wYear, lTime.wMonth, lTime.wDayOfWeek, lTime.wDay, lTime.wHour, lTime.wMinute,
	       lTime.wSecond, lTime.wMilliseconds);

	return 0;
}
