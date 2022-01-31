#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/ssl.h>
#include <winpr/wlog.h>

#include <freerdp/assistance.h>

static const char TEST_MSRC_INCIDENT_PASSWORD_TYPE1[] = "Password1";

static const char TEST_MSRC_INCIDENT_FILE_TYPE1[] =
    "<?xml version=\"1.0\" encoding=\"Unicode\" ?>"
    "<UPLOADINFO TYPE=\"Escalated\">"
    "<UPLOADDATA "
    "USERNAME=\"Administrator\" "
    "RCTICKET=\"65538,1,10.0.3.105:3389;winxpsp3.contoso3.com:3389,*,"
    "rb+v0oPmEISmi8N2zK/vuhgul/ABqlDt6wW0VxMyxK8=,*,*,IuaRySSbPDNna4+2mKcsKxsbJFI=\""
    "RCTICKETENCRYPTED=\"1\" "
    "DtStart=\"1314905741\" "
    "DtLength=\"180\" "
    "PassStub=\"RT=0PvIndan52*\" "
    "L=\"0\" />"
    "</UPLOADINFO>";

#if __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"
#endif
static const BYTE TEST_MSRC_INCIDENT_EXPERT_BLOB_TYPE1[32] =
    "\x3C\x9C\xAE\x0B\xCE\x7A\xB1\x5C\x8A\xAC\x01\xD6\x76\x04\x5E\xDF"
    "\x3F\xFA\xF0\x92\xE2\xDE\x36\x8A\x20\x17\xE6\x8A\x0D\xED\x7C\x90";
#if __GNUC__
#pragma GCC diagnostic pop
#endif

static const char TEST_MSRC_INCIDENT_PASSWORD_TYPE2[] = "48BJQ853X3B4";

static const char TEST_MSRC_INCIDENT_FILE_TYPE2[] =
    "<?xml version=\"1.0\"?>"
    "<UPLOADINFO TYPE=\"Escalated\">"
    "<UPLOADDATA USERNAME=\"awake\" "
    "LHTICKET=\""
    "20FCC407AA53E95F8505AB56D485D26835064B03AF86CDA326248FD304626AD4"
    "DBDBDFFE0C473228EFFF7A1E6CEB445BBEC429294BB6616BBB600854438DDFB5"
    "82FC377CF65A2060EB3221647643C9B29BF5EC320856180B34D1BE9827A528C7"
    "E8F0DCD53C8D38F974160FEE317458FAC9DBDBA7B972D21DF3BC5B1AF0E01878"
    "65F07A3B915618C03E6EAF843FC1185770A1208C29C836DBCA5A040CB276D3C4"
    "1DDE2FA8CA9627E5E74FA750A92C0E01AD6C3D1000A5B1479DEB899BF5BCD402"
    "CE3BB3BF104CE0286C3F985AA711943C88C5EEEEE86F35B63F68883A90ADBCFD"
    "CBBAE3EAB993EFD9148E1A21D092CE9498695943946236D65D20B4A38D724C61"
    "72319E38E19C04E98EBC03F56A4A190E971F8EAEBFE6B415A3A2D8F35F7BF785"
    "26B9BFAAB48D11BDD6C905EFE503D2265678E1EAD2F2F124E570667F04103180"
    "2F63587276C14E6A5AB436CE234F722CE7C9B5D244508F14C012E84A49FE6992"
    "3F30320ABB3641F1EFA66205F3EA709E7E1C3E6874BB9642486FB96D2730CDF4"
    "514AA738167F00FC13B2978AED1D6678413FDF62008B03DD729E36173BE02742"
    "B69CAD44938512D0F56335394759338AF6ADBCF39CE829116D97435085D05BB5"
    "9320A134698050DCDBE01305A6B4712FD6BD48958BD2DC497498FF35CAECC9A8"
    "2C97FD1A5B5EC4BAF5FFB75A1471B765C465B35A7C950019066BB219B391C6E9"
    "8AE8FD2038E774F36F226D9FB9A38BCC313785612165D1EF69D19E2B9CF6E0F7"
    "FE1ECCF00AB81F9E8B626363CA82FAC719A3B7D243325C9D6042B2488EC95B80"
    "A31273FF9B72FBBB86F946E6D3DF8816BE4533F0B547C8BC028309EA9784C1E6\" "
    "RCTICKET=\"65538,1,192.168.1.200:49230;169.254.6.170:49231,*,"
    "+ULZ6ifjoCa6cGPMLQiGHRPwkg6VyJqGwxMnO6GcelwUh9a6/FBq3It5ADSndmLL,"
    "*,*,BNRjdu97DyczQSRuMRrDWoue+HA=\" "
    "PassStub=\"WB^6HsrIaFmEpi\" "
    "RCTICKETENCRYPTED=\"1\" "
    "DtStart=\"1403972263\" "
    "DtLength=\"14400\" "
    "L=\"0\"/>"
    "</UPLOADINFO>";

/**
 * Decrypted Connection String 2:
 *
 * <E>
 * <A KH="BNRjdu97DyczQSRuMRrDWoue+HA="
 * ID="+ULZ6ifjoCa6cGPMLQiGHRPwkg6VyJqGwxMnO6GcelwUh9a6/FBq3It5ADSndmLL"/> <C> <T ID="1" SID="0"> <L
 * P="49228" N="fe80::1032:53d9:5a01:909b%3"/> <L P="49229" N="fe80::3d8f:9b2d:6b4e:6aa%6"/> <L
 * P="49230" N="192.168.1.200"/> <L P="49231" N="169.254.6.170"/>
 * </T>
 * </C>
 * </E>
 */

static BOOL test_msrsc_incident_file_type1(wLog* log)
{
	BOOL rc = FALSE;
	int status;
	char* pass = NULL;
	char* expertBlob = NULL;
	const char* EncryptedPassStub;
	size_t EncryptedPassStubLength;
	rdpAssistanceFile* file = freerdp_assistance_file_new();

	if (!file)
		return -1;

	status = freerdp_assistance_parse_file_buffer(file, TEST_MSRC_INCIDENT_FILE_TYPE1,
	                                              sizeof(TEST_MSRC_INCIDENT_FILE_TYPE1),
	                                              TEST_MSRC_INCIDENT_PASSWORD_TYPE1);
	WLog_Print(log, WLOG_INFO, "freerdp_assistance_parse_file_buffer: %d", status);

	if (status < 0)
		goto fail;

	freerdp_assistance_print_file(file, WLog_Get("foo"), WLOG_INFO);

	if (!freerdp_assistance_get_encrypted_pass_stub(file, &EncryptedPassStub,
	                                                &EncryptedPassStubLength))
		goto fail;

	pass = freerdp_assistance_bin_to_hex_string(EncryptedPassStub, EncryptedPassStubLength);

	if (!pass)
		goto fail;

	WLog_Print(log, WLOG_INFO, "freerdp_assistance_decrypt: %d %s [%" PRIdz "]", status, pass,
	           EncryptedPassStubLength);
	expertBlob = freerdp_assistance_construct_expert_blob("Edgar Olougouna", pass);
	WLog_Print(log, WLOG_INFO, "expertBlob='%s'", expertBlob);
	rc = TRUE;
fail:
	freerdp_assistance_file_free(file);
	free(pass);
	free(expertBlob);
	return rc;
}

static BOOL test_msrsc_incident_file_type2(wLog* log)
{
	int status = -1;
	char* pass = NULL;
	char* expertBlob = NULL;
	const char* EncryptedPassStub = NULL;
	size_t EncryptedPassStubLength;
	rdpAssistanceFile* file = freerdp_assistance_file_new();

	if (!file)
		return -1;

	status = freerdp_assistance_parse_file_buffer(file, TEST_MSRC_INCIDENT_FILE_TYPE2,
	                                              sizeof(TEST_MSRC_INCIDENT_FILE_TYPE2),
	                                              TEST_MSRC_INCIDENT_PASSWORD_TYPE2);
	printf("freerdp_assistance_parse_file_buffer: %d\n", status);

	if (status < 0)
		goto fail;

	freerdp_assistance_print_file(file, log, WLOG_INFO);
	status = freerdp_assistance_get_encrypted_pass_stub(file, &EncryptedPassStub,
	                                                    &EncryptedPassStubLength);
	pass = freerdp_assistance_bin_to_hex_string(EncryptedPassStub, EncryptedPassStubLength);

	if (!pass)
		goto fail;

	WLog_Print(log, WLOG_INFO, "freerdp_assistance_decrypt: %d %s [%" PRIdz "]", status, pass,
	           EncryptedPassStubLength);

	if (status < 0)
		goto fail;

	expertBlob = freerdp_assistance_construct_expert_blob("Edgar Olougouna", pass);
	WLog_Print(log, WLOG_INFO, "expertBlob='%s'", expertBlob);
fail:
	freerdp_assistance_file_free(file);
	free(expertBlob);
	free(pass);
	return status >= 0 ? TRUE : FALSE;
}

int TestCommonAssistance(int argc, char* argv[])
{
	wLog* log;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	log = WLog_Get(__FUNCTION__);
	winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT);

	if (!test_msrsc_incident_file_type1(log))
	{
		WLog_Print(log, WLOG_ERROR, "test_msrsc_incident_file_type1 failed");
		return -1;
	}

	if (!test_msrsc_incident_file_type2(log))
	{
		WLog_Print(log, WLOG_ERROR, "test_msrsc_incident_file_type1 failed");
		return -1;
	}

	return 0;
}
