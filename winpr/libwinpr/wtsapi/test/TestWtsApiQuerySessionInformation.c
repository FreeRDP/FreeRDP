
#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/wtsapi.h>
#include <winpr/environment.h>

int TestWtsApiQuerySessionInformation(int argc, char* argv[])
{
	DWORD count = 0;
	BOOL bSuccess = 0;
	HANDLE hServer = NULL;
	LPSTR pBuffer = NULL;
	DWORD sessionId = 0;
	DWORD bytesReturned = 0;
	PWTS_SESSION_INFOA pSessionInfo = NULL;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

#ifndef _WIN32
	if (!GetEnvironmentVariableA("WTSAPI_LIBRARY", NULL, 0))
	{
		printf("%s: No RDS environment detected, skipping test\n", __func__);
		return 0;
	}
#endif

	hServer = WTS_CURRENT_SERVER_HANDLE;

	count = 0;
	pSessionInfo = NULL;

	bSuccess = WTSEnumerateSessionsA(hServer, 0, 1, &pSessionInfo, &count);

	if (!bSuccess)
	{
		printf("WTSEnumerateSessions failed: %" PRIu32 "\n", GetLastError());
		return 0;
	}

	printf("WTSEnumerateSessions count: %" PRIu32 "\n", count);

	for (DWORD index = 0; index < count; index++)
	{
		char* Username = NULL;
		char* Domain = NULL;
		char* ClientName = NULL;
		ULONG ClientBuildNumber = 0;
		USHORT ClientProductId = 0;
		ULONG ClientHardwareId = 0;
		USHORT ClientProtocolType = 0;
		PWTS_CLIENT_DISPLAY ClientDisplay = NULL;
		PWTS_CLIENT_ADDRESS ClientAddress = NULL;
		WTS_CONNECTSTATE_CLASS ConnectState = WTSInit;

		pBuffer = NULL;
		bytesReturned = 0;

		sessionId = pSessionInfo[index].SessionId;

		printf("[%" PRIu32 "] SessionId: %" PRIu32 " State: %s (%u) WinstationName: '%s'\n", index,
		       pSessionInfo[index].SessionId, WTSSessionStateToString(pSessionInfo[index].State),
		       pSessionInfo[index].State, pSessionInfo[index].pWinStationName);

		/* WTSUserName */

		bSuccess =
		    WTSQuerySessionInformationA(hServer, sessionId, WTSUserName, &pBuffer, &bytesReturned);

		if (!bSuccess)
		{
			printf("WTSQuerySessionInformation WTSUserName failed: %" PRIu32 "\n", GetLastError());
			return -1;
		}

		Username = (char*)pBuffer;
		printf("\tWTSUserName: '%s'\n", Username);

		/* WTSDomainName */

		bSuccess = WTSQuerySessionInformationA(hServer, sessionId, WTSDomainName, &pBuffer,
		                                       &bytesReturned);

		if (!bSuccess)
		{
			printf("WTSQuerySessionInformation WTSDomainName failed: %" PRIu32 "\n",
			       GetLastError());
			return -1;
		}

		Domain = (char*)pBuffer;
		printf("\tWTSDomainName: '%s'\n", Domain);

		/* WTSConnectState */

		bSuccess = WTSQuerySessionInformationA(hServer, sessionId, WTSConnectState, &pBuffer,
		                                       &bytesReturned);

		if (!bSuccess)
		{
			printf("WTSQuerySessionInformation WTSConnectState failed: %" PRIu32 "\n",
			       GetLastError());
			return -1;
		}

		ConnectState = *((WTS_CONNECTSTATE_CLASS*)pBuffer);
		printf("\tWTSConnectState: %u (%s)\n", ConnectState, WTSSessionStateToString(ConnectState));

		/* WTSClientBuildNumber */

		bSuccess = WTSQuerySessionInformationA(hServer, sessionId, WTSClientBuildNumber, &pBuffer,
		                                       &bytesReturned);

		if (!bSuccess)
		{
			printf("WTSQuerySessionInformation WTSClientBuildNumber failed: %" PRIu32 "\n",
			       GetLastError());
			return -1;
		}

		ClientBuildNumber = *((ULONG*)pBuffer);
		printf("\tWTSClientBuildNumber: %" PRIu32 "\n", ClientBuildNumber);

		/* WTSClientName */

		bSuccess = WTSQuerySessionInformationA(hServer, sessionId, WTSClientName, &pBuffer,
		                                       &bytesReturned);

		if (!bSuccess)
		{
			printf("WTSQuerySessionInformation WTSClientName failed: %" PRIu32 "\n",
			       GetLastError());
			return -1;
		}

		ClientName = (char*)pBuffer;
		printf("\tWTSClientName: '%s'\n", ClientName);

		/* WTSClientProductId */

		bSuccess = WTSQuerySessionInformationA(hServer, sessionId, WTSClientProductId, &pBuffer,
		                                       &bytesReturned);

		if (!bSuccess)
		{
			printf("WTSQuerySessionInformation WTSClientProductId failed: %" PRIu32 "\n",
			       GetLastError());
			return -1;
		}

		ClientProductId = *((USHORT*)pBuffer);
		printf("\tWTSClientProductId: %" PRIu16 "\n", ClientProductId);

		/* WTSClientHardwareId */

		bSuccess = WTSQuerySessionInformationA(hServer, sessionId, WTSClientHardwareId, &pBuffer,
		                                       &bytesReturned);

		if (!bSuccess)
		{
			printf("WTSQuerySessionInformation WTSClientHardwareId failed: %" PRIu32 "\n",
			       GetLastError());
			return -1;
		}

		ClientHardwareId = *((ULONG*)pBuffer);
		printf("\tWTSClientHardwareId: %" PRIu32 "\n", ClientHardwareId);

		/* WTSClientAddress */

		bSuccess = WTSQuerySessionInformationA(hServer, sessionId, WTSClientAddress, &pBuffer,
		                                       &bytesReturned);

		if (!bSuccess)
		{
			printf("WTSQuerySessionInformation WTSClientAddress failed: %" PRIu32 "\n",
			       GetLastError());
			return -1;
		}

		ClientAddress = (PWTS_CLIENT_ADDRESS)pBuffer;
		printf("\tWTSClientAddress: AddressFamily: %" PRIu32 " Address: ",
		       ClientAddress->AddressFamily);
		for (DWORD i = 0; i < sizeof(ClientAddress->Address); i++)
			printf("%02" PRIX8 "", ClientAddress->Address[i]);
		printf("\n");

		/* WTSClientDisplay */

		bSuccess = WTSQuerySessionInformationA(hServer, sessionId, WTSClientDisplay, &pBuffer,
		                                       &bytesReturned);

		if (!bSuccess)
		{
			printf("WTSQuerySessionInformation WTSClientDisplay failed: %" PRIu32 "\n",
			       GetLastError());
			return -1;
		}

		ClientDisplay = (PWTS_CLIENT_DISPLAY)pBuffer;
		printf("\tWTSClientDisplay: HorizontalResolution: %" PRIu32 " VerticalResolution: %" PRIu32
		       " ColorDepth: %" PRIu32 "\n",
		       ClientDisplay->HorizontalResolution, ClientDisplay->VerticalResolution,
		       ClientDisplay->ColorDepth);

		/* WTSClientProtocolType */

		bSuccess = WTSQuerySessionInformationA(hServer, sessionId, WTSClientProtocolType, &pBuffer,
		                                       &bytesReturned);

		if (!bSuccess)
		{
			printf("WTSQuerySessionInformation WTSClientProtocolType failed: %" PRIu32 "\n",
			       GetLastError());
			return -1;
		}

		ClientProtocolType = *((USHORT*)pBuffer);
		printf("\tWTSClientProtocolType: %" PRIu16 "\n", ClientProtocolType);
	}

	WTSFreeMemory(pSessionInfo);

	return 0;
}
