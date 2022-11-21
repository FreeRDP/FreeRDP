
#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/crypto.h>
#include <winpr/ssl.h>
#include <winpr/wlog.h>

static const char* SECRET_PASSWORD_TEST = "MySecretPassword123!";

int TestCryptoProtectMemory(int argc, char* argv[])
{
	UINT32 cbPlainText;
	UINT32 cbCipherText;
	const char* pPlainText;
	BYTE* pCipherText;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	pPlainText = SECRET_PASSWORD_TEST;
	cbPlainText = strlen(pPlainText) + 1;
	cbCipherText = cbPlainText +
	               (CRYPTPROTECTMEMORY_BLOCK_SIZE - (cbPlainText % CRYPTPROTECTMEMORY_BLOCK_SIZE));
	printf("cbPlainText: %" PRIu32 " cbCipherText: %" PRIu32 "\n", cbPlainText, cbCipherText);
	pCipherText = (BYTE*)malloc(cbCipherText);
	if (!pCipherText)
	{
		printf("Unable to allocate memory\n");
		return -1;
	}
	CopyMemory(pCipherText, pPlainText, cbPlainText);
	ZeroMemory(&pCipherText[cbPlainText], (cbCipherText - cbPlainText));
	winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT);

	if (!CryptProtectMemory(pCipherText, cbCipherText, CRYPTPROTECTMEMORY_SAME_PROCESS))
	{
		printf("CryptProtectMemory failure\n");
		return -1;
	}

	printf("PlainText: %s (cbPlainText = %" PRIu32 ", cbCipherText = %" PRIu32 ")\n", pPlainText,
	       cbPlainText, cbCipherText);
	winpr_HexDump("crypto.test", WLOG_DEBUG, pCipherText, cbCipherText);

	if (!CryptUnprotectMemory(pCipherText, cbCipherText, CRYPTPROTECTMEMORY_SAME_PROCESS))
	{
		printf("CryptUnprotectMemory failure\n");
		return -1;
	}

	printf("Decrypted CipherText: %s\n", pCipherText);
	SecureZeroMemory(pCipherText, cbCipherText);
	free(pCipherText);
	return 0;
}
