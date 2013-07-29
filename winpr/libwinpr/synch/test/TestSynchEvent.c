
#include <winpr/crt.h>
#include <winpr/synch.h>

int TestSynchEvent(int argc, char* argv[])
{
	HANDLE event;

	event = CreateEvent(NULL, TRUE, TRUE, NULL);

	if (!event)
	{
		printf("CreateEvent failure\n");
		return -1;
	}

	if (WaitForSingleObject(event, INFINITE) != WAIT_OBJECT_0)
	{
		printf("WaitForSingleObject(event, INFINITE) failure\n");
		return -1;
	}

	ResetEvent(event);

	if (WaitForSingleObject(event, 0) != WAIT_TIMEOUT)
	{
		printf("WaitForSingleObject(event, 0) failure\n");
		return -1;
	}

	SetEvent(event);

	if (WaitForSingleObject(event, 0) != WAIT_OBJECT_0)
	{
		printf("WaitForSingleObject(event, 0) failure\n");
		return -1;
	}

	CloseHandle(event);

	return 0;
}
