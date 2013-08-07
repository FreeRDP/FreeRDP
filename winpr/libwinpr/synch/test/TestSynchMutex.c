
#include <winpr/crt.h>
#include <winpr/synch.h>

int TestSynchMutex(int argc, char* argv[])
{
	HANDLE mutex;

	mutex = CreateMutex(NULL, FALSE, NULL);

	if (!mutex)
	{
		printf("CreateMutex failure\n");
		return -1;
	}

	CloseHandle(mutex);

	return 0;
}

