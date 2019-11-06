
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
	if (!queue)
		return -1;

	for (index = 1; index <= 10; index++)
	{
		Queue_Enqueue(queue, (void*)(size_t)index);
	}

	count = Queue_Count(queue);
	printf("queue count: %d\n", count);

	for (index = 1; index <= 10; index++)
	{
		item = (int)(size_t)Queue_Dequeue(queue);

		if (item != index)
			return -1;
	}

	count = Queue_Count(queue);
	printf("queue count: %d\n", count);

	Queue_Enqueue(queue, (void*)(size_t)1);
	Queue_Enqueue(queue, (void*)(size_t)2);
	Queue_Enqueue(queue, (void*)(size_t)3);

	Queue_Dequeue(queue);
	Queue_Dequeue(queue);

	Queue_Enqueue(queue, (void*)(size_t)4);
	Queue_Enqueue(queue, (void*)(size_t)5);
	Queue_Enqueue(queue, (void*)(size_t)6);

	Queue_Dequeue(queue);
	Queue_Dequeue(queue);
	Queue_Dequeue(queue);
	Queue_Dequeue(queue);

	Queue_Clear(queue);
	Queue_Free(queue);

	return 0;
}
