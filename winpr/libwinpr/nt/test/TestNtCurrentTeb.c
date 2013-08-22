
#include <stdio.h>

#include <winpr/nt.h>

int TestNtCurrentTeb(int argc, char* argv[])
{
	PTEB teb;

	teb = NtCurrentTeb();

	if (!teb)
	{
		printf("NtCurrentTeb() returned NULL\n");
		return -1;
	}

	return 0;
}

