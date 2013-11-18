
#include <winpr/crt.h>
#include <winpr/synch.h>

int TestSynchMutex(int argc, char* argv[])
{
	DWORD rc;
	HANDLE mutex;

	mutex = CreateMutex(NULL, FALSE, NULL);
	if (!mutex)
	{
		printf("CreateMutex failure\n");
		return -1;
	}

	/* Lock the mutex */
	rc = WaitForSingleObject(mutex, INFINITE);
	if (WAIT_OBJECT_0 != rc)
	{
		printf("WaitForSingleObject on mutex failed with %d\n", rc);
		return -2;
	}

	/* TryLock should now fail. */
	rc = WaitForSingleObject(mutex, 0);
	if (WAIT_TIMEOUT != rc)
	{
		printf("Timed WaitForSingleObject on locked mutex failed with %d\n", rc);
		return -3;
	}

	/* Unlock the mutex */
	rc = ReleaseMutex(mutex);
	if (!rc)
	{
		printf("ReleaseMutex failed.\n");
		return -4;
	}

	/* TryLock should now succeed. */
	rc = WaitForSingleObject(mutex, 0);
	if (WAIT_OBJECT_0 != rc)
	{
		printf("Timed WaitForSingleObject on free mutex failed with %d\n", rc);
		return -5;
	}

	CloseHandle(mutex);

	return 0;
}

