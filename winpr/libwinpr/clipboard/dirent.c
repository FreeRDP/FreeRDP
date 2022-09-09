
#include <stdio.h>
#include "dirent.h"

#include "../log.h"
#define TAG WINPR_TAG("clipboard.synthetic.file")

DIR* opendir(const char* name)
{
	DIR* dir = NULL;
	WIN32_FIND_DATAA FindData;
	char namebuf[MAX_PATH];

	int nLen = 0;
	int nRet = 0;

	sprintf(namebuf, "%s\\*.*", name);
	nLen = strlen(namebuf);

	HANDLE hFind = FindFirstFileA(namebuf, &FindData);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		WLog_ERR(TAG, "FindFirstFile failed (%d)", GetLastError());
		return NULL;
	}

	dir = (DIR*)calloc(1, sizeof(DIR));
	if (!dir)
	{
		WLog_ERR(TAG, "DIR memory allocate fail");
		return NULL;
	}

	dir->dd_fd = hFind; // simulate return

	return dir;
}

struct dirent* readdir(DIR* d)
{
	size_t nRet = 0;
	size_t nLen = 0;
	BOOL bf = false;
	WIN32_FIND_DATAA FileData;

	if (!d)
		return NULL;

	bf = FindNextFileA(d->dd_fd, &FileData);
	// fail or end
	if (!bf)
		return NULL;

	struct dirent* dirent =
	    (struct dirent*)calloc(1, sizeof(struct dirent) + ARRAYSIZE(FileData.cFileName));
	if (!dirent)
		return NULL;
	nLen = lstrlenW(FileData.cFileName);
	memcpy(&dirent->d_name[0], &FileData.cFileName[0], nLen);
	dirent->d_reclen = nLen;

	// check there is file or directory
	if (FileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		dirent->d_type = 2;
	else
		dirent->d_type = 1;

	return dirent;
}

int closedir(DIR* d)
{
	if (!d)
		return -1;
	FindClose(d->dd_fd);
	free(d);
	return 0;
}