#ifndef DIRENT_H
#define DIRENT_H

#include <windows.h>

typedef struct _dirdesc
{
	HANDLE dd_fd; /** file descriptor associated with directory */
	long dd_loc;  /** offset in current buffer */
	long dd_size; /** amount of data returned by getdirentries */
	char* dd_buf; /** data buffer */
	int dd_len;   /** size of data buffer */
	long dd_seek; /** magic cookie returned by getdirentries */
} DIR;

#include <sys/types.h>

struct dirent
{
	long d_ino;              /* inode number*/
	off_t d_off;             /* offset to this dirent*/
	unsigned short d_reclen; /* length of this d_name*/
	unsigned char d_type;    /* the type of d_name*/
	char d_name[1];          /* file name (null-terminated)*/
};

DIR* opendir(const char*);
struct dirent* readdir(DIR*);
int closedir(DIR*);

#endif // DIRENT_H
