
#include "winpr/wlog.h"
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winpr/winpr.h>

#include "../clipboard.h"

#define WINPR_TAG(tag) "com.winpr." tag
#define TAG WINPR_TAG("clipboard.posix")

int TestUri(int argc, char* argv[])
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

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	printf("input length:%" PRIuz "\n", nLen / 2);

	for (size_t i = 0; i < nLen; i += 2)
	{
		const char* in = input[i];
		const char* cmp = input[i + 1];
		int bTest = 0;
		char* name = parse_uri_to_local_file(in, strlen(in));
		if (name)
		{
			bTest = !strcmp(name, cmp);
			if (!bTest)
			{
				printf("Test error: input: %s; Expected value: %s; output: %s\n", in, cmp, name);
				nRet++;
			}
			free(name);
		}
		else
		{
			if (cmp)
			{
				printf("Test error: input: %s; Expected value: %s; output: %s\n", in, cmp, name);
				nRet++;
			}
		}
	}

	printf("TestUri return value: %d\n", nRet);
	return nRet;
}
