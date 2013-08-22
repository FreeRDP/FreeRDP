
#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

#include <winpr/error.h>

static int status = 0;

static DWORD errors[4] =
{
	ERROR_INVALID_DATA,
	ERROR_BROKEN_PIPE,
	ERROR_INVALID_NAME,
	ERROR_BAD_ARGUMENTS
};

static void* test_error_thread(void* arg)
{
	int id;
	DWORD error;

	id = (int) (size_t) arg;

	error = errors[id];

	SetLastError(error);

	Sleep(10);

	error = GetLastError();

	if (error != errors[id])
	{
		printf("GetLastError() failure (thread %d): Expected: 0x%04X, Actual: 0x%04X\n",
				id, errors[id], error);

		if (!status)
			status = -1;

		return NULL;
	}

	return NULL;
}

int TestErrorSetLastError(int argc, char* argv[])
{
	DWORD error;
	HANDLE threads[4];

	SetLastError(ERROR_ACCESS_DENIED);

	error = GetLastError();

	if (error != ERROR_ACCESS_DENIED)
	{
		printf("GetLastError() failure: Expected: 0x%04X, Actual: 0x%04X\n",
				ERROR_ACCESS_DENIED, error);
		return -1;
	}

	threads[0] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) test_error_thread, (void*) (size_t) 0, 0, NULL);
	threads[1] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) test_error_thread, (void*) (size_t) 1, 0, NULL);
	threads[2] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) test_error_thread, (void*) (size_t) 2, 0, NULL);
	threads[3] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) test_error_thread, (void*) (size_t) 3, 0, NULL);

	WaitForSingleObject(threads[0], INFINITE);
	WaitForSingleObject(threads[1], INFINITE);
	WaitForSingleObject(threads[2], INFINITE);
	WaitForSingleObject(threads[3], INFINITE);

	CloseHandle(threads[0]);
	CloseHandle(threads[1]);
	CloseHandle(threads[2]);
	CloseHandle(threads[3]);

	error = GetLastError();

	if (error != ERROR_ACCESS_DENIED)
	{
		printf("GetLastError() failure: Expected: 0x%04X, Actual: 0x%04X\n",
				ERROR_ACCESS_DENIED, error);
		return -1;
	}

	return status;
}

