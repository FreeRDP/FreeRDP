#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <freerdp/log.h>

#include "scquery.h"
#include "certificate.h"
#include "smartcard_certificate.h"
#include "scquery_error.h"
#include "scquery_string.h"
#include "x509_alt_names.h"


static const char* scquery_upn_oid = "1.3.6.1.4.1.311.20.2.3";
/* static const char* scquery_kpn_oid = "1.3.6.1.5.2.2"; */


#define TAG CLIENT_TAG("scquery")

void* error_out_of_memory(size_t size)
{
	ERROR(EX_OSERR, "Out of memory, could not allocate %u bytes", size);
	return NULL;
}

static void report_level(DWORD level, const char* file, unsigned long line, const char* function,
	int status, const char * format, va_list ap)
{
	wLog* log = WLog_Get(TAG);

	if ((log != NULL) && (level >= WLog_GetLogLevel(log)))
	{
		WLog_PrintMessageVA(log, WLOG_MESSAGE_TEXT, level, line, file, function, format, ap);
	}
}

void report_error(const char* file, unsigned long line, const char* function, int status,
                  const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	report_level(WLOG_ERROR, file, line, function, status, format, ap);
	va_end(ap);
}

void report_warning(const char* file, unsigned long line, const char* function, int status,
                    const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
<<<<<<< HEAD
	report_level(WLOG_WARN, file, line, function, status, format, ap);
=======
	report_level(WLOG_WARN, file, line, function, status, ap);
>>>>>>> Renamed some scquery files. Added the scquery module to the libfreerdp CMakeLists.txt.
	va_end(ap);
}

const char * next_arg(va_list ap)
{
	return va_arg(ap, const char *);
}

void report_verbose(const char* file, unsigned long line, const char* function, const char* format,
                    ...)
{
	va_list ap;
	va_start(ap, format);
<<<<<<< HEAD
	report_level(WLOG_INFO, file, line, function, 0, format, ap);
=======
	report_level(WLOG_INFO, file, line, function, 0, ap);
>>>>>>> Renamed some scquery files. Added the scquery module to the libfreerdp CMakeLists.txt.
	va_end(ap);
}

void initialize_error_handling(void)
{
	handle_out_of_memory = error_out_of_memory;
	handle_error = report_error;
	handle_warning = report_warning;
	handle_verbose = report_verbose;
}

scquery_result query_X509_user_identities(const char* module, const char* reader_name,
        const char* card_name, int verbose)
{
	scquery_result result = NULL;
	certificate_list current;
	certificate_list clist = find_x509_certificates_with_signing_rsa_private_key(module, reader_name,
	                         card_name, verbose);
	smartcard_certificate entry = NULL;
	char* X509_user_identity = NULL;
	char* upn = NULL;
	DO_CERTIFICATE_LIST(entry, current, clist)
	{
		alt_name name;
		alt_name_list current;
		alt_name_list alist = certificate_extract_subject_alt_names(entry->value);
		DO_ALT_NAME_LIST(name, current, alist)
		{
			if ((0 != strcasecmp("OTHERNAME", name->type))
			    || (name->count < 2)
			    || (0 != strcmp(scquery_upn_oid, name->components[0])))
			{
				continue;
			}

			upn = check_memory(strdup(name->components[1]), 1 + strlen(name->components[1]));
			break;
		}
		alt_name_list_deepfree(alist);

		if (upn != NULL)
		{
			break;
 		}
	}

	if ((entry != NULL) && (upn != NULL))
	{
		X509_user_identity = string_format("PKCS11:module_name=%s:slotid=%lu:token=%s:certid=%s",
			module, entry->slot_id, entry->token_label, entry->id);

		if (X509_user_identity != NULL)
		{
			result = scquery_result_new(entry, X509_user_identity, upn);
			/* Remove entry from clist,  since we keep entry in the result,  and we'll free clist. */
			clist = certificate_list_delete(entry, clist);
		}
	}
	else
	{
		entry = NULL;
	}

	certificate_list_deepfree(clist);

	if (result == NULL)
	{
<<<<<<< HEAD
		scquery_certificate_deepfree(entry);
=======
		scquery_certificate_free(entry);
>>>>>>> Renamed some scquery files. Added the scquery module to the libfreerdp CMakeLists.txt.
		free(X509_user_identity);
		free(upn);
	}

	return result;
}

scquery_result scquery_X509_user_identities(const char* module, const char* reader_name,
        const char* card_name, int verbose)
{
	initialize_error_handling();
	return query_X509_user_identities(module, reader_name, card_name, verbose);
}

scquery_result scquery_result_new(smartcard_certificate  certificate,
                                  char*  X509_user_identity,
                                  char*  upn)
{
	scquery_result result = checked_malloc(sizeof(*result));

	if (result)
	{
		result->certificate = certificate;
		result->X509_user_identity = X509_user_identity;
		result->upn = upn;
	}

	return result;
}

void  scquery_result_free(scquery_result that)
{
	if (that)
	{
		scquery_certificate_deepfree(that->certificate);
		free(that->X509_user_identity);
		free(that->upn);
		free(that);
	}
}

/**** THE END ****/
