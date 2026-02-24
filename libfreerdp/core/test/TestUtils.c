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
	char* wvendor = nullptr;
	size_t wlen = 0;
	if (version < 0)
		(void)winpr_asprintf(&wvendor, &wlen, "%s%c%s", vendor, separator, product);
	else
		(void)winpr_asprintf(&wvendor, &wlen, "%s%c%s%" PRIdz, vendor, separator, product, version);
	if (!wvendor)
	{
		(void)fprintf(stderr,
		              "%s(vendor=%s, product=%s, version=%" PRIdz ", separator=%c) nullptr\n",
		              __func__, vendor, product, version, separator);
	}
	return wvendor;
}

static bool checkCombined(const char* what, const char* vendor, const char* product,
                          SSIZE_T version, char separator)
{
	if (!what || !vendor || !product)
	{
		(void)fprintf(stderr,
		              "%s(what=%s, vendor=%s, product=%s, version=%" PRIdz ", separator=%c)\n",
		              what, vendor, product, version, separator);
		return false;
	}

	char* cmp = create(vendor, product, version, separator);
	if (!cmp)
		return false;

	const bool rc = strcmp(what, cmp) == 0;
	if (!rc)
	{
		(void)fprintf(stderr,
		              "%s(what=%s, vendor=%s, product=%s, version=%" PRIdz
		              ", separator=%c) -> got %s\n",
		              what, vendor, product, version, separator, cmp);
	}
	free(cmp);
	return rc;
}

#if !defined(FREERDP_USE_VENDOR_PRODUCT_CONFIG_DIR)
#if !defined(WITH_FULL_CONFIG_PATH)
WINPR_ATTR_MALLOC(free, 1)
WINPR_ATTR_NODISCARD
static char* freerdp_settings_get_legacy_config_path(const char* filename, const char* cproduct)
{
	char product[MAX_PATH] = WINPR_C_ARRAY_INIT;

	const size_t plen = strnlen(cproduct, sizeof(product));
	if (plen == MAX_PATH)
		return nullptr;

	for (size_t i = 0; i < plen; i++)
		product[i] = (char)tolower(cproduct[i]);

	char* path = GetKnownSubPath(KNOWN_PATH_XDG_CONFIG_HOME, product);

	if (!path)
		return nullptr;

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
		return nullptr;

#if !defined(WITH_FULL_CONFIG_PATH)
	if (!system && (_stricmp(vendor, product) == 0))
		return freerdp_settings_get_legacy_config_path(filename, product);
#endif

	char* config = GetKnownPath(id);
	if (!config)
		return nullptr;

	char* base = nullptr;
	if (version < 0)
		base = GetCombinedPathV(config, "%s", product);
	else
		base = GetCombinedPathV(config, "%s%" PRIdz, product, version);
	free(config);

	if (!base)
		return nullptr;

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
		return nullptr;

	char* base = nullptr;
	if (version < 0)
		base = GetCombinedPathV(config, "%s", product);
	else
		base = GetCombinedPathV(config, "%s%" PRIdz, product, version);
	free(config);

	if (!base)
		return nullptr;
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
	{
		(void)fprintf(stderr,
		              "%s(custom=%d, what=%s, system=%d, vendor=%s, product=%s, version=%" PRIdz
		              ", file=%s) failed\n",
		              __func__, custom, what, system, vendor, product, version, filename);
		return false;
	}

	const bool rc = strcmp(what, cmp) == 0;
	if (!rc)
	{
		(void)fprintf(stderr,
		              "%s(custom=%d, what=%s, system=%d, vendor=%s, product=%s, version=%" PRIdz
		              ", file=%s) failed compare: got %s\n",
		              __func__, custom, what, system, vendor, product, version, filename, cmp);
	}
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
		char* sys = freerdp_GetConfigFilePath(TRUE, nullptr);
		const bool rc = checkFreeRDPConfig(custom, sys, TRUE, vendor, product, version, nullptr);
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
		char* sys = freerdp_GetConfigFilePath(FALSE, nullptr);
		const bool rc = checkFreeRDPConfig(custom, sys, FALSE, vendor, product, version, nullptr);
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
		char* cmp = nullptr;
		size_t clen = 0;
#define FMT "foo\\bar\\%s\\gaga"
		(void)winpr_asprintf(&cmp, &clen, FMT, pcmp);
		free(pcmp);
		if (!cmp)
		{
			(void)fprintf(stderr, "winpr_asprintf cmp nullptr\n");
			return false;
		}

		char* comb = freerdp_getApplicatonDetailsRegKey(FMT);
#undef FMT

		bool rc = false;
		if (comb)
		{
			rc = strcmp(cmp, comb) == 0;
			if (!rc)
				(void)fprintf(stderr, "strcmp(%s, %s) compare reg failed\n", cmp, comb);
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
		return nullptr;

	char* base = nullptr;
	if (version < 0)
		base = GetCombinedPathV(config, "%s", product);
	else
		base = GetCombinedPathV(config, "%s%" PRIdz, product, version);
	free(config);

	if (!base)
		return nullptr;
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
		char* sys = winpr_GetConfigFilePath(TRUE, nullptr);
		const bool rc = checkWinPRConfig(sys, TRUE, vendor, product, version, nullptr);
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
		char* sys = winpr_GetConfigFilePath(FALSE, nullptr);
		const bool rc = checkWinPRConfig(sys, FALSE, vendor, product, version, nullptr);
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
	int rc = -23;

	if (!checkWinPRResults(false, WINPR_VENDOR_STRING, WINPR_PRODUCT_STRING, WINPR_VERSION_MAJOR))
	{
		rc = -1;
		goto fail;
	}
	if (!checkFreeRDPResults(false, FREERDP_VENDOR_STRING, FREERDP_PRODUCT_STRING,
	                         FREERDP_VERSION_MAJOR))
	{
		rc = -2;
		goto fail;
	}

	for (size_t x = 0; x < ARRAYSIZE(tests); x++)
	{
		const test_case_t* cur = &tests[x];

		if (!freerdp_setApplicationDetails(cur->vendor, cur->product, cur->version))
		{
			(void)fprintf(stderr, "freerdp_setApplicationDetails(%s, %s, %" PRIdz ") failed\n",
			              cur->vendor, cur->product, cur->version);
			{
				rc = -3;
				goto fail;
			}
		}

		const char separator = PathGetSeparatorA(PATH_STYLE_NATIVE);
#if defined(BUILD_TESTING_INTERNAL)
		char* wvendor = freerdp_getApplicatonDetailsCombined(separator);
#else
		char* wvendor = create(cur->vendor, cur->product, cur->version, separator);
#endif
		if (!wvendor)
		{
			rc = -4;
			goto fail;
		}
		const BOOL wrc = checkWinPRResults(true, wvendor, "WinPR", -1);
		free(wvendor);
		if (!wrc)
		{
			rc = -5;
			goto fail;
		}
		if (!checkFreeRDPResults(true, cur->vendor, cur->product, cur->version))
		{
			rc = -6;
			goto fail;
		}
	}

	rc = 0;
fail:
	printf("%s: result %d\n", __func__, rc);
	return rc;
}
