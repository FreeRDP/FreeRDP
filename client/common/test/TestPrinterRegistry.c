#include <freerdp/client/printer.h>

#include <winpr/crt.h>

static void test_printer_reference(WINPR_ATTR_UNUSED rdpPrinter* printer)
{
}

static BOOL test_printer_capabilities(WINPR_ATTR_UNUSED rdpPrinter* printer, char** xml,
                                      size_t* length)
{
	static const char document[] = "<PrintCapabilities/>";

	*xml = _strdup(document);
	if (!*xml)
		return FALSE;
	*length = ARRAYSIZE(document) - 1;
	return TRUE;
}

int TestPrinterRegistry(int argc, char* argv[])
{
	const rdpContext* first = (const rdpContext*)(size_t)1;
	const rdpContext* second = (const rdpContext*)(size_t)2;
	rdpPrinter firstPrinter = WINPR_C_ARRAY_INIT;
	rdpPrinter secondPrinter = WINPR_C_ARRAY_INIT;
	char* xml = nullptr;
	size_t length = 0;
	int rc = -1;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	firstPrinter.AddRef = test_printer_reference;
	firstPrinter.ReleaseRef = test_printer_reference;
	firstPrinter.GetCapabilities = test_printer_capabilities;
	secondPrinter.AddRef = test_printer_reference;
	secondPrinter.ReleaseRef = test_printer_reference;

	if (freerdp_printer_device_exists(first, 1))
		goto out;
	if (!freerdp_printer_device_register(first, 1, &firstPrinter))
		goto out;
	if (!freerdp_printer_device_exists(first, 1))
		goto out;
	if (freerdp_printer_device_exists(second, 1))
		goto out;
	if (!freerdp_printer_device_register(second, 1, &secondPrinter))
		goto out;
	if (!freerdp_printer_device_register(first, 1, &firstPrinter))
		goto out;
	if (!freerdp_printer_device_get_capabilities(first, 1, &xml, &length) ||
	    (length != strlen("<PrintCapabilities/>")) || (strcmp(xml, "<PrintCapabilities/>") != 0))
		goto out;
	free(xml);
	xml = nullptr;
	freerdp_printer_device_unregister(first, 1);
	if (freerdp_printer_device_exists(first, 1) || !freerdp_printer_device_exists(second, 1))
		goto out;

	rc = 0;
out:
	free(xml);
	freerdp_printer_device_unregister(first, 1);
	freerdp_printer_device_unregister(second, 1);
	return rc;
}
