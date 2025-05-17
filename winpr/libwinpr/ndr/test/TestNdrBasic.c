#include <winpr/ndr.h>

int TestNdrBasic(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	int retCode = -2;
	WinPrNdrContext* context = winpr_ndr_context_new(FALSE, 1);
	if (!context)
		return -1;

	BYTE payload[] = {
		// == conformant array ==
		0x02, 0x00, 0x00, 0x00, // (nitems)
		0x30, 0x00,             // content
		0x00, 0x00              // (padding)
	};
	wStream staticS;
	wStream* s = Stream_StaticInit(&staticS, payload, sizeof(payload));

	BYTE* target = NULL;
	WinPrNdrArrayHints hints = { 2 };
	WinPrNdrDeferredEntry e = { 0x020028, "arrayContent", &hints, (void*)&target,
		                        winpr_ndr_uint8Array_descr() };

	if (!winpr_ndr_push_deferreds(context, &e, 1))
		goto out;

	if (!winpr_ndr_treat_deferred_read(context, s))
		goto out;

	WinPrNdrMessageType descr = winpr_ndr_uint8Array_descr();
	descr->destroyFn(context, &hints, target);
	free(target);
	retCode = 0;
out:
	winpr_ndr_context_destroy(&context);
	return retCode;
}
