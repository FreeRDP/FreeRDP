#ifndef LIBFREERDP_SCQUERY_ERROR_H
#define LIBFREERDP_SCQUERY_ERROR_H
#include <stddef.h>
#include <errno.h>
#include <sysexits.h>

/* out_of_memory
handles the out of memory error (when malloc returns NULL).
It may not return, or it should return a pointer returned
untouched by the caller.
*/
typedef void* (*out_of_memory_handler)(size_t size);
out_of_memory_handler handle_out_of_memory;

/* check_memory
checks the memory pointer is not null, and returns it.
If it's null, then call out_of_memory and return its results.
*/
void* check_memory(void* memory, size_t size);

/* checked_malloc
allocates size bytes of memory, or if it can't, calls out_of_memory and return its results.
*/
void* checked_malloc(size_t size);

/* checked_calloc
allocates nmemb * size bytes of memory and clears it,
or if it can't, calls out_of_memory and return its results.
*/
void* checked_calloc(size_t nmemb, size_t size);

/* error
handles other errors, displaying the formated error message.
It may return or not.
*/
typedef void (*error_handler)(const char* file, unsigned long line, const char* function,
                              int status, const char* format, ...);
error_handler handle_error;
#define ERROR(status,format, ...) (handle_error(__FILE__, __LINE__, __FUNCTION__, status, format, ## __VA_ARGS__))

/* warn
handles the warning, displaying the formated error message, and returning.
*/
typedef void (*warning_handler)(const char* file, unsigned long line, const char* function,
                                int status, const char* format, ...);
warning_handler handle_warning;
#define WARN(status,format, ...) (handle_warning(__FILE__, __LINE__, __FUNCTION__, status, format, ## __VA_ARGS__))


typedef void (*verbose_handler)(const char* file, unsigned long line, const char* function,
                                const char* format, ...);
verbose_handler handle_verbose;
#define VERBOSE(verbose,format, ...) ((verbose)?handle_verbose(__FILE__, __LINE__, __FUNCTION__, format, ## __VA_ARGS__):(void)0)


#ifdef assert
#undef assert
#endif
#define assert(condition) do{if(!(condition)){ERROR(EX_SOFTWARE,"Assertion failed: %s",#condition);}}while(0)

#endif

