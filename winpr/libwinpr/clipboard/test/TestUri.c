
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <stdlib.h>
#include <winpr/winpr.h>
#include "winpr/wlog.h"

#define WINPR_TAG(tag) "com.winpr." tag
#define TAG WINPR_TAG("clipboard.posix")

char* parse_uri_to_local_file(const char* uri, size_t uri_len);

int main(void)
{
	int nRet = 0;
	const char* input[] = { /*uri,                      file or NULL*/
		                    "file://root/a.txt",
		                    NULL,
		                    "file:a.txt",
		                    NULL,
		                    "file:///c:/windows/a.txt",
		                    "c:/windows/a.txt",
		                    "file:c:/windows/a.txt",
		                    "c:/windows/a.txt",
		                    "file:c|/windows/a.txt",
		                    "c:/windows/a.txt",
		                    "file:///root/a.txt",
		                    "/root/a.txt",
		                    "file:/root/a.txt",
		                    "/root/a.txt"
	};

	const size_t nLen = ARRAYSIZE(input);
	printf("input length:%" PRIuz "\n", nLen / 2);

	WLog_SetLogLevel(WLog_Get(TAG), WLOG_ERROR);

	for (size_t i = 0; i < nLen; i += 2)
	{
		int bTest = 0;
		char* name = parse_uri_to_local_file(input[i], strlen(input[i]));
		if (name)
		{
			bTest = !strcmp(name, input[i + 1]);
			if (!bTest)
			{
				printf("Test error: input: %s; Expected value: %s; output: %s\n", input[i],
				       input[i + 1], name);
				nRet++;
			}
			free(name);
		}
		else
		{
			if (input[i + 1])
			{
				printf("Test error: input: %s; Expected value: %s; output: %s\n", input[i],
				       input[i + 1], name);
				nRet++;
			}
		}
	}

	printf("TestUri return value: %d\n", nRet);
	return nRet;
}
