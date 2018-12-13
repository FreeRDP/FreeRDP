#ifndef LIBFREERDP_SCQUERY_STRING_H
#define LIBFREERDP_SCQUERY_STRING_H

#include <string.h>

#if (_POSIX_C_SOURCE >= 200809L) || defined(_GNU_SOURCE)
/* strndup and strnlen are defined in string.h */
#else

size_t strnlen(const char* string, size_t length);
char* strndup(const char* string, size_t length);

#endif

#if (_XOPEN_SOURCE >= 500) || (_POSIX_C_SOURCE >= 200809L) || _BSD_SOURCE || _SVID_SOURCE
/* strdup is defined in string.h */
#else

char* strdup(const char* string);

#endif



typedef char* (* string_preprocess_pr)(const char*);
typedef void (* string_postprocess_pr)(char*);
char* string_mapconcat(string_preprocess_pr preprocess,
                       string_postprocess_pr postprocess,
                       unsigned count, const char** strings,
                       const char* separator);

size_t string_count(const char* string, char character);


size_t padded_string_length(const char* padded_string, size_t max_size, char pad);
char* string_from_padded_string(const char* padded_string, size_t max_size, char pad);

/*
string_format
format a string as with sprintf,  but first computing the output size
and allocating buffer of that size (plus terminating nul).
@result returns the new string,  or NULL if it couldn't be allocated.
 */
char* string_format(const char* format_string, ...);

#endif

