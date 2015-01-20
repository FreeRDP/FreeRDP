
#include <winpr/crt.h>
#include <winpr/sysinfo.h>

int TestLocalTime(int argc, char* argv[])
{
	SYSTEMTIME lTime;

	GetLocalTime(&lTime);

	printf("GetLocalTime: wYear: %d wMonth: %d wDayOfWeek: %d wDay: %d wHour: %d wMinute: %d wSecond: %d wMilliseconds: %d\n",
			lTime.wYear, lTime.wMonth, lTime.wDayOfWeek, lTime.wDay, lTime.wHour, lTime.wMinute, lTime.wSecond, lTime.wMilliseconds);

	return 0;
}
