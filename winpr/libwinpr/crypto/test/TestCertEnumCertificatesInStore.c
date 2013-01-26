

#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/crypto.h>

#ifdef _WIN32
//#define WITH_CRYPTUI	1
#endif

#ifdef WITH_CRYPTUI
#include <cryptuiapi.h>
#endif

int TestCertEnumCertificatesInStore(int argc, char* argv[])
{
	int index;
	DWORD status;
	LPTSTR pszNameString; 
	HCERTSTORE hCertStore = NULL;
	PCCERT_CONTEXT pCertContext = NULL;

	/**
	 * System Store Locations:
	 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa388136/
	 */

	/**
	 * Requires elevated rights:
	 * hCertStore = CertOpenStore(CERT_STORE_PROV_SYSTEM, 0, (HCRYPTPROV_LEGACY) NULL, CERT_SYSTEM_STORE_LOCAL_MACHINE, _T("Remote Desktop"));
	 */

	hCertStore = CertOpenSystemStore((HCRYPTPROV_LEGACY) NULL, _T("MY"));
	// hCertStore = CertOpenStore(CERT_STORE_PROV_SYSTEM, 0, (HCRYPTPROV_LEGACY) NULL, CERT_SYSTEM_STORE_CURRENT_USER, _T("MY"));

	if (!hCertStore)
	{
		printf("Failed to open system store\n");
		return -1;
	}

	index = 0;

	while ((pCertContext = CertEnumCertificatesInStore(hCertStore, pCertContext)))
	{
		status = CertGetNameString(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, NULL, 0);

		pszNameString = (LPTSTR) malloc(status * sizeof(TCHAR));
		status = CertGetNameString(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, pszNameString, status);

		_tprintf(_T("Certificate #%d: %s\n"), index++, pszNameString);

#ifdef WITH_CRYPTUI
		CryptUIDlgViewContext(CERT_STORE_CERTIFICATE_CONTEXT, pCertContext, NULL, NULL, 0, NULL);
#endif
	}

	if (!CertCloseStore(hCertStore, 0))
	{
		printf("Failed to close system store\n");
		return -1;
	}

	return 0;
}

