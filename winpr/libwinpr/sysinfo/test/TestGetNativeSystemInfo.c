
#include <winpr/crt.h>
#include <winpr/sysinfo.h>

int TestGetNativeSystemInfo(int argc, char* argv[])
{
	SYSTEM_INFO sysinfo;

	GetNativeSystemInfo(&sysinfo);

	printf("SystemInfo:\n");
	printf("\twProcessorArchitecture: %" PRIu16 "\n", sysinfo.wProcessorArchitecture);
	printf("\twReserved: %" PRIu16 "\n", sysinfo.wReserved);
	printf("\tdwPageSize: 0x%08" PRIX32 "\n", sysinfo.dwPageSize);
	printf("\tlpMinimumApplicationAddress: %p\n", sysinfo.lpMinimumApplicationAddress);
	printf("\tlpMaximumApplicationAddress: %p\n", sysinfo.lpMaximumApplicationAddress);
	printf("\tdwActiveProcessorMask: %p\n", (void*)sysinfo.dwActiveProcessorMask);
	printf("\tdwNumberOfProcessors: %" PRIu32 "\n", sysinfo.dwNumberOfProcessors);
	printf("\tdwProcessorType: %" PRIu32 "\n", sysinfo.dwProcessorType);
	printf("\tdwAllocationGranularity: %" PRIu32 "\n", sysinfo.dwAllocationGranularity);
	printf("\twProcessorLevel: %" PRIu16 "\n", sysinfo.wProcessorLevel);
	printf("\twProcessorRevision: %" PRIu16 "\n", sysinfo.wProcessorRevision);
	printf("\n");

	return 0;
}
