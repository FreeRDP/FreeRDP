#include <stdlib.h>
#include <string.h>
#include "buffer.h"
#include "scquery_error.h"

enum
{
	buffer_flag_allocated = (1 << 0)
};

typedef struct
{
	CK_ULONG flags;
	CK_ULONG size;
	CK_BYTE* data;
} buffer_t;

CK_ULONG buffer_size(buffer buf)
{
	buffer_t* buffer = buf;
	return buffer->size;
}

CK_BYTE* buffer_data(buffer buf)
{
	buffer_t* buffer = buf;
	return buffer->data;
}

buffer buffer_new_copy(CK_ULONG size, CK_BYTE* data)
{
	buffer_t* buffer = checked_malloc(sizeof(*buffer));

	if (buffer == NULL)
	{
		return NULL;
	}

	buffer->flags = buffer_flag_allocated;
	buffer->size = size;
	buffer->data = checked_malloc(buffer->size);

	if (buffer->data == NULL)
	{
		free(buffer);
		return NULL;
	}

	memcpy(buffer->data, data, buffer->size);
	return buffer;
}

buffer buffer_new(CK_ULONG size, CK_BYTE* data)
{
	buffer_t* buffer = checked_malloc(sizeof(*buffer));

	if (buffer == NULL)
	{
		return NULL;
	}

	buffer->flags = 0;
	buffer->size = size;
	buffer->data = data;
	return buffer;
}

void buffer_free(buffer buf)
{
	buffer_t* buffer = buf;

	if (buffer == NULL)
	{
		return;
	}

	if (buffer->flags & buffer_flag_allocated)
	{
		memset(buffer->data, 0, buffer->size);
		free(buffer->data);
	}

	memset(buffer, 0, sizeof(*buffer));
	free(buffer);
}

/**** THE END ****/
