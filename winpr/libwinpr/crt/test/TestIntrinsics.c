
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/windows.h>

int test_lzcnt()
{
	if (__lzcnt(0x0) != 32) {
		fprintf(stderr, "__lzcnt(0x0) != 32\n");
		return -1;
	}

	if (__lzcnt(0x1) != 31) {
		fprintf(stderr, "__lzcnt(0x1) != 31\n");
		return -1;
	}

	if (__lzcnt(0xFF) != 24) {
		fprintf(stderr, "__lzcnt(0xFF) != 24\n");
		return -1;
	}

	if (__lzcnt(0xFFFF) != 16) {
		fprintf(stderr, "__lzcnt(0xFFFF) != 16\n");
		return -1;
	}

	if (__lzcnt(0xFFFFFF) != 8) {
		fprintf(stderr, "__lzcnt(0xFFFFFF) != 8\n");
		return -1;
	}

	if (__lzcnt(0xFFFFFFFF) != 0) {
		fprintf(stderr, "__lzcnt(0xFFFFFFFF) != 0\n");
		return -1;
	}

	return 1;
}

int test_lzcnt16()
{
	if (__lzcnt16(0x0) != 16) {
		fprintf(stderr, "__lzcnt16(0x0) != 16\n");
		return -1;
	}

	if (__lzcnt16(0x1) != 15) {
		fprintf(stderr, "__lzcnt16(0x1) != 15\n");
		return -1;
	}

	if (__lzcnt16(0xFF) != 8) {
		fprintf(stderr, "__lzcnt16(0xFF) != 8\n");
		return -1;
	}

	if (__lzcnt16(0xFFFF) != 0) {
		fprintf(stderr, "__lzcnt16(0xFFFF) != 0\n");
		return -1;
	}

	return 1;
}

int test_lzcnt64()
{
	if (__lzcnt64(0x0) != 64) {
		fprintf(stderr, "__lzcnt64(0x0) != 64\n");
		return -1;
	}

	if (__lzcnt64(0x1) != 63) {
		fprintf(stderr, "__lzcnt64(0x1) != 63\n");
		return -1;
	}

	if (__lzcnt64(0xFF) != 56) {
		fprintf(stderr, "__lzcnt64(0xFF) != 56\n");
		return -1;
	}

	if (__lzcnt64(0xFFFF) != 48) {
		fprintf(stderr, "__lzcnt64(0xFFFF) != 48\n");
		return -1;
	}

	if (__lzcnt64(0xFFFFFF) != 40) {
		fprintf(stderr, "__lzcnt64(0xFFFFFF) != 40\n");
		return -1;
	}

	if (__lzcnt64(0xFFFFFFFF) != 32) {
		fprintf(stderr, "__lzcnt64(0xFFFFFFFF) != 32\n");
		return -1;
	}

	return 1;
}

int TestIntrinsics(int argc, char* argv[])
{
	test_lzcnt();
	test_lzcnt16();
	test_lzcnt64();

	return 0;
}

