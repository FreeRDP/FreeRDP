
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/collections.h>

int TestQueue(int argc, char* argv[])
{
	int item;
	int index;
	int count;
	wQueue* queue;

	queue = Queue_New(TRUE, -1, -1);

	for (index = 1; index <= 10; index++)
	{
		Queue_Enqueue(queue, (void*) (size_t) index);
	}

	count = Queue_Count(queue);

	printf("queue count: %d\n", count);

	for (index = 1; index <= 10; index++)
	{
		item = (int) (size_t) Queue_Dequeue(queue);

		if (item != index)
			return -1;
	}

	Queue_Clear(queue);
	Queue_Free(queue);

	return 0;
}
