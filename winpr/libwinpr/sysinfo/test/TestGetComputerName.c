#include <stdio.h>
#include <string.h>
#include <winpr/wtypes.h>
#include <winpr/sysinfo.h>
#include <winpr/error.h>

static BOOL Test_GetComputerName(void)
{
	/**
	 * BOOL WINAPI GetComputerName(LPTSTR lpBuffer, LPDWORD lpnSize);
	 *
	 * GetComputerName retrieves the NetBIOS name of the local computer.
	 *
	 * lpBuffer [out]
	 * A pointer to a buffer that receives the computer name or the cluster virtual server name.
	 * The buffer size should be large enough to contain MAX_COMPUTERNAME_LENGTH + 1 characters.
	 *
	 * lpnSize [in, out]
	 * On input, specifies the size of the buffer, in TCHARs.
	 * On output, the number of TCHARs copied to the destination buffer, not including the
	 * terminating null character. If the buffer is too small, the function fails and GetLastError
	 * returns ERROR_BUFFER_OVERFLOW. The lpnSize parameter specifies the size of the buffer
	 * required, including the terminating null character
	 *
	 */

	CHAR netbiosName1[MAX_COMPUTERNAME_LENGTH + 1];
	CHAR netbiosName2[MAX_COMPUTERNAME_LENGTH + 1];
	const DWORD netbiosBufferSize = sizeof(netbiosName1) / sizeof(CHAR);
	DWORD dwSize;
	DWORD dwNameLength;
	DWORD dwError;

	memset(netbiosName1, 0xAA, netbiosBufferSize);
	memset(netbiosName2, 0xBB, netbiosBufferSize);

	/* test with null buffer and zero size (required if buffer is null) */
	dwSize = 0;
	if (GetComputerNameA(NULL, &dwSize) == TRUE)
	{
		fprintf(stderr, "%s: (1) GetComputerNameA unexpectedly succeeded with null buffer\n",
		        __FUNCTION__);
		return FALSE;
	}
	if ((dwError = GetLastError()) != ERROR_BUFFER_OVERFLOW)
	{
		fprintf(stderr,
		        "%s: (2) GetLastError returned 0x%08" PRIX32 " (expected ERROR_BUFFER_OVERFLOW)\n",
		        __FUNCTION__, dwError);
		return FALSE;
	}

	/* test with valid buffer and zero size */
	dwSize = 0;
	if (GetComputerNameA(netbiosName1, &dwSize) == TRUE)
	{
		fprintf(stderr,
		        "%s: (3) GetComputerNameA unexpectedly succeeded with zero size parameter\n",
		        __FUNCTION__);
		return FALSE;
	}
	if ((dwError = GetLastError()) != ERROR_BUFFER_OVERFLOW)
	{
		fprintf(stderr,
		        "%s: (4) GetLastError returned 0x%08" PRIX32 " (expected ERROR_BUFFER_OVERFLOW)\n",
		        __FUNCTION__, dwError);
		return FALSE;
	}
	/* check if returned size is valid: must be the size of the buffer required, including the
	 * terminating null character in this case */
	if (dwSize < 2 || dwSize > netbiosBufferSize)
	{
		fprintf(stderr,
		        "%s: (5) GetComputerNameA returned wrong size %" PRIu32
		        " (expected something in the range from 2 to %" PRIu32 ")\n",
		        __FUNCTION__, dwSize, netbiosBufferSize);
		return FALSE;
	}
	dwNameLength = dwSize - 1;

	/* test with returned size */
	if (GetComputerNameA(netbiosName1, &dwSize) == FALSE)
	{
		fprintf(stderr, "%s: (6) GetComputerNameA failed with error: 0x%08" PRIX32 "\n",
		        __FUNCTION__, GetLastError());
		return FALSE;
	}
	/* check if returned size is valid */
	if (dwSize != dwNameLength)
	{
		fprintf(stderr,
		        "%s: (7) GetComputerNameA returned wrong size %" PRIu32 " (expected %" PRIu32 ")\n",
		        __FUNCTION__, dwSize, dwNameLength);
		return FALSE;
	}
	/* check if string is correctly terminated */
	if (netbiosName1[dwSize] != 0)
	{
		fprintf(stderr, "%s: (8) string termination error\n", __FUNCTION__);
		return FALSE;
	}

	/* test with real buffer size */
	dwSize = netbiosBufferSize;
	if (GetComputerNameA(netbiosName2, &dwSize) == FALSE)
	{
		fprintf(stderr, "%s: (9) GetComputerNameA failed with error: 0x%08" PRIX32 "\n",
		        __FUNCTION__, GetLastError());
		return FALSE;
	}
	/* check if returned size is valid */
	if (dwSize != dwNameLength)
	{
		fprintf(stderr,
		        "%s: (10) GetComputerNameA returned wrong size %" PRIu32 " (expected %" PRIu32
		        ")\n",
		        __FUNCTION__, dwSize, dwNameLength);
		return FALSE;
	}
	/* check if string is correctly terminated */
	if (netbiosName2[dwSize] != 0)
	{
		fprintf(stderr, "%s: (11) string termination error\n", __FUNCTION__);
		return FALSE;
	}

	/* compare the results */
	if (strcmp(netbiosName1, netbiosName2))
	{
		fprintf(stderr, "%s: (12) string compare mismatch\n", __FUNCTION__);
		return FALSE;
	}

	/* test with off by one buffer size */
	dwSize = dwNameLength;
	if (GetComputerNameA(netbiosName1, &dwSize) == TRUE)
	{
		fprintf(stderr,
		        "%s: (13) GetComputerNameA unexpectedly succeeded with limited buffer size\n",
		        __FUNCTION__);
		return FALSE;
	}
	/* check if returned size is valid */
	if (dwSize != dwNameLength + 1)
	{
		fprintf(stderr,
		        "%s: (14) GetComputerNameA returned wrong size %" PRIu32 " (expected %" PRIu32
		        ")\n",
		        __FUNCTION__, dwSize, dwNameLength + 1);
		return FALSE;
	}

	return TRUE;
}

static BOOL Test_GetComputerNameEx_Format(COMPUTER_NAME_FORMAT format)
{
	/**
	 * BOOL WINAPI GetComputerNameEx(COMPUTER_NAME_FORMAT NameType, LPTSTR lpBuffer, LPDWORD
	 * lpnSize);
	 *
	 * Retrieves a NetBIOS or DNS name associated with the local computer.
	 *
	 * NameType [in]
	 *   ComputerNameNetBIOS
	 *   ComputerNameDnsHostname
	 *   ComputerNameDnsDomain
	 *   ComputerNameDnsFullyQualified
	 *   ComputerNamePhysicalNetBIOS
	 *   ComputerNamePhysicalDnsHostname
	 *   ComputerNamePhysicalDnsDomain
	 *   ComputerNamePhysicalDnsFullyQualified
	 *
	 * lpBuffer [out]
	 * A pointer to a buffer that receives the computer name or the cluster virtual server name.
	 * The length of the name may be greater than MAX_COMPUTERNAME_LENGTH characters because DNS
	 * allows longer names. To ensure that this buffer is large enough, set this parameter to NULL
	 * and use the required buffer size returned in the lpnSize parameter.
	 *
	 * lpnSize [in, out]
	 * On input, specifies the size of the buffer, in TCHARs.
	 * On output, receives the number of TCHARs copied to the destination buffer, not including the
	 * terminating null character. If the buffer is too small, the function fails and GetLastError
	 * returns ERROR_MORE_DATA. This parameter receives the size of the buffer required, including
	 * the terminating null character. If lpBuffer is NULL, this parameter must be zero.
	 *
	 */

	CHAR computerName1[255 + 1];
	CHAR computerName2[255 + 1];

	const DWORD nameBufferSize = sizeof(computerName1) / sizeof(CHAR);
	DWORD dwSize;
	DWORD dwMinSize;
	DWORD dwNameLength;
	DWORD dwError;

	memset(computerName1, 0xAA, nameBufferSize);
	memset(computerName2, 0xBB, nameBufferSize);

	if (format == ComputerNameDnsDomain || format == ComputerNamePhysicalDnsDomain)
	{
		/* domain names may be empty, terminating null only */
		dwMinSize = 1;
	}
	else
	{
		/* computer names must be at least 1 character */
		dwMinSize = 2;
	}

	/* test with null buffer and zero size (required if buffer is null) */
	dwSize = 0;
	if (GetComputerNameExA(format, NULL, &dwSize) == TRUE)
	{
		fprintf(stderr, "%s: (1/%d) GetComputerNameExA unexpectedly succeeded with null buffer\n",
		        __FUNCTION__, format);
		return FALSE;
	}
	if ((dwError = GetLastError()) != ERROR_MORE_DATA)
	{
		fprintf(stderr,
		        "%s: (2/%d) GetLastError returned 0x%08" PRIX32 " (expected ERROR_MORE_DATA)\n",
		        __FUNCTION__, format, dwError);
		return FALSE;
	}

	/* test with valid buffer and zero size */
	dwSize = 0;
	if (GetComputerNameExA(format, computerName1, &dwSize) == TRUE)
	{
		fprintf(stderr,
		        "%s: (3/%d) GetComputerNameExA unexpectedly succeeded with zero size parameter\n",
		        __FUNCTION__, format);
		return FALSE;
	}
	if ((dwError = GetLastError()) != ERROR_MORE_DATA)
	{
		fprintf(stderr,
		        "%s: (4/%d) GetLastError returned 0x%08" PRIX32 " (expected ERROR_MORE_DATA)\n",
		        __FUNCTION__, format, dwError);
		return FALSE;
	}
	/* check if returned size is valid: must be the size of the buffer required, including the
	 * terminating null character in this case */
	if (dwSize < dwMinSize || dwSize > nameBufferSize)
	{
		fprintf(stderr,
		        "%s: (5/%d) GetComputerNameExA returned wrong size %" PRIu32
		        " (expected something in the range from %" PRIu32 " to %" PRIu32 ")\n",
		        __FUNCTION__, format, dwSize, dwMinSize, nameBufferSize);
		return FALSE;
	}
	dwNameLength = dwSize - 1;

	/* test with returned size */
	if (GetComputerNameExA(format, computerName1, &dwSize) == FALSE)
	{
		fprintf(stderr, "%s: (6/%d) GetComputerNameExA failed with error: 0x%08" PRIX32 "\n",
		        __FUNCTION__, format, GetLastError());
		return FALSE;
	}
	/* check if returned size is valid */
	if (dwSize != dwNameLength)
	{
		fprintf(stderr,
		        "%s: (7/%d) GetComputerNameExA returned wrong size %" PRIu32 " (expected %" PRIu32
		        ")\n",
		        __FUNCTION__, format, dwSize, dwNameLength);
		return FALSE;
	}
	/* check if string is correctly terminated */
	if (computerName1[dwSize] != 0)
	{
		fprintf(stderr, "%s: (8/%d) string termination error\n", __FUNCTION__, format);
		return FALSE;
	}

	/* test with real buffer size */
	dwSize = nameBufferSize;
	if (GetComputerNameExA(format, computerName2, &dwSize) == FALSE)
	{
		fprintf(stderr, "%s: (9/%d) GetComputerNameExA failed with error: 0x%08" PRIX32 "\n",
		        __FUNCTION__, format, GetLastError());
		return FALSE;
	}
	/* check if returned size is valid */
	if (dwSize != dwNameLength)
	{
		fprintf(stderr,
		        "%s: (10/%d) GetComputerNameExA returned wrong size %" PRIu32 " (expected %" PRIu32
		        ")\n",
		        __FUNCTION__, format, dwSize, dwNameLength);
		return FALSE;
	}
	/* check if string is correctly terminated */
	if (computerName2[dwSize] != 0)
	{
		fprintf(stderr, "%s: (11/%d) string termination error\n", __FUNCTION__, format);
		return FALSE;
	}

	/* compare the results */
	if (strcmp(computerName1, computerName2))
	{
		fprintf(stderr, "%s: (12/%d) string compare mismatch\n", __FUNCTION__, format);
		return FALSE;
	}

	/* test with off by one buffer size */
	dwSize = dwNameLength;
	if (GetComputerNameExA(format, computerName1, &dwSize) == TRUE)
	{
		fprintf(stderr,
		        "%s: (13/%d) GetComputerNameExA unexpectedly succeeded with limited buffer size\n",
		        __FUNCTION__, format);
		return FALSE;
	}
	/* check if returned size is valid */
	if (dwSize != dwNameLength + 1)
	{
		fprintf(stderr,
		        "%s: (14/%d) GetComputerNameExA returned wrong size %" PRIu32 " (expected %" PRIu32
		        ")\n",
		        __FUNCTION__, format, dwSize, dwNameLength + 1);
		return FALSE;
	}

	return TRUE;
}

int TestGetComputerName(int argc, char* argv[])
{
	if (!Test_GetComputerName())
		return -1;

	if (!Test_GetComputerNameEx_Format(ComputerNameNetBIOS))
		return -1;

	if (!Test_GetComputerNameEx_Format(ComputerNameDnsHostname))
		return -1;

	if (!Test_GetComputerNameEx_Format(ComputerNameDnsDomain))
		return -1;

	if (!Test_GetComputerNameEx_Format(ComputerNameDnsFullyQualified))
		return -1;

	if (!Test_GetComputerNameEx_Format(ComputerNamePhysicalNetBIOS))
		return -1;

	if (!Test_GetComputerNameEx_Format(ComputerNamePhysicalDnsHostname))
		return -1;

	if (!Test_GetComputerNameEx_Format(ComputerNamePhysicalDnsDomain))
		return -1;

	if (!Test_GetComputerNameEx_Format(ComputerNamePhysicalDnsFullyQualified))
		return -1;

	return 0;
}
