
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/windows.h>
#include <winpr/interlocked.h>

int TestInterlockedAccess(int argc, char* argv[])
{
	int index;
	LONG* Addend;
	LONG* Target;
	LONG oldValue;
	LONG* Destination;
	LONGLONG oldValue64;
	LONGLONG* Destination64;

	/* InterlockedIncrement */

	Addend = _aligned_malloc(sizeof(LONG), sizeof(LONG));

	*Addend = 0;

	for (index = 0; index < 10; index ++)
		InterlockedIncrement(Addend);

	if (*Addend != 10)
	{
		printf("InterlockedIncrement failure: Actual: %d, Expected: %d\n", (int) *Addend, 10);
		return -1;
	}

	/* InterlockedDecrement */

	for (index = 0; index < 10; index ++)
		InterlockedDecrement(Addend);

	if (*Addend != 0)
	{
		printf("InterlockedDecrement failure: Actual: %d, Expected: %d\n", (int) *Addend, 0);
		return -1;
	}

	/* InterlockedExchange */

	Target = _aligned_malloc(sizeof(LONG), sizeof(LONG));

	*Target = 0xAA;

	oldValue = InterlockedExchange(Target, 0xFF);

	if (oldValue != 0xAA)
	{
		printf("InterlockedExchange failure: Actual: 0x%08X, Expected: 0x%08X\n", (int) oldValue, 0xAA);
		return -1;
	}

	if (*Target != 0xFF)
	{
		printf("InterlockedExchange failure: Actual: 0x%08X, Expected: 0x%08X\n", (int) *Target, 0xFF);
		return -1;
	}

	/* InterlockedExchangeAdd */

	*Addend = 25;

	oldValue = InterlockedExchangeAdd(Addend, 100);

	if (oldValue != 25)
	{
		printf("InterlockedExchangeAdd failure: Actual: %d, Expected: %d\n", (int) oldValue, 25);
		return -1;
	}

	if (*Addend != 125)
	{
		printf("InterlockedExchangeAdd failure: Actual: %d, Expected: %d\n", (int) *Addend, 125);
		return -1;
	}

	/* InterlockedCompareExchange (*Destination == Comparand) */

	Destination = _aligned_malloc(sizeof(LONG), sizeof(LONG));

	*Destination = 0xAABBCCDD;

	oldValue = InterlockedCompareExchange(Destination, 0xCCDDEEFF, 0xAABBCCDD);

	if (oldValue != 0xAABBCCDD)
	{
		printf("InterlockedCompareExchange failure: Actual: 0x%08X, Expected: 0x%08X\n", (int) oldValue, 0xAABBCCDD);
		return -1;
	}

	if (*Destination != 0xCCDDEEFF)
	{
		printf("InterlockedCompareExchange failure: Actual: 0x%08X, Expected: 0x%08X\n", (int) *Destination, 0xCCDDEEFF);
		return -1;
	}

	/* InterlockedCompareExchange (*Destination != Comparand) */

	*Destination = 0xAABBCCDD;

	oldValue = InterlockedCompareExchange(Destination, 0xCCDDEEFF, 0x66778899);

	if (oldValue != 0xAABBCCDD)
	{
		printf("InterlockedCompareExchange failure: Actual: 0x%08X, Expected: 0x%08X\n", (int) oldValue, 0xAABBCCDD);
		return -1;
	}

	if (*Destination != 0xAABBCCDD)
	{
		printf("InterlockedCompareExchange failure: Actual: 0x%08X, Expected: 0x%08X\n", (int) *Destination, 0xAABBCCDD);
		return -1;
	}

	/* InterlockedCompareExchange64 (*Destination == Comparand) */

	Destination64 = _aligned_malloc(sizeof(LONGLONG), sizeof(LONGLONG));

	*Destination64 = 0x66778899AABBCCDD;

	oldValue64 = InterlockedCompareExchange64(Destination64, 0x8899AABBCCDDEEFF, 0x66778899AABBCCDD);

	if (oldValue64 != 0x66778899AABBCCDD)
	{
		printf("InterlockedCompareExchange failure: Actual: %lld, Expected: %lld\n", oldValue64, (LONGLONG) 0x66778899AABBCCDD);
		return -1;
	}

	if (*Destination64 != 0x8899AABBCCDDEEFF)
	{
		printf("InterlockedCompareExchange failure: Actual: %lld, Expected: %lld\n",  *Destination64, (LONGLONG) 0x8899AABBCCDDEEFF);
		return -1;
	}

	/* InterlockedCompareExchange64 (*Destination != Comparand) */

	*Destination64 = 0x66778899AABBCCDD;

	oldValue64 = InterlockedCompareExchange64(Destination64, 0x8899AABBCCDDEEFF, 12345);

	if (oldValue64 != 0x66778899AABBCCDD)
	{
		printf("InterlockedCompareExchange failure: Actual: %lld, Expected: %lld\n", oldValue64, (LONGLONG) 0x66778899AABBCCDD);
		return -1;
	}

	if (*Destination64 != 0x66778899AABBCCDD)
	{
		printf("InterlockedCompareExchange failure: Actual: %lld, Expected: %lld\n",  *Destination64, (LONGLONG) 0x66778899AABBCCDD);
		return -1;
	}

	_aligned_free(Addend);
	_aligned_free(Target);
	_aligned_free(Destination);
	_aligned_free(Destination64);

	return 0;
}
