/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Smartcard logon
 *
 * Copyright 2017 Dorian Ducournau <dorian.ducournau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <errno.h>

#include <freerdp/client.h>
#include <freerdp/error.h>
#include <freerdp/log.h>

#include "transport.h"
#include "smartcardlogon.h"

#define TAG FREERDP_TAG("core.smartcardlogon")

static CK_FUNCTION_LIST_PTR p11 = NULL;

errno_t memset_s(void *v, rsize_t smax, int c, rsize_t n) {
	if (v == NULL) return EINVAL;
	if (smax > RSIZE_MAX) return EINVAL;
	if (n > smax) return EINVAL;

	volatile unsigned char *p = v;
	while (smax-- && n--) {
		*p++ = c;
	}
	return 0;
}

CK_RV C_UnloadModule(void *module)
{
	pkcs11_module_t *mod = (pkcs11_module_t *) module;

	if (!mod || mod->_magic != MAGIC)
		return CKR_ARGUMENTS_BAD;

	if (mod->handle != NULL && dlclose(mod->handle) < 0)
		return CKR_FUNCTION_FAILED;

	memset(mod, 0, sizeof(*mod));
	free(mod);
	return CKR_OK;
}

void * C_LoadModule(const char *mspec, CK_FUNCTION_LIST_PTR_PTR funcs)
{
	pkcs11_module_t *mod;
	CK_RV rv, (*c_get_function_list)(CK_FUNCTION_LIST_PTR_PTR);
	mod = calloc(1, sizeof(*mod));
	mod->_magic = MAGIC;

	if (mspec == NULL) {
		free(mod);
		return NULL;
	}
	mod->handle = dlopen(mspec, RTLD_LAZY);
	if (mod->handle == NULL) {
		WLog_ERR(TAG, "dlopen failed: %s\n", dlerror());
		goto failed;
	}

	/* Get the list of function pointers */
	c_get_function_list = (CK_RV (*)(CK_FUNCTION_LIST_PTR_PTR))
	dlsym(mod->handle, "C_GetFunctionList");
	if (!c_get_function_list)
		goto failed;
	rv = c_get_function_list(funcs);
	if (rv == CKR_OK)
		return (void *) mod;
	else
		WLog_ERR(TAG, "C_GetFunctionList failed %lx", rv);
failed:
	C_UnloadModule((void *) mod);
	return NULL;
}

#define ATTR_METHOD(ATTR, TYPE) \
static TYPE \
get##ATTR(CK_SESSION_HANDLE sess, CK_OBJECT_HANDLE obj) \
{ \
	TYPE		type = 0; \
	CK_ATTRIBUTE	attr = { CKA_##ATTR, &type, sizeof(type) }; \
	CK_RV		rv; \
 \
	rv = p11->C_GetAttributeValue(sess, obj, &attr, 1); \
	if (rv != CKR_OK) \
		WLog_DBG(TAG, "C_GetAttributeValue(" #ATTR ")", rv); \
	return type; \
}

#define VARATTR_METHOD(ATTR, TYPE) \
static TYPE * \
get##ATTR(CK_SESSION_HANDLE sess, CK_OBJECT_HANDLE obj, CK_ULONG_PTR pulCount) \
{ \
	CK_ATTRIBUTE	attr = { CKA_##ATTR, NULL, 0 }; \
	CK_RV		rv; \
 \
	rv = p11->C_GetAttributeValue(sess, obj, &attr, 1); \
	if (rv == CKR_OK) { \
		if (!(attr.pValue = calloc(1, attr.ulValueLen + 1))) \
			WLog_ERR(TAG, "out of memory in get" #ATTR ": %m"); \
		rv = p11->C_GetAttributeValue(sess, obj, &attr, 1); \
		if (pulCount) \
			*pulCount = attr.ulValueLen / sizeof(TYPE); \
	} else {\
		WLog_DBG(TAG, "C_GetAttributeValue(" #ATTR ")", rv); \
	} \
	return (TYPE *) attr.pValue; \
}

VARATTR_METHOD(LABEL, char);
VARATTR_METHOD(ID, unsigned char);

const char *p11_utf8_to_local(CK_UTF8CHAR *string, size_t len)
{
	static char	buffer[512];
	size_t		n, m;

	while (len && string[len-1] == ' ')
		len--;

	/* For now, simply copy this thing */
	for (n = m = 0; n < sizeof(buffer) - 1; n++) {
		if (m >= len)
			break;
		buffer[n] = string[m++];
	}
	buffer[n] = '\0';
	return buffer;
}

int find_object(CK_SESSION_HANDLE sess, CK_OBJECT_CLASS cls,
		CK_OBJECT_HANDLE_PTR ret,
		const unsigned char *id, size_t id_len, int obj_index)
{
	CK_ATTRIBUTE attrs[2];
	unsigned int nattrs = 0;
	CK_ULONG count;
	CK_RV rv;
	int i;

	attrs[0].type = CKA_CLASS;
	attrs[0].pValue = &cls;
	attrs[0].ulValueLen = sizeof(cls);
	nattrs++;
	if (id) {
		attrs[nattrs].type = CKA_ID;
		attrs[nattrs].pValue = (void *) id;
		attrs[nattrs].ulValueLen = id_len;
		nattrs++;
	}

	rv = p11->C_FindObjectsInit(sess, attrs, nattrs);
	if (rv != CKR_OK)
		WLog_ERR(TAG, "Error C_FindObjectsInit %lu\n", rv);

	for (i = 0; i < obj_index; i++) {
		rv = p11->C_FindObjects(sess, ret, 1, &count);
		if (rv != CKR_OK)
			WLog_ERR(TAG, "Error C_FindObjects %lu\n", rv);
		if (count == 0)
			goto done;
	}
	rv = p11->C_FindObjects(sess, ret, 1, &count);
	if (rv != CKR_OK)
		WLog_ERR(TAG, "Error C_FindObjects %lu\n", rv);

done:
	if (count == 0)
		*ret = CK_INVALID_HANDLE;
	p11->C_FindObjectsFinal(sess);

	return count;
}

CK_RV find_object_with_attributes(CK_SESSION_HANDLE session,
		CK_OBJECT_HANDLE *out,
		CK_ATTRIBUTE *attrs, CK_ULONG attrsLen,
		CK_ULONG obj_index)
{
	CK_ULONG count, ii;
	CK_OBJECT_HANDLE ret;
	CK_RV rv;

	if (!out || !attrs || !attrsLen)
		return CKR_ARGUMENTS_BAD;
	else
		*out = CK_INVALID_HANDLE;

	rv = p11->C_FindObjectsInit(session, attrs, attrsLen);
	if (rv != CKR_OK)
		return rv;

	for (ii = 0; ii < obj_index; ii++) {
		rv = p11->C_FindObjects(session, &ret, 1, &count);
		if (rv != CKR_OK)
			return rv;
		else if (!count)
			goto done;
	}

	rv = p11->C_FindObjects(session, &ret, 1, &count);
	if (rv != CKR_OK)
		return rv;
	else if (count)
		*out = ret;

done:
	p11->C_FindObjectsFinal(session);
	return CKR_OK;
}

int compare_id(rdpSettings * settings, cert_object * cert)
{
	if( (settings->IdCertificateLength != cert->id_cert_length) &&
			( strncmp( (const char *) settings->IdCertificate, (const char *) cert->id_cert, cert->id_cert_length) != 0)
	)
		return -1;

	return 0;
}

CK_ULONG get_mechanisms(CK_SLOT_ID slot, CK_MECHANISM_TYPE_PTR *pList, CK_FLAGS flags)
{
	CK_ULONG	m, n, ulCount = 0;
	CK_RV		rv;

	rv = p11->C_GetMechanismList(slot, *pList, &ulCount);
	*pList = calloc(ulCount, sizeof(**pList));
	if (*pList == NULL)
		WLog_ERR(TAG, "calloc failed: %m");

	rv = p11->C_GetMechanismList(slot, *pList, &ulCount);
	if (rv != CKR_OK)
		WLog_ERR(TAG, "Error C_GetMechanismList %lu\n", rv);

	if (flags != (CK_FLAGS)-1) {
		CK_MECHANISM_TYPE *mechs = *pList;
		CK_MECHANISM_INFO info;

		for (m = n = 0; n < ulCount; n++) {
			rv = p11->C_GetMechanismInfo(slot, mechs[n], &info);
			if (rv != CKR_OK)
				continue;
			if ((info.flags & flags) == flags)
				mechs[m++] = mechs[n];
		}
		ulCount = m;
	}

	return ulCount;
}

int find_mechanism(CK_SLOT_ID slot, CK_FLAGS flags,
		CK_MECHANISM_TYPE_PTR list, size_t list_len,
		CK_MECHANISM_TYPE_PTR result)
{
	CK_MECHANISM_TYPE *mechs = NULL;
	CK_ULONG	count = 0;

	count = get_mechanisms(slot, &mechs, flags);
	if (count)   {
		if (list && list_len)   {
			unsigned ii, jj;

			for (jj=0; jj<count; jj++)   {
				for (ii=0; ii<list_len; ii++)
					if (*(mechs + jj) == *(list + ii))
						break;
				if (ii<list_len)
					break;
			}

			if (jj < count && ii < list_len)
				*result = mechs[jj];
			else
				count = 0;
		}
		else   {
			*result = mechs[0];
		}
	}
	free(mechs);

	return count;
}

int get_slot_login_required(CK_SLOT_ID slot_id)
{
	int rv;
	CK_TOKEN_INFO tinfo;

	rv = p11->C_GetTokenInfo(slot_id, &tinfo);
	if (rv != CKR_OK) {
		WLog_ERR(TAG, "C_GetTokenInfo() failed: 0x%08lX", rv);
		return -1;
	}
	return tinfo.flags & CKF_LOGIN_REQUIRED;
}

int get_slot_protected_authentication_path(rdpSettings * settings, CK_SLOT_ID slot_id)
{
	int rv;
	CK_TOKEN_INFO tinfo;

	rv = p11->C_GetTokenInfo(slot_id, &tinfo);
	if (rv != CKR_OK) {
		WLog_ERR(TAG, "C_GetTokenInfo() failed: 0x%08lX", rv);
		return -1;
	}

	/* get token label */
	settings->TokenLabel = calloc( 1, SIZE_TOKEN_LABEL_MAX);
	if ( settings->TokenLabel == NULL ){
		WLog_ERR(TAG, "Error allocation LabelToken");
		return -1;
	}

	unsigned int i = 1;
	char c = *tinfo.label;

	while( c != '\0' && i < SIZE_TOKEN_LABEL_MAX)
	{
		sprintf(settings->TokenLabel, "%s%c", settings->TokenLabel, c);

		c = *(tinfo.label+i);

		if( c == ' '){
			char temp = *(tinfo.label+i+1);
			/* two consecutive spaces mean the end of token label */
			if ( temp == ' ' || temp == '\0' )
				break;
		}
		i++;
	}

	settings->TokenLabel[i] = '\0';

	WLog_DBG(TAG, "TokenLabel=%s", settings->TokenLabel);

	return tinfo.flags & CKF_PROTECTED_AUTHENTICATION_PATH;
}

int crypto_init(cert_policy *policy)
{
	/* arg is ignored for OPENSSL */
	(void)policy;
	OpenSSL_add_all_algorithms();
	ERR_load_crypto_strings();
	return 0;
}

/** init_authentication_pin is used to login to the token
 * 	and to get #PKCS11 session informations, then use
 * 	to retrieve valid certificate and UPN.
 *  This function is actually called in get_valid_smartcard_cert()
 *  @param nla - pointer to the rdpNla structure that contains nla connection settings
 *  @return CKR_OK if all pkcs11 functions succeed.
 */
CK_RV init_authentication_pin(rdpNla * nla)
{
	freerdp * instance = nla->instance;
	const char * opt_module = instance->settings->Pkcs11Module;
	void * module = NULL;
	CK_SESSION_HANDLE session = CK_INVALID_HANDLE;
	CK_ULONG p11_num_slots = 0;
	CK_SLOT_ID_PTR p11_slots = NULL;
	CK_RV rv = CKR_GENERAL_ERROR;
	CK_ULONG n;
	CK_SLOT_INFO info;
	CK_SESSION_INFO sessionInfo;
	CK_SLOT_ID	opt_slot = 0;
	int flags = CKF_SERIAL_SESSION;
	CK_OBJECT_HANDLE privKeyObject;
	CK_ULONG j;
	char *label = NULL;
	unsigned char *id = NULL;
	CK_ULONG idLen;

	module = C_LoadModule(opt_module, &p11);
	if (module == NULL) {
		WLog_ERR(TAG, "Failed to load pkcs11 module %s", opt_module);
		free(nla->p11handle);
		return CKR_FUNCTION_FAILED;
	}
	else
		WLog_DBG(TAG, "Load %s module with success !", opt_module);

	nla->p11handle->p11_module_handle = module;

	rv = p11->C_Initialize(NULL);

	if (rv == CKR_CRYPTOKI_ALREADY_INITIALIZED)
		WLog_ERR(TAG, "Cryptoki library has already been initialized");
	else if (rv != CKR_OK) {
		WLog_ERR(TAG, "Error C_Initialize, %lu", rv);
		goto closing_session;
	}

	/* list slots */
	rv = p11->C_GetSlotList(TRUE, NULL, &p11_num_slots);
	if (rv != CKR_OK) {
		WLog_ERR(TAG, "Error C_GetSlotList(NULL), %lu", rv);
		goto closing_session;
	}

	p11_slots = calloc(p11_num_slots, sizeof(CK_SLOT_ID));
	if (p11_slots == NULL) {
		WLog_ERR(TAG, "calloc failed");
		goto closing_session;
	}

	rv = p11->C_GetSlotList(TRUE, p11_slots, &p11_num_slots);
	if (rv != CKR_OK) {
		WLog_ERR(TAG, "Error C_GetSlotList() %lu", rv);
		goto closing_session;
	}

	for (n = 0; n < p11_num_slots; n++) {
		WLog_DBG(TAG, "Slot %lu (0x%lx): ", n, p11_slots[n]);
		rv = p11->C_GetSlotInfo(p11_slots[n], &info);
		if (rv != CKR_OK) {
			WLog_ERR(TAG, "(GetSlotInfo failed, %lu)", rv);
			continue;
		}
		WLog_DBG(TAG, "%s", p11_utf8_to_local(info.slotDescription,
				sizeof(info.slotDescription)));
		if (!(info.flags & CKF_TOKEN_PRESENT)) {
			WLog_ERR(TAG, "  (empty)");
			continue;
		}

		if (info.flags & CKF_TOKEN_PRESENT)
			opt_slot = p11_slots[n];
	}

	if (p11_num_slots == 0) {
		WLog_ERR(TAG, "No slots.");
		goto exit_failure;
	}

	instance->settings->PinLoginRequired = FALSE;

	if( get_slot_protected_authentication_path(instance->settings, opt_slot) ){

		if( get_slot_login_required(opt_slot) ){
			/* if TRUE user must login */
			instance->settings->PinLoginRequired = TRUE;
		}

		/* TRUE if token has a "protected authentication path",
		 * whereby a user can log into the token without passing a PIN through the Cryptoki library,
		 * means PINPAD is present */
		instance->settings->PinPadIsPresent = TRUE;
	}
	else{
		if( get_slot_login_required(opt_slot) ){
			instance->settings->PinLoginRequired = TRUE;
		}
		instance->settings->PinPadIsPresent = FALSE;
	}

	rv = p11->C_OpenSession(opt_slot, flags, NULL, NULL, &session);
	if (rv != CKR_OK) {
		WLog_ERR(TAG, "C_OpenSession() failed: 0x%08lX", rv);
		goto closing_session;
	}

	/* set PIN settings to string "NULL" for further credentials delegation */
	if( instance->settings->Pin == NULL ){
		instance->settings->Pin = (char *) calloc( PIN_LENGTH + 1, sizeof(char) );

		if(instance->settings->Pin != NULL){
			memcpy(instance->settings->Pin, "NULL", PIN_LENGTH+1 );
		}
		else{
			WLog_ERR(TAG, "Error allocating memory for PIN");
			goto closing_session;
		}
	}

	/* Login token */
	/* call pkcs#11 login to ensure that the user is the real owner of the card,
	 * but also because some tokens cannot read protected data until the PIN Global is unlocked */
	if( (instance->settings->PinPadIsPresent && !instance->settings->PinLoginRequired) ){
		rv = p11->C_Login(session, CKU_USER, NULL_PTR,  0 );
	}
	else if ( !instance->settings->PinPadIsPresent ){
		rv = pkcs11_do_login(session, opt_slot, instance->settings);
	}
	else{
#ifndef HANDLE_PINPAD_WITH_LOGIN_REQUIRED
		/* Using a pinpad with the slot's setting 'login required' is not handled by default,
		 * because it would require users to type two times their PIN code in a few seconds.
		 * First, to unlock the token to be able to read protected data. And then, later in PKINIT,
		 * to get Kerberos ticket (TGT) to authenticate against KDC.
		 * Nevertheless, if you want to handle this case uncomment #define HANDLE_PINPAD_WITH_LOGIN_REQUIRED
		 * in pkinit/pkinit.h */
		WLog_ERR(TAG, "Error configuration slot token");
		goto closing_session;
#else
		rv = p11->C_Login(session, CKU_USER, NULL_PTR,  0 );
#endif
	}

	if ((rv != CKR_OK) && (rv != CKR_USER_ALREADY_LOGGED_IN)) {
		WLog_ERR(TAG, "C_Login() failed: 0x%08lX", rv);
		goto closing_session;
	}
	else {
		WLog_DBG(TAG, "-------------- Login token OK --------------");
	}

	rv = p11->C_GetSessionInfo(session, &sessionInfo);
	if (rv != CKR_OK) {
		WLog_ERR(TAG, "Error C_OpenSession %lu\n", rv);
		goto closing_session;
	}

	for (j = 0; find_object(session, CKO_PRIVATE_KEY, &privKeyObject, NULL, 0, j); j++){
		if ((label = getLABEL(session, privKeyObject, NULL)) != NULL){
			WLog_DBG(TAG, "label = (%s)", label);
		}

		id = getID(session, privKeyObject, &idLen);
		if (id == NULL) {
			WLog_ERR(TAG, "private key has no ID : can't find corresponding certificate without it");
			goto closing_session;
		}
		else
		{
			int i;
			instance->settings->IdCertificateLength = idLen;

			/* (idLen * 2) because idCertificate is stored in hexadecimal : 1 unity for id cert length equals to 2 characters for id cert string */
			instance->settings->IdCertificate = (CK_BYTE *) calloc( idLen * 2 + 1, sizeof(CK_BYTE) );
			if(instance->settings->IdCertificate == NULL){
				WLog_ERR(TAG, "Error allocating memory for IdCertificate");
				goto closing_session;
			}

			for (i = 1; i <= idLen; i++) {
				sprintf( (char *) instance->settings->IdCertificate, "%s%02x", instance->settings->IdCertificate, id[i-1]);
			}
			instance->settings->IdCertificate[idLen*2] = '\0';

			instance->settings->SlotID = calloc( SIZE_NB_SLOT_ID_MAX + 1, sizeof(char) );
			if( instance->settings->SlotID == NULL )
				goto closing_session;

			sprintf(instance->settings->SlotID, "%lu", sessionInfo.slotID);
			if(sessionInfo.slotID > 9)
				instance->settings->SlotID[2] = '\0';
			else
				instance->settings->SlotID[1] = '\0';
		}

		break;
	}

	if (privKeyObject == CK_INVALID_HANDLE) {
		WLog_ERR(TAG, "Signatures: no private key found in this slot");
		goto closing_session;
	}

	free(label);
	free(id);
	free(p11_slots);

	nla->p11handle->session = session;
	nla->p11handle->slot_id = opt_slot;
	nla->p11handle->private_key = privKeyObject;

	return CKR_OK;

closing_session:
	free(id);
	free(label);
	free(p11_slots);

	if(session != CK_INVALID_HANDLE){
		WLog_ERR(TAG, "closing the PKCS #11 session due to previous event");
		rv = p11->C_CloseSession(session);
		if ( (rv != CKR_OK) && (rv != CKR_FUNCTION_NOT_SUPPORTED) ) {
			WLog_ERR(TAG, "C_CloseSession() failed: 0x%08lX", rv);
			if(opt_slot)
				rv = p11->C_CloseAllSessions(opt_slot);
			if ( (rv != CKR_OK) && (rv != CKR_FUNCTION_NOT_SUPPORTED) )
				WLog_ERR(TAG, "C_CloseAllSessions() failed: 0x%08lX", rv);
		}
	}

	p11->C_Finalize(NULL);
	C_UnloadModule(module);
	free(nla->p11handle);

	if(rv==CKR_OK) /* means error other than pkcs11 functions */
		return CKR_GENERAL_ERROR;
	else /* means a pkcs11 function failed */
		return rv;

exit_failure:
	free(p11_slots);
	p11->C_Finalize(NULL);
	C_UnloadModule(module);
	free(nla->p11handle);
	return CKR_TOKEN_NOT_PRESENT;
}

/** pkcs11_do_login is used to do login by asking PIN code.
 *  Function called only if pinpad is NOT used.
 *  This function is actually called in init_authentication_pin()
 *  @param session - valid PKCS11 session handle
 *  @param slod_id - slot id of present token
 *  @param settings - pointer to rdpSettings structure that contains settings
 *  @return CKR_OK if pkcs11 login succeed.
 */
CK_RV pkcs11_do_login(CK_SESSION_HANDLE session, CK_SLOT_ID slot_id, rdpSettings * settings)
{
	CK_RV ret=0, ret_token=0;
	unsigned int try_left=NB_TRY_MAX_LOGIN_TOKEN;
	char *pin;
	BOOL retypePin = FALSE;

	ssize_t size_prompt_message = strlen(settings->TokenLabel) + strlen(" PIN :");
	char * prompt_message = calloc( size_prompt_message + 1, sizeof(char) );
	if(prompt_message == NULL) return -1;
	strncpy(prompt_message, settings->TokenLabel, strlen(settings->TokenLabel) );
	strncat(prompt_message, " PIN:", strlen(" PIN:") );
	prompt_message[strlen(settings->TokenLabel) + strlen(" PIN :")] = '\0';

	while(try_left>0)
	{
		/* get PIN if no already given in command line argument */
		if( strncmp(settings->Pin, "NULL", 4) == 0 ){
			pin = getpass(prompt_message);
			retypePin = FALSE;
		}
		else if( try_left == NB_TRY_MAX_LOGIN_TOKEN && !retypePin){
			pin = calloc( PIN_LENGTH + 1, sizeof(char) );
			if(!pin)
				return -1;
			else {
				strncpy(pin, settings->Pin, PIN_LENGTH);
				pin[PIN_LENGTH] = '\0';
			}
		}
		else{
			/* Login fail using PIN from getpass() or bad formatted PIN code
			 * given in command line argument.
			 * Reset PIN to "NULL" to get it again from getpass */
			strncpy(settings->Pin, "NULL", PIN_LENGTH + 1);
			continue;
		}

		if (NULL == pin || strlen(pin) > 4) {
			WLog_ERR(TAG, "Error encountered while reading PIN");
			continue;
		}

		/* check pin length */
		if ( strlen(pin) == 0) {
			WLog_ERR(TAG, "Empty PIN are not allowed");
			continue;
		}

		/* check if pin characters are [0-9] to avoid keyboard num lock errors */
		int i;
		for( i=0; i<strlen(pin); i++){
			if( !isdigit(pin[i]) ){
				retypePin = TRUE;
				break;
			}
		}

		if(retypePin){
			WLog_ERR(TAG, "Bad format - Please retype PIN (4-digits)");
			continue;
		}

		CK_TOKEN_INFO tinfo;
		ret_token = p11->C_GetTokenInfo( slot_id, &tinfo);
		if (ret_token != CKR_OK) {
			WLog_ERR(TAG, "C_GetTokenInfo() failed: 0x%08lX", ret_token);
			return -1;
		}

		/* store token flags before login try */
		CK_FLAGS flags_before_login = tinfo.flags;

		/* check (if these token flags are set) how many PIN tries left before first login try */
		if( flags_before_login & CKF_USER_PIN_COUNT_LOW ){
			WLog_ERR(TAG, "An incorrect user login PIN has been entered at least once since the last successful authentication /!\\ ");
		}
		if( flags_before_login & CKF_USER_PIN_FINAL_TRY ){
			WLog_ERR(TAG, "/!\\ Supplying an incorrect user PIN will cause it to become locked /!\\ ");
		}
		if( flags_before_login & CKF_USER_PIN_LOCKED ){
			WLog_ERR(TAG, "/!\\ /!\\ The user PIN has been locked. User login to the token is not possible /!\\ /!\\ ");
			return -1;
		}

		/* perform pkcs #11 login */
		ret = pkcs11_login(session, settings, pin);

		ret_token = p11->C_GetTokenInfo( slot_id, &tinfo);
		if (ret_token != CKR_OK) {
			WLog_ERR(TAG, "C_GetTokenInfo() failed: 0x%08lX", ret_token);
			return -1;
		}

		CK_FLAGS flags_after_login = tinfo.flags;
		WLog_DBG(TAG, "flags after login : %d", flags_after_login);

		/* Some middlewares do not set standard token flags if PIN entry was wrong.
		 * That's why we use here the C_Login return to check if the PIN was correct or not
		 * rather than the token flags. In the case where flags are not set properly,
		 * the C_Login return is the only (non middleware specific) way to check how many PIN try left us.
		 * However, Kerberos PKINIT uses standard token flags to set responder callback questions.
		 * Thus, for both middlewares (setting token flags or not) we store the state of token flags
		 * to use it later in PKINIT.
		 */

		if( flags_before_login == flags_after_login ){

			if( ret == CKR_OK && try_left == NB_TRY_MAX_LOGIN_TOKEN )
				settings->TokenFlags |= FLAGS_TOKEN_USER_PIN_OK; /* means no error while logging to the token */

			if( flags_after_login & CKF_USER_PIN_INITIALIZED )
				WLog_DBG(TAG, "CKF_USER_PIN_INITIALIZED set" );
			if( flags_after_login & CKF_LOGIN_REQUIRED )
				WLog_DBG(TAG, "CKF_LOGIN_REQUIRED set" );
			if( flags_after_login & CKF_TOKEN_INITIALIZED )
				WLog_DBG(TAG, "CKF_TOKEN_INITIALIZED set" );
			if( flags_after_login & CKF_PROTECTED_AUTHENTICATION_PATH )
				WLog_DBG(TAG, "CKF_PROTECTED_AUTHENTICATION_PATH set");
		}
		else{
			/* We set the flags CKF_USER_PIN_COUNT_LOW, CKF_USER_PIN_FINAL_TRY and CKF_USER_PIN_LOCKED
			 * only when the middleware set them itself.
			 */

			if( flags_after_login & CKF_USER_PIN_COUNT_LOW ){
				settings->TokenFlags |= FLAGS_TOKEN_USER_PIN_COUNT_LOW;

				WLog_ERR(TAG, "/!\\    WARNING    /!\\ 	  PIN INCORRECT (x1)	 /!\\	2 tries left  /!\\");
				try_left--;
				continue;
			}
			if( flags_after_login & CKF_USER_PIN_FINAL_TRY ){
				settings->TokenFlags |= FLAGS_TOKEN_USER_PIN_FINAL_TRY;

				WLog_ERR(TAG, "/!\\ 	DANGER   /!\\   PIN INCORRECT (x2)   /!\\	  Only 1 try left   /!\\");
				try_left--;
				continue;
			}
			if( flags_after_login & CKF_USER_PIN_LOCKED ){
				settings->TokenFlags |= FLAGS_TOKEN_USER_PIN_LOCKED;
				WLog_ERR(TAG, "/!\\ **** CRITICAL ERROR **** /!\\ **** PIN LOCKED **** /!\\ **** END OF PROGRAM **** /!\\");
				goto error_pin_entry;
			}
		}

		/* store PIN code in settings if C_Login() succeeded */
		if( ret == CKR_OK ) {
			strncpy(settings->Pin, pin, PIN_LENGTH + 1);
			break;
		}
		else {
			try_left--; /* 3 C_Login() tries at maximum */

			if( ret & CKR_PIN_INCORRECT ){
				if( try_left == 2){

					if( ( settings->TokenFlags & FLAGS_TOKEN_USER_PIN_COUNT_LOW ) == 0 ){

						/* It means that middleware does not set CKF_USER_PIN_COUNT_LOW token flag.
						 *  If so, that would have already been done previously with the corresponding token flag.
						 *  Thus, we set the token flag to FLAGS_TOKEN_USER_PIN_NOT_IMPLEMENTED.
						 */
						settings->TokenFlags |= FLAGS_TOKEN_USER_PIN_NOT_IMPLEMENTED;
					}

					WLog_ERR(TAG, "/!\\    WARNING    /!\\ 	  PIN INCORRECT (x1)	 /!\\	2 tries left  /!\\");
					continue;
				}
				if( try_left == 1){

					if( ( settings->TokenFlags & FLAGS_TOKEN_USER_PIN_FINAL_TRY ) == 0 ){
						/* means that middleware does not set CKF_USER_PIN_FINAL_TRY token flag */
						settings->TokenFlags |= FLAGS_TOKEN_USER_PIN_NOT_IMPLEMENTED;
					}

					WLog_ERR(TAG, "/!\\ 	DANGER   /!\\   PIN INCORRECT (x2)   /!\\	  Only 1 try left   /!\\");
					continue;
				}
			}
			if( ret & CKR_PIN_LOCKED ){

				if( ( settings->TokenFlags & FLAGS_TOKEN_USER_PIN_LOCKED ) == 0 ){
					/* means that middleware does not set CKF_USER_PIN_LOCKED token flag */
					settings->TokenFlags |= FLAGS_TOKEN_USER_PIN_NOT_IMPLEMENTED;
				}

				WLog_ERR(TAG, "/!\\ **** CRITICAL ERROR **** /!\\ **** PIN LOCKED **** /!\\ **** END OF PROGRAM **** /!\\");
				goto error_pin_entry;
			}
		}
	}

	WLog_DBG(TAG, "%s %d : tokenFlags=%"PRId32"", __FUNCTION__, __LINE__, settings->TokenFlags);

error_pin_entry:
		free(prompt_message);
		if(pin){
			if( memset_s(pin, PIN_LENGTH, 0, PIN_LENGTH) )
				memset(pin, 0, PIN_LENGTH);
			free(pin);
		}

	return ret;
}

/* perform #PKCS11 C_Login */
CK_RV pkcs11_login(CK_SESSION_HANDLE session, rdpSettings *h, char *pin)
{
	CK_RV rv;

	WLog_DBG(TAG, "login as user CKU_USER");
	if (pin){
		WLog_DBG(TAG, "C_Login with PIN");
		rv = p11->C_Login(session, CKU_USER, (unsigned char*)pin, strlen(pin));
	}
	else{
		WLog_DBG(TAG, "C_Login without PIN");
		rv = p11->C_Login(session, CKU_USER, NULL, 0);
	}
	if ((rv != CKR_OK) && (rv != CKR_USER_ALREADY_LOGGED_IN)) {
		WLog_ERR(TAG, "C_Login() failed: 0x%08lX", rv);
		return rv;
	}
	return rv;
}

/* init_pkcs_data_handle allocates memory to manage pkcs11 session. */
int init_pkcs_data_handle(rdpNla * nla)
{
	nla->p11handle = (pkcs11_handle *) calloc(1, sizeof( pkcs11_handle ));
	if( !nla->p11handle ){
		WLog_ERR(TAG, "Error allocation pkcs11_handle");
		return -1;
	}

	memset(nla->p11handle, 0, sizeof(pkcs11_handle));

	return 1;
}

/** get_info_smartcard is used to retrieve a valid authentication certificate.
 *  This function is actually called in nla_client_init().
 *  @param nla : rdpNla structure that contains nla settings
 *  @return CKR_OK if successful, CKR_GENERAL_ERROR if error occurred
 */
CK_RV get_info_smartcard(rdpNla * nla)
{
	int ret=0;
	CK_RV rv;

	ret = init_pkcs_data_handle(nla);
	if(ret == -1)
		return CKR_GENERAL_ERROR;

	/* retrieve a valid authentication certificate */
	ret = get_valid_smartcard_cert(nla);
	if(ret != 0)
		return CKR_GENERAL_ERROR;

	WLog_DBG(TAG, "settings : Id Authentication Certificate [%s]", nla->settings->IdCertificate );

	/* retrieve UPN from smartcard */
	ret = get_valid_smartcard_UPN(nla->settings, nla->p11handle->valid_cert->x509);

	if ( ret < 0 ) {
		WLog_ERR(TAG, "Fail to get valid UPN (upn=%s)", nla->settings->UserPrincipalName);
		goto auth_failed;
	}
	else {
		WLog_DBG(TAG, "UPN is correct (upn=%s)", nla->settings->UserPrincipalName);
	}

	/* close pkcs #11 session */
	rv = close_pkcs11_session(nla->p11handle);
	if (rv != 0) {
		release_pkcs11_module(nla->p11handle);
		WLog_ERR(TAG, "close_pkcs11_session() failed: %s", get_error());
		return CKR_GENERAL_ERROR;
	}

	/* release pkcs #11 module */
	WLog_DBG(TAG, "releasing pkcs #11 module...");
	release_pkcs11_module(nla->p11handle);

	WLog_DBG(TAG, "UPN retrieving process completed");

	return CKR_OK;

auth_failed:
	close_pkcs11_session(nla->p11handle);
	release_pkcs11_module(nla->p11handle);
	return CKR_GENERAL_ERROR;
}


/* retrieve the private key associated with a given certificate */
int get_private_key(pkcs11_handle *h, cert_object *cert) {
	CK_OBJECT_CLASS key_class = CKO_PRIVATE_KEY;
	CK_BBOOL key_sign = CK_TRUE;
	CK_ATTRIBUTE key_template[] = {
			{CKA_CLASS, &key_class, sizeof(key_class)}
			,
			{CKA_SIGN, &key_sign, sizeof(key_sign)}
			,
			{CKA_ID, NULL, 0}
	};
	CK_OBJECT_HANDLE object;
	CK_ULONG object_count;
	int rv;

	if ( h->private_key != CK_INVALID_HANDLE ) {
		/* we've already found the private key for this certificate */
		return 0;
	}

	/* search for a specific ID is any */
	if (cert->id_cert && cert->id_cert_length) {
		key_template[2].pValue = cert->id_cert;
		key_template[2].ulValueLen = cert->id_cert_length;
		rv = p11->C_FindObjectsInit(h->session, key_template, 3);
	} else {
		rv = p11->C_FindObjectsInit(h->session, key_template, 2);
	}
	if (rv != CKR_OK) {
		WLog_ERR(TAG, "C_FindObjectsInit() failed: 0x%08lX", rv);
		return -1;
	}
	rv = p11->C_FindObjects(h->session, &object, 1, &object_count);
	if (rv != CKR_OK) {
		WLog_ERR(TAG, "C_FindObjects() failed: 0x%08lX", rv);
		goto get_privkey_failed;
	}
	if (object_count <= 0) {
		/* cert without prk: perhaps CA or CA-chain cert */
		WLog_ERR(TAG, "No private key found for cert: 0x%08lX", rv);
		goto get_privkey_failed;
	}

	/* and finally release Find session */
	rv = p11->C_FindObjectsFinal(h->session);
	if (rv != CKR_OK) {
		WLog_ERR(TAG, "C_FindObjectsFinal() failed: 0x%08lX", rv);
		return -1;
	}

	cert->private_key = object;
	cert->key_type = CKK_RSA;

	return 0;

get_privkey_failed:
	rv = p11->C_FindObjectsFinal(h->session);
	if (rv != CKR_OK) {
		WLog_ERR(TAG, "C_FindObjectsFinal() failed: 0x%08lX", rv);
	}
	return -1;
}


void free_certs(cert_object **certs, int cert_count)
{
	int i;

	for (i = 0; i < cert_count; i++) {
		if (!certs[i]) {
			continue;
		}
		if (certs[i]->x509 != NULL)
			X509_free(certs[i]->x509);
		if (certs[i]->id_cert != NULL)
			free(certs[i]->id_cert);
		free(certs[i]);
	}
	free(certs);
}


void release_pkcs11_module(pkcs11_handle * handle)
{
	/* finalise pkcs #11 module */
	p11->C_Finalize(NULL);

	/* Unload module */
	if(handle->p11_module_handle->handle)
		C_UnloadModule(handle->p11_module_handle);

	free_certs(handle->certs, handle->cert_count);
	memset(handle, 0, sizeof(pkcs11_handle));
	free(handle);
}


int close_pkcs11_session(pkcs11_handle * h)
{
	int rv;

	if(h->session == CK_INVALID_HANDLE)
		return 0;

	/* close user-session */
	WLog_DBG(TAG, "logout user");
	rv = p11->C_Logout(h->session);
	if (rv != CKR_OK && rv != CKR_USER_NOT_LOGGED_IN
			&& rv != CKR_FUNCTION_NOT_SUPPORTED) {
		WLog_ERR(TAG, "C_Logout() failed: 0x%08lX", rv);
		return -1;
	}
	WLog_DBG(TAG, "closing the PKCS #11 session");
	rv = p11->C_CloseSession(h->session);
	if (rv != CKR_OK && rv != CKR_FUNCTION_NOT_SUPPORTED) {
		WLog_ERR(TAG, "C_CloseSession() failed: 0x%08lX", rv);
		return -1;
	}
	WLog_DBG(TAG, "releasing keys and certificates");
	if (h->certs != NULL) {
		free_certs(h->certs, h->cert_count);
		h->certs = NULL;
		h->cert_count = 0;
	}
	return 0;
}


/** get_list_certificate find all certificates present on smartcard.
 *  This function is actually called in get_valid_smartcard_cert().
 *  @param p11handle - pointer to the pkcs11_handle structure that contains the variables
 *  to manage pkcs11 session
 *  @param ncerts - number of certificates of the smartcard
 *  @return list of certificates found
 */
cert_object **get_list_certificate(pkcs11_handle * p11handle, int *ncerts)
{
	CK_BYTE *id_value = NULL;
	CK_BYTE *cert_value = NULL;
	CK_OBJECT_HANDLE object;
	CK_ULONG object_count;
	X509 *x509 = NULL;
	int rv;

	CK_OBJECT_CLASS cert_class = CKO_CERTIFICATE;
	CK_CERTIFICATE_TYPE cert_type = CKC_X_509;
	CK_ATTRIBUTE cert_template[] = {
			{CKA_CLASS, &cert_class, sizeof(CK_OBJECT_CLASS)}
			,
			{CKA_CERTIFICATE_TYPE, &cert_type, sizeof(CK_CERTIFICATE_TYPE)}
			,
			{CKA_ID, NULL, 0}
			,
			{CKA_VALUE, NULL, 0}
	};

	if (p11handle->certs) {
		*ncerts = p11handle->cert_count;
		return p11handle->certs;
	}

	rv = p11->C_FindObjectsInit(p11handle->session, cert_template, 2);
	if (rv != CKR_OK) {
		WLog_ERR(TAG, "C_FindObjectsInit() failed: 0x%08lX", rv);
		return NULL;
	}
	while(1) {
		/* look for certificates */
		rv = p11->C_FindObjects(p11handle->session, &object, 1, &object_count);
		if (rv != CKR_OK) {
			WLog_ERR(TAG, "C_FindObjects() failed: 0x%08lX", rv);
			goto getlist_error;
		}
		if (object_count == 0) break; /* no more certs */

		/* Pass 1: get cert id */

		/* retrieve cert object id length */
		cert_template[2].pValue = NULL;
		cert_template[2].ulValueLen = 0;
		rv = p11->C_GetAttributeValue(p11handle->session, object, cert_template, 3);
		if (rv != CKR_OK) {
			WLog_ERR(TAG, "CertID length: C_GetAttributeValue() failed: 0x%08lX", rv);
			goto getlist_error;
		}
		/* allocate enought space */
		id_value = malloc(cert_template[2].ulValueLen);
		if (id_value == NULL) {
			WLog_ERR(TAG, "Cert id malloc(%"PRIu32"): not enough free memory available", cert_template[2].ulValueLen);
			goto getlist_error;
		}
		/* read cert id into allocated space */
		cert_template[2].pValue = id_value;
		rv = p11->C_GetAttributeValue(p11handle->session, object, cert_template, 3);
		if (rv != CKR_OK) {
			WLog_ERR(TAG, "CertID value: C_GetAttributeValue() failed: 0x%08lX", rv);
			goto getlist_error;
		}

		/* Pass 2: get certificate */

		/* retrieve cert length */
		cert_template[3].pValue = NULL;
		rv = p11->C_GetAttributeValue(p11handle->session, object, cert_template, 4);
		if (rv != CKR_OK) {
			WLog_ERR(TAG, "Cert Length: C_GetAttributeValue() failed: 0x%08lX", rv);
			goto getlist_error;
		}
		/* allocate enough space */
		cert_value = malloc(cert_template[3].ulValueLen);
		if (cert_value == NULL) {
			WLog_ERR(TAG, "Cert value malloc(%"PRIu32"): not enough free memory available", cert_template[3].ulValueLen);
			goto getlist_error;
		}
		/* read certificate into allocated space */
		cert_template[3].pValue = cert_value;
		rv = p11->C_GetAttributeValue(p11handle->session, object, cert_template, 4);
		if (rv != CKR_OK) {
			WLog_ERR(TAG, "Cert Value: C_GetAttributeValue() failed: 0x%08lX", rv);
			goto getlist_error;
		}

		/* Pass 3: store certificate */

		/* convert to X509 data structure */
		x509 = d2i_X509(NULL, (const unsigned char **)&cert_template[3].pValue, cert_template[3].ulValueLen);
		if (x509 == NULL) {
			WLog_ERR(TAG, "d2i_x509() failed: %s", ERR_error_string(ERR_get_error(), NULL));
			goto getlist_error;
		}

		/* finally add certificate to chain */
		p11handle->certs = realloc(p11handle->certs, (p11handle->cert_count + 1) * sizeof(cert_object *));
		if (!p11handle->certs) {
			WLog_ERR(TAG, "realloc() not space to re-size cert table");
			goto getlist_error;
		}

		WLog_DBG(TAG, "Saving Certificate #%d", p11handle->cert_count + 1);
		p11handle->certs[p11handle->cert_count] = NULL;
		p11handle->certs[p11handle->cert_count] = (cert_object *) calloc( 1, sizeof(cert_object) );
		if (p11handle->certs[p11handle->cert_count] == NULL) {
			WLog_ERR(TAG, "calloc() not space to allocate cert object");
			goto getlist_error;
		}

		p11handle->certs[p11handle->cert_count]->type = cert_type;
		p11handle->certs[p11handle->cert_count]->id_cert_length = cert_template[2].ulValueLen;

		p11handle->certs[p11handle->cert_count]->x509 = x509;

		p11handle->certs[p11handle->cert_count]->private_key = CK_INVALID_HANDLE;
		p11handle->certs[p11handle->cert_count]->key_type = 0;

		int i;

		p11handle->certs[p11handle->cert_count]->id_cert = (CK_BYTE*) calloc( cert_template[2].ulValueLen * 2 + 1 , sizeof(CK_BYTE) );
		if( p11handle->certs[p11handle->cert_count]->id_cert == NULL ){
			WLog_ERR(TAG, "Error allocating memory for id_cert");
			free(p11handle->certs[p11handle->cert_count]);
			goto getlist_error;
		}

		for(i=1; i <= cert_template[2].ulValueLen; i++){
			sprintf( (char *) p11handle->certs[p11handle->cert_count]->id_cert, "%s%02x", p11handle->certs[p11handle->cert_count]->id_cert, id_value[i-1]);
		}
		p11handle->certs[p11handle->cert_count]->id_cert[cert_template[2].ulValueLen * 2] = '\0';

		/* Certificate found and save, increment the count */
		++p11handle->cert_count;

		free(id_value);
		free(cert_value);
	} /* while(1) */

	/* release FindObject Session */
	rv = p11->C_FindObjectsFinal(p11handle->session);
	if (rv != CKR_OK) {
		WLog_ERR(TAG, "C_FindObjectsFinal() failed: 0x%08lX", rv);
		free_certs(p11handle->certs, p11handle->cert_count);
		p11handle->certs = NULL;
		p11handle->cert_count = 0;
		goto getlist_error;
	}

	*ncerts = p11handle->cert_count;

	/* arriving here means that's all right */
	WLog_DBG(TAG, "Found %d certificates in token", p11handle->cert_count);
	return p11handle->certs;

	/* some error arrived: clean as possible, and return fail */
getlist_error:
	free(id_value);
	id_value = NULL;
	free(cert_value);
	cert_value = NULL;
	rv = p11->C_FindObjectsFinal(p11handle->session);
	if (rv != CKR_OK) {
		WLog_ERR(TAG, "C_FindObjectsFinal() failed: 0x%08lX", rv);
	}
	free_certs(p11handle->certs, p11handle->cert_count);
	p11handle->certs = NULL;
	p11handle->cert_count = 0;
	return NULL;
}


const X509 *get_X509_certificate(cert_object *cert)
{
	return cert->x509;
}


/** match_id compare id's certificate.
 *  This function is actually called in find_valid_matching_cert().
 *  @param settings - pointer to the rdpSettings structure that contains the settings
 *  @param cert - pointer to the cert_handle structure that contains a certificate
 *  @return 0 if match occurred; 0 otherwise
 */
int match_id(rdpSettings * settings, cert_object * cert)
{
	/* if no login provided, call  */
	if (!cert->id_cert) return 0;

	int res=0; /* default: no match */

	res = compare_id(settings, cert);

	if ( res == 0 )
		WLog_DBG( TAG, "Certificate matches with id (%s)", settings->IdCertificate);
	else
		WLog_DBG( TAG, "Certificate DOES NOT MATCH with id (%s)", settings->IdCertificate);

	return res;
}


/** find_valid_matching_cert find a valid certificate that matches requirements.
 *  This function is actually called in get_valid_smartcard_cert().
 *  @param settings - pointer to the rdpSettings structure that contains the settings
 *  @param p11handle - pointer to the pkcs11_handle structure that contains smartcard data
 *  @return 0 if a valid certificate matches requirements (<0 otherwise)
 */
int find_valid_matching_cert(rdpSettings * settings, pkcs11_handle * p11handle)
{
	int i, rv=0;
	p11handle->valid_cert = NULL;
	X509 * x509 = NULL;

	/* find a valid and matching certificates */
	for (i = 0; i < p11handle->cert_count; i++) {
		x509 = (X509 *)get_X509_certificate(p11handle->certs[i]);
		if (!x509 ) continue; /* sanity check */

		/* verify certificate (date, signature, CRL, ...) */
		rv = verify_certificate(x509, &p11handle->policy);
		if (rv < 0) {
			WLog_ERR(TAG, "verify_certificate() failed: %s", get_error());
			switch (rv) {
			case -2: // X509_V_ERR_CERT_HAS_EXPIRED:
				WLog_ERR(TAG, "Error 2324: Certificate has expired");
				break;
			case -3: // X509_V_ERR_CERT_NOT_YET_VALID:
				WLog_ERR(TAG, "Error 2326: Certificate not yet valid");
				break;
			case -4: // X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
				WLog_ERR(TAG, "Error 2328: Certificate signature invalid");
				break;
			default:
				WLog_ERR(TAG, "Error 2330: Certificate invalid");
				break;
			}
			continue; /* try next certificate */
		}

		/* ensure we extract the right certificate from the list by checking
		 * whether its id matches the id stored in settings previously */
		rv = match_id(settings, p11handle->certs[i]);
		if ( rv < 0 ) { /* match error; abort and return */
			WLog_ERR(TAG, "Fail to match id(%s) : %s\n", p11handle->certs[i]->id_cert, get_error());
			break;
		}
		else if( rv == 0 ){ /* match success */
			p11handle->valid_cert = p11handle->certs[i];
			break;
		}
	}

	/* now valid_cert points to our found certificate or null if none found */
	if (!p11handle->valid_cert) {
		free(p11handle->valid_cert);
		WLog_ERR(TAG, "Error: No matching certificate found");
		return -1;
	}

	return 0;
}


/** get_valid_smartcard_cert find and verify the authentication certificate.
 *  This function is actually called in get_info_smartcard().
 *  @param nla - pointer to rdpNla structure that contains nla settings
 *  @return 0 if the certificate was successfully retrieved; -1 otherwise
 */
int get_valid_smartcard_cert(rdpNla * nla)
{
	int i, ret, ncerts;
	CK_RV ck_rv;
	cert_object **certs = NULL;

	/* init openssl */
	ret = crypto_init(&nla->p11handle->policy);
	if (ret != 0) {
		WLog_ERR(TAG, "Couldn't initialize crypto module ");
		return -1;
	}

	ck_rv = init_authentication_pin(nla);
	if( ck_rv != CKR_OK ){
		WLog_ERR(TAG, "Error initialization PKCS11 session : 0x%08lX", ck_rv);
		return -1;
	}

	/* get list certificate */
	certs = get_list_certificate(nla->p11handle, &ncerts);
	if (certs == NULL)
		goto get_error;

	/* print some info on found certificates */
	WLog_DBG(TAG, "Found '%d' certificate(s)", ncerts);
	for(i =0; i< ncerts;i++) {
		char **name;
		X509 *cert = (X509 *) get_X509_certificate(certs[i]);

		WLog_DBG(TAG, "Certificate #%d:", i+1);
		name = cert_info(cert, CERT_CN, ALGORITHM_NULL);
		WLog_DBG(TAG, "- Common Name:   %s", name[0]); free(name[0]);
		name = cert_info(cert, CERT_SUBJECT, ALGORITHM_NULL);
		WLog_DBG(TAG, "- Subject:   %s", name[0]); free(name[0]);
		name = cert_info(cert, CERT_ISSUER, ALGORITHM_NULL);
		WLog_DBG(TAG, "- Issuer:    %s", name[0]); free(name[0]);
		name = cert_info(cert, CERT_KEY_ALG, ALGORITHM_NULL);
		WLog_DBG(TAG, "- Algorithm: %s", name[0]); free(name[0]);
		ret = verify_certificate(cert,&nla->p11handle->policy);
		if (ret < 0) {
			WLog_ERR(TAG, "verify_certificate() process error: %s", get_error());
			goto get_error;
		} else if (ret != 1) {
			WLog_ERR(TAG, "verify_certificate() failed: %s", get_error());
			continue; /* try next certificate */
		}
		ret = get_private_key(nla->p11handle, certs[i]);
		if (ret<0) {
			WLog_ERR(TAG, "%s %d : Certificate #%d does not have associated private key", __FUNCTION__, __LINE__, i+1);
			goto get_error;
		}

		/* find valid matching authentication certificate */
		int ret_find = find_valid_matching_cert(nla->settings, nla->p11handle);
		if ( ret_find == -1 ) {
			WLog_ERR(TAG, "None certificate found valid and matching requirements");
			goto get_error;
		}
		else if( ret_find == 0 ){
			WLog_INFO(TAG, "Found 1 valid authentication certificate");
			break;
		}
	}

	return 0;

get_error:
	close_pkcs11_session(nla->p11handle);
	release_pkcs11_module(nla->p11handle);
	return -1;
}


/** get_valid_smartcard_UPN is used to get valid UPN and KPN from the smartcard.
 *  This function is actually called in init_authentication_pin().
 *  @param settings - pointer to stucture rdpSettings
 *  @param x509 - pointer to X509 certificate
 *  @return 0 if UPN was successfully retrieved
 */
int get_valid_smartcard_UPN(rdpSettings * settings, X509 * x509)
{
	if (x509 == NULL) {
		WLog_ERR(TAG, "Null certificate provided");
		return -1;
	}

	if(settings->UserPrincipalName){
		WLog_ERR(TAG, "Reset UserPrincipalName");
		free(settings->UserPrincipalName);
		settings->UserPrincipalName = NULL;
	}

	/* retrieve UPN */
	static char **entries_upn = NULL;

	entries_upn = cert_info(x509, CERT_UPN, NULL);
	if (!entries_upn) {
		WLog_ERR(TAG, "cert_info() failed");
		return -1;
	}

	/* set UPN in rdp settings */
	settings->UserPrincipalName = calloc( strlen(*entries_upn) + 1, sizeof(char) );
	if(settings->UserPrincipalName == NULL){
		WLog_ERR(TAG, "Error allocation UserPrincipalName");
		return -1;
	}
	strncpy(settings->UserPrincipalName, *entries_upn, strlen(*entries_upn));
	settings->UserPrincipalName[strlen(*entries_upn)] = '\0';

	WLog_DBG(TAG, "UPN=%s", settings->UserPrincipalName);

	return 0;
}
