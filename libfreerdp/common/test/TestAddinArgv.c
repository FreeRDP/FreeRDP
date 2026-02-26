#include <winpr/crt.h>
#include <winpr/crypto.h>

#include <freerdp/settings.h>

static BOOL test_alloc(void)
{
	BOOL rc = FALSE;
	int rng = 0;
	const char* param[] = { "foo:", "bar", "bla", "rdp", nullptr };
	ADDIN_ARGV* arg1 = nullptr;
	ADDIN_ARGV* arg2 = nullptr;
	ADDIN_ARGV* arg3 = nullptr;
	ADDIN_ARGV* arg4 = nullptr;

	/* Test empty allocation */
	arg1 = freerdp_addin_argv_new(0, nullptr);
	if (!arg1 || (arg1->argc != 0) || (arg1->argv))
		goto fail;

	/* Test allocation without initializing arguments of random size > 0 */
	winpr_RAND(&rng, sizeof(rng));
	rng = abs(rng % 8192) + 1;

	arg2 = freerdp_addin_argv_new(rng, nullptr);
	if (!arg2 || (arg2->argc != rng) || (!arg2->argv))
		goto fail;
	for (int x = 0; x < arg2->argc; x++)
	{
		if (arg2->argv[x])
			goto fail;
	}

	/* Test allocation with initializing arguments of size > 0 */
	arg3 = freerdp_addin_argv_new(ARRAYSIZE(param) - 1, param);
	if (!arg3 || (arg3->argc != ARRAYSIZE(param) - 1) || (!arg3->argv))
		goto fail;

	for (int x = 0; x < arg3->argc; x++)
	{
		if (strcmp(arg3->argv[x], param[x]) != 0)
			goto fail;
	}

	/* Input lists with nullptr elements are not allowed */
	arg4 = freerdp_addin_argv_new(ARRAYSIZE(param), param);
	if (arg4)
		goto fail;
	rc = TRUE;
fail:
	freerdp_addin_argv_free(arg1);
	freerdp_addin_argv_free(arg2);
	freerdp_addin_argv_free(arg3);
	freerdp_addin_argv_free(arg4);
	printf("%s: %d\n", __func__, rc);
	return rc;
}

static BOOL test_clone(void)
{
	BOOL rc = FALSE;
	const char* param[] = { "foo:", "bar", "bla", "rdp" };
	ADDIN_ARGV* arg = nullptr;
	ADDIN_ARGV* clone = nullptr;
	ADDIN_ARGV* clone2 = nullptr;

	arg = freerdp_addin_argv_new(ARRAYSIZE(param), param);
	if (!arg || (arg->argc != ARRAYSIZE(param)))
		goto fail;
	clone = freerdp_addin_argv_clone(arg);
	if (!clone || (clone->argc != arg->argc))
		goto fail;

	for (int x = 0; x < arg->argc; x++)
	{
		if (strcmp(param[x], arg->argv[x]) != 0)
			goto fail;
		if (strcmp(param[x], clone->argv[x]) != 0)
			goto fail;
	}

	clone2 = freerdp_addin_argv_clone(nullptr);
	if (clone2)
		goto fail;
	rc = TRUE;
fail:
	freerdp_addin_argv_free(arg);
	freerdp_addin_argv_free(clone);
	freerdp_addin_argv_free(clone2);
	printf("%s: %d\n", __func__, rc);
	return rc;
}

static BOOL test_add_remove(void)
{
	const char* args[] = { "foo", "bar", "bla", "gaga" };
	BOOL rc = FALSE;
	ADDIN_ARGV* arg = freerdp_addin_argv_new(0, nullptr);

	if (!arg || (arg->argc != 0) || arg->argv)
		goto fail;
	for (size_t y = 0; y < ARRAYSIZE(args); y++)
	{
		const char* param = args[y];
		if (!freerdp_addin_argv_add_argument(arg, param))
			goto fail;
		if (arg->argc != (int)y + 1)
			goto fail;
		if (!arg->argv)
			goto fail;
		if (strcmp(arg->argv[y], param) != 0)
			goto fail;
	}

	/* Try to remove non existing element, must not return TRUE */
	if (freerdp_addin_argv_del_argument(arg, "foobar"))
		goto fail;

	/* Invalid parameters, must return FALSE */
	if (freerdp_addin_argv_del_argument(nullptr, "foobar"))
		goto fail;

	/* Invalid parameters, must return FALSE */
	if (freerdp_addin_argv_del_argument(arg, nullptr))
		goto fail;

	/* Remove elements one by one to test argument index move */
	for (size_t y = 0; y < ARRAYSIZE(args); y++)
	{
		const char* param = args[y];
		if (!freerdp_addin_argv_del_argument(arg, param))
			goto fail;
		for (size_t x = y + 1; x < ARRAYSIZE(args); x++)
		{
			if (strcmp(arg->argv[x - y - 1], args[x]) != 0)
				goto fail;
		}
	}
	rc = TRUE;
fail:
	freerdp_addin_argv_free(arg);
	printf("%s: %d\n", __func__, rc);
	return rc;
}

static BOOL test_set_argument(void)
{
	int ret = 0;
	const char* newarg = "foobar";
	const char* args[] = { "foo", "bar", "bla", "gaga" };
	BOOL rc = FALSE;
	ADDIN_ARGV* arg = nullptr;

	arg = freerdp_addin_argv_new(ARRAYSIZE(args), args);
	if (!arg || (arg->argc != ARRAYSIZE(args)) || !arg->argv)
		goto fail;

	/* Check invalid parameters */
	ret = freerdp_addin_set_argument(nullptr, "foo");
	if (ret >= 0)
		goto fail;
	ret = freerdp_addin_set_argument(arg, nullptr);
	if (ret >= 0)
		goto fail;

	/* Try existing parameter */
	ret = freerdp_addin_set_argument(arg, "foo");
	if ((ret != 1) || (arg->argc != ARRAYSIZE(args)))
		goto fail;

	/* Try new parameter */
	ret = freerdp_addin_set_argument(arg, newarg);
	if ((ret != 0) || (arg->argc != ARRAYSIZE(args) + 1) ||
	    (strcmp(newarg, arg->argv[ARRAYSIZE(args)]) != 0))
		goto fail;

	rc = TRUE;
fail:
	freerdp_addin_argv_free(arg);
	printf("%s: %d\n", __func__, rc);
	return rc;
}

static BOOL test_replace_argument(void)
{
	int ret = 0;
	const char* newarg = "foobar";
	const char* args[] = { "foo", "bar", "bla", "gaga" };
	BOOL rc = FALSE;
	ADDIN_ARGV* arg = nullptr;

	arg = freerdp_addin_argv_new(ARRAYSIZE(args), args);
	if (!arg || (arg->argc != ARRAYSIZE(args)) || !arg->argv)
		goto fail;

	/* Check invalid parameters */
	ret = freerdp_addin_replace_argument(nullptr, "foo", newarg);
	if (ret >= 0)
		goto fail;
	ret = freerdp_addin_replace_argument(arg, nullptr, newarg);
	if (ret >= 0)
		goto fail;
	ret = freerdp_addin_replace_argument(arg, "foo", nullptr);
	if (ret >= 0)
		goto fail;

	/* Try existing parameter */
	ret = freerdp_addin_replace_argument(arg, "foo", newarg);
	if ((ret != 1) || (arg->argc != ARRAYSIZE(args)) || (strcmp(arg->argv[0], newarg) != 0))
		goto fail;

	/* Try new parameter */
	ret = freerdp_addin_replace_argument(arg, "lalala", newarg);
	if ((ret != 0) || (arg->argc != ARRAYSIZE(args) + 1) ||
	    (strcmp(newarg, arg->argv[ARRAYSIZE(args)]) != 0))
		goto fail;

	rc = TRUE;
fail:
	freerdp_addin_argv_free(arg);
	printf("%s: %d\n", __func__, rc);
	return rc;
}

static BOOL test_set_argument_value(void)
{
	int ret = 0;
	const char* newarg1 = "foobar";
	const char* newarg2 = "lalala";
	const char* fullnewarg1 = "foo:foobar";
	const char* fullnewarg2 = "foo:lalala";
	const char* fullnewvalue = "lalala:foobar";
	const char* args[] = { "foo", "foo:", "bar", "bla", "gaga" };
	BOOL rc = FALSE;
	ADDIN_ARGV* arg = nullptr;

	arg = freerdp_addin_argv_new(ARRAYSIZE(args), args);
	if (!arg || (arg->argc != ARRAYSIZE(args)) || !arg->argv)
		goto fail;

	/* Check invalid parameters */
	ret = freerdp_addin_set_argument_value(nullptr, "foo", newarg1);
	if (ret >= 0)
		goto fail;
	ret = freerdp_addin_set_argument_value(arg, nullptr, newarg1);
	if (ret >= 0)
		goto fail;
	ret = freerdp_addin_set_argument_value(arg, "foo", nullptr);
	if (ret >= 0)
		goto fail;

	/* Try existing parameter */
	ret = freerdp_addin_set_argument_value(arg, "foo", newarg1);
	if ((ret != 1) || (arg->argc != ARRAYSIZE(args)) || (strcmp(arg->argv[1], fullnewarg1) != 0))
		goto fail;
	ret = freerdp_addin_set_argument_value(arg, "foo", newarg2);
	if ((ret != 1) || (arg->argc != ARRAYSIZE(args)) || (strcmp(arg->argv[1], fullnewarg2) != 0))
		goto fail;

	/* Try new parameter */
	ret = freerdp_addin_set_argument_value(arg, newarg2, newarg1);
	if ((ret != 0) || (arg->argc != ARRAYSIZE(args) + 1) ||
	    (strcmp(fullnewvalue, arg->argv[ARRAYSIZE(args)]) != 0))
		goto fail;

	rc = TRUE;
fail:
	freerdp_addin_argv_free(arg);
	printf("%s: %d\n", __func__, rc);
	return rc;
}

static BOOL test_replace_argument_value(void)
{
	int ret = 0;
	const char* newarg1 = "foobar";
	const char* newarg2 = "lalala";
	const char* fullnewarg1 = "foo:foobar";
	const char* fullnewarg2 = "foo:lalala";
	const char* fullnewvalue = "lalala:foobar";
	const char* args[] = { "foo", "foo:", "bar", "bla", "gaga" };
	BOOL rc = FALSE;
	ADDIN_ARGV* arg = nullptr;

	arg = freerdp_addin_argv_new(ARRAYSIZE(args), args);
	if (!arg || (arg->argc != ARRAYSIZE(args)) || !arg->argv)
		goto fail;

	/* Check invalid parameters */
	ret = freerdp_addin_replace_argument_value(nullptr, "bar", "foo", newarg1);
	if (ret >= 0)
		goto fail;
	ret = freerdp_addin_replace_argument_value(arg, nullptr, "foo", newarg1);
	if (ret >= 0)
		goto fail;
	ret = freerdp_addin_replace_argument_value(arg, "foo", nullptr, newarg1);
	if (ret >= 0)
		goto fail;
	ret = freerdp_addin_replace_argument_value(arg, "bar", "foo", nullptr);
	if (ret >= 0)
		goto fail;

	/* Try existing parameter */
	ret = freerdp_addin_replace_argument_value(arg, "bla", "foo", newarg1);
	if ((ret != 1) || (arg->argc != ARRAYSIZE(args)) || (strcmp(arg->argv[3], fullnewarg1) != 0))
		goto fail;
	ret = freerdp_addin_replace_argument_value(arg, "foo", "foo", newarg2);
	if ((ret != 1) || (arg->argc != ARRAYSIZE(args)) || (strcmp(arg->argv[0], fullnewarg2) != 0))
		goto fail;

	/* Try new parameter */
	ret = freerdp_addin_replace_argument_value(arg, "hahaha", newarg2, newarg1);
	if ((ret != 0) || (arg->argc != ARRAYSIZE(args) + 1) ||
	    (strcmp(fullnewvalue, arg->argv[ARRAYSIZE(args)]) != 0))
		goto fail;

	rc = TRUE;
fail:
	freerdp_addin_argv_free(arg);
	printf("%s: %d\n", __func__, rc);
	return rc;
}

int TestAddinArgv(int argc, char* argv[])
{

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!test_alloc())
		return -1;
	if (!test_clone())
		return -1;
	if (!test_add_remove())
		return -1;
	if (!test_set_argument())
		return -1;
	if (!test_replace_argument())
		return -1;
	if (!test_set_argument_value())
		return -1;
	if (!test_replace_argument_value())
		return -1;
	return 0;
}
