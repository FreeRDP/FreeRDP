
#include <winpr/crt.h>
#include <winpr/smartcard.h>

int TestSmartCardCache(int argc, char* argv[])
{
	LONG lStatus = 0;
	LPSTR pReader = NULL;
	SCARDCONTEXT hSC = 0;
	LPSTR mszReaders = NULL;
	DWORD cchReaders = SCARD_AUTOALLOCATE;
	char * cacheValue = calloc(17, sizeof(char));
	memcpy(cacheValue, "test-cache-value", 16);

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	lStatus = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hSC);

	if (lStatus != SCARD_S_SUCCESS)
	{
		printf("SCardEstablishContext failure: %s (0x%08" PRIX32 ")\n",
		       SCardGetErrorString(lStatus), lStatus);
		return 0;
	}

	lStatus = SCardListReadersA(hSC, NULL, (LPSTR)&mszReaders, &cchReaders);
	if (lStatus != SCARD_S_SUCCESS)
	{
		if (lStatus == SCARD_E_NO_READERS_AVAILABLE)
			printf("SCARD_E_NO_READERS_AVAILABLE\n");
		else
			return -1;
	}
	pReader = mszReaders;
	printf("connecting to reader: %s\n", pReader);
	SCARDHANDLE phCard = 0;
	DWORD pdwActiveProtocol = 0;
	lStatus = SCardConnectA(hSC, pReader, SCARD_SHARE_SHARED, SCARD_PROTOCOL_Tx, &phCard, &pdwActiveProtocol);
	if (lStatus != SCARD_S_SUCCESS)
	{
		printf("SCardConnectA failure: %s (0x%08" PRIX32 ")\n",
		       SCardGetErrorString(lStatus), lStatus);
		return 0;
	}
	printf("reader connected: %s\n", pReader);
	UUID cardUuid = {.Data1 = 0x12345678, .Data2 = 0x9ABC, .Data3 = 0xDEF0, .Data4 = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC}};
	printf("writing cache...\n");
	lStatus = SCardWriteCacheA(hSC, &cardUuid, 1, "test-cache-key", cacheValue, 17);
	if (lStatus != SCARD_S_SUCCESS)
	{
		printf("SCardWriteCacheA failure: %s (0x%08" PRIX32 ")\n",
		       SCardGetErrorString(lStatus), lStatus);
		return -1;
	}
	printf("write cache success\n");
	LPSTR pszCacheValue = calloc(17, sizeof(char));
	DWORD dwCacheValueLength = 17;
	printf("reading cache...\n");
	lStatus = SCardReadCacheA(hSC, &cardUuid, 1, "test-cache-key", pszCacheValue, &dwCacheValueLength);
	if (lStatus != SCARD_S_SUCCESS)
	{
		printf("SCardReadCacheA failure: %s (0x%08" PRIX32 ")\n",
		       SCardGetErrorString(lStatus), lStatus);
		return -1;
	}
	printf("read cache success. read: %d bytes\n", dwCacheValueLength);
	for (int i = 0; i < dwCacheValueLength; i++)
	{
		printf("%02X ", pszCacheValue[i]);
	}
	printf("read cache success. read: %d bytes. data: %s\n", dwCacheValueLength, pszCacheValue);
	// compare pszCacheValue with "test-cache-value"
	if (dwCacheValueLength != 17 || _stricmp(pszCacheValue, cacheValue) != 0)
	{
		printf("Cache Value Mismatch\n");
		return -1;
	}
	printf("disconnecting from card\n");
	lStatus = SCardDisconnect(phCard, SCARD_LEAVE_CARD);
	if (lStatus != SCARD_S_SUCCESS)
	{
		printf("SCardDisconnect failure: %s (0x%08" PRIX32 ")\n",
		       SCardGetErrorString(lStatus), lStatus);
		return -1;
	}
	SCARDCONTEXT hSC2 = 0;
	printf("establishing new context\n");
	lStatus = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hSC2);
	if (lStatus != SCARD_S_SUCCESS)
	{
		printf("SCardEstablishContext failure: %s (0x%08" PRIX32 ")\n",
		       SCardGetErrorString(lStatus), lStatus);
		return -1;
	}
	printf("new context established\n");
	printf("connecting to card\n");
	lStatus = SCardConnectA(hSC2, pReader, SCARD_SHARE_SHARED, SCARD_PROTOCOL_Tx, &phCard, &pdwActiveProtocol);
	if (lStatus != SCARD_S_SUCCESS)
	{
		printf("SCardConnectA failure: %s (0x%08" PRIX32 ")\n",
		       SCardGetErrorString(lStatus), lStatus);
		return -1;
	}
	printf("connected to card\n");
	printf("reading cache\n");
	lStatus = SCardReadCacheA(hSC2, &cardUuid, 1, "test-cache-key", &pszCacheValue, &dwCacheValueLength);
	if (lStatus != SCARD_S_SUCCESS)
	{
		printf("SCardReadCacheA failure: %s (0x%08" PRIX32 ")\n",
		       SCardGetErrorString(lStatus), lStatus);
		return -1;
	}
	// compare pszCacheValue with "test-cache-value"
	if (dwCacheValueLength != 16 || _stricmp(pszCacheValue, cacheValue) != 0)
	{
		printf("Cache Value Mismatch from different contexts\n");
		return -1;
	}
	return 0;
}
