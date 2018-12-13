#include <stdlib.h>

#include "scquery_error.h"

/* ========================================================================== */
/* Error Handling */

void* check_memory(void* memory, size_t size)
{
	return memory
	       ? memory
	       : handle_out_of_memory(size);
}

void* checked_malloc(size_t size)
{
	return check_memory(malloc(size), size);
}

void* checked_calloc(size_t nmemb, size_t size)
{
	return check_memory(calloc(nmemb, size), nmemb * size);
}



