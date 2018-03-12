
#include <winpr/crt.h>
#include <winpr/synch.h>

int TestSynchWaitableTimer(int argc, char* argv[])
{
	DWORD status;
	HANDLE timer;
	LONG period;
	LARGE_INTEGER due;
	int result = -1;
	timer = CreateWaitableTimer(NULL, FALSE, NULL);

	if (!timer)
	{
		printf("CreateWaitableTimer failure\n");
		goto out;
	}

	due.QuadPart = -15000000LL; /* 1.5 seconds */

	if (!SetWaitableTimer(timer, &due, 0, NULL, NULL, 0))
	{
		printf("SetWaitableTimer failure\n");
		goto out;
	}

	status = WaitForSingleObject(timer, INFINITE);

	if (status != WAIT_OBJECT_0)
	{
		printf("WaitForSingleObject(timer, INFINITE) failure\n");
		goto out;
	}

	printf("Timer Signaled\n");
	status = WaitForSingleObject(timer, 2000);

	if (status != WAIT_TIMEOUT)
	{
		printf("WaitForSingleObject(timer, 2000) failure: Actual: 0x%08"PRIX32", Expected: 0x%08X\n",
		       status, WAIT_TIMEOUT);
		goto out;
	}

	due.QuadPart = 0;
	period = 1200; /* 1.2 seconds */

	if (!SetWaitableTimer(timer, &due, period, NULL, NULL, 0))
	{
		printf("SetWaitableTimer failure\n");
		goto out;
	}

	if (WaitForSingleObject(timer, INFINITE) != WAIT_OBJECT_0)
	{
		printf("WaitForSingleObject(timer, INFINITE) failure\n");
		goto out;
	}

	printf("Timer Signaled\n");

	if (!SetWaitableTimer(timer, &due, period, NULL, NULL, 0))
	{
		printf("SetWaitableTimer failure\n");
		goto out;
	}

	if (WaitForMultipleObjects(1, &timer, FALSE, INFINITE) != WAIT_OBJECT_0)
	{
		printf("WaitForMultipleObjects(timer, INFINITE) failure\n");
		goto out;
	}

	printf("Timer Signaled\n");
	result = 0;
out:
	CloseHandle(timer);
	return result;
}

