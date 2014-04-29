
#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/wtsapi.h>

int TestWtsApiQuerySessionInformation(int argc, char* argv[])
{
	DWORD index;
	DWORD count;
	BOOL bSuccess;
	HANDLE hServer;
	LPSTR pBuffer;
	DWORD sessionId;
	DWORD bytesReturned;
	PWTS_SESSION_INFO pSessionInfo;

	hServer = WTS_CURRENT_SERVER_HANDLE;

	count = 0;
	pSessionInfo = NULL;

	bSuccess = WTSEnumerateSessions(hServer, 0, 1, &pSessionInfo, &count);

	if (!bSuccess)
	{
		printf("WTSEnumerateSessions failed: %d\n", (int) GetLastError());
		return 0;
	}

	printf("WTSEnumerateSessions count: %d\n", (int) count);

	for (index = 0; index < count; index++)
	{
		pBuffer = NULL;
		bytesReturned = 0;
		char* Username;
		char* Domain;
		char* ClientName;
		ULONG ClientBuildNumber;
		USHORT ClientProductId;
		ULONG ClientHardwareId;
		USHORT ClientProtocolType;
		PWTS_CLIENT_DISPLAY ClientDisplay;
		PWTS_CLIENT_ADDRESS ClientAddress;
		WTS_CONNECTSTATE_CLASS ConnectState;

		sessionId = pSessionInfo[index].SessionId;

		printf("[%d] SessionId: %d State: %d\n", (int) index,
				(int) pSessionInfo[index].SessionId,
				(int) pSessionInfo[index].State);

		/* WTSUserName */

		bSuccess = WTSQuerySessionInformation(hServer, sessionId, WTSUserName, &pBuffer, &bytesReturned);

		if (!bSuccess)
		{
			printf("WTSQuerySessionInformation WTSUserName failed: %d\n", (int) GetLastError());
			return -1;
		}

		Username = (char*) pBuffer;
		printf("\tWTSUserName: %s\n", Username);

		/* WTSDomainName */

		bSuccess = WTSQuerySessionInformation(hServer, sessionId, WTSDomainName, &pBuffer, &bytesReturned);

		if (!bSuccess)
		{
			printf("WTSQuerySessionInformation WTSDomainName failed: %d\n", (int) GetLastError());
			return -1;
		}

		Domain = (char*) pBuffer;
		printf("\tWTSDomainName: %s\n", Domain);

		/* WTSConnectState */

		bSuccess = WTSQuerySessionInformation(hServer, sessionId, WTSConnectState, &pBuffer, &bytesReturned);

		if (!bSuccess)
		{
			printf("WTSQuerySessionInformation WTSConnectState failed: %d\n", (int) GetLastError());
			return -1;
		}

		ConnectState = *((WTS_CONNECTSTATE_CLASS*) pBuffer);
		printf("\tWTSConnectState: %d\n", (int) ConnectState);

		/* WTSClientBuildNumber */

		bSuccess = WTSQuerySessionInformation(hServer, sessionId, WTSClientBuildNumber, &pBuffer, &bytesReturned);

		if (!bSuccess)
		{
			printf("WTSQuerySessionInformation WTSClientBuildNumber failed: %d\n", (int) GetLastError());
			return -1;
		}

		ClientBuildNumber = *((ULONG*) pBuffer);
		printf("\tWTSClientBuildNumber: %d\n", (int) ClientBuildNumber);

		/* WTSClientName */

		bSuccess = WTSQuerySessionInformation(hServer, sessionId, WTSClientName, &pBuffer, &bytesReturned);

		if (!bSuccess)
		{
			printf("WTSQuerySessionInformation WTSClientName failed: %d\n", (int) GetLastError());
			return -1;
		}

		ClientName = (char*) pBuffer;
		printf("\tWTSClientName: %s\n", ClientName);

		/* WTSClientProductId */

		bSuccess = WTSQuerySessionInformation(hServer, sessionId, WTSClientProductId, &pBuffer, &bytesReturned);

		if (!bSuccess)
		{
			printf("WTSQuerySessionInformation WTSClientProductId failed: %d\n", (int) GetLastError());
			return -1;
		}

		ClientProductId = *((USHORT*) pBuffer);
		printf("\tWTSClientProductId: %d\n", (int) ClientProductId);

		/* WTSClientHardwareId */

		bSuccess = WTSQuerySessionInformation(hServer, sessionId, WTSClientHardwareId, &pBuffer, &bytesReturned);

		if (!bSuccess)
		{
			printf("WTSQuerySessionInformation WTSClientHardwareId failed: %d\n", (int) GetLastError());
			return -1;
		}

		ClientHardwareId = *((ULONG*) pBuffer);
		printf("\tWTSClientHardwareId: %d\n", (int) ClientHardwareId);

		/* WTSClientAddress */

		bSuccess = WTSQuerySessionInformation(hServer, sessionId, WTSClientAddress, &pBuffer, &bytesReturned);

		if (!bSuccess)
		{
			printf("WTSQuerySessionInformation WTSClientAddress failed: %d\n", (int) GetLastError());
			return -1;
		}

		ClientAddress = (PWTS_CLIENT_ADDRESS) pBuffer;
		printf("\tWTSClientAddress: AddressFamily: %d\n",
				(int) ClientAddress->AddressFamily);

		/* WTSClientDisplay */

		bSuccess = WTSQuerySessionInformation(hServer, sessionId, WTSClientDisplay, &pBuffer, &bytesReturned);

		if (!bSuccess)
		{
			printf("WTSQuerySessionInformation WTSClientDisplay failed: %d\n", (int) GetLastError());
			return -1;
		}

		ClientDisplay = (PWTS_CLIENT_DISPLAY) pBuffer;
		printf("\tWTSClientDisplay: HorizontalResolution: %d VerticalResolution: %d ColorDepth: %d\n",
				(int) ClientDisplay->HorizontalResolution, (int) ClientDisplay->VerticalResolution,
				(int) ClientDisplay->ColorDepth);

		/* WTSClientProtocolType */

		bSuccess = WTSQuerySessionInformation(hServer, sessionId, WTSClientProtocolType, &pBuffer, &bytesReturned);

		if (!bSuccess)
		{
			printf("WTSQuerySessionInformation WTSClientProtocolType failed: %d\n", (int) GetLastError());
			return -1;
		}

		ClientProtocolType = *((USHORT*) pBuffer);
		printf("\tWTSClientProtocolType: %d\n", (int) ClientProtocolType);
	}

	WTSFreeMemory(pSessionInfo);

	return 0;
}
