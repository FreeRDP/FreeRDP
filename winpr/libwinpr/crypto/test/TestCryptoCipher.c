
#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/crypto.h>

static const BYTE* TEST_RC4_KEY = (BYTE*) "Key";
static const char* TEST_RC4_PLAINTEXT = "Plaintext";
static const BYTE* TEST_RC4_CIPHERTEXT = (BYTE*) "\xBB\xF3\x16\xE8\xD9\x40\xAF\x0A\xD3";

int test_crypto_cipher_rc4()
{
	size_t len;
	BYTE* text;
	WINPR_RC4_CTX ctx;

	len = strlen(TEST_RC4_PLAINTEXT);

	text = (BYTE*) calloc(1, len);

	if (!text)
		return -1;

	winpr_RC4_Init(&ctx, TEST_RC4_KEY, strlen((char*) TEST_RC4_KEY));
	winpr_RC4_Update(&ctx, len, (BYTE*) TEST_RC4_PLAINTEXT, text);
	winpr_RC4_Final(&ctx);

	if (memcmp(text, TEST_RC4_CIPHERTEXT, len) != 0)
	{
		char* actual;
		char* expected;

		actual = winpr_BinToHexString(text, len, FALSE);
		expected = winpr_BinToHexString(TEST_RC4_CIPHERTEXT, len, FALSE);

		fprintf(stderr, "unexpected RC4 ciphertext: Actual: %s Expected: %s\n", actual, expected);

		free(actual);
		free(expected);

		return -1;
	}

	return 0;
}

int TestCryptoCipher(int argc, char* argv[])
{
	if (!test_crypto_cipher_rc4())
		return -1;

	return 0;
}
