/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 Armin Novak <armin.novak@thincast.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <winpr/path.h>
#include <winpr/file.h>
#include <winpr/sysinfo.h>

#include <freerdp/crypto/certificate.h>

/* Some certificates copied from /usr/share/ca-certificates */
static const char pem1[] = "-----BEGIN CERTIFICATE-----\n"
                           "MIIFWjCCA0KgAwIBAgIQbkepxUtHDA3sM9CJuRz04TANBgkqhkiG9w0BAQwFADBH\n"
                           "MQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExM\n"
                           "QzEUMBIGA1UEAxMLR1RTIFJvb3QgUjEwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIy\n"
                           "MDAwMDAwWjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNl\n"
                           "cnZpY2VzIExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjEwggIiMA0GCSqGSIb3DQEB\n"
                           "AQUAA4ICDwAwggIKAoICAQC2EQKLHuOhd5s73L+UPreVp0A8of2C+X0yBoJx9vaM\n"
                           "f/vo27xqLpeXo4xL+Sv2sfnOhB2x+cWX3u+58qPpvBKJXqeqUqv4IyfLpLGcY9vX\n"
                           "mX7wCl7raKb0xlpHDU0QM+NOsROjyBhsS+z8CZDfnWQpJSMHobTSPS5g4M/SCYe7\n"
                           "zUjwTcLCeoiKu7rPWRnWr4+wB7CeMfGCwcDfLqZtbBkOtdh+JhpFAz2weaSUKK0P\n"
                           "fyblqAj+lug8aJRT7oM6iCsVlgmy4HqMLnXWnOunVmSPlk9orj2XwoSPwLxAwAtc\n"
                           "vfaHszVsrBhQf4TgTM2S0yDpM7xSma8ytSmzJSq0SPly4cpk9+aCEI3oncKKiPo4\n"
                           "Zor8Y/kB+Xj9e1x3+naH+uzfsQ55lVe0vSbv1gHR6xYKu44LtcXFilWr06zqkUsp\n"
                           "zBmkMiVOKvFlRNACzqrOSbTqn3yDsEB750Orp2yjj32JgfpMpf/VjsPOS+C12LOO\n"
                           "Rc92wO1AK/1TD7Cn1TsNsYqiA94xrcx36m97PtbfkSIS5r762DL8EGMUUXLeXdYW\n"
                           "k70paDPvOmbsB4om3xPXV2V4J95eSRQAogB/mqghtqmxlbCluQ0WEdrHbEg8QOB+\n"
                           "DVrNVjzRlwW5y0vtOUucxD/SVRNuJLDWcfr0wbrM7Rv1/oFB2ACYPTrIrnqYNxgF\n"
                           "lQIDAQABo0IwQDAOBgNVHQ8BAf8EBAMCAQYwDwYDVR0TAQH/BAUwAwEB/zAdBgNV\n"
                           "HQ4EFgQU5K8rJnEaK0gnhS9SZizv8IkTcT4wDQYJKoZIhvcNAQEMBQADggIBADiW\n"
                           "Cu49tJYeX++dnAsznyvgyv3SjgofQXSlfKqE1OXyHuY3UjKcC9FhHb8owbZEKTV1\n"
                           "d5iyfNm9dKyKaOOpMQkpAWBz40d8U6iQSifvS9efk+eCNs6aaAyC58/UEBZvXw6Z\n"
                           "XPYfcX3v73svfuo21pdwCxXu11xWajOl40k4DLh9+42FpLFZXvRq4d2h9mREruZR\n"
                           "gyFmxhE+885H7pwoHyXa/6xmld01D1zvICxi/ZG6qcz8WpyTgYMpl0p8WnK0OdC3\n"
                           "d8t5/Wk6kjftbjhlRn7pYL15iJdfOBL07q9bgsiG1eGZbYwE8na6SfZu6W0eX6Dv\n"
                           "J4J2QPim01hcDyxC2kLGe4g0x8HYRZvBPsVhHdljUEn2NIVq4BjFbkerQUIpm/Zg\n"
                           "DdIx02OYI5NaAIFItO/Nis3Jz5nu2Z6qNuFoS3FJFDYoOj0dzpqPJeaAcWErtXvM\n"
                           "+SUWgeExX6GjfhaknBZqlxi9dnKlC54dNuYvoS++cJEPqOba+MSSQGwlfnuzCdyy\n"
                           "F62ARPBopY+Udf90WuioAnwMCeKpSwughQtiue+hMZL77/ZRBIls6Kl0obsXs7X9\n"
                           "SQ98POyDGCBDTtWTurQ0sR8WNh8M5mQ5Fkzc4P4dyKliPUDqysU0ArSuiYgzNdws\n"
                           "E3PYJ/HQcu51OyLemGhmW/HGY0dVHLqlCFF1pkgl\n"
                           "-----END CERTIFICATE-----";

static const char pem2[] = "-----BEGIN CERTIFICATE-----\n"
                           "MIIFWjCCA0KgAwIBAgIQbkepxlqz5yDFMJo/aFLybzANBgkqhkiG9w0BAQwFADBH\n"
                           "MQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExM\n"
                           "QzEUMBIGA1UEAxMLR1RTIFJvb3QgUjIwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIy\n"
                           "MDAwMDAwWjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNl\n"
                           "cnZpY2VzIExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjIwggIiMA0GCSqGSIb3DQEB\n"
                           "AQUAA4ICDwAwggIKAoICAQDO3v2m++zsFDQ8BwZabFn3GTXd98GdVarTzTukk3Lv\n"
                           "CvptnfbwhYBboUhSnznFt+4orO/LdmgUud+tAWyZH8QiHZ/+cnfgLFuv5AS/T3Kg\n"
                           "GjSY6Dlo7JUle3ah5mm5hRm9iYz+re026nO8/4Piy33B0s5Ks40FnotJk9/BW9Bu\n"
                           "XvAuMC6C/Pq8tBcKSOWIm8Wba96wyrQD8Nr0kLhlZPdcTK3ofmZemde4wj7I0BOd\n"
                           "re7kRXuJVfeKH2JShBKzwkCX44ofR5GmdFrS+LFjKBC4swm4VndAoiaYecb+3yXu\n"
                           "PuWgf9RhD1FLPD+M2uFwdNjCaKH5wQzpoeJ/u1U8dgbuak7MkogwTZq9TwtImoS1\n"
                           "mKPV+3PBV2HdKFZ1E66HjucMUQkQdYhMvI35ezzUIkgfKtzra7tEscszcTJGr61K\n"
                           "8YzodDqs5xoic4DSMPclQsciOzsSrZYuxsN2B6ogtzVJV+mSSeh2FnIxZyuWfoqj\n"
                           "x5RWIr9qS34BIbIjMt/kmkRtWVtd9QCgHJvGeJeNkP+byKq0rxFROV7Z+2et1VsR\n"
                           "nTKaG73VululycslaVNVJ1zgyjbLiGH7HrfQy+4W+9OmTN6SpdTi3/UGVN4unUu0\n"
                           "kzCqgc7dGtxRcw1PcOnlthYhGXmy5okLdWTK1au8CcEYof/UVKGFPP0UJAOyh9Ok\n"
                           "twIDAQABo0IwQDAOBgNVHQ8BAf8EBAMCAQYwDwYDVR0TAQH/BAUwAwEB/zAdBgNV\n"
                           "HQ4EFgQUu//KjiOfT5nK2+JopqUVJxce2Q4wDQYJKoZIhvcNAQEMBQADggIBALZp\n"
                           "8KZ3/p7uC4Gt4cCpx/k1HUCCq+YEtN/L9x0Pg/B+E02NjO7jMyLDOfxA325BS0JT\n"
                           "vhaI8dI4XsRomRyYUpOM52jtG2pzegVATX9lO9ZY8c6DR2Dj/5epnGB3GFW1fgiT\n"
                           "z9D2PGcDFWEJ+YF59exTpJ/JjwGLc8R3dtyDovUMSRqodt6Sm2T4syzFJ9MHwAiA\n"
                           "pJiS4wGWAqoC7o87xdFtCjMwc3i5T1QWvwsHoaRc5svJXISPD+AVdyx+Jn7axEvb\n"
                           "pxZ3B7DNdehyQtaVhJ2Gg/LkkM0JR9SLA3DaWsYDQvTtN6LwG1BUSw7YhN4ZKJmB\n"
                           "R64JGz9I0cNv4rBgF/XuIwKl2gBbbZCr7qLpGzvpx0QnRY5rn/WkhLx3+WuXrD5R\n"
                           "RaIRpsyF7gpo8j5QOHokYh4XIDdtak23CZvJ/KRY9bb7nE4Yu5UC56GtmwfuNmsk\n"
                           "0jmGwZODUNKBRqhfYlcsu2xkiAhu7xNUX90txGdj08+JN7+dIPT7eoOboB6BAFDC\n"
                           "5AwiWVIQ7UNWhwD4FFKnHYuTjKJNRn8nxnGbJN7k2oaLDX5rIMHAnuFl2GqjpuiF\n"
                           "izoHCBy69Y9Vmhh1fuXsgWbRIXOhNUQLgD1bnF5vKheW0YMjiGZt5obicDIvUiLn\n"
                           "yOd/xCxgXS/Dr55FBcOEArf9LAhST4Ldo/DUhgkC\n"
                           "-----END CERTIFICATE-----";

static const char pem3[] = "-----BEGIN CERTIFICATE-----\n"
                           "MIICDDCCAZGgAwIBAgIQbkepx2ypcyRAiQ8DVd2NHTAKBggqhkjOPQQDAzBHMQsw\n"
                           "CQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEU\n"
                           "MBIGA1UEAxMLR1RTIFJvb3QgUjMwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAw\n"
                           "MDAwWjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZp\n"
                           "Y2VzIExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjMwdjAQBgcqhkjOPQIBBgUrgQQA\n"
                           "IgNiAAQfTzOHMymKoYTey8chWEGJ6ladK0uFxh1MJ7x/JlFyb+Kf1qPKzEUURout\n"
                           "736GjOyxfi//qXGdGIRFBEFVbivqJn+7kAHjSxm65FSWRQmx1WyRRK2EE46ajA2A\n"
                           "DDL24CejQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNVHRMBAf8EBTADAQH/MB0GA1Ud\n"
                           "DgQWBBTB8Sa6oC2uhYHP0/EqEr24Cmf9vDAKBggqhkjOPQQDAwNpADBmAjEAgFuk\n"
                           "fCPAlaUs3L6JbyO5o91lAFJekazInXJ0glMLfalAvWhgxeG4VDvBNhcl2MG9AjEA\n"
                           "njWSdIUlUfUk7GRSJFClH9voy8l27OyCbvWFGFPouOOaKaqW04MjyaR7YbPMAuhd\n"
                           "-----END CERTIFICATE-----";

static const char pem4[] = "-----BEGIN CERTIFICATE-----\n"
                           "MIICCjCCAZGgAwIBAgIQbkepyIuUtui7OyrYorLBmTAKBggqhkjOPQQDAzBHMQsw\n"
                           "CQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEU\n"
                           "MBIGA1UEAxMLR1RTIFJvb3QgUjQwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAw\n"
                           "MDAwWjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZp\n"
                           "Y2VzIExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjQwdjAQBgcqhkjOPQIBBgUrgQQA\n"
                           "IgNiAATzdHOnaItgrkO4NcWBMHtLSZ37wWHO5t5GvWvVYRg1rkDdc/eJkTBa6zzu\n"
                           "hXyiQHY7qca4R9gq55KRanPpsXI5nymfopjTX15YhmUPoYRlBtHci8nHc8iMai/l\n"
                           "xKvRHYqjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNVHRMBAf8EBTADAQH/MB0GA1Ud\n"
                           "DgQWBBSATNbrdP9JNqPV2Py1PsVq8JQdjDAKBggqhkjOPQQDAwNnADBkAjBqUFJ0\n"
                           "CMRw3J5QdCHojXohw0+WbhXRIjVhLfoIN+4Zba3bssx9BzT1YBkstTTZbyACMANx\n"
                           "sbqjYAuG7ZoIapVon+Kz4ZNkfF6Tpt95LY2F45TPI11xzPKwTdb+mciUqXWi4w==\n"
                           "-----END CERTIFICATE-----";

static int prepare(const char* currentFileV2)
{
	int rc = -1;
	const char* hosts[] = { "#somecomment\r\n"
		                    "someurl 3389 ff:11:22:dd c3ViamVjdA== aXNzdWVy\r\n"
		                    " \t#anothercomment\r\n"
		                    "otherurl\t3389\taa:bb:cc:dd\tsubject2\tissuer2\r" };
	FILE* fc = NULL;
	size_t i;
	fc = winpr_fopen(currentFileV2, "w+");

	if (!fc)
		goto finish;

	for (i = 0; i < ARRAYSIZE(hosts); i++)
	{
		if (fwrite(hosts[i], strlen(hosts[i]), 1, fc) != 1)
			goto finish;
	}

	rc = 0;
finish:

	if (fc)
		fclose(fc);

	return rc;
}

static BOOL setup_config(rdpSettings** settings)
{
	BOOL rc = FALSE;
	char* path = NULL;
	char sname[8192];
	SYSTEMTIME systemTime;

	if (!settings)
		goto fail;
	*settings = freerdp_settings_new(0);
	if (!*settings)
		goto fail;

	GetSystemTime(&systemTime);
	sprintf_s(sname, sizeof(sname),
	          "TestKnownHostsCurrent-%04" PRIu16 "%02" PRIu16 "%02" PRIu16 "%02" PRIu16 "%02" PRIu16
	          "%02" PRIu16 "%04" PRIu16,
	          systemTime.wYear, systemTime.wMonth, systemTime.wDay, systemTime.wHour,
	          systemTime.wMinute, systemTime.wSecond, systemTime.wMilliseconds);

	path = GetKnownSubPath(KNOWN_PATH_TEMP, sname);
	if (!path)
		goto fail;
	if (!winpr_PathFileExists(path))
	{
		if (!CreateDirectoryA(path, NULL))
		{
			fprintf(stderr, "Could not create %s!\n", path);
			goto fail;
		}
	}

	rc = freerdp_settings_set_string(*settings, FreeRDP_ConfigPath, path);
fail:
	free(path);
	return rc;
}

/* Test if host is found in current file. */
static BOOL test_known_hosts_host_found(rdpCertificateStore* store)
{
	BOOL rc = FALSE;
	rdpCertificateData* stored_data = NULL;
	rdpCertificateData* data;

	printf("%s\n", __FUNCTION__);
	data = certificate_data_new("someurl", 3389);
	if (!data)
	{
		fprintf(stderr, "Could not create certificate data!\n");
		goto finish;
	}
	if (!certificate_data_set_subject(data, "subject") ||
	    !certificate_data_set_issuer(data, "issuer") ||
	    !certificate_data_set_fingerprint(data, "ff:11:22:dd"))
		goto finish;

	if (0 != certificate_store_contains_data(store, data))
	{
		fprintf(stderr, "Could not find data in v2 file!\n");
		goto finish;
	}

	/* Test if we can read out the old fingerprint. */
	stored_data = certificate_store_load_data(store, certificate_data_get_host(data),
	                                          certificate_data_get_port(data));
	if (!stored_data)
	{
		fprintf(stderr, "Could not read old fingerprint!\n");
		goto finish;
	}

	printf("Got %s, %s '%s'\n", certificate_data_get_subject(stored_data),
	       certificate_data_get_issuer(stored_data), certificate_data_get_fingerprint(stored_data));

	rc = TRUE;
finish:
	printf("certificate_data_free %d\n", rc);
	certificate_data_free(data);
	certificate_data_free(stored_data);
	return rc;
}

/* Test if host not found in current file. */
static BOOL test_known_hosts_host_not_found(rdpCertificateStore* store)
{
	BOOL rc = FALSE;
	rdpCertificateData* stored_data = NULL;
	rdpCertificateData* data;

	printf("%s\n", __FUNCTION__);
	data = certificate_data_new("somehost", 1234);

	if (!data)
	{
		fprintf(stderr, "Could not create certificate data!\n");
		goto finish;
	}

	if (!certificate_data_set_fingerprint(data, "ff:aa:bb:cc"))
		goto finish;

	if (0 == certificate_store_contains_data(store, data))
	{
		fprintf(stderr, "Invalid host found in v2 file!\n");
		goto finish;
	}

	/* Test if we read out the old fingerprint fails. */
	stored_data = certificate_store_load_data(store, certificate_data_get_host(data),
	                                          certificate_data_get_port(data));
	if (stored_data)
	{
		fprintf(stderr, "Read out not existing old fingerprint succeeded?!\n");
		goto finish;
	}

	rc = TRUE;
finish:
	printf("certificate_data_free %d\n", rc);
	certificate_data_free(data);
	certificate_data_free(stored_data);
	return rc;
}

/* Test host add current file. */
static BOOL test_known_hosts_host_add(rdpCertificateStore* store)
{
	BOOL rc = FALSE;
	rdpCertificateData* data;

	printf("%s\n", __FUNCTION__);

	data = certificate_data_new("somehost", 1234);

	if (!data)
	{
		fprintf(stderr, "Could not create certificate data!\n");
		goto finish;
	}
	if (!certificate_data_set_subject(data, "ff:aa:bb:cc") ||
	    !certificate_data_set_issuer(data, "ff:aa:bb:cc") ||
	    !certificate_data_set_fingerprint(data, "ff:aa:bb:cc"))
		goto finish;

	if (!certificate_store_save_data(store, data))
	{
		fprintf(stderr, "Could not add host to file!\n");
		goto finish;
	}

	if (0 != certificate_store_contains_data(store, data))
	{
		fprintf(stderr, "Could not find host written in v2 file!\n");
		goto finish;
	}
	rc = TRUE;
finish:
	printf("certificate_data_free %d\n", rc);
	certificate_data_free(data);
	return rc;
}

/* Test host add NULL subject, issuer current file. */
static BOOL test_known_hosts_host_add_remove_null(rdpCertificateStore* store)
{
	BOOL rc = FALSE;
	rdpCertificateData* data;

	printf("%s\n", __FUNCTION__);

	data = certificate_data_new("somehost", 1234);

	if (!data)
	{
		fprintf(stderr, "Could not create certificate data!\n");
		goto finish;
	}
	if (!certificate_data_set_subject(data, NULL) || !certificate_data_set_issuer(data, NULL) ||
	    !certificate_data_set_fingerprint(data, "ff:aa:bb:cc"))
		goto finish;

	if (!certificate_store_save_data(store, data))
	{
		fprintf(stderr, "Could not add host to file!\n");
		goto finish;
	}

	if (0 != certificate_store_contains_data(store, data))
	{
		fprintf(stderr, "Could not find host written in v2 file!\n");
		goto finish;
	}

	if (!certificate_store_remove_data(store, data))
	{
		fprintf(stderr, "Could not remove host written in v2 file!\n");
		goto finish;
	}
	rc = TRUE;
finish:
	printf("certificate_data_free %d\n", rc);
	certificate_data_free(data);
	return rc;
}

/* Test host replace current file. */
static BOOL test_known_hosts_host_replace(rdpCertificateStore* store)
{
	BOOL rc = FALSE;
	rdpCertificateData* data;

	printf("%s\n", __FUNCTION__);
	data = certificate_data_new("somehost", 1234);

	if (!data)
	{
		fprintf(stderr, "Could not create certificate data!\n");
		goto finish;
	}
	if (!certificate_data_set_subject(data, "ff:aa:xx:cc") ||
	    !certificate_data_set_issuer(data, "ff:aa:bb:ee") ||
	    !certificate_data_set_fingerprint(data, "ff:aa:bb:dd:ee"))
		goto finish;

	if (!certificate_store_save_data(store, data))
	{
		fprintf(stderr, "Could not replace data!\n");
		goto finish;
	}

	if (0 != certificate_store_contains_data(store, data))
	{
		fprintf(stderr, "Invalid host found in v2 file!\n");
		goto finish;
	}

	rc = TRUE;
finish:
	printf("certificate_data_free %d\n", rc);
	certificate_data_free(data);
	return rc;
}

/* Test host replace invalid entry in current file. */
static BOOL test_known_hosts_host_replace_invalid(rdpCertificateStore* store)
{
	BOOL rc = FALSE;
	rdpCertificateData* data;

	printf("%s\n", __FUNCTION__);
	data = certificate_data_new(NULL, 1234);

	if (data)
	{
		fprintf(stderr, "Could create invalid certificate data!\n");
		goto finish;
	}
	if (certificate_data_set_fingerprint(data, "ff:aa:bb:dd:ee"))
		goto finish;

	if (certificate_store_save_data(store, data))
	{
		fprintf(stderr, "Invalid return for replace invalid entry!\n");
		goto finish;
	}

	if (0 == certificate_store_contains_data(store, data))
	{
		fprintf(stderr, "Invalid host found in v2 file!\n");
		goto finish;
	}
	rc = TRUE;
finish:
	printf("certificate_data_free %d\n", rc);
	certificate_data_free(data);
	return rc;
}

static BOOL test_known_hosts_file_emtpy_single(BOOL (*fkt)(rdpCertificateStore* store))
{
	BOOL rc = FALSE;
	rdpSettings* settings = NULL;
	rdpCertificateStore* store = NULL;
	char* currentFileV2 = NULL;

	printf("%s", __FUNCTION__);
	if (!fkt)
		return FALSE;

	if (!setup_config(&settings))
		goto finish;
	if (!freerdp_settings_set_bool(settings, FreeRDP_CertificateUseKnownHosts, TRUE))
		goto finish;

	currentFileV2 =
	    GetCombinedPath(freerdp_settings_get_string(settings, FreeRDP_ConfigPath), "known_hosts2");

	if (!currentFileV2)
	{
		fprintf(stderr, "Could not get file path!\n");
		goto finish;
	}

	printf("certificate_store_new\n");
	store = certificate_store_new(settings);

	if (!store)
	{
		fprintf(stderr, "Could not create certificate store!\n");
		goto finish;
	}

	rc = fkt(store);

finish:
	freerdp_settings_free(settings);

	printf("certificate_store_free\n");
	certificate_store_free(store);

	DeleteFileA(currentFileV2);
	free(currentFileV2);
	return rc;
}

static BOOL test_known_hosts_file_empty(void)
{
	BOOL rc = FALSE;

	if (test_known_hosts_file_emtpy_single(test_known_hosts_host_found))
	{
		fprintf(stderr, "[%s] test_known_hosts_file_emtpy_single failed\n", __FUNCTION__);
		goto finish;
	}

	if (!test_known_hosts_file_emtpy_single(test_known_hosts_host_not_found))
	{
		fprintf(stderr, "[%s] test_known_hosts_file_emtpy_single failed\n", __FUNCTION__);
		goto finish;
	}

	if (!test_known_hosts_file_emtpy_single(test_known_hosts_host_add))
	{
		fprintf(stderr, "[%s] test_known_hosts_file_emtpy_single failed\n", __FUNCTION__);
		goto finish;
	}

	if (!test_known_hosts_file_emtpy_single(test_known_hosts_host_add_remove_null))
	{
		fprintf(stderr, "[%s] test_known_hosts_file_emtpy_single failed\n", __FUNCTION__);
		goto finish;
	}

	if (!test_known_hosts_file_emtpy_single(test_known_hosts_host_replace))
	{
		fprintf(stderr, "[%s] test_known_hosts_file_emtpy_single failed\n", __FUNCTION__);
		goto finish;
	}

	if (!test_known_hosts_file_emtpy_single(test_known_hosts_host_replace_invalid))
	{
		fprintf(stderr, "[%s] test_known_hosts_file_emtpy_single failed\n", __FUNCTION__);
		goto finish;
	}

	rc = TRUE;
finish:

	return rc;
}

static BOOL test_known_hosts_file(void)
{
	BOOL rc = FALSE;
	rdpSettings* settings = NULL;
	rdpCertificateStore* store = NULL;
	char* currentFileV2 = NULL;

	printf("%s", __FUNCTION__);
	if (!setup_config(&settings))
		goto finish;
	if (!freerdp_settings_set_bool(settings, FreeRDP_CertificateUseKnownHosts, TRUE))
		goto finish;

	currentFileV2 =
	    GetCombinedPath(freerdp_settings_get_string(settings, FreeRDP_ConfigPath), "known_hosts2");

	if (!currentFileV2)
	{
		fprintf(stderr, "Could not get file path!\n");
		goto finish;
	}

	printf("certificate_store_new\n");
	store = certificate_store_new(settings);

	if (!store)
	{
		fprintf(stderr, "Could not create certificate store!\n");
		goto finish;
	}

	if (prepare(currentFileV2))
		goto finish;

	if (!test_known_hosts_host_found(store))
		goto finish;

	if (!test_known_hosts_host_not_found(store))
		goto finish;

	if (!test_known_hosts_host_add(store))
		goto finish;

	if (!test_known_hosts_host_add_remove_null(store))
		goto finish;

	if (!test_known_hosts_host_replace(store))
		goto finish;

	if (!test_known_hosts_host_replace_invalid(store))
		goto finish;

	rc = TRUE;
finish:
	freerdp_settings_free(settings);

	printf("certificate_store_free\n");
	certificate_store_free(store);

	winpr_DeleteFile(currentFileV2);
	free(currentFileV2);

	return rc;
}

static BOOL equal(const char* a, const char* b)
{
	if (!a && !b)
		return TRUE;
	if (!a || !b)
		return FALSE;
	return strcmp(a, b) == 0;
}

static BOOL compare(const rdpCertificateData* data, const rdpCertificateData* stored)
{
	if (!data || !stored)
		return FALSE;
	if (!equal(certificate_data_get_subject(data), certificate_data_get_subject(stored)))
		return FALSE;
	if (!equal(certificate_data_get_issuer(data), certificate_data_get_issuer(stored)))
		return FALSE;
	if (!equal(certificate_data_get_fingerprint(data), certificate_data_get_fingerprint(stored)))
		return FALSE;
	return TRUE;
}

static BOOL pem_equal(const char* a, const char* b)
{
	BOOL rc = FALSE;
	size_t sa = strlen(a);
	size_t sb = strlen(b);
	X509* x1 = crypto_cert_from_pem(a, sa, FALSE);
	X509* x2 = crypto_cert_from_pem(b, sb, FALSE);
	char* f1 = NULL;
	char* f2 = NULL;
	if (!x1 || !x2)
		goto fail;
	f1 = crypto_cert_fingerprint(x1);
	f2 = crypto_cert_fingerprint(x1);
	if (!f1 || !f2)
		goto fail;

	rc = strcmp(f1, f2) == 0;

fail:
	free(f1);
	free(f2);
	X509_free(x1);
	X509_free(x2);
	return rc;
}

static BOOL compare_ex(const rdpCertificateData* data, const rdpCertificateData* stored)
{
	if (!compare(data, stored))
		return FALSE;
	if (!pem_equal(certificate_data_get_pem(data), certificate_data_get_pem(stored)))
		return FALSE;

	return TRUE;
}

static BOOL test_get_data(rdpCertificateStore* store, const rdpCertificateData* data)
{
	BOOL res;
	rdpCertificateData* stored = certificate_store_load_data(store, certificate_data_get_host(data),
	                                                         certificate_data_get_port(data));
	if (!stored)
		return FALSE;

	res = compare(data, stored);
	certificate_data_free(stored);
	return res;
}

static BOOL test_get_data_ex(rdpCertificateStore* store, const rdpCertificateData* data)
{
	BOOL res;
	rdpCertificateData* stored = certificate_store_load_data(store, certificate_data_get_host(data),
	                                                         certificate_data_get_port(data));
	if (!stored)
		return FALSE;

	res = compare_ex(data, stored);
	certificate_data_free(stored);
	return res;
}

static BOOL test_certs_dir(BOOL useHostsFile)
{
	BOOL rc = FALSE;
	rdpSettings* settings = NULL;
	rdpCertificateStore* store = NULL;
	rdpCertificateData* data1 = NULL;
	rdpCertificateData* data2 = NULL;
	rdpCertificateData* data3 = NULL;
	rdpCertificateData* data4 = NULL;

	printf("%s %d\n", __FUNCTION__, useHostsFile);
	if (!setup_config(&settings))
		goto fail;
	/* Initialize certificate folder backend */
	if (!freerdp_settings_set_bool(settings, FreeRDP_CertificateUseKnownHosts, useHostsFile))
		goto fail;
	printf("certificate_store_new()\n");
	store = certificate_store_new(settings);
	if (!store)
		goto fail;

	{
		printf("certificate_data_new()\n");
		data1 = certificate_data_new("somehost", 1234);
		data2 = certificate_data_new("otherhost", 4321);
		data3 = certificate_data_new("otherhost4", 444);
		data4 = certificate_data_new("otherhost", 4321);
		if (!data1 || !data2 || !data3 || !data4)
			goto fail;

		printf("certificate_data_set_pem(1 [%" PRIuz "])\n", strlen(pem1));
		if (!certificate_data_set_pem(data1, pem1))
			goto fail;
		printf("certificate_data_set_pem(2 [%" PRIuz "])\n", strlen(pem2));
		if (!certificate_data_set_pem(data2, pem2))
			goto fail;
		printf("certificate_data_set_pem(3 [%" PRIuz "])\n", strlen(pem3));
		if (!certificate_data_set_pem(data3, pem3))
			goto fail;
		printf("certificate_data_set_pem(4 [%" PRIuz "])\n", strlen(pem4));
		if (!certificate_data_set_pem(data4, pem4))
			goto fail;

		/* Find non existing in empty store */
		printf("certificate_store_load_data on empty store\n");
		if (test_get_data(store, data1))
			goto fail;
		if (test_get_data_ex(store, data1))
			goto fail;
		if (test_get_data(store, data2))
			goto fail;
		if (test_get_data_ex(store, data2))
			goto fail;
		if (test_get_data(store, data3))
			goto fail;
		if (test_get_data_ex(store, data3))
			goto fail;

		/* Add certificates */
		printf("certificate_store_save_data\n");
		if (!certificate_store_save_data(store, data1))
			goto fail;
		if (!certificate_store_save_data(store, data2))
			goto fail;

		/* Find non existing in non empty store */
		printf("certificate_store_load_data on filled store, non existing value\n");
		if (test_get_data(store, data3))
			goto fail;
		if (test_get_data_ex(store, data3))
			goto fail;

		/* Add remaining certs */
		printf("certificate_store_save_data\n");
		if (!certificate_store_save_data(store, data3))
			goto fail;

		/* Check existing can all be found */
		printf("certificate_store_load_data on filled store, existing value\n");
		if (!test_get_data(store, data1))
			goto fail;
		if (!test_get_data_ex(store, data1))
			goto fail;
		if (!test_get_data(store, data2))
			goto fail;
		if (!test_get_data_ex(store, data2))
			goto fail;
		if (!test_get_data(store, data3))
			goto fail;
		if (!test_get_data_ex(store, data3))
			goto fail;

		/* Modify existing entry */
		printf("certificate_store_save_data modify data\n");
		if (!certificate_store_save_data(store, data4))
			goto fail;

		/* Check new data is in store */
		printf("certificate_store_load_data check modified data can be loaded\n");
		if (!test_get_data(store, data4))
			goto fail;
		if (!test_get_data_ex(store, data4))
			goto fail;

		/* Check old data is no longer valid */
		printf("certificate_store_load_data check original data no longer there\n");
		if (test_get_data(store, data2))
			goto fail;
		if (test_get_data_ex(store, data2))
			goto fail;

		/* Delete a cert */
		printf("certificate_store_remove_data\n");
		if (!certificate_store_remove_data(store, data3))
			goto fail;
		/* Delete non existing, should succeed */
		printf("certificate_store_remove_data missing value\n");
		if (!certificate_store_remove_data(store, data3))
			goto fail;

		printf("certificate_store_load_data on filled store, existing value\n");
		if (!test_get_data(store, data1))
			goto fail;
		if (!test_get_data_ex(store, data1))
			goto fail;
		if (!test_get_data(store, data4))
			goto fail;
		if (!test_get_data_ex(store, data4))
			goto fail;

		printf("certificate_store_load_data on filled store, removed value\n");
		if (test_get_data(store, data3))
			goto fail;
		if (test_get_data_ex(store, data3))
			goto fail;
	}

	rc = TRUE;
fail:
	printf("certificate_data_free %d\n", rc);
	certificate_data_free(data1);
	certificate_data_free(data2);
	certificate_data_free(data3);
	certificate_data_free(data4);
	certificate_store_free(store);
	freerdp_settings_free(settings);
	return rc;
}

int TestKnownHosts(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	if (!test_known_hosts_file_empty())
		return -1;

	if (!test_known_hosts_file())
		return -1;
	if (!test_certs_dir(FALSE))
		return -1;
	if (!test_certs_dir(TRUE))
		return -1;
	return 0;
}
