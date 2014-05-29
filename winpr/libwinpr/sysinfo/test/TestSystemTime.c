
#include <winpr/crt.h>
#include <winpr/sysinfo.h>

int TestSystemTime(int argc, char* argv[])
{
	SYSTEMTIME sTime;

	GetSystemTime(&sTime);

	printf("GetSystemTime: wYear: %d wMonth: %d wDayOfWeek: %d wDay: %d wHour: %d wMinute: %d wSecond: %d wMilliseconds: %d\n",
			sTime.wYear, sTime.wMonth, sTime.wDayOfWeek, sTime.wDay, sTime.wHour, sTime.wMinute, sTime.wSecond, sTime.wMilliseconds);

	return 0;
}

