
#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/stream.h>

static BOOL TestStream_Verify(wStream* s, int mincap, int len, int pos)
{
	if (Stream_Buffer(s) == NULL)
	{
		printf("stream buffer is null\n");
		return FALSE;
	}
	if (Stream_Pointer(s) == NULL)
	{
		printf("stream pointer is null\n");
		return FALSE;
	}
	if (Stream_Pointer(s) < Stream_Buffer(s))
	{
		printf("stream pointer (%p) or buffer (%p) is invalid\n",
			Stream_Pointer(s), Stream_Buffer(s));
		return FALSE;
	}
	if (Stream_Capacity(s) < mincap) {
		printf("stream capacity is %ld but minimum expected value is %d\n",
			Stream_Capacity(s), mincap);
		return FALSE;
	}
	if (Stream_Length(s) != len) {
		printf("stream has unexpected length (%ld instead of %d)\n",
			Stream_Length(s), len);
		return FALSE;
	}
	if (Stream_GetPosition(s) != pos)
	{
		printf("stream has unexpected position (%ld instead of %d)\n",
			Stream_GetPosition(s), pos);
		return FALSE;
	}
	if (Stream_GetPosition(s) > Stream_Length(s))
	{
		printf("stream position (%ld) exceeds length (%ld)\n",
			Stream_GetPosition(s), Stream_Length(s));
		return FALSE;
	}
	if (Stream_GetPosition(s) > Stream_Capacity(s))
	{
		printf("stream position (%ld) exceeds capacity (%ld)\n",
			Stream_GetPosition(s), Stream_Capacity(s));
		return FALSE;
	}
	if (Stream_Length(s) > Stream_Capacity(s))
	{
		printf("stream length (%ld) exceeds capacity (%ld)\n",
			Stream_Length(s), Stream_Capacity(s));
		return FALSE;
	}
	if (Stream_GetRemainingLength(s) != len - pos)
	{
		printf("stream remaining length (%ld instead of %d)\n",
			Stream_GetRemainingLength(s), len - pos);
		return FALSE;
	}

	return TRUE;
}

static BOOL TestStream_New()
{
	wStream *s = NULL;
	/* Test creation of a 0-size stream with no buffer */
	s = Stream_New(NULL, 0);
	if (s)
		return FALSE;
	return TRUE;
}


static BOOL TestStream_Create(int count, BOOL selfAlloc)
{
	int i, len, cap, pos;
	wStream *s = NULL;
	void* buffer = NULL;

	for (i = 0; i < count; i++)
	{
		len = cap = i+1;
		pos = 0;

		if (selfAlloc)
		{
			if (!(buffer = malloc(cap)))
			{
				printf("%s: failed to allocate buffer of size %d\n", __FUNCTION__, cap);
				goto fail;
			}
		}

		if (!(s = Stream_New(selfAlloc ? buffer : NULL, len)))
		{
			printf("%s: Stream_New failed for stream #%d\n", __FUNCTION__, i);
			goto fail;
		}

		if (!TestStream_Verify(s, cap, len, pos))
		{
			goto fail;
		}

		for (pos = 0; pos < len; pos++)
		{
			Stream_SetPosition(s, pos);
			Stream_SealLength(s);
			if (!TestStream_Verify(s, cap, pos, pos))
			{
				goto fail;
			}
		}

		if (selfAlloc)
		{
			memset(buffer, i%256, cap);
			if (memcmp(buffer, Stream_Buffer(s), cap))
			{
				printf("%s: buffer memory corruption\n", __FUNCTION__);
				goto fail;
			}
		}

		Stream_Free(s, buffer ? FALSE : TRUE);
		free(buffer);
	}

	return TRUE;

fail:
	free(buffer);
	if (s)
	{
		Stream_Free(s, buffer ? FALSE : TRUE);
	}

	return FALSE;
}

static BOOL TestStream_Extent(UINT32 maxSize)
{
	int i;
	wStream *s = NULL;
	BOOL result = FALSE;

	if (!(s = Stream_New(NULL, 1)))
	{
		printf("%s: Stream_New failed\n", __FUNCTION__);
		return FALSE;
	}

	for (i = 1; i < maxSize; i++)
	{
		if (i % 2)
		{
			if (!Stream_EnsureRemainingCapacity(s, i))
				goto fail;
		}
		else
		{
			if (!Stream_EnsureCapacity(s, i))
				goto fail;
		}

		Stream_SetPosition(s, i);
		Stream_SealLength(s);

		if (!TestStream_Verify(s, i, i, i))
		{
			printf("%s: failed to verify stream in iteration %d\n", __FUNCTION__, i);
			goto fail;
		}
	}

	result = TRUE;

fail:
	if (s)
	{
		Stream_Free(s, TRUE);
	}

	return result;
}

#define Stream_Peek_UINT8_BE	Stream_Peek_UINT8
#define Stream_Read_UINT8_BE	Stream_Read_UINT8
#define Stream_Peek_INT8_BE	Stream_Peek_UINT8
#define Stream_Read_INT8_BE	Stream_Read_UINT8

#define TestStream_PeekAndRead(_s, _r, _t) do                                \
{                                                                            \
  _t _a, _b;                                                                 \
  int _i;                                                                    \
  BYTE* _p = Stream_Buffer(_s);                                              \
  Stream_SetPosition(_s, 0);                                                 \
  Stream_Peek_ ## _t(_s, _a);                                                \
  Stream_Read_ ## _t(_s, _b);                                                \
  if (_a != _b)                                                              \
  {                                                                          \
    printf("%s: test1 " #_t "_LE failed\n", __FUNCTION__);                   \
    _r = FALSE;                                                              \
  }                                                                          \
  for (_i=0; _i<sizeof(_t); _i++) {                                          \
    if (((BYTE*)&_a)[_i] != _p[_i]) {                                        \
      printf("%s: test2 " #_t "_LE failed\n", __FUNCTION__);                 \
      _r = FALSE;                                                            \
      break;                                                                 \
    }                                                                        \
  }                                                                          \
  /* printf("a: 0x%016llX\n", a); */                                         \
  Stream_SetPosition(_s, 0);                                                 \
  Stream_Peek_ ## _t ## _BE(_s, _a);                                         \
  Stream_Read_ ## _t ## _BE(_s, _b);                                         \
  if (_a != _b)                                                              \
  {                                                                          \
    printf("%s: test1 " #_t "_BE failed\n", __FUNCTION__);                   \
    _r = FALSE;                                                              \
  }                                                                          \
  for (_i=0; _i<sizeof(_t); _i++) {                                          \
    if (((BYTE*)&_a)[_i] != _p[sizeof(_t)-_i-1]) {                           \
      printf("%s: test2 " #_t "_BE failed\n", __FUNCTION__);                 \
      _r = FALSE;                                                            \
      break;                                                                 \
    }                                                                        \
  }                                                                          \
  /* printf("a: 0x%016llX\n", a); */                                         \
} while (0)


static BOOL TestStream_Reading(void)
{
	BYTE src[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };

	wStream *s = NULL;
	BOOL result = TRUE;

	if (!(s = Stream_New(src, sizeof(src))))
	{
		printf("%s: Stream_New failed\n", __FUNCTION__);
		return FALSE;
	}

	TestStream_PeekAndRead(s, result, UINT8);
	TestStream_PeekAndRead(s, result, INT8);
	TestStream_PeekAndRead(s, result, UINT16);
	TestStream_PeekAndRead(s, result, INT16);
	TestStream_PeekAndRead(s, result, UINT32);
	TestStream_PeekAndRead(s, result, INT32);
	TestStream_PeekAndRead(s, result, UINT64);
	TestStream_PeekAndRead(s, result, INT64);

	Stream_Free(s, FALSE);

	return result;
}


int TestStream(int argc, char* argv[])
{
	if (!TestStream_Create(200, FALSE))
		return 1;

	if (!TestStream_Create(200, TRUE))
		return 2;

	if (!TestStream_Extent(4096))
		return 3;

	if (!TestStream_Reading())
		return 4;

	if (!TestStream_New())
		return 5;
	/**
	 * FIXME: Add tests for
	 * Stream_Write_*
	 * Stream_Seek_*
	 * Stream_Rewind_*
	 * Stream_Zero
	 * Stream_Fill
	 * Stream_Copy
	 */

	return 0;
}

