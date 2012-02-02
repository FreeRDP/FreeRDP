
#ifndef __CERTSTORE_UTILS_H
#define __CERTSTORE_UTILS_H

typedef struct rdp_certificate_data rdpCertificateData;
typedef struct rdp_certificate_store rdpCertificateStore;

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/utils/memory.h>

struct rdp_certificate_data
{
	char* hostname;
	char* fingerprint;
};

struct rdp_certificate_store
{
	FILE* fp;
	int match;
	char* path;
	char* file;
	rdpSettings* settings;
	rdpCertificateData* certificate_data;
};

FREERDP_API char* certificate_store_get_path(rdpCertificateStore* certificate_store);
FREERDP_API rdpCertificateData* certificate_data_new(char* hostname, char* fingerprint);
FREERDP_API void certificate_data_free(rdpCertificateData* certificate_data);
FREERDP_API rdpCertificateStore* certificate_store_new(rdpSettings* settings);
FREERDP_API void certificate_store_free(rdpCertificateStore* certificate_store);
FREERDP_API int certificate_data_match(rdpCertificateStore* certificate_store, rdpCertificateData* certificate_data);
FREERDP_API void certificate_data_print(rdpCertificateStore* certificate_store, rdpCertificateData* certificate_data);

#endif /* __CERTSTORE_UTILS_H */
