
#include <winpr/crt.h>
#include <winpr/pool.h>

void test_WorkCallback(PTP_CALLBACK_INSTANCE instance, void* context, PTP_WORK work)
{
	printf("Hello %s", context);
}

int TestPoolWork(int argc, char* argv[])
{
	TP_WORK * work;

	work = CreateThreadpoolWork(test_WorkCallback, "world", NULL);

	if (!work)
	{
		printf("CreateThreadpoolWork failure\n");
		return -1;
	}

	SubmitThreadpoolWork(work);
	WaitForThreadpoolWorkCallbacks(work, FALSE);
	CloseThreadpoolWork(work);

	return 0;
}

