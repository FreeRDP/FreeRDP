#include <ctype.h>

#include <winpr/version.h>
#include <winpr/build-config.h>
#include <winpr/path.h>
#include <winpr/file.h>

#include <freerdp/api.h>
#include <freerdp/version.h>
#include <freerdp/build-config.h>
#include <freerdp/utils/helpers.h>

#if defined(BUILD_TESTING_INTERNAL)
#include "../utils.h"
#endif

typedef struct
{
	const char* vendor;
	const char* product;
	SSIZE_T version;
} test_case_t;

static const test_case_t tests[] = { { "foobar", "gaga", 23 },
	                                 { "foobar1", "gaga1", -1 },
	                                 { "foobar2", "gaga2", 23 },
	                                 { "foobar3", "gaga3", -1 } };

WINPR_ATTR_MALLOC(free, 1)
WINPR_ATTR_NODISCARD
static char* create(const char* vendor, const char* product, SSIZE_T version, char separator)
{
	char* wvendor = NULL;
	size_t wlen = 0;
	if (version < 0)
		(void)winpr_asprintf(&wvendor, &wlen, "%s%c%s", vendor, separator, product);
	else
		(void)winpr_asprintf(&wvendor, &wlen, "%s%c%s%" PRIdz, vendor, separator, product, version);
	return wvendor;
}

static bool checkCombined(const char* what, const char* vendor, const char* product,
                          SSIZE_T version, char separator)
{
	if (!what || !vendor || !product)
		return false;

	char* cmp = create(vendor, product, version, separator);
	if (!cmp)
		return false;

	const bool rc = strcmp(what, cmp) == 0;
	free(cmp);
	return rc;
}

#if !defined(FREERDP_USE_VENDOR_PRODUCT_CONFIG_DIR)
#if !defined(WITH_FULL_CONFIG_PATH)
WINPR_ATTR_MALLOC(free, 1)
WINPR_ATTR_NODISCARD
static char* freerdp_settings_get_legacy_config_path(const char* filename, const char* cproduct)
{
	char product[MAX_PATH] = { 0 };

	const size_t plen = strnlen(cproduct, sizeof(product));
	if (plen == MAX_PATH)
		return NULL;

	for (size_t i = 0; i < plen; i++)
		product[i] = (char)tolower(cproduct[i]);

	char* path = GetKnownSubPath(KNOWN_PATH_XDG_CONFIG_HOME, product);

	if (!path)
		return NULL;

	if (!filename)
		return path;

	char* filepath = GetCombinedPath(path, filename);
	free(path);
	return filepath;
}
#endif

WINPR_ATTR_MALLOC(free, 1)
WINPR_ATTR_NODISCARD
char* getFreeRDPDefaultConfig(BOOL system, const char* product, const char* vendor, SSIZE_T version,
                              const char* filename)
{
	eKnownPathTypes id = system ? KNOWN_PATH_SYSTEM_CONFIG_HOME : KNOWN_PATH_XDG_CONFIG_HOME;

	if (!vendor || !product)
		return NULL;

#if !defined(WITH_FULL_CONFIG_PATH)
	if (!system && (_stricmp(vendor, product) == 0))
		return freerdp_settings_get_legacy_config_path(filename, product);
#endif

	char* config = GetKnownPath(id);
	if (!config)
		return NULL;

	char* base = NULL;
	if (version < 0)
		base = GetCombinedPathV(config, "%s", product);
	else
		base = GetCombinedPathV(config, "%s%" PRIdz, product, version);
	free(config);

	if (!base)
		return NULL;

	if (!filename)
		return base;

	char* path = GetCombinedPathV(base, "%s", filename);
	free(base);
	return path;
}
#endif

WINPR_ATTR_MALLOC(free, 1)
WINPR_ATTR_NODISCARD
static char* getFreeRDPConfig(bool custom, BOOL system, const char* vendor, const char* product,
                              SSIZE_T version, const char* filename)
{
#if !defined(FREERDP_USE_VENDOR_PRODUCT_CONFIG_DIR)
	if (!custom)
		return getFreeRDPDefaultConfig(system, vendor, product, version, filename);
#endif
	eKnownPathTypes id = system ? KNOWN_PATH_SYSTEM_CONFIG_HOME : KNOWN_PATH_XDG_CONFIG_HOME;
	char* config = GetKnownSubPathV(id, "%s", vendor);
	if (!config)
		return NULL;

	char* base = NULL;
	if (version < 0)
		base = GetCombinedPathV(config, "%s", product);
	else
		base = GetCombinedPathV(config, "%s%" PRIdz, product, version);
	free(config);

	if (!base)
		return NULL;
	if (!filename)
		return base;

	char* path = GetCombinedPath(base, filename);
	free(base);
	return path;
}

WINPR_ATTR_NODISCARD
static bool checkFreeRDPConfig(bool custom, const char* what, BOOL system, const char* vendor,
                               const char* product, SSIZE_T version, const char* filename)
{
	if (!what)
		return false;
	char* cmp = getFreeRDPConfig(custom, system, vendor, product, version, filename);
	if (!cmp)
		return false;

	const bool rc = strcmp(what, cmp) == 0;
	free(cmp);
	return rc;
}

static bool checkFreeRDPResults(bool custom, const char* vendor, const char* product,
                                SSIZE_T version)
{
	const char* cvendor = freerdp_getApplicationDetailsVendor();
	const char* cproduct = freerdp_getApplicationDetailsProduct();
	const SSIZE_T cversion = freerdp_getApplicationDetailsVersion();

	if (!custom)
	{
#if !defined(WITH_RESOURCE_VERSIONING)
		version = -1;
#endif
	}

	if (strcmp(cvendor, vendor) != 0)
	{
		(void)fprintf(stderr, "freerdp_getApplicationDetailsVendor returned '%s', expected '%s'\n",
		              cvendor, vendor);
		return false;
	}
	if (strcmp(cvendor, vendor) != 0)
	{
		(void)fprintf(stderr, "freerdp_getApplicationDetailsProduct returned '%s', expected '%s'\n",
		              cproduct, product);
		return false;
	}
	if (cversion != version)
	{
		(void)fprintf(stderr,
		              "freerdp_getApplicationDetailsVersion returned %" PRIdz ", expected %" PRIdz
		              "\n",
		              cversion, version);
		return false;
	}

	{
		char* sys = freerdp_GetConfigFilePath(TRUE, NULL);
		const bool rc = checkFreeRDPConfig(custom, sys, TRUE, vendor, product, version, NULL);
		free(sys);
		if (!rc)
			return rc;
	}
	{
		const char name[] = "systest";
		char* sys = freerdp_GetConfigFilePath(TRUE, name);
		const bool rc = checkFreeRDPConfig(custom, sys, TRUE, vendor, product, version, name);
		free(sys);
		if (!rc)
			return rc;
	}
	{
		char* sys = freerdp_GetConfigFilePath(FALSE, NULL);
		const bool rc = checkFreeRDPConfig(custom, sys, FALSE, vendor, product, version, NULL);
		free(sys);
		if (!rc)
			return rc;
	}
	{
		const char name[] = "usertest";
		char* sys = freerdp_GetConfigFilePath(FALSE, name);
		const bool rc = checkFreeRDPConfig(custom, sys, FALSE, vendor, product, version, name);
		free(sys);
		if (!rc)
			return rc;
	}

#if defined(BUILD_TESTING_INTERNAL)
	{
		char* pcmp = create(vendor, product, version, '\\');
		if (!pcmp)
			return false;
		char* cmp = NULL;
		size_t clen = 0;
#define FMT "foo\\bar\\%s\\gaga"
		(void)winpr_asprintf(&cmp, &clen, FMT, pcmp);
		free(pcmp);
		if (!cmp)
			return false;

		char* comb = freerdp_getApplicatonDetailsRegKey(FMT);
#undef FMT

		bool rc = false;
		if (comb)
		{
			rc = strcmp(cmp, comb) == 0;
		}
		free(comb);
		free(cmp);

		if (!rc)
			return false;
	}
	{
		char* comb = freerdp_getApplicatonDetailsCombined('/');
		const bool rc = checkCombined(comb, vendor, product, version, '/');
		free(comb);
		if (!rc)
			return false;
	}
	{
		char* comb = freerdp_getApplicatonDetailsCombined('\\');
		const bool rc = checkCombined(comb, vendor, product, version, '\\');
		free(comb);
		if (!rc)
			return false;
	}
	const BOOL ccustom = freerdp_areApplicationDetailsCustomized();
	if (ccustom != custom)
	{
		(void)fprintf(stderr, "freerdp_areApplicationDetailsCustomized returned %d, expected%d\n",
		              ccustom, custom);
		return false;
	}
#endif

	return true;
}

WINPR_ATTR_MALLOC(free, 1)
WINPR_ATTR_NODISCARD
static char* getWinPRConfig(BOOL system, const char* vendor, const char* product, SSIZE_T version,
                            const char* filename)
{
	eKnownPathTypes id = system ? KNOWN_PATH_SYSTEM_CONFIG_HOME : KNOWN_PATH_XDG_CONFIG_HOME;
	char* config = GetKnownSubPathV(id, "%s", vendor);
	if (!config)
		return NULL;

	char* base = NULL;
	if (version < 0)
		base = GetCombinedPathV(config, "%s", product);
	else
		base = GetCombinedPathV(config, "%s%" PRIdz, product, version);
	free(config);

	if (!base)
		return NULL;
	if (!filename)
		return base;

	char* path = GetCombinedPath(base, filename);
	free(base);
	return path;
}

WINPR_ATTR_NODISCARD
static bool checkWinPRConfig(const char* what, BOOL system, const char* vendor, const char* product,
                             SSIZE_T version, const char* filename)
{
	if (!what)
		return false;
	char* cmp = getWinPRConfig(system, vendor, product, version, filename);
	if (!cmp)
		return false;

	const bool rc = strcmp(what, cmp) == 0;
	free(cmp);
	return rc;
}

WINPR_ATTR_NODISCARD
static bool checkWinPRResults(bool custom, const char* vendor, const char* product, SSIZE_T version)
{
	const char* cvendor = winpr_getApplicationDetailsVendor();
	const char* cproduct = winpr_getApplicationDetailsProduct();
	const SSIZE_T cversion = winpr_getApplicationDetailsVersion();

	if (!custom)
	{
#if !defined(WITH_RESOURCE_VERSIONING)
		version = -1;
#endif
	}

	if (strcmp(cvendor, vendor) != 0)
	{
		(void)fprintf(stderr, "winpr_getApplicationDetailsVendor returned '%s', expected '%s'\n",
		              cvendor, vendor);
		return false;
	}
	if (strcmp(cvendor, vendor) != 0)
	{
		(void)fprintf(stderr, "winpr_getApplicationDetailsProduct returned '%s', expected '%s'\n",
		              cproduct, product);
		return false;
	}
	if (cversion != version)
	{
		(void)fprintf(
		    stderr, "winpr_getApplicationDetailsVersion returned %" PRIdz ", expected %" PRIdz "\n",
		    cversion, version);
		return false;
	}

	{
		char* sys = winpr_GetConfigFilePath(TRUE, NULL);
		const bool rc = checkWinPRConfig(sys, TRUE, vendor, product, version, NULL);
		free(sys);
		if (!rc)
			return rc;
	}
	{
		char* sys = winpr_GetConfigFilePath(TRUE, "systest");
		const bool rc = checkWinPRConfig(sys, TRUE, vendor, product, version, "systest");
		free(sys);
		if (!rc)
			return rc;
	}
	{
		char* sys = winpr_GetConfigFilePath(FALSE, NULL);
		const bool rc = checkWinPRConfig(sys, FALSE, vendor, product, version, NULL);
		free(sys);
		if (!rc)
			return rc;
	}
	{
		char* sys = winpr_GetConfigFilePath(FALSE, "usertest");
		const bool rc = checkWinPRConfig(sys, FALSE, vendor, product, version, "usertest");
		free(sys);
		if (!rc)
			return rc;
	}

	return true;
}

int TestUtils(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char* argv[])
{
	if (!checkWinPRResults(false, WINPR_VENDOR_STRING, WINPR_PRODUCT_STRING, WINPR_VERSION_MAJOR))
		return -1;
	if (!checkFreeRDPResults(false, FREERDP_VENDOR_STRING, FREERDP_PRODUCT_STRING,
	                         FREERDP_VERSION_MAJOR))
		return -2;

	for (size_t x = 0; x < ARRAYSIZE(tests); x++)
	{
		const test_case_t* cur = &tests[x];

		if (!freerdp_setApplicationDetails(cur->vendor, cur->product, cur->version))
		{
			(void)fprintf(stderr, "freerdp_setApplicationDetails(%s, %s, %" PRIdz ") failed\n",
			              cur->vendor, cur->product, cur->version);
			return -3;
		}

		const char separator = PathGetSeparatorA(PATH_STYLE_NATIVE);
#if defined(BUILD_TESTING_INTERNAL)
		char* wvendor = freerdp_getApplicatonDetailsCombined(separator);
#else
		char* wvendor = create(cur->vendor, cur->product, cur->version, separator);
#endif
		if (!wvendor)
			return -4;
		const BOOL wrc = checkWinPRResults(true, wvendor, "WinPR", -1);
		free(wvendor);
		if (!wrc)
			return -5;
		if (!checkFreeRDPResults(true, cur->vendor, cur->product, cur->version))
			return -6;
	}

	printf("%s: success\n", __func__);
	return 0;
}
