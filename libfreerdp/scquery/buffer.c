#include <stdlib.h>
#include <string.h>
#include "buffer.h"
<<<<<<< HEAD
#include "scquery_error.h"
=======
#include "error.h"
>>>>>>> Added scquery.

enum
{
	buffer_flag_allocated = (1 << 0)
};

buffer buffer_new_copy(CK_ULONG size, CK_BYTE* data)
{
	buffer buffer = checked_malloc(sizeof(*buffer));

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
	buffer buffer = checked_malloc(sizeof(*buffer));

	if (buffer == NULL)
	{
		return NULL;
	}

	buffer->flags = 0;
	buffer->size = size;
	buffer->data = data;
	return buffer;
}

void buffer_free(buffer buffer)
{
	if (buffer == NULL)
	{
		return;
	}

	if (buffer->flags & buffer_flag_allocated)
	{
<<<<<<< HEAD
		memset(buffer->data, 0, buffer->size);
		free(buffer->data);
	}

	memset(buffer, 0, sizeof (*buffer));
=======
		free(buffer->data);
	}

>>>>>>> Added scquery.
	free(buffer);
}

/**** THE END ****/
