
#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/crypto.h>

static const BYTE* TEST_RC4_KEY = (BYTE*) "Key";
static const char* TEST_RC4_PLAINTEXT = "Plaintext";
static const BYTE* TEST_RC4_CIPHERTEXT = (BYTE*) "\xBB\xF3\x16\xE8\xD9\x40\xAF\x0A\xD3";

static BOOL test_crypto_cipher_rc4()
{
	size_t len;
	BOOL rc = FALSE;
	BYTE* text = NULL;
	WINPR_RC4_CTX* ctx;

	len = strlen(TEST_RC4_PLAINTEXT);

	text = (BYTE*) calloc(1, len);

	if (!text)
		goto out;

	if ((ctx = winpr_RC4_New(TEST_RC4_KEY, strlen((char*) TEST_RC4_KEY))) == NULL)
		goto out;
	rc = winpr_RC4_Update(ctx, len, (BYTE*) TEST_RC4_PLAINTEXT, text);
	winpr_RC4_Free(ctx);
	if (!rc)
		goto out;

	if (memcmp(text, TEST_RC4_CIPHERTEXT, len) != 0)
	{
		char* actual;
		char* expected;

		actual = winpr_BinToHexString(text, len, FALSE);
		expected = winpr_BinToHexString(TEST_RC4_CIPHERTEXT, len, FALSE);

		fprintf(stderr, "unexpected RC4 ciphertext: Actual: %s Expected: %s\n", actual, expected);

		free(actual);
		free(expected);
		goto out;
	}

	rc = TRUE;

out:
	free(text);
	return rc;
}

static const BYTE* TEST_RAND_DATA = (BYTE*)
	"\x1F\xC2\xEE\x4C\xA3\x66\x80\xA2\xCE\xFE\x56\xB4\x9E\x08\x30\x96"
	"\x33\x6A\xA9\x6D\x36\xFD\x3C\xB7\x83\x04\x4E\x5E\xDC\x22\xCD\xF3"
	"\x48\xDF\x3A\x2A\x61\xF1\xA8\xFA\x1F\xC6\xC7\x1B\x81\xB4\xE1\x0E"
	"\xCB\xA2\xEF\xA1\x12\x4A\x83\xE5\x1D\x72\x1D\x2D\x26\xA8\x6B\xC0";

static const BYTE* TEST_CIPHER_KEY = (BYTE*)
	"\x9D\x7C\xC0\xA1\x94\x3B\x07\x67\x2F\xD3\x83\x10\x51\x83\x38\x0E"
	"\x1C\x74\x8C\x4E\x15\x79\xD6\xFF\xE2\xF0\x37\x7F\x8C\xD7\xD2\x13";

static const BYTE* TEST_CIPHER_IV = (BYTE*)
	"\xFE\xE3\x9F\xF0\xD1\x5E\x37\x0C\xAB\xAB\x9B\x04\xF3\xDB\x99\x15";

static BOOL test_crypto_cipher_key()
{
	int status;
	BYTE key[32];
	BYTE iv[16];
	BYTE salt[8] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77 };

	ZeroMemory(key, sizeof(key));
	ZeroMemory(iv, sizeof(iv));

	status = winpr_openssl_BytesToKey(WINPR_CIPHER_AES_256_CBC, WINPR_MD_SHA1,
			salt, TEST_RAND_DATA, 64, 4, key, iv);

	if (status != 32 || memcmp(key, TEST_CIPHER_KEY, 32) || memcmp(iv, TEST_CIPHER_IV, 16))
	{
		char* akstr;
		char* ekstr;
		char* aivstr;
		char* eivstr;

		akstr = winpr_BinToHexString(key, 32, 0);
		ekstr = winpr_BinToHexString(TEST_CIPHER_KEY, 32, 0);

		aivstr = winpr_BinToHexString(iv, 16, 0);
		eivstr = winpr_BinToHexString(TEST_CIPHER_IV, 16, 0);

		fprintf(stderr, "Unexpected EVP_BytesToKey Key: Actual: %s, Expected: %s\n", akstr, ekstr);
		fprintf(stderr, "Unexpected EVP_BytesToKey IV : Actual: %s, Expected: %s\n", aivstr, eivstr);

		free(akstr);
		free(ekstr);
		free(aivstr);
		free(eivstr);
	}

	return TRUE;
}

int TestCryptoCipher(int argc, char* argv[])
{
	if (!test_crypto_cipher_rc4())
		return -1;

	if (!test_crypto_cipher_key())
		return -1;

	return 0;
}
