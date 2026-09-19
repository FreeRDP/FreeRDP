#include <freerdp/client/printer.h>

int TestPrinterRegistry(int argc, char* argv[])
{
	const rdpContext* first = (const rdpContext*)(size_t)1;
	const rdpContext* second = (const rdpContext*)(size_t)2;
	int rc = -1;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (freerdp_printer_device_exists(first, 1))
		goto out;
	if (!freerdp_printer_device_register(first, 1))
		goto out;
	if (!freerdp_printer_device_exists(first, 1))
		goto out;
	if (freerdp_printer_device_exists(second, 1))
		goto out;
	if (!freerdp_printer_device_register(second, 1))
		goto out;
	if (!freerdp_printer_device_register(first, 1))
		goto out;
	freerdp_printer_device_unregister(first, 1);
	if (freerdp_printer_device_exists(first, 1) || !freerdp_printer_device_exists(second, 1))
		goto out;

	rc = 0;
out:
	freerdp_printer_device_unregister(first, 1);
	freerdp_printer_device_unregister(second, 1);
	return rc;
}
