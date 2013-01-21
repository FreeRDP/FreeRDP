
#include <winpr/crt.h>
#include <winpr/pool.h>

static int count = 0;

void test_WorkCallback(PTP_CALLBACK_INSTANCE instance, void* context, PTP_WORK work)
{
	printf("Hello %s: %d\n", context, count++);
}

int TestPoolWork(int argc, char* argv[])
{
	int index;
	TP_WORK * work;

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

	return 0;
}
