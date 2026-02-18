
#include <winpr/crt.h>
#include <winpr/thread.h>
#include <winpr/collections.h>

static DWORD WINAPI message_queue_consumer_thread(LPVOID arg)
{
	wMessage message = { 0 };
	wMessageQueue* queue = (wMessageQueue*)arg;

	while (MessageQueue_Wait(queue))
	{
		if (MessageQueue_Peek(queue, &message, TRUE))
		{
			if (message.id == WMQ_QUIT)
				break;

			printf("Message.Type: %" PRIu32 "\n", message.id);
		}
	}

	return 0;
}

static bool wrap_test(bool (*fkt)(wMessageQueue* queue))
{
	wMessageQueue* queue = MessageQueue_New(NULL);
	if (!queue)
		return false;

	WINPR_ASSERT(fkt);
	const bool rc = fkt(queue);
	MessageQueue_Free(queue);
	return rc;
}

static bool check(const wMessage* message, size_t pos)
{
	if (!message)
		return false;
	if (message->context != (void*)13)
		return false;
	if (message->id != pos)
		return false;
	if (message->wParam != (void*)23)
		return false;
	if (message->lParam != (void*)42)
		return false;
	if (message->Free != NULL)
		return false;
	return true;
}

static bool append(wMessageQueue* queue, size_t pos)
{
	const wMessage message = { .context = (void*)13,
		                       .id = WINPR_ASSERTING_INT_CAST(DWORD, pos),
		                       .wParam = (void*)23,
		                       .lParam = (void*)42,
		                       .Free = NULL };

	return MessageQueue_Dispatch(queue, &message);
}

static bool fill_capcity(wMessageQueue* queue, size_t* pos)
{
	WINPR_ASSERT(pos);

	size_t cpos = *pos;
	const size_t capacity = MessageQueue_Capacity(queue);
	while (MessageQueue_Size(queue) < capacity)
	{
		if (!append(queue, cpos++))
			return false;
	}
	*pos = cpos;
	return true;
}

static bool drain(wMessageQueue* queue, size_t expect)
{
	wMessage message = { 0 };
	if (MessageQueue_Get(queue, &message) < 0)
		return false;
	if (!check(&message, expect))
		return false;
	return true;
}

static bool drain_capcity(wMessageQueue* queue, size_t remain, size_t* pos)
{
	WINPR_ASSERT(pos);

	size_t cpos = *pos;
	while (MessageQueue_Size(queue) > remain)
	{
		if (!drain(queue, cpos++))
			return false;
	}
	*pos = cpos;

	return true;
}

static bool test_growth_move(wMessageQueue* queue, bool big)
{
	WINPR_ASSERT(queue);

	const size_t cap = MessageQueue_Capacity(queue);
	if (cap < 4)
		return false;

	size_t wpos = 0;
	size_t rpos = 0;
	if (!fill_capcity(queue, &wpos))
		return false;

	if (big)
	{
		if (!append(queue, wpos++))
			return false;
	}

	if (!drain_capcity(queue, 3, &rpos))
		return false;

	if (!fill_capcity(queue, &wpos))
		return false;

	if (!append(queue, wpos++))
		return false;

	return drain_capcity(queue, 0, &rpos);
}

static bool test_growth_big_move(wMessageQueue* queue)
{
	return test_growth_move(queue, true);
}

static bool test_growth_small_move(wMessageQueue* queue)
{
	return test_growth_move(queue, false);
}

static bool test_operation_run(wMessageQueue* queue, HANDLE thread)
{
	WINPR_ASSERT(queue);
	WINPR_ASSERT(thread);

	if (!MessageQueue_Post(queue, NULL, 123, NULL, NULL))
		return false;

	if (!MessageQueue_Post(queue, NULL, 456, NULL, NULL))
		return false;

	if (!MessageQueue_Post(queue, NULL, 789, NULL, NULL))
		return false;

	if (!MessageQueue_PostQuit(queue, 0))
		return false;

	const DWORD status = WaitForSingleObject(thread, INFINITE);
	if (status != WAIT_OBJECT_0)
		return false;

	return true;
}

static bool test_operation(wMessageQueue* queue)
{
	WINPR_ASSERT(queue);

	HANDLE thread = CreateThread(NULL, 0, message_queue_consumer_thread, queue, 0, NULL);
	if (!thread)
	{
		printf("failed to create thread\n");
		return false;
	}
	const bool rc = test_operation_run(queue, thread);
	if (!CloseHandle(thread))
		return false;
	return rc;
}

int TestMessageQueue(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char* argv[])
{
	if (!wrap_test(test_growth_big_move))
		return -1;
	if (!wrap_test(test_growth_small_move))
		return -2;
	if (!wrap_test(test_operation))
		return -3;
	return 0;
}
