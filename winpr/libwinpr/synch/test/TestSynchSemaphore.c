
#include <winpr/crt.h>
#include <winpr/synch.h>

int TestSynchSemaphore(int argc, char* argv[])
{
	HANDLE semaphore = nullptr;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	semaphore = CreateSemaphore(nullptr, 0, 1, nullptr);

	if (!semaphore)
	{
		printf("CreateSemaphore failure\n");
		return -1;
	}

	(void)CloseHandle(semaphore);

	return 0;
}
