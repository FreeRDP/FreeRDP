
#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/crypto.h>

static const char* TEST_MD5_DATA = "test";
static const BYTE* TEST_MD5_HASH = (BYTE*) "\x09\x8f\x6b\xcd\x46\x21\xd3\x73\xca\xde\x4e\x83\x26\x27\xb4\xf6";

BOOL test_crypto_hash_md5()
{
	BYTE hash[16];
	WINPR_MD5_CTX ctx;

	winpr_MD5_Init(&ctx);
	winpr_MD5_Update(&ctx, (BYTE*) TEST_MD5_DATA, strlen(TEST_MD5_DATA));
	winpr_MD5_Final(&ctx, hash);

	if (memcmp(hash, TEST_MD5_HASH, 16) != 0)
	{
		char* actual;
		char* expected;

		actual = winpr_BinToHexString(hash, 16, FALSE);
		expected = winpr_BinToHexString(TEST_MD5_HASH, 16, FALSE);

		fprintf(stderr, "unexpected MD5 hash: Actual: %s Expected: %s\n", actual, expected);

		free(actual);
		free(expected);

		return -1;
	}

	return TRUE;
}

static const char* TEST_MD4_DATA = "test";
static const BYTE* TEST_MD4_HASH = (BYTE*) "\xdb\x34\x6d\x69\x1d\x7a\xcc\x4d\xc2\x62\x5d\xb1\x9f\x9e\x3f\x52";

BOOL test_crypto_hash_md4()
{
	BYTE hash[16];
	WINPR_MD4_CTX ctx;

	winpr_MD4_Init(&ctx);
	winpr_MD4_Update(&ctx, (BYTE*) TEST_MD4_DATA, strlen(TEST_MD4_DATA));
	winpr_MD4_Final(&ctx, hash);

	if (memcmp(hash, TEST_MD4_HASH, 16) != 0)
	{
		char* actual;
		char* expected;

		actual = winpr_BinToHexString(hash, 16, FALSE);
		expected = winpr_BinToHexString(TEST_MD4_HASH, 16, FALSE);

		fprintf(stderr, "unexpected MD4 hash: Actual: %s Expected: %s\n", actual, expected);

		free(actual);
		free(expected);

		return -1;
	}

	return TRUE;
}

static const char* TEST_SHA1_DATA = "test";
static const BYTE* TEST_SHA1_HASH = (BYTE*) "\xa9\x4a\x8f\xe5\xcc\xb1\x9b\xa6\x1c\x4c\x08\x73\xd3\x91\xe9\x87\x98\x2f\xbb\xd3";

BOOL test_crypto_hash_sha1()
{
	BYTE hash[20];
	WINPR_SHA1_CTX ctx;

	winpr_SHA1_Init(&ctx);
	winpr_SHA1_Update(&ctx, (BYTE*) TEST_SHA1_DATA, strlen(TEST_SHA1_DATA));
	winpr_SHA1_Final(&ctx, hash);

	if (memcmp(hash, TEST_SHA1_HASH, 20) != 0)
	{
		char* actual;
		char* expected;

		actual = winpr_BinToHexString(hash, 20, FALSE);
		expected = winpr_BinToHexString(TEST_SHA1_HASH, 20, FALSE);

		fprintf(stderr, "unexpected SHA1 hash: Actual: %s Expected: %s\n", actual, expected);

		free(actual);
		free(expected);

		return -1;
	}

	return TRUE;
}

static const char* TEST_HMAC_MD5_DATA = "Hi There";
static const BYTE* TEST_HMAC_MD5_KEY = (BYTE*) "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b";
static const BYTE* TEST_HMAC_MD5_HASH = (BYTE*) "\x92\x94\x72\x7a\x36\x38\xbb\x1c\x13\xf4\x8e\xf8\x15\x8b\xfc\x9d";

BOOL test_crypto_hash_hmac_md5()
{
	BYTE hash[16];
	WINPR_HMAC_CTX ctx;

	winpr_HMAC_Init(&ctx, WINPR_MD_MD5, TEST_HMAC_MD5_KEY, 16);
	winpr_HMAC_Update(&ctx, (BYTE*) TEST_HMAC_MD5_DATA, strlen(TEST_HMAC_MD5_DATA));
	winpr_HMAC_Final(&ctx, hash);

	if (memcmp(hash, TEST_HMAC_MD5_HASH, 16) != 0)
	{
		char* actual;
		char* expected;

		actual = winpr_BinToHexString(hash, 16, FALSE);
		expected = winpr_BinToHexString(TEST_HMAC_MD5_HASH, 16, FALSE);

		fprintf(stderr, "unexpected HMAC-MD5 hash: Actual: %s Expected: %s\n", actual, expected);

		free(actual);
		free(expected);

		return -1;
	}

	return TRUE;
}

static const char* TEST_HMAC_SHA1_DATA = "Hi There";
static const BYTE* TEST_HMAC_SHA1_KEY = (BYTE*) "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b";
static const BYTE* TEST_HMAC_SHA1_HASH = (BYTE*) "\xb6\x17\x31\x86\x55\x05\x72\x64\xe2\x8b\xc0\xb6\xfb\x37\x8c\x8e\xf1\x46\xbe\x00";

BOOL test_crypto_hash_hmac_sha1()
{
	BYTE hash[20];
	WINPR_HMAC_CTX ctx;

	winpr_HMAC_Init(&ctx, WINPR_MD_SHA1, TEST_HMAC_SHA1_KEY, 20);
	winpr_HMAC_Update(&ctx, (BYTE*) TEST_HMAC_SHA1_DATA, strlen(TEST_HMAC_SHA1_DATA));
	winpr_HMAC_Final(&ctx, hash);

	if (memcmp(hash, TEST_HMAC_SHA1_HASH, 20) != 0)
	{
		char* actual;
		char* expected;

		actual = winpr_BinToHexString(hash, 20, FALSE);
		expected = winpr_BinToHexString(TEST_HMAC_SHA1_HASH, 20, FALSE);

		fprintf(stderr, "unexpected HMAC-SHA1 hash: Actual: %s Expected: %s\n", actual, expected);

		free(actual);
		free(expected);

		return -1;
	}

	return TRUE;
}

int TestCryptoHash(int argc, char* argv[])
{
	if (!test_crypto_hash_md5())
		return -1;

	if (!test_crypto_hash_md4())
		return -1;

	if (!test_crypto_hash_sha1())
		return -1;

	if (!test_crypto_hash_hmac_md5())
		return -1;

	if (!test_crypto_hash_hmac_sha1())
		return -1;

	return 0;
}

