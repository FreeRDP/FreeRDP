
#include <winpr/crt.h>
#include <winpr/sysinfo.h>

int TestGetNativeSystemInfo(int argc, char* argv[])
{
	SYSTEM_INFO sysinfo;

	GetNativeSystemInfo(&sysinfo);

	printf("SystemInfo:\n");
	printf("\twProcessorArchitecture: %d\n", sysinfo.wProcessorArchitecture);
	printf("\twReserved: %d\n", sysinfo.wReserved);
	printf("\tdwPageSize: 0x%08lX\n", sysinfo.dwPageSize);
	printf("\tlpMinimumApplicationAddress: %p\n", sysinfo.lpMinimumApplicationAddress);
	printf("\tlpMaximumApplicationAddress: %p\n", sysinfo.lpMaximumApplicationAddress);
	printf("\tdwActiveProcessorMask: 0x%08llX\n", (unsigned long long)sysinfo.dwActiveProcessorMask);
	printf("\tdwNumberOfProcessors: %ld\n", sysinfo.dwNumberOfProcessors);
	printf("\tdwProcessorType: %ld\n", sysinfo.dwProcessorType);
	printf("\tdwAllocationGranularity: %ld\n", sysinfo.dwAllocationGranularity);
	printf("\twProcessorLevel: %d\n", sysinfo.wProcessorLevel);
	printf("\twProcessorRevision: %d\n", sysinfo.wProcessorRevision);
	printf("\n");

	return 0;
}
