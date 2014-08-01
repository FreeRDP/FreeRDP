
#include <winpr/crt.h>
#include <winpr/sysinfo.h>

int TestGetNativeSystemInfo(int argc, char* argv[])
{
	SYSTEM_INFO sysinfo;

	GetNativeSystemInfo(&sysinfo);

	printf("SystemInfo:\n");
	printf("\twProcessorArchitecture: %d\n", sysinfo.wProcessorArchitecture);
	printf("\twReserved: %d\n", sysinfo.wReserved);
	printf("\tdwPageSize: 0x%08X\n", sysinfo.dwPageSize);
	printf("\tlpMinimumApplicationAddress: %p\n", sysinfo.lpMinimumApplicationAddress);
	printf("\tlpMaximumApplicationAddress: %p\n", sysinfo.lpMaximumApplicationAddress);
	printf("\tdwActiveProcessorMask: 0x%08X\n", (unsigned int) sysinfo.dwActiveProcessorMask);
	printf("\tdwNumberOfProcessors: %d\n", sysinfo.dwNumberOfProcessors);
	printf("\tdwProcessorType: %d\n", sysinfo.dwProcessorType);
	printf("\tdwAllocationGranularity: %d\n", sysinfo.dwAllocationGranularity);
	printf("\twProcessorLevel: %d\n", sysinfo.wProcessorLevel);
	printf("\twProcessorRevision: %d\n", sysinfo.wProcessorRevision);
	printf("\n");

	return 0;
}
