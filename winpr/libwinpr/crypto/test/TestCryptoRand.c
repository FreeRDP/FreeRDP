
#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/crypto.h>

int TestCryptoRand(int argc, char* argv[])
{
	char* str = NULL;
	BYTE rnd[16] = { 0 };

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	winpr_RAND(rnd, sizeof(rnd));

	str = winpr_BinToHexString(rnd, sizeof(rnd), FALSE);
	// (void)fprintf(stderr, "Rand: %s\n", str);
	free(str);

	if (memcmp(rnd, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16) == 0)
	{
		return -1;
	}

	return 0;
}
