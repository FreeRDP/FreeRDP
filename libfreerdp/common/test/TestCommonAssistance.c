
#include <winpr/crt.h>
#include <winpr/print.h>

#include <freerdp/assistance.h>

const char* TEST_MSRC_INCIDENT_PASSWORD_TYPE1 = "Password1";

static const char* TEST_MSRC_INCIDENT_FILE_TYPE1 =
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

const BYTE TEST_MSRC_INCIDENT_EXPERT_BLOB_TYPE1[32] =
	"\x3C\x9C\xAE\x0B\xCE\x7A\xB1\x5C\x8A\xAC\x01\xD6\x76\x04\x5E\xDF"
	"\x3F\xFA\xF0\x92\xE2\xDE\x36\x8A\x20\x17\xE6\x8A\x0D\xED\x7C\x90";

const char* TEST_MSRC_INCIDENT_PASSWORD_TYPE2 = "48BJQ853X3B4";

static const char* TEST_MSRC_INCIDENT_FILE_TYPE2 =
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
 * <A KH="BNRjdu97DyczQSRuMRrDWoue+HA=" ID="+ULZ6ifjoCa6cGPMLQiGHRPwkg6VyJqGwxMnO6GcelwUh9a6/FBq3It5ADSndmLL"/>
 * <C>
 * <T ID="1" SID="0">
 * 	<L P="49228" N="fe80::1032:53d9:5a01:909b%3"/>
 * 	<L P="49229" N="fe80::3d8f:9b2d:6b4e:6aa%6"/>
 * 	<L P="49230" N="192.168.1.200"/>
 * 	<L P="49231" N="169.254.6.170"/>
 * </T>
 * </C>
 * </E>
 */

int test_msrsc_incident_file_type1()
{
	int status;
	char* pass;
	char* expertBlob;
	rdpAssistanceFile* file;

	file = freerdp_assistance_file_new();

	status = freerdp_assistance_parse_file_buffer(file,
			TEST_MSRC_INCIDENT_FILE_TYPE1, sizeof(TEST_MSRC_INCIDENT_FILE_TYPE1));

	printf("freerdp_assistance_parse_file_buffer: %d\n", status);

	if (status < 0)
		return -1;

	printf("Username: %s\n", file->Username);
	printf("LHTicket: %s\n", file->LHTicket);
	printf("RCTicket: %s\n", file->RCTicket);
	printf("RCTicketEncrypted: %d\n", file->RCTicketEncrypted);
	printf("PassStub: %s\n", file->PassStub);
	printf("DtStart: %d\n", file->DtStart);
	printf("DtLength: %d\n", file->DtLength);
	printf("LowSpeed: %d\n", file->LowSpeed);

	printf("RASessionId: %s\n", file->RASessionId);
	printf("RASpecificParams: %s\n", file->RASpecificParams);
	printf("MachineAddress: %s\n", file->MachineAddress);
	printf("MachinePort: %d\n", (int) file->MachinePort);

	status = freerdp_assistance_decrypt(file, TEST_MSRC_INCIDENT_PASSWORD_TYPE1);

	printf("freerdp_assistance_decrypt: %d\n", status);

	if (status < 0)
		return -1;

	pass = freerdp_assistance_bin_to_hex_string(file->EncryptedPassStub, file->EncryptedPassStubLength);

	if (!pass)
		return -1;

	expertBlob = freerdp_assistance_construct_expert_blob("Edgar Olougouna", pass);

	freerdp_assistance_file_free(file);

	free(pass);
	free(expertBlob);

	return 0;
}

int test_msrsc_incident_file_type2()
{
	int status;
	rdpAssistanceFile* file;

	file = freerdp_assistance_file_new();

	status = freerdp_assistance_parse_file_buffer(file,
			TEST_MSRC_INCIDENT_FILE_TYPE2, sizeof(TEST_MSRC_INCIDENT_FILE_TYPE2));

	printf("freerdp_assistance_parse_file_buffer: %d\n", status);

	if (status < 0)
		return -1;

	printf("Username: %s\n", file->Username);
	printf("LHTicket: %s\n", file->LHTicket);
	printf("RCTicket: %s\n", file->RCTicket);
	printf("RCTicketEncrypted: %d\n", file->RCTicketEncrypted);
	printf("PassStub: %s\n", file->PassStub);
	printf("DtStart: %d\n", file->DtStart);
	printf("DtLength: %d\n", file->DtLength);
	printf("LowSpeed: %d\n", file->LowSpeed);

	printf("RASessionId: %s\n", file->RASessionId);
	printf("RASpecificParams: %s\n", file->RASpecificParams);
	printf("MachineAddress: %s\n", file->MachineAddress);
	printf("MachinePort: %d\n", (int) file->MachinePort);

	status = freerdp_assistance_decrypt(file, TEST_MSRC_INCIDENT_PASSWORD_TYPE2);

	printf("freerdp_assistance_decrypt: %d\n", status);

	if (status < 0)
		return -1;

	printf("ConnectionString2: %s\n", file->ConnectionString2);

	freerdp_assistance_file_free(file);

	return 0;
}

int TestCommonAssistance(int argc, char* argv[])
{
	test_msrsc_incident_file_type1();

	test_msrsc_incident_file_type2();

	return 0;
}

