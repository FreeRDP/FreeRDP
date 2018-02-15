// compile against PCSC  gcc -o scardtest TestSmartCardStatus.c -DPCSC=1 -I /usr/include/PCSC -lpcsclite
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__APPLE__) || defined(PCSC)
#include <PCSC/winscard.h>
#include <PCSC/wintypes.h>
#elif defined (__linux__)
#include <winpr/crt.h>
#include <winpr/smartcard.h>
#include <winpr/synch.h>
#else
#include <winscard.h>
#endif

#if defined(PCSC)
int main(int argc, char* argv[])
#else
int TestSmartCardStatus(int argc, char* argv[])
#endif
{
	SCARDCONTEXT hContext;
	LPSTR mszReaders;
	DWORD cchReaders = 0;
	DWORD err;
	SCARDHANDLE hCard;
	DWORD dwActiveProtocol;
	char name[100];
	char* aname = NULL;
	char* aatr = NULL;
	DWORD len;
	BYTE atr[32];
	DWORD atrlen = 32;
	DWORD status = 0;
	DWORD protocol = 0;
	err = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);

	if (err != SCARD_S_SUCCESS)
	{
		printf("ScardEstablishedContext: 0x%08x\n", err);
		return winpr_exit(-1);
	}

	err = SCardListReaders(hContext, "SCard$AllReaders", NULL, &cchReaders);

	if (err != 0)
	{
		printf("ScardListReaders: 0x%08x\n", err);
		return winpr_exit(-1);
	}

	mszReaders = calloc(cchReaders, sizeof(char));

	if (!mszReaders)
	{
		printf("calloc\n");
		return winpr_exit(-1);
	}

	err = SCardListReaders(hContext, "SCard$AllReaders", mszReaders, &cchReaders);

	if (err != SCARD_S_SUCCESS)
	{
		printf("ScardListReaders: 0x%08x\n", err);
		return winpr_exit(-1);
	}

	printf("Reader: %s\n", mszReaders);
	err = SCardConnect(hContext, mszReaders, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
	                   &hCard, &dwActiveProtocol);

	if (err != SCARD_S_SUCCESS)
	{
		printf("ScardConnect: 0x%08x\n", err);
		return winpr_exit(-1);
	}

	free(mszReaders);


	printf("# test 1 - get reader length\n");
	err = SCardStatus(hCard, NULL, &len, NULL, NULL, NULL, NULL);
	if (err != SCARD_S_SUCCESS)
	{
		printf("SCardStatus: 0x%08x\n", err);
		return winpr_exit(-1);
	}
	printf("reader name length: %u\n", len);


	printf("# test 2 - get reader name value\n");
	err = SCardStatus(hCard, name, &len, NULL, NULL, NULL, NULL);
	if (err != SCARD_S_SUCCESS)
	{
		printf("SCardStatus: 0x%08x\n", err);
		return winpr_exit(-1);
	}
	printf("Reader name: %s (%ld)\n", name, strlen(name));


	printf("# test 3 - get all values - pre allocated\n");
	err = SCardStatus(hCard, name, &len, &status, &protocol, atr, &atrlen);
	if (err != SCARD_S_SUCCESS)
	{
		printf("SCardStatus: 0x%08x\n", err);
		return winpr_exit(-1);
	}
	printf("Reader name: %s (%ld/len %u)\n", name, strlen(name), len);
	printf("status: 0x%08X\n", status);
	printf("proto: 0x%08X\n", protocol);
	printf("atrlen: %u\n", atrlen);


	printf("# test 4 - get all values - auto allocate\n");
	len = atrlen = SCARD_AUTOALLOCATE;
	err = SCardStatus(hCard, (LPSTR)&aname, &len, &status, &protocol, (LPBYTE)&aatr, &atrlen);
	if (err != SCARD_S_SUCCESS)
	{
		printf("SCardStatus: 0x%08x\n", err);
		return winpr_exit(-1);
	}
	printf("Reader name: %s (%ld/%u)\n", aname, strlen(aname), len);
	printf("status: 0x%08X\n", status);
	printf("proto: 0x%08X\n", protocol);
	printf("atrlen: %u\n", atrlen);
	SCardFreeMemory(hContext, aname);
	SCardFreeMemory(hContext, aatr);


	printf("# test 5 - get status and protocol only\n");
	err = SCardStatus(hCard, NULL, NULL, &status, &protocol, NULL, NULL);
	if (err != SCARD_S_SUCCESS)
	{
		printf("SCardStatus: 0x%08x\n", err);
		return winpr_exit(-1);
	}
	printf("status: 0x%08X\n", status);
	printf("proto: 0x%08X\n", protocol);


	printf("# test 6 - get atr only auto allocated\n");
	atrlen = SCARD_AUTOALLOCATE;
	err = SCardStatus(hCard, NULL, NULL, NULL, NULL, (LPBYTE)&aatr, &atrlen);
	if (err != SCARD_S_SUCCESS)
	{
		printf("SCardStatus: 0x%08x\n", err);
		return winpr_exit(-1);
	}
	printf("atrlen: %u\n", atrlen);
	SCardFreeMemory(hContext, aatr);


	printf("# test 7 - get atr only pre allocated\n");
	atrlen = 32;
	err = SCardStatus(hCard, NULL, NULL, NULL, NULL, atr, &atrlen);
	if (err != SCARD_S_SUCCESS)
	{
		printf("SCardStatus: 0x%08x\n", err);
		return winpr_exit(-1);
	}
	printf("atrlen: %u\n", atrlen);
	SCardDisconnect(hCard, SCARD_LEAVE_CARD);
	SCardReleaseContext(hContext);

	return winpr_exit(0);
}
