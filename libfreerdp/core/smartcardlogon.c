#include <freerdp/log.h>
#include <freerdp/settings.h>

#include "../scquery/scquery.h"
#include "../scquery/scquery_error.h"

#define TAG CLIENT_TAG("smartcardlogon")
#define ORNIL(x)  ((x)?(x):"(nil)")

static void copy_string(char** old_string, char* new_string)
{
	free(*old_string);
	(*old_string) = NULL;

	if (new_string != NULL)
	{
		(*old_string) = check_memory(strdup(new_string), strlen(new_string));
	}
}

int get_info_smartcard(rdpSettings* settings)
{
	scquery_result identity = NULL;

	if (settings->Pkcs11Module == NULL)
	{
		WLog_ERR(TAG, "Missing /pkcs11module");
		return -1;
	}

	identity = scquery_X509_user_identities(settings->Pkcs11Module,
		settings->ReaderName,
		settings->CardName,
		settings->Krb5Trace);

	if (identity == NULL)
	{
		WLog_ERR(TAG, "Could not get an identity from the smartcard %s (reader %s)",
			ORNIL(settings->CardName),
			ORNIL(settings->ReaderName));
		return -1;
	}

	copy_string(&settings->CardName,          identity->certificate->token_label);
	copy_string(&settings->ReaderName,        identity->certificate->slot_description);
	copy_string(&settings->UserPrincipalName, identity->upn);
	copy_string(&settings->PkinitIdentity, 	  identity->X509_user_identity);
	copy_string(&settings->TokenLabel,    	  identity->certificate->token_label);
	copy_string(&settings->IdCertificate, 	  identity->certificate->id);
	settings->SlotID = identity->certificate->slot_id;
	settings->IdCertificateLength = strlen(identity->certificate->id);
	settings->PinPadIsPresent = identity->certificate->protected_authentication_path;
	WLog_INFO(TAG, "Got identity from the smartcard %s (reader %s): %s (UPN = %s)",
		ORNIL(settings->CardName),
		ORNIL(settings->ReaderName),
		identity->X509_user_identity,
		identity->upn);
	scquery_result_free(identity);
	return 0;
}
