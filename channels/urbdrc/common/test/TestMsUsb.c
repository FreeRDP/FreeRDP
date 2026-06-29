#include <winpr/crt.h>
#include <winpr/stream.h>

#include <msusb.h>

/* One MSUSB interface descriptor with NumberOfPipes == 0 (12 bytes) followed by
 * the 6 byte configuration descriptor trailer that msusb_msconfig_read reads. */
static const BYTE config_one_iface[] = {
	/* interface[0] */
	0x12, 0x00,             /* Length */
	0x00, 0x00,             /* NumberOfPipesExpected */
	0x00,                   /* InterfaceNumber */
	0x00,                   /* AlternateSetting */
	0x00, 0x00,             /* padding */
	0x00, 0x00, 0x00, 0x00, /* NumberOfPipes = 0 */
	/* configuration descriptor trailer */
	0x09,       /* lenConfiguration */
	0x02,       /* typeConfiguration */
	0xef, 0xbe, /* wTotalLength = 0xBEEF */
	0x00,       /* padding */
	0x42        /* bConfigurationValue */
};

static int test_full_config(void)
{
	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticConstInit(&sbuffer, config_one_iface, sizeof(config_one_iface));

	MSUSB_CONFIG_DESCRIPTOR* cfg = msusb_msconfig_read(s, 1);
	if (!cfg)
	{
		(void)fprintf(stderr, "msusb_msconfig_read rejected a complete descriptor\n");
		return -1;
	}

	int rc = 0;
	if ((cfg->wTotalLength != 0xBEEF) || (cfg->bConfigurationValue != 0x42) ||
	    (cfg->NumInterfaces != 1))
	{
		(void)fprintf(stderr, "msusb_msconfig_read parsed wrong values\n");
		rc = -1;
	}
	msusb_msconfig_free(cfg);
	return rc;
}

/* The descriptor data ends right after the interface list: the 6 byte
 * configuration trailer is missing. The reader must not walk past the end of
 * the received data. */
static int test_truncated_trailer(void)
{
	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticConstInit(&sbuffer, config_one_iface, 12);

	MSUSB_CONFIG_DESCRIPTOR* cfg = msusb_msconfig_read(s, 1);
	if (cfg)
	{
		(void)fprintf(stderr, "msusb_msconfig_read accepted a truncated trailer\n");
		msusb_msconfig_free(cfg);
		return -1;
	}
	return 0;
}

/* Capacity larger than the sealed length must not let the reader consume the
 * unused tail of the (recycled) buffer. */
static int test_capacity_exceeds_length(void)
{
	BYTE buffer[64] = WINPR_C_ARRAY_INIT;
	CopyMemory(buffer, config_one_iface, sizeof(config_one_iface));

	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticInit(&sbuffer, buffer, sizeof(buffer));
	if (!Stream_SetLength(s, 12))
		return -1;

	MSUSB_CONFIG_DESCRIPTOR* cfg = msusb_msconfig_read(s, 1);
	if (cfg)
	{
		(void)fprintf(stderr, "msusb_msconfig_read read past the sealed length\n");
		msusb_msconfig_free(cfg);
		return -1;
	}
	return 0;
}

/* msusb_msinterface_read claims NumberOfPipes pipes whose data is not present. */
static int test_interface_truncated_pipes(void)
{
	const BYTE iface[] = {
		0x10, 0x00,            /* Length */
		0x01, 0x00,            /* NumberOfPipesExpected */
		0x00,                  /* InterfaceNumber */
		0x00,                  /* AlternateSetting */
		0x00, 0x00,            /* padding */
		0x04, 0x00, 0x00, 0x00 /* NumberOfPipes = 4, but no pipe data follows */
	};
	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticConstInit(&sbuffer, iface, sizeof(iface));

	MSUSB_INTERFACE_DESCRIPTOR* desc = msusb_msinterface_read(s);
	if (desc)
	{
		(void)fprintf(stderr, "msusb_msinterface_read accepted missing pipe data\n");
		msusb_msinterface_free(desc);
		return -1;
	}
	return 0;
}

int TestMsUsb(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (test_full_config() != 0)
		return -1;
	if (test_truncated_trailer() != 0)
		return -1;
	if (test_capacity_exceeds_length() != 0)
		return -1;
	if (test_interface_truncated_pipes() != 0)
		return -1;
	return 0;
}
