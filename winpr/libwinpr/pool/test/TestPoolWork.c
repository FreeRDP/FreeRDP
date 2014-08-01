
#include <winpr/crt.h>
#include <winpr/pool.h>
#include <winpr/interlocked.h>

static LONG count = 0;

void CALLBACK test_WorkCallback(PTP_CALLBACK_INSTANCE instance, void* context, PTP_WORK work)
{
	int index;
	BYTE a[1024];
	BYTE b[1024];
	BYTE c[1024];

	printf("Hello %s: %d (thread: %d)\n", (char*) context,
		InterlockedIncrement(&count), GetCurrentThreadId());

	for (index = 0; index < 100; index++)
	{
		ZeroMemory(a, 1024);
		ZeroMemory(b, 1024);
		ZeroMemory(c, 1024);
		FillMemory(a, 1024, 0xAA);
		FillMemory(b, 1024, 0xBB);
		CopyMemory(c, a, 1024);
		CopyMemory(c, b, 1024);
	}
}

int TestPoolWork(int argc, char* argv[])
{
	int index;
	PTP_POOL pool;
	PTP_WORK work;
	PTP_CLEANUP_GROUP cleanupGroup;
	TP_CALLBACK_ENVIRON environment;

	printf("Global Thread Pool\n");

	work = CreateThreadpoolWork((PTP_WORK_CALLBACK) test_WorkCallback, "world", NULL);

	if (!work)
	{
		printf("CreateThreadpoolWork failure\n");
		return -1;
	}

	/**
	 * You can post a work object one or more times (up to MAXULONG) without waiting for prior callbacks to complete.
	 * The callbacks will execute in parallel. To improve efficiency, the thread pool may throttle the threads.
	 */

	for (index = 0; index < 10; index++)
		SubmitThreadpoolWork(work);

	WaitForThreadpoolWorkCallbacks(work, FALSE);
	CloseThreadpoolWork(work);

	printf("Private Thread Pool\n");

	pool = CreateThreadpool(NULL);

	SetThreadpoolThreadMinimum(pool, 4);
	SetThreadpoolThreadMaximum(pool, 8);

	InitializeThreadpoolEnvironment(&environment);
	SetThreadpoolCallbackPool(&environment, pool);

	cleanupGroup = CreateThreadpoolCleanupGroup();

	if (!cleanupGroup)
	{
		printf("CreateThreadpoolCleanupGroup failure\n");
		return -1;
	}

	SetThreadpoolCallbackCleanupGroup(&environment, cleanupGroup, NULL);

	work = CreateThreadpoolWork((PTP_WORK_CALLBACK) test_WorkCallback, "world", &environment);

	if (!work)
	{
		printf("CreateThreadpoolWork failure\n");
		return -1;
	}

	for (index = 0; index < 10; index++)
		SubmitThreadpoolWork(work);

	WaitForThreadpoolWorkCallbacks(work, FALSE);

	CloseThreadpoolCleanupGroupMembers(cleanupGroup, TRUE, NULL);

	CloseThreadpoolCleanupGroup(cleanupGroup);

	DestroyThreadpoolEnvironment(&environment);

	CloseThreadpoolWork(work);
	CloseThreadpool(pool);

	return 0;
}
