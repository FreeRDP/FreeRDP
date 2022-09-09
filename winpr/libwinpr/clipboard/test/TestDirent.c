#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <stdlib.h>
#include <winpr/winpr.h>
#include <winpr/error.h>
#include <winpr/print.h>
#include "winpr/wlog.h"
#include "../dirent.h"

#define WINPR_TAG(tag) "com.winpr." tag
#define TAG WINPR_TAG("clipboard.synthetic.file")

static int Test(char* path)
{
	int nRet = 0;
	DIR* dirp = NULL;

	dirp = opendir(path);
	if (!dirp)
	{
		WLog_ERR(TAG, "failed to open directory %s", path);
		return -1;
	}

	while (1)
	{
		struct dirent* entry = NULL;
		entry = readdir(dirp);
		if (!entry)
		{
			WLog_ERR(TAG, "failed to read directory");
			break;
		}

		if (entry->d_type == 2)
		{
			printf("[%s] ", entry->d_name);
		}
		else
		{
			printf("%s ", entry->d_name);
		}
	}

	nRet = closedir(dirp);
	if (nRet)
	{
		WLog_ERR(TAG, "failed to close directory");
		return nRet;
	}

	return nRet;
}

int main(int argc, char* argv[])
{
	int nRet = 0;
	WLog_SetLogLevel(WLog_Get(TAG), WLOG_ERROR);

	if (argc != 2)
	{
		printf("Usage: %s path\n", argv[0]);
		return -1;
	}

	char* path = argv[1];

	printf("\nTest char path: %s\n\n", path);
	nRet = Test(path);

	return nRet;
}
