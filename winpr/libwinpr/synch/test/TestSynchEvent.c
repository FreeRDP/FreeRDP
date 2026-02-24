
#include <winpr/crt.h>
#include <winpr/synch.h>

int TestSynchEvent(int argc, char* argv[])
{
	HANDLE event = nullptr;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	if (ResetEvent(nullptr))
	{
		printf("ResetEvent(nullptr) unexpectedly succeeded\n");
		return -1;
	}

	if (SetEvent(nullptr))
	{
		printf("SetEvent(nullptr) unexpectedly succeeded\n");
		return -1;
	}

	event = CreateEvent(nullptr, TRUE, TRUE, nullptr);

	if (!event)
	{
		printf("CreateEvent failure\n");
		return -1;
	}

	if (WaitForSingleObject(event, INFINITE) != WAIT_OBJECT_0)
	{
		printf("WaitForSingleObject failure 1\n");
		return -1;
	}

	if (!ResetEvent(event))
	{
		printf("ResetEvent failure with signaled event object\n");
		return -1;
	}

	if (WaitForSingleObject(event, 0) != WAIT_TIMEOUT)
	{
		printf("WaitForSingleObject failure 2\n");
		return -1;
	}

	if (!ResetEvent(event))
	{
		/* Note: ResetEvent must also succeed if event is currently nonsignaled */
		printf("ResetEvent failure with nonsignaled event object\n");
		return -1;
	}

	if (!SetEvent(event))
	{
		printf("SetEvent failure with nonsignaled event object\n");
		return -1;
	}

	if (WaitForSingleObject(event, 0) != WAIT_OBJECT_0)
	{
		printf("WaitForSingleObject failure 3\n");
		return -1;
	}

	for (int i = 0; i < 10000; i++)
	{
		if (!SetEvent(event))
		{
			printf("SetEvent failure with signaled event object (i = %d)\n", i);
			return -1;
		}
	}

	if (!ResetEvent(event))
	{
		printf("ResetEvent failure after multiple SetEvent calls\n");
		return -1;
	}

	/* Independent of the amount of the previous SetEvent calls, a single
	   ResetEvent must be sufficient to get into nonsignaled state */

	if (WaitForSingleObject(event, 0) != WAIT_TIMEOUT)
	{
		printf("WaitForSingleObject failure 4\n");
		return -1;
	}

	(void)CloseHandle(event);

	return 0;
}
