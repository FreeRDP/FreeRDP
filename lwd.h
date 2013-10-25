
#ifndef __LWD_H__
#define __LWD_H__

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define LWD(fmt, ...) do { \
			time_t tod = time(NULL); \
			char buf[25]; \
			struct tm* tm_info = localtime(&tod); \
			strftime(buf, 25, "%H:%M:%S", tm_info); \
	                fprintf(stderr, "%20.20s [%s] ", __FUNCTION__, buf); \
	                fprintf(stderr, fmt, ## __VA_ARGS__); \
	                fprintf(stderr, "\n"); \
	        } while( 0 )

//                         fflush(stderr);

#endif
