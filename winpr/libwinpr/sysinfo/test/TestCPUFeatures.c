
#include <winpr/crt.h>
#include <winpr/sysinfo.h>
#include <winpr/platform.h>

int TestCPUFeatures(int argc, char* argv[])
{
	printf("Base CPU Flags:\n");
#ifdef _M_IX86_AMD64
	printf("\tPF_MMX_INSTRUCTIONS_AVAILABLE:  %s\n", IsProcessorFeaturePresent(PF_MMX_INSTRUCTIONS_AVAILABLE) ? "yes" : "no");
	printf("\tPF_XMMI_INSTRUCTIONS_AVAILABLE:  %s\n", IsProcessorFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE) ? "yes" : "no");
	printf("\tPF_XMMI64_INSTRUCTIONS_AVAILABLE:  %s\n", IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE) ? "yes" : "no");
	printf("\tPF_3DNOW_INSTRUCTIONS_AVAILABLE:  %s\n", IsProcessorFeaturePresent(PF_3DNOW_INSTRUCTIONS_AVAILABLE) ? "yes" : "no");
	printf("\tPF_SSE3_INSTRUCTIONS_AVAILABLE:  %s\n", IsProcessorFeaturePresent(PF_SSE3_INSTRUCTIONS_AVAILABLE) ? "yes" : "no");
	printf("\n");
	printf("Extended CPU Flags (not found in windows API):\n");
	printf("\tPF_EX_3DNOW_PREFETCH:  %s\n", IsProcessorFeaturePresentEx(PF_EX_3DNOW_PREFETCH) ? "yes" : "no");
	printf("\tPF_EX_SSSE3:  %s\n", IsProcessorFeaturePresentEx(PF_EX_SSSE3) ? "yes" : "no");
	printf("\tPF_EX_SSE41:  %s\n", IsProcessorFeaturePresentEx(PF_EX_SSE41) ? "yes" : "no");
	printf("\tPF_EX_SSE42:  %s\n", IsProcessorFeaturePresentEx(PF_EX_SSE42) ? "yes" : "no");
	printf("\tPF_EX_AVX:  %s\n", IsProcessorFeaturePresentEx(PF_EX_AVX) ? "yes" : "no");
	printf("\tPF_EX_FMA:  %s\n", IsProcessorFeaturePresentEx(PF_EX_FMA) ? "yes" : "no");
	printf("\tPF_EX_AVX_AES:  %s\n", IsProcessorFeaturePresentEx(PF_EX_AVX_AES) ? "yes" : "no");
	printf("\tPF_EX_AVX_PCLMULQDQD:  %s\n", IsProcessorFeaturePresentEx(PF_EX_AVX_PCLMULQDQ) ? "yes" : "no");
#elif defined(_M_ARM)
	printf("\tPF_ARM_NEON_INSTRUCTIONS_AVAILABLE:  %s\n", IsProcessorFeaturePresent(PF_ARM_NEON_INSTRUCTIONS_AVAILABLE) ? "yes" : "no");
	printf("\tPF_ARM_THUMB:  %s\n", IsProcessorFeaturePresent(PF_ARM_THUMB) ? "yes" : "no");
	printf("\tPF_ARM_VFP_32_REGISTERS_AVAILABLE:  %s\n", IsProcessorFeaturePresent(PF_ARM_VFP_32_REGISTERS_AVAILABLE) ? "yes" : "no");
	printf("\tPF_ARM_DIVIDE_INSTRUCTION_AVAILABLE:  %s\n", IsProcessorFeaturePresent(PF_ARM_DIVIDE_INSTRUCTION_AVAILABLE) ? "yes" : "no");
	printf("\tPF_ARM_VFP3:  %s\n", IsProcessorFeaturePresent(PF_ARM_VFP3) ? "yes" : "no");
	printf("\tPF_ARM_THUMB:  %s\n", IsProcessorFeaturePresent(PF_ARM_THUMB) ? "yes" : "no");
	printf("\tPF_ARM_JAZELLE:  %s\n", IsProcessorFeaturePresent(PF_ARM_JAZELLE) ? "yes" : "no");
	printf("\tPF_ARM_DSP:  %s\n", IsProcessorFeaturePresent(PF_ARM_DSP) ? "yes" : "no");
	printf("\tPF_ARM_THUMB2:  %s\n", IsProcessorFeaturePresent(PF_ARM_THUMB2) ? "yes" : "no");
	printf("\tPF_ARM_T2EE:  %s\n", IsProcessorFeaturePresent(PF_ARM_T2EE) ? "yes" : "no");
	printf("\tPF_ARM_INTEL_WMMX:  %s\n", IsProcessorFeaturePresent(PF_ARM_INTEL_WMMX) ? "yes" : "no");
	printf("Extended CPU Flags (not found in windows API):\n");
	printf("\tPF_EX_ARM_VFP1:  %s\n", IsProcessorFeaturePresentEx(PF_EX_ARM_VFP1) ? "yes" : "no");
	printf("\tPF_EX_ARM_VFP3D16:  %s\n", IsProcessorFeaturePresentEx(PF_EX_ARM_VFP3D16) ? "yes" : "no");
	printf("\tPF_EX_ARM_VFP4:  %s\n", IsProcessorFeaturePresentEx(PF_EX_ARM_VFP4) ? "yes" : "no");
	printf("\tPF_EX_ARM_IDIVA:  %s\n", IsProcessorFeaturePresentEx(PF_EX_ARM_IDIVA) ? "yes" : "no");
	printf("\tPF_EX_ARM_IDIVT:  %s\n", IsProcessorFeaturePresentEx(PF_EX_ARM_IDIVT) ? "yes" : "no");
#endif
	printf("\n");
	return 0;
}
