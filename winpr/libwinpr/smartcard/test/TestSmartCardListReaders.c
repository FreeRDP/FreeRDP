
#include <winpr/crt.h>
#include <winpr/smartcard.h>

int TestSmartCardListReaders(int argc, char* argv[])
{
	LONG lStatus;
	LPSTR pReader;
	SCARDCONTEXT hSC;
	LPSTR mszReaders = NULL;
	DWORD cchReaders = SCARD_AUTOALLOCATE;

	lStatus = SCardEstablishContext(SCARD_SCOPE_USER, NULL, NULL, &hSC);

	if (lStatus != SCARD_S_SUCCESS)
	{
		printf("SCardEstablishContext failure: %s (0x%08X)\n",
				SCardGetErrorString(lStatus), (int) lStatus);
		return 0;
	}

	lStatus = SCardListReadersA(hSC, NULL, (LPSTR) &mszReaders, &cchReaders);

	if (lStatus != SCARD_S_SUCCESS)
	{
		if (lStatus == SCARD_E_NO_READERS_AVAILABLE)
			printf("SCARD_E_NO_READERS_AVAILABLE\n");
		else
			return -1;
	}
	else
	{
		pReader = mszReaders;

		while (*pReader)
		{
			printf("Reader: %s\n", pReader);
			pReader = pReader + strlen((CHAR*) pReader) + 1;
		}

		lStatus = SCardFreeMemory(hSC, mszReaders);

		if (lStatus != SCARD_S_SUCCESS)
			printf("Failed SCardFreeMemory\n");
	}

	SCardReleaseContext(hSC);

	return 0;
}

