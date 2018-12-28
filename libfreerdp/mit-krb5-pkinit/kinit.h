#ifndef LIBFREERDP_MIT_KRB5_PKINIT_H
#define LIBFREERDP_MIT_KRB5_PKINIT_H
#include <freerdp/settings.h>

/**
 * @brief Gets a kerberos Ticket Granting Ticket.
 * @param settings->Krb5Trace [input] boolean to set the verbose flag.
 * @param settings->KerberosStartTime [input] string
 * @param settings->KerberosLifeTime [input] string
 * @param settings->KerberosRenewableLifeTime [input] string
 * @param settings->UserPrincipalName [input] string user principal name setting", goto error);
 * @param settings->PkinitIdentity [input] string The PKInit identity string.
 * @param settings->PkinitAnchors [input] string The list of certifciate anchor files, separated with commas.
 * @param settings->CanonicalizedUserHint [output] string Stores the canonicalized UserHint.
 * @return 0 in case of success,  non 0 in case of failure.
 */
int kerberos_get_tgt(rdpSettings* settings);

#endif
