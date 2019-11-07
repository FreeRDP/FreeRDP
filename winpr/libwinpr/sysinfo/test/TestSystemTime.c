
#include <winpr/crt.h>
#include <winpr/sysinfo.h>

int TestSystemTime(int argc, char* argv[])
{
	SYSTEMTIME sTime;

	GetSystemTime(&sTime);

	printf("GetSystemTime: wYear: %" PRIu16 " wMonth: %" PRIu16 " wDayOfWeek: %" PRIu16
	       " wDay: %" PRIu16 " wHour: %" PRIu16 " wMinute: %" PRIu16 " wSecond: %" PRIu16
	       " wMilliseconds: %" PRIu16 "\n",
	       sTime.wYear, sTime.wMonth, sTime.wDayOfWeek, sTime.wDay, sTime.wHour, sTime.wMinute,
	       sTime.wSecond, sTime.wMilliseconds);

	return 0;
}
