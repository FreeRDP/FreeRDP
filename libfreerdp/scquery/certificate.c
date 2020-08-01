#include <stdlib.h>
#include "certificate.h"
#include "scquery_error.h"

/* ========================================================================== */
/* smartcard_certificate */

smartcard_certificate scquery_certificate_allocate()
{
	smartcard_certificate certificate = checked_malloc(sizeof(*certificate));

	if (certificate)
	{
		certificate->slot_id = 0;
		certificate->slot_description = NULL;
		certificate->token_label = NULL;
		certificate->token_serial = NULL;
		certificate->id = NULL;
		certificate->label = NULL;
		certificate->type = 0;
		certificate->issuer = NULL;
		certificate->subject = NULL;
		certificate->value = NULL;
		certificate->key_type = 0;
		certificate->protected_authentication_path = 0;
	}

	return certificate;
}

smartcard_certificate scquery_certificate_new(CK_SLOT_ID slot_id, char* slot_description,
                                              char* token_label, char* token_serial, char* id,
                                              char* label, CK_CERTIFICATE_TYPE type, buffer issuer,
                                              buffer subject, buffer value, CK_KEY_TYPE key_type,
                                              int protected_authentication_path)
{
	smartcard_certificate certificate = scquery_certificate_allocate();

	if (certificate)
	{
		certificate->slot_id = slot_id;
		certificate->slot_description = slot_description;
		certificate->token_label = token_label;
		certificate->token_serial = token_serial;
		certificate->id = id;
		certificate->label = label;
		certificate->type = type;
		certificate->issuer = issuer;
		certificate->subject = subject;
		certificate->value = value;
		certificate->key_type = key_type;
		certificate->protected_authentication_path = protected_authentication_path;
	}

	return certificate;
}

void scquery_certificate_deepfree(smartcard_certificate certificate)
{
	if (certificate)
	{
		free(certificate->slot_description);
		free(certificate->token_label);
		free(certificate->token_serial);
		free(certificate->id);
		free(certificate->label);
		buffer_free(certificate->issuer);
		buffer_free(certificate->subject);
		buffer_free(certificate->value);
		scquery_certificate_free(certificate);
	}
}

void scquery_certificate_free(smartcard_certificate certificate)
{
	free(certificate);
}

/**** THE END ****/
