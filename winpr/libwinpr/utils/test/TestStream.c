#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/stream.h>

static BOOL TestStream_Verify(wStream* s, int mincap, int len, size_t pos)
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
			(void*) Stream_Pointer(s), (void*) Stream_Buffer(s));
		return FALSE;
	}
	if (Stream_Capacity(s) < mincap) {
		printf("stream capacity is %"PRIuz" but minimum expected value is %d\n",
			Stream_Capacity(s), mincap);
		return FALSE;
	}
	if (Stream_Length(s) != len) {
		printf("stream has unexpected length (%"PRIuz" instead of %d)\n",
			Stream_Length(s), len);
		return FALSE;
	}
	if (Stream_GetPosition(s) != pos)
	{
		printf("stream has unexpected position (%"PRIuz" instead of %d)\n",
			Stream_GetPosition(s), pos);
		return FALSE;
	}
	if (Stream_GetPosition(s) > Stream_Length(s))
	{
		printf("stream position (%"PRIuz") exceeds length (%"PRIuz")\n",
			Stream_GetPosition(s), Stream_Length(s));
		return FALSE;
	}
	if (Stream_GetPosition(s) > Stream_Capacity(s))
	{
		printf("stream position (%"PRIuz") exceeds capacity (%"PRIuz")\n",
			Stream_GetPosition(s), Stream_Capacity(s));
		return FALSE;
	}
	if (Stream_Length(s) > Stream_Capacity(s))
	{
		printf("stream length (%"PRIuz") exceeds capacity (%"PRIuz")\n",
			Stream_Length(s), Stream_Capacity(s));
		return FALSE;
	}
	if (Stream_GetRemainingLength(s) != len - pos)
	{
		printf("stream remaining length (%"PRIuz" instead of %d)\n",
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
	UINT32 i;
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
			printf("%s: failed to verify stream in iteration %"PRIu32"\n", __FUNCTION__, i);
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
    if (((_a >> (_i * 8)) & 0xFF) != _p[_i]) {                               \
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
    if (((_a >> (_i * 8)) & 0xFF) != _p[sizeof(_t)-_i-1]) {                  \
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

static BOOL TestStream_Write(void)
{
	BOOL rc = FALSE;
	UINT8 u8;
	UINT16 u16;
	UINT32 u32;
	UINT64 u64;
	const BYTE data[] = "someteststreamdata";
	wStream* s = Stream_New(NULL, 100);
	if (!s)
		goto out;
	if (s->pointer != s->buffer)
		goto out;

	Stream_Write(s, data, sizeof(data));
	if (memcmp(Stream_Buffer(s), data, sizeof(data)) == 0)
		rc = TRUE;
	if (s->pointer != s->buffer + sizeof(data))
		goto out;

	Stream_SetPosition(s, 0);
	if (s->pointer != s->buffer)
		goto out;
	Stream_Write_UINT8(s, 42);
	if (s->pointer != s->buffer + 1)
		goto out;
	Stream_SetPosition(s, 0);
	if (s->pointer != s->buffer)
		goto out;
	Stream_Peek_UINT8(s, u8);
	if (u8 != 42)
		goto out;

	Stream_Write_UINT16(s, 0x1234);
	if (s->pointer != s->buffer + 2)
		goto out;
	Stream_SetPosition(s, 0);
	if (s->pointer != s->buffer)
		goto out;
	Stream_Peek_UINT16(s, u16);
	if (u16 != 0x1234)
		goto out;

	Stream_Write_UINT32(s, 0x12345678UL);
	if (s->pointer != s->buffer + 4)
		goto out;
	Stream_SetPosition(s, 0);
	if (s->pointer != s->buffer)
		goto out;
	Stream_Peek_UINT32(s, u32);
	if (u32 != 0x12345678UL)
		goto out;

	Stream_Write_UINT64(s, 0x1234567890ABCDEFULL);
	if (s->pointer != s->buffer + 8)
		goto out;
	Stream_SetPosition(s, 0);
	if (s->pointer != s->buffer)
		goto out;
	Stream_Peek_UINT64(s, u64);
	if (u64 != 0x1234567890ABCDEFULL)
		goto out;
out:
	Stream_Free(s, TRUE);
	return rc;
}

static BOOL TestStream_Seek(void)
{
	BOOL rc = FALSE;
	wStream* s = Stream_New(NULL, 100);
	if (!s)
		goto out;

	if (s->pointer != s->buffer)
		goto out;

	Stream_Seek(s, 5);
	if (s->pointer != s->buffer + 5)
		goto out;
	Stream_Seek_UINT8(s);
	if (s->pointer != s->buffer + 6)
		goto out;

	Stream_Seek_UINT16(s);
	if (s->pointer != s->buffer + 8)
		goto out;

	Stream_Seek_UINT32(s);
	if (s->pointer != s->buffer + 12)
		goto out;
	Stream_Seek_UINT64(s);
	if (s->pointer != s->buffer + 20)
		goto out;

	rc = TRUE;
out:
	Stream_Free(s, TRUE);
	return rc;
}

static BOOL TestStream_Rewind(void)
{
	BOOL rc = FALSE;
	wStream* s = Stream_New(NULL, 100);
	if (!s)
		goto out;
	if (s->pointer != s->buffer)
		goto out;

	Stream_Seek(s, 100);
	if (s->pointer != s->buffer + 100)
		goto out;

	Stream_Rewind(s, 10);
	if (s->pointer != s->buffer + 90)
		goto out;
	Stream_Rewind_UINT8(s);
	if (s->pointer != s->buffer + 89)
		goto out;
	Stream_Rewind_UINT16(s);
	if (s->pointer != s->buffer + 87)
		goto out;

	Stream_Rewind_UINT32(s);
	if (s->pointer != s->buffer + 83)
		goto out;
	Stream_Rewind_UINT64(s);
	if (s->pointer != s->buffer + 75)
		goto out;

	rc = TRUE;
out:
	Stream_Free(s, TRUE);
	return rc;
}

static BOOL TestStream_Zero(void)
{
	UINT32 x;
	BOOL rc = FALSE;
	const BYTE data[] = "someteststreamdata";
	wStream* s = Stream_New(NULL, sizeof(data));
	if (!s)
		goto out;

	Stream_Write(s, data, sizeof(data));
	if (memcmp(Stream_Buffer(s), data, sizeof(data)) != 0)
		goto out;
	Stream_SetPosition(s, 0);
	if (s->pointer != s->buffer)
		goto out;
	Stream_Zero(s, 5);
	if (s->pointer != s->buffer + 5)
		goto out;
	if (memcmp(Stream_Pointer(s), data+5, sizeof(data)-5) != 0)
		goto out;
	Stream_SetPosition(s, 0);
	if (s->pointer != s->buffer)
		goto out;
	for (x=0; x<5; x++)
	{
		UINT8 val;
		Stream_Read_UINT8(s, val);
		if (val != 0)
			goto out;
	}

	rc = TRUE;
out:
	Stream_Free(s, TRUE);
	return rc;
}

static BOOL TestStream_Fill(void)
{
	BOOL rc = FALSE;
	const BYTE fill[7] = "XXXXXXX";
	const BYTE data[] = "someteststreamdata";
	wStream* s = Stream_New(NULL, sizeof(data));
	if (!s)
		goto out;

	Stream_Write(s, data, sizeof(data));
	if (memcmp(Stream_Buffer(s), data, sizeof(data)) != 0)
		goto out;
	Stream_SetPosition(s, 0);
	if (s->pointer != s->buffer)
		goto out;
	Stream_Fill(s, fill[0], sizeof(fill));
	if (s->pointer != s->buffer + sizeof(fill))
		goto out;
	if (memcmp(Stream_Pointer(s), data+sizeof(fill), sizeof(data)-sizeof(fill)) != 0)
		goto out;
	Stream_SetPosition(s, 0);
	if (s->pointer != s->buffer)
		goto out;
	if (memcmp(Stream_Pointer(s), fill, sizeof(fill)) != 0)
		goto out;

	rc = TRUE;
out:
	Stream_Free(s, TRUE);
	return rc;
}

static BOOL TestStream_Copy(void)
{
	BOOL rc = FALSE;
	const BYTE data[] = "someteststreamdata";
	wStream* s = Stream_New(NULL, sizeof(data));
	wStream* d = Stream_New(NULL, sizeof(data));
	if (!s || !d)
		goto out;
	if (s->pointer != s->buffer)
		goto out;

	Stream_Write(s, data, sizeof(data));
	if (memcmp(Stream_Buffer(s), data, sizeof(data)) != 0)
		goto out;
	if (s->pointer != s->buffer + sizeof(data))
		goto out;
	Stream_SetPosition(s, 0);
	if (s->pointer != s->buffer)
		goto out;

	Stream_Copy(s, d, sizeof(data));
	if (s->pointer != s->buffer + sizeof(data))
		goto out;
	if (d->pointer != d->buffer + sizeof(data))
		goto out;
	if (Stream_GetPosition(s) != Stream_GetPosition(d))
		goto out;

	if (memcmp(Stream_Buffer(s), data, sizeof(data)) != 0)
		goto out;
	if (memcmp(Stream_Buffer(d), data, sizeof(data)) != 0)
		goto out;

	rc = TRUE;
out:
	Stream_Free(s, TRUE);
	Stream_Free(d, TRUE);
	return rc;
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

	if (!TestStream_Write())
		return 6;

	if (!TestStream_Seek())
		return 7;

	if (!TestStream_Rewind())
		return 8;

	if (!TestStream_Zero())
		return 9;

	if (!TestStream_Fill())
		return 10;

	if (!TestStream_Copy())
		return 11;

	return 0;
}

