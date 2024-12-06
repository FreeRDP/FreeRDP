#include <rdpear-common/ndr.h>

int TestNdr(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	int retCode = -2;
	NdrContext* context = ndr_context_new(FALSE, 1);
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
	NdrArrayHints hints = { 2 };
	NdrDeferredEntry e = { 0x020028, "arrayContent", &hints, (void*)&target,
		                   ndr_uint8Array_descr() };

	if (!ndr_push_deferreds(context, &e, 1))
		goto out;

	if (!ndr_treat_deferred_read(context, s))
		goto out;

	NdrMessageType descr = ndr_uint8Array_descr();
	descr->destroyFn(context, &hints, target);
	free(target);
	retCode = 0;
out:
	ndr_context_destroy(&context);
	return retCode;
}
