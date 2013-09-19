
#include <winpr/crt.h>
#include <winpr/synch.h>

int TestSynchWaitableTimer(int argc, char* argv[])
{
	DWORD status;
	HANDLE timer;
	LONG period;
	LARGE_INTEGER due;

	timer = CreateWaitableTimer(NULL, FALSE, NULL);

	if (!timer)
	{
		printf("CreateWaitableTimer failure\n");
		return -1;
	}

	due.QuadPart = -15000000LL; /* 1.5 seconds */

	if (!SetWaitableTimer(timer, &due, 0, NULL, NULL, 0))
	{
		printf("SetWaitableTimer failure\n");
		return -1;
	}

	status = WaitForSingleObject(timer, INFINITE);

	if (status != WAIT_OBJECT_0)
	{
		printf("WaitForSingleObject(timer, INFINITE) failure\n");
		return -1;
	}

	printf("Timer Signaled\n");

	status = WaitForSingleObject(timer, 2000);

	if (status != WAIT_TIMEOUT)
	{
		printf("WaitForSingleObject(timer, 2000) failure: Actual: 0x%04X, Expected: 0x%04X\n", status, WAIT_TIMEOUT);
		return -1;
	}

	due.QuadPart = 0;

	period = 1200; /* 1.2 seconds */

	if (!SetWaitableTimer(timer, &due, period, NULL, NULL, 0))
	{
		printf("SetWaitableTimer failure\n");
		return -1;
	}

	if (WaitForSingleObject(timer, INFINITE) != WAIT_OBJECT_0)
	{
		printf("WaitForSingleObject(timer, INFINITE) failure\n");
		return -1;
	}

	printf("Timer Signaled\n");

	if (WaitForMultipleObjects(1, &timer, FALSE, INFINITE) != WAIT_OBJECT_0)
	{
		printf("WaitForMultipleObjects(timer, INFINITE) failure\n");
		return -1;
	}

	printf("Timer Signaled\n");

	CloseHandle(timer);

	return 0;
}

