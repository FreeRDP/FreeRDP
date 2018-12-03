#include <freerdp/log.h>
#include <freerdp/settings.h>

#include "../scquery/scquery.h"
#include "../scquery/scquery_error.h"

#define TAG CLIENT_TAG("smartcardlogon")


static void copy_string(char** old_string, char* new_string)
{
	free(*old_string);
	(*old_string) = NULL;

	if (new_string != NULL)
	{
		char*   copy = check_memory(strdup(new_string), strlen(new_string));
		(*old_string) = copy;
	}
}

int get_info_smartcard(rdpSettings* settings)
{
	if (settings->Pkcs11Module == NULL)
	{
		WLog_ERR(TAG, "Missing /pkcs11module");
		return -1;
	}

	scquery_result identity = scquery_X509_user_identities(settings->Pkcs11Module,
	                          settings->ReaderName,
	                          settings->CardName,
	                          1 || settings->Krb5Trace);

	if (identity == NULL)
	{
		WLog_ERR(TAG, "Could not get an identity from the smartcard %s (reader %s)",
		         settings->CardName,
		         settings->ReaderName);
		return -1;
	}

	copy_string(&settings->UserPrincipalName, identity->upn);
	copy_string(&settings->PkinitIdentity, 	  identity->X509_user_identity);
	copy_string(&settings->TokenLabel,    	  identity->certificate->token_label);
	copy_string(&settings->ContainerName, 	  identity->certificate->label);
	copy_string(&settings->IdCertificate, 	  identity->certificate->id);
	settings->SlotID = identity->certificate->slot_id;
	settings->IdCertificateLength = strlen(identity->certificate->id);
	scquery_result_free(identity);
	return 0;
}
