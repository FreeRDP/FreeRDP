
#include <winpr/crt.h>
#include <winpr/synch.h>

int TestSynchSemaphore(int argc, char* argv[])
{
	HANDLE semaphore = NULL;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	semaphore = CreateSemaphore(NULL, 0, 1, NULL);

	if (!semaphore)
	{
		printf("CreateSemaphore failure\n");
		return -1;
	}

	(void)CloseHandle(semaphore);

	return 0;
}
