/*
 * WinPR regression test: winpr_Cipher_NewEx() must honor the caller-provided IV
 * (backend-independent).
 *
 * Motivated by: on mbedTLS builds the mbedTLS branch of winpr_Cipher_NewEx()
 * never calls mbedtls_cipher_set_iv(), so the IV argument is silently ignored
 * (see winpr/libwinpr/crypto/cipher.c, OpenSSL branch :619 vs mbedTLS branch
 * :625-649). The existing TestCryptoCipher round-trip cannot detect this,
 * because it uses the same (ignored) IV for encryption and decryption.
 *
 * This test has two independent checks (AES-128-CBC):
 *   1) NIST SP 800-38A F.2.1, CBC-AES128, block #1 known-answer test
 *   2) The API contract: same key + same plaintext + different IV must yield
 *      different ciphertexts
 *
 * Expected result: pass on all backends that honor the IV (OpenSSL, and
 * mbedTLS after the fix). The test fails on an unpatched mbedTLS build.
 *
 * To wire up: add TestCryptoCipherIV.c to TESTS in
 * winpr/libwinpr/crypto/test/CMakeLists.txt and run
 *   TestCrypto TestCryptoCipherIV
 */
#include <winpr/crt.h>
#include <winpr/crypto.h>
#include <winpr/print.h>
#include <winpr/ssl.h>

#include <stdio.h>
#include <string.h>

static BOOL encrypt_aes128cbc(const BYTE* key, size_t keylen, const BYTE* iv, size_t ivlen,
                              const BYTE* in, size_t inlen, BYTE* out, size_t* outlen)
{
	WINPR_CIPHER_CTX* ctx =
	    winpr_Cipher_NewEx(WINPR_CIPHER_AES_128_CBC, WINPR_ENCRYPT, key, keylen, iv, ivlen);
	size_t o = 0;
	size_t f = 0;

	if (!ctx)
		return FALSE;

	if (!winpr_Cipher_Update(ctx, in, inlen, out, &o))
		goto fail;

	if (!winpr_Cipher_Final(ctx, out + o, &f))
		goto fail;

	*outlen = o + f;
	winpr_Cipher_Free(ctx);
	return TRUE;

fail:
	winpr_Cipher_Free(ctx);
	return FALSE;
}

int TestCryptoCipherIV(int argc, char* argv[])
{
	/* NIST SP 800-38A F.2.1: CBC-AES128.Encrypt, block #1 */
	const BYTE kat_key[16] = { 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
		                       0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c };
	const BYTE kat_iv[16] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		                      0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };
	const BYTE kat_pt[16] = { 0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
		                      0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a };
	const BYTE kat_ct[16] = { 0x76, 0x49, 0xab, 0xac, 0x81, 0x19, 0xb2, 0x46,
		                      0xce, 0xe9, 0x8e, 0x9b, 0x12, 0xe9, 0x19, 0x7d };

	BYTE iv1[16];
	BYTE iv2[16];
	BYTE c_kat[32];
	BYTE c1[32];
	BYTE c2[32];
	size_t l_kat = 0;
	size_t l1 = 0;
	size_t l2 = 0;
	int rc = -1;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT))
	{
		(void)fprintf(stderr, "%s: winpr_InitializeSSL failed\n", __func__);
		return -1;
	}

	memset(iv1, 0x11, sizeof(iv1));
	memset(iv2, 0x22, sizeof(iv2));

	if (!encrypt_aes128cbc(kat_key, sizeof(kat_key), kat_iv, sizeof(kat_iv), kat_pt, sizeof(kat_pt),
	                       c_kat, &l_kat) ||
	    !encrypt_aes128cbc(kat_key, sizeof(kat_key), iv1, sizeof(iv1), kat_pt, sizeof(kat_pt), c1,
	                       &l1) ||
	    !encrypt_aes128cbc(kat_key, sizeof(kat_key), iv2, sizeof(iv2), kat_pt, sizeof(kat_pt), c2,
	                       &l2))
	{
		(void)fprintf(stderr, "%s: encryption failed\n", __func__);
		return -1;
	}

	if ((l_kat != sizeof(kat_ct)) || (memcmp(c_kat, kat_ct, sizeof(kat_ct)) != 0))
	{
		(void)fprintf(stderr,
		              "%s: AES-128-CBC known-answer test failed - the IV is not honored by the "
		              "cipher backend\n",
		              __func__);
		goto out;
	}

	if ((l1 != l2) || (memcmp(c1, c2, l1) == 0))
	{
		(void)fprintf(stderr,
		              "%s: ciphertexts with different IVs are identical - the IV is not honored "
		              "by the cipher backend\n",
		              __func__);
		goto out;
	}

	(void)printf("%s: IV handling OK\n", __func__);
	rc = 0;

out:
	return rc;
}