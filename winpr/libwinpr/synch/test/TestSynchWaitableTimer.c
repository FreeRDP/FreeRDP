
#include <winpr/crt.h>
#include <winpr/synch.h>

int TestSynchWaitableTimer(int argc, char* argv[])
{
	DWORD status;
	HANDLE timer;
	LONG period;
	LARGE_INTEGER due;
	int result = -1;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	timer = CreateWaitableTimer(NULL, FALSE, NULL);

	if (!timer)
	{
		printf("CreateWaitableTimer failure\n");
		goto out;
	}

	due.QuadPart = -1500000LL; /* 0.15 seconds */

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
	status = WaitForSingleObject(timer, 200);

	if (status != WAIT_TIMEOUT)
	{
		printf("WaitForSingleObject(timer, 200) failure: Actual: 0x%08" PRIX32
		       ", Expected: 0x%08X\n",
		       status, WAIT_TIMEOUT);
		goto out;
	}

	due.QuadPart = 0;
	period = 120; /* 0.12 seconds */

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
