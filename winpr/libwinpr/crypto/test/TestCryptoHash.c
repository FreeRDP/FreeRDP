
#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/crypto.h>
#include <winpr/ssl.h>

static const char TEST_MD5_DATA[] = "test";
static const BYTE TEST_MD5_HASH[] =
    "\x09\x8f\x6b\xcd\x46\x21\xd3\x73\xca\xde\x4e\x83\x26\x27\xb4\xf6";

static BOOL test_crypto_hash_md5(void)
{
	BOOL result = FALSE;
	BYTE hash[WINPR_MD5_DIGEST_LENGTH];
	WINPR_DIGEST_CTX* ctx;

	if (!(ctx = winpr_Digest_New()))
	{
		fprintf(stderr, "%s: winpr_Digest_New failed\n", __FUNCTION__);
		return FALSE;
	}
	if (!winpr_Digest_Init(ctx, WINPR_MD_MD5))
	{
		fprintf(stderr, "%s: winpr_Digest_Init failed\n", __FUNCTION__);
		goto out;
	}
	if (!winpr_Digest_Update(ctx, (const BYTE*)TEST_MD5_DATA,
	                         strnlen(TEST_MD5_DATA, sizeof(TEST_MD5_DATA))))
	{
		fprintf(stderr, "%s: winpr_Digest_Update failed\n", __FUNCTION__);
		goto out;
	}
	if (!winpr_Digest_Final(ctx, hash, sizeof(hash)))
	{
		fprintf(stderr, "%s: winpr_Digest_Final failed\n", __FUNCTION__);
		goto out;
	}
	if (memcmp(hash, TEST_MD5_HASH, WINPR_MD5_DIGEST_LENGTH) != 0)
	{
		char* actual;
		char* expected;

		actual = winpr_BinToHexString(hash, WINPR_MD5_DIGEST_LENGTH, FALSE);
		expected = winpr_BinToHexString(TEST_MD5_HASH, WINPR_MD5_DIGEST_LENGTH, FALSE);

		fprintf(stderr, "unexpected MD5 hash: Actual: %s Expected: %s\n", actual, expected);

		free(actual);
		free(expected);

		goto out;
	}

	result = TRUE;
out:
	winpr_Digest_Free(ctx);
	return result;
}

static const char TEST_MD4_DATA[] = "test";
static const BYTE TEST_MD4_HASH[] =
    "\xdb\x34\x6d\x69\x1d\x7a\xcc\x4d\xc2\x62\x5d\xb1\x9f\x9e\x3f\x52";

static BOOL test_crypto_hash_md4(void)
{
	BOOL result = FALSE;
	BYTE hash[WINPR_MD4_DIGEST_LENGTH];
	WINPR_DIGEST_CTX* ctx;

	if (!(ctx = winpr_Digest_New()))
	{
		fprintf(stderr, "%s: winpr_Digest_New failed\n", __FUNCTION__);
		return FALSE;
	}
	if (!winpr_Digest_Init(ctx, WINPR_MD_MD4))
	{
		fprintf(stderr, "%s: winpr_Digest_Init failed\n", __FUNCTION__);
		goto out;
	}
	if (!winpr_Digest_Update(ctx, (const BYTE*)TEST_MD4_DATA,
	                         strnlen(TEST_MD4_DATA, sizeof(TEST_MD4_DATA))))
	{
		fprintf(stderr, "%s: winpr_Digest_Update failed\n", __FUNCTION__);
		goto out;
	}
	if (!winpr_Digest_Final(ctx, hash, sizeof(hash)))
	{
		fprintf(stderr, "%s: winpr_Digest_Final failed\n", __FUNCTION__);
		goto out;
	}
	if (memcmp(hash, TEST_MD4_HASH, WINPR_MD4_DIGEST_LENGTH) != 0)
	{
		char* actual;
		char* expected;

		actual = winpr_BinToHexString(hash, WINPR_MD4_DIGEST_LENGTH, FALSE);
		expected = winpr_BinToHexString(TEST_MD4_HASH, WINPR_MD4_DIGEST_LENGTH, FALSE);

		fprintf(stderr, "unexpected MD4 hash: Actual: %s Expected: %s\n", actual, expected);

		free(actual);
		free(expected);

		goto out;
	}

	result = TRUE;
out:
	winpr_Digest_Free(ctx);
	return result;
}

static const char TEST_SHA1_DATA[] = "test";
static const BYTE TEST_SHA1_HASH[] =
    "\xa9\x4a\x8f\xe5\xcc\xb1\x9b\xa6\x1c\x4c\x08\x73\xd3\x91\xe9\x87\x98\x2f\xbb\xd3";

static BOOL test_crypto_hash_sha1(void)
{
	BOOL result = FALSE;
	BYTE hash[WINPR_SHA1_DIGEST_LENGTH];
	WINPR_DIGEST_CTX* ctx;

	if (!(ctx = winpr_Digest_New()))
	{
		fprintf(stderr, "%s: winpr_Digest_New failed\n", __FUNCTION__);
		return FALSE;
	}
	if (!winpr_Digest_Init(ctx, WINPR_MD_SHA1))
	{
		fprintf(stderr, "%s: winpr_Digest_Init failed\n", __FUNCTION__);
		goto out;
	}
	if (!winpr_Digest_Update(ctx, (const BYTE*)TEST_SHA1_DATA,
	                         strnlen(TEST_SHA1_DATA, sizeof(TEST_SHA1_DATA))))
	{
		fprintf(stderr, "%s: winpr_Digest_Update failed\n", __FUNCTION__);
		goto out;
	}
	if (!winpr_Digest_Final(ctx, hash, sizeof(hash)))
	{
		fprintf(stderr, "%s: winpr_Digest_Final failed\n", __FUNCTION__);
		goto out;
	}

	if (memcmp(hash, TEST_SHA1_HASH, WINPR_MD5_DIGEST_LENGTH) != 0)
	{
		char* actual;
		char* expected;

		actual = winpr_BinToHexString(hash, WINPR_SHA1_DIGEST_LENGTH, FALSE);
		expected = winpr_BinToHexString(TEST_SHA1_HASH, WINPR_SHA1_DIGEST_LENGTH, FALSE);

		fprintf(stderr, "unexpected SHA1 hash: Actual: %s Expected: %s\n", actual, expected);

		free(actual);
		free(expected);

		goto out;
	}

	result = TRUE;
out:
	winpr_Digest_Free(ctx);
	return result;
}

static const char TEST_HMAC_MD5_DATA[] = "Hi There";
static const BYTE TEST_HMAC_MD5_KEY[] =
    "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b";
static const BYTE TEST_HMAC_MD5_HASH[] =
    "\x92\x94\x72\x7a\x36\x38\xbb\x1c\x13\xf4\x8e\xf8\x15\x8b\xfc\x9d";

static BOOL test_crypto_hash_hmac_md5(void)
{
	BYTE hash[WINPR_MD5_DIGEST_LENGTH];
	WINPR_HMAC_CTX* ctx;
	BOOL result = FALSE;

	if (!(ctx = winpr_HMAC_New()))
	{
		fprintf(stderr, "%s: winpr_HMAC_New failed\n", __FUNCTION__);
		return FALSE;
	}

	if (!winpr_HMAC_Init(ctx, WINPR_MD_MD5, TEST_HMAC_MD5_KEY, WINPR_MD5_DIGEST_LENGTH))
	{
		fprintf(stderr, "%s: winpr_HMAC_Init failed\n", __FUNCTION__);
		goto out;
	}
	if (!winpr_HMAC_Update(ctx, (const BYTE*)TEST_HMAC_MD5_DATA,
	                       strnlen(TEST_HMAC_MD5_DATA, sizeof(TEST_HMAC_MD5_DATA))))
	{
		fprintf(stderr, "%s: winpr_HMAC_Update failed\n", __FUNCTION__);
		goto out;
	}
	if (!winpr_HMAC_Final(ctx, hash, sizeof(hash)))
	{
		fprintf(stderr, "%s: winpr_HMAC_Final failed\n", __FUNCTION__);
		goto out;
	}

	if (memcmp(hash, TEST_HMAC_MD5_HASH, WINPR_MD5_DIGEST_LENGTH) != 0)
	{
		char* actual;
		char* expected;

		actual = winpr_BinToHexString(hash, WINPR_MD5_DIGEST_LENGTH, FALSE);
		expected = winpr_BinToHexString(TEST_HMAC_MD5_HASH, WINPR_MD5_DIGEST_LENGTH, FALSE);

		fprintf(stderr, "unexpected HMAC-MD5 hash: Actual: %s Expected: %s\n", actual, expected);

		free(actual);
		free(expected);

		goto out;
	}

	result = TRUE;
out:
	winpr_HMAC_Free(ctx);
	return result;
}

static const char TEST_HMAC_SHA1_DATA[] = "Hi There";
static const BYTE TEST_HMAC_SHA1_KEY[] =
    "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b";
static const BYTE TEST_HMAC_SHA1_HASH[] =
    "\xb6\x17\x31\x86\x55\x05\x72\x64\xe2\x8b\xc0\xb6\xfb\x37\x8c\x8e\xf1\x46\xbe\x00";

static BOOL test_crypto_hash_hmac_sha1(void)
{
	BYTE hash[WINPR_SHA1_DIGEST_LENGTH];
	WINPR_HMAC_CTX* ctx;
	BOOL result = FALSE;

	if (!(ctx = winpr_HMAC_New()))
	{
		fprintf(stderr, "%s: winpr_HMAC_New failed\n", __FUNCTION__);
		return FALSE;
	}

	if (!winpr_HMAC_Init(ctx, WINPR_MD_SHA1, TEST_HMAC_SHA1_KEY, WINPR_SHA1_DIGEST_LENGTH))
	{
		fprintf(stderr, "%s: winpr_HMAC_Init failed\n", __FUNCTION__);
		goto out;
	}
	if (!winpr_HMAC_Update(ctx, (const BYTE*)TEST_HMAC_SHA1_DATA,
	                       strnlen(TEST_HMAC_SHA1_DATA, sizeof(TEST_HMAC_SHA1_DATA))))
	{
		fprintf(stderr, "%s: winpr_HMAC_Update failed\n", __FUNCTION__);
		goto out;
	}
	if (!winpr_HMAC_Final(ctx, hash, sizeof(hash)))
	{
		fprintf(stderr, "%s: winpr_HMAC_Final failed\n", __FUNCTION__);
		goto out;
	}

	if (memcmp(hash, TEST_HMAC_SHA1_HASH, WINPR_SHA1_DIGEST_LENGTH) != 0)
	{
		char* actual;
		char* expected;

		actual = winpr_BinToHexString(hash, WINPR_SHA1_DIGEST_LENGTH, FALSE);
		expected = winpr_BinToHexString(TEST_HMAC_SHA1_HASH, WINPR_SHA1_DIGEST_LENGTH, FALSE);

		fprintf(stderr, "unexpected HMAC-SHA1 hash: Actual: %s Expected: %s\n", actual, expected);

		free(actual);
		free(expected);

		goto out;
	}

	result = TRUE;
out:
	winpr_HMAC_Free(ctx);
	return result;
}

int TestCryptoHash(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT);

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
