
#ifndef __LWD_H__
#define __LWD_H__

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define LWD(fmt, ...) do { \
			time_t tod = time(NULL); \
			char buf[25]; \
			struct tm* tm_info = localtime(&tod); \
			strftime(buf, 25, "%Y:%m:%d %H:%M:%S", tm_info); \
	                fprintf(stderr, "%s [%s] ", __FUNCTION__, buf); \
	                fprintf(stderr, fmt, ## __VA_ARGS__); \
	                fprintf(stderr, "\n"); \
			fflush(stderr); \
	        } while( 0 )



#endif
