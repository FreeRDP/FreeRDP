
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/collections.h>

static bool wrap_test(bool (*fkt)(wQueue* queue))
{
	wQueue* queue = Queue_New(TRUE, -1, -1);
	if (!queue)
		return false;

	WINPR_ASSERT(fkt);
	const bool rc = fkt(queue);
	Queue_Free(queue);
	return rc;
}

static bool check(const void* ptr, size_t pos)
{
	if (!ptr)
		return false;
	if (ptr != (void*)(pos + 23))
		return false;
	return true;
}

static bool append(wQueue* queue, size_t pos)
{
	void* ptr = (void*)(pos + 23);
	return Queue_Enqueue(queue, ptr);
}

static bool fill_capcity(wQueue* queue, size_t* pos)
{
	WINPR_ASSERT(pos);

	size_t cpos = *pos;
	const size_t capacity = Queue_Capacity(queue);
	while (Queue_Count(queue) < capacity)
	{
		if (!append(queue, cpos++))
			return false;
	}
	*pos = cpos;
	return true;
}

static bool drain(wQueue* queue, size_t expect)
{
	void* ptr = Queue_Dequeue(queue);
	return check(ptr, expect);
}

static bool drain_capcity(wQueue* queue, size_t remain, size_t* pos)
{
	WINPR_ASSERT(pos);

	size_t cpos = *pos;
	while (Queue_Count(queue) > remain)
	{
		if (!drain(queue, cpos++))
			return false;
	}
	*pos = cpos;

	return true;
}

static bool test_growth_move(wQueue* queue, bool big)
{
	WINPR_ASSERT(queue);

	const size_t cap = Queue_Capacity(queue);
	if (cap < 4)
		return false;

	size_t wpos = 0;
	size_t rpos = 0;
	if (!fill_capcity(queue, &wpos))
		return false;

	/* Ensure the (base) capacity is larger than the allocation step
	 * so a full copy of tail will not be possible */
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

static bool test_growth_big_move(wQueue* queue)
{
	return test_growth_move(queue, true);
}
static bool test_growth_small_move(wQueue* queue)
{
	return test_growth_move(queue, false);
}

static bool check_size(wQueue* queue, size_t expected)
{
	WINPR_ASSERT(queue);
	const size_t count = Queue_Count(queue);
	printf("queue count: %" PRIuz "\n", count);
	if (count != expected)
		return false;
	return true;
}

static bool enqueue(wQueue* queue, size_t val)
{
	WINPR_ASSERT(queue);
	void* ptr = (void*)(23 + val);
	return Queue_Enqueue(queue, ptr);
}

static bool dequeue(wQueue* queue, size_t expected)
{
	WINPR_ASSERT(queue);
	const void* pexpect = (void*)(23 + expected);
	void* ptr = Queue_Dequeue(queue);
	if (pexpect != ptr)
		return false;
	return true;
}

static bool legacy_test(wQueue* queue)
{
	WINPR_ASSERT(queue);

	for (size_t index = 1; index <= 10; index++)
	{
		if (!enqueue(queue, index))
			return false;
	}

	if (!check_size(queue, 10))
		return false;

	for (size_t index = 1; index <= 10; index++)
	{
		if (!dequeue(queue, index))
			return false;
	}

	if (!check_size(queue, 0))
		return false;

	if (!enqueue(queue, 1))
		return false;
	if (!enqueue(queue, 2))
		return false;
	if (!enqueue(queue, 3))
		return false;

	if (!check_size(queue, 3))
		return false;

	if (!dequeue(queue, 1))
		return false;
	if (!dequeue(queue, 2))
		return false;

	if (!check_size(queue, 1))
		return false;

	if (!enqueue(queue, 4))
		return false;
	if (!enqueue(queue, 5))
		return false;
	if (!enqueue(queue, 6))
		return false;

	if (!check_size(queue, 4))
		return false;

	if (!dequeue(queue, 3))
		return false;
	if (!dequeue(queue, 4))
		return false;
	if (!dequeue(queue, 5))
		return false;
	if (!dequeue(queue, 6))
		return false;

	if (!check_size(queue, 0))
		return false;

	Queue_Clear(queue);

	if (!check_size(queue, 0))
		return false;

	for (size_t x = 0; x < 32; x++)
	{
		void* ptr = (void*)(23 + x);
		if (!Queue_Enqueue(queue, ptr))
			return false;
	}

	if (!check_size(queue, 32))
		return false;

	Queue_Clear(queue);

	if (!check_size(queue, 0))
		return false;
	return true;
}

int TestQueue(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char* argv[])
{
	if (!wrap_test(test_growth_big_move))
		return -1;
	if (!wrap_test(test_growth_small_move))
		return -2;
	if (!wrap_test(legacy_test))
		return -3;
	return 0;
}
